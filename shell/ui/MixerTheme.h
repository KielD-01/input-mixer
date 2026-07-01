#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace MixerTheme
{
struct ThemeState
{
    juce::Colour background { 0xff0e0f12 };
    juce::Colour panel { 0xff14161a };
    juce::Colour elevated { 0xff1a1d22 };
    juce::Colour meterTrack { 0xff2a2d33 };
    juce::Colour meterGreen { 0xff66bb6a };
    juce::Colour meterYellow { 0xffffd54f };
    juce::Colour meterRed { 0xffef5350 };
    juce::Colour warning { 0xffffd54f };
    juce::Colour error { 0xffef5350 };
    juce::Colour accent { 0xffFFB800 };
    juce::Colour textPrimary { 0xffF4F1EA };
    juce::Colour textMuted { 0xff8E93A0 };
    juce::Colour faderTrack { 0xff2a2d33 };
    juce::Colour faderFill { 0xffFFB800 };
    juce::Colour faderThumb { 0xffF4F1EA };
    juce::Colour buttonOff { 0xff3A3D44 };
    juce::Colour buttonOn { 0xffef5350 };
    juce::Colour separator { 0xff2a2d33 };

    int outerPadding = 12;
    int stripInnerPadding = 8;
    int stripMinWidth = 120;
    int stripGap = 8;
    int rightPanelWidth = 232;
    int masterColumnMasterRatioPercent = 52;
    int panelCornerRadius = 6;
    int meterSegments = 12;
    float meterSegmentGap = 2.0f;

    int minWindowWidth = 900;
    int minWindowHeight = 560;
};

ThemeState& state();
ThemeState defaultState();

void loadFromSettings();
void saveToSettings();
void resetToDefaults();
juce::var exportToJson();
bool importFromJson(const juce::var& json, juce::String& errorMessage);
bool importDrawdioJson(const juce::var& json, juce::String& errorMessage);

inline juce::Colour& background() { return state().background; }
inline juce::Colour& panel() { return state().panel; }
inline juce::Colour& elevated() { return state().elevated; }
inline juce::Colour& meterTrack() { return state().meterTrack; }
inline juce::Colour& meterGreen() { return state().meterGreen; }
inline juce::Colour& meterYellow() { return state().meterYellow; }
inline juce::Colour& meterRed() { return state().meterRed; }
inline juce::Colour& warning() { return state().warning; }
inline juce::Colour& error() { return state().error; }
inline juce::Colour& accent() { return state().accent; }
inline juce::Colour selection() { return state().accent; }
inline juce::Colour& textPrimary() { return state().textPrimary; }
inline juce::Colour& textMuted() { return state().textMuted; }
inline juce::Colour& faderTrack() { return state().faderTrack; }
inline juce::Colour& faderFill() { return state().faderFill; }
inline juce::Colour& faderThumb() { return state().faderThumb; }
inline juce::Colour& buttonOff() { return state().buttonOff; }
inline juce::Colour& buttonOn() { return state().buttonOn; }
inline juce::Colour& separator() { return state().separator; }

inline int& kOuterPadding() { return state().outerPadding; }
inline int& kStripInnerPadding() { return state().stripInnerPadding; }
inline int& kStripMinWidth() { return state().stripMinWidth; }
inline int& kStripGap() { return state().stripGap; }
inline int& kRightPanelWidth() { return state().rightPanelWidth; }
inline int& kMasterColumnMasterRatioPercent() { return state().masterColumnMasterRatioPercent; }
inline int& kPanelCornerRadius() { return state().panelCornerRadius; }
inline int& kMeterSegments() { return state().meterSegments; }
inline float& kMeterSegmentGap() { return state().meterSegmentGap; }
inline int& kMinWindowWidth() { return state().minWindowWidth; }
inline int& kMinWindowHeight() { return state().minWindowHeight; }

inline juce::Font headerFont() { return juce::Font(juce::FontOptions(22.0f).withStyle("Bold")); }
inline juce::Font sectionTitleFont() { return juce::Font(juce::FontOptions(14.0f).withStyle("Bold")); }
inline juce::Font labelFont() { return juce::Font(juce::FontOptions(13.0f)); }
inline juce::Font smallFont() { return juce::Font(juce::FontOptions(11.0f)); }
inline juce::Font channelHeaderFont() { return juce::Font(juce::FontOptions(12.0f).withStyle("Bold")); }

} // namespace MixerTheme
