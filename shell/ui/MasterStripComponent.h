#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

#include "LevelMeterComponent.h"

class MasterStripComponent : public juce::Component
{
public:
    using ChangeFn = std::function<void(float volume, bool muted)>;

    MasterStripComponent();

    void setVolume(float volume);
    void setMuted(bool muted);
    void setPeak(float peak);
    void setMeterAnimationPaused(bool paused);
    void setOnChange(ChangeFn fn);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void updateVolumeLabel();
    void updateLimiterBadge(float peak);

    juce::Label titleLabel;
    LevelMeterComponent meter;
    juce::Label volumeLabel;
    juce::Slider volumeSlider;
    juce::ToggleButton muteButton;
    juce::Label limiterLabel;
    ChangeFn onChange;
};
