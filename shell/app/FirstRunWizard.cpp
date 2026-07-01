#include "FirstRunWizard.h"

#include "DriverInstaller.h"
#include "SettingsStore.h"
#include "bridge/CxxString.h"
#include "bridge/RustBridge.h"

namespace
{
constexpr const char* kVirtualMicDisplayName = "Input Mixer Channel";

juce::String virtualMicSelectionLabel()
{
    const auto detected = input_mixer::RustBridge::detectedVirtualMicName();
    return detected.isNotEmpty() ? detected : juce::String(kVirtualMicDisplayName);
}

constexpr int kOuterPadding = 24;
constexpr int kTitleHeight = 32;
constexpr int kSectionGap = 12;
constexpr int kButtonRowHeight = 40;
constexpr int kButtonGap = 8;
constexpr int kFooterMinHeight = 36;
constexpr int kFooterMaxHeight = 72;

bool isVirtualDriverDetected()
{
    return input_mixer::RustBridge::detectVirtualDriver();
}

bool isVirtualDriverInstalledButNotLoaded()
{
    return ! isVirtualDriverDetected() && DriverInstaller::isBlackHoleHalOnDisk();
}

void styleWizardLabel(juce::Label& label, float fontSize, bool bold = false)
{
    label.setFont(juce::FontOptions(fontSize).withStyle(bold ? "Bold" : "Regular"));
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    label.setJustificationType(juce::Justification::topLeft);
}

void styleWizardBody(juce::TextEditor& editor)
{
    editor.setMultiLine(true, true);
    editor.setReadOnly(true);
    editor.setCaretVisible(false);
    editor.setScrollbarsShown(true);
    editor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1e1e22));
    editor.setColour(juce::TextEditor::textColourId, juce::Colour(0xffe8e8ec));
    editor.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    editor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    editor.setFont(juce::FontOptions(15.0f));
    editor.setIndents(12, 12);
}

void setFooterMessage(juce::Label& label, const juce::String& text, bool isError)
{
    label.setText(text, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId,
                    isError ? juce::Colour(0xffe85d5d) : juce::Colour(0xffe8c84d));
}
} // namespace

FirstRunWizard::FirstRunWizard(CompleteFn completeFn)
    : onComplete(std::move(completeFn))
{
    styleWizardLabel(titleLabel, 20.0f, true);
    styleWizardBody(bodyText);

    scanResultsLabel.setFont(juce::FontOptions(13.0f));
    scanResultsLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe8c84d));
    scanResultsLabel.setJustificationType(juce::Justification::centredLeft);

    nextButton.onClick = [this] { advance(); };
    backButton.onClick = [this] {
        switch (currentStep)
        {
            case Step::Driver: showStep(Step::Welcome); break;
            case Step::HardwareScan: showStep(Step::Driver); break;
            case Step::Permissions: showStep(Step::HardwareScan); break;
            case Step::Done: showStep(Step::Permissions); break;
            default: break;
        }
    };
    installButton.onClick = [this] { installDriver(); };
    manualDownloadButton.onClick = [] {
        juce::URL(DriverInstaller::blackHoleDownloadPageUrl()).launchInDefaultBrowser();
    };
#if JUCE_MAC
    openSettingsButton.onClick = [] {
        juce::URL("x-apple.systempreferences:com.apple.settings.PrivacySecurity.extension?Privacy_ScreenCapture")
            .launchInDefaultBrowser();
    };
#endif

    addAndMakeVisible(titleLabel);
    addAndMakeVisible(bodyText);
    addAndMakeVisible(nextButton);
    addAndMakeVisible(backButton);
    addAndMakeVisible(installButton);
    addAndMakeVisible(manualDownloadButton);
#if JUCE_MAC
    addAndMakeVisible(openSettingsButton);
#endif
    addAndMakeVisible(scanResultsLabel);

    installButton.setVisible(false);
    manualDownloadButton.setVisible(false);
#if JUCE_MAC
    openSettingsButton.setVisible(false);
#endif
    showStep(Step::Welcome);
}

