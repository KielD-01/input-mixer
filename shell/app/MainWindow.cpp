#include "MainWindow.h"

#include <array>

#include "DriverInstaller.h"
#include "ui/MixerLookAndFeel.h"

#include "bridge/CxxString.h"

#include "bridge/RustBridge.h"

#include "bridge/SourceMatching.h"

#include "ui/MixerTheme.h"



namespace

{

constexpr int kUnavailableMissThreshold = 3;

constexpr float kActiveCaptureMeterThreshold = 0.0005f;

std::vector<input_mixer::SourceItem> collectUsedSources(

    const std::vector<std::unique_ptr<ChannelStripComponent>>& channels,

    int excludeChannelIndex)

{

    std::vector<input_mixer::SourceItem> used;

    for (int i = 0; i < static_cast<int>(channels.size()); ++i)

    {

        if (i == excludeChannelIndex)

            continue;



        const auto state = channels[static_cast<size_t>(i)]->getChannelState();

        if (state.hasSource)

            used.push_back({ state.source });

    }

    return used;

}



bool channelHasAssignedSource(const input_mixer::ChannelState& state)

{

    if (! state.hasSource)

        return false;



    if (state.source.kind == 0)

        return state.source.hardware_uid.size() > 0 || state.source.display_name.size() > 0;



    if (state.source.kind == 1)

        return state.source.app_bundle_id.size() > 0 || state.source.window_id != 0

               || state.source.display_name.size() > 0;



    return false;

}



bool isSourceCurrentlyAvailable(const input_mixer::ChannelState& state,

                                const std::vector<input_mixer::SourceItem>& hardware,

                                const std::vector<input_mixer::SourceItem>& applications)

{

    if (! channelHasAssignedSource(state))

        return true;



    return input_mixer::findMatchingSourceDescriptor(state.source, hardware, applications).has_value();

}



juce::String buildStatusText(uint8_t engineStatus)
{
    if (input_mixer::RustBridge::virtualMicNeedsAudioRestart())
        return "Status: Mic driver not loaded";

    if (input_mixer::RustBridge::virtualMicHalOnDisk() && ! input_mixer::RustBridge::detectVirtualDriver())
        return "Status: Mic not available";

#if JUCE_MAC
    if (input_mixer::RustBridge::isMicrophoneAccessDenied())
        return "Status: Mic access denied — open Settings → Privacy → Microphone";
#endif

    return "Status: " + input_mixer::RustBridge::statusLabel(engineStatus);
}

juce::String sourceDisplayName(const input_mixer::CxxSourceDescriptor& source);

#if JUCE_MAC
juce::String screenCaptureStatusDetail()
{
    if (input_mixer::RustBridge::hasScreenCaptureAccess())
        return {};

    switch (input_mixer::RustBridge::screenCaptureAccessState())
    {
        case 2:
            return "Restart Input Mixer to apply Screen Recording";
        case 3:
            return "Re-grant Screen Recording in System Settings (dev rebuild)";
        default:
            return "Allow Screen Recording for app audio";
    }
}

juce::String screenCaptureChannelDetail()
{
    if (input_mixer::RustBridge::hasScreenCaptureAccess())
        return {};

    switch (input_mixer::RustBridge::screenCaptureAccessState())
    {
        case 2:
            return "Restart Input Mixer to apply Screen Recording";
        case 3:
            return "Re-grant Screen Recording in System Settings";
        default:
            return "Allow Screen Recording in System Settings";
    }
}
#endif

juce::String captureChannelMessage(const input_mixer::ChannelState& state, bool captureUnavailable)
{
    if (! channelHasAssignedSource(state) || ! captureUnavailable)
        return {};

    if (state.source.kind == 0)
    {
#if JUCE_MAC
        if (input_mixer::RustBridge::isMicrophoneAccessDenied())
            return "Allow Microphone access in System Settings";
#endif
        return sourceDisplayName(state.source) + " could not be opened";
    }

    if (state.source.kind != 1)
        return {};

#if JUCE_MAC
    if (! input_mixer::RustBridge::hasScreenCaptureAccess())
        return screenCaptureChannelDetail();
#endif

    return "Couldn't capture audio — pick a specific window";
}

juce::String sourceDisplayName(const input_mixer::CxxSourceDescriptor& source)
{
    auto name = input_mixer::toJuceString(source.display_name);
    if (name.isEmpty())
        name = "Source";
    return name;
}

bool deduplicateChannelSources(const std::vector<std::unique_ptr<ChannelStripComponent>>& channels)
{
    std::vector<input_mixer::CxxSourceDescriptor> seen;
    bool anyCleared = false;

    for (const auto& strip : channels)
    {
        const auto state = strip->getChannelState();
        if (! channelHasAssignedSource(state))
            continue;

        bool duplicate = false;
        for (const auto& prior : seen)
        {
            if (input_mixer::sourcesEquivalent(prior, state.source))
            {
                duplicate = true;
                break;
            }
        }

        if (duplicate)
        {
            const auto message = sourceDisplayName(state.source) + " is already used on another channel";

            input_mixer::ChannelState cleared = state;
            cleared.hasSource = false;
            cleared.source = {};
            strip->setChannelState(cleared);
            strip->setUnavailableSourceMessage(message);
            anyCleared = true;
        }
        else
        {
            seen.push_back(state.source);
        }
    }

    return anyCleared;
}

bool statusTextIndicatesProblem(const juce::String& text)
{
    return text.contains("not") || text.contains("Permission") || text.contains("wrong")
           || text.contains("Stopped") || text.contains("Allow Screen Recording")
           || text.contains("Restart Input Mixer") || text.contains("Re-grant Screen Recording");
}

} // namespace



