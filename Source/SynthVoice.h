#pragma once

#include <JuceHeader.h>

class AISynthAudioProcessor;

// A single playable voice containing oscillator, envelope, FM, filter and unison support.
class SynthVoice final : public juce::SynthesiserVoice
{
public:
    explicit SynthVoice(AISynthAudioProcessor& p);

    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int currentPitchWheelPosition) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}
    void renderNextBlock(juce::AudioBuffer<float>&, int startSample, int numSamples) override;

    void prepare(double sampleRate, int samplesPerBlock, int numChannels);

private:
    float renderOscillatorSample(size_t unisonIndex);
    float getEnvelopeValue();
    float applySteppedEnvelope(float rawEnvelope) const;
    float curveTransform(float value, float curveAmount) const;

    AISynthAudioProcessor& processor;

    juce::Random random;
    juce::ADSR ampAdsr;
    juce::ADSR::Parameters adsrParams;

    juce::dsp::StateVariableTPTFilter<float> filter;

    double currentSampleRate { 44100.0 };
    float noteFrequency { 440.0f };
    float level { 0.0f };
    float fmPhase { 0.0f };

    std::vector<float> unisonPhases;
    std::vector<float> unisonPan;
    std::vector<float> unisonDetuneHz;
    std::vector<float> unisonFineTune;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthVoice)
};
