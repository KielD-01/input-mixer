#[cxx::bridge(namespace = "input_mixer")]
mod bridge {
    struct CxxSourceDescriptor {
        kind: u8,
        hardware_uid: String,
        app_bundle_id: String,
        display_name: String,
        subtitle: String,
        window_id: u64,
        process_id: u32,
        already_used: bool,
        available: bool,
    }

    struct CxxOutputDevice {
        uid: String,
        name: String,
    }

    struct CxxChannelConfig {
        has_source: bool,
        kind: u8,
        hardware_uid: String,
        app_bundle_id: String,
        display_name: String,
        subtitle: String,
        window_id: u64,
        process_id: u32,
        volume: f32,
        muted: bool,
    }

    struct CxxMasterConfig {
        volume: f32,
        muted: bool,
    }

    struct CxxMonitorConfig {
        enabled: bool,
        output_device_uid: String,
        volume: f32,
    }

    struct CxxMeterSnapshot {
        channel_input_peaks: Vec<f32>,
        channel_output_peaks: Vec<f32>,
        channel_unavailable: Vec<bool>,
        master_peak: f32,
        monitor_peak: f32,
    }

    extern "Rust" {
        fn im_core_init();
        fn im_scan_hardware_inputs() -> Vec<CxxSourceDescriptor>;
        fn im_scan_hardware_outputs() -> Vec<CxxOutputDevice>;
        fn im_scan_application_sources() -> Vec<CxxSourceDescriptor>;
        fn im_set_channels(channels: Vec<CxxChannelConfig>) -> String;
        fn im_reopen_channel(index: u8) -> String;
        fn im_set_master(config: CxxMasterConfig) -> String;
        fn im_set_monitor(config: CxxMonitorConfig) -> String;
        fn im_get_meter_levels() -> CxxMeterSnapshot;
        fn im_get_engine_status() -> u8;
        fn im_start_engine() -> String;
        fn im_stop_engine();
        fn im_detect_virtual_driver() -> bool;
        fn im_virtual_mic_needs_audio_restart() -> bool;
        fn im_virtual_mic_hal_on_disk() -> bool;
        fn im_detected_virtual_mic_name() -> String;
        fn im_has_microphone_access() -> bool;
        fn im_is_microphone_access_denied() -> bool;
        fn im_should_request_microphone_access() -> bool;
        fn im_set_microphone_prompted(prompted: bool);
        fn im_request_microphone_access() -> bool;
        fn im_has_screen_capture_access() -> bool;
        fn im_should_request_screen_capture_access() -> bool;
        fn im_request_screen_capture_access() -> bool;
        fn im_screen_capture_access_state() -> i32;
        fn im_refresh_screen_capture_access();
        fn im_is_adhoc_build() -> bool;
    }
}

use bridge::{
    CxxChannelConfig, CxxMasterConfig, CxxMeterSnapshot, CxxMonitorConfig, CxxOutputDevice,
    CxxSourceDescriptor,
};

use crate::ffi_bridge;
use crate::sources::{ChannelConfig, MasterConfig, MonitorConfig, SourceId, SourceKind};

fn to_cxx_source(d: crate::sources::SourceDescriptor) -> CxxSourceDescriptor {
    CxxSourceDescriptor {
        kind: d.id.kind.as_u8(),
        hardware_uid: d.id.hardware_uid,
        app_bundle_id: d.id.app_bundle_id,
        display_name: d.id.display_name,
        subtitle: d.id.subtitle,
        window_id: d.id.window_id,
        process_id: d.id.process_id,
        already_used: d.already_used,
        available: d.available,
    }
}

fn from_cxx_channel(c: &CxxChannelConfig) -> ChannelConfig {
    let source = if c.has_source {
        Some(SourceId {
            kind: SourceKind::from_u8(c.kind),
            hardware_uid: c.hardware_uid.clone(),
            app_bundle_id: c.app_bundle_id.clone(),
            window_id: c.window_id,
            process_id: c.process_id,
            display_name: c.display_name.clone(),
            subtitle: c.subtitle.clone(),
        })
    } else {
        None
    };
    ChannelConfig {
        source,
        volume: c.volume,
        muted: c.muted,
    }
}

fn im_core_init() {
    ffi_bridge::core_init();
}