class MainWindow::ChannelStripContainer : public juce::Component

{

public:

    void setStrips(const std::vector<ChannelStripComponent*>& stripPtrs)

    {

        strips = stripPtrs;

        layoutStrips();

    }



    void layoutStrips()

    {

        resized();

    }



    void resized() override

    {

        const int count = static_cast<int>(strips.size());

        if (count == 0)

            return;



        const int gap = MixerTheme::kStripGap();

        const int totalGaps = juce::jmax(0, count - 1) * gap;

        const int parentWidth = getWidth() > 0 ? getWidth() : count * MixerTheme::kStripMinWidth();

        const int stripWidth = juce::jmax(MixerTheme::kStripMinWidth(),

                                          (parentWidth - totalGaps) / count);



        const int targetHeight = getParentHeight() > 0 ? getParentHeight() : getHeight();

        if (getWidth() != parentWidth || getHeight() != targetHeight)

            setSize(parentWidth, targetHeight);



        int x = 0;

        for (auto* strip : strips)

        {

            strip->setBounds(x, 0, stripWidth, getHeight());

            x += stripWidth + gap;

        }

    }



private:

    std::vector<ChannelStripComponent*> strips;

};



class MainWindow::Content : public juce::Component

{

public:

    Content(MainWindow& ownerRef) : owner(ownerRef)

    {

        setOpaque(true);

        headerLabel.setText("Input Mixer", juce::dontSendNotification);

        headerLabel.setFont(MixerTheme::headerFont());

        statusLabel.setFont(MixerTheme::smallFont());

        statusLabel.setColour(juce::Label::textColourId, MixerTheme::textMuted());

        statusLabel.setJustificationType(juce::Justification::centredLeft);

        settingsButton.getProperties().set("iconSettings", true);

        settingsButton.onClick = [this] { owner.showSettingsMenu(settingsButton); };



        masterStrip.setOnChange([](float vol, bool muted) {

            SettingsStore::get().saveMaster(vol, muted);

            input_mixer::RustBridge::setMaster(vol, muted);

        });



        monitorStrip.setOnChange([](bool enabled, const juce::String& uid, float vol) {

            SettingsStore::get().saveMonitor(enabled, vol, uid);

            input_mixer::RustBridge::setMonitor(enabled, uid, vol);

        });



        channelViewport.setViewedComponent(&channelContainer, false);

        channelViewport.setScrollBarsShown(true, false);

        channelViewport.setScrollBarThickness(10);



        addAndMakeVisible(headerLabel);

        addAndMakeVisible(statusLabel);

        addAndMakeVisible(settingsButton);

        addAndMakeVisible(channelViewport);

        addAndMakeVisible(masterStrip);

        addAndMakeVisible(monitorStrip);

    }



