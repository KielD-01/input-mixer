#include "MasterStripComponent.h"

#include "MixerTheme.h"

MasterStripComponent::MasterStripComponent()
    : titleLabel("title", "MASTER"),
      volumeLabel("volLabel", "100%"),
      muteButton("mute"),
      limiterLabel("limiter", "Auto limiter")
{
    titleLabel.setFont(MixerTheme::sectionTitleFont());

    volumeLabel.setFont(MixerTheme::smallFont());
    volumeLabel.setColour(juce::Label::textColourId, MixerTheme::textMuted());
    volumeLabel.setJustificationType(juce::Justification::centred);

    limiterLabel.setFont(MixerTheme::smallFont());
    limiterLabel.setColour(juce::Label::textColourId, MixerTheme::textMuted());
    limiterLabel.setJustificationType(juce::Justification::centredLeft);

    meter.setShowSegmentTicks(false);

    volumeSlider.setRange(0.0, 100.0, 0.1);
    volumeSlider.setValue(100.0);
    volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volumeSlider.getProperties().set("isMasterFader", true);
    volumeSlider.onValueChange = [this] {
        updateVolumeLabel();
        if (onChange)
            onChange(static_cast<float>(volumeSlider.getValue() / 100.0), muteButton.getToggleState());
    };

    muteButton.setButtonText({});
    muteButton.getProperties().set("compactMute", true);
    muteButton.onClick = [this] {
        if (onChange)
            onChange(static_cast<float>(volumeSlider.getValue() / 100.0), muteButton.getToggleState());
    };

    addAndMakeVisible(titleLabel);
    addAndMakeVisible(meter);
    addAndMakeVisible(volumeLabel);
    addAndMakeVisible(volumeSlider);
    addAndMakeVisible(muteButton);
    addAndMakeVisible(limiterLabel);
}

void MasterStripComponent::setVolume(float volume)
{
    volumeSlider.setValue(volume * 100.0, juce::dontSendNotification);
    updateVolumeLabel();
}

void MasterStripComponent::setMuted(bool muted)
{
    muteButton.setToggleState(muted, juce::dontSendNotification);
}

void MasterStripComponent::setPeak(float peak)
{
    meter.setLevel(peak);
    updateLimiterBadge(peak);
}

void MasterStripComponent::setMeterAnimationPaused(bool paused)
{
    meter.setAnimationPaused(paused);
}

void MasterStripComponent::setOnChange(ChangeFn fn)
{
    onChange = std::move(fn);
}

void MasterStripComponent::updateVolumeLabel()
{
    volumeLabel.setText(juce::String(static_cast<int>(volumeSlider.getValue())) + "%",
                        juce::dontSendNotification);
}

void MasterStripComponent::updateLimiterBadge(float peak)
{
    if (peak > 0.85f)
    {
        limiterLabel.setText("Mix is loud — limiter active", juce::dontSendNotification);
        limiterLabel.setColour(juce::Label::textColourId, MixerTheme::warning());
    }
    else
    {
        limiterLabel.setText("Auto limiter", juce::dontSendNotification);
        limiterLabel.setColour(juce::Label::textColourId, MixerTheme::textMuted());
    }
}

void MasterStripComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(MixerTheme::panel());
    g.fillRoundedRectangle(bounds, static_cast<float>(MixerTheme::kPanelCornerRadius()));
    g.setColour(MixerTheme::separator());
    g.drawRoundedRectangle(bounds.reduced(0.5f), static_cast<float>(MixerTheme::kPanelCornerRadius()), 1.0f);

    auto sep = getLocalBounds().reduced(MixerTheme::kStripInnerPadding());
    sep = sep.removeFromTop(22);
    g.setColour(MixerTheme::separator());
    g.drawHorizontalLine(sep.getBottom(), static_cast<float>(sep.getX()), static_cast<float>(sep.getRight()));
}

void MasterStripComponent::resized()
{
    auto area = getLocalBounds().reduced(MixerTheme::kStripInnerPadding());
    titleLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(6);

    meter.setBounds(area.removeFromTop(80).withSizeKeepingCentre(16, 80));
    area.removeFromTop(8);
    volumeLabel.setBounds(area.removeFromTop(14));
    area.removeFromTop(2);
    volumeSlider.setBounds(area.removeFromTop(28));
    area.removeFromTop(8);

    auto controlRow = area.removeFromTop(28);
    muteButton.setBounds(controlRow.removeFromLeft(28));
    controlRow.removeFromLeft(8);
    limiterLabel.setBounds(controlRow);
}
