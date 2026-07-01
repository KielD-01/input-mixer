use crate::sources::SourceId;

#[derive(Debug, thiserror::Error)]
pub enum PlatformError {
    #[error("This feature is not supported on this platform")]
    Unsupported,
    #[error("Permission required: {0}")]
    Permission(String),
    #[error("Source unavailable")]
    SourceUnavailable,
    #[error("Driver not installed")]
    DriverMissing,
    #[error("{0}")]
    Other(String),
}

/// Reads application-captured audio into the mixer.
pub trait AppCapture: Send {
    fn read_samples(&mut self, buffer: &mut [f32]) -> usize;
    fn is_available(&self) -> bool;
}

/// Writes mixed PCM to virtual mic or monitor device.
pub trait VirtualOutput {
    fn write_samples(&mut self, buffer: &[f32]) -> Result<(), PlatformError>;
    fn device_uid(&self) -> &str;
}

/// Platform abstraction for application audio capture.
pub trait PlatformCapture: Send {
    fn scan_application_sources(&self) -> Result<Vec<SourceId>, PlatformError>;
    fn create_app_capture(&self, source: &SourceId) -> Result<Box<dyn AppCapture>, PlatformError>;
}

/// Platform abstraction for virtual microphone output.
pub trait PlatformVirtualOutput: Send {
    fn detect_virtual_mic(&self) -> bool;
    fn virtual_device_name(&self) -> &'static str;
    fn create_virtual_output(&self) -> Result<Box<dyn VirtualOutput>, PlatformError>;
}

pub fn platform_capture() -> Box<dyn PlatformCapture> {
    #[cfg(target_os = "macos")]
    {
        return Box::new(super::macos::MacPlatformCapture::new());
    }
    #[cfg(target_os = "windows")]
    {
        return Box::new(super::windows::WindowsPlatformCapture::new());
    }
    #[cfg(target_os = "linux")]
    {
        return Box::new(super::linux::LinuxPlatformCapture::new());
    }
    #[cfg(not(any(target_os = "macos", target_os = "windows", target_os = "linux")))]
    {
        struct Unsupported;
        impl PlatformCapture for Unsupported {
            fn scan_application_sources(&self) -> Result<Vec<SourceId>, PlatformError> {
                Err(PlatformError::Unsupported)
            }
            fn create_app_capture(
                &self,
                _source: &SourceId,
            ) -> Result<Box<dyn AppCapture>, PlatformError> {
                Err(PlatformError::Unsupported)
            }
        }
        Box::new(Unsupported)
    }
}

pub fn platform_virtual_output() -> Box<dyn PlatformVirtualOutput> {
    #[cfg(target_os = "macos")]
    {
        return Box::new(super::macos::MacVirtualOutput::new());
    }
    #[cfg(target_os = "windows")]
    {
        return Box::new(super::windows::WindowsVirtualOutput::new());
    }
    #[cfg(target_os = "linux")]
    {
        return Box::new(super::linux::LinuxVirtualOutput::new());
    }
    #[cfg(not(any(target_os = "macos", target_os = "windows", target_os = "linux")))]
    {
        struct Unsupported;
        impl PlatformVirtualOutput for Unsupported {
            fn detect_virtual_mic(&self) -> bool {
                false
            }
            fn virtual_device_name(&self) -> &'static str {
                "Input Mixer"
            }
            fn create_virtual_output(&self) -> Result<Box<dyn VirtualOutput>, PlatformError> {
                Err(PlatformError::Unsupported)
            }
        }
        Box::new(Unsupported)
    }
}
