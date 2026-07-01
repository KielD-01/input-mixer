#include "InputMixerApplication.h"

#include "FirstRunWizard.h"
#include "MainWindow.h"
#include "SettingsStore.h"
#include "bridge/RustBridge.h"
#include "ui/MixerLookAndFeel.h"
#include "ui/MixerTheme.h"

const juce::String InputMixerApplication::getApplicationName()
{
    return "Input Mixer";
}

const juce::String InputMixerApplication::getApplicationVersion()
{
    return "0.1.0";
}

bool InputMixerApplication::moreThanOneInstanceAllowed()
{
    return false;
}

void InputMixerApplication::initialise(const juce::String&)
{
    mixerLookAndFeel = std::make_unique<MixerLookAndFeel>();
    juce::LookAndFeel::setDefaultLookAndFeel(mixerLookAndFeel.get());

    MixerTheme::loadFromSettings();
    mixerLookAndFeel->applyTheme();

    input_mixer::RustBridge::initialise();

#if JUCE_MAC
    input_mixer::RustBridge::setMicrophonePrompted(SettingsStore::get().loadMicrophonePrompted());
#endif

    if (! SettingsStore::get().isWizardCompleted())
    {
        class WizardWindow : public juce::DocumentWindow
        {
        public:
            WizardWindow(InputMixerApplication& appRef)
                : DocumentWindow("Input Mixer Setup",
                                 MixerTheme::background(),
                                 DocumentWindow::closeButton),
                  app(appRef)
            {
                setContentOwned(new FirstRunWizard([this] {
                    SettingsStore::get().setWizardCompleted(true);
                    app.showMainWindow();
                    setVisible(false);
                }),
                                  true);
                setResizable(true, true);
                setResizeLimits(520, 460, 900, 900);
                centreWithSize(600, 540);
                setVisible(true);
            }

            void closeButtonPressed() override
            {
                quit();
            }

        private:
            InputMixerApplication& app;
        };

        wizardWindow = std::make_unique<WizardWindow>(*this);
    }
    else
    {
        showMainWindow();
    }
}

void InputMixerApplication::showMainWindow()
{
    if (mainWindow == nullptr)
        mainWindow = std::make_unique<MainWindow>();

    wizardWindow.reset();
}

void InputMixerApplication::shutdown()
{
    mainWindow.reset();
    wizardWindow.reset();
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    mixerLookAndFeel.reset();
    input_mixer::RustBridge::shutdown();
}

void InputMixerApplication::systemRequestedQuit()
{
    quit();
}

START_JUCE_APPLICATION(InputMixerApplication)
