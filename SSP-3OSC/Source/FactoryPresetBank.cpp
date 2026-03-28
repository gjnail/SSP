#include "FactoryPresetBank.h"

#include <array>
#include <cmath>

namespace factorypresetbank
{
namespace
{
constexpr int presetsPerGroup = 5;

struct GroupDefinition
{
    PackStyle packStyle;
    const char* pack;
    const char* category;
    const char* subCategory;
    SoundClass soundClass;
    Recipe recipe;
    std::array<const char*, presetsPerGroup> names;
    const char* usage;
    std::array<float, 8> profile;
};

float hash01(int a, int b, int c)
{
    const double raw = std::sin((double) a * 12.9898 + (double) b * 78.233 + (double) c * 37.719) * 43758.5453;
    return (float) (raw - std::floor(raw));
}

float sweepValue(int localIndex, int multiplier)
{
    return (float) ((localIndex * multiplier) % presetsPerGroup) / (float) (presetsPerGroup - 1);
}

const std::array<std::array<float, 8>, presetsPerGroup>& getPresetLaneOffsets()
{
    static const std::array<std::array<float, 8>, presetsPerGroup> offsets
    {{
        { 0.10f, -0.08f, 0.08f, -0.10f, 0.10f, -0.12f, 0.12f, -0.06f },
        { 0.08f,  0.00f, -0.04f, 0.16f, -0.08f, 0.18f, -0.04f, 0.04f },
        { -0.02f, 0.18f, 0.02f, -0.04f, 0.12f, -0.10f, 0.18f, 0.10f },
        { -0.08f, 0.04f, 0.18f, -0.12f, 0.18f, -0.16f, 0.06f, 0.18f },
        { 0.04f,  0.14f, -0.08f, 0.10f, -0.02f, 0.16f, -0.06f, 0.20f }
    }};

    return offsets;
}

float buildSeedValue(float base,
                     int groupIndex,
                     int localIndex,
                     int dimensionIndex,
                     int multiplier,
                     int salt,
                     float sweepAmount,
                     float jitterAmount)
{
    const auto& laneOffsets = getPresetLaneOffsets();
    const float laneOffset = laneOffsets[(size_t) juce::jlimit(0, presetsPerGroup - 1, localIndex)][(size_t) juce::jlimit(0, 7, dimensionIndex)];
    const float sweep = (sweepValue(localIndex, multiplier) - 0.5f) * sweepAmount;
    const float jitter = (hash01(groupIndex + 1, localIndex + 3, salt) - 0.5f) * jitterAmount;
    return juce::jlimit(0.0f, 1.0f, base + laneOffset + sweep + jitter);
}

juce::String describeBrightness(float brightness, float air)
{
    if (brightness > 0.72f)
        return air > 0.62f ? "Bright and high-gloss" : "Bright and cutting";

    if (brightness < 0.32f)
        return air > 0.45f ? "Dark but still open" : "Dark and weighty";

    return air > 0.65f ? "Balanced and airy" : "Balanced and focused";
}

juce::String describeMotion(float movement)
{
    if (movement > 0.72f)
        return "It leans into rhythmic motion and animated filter travel.";

    if (movement < 0.32f)
        return "It stays more planted and centered on held notes.";

    return "It carries enough motion to stay alive in a modern mix.";
}

juce::String describeEdge(float edge, float contour)
{
    if (edge > 0.68f)
        return contour > 0.68f ? "Transient behavior is sharp and assertive." : "The tone pushes forward with club-ready bite.";

    if (edge < 0.30f)
        return contour < 0.35f ? "The envelope blooms gently with softer edges." : "The tone stays rounded instead of aggressive.";

    return "The attack sits in a polished middle ground between snap and smoothness.";
}

juce::String describePresetPosture(int localIndex)
{
    switch (juce::jlimit(0, presetsPerGroup - 1, localIndex))
    {
        case 0:  return "This version stays direct and mix-forward.";
        case 1:  return "This version opens up into a wider stereo silhouette.";
        case 2:  return "This version leans into more rhythmic articulation.";
        case 3:  return "This version pushes harder on drive and low-mid pressure.";
        default: return "This version reaches for a more synthetic, effected finish.";
    }
}

juce::String buildDescription(const GroupDefinition& group,
                              float brightness,
                              float movement,
                              float edge,
                              float air,
                              float contour,
                              int localIndex)
{
    return describeBrightness(brightness, air)
        + " "
        + juce::String(group.subCategory).toLowerCase()
        + " "
        + juce::String(group.category).toLowerCase()
        + " voiced for "
        + group.usage
        + " "
        + describeMotion(movement)
        + " "
        + describeEdge(edge, contour)
        + " "
        + describePresetPosture(localIndex);
}

const std::array<GroupDefinition, 40>& getGroupDefinitions()
{
    using R = Recipe;
    using S = SoundClass;
    using P = PackStyle;

    static const std::array<GroupDefinition, 40> groups
    {{
        {
            P::skyline,
            "Skyline Mainroom",
            "Lead",
            "Mainstage",
            S::lead,
            R::mainstageLead,
            { "Atlas Lift", "Crown Voltage", "Horizon Siren", "Nova Anthem", "Skyline Helix" },
            "festival hooks, stacked refrains, and peak-hour toplines.",
            { 0.84f, 0.52f, 0.58f, 0.92f, 0.42f, 0.68f, 0.50f, 0.34f }
        },
        {
            P::skyline,
            "Skyline Mainroom",
            "Lead",
            "Future Rave",
            S::lead,
            R::futureRaveHook,
            { "Carbon Halo", "Chrome Heat", "Circuit Empire", "Plasma Regent", "Titan Spark" },
            "future-rave hooks, crossover drops, and tense mainroom phrases.",
            { 0.76f, 0.66f, 0.70f, 0.54f, 0.62f, 0.40f, 0.58f, 0.70f }
        },
        {
            P::skyline,
            "Skyline Mainroom",
            "Pluck",
            "Festival",
            S::pluck,
            R::festivalPluck,
            { "Candy Vector", "Glitter Drive", "Neon Petal", "Prism Bounce", "Velvet Pixel" },
            "bright chord shots, melodic riffing, and vocal-friendly drop loops.",
            { 0.80f, 0.48f, 0.34f, 0.72f, 0.30f, 0.58f, 0.88f, 0.38f }
        },
        {
            P::skyline,
            "Skyline Mainroom",
            "Chord",
            "Mainroom",
            S::chord,
            R::mainroomChord,
            { "Alloy Banner", "Diamond Rush", "Empire Chord", "Satin Strike", "Silver Plaza" },
            "festival stabs, anthem support chords, and broad harmonic lifts.",
            { 0.68f, 0.36f, 0.54f, 0.62f, 0.38f, 0.42f, 0.70f, 0.28f }
        },
        {
            P::skyline,
            "Skyline Mainroom",
            "Brass",
            "Festival",
            S::chord,
            R::festivalBrass,
            { "Brass Torrent", "Hero Signal", "Iron Flare", "Rally Forge", "Stadium Torch" },
            "heroic brass stacks, festival hits, and impact-heavy chord punches.",
            { 0.74f, 0.42f, 0.66f, 0.46f, 0.60f, 0.34f, 0.58f, 0.50f }
        },
        {
            P::skyline,
            "Skyline Mainroom",
            "Lead",
            "Trance",
            S::lead,
            R::tranceLead,
            { "Aurora Flight", "Celestial Dash", "Eon Voyage", "Pulse Horizon", "Star Sail" },
            "euphoric trance leads, cresting melodies, and classic uplift energy.",
            { 0.88f, 0.62f, 0.46f, 0.94f, 0.36f, 0.84f, 0.56f, 0.42f }
        },
        {
            P::skyline,
            "Skyline Mainroom",
            "Pluck",
            "Gate",
            S::pluck,
            R::tranceGatePluck,
            { "Crystal Gate", "Lumen Step", "Orbit Tick", "Sky Pattern", "Zenith Pulse" },
            "gated trance patterns, bright arp work, and energetic breakdown figures.",
            { 0.82f, 0.72f, 0.30f, 0.76f, 0.28f, 0.72f, 0.92f, 0.36f }
        },
        {
            P::skyline,
            "Skyline Mainroom",
            "Pad",
            "Dream",
            S::pad,
            R::tranceDreamPad,
            { "Dreamstate Arc", "Ether Bloom", "Halo Span", "Lucid Expanse", "Seraph Drift" },
            "dreamy breakdown pads, euphoric chord beds, and emotional intros.",
            { 0.72f, 0.30f, 0.28f, 0.96f, 0.12f, 0.94f, 0.24f, 0.44f }
        },
        {
            P::skyline,
            "Skyline Mainroom",
            "Pluck",
            "Progressive",
            S::pluck,
            R::progressivePluck,
            { "Amber Terrace", "Glass Harbour", "Mirage Stair", "Reflect Avenue", "Sunline Echo" },
            "progressive house melodies, rolling chord plucks, and warm hook lines.",
            { 0.68f, 0.40f, 0.34f, 0.70f, 0.22f, 0.76f, 0.72f, 0.30f }
        },
        {
            P::skyline,
            "Skyline Mainroom",
            "Keys",
            "Progressive",
            S::key,
            R::progressiveKeys,
            { "Afterglow Keys", "Balearic Glass", "Golden Atrium", "Marina Tide", "Rooftop Memory" },
            "progressive chords, sunset leads, and emotional house songwriting.",
            { 0.60f, 0.22f, 0.42f, 0.68f, 0.18f, 0.70f, 0.52f, 0.24f }
        },
        {
            P::midnight,
            "Midnight Circuits",
            "Lead",
            "Melodic Techno",
            S::lead,
            R::melodicTechnoLead,
            { "Afterdark Beacon", "Axis Bloom", "Noir Current", "Pulse Cathedral", "Signal Ember" },
            "melodic techno toplines, cinematic hooks, and night-drive phrases.",
            { 0.64f, 0.62f, 0.56f, 0.62f, 0.50f, 0.50f, 0.52f, 0.66f }
        },
        {
            P::midnight,
            "Midnight Circuits",
            "Bass",
            "Melodic Techno",
            S::bass,
            R::melodicTechnoBass,
            { "Black Marble", "Depth Vector", "Engine Below", "Night Coil", "Tunnel Metric" },
            "melodic techno bass foundations, dark drives, and hypnotic low-end.",
            { 0.30f, 0.42f, 0.88f, 0.18f, 0.58f, 0.10f, 0.58f, 0.52f }
        },
        {
            P::midnight,
            "Midnight Circuits",
            "Sequence",
            "Melodic Techno",
            S::sequence,
            R::melodicTechnoSequence,
            { "Grid Chapel", "Iron Motive", "Motion Column", "Rail Syntax", "Shadow Relay" },
            "melodic techno sequences, motoric pulses, and evolving pattern work.",
            { 0.58f, 0.86f, 0.42f, 0.46f, 0.48f, 0.42f, 0.82f, 0.68f }
        },
        {
            P::midnight,
            "Midnight Circuits",
            "Bass",
            "Tech House",
            S::bass,
            R::techHouseBass,
            { "Block Party Low", "Club Lever", "Floor Jack", "Rubber Freight", "Warehouse Grip" },
            "tech-house grooves, short punchy drops, and low-end pocket work.",
            { 0.34f, 0.34f, 0.84f, 0.14f, 0.54f, 0.08f, 0.76f, 0.36f }
        },
        {
            P::midnight,
            "Midnight Circuits",
            "Stab",
            "Tech House",
            S::chord,
            R::techHouseStab,
            { "Brick Snap", "Cut Signal", "Dockyard Hit", "Loop Quarter", "Slate Jab" },
            "tech-house stabs, dry chord chops, and groove-led fills.",
            { 0.56f, 0.48f, 0.48f, 0.44f, 0.42f, 0.24f, 0.82f, 0.32f }
        },
        {
            P::midnight,
            "Midnight Circuits",
            "Bass",
            "Acid",
            S::bass,
            R::acidBass,
            { "303 District", "Acid Vandal", "Lime Circuit", "Resonant Mile", "Warehouse Serpent" },
            "acid runs, warehouse loops, and resonance-heavy club sequences.",
            { 0.48f, 0.78f, 0.66f, 0.16f, 0.82f, 0.14f, 0.88f, 0.94f }
        },
        {
            P::midnight,
            "Midnight Circuits",
            "Bass",
            "Peak Techno",
            S::bass,
            R::peakTechnoBass,
            { "Concrete Flame", "Pressure Siren", "Rave Diesel", "Steel Torque", "Tunnel Charger" },
            "peak-time techno drops, warehouse pressure, and aggressive low-end thrust.",
            { 0.40f, 0.56f, 0.90f, 0.22f, 0.74f, 0.12f, 0.64f, 0.62f }
        },
        {
            P::midnight,
            "Midnight Circuits",
            "Stab",
            "Peak Techno",
            S::chord,
            R::peakTechnoStab,
            { "Blackout Jab", "Flash Bunker", "Floodlight Hit", "Riot Beacon", "Siren Plate" },
            "peak-time techno stabs, rave punctuation, and warehouse lead accents.",
            { 0.68f, 0.74f, 0.44f, 0.48f, 0.78f, 0.24f, 0.90f, 0.72f }
        },
        {
            P::midnight,
            "Midnight Circuits",
            "Drone",
            "Dark",
            S::drone,
            R::darkDrone,
            { "Ash Monolith", "Coal Atmosphere", "Null Basilica", "Obsidian Slowburn", "Phantom Vault" },
            "dark intros, ominous breakdowns, and low-motion tension beds.",
            { 0.14f, 0.24f, 0.70f, 0.60f, 0.28f, 0.42f, 0.12f, 0.56f }
        },
        {
            P::midnight,
            "Midnight Circuits",
            "Texture",
            "Cinema",
            S::texture,
            R::cinemaTexture,
            { "Dust Theatre", "Film Haze", "Grey Particles", "Signal Weather", "Static Panorama" },
            "cinematic washes, transition layers, and atmospheric motion design.",
            { 0.38f, 0.68f, 0.30f, 0.82f, 0.26f, 0.88f, 0.22f, 0.80f }
        },
        {
            P::rhythm,
            "Rhythm Borough",
            "Pluck",
            "Afro",
            S::pluck,
            R::afroPluck,
            { "Baobab Spark", "Copper Ritual", "Dusk Palm", "Kora Jet", "Sunskin Pulse" },
            "afro-house riffs, rhythmic chord plucks, and warm melodic syncopation.",
            { 0.60f, 0.54f, 0.36f, 0.58f, 0.26f, 0.62f, 0.80f, 0.42f }
        },
        {
            P::rhythm,
            "Rhythm Borough",
            "Organ",
            "Afro",
            S::organ,
            R::afroOrgan,
            { "Cedar Prayer", "Earth Rotor", "Horizon Shrine", "Nomad Gospel", "Safari Velvet" },
            "afro-house organs, spiritual grooves, and rolling harmonic movement.",
            { 0.48f, 0.46f, 0.48f, 0.36f, 0.26f, 0.40f, 0.48f, 0.58f }
        },
        {
            P::rhythm,
            "Rhythm Borough",
            "Vocal",
            "Afro",
            S::vocal,
            R::afroVocal,
            { "Desert Vow", "Ember Choir", "Mirage Calling", "Oasis Breath", "Sand Hymn" },
            "afro-house vocals, breathy hooks, and emotive breakdown swells.",
            { 0.62f, 0.42f, 0.34f, 0.72f, 0.18f, 0.82f, 0.38f, 0.90f }
        },
        {
            P::rhythm,
            "Rhythm Borough",
            "Chord",
            "Afro",
            S::chord,
            R::afroChord,
            { "Bronze Lagoon", "Coastal Ceremony", "Sunset Ancestry", "Tide Lantern", "Tribal Glow" },
            "afro-house chords, rolling progressions, and percussive harmony beds.",
            { 0.58f, 0.38f, 0.44f, 0.64f, 0.24f, 0.56f, 0.66f, 0.46f }
        },
        {
            P::rhythm,
            "Rhythm Borough",
            "Chord",
            "UK Garage",
            S::chord,
            R::ukgChord,
            { "2Step Plaza", "Blue Estate", "Croydon Silk", "Garage Fluorescent", "Southbank Tone" },
            "UK garage chords, shuffling organ shots, and two-step hooks.",
            { 0.66f, 0.58f, 0.40f, 0.56f, 0.34f, 0.44f, 0.90f, 0.40f }
        },
        {
            P::rhythm,
            "Rhythm Borough",
            "Bass",
            "UK Garage",
            S::bass,
            R::ukgBass,
            { "Bounce Borough", "Curb Pressure", "Elastic Alley", "Low Pavement", "Subside Lane" },
            "UK garage basslines, swing-heavy subs, and elastic low-end movement.",
            { 0.32f, 0.66f, 0.78f, 0.18f, 0.48f, 0.10f, 0.72f, 0.44f }
        },
        {
            P::rhythm,
            "Rhythm Borough",
            "Stab",
            "Speed Garage",
            S::chord,
            R::speedGarageStab,
            { "Cutback Organ", "Forward Slip", "Hustle Chord", "Shuffle Beam", "Snap Carriage" },
            "speed-garage stabs, pumped organ chops, and fast shuffle hooks.",
            { 0.70f, 0.74f, 0.34f, 0.52f, 0.44f, 0.36f, 0.94f, 0.50f }
        },
        {
            P::rhythm,
            "Rhythm Borough",
            "Lead",
            "Bassline",
            S::lead,
            R::basslineHook,
            { "Bassline Battery", "Grit Swing", "Neon Curb", "Reload Valve", "Wicked Lever" },
            "bassline hooks, rude mids, and aggressive club-call phrases.",
            { 0.62f, 0.72f, 0.74f, 0.22f, 0.70f, 0.18f, 0.72f, 0.64f }
        },
        {
            P::rhythm,
            "Rhythm Borough",
            "Lead",
            "Breaks",
            S::lead,
            R::breaksLead,
            { "Breakroom Laser", "Corner Voltage", "Flip Horizon", "Split Current", "Stereo Riot" },
            "breakbeat hooks, electro crossover leads, and edgy rave phrases.",
            { 0.72f, 0.78f, 0.50f, 0.68f, 0.52f, 0.54f, 0.62f, 0.68f }
        },
        {
            P::rhythm,
            "Rhythm Borough",
            "Sequence",
            "Breaks",
            S::sequence,
            R::breaksSequence,
            { "Broken Clockwork", "Detour Runner", "Edge Pattern", "Junction Click", "Rotor Skip" },
            "breakbeat sequences, syncopated synth loops, and electro percussion lines.",
            { 0.56f, 0.88f, 0.38f, 0.52f, 0.46f, 0.38f, 0.86f, 0.72f }
        },
        {
            P::pressure,
            "Pressure Engine",
            "Bass",
            "Reese",
            S::bass,
            R::dnbReese,
            { "Alloy Menace", "Grid Predator", "Night Howler", "Razor Subway", "Vector Fang" },
            "drum-and-bass reese lines, rolling drops, and grimy low-mid pressure.",
            { 0.34f, 0.64f, 0.88f, 0.28f, 0.66f, 0.12f, 0.58f, 0.70f }
        },
        {
            P::pressure,
            "Pressure Engine",
            "Bass",
            "Neuro",
            S::bass,
            R::dnbNeuro,
            { "Binary Crusher", "Chrome Pathogen", "Fracture Engine", "Plasma Toxin", "Surge Mutation" },
            "neuro bass design, high-pressure drops, and hyper-detailed movement.",
            { 0.52f, 0.82f, 0.86f, 0.30f, 0.82f, 0.18f, 0.74f, 0.96f }
        },
        {
            P::pressure,
            "Pressure Engine",
            "Lead",
            "Roller",
            S::lead,
            R::dnbRoller,
            { "Chase Module", "Fastlane Echo", "Midnight Dash", "Roller Signal", "Sprint Theory" },
            "drum-and-bass hooks, rolling mids, and quick-response melodic pressure.",
            { 0.60f, 0.78f, 0.60f, 0.34f, 0.56f, 0.30f, 0.76f, 0.58f }
        },
        {
            P::pressure,
            "Pressure Engine",
            "Pad",
            "Atmos",
            S::pad,
            R::dnbAtmos,
            { "Cold Rain Atlas", "Hush Overpass", "Neon Fog", "Silver Drizzle", "Urban Aurora" },
            "liquid-style atmospheres, misty intros, and tension-softening breakdown space.",
            { 0.42f, 0.44f, 0.34f, 0.88f, 0.20f, 0.86f, 0.30f, 0.54f }
        },
        {
            P::pressure,
            "Pressure Engine",
            "Bell",
            "Arp",
            S::bell,
            R::bellArp,
            { "Alloy Starlight", "Digital Chime", "Ice Sonar", "Quartz Blink", "Vector Glass" },
            "arp tops, crystalline accents, and high-register rhythmic sparkle.",
            { 0.86f, 0.86f, 0.20f, 0.58f, 0.38f, 0.74f, 0.92f, 0.88f }
        },
        {
            P::pressure,
            "Pressure Engine",
            "FX",
            "Riser",
            S::fx,
            R::riserFx,
            { "Arc Ascender", "Build Reactor", "Climb Theory", "Pressure Lift", "Voltage Tower" },
            "rises, sweep layers, and long-build tension work.",
            { 0.70f, 0.96f, 0.24f, 0.76f, 0.46f, 0.72f, 0.52f, 0.98f }
        },
        {
            P::pressure,
            "Pressure Engine",
            "FX",
            "Impact",
            S::fx,
            R::impactFx,
            { "Blast Marker", "Drop Hammer", "Floorquake Stamp", "Shock Aperture", "Slam Transit" },
            "drop accents, impact layers, and transition punctuation.",
            { 0.46f, 0.62f, 0.68f, 0.28f, 0.84f, 0.18f, 0.98f, 0.82f }
        },
        {
            P::pressure,
            "Pressure Engine",
            "Perc",
            "Synth",
            S::percussion,
            R::percSynth,
            { "Anvil Click", "Carbon Knock", "Forge Tick", "Metal Thumb", "Rivet Pop" },
            "synthetic percussion, metallic tops, and transient detail layers.",
            { 0.54f, 0.60f, 0.36f, 0.20f, 0.78f, 0.16f, 0.96f, 0.84f }
        },
        {
            P::pressure,
            "Pressure Engine",
            "Lead",
            "Digital",
            S::lead,
            R::hybridDigitalLead,
            { "Data Halo", "Fracture Crown", "Hyper Relay", "Quantum Script", "Static Driver" },
            "hybrid digital leads, processed hooks, and crossover synth-top moments.",
            { 0.64f, 0.74f, 0.56f, 0.58f, 0.64f, 0.40f, 0.60f, 0.92f }
        },
        {
            P::pressure,
            "Pressure Engine",
            "Texture",
            "Glitch",
            S::texture,
            R::glitchTexture,
            { "Broken Ribbon", "Codec Mist", "Error Bloom", "Packet Dust", "Stutter Cloud" },
            "glitch beds, broken transitions, and digital atmosphere design.",
            { 0.44f, 0.90f, 0.28f, 0.74f, 0.34f, 0.78f, 0.28f, 0.94f }
        }
    }};

    return groups;
}
}

const juce::Array<PresetSeed>& getPresetSeeds()
{
    static const juce::Array<PresetSeed> seeds = []
    {
        juce::Array<PresetSeed> items;
        int globalIndex = 0;

        const auto& groups = getGroupDefinitions();
        for (int groupIndex = 0; groupIndex < (int) groups.size(); ++groupIndex)
        {
            const auto& group = groups[(size_t) groupIndex];
            for (int localIndex = 0; localIndex < presetsPerGroup; ++localIndex)
            {
                PresetSeed seed;
                seed.info.index = globalIndex++;
                seed.info.name = group.names[(size_t) localIndex];
                seed.info.pack = group.pack;
                seed.info.category = group.category;
                seed.info.subCategory = group.subCategory;
                seed.packStyle = group.packStyle;
                seed.soundClass = group.soundClass;
                seed.recipe = group.recipe;
                seed.brightness = buildSeedValue(group.profile[0], groupIndex, localIndex, 0, 1, 11, 0.24f, 0.14f);
                seed.movement = buildSeedValue(group.profile[1], groupIndex, localIndex, 1, 2, 17, 0.24f, 0.16f);
                seed.weight = buildSeedValue(group.profile[2], groupIndex, localIndex, 2, 3, 23, 0.18f, 0.14f);
                seed.width = buildSeedValue(group.profile[3], groupIndex, localIndex, 3, 4, 29, 0.22f, 0.14f);
                seed.edge = buildSeedValue(group.profile[4], groupIndex, localIndex, 4, 5, 31, 0.20f, 0.16f);
                seed.air = buildSeedValue(group.profile[5], groupIndex, localIndex, 5, 6, 37, 0.22f, 0.14f);
                seed.contour = buildSeedValue(group.profile[6], groupIndex, localIndex, 6, 7, 41, 0.24f, 0.16f);
                seed.special = buildSeedValue(group.profile[7], groupIndex, localIndex, 7, 8, 43, 0.28f, 0.18f);
                seed.info.description = buildDescription(group,
                                                         seed.brightness,
                                                         seed.movement,
                                                         seed.edge,
                                                         seed.air,
                                                         seed.contour,
                                                         localIndex);
                items.add(seed);
            }
        }

        return items;
    }();

    return seeds;
}

const juce::Array<PluginProcessor::FactoryPresetInfo>& getPresetLibrary()
{
    static const juce::Array<PluginProcessor::FactoryPresetInfo> library = []
    {
        juce::Array<PluginProcessor::FactoryPresetInfo> items;
        for (const auto& seed : getPresetSeeds())
            items.add(seed.info);
        return items;
    }();

    return library;
}
}
