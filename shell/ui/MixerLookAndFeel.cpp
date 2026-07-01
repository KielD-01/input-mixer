#include "MixerLookAndFeel.h"

#include "FontAudioIcons.h"
#include "MixerTheme.h"

MixerLookAndFeel::MixerLookAndFeel()
{
    applyTheme();
}

void MixerLookAndFeel::applyTheme()
{
    setColour(juce::ResizableWindow::backgroundColourId, MixerTheme::background());
    setColour(juce::Label::textColourId, MixerTheme::textPrimary());
    setColour(juce::Slider::textBoxTextColourId, MixerTheme::textPrimary());
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TextButton::buttonColourId, MixerTheme::buttonOff());
    setColour(juce::TextButton::textColourOffId, MixerTheme::textPrimary());
    setColour(juce::ToggleButton::textColourId, MixerTheme::textPrimary());
    setColour(juce::ToggleButton::tickColourId, MixerTheme::buttonOn());
    setColour(juce::ToggleButton::tickDisabledColourId, MixerTheme::textMuted());
    setColour(juce::ComboBox::backgroundColourId, MixerTheme::elevated());
    setColour(juce::ComboBox::textColourId, MixerTheme::textPrimary());
    setColour(juce::ComboBox::outlineColourId, MixerTheme::separator());
    setColour(juce::ComboBox::arrowColourId, MixerTheme::textMuted());
    setColour(juce::PopupMenu::backgroundColourId, MixerTheme::elevated());
    setColour(juce::PopupMenu::textColourId, MixerTheme::textPrimary());
    setColour(juce::PopupMenu::highlightedBackgroundColourId, MixerTheme::accent().withAlpha(0.12f));
    setColour(juce::PopupMenu::highlightedTextColourId, MixerTheme::textPrimary());
    setColour(juce::ScrollBar::thumbColourId, MixerTheme::separator());
    setColour(juce::ScrollBar::trackColourId, MixerTheme::elevated());
}

void MixerLookAndFeel::drawLinearSlider(juce::Graphics& g,
                                        int x,
                                        int y,
                                        int width,
                                        int height,
                                        float sliderPos,
                                        float,
                                        float,
                                        juce::Slider::SliderStyle style,
                                        juce::Slider& slider)
{
    if (style == juce::Slider::LinearHorizontal)
    {
        const bool isMaster = slider.getProperties().getWithDefault("isMasterFader", false);
        drawHorizontalFader(g, juce::Rectangle<float>(static_cast<float>(x),
                                                      static_cast<float>(y),
                                                      static_cast<float>(width),
                                                      static_cast<float>(height)),
                            sliderPos,
                            isMaster);
        return;
    }

    LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, 0, 0, style, slider);
}

void MixerLookAndFeel::drawHorizontalFader(juce::Graphics& g,
                                           juce::Rectangle<float> bounds,
                                           float sliderPos,
                                           bool isMaster)
{
    const float trackAreaHeight = bounds.getHeight();
    const float trackHeight = juce::jmax(2.0f, trackAreaHeight * 0.15f);
    const float thumbRadius = juce::jmin(trackAreaHeight * 0.5f - 1.0f, bounds.getWidth() * 0.06f, isMaster ? 10.0f : 8.0f);
    const float trackWidth = bounds.getWidth() - thumbRadius * 2.0f;
    const float trackX = bounds.getX() + thumbRadius;
    const float trackY = bounds.getCentreY();

    auto track = juce::Rectangle<float>(trackX, trackY - trackHeight * 0.5f, trackWidth, trackHeight);
    g.setColour(MixerTheme::faderTrack());
    g.fillRoundedRectangle(track, trackHeight * 0.5f);

    const float fillEnd = juce::jlimit(trackX, trackX + trackWidth, sliderPos);
    const float fillWidth = juce::jmax(0.0f, fillEnd - trackX);
    if (fillWidth > 0.0f)
    {
        g.setColour(MixerTheme::faderFill().withAlpha(0.5f));
        g.fillRoundedRectangle(track.getX(), track.getY(), fillWidth, track.getHeight(), trackHeight * 0.5f);
    }

    const float thumbX = juce::jlimit(trackX, trackX + trackWidth, sliderPos);
    g.setColour(MixerTheme::faderThumb());
    g.fillEllipse(thumbX - thumbRadius, trackY - thumbRadius, thumbRadius * 2.0f, thumbRadius * 2.0f);
    g.setColour(MixerTheme::faderFill());
    g.drawEllipse(thumbX - thumbRadius, trackY - thumbRadius, thumbRadius * 2.0f, thumbRadius * 2.0f, 1.5f);
}

