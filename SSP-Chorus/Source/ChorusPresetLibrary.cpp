#include "ChorusPresetLibrary.h"

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
        value("loCutHz", 80.0f), value("hiCutHz", 16000.0f), value("motionAmount", 42.0f),
        value("motionRate", 0.55f), value("motionShape", 48.0f), value("motionSpin", 1.0f),
        value("voiceCrossoverHz", 1600.0f), value("lowVoiceAmount", 58.0f), value("lowVoiceScale", 118.0f),
        value("lowVoiceRate", 0.32f), value("highVoiceAmount", 72.0f), value("highVoiceScale", 92.0f),
        value("highVoiceRate", 0.88f), value("delayMs", 11.5f), value("feedback", 24.0f),
        value("tone", 54.0f), value("stereoWidth", 132.0f), value("drive", 14.0f),
        value("dryWet", 38.0f), value("shineAmount", 18.0f), value("shineRate", 1.25f),
        value("spreadMs", 5.5f), value("vibrato", 0.0f), value("focusCut", 0.0f)
    }));

    presets.add(preset("Studio Width", "Basics", {
        value("loCutHz", 120.0f), value("hiCutHz", 15000.0f), value("motionAmount", 34.0f),
        value("motionRate", 0.45f), value("motionShape", 42.0f), value("motionSpin", 1.0f),
        value("voiceCrossoverHz", 1400.0f), value("lowVoiceAmount", 56.0f), value("lowVoiceScale", 108.0f),
        value("lowVoiceRate", 0.28f), value("highVoiceAmount", 64.0f), value("highVoiceScale", 84.0f),
        value("highVoiceRate", 0.72f), value("delayMs", 9.0f), value("feedback", 18.0f),
        value("tone", 54.0f), value("stereoWidth", 118.0f), value("drive", 8.0f),
        value("dryWet", 32.0f), value("shineAmount", 12.0f), value("shineRate", 1.00f),
        value("spreadMs", 4.0f), value("vibrato", 0.0f), value("focusCut", 0.0f)
    }));

    presets.add(preset("Wide Doubler", "Mix", {
        value("loCutHz", 90.0f), value("hiCutHz", 17000.0f), value("motionAmount", 48.0f),
        value("motionRate", 0.62f), value("motionShape", 56.0f), value("motionSpin", 1.0f),
        value("voiceCrossoverHz", 1250.0f), value("lowVoiceAmount", 62.0f), value("lowVoiceScale", 128.0f),
        value("lowVoiceRate", 0.34f), value("highVoiceAmount", 78.0f), value("highVoiceScale", 96.0f),
        value("highVoiceRate", 0.95f), value("delayMs", 10.8f), value("feedback", 22.0f),
        value("tone", 60.0f), value("stereoWidth", 156.0f), value("drive", 10.0f),
        value("dryWet", 40.0f), value("shineAmount", 18.0f), value("shineRate", 1.28f),
        value("spreadMs", 6.8f), value("vibrato", 0.0f), value("focusCut", 0.0f)
    }));

    presets.add(preset("Liquid Sweep", "Motion", {
        value("loCutHz", 110.0f), value("hiCutHz", 18000.0f), value("motionAmount", 66.0f),
        value("motionRate", 0.86f), value("motionShape", 72.0f), value("motionSpin", 1.0f),
        value("voiceCrossoverHz", 1800.0f), value("lowVoiceAmount", 48.0f), value("lowVoiceScale", 102.0f),
        value("lowVoiceRate", 0.48f), value("highVoiceAmount", 84.0f), value("highVoiceScale", 88.0f),
        value("highVoiceRate", 1.65f), value("delayMs", 7.5f), value("feedback", 20.0f),
        value("tone", 68.0f), value("stereoWidth", 126.0f), value("drive", 12.0f),
        value("dryWet", 38.0f), value("shineAmount", 32.0f), value("shineRate", 1.85f),
        value("spreadMs", 5.0f), value("vibrato", 0.0f), value("focusCut", 0.0f)
    }));

    presets.add(preset("Body Ensemble", "Ensemble", {
        value("loCutHz", 70.0f), value("hiCutHz", 15500.0f), value("motionAmount", 58.0f),
        value("motionRate", 0.38f), value("motionShape", 64.0f), value("motionSpin", 1.0f),
        value("voiceCrossoverHz", 980.0f), value("lowVoiceAmount", 74.0f), value("lowVoiceScale", 148.0f),
        value("lowVoiceRate", 0.24f), value("highVoiceAmount", 86.0f), value("highVoiceScale", 118.0f),
        value("highVoiceRate", 0.62f), value("delayMs", 13.5f), value("feedback", 28.0f),
        value("tone", 50.0f), value("stereoWidth", 148.0f), value("drive", 14.0f),
        value("dryWet", 46.0f), value("shineAmount", 24.0f), value("shineRate", 0.84f),
        value("spreadMs", 8.5f), value("vibrato", 0.0f), value("focusCut", 1.0f)
    }));

    presets.add(preset("Tape Bloom", "Color", {
        value("loCutHz", 85.0f), value("hiCutHz", 13500.0f), value("motionAmount", 44.0f),
        value("motionRate", 0.28f), value("motionShape", 46.0f), value("motionSpin", 1.0f),
        value("voiceCrossoverHz", 1120.0f), value("lowVoiceAmount", 64.0f), value("lowVoiceScale", 136.0f),
        value("lowVoiceRate", 0.20f), value("highVoiceAmount", 74.0f), value("highVoiceScale", 102.0f),
        value("highVoiceRate", 0.55f), value("delayMs", 15.5f), value("feedback", 34.0f),
        value("tone", 46.0f), value("stereoWidth", 138.0f), value("drive", 24.0f),
        value("dryWet", 42.0f), value("shineAmount", 36.0f), value("shineRate", 0.72f),
        value("spreadMs", 9.5f), value("vibrato", 0.0f), value("focusCut", 1.0f)
    }));

    presets.add(preset("Glass Vibrato", "Vibrato", {
        value("loCutHz", 140.0f), value("hiCutHz", 14500.0f), value("motionAmount", 72.0f),
        value("motionRate", 1.35f), value("motionShape", 58.0f), value("motionSpin", 0.0f),
        value("voiceCrossoverHz", 2200.0f), value("lowVoiceAmount", 36.0f), value("lowVoiceScale", 82.0f),
        value("lowVoiceRate", 0.68f), value("highVoiceAmount", 58.0f), value("highVoiceScale", 72.0f),
        value("highVoiceRate", 2.20f), value("delayMs", 6.0f), value("feedback", 12.0f),
        value("tone", 62.0f), value("stereoWidth", 110.0f), value("drive", 6.0f),
        value("dryWet", 100.0f), value("shineAmount", 44.0f), value("shineRate", 2.30f),
        value("spreadMs", 3.2f), value("vibrato", 1.0f), value("focusCut", 0.0f)
    }));

    presets.add(preset("Air Mist", "Ambient", {
        value("loCutHz", 160.0f), value("hiCutHz", 19000.0f), value("motionAmount", 52.0f),
        value("motionRate", 0.22f), value("motionShape", 68.0f), value("motionSpin", 1.0f),
        value("voiceCrossoverHz", 2400.0f), value("lowVoiceAmount", 22.0f), value("lowVoiceScale", 72.0f),
        value("lowVoiceRate", 0.14f), value("highVoiceAmount", 88.0f), value("highVoiceScale", 104.0f),
        value("highVoiceRate", 0.44f), value("delayMs", 17.5f), value("feedback", 30.0f),
        value("tone", 72.0f), value("stereoWidth", 166.0f), value("drive", 4.0f),
        value("dryWet", 36.0f), value("shineAmount", 42.0f), value("shineRate", 0.60f),
        value("spreadMs", 10.0f), value("vibrato", 0.0f), value("focusCut", 0.0f)
    }));

    return presets;
}
} // namespace

namespace sspchorus::presets
{
const juce::Array<PresetRecord>& getFactoryPresetLibrary()
{
    static const auto presets = buildFactoryPresetLibrary();
    return presets;
}
}
