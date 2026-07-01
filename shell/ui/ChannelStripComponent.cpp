#include "ChannelStripComponent.h"

#include "FontAudioIcons.h"
#include "MixerTheme.h"

#include "bridge/CxxString.h"
#include "bridge/SourceMatching.h"



namespace

{

bool isUsedOnOtherChannel(const input_mixer::SourceItem& item,

                          const std::vector<input_mixer::SourceItem>& usedOnOtherChannels)

{

    for (const auto& used : usedOnOtherChannels)

    {

        if (input_mixer::sourcesEquivalent(used.descriptor, item.descriptor))

            return true;

    }

    return false;

}



void styleFilterButton(juce::TextButton& button, const juce::String& iconKind = {})

{

    button.getProperties().set("sourceFilterSegment", true);

    if (iconKind.isNotEmpty())
        button.getProperties().set("sourceFilterIcon", iconKind);

    button.setClickingTogglesState(false);

}

} // namespace



ChannelStripComponent::ChannelStripComponent(int channelIndex)

    : index(channelIndex),

      titleLabel("title", "CH " + juce::String(channelIndex + 1)),

      sourceFieldLabel("sourceLabel", "Source"),

      volumeLabel("volLabel", "100%"),

      muteButton("mute"),

      unavailableLabel("unavail", {})

{

    titleLabel.setFont(MixerTheme::channelHeaderFont());

    titleLabel.setColour(juce::Label::textColourId, MixerTheme::textMuted());



    sourceFieldLabel.setFont(MixerTheme::smallFont());

    sourceFieldLabel.setColour(juce::Label::textColourId, MixerTheme::textMuted());



    volumeLabel.setFont(MixerTheme::smallFont());

    volumeLabel.setColour(juce::Label::textColourId, MixerTheme::textMuted());

    volumeLabel.setJustificationType(juce::Justification::centred);



    unavailableLabel.setFont(MixerTheme::smallFont());

    unavailableLabel.setColour(juce::Label::textColourId, MixerTheme::error());

    unavailableLabel.setJustificationType(juce::Justification::centred);

    unavailableLabel.setVisible(false);



    styleFilterButton(allSourcesFilterButton);

    styleFilterButton(hardwareFilterButton, "hardware");

    styleFilterButton(softwareFilterButton, "software");

    allSourcesFilterButton.setTooltip("All sources");

    hardwareFilterButton.setButtonText({});

    hardwareFilterButton.setTooltip("Hardware");

    softwareFilterButton.setButtonText({});

    softwareFilterButton.setTooltip("Software");



    allSourcesFilterButton.onClick = [this] { setSourceFilter(SourceFilter::all); };

    hardwareFilterButton.onClick = [this] { setSourceFilter(SourceFilter::hardware); };

    softwareFilterButton.onClick = [this] { setSourceFilter(SourceFilter::software); };



    sourceCombo.addMouseListener(this, false);



    volumeSlider.setRange(0.0, 100.0, 0.1);

    volumeSlider.setValue(100.0);

    volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);

    volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

    volumeSlider.onValueChange = [this] {

        state.volume = static_cast<float>(volumeSlider.getValue() / 100.0);

        updateVolumeLabel();

        if (onChange)

            onChange();

    };



    muteButton.setButtonText({});

    muteButton.getProperties().set("compactMute", true);

    muteButton.onClick = [this] {

        state.muted = muteButton.getToggleState();

        if (onChange)

            onChange();

    };



    sourceCombo.onChange = [this] {
        if (updatingCombo)

            return;



        const int selectedId = sourceCombo.getSelectedId();

        if (selectedId == kNoneItemId)

        {

            state.hasSource = false;

            state.source = {};

            setUnavailableSourceMessage({});

        }

        else if (selectedId >= kFirstSourceItemId)

        {

            const auto sourceIndex = static_cast<size_t>(selectedId - kFirstSourceItemId);

            if (juce::isPositiveAndBelow(sourceIndex, comboSources.size()))

            {

                const auto& selected = comboSources[sourceIndex];

                if (isUsedOnOtherChannel(selected, cachedUsedOnOtherChannels))

                {

                    rebuildComboFromFilter(cachedUsedOnOtherChannels);

                    updateControlsEnabled();

                    return;

                }



                state.hasSource = true;

                state.source = selected.descriptor;

                setUnavailableSourceMessage({});

            }

        }



        updateControlsEnabled();

        if (onChange)

            onChange();

    };



    reconnectButton.getProperties().set("iconReconnect", true);

    reconnectButton.setTooltip("Reconnect source");

    reconnectButton.onClick = [this] {

        if (onReconnect && hasAssignedSource())

            onReconnect(index);

    };



    addAndMakeVisible(titleLabel);

    addAndMakeVisible(sourceFieldLabel);

    addAndMakeVisible(sourceCombo);

    addAndMakeVisible(reconnectButton);

    addAndMakeVisible(allSourcesFilterButton);

    addAndMakeVisible(hardwareFilterButton);

    addAndMakeVisible(softwareFilterButton);

    addAndMakeVisible(inputMeter);

    addAndMakeVisible(outputMeter);

    addAndMakeVisible(volumeLabel);

    addAndMakeVisible(volumeSlider);

    addAndMakeVisible(muteButton);

    addAndMakeVisible(unavailableLabel);



    updateFilterButtonStyles();

    updateControlsEnabled();

}



