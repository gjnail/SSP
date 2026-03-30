#include "ImagerPresetLibrary.h"

namespace
{
using PresetRecord = PluginProcessor::PresetRecord;
using PresetBandData = PluginProcessor::PresetBandData;

PresetRecord makePreset(const juce::String& id,
                        const juce::String& name,
                        const juce::String& category,
                        float crossover1,
                        float crossover2,
                        float crossover3,
                        std::array<float, PluginProcessor::numBands> widths,
                        std::array<float, PluginProcessor::numBands> pans = {},
                        float outputGain = 0.0f,
                        const juce::String& author = "SSP")
{
    PresetRecord preset;
    preset.id = id;
    preset.name = name;
    preset.category = category;
    preset.author = author;
    preset.isFactory = true;
    preset.crossover1 = crossover1;
    preset.crossover2 = crossover2;
    preset.crossover3 = crossover3;
    preset.outputGain = outputGain;

    for (int band = 0; band < PluginProcessor::numBands; ++band)
    {
        preset.bands[(size_t) band].width = widths[(size_t) band];
        preset.bands[(size_t) band].pan = pans[(size_t) band];
    }

    return preset;
}
} // namespace

namespace sspimager::presets
{
const juce::Array<PluginProcessor::PresetRecord>& getFactoryPresetLibrary()
{
    static const auto library = []
    {
        juce::Array<PresetRecord> presets;
        presets.add(makePreset("factory:balanced-wide", "Balanced Wide", "Mastering/Balanced", 140.0f, 1550.0f, 7600.0f,
                               { 78.0f, 102.0f, 122.0f, 132.0f }));
        presets.add(makePreset("factory:vinyl-focus", "Vinyl Focus", "Mastering/Mono Control", 160.0f, 1950.0f, 7200.0f,
                               { 0.0f, 72.0f, 118.0f, 126.0f }));
        presets.add(makePreset("factory:club-lift", "Club Lift", "Mastering/Modern", 120.0f, 1700.0f, 9000.0f,
                               { 45.0f, 96.0f, 138.0f, 156.0f }));
        presets.add(makePreset("factory:edm-topline", "EDM Topline", "Production/Synth Bus", 180.0f, 2400.0f, 9800.0f,
                               { 28.0f, 112.0f, 150.0f, 176.0f }));
        presets.add(makePreset("factory:drum-bus-punch", "Drum Bus Punch", "Production/Drums", 110.0f, 1450.0f, 6000.0f,
                               { 18.0f, 88.0f, 118.0f, 136.0f }));
        presets.add(makePreset("factory:vocal-polish", "Vocal Polish", "Production/Vocals", 220.0f, 2900.0f, 9200.0f,
                               { 10.0f, 68.0f, 128.0f, 144.0f },
                               { 0.0f, -4.0f, 3.0f, 0.0f }));
        presets.add(makePreset("factory:mono-anchor", "Mono Anchor", "Utility/Translation", 180.0f, 2400.0f, 9200.0f,
                               { 0.0f, 42.0f, 84.0f, 108.0f }));
        presets.add(makePreset("factory:hi-fi-expanse", "Hi-Fi Expanse", "Creative/Expansive", 150.0f, 1800.0f, 8200.0f,
                               { 82.0f, 132.0f, 168.0f, 190.0f }));
        presets.add(makePreset("factory:tilted-stage", "Tilted Stage", "Creative/Asymmetric", 170.0f, 2100.0f, 8600.0f,
                               { 56.0f, 112.0f, 142.0f, 132.0f },
                               { -8.0f, -4.0f, 5.0f, 11.0f }));
        presets.add(makePreset("factory:current-settings", "Current Settings", "Utility/Basics", 200.0f, 2000.0f, 8000.0f,
                               { 100.0f, 100.0f, 100.0f, 100.0f }));
        return presets;
    }();

    return library;
}
} // namespace sspimager::presets