void MixerLookAndFeel::drawToggleButton(juce::Graphics& g,
                                        juce::ToggleButton& button,
                                        bool shouldDrawButtonAsHighlighted,
                                        bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    const bool isOn = button.getToggleState();
    const bool isCompactMute = button.getProperties().getWithDefault("compactMute", false);

    if (isCompactMute)
    {
        g.setColour(isOn ? MixerTheme::buttonOn()
                         : (shouldDrawButtonAsDown ? MixerTheme::separator()
                                                   : (shouldDrawButtonAsHighlighted ? MixerTheme::buttonOff().brighter(0.15f)
                                                                                    : MixerTheme::buttonOff())));
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(isOn ? MixerTheme::textPrimary() : MixerTheme::textMuted());
        g.setFont(juce::Font(juce::FontOptions(juce::jmin(bounds.getHeight() * 0.58f, 16.0f)).withStyle("Bold")));
        g.drawText("M", bounds, juce::Justification::centred, false);
        return;
    }

    const bool isListenToggle = button.getProperties().getWithDefault("listenToggle", false);
    if (isListenToggle)
    {
        g.setColour(isOn ? MixerTheme::accent().withAlpha(0.12f)
                         : (shouldDrawButtonAsDown ? MixerTheme::separator()
                                                   : (shouldDrawButtonAsHighlighted ? MixerTheme::buttonOff().brighter(0.15f)
                                                                                    : MixerTheme::buttonOff())));
        g.fillRoundedRectangle(bounds, 4.0f);

        if (isOn)
        {
            g.setColour(MixerTheme::accent().withAlpha(0.45f));
            g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
        }

        auto iconArea = bounds.removeFromLeft(bounds.getHeight()).reduced(4.0f);
        FontAudioIcons::drawHeadphonesIcon(g, iconArea, isOn);

        g.setColour(isOn ? MixerTheme::accent() : MixerTheme::textMuted());
        g.setFont(MixerTheme::smallFont());
        g.drawText(button.getButtonText(), bounds.toNearestInt(), juce::Justification::centredLeft, false);
        return;
    }

    LookAndFeel_V4::drawToggleButton(g, button, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
}

void MixerLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                            juce::Button& button,
                                            const juce::Colour&,
                                            bool shouldDrawButtonAsHighlighted,
                                            bool shouldDrawButtonAsDown)
{
    const bool isRemove = button.getProperties().getWithDefault("iconRemove", false);
    const bool isSettings = button.getProperties().getWithDefault("iconSettings", false);
    const bool isReconnect = button.getProperties().getWithDefault("iconReconnect", false);
    const bool isFilterSegment = button.getProperties().getWithDefault("sourceFilterSegment", false);
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);

    if (isRemove)
    {
        g.setColour(shouldDrawButtonAsDown ? MixerTheme::error().withAlpha(0.35f)
                                         : (shouldDrawButtonAsHighlighted ? MixerTheme::buttonOff().brighter(0.15f)
                                                                          : MixerTheme::buttonOff()));
        g.fillRoundedRectangle(bounds, 4.0f);
        return;
    }

    if (isSettings)
    {
        g.setColour(shouldDrawButtonAsDown ? MixerTheme::buttonOff().brighter(0.1f)
                                         : (shouldDrawButtonAsHighlighted ? MixerTheme::buttonOff().brighter(0.2f)
                                                                          : juce::Colours::transparentBlack));
        if (shouldDrawButtonAsDown || shouldDrawButtonAsHighlighted)
            g.fillRoundedRectangle(bounds, 4.0f);
        return;
    }

    if (isReconnect)
    {
        g.setColour(shouldDrawButtonAsDown ? MixerTheme::accent().withAlpha(0.18f)
                                         : (shouldDrawButtonAsHighlighted ? MixerTheme::accent().withAlpha(0.12f)
                                                                          : MixerTheme::buttonOff()));
        g.fillRoundedRectangle(bounds, 4.0f);
        return;
    }

    if (isFilterSegment)
    {
        g.setColour(button.findColour(juce::TextButton::buttonColourId));
        g.fillRoundedRectangle(bounds, 4.0f);
        return;
    }

    g.setColour(shouldDrawButtonAsDown ? MixerTheme::buttonOff().brighter(0.1f)
                                       : (shouldDrawButtonAsHighlighted ? MixerTheme::buttonOff().brighter(0.2f)
                                                                        : MixerTheme::buttonOff()));
    g.fillRoundedRectangle(bounds, 4.0f);
}

