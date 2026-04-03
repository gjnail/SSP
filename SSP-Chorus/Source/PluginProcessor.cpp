#include "PluginProcessor.h"
#include "ChorusPresetLibrary.h"
#include "PluginEditor.h"

namespace
{
using Parameter = juce::AudioProcessorValueTreeState::Parameter;
using Attributes = juce::AudioProcessorValueTreeStateParameterAttributes;

constexpr float maxDelayMs = 32.0f;
constexpr float maxMotionDelayMs = 42.0f;
constexpr auto presetStateWrapperTag = "SSPCHORUS_STATE";
constexpr auto presetMetaTag = "PRESET_META";
constexpr auto presetFileExtension = ".sspchoruspreset";

const std::array<const char*, 24> presetParameterIDs{{
    "loCutHz", "hiCutHz", "motionAmount", "motionRate", "motionShape", "motionSpin",
    "voiceCrossoverHz", "lowVoiceAmount", "lowVoiceScale", "lowVoiceRate",
    "highVoiceAmount", "highVoiceScale", "highVoiceRate", "delayMs", "feedback",
    "tone", "stereoWidth", "drive", "dryWet", "shineAmount", "shineRate",
    "spreadMs", "vibrato", "focusCut"
}};

juce::NormalisableRange<float> makeLogRange(float minValue, float maxValue, float interval = 0.0f)
{
    juce::NormalisableRange<float> range(minValue, maxValue, interval);
    range.setSkewForCentre(std::sqrt(minValue * maxValue));
    return range;
}

juce::String formatHz(float value)
{
    if (value >= 1000.0f)
        return juce::String(value / 1000.0f, value >= 10000.0f ? 1 : 2) + " kHz";

    return juce::String(juce::roundToInt(value)) + " Hz";
}

juce::String formatPercent(float value)
{
    return juce::String(juce::roundToInt(value)) + "%";
}

juce::String formatMilliseconds(float value)
{
    return juce::String(value, value < 10.0f ? 2 : 1) + " ms";
}

juce::String formatRate(float value)
{
    return juce::String(value, value < 1.0f ? 2 : 1) + " Hz";
}

float parseNumeric(const juce::String& text)
{
    juce::String cleaned;
    cleaned.preallocateBytes(text.getNumBytesAsUTF8());

    for (juce::juce_wchar ch : text)
    {
        if ((ch >= '0' && ch <= '9') || ch == '.' || ch == '-')
            cleaned << ch;
    }

    return cleaned.getFloatValue();
}

std::unique_ptr<juce::RangedAudioParameter> makeFloatParameter(const juce::String& id,
                                                               const juce::String& name,
                                                               juce::NormalisableRange<float> range,
                                                               float defaultValue,
                                                               juce::String (*formatter)(float),
                                                               const juce::String& label = {})
{
    return std::make_unique<Parameter>(juce::ParameterID { id, 1 },
                                       name,
                                       range,
                                       defaultValue,
                                       Attributes().withStringFromValueFunction([formatter](float value, int) { return formatter(value); })
                                                   .withValueFromStringFunction([](const juce::String& text) { return parseNumeric(text); })
                                                   .withLabel(label));
}

std::unique_ptr<juce::RangedAudioParameter> makeToggleParameter(const juce::String& id,
                                                                const juce::String& name,
                                                                bool defaultValue)
{
    return std::make_unique<juce::AudioParameterBool>(juce::ParameterID { id, 1 },
                                                      name,
                                                      defaultValue,
                                                      juce::AudioParameterBoolAttributes()
                                                          .withStringFromValueFunction([](bool enabled, int) { return enabled ? "On" : "Off"; })
                                                          .withLabel("state"));
}

float triangleFromPhase(float phase)
{
    return (2.0f / juce::MathConstants<float>::pi) * std::asin(std::sin(phase));
}

juce::String sanitisePresetText(const juce::String& text)
{
    return text.trim().replaceCharacters(":*?\"<>|", "--------");
}

juce::String sanitisePresetCategory(const juce::String& text)
{
    auto category = sanitisePresetText(text).replaceCharacter('\\', '/');
    while (category.contains("//"))
        category = category.replace("//", "/");
    return category.trimCharactersAtStart("/ ").trimCharactersAtEnd("/ ");
}

juce::String sanitisePresetName(const juce::String& text)
{
    return sanitisePresetText(text).trimCharactersAtStart(" ").trimCharactersAtEnd(" ");
}

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

juce::String makePresetFilenameStem(const juce::String& category, const juce::String& name)
{
    const auto categorySlug = makeSlug(category);
    const auto nameSlug = makeSlug(name);
    return categorySlug.isNotEmpty() ? categorySlug + "__" + nameSlug : nameSlug;
}

juce::RangedAudioParameter* getRangedParameter(juce::AudioProcessorValueTreeState& state, const juce::String& id)
{
    return dynamic_cast<juce::RangedAudioParameter*>(state.getParameter(id));
}

const juce::RangedAudioParameter* getRangedParameter(const juce::AudioProcessorValueTreeState& state, const juce::String& id)
{
    return dynamic_cast<const juce::RangedAudioParameter*>(state.getParameter(id));
}

PluginProcessor::PresetValue makeValue(const juce::String& id, float raw)
{
    PluginProcessor::PresetValue value;
    value.id = id;
    value.value = raw;
    return value;
}

juce::var presetValueToVar(const PluginProcessor::PresetValue& value)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("id", value.id);
    object->setProperty("value", value.value);
    return juce::var(object);
}

