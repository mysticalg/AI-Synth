#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SynthVoice.h"

namespace
{
class BasicSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};
}

AISynthAudioProcessor::AISynthAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    for (int i = 0; i < 16; ++i)
        synth.addVoice(new SynthVoice(*this));

    synth.addSound(new BasicSound());

    distortion.functionToUse = [] (float x)
    {
        return std::tanh(x);
    };
}

void AISynthAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate(sampleRate);

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<SynthVoice*>(synth.getVoice(i)))
            voice->prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());

    spec = { sampleRate, static_cast<juce::uint32>(samplesPerBlock), static_cast<juce::uint32>(getTotalNumOutputChannels()) };

    phaser.prepare(spec);
    flanger.prepare(spec);
    distortion.prepare(spec);

    delayLine.prepare(spec);
    delayLine.reset();
}

void AISynthAudioProcessor::releaseResources() {}

bool AISynthAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

float AISynthAudioProcessor::getParam(const juce::String& id) const
{
    if (auto* p = apvts.getRawParameterValue(id))
        return p->load();
    return 0.0f;
}

int AISynthAudioProcessor::getActiveWeatherIndex() const
{
    const auto seconds = juce::Time::getCurrentTime().toMilliseconds() / 1000;
    const auto weeks = seconds / (7 * 24 * 60 * 60);
    return static_cast<int>(weeks % 4); // 0=sun, 1=rain, 2=snow, 3=wind
}

void AISynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());

    phaser.setRate(getParam("phaserRate"));
    phaser.setDepth(getParam("phaserDepth"));

    flanger.setRate(getParam("flangerRate"));
    flanger.setDepth(getParam("flangerDepth"));
    flanger.setCentreDelay(3.0f + getParam("flangerDepth") * 15.0f);

    auto block = juce::dsp::AudioBlock<float>(buffer);
    auto context = juce::dsp::ProcessContextReplacing<float>(block);

    phaser.process(context);
    flanger.process(context);

    auto drive = getParam("drive");
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample(c, i, std::tanh(buffer.getSample(c, i) * (1.0f + drive * 20.0f)));

    // Low-level procedural SFX bed to represent weekly weather ambience.
    const auto sfxLevel = getParam("sfxLevel");
    const auto weather = getActiveWeatherIndex();
    const auto inverseSampleRate = static_cast<float>(1.0 / juce::jmax(1.0, getSampleRate()));

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto white = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
        weatherNoiseState += 0.08f * (white - weatherNoiseState);
        weatherLfoPhase += juce::MathConstants<float>::twoPi * 0.2f * inverseSampleRate;
        if (weatherLfoPhase > juce::MathConstants<float>::twoPi)
            weatherLfoPhase -= juce::MathConstants<float>::twoPi;

        float ambience = 0.0f;
        switch (weather)
        {
            case 0: ambience = 0.06f * std::sin(weatherLfoPhase * 1.5f); break; // sunny birds/air shimmer
            case 1: ambience = weatherNoiseState * 0.11f; break;                // rain noise
            case 2: ambience = weatherNoiseState * 0.05f + 0.03f * std::sin(weatherLfoPhase * 0.6f); break; // snow hush
            default: ambience = weatherNoiseState * 0.09f + 0.05f * std::sin(weatherLfoPhase * 3.0f); break; // wind gusts
        }

        for (int c = 0; c < buffer.getNumChannels(); ++c)
            buffer.addSample(c, i, ambience * sfxLevel);
    }

    const auto delayMix = getParam("delayMix");
    const auto delayTimeMs = getParam("delayTime");
    const auto feedback = getParam("delayFeedback");
    const auto delaySamples = static_cast<int>((delayTimeMs / 1000.0f) * getSampleRate());

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            auto in = buffer.getSample(ch, i);
            auto delayed = delayLine.popSample(ch, static_cast<float>(delaySamples));
            delayLine.pushSample(ch, in + delayed * feedback);
            buffer.setSample(ch, i, juce::jmap(delayMix, in, delayed));
        }
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout AISynthAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>("waveform", "Waveform", juce::StringArray { "Sine", "Saw", "Square", "Noise" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("attack", "Attack", juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.4f), 0.02f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("decay", "Decay", juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.4f), 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("sustain", "Sustain", 0.0f, 1.0f, 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("release", "Release", juce::NormalisableRange<float>(0.001f, 8.0f, 0.001f, 0.4f), 0.4f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("envCurve", "Envelope Curve", -1.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("envStepped", "Step ADSR", false));
    params.push_back(std::make_unique<juce::AudioParameterInt>("envSteps", "Step Count", 1, 32, 8));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("cutoff", "Cutoff", juce::NormalisableRange<float>(40.0f, 18000.0f, 1.0f, 0.2f), 12000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("resonance", "Resonance", 0.1f, 1.2f, 0.4f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("lfoRate", "LFO Rate", 0.01f, 40.0f, 3.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lfoDepth", "LFO Depth", 0.0f, 1.0f, 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("modLfoToFilter", "LFO->Filter", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("modEnvToFilter", "ENV->Filter", 0.0f, 1.0f, 0.4f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("fmMix", "FM Mix", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("fmAmount", "FM Amount", 0.0f, 3.0f, 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("fmRatio", "FM Ratio", 0.1f, 8.0f, 2.0f));

    params.push_back(std::make_unique<juce::AudioParameterInt>("unisonVoices", "Unison Voices", 1, 128, 8));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("unisonSpread", "Unison Spread Hz", 0.0f, 20000.0f, 40.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("unisonPan", "Unison Pan", 0.0f, 1.0f, 0.8f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("delayMix", "Delay Mix", 0.0f, 1.0f, 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("delayTime", "Delay Time", 1.0f, 1200.0f, 320.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("delayFeedback", "Delay Feedback", 0.0f, 0.95f, 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("phaserRate", "Phaser Rate", 0.01f, 8.0f, 0.6f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("phaserDepth", "Phaser Depth", 0.0f, 1.0f, 0.4f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("flangerRate", "Flanger Rate", 0.01f, 6.0f, 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("flangerDepth", "Flanger Depth", 0.0f, 1.0f, 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("drive", "Drive", 0.0f, 1.0f, 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("sfxLevel", "Class SFX", 0.0f, 1.0f, 0.2f));

    return { params.begin(), params.end() };
}

void AISynthAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void AISynthAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* AISynthAudioProcessor::createEditor()
{
    return new AISynthAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AISynthAudioProcessor();
}