void FirstRunWizard::updateDriverStepButtons()
{
#if JUCE_MAC
    const bool driverMissing = ! isVirtualDriverDetected();
    const bool needsRestart = isVirtualDriverInstalledButNotLoaded();

    installButton.setVisible(driverMissing && DriverInstaller::isDriverPackageAvailable());
    installButton.setButtonText(needsRestart ? "Restart audio services" : "Install virtual microphone");
    manualDownloadButton.setVisible(driverMissing && DriverInstaller::isDriverPackageAvailable());
    nextButton.setButtonText(driverMissing ? "Check again" : "Continue");
#else
    installButton.setVisible(false);
    manualDownloadButton.setVisible(false);
    nextButton.setButtonText(isVirtualDriverDetected() ? "Continue" : "Check again");
#endif
}

void FirstRunWizard::showStep(Step step)
{
    currentStep = step;
    backButton.setVisible(step != Step::Welcome);
    scanResultsLabel.setText({}, juce::dontSendNotification);
    scanResultsLabel.setVisible(false);
    installButton.setEnabled(true);
    nextButton.setEnabled(true);
    manualDownloadButton.setEnabled(true);
#if JUCE_MAC
    openSettingsButton.setEnabled(true);
    if (step != Step::Permissions)
        openSettingsButton.setVisible(false);
#endif

    switch (step)
    {
        case Step::Welcome:
            titleLabel.setText("Welcome to Input Mixer", juce::dontSendNotification);
            bodyText.setText(
                "Input Mixer combines up to six audio sources into one virtual microphone "
                "you can use in video calls and voice chat apps.\n\n"
                "This setup will check your audio devices, install the virtual microphone if needed, "
                "and request the permissions required to capture application audio.");
            nextButton.setButtonText("Continue");
            installButton.setVisible(false);
            manualDownloadButton.setVisible(false);
            break;

        case Step::Driver:
            titleLabel.setText("Virtual microphone", juce::dontSendNotification);
            if (isVirtualDriverDetected())
            {
                bodyText.setText(
                    "A virtual microphone is installed and ready to use.\n\n"
                    "In other apps, choose \"" + virtualMicSelectionLabel()
                        + "\" as your microphone. "
                        "Keep Input Mixer running while you use it.");
                nextButton.setButtonText("Continue");
                installButton.setVisible(false);
                manualDownloadButton.setVisible(false);
            }
            else if (isVirtualDriverInstalledButNotLoaded())
            {
                bodyText.setText(
                    "The virtual microphone driver is installed, but macOS has not loaded it yet.\n\n"
                    "Click Restart audio services below (macOS will ask for your administrator "
                    "password). If the microphone still does not appear, restart your Mac, then "
                    "click Check again.\n\n"
                    "In other apps, select \"" + virtualMicSelectionLabel()
                        + "\" as your microphone.");
                updateDriverStepButtons();
            }
            else
            {
#if JUCE_MAC
                bodyText.setText(
                    "Input Mixer needs a virtual microphone driver so other apps can hear your mix.\n\n"
                    "Setup installs the required audio driver (macOS will ask for your administrator "
                    "password). After installation, select \"Input Mixer Channel\" as your microphone "
                    "in other apps.\n\n"
                    "You can still hear your mix on speakers or headphones without installing anything.");
                updateDriverStepButtons();
#else
                bodyText.setText(
                    "The Input Mixer virtual microphone driver is not installed yet.\n\n"
                    "Install the driver package, then click Check again. Until the driver is "
                    "installed, mixed audio can still be monitored on your speakers but other apps "
                    "will not see Input Mixer as a microphone.");
#endif
            }
            break;

        case Step::HardwareScan:
            titleLabel.setText("Audio devices", juce::dontSendNotification);
            bodyText.setText("Scanning for microphones and speakers");
            nextButton.setButtonText("Continue");
            installButton.setVisible(false);
            manualDownloadButton.setVisible(false);
            scanHardware();
            break;

        case Step::Permissions:
            titleLabel.setText("Permissions", juce::dontSendNotification);
#if JUCE_MAC
            bodyText.setText(
                "Input Mixer needs microphone access for hardware sources.\n\n"
                "To capture audio from other apps, allow Screen Recording for Input Mixer in "
                "System Settings → Privacy & Security → Screen Recording.\n\n"
                "Use the button below to open that page. You can grant permission now or later "
                "when you choose an application source.");
            openSettingsButton.setVisible(true);
            if (input_mixer::RustBridge::shouldRequestMicrophoneAccess())
            {
                SettingsStore::get().setMicrophonePrompted(true);
                input_mixer::RustBridge::setMicrophonePrompted(true);
                input_mixer::RustBridge::requestMicrophoneAccess();
            }
            // Screen Recording: never auto-prompt; user opens System Settings explicitly.
#elif JUCE_WINDOWS
            bodyText.setText(
                "Input Mixer needs microphone access for hardware sources.\n\n"
                "Allow microphone access when Windows prompts you.");
#else
            bodyText.setText(
                "Input Mixer needs access to your audio devices through PipeWire.");
#endif
            nextButton.setButtonText("Continue");
            installButton.setVisible(false);
            manualDownloadButton.setVisible(false);
            break;

        case Step::Done:
            titleLabel.setText("You're all set", juce::dontSendNotification);
            bodyText.setText(
                "Input Mixer is ready. Choose your sources, adjust levels, and select "
                "\"" + virtualMicSelectionLabel()
                    + "\" as your microphone in apps like Chrome, Discord, or Slack.\n\n"
                "Keep Input Mixer running while you use the virtual microphone.");
            nextButton.setButtonText("Open Input Mixer");
            installButton.setVisible(false);
            manualDownloadButton.setVisible(false);
            break;
    }

    layoutContent();
}

