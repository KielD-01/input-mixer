#include "RustBridge.h"

#include "CxxString.h"

namespace input_mixer
{

void RustBridge::initialise()
{
    im_core_init();
}

void RustBridge::shutdown()
{
    im_stop_engine();
}

std::vector<SourceItem> RustBridge::scanHardwareInputs()
{
    std::vector<SourceItem> items;
    for (const auto& d : im_scan_hardware_inputs())
        items.push_back({ d });
    return items;
}

std::vector<OutputDeviceItem> RustBridge::scanHardwareOutputs()
{
    std::vector<OutputDeviceItem> items;
    for (const auto& d : im_scan_hardware_outputs())
        items.push_back({ toJuceString(d.uid), toJuceString(d.name) });
    return items;
}

std::vector<SourceItem> RustBridge::scanApplicationSources()
{
    std::vector<SourceItem> items;
    for (const auto& d : im_scan_application_sources())
        items.push_back({ d });
    return items;
}

rust::Vec<input_mixer::CxxChannelConfig> RustBridge::toCxxChannels(const std::vector<ChannelState>& channels)
{
    rust::Vec<input_mixer::CxxChannelConfig> result;
    result.reserve(channels.size());

    for (const auto& ch : channels)
    {
        input_mixer::CxxChannelConfig cfg;
        cfg.has_source = ch.hasSource;
        cfg.kind = ch.source.kind;
        cfg.hardware_uid = ch.source.hardware_uid;
        cfg.app_bundle_id = ch.source.app_bundle_id;
        cfg.display_name = ch.source.display_name;
        cfg.subtitle = ch.source.subtitle;
        cfg.window_id = ch.source.window_id;
        cfg.process_id = ch.source.process_id;
        cfg.volume = ch.volume;
        cfg.muted = ch.muted;
        result.push_back(cfg);
    }

    return result;
}

juce::String RustBridge::setChannels(const std::vector<ChannelState>& channels)
{
    return toJuceString(im_set_channels(toCxxChannels(channels)));
}

juce::String RustBridge::reopenChannel(int channelIndex)
{
    return toJuceString(im_reopen_channel(static_cast<uint8_t>(channelIndex)));
}

juce::String RustBridge::setMaster(float volume, bool muted)
{
    input_mixer::CxxMasterConfig cfg;
    cfg.volume = volume;
    cfg.muted = muted;
    return toJuceString(im_set_master(cfg));
}

juce::String RustBridge::setMonitor(bool enabled, const juce::String& deviceUid, float volume)
{
    input_mixer::CxxMonitorConfig cfg;
    cfg.enabled = enabled;
    cfg.output_device_uid = deviceUid.toStdString();
    cfg.volume = volume;
    return toJuceString(im_set_monitor(cfg));
}

input_mixer::CxxMeterSnapshot RustBridge::getMeterLevels()
{
    return im_get_meter_levels();
}

uint8_t RustBridge::getEngineStatus()
{
    return im_get_engine_status();
}

bool RustBridge::detectVirtualDriver()
{
    return im_detect_virtual_driver();
}

bool RustBridge::virtualMicNeedsAudioRestart()
{
    return im_virtual_mic_needs_audio_restart();
}

bool RustBridge::virtualMicHalOnDisk()
{
    return im_virtual_mic_hal_on_disk();
}

juce::String RustBridge::detectedVirtualMicName()
{
    return toJuceString(im_detected_virtual_mic_name());
}

bool RustBridge::hasMicrophoneAccess()
{
    return im_has_microphone_access();
}

bool RustBridge::isMicrophoneAccessDenied()
{
    return im_is_microphone_access_denied();
}

bool RustBridge::shouldRequestMicrophoneAccess()
{
    return im_should_request_microphone_access();
}

void RustBridge::setMicrophonePrompted(bool prompted)
{
    im_set_microphone_prompted(prompted);
}

bool RustBridge::requestMicrophoneAccess()
{
    return im_request_microphone_access();
}

bool RustBridge::hasScreenCaptureAccess()
{
    return im_has_screen_capture_access();
}

bool RustBridge::shouldRequestScreenCaptureAccess()
{
    return im_should_request_screen_capture_access();
}

bool RustBridge::requestScreenCaptureAccess()
{
    return im_request_screen_capture_access();
}

int RustBridge::screenCaptureAccessState()
{
    return im_screen_capture_access_state();
}

void RustBridge::refreshScreenCaptureAccess()
{
    im_refresh_screen_capture_access();
}

bool RustBridge::isAdhocBuild()
{
    return im_is_adhoc_build();
}

juce::String RustBridge::startEngine()
{
    return toJuceString(im_start_engine());
}

void RustBridge::stopEngine()
{
    im_stop_engine();
}

juce::String RustBridge::statusLabel(uint8_t status)
{
    switch (status)
    {
        case 1: return "Ready";
        case 2: return "Running";
        case 3: return "Virtual microphone not installed";
        case 4: return "Permission needed";
        case 5: return "Something went wrong";
        default: return "Stopped";
    }
}

} // namespace input_mixer
