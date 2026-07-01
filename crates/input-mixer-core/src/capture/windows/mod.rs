//! Windows WASAPI process loopback capture.

mod loopback;

use crate::sources::SourceId;

use super::platform::{AppCapture, PlatformCapture, PlatformError, PlatformVirtualOutput, VirtualOutput};

pub struct WindowsPlatformCapture;

impl WindowsPlatformCapture {
    pub fn new() -> Self {
        Self
    }
}

impl PlatformCapture for WindowsPlatformCapture {
    fn scan_application_sources(&self) -> Result<Vec<SourceId>, PlatformError> {
        loopback::enumerate_audio_processes()
    }

    fn create_app_capture(&self, source: &SourceId) -> Result<Box<dyn AppCapture>, PlatformError> {
        loopback::create_process_capture(source)
    }
}

pub struct WindowsVirtualOutput;

impl WindowsVirtualOutput {
    pub fn new() -> Self {
        Self
    }
}

impl PlatformVirtualOutput for WindowsVirtualOutput {
    fn detect_virtual_mic(&self) -> bool {
        cpal::default_host()
            .input_devices()
            .ok()
            .map(|devices| {
                devices
                    .filter_map(|d| d.name().ok())
                    .any(|n| n.contains("Input Mixer") || n.contains("SYSVAD"))
            })
            .unwrap_or(false)
    }

    fn virtual_device_name(&self) -> &'static str {
        "Input Mixer"
    }

    fn create_virtual_output(&self) -> Result<Box<dyn VirtualOutput>, PlatformError> {
        Ok(Box::new(DevVirtualOutput))
    }
}

struct DevVirtualOutput;

impl VirtualOutput for DevVirtualOutput {
    fn write_samples(&mut self, _buffer: &[f32]) -> Result<(), PlatformError> {
        Ok(())
    }

    fn device_uid(&self) -> &str {
        "input-mixer-virtual"
    }
}
