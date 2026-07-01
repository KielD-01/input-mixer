#include "DesignModePanel.h"

#include "MixerTheme.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace
{
constexpr int kRowHeight = 28;
constexpr int kRowGap = 2;
constexpr int kSectionGap = 10;
constexpr int kContentPadding = 12;
constexpr int kButtonHeight = 28;
constexpr int kButtonGap = 6;
constexpr int kScrollBarWidth = 12;

void showImportResult(bool ok, const juce::String& detail)
{
    juce::AlertWindow::showMessageBoxAsync(
        ok ? juce::AlertWindow::InfoIcon : juce::AlertWindow::WarningIcon,
        ok ? "Theme imported" : "Could not import",
        detail);
}
} // namespace

DesignModePanel::ColourRow::ColourRow(const juce::String& labelText,
                                     juce::Colour& colourRef,
                                     std::function<void()> onChange)
    : colour(colourRef),
      changeHandler(std::move(onChange))
{
    label.setText(labelText, juce::dontSendNotification);
    label.setFont(MixerTheme::labelFont());
    label.setColour(juce::Label::textColourId, MixerTheme::textPrimary());
    addAndMakeVisible(label);

    swatch.setColour(juce::TextButton::buttonColourId, colour);
    swatch.onClick = [this] {
        auto selector = std::make_unique<juce::ColourSelector>(juce::ColourSelector::showColourAtTop
                                                               | juce::ColourSelector::showSliders
                                                               | juce::ColourSelector::showColourspace);
        selector->setCurrentColour(colour);
        selector->setSize(300, 380);

        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(selector.release());
        options.dialogTitle = label.getText();
        options.dialogBackgroundColour = MixerTheme::panel();
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;
        options.componentToCentreAround = this;

        if (auto* dialog = options.launchAsync())
        {
            dialog->enterModalState(true, juce::ModalCallbackFunction::create([this, dialog](int) {
                if (auto* picker = dynamic_cast<juce::ColourSelector*>(dialog->getContentComponent()))
                {
                    colour = picker->getCurrentColour();
                    swatch.setColour(juce::TextButton::buttonColourId, colour);
                    if (changeHandler)
                        changeHandler();
                }
            }));
        }
    };

    addAndMakeVisible(swatch);
}

void DesignModePanel::ColourRow::paint(juce::Graphics& g)
{
    g.fillAll(MixerTheme::panel());
}

void DesignModePanel::ColourRow::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromLeft(160));
    swatch.setBounds(area.removeFromLeft(72).reduced(2));
}

void DesignModePanel::ColourRow::syncFromTheme()
{
    swatch.setColour(juce::TextButton::buttonColourId, colour);
    repaint();
}

void DesignModePanel::IntSliderRow::syncFromTheme()
{
    slider.setValue(value, juce::dontSendNotification);
}

DesignModePanel::IntSliderRow::IntSliderRow(const juce::String& labelText,
                                            int& valueRef,
                                            const int min,
                                            const int max,
                                            std::function<void()> onChange)
    : value(valueRef),
      changeHandler(std::move(onChange))
{
    label.setText(labelText, juce::dontSendNotification);
    label.setFont(MixerTheme::labelFont());
    label.setColour(juce::Label::textColourId, MixerTheme::textPrimary());
    addAndMakeVisible(label);

    slider.setRange(min, max, 1.0);
    slider.setValue(value, juce::dontSendNotification);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, kRowHeight);
    slider.onValueChange = [this] {
        value = static_cast<int>(slider.getValue());
        if (changeHandler)
            changeHandler();
    };
    addAndMakeVisible(slider);
}

void DesignModePanel::IntSliderRow::resized()
{
    auto area = getLocalBounds();
    label.setBounds(area.removeFromLeft(160));
    slider.setBounds(area);
}

