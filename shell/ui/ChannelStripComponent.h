#pragma once



#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>



#include "LevelMeterComponent.h"

#include "bridge/RustBridge.h"



class ChannelStripComponent : public juce::Component

{

public:

    enum class SourceFilter

    {

        all,

        hardware,

        software

    };



    using ChangeFn = std::function<void()>;
    using ReconnectFn = std::function<void(int channelIndex)>;



    ChannelStripComponent(int channelIndex);



    void setChannelState(const input_mixer::ChannelState& state);

    input_mixer::ChannelState getChannelState() const;



    void refreshSourceCombo(const std::vector<input_mixer::SourceItem>& hardware,

                            const std::vector<input_mixer::SourceItem>& applications,

                            const std::vector<input_mixer::SourceItem>& usedOnOtherChannels);



    void setMeterLevels(float inputPeak, float outputPeak);
    void setMeterAnimationPaused(bool paused);

    void setUnavailableSourceMessage(const juce::String& message);



    void setOnChange(ChangeFn fn);
    void setOnReconnect(ReconnectFn fn);
    void setOnSourceDropdownOpening(std::function<void(int)> fn);



    void paint(juce::Graphics& g) override;

    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;



private:

    static constexpr int kNoneItemId = 1;

    static constexpr int kFirstSourceItemId = 2;



    bool sourceMatches(const input_mixer::CxxSourceDescriptor& a,

                       const input_mixer::CxxSourceDescriptor& b) const;



    juce::String formatSourceName(const input_mixer::CxxSourceDescriptor& descriptor) const;

    void updateControlsEnabled();

    void updateVolumeLabel();

    void setSourceFilter(SourceFilter filter);

    void updateFilterButtonStyles();

    void rebuildComboFromFilter(const std::vector<input_mixer::SourceItem>& usedOnOtherChannels);

    bool hasAssignedSource() const;



    int index;

    input_mixer::ChannelState state;

    juce::String unavailableSourceMessage;

    bool updatingCombo = false;

    SourceFilter activeFilter = SourceFilter::all;



    std::vector<input_mixer::SourceItem> cachedHardwareSources;

    std::vector<input_mixer::SourceItem> cachedApplicationSources;

    std::vector<input_mixer::SourceItem> cachedUsedOnOtherChannels;

    std::vector<input_mixer::SourceItem> comboSources;



    juce::Label titleLabel;

    juce::Label sourceFieldLabel;

    juce::ComboBox sourceCombo;

    juce::TextButton reconnectButton;

    juce::TextButton allSourcesFilterButton { "All" };

    juce::TextButton hardwareFilterButton;

    juce::TextButton softwareFilterButton;

    juce::Rectangle<int> inputMeterIconBounds;

    juce::Rectangle<int> outputMeterIconBounds;

    LevelMeterComponent inputMeter;

    LevelMeterComponent outputMeter;

    juce::Label volumeLabel;

    juce::Slider volumeSlider;

    juce::ToggleButton muteButton;

    juce::Label unavailableLabel;



    ChangeFn onChange;

    ReconnectFn onReconnect;

    std::function<void(int)> onSourceDropdownOpening;

};