    void addChannelStrip(std::unique_ptr<ChannelStripComponent> strip)

    {

        auto* raw = strip.get();

        channels.push_back(std::move(strip));

        channelContainer.addAndMakeVisible(raw);

        rebuildStripPointers();

        rebuildStripLayout();

    }



    void rebuildStripPointers()

    {

        std::vector<ChannelStripComponent*> stripPtrs;

        stripPtrs.reserve(channels.size());

        for (const auto& channel : channels)

            stripPtrs.push_back(channel.get());



        channelContainer.setStrips(stripPtrs);

    }



    bool handleUnavailableChannelSources()
    {
        if (sourceScanInProgress)
            return false;

        if (cachedHardwareSources.empty() && cachedApplicationSources.empty())
            return false;

        const auto nowMs = juce::Time::getMillisecondCounterHiRes();
        const bool inStartupGrace = (nowMs - startupTimeMs) < 5000.0;

        bool anyCleared = false;

        for (size_t i = 0; i < channels.size(); ++i)
        {
            auto* strip = channels[i].get();
            const auto state = strip->getChannelState();

            if (! channelHasAssignedSource(state))
            {
                sourceUnavailableMissCount[i] = 0;
                continue;
            }

            if (isSourceCurrentlyAvailable(state, cachedHardwareSources, cachedApplicationSources))
            {
                sourceUnavailableMissCount[i] = 0;
                continue;
            }

            if (juce::isPositiveAndBelow(static_cast<int>(i), static_cast<int>(lastChannelInputPeaks.size()))
                && lastChannelInputPeaks[i] > kActiveCaptureMeterThreshold)
            {
                sourceUnavailableMissCount[i] = 0;
                continue;
            }

            if (inStartupGrace)
                continue;

            ++sourceUnavailableMissCount[i];

            if (sourceUnavailableMissCount[i] < kUnavailableMissThreshold)
                continue;

            const auto message = sourceDisplayName(state.source) + " is not currently available";

            input_mixer::ChannelState cleared = state;
            cleared.hasSource = false;
            cleared.source = {};

            strip->setChannelState(cleared);
            strip->setUnavailableSourceMessage(message);
            sourceUnavailableMissCount[i] = 0;
            anyCleared = true;
        }

        return anyCleared;
    }

    void persistChannelStates()
    {
        std::vector<input_mixer::ChannelState> states;
        states.reserve(6);

        for (const auto& strip : channels)
            states.push_back(strip->getChannelState());

        while (states.size() < 6)
            states.emplace_back();

        SettingsStore::get().saveChannels(states);

        const auto err = input_mixer::RustBridge::setChannels(states);
        if (err.isNotEmpty())
            statusLabel.setText("Status: " + err, juce::dontSendNotification);
    }



    void paint(juce::Graphics& g) override

    {

        g.fillAll(MixerTheme::background());

    }



    void resized() override

    {

        auto area = getLocalBounds().reduced(MixerTheme::kOuterPadding());

        auto header = area.removeFromTop(36);

        headerLabel.setBounds(header.removeFromLeft(200));

        constexpr int settingsSize = 32;

        settingsButton.setBounds(header.removeFromRight(settingsSize).withSizeKeepingCentre(settingsSize, settingsSize));

        area.removeFromTop(8);



        auto footer = area.removeFromBottom(22);

        statusLabel.setBounds(footer);



        auto rightPanel = area.removeFromRight(MixerTheme::kRightPanelWidth());

        const int masterHeight = juce::roundToInt(static_cast<float>(rightPanel.getHeight())

                                                  * static_cast<float>(MixerTheme::kMasterColumnMasterRatioPercent())

                                                  / 100.0f);

        masterStrip.setBounds(rightPanel.removeFromTop(masterHeight));

        rightPanel.removeFromTop(8);

        monitorStrip.setBounds(rightPanel);



        channelViewport.setBounds(area);

        channelContainer.setSize(area.getWidth(), area.getHeight());

        rebuildStripLayout();

    }



