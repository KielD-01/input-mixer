use std::sync::atomic::{AtomicU8, Ordering};
use std::sync::Arc;
use std::thread::{self, JoinHandle};
use std::time::{Duration, Instant};

use crossbeam_channel::{bounded, Receiver, Sender};
use parking_lot::RwLock;

use crate::capture::hardware::{HardwareCapture, MonitorOutput};
use crate::capture::platform::{platform_capture, platform_virtual_output, AppCapture};
use crate::sources::{
    ChannelConfig, EngineStatus, MasterConfig, MeterSnapshot, MonitorConfig, SourceId,
    SourceKind, SourceRegistry, CHANNELS, MAX_CHANNELS, SAMPLE_RATE,
};

use super::limiter;
use super::{apply_volume, mix_add, peak_level};

/// Mixing block size for capture and future virtual-mic output (stability).
const BUFFER_FRAMES: usize = 512;
/// Smaller engine iteration when monitor is active — lowers end-to-end monitor latency.
const MONITOR_LOOP_FRAMES: usize = 128;

enum EngineCommand {
    SetChannels(Vec<ChannelConfig>),
    ReopenChannel(usize),
    SetMaster(MasterConfig),
    SetMonitor(MonitorConfig),
    Stop,
}

struct ChannelRuntime {
    config: ChannelConfig,
    hardware: Option<HardwareCapture>,
    app_capture: Option<Box<dyn AppCapture>>,
    scratch: Vec<f32>,
    input_peak: f32,
    output_peak: f32,
    unavailable: bool,
    capture_failed: bool,
}

pub struct MixerEngine {
    command_tx: Sender<EngineCommand>,
    thread: Option<JoinHandle<()>>,
    meters: Arc<RwLock<MeterSnapshot>>,
    status: Arc<AtomicU8>,
    registry: Arc<RwLock<SourceRegistry>>,
}

impl MixerEngine {
    pub fn new() -> Self {
        let (tx, rx) = bounded(32);
        let meters = Arc::new(RwLock::new(MeterSnapshot::default()));
        let status = Arc::new(AtomicU8::new(EngineStatus::Stopped.as_u8()));
        let registry = Arc::new(RwLock::new(SourceRegistry::new()));

        let meters_thread = meters.clone();
        let status_thread = status.clone();
        let registry_thread = registry.clone();

        let thread = thread::Builder::new()
            .name("input-mixer-engine".into())
            .spawn(move || {
                engine_thread(rx, meters_thread, status_thread, registry_thread);
            })
            .ok();

        Self {
            command_tx: tx,
            thread,
            meters,
            status,
            registry,
        }
    }

    pub fn status(&self) -> EngineStatus {
        EngineStatus::from_u8(self.status.load(Ordering::Relaxed))
    }

    pub fn meters(&self) -> MeterSnapshot {
        self.meters.read().clone()
    }

    pub fn registry(&self) -> Arc<RwLock<SourceRegistry>> {
        self.registry.clone()
    }

    pub fn set_channels(&self, channels: Vec<ChannelConfig>) -> Result<(), String> {
        let sources: Vec<Option<SourceId>> = channels.iter().map(|c| c.source.clone()).collect();
        SourceRegistry::validate_unique(&sources)?;
        self.command_tx
            .send(EngineCommand::SetChannels(channels))
            .map_err(|e| e.to_string())
    }

    pub fn reopen_channel(&self, index: usize) -> Result<(), String> {
        if index >= MAX_CHANNELS {
            return Err(format!("Invalid channel index: {index}"));
        }
        self.command_tx
            .send(EngineCommand::ReopenChannel(index))
            .map_err(|e| e.to_string())
    }

    pub fn set_master(&self, config: MasterConfig) -> Result<(), String> {
        self.command_tx
            .send(EngineCommand::SetMaster(config))
            .map_err(|e| e.to_string())
    }

    pub fn set_monitor(&self, config: MonitorConfig) -> Result<(), String> {
        self.command_tx
            .send(EngineCommand::SetMonitor(config))
            .map_err(|e| e.to_string())
    }

    pub fn start(&self) -> Result<(), String> {
        self.status
            .store(EngineStatus::Ready.as_u8(), Ordering::Relaxed);
        Ok(())
    }

    pub fn stop(&self) {
        let _ = self.command_tx.send(EngineCommand::Stop);
        self.status
            .store(EngineStatus::Stopped.as_u8(), Ordering::Relaxed);
    }
}

impl Default for MixerEngine {
    fn default() -> Self {
        Self::new()
    }
}

