#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SynthVoice.h"

namespace
{
class BasicSound final : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

const juce::StringArray waveformChoices { "Sine", "Saw", "Square", "Triangle", "Noise" };
const juce::StringArray subWaveformChoices { "Sine", "Square", "Saw", "Triangle" };
const juce::StringArray filterChoices { "Low Pass", "Band Pass", "High Pass" };
const juce::StringArray lfoShapeChoices { "Sine", "Triangle", "Square", "Saw", "S&H" };
const juce::StringArray pwmSourceChoices { "Off", "LFO 1", "LFO 2", "LFO 3", "Env 2", "Mod Wheel" };
const juce::StringArray syncSourceChoices { "Off", "Osc 1", "Osc 2", "Osc 3" };
const juce::StringArray arpModeChoices { "Up", "Down", "Up/Down", "Random" };
const juce::StringArray gatePatternChoices { "Straight", "Trance", "Syncopated", "Stutter" };
const juce::StringArray keyModeChoices { "Poly", "Mono" };
const juce::StringArray portamentoModeChoices { "Off", "Retrig", "Legato" };
const juce::StringArray timingDivisionChoices { "1/32", "1/16", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/2", "1 Bar" };
const juce::StringArray matrixSourceChoices { "Off", "LFO 1", "LFO 2", "LFO 3", "Env 1", "Env 2", "Mod Wheel", "Velocity", "Pitch Bend" };
const juce::StringArray matrixDestinationChoices
{
    "Off", "Pitch", "Cutoff", "Resonance", "Amp",
    "Osc 1 PWM", "Osc 2 PWM", "Osc 3 PWM", "Sub PWM", "Detune", "Drive"
};

const std::array<std::array<float, 16>, 4> gatePatterns
{{
    { 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f },
    { 1.0f, 0.0f, 0.7f, 0.0f, 1.0f, 0.3f, 0.8f, 0.0f, 1.0f, 0.0f, 0.7f, 0.0f, 1.0f, 0.35f, 0.8f, 0.0f },
    { 1.0f, 0.2f, 0.0f, 0.85f, 1.0f, 0.0f, 0.65f, 0.0f, 1.0f, 0.2f, 0.0f, 0.85f, 0.9f, 0.0f, 0.55f, 0.0f },
    { 1.0f, 1.0f, 0.0f, 0.35f, 1.0f, 0.0f, 1.0f, 0.0f, 0.85f, 0.0f, 0.55f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f }
}};

using ParameterValues = std::vector<std::pair<juce::String, float>>;

struct StepPatternTemplate
{
    juce::String name;
    bool enabled { true };
    bool sync { true };
    float bpm { 124.0f };
    int stepCount { 8 };
    int divisionIndex { 3 };
    std::array<int, 8> notes {};
    std::array<float, 8> gates {};
    std::array<float, 8> velocities {};
};

void setOrAddValue(ParameterValues& values, const juce::String& id, float value)
{
    for (auto& [existingId, existingValue] : values)
    {
        if (existingId == id)
        {
            existingValue = value;
            return;
        }
    }

    values.push_back({ id, value });
}

void applyStepPatternValues(ParameterValues& values, const StepPatternTemplate& pattern)
{
    setOrAddValue(values, "stepSeqEnabled", pattern.enabled ? 1.0f : 0.0f);
    setOrAddValue(values, "stepSeqSync", pattern.sync ? 1.0f : 0.0f);
    setOrAddValue(values, "stepSeqBpm", pattern.bpm);
    setOrAddValue(values, "stepSeqSteps", static_cast<float>(pattern.stepCount));
    setOrAddValue(values, "stepSeqLength", static_cast<float>(pattern.divisionIndex));

    for (int step = 0; step < 8; ++step)
    {
        const auto stepId = juce::String(step + 1);
        setOrAddValue(values, "seqStep" + stepId + "Note", static_cast<float>(pattern.notes[static_cast<size_t>(step)]));
        setOrAddValue(values, "seqStep" + stepId + "Gate", pattern.gates[static_cast<size_t>(step)]);
        setOrAddValue(values, "seqStep" + stepId + "Velocity", pattern.velocities[static_cast<size_t>(step)]);
    }
}

std::vector<StepPatternTemplate> createStepPatternTemplates()
{
    return
    {
        { "Init Pattern", false, true, 124.0f, 8, 3,
          { 0, 0, 0, 0, 0, 0, 0, 0 },
          { 0.85f, 0.85f, 0.85f, 0.85f, 0.85f, 0.85f, 0.85f, 0.85f },
          { 0.92f, 0.92f, 0.92f, 0.92f, 0.92f, 0.92f, 0.92f, 0.92f } },
        { "Bass / Acid Walk", true, true, 126.0f, 8, 3,
          { 0, 7, 10, 7, 12, 7, 10, 5 },
          { 0.92f, 0.55f, 0.84f, 0.48f, 0.94f, 0.62f, 0.78f, 0.5f },
          { 1.0f, 0.76f, 0.9f, 0.72f, 1.0f, 0.78f, 0.88f, 0.74f } },
        { "Bass / Octave Bite", true, true, 124.0f, 8, 4,
          { 0, 12, 7, 12, 0, 12, 3, 10 },
          { 0.95f, 0.62f, 0.8f, 0.56f, 0.95f, 0.62f, 0.72f, 0.58f },
          { 1.0f, 0.8f, 0.86f, 0.78f, 1.0f, 0.84f, 0.8f, 0.76f } },
        { "Lead / Triplet Lift", true, true, 128.0f, 8, 2,
          { 0, 3, 7, 10, 7, 12, 10, 15 },
          { 0.74f, 0.58f, 0.72f, 0.58f, 0.7f, 0.54f, 0.7f, 0.5f },
          { 0.9f, 0.74f, 0.86f, 0.7f, 0.82f, 0.72f, 0.88f, 0.78f } },
        { "Lead / Pulse Grid", true, false, 136.0f, 8, 1,
          { 0, 7, 12, 7, 10, 7, 12, 15 },
          { 0.64f, 0.64f, 0.52f, 0.64f, 0.58f, 0.64f, 0.52f, 0.74f },
          { 0.84f, 0.84f, 0.7f, 0.84f, 0.8f, 0.84f, 0.72f, 0.92f } },
        { "Motion / Spiral Engine", true, true, 122.0f, 8, 3,
          { 0, 7, 3, 10, 5, 12, 7, 14 },
          { 0.84f, 0.42f, 0.7f, 0.38f, 0.82f, 0.46f, 0.7f, 0.32f },
          { 0.96f, 0.62f, 0.86f, 0.58f, 1.0f, 0.66f, 0.88f, 0.62f } },
        { "Motion / Offbeat Runner", true, true, 124.0f, 8, 4,
          { 0, 0, 7, 10, 0, 12, 7, 3 },
          { 0.9f, 0.0f, 0.6f, 0.55f, 0.84f, 0.0f, 0.72f, 0.48f },
          { 1.0f, 0.0f, 0.78f, 0.82f, 0.92f, 0.0f, 0.84f, 0.72f } },
        { "Percussion / Stab Chain", true, true, 128.0f, 8, 1,
          { 0, 0, 12, 0, 7, 0, 10, 0 },
          { 0.28f, 0.0f, 0.26f, 0.0f, 0.24f, 0.0f, 0.22f, 0.0f },
          { 1.0f, 0.0f, 0.9f, 0.0f, 0.82f, 0.0f, 0.78f, 0.0f } },
        { "Percussion / Broken Hats", true, true, 132.0f, 8, 0,
          { 0, 2, 0, 5, 0, 7, 0, 10 },
          { 0.16f, 0.12f, 0.18f, 0.1f, 0.16f, 0.1f, 0.18f, 0.14f },
          { 0.72f, 0.62f, 0.76f, 0.58f, 0.7f, 0.56f, 0.78f, 0.64f } },
        { "Drone / Slow Bloom", true, false, 78.0f, 8, 7,
          { 0, 7, 10, 12, 10, 7, 5, 3 },
          { 0.95f, 0.88f, 0.92f, 0.84f, 0.96f, 0.88f, 0.9f, 0.82f },
          { 0.74f, 0.66f, 0.8f, 0.68f, 0.86f, 0.7f, 0.78f, 0.62f } },
        { "SFX / Glitch Drop", true, false, 142.0f, 8, 1,
          { 12, 7, 0, -5, 10, 3, -7, -12 },
          { 0.44f, 0.18f, 0.8f, 0.12f, 0.52f, 0.26f, 0.88f, 0.2f },
          { 0.8f, 0.42f, 1.0f, 0.36f, 0.88f, 0.52f, 1.0f, 0.44f } },
        { "Voice / Formant Pulse", true, true, 110.0f, 8, 4,
          { 0, 2, 5, 7, 5, 2, 0, -2 },
          { 0.78f, 0.54f, 0.7f, 0.56f, 0.76f, 0.52f, 0.72f, 0.48f },
          { 0.86f, 0.72f, 0.82f, 0.7f, 0.88f, 0.68f, 0.84f, 0.64f } }
    };
}

juce::String getTopLevelGroup(const juce::String& fullName)
{
    if (fullName.contains("/"))
        return fullName.upToFirstOccurrenceOf("/", false, false).trim();

    return fullName == "Init" ? "Factory" : "User";
}

juce::String getLeafName(const juce::String& fullName)
{
    if (fullName.contains("/"))
        return fullName.fromLastOccurrenceOf("/", false, false).trim();

    return fullName.trim();
}

double divisionIndexToQuarterNotes(int divisionIndex)
{
    static constexpr std::array<double, 9> values
    {{
        0.125, 0.25, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 2.0, 4.0
    }};

    return values[static_cast<size_t>(juce::jlimit(0, static_cast<int>(values.size()) - 1, divisionIndex))];
}

int wrapStepIndex(int absoluteStep, int stepCount)
{
    const auto safeCount = juce::jmax(1, stepCount);
    const auto wrapped = absoluteStep % safeCount;
    return wrapped < 0 ? wrapped + safeCount : wrapped;
}
}

void AISynthesiser::setPlayMode(int newKeyMode, int newVoiceCount)
{
    const auto clampedMode = juce::jlimit(0, 1, newKeyMode);
    const auto clampedVoiceCount = juce::jlimit(1, juce::jmax(1, getNumVoices()), newVoiceCount);
    const auto modeChanged = clampedMode != keyMode;

    keyMode = clampedMode;
    maxPlayableVoices = clampedVoiceCount;

    if (modeChanged)
    {
        monoHeldNotes.clear();
        currentMonoNote = -1;
        currentMonoChannel = 1;
        allNotesOff(0, false);
    }

    trimVoices();
}

void AISynthesiser::resetPlayState()
{
    monoHeldNotes.clear();
    currentMonoNote = -1;
    currentMonoChannel = 1;
    allNotesOff(0, false);
}

void AISynthesiser::noteOn(int midiChannel, int midiNoteNumber, float velocity)
{
    if (keyMode == 0)
    {
        juce::Synthesiser::noteOn(midiChannel, midiNoteNumber, velocity);
        return;
    }

    std::erase_if(monoHeldNotes,
                  [midiNoteNumber, midiChannel] (const MonoHeldNote& held)
                  {
                      return held.midiNote == midiNoteNumber && held.midiChannel == midiChannel;
                  });

    monoHeldNotes.push_back({ midiNoteNumber, midiChannel, velocity });
    startMonoVoice(midiChannel, midiNoteNumber, velocity);
    currentMonoNote = midiNoteNumber;
    currentMonoChannel = midiChannel;
}

void AISynthesiser::noteOff(int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff)
{
    if (keyMode == 0)
    {
        juce::Synthesiser::noteOff(midiChannel, midiNoteNumber, velocity, allowTailOff);
        return;
    }

    std::erase_if(monoHeldNotes,
                  [midiNoteNumber, midiChannel] (const MonoHeldNote& held)
                  {
                      return held.midiNote == midiNoteNumber && held.midiChannel == midiChannel;
                  });

    if (midiNoteNumber != currentMonoNote || midiChannel != currentMonoChannel)
        return;

    if (! monoHeldNotes.empty())
    {
        const auto replacement = monoHeldNotes.back();
        startMonoVoice(replacement.midiChannel, replacement.midiNote, replacement.velocity);
        currentMonoNote = replacement.midiNote;
        currentMonoChannel = replacement.midiChannel;
        return;
    }

    const auto voiceLimit = juce::jlimit(1, juce::jmax(1, getNumVoices()), maxPlayableVoices);

    for (int voiceIndex = 0; voiceIndex < voiceLimit; ++voiceIndex)
    {
        if (auto* voice = getVoice(voiceIndex);
            voice != nullptr
            && voice->isVoiceActive()
            && voice->isPlayingChannel(midiChannel)
            && voice->getCurrentlyPlayingNote() == midiNoteNumber)
        {
            stopVoice(voice, velocity, allowTailOff);
        }
    }

    currentMonoNote = -1;
    currentMonoChannel = 1;
}

juce::SynthesiserVoice* AISynthesiser::findFreeVoice(juce::SynthesiserSound* soundToPlay,
                                                     int midiChannel,
                                                     int midiNoteNumber,
                                                     bool stealIfNoneAvailable) const
{
    const auto voiceLimit = juce::jlimit(1, juce::jmax(1, getNumVoices()), maxPlayableVoices);

    for (int voiceIndex = 0; voiceIndex < voiceLimit; ++voiceIndex)
    {
        if (auto* voice = getVoice(voiceIndex);
            voice != nullptr
            && voice->canPlaySound(soundToPlay)
            && ! voice->isVoiceActive())
        {
            return voice;
        }
    }

    return stealIfNoneAvailable ? findVoiceToSteal(soundToPlay, midiChannel, midiNoteNumber) : nullptr;
}

juce::SynthesiserVoice* AISynthesiser::findVoiceToSteal(juce::SynthesiserSound* soundToPlay,
                                                        int midiChannel,
                                                        int midiNoteNumber) const
{
    juce::ignoreUnused(midiChannel, midiNoteNumber);

    const auto voiceLimit = juce::jlimit(1, juce::jmax(1, getNumVoices()), maxPlayableVoices);
    juce::SynthesiserVoice* oldestVoice = nullptr;

    for (int voiceIndex = 0; voiceIndex < voiceLimit; ++voiceIndex)
    {
        if (auto* voice = getVoice(voiceIndex);
            voice != nullptr
            && voice->canPlaySound(soundToPlay))
        {
            if (! voice->isVoiceActive())
                return voice;

            if (oldestVoice == nullptr || voice->wasStartedBefore(*oldestVoice))
                oldestVoice = voice;
        }
    }

    return oldestVoice;
}

void AISynthesiser::startMonoVoice(int midiChannel, int midiNoteNumber, float velocity)
{
    const auto voiceLimit = juce::jlimit(1, juce::jmax(1, getNumVoices()), maxPlayableVoices);
    juce::SynthesiserVoice* monoVoice = nullptr;

    for (int voiceIndex = 0; voiceIndex < voiceLimit; ++voiceIndex)
    {
        if (auto* voice = getVoice(voiceIndex); voice != nullptr && voice->isVoiceActive())
        {
            monoVoice = voice;
            break;
        }
    }

    for (int soundIndex = 0; soundIndex < getNumSounds(); ++soundIndex)
    {
        auto sound = getSound(soundIndex);
        if (sound == nullptr || ! sound->appliesToNote(midiNoteNumber) || ! sound->appliesToChannel(midiChannel))
            continue;

        if (monoVoice == nullptr)
            monoVoice = findFreeVoice(sound.get(), midiChannel, midiNoteNumber, true);

        if (monoVoice == nullptr)
            monoVoice = findVoiceToSteal(sound.get(), midiChannel, midiNoteNumber);

        if (monoVoice == nullptr)
            return;

        const auto reusingActiveVoice = monoVoice->isVoiceActive();
        if (auto* synthVoice = dynamic_cast<SynthVoice*>(monoVoice))
            synthVoice->setPendingPortamento(reusingActiveVoice, reusingActiveVoice);

        for (int voiceIndex = 0; voiceIndex < voiceLimit; ++voiceIndex)
        {
            if (auto* voice = getVoice(voiceIndex);
                voice != nullptr
                && voice != monoVoice
                && voice->isVoiceActive())
            {
                stopVoice(voice, 0.0f, false);
            }
        }

        startVoice(monoVoice, sound.get(), midiChannel, midiNoteNumber, velocity);
        return;
    }
}

void AISynthesiser::trimVoices()
{
    const auto voiceLimit = juce::jlimit(1, juce::jmax(1, getNumVoices()), maxPlayableVoices);
    bool monoVoiceKept = false;

    for (int voiceIndex = 0; voiceIndex < getNumVoices(); ++voiceIndex)
    {
        auto* voice = getVoice(voiceIndex);
        if (voice == nullptr || ! voice->isVoiceActive())
            continue;

        if (voiceIndex >= voiceLimit)
        {
            stopVoice(voice, 0.0f, false);
            continue;
        }

        if (keyMode == 1)
        {
            if (! monoVoiceKept)
            {
                monoVoiceKept = true;
            }
            else
            {
                stopVoice(voice, 0.0f, false);
            }
        }
    }

    if (keyMode == 1 && monoHeldNotes.empty())
    {
        currentMonoNote = -1;
        currentMonoChannel = 1;
    }
}

AISynthAudioProcessor::AISynthAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout()),
      factoryPresets(createFactoryPresets()),
      factoryPatterns(createFactoryPatterns())
{
    for (int i = 0; i < 16; ++i)
        synth.addVoice(new SynthVoice(*this));

    synth.addSound(new BasicSound());
    synth.setNoteStealingEnabled(true);

    refreshPatternList();
    refreshPresetList();
    loadPresetByName("Init");
}

void AISynthAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate(sampleRate);
    synth.setPlayMode(static_cast<int>(getParam("keyMode")), static_cast<int>(getParam("voiceCount")));

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<SynthVoice*>(synth.getVoice(i)))
            voice->prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());

    spec = { sampleRate, static_cast<juce::uint32>(samplesPerBlock), static_cast<juce::uint32>(juce::jmax(1, getTotalNumOutputChannels())) };

    chorus.prepare(spec);
    chorus.reset();

    compressor.prepare(spec);
    compressor.reset();
    compressor.setAttack(12.0f);
    compressor.setRelease(90.0f);

    reverb.prepare(spec);
    reverb.reset();

    bitcrusherHeldSamples.assign(static_cast<size_t>(juce::jmax(1, getTotalNumOutputChannels())), 0.0f);
    delayBuffer.setSize(juce::jmax(1, getTotalNumOutputChannels()),
                        juce::jmax(1, static_cast<int>(std::ceil(sampleRate * 8.0)) + samplesPerBlock));
    delayBuffer.clear();
    bitcrusherCounter = 0;
    delayWritePosition = 0;
    rhythmGateSamplePosition = 0;
    sequencerFreeSamplePosition = 0;
    arpSamplesUntilStep = 0;
    arpSamplesUntilOff = 0;
    sequencerCurrentNote = -1;
    sequencerLastAbsoluteStep = std::numeric_limits<int>::min();
    sequencerGateClosedStep = std::numeric_limits<int>::min();
    lastSequencerSourceNote = {};
    currentSequencerStep.store(-1);
    synth.resetPlayState();
}

void AISynthAudioProcessor::releaseResources() {}

bool AISynthAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

float AISynthAudioProcessor::getParam(const juce::String& id) const
{
    if (auto* rawValue = apvts.getRawParameterValue(id))
        return rawValue->load();

    return 0.0f;
}

void AISynthAudioProcessor::updatePerformanceStateFromMidi(const juce::MidiBuffer& midiMessages)
{
    for (const auto metadata : midiMessages)
    {
        const auto& message = metadata.getMessage();

        if (message.isPitchWheel())
            currentPitchWheelValue.store(message.getPitchWheelValue());
        else if (message.isController() && message.getControllerNumber() == 1)
            currentModWheelValue.store(juce::jlimit(0.0f, 1.0f, message.getControllerValue() / 127.0f));
    }
}

void AISynthAudioProcessor::handleMidiControllerMappings(const juce::MidiBuffer& midiMessages)
{
    for (const auto metadata : midiMessages)
    {
        const auto& message = metadata.getMessage();
        if (! message.isController())
            continue;

        const auto controllerNumber = message.getControllerNumber();
        if (controllerNumber < 0 || controllerNumber >= 120)
            continue;

        juce::String learnTarget;
        std::vector<MidiMapping> mappingsCopy;
        {
            const juce::SpinLock::ScopedLockType lock(midiMappingLock);
            learnTarget = pendingMidiLearnParameterId;
            mappingsCopy = currentMidiMappings;
        }

        if (learnTarget.isNotEmpty())
        {
            assignMidiControllerToParameter(learnTarget, controllerNumber);

            {
                const juce::SpinLock::ScopedLockType lock(midiMappingLock);
                mappingsCopy = currentMidiMappings;
            }
        }

        for (const auto& mapping : mappingsCopy)
            if (mapping.controllerNumber == controllerNumber)
                applyMappedControllerToParameter(mapping, message.getControllerValue());
    }
}

AISynthAudioProcessor::TransportState AISynthAudioProcessor::getTransportState() const
{
    TransportState state;

    if (auto* currentPlayHead = getPlayHead())
    {
        if (const auto position = currentPlayHead->getPosition())
        {
            if (const auto bpm = position->getBpm())
            {
                state.bpm = *bpm;
                state.hasBpm = true;
            }

            if (const auto ppqPosition = position->getPpqPosition())
            {
                state.ppqPosition = *ppqPosition;
                state.hasPpq = true;
            }

            state.isPlaying = position->getIsPlaying();
        }
    }

    return state;
}

void AISynthAudioProcessor::syncHeldNotesFromMidi(const juce::MidiBuffer& midiMessages, bool latchEnabled)
{
    for (const auto metadata : midiMessages)
    {
        const auto& message = metadata.getMessage();

        if (message.isNoteOnOrOff()
            || message.isAllNotesOff()
            || (message.isController() && (message.getControllerNumber() == 120 || message.getControllerNumber() == 123)))
        {
            updateHeldNotes(message, latchEnabled);
        }
    }
}

void AISynthAudioProcessor::updateHeldNotes(const juce::MidiMessage& message, bool latchEnabled)
{
    const auto noteNumber = message.getNoteNumber();

    if (message.isNoteOn())
    {
        const auto velocity = juce::jlimit(0.0f, 1.0f, message.getFloatVelocity());
        const auto match = [noteNumber] (const ArpHeldNote& held) { return held.midiNote == noteNumber; };
        const auto existing = std::find_if(heldNotes.begin(), heldNotes.end(), match);

        if (existing == heldNotes.end())
            heldNotes.push_back({ noteNumber, velocity });
        else
            existing->velocity = velocity;

        std::sort(heldNotes.begin(), heldNotes.end(),
                  [] (const ArpHeldNote& a, const ArpHeldNote& b) { return a.midiNote < b.midiNote; });
    }
    else if (message.isNoteOff())
    {
        if (latchEnabled)
            return;

        std::erase_if(heldNotes, [noteNumber] (const ArpHeldNote& held) { return held.midiNote == noteNumber; });
    }
    else if (message.isAllNotesOff() || (message.isController() && (message.getControllerNumber() == 120 || message.getControllerNumber() == 123)))
    {
        heldNotes.clear();
    }
}

std::vector<AISynthAudioProcessor::ArpHeldNote> AISynthAudioProcessor::buildArpPool() const
{
    std::vector<ArpHeldNote> pool;
    const auto octaveCount = juce::jmax(1, static_cast<int>(getParam("arpOctaves")));

    for (const auto& held : heldNotes)
        for (int octave = 0; octave < octaveCount; ++octave)
            pool.push_back({ juce::jlimit(0, 127, held.midiNote + octave * 12), held.velocity });

    std::sort(pool.begin(), pool.end(),
              [] (const ArpHeldNote& a, const ArpHeldNote& b) { return a.midiNote < b.midiNote; });

    return pool;
}

AISynthAudioProcessor::ArpHeldNote AISynthAudioProcessor::chooseNextArpNote()
{
    const auto pool = buildArpPool();
    if (pool.empty())
        return {};

    const auto mode = static_cast<int>(getParam("arpMode"));
    const auto count = static_cast<int>(pool.size());

    switch (mode)
    {
        case 1:
        {
            if (arpIndex <= 0 || arpIndex >= count)
                arpIndex = count - 1;

            const auto note = pool[static_cast<size_t>(arpIndex)];
            arpIndex = (arpIndex - 1 + count) % count;
            return note;
        }

        case 2:
        {
            arpIndex = juce::jlimit(0, count - 1, arpIndex);
            const auto note = pool[static_cast<size_t>(arpIndex)];

            if (arpDirectionForward)
            {
                if (arpIndex >= count - 1)
                {
                    arpDirectionForward = false;
                    if (count > 1)
                        --arpIndex;
                }
                else
                {
                    ++arpIndex;
                }
            }
            else
            {
                if (arpIndex <= 0)
                {
                    arpDirectionForward = true;
                    if (count > 1)
                        ++arpIndex;
                }
                else
                {
                    --arpIndex;
                }
            }

            return note;
        }

        case 3:
            return pool[static_cast<size_t>(random.nextInt(count))];

        default:
        {
            arpIndex %= count;
            const auto note = pool[static_cast<size_t>(arpIndex)];
            arpIndex = (arpIndex + 1) % count;
            return note;
        }
    }
}

