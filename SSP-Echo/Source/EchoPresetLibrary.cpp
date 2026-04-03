#include "EchoPresetLibrary.h"

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
        value("timeMs", 340.0f), value("feedback", 0.42f), value("mix", 0.28f),
        value("color", 0.45f), value("driveDb", 4.0f), value("flutter", 0.18f)
    }));
    presets.add(preset("Slapback", "Basics", {
        value("timeMs", 110.0f), value("feedback", 0.24f), value("mix", 0.20f),
        value("color", 0.62f), value("driveDb", 2.2f), value("flutter", 0.05f)
    }));
    presets.add(preset("Vocal Space", "Basics", {
        value("timeMs", 260.0f), value("feedback", 0.36f), value("mix", 0.24f),
        value("color", 0.56f), value("driveDb", 3.4f), value("flutter", 0.12f)
    }));
    presets.add(preset("Soft Doubler", "Basics", {
        value("timeMs", 78.0f), value("feedback", 0.18f), value("mix", 0.18f),
        value("color", 0.58f), value("driveDb", 1.2f), value("flutter", 0.10f)
    }));

    presets.add(preset("Worn Loop", "Tape", {
        value("timeMs", 420.0f), value("feedback", 0.58f), value("mix", 0.31f),
        value("color", 0.30f), value("driveDb", 7.0f), value("flutter", 0.28f)
    }));
    presets.add(preset("Dusty Plate", "Tape", {
        value("timeMs", 300.0f), value("feedback", 0.48f), value("mix", 0.27f),
        value("color", 0.36f), value("driveDb", 5.8f), value("flutter", 0.22f)
    }));
    presets.add(preset("Crushed Return", "Tape", {
        value("timeMs", 510.0f), value("feedback", 0.63f), value("mix", 0.35f),
        value("color", 0.26f), value("driveDb", 10.5f), value("flutter", 0.20f)
    }));

    presets.add(preset("Dub Throw", "Dub", {
        value("timeMs", 560.0f), value("feedback", 0.74f), value("mix", 0.36f),
        value("color", 0.24f), value("driveDb", 6.2f), value("flutter", 0.18f)
    }));
    presets.add(preset("Filter Fountain", "Dub", {
        value("timeMs", 690.0f), value("feedback", 0.80f), value("mix", 0.40f),
        value("color", 0.18f), value("driveDb", 8.5f), value("flutter", 0.24f)
    }));
    presets.add(preset("Dark Spiral", "Dub", {
        value("timeMs", 820.0f), value("feedback", 0.86f), value("mix", 0.42f),
        value("color", 0.12f), value("driveDb", 9.8f), value("flutter", 0.30f)
    }));

    presets.add(preset("Warped Halo", "Motion", {
        value("timeMs", 390.0f), value("feedback", 0.52f), value("mix", 0.32f),
        value("color", 0.48f), value("driveDb", 4.6f), value("flutter", 0.42f)
    }));
    presets.add(preset("Seasick Bounce", "Motion", {
        value("timeMs", 240.0f), value("feedback", 0.44f), value("mix", 0.28f),
        value("color", 0.41f), value("driveDb", 5.0f), value("flutter", 0.56f)
    }));
    presets.add(preset("Neon Drift", "Motion", {
        value("timeMs", 470.0f), value("feedback", 0.60f), value("mix", 0.34f),
        value("color", 0.54f), value("driveDb", 3.8f), value("flutter", 0.48f)
    }));

    return presets;
}
} // namespace

namespace sspecho::presets
{
const juce::Array<PresetRecord>& getFactoryPresetLibrary()
{
    static const auto presets = buildFactoryPresetLibrary();
    return presets;
}
}
