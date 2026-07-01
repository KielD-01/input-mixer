use std::sync::{Arc, Mutex};
use std::thread;

use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
use cpal::{BufferSize, SampleFormat, Stream, StreamConfig};

use crate::sources::{OutputDeviceDescriptor, SourceId, CHANNELS, SAMPLE_RATE};

use super::monitor_ring::MonitorRingBuffer;
use super::platform::PlatformError;

#[cfg(target_os = "macos")]
fn can_capture_microphone_inputs() -> bool {
    crate::capture::macos::has_microphone_access()
}

#[cfg(not(target_os = "macos"))]
fn can_capture_microphone_inputs() -> bool {
    true
}

/// Input device buffer when monitor is active (lower capture latency).
const LOW_LATENCY_INPUT_BUFFER_FRAMES: u32 = 256;
/// Max queued input frames before dropping oldest (keeps monitor path near realtime).
const LOW_LATENCY_INPUT_RING_MAX_FRAMES: usize = 384;

#[cfg(target_os = "macos")]
pub fn enumerate_hardware_inputs() -> Vec<SourceId> {
    // HAL property queries only — cpal::input_devices() opens input AudioUnits and triggers mic TCC.
    crate::capture::macos::hal_enumerate_input_devices()
}

#[cfg(not(target_os = "macos"))]
pub fn enumerate_hardware_inputs() -> Vec<SourceId> {
    let host = cpal::default_host();
    let mut sources = Vec::new();

    if let Ok(devices) = host.input_devices() {
        for device in devices {
            if let Ok(name) = device.name() {
                let uid = format!("hw-in:{}", name);
                sources.push(SourceId::hardware(uid, name));
            }
        }
    }

    sources
}

#[cfg(target_os = "macos")]
pub fn enumerate_hardware_outputs() -> Vec<OutputDeviceDescriptor> {
    crate::capture::macos::hal_enumerate_output_devices()
}

#[cfg(not(target_os = "macos"))]
pub fn enumerate_hardware_outputs() -> Vec<OutputDeviceDescriptor> {
    let host = cpal::default_host();
    let mut outputs = Vec::new();

    if let Ok(devices) = host.output_devices() {
        for device in devices {
            if let Ok(name) = device.name() {
                outputs.push(OutputDeviceDescriptor {
                    uid: format!("hw-out:{}", name),
                    name,
                });
            }
        }
    }

    outputs
}

pub struct HardwareCapture {
    _stream: Stream,
    ring: Arc<Mutex<Vec<f32>>>,
    channels: usize,
    sample_rate: u32,
}

impl HardwareCapture {
    pub fn open(source: &SourceId) -> Result<Self, PlatformError> {
        Self::open_with_options(source, false)
    }