void AISynthAudioProcessor::buildPerformanceMidi(const juce::MidiBuffer& source, juce::MidiBuffer& destination, int numSamples)
{
    destination.clear();
    updatePerformanceStateFromMidi(source);

    for (const auto metadata : source)
    {
        const auto& message = metadata.getMessage();

        if (! message.isNoteOnOrOff()
            && ! message.isAllNotesOff()
            && ! (message.isController() && (message.getControllerNumber() == 120 || message.getControllerNumber() == 123)))
        {
            destination.addEvent(message, metadata.samplePosition);
        }
    }

    if (heldNotes.empty())
    {
        if (arpCurrentNote >= 0)
            destination.addEvent(juce::MidiMessage::noteOff(1, arpCurrentNote), 0);

        arpCurrentNote = -1;
        arpSamplesUntilStep = 0;
        arpSamplesUntilOff = 0;
        return;
    }

    const auto rateHz = juce::jlimit(0.25f, 24.0f, getParam("arpRate"));
    const auto stepSamples = juce::jmax(1, static_cast<int>(std::round(getSampleRate() / rateHz)));
    const auto gateSamples = juce::jlimit(1, stepSamples, static_cast<int>(std::round(stepSamples * getParam("arpGate"))));

    int position = 0;
    while (position < numSamples)
    {
        if (arpCurrentNote >= 0 && arpSamplesUntilOff <= 0)
        {
            destination.addEvent(juce::MidiMessage::noteOff(1, arpCurrentNote), position);
            arpCurrentNote = -1;
        }

        if (arpSamplesUntilStep <= 0)
        {
            if (arpCurrentNote >= 0)
            {
                destination.addEvent(juce::MidiMessage::noteOff(1, arpCurrentNote), position);
                arpCurrentNote = -1;
            }

            const auto next = chooseNextArpNote();
            const auto velocity = static_cast<juce::uint8>(juce::jlimit(1, 127, static_cast<int>(std::round(next.velocity * 127.0f))));
            destination.addEvent(juce::MidiMessage::noteOn(1, next.midiNote, velocity), position);
            arpCurrentNote = next.midiNote;
            arpSamplesUntilOff = gateSamples;
            arpSamplesUntilStep = stepSamples;
        }

        int segmentLength = numSamples - position;

        if (arpCurrentNote >= 0 && arpSamplesUntilOff > 0)
            segmentLength = juce::jmin(segmentLength, arpSamplesUntilOff);

        if (arpSamplesUntilStep > 0)
            segmentLength = juce::jmin(segmentLength, arpSamplesUntilStep);

        if (segmentLength <= 0)
            break;

        position += segmentLength;
        if (arpSamplesUntilStep > 0)
            arpSamplesUntilStep -= segmentLength;
        if (arpCurrentNote >= 0 && arpSamplesUntilOff > 0)
            arpSamplesUntilOff -= segmentLength;
    }
}

void AISynthAudioProcessor::buildSequencerMidi(const juce::MidiBuffer& source,
                                               juce::MidiBuffer& destination,
                                               int numSamples,
                                               const TransportState& transport,
                                               bool arpEnabled)
{
    destination.clear();
    updatePerformanceStateFromMidi(source);

    const auto syncToHost = getParam("stepSeqSync") > 0.5f && transport.hasBpm && transport.hasPpq;
    const auto freeBpm = juce::jlimit(40.0f, 240.0f, getParam("stepSeqBpm"));
    const auto stepCount = juce::jmax(1, static_cast<int>(getParam("stepSeqSteps")));
    const auto divisionIndex = static_cast<int>(getParam("stepSeqLength"));
    const auto stepQuarterNotes = divisionIndexToQuarterNotes(divisionIndex);
    const auto freeSamplesPerStep = juce::jmax(1.0, getSampleRate() * (60.0 / static_cast<double>(freeBpm)) * stepQuarterNotes);
    const auto ppqPerSample = syncToHost && transport.isPlaying
        ? (transport.bpm / 60.0) / getSampleRate()
        : 0.0;

    std::vector<std::pair<int, juce::MidiMessage>> sourceEvents;
    sourceEvents.reserve(static_cast<size_t>(source.getNumEvents()));

    for (const auto metadata : source)
        sourceEvents.emplace_back(metadata.samplePosition, metadata.getMessage());

    size_t eventIndex = 0;
    const auto clearStepState = [&]
    {
        if (sequencerCurrentNote >= 0)
            destination.addEvent(juce::MidiMessage::noteOff(1, sequencerCurrentNote), 0);

        sequencerCurrentNote = -1;
        sequencerLastAbsoluteStep = std::numeric_limits<int>::min();
        sequencerGateClosedStep = std::numeric_limits<int>::min();
        currentSequencerStep.store(-1);
    };

    if (arpEnabled && heldNotes.empty())
    {
        lastSequencerSourceNote = {};
        sequencerFreeSamplePosition = 0;
        clearStepState();
        for (const auto& [samplePosition, message] : sourceEvents)
            if (! message.isNoteOnOrOff() && ! message.isAllNotesOff())
                destination.addEvent(message, samplePosition);
        return;
    }

    for (int sample = 0; sample < numSamples; ++sample)
    {
        while (eventIndex < sourceEvents.size() && sourceEvents[eventIndex].first == sample)
        {
            const auto& message = sourceEvents[eventIndex].second;

            if (message.isNoteOn())
            {
                lastSequencerSourceNote = { message.getNoteNumber(), juce::jlimit(0.0f, 1.0f, message.getFloatVelocity()), true };
            }
            else if (! arpEnabled && message.isNoteOff() && lastSequencerSourceNote.valid && message.getNoteNumber() == lastSequencerSourceNote.midiNote)
            {
                lastSequencerSourceNote.valid = false;
                clearStepState();
            }
            else if (message.isAllNotesOff() || (message.isController() && (message.getControllerNumber() == 120 || message.getControllerNumber() == 123)))
            {
                lastSequencerSourceNote = {};
                clearStepState();
            }
            else
            {
                destination.addEvent(message, sample);
            }

            ++eventIndex;
        }

        if (! lastSequencerSourceNote.valid)
            continue;

        const auto stepPosition = syncToHost
            ? ((transport.ppqPosition + (ppqPerSample * static_cast<double>(sample))) / stepQuarterNotes)
            : ((static_cast<double>(sequencerFreeSamplePosition) + static_cast<double>(sample)) / freeSamplesPerStep);

        const auto absoluteStep = static_cast<int>(std::floor(stepPosition));
        const auto stepPhase = static_cast<float>(stepPosition - std::floor(stepPosition));
        const auto stepIndex = wrapStepIndex(absoluteStep, stepCount);

        if (absoluteStep != sequencerLastAbsoluteStep)
        {
            if (sequencerCurrentNote >= 0)
            {
                destination.addEvent(juce::MidiMessage::noteOff(1, sequencerCurrentNote), sample);
                sequencerCurrentNote = -1;
            }

            sequencerLastAbsoluteStep = absoluteStep;
            sequencerGateClosedStep = std::numeric_limits<int>::min();
            currentSequencerStep.store(stepIndex);

            const auto stepId = juce::String(stepIndex + 1);
            const auto stepNote = static_cast<int>(std::round(getParam("seqStep" + stepId + "Note")));
            const auto stepGate = juce::jlimit(0.0f, 1.0f, getParam("seqStep" + stepId + "Gate"));
            const auto stepVelocity = juce::jlimit(0.0f, 1.0f, getParam("seqStep" + stepId + "Velocity"));

            if (stepGate > 0.001f && stepVelocity > 0.001f)
            {
                const auto noteNumber = juce::jlimit(0, 127, lastSequencerSourceNote.midiNote + stepNote);
                const auto velocity = static_cast<juce::uint8>(juce::jlimit(1, 127, static_cast<int>(std::round(lastSequencerSourceNote.velocity * stepVelocity * 127.0f))));
                destination.addEvent(juce::MidiMessage::noteOn(1, noteNumber, velocity), sample);
                sequencerCurrentNote = noteNumber;
            }
        }

        if (sequencerCurrentNote >= 0)
        {
            const auto stepId = juce::String(stepIndex + 1);
            const auto stepGate = juce::jlimit(0.0f, 1.0f, getParam("seqStep" + stepId + "Gate"));

            if (stepPhase >= stepGate && sequencerGateClosedStep != absoluteStep)
            {
                destination.addEvent(juce::MidiMessage::noteOff(1, sequencerCurrentNote), sample);
                sequencerCurrentNote = -1;
                sequencerGateClosedStep = absoluteStep;
            }
        }
    }

    if (! syncToHost)
        sequencerFreeSamplePosition += static_cast<juce::uint64>(numSamples);
}

void AISynthAudioProcessor::applyRhythmGate(juce::AudioBuffer<float>& buffer)
{
    if (getParam("rhythmGateEnabled") <= 0.5f)
        return;

    const auto rateHz = juce::jlimit(0.25f, 24.0f, getParam("rhythmGateRate"));
    const auto depth = juce::jlimit(0.0f, 1.0f, getParam("rhythmGateDepth"));
    const auto patternIndex = juce::jlimit(0, static_cast<int>(gatePatterns.size()) - 1, static_cast<int>(getParam("rhythmGatePattern")));
    const auto roundedSamplesPerStep = static_cast<juce::uint64>(std::round(getSampleRate() / rateHz));
    const auto samplesPerStep = roundedSamplesPerStep > 0 ? roundedSamplesPerStep : juce::uint64 { 1 };
    const auto& pattern = gatePatterns[static_cast<size_t>(patternIndex)];

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto step = static_cast<size_t>((rhythmGateSamplePosition / samplesPerStep) % pattern.size());
        const auto pulse = pattern[step];
        const auto gain = (1.0f - depth) + (pulse * depth);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample(channel, sample, buffer.getSample(channel, sample) * gain);

        ++rhythmGateSamplePosition;
    }
}

void AISynthAudioProcessor::applyBitcrusher(juce::AudioBuffer<float>& buffer)
{
    const auto mix = juce::jlimit(0.0f, 1.0f, getParam("bitcrusherMix"));
    if (mix <= 0.0001f)
        return;

    const auto bitDepth = juce::jmax(2, static_cast<int>(getParam("bitDepth")));
    const auto downsample = juce::jmax(1, static_cast<int>(getParam("bitDownsample")));
    const auto levels = static_cast<float>(1 << (bitDepth - 1));

    if (static_cast<int>(bitcrusherHeldSamples.size()) < buffer.getNumChannels())
        bitcrusherHeldSamples.resize(static_cast<size_t>(buffer.getNumChannels()), 0.0f);

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        if (bitcrusherCounter <= 0)
        {
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                const auto dry = buffer.getSample(channel, sample);
                bitcrusherHeldSamples[static_cast<size_t>(channel)] = std::round(dry * levels) / levels;
            }

            bitcrusherCounter = downsample;
        }

        --bitcrusherCounter;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto dry = buffer.getSample(channel, sample);
            const auto crushed = bitcrusherHeldSamples[static_cast<size_t>(channel)];
            buffer.setSample(channel, sample, juce::jmap(mix, dry, crushed));
        }
    }
}

void AISynthAudioProcessor::applyDelay(juce::AudioBuffer<float>& buffer, const TransportState& transport)
{
    const auto mix = juce::jlimit(0.0f, 1.0f, getParam("delayMix"));
    if (mix <= 0.0001f || delayBuffer.getNumSamples() <= 1)
        return;

    auto delayTimeSeconds = static_cast<double>(juce::jlimit(0.02f, 2.0f, getParam("delayTime")));
    if (getParam("delaySync") > 0.5f && transport.hasBpm)
        delayTimeSeconds = (60.0 / transport.bpm) * divisionIndexToQuarterNotes(static_cast<int>(getParam("delayDivision")));

    const auto feedback = juce::jlimit(0.0f, 0.95f, getParam("delayFeedback"));
    const auto delaySamples = juce::jlimit(1,
                                           delayBuffer.getNumSamples() - 1,
                                           static_cast<int>(std::round(delayTimeSeconds * getSampleRate())));

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto readPosition = (delayWritePosition + delayBuffer.getNumSamples() - delaySamples) % delayBuffer.getNumSamples();

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto bufferChannel = juce::jmin(channel, delayBuffer.getNumChannels() - 1);
            const auto dry = buffer.getSample(channel, sample);
            const auto delayed = delayBuffer.getSample(bufferChannel, readPosition);
            delayBuffer.setSample(bufferChannel, delayWritePosition, dry + delayed * feedback);
            buffer.setSample(channel, sample, juce::jmap(mix, dry, delayed));
        }

        delayWritePosition = (delayWritePosition + 1) % delayBuffer.getNumSamples();
    }
}

void AISynthAudioProcessor::applyEffects(juce::AudioBuffer<float>& buffer, const TransportState& transport)
{
    auto block = juce::dsp::AudioBlock<float>(buffer);
    auto context = juce::dsp::ProcessContextReplacing<float>(block);

    chorus.setMix(getParam("chorusMix"));
    chorus.setRate(getParam("chorusRate"));
    chorus.setDepth(getParam("chorusDepth"));
    chorus.setCentreDelay(7.0f);
    chorus.process(context);

    const auto distortionAmount = getParam("distortionAmount");
    const auto saturationAmount = getParam("saturationAmount");

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            auto value = buffer.getSample(channel, sample);

            if (distortionAmount > 0.0001f)
            {
                const auto drive = 1.0f + distortionAmount * 22.0f;
                value = std::tanh(value * drive);
            }

            if (saturationAmount > 0.0001f)
            {
                const auto drive = 1.0f + saturationAmount * 9.0f;
                value = std::atan(value * drive) / std::atan(drive);
            }

            buffer.setSample(channel, sample, value);
        }
    }

    applyBitcrusher(buffer);

    compressor.setThreshold(getParam("compressorThreshold"));
    compressor.setRatio(getParam("compressorRatio"));
    compressor.process(context);

    applyDelay(buffer, transport);

    juce::dsp::Reverb::Parameters reverbParameters;
    reverbParameters.roomSize = getParam("reverbSize");
    reverbParameters.damping = getParam("reverbDamping");
    reverbParameters.width = 1.0f;
    reverbParameters.freezeMode = 0.0f;
    reverbParameters.wetLevel = getParam("reverbMix");
    reverbParameters.dryLevel = 1.0f - getParam("reverbMix");
    reverb.setParameters(reverbParameters);

    reverb.process(context);
}

void AISynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);
    handleMidiControllerMappings(midiMessages);
    synth.setPlayMode(static_cast<int>(getParam("keyMode")), static_cast<int>(getParam("voiceCount")));

    const auto transport = getTransportState();
    const auto arpEnabled = getParam("arpEnabled") > 0.5f;
    const auto stepSequencerEnabled = getParam("stepSeqEnabled") > 0.5f;
    syncHeldNotesFromMidi(midiMessages, arpEnabled && getParam("arpLatch") > 0.5f);

    juce::MidiBuffer arpMidi;

    if (arpEnabled)
    {
        buildPerformanceMidi(midiMessages, arpMidi, buffer.getNumSamples());
    }
    else
    {
        updatePerformanceStateFromMidi(midiMessages);
        arpMidi = midiMessages;

        if (arpCurrentNote >= 0)
            arpMidi.addEvent(juce::MidiMessage::noteOff(1, arpCurrentNote), 0);

        arpCurrentNote = -1;
        arpSamplesUntilStep = 0;
        arpSamplesUntilOff = 0;
    }

    juce::MidiBuffer performanceMidi;
    if (stepSequencerEnabled)
    {
        buildSequencerMidi(arpMidi, performanceMidi, buffer.getNumSamples(), transport, arpEnabled);
    }
    else
    {
        performanceMidi = arpMidi;

        if (sequencerCurrentNote >= 0)
            performanceMidi.addEvent(juce::MidiMessage::noteOff(1, sequencerCurrentNote), 0);

        if (heldNotes.empty())
            lastSequencerSourceNote = {};

        sequencerCurrentNote = -1;
        sequencerLastAbsoluteStep = std::numeric_limits<int>::min();
        sequencerGateClosedStep = std::numeric_limits<int>::min();
        sequencerFreeSamplePosition = 0;
        currentSequencerStep.store(-1);
    }

    buffer.clear();
    synth.renderNextBlock(buffer, performanceMidi, 0, buffer.getNumSamples());

    applyRhythmGate(buffer);
    applyEffects(buffer, transport);
    buffer.applyGain(getParam("masterGain"));
}

juce::AudioProcessorValueTreeState::ParameterLayout AISynthAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto addChoice = [&params] (const juce::String& id, const juce::String& name, const juce::StringArray& choices, int defaultIndex)
    {
        params.push_back(std::make_unique<juce::AudioParameterChoice>(id, name, choices, defaultIndex));
    };

    auto addFloat = [&params] (const juce::String& id, const juce::String& name, juce::NormalisableRange<float> range, float defaultValue)
    {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(id, name, range, defaultValue));
    };

    auto addInt = [&params] (const juce::String& id, const juce::String& name, int minValue, int maxValue, int defaultValue)
    {
        params.push_back(std::make_unique<juce::AudioParameterInt>(id, name, minValue, maxValue, defaultValue));
    };

    addFloat("masterGain", "Master Gain", { 0.0f, 1.0f }, 0.78f);
    addFloat("pitchBendRange", "Pitch Bend Range", { 0.0f, 24.0f }, 2.0f);
    addChoice("keyMode", "Key Mode", keyModeChoices, 0);
    addChoice("portamentoMode", "Portamento Mode", portamentoModeChoices, 0);
    addFloat("portamentoTime", "Portamento Time", { 0.0f, 2.0f, 0.001f, 0.35f }, 0.0f);
    addInt("voiceCount", "Voice Count", 1, 16, 8);
    addInt("unisonVoices", "Unison Voices", 1, 8, 4);
    addFloat("unisonDetune", "Unison Detune", { 0.0f, 40.0f }, 8.0f);
    addFloat("stereoWidth", "Stereo Width", { 0.0f, 1.0f }, 0.68f);
    addFloat("velocitySensitivity", "Velocity Sensitivity", { 0.0f, 1.0f }, 0.72f);
    addChoice("velocityDestination", "Velocity Destination", matrixDestinationChoices, 0);
    addFloat("velocityAmount", "Velocity Amount", { -1.0f, 1.0f }, 0.0f);

    addChoice("osc1Wave", "Osc 1 Wave", waveformChoices, 1);
    addFloat("osc1Level", "Osc 1 Level", { 0.0f, 1.0f }, 0.82f);
    addInt("osc1Coarse", "Osc 1 Coarse", -24, 24, 0);
    addFloat("osc1Fine", "Osc 1 Fine", { -100.0f, 100.0f, 0.01f }, 0.0f);
    addFloat("osc1PulseWidth", "Osc 1 Pulse Width", { 0.05f, 0.95f }, 0.5f);
    addFloat("osc1PwmAmount", "Osc 1 PWM Amount", { 0.0f, 1.0f }, 0.2f);
    addChoice("osc1PwmSource", "Osc 1 PWM Source", pwmSourceChoices, 1);

    addChoice("osc2Wave", "Osc 2 Wave", waveformChoices, 2);
    addFloat("osc2Level", "Osc 2 Level", { 0.0f, 1.0f }, 0.58f);
    addInt("osc2Coarse", "Osc 2 Coarse", -24, 24, 7);
    addFloat("osc2Fine", "Osc 2 Fine", { -100.0f, 100.0f, 0.01f }, -4.0f);
    addFloat("osc2PulseWidth", "Osc 2 Pulse Width", { 0.05f, 0.95f }, 0.42f);
    addFloat("osc2PwmAmount", "Osc 2 PWM Amount", { 0.0f, 1.0f }, 0.24f);
    addChoice("osc2PwmSource", "Osc 2 PWM Source", pwmSourceChoices, 2);
    addChoice("osc2SyncSource", "Osc 2 Sync Source", syncSourceChoices, 0);

    addChoice("osc3Wave", "Osc 3 Wave", waveformChoices, 3);
    addFloat("osc3Level", "Osc 3 Level", { 0.0f, 1.0f }, 0.42f);
    addInt("osc3Coarse", "Osc 3 Coarse", -24, 24, -12);
    addFloat("osc3Fine", "Osc 3 Fine", { -100.0f, 100.0f, 0.01f }, 6.0f);
    addFloat("osc3PulseWidth", "Osc 3 Pulse Width", { 0.05f, 0.95f }, 0.6f);
    addFloat("osc3PwmAmount", "Osc 3 PWM Amount", { 0.0f, 1.0f }, 0.18f);
    addChoice("osc3PwmSource", "Osc 3 PWM Source", pwmSourceChoices, 3);
    addChoice("osc3SyncSource", "Osc 3 Sync Source", syncSourceChoices, 0);

    addChoice("subWave", "Sub Wave", subWaveformChoices, 1);
    addFloat("subLevel", "Sub Level", { 0.0f, 1.0f }, 0.35f);
    addInt("subOctave", "Sub Octave", -3, 0, -1);
    addFloat("subPulseWidth", "Sub Pulse Width", { 0.05f, 0.95f }, 0.5f);
    addFloat("subPwmAmount", "Sub PWM Amount", { 0.0f, 1.0f }, 0.15f);
    addChoice("subPwmSource", "Sub PWM Source", pwmSourceChoices, 1);

    addChoice("filterType", "Filter Type", filterChoices, 0);
    addFloat("cutoff", "Cutoff", { 40.0f, 18000.0f, 1.0f, 0.22f }, 11000.0f);
    addFloat("resonance", "Resonance", { 0.1f, 1.4f }, 0.38f);
    addFloat("drive", "Drive", { 0.0f, 1.0f }, 0.2f);
    addFloat("filterAccent", "Filter Accent", { 0.0f, 1.0f }, 0.45f);
    addFloat("env1ToFilter", "Env 1 To Filter", { 0.0f, 1.0f }, 0.46f);
    addFloat("env2ToFilter", "Env 2 To Filter", { -1.0f, 1.0f }, 0.28f);
    addFloat("env2ToPitch", "Env 2 To Pitch", { -24.0f, 24.0f }, 0.0f);

    addFloat("env1Attack", "Env 1 Attack", { 0.001f, 5.0f, 0.001f, 0.4f }, 0.02f);
    addFloat("env1Decay", "Env 1 Decay", { 0.001f, 5.0f, 0.001f, 0.4f }, 0.22f);
    addFloat("env1Sustain", "Env 1 Sustain", { 0.0f, 1.0f }, 0.72f);
    addFloat("env1Release", "Env 1 Release", { 0.001f, 8.0f, 0.001f, 0.4f }, 0.45f);

    addFloat("env2Attack", "Env 2 Attack", { 0.001f, 5.0f, 0.001f, 0.4f }, 0.05f);
    addFloat("env2Decay", "Env 2 Decay", { 0.001f, 5.0f, 0.001f, 0.4f }, 0.4f);
    addFloat("env2Sustain", "Env 2 Sustain", { 0.0f, 1.0f }, 0.0f);
    addFloat("env2Release", "Env 2 Release", { 0.001f, 8.0f, 0.001f, 0.4f }, 0.35f);

    for (int lfo = 1; lfo <= 3; ++lfo)
    {
        addChoice("lfo" + juce::String(lfo) + "Shape", "LFO " + juce::String(lfo) + " Shape", lfoShapeChoices, lfo == 2 ? 1 : 0);
        addFloat("lfo" + juce::String(lfo) + "Rate", "LFO " + juce::String(lfo) + " Rate", { 0.01f, 20.0f }, lfo == 1 ? 2.0f : (lfo == 2 ? 0.45f : 5.5f));
        addFloat("lfo" + juce::String(lfo) + "Depth", "LFO " + juce::String(lfo) + " Depth", { 0.0f, 1.0f }, lfo == 1 ? 0.32f : (lfo == 2 ? 0.22f : 0.15f));
    }

    params.push_back(std::make_unique<juce::AudioParameterBool>("arpEnabled", "Arpeggiator Enabled", false));
    addChoice("arpMode", "Arpeggiator Mode", arpModeChoices, 0);
    addFloat("arpRate", "Arpeggiator Rate", { 0.25f, 20.0f }, 6.0f);
    addFloat("arpGate", "Arpeggiator Gate", { 0.1f, 0.95f }, 0.62f);
    addInt("arpOctaves", "Arpeggiator Octaves", 1, 4, 2);
    params.push_back(std::make_unique<juce::AudioParameterBool>("arpLatch", "Arpeggiator Latch", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>("rhythmGateEnabled", "Rhythm Gate Enabled", false));
    addFloat("rhythmGateRate", "Rhythm Gate Rate", { 0.25f, 20.0f }, 8.0f);
    addFloat("rhythmGateDepth", "Rhythm Gate Depth", { 0.0f, 1.0f }, 0.7f);
    addChoice("rhythmGatePattern", "Rhythm Gate Pattern", gatePatternChoices, 1);

    params.push_back(std::make_unique<juce::AudioParameterBool>("stepSeqEnabled", "Step Sequencer Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("stepSeqSync", "Step Sequencer Sync", true));
    addFloat("stepSeqBpm", "Step Sequencer BPM", { 40.0f, 240.0f }, 124.0f);
    addInt("stepSeqSteps", "Step Sequencer Steps", 1, 8, 8);
    addChoice("stepSeqLength", "Step Sequencer Length", timingDivisionChoices, 3);

    for (int step = 1; step <= 8; ++step)
    {
        addInt("seqStep" + juce::String(step) + "Note", "Sequencer Step " + juce::String(step) + " Note", -24, 24, step == 1 ? 0 : (step % 2 == 0 ? 7 : 0));
        addFloat("seqStep" + juce::String(step) + "Gate", "Sequencer Step " + juce::String(step) + " Gate", { 0.0f, 1.0f }, step % 3 == 0 ? 0.55f : 0.82f);
        addFloat("seqStep" + juce::String(step) + "Velocity", "Sequencer Step " + juce::String(step) + " Velocity", { 0.0f, 1.0f }, step % 4 == 0 ? 0.68f : 0.92f);
    }

    addFloat("delayMix", "Delay Mix", { 0.0f, 1.0f }, 0.0f);
    addFloat("delayFeedback", "Delay Feedback", { 0.0f, 0.95f }, 0.35f);
    addFloat("delayTime", "Delay Time", { 0.02f, 2.0f }, 0.32f);
    params.push_back(std::make_unique<juce::AudioParameterBool>("delaySync", "Delay Sync", true));
    addChoice("delayDivision", "Delay Division", timingDivisionChoices, 3);

    addFloat("chorusMix", "Chorus Mix", { 0.0f, 1.0f }, 0.18f);
    addFloat("chorusRate", "Chorus Rate", { 0.01f, 6.0f }, 0.35f);
    addFloat("chorusDepth", "Chorus Depth", { 0.0f, 1.0f }, 0.42f);
    addFloat("distortionAmount", "Distortion", { 0.0f, 1.0f }, 0.12f);
    addFloat("saturationAmount", "Saturation", { 0.0f, 1.0f }, 0.18f);
    addFloat("compressorThreshold", "Compressor Threshold", { -40.0f, 0.0f }, -18.0f);
    addFloat("compressorRatio", "Compressor Ratio", { 1.0f, 20.0f }, 3.0f);
    addFloat("reverbMix", "Reverb Mix", { 0.0f, 1.0f }, 0.2f);
    addFloat("reverbSize", "Reverb Size", { 0.0f, 1.0f }, 0.45f);
    addFloat("reverbDamping", "Reverb Damping", { 0.0f, 1.0f }, 0.4f);
    addFloat("bitcrusherMix", "Bitcrusher Mix", { 0.0f, 1.0f }, 0.0f);
    addInt("bitDepth", "Bit Depth", 2, 16, 10);
    addInt("bitDownsample", "Bit Downsample", 1, 32, 4);

    for (int slot = 1; slot <= 4; ++slot)
    {
        addChoice("matrixSource" + juce::String(slot), "Matrix Source " + juce::String(slot), matrixSourceChoices, 0);
        addChoice("matrixDest" + juce::String(slot), "Matrix Destination " + juce::String(slot), matrixDestinationChoices, 0);
        addFloat("matrixAmount" + juce::String(slot), "Matrix Amount " + juce::String(slot), { -1.0f, 1.0f }, 0.0f);
    }

    return { params.begin(), params.end() };
}

juce::File AISynthAudioProcessor::getPresetDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("AI Synth")
        .getChildFile("Presets");
}

juce::File AISynthAudioProcessor::getPatternDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("AI Synth")
        .getChildFile("Patterns");
}

