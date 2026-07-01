#include "MixerTheme.h"

#include "../app/SettingsStore.h"

namespace
{
juce::String colourToHex(const juce::Colour& c)
{
    return c.toDisplayString(true);
}

bool parseHexColour(const juce::String& text, juce::Colour& out)
{
    const auto trimmed = text.trim();
    if (trimmed.isEmpty())
        return false;

    const auto parsed = juce::Colour::fromString(trimmed.startsWithChar('#') ? trimmed : "#" + trimmed);
    if (parsed.isTransparent() && ! trimmed.equalsIgnoreCase("#00000000"))
        return false;

    out = parsed;
    return true;
}

void setColourFromVar(const juce::var& obj, const char* key, juce::Colour& target)
{
    const auto str = obj.getProperty(key, {}).toString().trim();
    if (str.isNotEmpty())
        parseHexColour(str, target);
}

void setIntFromVar(const juce::var& obj, const char* key, int& target)
{
    const auto value = obj.getProperty(key, {});
    if (value.isInt() || value.isInt64() || value.isDouble())
        target = static_cast<int>(value);
}

void setFloatFromVar(const juce::var& obj, const char* key, float& target)
{
    const auto value = obj.getProperty(key, {});
    if (value.isDouble() || value.isInt())
        target = static_cast<float>(static_cast<double>(value));
}

void applyDrawdioComponentHints(const juce::Array<juce::var>* components, MixerTheme::ThemeState& theme)
{
    if (components == nullptr)
        return;

    for (const auto& item : *components)
    {
        const auto type = item.getProperty("type", {}).toString();
        const auto color = item.getProperty("color", {}).toString();
        const auto props = item.getProperty("properties", {});

        if (type == "level_meter")
        {
            if (const auto green = props.getProperty("greenColor", {}).toString(); green.isNotEmpty())
                parseHexColour(green, theme.meterGreen);
            if (const auto yellow = props.getProperty("yellowColor", {}).toString(); yellow.isNotEmpty())
                parseHexColour(yellow, theme.meterYellow);
            if (const auto red = props.getProperty("redColor", {}).toString(); red.isNotEmpty())
                parseHexColour(red, theme.meterRed);
        }
        else if (type == "horizontal_slider" || type == "vertical_slider")
        {
            if (color.isNotEmpty())
            {
                parseHexColour(color, theme.faderFill);
                theme.accent = theme.faderFill;
            }
        }
        else if (type == "panel_group" || type == "section_header")
        {
            if (const auto bg = props.getProperty("bgColor", {}).toString(); bg.isNotEmpty())
                parseHexColour(bg, theme.panel);
            if (const auto radius = props.getProperty("cornerRadius", {}); radius.isInt() || radius.isDouble())
                theme.panelCornerRadius = static_cast<int>(radius);
        }
    }
}

} // namespace

