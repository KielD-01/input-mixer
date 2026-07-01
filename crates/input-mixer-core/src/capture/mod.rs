pub mod hardware;
mod monitor_ring;
pub mod platform;

#[cfg(target_os = "linux")]
pub mod linux;

#[cfg(target_os = "macos")]
pub mod macos;

#[cfg(target_os = "windows")]
pub mod windows;

pub use hardware::{enumerate_hardware_inputs, enumerate_hardware_outputs, HardwareCapture};
pub use platform::{AppCapture, PlatformError, VirtualOutput};
