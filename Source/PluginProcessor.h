#pragma once

#include <JuceHeader.h>

class SynthVoice;
class AISynthAudioProcessor;

class AISynthesiser final : public juce::Synthesiser
{
public:
    AISynthesiser() = default;

    void setPlayMode(int newKeyMode, int newVoiceCount);
    void resetPlayState();

protected:
    void noteOn(int midiChannel, int midiNoteNumber, float velocity) override;
    void noteOff(int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff) override;
    SynthesiserVoice* findFreeVoice(SynthesiserSound* soundToPlay,
                                    int midiChannel,
                                    int midiNoteNumber,
                                    bool stealIfNoneAvailable) const override;
    SynthesiserVoice* findVoiceToSteal(SynthesiserSound* soundToPlay,
                                       int midiChannel,
                                       int midiNoteNumber) const override;

private:
    struct MonoHeldNote
    {
        int midiNote { 60 };
        int midiChannel { 1 };
        float velocity { 0.8f };
    };

    void startMonoVoice(int midiChannel, int midiNoteNumber, float velocity);
    void trimVoices();

    int keyMode { 0 };
    int maxPlayableVoices { 16 };
    std::vector<MonoHeldNote> monoHeldNotes;
    int currentMonoNote { -1 };
    int currentMonoChannel { 1 };
};

class AISynthAudioProcessor final : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    AISynthAudioProcessor();
    ~AISynthAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override { juce::ignoreUnused(index); }
    const juce::String getProgramName(int index) override { juce::ignoreUnused(index); return {}; }
    void changeProgramName(int index, const juce::String& name) override { juce::ignoreUnused(index, name); }

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    float getParam(const juce::String& id) const;
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }
    int getCurrentPitchWheelValue() const noexcept { return currentPitchWheelValue.load(); }
    float getCurrentModWheelValue() const noexcept { return currentModWheelValue.load(); }
    int getCurrentSequencerStep() const noexcept { return currentSequencerStep.load(); }

    const juce::StringArray& getPresetNames() const noexcept { return presetNames; }
    const juce::String& getCurrentPresetName() const noexcept { return currentPresetName; }
    void refreshPresetList();
    bool loadPresetByName(const juce::String& presetName);
    bool saveUserPreset(const juce::String& presetName);
    const juce::StringArray& getPatternNames() const noexcept { return patternNames; }
    const juce::String& getCurrentPatternName() const noexcept { return currentPatternName; }
    void refreshPatternList();
    bool loadPatternByName(const juce::String& patternName);
    bool saveSequencerPattern(const juce::String& patternName);
    void beginMidiLearn(const juce::String& parameterId);
    void cancelMidiLearn();
    bool clearMidiMappingsForCurrentPreset();
    juce::String getMidiLearnTargetParameter() const;
    bool isMidiLearnActiveFor(const juce::String& parameterId) const;
    juce::String getMidiMappingDescription(const juce::String& parameterId) const;
    int getCurrentThemeIndex() const noexcept { return currentThemeIndex; }
    void setCurrentThemeIndex(int newThemeIndex) noexcept;

    juce::AudioProcessorValueTreeState apvts;

