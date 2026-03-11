#include "SynthVoice.h"
#include "PluginProcessor.h"

namespace
{
constexpr float twoPi = juce::MathConstants<float>::twoPi;
}

SynthVoice::SynthVoice(AISynthAudioProcessor& p) : processor(p)
{
    filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
}

bool SynthVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<juce::SynthesiserSound*>(sound) != nullptr;
}

void SynthVoice::prepare(double sampleRate, int samplesPerBlock, int numChannels)
{
    currentSampleRate = sampleRate;
    ampAdsr.setSampleRate(sampleRate);

    juce::ignoreUnused(samplesPerBlock, numChannels);

    juce::dsp::ProcessSpec localSpec { sampleRate, static_cast<juce::uint32>(samplesPerBlock), static_cast<juce::uint32>(numChannels) };
    filter.prepare(localSpec);
}

void SynthVoice::startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int)
{
    noteFrequency = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    level = velocity;
    fmPhase = 0.0f;

    const auto unisonCount = juce::jmax(1, static_cast<int>(processor.getParam("unisonVoices")));
    unisonPhases.assign(static_cast<size_t>(unisonCount), 0.0f);
    unisonPan.assign(static_cast<size_t>(unisonCount), 0.0f);
    unisonDetuneHz.assign(static_cast<size_t>(unisonCount), 0.0f);
    unisonFineTune.assign(static_cast<size_t>(unisonCount), 0.0f);

    const auto spread = processor.getParam("unisonSpread");
    const auto panWidth = processor.getParam("unisonPan");

    for (int i = 0; i < unisonCount; ++i)
    {
        const auto t = unisonCount == 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(unisonCount - 1);
        const auto centered = (t * 2.0f) - 1.0f;
        unisonPan[static_cast<size_t>(i)] = centered * panWidth;
        unisonDetuneHz[static_cast<size_t>(i)] = centered * spread;
        unisonFineTune[static_cast<size_t>(i)] = random.nextFloat() * 0.15f;
    }

    adsrParams.attack = processor.getParam("attack");
    adsrParams.decay = processor.getParam("decay");
    adsrParams.sustain = processor.getParam("sustain");
    adsrParams.release = processor.getParam("release");
    ampAdsr.setParameters(adsrParams);
    ampAdsr.noteOn();
}

void SynthVoice::stopNote(float, bool allowTailOff)
{
    ampAdsr.noteOff();
    if (! allowTailOff || ! ampAdsr.isActive())
        clearCurrentNote();
}

float SynthVoice::curveTransform(float value, float curveAmount) const
{
    if (curveAmount >= 0.0f)
        return std::pow(value, 1.0f + (curveAmount * 4.0f));

    return 1.0f - std::pow(1.0f - value, 1.0f + (-curveAmount * 4.0f));
}

float SynthVoice::applySteppedEnvelope(float rawEnvelope) const
{
    const auto stepCount = juce::jmax(1, static_cast<int>(processor.getParam("envSteps")));
    const auto step = std::floor(rawEnvelope * static_cast<float>(stepCount)) / static_cast<float>(stepCount);
    return juce::jlimit(0.0f, 1.0f, step);
}

float SynthVoice::getEnvelopeValue()
{
    auto env = ampAdsr.getNextSample();
    env = curveTransform(env, processor.getParam("envCurve"));

    if (processor.getParam("envStepped") > 0.5f)
        env = applySteppedEnvelope(env);

    return env;
}

float SynthVoice::renderOscillatorSample(size_t unisonIndex)
{
    const auto waveform = static_cast<int>(processor.getParam("waveform"));
    const auto fmMix = processor.getParam("fmMix");
    const auto fmAmount = processor.getParam("fmAmount");
    const auto fmRatio = processor.getParam("fmRatio");

    const auto frequency = noteFrequency + unisonDetuneHz[unisonIndex] + unisonFineTune[unisonIndex];

    const auto fmHz = noteFrequency * fmRatio;
    fmPhase += twoPi * (fmHz / static_cast<float>(currentSampleRate));
    if (fmPhase > twoPi)
        fmPhase -= twoPi;

    const auto fmSample = std::sin(fmPhase);

    auto& phase = unisonPhases[unisonIndex];
    phase += twoPi * ((frequency + (fmSample * fmAmount * noteFrequency)) / static_cast<float>(currentSampleRate));
    if (phase > twoPi)
        phase -= twoPi;

    float out = 0.0f;
    switch (waveform)
    {
        case 0: out = std::sin(phase); break;
        case 1: out = 1.0f - (phase / juce::MathConstants<float>::pi); break;
        case 2: out = std::sin(phase) >= 0.0f ? 1.0f : -1.0f; break;
        case 3: out = random.nextFloat() * 2.0f - 1.0f; break;
        default: break;
    }

    return juce::jmap(fmMix, out, fmSample);
}

void SynthVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;

    const auto unisonCount = juce::jmax(1, static_cast<int>(processor.getParam("unisonVoices")));
    const auto cutoff = processor.getParam("cutoff");
    const auto resonance = processor.getParam("resonance");
    const auto lfoRate = processor.getParam("lfoRate");
    const auto lfoDepth = processor.getParam("lfoDepth");
    const auto lfoToFilter = processor.getParam("modLfoToFilter");
    const auto envToFilter = processor.getParam("modEnvToFilter");

    float lfoPhase = 0.0f;

    while (--numSamples >= 0)
    {
        const auto env = getEnvelopeValue();

        lfoPhase += twoPi * (lfoRate / static_cast<float>(currentSampleRate));
        if (lfoPhase > twoPi)
            lfoPhase -= twoPi;
        const auto lfo = std::sin(lfoPhase) * lfoDepth;

        const auto dynamicCutoff = juce::jlimit(40.0f,
                                                18000.0f,
                                                cutoff + (env * envToFilter * 9000.0f) + (lfo * lfoToFilter * 6000.0f));
        filter.setCutoffFrequency(dynamicCutoff);
        filter.setResonance(resonance);

        float mixed = 0.0f;
        float left = 0.0f;
        float right = 0.0f;

        for (int i = 0; i < unisonCount; ++i)
        {
            const auto sample = renderOscillatorSample(static_cast<size_t>(i));
            const auto pan = unisonPan[static_cast<size_t>(i)];
            left += sample * (1.0f - pan);
            right += sample * (1.0f + pan);
            mixed += sample;
        }

        const auto gain = level / static_cast<float>(unisonCount);
        left *= gain * env * 0.5f;
        right *= gain * env * 0.5f;

        left = filter.processSample(0, left);
        right = filter.processSample(1, right);

        outputBuffer.addSample(0, startSample, left);
        if (outputBuffer.getNumChannels() > 1)
            outputBuffer.addSample(1, startSample, right);

        ++startSample;
    }

    if (! ampAdsr.isActive())
        clearCurrentNote();
}