void FirstRunWizard::installDriver()
{
#if JUCE_MAC
    const bool needsRestart = isVirtualDriverInstalledButNotLoaded();

    installButton.setEnabled(false);
    nextButton.setEnabled(false);
    manualDownloadButton.setEnabled(false);
    scanResultsLabel.setVisible(false);

    bodyText.setText(
        needsRestart
            ? "Restarting audio services so the virtual microphone appears. "
              "macOS will ask for your administrator password."
            : "Downloading and installing the virtual microphone driver. "
              "macOS will ask for your administrator password.");

    juce::Thread::launch([this] {
        juce::String error;
        const bool ok = DriverInstaller::installVirtualMicDriver(error);

        juce::MessageManager::callAsync([this, ok, error] {
            if (ok)
            {
                showStep(Step::Driver);
                return;
            }

            if (DriverInstaller::isBlackHoleHalOnDisk())
            {
                bodyText.setText(
                    "The virtual microphone driver was installed, but it is not available yet.\n\n"
                    + error
                    + "\n\nAfter restarting, click Check again. In other apps, select "
                      "\"" + virtualMicSelectionLabel() + "\" as your microphone.");
                setFooterMessage(scanResultsLabel,
                                 "Driver installed — restart your Mac, then click Check again.",
                                 false);
            }
            else
            {
                bodyText.setText(
                    "We could not install the virtual microphone.\n\n"
                    + error
                    + "\n\nYou can download the driver manually using the button below, "
                      "then click Check again.");
                setFooterMessage(scanResultsLabel, "Installation failed — see details above.", true);
            }

            scanResultsLabel.setVisible(true);
            updateDriverStepButtons();
            installButton.setEnabled(true);
            nextButton.setEnabled(true);
            manualDownloadButton.setEnabled(true);
            layoutContent();
        });
    });
#endif
}

void FirstRunWizard::scanHardware()
{
    const auto inputs = input_mixer::RustBridge::scanHardwareInputs();
    const auto outputs = input_mixer::RustBridge::scanHardwareOutputs();

    juce::String text;
    text << "Found " << inputs.size() << " input device(s) and "
         << outputs.size() << " output device(s).\n\n";

    text << "Inputs:\n";
    for (const auto& in : inputs)
        text << "  • " << input_mixer::toJuceString(in.descriptor.display_name) << "\n";

    text << "\nOutputs:\n";
    for (const auto& out : outputs)
        text << "  • " << out.name << "\n";

    bodyText.setText(text);

    if (inputs.empty())
    {
        setFooterMessage(scanResultsLabel,
                         "No microphones found. Connect a mic and run setup again.",
                         true);
        scanResultsLabel.setVisible(true);
    }
#if JUCE_MAC
    else if (input_mixer::RustBridge::isMicrophoneAccessDenied())
    {
        setFooterMessage(scanResultsLabel,
                         "Microphone access is denied. Enable Input Mixer in "
                         "System Settings → Privacy & Security → Microphone to capture hardware.",
                         true);
        scanResultsLabel.setVisible(true);
    }
#endif

    layoutContent();
}

