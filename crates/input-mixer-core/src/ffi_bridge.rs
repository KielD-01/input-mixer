use std::sync::OnceLock;

use parking_lot::Mutex;

use crate::capture::{enumerate_hardware_inputs, enumerate_hardware_outputs};
use crate::capture::platform::platform_capture;
use crate::engine::MixerEngine;
use crate::sources::{
    ChannelConfig, EngineStatus, MasterConfig, MeterSnapshot, MonitorConfig, SourceDescriptor,
    SourceId, SourceKind, SourceRegistry, MAX_CHANNELS,
};

static ENGINE: OnceLock<Mutex<MixerEngine>> = OnceLock::new();

fn engine() -> &'static Mutex<MixerEngine> {
    ENGINE.get_or_init(|| Mutex::new(MixerEngine::new()))
}

pub fn core_init() {
    let _ = engine();
    #[cfg(target_os = "macos")]
    crate::capture::macos::ensure_virtual_mic_channel();
}

pub fn scan_hardware_inputs_ffi() -> Vec<SourceDescriptor> {
    let registry = engine().lock().registry();
    let reg = registry.read();
    let sources = enumerate_hardware_inputs();
    reg.mark_available(sources.iter())
}

pub fn scan_hardware_outputs_ffi() -> Vec<crate::sources::OutputDeviceDescriptor> {
    enumerate_hardware_outputs()
}

pub fn scan_application_sources_ffi() -> Vec<SourceDescriptor> {
    let registry = engine().lock().registry();
    let reg = registry.read();
    let platform = platform_capture();
    let sources = platform
        .scan_application_sources()
        .unwrap_or_default();
    reg.mark_available(sources.iter())
}

pub fn set_channels_ffi(channels: Vec<ChannelConfig>) -> Result<(), String> {
    engine().lock().set_channels(channels)
}

pub fn reopen_channel_ffi(index: u8) -> Result<(), String> {
    engine().lock().reopen_channel(index as usize)
}

pub fn set_master_ffi(config: MasterConfig) -> Result<(), String> {
    engine().lock().set_master(config)
}

pub fn set_monitor_ffi(config: MonitorConfig) -> Result<(), String> {
    engine().lock().set_monitor(config)
}

pub fn get_meter_levels_ffi() -> MeterSnapshot {
    engine().lock().meters()
}

pub fn get_engine_status_ffi() -> EngineStatus {
    engine().lock().status()
}

pub fn start_engine_ffi() -> Result<(), String> {
    engine().lock().start()
}

pub fn stop_engine_ffi() {
    engine().lock().stop();
}

pub fn detect_virtual_driver_ffi() -> bool {
    crate::capture::platform::platform_virtual_output().detect_virtual_mic()
}

#[cfg(target_os = "macos")]
pub fn virtual_mic_needs_audio_restart_ffi() -> bool {
    crate::capture::macos::virtual_mic_needs_audio_restart()
}

#[cfg(not(target_os = "macos"))]
pub fn virtual_mic_needs_audio_restart_ffi() -> bool {
    false
}

#[cfg(target_os = "macos")]
pub fn detected_virtual_mic_name_ffi() -> Option<String> {
    crate::capture::macos::detected_virtual_mic_name()
}

#[cfg(not(target_os = "macos"))]
pub fn detected_virtual_mic_name_ffi() -> Option<String> {
    None
}

#[cfg(target_os = "macos")]
pub fn virtual_mic_hal_on_disk_ffi() -> bool {
    crate::capture::macos::virtual_mic_hal_on_disk()
}

#[cfg(not(target_os = "macos"))]
pub fn virtual_mic_hal_on_disk_ffi() -> bool {
    false
}

#[cfg(target_os = "macos")]
pub fn set_microphone_prompted_ffi(prompted: bool) {
    crate::capture::macos::set_microphone_prompted(prompted);
}

#[cfg(not(target_os = "macos"))]
pub fn set_microphone_prompted_ffi(_prompted: bool) {}

#[cfg(target_os = "macos")]
pub fn should_request_microphone_access_ffi() -> bool {
    crate::capture::macos::should_request_microphone_access()
}

#[cfg(not(target_os = "macos"))]
pub fn should_request_microphone_access_ffi() -> bool {
    false
}

#[cfg(target_os = "macos")]
pub fn has_microphone_access_ffi() -> bool {
    crate::capture::macos::has_microphone_access()
}

#[cfg(not(target_os = "macos"))]
pub fn has_microphone_access_ffi() -> bool {
    true
}

#[cfg(target_os = "macos")]
pub fn is_microphone_access_denied_ffi() -> bool {
    crate::capture::macos::is_microphone_access_denied()
}

#[cfg(not(target_os = "macos"))]
pub fn is_microphone_access_denied_ffi() -> bool {
    false
}

#[cfg(target_os = "macos")]
pub fn request_microphone_access_ffi() -> bool {
    crate::capture::macos::request_microphone_access()
}

#[cfg(not(target_os = "macos"))]
pub fn request_microphone_access_ffi() -> bool {
    true
}

#[cfg(target_os = "macos")]
pub fn should_request_screen_capture_access_ffi() -> bool {
    crate::capture::macos::should_request_screen_capture_access()
}

#[cfg(not(target_os = "macos"))]
pub fn should_request_screen_capture_access_ffi() -> bool {
    false
}

#[cfg(target_os = "macos")]
pub fn has_screen_capture_access_ffi() -> bool {
    crate::capture::macos::has_screen_capture_access()
}

#[cfg(not(target_os = "macos"))]
pub fn has_screen_capture_access_ffi() -> bool {
    true
}

#[cfg(target_os = "macos")]
pub fn request_screen_capture_access_ffi() -> bool {
    crate::capture::macos::request_screen_capture_access()
}

#[cfg(not(target_os = "macos"))]
pub fn request_screen_capture_access_ffi() -> bool {
    true
}

#[cfg(target_os = "macos")]
pub fn screen_capture_access_state_ffi() -> i32 {
    crate::capture::macos::screen_capture_access_state()
}

#[cfg(not(target_os = "macos"))]
pub fn screen_capture_access_state_ffi() -> i32 {
    0
}

#[cfg(target_os = "macos")]
pub fn refresh_screen_capture_access_ffi() {
    crate::capture::macos::refresh_screen_capture_access();
}

#[cfg(not(target_os = "macos"))]
pub fn refresh_screen_capture_access_ffi() {}

#[cfg(target_os = "macos")]
pub fn is_adhoc_build_ffi() -> bool {
    crate::capture::macos::is_adhoc_build()
}

#[cfg(not(target_os = "macos"))]
pub fn is_adhoc_build_ffi() -> bool {
    false
}

pub fn source_id_from_ffi(
    kind: u8,
    hardware_uid: &str,
    app_bundle_id: &str,
    display_name: &str,
    subtitle: &str,
    window_id: u64,
    process_id: u32,
) -> SourceId {
    SourceId {
        kind: SourceKind::from_u8(kind),
        hardware_uid: hardware_uid.to_string(),
        app_bundle_id: app_bundle_id.to_string(),
        window_id,
        process_id,
        display_name: display_name.to_string(),
        subtitle: subtitle.to_string(),
    }
}

pub fn default_channels() -> Vec<ChannelConfig> {
    (0..MAX_CHANNELS).map(|_| ChannelConfig::default()).collect()
}
