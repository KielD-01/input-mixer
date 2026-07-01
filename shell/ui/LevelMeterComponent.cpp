#include "LevelMeterComponent.h"

#include "MixerTheme.h"

namespace
{
juce::Colour segmentColour(float segmentPercent)
{
    if (segmentPercent < 0.6f)
        return MixerTheme::meterGreen();
    if (segmentPercent < 0.8f)
        return MixerTheme::meterYellow();
    return MixerTheme::meterRed();
}

void drawSegmentedMeter(juce::Graphics& g,
                        juce::Rectangle<float> bounds,
                        float level,
                        bool vertical)
{
    const int segments = MixerTheme::kMeterSegments();
    const float gap = MixerTheme::kMeterSegmentGap();
    const int litSegments = juce::jlimit(0, segments, juce::roundToInt(level * static_cast<float>(segments)));

    if (vertical)
    {
        const float totalGap = gap * static_cast<float>(segments - 1);
        const float segmentHeight = (bounds.getHeight() - totalGap) / static_cast<float>(segments);

        for (int i = 0; i < segments; ++i)
        {
            const float segmentPercent = static_cast<float>(i) / static_cast<float>(segments);
            const float y = bounds.getBottom() - static_cast<float>(i + 1) * (segmentHeight + gap) + gap;
            auto segment = juce::Rectangle<float>(bounds.getX(), y, bounds.getWidth(), segmentHeight);
            const bool lit = i < litSegments;
            g.setColour(segmentColour(segmentPercent).withAlpha(lit ? 0.9f : 0.15f));
            g.fillRoundedRectangle(segment, 1.0f);
        }
    }
    else
    {
        const float totalGap = gap * static_cast<float>(segments - 1);
        const float segmentWidth = (bounds.getWidth() - totalGap) / static_cast<float>(segments);

        for (int i = 0; i < segments; ++i)
        {
            const float segmentPercent = static_cast<float>(i) / static_cast<float>(segments);
            const float x = bounds.getX() + static_cast<float>(i) * (segmentWidth + gap);
            auto segment = juce::Rectangle<float>(x, bounds.getY(), segmentWidth, bounds.getHeight());
            const bool lit = i < litSegments;
            g.setColour(segmentColour(segmentPercent).withAlpha(lit ? 0.9f : 0.15f));
            g.fillRoundedRectangle(segment, 1.0f);
        }
    }
}
} // namespace

LevelMeterComponent::LevelMeterComponent()
{
    startTimerHz(30);
}

void LevelMeterComponent::setLevel(float peak)
{
    currentPeak = juce::jlimit(0.0f, 1.0f, peak);
}

void LevelMeterComponent::setAnimationPaused(bool paused)
{
    if (animationPaused == paused)
        return;

    animationPaused = paused;

    if (paused)
        stopTimer();
    else
        startTimerHz(30);
}

void LevelMeterComponent::setOrientation(Orientation newOrientation)
{
    orientation = newOrientation;
    repaint();
}

void LevelMeterComponent::setShowSegmentTicks(bool show)
{
    showSegmentTicks = show;
    repaint();
}

void LevelMeterComponent::timerCallback()
{
    if (animationPaused)
        return;

    displayPeak = displayPeak * 0.85f + currentPeak * 0.15f;
    repaint();
}

void LevelMeterComponent::drawSegmentTicks(juce::Graphics& g, juce::Rectangle<float> bounds, bool vertical)
{
    if (! showSegmentTicks)
        return;

    const float levels[] = { 0.25f, 0.5f, 1.0f };
    g.setColour(MixerTheme::separator().withAlpha(0.6f));

    for (const float level : levels)
    {
        if (vertical)
        {
            const float y = bounds.getBottom() - bounds.getHeight() * level;
            g.drawLine(bounds.getX(), y, bounds.getRight(), y, 0.5f);
        }
        else
        {
            const float x = bounds.getX() + bounds.getWidth() * level;
            g.drawLine(x, bounds.getY(), x, bounds.getBottom(), 0.5f);
        }
    }
}

void LevelMeterComponent::paintVertical(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    drawSegmentedMeter(g, bounds, displayPeak, true);
    drawSegmentTicks(g, bounds, true);
}

void LevelMeterComponent::paintHorizontal(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    drawSegmentedMeter(g, bounds, displayPeak, false);
    drawSegmentTicks(g, bounds, false);
}

void LevelMeterComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);

    if (orientation == Orientation::horizontal)
        paintHorizontal(g, bounds);
    else
        paintVertical(g, bounds);
}