bool readPresetValueFromVar(const juce::var& valueVar, PluginProcessor::PresetValue& value)
{
    if (auto* object = valueVar.getDynamicObject())
    {
        value.id = object->getProperty("id").toString();
        value.value = (float) object->getProperty("value");
        return value.id.isNotEmpty();
    }

    return false;
}

juce::String serialisePresetRecordToJson(const PluginProcessor::PresetRecord& preset)
{
    juce::Array<juce::var> values;
    for (const auto& item : preset.values)
        values.add(presetValueToVar(item));

    auto* object = new juce::DynamicObject();
    object->setProperty("name", preset.name);
    object->setProperty("category", preset.category);
    object->setProperty("author", preset.author);
    object->setProperty("isFactory", preset.isFactory);
    object->setProperty("version", preset.version);
    object->setProperty("values", juce::var(values));
    return juce::JSON::toString(juce::var(object), true);
}

bool readPresetRecordFromJson(const juce::String& json, PluginProcessor::PresetRecord& preset)
{
    const auto parsed = juce::JSON::parse(json);
    auto* object = parsed.getDynamicObject();
    if (object == nullptr)
        return false;

    preset.name = sanitisePresetName(object->getProperty("name").toString());
    preset.category = sanitisePresetCategory(object->getProperty("category").toString());
    preset.author = object->getProperty("author").toString();
    preset.isFactory = (bool) object->getProperty("isFactory");
    preset.version = object->getProperty("version").toString();
    if (preset.version.isEmpty())
        preset.version = "1.0";
    if (preset.author.isEmpty())
        preset.author = "Imported";
    if (preset.name.isEmpty())
        return false;

    preset.values.clearQuick();
    if (auto* array = object->getProperty("values").getArray())
    {
        for (const auto& entry : *array)
        {
            PluginProcessor::PresetValue value;
            if (readPresetValueFromVar(entry, value))
                preset.values.add(value);
        }
    }

    preset.id = makePresetKey(preset.isFactory, preset.category, preset.name);
    return true;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(makeFloatParameter("loCutHz", "Lo Cut", makeLogRange(20.0f, 1600.0f), 80.0f, formatHz, "Hz"));
    params.push_back(makeFloatParameter("hiCutHz", "Hi Cut", makeLogRange(1200.0f, 20000.0f), 16000.0f, formatHz, "Hz"));
    params.push_back(makeFloatParameter("motionAmount", "Motion Amount", { 0.0f, 100.0f, 0.1f }, 42.0f, formatPercent, "%"));
    params.push_back(makeFloatParameter("motionRate", "Motion Rate", { 0.05f, 6.0f, 0.01f }, 0.55f, formatRate, "Hz"));
    params.push_back(makeFloatParameter("motionShape", "Motion Shape", { 0.0f, 100.0f, 0.1f }, 48.0f, formatPercent, "%"));
    params.push_back(makeToggleParameter("motionSpin", "Spin", true));
    params.push_back(makeFloatParameter("voiceCrossoverHz", "Voice Crossover", makeLogRange(180.0f, 6000.0f), 1600.0f, formatHz, "Hz"));
    params.push_back(makeFloatParameter("lowVoiceAmount", "Low Voice Amount", { 0.0f, 100.0f, 0.1f }, 58.0f, formatPercent, "%"));
    params.push_back(makeFloatParameter("lowVoiceScale", "Low Voice Depth", { 25.0f, 200.0f, 0.1f }, 118.0f, formatPercent, "%"));
    params.push_back(makeFloatParameter("lowVoiceRate", "Low Voice Rate", { 0.05f, 4.0f, 0.01f }, 0.32f, formatRate, "Hz"));
    params.push_back(makeFloatParameter("highVoiceAmount", "High Voice Amount", { 0.0f, 100.0f, 0.1f }, 72.0f, formatPercent, "%"));
    params.push_back(makeFloatParameter("highVoiceScale", "High Voice Depth", { 25.0f, 200.0f, 0.1f }, 92.0f, formatPercent, "%"));
    params.push_back(makeFloatParameter("highVoiceRate", "High Voice Rate", { 0.05f, 8.0f, 0.01f }, 0.88f, formatRate, "Hz"));
    params.push_back(makeFloatParameter("delayMs", "Delay", { 3.0f, 28.0f, 0.1f }, 11.5f, formatMilliseconds, "ms"));
    params.push_back(makeFloatParameter("feedback", "Feedback", { 0.0f, 100.0f, 0.1f }, 24.0f, formatPercent, "%"));
    params.push_back(makeFloatParameter("tone", "Tone", { 0.0f, 100.0f, 0.1f }, 54.0f, formatPercent, "%"));
    params.push_back(makeFloatParameter("stereoWidth", "Stereo Width", { 0.0f, 200.0f, 0.1f }, 132.0f, formatPercent, "%"));
    params.push_back(makeFloatParameter("drive", "Drive", { 0.0f, 100.0f, 0.1f }, 14.0f, formatPercent, "%"));
    params.push_back(makeFloatParameter("dryWet", "Dry/Wet", { 0.0f, 100.0f, 0.1f }, 38.0f, formatPercent, "%"));
    params.push_back(makeFloatParameter("shineAmount", "Shine Amount", { 0.0f, 100.0f, 0.1f }, 18.0f, formatPercent, "%"));
    params.push_back(makeFloatParameter("shineRate", "Shine Rate", { 0.05f, 8.0f, 0.01f }, 1.25f, formatRate, "Hz"));
    params.push_back(makeFloatParameter("spreadMs", "Spread", { 0.0f, 20.0f, 0.1f }, 5.5f, formatMilliseconds, "ms"));
    params.push_back(makeToggleParameter("vibrato", "Vibrato", false));
    params.push_back(makeToggleParameter("focusCut", "Focus Cut", false));

    return { params.begin(), params.end() };
}

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    for (const auto* id : presetParameterIDs)
        apvts.addParameterListener(id, this);

    initialisePresetTracking();
    refreshUserPresets();

    int defaultPresetIndex = 0;
    for (int i = 0; i < getFactoryPresets().size(); ++i)
    {
        if (getFactoryPresets().getReference(i).name == "Default Setting")
        {
            defaultPresetIndex = i;
            break;
        }
    }

    loadFactoryPreset(defaultPresetIndex);
}