    pub fn open_with_options(source: &SourceId, low_latency: bool) -> Result<Self, PlatformError> {
        if !can_capture_microphone_inputs() {
            eprintln!(
                "Hardware capture denied (no mic permission): {}",
                source.display_name
            );
            return Err(PlatformError::Permission("Microphone".into()));
        }

        let host = cpal::default_host();
        let device = find_input_device(&host, &source.hardware_uid)?;

        let config = best_input_config(&device)?;
        let sample_rate = config.sample_rate().0;

        let mut stream_config: StreamConfig = config.clone().into();
        if low_latency {
            stream_config.buffer_size = BufferSize::Fixed(LOW_LATENCY_INPUT_BUFFER_FRAMES);
        }

        let ring = Arc::new(Mutex::new(Vec::with_capacity(sample_rate as usize * 2)));
        let ring_cb = ring.clone();
        let channels = config.channels() as usize;
        let max_ring_samples = if low_latency {
            LOW_LATENCY_INPUT_RING_MAX_FRAMES * channels
        } else {
            0
        };

        let stream = match config.sample_format() {
            SampleFormat::F32 => {
                let err_channels = channels;
                device
                    .build_input_stream(
                        &stream_config,
                        move |data: &[f32], _| {
                            let mut buf = ring_cb.lock().unwrap();
                            buf.extend_from_slice(data);
                            if max_ring_samples > 0 && buf.len() > max_ring_samples {
                                let excess = buf.len() - max_ring_samples;
                                buf.drain(..excess);
                            }
                        },
                        move |e| eprintln!("Input stream error ({err_channels}ch): {e}"),
                        None,
                    )
                    .map_err(|e| PlatformError::Other(e.to_string()))?
            }
            SampleFormat::I16 => {
                let ring_cb = ring.clone();
                let err_channels = channels;
                device
                    .build_input_stream(
                        &stream_config,
                        move |data: &[i16], _| {
                            let mut buf = ring_cb.lock().unwrap();
                            for &s in data {
                                buf.push(s as f32 / i16::MAX as f32);
                            }
                            if max_ring_samples > 0 && buf.len() > max_ring_samples {
                                let excess = buf.len() - max_ring_samples;
                                buf.drain(..excess);
                            }
                        },
                        move |e| eprintln!("Input stream error ({err_channels}ch): {e}"),
                        None,
                    )
                    .map_err(|e| PlatformError::Other(e.to_string()))?
            }
            _ => {
                return Err(PlatformError::Other(
                    "Unsupported input sample format".into(),
                ));
            }
        };

        stream
            .play()
            .map_err(|e| PlatformError::Other(e.to_string()))?;

        eprintln!(
            "Hardware capture opened: {} @ {} Hz ({} ch)",
            source.display_name, sample_rate, channels
        );

        Ok(Self {
            _stream: stream,
            ring,
            channels,
            sample_rate,
        })
    }

    pub fn read_interleaved(&mut self, out_stereo: &mut [f32]) -> usize {
        let out_frames = out_stereo.len() / CHANNELS;
        if out_frames == 0 {
            return 0;
        }

        if self.sample_rate == SAMPLE_RATE {
            return self.read_native_interleaved(out_stereo);
        }

        let in_frames_needed = native_frames_for_output(out_frames, self.sample_rate);
        let in_samples_needed = in_frames_needed * self.channels.max(1);

        let native = {
            let mut ring = self.ring.lock().unwrap();
            if ring.len() < in_samples_needed {
                out_stereo.fill(0.0);
                return 0;
            }
            ring.drain(..in_samples_needed).collect::<Vec<f32>>()
        };

        if self.channels == 1 {
            let mono = resample_mono(&native, self.sample_rate, out_frames);
            for (i, &s) in mono.iter().enumerate().take(out_frames) {
                out_stereo[i * 2] = s;
                out_stereo[i * 2 + 1] = s;
            }
        } else {
            let ch = self.channels.min(CHANNELS);
            let mut resampled_channels = Vec::with_capacity(ch);
            for c in 0..ch {
                let channel_samples: Vec<f32> = native
                    .iter()
                    .skip(c)
                    .step_by(self.channels)
                    .copied()
                    .collect();
                resampled_channels.push(resample_mono(
                    &channel_samples,
                    self.sample_rate,
                    out_frames,
                ));
            }
            for i in 0..out_frames {
                for c in 0..ch {
                    out_stereo[i * CHANNELS + c] = resampled_channels[c][i];
                }
                if ch < CHANNELS {
                    out_stereo[i * CHANNELS + 1] = 0.0;
                }
            }
        }

        out_frames
    }

    fn read_native_interleaved(&mut self, out_stereo: &mut [f32]) -> usize {
        let mut ring = self.ring.lock().unwrap();
        let frames_available = ring.len() / self.channels.max(1);
        let frames_needed = out_stereo.len() / CHANNELS;
        let frames = frames_available.min(frames_needed);

        if frames == 0 {
            out_stereo.fill(0.0);
            return 0;
        }

        if self.channels == 1 {
            for i in 0..frames {
                let s = ring[i];
                out_stereo[i * 2] = s;
                out_stereo[i * 2 + 1] = s;
            }
            ring.drain(..frames);
        } else {
            let samples = frames * self.channels.min(CHANNELS);
            out_stereo[..samples].copy_from_slice(&ring[..samples]);
            if frames < frames_needed {
                for frame in frames..frames_needed {
                    let base = frame * CHANNELS;
                    if base + 1 < out_stereo.len() {
                        out_stereo[base] = 0.0;
                        out_stereo[base + 1] = 0.0;
                    }
                }
            }
            ring.drain(..samples);
        }

        frames
    }
}

