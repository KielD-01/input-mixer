#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

class MixerLookAndFeel;

class InputMixerApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override;
    const juce::String getApplicationVersion() override;
    bool moreThanOneInstanceAllowed() override;

    void initialise(const juce::String& commandLine) override;
    void shutdown() override;
    void systemRequestedQuit() override;

    void showMainWindow();

private:

    std::unique_ptr<MixerLookAndFeel> mixerLookAndFeel;
    std::unique_ptr<juce::DocumentWindow> wizardWindow;
    std::unique_ptr<class MainWindow> mainWindow;
};