void ChannelStripComponent::setChannelState(const input_mixer::ChannelState& newState)

{

    state = newState;

    volumeSlider.setValue(state.volume * 100.0, juce::dontSendNotification);

    muteButton.setToggleState(state.muted, juce::dontSendNotification);

    updateVolumeLabel();

    updateControlsEnabled();

}



input_mixer::ChannelState ChannelStripComponent::getChannelState() const

{

    return state;

}



void ChannelStripComponent::refreshSourceCombo(

    const std::vector<input_mixer::SourceItem>& hardware,

    const std::vector<input_mixer::SourceItem>& applications,

    const std::vector<input_mixer::SourceItem>& usedOnOtherChannels)

{

    cachedHardwareSources = hardware;

    cachedApplicationSources = applications;

    cachedUsedOnOtherChannels = usedOnOtherChannels;

    rebuildComboFromFilter(usedOnOtherChannels);

}



void ChannelStripComponent::setSourceFilter(SourceFilter filter)

{

    if (activeFilter == filter)

        return;



    activeFilter = filter;

    updateFilterButtonStyles();

    rebuildComboFromFilter(cachedUsedOnOtherChannels);

}



void ChannelStripComponent::updateFilterButtonStyles()

{

    const auto styleButton = [](juce::TextButton& button, bool active) {

        button.setColour(juce::TextButton::buttonColourId,

                         active ? MixerTheme::accent().withAlpha(0.12f) : MixerTheme::buttonOff());

        button.setColour(juce::TextButton::textColourOffId,

                         active ? MixerTheme::accent() : MixerTheme::textMuted());

    };



    styleButton(allSourcesFilterButton, activeFilter == SourceFilter::all);

    styleButton(hardwareFilterButton, activeFilter == SourceFilter::hardware);

    styleButton(softwareFilterButton, activeFilter == SourceFilter::software);

}



void ChannelStripComponent::rebuildComboFromFilter(

    const std::vector<input_mixer::SourceItem>& usedOnOtherChannels)