PluginProcessor::~PluginProcessor()
{
    for (const auto* id : presetParameterIDs)
        apvts.removeParameterListener(id, this);
}

const juce::Array<PluginProcessor::PresetRecord>& PluginProcessor::getFactoryPresets()
{
    return sspchorus::presets::getFactoryPresetLibrary();
}

juce::File PluginProcessor::getUserPresetDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("SSP Chorus Presets");
}

juce::String PluginProcessor::getCurrentPresetName() const noexcept { return currentPresetName; }
juce::String PluginProcessor::getCurrentPresetCategory() const noexcept { return currentPresetCategory; }
juce::String PluginProcessor::getCurrentPresetAuthor() const noexcept { return currentPresetAuthor; }
juce::String PluginProcessor::getCurrentPresetKey() const noexcept { return currentPresetKey; }
bool PluginProcessor::isCurrentPresetFactory() const noexcept { return currentPresetIsFactory; }
bool PluginProcessor::isCurrentPresetDirty() const noexcept { return currentPresetDirty.load(); }

bool PluginProcessor::matchesCurrentPreset(const PresetRecord& preset) const noexcept
{
    return preset.id.isNotEmpty() && preset.id == currentPresetKey;
}

juce::StringArray PluginProcessor::getAvailablePresetCategories() const
{
    juce::StringArray categories;
    for (const auto& preset : getFactoryPresets())
        if (preset.category.isNotEmpty())
            categories.addIfNotAlreadyThere(preset.category);
    for (const auto& preset : userPresets)
        if (preset.category.isNotEmpty())
            categories.addIfNotAlreadyThere(preset.category);
    categories.sort(true);
    return categories;
}

void PluginProcessor::refreshUserPresets()
{
    userPresets.clearQuick();
    userPresetFiles.clearQuick();

    auto directory = getUserPresetDirectory();
    if (! directory.isDirectory())
        return;

    juce::Array<juce::File> files;
    directory.findChildFiles(files, juce::File::findFiles, false, "*" + juce::String(presetFileExtension));
    files.sort();

    for (const auto& file : files)
    {
        PresetRecord preset;
        if (! readPresetRecordFromJson(file.loadFileAsString(), preset))
            continue;

        preset.isFactory = false;
        preset.id = makePresetKey(false, preset.category, preset.name);
        userPresets.add(preset);
        userPresetFiles.add(file);
    }
}

bool PluginProcessor::loadFactoryPreset(int index)
{
    if (! juce::isPositiveAndBelow(index, getFactoryPresets().size()))
        return false;

    applyPresetRecord(getFactoryPresets().getReference(index), true);
    return true;
}

bool PluginProcessor::loadUserPreset(int index)
{
    refreshUserPresets();
    if (! juce::isPositiveAndBelow(index, userPresets.size()))
        return false;

    applyPresetRecord(userPresets.getReference(index), true);
    return true;
}

bool PluginProcessor::saveUserPreset(const juce::String& presetName, const juce::String& category)
{
    const auto cleanName = sanitisePresetName(presetName);
    const auto cleanCategory = sanitisePresetCategory(category);
    if (cleanName.isEmpty())
        return false;

    auto directory = getUserPresetDirectory();
    directory.createDirectory();

    auto preset = buildCurrentPresetRecord(cleanName,
                                           cleanCategory,
                                           currentPresetAuthor.isNotEmpty() ? currentPresetAuthor : "SSP User",
                                           false);
    preset.id = makePresetKey(false, preset.category, preset.name);

    const auto file = directory.getChildFile(makePresetFilenameStem(preset.category, preset.name) + presetFileExtension);
    if (! file.replaceWithText(serialisePresetRecordToJson(preset)))
        return false;

    refreshUserPresets();
    setCurrentPresetMetadata(preset.id, preset.name, preset.category, preset.author, false);
    lastLoadedPresetSnapshot = captureCurrentPresetSnapshot();
    currentPresetDirty.store(false);
    return true;
}

bool PluginProcessor::deleteUserPreset(int index)
{
    refreshUserPresets();
    if (! juce::isPositiveAndBelow(index, userPresetFiles.size()))
        return false;

    const auto preset = userPresets.getReference(index);
    if (! userPresetFiles.getReference(index).deleteFile())
        return false;

    refreshUserPresets();
    if (preset.id == currentPresetKey)
    {
        currentPresetKey.clear();
        currentPresetDirty.store(true);
        lastLoadedPresetSnapshot.valid = false;
    }

    return true;
}

bool PluginProcessor::importPresetFromFile(const juce::File& file, bool loadAfterImport)
{
    PresetRecord preset;
    if (! readPresetRecordFromJson(file.loadFileAsString(), preset))
        return false;

    preset.isFactory = false;
    preset.id = makePresetKey(false, preset.category, preset.name);

    auto directory = getUserPresetDirectory();
    directory.createDirectory();
    const auto destination = directory.getChildFile(makePresetFilenameStem(preset.category, preset.name) + presetFileExtension);
    if (! destination.replaceWithText(serialisePresetRecordToJson(preset)))
        return false;

    refreshUserPresets();

    if (loadAfterImport)
    {
        for (int i = 0; i < userPresets.size(); ++i)
            if (userPresets.getReference(i).id == preset.id)
                return loadUserPreset(i);
    }

    return true;
}

