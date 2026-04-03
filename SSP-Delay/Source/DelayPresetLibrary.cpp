#include "DelayPresetLibrary.h"

namespace
{
using PresetValue = PluginProcessor::PresetValue;
using PresetRecord = PluginProcessor::PresetRecord;

juce::String makeSlug(const juce::String& text)
{
    auto slug = text.toLowerCase().trim();
    juce::String out;

    for (auto c : slug)
    {
        if (juce::CharacterFunctions::isLetterOrDigit(c))
            out << c;
        else if (c == ' ' || c == '-' || c == '_' || c == '/')
            out << '-';
    }

    while (out.contains("--"))
        out = out.replace("--", "-");

    out = out.trimCharactersAtStart("-").trimCharactersAtEnd("-");
    return out.isNotEmpty() ? out : "preset";
}

juce::String makePresetKey(bool isFactory, const juce::String& category, const juce::String& name)
{
    const auto prefix = isFactory ? "factory:" : "user:";
    const auto categorySlug = makeSlug(category);
    const auto nameSlug = makeSlug(name);
    return prefix + (categorySlug.isNotEmpty() ? categorySlug + "/" : juce::String{}) + nameSlug;
}

PresetValue value(const char* id, float raw)
{
    PresetValue item;
    item.id = id;
    item.value = raw;
    return item;
}

PresetRecord preset(const juce::String& name, const juce::String& category, std::initializer_list<PresetValue> values)
{
    PresetRecord item;
    item.name = name;
    item.category = category;
    item.author = "SSP Factory";
    item.isFactory = true;
    item.id = makePresetKey(true, category, name);

    for (const auto& entry : values)
        item.values.add(entry);

    return item;
}

juce::Array<PresetRecord> buildFactoryPresetLibrary()
{
    juce::Array<PresetRecord> presets;

    presets.add(preset("Default Setting", "Basics", {
        value("timeMode", 0.0f), value("timeMs", 380.0f), value("feedback", 0.40f),
        value("mix", 0.26f), value("tone", 0.55f), value("width", 0.32f)
    }));
    presets.add(preset("Clean Utility", "Basics", {
        value("timeMode", 0.0f), value("timeMs", 240.0f), value("feedback", 0.24f),
        value("mix", 0.18f), value("tone", 0.66f), value("width", 0.18f)
    }));
    presets.add(preset("Tight Vocal", "Basics", {
        value("timeMode", 0.0f), value("timeMs", 160.0f), value("feedback", 0.32f),
        value("mix", 0.20f), value("tone", 0.60f), value("width", 0.26f)
    }));
    presets.add(preset("Wide Repeat", "Basics", {
        value("timeMode", 0.0f), value("timeMs", 420.0f), value("feedback", 0.38f),
        value("mix", 0.28f), value("tone", 0.58f), value("width", 0.62f)
    }));

    presets.add(preset("Eighth Pulse", "Sync", {
        value("timeMode", 4.0f), value("timeMs", 380.0f), value("feedback", 0.42f),
        value("mix", 0.24f), value("tone", 0.64f), value("width", 0.30f)
    }));
    presets.add(preset("Dotted Bounce", "Sync", {
        value("timeMode", 5.0f), value("timeMs", 380.0f), value("feedback", 0.48f),
        value("mix", 0.27f), value("tone", 0.56f), value("width", 0.42f)
    }));
    presets.add(preset("Half Wash", "Sync", {
        value("timeMode", 7.0f), value("timeMs", 380.0f), value("feedback", 0.58f),
        value("mix", 0.32f), value("tone", 0.48f), value("width", 0.54f)
    }));

    presets.add(preset("Ping Spread", "Stereo", {
        value("timeMode", 0.0f), value("timeMs", 330.0f), value("feedback", 0.44f),
        value("mix", 0.28f), value("tone", 0.62f), value("width", 0.72f)
    }));
    presets.add(preset("Crossfeed Bloom", "Stereo", {
        value("timeMode", 4.0f), value("timeMs", 380.0f), value("feedback", 0.50f),
        value("mix", 0.31f), value("tone", 0.46f), value("width", 0.78f)
    }));
    presets.add(preset("Twin Taps", "Stereo", {
        value("timeMode", 0.0f), value("timeMs", 520.0f), value("feedback", 0.36f),
        value("mix", 0.25f), value("tone", 0.60f), value("width", 0.85f)
    }));

    presets.add(preset("Cloud Tail", "Atmosphere", {
        value("timeMode", 7.0f), value("timeMs", 380.0f), value("feedback", 0.66f),
        value("mix", 0.38f), value("tone", 0.34f), value("width", 0.66f)
    }));
    presets.add(preset("Night Pad", "Atmosphere", {
        value("timeMode", 8.0f), value("timeMs", 380.0f), value("feedback", 0.74f),
        value("mix", 0.42f), value("tone", 0.28f), value("width", 0.82f)
    }));
    presets.add(preset("Fog Bank", "Atmosphere", {
        value("timeMode", 6.0f), value("timeMs", 380.0f), value("feedback", 0.80f),
        value("mix", 0.46f), value("tone", 0.22f), value("width", 0.90f)
    }));

    return presets;
}
} // namespace

namespace sspdelay::presets
{
const juce::Array<PresetRecord>& getFactoryPresetLibrary()
{
    static const auto presets = buildFactoryPresetLibrary();
    return presets;
}
}