    void rebuildStripLayout()

    {

        channelContainer.layoutStrips();

    }



    juce::Label headerLabel;

    juce::Label statusLabel;

    juce::TextButton settingsButton;

    juce::Viewport channelViewport;

    ChannelStripContainer channelContainer;

    MasterStripComponent masterStrip;

    MonitorStripComponent monitorStrip;

    std::vector<std::unique_ptr<ChannelStripComponent>> channels;

    std::vector<input_mixer::SourceItem> cachedHardwareSources;

    std::vector<input_mixer::SourceItem> cachedApplicationSources;

    double startupTimeMs = juce::Time::getMillisecondCounterHiRes();

    bool sourceScanInProgress = false;

    bool completedInitialSourceScan = false;

    std::array<float, SettingsStore::kMaxChannels> lastChannelInputPeaks {};

    std::array<int, SettingsStore::kMaxChannels> sourceUnavailableMissCount {};

    MainWindow& owner;



    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Content)

};



MainWindow::MainWindow()

    : DocumentWindow("Input Mixer",

                     MixerTheme::background(),

                     DocumentWindow::allButtons)

{

    input_mixer::RustBridge::startEngine();



    auto panel = std::make_unique<Content>(*this);



    const auto channelStates = SettingsStore::get().loadChannels();

    for (int i = 0; i < SettingsStore::kMaxChannels; ++i)

    {

        auto strip = std::make_unique<ChannelStripComponent>(i);

        strip->setChannelState(channelStates[static_cast<size_t>(i)]);



        strip->setOnChange([this] { syncChannelsToEngine(); });

        strip->setOnReconnect([this](int channelIndex) { reconnectChannel(channelIndex); });

        strip->setOnSourceDropdownOpening([this](int) { refreshChannelSourceMenus(); });



        panel->addChannelStrip(std::move(strip));

    }



    panel->masterStrip.setVolume(SettingsStore::get().loadMasterVolume());

    panel->masterStrip.setMuted(SettingsStore::get().loadMasterMuted());

    panel->monitorStrip.setEnabled(SettingsStore::get().loadMonitorEnabled());

    panel->monitorStrip.setVolume(SettingsStore::get().loadMonitorVolume());

    panel->monitorStrip.setDeviceUid(SettingsStore::get().loadMonitorDeviceUid());

    panel->monitorStrip.refreshOutputDevices();



    setContentOwned(panel.release(), true);

    setResizable(true, true);

    setResizeLimits(MixerTheme::kMinWindowWidth(),

                    MixerTheme::kMinWindowHeight(),

                    10000,

                    10000);

    centreWithSize(1100, 720);

#if JUCE_MAC
    MenuBarModel::setMacMainMenu(this);
#endif

#if JUCE_MAC
    if (input_mixer::RustBridge::virtualMicNeedsAudioRestart())

    {

        juce::MessageManager::callAsync([this] {

            juce::AlertWindow::showOkCancelBox(

                juce::AlertWindow::WarningIcon,

                "Restart audio services",

                "The virtual microphone driver is installed, but macOS has not loaded it yet.\n\n"

                "Input Mixer can restart audio services now (administrator password required). "

                "After that, choose \"Input Mixer Channel\" as your microphone in Chrome, Meet, and other apps.",

                "Restart now",

                "Later",

                this,

                juce::ModalCallbackFunction::create([this](int result) {

                    if (result == 1)

                        promptRestartAudioServices();

                }));

        });

    }

#endif



    setVisible(true);



    applyEngineConfig();

    refreshChannelSourceMenus();

    startTimerHz(30);

}



MainWindow::~MainWindow()
{
#if JUCE_MAC
    MenuBarModel::setMacMainMenu(nullptr);
#endif

    input_mixer::RustBridge::stopEngine();
}



void MainWindow::closeButtonPressed()

{

    juce::JUCEApplication::getInstance()->systemRequestedQuit();

}