fn best_input_config(device: &cpal::Device) -> Result<cpal::SupportedStreamConfig, PlatformError> {
    use cpal::traits::DeviceTrait;

    let preferred = cpal::SampleRate(SAMPLE_RATE);
    let configs: Vec<_> = device
        .supported_input_configs()
        .map_err(|e| PlatformError::Other(e.to_string()))?
        .collect();

    if let Some(cfg) = configs
        .iter()
        .filter(|c| c.min_sample_rate() <= preferred && c.max_sample_rate() >= preferred)
        .max_by(|a, b| {
            a.channels()
                .cmp(&b.channels())
                .then(a.cmp_default_heuristics(b))
        })
        .map(|c| c.with_sample_rate(preferred))
    {
        return Ok(cfg);
    }

    device
        .default_input_config()
        .map_err(|e| PlatformError::Other(e.to_string()))
}

fn native_frames_for_output(out_frames: usize, input_rate: u32) -> usize {
    ((out_frames as f64) * input_rate as f64 / SAMPLE_RATE as f64).ceil() as usize
}

fn resample_mono(input: &[f32], input_rate: u32, out_frames: usize) -> Vec<f32> {
    if input.is_empty() || out_frames == 0 {
        return vec![0.0; out_frames];
    }

    let ratio = input_rate as f64 / SAMPLE_RATE as f64;
    (0..out_frames)
        .map(|i| {
            let src = i as f64 * ratio;
            let idx = src.floor() as usize;
            let frac = (src - idx as f64) as f32;
            let s0 = input.get(idx).copied().unwrap_or(0.0);
            let s1 = input.get(idx + 1).copied().unwrap_or(s0);
            s0 + frac * (s1 - s0)
        })
        .collect()
}

fn find_input_device(host: &cpal::Host, uid: &str) -> Result<cpal::Device, PlatformError> {
    if !can_capture_microphone_inputs() {
        return Err(PlatformError::Permission("Microphone".into()));
    }

    let key = uid.strip_prefix("hw-in:").unwrap_or(uid);
    let devices: Vec<_> = host
        .input_devices()
        .map_err(|e| PlatformError::Other(e.to_string()))?
        .collect();

    if let Some(device) = devices
        .into_iter()
        .find(|d| d.name().ok().as_deref() == Some(key))
    {
        return Ok(device);
    }

    #[cfg(target_os = "macos")]
    if let Some(name) = crate::capture::macos::hal_input_name_for_uid(key) {
        let devices: Vec<_> = host
            .input_devices()
            .map_err(|e| PlatformError::Other(e.to_string()))?
            .collect();
        if let Some(device) = devices
            .into_iter()
            .find(|d| d.name().ok().as_deref() == Some(name.as_str()))
        {
            return Ok(device);
        }
    }

    eprintln!("Hardware input not found for uid={uid:?}");
    Err(PlatformError::SourceUnavailable)
}

/// Monitor playback uses a smaller device buffer than capture/mixing for lower latency.
pub const MONITOR_OUTPUT_BUFFER_FRAMES: u32 = 128;
/// Fixed ring depth in output periods — backpressure instead of sample drops.
pub const MONITOR_RING_PERIODS: usize = 3;
/// Total monitor ring capacity in interleaved stereo samples.
pub const MONITOR_RING_CAPACITY: usize =
    MONITOR_OUTPUT_BUFFER_FRAMES as usize * CHANNELS * MONITOR_RING_PERIODS;