DesignModePanel::DesignModePanel(ThemeChangedCallback onThemeChanged)
    : themeChanged(std::move(onThemeChanged))
{
    setOpaque(true);

    coloursHeading.setFont(MixerTheme::sectionTitleFont());
    coloursHeading.setColour(juce::Label::textColourId, MixerTheme::textPrimary());
    layoutHeading.setFont(MixerTheme::sectionTitleFont());
    layoutHeading.setColour(juce::Label::textColourId, MixerTheme::textPrimary());

    auto notify = [this] { notifyThemeChanged(); };

    auto& t = MixerTheme::state();
    colourRows.push_back(std::make_unique<ColourRow>("Background", t.background, notify));
    colourRows.push_back(std::make_unique<ColourRow>("Strip panel", t.panel, notify));
    colourRows.push_back(std::make_unique<ColourRow>("Accent", t.accent, notify));
    colourRows.push_back(std::make_unique<ColourRow>("Primary text", t.textPrimary, notify));
    colourRows.push_back(std::make_unique<ColourRow>("Muted text", t.textMuted, notify));
    colourRows.push_back(std::make_unique<ColourRow>("Fader track", t.faderTrack, notify));
    colourRows.push_back(std::make_unique<ColourRow>("Fader accent", t.faderFill, notify));
    colourRows.push_back(std::make_unique<ColourRow>("Fader thumb", t.faderThumb, notify));
    colourRows.push_back(std::make_unique<ColourRow>("Meter green", t.meterGreen, notify));
    colourRows.push_back(std::make_unique<ColourRow>("Meter yellow", t.meterYellow, notify));
    colourRows.push_back(std::make_unique<ColourRow>("Meter red", t.meterRed, notify));
    colourRows.push_back(std::make_unique<ColourRow>("Separator", t.separator, notify));
    colourRows.push_back(std::make_unique<ColourRow>("Button off", t.buttonOff, notify));
    colourRows.push_back(std::make_unique<ColourRow>("Button on", t.buttonOn, notify));

    layoutRows.push_back(std::make_unique<IntSliderRow>("Strip width", t.stripMinWidth, 80, 220, notify));
    layoutRows.push_back(std::make_unique<IntSliderRow>("Strip gap", t.stripGap, 0, 24, notify));
    layoutRows.push_back(std::make_unique<IntSliderRow>("Outer padding", t.outerPadding, 0, 32, notify));
    layoutRows.push_back(std::make_unique<IntSliderRow>("Strip padding", t.stripInnerPadding, 0, 24, notify));
    layoutRows.push_back(std::make_unique<IntSliderRow>("Panel corner radius", t.panelCornerRadius, 0, 16, notify));
    layoutRows.push_back(std::make_unique<IntSliderRow>("Right panel width", t.rightPanelWidth, 160, 360, notify));
    layoutRows.push_back(std::make_unique<IntSliderRow>("Master height %", t.masterColumnMasterRatioPercent, 30, 80, notify));
    layoutRows.push_back(std::make_unique<IntSliderRow>("Meter segments", t.meterSegments, 4, 24, notify));

    for (auto& row : colourRows)
        content.addAndMakeVisible(*row);
    for (auto& row : layoutRows)
        content.addAndMakeVisible(*row);

    content.addAndMakeVisible(coloursHeading);
    content.addAndMakeVisible(layoutHeading);
    content.addAndMakeVisible(resetButton);
    content.addAndMakeVisible(exportButton);
    content.addAndMakeVisible(importButton);
    content.addAndMakeVisible(importDrawdioButton);
    content.addAndMakeVisible(openDrawdioButton);

    resetButton.onClick = [this] { resetToDefaults(); };
    exportButton.onClick = [this] { exportTheme(); };
    importButton.onClick = [this] { importTheme(); };
    importDrawdioButton.onClick = [this] { importDrawdio(); };
    openDrawdioButton.onClick = [this] { openDrawdio(); };

    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
    viewport.setScrollBarThickness(kScrollBarWidth);
    addAndMakeVisible(viewport);
}

void DesignModePanel::paint(juce::Graphics& g)
{
    g.fillAll(MixerTheme::background());
}

