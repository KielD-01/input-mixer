#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

#include "LevelMeterComponent.h"
#include "bridge/RustBridge.h"

class MonitorStripComponent : public juce::Component
{
public:
    using ChangeFn = std::function<void(bool enabled, const juce::String& deviceUid, float volume)>;

    MonitorStripComponent();

    void setEnabled(bool enabled);
    void setVolume(float volume);
    void setDeviceUid(const juce::String& uid);
    void setPeak(float peak);
    void setMeterAnimationPaused(bool paused);
    void setOnChange(ChangeFn fn);

    void refreshOutputDevices();
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void notifyChange();
    void updateVolumeLabel();
    void updateWarningVisibility();

    juce::Label titleLabel;
    juce::ToggleButton enableButton;
    juce::Label outputLabel;
    juce::ComboBox outputSelector;
    juce::Label warningLabel;
    LevelMeterComponent meter;
    juce::Label volumeLabel;
    juce::Slider volumeSlider;

    std::vector<input_mixer::OutputDeviceItem> outputs;
    ChangeFn onChange;
};