void MainWindow::activeWindowStatusChanged()

{

    DocumentWindow::activeWindowStatusChanged();

#if JUCE_MAC

    if (isActiveWindow())

        input_mixer::RustBridge::refreshScreenCaptureAccess();

#endif

}



void MainWindow::resized()

{

    DocumentWindow::resized();

    beginResizeGesture();

}



void MainWindow::beginResizeGesture()

{

    if (! resizeGestureActive)

        setMeterAnimationsPaused(true);



    resizeGestureActive = true;

    resizeIdleFrames = 0;

}



void MainWindow::endResizeGesture()

{

    if (! resizeGestureActive)

        return;



    resizeGestureActive = false;

    resizeIdleFrames = 0;

    setMeterAnimationsPaused(false);



    if (pendingSourceMenuRefresh)

    {

        pendingSourceMenuRefresh = false;

        refreshChannelSourceMenus();

    }

}



void MainWindow::setMeterAnimationsPaused(bool paused)

{

    if (auto* c = dynamic_cast<Content*>(getContentComponent()))

    {

        for (auto& strip : c->channels)

            strip->setMeterAnimationPaused(paused);



        c->masterStrip.setMeterAnimationPaused(paused);

        c->monitorStrip.setMeterAnimationPaused(paused);

    }

}



void MainWindow::timerCallback()

{

    if (resizeGestureActive)

    {

        if (++resizeIdleFrames >= kResizeIdleFramesBeforeEnd)

            endResizeGesture();

    }



    if (auto* c = dynamic_cast<Content*>(getContentComponent()))

    {

        if (! resizeGestureActive)

        {

            const auto status = input_mixer::RustBridge::getEngineStatus();

            juce::String statusText = buildStatusText(status);

#if JUCE_MAC
            for (const auto& strip : c->channels)
            {
                const auto state = strip->getChannelState();
                if (channelHasAssignedSource(state) && state.source.kind == 1
                    && ! input_mixer::RustBridge::hasScreenCaptureAccess())
                {
                    const auto detail = screenCaptureStatusDetail();
                    if (detail.isNotEmpty())
                        statusText = "Status: " + detail;
                    break;
                }
            }
#endif

            c->statusLabel.setText(statusText, juce::dontSendNotification);

            c->statusLabel.setColour(juce::Label::textColourId,

                                     statusTextIndicatesProblem(statusText) ? MixerTheme::warning()

                                                                            : MixerTheme::textMuted());

        }



        const auto meters = input_mixer::RustBridge::getMeterLevels();

        for (int i = 0; i < static_cast<int>(c->channels.size()); ++i)

        {

            float inPeak = 0.0f;

            float outPeak = 0.0f;

            if (juce::isPositiveAndBelow(i, static_cast<int>(meters.channel_input_peaks.size())))

                inPeak = meters.channel_input_peaks[static_cast<size_t>(i)];

            if (juce::isPositiveAndBelow(i, static_cast<int>(meters.channel_output_peaks.size())))

                outPeak = meters.channel_output_peaks[static_cast<size_t>(i)];

            if (juce::isPositiveAndBelow(i, static_cast<int>(c->lastChannelInputPeaks.size())))

                c->lastChannelInputPeaks[static_cast<size_t>(i)] = inPeak;

            c->channels[static_cast<size_t>(i)]->setMeterLevels(inPeak, outPeak);

            const auto state = c->channels[static_cast<size_t>(i)]->getChannelState();

            bool captureUnavailable = false;

            if (juce::isPositiveAndBelow(i, static_cast<int>(meters.channel_unavailable.size())))

                captureUnavailable = meters.channel_unavailable[static_cast<size_t>(i)];

            c->channels[static_cast<size_t>(i)]->setUnavailableSourceMessage(

                captureChannelMessage(state, captureUnavailable));

        }



        c->masterStrip.setPeak(meters.master_peak);

        c->monitorStrip.setPeak(meters.monitor_peak);



        if (! resizeGestureActive && ++sourceRescanTicks >= 90)

        {

            sourceRescanTicks = 0;

            refreshChannelSourceMenus();

        }

    }

}



