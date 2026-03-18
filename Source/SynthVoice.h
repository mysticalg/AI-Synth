#pragma once

#include <JuceHeader.h>

class AISynthAudioProcessor;

class SynthVoice final : public juce::SynthesiserVoice
{
public:
    explicit SynthVoice(AISynthAudioProcessor& processorRef);

    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int currentPitchWheelPosition) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int newValue) override;
    void controllerMoved(int controllerNumber, int newValue) override;
    void renderNextBlock(juce::AudioBuffer<float>&, int startSample, int numSamples) override;

    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void setPendingPortamento(bool shouldGlide, bool preserveEnvelope) noexcept;

private:
    static constexpr int numLfos = 3;
    static constexpr int numOscillators = 4;
    static constexpr int numMatrixSlots = 4;

    struct ModulationState
    {
        float pitchSemitones { 0.0f };
        float cutoffHz { 0.0f };
        float resonance { 0.0f };
        float ampGain { 0.0f };
        float detuneCents { 0.0f };
        float drive { 0.0f };
        std::array<float, numOscillators> pwmOffsets {};
    };

    float getLfoValue(int index, int shape, float rate);
    float getPwmSourceValue(int source, float env2, const std::array<float, numLfos>& lfos) const;
    float renderWaveform(int waveform, float phase, float pulseWidth);
    float saturate(float sample, float amount) const;
    void applyDestinationModulation(ModulationState& state, int destination, float scaled) const;
    void applyMatrix(ModulationState& state,
                     float env1,
                     float env2,
                     const std::array<float, numLfos>& lfos,
                     const std::array<int, numMatrixSlots>& sources,
                     const std::array<int, numMatrixSlots>& destinations,
                     const std::array<float, numMatrixSlots>& amounts) const;

    AISynthAudioProcessor& processor;

    juce::Random random;
    juce::ADSR ampAdsr;
    juce::ADSR modAdsr;
    juce::ADSR::Parameters ampParams;
    juce::ADSR::Parameters modParams;
    juce::dsp::StateVariableTPTFilter<float> filter;

    double currentSampleRate { 44100.0 };
    float currentNoteFrequency { 440.0f };
    float targetNoteFrequency { 440.0f };
    float noteFrequencyStep { 0.0f };
    float noteVelocity { 0.0f };
    int currentPitchWheelValue { 8192 };
    float currentModWheelValue { 0.0f };
    int glideSamplesRemaining { 0 };
    bool pendingPortamento { false };
    bool pendingEnvelopeHold { false };

    std::array<float, numLfos> lfoPhases {};
    std::array<float, numLfos> sampleHoldValues {};

    std::vector<std::array<float, numOscillators>> oscillatorPhases;
    std::vector<std::array<bool, numOscillators>> previousWrapFlags;
    std::vector<float> unisonPositions;
    std::vector<float> unisonDrift;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthVoice)
};