bool PluginProcessor::exportCurrentPresetToFile(const juce::File& file) const
{
    const auto presetName = currentPresetName.isNotEmpty() ? currentPresetName : "Current Settings";
    const auto preset = buildCurrentPresetRecord(presetName,
                                                 currentPresetCategory,
                                                 currentPresetAuthor.isNotEmpty() ? currentPresetAuthor : "SSP User",
                                                 currentPresetIsFactory);
    return file.replaceWithText(serialisePresetRecordToJson(preset));
}

bool PluginProcessor::resetUserPresetsToFactoryDefaults()
{
    refreshUserPresets();

    bool removedAny = false;
    for (const auto& file : userPresetFiles)
        removedAny = file.deleteFile() || removedAny;

    refreshUserPresets();
    if (! currentPresetIsFactory)
    {
        currentPresetKey.clear();
        currentPresetDirty.store(true);
        lastLoadedPresetSnapshot.valid = false;
    }

    return removedAny || userPresetFiles.isEmpty();
}

void PluginProcessor::initialisePresetTracking()
{
    currentPresetKey.clear();
    currentPresetName.clear();
    currentPresetCategory.clear();
    currentPresetAuthor.clear();
    currentPresetIsFactory = false;
    currentPresetDirty.store(false);
    lastLoadedPresetSnapshot = {};
    previewRestoreSnapshot = {};
    previewPresetKey.clear();
}

void PluginProcessor::setCurrentPresetMetadata(juce::String presetKey, juce::String presetName, juce::String category, juce::String author, bool isFactory)
{
    currentPresetKey = std::move(presetKey);
    currentPresetName = std::move(presetName);
    currentPresetCategory = std::move(category);
    currentPresetAuthor = std::move(author);
    currentPresetIsFactory = isFactory;
}

PluginProcessor::PresetStateSnapshot PluginProcessor::captureCurrentPresetSnapshot() const
{
    PresetStateSnapshot snapshot;
    snapshot.valid = true;
    for (size_t i = 0; i < presetParameterIDs.size(); ++i)
        snapshot.values[i] = apvts.getRawParameterValue(presetParameterIDs[i])->load();
    return snapshot;
}

PluginProcessor::PresetStateSnapshot PluginProcessor::makeDefaultPresetSnapshot() const
{
    PresetStateSnapshot snapshot;
    snapshot.valid = true;
    for (size_t i = 0; i < presetParameterIDs.size(); ++i)
        if (const auto* parameter = getRangedParameter(apvts, presetParameterIDs[i]))
            snapshot.values[i] = parameter->convertFrom0to1(parameter->getDefaultValue());
    return snapshot;
}

void PluginProcessor::applyPresetSnapshot(const PresetStateSnapshot& snapshot)
{
    if (! snapshot.valid)
        return;

    juce::ScopedValueSetter<bool> suppressor(suppressParameterListener, true);
    for (size_t i = 0; i < presetParameterIDs.size(); ++i)
    {
        if (auto* parameter = getRangedParameter(apvts, presetParameterIDs[i]))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(snapshot.values[i]));
            parameter->endChangeGesture();
        }
    }
}

void PluginProcessor::applyPresetRecord(const PresetRecord& preset, bool updateLoadedPresetReference)
{
    endPresetPreview();

    auto snapshot = makeDefaultPresetSnapshot();
    for (const auto& value : preset.values)
    {
        for (size_t i = 0; i < presetParameterIDs.size(); ++i)
        {
            if (value.id == presetParameterIDs[i])
            {
                snapshot.values[i] = value.value;
                break;
            }
        }
    }

    applyPresetSnapshot(snapshot);

    if (updateLoadedPresetReference)
    {
        setCurrentPresetMetadata(makePresetKey(preset.isFactory, preset.category, preset.name),
                                 preset.name,
                                 preset.category,
                                 preset.author,
                                 preset.isFactory);
        lastLoadedPresetSnapshot = snapshot;
        currentPresetDirty.store(false);
    }
}

PluginProcessor::PresetRecord PluginProcessor::buildCurrentPresetRecord(juce::String presetName, juce::String category, juce::String author, bool isFactory) const
{
    PresetRecord preset;
    preset.name = sanitisePresetName(presetName);
    preset.category = sanitisePresetCategory(category);
    preset.author = author.isNotEmpty() ? author : (isFactory ? "SSP Factory" : "SSP User");
    preset.isFactory = isFactory;
    preset.id = makePresetKey(isFactory, preset.category, preset.name);

    for (const auto* id : presetParameterIDs)
        preset.values.add(makeValue(id, apvts.getRawParameterValue(id)->load()));

    return preset;
}

void PluginProcessor::refreshDirtyFlagFromCurrentState()
{
    if (! lastLoadedPresetSnapshot.valid)
        return;

    const auto current = captureCurrentPresetSnapshot();
    bool dirty = false;
    for (size_t i = 0; i < current.values.size(); ++i)
    {
        if (std::abs(current.values[i] - lastLoadedPresetSnapshot.values[i]) > 0.0001f)
        {
            dirty = true;
            break;
        }
    }

    currentPresetDirty.store(dirty);
}

void PluginProcessor::parameterChanged(const juce::String&, float)
{
    if (suppressParameterListener)
        return;

    refreshDirtyFlagFromCurrentState();
}