juce::File AISynthAudioProcessor::getMidiMappingDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("AI Synth")
        .getChildFile("MidiMappings");
}

void AISynthAudioProcessor::resetParametersToDefaults()
{
    for (auto* parameter : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
            ranged->setValueNotifyingHost(ranged->getDefaultValue());
}

void AISynthAudioProcessor::setParameterValue(const juce::String& id, float value)
{
    if (auto* parameter = apvts.getParameter(id))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        parameter->endChangeGesture();
    }
}

juce::ValueTree AISynthAudioProcessor::createMidiMappingsState(const juce::String& presetName,
                                                               const std::vector<MidiMapping>& mappings) const
{
    auto state = juce::ValueTree("MIDI_MAPPINGS");
    state.setProperty("presetName", presetName, nullptr);
    state.setProperty("displayName", presetName, nullptr);

    for (const auto& mapping : mappings)
    {
        auto child = juce::ValueTree("MAP");
        child.setProperty("parameterId", mapping.parameterId, nullptr);
        child.setProperty("controllerNumber", mapping.controllerNumber, nullptr);
        state.appendChild(child, nullptr);
    }

    return state;
}

void AISynthAudioProcessor::setCurrentMidiMappings(std::vector<MidiMapping> mappings)
{
    const juce::SpinLock::ScopedLockType lock(midiMappingLock);
    currentMidiMappings = std::move(mappings);
    pendingMidiLearnParameterId.clear();
}

void AISynthAudioProcessor::queueMidiMappingSave(const juce::String& presetName)
{
    {
        const juce::SpinLock::ScopedLockType lock(midiMappingLock);
        midiMappingSnapshotToPersist.presetName = presetName;
        midiMappingSnapshotToPersist.mappings = currentMidiMappings;
    }

    triggerAsyncUpdate();
}

bool AISynthAudioProcessor::loadMidiMappingsFromFile(const juce::File& file, std::vector<MidiMapping>& mappings) const
{
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr)
        return false;

    auto state = juce::ValueTree::fromXml(*xml);
    if (! state.isValid())
        return false;

    mappings.clear();
    for (int index = 0; index < state.getNumChildren(); ++index)
    {
        const auto child = state.getChild(index);
        if (! child.hasType("MAP"))
            continue;

        const auto parameterId = child.getProperty("parameterId").toString();
        const auto controllerNumber = static_cast<int>(child.getProperty("controllerNumber", -1));
        if (parameterId.isNotEmpty() && controllerNumber >= 0 && controllerNumber < 120)
            mappings.push_back({ parameterId, controllerNumber });
    }

    return true;
}

void AISynthAudioProcessor::loadMidiMappingsForCurrentPreset(const juce::ValueTree* embeddedState)
{
    std::vector<MidiMapping> mappings;

    if (embeddedState != nullptr && embeddedState->isValid())
    {
        for (int index = 0; index < embeddedState->getNumChildren(); ++index)
        {
            const auto child = embeddedState->getChild(index);
            if (! child.hasType("MAP"))
                continue;

            const auto parameterId = child.getProperty("parameterId").toString();
            const auto controllerNumber = static_cast<int>(child.getProperty("controllerNumber", -1));
            if (parameterId.isNotEmpty() && controllerNumber >= 0 && controllerNumber < 120)
                mappings.push_back({ parameterId, controllerNumber });
        }
    }

    const auto trimmedName = currentPresetName.trim();
    if (trimmedName.isNotEmpty())
    {
        auto mappingFile = getMidiMappingDirectory().getChildFile(juce::File::createLegalFileName(trimmedName) + ".xml");
        if (mappingFile.existsAsFile())
            loadMidiMappingsFromFile(mappingFile, mappings);
    }

    setCurrentMidiMappings(std::move(mappings));
}

void AISynthAudioProcessor::applyMappedControllerToParameter(const MidiMapping& mapping, int controllerValue)
{
    if (auto* parameter = apvts.getParameter(mapping.parameterId))
        parameter->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, controllerValue / 127.0f));
}

void AISynthAudioProcessor::assignMidiControllerToParameter(const juce::String& parameterId, int controllerNumber)
{
    if (parameterId.isEmpty() || controllerNumber < 0 || controllerNumber >= 120 || currentPresetName.trim().isEmpty())
        return;

    {
        const juce::SpinLock::ScopedLockType lock(midiMappingLock);

        std::erase_if(currentMidiMappings,
                      [&] (const MidiMapping& mapping)
                      {
                          return mapping.parameterId == parameterId || mapping.controllerNumber == controllerNumber;
                      });

        currentMidiMappings.push_back({ parameterId, controllerNumber });
        pendingMidiLearnParameterId.clear();
    }

    queueMidiMappingSave(currentPresetName);
}

bool AISynthAudioProcessor::loadPresetFile(const juce::File& file)
{
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr)
        return false;

    auto state = juce::ValueTree::fromXml(*xml);
    if (! state.isValid())
        return false;

    apvts.replaceState(state);
    const auto displayName = state.getProperty("displayName").toString();
    const auto storedName = state.getProperty("currentPresetName").toString();
    currentPresetName = displayName.isNotEmpty() ? displayName
                      : (storedName.isNotEmpty() ? storedName : file.getFileNameWithoutExtension());
    currentPatternName = state.getProperty("currentPatternName", currentPatternName).toString();
    setCurrentThemeIndex(static_cast<int>(state.getProperty("themeIndex", currentThemeIndex)));
    auto embeddedMappings = state.getChildWithName("MIDI_MAPPINGS");
    loadMidiMappingsForCurrentPreset(embeddedMappings.isValid() ? &embeddedMappings : nullptr);
    return true;
}

bool AISynthAudioProcessor::loadPatternFile(const juce::File& file)
{
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr)
        return false;

    auto state = juce::ValueTree::fromXml(*xml);
    if (! state.isValid())
        return false;

    const auto applyFloatProperty = [this, &state] (const juce::String& id)
    {
        if (state.hasProperty(id))
            setParameterValue(id, static_cast<float>(static_cast<double>(state.getProperty(id))));
    };

    applyFloatProperty("stepSeqEnabled");
    applyFloatProperty("stepSeqSync");
    applyFloatProperty("stepSeqBpm");
    applyFloatProperty("stepSeqSteps");
    applyFloatProperty("stepSeqLength");

    for (int step = 1; step <= 8; ++step)
    {
        const auto stepId = juce::String(step);
        applyFloatProperty("seqStep" + stepId + "Note");
        applyFloatProperty("seqStep" + stepId + "Gate");
        applyFloatProperty("seqStep" + stepId + "Velocity");
    }

    currentPatternName = state.getProperty("displayName", file.getFileNameWithoutExtension()).toString();
    return true;
}

void AISynthAudioProcessor::applyFactoryPreset(const PresetDefinition& preset)
{
    resetParametersToDefaults();

    for (const auto& [id, value] : preset.values)
        setParameterValue(id, value);

    currentPresetName = preset.name;
    currentPatternName = preset.patternName.isNotEmpty() ? preset.patternName : "Init Pattern";
    loadMidiMappingsForCurrentPreset(nullptr);
}

void AISynthAudioProcessor::applyPatternDefinition(const PatternDefinition& pattern)
{
    for (const auto& [id, value] : pattern.values)
        setParameterValue(id, value);

    currentPatternName = pattern.name;
}