{

    updatingCombo = true;

    sourceCombo.clear(juce::dontSendNotification);

    comboSources.clear();



    sourceCombo.addItem("None", kNoneItemId);



    auto addSources = [&](const juce::String& heading, const std::vector<input_mixer::SourceItem>& items) {

        if (items.empty())

            return;



        sourceCombo.addSectionHeading(heading);

        for (const auto& item : items)

        {

            const bool usedElsewhere = isUsedOnOtherChannel(item, usedOnOtherChannels);

            const bool isCurrentSelection = hasAssignedSource()

                                            && input_mixer::sourcesEquivalent(item.descriptor, state.source);

            if (usedElsewhere && ! isCurrentSelection)

                continue;



            const int itemId = kFirstSourceItemId + static_cast<int>(comboSources.size());

            juce::String label = formatSourceName(item.descriptor);

            if (usedElsewhere && isCurrentSelection)

                label += " (in use)";

            sourceCombo.addItem(label, itemId);

            comboSources.push_back(item);

        }

    };



    const bool includeHardware = activeFilter == SourceFilter::all

                                 || activeFilter == SourceFilter::hardware;

    const bool includeSoftware = activeFilter == SourceFilter::all

                                 || activeFilter == SourceFilter::software;



    if (includeHardware)

        addSources("Hardware", cachedHardwareSources);

    if (includeSoftware)

        addSources("Software", cachedApplicationSources);



    if (hasAssignedSource())

    {

        bool found = false;

        for (size_t i = 0; i < comboSources.size(); ++i)

        {

            if (input_mixer::sourcesEquivalent(comboSources[i].descriptor, state.source))

            {

                sourceCombo.setSelectedId(kFirstSourceItemId + static_cast<int>(i),

                                          juce::dontSendNotification);

                state.source = comboSources[i].descriptor;

                found = true;

                break;

            }

        }



        if (! found)
        {
            const int itemId = kFirstSourceItemId + static_cast<int>(comboSources.size());
            sourceCombo.addItem(formatSourceName(state.source), itemId);
            comboSources.push_back({ state.source });
            sourceCombo.setSelectedId(itemId, juce::dontSendNotification);
        }

    }

    else

    {

        sourceCombo.setSelectedId(kNoneItemId, juce::dontSendNotification);

    }



    updatingCombo = false;

}



bool ChannelStripComponent::hasAssignedSource() const

{

    if (! state.hasSource)

        return false;



    if (state.source.kind == 0)

        return state.source.hardware_uid.size() > 0 || state.source.display_name.size() > 0;



    if (state.source.kind == 1)

        return state.source.app_bundle_id.size() > 0 || state.source.window_id != 0;



    return false;

}



void ChannelStripComponent::setMeterLevels(float inputPeak, float outputPeak)

{

    inputMeter.setLevel(inputPeak);

    outputMeter.setLevel(outputPeak);

}



void ChannelStripComponent::setMeterAnimationPaused(bool paused)

{

    inputMeter.setAnimationPaused(paused);

    outputMeter.setAnimationPaused(paused);

}



void ChannelStripComponent::setUnavailableSourceMessage(const juce::String& message)

{

    if (unavailableSourceMessage == message)

        return;



    unavailableSourceMessage = message;

    unavailableLabel.setText(message, juce::dontSendNotification);

    unavailableLabel.setVisible(message.isNotEmpty());

    resized();

    repaint();

}



void ChannelStripComponent::setOnChange(ChangeFn fn) { onChange = std::move(fn); }



void ChannelStripComponent::setOnReconnect(ReconnectFn fn) { onReconnect = std::move(fn); }



void ChannelStripComponent::setOnSourceDropdownOpening(std::function<void(int)> fn)

{

    onSourceDropdownOpening = std::move(fn);

}



void ChannelStripComponent::mouseDown(const juce::MouseEvent& event)

{

    if (event.eventComponent == &sourceCombo || sourceCombo.isParentOf(event.eventComponent))

    {

        if (onSourceDropdownOpening)

            onSourceDropdownOpening(index);

    }



    juce::Component::mouseDown(event);

}



bool ChannelStripComponent::sourceMatches(const input_mixer::CxxSourceDescriptor& a,

                                          const input_mixer::CxxSourceDescriptor& b) const

{

    return input_mixer::sourcesEquivalent(a, b);

}



juce::String ChannelStripComponent::formatSourceName(

    const input_mixer::CxxSourceDescriptor& descriptor) const

{

    auto name = input_mixer::toJuceString(descriptor.display_name);

    if (descriptor.subtitle.size() > 0)

        name += " — " + input_mixer::toJuceString(descriptor.subtitle);

    return name;

}



void ChannelStripComponent::updateControlsEnabled()