bool PluginProcessor::beginPresetPreview(const PresetRecord& preset)
{
    if (preset.id.isEmpty())
        return false;

    if (previewPresetKey.isEmpty())
        previewRestoreSnapshot = captureCurrentPresetSnapshot();

    if (previewPresetKey == preset.id)
        return true;

    previewPresetKey = preset.id;

    auto snapshot = makeDefaultPresetSnapshot();
    for (const auto& value : preset.values)
    {
        for (size_t i = 0; i < presetParameterIDs.size(); ++i)
        {
            if (value.id == presetParameterIDs[i])
            {
                snapshot.values[i] = value.value;
                break;
            }
        }
    }

    applyPresetSnapshot(snapshot);
    return true;
}

void PluginProcessor::endPresetPreview()
{
    if (previewPresetKey.isEmpty())
        return;

    if (previewRestoreSnapshot.valid)
        applyPresetSnapshot(previewRestoreSnapshot);

    previewRestoreSnapshot = {};
    previewPresetKey.clear();
    refreshDirtyFlagFromCurrentState();
}

bool PluginProcessor::isPresetPreviewActive() const noexcept
{
    return previewPresetKey.isNotEmpty();
}

juce::Array<PluginProcessor::PresetRecord> PluginProcessor::getSequentialPresetList() const
{
    juce::Array<PresetRecord> ordered;
    for (const auto& preset : getFactoryPresets())
        ordered.add(preset);
    for (const auto& preset : userPresets)
        ordered.add(preset);
    return ordered;
}

bool PluginProcessor::loadPresetByKey(const juce::String& presetKey)
{
    if (presetKey.isEmpty())
        return false;

    for (int i = 0; i < getFactoryPresets().size(); ++i)
        if (getFactoryPresets().getReference(i).id == presetKey)
            return loadFactoryPreset(i);

    refreshUserPresets();
    for (int i = 0; i < userPresets.size(); ++i)
        if (userPresets.getReference(i).id == presetKey)
            return loadUserPreset(i);

    return false;
}

bool PluginProcessor::stepPreset(int delta)
{
    const auto ordered = getSequentialPresetList();
    if (ordered.isEmpty())
        return false;

    int currentIndex = -1;
    for (int i = 0; i < ordered.size(); ++i)
    {
        if (ordered.getReference(i).id == currentPresetKey)
        {
            currentIndex = i;
            break;
        }
    }

    if (currentIndex < 0)
        currentIndex = 0;

    const int nextIndex = (currentIndex + delta + ordered.size()) % ordered.size();
    return loadPresetByKey(ordered.getReference(nextIndex).id);
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec stereoSpec;
    stereoSpec.sampleRate = sampleRate;
    stereoSpec.maximumBlockSize = (juce::uint32) juce::jmax(1, samplesPerBlock);
    stereoSpec.numChannels = 2;

    juce::dsp::ProcessSpec monoSpec = stereoSpec;
    monoSpec.numChannels = 1;

    auto prepareFilterArray = [monoSpec](auto& filters, juce::dsp::StateVariableTPTFilterType type)
    {
        for (auto& filter : filters)
        {
            filter.reset();
            filter.prepare(monoSpec);
            filter.setType(type);
        }
    };

    prepareFilterArray(inputLowPassFilters, juce::dsp::StateVariableTPTFilterType::lowpass);
    prepareFilterArray(inputHighPassFilters, juce::dsp::StateVariableTPTFilterType::highpass);
    prepareFilterArray(outputLowPassFilters, juce::dsp::StateVariableTPTFilterType::lowpass);
    prepareFilterArray(outputHighPassFilters, juce::dsp::StateVariableTPTFilterType::highpass);
    prepareFilterArray(crossoverLowFilters, juce::dsp::StateVariableTPTFilterType::lowpass);
    prepareFilterArray(crossoverHighFilters, juce::dsp::StateVariableTPTFilterType::highpass);

    for (auto& line : baseDelayLines)
    {
        line.setMaximumDelayInSamples((int) std::ceil(maxDelayMs * 0.001 * sampleRate) + 2);
        line.prepare(monoSpec);
        line.reset();
    }

    for (auto& line : motionDelayLines)
    {
        line.setMaximumDelayInSamples((int) std::ceil(maxMotionDelayMs * 0.001 * sampleRate) + 2);
        line.prepare(monoSpec);
        line.reset();
    }

    lowVoiceChorus.prepare(stereoSpec);
    highVoiceChorus.prepare(stereoSpec);
    shineChorus.prepare(stereoSpec);
    lowVoiceChorus.reset();
    highVoiceChorus.reset();
    shineChorus.reset();

    mixSmoothed.reset(sampleRate, 0.05);
    mixSmoothed.setCurrentAndTargetValue(getValue("dryWet") / 100.0f);
    motionPhase = 0.0f;

    updateScratchBuffers(2, juce::jmax(1, samplesPerBlock));
}

void PluginProcessor::releaseResources()
{
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == mainOut;
}

void PluginProcessor::updateScratchBuffers(int numChannels, int numSamples)
{
    dryBuffer.setSize(numChannels, numSamples, false, false, true);
    filteredInputBuffer.setSize(numChannels, numSamples, false, false, true);
    wetBuffer.setSize(numChannels, numSamples, false, false, true);
    lowBandBuffer.setSize(numChannels, numSamples, false, false, true);
    highBandBuffer.setSize(numChannels, numSamples, false, false, true);
    motionBuffer.setSize(numChannels, numSamples, false, false, true);
}

