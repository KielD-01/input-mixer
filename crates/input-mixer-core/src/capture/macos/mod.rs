//! macOS ScreenCaptureKit application audio capture.

mod hal;
mod sck;

pub(crate) use hal::{
    enumerate_input_devices as hal_enumerate_input_devices,
    enumerate_output_devices as hal_enumerate_output_devices,
    input_device_names as hal_input_device_names,
    input_name_for_uid as hal_input_name_for_uid,
};

pub use sck::{
    has_microphone_access, has_screen_capture_access, is_adhoc_build, is_microphone_access_denied,
    refresh_screen_capture_access, request_microphone_access, request_screen_capture_access,
    screen_capture_access_state, set_microphone_prompted, should_request_microphone_access,
    should_request_screen_capture_access,
};

use crate::sources::SourceId;

use super::hardware::{
    detect_virtual_mic_enumerated, detected_virtual_mic_input_name, VIRTUAL_MIC_DISPLAY_NAME,
    VirtualMicOutput,
};

extern "C" {
    fn input_mixer_ensure_channel_aggregate() -> i32;
}

/// Creates the CoreAudio aggregate device that exposes `VIRTUAL_MIC_DISPLAY_NAME` to other apps.
pub fn ensure_virtual_mic_channel() {
    if !has_microphone_access() {
        return;
    }
    unsafe {
        input_mixer_ensure_channel_aggregate();
    }
}
use super::platform::{AppCapture, PlatformCapture, PlatformError, PlatformVirtualOutput, VirtualOutput};

pub struct MacPlatformCapture;

impl MacPlatformCapture {
    pub fn new() -> Self {
        Self
    }
}

impl PlatformCapture for MacPlatformCapture {
    fn scan_application_sources(&self) -> Result<Vec<SourceId>, PlatformError> {
        sck::enumerate_application_sources()
    }

    fn create_app_capture(&self, source: &SourceId) -> Result<Box<dyn AppCapture>, PlatformError> {
        sck::create_capture(source)
    }
}

pub struct MacVirtualOutput;

impl MacVirtualOutput {
    pub fn new() -> Self {
        Self
    }
}

pub fn virtual_mic_hal_on_disk() -> bool {
    const HAL_PATHS: &[&str] = &[
        "/Library/Audio/Plug-Ins/HAL/BlackHole2ch.driver",
        "/Library/Audio/Plug-Ins/HAL/BlackHole16ch.driver",
        "/Library/Audio/Plug-Ins/HAL/BlackHole64ch.driver",
        "/Library/Audio/Plug-Ins/HAL/BlackHole.driver",
        "/Library/Audio/Plug-Ins/HAL/InputMixer.driver",
    ];
    HAL_PATHS.iter().any(|path| std::path::Path::new(path).exists())
}

impl PlatformVirtualOutput for MacVirtualOutput {
    fn detect_virtual_mic(&self) -> bool {
        ensure_virtual_mic_channel();
        detect_virtual_mic_enumerated()
    }

    fn virtual_device_name(&self) -> &'static str {
        VIRTUAL_MIC_DISPLAY_NAME
    }

    fn create_virtual_output(&self) -> Result<Box<dyn VirtualOutput>, PlatformError> {
        let sink = VirtualMicOutput::open()?;
        Ok(Box::new(CpalVirtualMicOutput { sink }))
    }
}

struct CpalVirtualMicOutput {
    sink: VirtualMicOutput,
}

impl VirtualOutput for CpalVirtualMicOutput {
    fn write_samples(&mut self, buffer: &[f32]) -> Result<(), PlatformError> {
        self.sink.push_samples(buffer);
        Ok(())
    }

    fn device_uid(&self) -> &str {
        "virtual-mic-out"
    }
}

/// User-facing mic name when enumerated (e.g. "Input Mixer Channel"), else None.
pub fn detected_virtual_mic_name() -> Option<String> {
    ensure_virtual_mic_channel();
    detected_virtual_mic_input_name()
        .or_else(|| {
            if detect_virtual_mic_enumerated() {
                Some(VIRTUAL_MIC_DISPLAY_NAME.to_string())
            } else {
                None
            }
        })
}

/// HAL driver files exist but CoreAudio has not loaded the virtual mic yet.
pub fn virtual_mic_needs_audio_restart() -> bool {
    virtual_mic_hal_on_disk() && !detect_virtual_mic_enumerated()
}