void MixerLookAndFeel::drawButtonText(juce::Graphics& g,
                                      juce::TextButton& button,
                                      bool,
                                      bool)
{
    if (button.getProperties().getWithDefault("iconRemove", false))
    {
        g.setColour(button.isEnabled() ? MixerTheme::textMuted() : MixerTheme::textMuted().withAlpha(0.4f));
        g.setFont(juce::Font(juce::FontOptions(16.0f).withStyle("Bold")));
        g.drawText("\xC3\x97", button.getLocalBounds().toFloat(), juce::Justification::centred);
        return;
    }

    if (button.getProperties().getWithDefault("iconSettings", false))
    {
        const auto colour = button.isEnabled()
                                ? (button.isMouseOver() ? MixerTheme::textPrimary() : MixerTheme::textMuted())
                                : MixerTheme::textMuted().withAlpha(0.4f);
        FontAudioIcons::drawSettingsIcon(g, button.getLocalBounds().toFloat().reduced(4.0f), colour);
        return;
    }

    if (button.getProperties().getWithDefault("iconReconnect", false))
    {
        const auto colour = button.isEnabled()
                                ? (button.isMouseOver() ? MixerTheme::accent() : MixerTheme::textMuted())
                                : MixerTheme::textMuted().withAlpha(0.4f);
        FontAudioIcons::drawReconnectIcon(g, button.getLocalBounds().toFloat().reduced(3.0f), colour);
        return;
    }

    const auto textColour = button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId
                                                                      : juce::TextButton::textColourOffId);

    if (button.getProperties().getWithDefault("sourceFilterSegment", false))
    {
        const auto iconKind = button.getProperties()["sourceFilterIcon"].toString();

        if (iconKind == "hardware")
        {
            FontAudioIcons::drawHardwareSourceIcon(g, button.getLocalBounds().toFloat(), textColour);
            return;
        }

        if (iconKind == "software")
        {
            FontAudioIcons::drawSoftwareSourceIcon(g, button.getLocalBounds().toFloat(), textColour);
            return;
        }

        g.setColour(textColour);
        g.setFont(MixerTheme::smallFont());
        g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
        return;
    }

    g.setColour(textColour);
    g.setFont(getTextButtonFont(button, button.getHeight()));
    g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
}

void MixerLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.fillAll(label.findColour(juce::Label::backgroundColourId));

    if (! label.isBeingEdited())
    {
        const float alpha = label.isEnabled() ? 1.0f : 0.5f;
        const juce::Font font(getLabelFont(label));

        g.setColour(label.findColour(juce::Label::textColourId).withMultipliedAlpha(alpha));
        g.setFont(font);

        const auto textArea = getLabelBorderSize(label).subtractedFrom(label.getLocalBounds());
        g.drawText(label.getText(), textArea, label.getJustificationType(), false);
    }
}

void MixerLookAndFeel::drawComboBox(juce::Graphics& g,
                                    int width,
                                    int height,
                                    bool,
                                    int,
                                    int,
                                    int,
                                    int,
                                    juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat();
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(box.findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);

    juce::Path arrow;
    const float arrowSize = 5.0f;
    const float cx = static_cast<float>(width) - 12.0f;
    const float cy = static_cast<float>(height) * 0.5f;
    arrow.addTriangle(cx - arrowSize, cy - arrowSize * 0.5f,
                      cx + arrowSize, cy - arrowSize * 0.5f,
                      cx, cy + arrowSize * 0.5f);
    g.setColour(box.findColour(juce::ComboBox::arrowColourId));
    g.fillPath(arrow);
}

void MixerLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.fillAll(findColour(juce::PopupMenu::backgroundColourId));
    g.setColour(MixerTheme::separator());
    g.drawRect(0, 0, width, height);
}

void MixerLookAndFeel::drawPopupMenuItem(juce::Graphics& g,
                                         const juce::Rectangle<int>& area,
                                         bool isSeparator,
                                         bool isActive,
                                         bool isHighlighted,
                                         bool isTicked,
                                         bool,
                                         const juce::String& text,
                                         const juce::String&,
                                         const juce::Drawable*,
                                         const juce::Colour* textColour)
{
    if (isSeparator)
    {
        g.setColour(MixerTheme::separator());
        g.fillRect(area.withSizeKeepingCentre(area.getWidth(), 1));
        return;
    }

    if (isHighlighted && isActive)
        g.fillAll(findColour(juce::PopupMenu::highlightedBackgroundColourId));

    g.setColour(textColour != nullptr ? *textColour
                                      : (isActive ? findColour(juce::PopupMenu::textColourId)
                                                  : findColour(juce::PopupMenu::textColourId).withAlpha(0.5f)));
    g.setFont(getPopupMenuFont());

    auto textArea = area.reduced(8, 0);
    if (isTicked)
        textArea.removeFromLeft(16);

    g.drawText(text, textArea, juce::Justification::centredLeft, false);
}

juce::Font MixerLookAndFeel::getComboBoxFont(juce::ComboBox&) { return MixerTheme::labelFont(); }

juce::Font MixerLookAndFeel::getPopupMenuFont() { return MixerTheme::labelFont(); }
