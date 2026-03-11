#pragma once

#include <JuceHeader.h>

class SynthVoice;

class AISynthAudioProcessor final : public juce::AudioProcessor
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
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override { juce::ignoreUnused(index); }
    const juce::String getProgramName(int index) override { juce::ignoreUnused(index); return {}; }
    void changeProgramName(int index, const juce::String& name) override { juce::ignoreUnused(index, name); }

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    float getParam(const juce::String& id) const;

private:
    juce::Synthesiser synth;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine { 96000 };
    juce::AudioBuffer<float> delayBuffer;
    int delayWritePosition { 0 };

    juce::dsp::Phaser<float> phaser;
    juce::dsp::Chorus<float> flanger;
    juce::dsp::WaveShaper<float> distortion;

    juce::dsp::ProcessSpec spec;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AISynthAudioProcessor)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
