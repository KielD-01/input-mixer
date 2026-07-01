#include "FontAudioIcons.h"

#include <fontaudio/fontaudio.h>
#include <fontaudio/data/Icons.h>

#include "MixerTheme.h"

namespace
{
fontaudio::IconHelper& iconHelper()
{
    static fontaudio::IconHelper helper;
    return helper;
}

constexpr float kMuteIconSize = 14.0f;
constexpr float kHeadphonesIconSize = 13.0f;
constexpr float kMeterIconSize = 13.0f;
constexpr float kSourceFilterIconSize = 13.0f;
} // namespace

namespace FontAudioIcons
{
void drawMuteIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool muted)
{
    const auto icon = muted ? fontaudio::Mute : fontaudio::Speaker;
    const auto colour = muted ? juce::Colours::white : MixerTheme::textMuted();
    iconHelper().drawCenterd(g, icon, kMuteIconSize, colour, bounds.toNearestInt());
}

void drawHeadphonesIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool enabled)
{
    const auto colour = enabled ? MixerTheme::textPrimary() : MixerTheme::textMuted();
    iconHelper().drawCenterd(g, fontaudio::Headphones, kHeadphonesIconSize, colour, bounds.toNearestInt());
}

void drawMicrophoneIcon(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    iconHelper().drawCenterd(g, fontaudio::Microphone, kMeterIconSize, MixerTheme::textMuted(), bounds.toNearestInt());
}

void drawSpeakerIcon(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    iconHelper().drawCenterd(g, fontaudio::Speaker, kMeterIconSize, MixerTheme::textMuted(), bounds.toNearestInt());
}

void drawHardwareSourceIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour)
{
    iconHelper().drawCenterd(g, fontaudio::Microphone, kSourceFilterIconSize, colour, bounds.toNearestInt());
}

void drawSoftwareSourceIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour)
{
    iconHelper().drawCenterd(g, fontaudio::Waveform, kSourceFilterIconSize, colour, bounds.toNearestInt());
}

void drawSettingsIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour)
{
    const auto centre = bounds.getCentre();
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.32f;
    const float lineWidth = 1.6f;

    g.setColour(colour);
    g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, lineWidth);

    for (int i = 0; i < 6; ++i)
    {
        const float angle = static_cast<float>(i) * juce::MathConstants<float>::pi / 3.0f;
        const float cosA = std::cos(angle);
        const float sinA = std::sin(angle);
        g.drawLine(centre.x + radius * 0.55f * cosA,
                   centre.y + radius * 0.55f * sinA,
                   centre.x + radius * 1.15f * cosA,
                   centre.y + radius * 1.15f * sinA,
                   lineWidth);
    }
}

void drawReconnectIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour)
{
    iconHelper().drawCenterd(g, fontaudio::Loop, 13.0f, colour, bounds.toNearestInt());
}
} // namespace FontAudioIcons