void PluginProcessor::updateFilterCutoffs(float loCutHz, float hiCutHz, float crossoverHz, float tonePercent)
{
    const float toneNorm = juce::jlimit(0.0f, 1.0f, tonePercent / 100.0f);
    const float shapedLowCut = juce::jlimit(20.0f, 16000.0f, loCutHz);
    const float shapedHighCut = juce::jlimit(shapedLowCut + 40.0f, 20000.0f, hiCutHz);

    for (auto& filter : inputHighPassFilters)
        filter.setCutoffFrequency(shapedLowCut);

    for (auto& filter : inputLowPassFilters)
        filter.setCutoffFrequency(shapedHighCut);

    const float focusLowCut = juce::jlimit(20.0f, 18000.0f, shapedLowCut * (0.72f + toneNorm * 0.72f));
    const float focusHighCut = juce::jlimit(700.0f, 20000.0f, shapedHighCut * (0.52f + toneNorm * 0.54f));

    for (auto& filter : outputHighPassFilters)
        filter.setCutoffFrequency(focusLowCut);

    for (auto& filter : outputLowPassFilters)
        filter.setCutoffFrequency(focusHighCut);

    for (auto& filter : crossoverLowFilters)
        filter.setCutoffFrequency(crossoverHz);

    for (auto& filter : crossoverHighFilters)
        filter.setCutoffFrequency(crossoverHz);
}

void PluginProcessor::applyInputFilters(juce::AudioBuffer<float>& buffer)
{
    const auto channels = juce::jmin(2, buffer.getNumChannels());
    const auto samples = buffer.getNumSamples();

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer(channel);

        for (int sample = 0; sample < samples; ++sample)
        {
            auto filtered = inputHighPassFilters[(size_t) channel].processSample(0, data[sample]);
            filtered = inputLowPassFilters[(size_t) channel].processSample(0, filtered);
            data[sample] = filtered;
        }
    }
}

void PluginProcessor::applyFocusCutFilters(juce::AudioBuffer<float>& buffer)
{
    const auto channels = juce::jmin(2, buffer.getNumChannels());
    const auto samples = buffer.getNumSamples();

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer(channel);

        for (int sample = 0; sample < samples; ++sample)
        {
            auto filtered = outputHighPassFilters[(size_t) channel].processSample(0, data[sample]);
            filtered = outputLowPassFilters[(size_t) channel].processSample(0, filtered);
            data[sample] = filtered;
        }
    }
}

void PluginProcessor::buildBaseDelayedInput(const juce::AudioBuffer<float>& source,
                                            juce::AudioBuffer<float>& destination,
                                            float delaySamples)
{
    destination.clear();

    const auto channels = juce::jmin(2, source.getNumChannels());
    const auto samples = source.getNumSamples();

    for (int channel = 0; channel < channels; ++channel)
    {
        const auto* input = source.getReadPointer(channel);
        auto* output = destination.getWritePointer(channel);

        for (int sample = 0; sample < samples; ++sample)
        {
            baseDelayLines[(size_t) channel].pushSample(0, input[sample]);
            output[sample] = baseDelayLines[(size_t) channel].popSample(0, delaySamples);
        }
    }
}

void PluginProcessor::buildMotionLayer(const juce::AudioBuffer<float>& source,
                                       juce::AudioBuffer<float>& destination,
                                       float amount,
                                       float spreadMs,
                                       float shape,
                                       float rate,
                                       bool spinEnabled)
{
    destination.clear();

    const auto channels = juce::jmin(2, source.getNumChannels());
    const auto samples = source.getNumSamples();
    const float amountNorm = juce::jlimit(0.0f, 1.0f, amount);
    const float shapeNorm = juce::jlimit(0.0f, 1.0f, shape);
    const float spread = juce::jlimit(0.0f, 20.0f, spreadMs);
    const float phaseStep = juce::MathConstants<float>::twoPi * juce::jlimit(0.01f, 8.0f, rate) / (float) currentSampleRate;
    const std::array<float, 3> baseDelaysMs {
        4.8f + spread * 0.20f,
        8.4f + spread * 0.46f,
        13.5f + spread * 0.76f
    };
    const std::array<float, 3> tapGains {
        0.42f,
        0.33f,
        0.21f
    };

    for (int sample = 0; sample < samples; ++sample)
    {
        const float sineMod = std::sin(motionPhase);
        const float triangleMod = triangleFromPhase(motionPhase);
        const float blendedMod = sineMod + (triangleMod - sineMod) * shapeNorm;
        const float orthoMod = std::sin(motionPhase + juce::MathConstants<float>::halfPi);

        for (int channel = 0; channel < channels; ++channel)
        {
            const auto* input = source.getReadPointer(channel);
            auto* output = destination.getWritePointer(channel);
            const float channelOffset = spinEnabled ? ((channel == 0 ? -1.0f : 1.0f) * orthoMod) : 0.0f;
            motionDelayLines[(size_t) channel].pushSample(0, input[sample]);

            float motionSample = 0.0f;

            for (size_t tap = 0; tap < baseDelaysMs.size(); ++tap)
            {
                const float modDepth = 0.8f + shapeNorm * 1.5f + 0.25f * (float) tap;
                const float delayMs = juce::jlimit(1.0f,
                                                   maxMotionDelayMs,
                                                   baseDelaysMs[tap] + blendedMod * modDepth + channelOffset * (0.55f + 0.12f * (float) tap));
                const float delaySamples = delayMs * 0.001f * (float) currentSampleRate;
                motionSample += tapGains[tap] * motionDelayLines[(size_t) channel].popSample(0, delaySamples, false);
            }

            output[sample] = motionSample * amountNorm;
        }

        motionPhase += phaseStep;
        if (motionPhase > juce::MathConstants<float>::twoPi)
            motionPhase -= juce::MathConstants<float>::twoPi;
    }
}

