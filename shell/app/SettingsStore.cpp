#include "SettingsStore.h"

#include "bridge/CxxString.h"

namespace
{
bool channelHasAssignedSource(const input_mixer::ChannelState& state)
{
    if (! state.hasSource)
        return false;

    if (state.source.kind == 0)
        return state.source.hardware_uid.size() > 0 || state.source.display_name.size() > 0;

    if (state.source.kind == 1)
        return state.source.app_bundle_id.size() > 0 || state.source.window_id != 0;

    return false;
}
} // namespace

SettingsStore& SettingsStore::get()
{
    static SettingsStore instance;
    return instance;
}

SettingsStore::SettingsStore()
    : properties([&] {
          juce::PropertiesFile::Options options;
          options.applicationName = "InputMixer";
          options.filenameSuffix = ".json";
          options.osxLibrarySubFolder = "Application Support";
          options.storageFormat = juce::PropertiesFile::storeAsXML;
          return options;
      }())
{
}

bool SettingsStore::isWizardCompleted() const
{
    if (properties.getBoolValue("wizardCompleted", false))
        return true;

    for (int i = 0; i < kMaxChannels; ++i)
    {
        if (properties.getBoolValue("ch" + juce::String(i) + "_hasSource", false))
            return true;
    }

    if (properties.getBoolValue("monitorEnabled", false))
        return true;

    const auto monitorUid = properties.getValue("monitorDeviceUid", {});
    if (monitorUid.isNotEmpty())
        return true;

    return false;
}

void SettingsStore::setWizardCompleted(bool completed)
{
    properties.setValue("wizardCompleted", completed);
    properties.saveIfNeeded();
}

bool SettingsStore::loadMicrophonePrompted() const
{
    return properties.getBoolValue("microphonePrompted", false);
}

void SettingsStore::setMicrophonePrompted(bool prompted)
{
    properties.setValue("microphonePrompted", prompted);
    properties.saveIfNeeded();
}

std::vector<input_mixer::ChannelState> SettingsStore::loadChannels() const
{
    std::vector<input_mixer::ChannelState> channels(static_cast<size_t>(kMaxChannels));

    for (int i = 0; i < kMaxChannels; ++i)
    {
        const auto prefix = "ch" + juce::String(i) + "_";
        channels[static_cast<size_t>(i)].hasSource = properties.getBoolValue(prefix + "hasSource", false);
        channels[static_cast<size_t>(i)].volume = static_cast<float>(properties.getDoubleValue(prefix + "volume", 1.0));
        channels[static_cast<size_t>(i)].muted = properties.getBoolValue(prefix + "muted", false);

        auto& src = channels[static_cast<size_t>(i)].source;
        src.kind = static_cast<uint8_t>(properties.getIntValue(prefix + "kind", 0));
        src.hardware_uid = properties.getValue(prefix + "hardware_uid", "").toStdString();
        src.app_bundle_id = properties.getValue(prefix + "app_bundle_id", "").toStdString();
        src.display_name = properties.getValue(prefix + "display_name", "").toStdString();
        src.subtitle = properties.getValue(prefix + "subtitle", "").toStdString();
        src.window_id = static_cast<uint64_t>(properties.getIntValue(prefix + "window_id", 0));
        src.process_id = static_cast<uint32_t>(properties.getIntValue(prefix + "process_id", 0));

        if (! channelHasAssignedSource(channels[static_cast<size_t>(i)]))
        {
            channels[static_cast<size_t>(i)].hasSource = false;
            channels[static_cast<size_t>(i)].source = {};
        }
    }

    return channels;
}

void SettingsStore::saveChannels(const std::vector<input_mixer::ChannelState>& channels)
{
    for (int i = 0; i < kMaxChannels && i < static_cast<int>(channels.size()); ++i)
    {
        const auto prefix = "ch" + juce::String(i) + "_";
        const auto& ch = channels[static_cast<size_t>(i)];
        properties.setValue(prefix + "hasSource", ch.hasSource);
        properties.setValue(prefix + "volume", ch.volume);
        properties.setValue(prefix + "muted", ch.muted);
        properties.setValue(prefix + "kind", static_cast<int>(ch.source.kind));
        properties.setValue(prefix + "hardware_uid", input_mixer::toJuceString(ch.source.hardware_uid));
        properties.setValue(prefix + "app_bundle_id", input_mixer::toJuceString(ch.source.app_bundle_id));
        properties.setValue(prefix + "display_name", input_mixer::toJuceString(ch.source.display_name));
        properties.setValue(prefix + "subtitle", input_mixer::toJuceString(ch.source.subtitle));
        properties.setValue(prefix + "window_id", static_cast<int>(ch.source.window_id));
        properties.setValue(prefix + "process_id", static_cast<int>(ch.source.process_id));
    }

    properties.saveIfNeeded();
}

float SettingsStore::loadMasterVolume() const
{
    return static_cast<float>(properties.getDoubleValue("masterVolume", 1.0));
}

bool SettingsStore::loadMasterMuted() const
{
    return properties.getBoolValue("masterMuted", false);
}

void SettingsStore::saveMaster(float volume, bool muted)
{
    properties.setValue("masterVolume", volume);
    properties.setValue("masterMuted", muted);
    properties.saveIfNeeded();
}

bool SettingsStore::loadMonitorEnabled() const
{
    return properties.getBoolValue("monitorEnabled", false);
}

float SettingsStore::loadMonitorVolume() const
{
    return static_cast<float>(properties.getDoubleValue("monitorVolume", 0.75));
}

juce::String SettingsStore::loadMonitorDeviceUid() const
{
    return properties.getValue("monitorDeviceUid", "");
}

void SettingsStore::saveMonitor(bool enabled, float volume, const juce::String& deviceUid)
{
    properties.setValue("monitorEnabled", enabled);
    properties.setValue("monitorVolume", volume);
    properties.setValue("monitorDeviceUid", deviceUid);
    properties.saveIfNeeded();
}

juce::var SettingsStore::loadThemeJson() const
{
    const auto raw = properties.getValue("themeJson", {});
    if (raw.isEmpty())
        return {};

    return juce::JSON::parse(raw);
}

void SettingsStore::saveThemeJson(const juce::var& themeJson)
{
    if (themeJson.isVoid() || themeJson.isUndefined())
    {
        properties.removeValue("themeJson");
    }
    else
    {
        properties.setValue("themeJson", juce::JSON::toString(themeJson));
    }

    properties.saveIfNeeded();
}