void MainWindow::applyEngineConfig()

{

    syncChannelsToEngine();

    input_mixer::RustBridge::setMaster(SettingsStore::get().loadMasterVolume(),

                                       SettingsStore::get().loadMasterMuted());

    input_mixer::RustBridge::setMonitor(SettingsStore::get().loadMonitorEnabled(),

                                        SettingsStore::get().loadMonitorDeviceUid(),

                                        SettingsStore::get().loadMonitorVolume());

}



void MainWindow::syncChannelsToEngine()

{

    auto* c = dynamic_cast<Content*>(getContentComponent());

    if (c == nullptr)

        return;



    std::vector<input_mixer::ChannelState> states;

    states.reserve(6);



    for (const auto& strip : c->channels)

        states.push_back(strip->getChannelState());



    while (states.size() < 6)

        states.emplace_back();



    input_mixer::resolveChannelSources(states, c->cachedHardwareSources, c->cachedApplicationSources);



    for (size_t i = 0; i < c->channels.size() && i < states.size(); ++i)

        c->channels[i]->setChannelState(states[i]);



    SettingsStore::get().saveChannels(states);

    const auto err = input_mixer::RustBridge::setChannels(states);

    if (err.isNotEmpty())

        c->statusLabel.setText("Status: " + err, juce::dontSendNotification);



    refreshChannelSourceMenus();

}



void MainWindow::reconnectChannel(int channelIndex)

{

    auto* c = dynamic_cast<Content*>(getContentComponent());

    if (c == nullptr)

        return;



    refreshChannelSourceMenus();



    if (juce::isPositiveAndBelow(channelIndex, static_cast<int>(c->channels.size())))

        c->channels[static_cast<size_t>(channelIndex)]->setUnavailableSourceMessage({});



    const auto err = input_mixer::RustBridge::reopenChannel(channelIndex);

    if (err.isNotEmpty())

        c->statusLabel.setText("Status: " + err, juce::dontSendNotification);

}



void MainWindow::refreshChannelSourceMenus()

{

    if (resizeGestureActive)

    {

        pendingSourceMenuRefresh = true;

        return;

    }



    auto* c = dynamic_cast<Content*>(getContentComponent());

    if (c == nullptr)

        return;



    c->sourceScanInProgress = true;



    c->cachedHardwareSources = input_mixer::RustBridge::scanHardwareInputs();

    c->cachedApplicationSources = input_mixer::RustBridge::scanApplicationSources();

    c->sourceScanInProgress = false;

    c->completedInitialSourceScan = true;

    std::vector<input_mixer::ChannelState> states;

    states.reserve(c->channels.size());

    for (const auto& strip : c->channels)

        states.push_back(strip->getChannelState());



    input_mixer::resolveChannelSources(states, c->cachedHardwareSources, c->cachedApplicationSources);



    for (size_t i = 0; i < c->channels.size() && i < states.size(); ++i)

        c->channels[i]->setChannelState(states[i]);



    const bool anyDeduped = deduplicateChannelSources(c->channels);

    const bool anyCleared = c->handleUnavailableChannelSources() || anyDeduped;

    if (anyCleared)
        c->persistChannelStates();

    for (int i = 0; i < static_cast<int>(c->channels.size()); ++i)

    {

        c->channels[static_cast<size_t>(i)]->refreshSourceCombo(

            c->cachedHardwareSources,

            c->cachedApplicationSources,

            collectUsedSources(c->channels, i));

    }

}



void MainWindow::promptRestartAudioServices()

{

#if JUCE_MAC

    juce::String message;

    if (DriverInstaller::restartCoreAudioServices(message))

    {

        refreshChannelSourceMenus();

        applyEngineConfig();



        if (input_mixer::RustBridge::detectVirtualDriver())

        {

            juce::AlertWindow::showMessageBoxAsync(

                juce::AlertWindow::InfoIcon,

                "Virtual microphone ready",

                "Virtual microphone ready.\n\n"

                "In Google Meet or Chrome, open microphone settings and choose \"Input Mixer Channel\".");

        }

        else

        {

            juce::AlertWindow::showMessageBoxAsync(

                juce::AlertWindow::WarningIcon,

                "Restart still needed",

                "Audio services were restarted, but the virtual microphone is still not loaded.\n\n"

                "Restart your Mac, then open Input Mixer and use Settings → Restart audio services.");

        }

    }

    else

    {

        juce::AlertWindow::showMessageBoxAsync(

            juce::AlertWindow::WarningIcon,

            "Could not restart audio services",

            message.isNotEmpty() ? message : "Try restarting your Mac manually.");

    }

#endif

}