std::vector<AISynthAudioProcessor::PresetDefinition> AISynthAudioProcessor::createFactoryPresets()
{
    using Values = ParameterValues;

    auto setValue = [] (Values& values, const juce::String& id, float value)
    {
        setOrAddValue(values, id, value);
    };

    const juce::StringArray variantNames
    {
        "Atlas", "Pulse", "Velour", "Strobe", "Nova",
        "Chrome", "Echo", "Monarch", "Prism", "Static",
        "Ember", "Rift", "Halo", "Vapor", "Mirror",
        "Signal", "Quartz", "Phantom", "Tundra", "Orbit"
    };

    std::vector<PresetDefinition> presets;
    presets.reserve(205);
    presets.push_back({ "Init", {}, "Init Pattern" });

    const auto patternTemplates = createStepPatternTemplates();

    const auto selectPattern = [&] (const juce::String& group, int index) -> const StepPatternTemplate&
    {
        if (group == "Bass")
            return patternTemplates[static_cast<size_t>(1 + (index % 2))];
        if (group == "Lead")
            return patternTemplates[static_cast<size_t>(3 + (index % 2))];
        if (group == "Pad")
            return patternTemplates[9];
        if (group == "Pluck")
            return patternTemplates[static_cast<size_t>(7 + (index % 2))];
        if (group == "Keys")
            return patternTemplates[static_cast<size_t>(3 + (index % 2))];
        if (group == "Percussion")
            return patternTemplates[static_cast<size_t>(7 + (index % 2))];
        if (group == "SFX")
            return patternTemplates[10];
        if (group == "Drone")
            return patternTemplates[9];
        if (group == "Voice")
            return patternTemplates[11];

        return patternTemplates[static_cast<size_t>(5 + (index % 2))];
    };

    const auto applyFactoryScene = [&] (const juce::String& group, int index, Values& values) -> juce::String
    {
        auto pattern = selectPattern(group, index);

        if (group == "Pad" || group == "Drone")
            pattern.enabled = (index % 4) == 0;
        else if (group == "Voice" || group == "Keys")
            pattern.enabled = (index % 3) == 0;
        else if (group == "Pluck")
            pattern.enabled = (index % 2) == 0;
        else
            pattern.enabled = true;

        if (group == "Lead" && (index % 3) == 1)
            pattern.divisionIndex = 2;
        else if (group == "Percussion")
            pattern.divisionIndex = index % 2 == 0 ? 0 : 1;
        else if (group == "Motion")
            pattern.divisionIndex = 3 + (index % 2);

        if (! pattern.sync)
            pattern.bpm += static_cast<float>((index % 5) * 4);

        applyStepPatternValues(values, pattern);

        const auto arpEnabled = group == "Motion"
            || (group == "Lead" && (index % 3) == 0)
            || (group == "Bass" && (index % 4) == 0)
            || (group == "SFX" && (index % 2) == 0);
        const auto gateEnabled = group == "Motion"
            || group == "Percussion"
            || (group == "Pluck" && (index % 3) == 0)
            || (group == "Bass" && (index % 5) == 0);

        setValue(values, "arpEnabled", arpEnabled ? 1.0f : 0.0f);
        setValue(values, "arpLatch", (group == "Motion" || group == "SFX") && (index % 2 == 0) ? 1.0f : 0.0f);
        setValue(values, "arpMode", static_cast<float>((group == "Bass") ? (index % 2) : (index % 4)));
        setValue(values, "arpRate", 3.5f + static_cast<float>((index % 7) * 1.1f));
        setValue(values, "arpGate", 0.35f + static_cast<float>(index % 4) * 0.1f);
        setValue(values, "arpOctaves", static_cast<float>(1 + (index % ((group == "Bass") ? 2 : 4))));

        setValue(values, "rhythmGateEnabled", gateEnabled ? 1.0f : 0.0f);
        setValue(values, "rhythmGatePattern", static_cast<float>(index % 4));
        setValue(values, "rhythmGateRate", 5.0f + static_cast<float>((index % 6) * 1.5f));
        setValue(values, "rhythmGateDepth", group == "Percussion"
            ? 0.45f + static_cast<float>(index % 4) * 0.12f
            : 0.18f + static_cast<float>(index % 5) * 0.11f);

        setValue(values, "delaySync", 1.0f);
        setValue(values, "delayDivision", static_cast<float>((group == "Drone") ? 7 : ((index + group.length()) % 7 + 1)));
        setValue(values, "delayTime", 0.18f + static_cast<float>(index % 5) * 0.08f);
        setValue(values, "delayMix",
                 group == "Drone" ? 0.16f + static_cast<float>(index % 4) * 0.06f :
                 group == "Pad" ? 0.12f + static_cast<float>(index % 3) * 0.05f :
                 group == "Lead" || group == "SFX" || group == "Motion" ? 0.08f + static_cast<float>(index % 4) * 0.05f :
                 0.02f + static_cast<float>(index % 3) * 0.03f);
        setValue(values, "delayFeedback",
                 group == "Drone" ? 0.48f + static_cast<float>(index % 3) * 0.08f :
                 group == "Motion" || group == "SFX" ? 0.32f + static_cast<float>(index % 4) * 0.08f :
                 0.18f + static_cast<float>(index % 3) * 0.06f);

        setValue(values, "chorusMix",
                 group == "Pad" || group == "Drone" ? 0.24f + static_cast<float>(index % 4) * 0.08f :
                 group == "Keys" || group == "Voice" ? 0.08f + static_cast<float>(index % 4) * 0.05f :
                 static_cast<float>(index % 4 == 0 ? 0.1f : 0.0f));
        setValue(values, "chorusRate", 0.18f + static_cast<float>(index % 5) * 0.18f);
        setValue(values, "chorusDepth",
                 group == "Pad" || group == "Drone" ? 0.4f + static_cast<float>(index % 4) * 0.1f :
                 0.2f + static_cast<float>(index % 4) * 0.08f);

        setValue(values, "distortionAmount",
                 group == "Percussion" || group == "SFX" ? 0.14f + static_cast<float>(index % 4) * 0.14f :
                 group == "Bass" || group == "Lead" ? 0.06f + static_cast<float>(index % 5) * 0.06f :
                 static_cast<float>(index % 5 == 0 ? 0.08f : 0.0f));
        setValue(values, "saturationAmount",
                 group == "Bass" ? 0.18f + static_cast<float>(index % 4) * 0.08f :
                 group == "Lead" || group == "Keys" ? 0.08f + static_cast<float>(index % 4) * 0.05f :
                 group == "Pad" || group == "Drone" ? 0.1f + static_cast<float>(index % 3) * 0.04f :
                 0.04f + static_cast<float>(index % 3) * 0.04f);

        setValue(values, "compressorThreshold",
                 group == "Percussion" ? -18.0f - static_cast<float>(index % 4) * 3.0f :
                 group == "Bass" ? -24.0f + static_cast<float>(index % 4) * 2.0f :
                 -20.0f + static_cast<float>(index % 4));
        setValue(values, "compressorRatio",
                 group == "Percussion" ? 4.0f + static_cast<float>(index % 4) * 1.5f :
                 group == "Bass" ? 3.0f + static_cast<float>(index % 4) :
                 2.0f + static_cast<float>(index % 4));

        setValue(values, "reverbMix",
                 group == "Pad" || group == "Drone" ? 0.26f + static_cast<float>(index % 4) * 0.08f :
                 group == "Voice" || group == "Keys" ? 0.1f + static_cast<float>(index % 4) * 0.05f :
                 group == "SFX" ? 0.18f + static_cast<float>(index % 4) * 0.08f :
                 0.03f + static_cast<float>(index % 4) * 0.04f);
        setValue(values, "reverbSize",
                 group == "Pad" || group == "Drone" || group == "SFX" ? 0.48f + static_cast<float>(index % 4) * 0.1f :
                 0.28f + static_cast<float>(index % 4) * 0.06f);
        setValue(values, "reverbDamping", 0.26f + static_cast<float>(index % 5) * 0.08f);

        setValue(values, "bitcrusherMix",
                 group == "SFX" ? 0.12f + static_cast<float>(index % 5) * 0.1f :
                 group == "Percussion" ? (index % 2 == 0 ? 0.08f + static_cast<float>(index % 4) * 0.05f : 0.0f) :
                 group == "Bass" && (index % 5 == 0) ? 0.12f : 0.0f);
        setValue(values, "bitDepth",
                 group == "SFX" ? 5.0f + static_cast<float>(index % 4) :
                 group == "Percussion" ? 7.0f + static_cast<float>(index % 4) :
                 8.0f + static_cast<float>(index % 4));
        setValue(values, "bitDownsample",
                 group == "SFX" ? 4.0f + static_cast<float>(index % 6) :
                 group == "Percussion" ? 2.0f + static_cast<float>(index % 4) :
                 2.0f + static_cast<float>(index % 3));

        return pattern.name;
    };

    auto addSeries = [&] (const juce::String& group, Values base, auto&& vary)
    {
        for (int index = 0; index < variantNames.size(); ++index)
        {
            auto values = base;
            vary(index, values);
            const auto patternName = applyFactoryScene(group, index, values);
            presets.push_back({ group + " / " + variantNames[index] + " " + juce::String(index + 1).paddedLeft('0', 2),
                                std::move(values),
                                patternName });
        }
    };

    addSeries("Bass",
              {
                  { "keyMode", 1.0f }, { "voiceCount", 1.0f }, { "portamentoMode", 2.0f }, { "portamentoTime", 0.08f },
                  { "velocitySensitivity", 0.56f }, { "velocityDestination", 2.0f }, { "velocityAmount", 0.18f },
                  { "osc1Wave", 1.0f }, { "osc1Level", 0.86f }, { "osc1PulseWidth", 0.52f }, { "osc1PwmAmount", 0.14f }, { "osc1PwmSource", 1.0f },
                  { "osc2Wave", 2.0f }, { "osc2Level", 0.5f }, { "osc2Coarse", -12.0f }, { "osc2PulseWidth", 0.42f }, { "osc2PwmAmount", 0.2f }, { "osc2PwmSource", 2.0f }, { "osc2SyncSource", 1.0f },
                  { "osc3Wave", 3.0f }, { "osc3Level", 0.24f }, { "osc3Coarse", -12.0f }, { "osc3Fine", 5.0f }, { "osc3PulseWidth", 0.6f },
                  { "subWave", 1.0f }, { "subLevel", 0.42f }, { "subOctave", -2.0f },
                  { "cutoff", 420.0f }, { "resonance", 0.42f }, { "drive", 0.38f }, { "env1ToFilter", 0.72f }, { "env2ToFilter", 0.2f },
                  { "env1Attack", 0.004f }, { "env1Decay", 0.16f }, { "env1Sustain", 0.58f }, { "env1Release", 0.14f },
                  { "env2Attack", 0.002f }, { "env2Decay", 0.14f }, { "env2Sustain", 0.0f }, { "env2Release", 0.12f }, { "env2ToPitch", 1.5f },
                  { "lfo1Rate", 1.4f }, { "lfo1Depth", 0.12f }, { "lfo2Rate", 0.32f }, { "lfo2Depth", 0.08f },
                  { "saturationAmount", 0.26f }, { "compressorThreshold", -23.0f }, { "compressorRatio", 4.0f }, { "reverbMix", 0.04f }
              },
              [&] (int index, Values& values)
              {
                  setValue(values, "osc1Wave", static_cast<float>((index + 1) % 4));
                  setValue(values, "osc2Wave", static_cast<float>((index % 3) + 1));
                  setValue(values, "osc3Wave", static_cast<float>((index + 2) % 4));
                  setValue(values, "cutoff", 220.0f + static_cast<float>((index % 5) * 170));
                  setValue(values, "resonance", 0.34f + static_cast<float>(index % 4) * 0.09f);
                  setValue(values, "drive", 0.22f + static_cast<float>(index % 6) * 0.07f);
                  setValue(values, "subLevel", 0.28f + static_cast<float>(index % 5) * 0.06f);
                  setValue(values, "unisonVoices", static_cast<float>(1 + (index % 3)));
                  setValue(values, "unisonDetune", 4.0f + static_cast<float>(index % 5) * 2.0f);
                  setValue(values, "bitcrusherMix", index % 5 == 0 ? 0.12f : 0.0f);
                  setValue(values, "bitDepth", 6.0f + static_cast<float>(index % 5));
                  setValue(values, "chorusMix", index % 4 == 0 ? 0.08f : 0.0f);
              });

    addSeries("Lead",
              {
                  { "keyMode", 1.0f }, { "voiceCount", 1.0f }, { "portamentoMode", 1.0f }, { "portamentoTime", 0.14f },
                  { "velocitySensitivity", 0.68f }, { "velocityDestination", 2.0f }, { "velocityAmount", 0.24f },
                  { "osc1Wave", 1.0f }, { "osc1Level", 0.74f }, { "osc1PulseWidth", 0.5f }, { "osc1PwmAmount", 0.28f }, { "osc1PwmSource", 1.0f },
                  { "osc2Wave", 2.0f }, { "osc2Level", 0.56f }, { "osc2Coarse", 12.0f }, { "osc2PulseWidth", 0.44f }, { "osc2PwmAmount", 0.22f }, { "osc2PwmSource", 3.0f }, { "osc2SyncSource", 1.0f },
                  { "osc3Wave", 1.0f }, { "osc3Level", 0.3f }, { "osc3Coarse", 7.0f }, { "osc3Fine", -4.0f },
                  { "subLevel", 0.1f },
                  { "cutoff", 7600.0f }, { "resonance", 0.5f }, { "drive", 0.2f }, { "env1ToFilter", 0.4f }, { "env2ToPitch", 4.0f },
                  { "env1Attack", 0.008f }, { "env1Decay", 0.18f }, { "env1Sustain", 0.62f }, { "env1Release", 0.22f },
                  { "env2Attack", 0.003f }, { "env2Decay", 0.2f }, { "env2Sustain", 0.0f }, { "env2Release", 0.1f },
                  { "lfo1Rate", 5.2f }, { "lfo1Depth", 0.14f }, { "lfo3Rate", 6.5f }, { "lfo3Depth", 0.08f },
                  { "chorusMix", 0.14f }, { "reverbMix", 0.18f }, { "reverbSize", 0.42f }
              },
              [&] (int index, Values& values)
              {
                  setValue(values, "osc1Wave", static_cast<float>((index % 4) == 0 ? 2 : 1));
                  setValue(values, "osc2SyncSource", static_cast<float>((index % 2) + 1));
                  setValue(values, "osc3Coarse", static_cast<float>((index % 4) * 5 + 7));
                  setValue(values, "cutoff", 5200.0f + static_cast<float>((index % 6) * 1400));
                  setValue(values, "drive", 0.12f + static_cast<float>(index % 5) * 0.05f);
                  setValue(values, "lfo1Rate", 3.0f + static_cast<float>(index % 7) * 0.8f);
                  setValue(values, "lfo1Depth", 0.08f + static_cast<float>(index % 5) * 0.04f);
                  setValue(values, "matrixSource1", 1.0f);
                  setValue(values, "matrixDest1", 1.0f);
                  setValue(values, "matrixAmount1", 0.04f + static_cast<float>(index % 4) * 0.03f);
                  setValue(values, "reverbMix", 0.1f + static_cast<float>(index % 5) * 0.04f);
              });

    addSeries("Pad",
              {
                  { "osc1Wave", 3.0f }, { "osc1Level", 0.62f }, { "osc1PwmAmount", 0.2f }, { "osc1PwmSource", 2.0f },
                  { "osc2Wave", 1.0f }, { "osc2Level", 0.48f }, { "osc2Fine", -8.0f }, { "osc2PwmAmount", 0.16f }, { "osc2PwmSource", 1.0f },
                  { "osc3Wave", 3.0f }, { "osc3Level", 0.34f }, { "osc3Fine", 8.0f }, { "osc3PwmAmount", 0.18f }, { "osc3PwmSource", 3.0f },
                  { "subWave", 0.0f }, { "subLevel", 0.14f },
                  { "cutoff", 4200.0f }, { "resonance", 0.24f }, { "env1ToFilter", 0.2f }, { "env2ToFilter", 0.2f },
                  { "env1Attack", 0.55f }, { "env1Decay", 1.0f }, { "env1Sustain", 0.82f }, { "env1Release", 2.8f },
                  { "env2Attack", 0.18f }, { "env2Decay", 2.2f }, { "env2Sustain", 0.58f }, { "env2Release", 2.4f },
                  { "lfo1Rate", 0.26f }, { "lfo1Depth", 0.28f }, { "lfo2Rate", 0.1f }, { "lfo2Depth", 0.2f },
                  { "chorusMix", 0.42f }, { "chorusDepth", 0.58f }, { "reverbMix", 0.42f }, { "reverbSize", 0.72f },
                  { "unisonVoices", 5.0f }, { "unisonDetune", 12.0f }, { "stereoWidth", 0.88f }
              },
              [&] (int index, Values& values)
              {
                  setValue(values, "osc1Wave", static_cast<float>((index + 3) % 4));
                  setValue(values, "osc2Wave", static_cast<float>((index + 1) % 4));
                  setValue(values, "cutoff", 1800.0f + static_cast<float>((index % 6) * 700));
                  setValue(values, "env1Attack", 0.18f + static_cast<float>(index % 5) * 0.18f);
                  setValue(values, "env1Release", 1.8f + static_cast<float>(index % 6) * 0.42f);
                  setValue(values, "lfo2Depth", 0.12f + static_cast<float>(index % 5) * 0.06f);
                  setValue(values, "reverbMix", 0.22f + static_cast<float>(index % 5) * 0.07f);
                  setValue(values, "chorusMix", 0.16f + static_cast<float>(index % 5) * 0.06f);
                  setValue(values, "unisonVoices", static_cast<float>(4 + (index % 3)));
              });

    addSeries("Pluck",
              {
                  { "osc1Wave", 2.0f }, { "osc1Level", 0.72f }, { "osc1PulseWidth", 0.3f }, { "osc1PwmAmount", 0.24f }, { "osc1PwmSource", 1.0f },
                  { "osc2Wave", 2.0f }, { "osc2Level", 0.34f }, { "osc2PulseWidth", 0.62f }, { "osc2Coarse", 12.0f }, { "osc2PwmAmount", 0.18f },
                  { "osc3Wave", 1.0f }, { "osc3Level", 0.18f }, { "subLevel", 0.2f },
                  { "cutoff", 2400.0f }, { "resonance", 0.62f }, { "drive", 0.16f }, { "env1ToFilter", 0.78f },
                  { "env1Attack", 0.001f }, { "env1Decay", 0.18f }, { "env1Sustain", 0.0f }, { "env1Release", 0.16f },
                  { "env2Attack", 0.001f }, { "env2Decay", 0.14f }, { "env2Sustain", 0.0f }, { "env2Release", 0.12f }, { "env2ToPitch", 2.0f },
                  { "reverbMix", 0.14f }, { "chorusMix", 0.08f }, { "bitcrusherMix", 0.06f }, { "bitDepth", 9.0f }
              },
              [&] (int index, Values& values)
              {
                  setValue(values, "cutoff", 1200.0f + static_cast<float>((index % 8) * 520));
                  setValue(values, "resonance", 0.38f + static_cast<float>(index % 5) * 0.08f);
                  setValue(values, "drive", 0.08f + static_cast<float>(index % 4) * 0.06f);
                  setValue(values, "env2ToPitch", index % 2 == 0 ? 0.0f : static_cast<float>(2 + (index % 4) * 2));
                  setValue(values, "reverbMix", 0.08f + static_cast<float>(index % 5) * 0.04f);
                  setValue(values, "bitcrusherMix", index % 3 == 0 ? 0.12f : 0.0f);
                  setValue(values, "osc2Coarse", index % 4 == 0 ? 7.0f : 12.0f);
              });

    addSeries("Keys",
              {
                  { "voiceCount", 8.0f }, { "velocitySensitivity", 0.82f }, { "velocityDestination", 4.0f }, { "velocityAmount", 0.1f },
                  { "osc1Wave", 1.0f }, { "osc1Level", 0.6f }, { "osc1PwmAmount", 0.12f }, { "osc1PwmSource", 2.0f },
                  { "osc2Wave", 3.0f }, { "osc2Level", 0.38f }, { "osc2Fine", 6.0f },
                  { "osc3Wave", 0.0f }, { "osc3Level", 0.14f }, { "subLevel", 0.1f },
                  { "cutoff", 5400.0f }, { "resonance", 0.3f }, { "env1ToFilter", 0.34f },
                  { "env1Attack", 0.01f }, { "env1Decay", 0.26f }, { "env1Sustain", 0.52f }, { "env1Release", 0.35f },
                  { "env2Attack", 0.01f }, { "env2Decay", 0.18f }, { "env2Sustain", 0.0f }, { "env2Release", 0.24f },
                  { "chorusMix", 0.16f }, { "reverbMix", 0.14f }, { "reverbSize", 0.36f }, { "compressorThreshold", -20.0f }
              },
              [&] (int index, Values& values)
              {
                  setValue(values, "osc1Wave", static_cast<float>(index % 4));
                  setValue(values, "osc2Wave", static_cast<float>((index + 2) % 4));
                  setValue(values, "cutoff", 3200.0f + static_cast<float>((index % 7) * 680));
                  setValue(values, "chorusMix", 0.04f + static_cast<float>(index % 5) * 0.06f);
                  setValue(values, "reverbMix", 0.08f + static_cast<float>(index % 5) * 0.05f);
                  setValue(values, "compressorRatio", 2.0f + static_cast<float>(index % 4));
                  setValue(values, "stereoWidth", 0.35f + static_cast<float>(index % 5) * 0.1f);
              });

    addSeries("Percussion",
              {
                  { "osc1Wave", 2.0f }, { "osc1Level", 0.74f }, { "osc1PulseWidth", 0.22f },
                  { "osc2Wave", 4.0f }, { "osc2Level", 0.28f }, { "osc2Coarse", 12.0f },
                  { "osc3Wave", 0.0f }, { "osc3Level", 0.12f }, { "subLevel", 0.18f },
                  { "filterType", 1.0f }, { "cutoff", 2800.0f }, { "resonance", 0.72f }, { "drive", 0.18f }, { "env1ToFilter", 0.9f },
                  { "env1Attack", 0.001f }, { "env1Decay", 0.11f }, { "env1Sustain", 0.0f }, { "env1Release", 0.09f },
                  { "env2Attack", 0.001f }, { "env2Decay", 0.08f }, { "env2Sustain", 0.0f }, { "env2Release", 0.06f }, { "env2ToPitch", 7.0f },
                  { "distortionAmount", 0.14f }, { "compressorThreshold", -18.0f }, { "compressorRatio", 5.0f }, { "bitcrusherMix", 0.08f }
              },
              [&] (int index, Values& values)
              {
                  setValue(values, "filterType", static_cast<float>(index % 3));
                  setValue(values, "cutoff", 800.0f + static_cast<float>((index % 8) * 780));
                  setValue(values, "resonance", 0.4f + static_cast<float>(index % 6) * 0.12f);
                  setValue(values, "env1Decay", 0.04f + static_cast<float>(index % 5) * 0.03f);
                  setValue(values, "env2ToPitch", 4.0f + static_cast<float>(index % 6) * 2.5f);
                  setValue(values, "bitcrusherMix", index % 2 == 0 ? 0.1f + static_cast<float>(index % 4) * 0.04f : 0.0f);
                  setValue(values, "reverbMix", index % 3 == 0 ? 0.12f : 0.02f);
              });

    addSeries("SFX",
              {
                  { "osc1Wave", 4.0f }, { "osc1Level", 0.56f }, { "osc1PwmAmount", 0.22f }, { "osc1PwmSource", 3.0f },
                  { "osc2Wave", 4.0f }, { "osc2Level", 0.34f }, { "osc2Coarse", 12.0f }, { "osc2SyncSource", 1.0f },
                  { "osc3Wave", 1.0f }, { "osc3Level", 0.18f }, { "osc3Coarse", -12.0f },
                  { "subWave", 0.0f }, { "subLevel", 0.12f },
                  { "filterType", 1.0f }, { "cutoff", 3200.0f }, { "resonance", 0.64f }, { "drive", 0.32f }, { "env1ToFilter", 0.46f }, { "env2ToFilter", 0.6f }, { "env2ToPitch", 10.0f },
                  { "env1Attack", 0.001f }, { "env1Decay", 0.38f }, { "env1Sustain", 0.18f }, { "env1Release", 0.45f },
                  { "env2Attack", 0.001f }, { "env2Decay", 0.4f }, { "env2Sustain", 0.0f }, { "env2Release", 0.36f },
                  { "lfo1Rate", 7.2f }, { "lfo1Depth", 0.26f }, { "lfo2Rate", 0.5f }, { "lfo2Depth", 0.22f }, { "lfo3Rate", 9.0f }, { "lfo3Depth", 0.22f },
                  { "reverbMix", 0.36f }, { "reverbSize", 0.74f }, { "bitcrusherMix", 0.18f }, { "bitDepth", 7.0f }, { "bitDownsample", 6.0f }
              },
              [&] (int index, Values& values)
              {
                  setValue(values, "osc1Wave", static_cast<float>(index % 5));
                  setValue(values, "osc2Wave", static_cast<float>((index + 2) % 5));
                  setValue(values, "osc3Wave", static_cast<float>((index + 4) % 5));
                  setValue(values, "filterType", static_cast<float>(index % 3));
                  setValue(values, "cutoff", 400.0f + static_cast<float>((index % 9) * 1400));
                  setValue(values, "env2ToPitch", 2.0f + static_cast<float>(index % 7) * 3.0f);
                  setValue(values, "bitcrusherMix", 0.08f + static_cast<float>(index % 5) * 0.08f);
                  setValue(values, "distortionAmount", 0.12f + static_cast<float>(index % 4) * 0.12f);
                  setValue(values, "matrixSource1", static_cast<float>(1 + (index % 3)));
                  setValue(values, "matrixDest1", static_cast<float>(5 + (index % 5)));
                  setValue(values, "matrixAmount1", 0.16f + static_cast<float>(index % 4) * 0.12f);
              });

    addSeries("Drone",
              {
                  { "osc1Wave", 0.0f }, { "osc1Level", 0.5f },
                  { "osc2Wave", 1.0f }, { "osc2Level", 0.42f }, { "osc2Coarse", -12.0f }, { "osc2Fine", -6.0f },
                  { "osc3Wave", 4.0f }, { "osc3Level", 0.18f },
                  { "subWave", 0.0f }, { "subLevel", 0.3f }, { "subOctave", -2.0f },
                  { "cutoff", 1200.0f }, { "resonance", 0.34f }, { "drive", 0.28f }, { "env1ToFilter", 0.14f }, { "env2ToFilter", 0.44f },
                  { "env1Attack", 0.9f }, { "env1Decay", 1.5f }, { "env1Sustain", 0.74f }, { "env1Release", 3.8f },
                  { "env2Attack", 0.12f }, { "env2Decay", 2.6f }, { "env2Sustain", 0.56f }, { "env2Release", 3.4f },
                  { "lfo1Rate", 0.18f }, { "lfo1Depth", 0.24f }, { "lfo2Rate", 0.08f }, { "lfo2Depth", 0.28f }, { "lfo3Rate", 5.8f }, { "lfo3Depth", 0.08f },
                  { "chorusMix", 0.24f }, { "reverbMix", 0.46f }, { "reverbSize", 0.8f }, { "reverbDamping", 0.28f }
              },
              [&] (int index, Values& values)
              {
                  setValue(values, "cutoff", 240.0f + static_cast<float>((index % 8) * 360));
                  setValue(values, "env1Attack", 0.24f + static_cast<float>(index % 6) * 0.2f);
                  setValue(values, "env1Release", 2.0f + static_cast<float>(index % 7) * 0.44f);
                  setValue(values, "lfo2Depth", 0.12f + static_cast<float>(index % 5) * 0.08f);
                  setValue(values, "chorusMix", 0.1f + static_cast<float>(index % 5) * 0.06f);
                  setValue(values, "bitcrusherMix", index % 4 == 0 ? 0.08f : 0.0f);
                  setValue(values, "filterType", static_cast<float>(index % 3 == 0 ? 1 : 0));
              });

    addSeries("Voice",
              {
                  { "osc1Wave", 0.0f }, { "osc1Level", 0.62f }, { "osc1PwmAmount", 0.1f }, { "osc1PwmSource", 2.0f },
                  { "osc2Wave", 3.0f }, { "osc2Level", 0.28f }, { "osc2Fine", 7.0f },
                  { "osc3Wave", 0.0f }, { "osc3Level", 0.16f }, { "osc3Coarse", 12.0f },
                  { "filterType", 1.0f }, { "cutoff", 2200.0f }, { "resonance", 0.62f }, { "env1ToFilter", 0.24f }, { "env2ToFilter", 0.36f },
                  { "env1Attack", 0.02f }, { "env1Decay", 0.36f }, { "env1Sustain", 0.5f }, { "env1Release", 0.44f },
                  { "env2Attack", 0.01f }, { "env2Decay", 0.52f }, { "env2Sustain", 0.22f }, { "env2Release", 0.4f },
                  { "lfo1Rate", 5.0f }, { "lfo1Depth", 0.08f }, { "chorusMix", 0.12f }, { "reverbMix", 0.22f }
              },
              [&] (int index, Values& values)
              {
                  setValue(values, "filterType", static_cast<float>((index % 2) + 1));
                  setValue(values, "cutoff", 1200.0f + static_cast<float>((index % 7) * 340));
                  setValue(values, "resonance", 0.42f + static_cast<float>(index % 5) * 0.1f);
                  setValue(values, "lfo1Rate", 4.0f + static_cast<float>(index % 6));
                  setValue(values, "matrixSource1", 6.0f);
                  setValue(values, "matrixDest1", 2.0f);
                  setValue(values, "matrixAmount1", 0.08f + static_cast<float>(index % 4) * 0.06f);
                  setValue(values, "reverbMix", 0.12f + static_cast<float>(index % 5) * 0.05f);
              });

    addSeries("Motion",
              {
                  { "osc1Wave", 1.0f }, { "osc1Level", 0.72f }, { "osc1PwmAmount", 0.22f }, { "osc1PwmSource", 1.0f },
                  { "osc2Wave", 3.0f }, { "osc2Level", 0.38f }, { "osc2Fine", -5.0f }, { "osc2PwmAmount", 0.2f }, { "osc2PwmSource", 2.0f },
                  { "osc3Wave", 2.0f }, { "osc3Level", 0.2f }, { "osc3PwmAmount", 0.16f }, { "osc3PwmSource", 3.0f },
                  { "subLevel", 0.12f }, { "cutoff", 5200.0f }, { "env1ToFilter", 0.34f },
                  { "env1Attack", 0.005f }, { "env1Decay", 0.22f }, { "env1Sustain", 0.48f }, { "env1Release", 0.2f },
                  { "lfo1Rate", 4.0f }, { "lfo1Depth", 0.16f }, { "lfo2Rate", 0.42f }, { "lfo2Depth", 0.22f }, { "lfo3Rate", 6.2f }, { "lfo3Depth", 0.14f },
                  { "arpEnabled", 1.0f }, { "arpMode", 0.0f }, { "arpRate", 7.5f }, { "arpGate", 0.48f }, { "arpOctaves", 2.0f },
                  { "rhythmGateEnabled", 1.0f }, { "rhythmGateRate", 9.0f }, { "rhythmGateDepth", 0.42f }, { "rhythmGatePattern", 2.0f },
                  { "chorusMix", 0.18f }, { "reverbMix", 0.22f }, { "delayMix", 0.0f }
              },
              [&] (int index, Values& values)
              {
                  setValue(values, "arpMode", static_cast<float>(index % 4));
                  setValue(values, "arpRate", 4.0f + static_cast<float>(index % 8) * 1.2f);
                  setValue(values, "arpOctaves", static_cast<float>(1 + (index % 4)));
                  setValue(values, "rhythmGatePattern", static_cast<float>(index % 4));
                  setValue(values, "rhythmGateDepth", 0.2f + static_cast<float>(index % 5) * 0.14f);
                  setValue(values, "matrixSource1", static_cast<float>(1 + (index % 3)));
                  setValue(values, "matrixDest1", static_cast<float>(1 + (index % 4)));
                  setValue(values, "matrixAmount1", 0.1f + static_cast<float>(index % 5) * 0.08f);
                  setValue(values, "reverbMix", 0.08f + static_cast<float>(index % 5) * 0.04f);
              });

    return presets;
}