/// Drop oldest virtual-mic samples when the queue exceeds this (virtual mic tolerates drops).
const VIRTUAL_MIC_PENDING_MAX_SAMPLES: usize = 512 * CHANNELS * 4;

pub struct MonitorOutput {
    _stream: Stream,
    ring: Arc<MonitorRingBuffer>,
}

fn monitor_stream_config(device: &cpal::Device) -> Result<(StreamConfig, SampleFormat), PlatformError> {
    let preferred_rate = cpal::SampleRate(SAMPLE_RATE);

    let from_range = device
        .supported_output_configs()
        .ok()
        .and_then(|configs| {
            configs
                .filter(|c| {
                    c.channels() >= CHANNELS as u16
                        && c.min_sample_rate() <= preferred_rate
                        && c.max_sample_rate() >= preferred_rate
                })
                .max_by(|a, b| a.cmp_default_heuristics(b))
                .map(|range| range.with_sample_rate(preferred_rate))
        });

    let supported = if let Some(config) = from_range {
        config
    } else {
        device
            .default_output_config()
            .map_err(|e| PlatformError::Other(e.to_string()))?
    };

    let sample_format = supported.sample_format();
    let mut config: StreamConfig = supported.into();
    config.channels = CHANNELS as u16;
    config.sample_rate = preferred_rate;
    config.buffer_size = BufferSize::Fixed(MONITOR_OUTPUT_BUFFER_FRAMES);

    Ok((config, sample_format))
}

fn f32_to_i16(sample: f32) -> i16 {
    (sample.clamp(-1.0, 1.0) * 32767.0).round() as i16
}

impl MonitorOutput {
    pub fn open(device_uid: Option<&str>) -> Result<Self, PlatformError> {
        use cpal::traits::HostTrait;

        let host = cpal::default_host();
        let device = if let Some(uid) = device_uid {
            let name = uid.strip_prefix("hw-out:").unwrap_or(uid);
            host.output_devices()
                .map_err(|e| PlatformError::Other(e.to_string()))?
                .find(|d| d.name().ok().as_deref() == Some(name))
                .ok_or_else(|| PlatformError::Other("Monitor device not found".into()))?
        } else {
            host.default_output_device()
                .ok_or_else(|| PlatformError::Other("No default output".into()))?
        };

        let (config, sample_format) = monitor_stream_config(&device)?;
        let ring = MonitorRingBuffer::new(MONITOR_RING_CAPACITY);
        let ring_cb = ring.clone();

        let stream = match sample_format {
            SampleFormat::F32 => device
                .build_output_stream(
                    &config,
                    move |out: &mut [f32], _| {
                        let n = ring_cb.pop_slice(out);
                        out[n..].fill(0.0);
                    },
                    |e| eprintln!("Monitor error: {e}"),
                    None,
                )
                .map_err(|e| PlatformError::Other(e.to_string()))?,
            SampleFormat::I16 => {
                let ring_cb = ring.clone();
                device
                    .build_output_stream(
                        &config,
                        move |out: &mut [i16], _| {
                            let mut temp = [0.0_f32; MONITOR_RING_CAPACITY];
                            let len = out.len().min(temp.len());
                            let n = ring_cb.pop_slice(&mut temp[..len]);
                            for (dst, &sample) in out.iter_mut().zip(temp[..n].iter()) {
                                *dst = f32_to_i16(sample);
                            }
                            for dst in &mut out[n..] {
                                *dst = 0;
                            }
                        },
                        |e| eprintln!("Monitor error: {e}"),
                        None,
                    )
                    .map_err(|e| PlatformError::Other(e.to_string()))?
            }
            _ => {
                return Err(PlatformError::Other(
                    "Unsupported monitor output sample format".into(),
                ));
            }
        };

        stream
            .play()
            .map_err(|e| PlatformError::Other(e.to_string()))?;

        Ok(Self { _stream: stream, ring })
    }