impl Drop for MixerEngine {
    fn drop(&mut self) {
        self.stop();
        if let Some(handle) = self.thread.take() {
            let _ = handle.join();
        }
    }
}

fn engine_thread(
    rx: Receiver<EngineCommand>,
    meters: Arc<RwLock<MeterSnapshot>>,
    status: Arc<AtomicU8>,
    registry: Arc<RwLock<SourceRegistry>>,
) {
    let platform_cap = platform_capture();
    let platform_out = platform_virtual_output();

    let driver_present = platform_out.detect_virtual_mic();
    if !driver_present {
        status.store(EngineStatus::DriverMissing.as_u8(), Ordering::Relaxed);
    } else {
        status.store(EngineStatus::Ready.as_u8(), Ordering::Relaxed);
    }

    let mut virtual_out = if driver_present {
        platform_out.create_virtual_output().ok()
    } else {
        None
    };

    let mut channels: Vec<ChannelRuntime> = (0..MAX_CHANNELS)
        .map(|_| ChannelRuntime {
            config: ChannelConfig::default(),
            hardware: None,
            app_capture: None,
            scratch: vec![0.0; BUFFER_FRAMES * CHANNELS],
            input_peak: 0.0,
            output_peak: 0.0,
            unavailable: false,
            capture_failed: false,
        })
        .collect();

    let mut master = MasterConfig::default();
    let mut monitor_cfg = MonitorConfig::default();
    let mut monitor_out: Option<MonitorOutput> = None;
    let mut running = true;

    let mut mix_buf = vec![0.0_f32; BUFFER_FRAMES * CHANNELS];
    let mut master_buf = vec![0.0_f32; BUFFER_FRAMES * CHANNELS];
    let mut monitor_buf = vec![0.0_f32; BUFFER_FRAMES * CHANNELS];

    while running {
        while let Ok(cmd) = rx.try_recv() {
            match cmd {
                EngineCommand::SetChannels(cfgs) => {
                    let sources: Vec<Option<SourceId>> =
                        cfgs.iter().map(|c| c.source.clone()).collect();
                    if SourceRegistry::validate_unique(&sources).is_ok() {
                        registry.write().assign(&sources);
                        for (i, cfg) in cfgs.into_iter().enumerate().take(MAX_CHANNELS) {
                            let needs_reopen =
                                !capture_sources_equivalent(&channels[i].config.source, &cfg.source);
                            channels[i].config = cfg.clone();
                            if needs_reopen {
                                reopen_channel(
                                    &mut channels[i],
                                    &cfg,
                                    platform_cap.as_ref(),
                                    monitor_cfg.enabled,
                                );
                            }
                        }
                    }
                }
                EngineCommand::ReopenChannel(index) => {
                    if index < MAX_CHANNELS && channels[index].config.source.is_some() {
                        let cfg = channels[index].config.clone();
                        reopen_channel(
                            &mut channels[index],
                            &cfg,
                            platform_cap.as_ref(),
                            monitor_cfg.enabled,
                        );
                    }
                }
                EngineCommand::SetMaster(m) => master = m,
                EngineCommand::SetMonitor(m) => {
                    let prev_enabled = monitor_cfg.enabled;
                    monitor_cfg = m.clone();
                    monitor_out = if m.enabled {
                        MonitorOutput::open(m.output_device_uid.as_deref()).ok()
                    } else {
                        None
                    };
                    if prev_enabled != m.enabled {
                        for ch in channels.iter_mut() {
                            if ch.config.source.is_some() {
                                let cfg = ch.config.clone();
                                reopen_channel(ch, &cfg, platform_cap.as_ref(), m.enabled);
                            }
                        }
                    }
                }
                EngineCommand::Stop => running = false,
            }
        }

        if !running {
            break;
        }

        let loop_start = Instant::now();
        let loop_frames = if monitor_cfg.enabled {
            MONITOR_LOOP_FRAMES
        } else {
            BUFFER_FRAMES
        };
        let loop_duration =
            Duration::from_micros((loop_frames as f64 / SAMPLE_RATE as f64 * 1_000_000.0) as u64);
        let loop_samples = loop_frames * CHANNELS;
        let mix_slice = &mut mix_buf[..loop_samples];
        let master_slice = &mut master_buf[..loop_samples];
        let monitor_slice = &mut monitor_buf[..loop_samples];

        mix_slice.fill(0.0);

        for ch in channels.iter_mut() {
            let scratch = &mut ch.scratch[..loop_samples];
            scratch.fill(0.0);

            if ch.config.source.is_none() {
                ch.input_peak = 0.0;
                ch.output_peak = 0.0;
                ch.unavailable = false;
                continue;
            }

            let frames = if let Some(ref mut hw) = ch.hardware {
                hw.read_interleaved(scratch)
            } else if let Some(ref mut app) = ch.app_capture {
                let read = app.read_samples(scratch);
                read / CHANNELS
            } else {
                0
            };

            let _ = frames;

            ch.unavailable = ch.capture_failed;
            if let Some(ref mut app) = ch.app_capture {
                if !app.is_available() {
                    ch.unavailable = true;
                }
            }

            ch.input_peak = peak_level(scratch);

            if !ch.config.muted {
                apply_volume(scratch, ch.config.volume.clamp(0.0, 1.0));
                ch.output_peak = peak_level(scratch);
                mix_add(mix_slice, scratch);
            } else {
                ch.output_peak = 0.0;
            }
        }

        master_slice.copy_from_slice(mix_slice);
        if !master.muted {
            apply_volume(master_slice, master.volume.clamp(0.0, 1.0));
        } else {
            master_slice.fill(0.0);
        }

        let master_peak = peak_level(master_slice);
        limiter::limit(master_slice, 0.95);

        if let Some(ref mut out) = virtual_out {
            let _ = out.write_samples(master_slice);
        }

        if monitor_cfg.enabled {
            monitor_slice.copy_from_slice(master_slice);
            apply_volume(monitor_slice, monitor_cfg.volume.clamp(0.0, 1.0));
            if let Some(ref mut out) = monitor_out {
                out.push_samples(monitor_slice);
            }
        }

        {
            let mut m = meters.write();
            m.channel_input_peaks = channels.iter().map(|c| c.input_peak).collect();
            m.channel_output_peaks = channels.iter().map(|c| c.output_peak).collect();
            m.channel_unavailable = channels.iter().map(|c| c.unavailable).collect();
            m.master_peak = master_peak;
            m.monitor_peak = if monitor_cfg.enabled {
                peak_level(monitor_slice)
            } else {
                0.0
            };
        }

        if status.load(Ordering::Relaxed) != EngineStatus::DriverMissing.as_u8() {
            status.store(EngineStatus::Running.as_u8(), Ordering::Relaxed);
        }

        let elapsed = loop_start.elapsed();
        if elapsed < loop_duration {
            thread::sleep(loop_duration - elapsed);
        }
    }

    status.store(EngineStatus::Stopped.as_u8(), Ordering::Relaxed);
}