std::vector<AISynthAudioProcessor::PatternDefinition> AISynthAudioProcessor::createFactoryPatterns()
{
    std::vector<PatternDefinition> patterns;
    const auto templates = createStepPatternTemplates();
    patterns.reserve(templates.size());

    for (const auto& patternTemplate : templates)
    {
        ParameterValues values;
        applyStepPatternValues(values, patternTemplate);
        patterns.push_back({ patternTemplate.name, std::move(values) });
    }

    return patterns;
}

void AISynthAudioProcessor::refreshPresetList()
{
    presetEntries.clear();
    presetNames.clear();

    for (const auto& preset : factoryPresets)
    {
        presetEntries.push_back({ preset.name, true, {} });
        presetNames.add(preset.name);
    }

    auto directory = getPresetDirectory();
    directory.createDirectory();

    juce::Array<juce::File> userPresetFiles;
    directory.findChildFiles(userPresetFiles, juce::File::findFiles, false, "*.xml");

    for (const auto& file : userPresetFiles)
    {
        juce::String name = file.getFileNameWithoutExtension();
        if (auto xml = juce::XmlDocument::parse(file))
        {
            auto state = juce::ValueTree::fromXml(*xml);
            if (state.isValid())
            {
                const auto displayName = state.getProperty("displayName").toString();
                const auto storedName = state.getProperty("currentPresetName").toString();
                name = displayName.isNotEmpty() ? displayName : (storedName.isNotEmpty() ? storedName : name);
            }
        }

        presetEntries.push_back({ name, false, file });
        presetNames.add(name);
    }

    if (currentPresetName.isNotEmpty() && ! presetNames.contains(currentPresetName))
        currentPresetName = presetNames.isEmpty() ? "Init" : presetNames[0];
}

