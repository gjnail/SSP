#include "EQPresetLibrary.h"

namespace sspeq::presets
{
namespace
{
using PresetBand = PluginProcessor::PresetBand;
using PresetRecord = PluginProcessor::PresetRecord;

juce::String slugify(const juce::String& text)
{
    juce::String slug;
    for (auto c : text.toLowerCase())
    {
        if (juce::CharacterFunctions::isLetterOrDigit(c))
            slug << c;
        else if (c == ' ' || c == '/' || c == '-' || c == '_')
            slug << '-';
    }

    while (slug.contains("--"))
        slug = slug.replace("--", "-");

    return slug.trimCharactersAtStart("-").trimCharactersAtEnd("-");
}

juce::String makeFactoryId(const juce::String& category, const juce::String& name)
{
    auto path = category.isNotEmpty() ? category + "/" + name : name;
    return "factory:" + slugify(path);
}

PresetBand band(const char* type, float frequency, float gain = 0.0f, float q = 0.707f, int slope = 12, const char* stereoMode = "stereo")
{
    PresetBand item;
    item.type = type;
    item.frequency = frequency;
    item.gain = gain;
    item.q = q;
    item.slope = slope;
    item.stereoMode = stereoMode;
    return item;
}

PresetRecord preset(const juce::String& name, const juce::String& category, std::initializer_list<PresetBand> bands)
{
    PresetRecord item;
    item.id = makeFactoryId(category, name);
    item.name = name;
    item.category = category;
    item.author = "SSP Factory";
    item.isFactory = true;
    for (const auto& entry : bands)
        item.bands.add(entry);
    return item;
}

PresetRecord flatPreset(const juce::String& name, std::initializer_list<float> frequencies)
{
    juce::Array<PresetBand> bands;
    for (auto frequency : frequencies)
        bands.add(band("bell", frequency, 0.0f, 1.0f));

    PresetRecord item;
    item.id = makeFactoryId({}, name);
    item.name = name;
    item.author = "SSP Factory";
    item.isFactory = true;
    item.bands = std::move(bands);
    return item;
}

juce::Array<PresetRecord> buildFactoryPresetLibrary()
{
    juce::Array<PresetRecord> presets;

    presets.add(preset("Band Pass Narrow", {}, { band("bandpass", 1000.0f, 0.0f, 4.0f) }));
    presets.add(preset("Band Pass Wide", {}, { band("bandpass", 1000.0f, 0.0f, 0.5f) }));
    presets.add(preset("Bell Flat Top", {}, { band("bell", 1000.0f, 0.0f, 0.1f) }));
    presets.add(preset("Bell Surgical", {}, { band("bell", 1000.0f, 0.0f, 8.0f) }));
    presets.add(preset("Clean", {}, {}));
    presets.add(preset("Default Setting", {}, { band("bell", 1000.0f, 0.0f, 1.0f) }));
    presets.add(flatPreset("Flat 5 Bands", { 80.0f, 300.0f, 1000.0f, 3500.0f, 10000.0f }));
    presets.add(flatPreset("Flat 6 Bands", { 50.0f, 140.0f, 420.0f, 1200.0f, 4200.0f, 12000.0f }));
    presets.add(flatPreset("Flat 7 Bands", { 40.0f, 110.0f, 280.0f, 850.0f, 2400.0f, 7000.0f, 15000.0f }));
    presets.add(flatPreset("Flat 8 Bands", { 35.0f, 80.0f, 180.0f, 420.0f, 1000.0f, 2400.0f, 6000.0f, 14000.0f }));
    presets.add(preset("High Boost", {}, { band("highshelf", 8000.0f, 3.0f, 0.7f) }));
    presets.add(preset("High Cut", {}, { band("lowpass", 12000.0f, 0.0f, 0.707f, 12) }));
    presets.add(preset("High Cut Brickwall", {}, { band("lowpass", 12000.0f, 0.0f, 0.707f, 96) }));
    presets.add(preset("High Shelf", {}, { band("highshelf", 8000.0f, 4.0f, 0.7f) }));
    presets.add(preset("High Shelf Brickwall", {}, { band("highshelf", 8000.0f, 4.0f, 0.7f, 96) }));
    presets.add(preset("Low Boost", {}, { band("lowshelf", 100.0f, 3.0f, 0.7f) }));
    presets.add(preset("Low Cut", {}, { band("highpass", 80.0f, 0.0f, 0.707f, 12) }));
    presets.add(preset("Low Cut Brickwall", {}, { band("highpass", 80.0f, 0.0f, 0.707f, 96) }));
    presets.add(preset("Low Shelf", {}, { band("lowshelf", 100.0f, 4.0f, 0.7f) }));
    presets.add(preset("Low Shelf Brickwall", {}, { band("lowshelf", 100.0f, 4.0f, 0.7f, 96) }));
    presets.add(preset("Phone", {}, { band("highpass", 300.0f, 0.0f, 0.707f, 24), band("lowpass", 3400.0f, 0.0f, 0.707f, 24) }));
    presets.add(preset("RIAA", {}, { band("lowshelf", 50.0f, 17.0f, 0.7f), band("bell", 500.0f, -4.0f, 0.9f), band("highshelf", 2122.0f, -13.7f, 0.7f) }));
    presets.add(preset("Tilt Shelf", {}, { band("tiltshelf", 1000.0f, 2.0f, 0.7f) }));
    presets.add(preset("Vocal", {}, { band("highpass", 80.0f, 0.0f, 0.707f, 12), band("bell", 300.0f, -2.0f, 1.5f), band("bell", 3000.0f, 2.0f, 1.0f), band("highshelf", 12000.0f, 1.5f, 0.7f) }));

    presets.add(preset("Kick Punch", "Dynamic/Drums", { band("lowshelf", 65.0f, 3.0f, 0.75f), band("bell", 280.0f, -2.0f, 1.4f), band("bell", 3200.0f, 2.5f, 1.1f) }));
    presets.add(preset("Kick Sub Focus", "Dynamic/Drums", { band("lowshelf", 45.0f, 4.0f, 0.7f), band("bell", 120.0f, -1.5f, 1.2f), band("highpass", 28.0f, 0.0f, 0.707f, 24) }));
    presets.add(preset("Snare Crack", "Dynamic/Drums", { band("highpass", 90.0f, 0.0f, 0.707f, 12), band("bell", 220.0f, -1.0f, 1.4f), band("bell", 4500.0f, 3.0f, 1.1f), band("highshelf", 9000.0f, 1.5f, 0.7f) }));
    presets.add(preset("Snare Body", "Dynamic/Drums", { band("bell", 180.0f, 2.5f, 1.1f), band("bell", 900.0f, -1.2f, 1.6f), band("bell", 3200.0f, 1.6f, 1.0f) }));
    presets.add(preset("Hi-Hat Tame", "Dynamic/Drums", { band("highpass", 350.0f, 0.0f, 0.707f, 12), band("bell", 6500.0f, -2.0f, 2.2f), band("highshelf", 12000.0f, -1.0f, 0.8f) }));
    presets.add(preset("Toms Clarity", "Dynamic/Drums", { band("bell", 120.0f, 2.0f, 1.0f), band("bell", 450.0f, -2.0f, 1.3f), band("bell", 4500.0f, 1.5f, 1.0f) }));
    presets.add(preset("Overhead Shimmer", "Dynamic/Drums", { band("highpass", 180.0f, 0.0f, 0.707f, 12), band("bell", 4500.0f, -1.2f, 1.8f), band("highshelf", 11000.0f, 3.0f, 0.7f) }));
    presets.add(preset("Room Scoop", "Dynamic/Drums", { band("highpass", 40.0f, 0.0f, 0.707f, 24), band("bell", 350.0f, -3.0f, 1.0f), band("highshelf", 8000.0f, 1.5f, 0.7f) }));
    presets.add(preset("Full Kit Mix", "Dynamic/Drums", { band("highpass", 28.0f, 0.0f, 0.707f, 12), band("bell", 280.0f, -2.0f, 1.0f), band("bell", 3500.0f, 1.8f, 1.1f), band("highshelf", 11000.0f, 1.5f, 0.7f) }));

    presets.add(preset("Bass Drop Sub", "Dynamic/EDM", { band("lowshelf", 45.0f, 4.0f, 0.7f), band("bell", 180.0f, -2.0f, 1.2f), band("highpass", 24.0f, 0.0f, 0.707f, 24) }));
    presets.add(preset("Bass Mid Growl", "Dynamic/EDM", { band("highpass", 35.0f, 0.0f, 0.707f, 12), band("bell", 250.0f, -1.5f, 1.3f), band("bell", 900.0f, 2.5f, 1.1f), band("bell", 2200.0f, 1.8f, 1.4f) }));
    presets.add(preset("Lead Presence", "Dynamic/EDM", { band("highpass", 120.0f, 0.0f, 0.707f, 12), band("bell", 2400.0f, 2.0f, 1.2f), band("highshelf", 9000.0f, 2.5f, 0.7f) }));
    presets.add(preset("Pad Space", "Dynamic/EDM", { band("highpass", 140.0f, 0.0f, 0.707f, 12), band("bell", 350.0f, -2.5f, 1.0f), band("highshelf", 9000.0f, 1.8f, 0.8f) }));
    presets.add(preset("Sidechain Scoop", "Dynamic/EDM", { band("bell", 90.0f, -2.0f, 1.1f), band("bell", 240.0f, -1.5f, 1.2f), band("bell", 2000.0f, 1.0f, 1.3f) }));
    presets.add(preset("Build-Up Sweep", "Dynamic/EDM", { band("highpass", 180.0f, 0.0f, 0.707f, 24), band("bell", 3500.0f, 2.0f, 1.2f), band("highshelf", 10000.0f, 3.0f, 0.7f) }));
    presets.add(preset("Drop Impact", "Dynamic/EDM", { band("lowshelf", 60.0f, 3.0f, 0.7f), band("bell", 300.0f, -2.0f, 1.2f), band("bell", 2500.0f, 2.0f, 1.1f), band("highshelf", 8000.0f, 1.5f, 0.8f) }));

    presets.add(preset("Acoustic Guitar Warmth", "Dynamic/Instruments", { band("highpass", 70.0f, 0.0f, 0.707f, 12), band("bell", 220.0f, 2.0f, 1.1f), band("bell", 2800.0f, -1.0f, 1.8f) }));
    presets.add(preset("Acoustic Guitar Sparkle", "Dynamic/Instruments", { band("highpass", 80.0f, 0.0f, 0.707f, 12), band("bell", 350.0f, -1.5f, 1.2f), band("highshelf", 9000.0f, 3.0f, 0.7f) }));
    presets.add(preset("Electric Guitar Crunch", "Dynamic/Instruments", { band("highpass", 85.0f, 0.0f, 0.707f, 12), band("bell", 250.0f, -1.2f, 1.4f), band("bell", 1800.0f, 1.6f, 1.0f), band("lowpass", 9500.0f, 0.0f, 0.707f, 12) }));
    presets.add(preset("Electric Guitar Solo Cut", "Dynamic/Instruments", { band("highpass", 110.0f, 0.0f, 0.707f, 12), band("bell", 800.0f, -1.0f, 1.2f), band("bell", 3200.0f, 2.5f, 1.0f), band("highshelf", 7000.0f, 1.2f, 0.8f) }));
    presets.add(preset("Piano Warmth", "Dynamic/Instruments", { band("highpass", 40.0f, 0.0f, 0.707f, 12), band("bell", 180.0f, 1.5f, 1.0f), band("bell", 2500.0f, -1.2f, 1.6f) }));
    presets.add(preset("Piano Brilliance", "Dynamic/Instruments", { band("highpass", 45.0f, 0.0f, 0.707f, 12), band("bell", 300.0f, -1.5f, 1.2f), band("highshelf", 8500.0f, 2.4f, 0.7f) }));
    presets.add(preset("Strings Air", "Dynamic/Instruments", { band("highpass", 120.0f, 0.0f, 0.707f, 12), band("bell", 450.0f, -1.0f, 1.1f), band("highshelf", 12000.0f, 2.8f, 0.7f) }));
    presets.add(preset("Strings Body", "Dynamic/Instruments", { band("bell", 250.0f, 1.8f, 1.0f), band("bell", 2200.0f, -1.0f, 1.6f), band("highshelf", 9000.0f, 1.0f, 0.8f) }));
    presets.add(preset("Brass Honk Remove", "Dynamic/Instruments", { band("highpass", 90.0f, 0.0f, 0.707f, 12), band("bell", 850.0f, -3.0f, 2.1f), band("bell", 3500.0f, 1.5f, 1.2f) }));
    presets.add(preset("Brass Presence", "Dynamic/Instruments", { band("highpass", 90.0f, 0.0f, 0.707f, 12), band("bell", 2200.0f, 2.2f, 1.0f), band("highshelf", 8000.0f, 1.5f, 0.8f) }));
    presets.add(preset("Synth Bass Fundamental", "Dynamic/Instruments", { band("lowshelf", 55.0f, 3.5f, 0.7f), band("bell", 150.0f, -1.5f, 1.1f), band("highpass", 28.0f, 0.0f, 0.707f, 24) }));
    presets.add(preset("Synth Lead Cut-Through", "Dynamic/Instruments", { band("highpass", 150.0f, 0.0f, 0.707f, 12), band("bell", 2800.0f, 2.5f, 1.1f), band("highshelf", 9000.0f, 2.0f, 0.7f) }));
    presets.add(preset("Synth Pad Widen", "Dynamic/Instruments", { band("highpass", 120.0f, 0.0f, 0.707f, 12), band("bell", 400.0f, -2.0f, 1.0f), band("highshelf", 10000.0f, 1.8f, 0.8f), band("bell", 7000.0f, 1.0f, 1.2f, 12, "side") }));
    presets.add(preset("Organ Clarity", "Dynamic/Instruments", { band("highpass", 70.0f, 0.0f, 0.707f, 12), band("bell", 320.0f, -2.0f, 1.2f), band("bell", 2600.0f, 1.8f, 1.0f) }));

    presets.add(preset("Mix Bus Gentle", "Dynamic/Mix", { band("lowshelf", 90.0f, 1.0f, 0.7f), band("bell", 320.0f, -1.0f, 1.0f), band("highshelf", 12000.0f, 1.2f, 0.7f) }));
    presets.add(preset("Mix Bus Smile Curve", "Dynamic/Mix", { band("lowshelf", 80.0f, 1.5f, 0.7f), band("bell", 700.0f, -1.5f, 0.8f), band("highshelf", 10000.0f, 1.8f, 0.7f) }));
    presets.add(preset("Mix Bus Tilt Bright", "Dynamic/Mix", { band("tiltshelf", 1000.0f, 1.8f, 0.7f) }));
    presets.add(preset("Mix Bus Tilt Dark", "Dynamic/Mix", { band("tiltshelf", 1000.0f, -1.8f, 0.7f) }));
    presets.add(preset("Mix Bus Mid Scoop", "Dynamic/Mix", { band("bell", 650.0f, -1.8f, 0.9f), band("lowshelf", 90.0f, 0.8f, 0.8f), band("highshelf", 9500.0f, 1.2f, 0.8f) }));
    presets.add(preset("Mastering Subtle Lift", "Dynamic/Mix", { band("lowshelf", 70.0f, 0.8f, 0.8f), band("highshelf", 14000.0f, 0.8f, 0.8f) }));
    presets.add(preset("Mastering Low End Tighten", "Dynamic/Mix", { band("highpass", 26.0f, 0.0f, 0.707f, 24), band("bell", 180.0f, -1.2f, 1.1f), band("lowshelf", 70.0f, 0.6f, 0.8f) }));
    presets.add(preset("Mastering High End Air", "Dynamic/Mix", { band("bell", 3500.0f, -0.8f, 1.8f), band("highshelf", 15000.0f, 1.6f, 0.7f) }));
    presets.add(preset("Mastering Final Polish", "Dynamic/Mix", { band("highpass", 24.0f, 0.0f, 0.707f, 12), band("bell", 280.0f, -0.8f, 1.0f), band("highshelf", 12000.0f, 1.0f, 0.8f), band("tiltshelf", 1600.0f, 0.8f, 0.7f) }));

    presets.add(preset("Male Vocal Warmth", "Dynamic/Vocals", { band("highpass", 80.0f, 0.0f, 0.707f, 12), band("bell", 180.0f, 1.8f, 1.1f), band("bell", 320.0f, -1.5f, 1.4f), band("highshelf", 9000.0f, 1.0f, 0.8f) }));
    presets.add(preset("Male Vocal Presence", "Dynamic/Vocals", { band("highpass", 80.0f, 0.0f, 0.707f, 12), band("bell", 350.0f, -1.2f, 1.4f), band("bell", 2800.0f, 2.2f, 1.0f), band("highshelf", 10000.0f, 1.2f, 0.8f) }));
    presets.add(preset("Male Vocal De-Mud", "Dynamic/Vocals", { band("highpass", 75.0f, 0.0f, 0.707f, 12), band("bell", 240.0f, -3.0f, 1.2f), band("bell", 4200.0f, 1.0f, 1.4f) }));
    presets.add(preset("Female Vocal Warmth", "Dynamic/Vocals", { band("highpass", 95.0f, 0.0f, 0.707f, 12), band("bell", 220.0f, 1.0f, 1.2f), band("bell", 420.0f, -1.8f, 1.5f), band("highshelf", 12000.0f, 1.0f, 0.8f) }));
    presets.add(preset("Female Vocal Air", "Dynamic/Vocals", { band("highpass", 95.0f, 0.0f, 0.707f, 12), band("bell", 3200.0f, 1.2f, 1.4f), band("highshelf", 14000.0f, 2.4f, 0.7f) }));
    presets.add(preset("Female Vocal De-Mud", "Dynamic/Vocals", { band("highpass", 90.0f, 0.0f, 0.707f, 12), band("bell", 280.0f, -2.8f, 1.3f), band("bell", 2800.0f, 1.0f, 1.2f) }));
    presets.add(preset("Vocal Telephone Effect", "Dynamic/Vocals", { band("highpass", 280.0f, 0.0f, 0.707f, 24), band("lowpass", 3400.0f, 0.0f, 0.707f, 24), band("bell", 1600.0f, 1.5f, 1.0f) }));
    presets.add(preset("Vocal Radio Ready", "Dynamic/Vocals", { band("highpass", 80.0f, 0.0f, 0.707f, 12), band("bell", 300.0f, -2.5f, 1.5f), band("bell", 2400.0f, 1.8f, 1.1f), band("highshelf", 10000.0f, 2.0f, 0.8f) }));
    presets.add(preset("Vocal Podcast Clean", "Dynamic/Vocals", { band("highpass", 70.0f, 0.0f, 0.707f, 12), band("bell", 220.0f, -2.0f, 1.2f), band("bell", 3200.0f, 1.5f, 1.1f), band("highshelf", 9000.0f, 0.8f, 0.8f) }));
    presets.add(preset("Vocal De-Honk 300-500 Hz", "Dynamic/Vocals", { band("bell", 380.0f, -3.0f, 2.0f), band("highpass", 75.0f, 0.0f, 0.707f, 12) }));
    presets.add(preset("Vocal De-Harsh 2-4 kHz", "Dynamic/Vocals", { band("bell", 3000.0f, -2.5f, 2.0f), band("highshelf", 11000.0f, 0.8f, 0.8f) }));
    presets.add(preset("Vocal De-Sibilance Shelf", "Dynamic/Vocals", { band("highshelf", 7600.0f, -2.5f, 0.8f), band("bell", 4200.0f, -1.0f, 1.8f) }));
    presets.add(preset("Background Vocals Tuck", "Dynamic/Vocals", { band("highpass", 120.0f, 0.0f, 0.707f, 12), band("bell", 2500.0f, -1.0f, 1.2f), band("highshelf", 9000.0f, -0.8f, 0.8f), band("bell", 350.0f, -1.5f, 1.0f) }));
    presets.add(preset("Vocal Double Brighten", "Dynamic/Vocals", { band("highpass", 110.0f, 0.0f, 0.707f, 12), band("bell", 2600.0f, 1.5f, 1.3f), band("highshelf", 13000.0f, 2.8f, 0.7f) }));

    presets.add(preset("Mid Boost Presence", "Stereo", { band("bell", 2800.0f, 2.2f, 1.1f, 12, "mid"), band("bell", 300.0f, -0.8f, 1.2f, 12, "mid") }));
    presets.add(preset("Mid Scoop Width", "Stereo", { band("bell", 600.0f, -2.0f, 0.9f, 12, "mid"), band("highshelf", 9000.0f, 1.0f, 0.8f, 12, "side") }));
    presets.add(preset("Side High Shelf Air", "Stereo", { band("highshelf", 12000.0f, 2.5f, 0.7f, 12, "side"), band("highpass", 150.0f, 0.0f, 0.707f, 12, "side") }));
    presets.add(preset("Side Low Cut Tighten", "Stereo", { band("highpass", 130.0f, 0.0f, 0.707f, 24, "side"), band("lowshelf", 120.0f, -1.5f, 0.8f, 12, "side") }));
    presets.add(preset("Mid/Side Vocal Focus", "Stereo", { band("bell", 2500.0f, 2.0f, 1.1f, 12, "mid"), band("bell", 3500.0f, -1.0f, 1.5f, 12, "side"), band("highshelf", 10000.0f, 1.2f, 0.8f, 12, "mid") }));
    presets.add(preset("Mid/Side Wide Mix", "Stereo", { band("lowshelf", 110.0f, -1.0f, 0.8f, 12, "mid"), band("highshelf", 11000.0f, 1.8f, 0.8f, 12, "side"), band("bell", 500.0f, -1.2f, 1.0f, 12, "mid") }));

    return presets;
}
} // namespace

const juce::Array<PresetRecord>& getFactoryPresetLibrary()
{
    static const auto presets = buildFactoryPresetLibrary();
    return presets;
}
} // namespace sspeq::presets