void FirstRunWizard::advance()
{
    switch (currentStep)
    {
        case Step::Welcome: showStep(Step::Driver); break;
        case Step::Driver:
            if (isVirtualDriverDetected())
                showStep(Step::HardwareScan);
            else
                showStep(Step::Driver);
            break;
        case Step::HardwareScan: showStep(Step::Permissions); break;
        case Step::Permissions: showStep(Step::Done); break;
        case Step::Done:
            if (onComplete)
            {
                SettingsStore::get().setWizardCompleted(true);
                onComplete();
            }
            break;
    }
}

void FirstRunWizard::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff16161a));
}

void FirstRunWizard::resized()
{
    layoutContent();
}

void FirstRunWizard::layoutContent()
{
    auto area = getLocalBounds().reduced(kOuterPadding);

    titleLabel.setBounds(area.removeFromTop(kTitleHeight));
    area.removeFromTop(kSectionGap);

    const bool needsTwoButtonRows = area.getWidth() < 520
                                    && installButton.isVisible()
                                    && manualDownloadButton.isVisible();
    const int buttonRows = needsTwoButtonRows ? 2 : 1;
    auto buttonArea = area.removeFromBottom(kButtonRowHeight * buttonRows + (needsTwoButtonRows ? kButtonGap : 0));

    if (scanResultsLabel.isVisible())
    {
        area.removeFromBottom(kSectionGap);
        const auto footerHeight = juce::jlimit(kFooterMinHeight,
                                               kFooterMaxHeight,
                                               juce::roundToInt(scanResultsLabel.getFont().getHeight() * 3.0f));
        scanResultsLabel.setBounds(area.removeFromBottom(footerHeight));
    }

    area.removeFromBottom(kSectionGap);
    bodyText.setBounds(area);

    const int backWidth = 88;
    const int nextWidth = 132;
    const int manualWidth = 156;
    const int installWidth = 132;

    if (needsTwoButtonRows)
    {
        auto topRow = buttonArea.removeFromTop(kButtonRowHeight);
        buttonArea.removeFromTop(kButtonGap);
        auto bottomRow = buttonArea;

        backButton.setBounds(topRow.removeFromLeft(backWidth));
        installButton.setBounds(topRow.removeFromRight(installWidth));
        topRow.removeFromRight(kButtonGap);
        manualDownloadButton.setBounds(topRow.removeFromRight(manualWidth));
        nextButton.setBounds(bottomRow.removeFromRight(nextWidth));
    }
    else if (installButton.isVisible())
    {
        backButton.setBounds(buttonArea.removeFromLeft(backWidth));

        auto rightButtons = buttonArea;
        nextButton.setBounds(rightButtons.removeFromRight(nextWidth));
        rightButtons.removeFromRight(kButtonGap);

        if (manualDownloadButton.isVisible())
        {
            manualDownloadButton.setBounds(rightButtons.removeFromRight(manualWidth));
            rightButtons.removeFromRight(kButtonGap);
        }

        installButton.setBounds(rightButtons.removeFromRight(installWidth));
    }
    else
    {
        backButton.setBounds(buttonArea.removeFromLeft(backWidth));
        nextButton.setBounds(buttonArea.removeFromRight(nextWidth));
#if JUCE_MAC
        if (openSettingsButton.isVisible())
        {
            const int settingsWidth = 176;
            openSettingsButton.setBounds(buttonArea.removeFromRight(settingsWidth));
            buttonArea.removeFromRight(kButtonGap);
        }
#endif
    }
}
