#pragma once

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

class DesignModePanel : public juce::Component
{
public:
    using ThemeChangedCallback = std::function<void()>;

    explicit DesignModePanel(ThemeChangedCallback onThemeChanged);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class ColourRow : public juce::Component
    {
    public:
        ColourRow(const juce::String& labelText, juce::Colour& colourRef, std::function<void()> onChange);

        void paint(juce::Graphics& g) override;
        void resized() override;
        void syncFromTheme();

    private:
        juce::Label label;
        juce::TextButton swatch { " " };
        juce::Colour& colour;
        std::function<void()> changeHandler;
    };

    class IntSliderRow : public juce::Component
    {
    public:
        IntSliderRow(const juce::String& labelText, int& valueRef, int min, int max, std::function<void()> onChange);

        void resized() override;
        void syncFromTheme();

    private:
        juce::Label label;
        juce::Slider slider;
        int& value;
        std::function<void()> changeHandler;
    };

    void notifyThemeChanged();
    void syncControlsFromTheme();
    void resetToDefaults();
    void exportTheme();
    void importTheme();
    void importDrawdio();
    void openDrawdio();

    ThemeChangedCallback themeChanged;
    juce::Viewport viewport;
    juce::Component content;
    juce::Label coloursHeading { {}, "Colors" };
    juce::Label layoutHeading { {}, "Layout" };
    juce::TextButton resetButton { "Reset to defaults" };
    juce::TextButton exportButton { "Export theme" };
    juce::TextButton importButton { "Import theme" };
    juce::TextButton importDrawdioButton { "Import Drawdio JSON" };
    juce::TextButton openDrawdioButton { "Open Drawdio in browser" };

    std::vector<std::unique_ptr<ColourRow>> colourRows;
    std::vector<std::unique_ptr<IntSliderRow>> layoutRows;
};

class DesignModeWindow : public juce::DocumentWindow
{
public:
    explicit DesignModeWindow(DesignModePanel::ThemeChangedCallback onThemeChanged);

    void closeButtonPressed() override;

    DesignModePanel& getPanel() { return *panel; }

private:
    std::unique_ptr<DesignModePanel> panel;
};