/// Same device identity as UI matching — avoids tearing down capture when scan
/// normalizes UIDs or display strings without changing the underlying source.
fn capture_sources_equivalent(a: &Option<SourceId>, b: &Option<SourceId>) -> bool {
    match (a, b) {
        (None, None) => true,
        (None, Some(_)) | (Some(_), None) => false,
        (Some(a), Some(b)) => {
            if a.kind != b.kind {
                return false;
            }
            match a.kind {
                SourceKind::Hardware => {
                    (!a.hardware_uid.is_empty() && a.hardware_uid == b.hardware_uid)
                        || (!a.display_name.is_empty() && a.display_name == b.display_name)
                }
                SourceKind::Application => {
                    (a.window_id != 0 && a.window_id == b.window_id)
                        || (!a.app_bundle_id.is_empty() && a.app_bundle_id == b.app_bundle_id)
                        || (!a.display_name.is_empty() && a.display_name == b.display_name)
                }
            }
        }
    }
}

fn reopen_channel(
    runtime: &mut ChannelRuntime,
    config: &ChannelConfig,
    platform_cap: &dyn crate::capture::platform::PlatformCapture,
    low_latency: bool,
) {
    runtime.hardware = None;
    runtime.app_capture = None;
    runtime.unavailable = false;
    runtime.capture_failed = false;

    let Some(ref source) = config.source else {
        return;
    };

    match source.kind {
        SourceKind::Hardware => {
            match HardwareCapture::open_with_options(source, low_latency) {
                Ok(cap) => runtime.hardware = Some(cap),
                Err(e) => {
                    eprintln!(
                        "Hardware capture failed for {}: {e}",
                        source.display_name
                    );
                    runtime.capture_failed = true;
                    runtime.unavailable = true;
                }
            }
        }
        SourceKind::Application => {
            match platform_cap.create_app_capture(source) {
                Ok(cap) => runtime.app_capture = Some(cap),
                Err(e) => {
                    eprintln!(
                        "Application capture failed for {}: {e}",
                        source.display_name
                    );
                    runtime.capture_failed = true;
                    runtime.unavailable = true;
                }
            }
        }
    }
}
