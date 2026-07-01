#include "MonitorStripComponent.h"

#include "MixerTheme.h"

MonitorStripComponent::MonitorStripComponent()
    : titleLabel("title", "MONITOR"),
      enableButton { "Listen to mix" },
      outputLabel("outputLabel", "Output"),
      warningLabel("warning", "No headphones or speakers found"),
      volumeLabel("volLabel", "75%")
{
    titleLabel.setFont(MixerTheme::sectionTitleFont());

    outputLabel.setFont(MixerTheme::smallFont());
    outputLabel.setColour(juce::Label::textColourId, MixerTheme::textMuted());

    warningLabel.setFont(MixerTheme::smallFont());
    warningLabel.setColour(juce::Label::textColourId, MixerTheme::warning());
    warningLabel.setVisible(false);

    volumeLabel.setFont(MixerTheme::smallFont());
    volumeLabel.setColour(juce::Label::textColourId, MixerTheme::textMuted());
    volumeLabel.setJustificationType(juce::Justification::centred);

    enableButton.onClick = [this] { notifyChange(); };
    enableButton.getProperties().set("listenToggle", true);
    outputSelector.onChange = [this] { notifyChange(); };

    volumeSlider.setRange(0.0, 100.0, 0.1);
    volumeSlider.setValue(75.0);
    volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volumeSlider.onValueChange = [this] {
        updateVolumeLabel();
        notifyChange();
    };

    addAndMakeVisible(titleLabel);
    addAndMakeVisible(enableButton);
    addAndMakeVisible(outputLabel);
    addAndMakeVisible(outputSelector);
    addAndMakeVisible(warningLabel);
    addAndMakeVisible(meter);
    addAndMakeVisible(volumeLabel);
    addAndMakeVisible(volumeSlider);

    refreshOutputDevices();
}

void MonitorStripComponent::notifyChange()
{
    if (! onChange)
        return;

    const auto idx = outputSelector.getSelectedItemIndex();
    juce::String uid;
    if (juce::isPositiveAndBelow(idx, static_cast<int>(outputs.size())))
        uid = outputs[static_cast<size_t>(idx)].uid;

    onChange(enableButton.getToggleState(), uid, static_cast<float>(volumeSlider.getValue() / 100.0));
}

void MonitorStripComponent::setEnabled(bool enabled)
{
    enableButton.setToggleState(enabled, juce::dontSendNotification);
}

void MonitorStripComponent::setVolume(float volume)
{
    volumeSlider.setValue(volume * 100.0, juce::dontSendNotification);
    updateVolumeLabel();
}

void MonitorStripComponent::setDeviceUid(const juce::String& uid)
{
    for (int i = 0; i < outputSelector.getNumItems(); ++i)
    {
        if (outputSelector.getItemText(i) == uid || outputs[static_cast<size_t>(i)].uid == uid)
        {
            outputSelector.setSelectedItemIndex(i, juce::dontSendNotification);
            return;
        }
    }
}

void MonitorStripComponent::setPeak(float peak)
{
    meter.setLevel(peak);
}

void MonitorStripComponent::setMeterAnimationPaused(bool paused)
{
    meter.setAnimationPaused(paused);
}

void MonitorStripComponent::setOnChange(ChangeFn fn)
{
    onChange = std::move(fn);
}

void MonitorStripComponent::refreshOutputDevices()
{
    outputs = input_mixer::RustBridge::scanHardwareOutputs();
    outputSelector.clear();

    for (int i = 0; i < static_cast<int>(outputs.size()); ++i)
        outputSelector.addItem(outputs[static_cast<size_t>(i)].name, i + 1);

    if (outputSelector.getNumItems() > 0)
        outputSelector.setSelectedItemIndex(0, juce::dontSendNotification);

    updateWarningVisibility();
    resized();
}

void MonitorStripComponent::updateVolumeLabel()
{
    volumeLabel.setText(juce::String(static_cast<int>(volumeSlider.getValue())) + "%",
                        juce::dontSendNotification);
}

void MonitorStripComponent::updateWarningVisibility()
{
    warningLabel.setVisible(outputs.empty());
}

void MonitorStripComponent::paint(juce::Graphics& g)
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

void MonitorStripComponent::resized()
{
    auto area = getLocalBounds().reduced(MixerTheme::kStripInnerPadding());
    titleLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(6);
    enableButton.setBounds(area.removeFromTop(24));
    area.removeFromTop(4);
    outputLabel.setBounds(area.removeFromTop(16));
    outputSelector.setBounds(area.removeFromTop(26));
    area.removeFromTop(4);

    if (warningLabel.isVisible())
    {
        warningLabel.setBounds(area.removeFromTop(32));
        area.removeFromTop(4);
    }

    volumeLabel.setBounds(area.removeFromTop(14));
    area.removeFromTop(2);

    auto faderRow = area.removeFromTop(24);
    meter.setBounds(faderRow.removeFromLeft(16));
    faderRow.removeFromLeft(6);
    volumeSlider.setBounds(faderRow);
}