namespace MixerTheme
{
ThemeState& state()
{
    static ThemeState instance;
    return instance;
}

ThemeState defaultState()
{
    return ThemeState {};
}

void loadFromSettings()
{
    juce::String ignored;
    importFromJson(SettingsStore::get().loadThemeJson(), ignored);
}

void saveToSettings()
{
    SettingsStore::get().saveThemeJson(exportToJson());
}

void resetToDefaults()
{
    state() = defaultState();
    saveToSettings();
}

juce::var exportToJson()
{
    const auto& t = state();
    auto* root = new juce::DynamicObject();
    root->setProperty("version", 1);

    auto* colours = new juce::DynamicObject();
    colours->setProperty("background", colourToHex(t.background));
    colours->setProperty("panel", colourToHex(t.panel));
    colours->setProperty("elevated", colourToHex(t.elevated));
    colours->setProperty("accent", colourToHex(t.accent));
    colours->setProperty("textPrimary", colourToHex(t.textPrimary));
    colours->setProperty("textMuted", colourToHex(t.textMuted));
    colours->setProperty("faderTrack", colourToHex(t.faderTrack));
    colours->setProperty("faderFill", colourToHex(t.faderFill));
    colours->setProperty("faderThumb", colourToHex(t.faderThumb));
    colours->setProperty("buttonOff", colourToHex(t.buttonOff));
    colours->setProperty("buttonOn", colourToHex(t.buttonOn));
    colours->setProperty("separator", colourToHex(t.separator));
    colours->setProperty("meterGreen", colourToHex(t.meterGreen));
    colours->setProperty("meterYellow", colourToHex(t.meterYellow));
    colours->setProperty("meterRed", colourToHex(t.meterRed));
    root->setProperty("colours", colours);

    auto* layout = new juce::DynamicObject();
    layout->setProperty("outerPadding", t.outerPadding);
    layout->setProperty("stripInnerPadding", t.stripInnerPadding);
    layout->setProperty("stripMinWidth", t.stripMinWidth);
    layout->setProperty("stripGap", t.stripGap);
    layout->setProperty("rightPanelWidth", t.rightPanelWidth);
    layout->setProperty("masterColumnMasterRatioPercent", t.masterColumnMasterRatioPercent);
    layout->setProperty("panelCornerRadius", t.panelCornerRadius);
    layout->setProperty("meterSegments", t.meterSegments);
    layout->setProperty("meterSegmentGap", t.meterSegmentGap);
    root->setProperty("layout", layout);

    return root;
}

bool importFromJson(const juce::var& json, juce::String& errorMessage)
{
    if (json.isVoid() || json.isUndefined())
        return true;

    if (! json.hasProperty("version"))
    {
        errorMessage = "Unrecognized theme file.";
        return false;
    }

    auto& t = state();
    const auto colours = json.getProperty("colours", {});
    setColourFromVar(colours, "background", t.background);
    setColourFromVar(colours, "panel", t.panel);
    setColourFromVar(colours, "elevated", t.elevated);
    setColourFromVar(colours, "accent", t.accent);
    setColourFromVar(colours, "textPrimary", t.textPrimary);
    setColourFromVar(colours, "textMuted", t.textMuted);
    setColourFromVar(colours, "faderTrack", t.faderTrack);
    setColourFromVar(colours, "faderFill", t.faderFill);
    setColourFromVar(colours, "faderThumb", t.faderThumb);
    setColourFromVar(colours, "buttonOff", t.buttonOff);
    setColourFromVar(colours, "buttonOn", t.buttonOn);
    setColourFromVar(colours, "separator", t.separator);
    setColourFromVar(colours, "meterGreen", t.meterGreen);
    setColourFromVar(colours, "meterYellow", t.meterYellow);
    setColourFromVar(colours, "meterRed", t.meterRed);

    t.warning = t.meterYellow;
    t.error = t.meterRed;
    t.faderFill = t.faderFill.getAlpha() > 0 ? t.faderFill : t.accent;

    const auto layout = json.getProperty("layout", {});
    setIntFromVar(layout, "outerPadding", t.outerPadding);
    setIntFromVar(layout, "stripInnerPadding", t.stripInnerPadding);
    setIntFromVar(layout, "stripMinWidth", t.stripMinWidth);
    setIntFromVar(layout, "stripGap", t.stripGap);
    setIntFromVar(layout, "rightPanelWidth", t.rightPanelWidth);
    setIntFromVar(layout, "masterColumnMasterRatioPercent", t.masterColumnMasterRatioPercent);
    setIntFromVar(layout, "panelCornerRadius", t.panelCornerRadius);
    setIntFromVar(layout, "meterSegments", t.meterSegments);
    setFloatFromVar(layout, "meterSegmentGap", t.meterSegmentGap);

    return true;
}

bool importDrawdioJson(const juce::var& json, juce::String& errorMessage)
{
    if (json.isVoid() || json.isUndefined())
    {
        errorMessage = "Empty Drawdio file.";
        return false;
    }

    auto& t = state();

    if (json.hasProperty("drawdio_version"))
    {
        juce::var workspace;
        if (auto* workspaces = json.getProperty("workspaces", {}).getArray())
        {
            const auto activeId = json.getProperty("activeWorkspaceId", {}).toString();
            for (const auto& ws : *workspaces)
            {
                if (activeId.isEmpty() || ws.getProperty("id", {}).toString() == activeId)
                {
                    workspace = ws;
                    break;
                }
            }

            if (workspace.isVoid() && ! workspaces->isEmpty())
                workspace = (*workspaces)[0];
        }

        if (! workspace.isVoid())
        {
            if (const auto bg = workspace.getProperty("bgColor", {}).toString(); bg.isNotEmpty())
                parseHexColour(bg, t.background);

            if (const auto grid = workspace.getProperty("gridSize", {}); grid.isInt() || grid.isDouble())
                t.stripGap = juce::jlimit(4, 24, static_cast<int>(grid) / 4);

            applyDrawdioComponentHints(workspace.getProperty("components", {}).getArray(), t);
        }
        else
        {
            if (const auto bg = json.getProperty("canvas", {}).getProperty("bgColor", {}).toString(); bg.isNotEmpty())
                parseHexColour(bg, t.background);

            applyDrawdioComponentHints(json.getProperty("components", {}).getArray(), t);
        }
    }
    else
    {
        errorMessage = "Not a Drawdio project file.";
        return false;
    }

    t.elevated = t.panel.brighter(0.08f);
    t.meterTrack = t.separator;
    t.faderTrack = t.separator;
    t.warning = t.meterYellow;
    t.error = t.meterRed;
    if (t.faderFill.getAlpha() == 0)
        t.faderFill = t.accent;

    return true;
}
} // namespace MixerTheme
