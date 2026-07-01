#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class FirstRunWizard : public juce::Component
{
public:
    using CompleteFn = std::function<void()>;

    explicit FirstRunWizard(CompleteFn onComplete);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    enum class Step
    {
        Welcome,
        Driver,
        HardwareScan,
        Permissions,
        Done
    };

    void showStep(Step step);
    void advance();
    void scanHardware();
    void installDriver();
    void updateDriverStepButtons();
    void layoutContent();

    CompleteFn onComplete;
    Step currentStep = Step::Welcome;

    juce::Label titleLabel;
    juce::TextEditor bodyText;
    juce::TextButton nextButton { "Continue" };
    juce::TextButton backButton { "Back" };
    juce::TextButton installButton { "Install" };
    juce::TextButton manualDownloadButton { "Download driver" };
    juce::TextButton openSettingsButton { "Open System Settings" };
    juce::Label scanResultsLabel;
};
