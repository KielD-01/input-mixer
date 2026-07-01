//! Input Mixer — Rust audio core.

pub mod capture;
pub mod engine;
mod ffi_bridge;
pub mod ffi;
pub mod sources;

pub use engine::MixerEngine;
pub use sources::{ChannelConfig, MasterConfig, MonitorConfig, SourceDescriptor, SourceId, SourceKind};