void PluginProcessor::splitIntoVoiceBands(const juce::AudioBuffer<float>& source,
                                          juce::AudioBuffer<float>& lowBand,
                                          juce::AudioBuffer<float>& highBand)
{
    lowBand.makeCopyOf(source, true);
    highBand.makeCopyOf(source, true);

    const auto channels = juce::jmin(2, source.getNumChannels());
    const auto samples = source.getNumSamples();

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* lowData = lowBand.getWritePointer(channel);
        auto* highData = highBand.getWritePointer(channel);

        for (int sample = 0; sample < samples; ++sample)
        {
            lowData[sample] = crossoverLowFilters[(size_t) channel].processSample(0, lowData[sample]);
            highData[sample] = crossoverHighFilters[(size_t) channel].processSample(0, highData[sample]);
        }
    }
}

void PluginProcessor::processChorus(juce::AudioBuffer<float>& buffer, juce::dsp::Chorus<float>& chorus)
{
    juce::dsp::AudioBlock<float> block(buffer);
    chorus.process(juce::dsp::ProcessContextReplacing<float>(block));
}

void PluginProcessor::applyDrive(juce::AudioBuffer<float>& buffer, float driveAmount)
{
    const float drive = juce::jlimit(0.0f, 1.0f, driveAmount);
    if (drive <= 0.0001f)
        return;

    const float pregain = 1.0f + drive * 4.5f;
    const float normalise = 1.0f / std::tanh(pregain);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* data = buffer.getWritePointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            data[sample] = std::tanh(data[sample] * pregain) * normalise;
    }
}

void PluginProcessor::applyWidth(juce::AudioBuffer<float>& buffer, float widthScale)
{
    if (buffer.getNumChannels() < 2)
        return;

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);
    const auto samples = buffer.getNumSamples();

    for (int sample = 0; sample < samples; ++sample)
    {
        const float mid = 0.5f * (left[sample] + right[sample]);
        const float side = 0.5f * (left[sample] - right[sample]) * widthScale;
        left[sample] = mid + side;
        right[sample] = mid - side;
    }
}