fn im_scan_hardware_inputs() -> Vec<CxxSourceDescriptor> {
    ffi_bridge::scan_hardware_inputs_ffi()
        .into_iter()
        .map(to_cxx_source)
        .collect()
}

fn im_scan_hardware_outputs() -> Vec<CxxOutputDevice> {
    ffi_bridge::scan_hardware_outputs_ffi()
        .into_iter()
        .map(|d| CxxOutputDevice {
            uid: d.uid,
            name: d.name,
        })
        .collect()
}

fn im_scan_application_sources() -> Vec<CxxSourceDescriptor> {
    ffi_bridge::scan_application_sources_ffi()
        .into_iter()
        .map(to_cxx_source)
        .collect()
}

fn im_set_channels(channels: Vec<CxxChannelConfig>) -> String {
    let configs: Vec<ChannelConfig> = channels.iter().map(from_cxx_channel).collect();
    ffi_bridge::set_channels_ffi(configs).err().unwrap_or_default()
}

fn im_reopen_channel(index: u8) -> String {
    ffi_bridge::reopen_channel_ffi(index).err().unwrap_or_default()
}

fn im_set_master(config: CxxMasterConfig) -> String {
    ffi_bridge::set_master_ffi(MasterConfig {
        volume: config.volume,
        muted: config.muted,
    })
    .err()
    .unwrap_or_default()
}

fn im_set_monitor(config: CxxMonitorConfig) -> String {
    let uid = if config.output_device_uid.is_empty() {
        None
    } else {
        Some(config.output_device_uid)
    };
    ffi_bridge::set_monitor_ffi(MonitorConfig {
        enabled: config.enabled,
        output_device_uid: uid,
        volume: config.volume,
    })
    .err()
    .unwrap_or_default()
}

fn im_get_meter_levels() -> CxxMeterSnapshot {
    let m = ffi_bridge::get_meter_levels_ffi();
    CxxMeterSnapshot {
        channel_input_peaks: m.channel_input_peaks,
        channel_output_peaks: m.channel_output_peaks,
        channel_unavailable: m.channel_unavailable,
        master_peak: m.master_peak,
        monitor_peak: m.monitor_peak,
    }
}

fn im_get_engine_status() -> u8 {
    ffi_bridge::get_engine_status_ffi().as_u8()
}

fn im_start_engine() -> String {
    ffi_bridge::start_engine_ffi().err().unwrap_or_default()
}

fn im_stop_engine() {
    ffi_bridge::stop_engine_ffi();
}

fn im_detect_virtual_driver() -> bool {
    ffi_bridge::detect_virtual_driver_ffi()
}

fn im_virtual_mic_needs_audio_restart() -> bool {
    ffi_bridge::virtual_mic_needs_audio_restart_ffi()
}

fn im_virtual_mic_hal_on_disk() -> bool {
    ffi_bridge::virtual_mic_hal_on_disk_ffi()
}

fn im_detected_virtual_mic_name() -> String {
    ffi_bridge::detected_virtual_mic_name_ffi().unwrap_or_default()
}

fn im_has_microphone_access() -> bool {
    ffi_bridge::has_microphone_access_ffi()
}

fn im_is_microphone_access_denied() -> bool {
    ffi_bridge::is_microphone_access_denied_ffi()
}

fn im_should_request_microphone_access() -> bool {
    ffi_bridge::should_request_microphone_access_ffi()
}

fn im_set_microphone_prompted(prompted: bool) {
    ffi_bridge::set_microphone_prompted_ffi(prompted);
}

fn im_request_microphone_access() -> bool {
    ffi_bridge::request_microphone_access_ffi()
}

fn im_has_screen_capture_access() -> bool {
    ffi_bridge::has_screen_capture_access_ffi()
}

fn im_should_request_screen_capture_access() -> bool {
    ffi_bridge::should_request_screen_capture_access_ffi()
}

fn im_request_screen_capture_access() -> bool {
    ffi_bridge::request_screen_capture_access_ffi()
}

fn im_screen_capture_access_state() -> i32 {
    ffi_bridge::screen_capture_access_state_ffi()
}

fn im_refresh_screen_capture_access() {
    ffi_bridge::refresh_screen_capture_access_ffi();
}

fn im_is_adhoc_build() -> bool {
    ffi_bridge::is_adhoc_build_ffi()
}
