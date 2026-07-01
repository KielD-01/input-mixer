#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>
#include <vector>

#include "SettingsStore.h"
#include "ui/ChannelStripComponent.h"
#include "ui/DesignModePanel.h"
#include "ui/MasterStripComponent.h"
#include "ui/MonitorStripComponent.h"

class MainWindow : public juce::DocumentWindow,
                   private juce::Timer,
                   private juce::MenuBarModel
{
public:
    MainWindow();
    ~MainWindow() override;

    void closeButtonPressed() override;
    void resized() override;
    void activeWindowStatusChanged() override;

private:
    class Content;
    class ChannelStripContainer;

    void timerCallback() override;
    void beginResizeGesture();
    void endResizeGesture();
    void setMeterAnimationsPaused(bool paused);
    void applyEngineConfig();
    void refreshChannelSourceMenus();
    void syncChannelsToEngine();
    void reconnectChannel(int channelIndex);
    void promptRestartAudioServices();
    void refreshTheme();
    void toggleDesignMode();
    void showSettingsMenu(juce::Component& target);

    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int topLevelMenuIndex, const juce::String& menuName) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

    std::unique_ptr<DesignModeWindow> designModeWindow;

    std::vector<input_mixer::SourceItem> getUsedSourcesExcluding(int channelIndex) const;

    int sourceRescanTicks = 0;
    bool resizeGestureActive = false;
    int resizeIdleFrames = 0;
    bool pendingSourceMenuRefresh = false;
    static constexpr int kResizeIdleFramesBeforeEnd = 5;
};
