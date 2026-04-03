#include "PluginProcessor.h"
#include "DelayPresetLibrary.h"
#include "PluginEditor.h"

namespace
{
constexpr float meterAttack = 0.34f;
constexpr float meterRelease = 0.10f;
constexpr float maxDelaySeconds = 4.5f;
constexpr auto presetStateWrapperTag = "SSPDELAY_STATE";
constexpr auto presetMetaTag = "PRESET_META";
constexpr auto presetFileExtension = ".sspdelaypreset";

const std::array<const char*, 10> presetParameterIDs{{
    "timeMode", "timeMs", "feedback", "mix", "tone", "width",
    "unlinkChannels", "rightTimeMode", "rightTimeMs", "rightFeedback"
}};

const std::array<float, 8> noteDivisions{{
    0.125f, 0.25f, 0.75f, 0.5f, 1.5f, 1.0f, 2.0f, 4.0f
}};

float getLowpassAlpha(double sampleRate, float cutoffHz)
{
    return 1.0f - std::exp(-juce::MathConstants<float>::twoPi * cutoffHz / (float) sampleRate);
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

const juce::StringArray& PluginProcessor::getTimeModeNames() noexcept
{
    static const juce::StringArray names{ "ms", "1/32", "1/16", "1/8D", "1/8", "1/4D", "1/4", "1/2", "1/1" };
    return names;
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>("timeMode",
                                                                  "Timing",
                                                                  getTimeModeNames(),
                                                                  0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("timeMs",
                                                                 "Time",
                                                                 juce::NormalisableRange<float>(20.0f, 2000.0f, 1.0f),
                                                                 380.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("feedback",
                                                                 "Feedback",
                                                                 juce::NormalisableRange<float>(0.0f, 0.95f, 0.001f),
                                                                 0.40f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix",
                                                                 "Mix",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.26f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("tone",
                                                                 "Tone",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.55f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("width",
                                                                 "Width",
                                                                 juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                                 0.32f));

    params.push_back(std::make_unique<juce::AudioParameterBool>("unlinkChannels",
                                                                "Unlink Channels",
                                                                false));

    params.push_back(std::make_unique<juce::AudioParameterChoice>("rightTimeMode",
                                                                  "Right Timing",
                                                                  getTimeModeNames(),
                                                                  0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("rightTimeMs",
                                                                 "Right Time",
                                                                 juce::NormalisableRange<float>(20.0f, 2000.0f, 1.0f),
                                                                 380.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("rightFeedback",
                                                                 "Right Feedback",
                                                                 juce::NormalisableRange<float>(0.0f, 0.95f, 0.001f),
                                                                 0.40f));

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

float PluginProcessor::getInputMeterLevel() const noexcept
{
    return inputMeter.load();
}

float PluginProcessor::getDelayMeterLevel() const noexcept
{
    return delayMeter.load();
}

float PluginProcessor::getOutputMeterLevel() const noexcept
{
    return outputMeter.load();
}

const juce::Array<PluginProcessor::PresetRecord>& PluginProcessor::getFactoryPresets()
{
    return sspdelay::presets::getFactoryPresetLibrary();
}

juce::File PluginProcessor::getUserPresetDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("SSP Delay Presets");
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

    auto preset = buildCurrentPresetRecord(cleanName, cleanCategory,
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
    normalisePresetSnapshot(snapshot);
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

void PluginProcessor::normalisePresetSnapshot(PresetStateSnapshot& snapshot) const
{
    if (! snapshot.valid)
        return;

    const bool unlinkChannels = snapshot.values[6] >= 0.5f;
    if (! unlinkChannels)
    {
        snapshot.values[7] = snapshot.values[0];
        snapshot.values[8] = snapshot.values[1];
        snapshot.values[9] = snapshot.values[2];
    }
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

    normalisePresetSnapshot(snapshot);

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

    const auto snapshot = captureCurrentPresetSnapshot();
    for (size_t i = 0; i < presetParameterIDs.size(); ++i)
        preset.values.add(makeValue(presetParameterIDs[i], snapshot.values[i]));

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

    normalisePresetSnapshot(snapshot);

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

float PluginProcessor::readDelaySample(const std::vector<float>& buffer, float delayInSamples) const noexcept
{
    if (buffer.empty() || delayBufferSize <= 1)
        return 0.0f;

    float readPosition = (float) writePosition - delayInSamples;
    while (readPosition < 0.0f)
        readPosition += (float) delayBufferSize;

    const int indexA = juce::jlimit(0, delayBufferSize - 1, (int) readPosition);
    const int indexB = (indexA + 1) % delayBufferSize;
    const float fraction = readPosition - (float) indexA;

    return buffer[(size_t) indexA] + (buffer[(size_t) indexB] - buffer[(size_t) indexA]) * fraction;
}

float PluginProcessor::getHostBpm() const noexcept
{
    if (auto* currentPlayHead = getPlayHead(); currentPlayHead != nullptr)
        if (const auto position = currentPlayHead->getPosition())
            if (const auto bpm = position->getBpm(); bpm.hasValue() && *bpm > 1.0)
                return (float) *bpm;

    return 120.0f;
}

float PluginProcessor::getDelayTimeMsFor(bool rightChannel) const noexcept
{
    const auto* modeParameter = apvts.getRawParameterValue(rightChannel ? "rightTimeMode" : "timeMode");
    const auto* timeParameter = apvts.getRawParameterValue(rightChannel ? "rightTimeMs" : "timeMs");
    const int mode = juce::roundToInt(modeParameter->load());
    if (mode <= 0)
        return timeParameter->load();

    const int noteIndex = juce::jlimit(0, (int) noteDivisions.size() - 1, mode - 1);
    const float quarterNoteMs = 60000.0f / getHostBpm();
    return quarterNoteMs * noteDivisions[(size_t) noteIndex];
}

void PluginProcessor::updateMeters(float inputPeak, float delayPeak, float outputPeak) noexcept
{
    const auto smoothValue = [](std::atomic<float>& meter, float target)
    {
        const float current = meter.load();
        const float coefficient = target > current ? meterAttack : meterRelease;
        meter.store(current + (target - current) * coefficient);
    };

    smoothValue(inputMeter, juce::jlimit(0.0f, 1.2f, inputPeak));
    smoothValue(delayMeter, juce::jlimit(0.0f, 1.2f, delayPeak));
    smoothValue(outputMeter, juce::jlimit(0.0f, 1.2f, outputPeak));
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    currentSampleRate = sampleRate;
    delayBufferSize = (int) std::ceil(maxDelaySeconds * sampleRate) + 4;

    for (auto& buffer : delayBuffers)
        buffer.assign((size_t) delayBufferSize, 0.0f);

    toneStates = { 0.0f, 0.0f };
    writePosition = 0;

    for (auto& smoother : timeSmoothed)
        smoother.reset(sampleRate, 0.04);
    for (auto& smoother : feedbackSmoothed)
        smoother.reset(sampleRate, 0.06);
    mixSmoothed.reset(sampleRate, 0.04);
    toneSmoothed.reset(sampleRate, 0.05);
    widthSmoothed.reset(sampleRate, 0.05);

    const float leftTimeMs = getDelayTimeMsFor(false);
    const bool unlinkChannels = apvts.getRawParameterValue("unlinkChannels")->load() >= 0.5f;
    const float rightTimeMs = unlinkChannels ? getDelayTimeMsFor(true) : leftTimeMs;
    const float leftFeedback = apvts.getRawParameterValue("feedback")->load();
    const float rightFeedback = unlinkChannels ? apvts.getRawParameterValue("rightFeedback")->load() : leftFeedback;

    timeSmoothed[0].setCurrentAndTargetValue(leftTimeMs);
    timeSmoothed[1].setCurrentAndTargetValue(rightTimeMs);
    feedbackSmoothed[0].setCurrentAndTargetValue(leftFeedback);
    feedbackSmoothed[1].setCurrentAndTargetValue(rightFeedback);
    mixSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("mix")->load());
    toneSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("tone")->load());
    widthSmoothed.setCurrentAndTargetValue(apvts.getRawParameterValue("width")->load());

    inputMeter.store(0.0f);
    delayMeter.store(0.0f);
    outputMeter.store(0.0f);
}

void PluginProcessor::releaseResources() {}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == mainOut;
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(2, buffer.getNumChannels());

    if (numChannels == 0 || numSamples == 0 || delayBufferSize <= 0)
        return;

    const float leftTimeMs = getDelayTimeMsFor(false);
    const bool unlinkChannels = apvts.getRawParameterValue("unlinkChannels")->load() >= 0.5f;
    const float rightTimeMs = unlinkChannels ? getDelayTimeMsFor(true) : leftTimeMs;
    const float leftFeedback = apvts.getRawParameterValue("feedback")->load();
    const float rightFeedback = unlinkChannels ? apvts.getRawParameterValue("rightFeedback")->load() : leftFeedback;

    timeSmoothed[0].setTargetValue(leftTimeMs);
    timeSmoothed[1].setTargetValue(rightTimeMs);
    feedbackSmoothed[0].setTargetValue(leftFeedback);
    feedbackSmoothed[1].setTargetValue(rightFeedback);
    mixSmoothed.setTargetValue(apvts.getRawParameterValue("mix")->load());
    toneSmoothed.setTargetValue(apvts.getRawParameterValue("tone")->load());
    widthSmoothed.setTargetValue(apvts.getRawParameterValue("width")->load());

    float inputPeak = 0.0f;
    float delayPeak = 0.0f;
    float outputPeak = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float leftTimeThisSample = timeSmoothed[0].getNextValue();
        const float rightTimeThisSample = timeSmoothed[1].getNextValue();
        const float leftFeedbackThisSample = feedbackSmoothed[0].getNextValue();
        const float rightFeedbackThisSample = feedbackSmoothed[1].getNextValue();
        const float mix = mixSmoothed.getNextValue();
        const float tone = toneSmoothed.getNextValue();
        const float width = widthSmoothed.getNextValue();

        const float leftBaseDelaySamples = juce::jlimit(8.0f,
                                                        (float) delayBufferSize - 2.0f,
                                                        leftTimeThisSample * (float) currentSampleRate * 0.001f);
        const float rightBaseDelaySamples = juce::jlimit(8.0f,
                                                         (float) delayBufferSize - 2.0f,
                                                         rightTimeThisSample * (float) currentSampleRate * 0.001f);

        const float stereoOffset = unlinkChannels ? 0.0f : width * 0.32f;
        const float leftDelay = juce::jlimit(8.0f,
                                             (float) delayBufferSize - 2.0f,
                                             leftBaseDelaySamples * (1.0f - stereoOffset));
        const float rightDelay = juce::jlimit(8.0f,
                                              (float) delayBufferSize - 2.0f,
                                              rightBaseDelaySamples * (1.0f + stereoOffset));
        const float crossfeed = width * 0.22f;
        const float toneCutoff = juce::jmap(tone, 850.0f, 14000.0f);
        const float toneAlpha = getLowpassAlpha(currentSampleRate, toneCutoff);

        const float leftTap = readDelaySample(delayBuffers[0], leftDelay);
        const float rightTap = readDelaySample(delayBuffers[1], rightDelay);

        const float wetLeftRaw = leftTap * (1.0f - crossfeed) + rightTap * crossfeed;
        const float wetRightRaw = rightTap * (1.0f - crossfeed) + leftTap * crossfeed;
        const std::array<float, 2> wetRaw{{ wetLeftRaw, wetRightRaw }};

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float dry = buffer.getSample(channel, sample);
            inputPeak = juce::jmax(inputPeak, std::abs(dry));

            toneStates[(size_t) channel] += (wetRaw[(size_t) channel] - toneStates[(size_t) channel]) * toneAlpha;
            const float wet = toneStates[(size_t) channel];
            const float out = dry * (1.0f - mix) + wet * mix;
            const float feedback = channel == 0 ? leftFeedbackThisSample : rightFeedbackThisSample;
            const float writeInput = dry + wet * feedback;

            delayBuffers[(size_t) channel][(size_t) writePosition] = std::tanh(writeInput);
            buffer.setSample(channel, sample, out);

            delayPeak = juce::jmax(delayPeak, std::abs(wet));
            outputPeak = juce::jmax(outputPeak, std::abs(out));
        }

        writePosition = (writePosition + 1) % delayBufferSize;
    }

    for (int channel = numChannels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear(channel, 0, numSamples);

    updateMeters(inputPeak, delayPeak, outputPeak);
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
