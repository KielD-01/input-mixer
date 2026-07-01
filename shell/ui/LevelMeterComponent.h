#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class LevelMeterComponent : public juce::Component, private juce::Timer
{
public:
    enum class Orientation
    {
        vertical,
        horizontal
    };

    LevelMeterComponent();
    void setLevel(float peak);
    void setAnimationPaused(bool paused);
    void setOrientation(Orientation orientation);
    void setShowSegmentTicks(bool show);

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;
    void paintVertical(juce::Graphics& g, juce::Rectangle<float> bounds);
    void paintHorizontal(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawSegmentTicks(juce::Graphics& g, juce::Rectangle<float> bounds, bool vertical);

    float currentPeak = 0.0f;
    float displayPeak = 0.0f;
    Orientation orientation = Orientation::vertical;
    bool showSegmentTicks = false;
    bool animationPaused = false;
};