void AISynthAudioProcessor::refreshPatternList()
{
    patternEntries.clear();
    patternNames.clear();

    for (const auto& pattern : factoryPatterns)
    {
        patternEntries.push_back({ pattern.name, true, {} });
        patternNames.add(pattern.name);
    }

    auto directory = getPatternDirectory();
    directory.createDirectory();

    juce::Array<juce::File> userPatternFiles;
    directory.findChildFiles(userPatternFiles, juce::File::findFiles, false, "*.xml");

    for (const auto& file : userPatternFiles)
    {
        juce::String name = file.getFileNameWithoutExtension();
        if (auto xml = juce::XmlDocument::parse(file))
        {
            auto state = juce::ValueTree::fromXml(*xml);
            if (state.isValid())
                name = state.getProperty("displayName", name).toString();
        }

        patternEntries.push_back({ name, false, file });
        patternNames.add(name);
    }

    if (currentPatternName.isNotEmpty() && ! patternNames.contains(currentPatternName))
        currentPatternName = patternNames.isEmpty() ? "Init Pattern" : patternNames[0];
}

bool AISynthAudioProcessor::loadPresetByName(const juce::String& presetName)
{
    for (const auto& factoryPreset : factoryPresets)
    {
        if (factoryPreset.name == presetName)
        {
            applyFactoryPreset(factoryPreset);
            return true;
        }
    }

    for (const auto& preset : presetEntries)
    {
        if (! preset.isFactory && preset.name == presetName)
            return loadPresetFile(preset.file);
    }

    return false;
}

bool AISynthAudioProcessor::loadPatternByName(const juce::String& patternName)
{
    for (const auto& factoryPattern : factoryPatterns)
    {
        if (factoryPattern.name == patternName)
        {
            applyPatternDefinition(factoryPattern);
            return true;
        }
    }

    for (const auto& pattern : patternEntries)
    {
        if (! pattern.isFactory && pattern.name == patternName)
            return loadPatternFile(pattern.file);
    }

    return false;
}

bool AISynthAudioProcessor::saveUserPreset(const juce::String& presetName)
{
    const auto trimmedName = presetName.trim();
    if (trimmedName.isEmpty())
        return false;

    auto directory = getPresetDirectory();
    if (! directory.exists() && ! directory.createDirectory())
        return false;

    const auto safeName = juce::File::createLegalFileName(trimmedName);
    auto file = directory.getChildFile(safeName + ".xml");

    auto state = apvts.copyState();
    state.setProperty("currentPresetName", trimmedName, nullptr);
    state.setProperty("displayName", trimmedName, nullptr);
    state.setProperty("themeIndex", currentThemeIndex, nullptr);
    state.setProperty("currentPatternName", currentPatternName, nullptr);

    std::vector<MidiMapping> mappingsCopy;
    {
        const juce::SpinLock::ScopedLockType lock(midiMappingLock);
        mappingsCopy = currentMidiMappings;
    }

    auto existingMappings = state.getChildWithName("MIDI_MAPPINGS");
    if (existingMappings.isValid())
        state.removeChild(existingMappings, nullptr);
    state.appendChild(createMidiMappingsState(trimmedName, mappingsCopy), nullptr);

    auto xml = state.createXml();
    if (xml == nullptr)
        return false;

    if (! file.replaceWithText(xml->toString()))
        return false;

    currentPresetName = trimmedName;
    queueMidiMappingSave(currentPresetName);
    refreshPresetList();
    return true;
}

bool AISynthAudioProcessor::saveSequencerPattern(const juce::String& patternName)
{
    const auto trimmedName = patternName.trim();
    if (trimmedName.isEmpty())
        return false;

    auto directory = getPatternDirectory();
    if (! directory.exists() && ! directory.createDirectory())
        return false;

    auto state = juce::ValueTree("STEP_PATTERN");
    state.setProperty("displayName", trimmedName, nullptr);
    state.setProperty("currentPatternName", trimmedName, nullptr);

    const auto setPatternProperty = [this, &state] (const juce::String& id)
    {
        state.setProperty(id, getParam(id), nullptr);
    };

    setPatternProperty("stepSeqEnabled");
    setPatternProperty("stepSeqSync");
    setPatternProperty("stepSeqBpm");
    setPatternProperty("stepSeqSteps");
    setPatternProperty("stepSeqLength");

    for (int step = 1; step <= 8; ++step)
    {
        const auto stepId = juce::String(step);
        setPatternProperty("seqStep" + stepId + "Note");
        setPatternProperty("seqStep" + stepId + "Gate");
        setPatternProperty("seqStep" + stepId + "Velocity");
    }

    auto file = directory.getChildFile(juce::File::createLegalFileName(trimmedName) + ".xml");
    auto xml = state.createXml();
    if (xml == nullptr)
        return false;

    if (! file.replaceWithText(xml->toString()))
        return false;

    currentPatternName = trimmedName;
    refreshPatternList();
    return true;
}

void AISynthAudioProcessor::beginMidiLearn(const juce::String& parameterId)
{
    const juce::SpinLock::ScopedLockType lock(midiMappingLock);
    pendingMidiLearnParameterId = parameterId;
}

void AISynthAudioProcessor::cancelMidiLearn()
{
    const juce::SpinLock::ScopedLockType lock(midiMappingLock);
    pendingMidiLearnParameterId.clear();
}

bool AISynthAudioProcessor::clearMidiMappingsForCurrentPreset()
{
    if (currentPresetName.trim().isEmpty())
        return false;

    {
        const juce::SpinLock::ScopedLockType lock(midiMappingLock);
        currentMidiMappings.clear();
        pendingMidiLearnParameterId.clear();
    }

    queueMidiMappingSave(currentPresetName);
    return true;
}

juce::String AISynthAudioProcessor::getMidiLearnTargetParameter() const
{
    const juce::SpinLock::ScopedLockType lock(midiMappingLock);
    return pendingMidiLearnParameterId;
}

bool AISynthAudioProcessor::isMidiLearnActiveFor(const juce::String& parameterId) const
{
    const juce::SpinLock::ScopedLockType lock(midiMappingLock);
    return pendingMidiLearnParameterId == parameterId;
}

juce::String AISynthAudioProcessor::getMidiMappingDescription(const juce::String& parameterId) const
{
    const juce::SpinLock::ScopedLockType lock(midiMappingLock);
    for (const auto& mapping : currentMidiMappings)
        if (mapping.parameterId == parameterId)
            return "CC " + juce::String(mapping.controllerNumber);

    return {};
}

void AISynthAudioProcessor::setCurrentThemeIndex(int newThemeIndex) noexcept
{
    currentThemeIndex = juce::jlimit(0, 4, newThemeIndex);
}

void AISynthAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("currentPresetName", currentPresetName, nullptr);
    state.setProperty("themeIndex", currentThemeIndex, nullptr);
    state.setProperty("currentPatternName", currentPatternName, nullptr);

    std::vector<MidiMapping> mappingsCopy;
    {
        const juce::SpinLock::ScopedLockType lock(midiMappingLock);
        mappingsCopy = currentMidiMappings;
    }

    auto existingMappings = state.getChildWithName("MIDI_MAPPINGS");
    if (existingMappings.isValid())
        state.removeChild(existingMappings, nullptr);
    state.appendChild(createMidiMappingsState(currentPresetName, mappingsCopy), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void AISynthAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml(*xml);
        apvts.replaceState(state);

        const auto loadedPresetName = state.getProperty("currentPresetName").toString();
        currentPresetName = loadedPresetName.isNotEmpty() ? loadedPresetName : "Init";
        setCurrentThemeIndex(static_cast<int>(state.getProperty("themeIndex", currentThemeIndex)));
        currentPatternName = state.getProperty("currentPatternName", currentPatternName).toString();
        auto embeddedMappings = state.getChildWithName("MIDI_MAPPINGS");
        loadMidiMappingsForCurrentPreset(embeddedMappings.isValid() ? &embeddedMappings : nullptr);
        refreshPatternList();
        refreshPresetList();
    }
}

void AISynthAudioProcessor::handleAsyncUpdate()
{
    MidiMappingSnapshot snapshot;
    {
        const juce::SpinLock::ScopedLockType lock(midiMappingLock);
        snapshot = midiMappingSnapshotToPersist;
    }

    const auto trimmedName = snapshot.presetName.trim();
    if (trimmedName.isEmpty())
        return;

    auto directory = getMidiMappingDirectory();
    if (! directory.exists() && ! directory.createDirectory())
        return;

    auto file = directory.getChildFile(juce::File::createLegalFileName(trimmedName) + ".xml");
    auto state = createMidiMappingsState(trimmedName, snapshot.mappings);
    if (auto xml = state.createXml())
        file.replaceWithText(xml->toString());
}

juce::AudioProcessorEditor* AISynthAudioProcessor::createEditor()
{
    return new AISynthAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AISynthAudioProcessor();
}
