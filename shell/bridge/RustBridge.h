#pragma once

#include <juce_core/juce_core.h>
#include <vector>

#include "input-mixer-core/src/ffi.rs.h"

namespace input_mixer
{

struct SourceItem
{
    input_mixer::CxxSourceDescriptor descriptor;
    bool operator==(const SourceItem& other) const
    {
        return descriptor.hardware_uid == other.descriptor.hardware_uid
            && descriptor.app_bundle_id == other.descriptor.app_bundle_id
            && descriptor.window_id == other.descriptor.window_id
            && descriptor.process_id == other.descriptor.process_id
            && descriptor.kind == other.descriptor.kind;
    }
};

struct OutputDeviceItem
{
    juce::String uid;
    juce::String name;
};

struct ChannelState
{
    bool hasSource = false;
    input_mixer::CxxSourceDescriptor source {};
    float volume = 1.0f;
    bool muted = false;
};

class RustBridge
{
public:
    static void initialise();
    static void shutdown();

    static std::vector<SourceItem> scanHardwareInputs();
    static std::vector<OutputDeviceItem> scanHardwareOutputs();
    static std::vector<SourceItem> scanApplicationSources();

    static juce::String setChannels(const std::vector<ChannelState>& channels);
    static juce::String reopenChannel(int channelIndex);
    static juce::String setMaster(float volume, bool muted);
    static juce::String setMonitor(bool enabled, const juce::String& deviceUid, float volume);

    static input_mixer::CxxMeterSnapshot getMeterLevels();
    static uint8_t getEngineStatus();
    static bool detectVirtualDriver();
    static bool virtualMicNeedsAudioRestart();
    static bool virtualMicHalOnDisk();
    static juce::String detectedVirtualMicName();

    static bool hasMicrophoneAccess();
    static bool isMicrophoneAccessDenied();
    static bool shouldRequestMicrophoneAccess();
    static void setMicrophonePrompted(bool prompted);
    static bool requestMicrophoneAccess();
    static bool hasScreenCaptureAccess();
    static bool shouldRequestScreenCaptureAccess();
    static bool requestScreenCaptureAccess();
    static int screenCaptureAccessState();
    static void refreshScreenCaptureAccess();
    static bool isAdhocBuild();

    static juce::String startEngine();
    static void stopEngine();

    static juce::String statusLabel(uint8_t status);

private:
    static rust::Vec<input_mixer::CxxChannelConfig> toCxxChannels(const std::vector<ChannelState>& channels);
};

} // namespace input_mixer
