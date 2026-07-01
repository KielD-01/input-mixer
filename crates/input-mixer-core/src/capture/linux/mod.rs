//! Linux PipeWire support — Phase E implementation with virtual mic via null sink.

mod pipewire;

use crate::sources::SourceId;

use super::platform::{AppCapture, PlatformCapture, PlatformError, PlatformVirtualOutput, VirtualOutput};

pub struct LinuxPlatformCapture;

impl LinuxPlatformCapture {
    pub fn new() -> Self {
        Self
    }
}

impl PlatformCapture for LinuxPlatformCapture {
    fn scan_application_sources(&self) -> Result<Vec<SourceId>, PlatformError> {
        pipewire::enumerate_application_nodes()
    }

    fn create_app_capture(&self, source: &SourceId) -> Result<Box<dyn AppCapture>, PlatformError> {
        pipewire::create_node_capture(source)
    }
}

pub struct LinuxVirtualOutput;

impl LinuxVirtualOutput {
    pub fn new() -> Self {
        Self
    }
}

impl PlatformVirtualOutput for LinuxVirtualOutput {
    fn detect_virtual_mic(&self) -> bool {
        pipewire::detect_virtual_mic()
    }

    fn virtual_device_name(&self) -> &'static str {
        "Input Mixer"
    }

    fn create_virtual_output(&self) -> Result<Box<dyn VirtualOutput>, PlatformError> {
        pipewire::create_virtual_output()
    }
}