private:
    struct ArpHeldNote
    {
        int midiNote { 60 };
        float velocity { 0.8f };
    };

    struct PresetDefinition
    {
        juce::String name;
        std::vector<std::pair<juce::String, float>> values;
        juce::String patternName;
    };

    struct PresetEntry
    {
        juce::String name;
        bool isFactory { true };
        juce::File file;
    };

    struct PatternDefinition
    {
        juce::String name;
        std::vector<std::pair<juce::String, float>> values;
    };

    struct PatternEntry
    {
        juce::String name;
        bool isFactory { true };
        juce::File file;
    };

    struct TransportState
    {
        double bpm { 120.0 };
        double ppqPosition { 0.0 };
        bool hasBpm { false };
        bool hasPpq { false };
        bool isPlaying { false };
    };

    struct StepSourceNote
    {
        int midiNote { 60 };
        float velocity { 0.8f };
        bool valid { false };
    };

    struct MidiMapping
    {
        juce::String parameterId;
        int controllerNumber { -1 };
    };

    struct MidiMappingSnapshot
    {
        juce::String presetName;
        std::vector<MidiMapping> mappings;
    };

    void updatePerformanceStateFromMidi(const juce::MidiBuffer& midiMessages);
    void handleMidiControllerMappings(const juce::MidiBuffer& midiMessages);
    void syncHeldNotesFromMidi(const juce::MidiBuffer& midiMessages, bool latchEnabled);
    void buildPerformanceMidi(const juce::MidiBuffer& source, juce::MidiBuffer& destination, int numSamples);
    void buildSequencerMidi(const juce::MidiBuffer& source,
                            juce::MidiBuffer& destination,
                            int numSamples,
                            const TransportState& transport,
                            bool arpEnabled);
    void updateHeldNotes(const juce::MidiMessage& message, bool latchEnabled);
    std::vector<ArpHeldNote> buildArpPool() const;
    ArpHeldNote chooseNextArpNote();

    void applyRhythmGate(juce::AudioBuffer<float>& buffer);
    void applyEffects(juce::AudioBuffer<float>& buffer, const TransportState& transport);
    void applyBitcrusher(juce::AudioBuffer<float>& buffer);
    void applyDelay(juce::AudioBuffer<float>& buffer, const TransportState& transport);
    TransportState getTransportState() const;

    juce::File getPresetDirectory() const;
    juce::File getPatternDirectory() const;
    juce::File getMidiMappingDirectory() const;
    void resetRealtimePlaybackState(bool clearHeldNotes) noexcept;
    void resetParametersToDefaults();
    void setParameterValue(const juce::String& id, float value);
    bool loadPresetFile(const juce::File& file);
    bool loadPatternFile(const juce::File& file);
    void applyFactoryPreset(const PresetDefinition& preset);
    void applyPatternDefinition(const PatternDefinition& pattern);
    juce::ValueTree createMidiMappingsState(const juce::String& presetName,
                                            const std::vector<MidiMapping>& mappings) const;
    void setCurrentMidiMappings(std::vector<MidiMapping> mappings);
    void queueMidiMappingSave(const juce::String& presetName);
    void loadMidiMappingsForCurrentPreset(const juce::ValueTree* embeddedState = nullptr);
    bool loadMidiMappingsFromFile(const juce::File& file, std::vector<MidiMapping>& mappings) const;
    void applyMappedControllerToParameter(const MidiMapping& mapping, int controllerValue);
    void assignMidiControllerToParameter(const juce::String& parameterId, int controllerNumber);
    void handleAsyncUpdate() override;
    static std::vector<PresetDefinition> createFactoryPresets();
    static std::vector<PatternDefinition> createFactoryPatterns();

    AISynthesiser synth;
    juce::MidiKeyboardState keyboardState;

    std::atomic<int> currentPitchWheelValue { 8192 };
    std::atomic<float> currentModWheelValue { 0.0f };

    juce::dsp::ProcessSpec spec;
    juce::dsp::Chorus<float> chorus;
    juce::dsp::Compressor<float> compressor;
    juce::dsp::Reverb reverb;

    std::vector<float> bitcrusherHeldSamples;
    juce::AudioBuffer<float> delayBuffer;
    int bitcrusherCounter { 0 };
    int delayWritePosition { 0 };
    juce::uint64 rhythmGateSamplePosition { 0 };
    juce::uint64 sequencerFreeSamplePosition { 0 };

    std::vector<ArpHeldNote> heldNotes;
    int arpIndex { 0 };
    bool arpDirectionForward { true };
    int arpCurrentNote { -1 };
    int arpSamplesUntilStep { 0 };
    int arpSamplesUntilOff { 0 };
    int sequencerCurrentNote { -1 };
    int sequencerLastAbsoluteStep { std::numeric_limits<int>::min() };
    int sequencerGateClosedStep { std::numeric_limits<int>::min() };
    StepSourceNote lastSequencerSourceNote;
    std::atomic<int> currentSequencerStep { -1 };
    juce::Random random;

    std::vector<PresetDefinition> factoryPresets;
    std::vector<PresetEntry> presetEntries;
    juce::StringArray presetNames;
    juce::String currentPresetName { "Init" };
    std::vector<PatternDefinition> factoryPatterns;
    std::vector<PatternEntry> patternEntries;
    juce::StringArray patternNames;
    juce::String currentPatternName { "Init Pattern" };
    mutable juce::SpinLock midiMappingLock;
    std::vector<MidiMapping> currentMidiMappings;
    juce::String pendingMidiLearnParameterId;
    MidiMappingSnapshot midiMappingSnapshotToPersist;
    int currentThemeIndex { 3 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AISynthAudioProcessor)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
