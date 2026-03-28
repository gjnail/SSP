#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace factorypresetbank
{
enum class PackStyle
{
    skyline,
    midnight,
    rhythm,
    pressure,
    solar = skyline,
    ground = rhythm,
    halo = midnight,
    shattered = pressure
};

enum class SoundClass
{
    lead,
    pluck,
    chord,
    bass,
    key,
    organ,
    pad,
    drone,
    texture,
    vocal,
    bell,
    fx,
    sequence,
    percussion,
    stringer = 100,
    hybrid = 101
};

enum class Recipe
{
    mainstageLead,
    futureRaveHook,
    festivalPluck,
    mainroomChord,
    festivalBrass,
    tranceLead,
    tranceGatePluck,
    tranceDreamPad,
    progressivePluck,
    progressiveKeys,
    melodicTechnoLead,
    melodicTechnoBass,
    melodicTechnoSequence,
    techHouseBass,
    techHouseStab,
    acidBass,
    peakTechnoBass,
    peakTechnoStab,
    darkDrone,
    cinemaTexture,
    afroPluck,
    afroOrgan,
    afroVocal,
    afroChord,
    ukgChord,
    ukgBass,
    speedGarageStab,
    basslineHook,
    breaksLead,
    breaksSequence,
    dnbReese,
    dnbNeuro,
    dnbRoller,
    dnbAtmos,
    bellArp,
    riserFx,
    impactFx,
    percSynth,
    hybridDigitalLead,
    glitchTexture
};

struct PresetSeed
{
    PluginProcessor::FactoryPresetInfo info;
    PackStyle packStyle = PackStyle::skyline;
    SoundClass soundClass = SoundClass::lead;
    Recipe recipe = Recipe::mainstageLead;
    float brightness = 0.5f;
    float movement = 0.5f;
    float weight = 0.5f;
    float width = 0.5f;
    float edge = 0.5f;
    float air = 0.5f;
    float contour = 0.5f;
    float special = 0.5f;
};

const juce::Array<PresetSeed>& getPresetSeeds();
const juce::Array<PluginProcessor::FactoryPresetInfo>& getPresetLibrary();
}