std::vector<input_mixer::SourceItem> MainWindow::getUsedSourcesExcluding(int channelIndex) const

{

    if (auto* c = dynamic_cast<const Content*>(getContentComponent()))

        return collectUsedSources(c->channels, channelIndex);

    return {};

}

void MainWindow::refreshTheme()
{
    if (auto* laf = dynamic_cast<MixerLookAndFeel*>(&getLookAndFeel()))
        laf->applyTheme();

    setBackgroundColour(MixerTheme::background());

    if (auto* c = dynamic_cast<Content*>(getContentComponent()))
    {
        c->statusLabel.setColour(juce::Label::textColourId, MixerTheme::textMuted());
        c->repaint();
        c->resized();
    }

    repaint();
}

void MainWindow::toggleDesignMode()
{
    if (designModeWindow == nullptr)
    {
        designModeWindow = std::make_unique<DesignModeWindow>([this] { refreshTheme(); });
        return;
    }

    if (designModeWindow->isVisible())
        designModeWindow->setVisible(false);
    else
        designModeWindow->setVisible(true);
}

void MainWindow::showSettingsMenu(juce::Component& target)
{
    juce::PopupMenu menu;
    menu.addItem(1, "Run setup again");
    menu.addItem(2, "Design Mode", true, designModeWindow != nullptr && designModeWindow->isVisible());
#if JUCE_MAC
    menu.addItem(3, "Open System Settings");
    if (input_mixer::RustBridge::isMicrophoneAccessDenied())
        menu.addItem(5, "Open Microphone Settings");
    if (input_mixer::RustBridge::isAdhocBuild() && ! input_mixer::RustBridge::hasScreenCaptureAccess())
        menu.addItem(6, "Re-grant Screen Recording after rebuild");
    menu.addItem(4, "Restart audio services");
#endif

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&target),
                       [this](int result) {
                           switch (result)
                           {
                               case 1:
                                   SettingsStore::get().setWizardCompleted(false);
                                   juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                                                          "Setup",
                                                                          "Restart the app to run setup again.");
                                   break;
                               case 2:
                                   toggleDesignMode();
                                   break;
#if JUCE_MAC
                               case 3:
                                   juce::URL("x-apple.systempreferences:com.apple.settings.PrivacySecurity.extension")
                                       .launchInDefaultBrowser();
                                   break;
                               case 5:
                                   juce::URL("x-apple.systempreferences:com.apple.settings.PrivacySecurity.extension?Privacy_Microphone")
                                       .launchInDefaultBrowser();
                                   break;
                               case 6:
                                   juce::URL("x-apple.systempreferences:com.apple.settings.PrivacySecurity.extension?Privacy_ScreenCapture")
                                       .launchInDefaultBrowser();
                                   break;
                               case 4:
                                   promptRestartAudioServices();
                                   break;
#endif
                               default:
                                   break;
                           }
                       });
}

juce::StringArray MainWindow::getMenuBarNames()
{
    return { "View" };
}

juce::PopupMenu MainWindow::getMenuForIndex(int topLevelMenuIndex, const juce::String& menuName)
{
    juce::PopupMenu menu;
    juce::ignoreUnused(topLevelMenuIndex, menuName);
    menu.addItem(1, "Design Mode", true, designModeWindow != nullptr && designModeWindow->isVisible());
    return menu;
}

void MainWindow::menuItemSelected(int menuItemID, int topLevelMenuIndex)
{
    juce::ignoreUnused(topLevelMenuIndex);
    if (menuItemID == 1)
        toggleDesignMode();
}