    /// Push mixed monitor samples; blocks with yield until the ring accepts all samples.
    pub fn push_samples(&mut self, samples: &[f32]) {
        let mut offset = 0;
        while offset < samples.len() {
            let written = self.ring.push_slice(&samples[offset..]);
            if written == 0 {
                thread::yield_now();
            } else {
                offset += written;
            }
        }
    }
}

/// User-facing virtual microphone name exposed to other apps (CoreAudio aggregate device).
pub const VIRTUAL_MIC_DISPLAY_NAME: &str = "Input Mixer Channel";

/// True when the device name is the branded aggregate mic shown in app pickers.
pub fn name_is_branded_virtual_mic(name: &str) -> bool {
    name.eq_ignore_ascii_case(VIRTUAL_MIC_DISPLAY_NAME)
}

/// True when the device name matches a known virtual microphone backend or branded channel.
pub fn name_looks_like_virtual_mic(name: &str) -> bool {
    name_is_branded_virtual_mic(name) || name_looks_like_virtual_mic_backend(name)
}

/// BlackHole / Input Mixer HAL loopback — used for writing mixed audio (not the aggregate alias).
fn name_looks_like_virtual_mic_backend(name: &str) -> bool {
    let lower = name.to_ascii_lowercase();
    (lower.contains("input mixer") || lower.contains("inputmixer"))
        && !lower.contains("channel")
        || lower.contains("blackhole")
        || lower.contains("black hole")
}

#[cfg(target_os = "macos")]
fn list_input_device_names() -> Vec<String> {
    crate::capture::macos::hal_input_device_names()
}

#[cfg(not(target_os = "macos"))]
fn list_input_device_names() -> Vec<String> {
    let host = cpal::default_host();
    host.input_devices()
        .ok()
        .map(|devices| devices.filter_map(|d| d.name().ok()).collect())
        .unwrap_or_default()
}

#[cfg(target_os = "macos")]
fn list_output_device_names() -> Vec<String> {
    crate::capture::macos::hal_enumerate_output_devices()
        .into_iter()
        .map(|d| d.name)
        .collect()
}

#[cfg(not(target_os = "macos"))]
fn list_output_device_names() -> Vec<String> {
    let host = cpal::default_host();
    host.output_devices()
        .ok()
        .map(|devices| devices.filter_map(|d| d.name().ok()).collect())
        .unwrap_or_default()
}

/// True when CoreAudio exposes a virtual mic as an input or loopback output device.
pub fn detect_virtual_mic_enumerated() -> bool {
    let input_listed = list_input_device_names()
        .iter()
        .any(|n| name_looks_like_virtual_mic(n));

    let output_listed = list_output_device_names()
        .iter()
        .any(|n| name_looks_like_virtual_mic(n));

    input_listed || output_listed
}

/// First matching virtual mic device name from CoreAudio (input side, for user-facing labels).
pub fn detected_virtual_mic_input_name() -> Option<String> {
    let names = list_input_device_names();

    if names.iter().any(|n| name_is_branded_virtual_mic(n)) {
        return Some(VIRTUAL_MIC_DISPLAY_NAME.to_string());
    }

    names.into_iter().find(|n| name_looks_like_virtual_mic(n))
}

fn find_virtual_mic_output_device(host: &cpal::Host) -> Result<cpal::Device, PlatformError> {
    host.output_devices()
        .map_err(|e| PlatformError::Other(e.to_string()))?
        .find(|d| {
            d.name()
                .ok()
                .map(|n| {
                    !name_is_branded_virtual_mic(&n) && name_looks_like_virtual_mic_backend(&n)
                })
                .unwrap_or(false)
        })
        .ok_or(PlatformError::DriverMissing)
}

fn drain_pending_into_output(out: &mut [f32], pending: &mut Vec<f32>) {
    let len = out.len().min(pending.len());
    out[..len].copy_from_slice(&pending[..len]);
    if len < out.len() {
        out[len..].fill(0.0);
    }
    if pending.len() > len {
        pending.drain(..len);
    } else {
        pending.clear();
    }
}

