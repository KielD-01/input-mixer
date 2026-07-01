#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <vector>

#include "bridge/RustBridge.h"

class SettingsStore
{
public:
    static constexpr int kMaxChannels = 6;

    static SettingsStore& get();

    bool isWizardCompleted() const;
    void setWizardCompleted(bool completed);

    bool loadMicrophonePrompted() const;
    void setMicrophonePrompted(bool prompted);

    std::vector<input_mixer::ChannelState> loadChannels() const;
    void saveChannels(const std::vector<input_mixer::ChannelState>& channels);

    float loadMasterVolume() const;
    bool loadMasterMuted() const;
    void saveMaster(float volume, bool muted);

    bool loadMonitorEnabled() const;
    float loadMonitorVolume() const;
    juce::String loadMonitorDeviceUid() const;
    void saveMonitor(bool enabled, float volume, const juce::String& deviceUid);

    juce::var loadThemeJson() const;
    void saveThemeJson(const juce::var& themeJson);

private:
    SettingsStore();
    juce::PropertiesFile properties;
};