float PluginProcessor::getValue(const juce::String& parameterID) const
{
    if (auto* value = apvts.getRawParameterValue(parameterID))
        return value->load();

    jassertfalse;
    return 0.0f;
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(2, buffer.getNumChannels());

    updateScratchBuffers(numChannels, numSamples);
    dryBuffer.makeCopyOf(buffer, true);

    const float loCutHz = getValue("loCutHz");
    const float hiCutHz = getValue("hiCutHz");
    const float motionAmount = getValue("motionAmount") / 100.0f;
    const float motionRate = getValue("motionRate");
    const float motionShape = getValue("motionShape") / 100.0f;
    const bool motionSpin = getValue("motionSpin") > 0.5f;
    const float voiceCrossoverHz = getValue("voiceCrossoverHz");
    const float lowVoiceAmount = getValue("lowVoiceAmount") / 100.0f;
    const float lowVoiceScale = getValue("lowVoiceScale") / 100.0f;
    const float lowVoiceRate = getValue("lowVoiceRate");
    const float highVoiceAmount = getValue("highVoiceAmount") / 100.0f;
    const float highVoiceScale = getValue("highVoiceScale") / 100.0f;
    const float highVoiceRate = getValue("highVoiceRate");
    const float delayMs = getValue("delayMs");
    const float feedbackNorm = getValue("feedback") / 100.0f;
    const float tone = getValue("tone");
    const float toneNorm = tone / 100.0f;
    const float stereoWidth = getValue("stereoWidth") / 100.0f;
    const float drive = getValue("drive") / 100.0f;
    const float dryWet = getValue("dryWet") / 100.0f;
    const float shineAmount = getValue("shineAmount") / 100.0f;
    const float shineRate = getValue("shineRate");
    const float spreadMs = getValue("spreadMs");
    const bool vibrato = getValue("vibrato") > 0.5f;
    const bool focusCut = getValue("focusCut") > 0.5f;

    mixSmoothed.setTargetValue(dryWet);
    if (dryWet <= 0.0001f)
    {
        mixSmoothed.setCurrentAndTargetValue(dryWet);
        return;
    }

    updateFilterCutoffs(loCutHz, hiCutHz, voiceCrossoverHz, tone);

    filteredInputBuffer.makeCopyOf(buffer, true);
    applyInputFilters(filteredInputBuffer);

    buildBaseDelayedInput(filteredInputBuffer,
                          wetBuffer,
                          delayMs * 0.001f * (float) currentSampleRate);

    splitIntoVoiceBands(wetBuffer, lowBandBuffer, highBandBuffer);

    lowVoiceChorus.setRate(juce::jlimit(0.05f, 6.0f, lowVoiceRate * (0.80f + motionAmount * 0.70f)));
    lowVoiceChorus.setDepth(juce::jlimit(0.05f, 1.0f, 0.08f + lowVoiceScale * 0.36f + motionAmount * 0.22f));
    lowVoiceChorus.setCentreDelay(juce::jlimit(1.0f, 100.0f, delayMs + 1.5f + lowVoiceScale * 7.0f + spreadMs * 0.18f));
    lowVoiceChorus.setFeedback(juce::jlimit(-1.0f, 1.0f, -0.12f + feedbackNorm * 0.42f));
    lowVoiceChorus.setMix(lowVoiceAmount);

    highVoiceChorus.setRate(juce::jlimit(0.05f, 9.0f, highVoiceRate * (0.92f + motionShape * 0.38f)));
    highVoiceChorus.setDepth(juce::jlimit(0.05f, 1.0f, 0.06f + highVoiceScale * 0.40f + motionAmount * 0.18f));
    highVoiceChorus.setCentreDelay(juce::jlimit(1.0f, 100.0f, juce::jmax(2.0f, delayMs - 1.25f) + highVoiceScale * 5.0f + spreadMs * 0.30f));
    highVoiceChorus.setFeedback(juce::jlimit(-1.0f, 1.0f, -0.04f + feedbackNorm * 0.34f));
    highVoiceChorus.setMix(highVoiceAmount);

    processChorus(lowBandBuffer, lowVoiceChorus);
    processChorus(highBandBuffer, highVoiceChorus);

    wetBuffer.makeCopyOf(lowBandBuffer, true);
    wetBuffer.addFrom(0, 0, highBandBuffer, 0, 0, numSamples);
    if (numChannels > 1)
        wetBuffer.addFrom(1, 0, highBandBuffer, 1, 0, numSamples);

    buildMotionLayer(filteredInputBuffer, motionBuffer, motionAmount, spreadMs, motionShape, motionRate, motionSpin);

    wetBuffer.addFrom(0, 0, motionBuffer, 0, 0, numSamples, 0.42f + motionAmount * 0.32f);
    if (numChannels > 1)
        wetBuffer.addFrom(1, 0, motionBuffer, 1, 0, numSamples, 0.42f + motionAmount * 0.32f);

    shineChorus.setRate(juce::jlimit(0.05f, 10.0f, vibrato ? shineRate * 1.35f : shineRate));
    shineChorus.setDepth(juce::jlimit(0.02f, 1.0f, 0.05f + shineAmount * 0.82f + (vibrato ? 0.12f : 0.0f)));
    shineChorus.setCentreDelay(juce::jlimit(1.0f, 100.0f, 3.5f + spreadMs * 0.40f + shineAmount * 14.0f));
    shineChorus.setFeedback(juce::jlimit(-1.0f, 1.0f, -0.06f + feedbackNorm * 0.30f));
    shineChorus.setMix(vibrato ? juce::jlimit(0.55f, 1.0f, 0.68f + shineAmount * 0.25f)
                               : shineAmount * 0.48f);
    processChorus(wetBuffer, shineChorus);

    if (focusCut)
        applyFocusCutFilters(wetBuffer);

    applyDrive(wetBuffer, drive);

    const float spectralTrim = juce::jlimit(0.55f, 1.18f, 0.84f + toneNorm * 0.24f - feedbackNorm * 0.08f + shineAmount * 0.06f);
    wetBuffer.applyGain(spectralTrim);
    applyWidth(wetBuffer, juce::jlimit(0.0f, 2.0f, stereoWidth));

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float mix = juce::jlimit(0.0f, 1.0f, mixSmoothed.getNextValue());

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* out = buffer.getWritePointer(channel);
            const auto* dry = dryBuffer.getReadPointer(channel);
            const auto* wet = wetBuffer.getReadPointer(channel);
            out[sample] = dry[sample] * (1.0f - mix) + wet[sample] * mix;
        }
    }

    for (int channel = numChannels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, numSamples);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::XmlElement wrapper(presetStateWrapperTag);
    if (auto state = apvts.copyState().createXml())
        wrapper.addChildElement(state.release());

    auto* meta = wrapper.createNewChildElement(presetMetaTag);
    meta->setAttribute("currentPresetKey", currentPresetKey);
    meta->setAttribute("currentPresetName", currentPresetName);
    meta->setAttribute("currentPresetCategory", currentPresetCategory);
    meta->setAttribute("currentPresetAuthor", currentPresetAuthor);
    meta->setAttribute("currentPresetIsFactory", currentPresetIsFactory);
    meta->setAttribute("currentPresetDirty", currentPresetDirty.load());

    copyXmlToBinary(wrapper, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xmlState = getXmlFromBinary(data, sizeInBytes);
    if (xmlState == nullptr)
        return;

    const juce::XmlElement* valueTreeXml = nullptr;
    const juce::XmlElement* metaXml = nullptr;

    if (xmlState->hasTagName(presetStateWrapperTag))
    {
        valueTreeXml = xmlState->getChildByName(apvts.state.getType());
        metaXml = xmlState->getChildByName(presetMetaTag);
    }
    else if (xmlState->hasTagName(apvts.state.getType()))
    {
        valueTreeXml = xmlState.get();
    }

    if (valueTreeXml != nullptr)
        apvts.replaceState(juce::ValueTree::fromXml(*valueTreeXml));

    if (metaXml != nullptr)
    {
        currentPresetKey = metaXml->getStringAttribute("currentPresetKey");
        currentPresetName = metaXml->getStringAttribute("currentPresetName");
        currentPresetCategory = metaXml->getStringAttribute("currentPresetCategory");
        currentPresetAuthor = metaXml->getStringAttribute("currentPresetAuthor");
        currentPresetIsFactory = metaXml->getBoolAttribute("currentPresetIsFactory", false);
        currentPresetDirty.store(metaXml->getBoolAttribute("currentPresetDirty", false));

        if (currentPresetKey.isNotEmpty() && ! currentPresetDirty.load())
            lastLoadedPresetSnapshot = captureCurrentPresetSnapshot();
        else
            lastLoadedPresetSnapshot.valid = false;
    }
    else
    {
        currentPresetKey.clear();
        currentPresetName.clear();
        currentPresetCategory.clear();
        currentPresetAuthor.clear();
        currentPresetIsFactory = false;
        currentPresetDirty.store(false);
        lastLoadedPresetSnapshot.valid = false;
    }

    previewRestoreSnapshot = {};
    previewPresetKey.clear();
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