{

    const bool controlsEnabled = state.hasSource;

    const bool showReconnect = hasAssignedSource();

    volumeSlider.setEnabled(controlsEnabled);

    muteButton.setEnabled(controlsEnabled);

    reconnectButton.setVisible(showReconnect);

    reconnectButton.setEnabled(showReconnect);

    resized();

}



void ChannelStripComponent::updateVolumeLabel()

{

    volumeLabel.setText(juce::String(static_cast<int>(volumeSlider.getValue())) + "%",

                        juce::dontSendNotification);

}



void ChannelStripComponent::paint(juce::Graphics& g)

{

    auto bounds = getLocalBounds().toFloat();

    g.setColour(MixerTheme::panel());

    g.fillRoundedRectangle(bounds, static_cast<float>(MixerTheme::kPanelCornerRadius()));

    g.setColour(MixerTheme::separator());

    g.drawRoundedRectangle(bounds.reduced(0.5f), static_cast<float>(MixerTheme::kPanelCornerRadius()), 1.0f);

    if (! inputMeterIconBounds.isEmpty())
        FontAudioIcons::drawMicrophoneIcon(g, inputMeterIconBounds.toFloat());

    if (! outputMeterIconBounds.isEmpty())
        FontAudioIcons::drawSpeakerIcon(g, outputMeterIconBounds.toFloat());

}



void ChannelStripComponent::resized()

{

    auto bounds = getLocalBounds().reduced(MixerTheme::kStripInnerPadding());



    juce::Rectangle<int> unavailableArea;

    if (unavailableSourceMessage.isNotEmpty())

    {

        unavailableArea = bounds.removeFromBottom(32);

        bounds.removeFromBottom(4);

    }



    auto area = bounds;

    titleLabel.setBounds(area.removeFromTop(18));

    area.removeFromTop(4);

    sourceFieldLabel.setBounds(area.removeFromTop(14));

    auto sourceRow = area.removeFromTop(26);

    constexpr int reconnectSize = 24;

    constexpr int reconnectGap = 4;

    if (reconnectButton.isVisible())

    {

        reconnectButton.setBounds(sourceRow.removeFromRight(reconnectSize));

        sourceRow.removeFromRight(reconnectGap);

    }

    sourceCombo.setBounds(sourceRow);

    area.removeFromTop(4);



    auto filterRow = area.removeFromTop(22);

    const int filterGap = 4;

    const int filterWidth = (filterRow.getWidth() - filterGap * 2) / 3;

    allSourcesFilterButton.setBounds(filterRow.removeFromLeft(filterWidth));

    filterRow.removeFromLeft(filterGap);

    hardwareFilterButton.setBounds(filterRow.removeFromLeft(filterWidth));

    filterRow.removeFromLeft(filterGap);

    softwareFilterButton.setBounds(filterRow);

    area.removeFromTop(4);



    auto meterLabels = area.removeFromTop(14);

    auto meterRow = area.removeFromTop(80);

    const int meterWidth = 16;

    const int meterGap = 8;

    const int meterBlockWidth = meterWidth * 2 + meterGap;

    const int blockX = meterRow.getX() + (meterRow.getWidth() - meterBlockWidth) / 2;

    inputMeterIconBounds = { blockX, meterLabels.getY(), meterWidth, meterLabels.getHeight() };

    outputMeterIconBounds = { blockX + meterWidth + meterGap, meterLabels.getY(), meterWidth, meterLabels.getHeight() };



    auto meterBlock = juce::Rectangle<int>(blockX, meterRow.getY(), meterBlockWidth, meterRow.getHeight());

    inputMeter.setBounds(meterBlock.removeFromLeft(meterWidth));

    meterBlock.removeFromLeft(meterGap);

    outputMeter.setBounds(meterBlock.removeFromLeft(meterWidth));



    area.removeFromTop(6);

    volumeLabel.setBounds(area.removeFromTop(14));

    area.removeFromTop(2);

    volumeSlider.setBounds(area.removeFromTop(24));

    area.removeFromTop(6);

    muteButton.setBounds(area.removeFromTop(28).removeFromLeft(28));



    if (unavailableSourceMessage.isNotEmpty())

        unavailableLabel.setBounds(unavailableArea);

}