fn push_pending_samples(pending: &Arc<Mutex<Vec<f32>>>, samples: &[f32]) {
    let mut pending = pending.lock().unwrap();
    pending.extend_from_slice(samples);
    if pending.len() > VIRTUAL_MIC_PENDING_MAX_SAMPLES {
        let drop = pending.len() - VIRTUAL_MIC_PENDING_MAX_SAMPLES;
        pending.drain(..drop);
    }
}

fn open_virtual_mic_output_stream(
    device: &cpal::Device,
) -> Result<(Stream, Arc<Mutex<Vec<f32>>>), PlatformError> {
    let preferred_rate = cpal::SampleRate(SAMPLE_RATE);

    let from_range = device
        .supported_output_configs()
        .ok()
        .and_then(|configs| {
            configs
                .filter(|c| {
                    c.channels() >= CHANNELS as u16
                        && c.min_sample_rate() <= preferred_rate
                        && c.max_sample_rate() >= preferred_rate
                })
                .max_by(|a, b| a.cmp_default_heuristics(b))
                .map(|range| range.with_sample_rate(preferred_rate))
        });

    let supported = if let Some(config) = from_range {
        config
    } else {
        device
            .default_output_config()
            .map_err(|e| PlatformError::Other(e.to_string()))?
    };

    let sample_format = supported.sample_format();
    let mut config: StreamConfig = supported.into();
    config.channels = CHANNELS as u16;
    config.sample_rate = preferred_rate;

    let pending = Arc::new(Mutex::new(Vec::with_capacity(
        VIRTUAL_MIC_PENDING_MAX_SAMPLES,
    )));
    let pending_cb = pending.clone();

    let stream = match sample_format {
        SampleFormat::F32 => device
            .build_output_stream(
                &config,
                move |out: &mut [f32], _| {
                    let mut src = pending_cb.lock().unwrap();
                    drain_pending_into_output(out, &mut src);
                },
                |e| eprintln!("Output stream error: {e}"),
                None,
            )
            .map_err(|e| PlatformError::Other(e.to_string()))?,
        SampleFormat::I16 => {
            let pending_cb = pending.clone();
            device
                .build_output_stream(
                    &config,
                    move |out: &mut [i16], _| {
                        let mut src = pending_cb.lock().unwrap();
                        let frames = out.len().min(src.len());
                        for (dst, &sample) in out.iter_mut().zip(src[..frames].iter()) {
                            *dst = f32_to_i16(sample);
                        }
                        for dst in &mut out[frames..] {
                            *dst = 0;
                        }
                        if src.len() > frames {
                            src.drain(..frames);
                        } else {
                            src.clear();
                        }
                    },
                    |e| eprintln!("Output stream error: {e}"),
                    None,
                )
                .map_err(|e| PlatformError::Other(e.to_string()))?
        }
        _ => {
            return Err(PlatformError::Other(
                "Unsupported virtual mic output sample format".into(),
            ));
        }
    };

    stream
        .play()
        .map_err(|e| PlatformError::Other(e.to_string()))?;

    Ok((stream, pending))
}

/// Routes mixed PCM to the virtual microphone loopback (e.g. BlackHole output → mic input).
pub struct VirtualMicOutput {
    _stream: Stream,
    pending: Arc<Mutex<Vec<f32>>>,
    device_name: String,
}

impl VirtualMicOutput {
    pub fn open() -> Result<Self, PlatformError> {
        let host = cpal::default_host();
        let device = find_virtual_mic_output_device(&host)?;
        let device_name = device
            .name()
            .unwrap_or_else(|_| "Virtual microphone".into());
        let (stream, pending) = open_virtual_mic_output_stream(&device)?;

        Ok(Self {
            _stream: stream,
            pending,
            device_name,
        })
    }

    pub fn device_name(&self) -> &str {
        &self.device_name
    }

    pub fn push_samples(&mut self, samples: &[f32]) {
        push_pending_samples(&self.pending, samples);
    }
}
