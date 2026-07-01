#pragma once

/*
 * FontAudio icon font integration
 * https://github.com/fefanto/fontaudio
 *
 * Icons: CC BY 4.0 — https://creativecommons.org/licenses/by/4.0/
 * Code (JUCE module): MIT License
 *
 * Attribution: FontAudio by fefanto / Audio UI design community.
 */

#include <juce_gui_basics/juce_gui_basics.h>

namespace FontAudioIcons
{
void drawMuteIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool muted);
void drawHeadphonesIcon(juce::Graphics& g, juce::Rectangle<float> bounds, bool enabled);
void drawMicrophoneIcon(juce::Graphics& g, juce::Rectangle<float> bounds);
void drawSpeakerIcon(juce::Graphics& g, juce::Rectangle<float> bounds);
void drawSettingsIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour);
void drawReconnectIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour);
void drawHardwareSourceIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour);
void drawSoftwareSourceIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour);
} // namespace FontAudioIcons