void DesignModePanel::resized()
{
    viewport.setBounds(getLocalBounds());

    int y = kContentPadding;
    const int width = juce::jmax(360, getWidth() - kScrollBarWidth - kContentPadding);

    coloursHeading.setBounds(kContentPadding, y, width, 22);
    y += 26;

    for (auto& row : colourRows)
    {
        row->setBounds(kContentPadding, y, width, kRowHeight);
        y += kRowHeight + kRowGap;
    }

    y += kSectionGap;
    layoutHeading.setBounds(kContentPadding, y, width, 22);
    y += 26;

    for (auto& row : layoutRows)
    {
        row->setBounds(kContentPadding, y, width, kRowHeight);
        y += kRowHeight + kRowGap;
    }

    y += kSectionGap;

    juce::TextButton* buttons[] = {
        &resetButton,
        &exportButton,
        &importButton,
        &importDrawdioButton,
        &openDrawdioButton,
    };

    for (auto* button : buttons)
    {
        button->setBounds(kContentPadding, y, width, kButtonHeight);
        y += kButtonHeight + kButtonGap;
    }

    content.setSize(width + kContentPadding * 2, y - kButtonGap + kContentPadding);
}

void DesignModePanel::notifyThemeChanged()
{
    auto& t = MixerTheme::state();
    t.warning = t.meterYellow;
    t.error = t.meterRed;
    t.elevated = t.panel.brighter(0.08f);
    t.meterTrack = t.separator;
    if (t.faderFill.getAlpha() == 0)
        t.faderFill = t.accent;

    MixerTheme::saveToSettings();
    if (themeChanged)
        themeChanged();
}

void DesignModePanel::syncControlsFromTheme()
{
    for (auto& row : colourRows)
        row->syncFromTheme();
    for (auto& row : layoutRows)
        row->syncFromTheme();
}

void DesignModePanel::resetToDefaults()
{
    MixerTheme::resetToDefaults();
    syncControlsFromTheme();
    notifyThemeChanged();
}

void DesignModePanel::exportTheme()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Export theme",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("input-mixer-theme.json"),
        "*.json");

    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                         [chooser](const juce::FileChooser& fc) {
                             const auto file = fc.getResult();
                             if (file == juce::File())
                                 return;

                             file.replaceWithText(juce::JSON::toString(MixerTheme::exportToJson()));
                             juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                                      "Theme exported",
                                                                      "Saved to " + file.getFullPathName());
                         });
}

void DesignModePanel::importTheme()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Import theme",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.json");

    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [this, chooser](const juce::FileChooser& fc) {
                             juce::ignoreUnused(chooser);
                             const auto file = fc.getResult();
                             if (file == juce::File())
                                 return;

                             juce::String error;
                             const auto json = juce::JSON::parse(file.loadFileAsString());
                             const bool ok = MixerTheme::importFromJson(json, error);
                             if (ok)
                             {
                                 MixerTheme::saveToSettings();
                                 syncControlsFromTheme();
                                 notifyThemeChanged();
                             }

                             showImportResult(ok, ok ? "Theme settings applied." : error);
                         });
}

void DesignModePanel::importDrawdio()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Import Drawdio project",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.json;*.drawdio.json");

    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [this, chooser](const juce::FileChooser& fc) {
                             juce::ignoreUnused(chooser);
                             const auto file = fc.getResult();
                             if (file == juce::File())
                                 return;

                             juce::String error;
                             const auto json = juce::JSON::parse(file.loadFileAsString());
                             const bool ok = MixerTheme::importDrawdioJson(json, error);
                             if (ok)
                             {
                                 MixerTheme::saveToSettings();
                                 syncControlsFromTheme();
                                 notifyThemeChanged();
                             }

                             showImportResult(ok,
                                              ok ? "Colors and spacing from Drawdio were applied where possible."
                                                 : error);
                         });
}

void DesignModePanel::openDrawdio()
{
    juce::URL("https://github.com/Hornfisk/drawdio").launchInDefaultBrowser();
}

DesignModeWindow::DesignModeWindow(DesignModePanel::ThemeChangedCallback onThemeChanged)
    : DocumentWindow("Design Mode",
                     MixerTheme::background(),
                     DocumentWindow::closeButton)
{
    panel = std::make_unique<DesignModePanel>(std::move(onThemeChanged));
    setContentNonOwned(panel.get(), true);
    setResizable(true, true);
    setResizeLimits(380, 480, 560, 1200);
    centreWithSize(420, 1000);
    setVisible(true);
}

void DesignModeWindow::closeButtonPressed()
{
    setVisible(false);
}
