#include "SynthVoice.h"
#include "PluginProcessor.h"

namespace
{
constexpr float twoPi = juce::MathConstants<float>::twoPi;

int mapSubWaveformToMainIndex(int subWaveform)
{
    switch (subWaveform)
    {
        case 1: return 2;
        case 2: return 1;
        case 3: return 3;
        default: return 0;
    }
}
}

SynthVoice::SynthVoice(AISynthAudioProcessor& processorRef) : processor(processorRef)
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
    modAdsr.setSampleRate(sampleRate);

    juce::dsp::ProcessSpec localSpec { sampleRate, static_cast<juce::uint32>(samplesPerBlock), static_cast<juce::uint32>(juce::jmax(1, numChannels)) };
    filter.prepare(localSpec);
}

void SynthVoice::setPendingPortamento(bool shouldGlide, bool preserveEnvelope) noexcept
{
    pendingPortamento = shouldGlide;
    pendingEnvelopeHold = preserveEnvelope;
}

void SynthVoice::startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int newPitchWheelValue)
{
    const auto newFrequency = static_cast<float>(juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));
    const auto portamentoMode = static_cast<int>(processor.getParam("portamentoMode"));
    const auto shouldGlide = pendingPortamento && portamentoMode > 0 && processor.getParam("portamentoTime") > 0.0001f;
    const auto preserveEnvelope = shouldGlide && pendingEnvelopeHold && portamentoMode == 2;
    pendingPortamento = false;
    pendingEnvelopeHold = false;

    if (shouldGlide)
    {
        targetNoteFrequency = newFrequency;
        glideSamplesRemaining = juce::jmax(1, static_cast<int>(std::round(processor.getParam("portamentoTime") * currentSampleRate)));
        noteFrequencyStep = (targetNoteFrequency - currentNoteFrequency) / static_cast<float>(glideSamplesRemaining);
    }
    else
    {
        currentNoteFrequency = newFrequency;
        targetNoteFrequency = newFrequency;
        noteFrequencyStep = 0.0f;
        glideSamplesRemaining = 0;
    }

    noteVelocity = velocity;
    currentPitchWheelValue = newPitchWheelValue;
    currentModWheelValue = processor.getCurrentModWheelValue();

    const auto unisonCount = juce::jmax(1, static_cast<int>(processor.getParam("unisonVoices")));

    if (! preserveEnvelope || static_cast<int>(oscillatorPhases.size()) != unisonCount)
    {
        std::fill(lfoPhases.begin(), lfoPhases.end(), 0.0f);
        for (auto& sampleHold : sampleHoldValues)
            sampleHold = random.nextFloat() * 2.0f - 1.0f;

        oscillatorPhases.assign(static_cast<size_t>(unisonCount), {});
        previousWrapFlags.assign(static_cast<size_t>(unisonCount), {});
        unisonPositions.assign(static_cast<size_t>(unisonCount), 0.0f);
        unisonDrift.assign(static_cast<size_t>(unisonCount), 0.0f);

        for (int voiceIndex = 0; voiceIndex < unisonCount; ++voiceIndex)
        {
            const auto normalised = unisonCount == 1 ? 0.0f : juce::jmap(static_cast<float>(voiceIndex), 0.0f, static_cast<float>(unisonCount - 1), -1.0f, 1.0f);
            unisonPositions[static_cast<size_t>(voiceIndex)] = normalised;
            unisonDrift[static_cast<size_t>(voiceIndex)] = (random.nextFloat() - 0.5f) * 0.45f;

            for (int osc = 0; osc < numOscillators; ++osc)
                oscillatorPhases[static_cast<size_t>(voiceIndex)][static_cast<size_t>(osc)] = random.nextFloat() * twoPi;
        }
    }

    ampParams.attack = processor.getParam("env1Attack");
    ampParams.decay = processor.getParam("env1Decay");
    ampParams.sustain = processor.getParam("env1Sustain");
    ampParams.release = processor.getParam("env1Release");
    ampAdsr.setParameters(ampParams);

    modParams.attack = processor.getParam("env2Attack");
    modParams.decay = processor.getParam("env2Decay");
    modParams.sustain = processor.getParam("env2Sustain");
    modParams.release = processor.getParam("env2Release");
    modAdsr.setParameters(modParams);

    if (! preserveEnvelope)
    {
        ampAdsr.noteOn();
        modAdsr.noteOn();
        filter.reset();
    }
}

void SynthVoice::stopNote(float, bool allowTailOff)
{
    if (! allowTailOff && pendingEnvelopeHold)
        return;

    ampAdsr.noteOff();
    modAdsr.noteOff();

    if (! allowTailOff || (! ampAdsr.isActive() && ! modAdsr.isActive()))
        clearCurrentNote();
}

void SynthVoice::pitchWheelMoved(int newValue)
{
    currentPitchWheelValue = newValue;
}

void SynthVoice::controllerMoved(int controllerNumber, int newValue)
{
    if (controllerNumber == 1)
        currentModWheelValue = juce::jlimit(0.0f, 1.0f, newValue / 127.0f);
}

float SynthVoice::getLfoValue(int index, int shape, float rate)
{
    const auto increment = twoPi * (rate / static_cast<float>(currentSampleRate));
    auto& phase = lfoPhases[static_cast<size_t>(index)];

    phase += increment;
    bool wrapped = false;
    while (phase >= twoPi)
    {
        phase -= twoPi;
        wrapped = true;
    }

    if (shape == 4 && wrapped)
        sampleHoldValues[static_cast<size_t>(index)] = random.nextFloat() * 2.0f - 1.0f;

    const auto phaseNormalised = phase / twoPi;

    switch (shape)
    {
        case 1:
            return 1.0f - 4.0f * std::abs(phaseNormalised - 0.5f);
        case 2:
            return phase < juce::MathConstants<float>::pi ? 1.0f : -1.0f;
        case 3:
            return juce::jmap(phaseNormalised, 0.0f, 1.0f, 1.0f, -1.0f);
        case 4:
            return sampleHoldValues[static_cast<size_t>(index)];
        default:
            return std::sin(phase);
    }
}

float SynthVoice::getPwmSourceValue(int source, float env2, const std::array<float, numLfos>& lfos) const
{
    switch (source)
    {
        case 1: return lfos[0];
        case 2: return lfos[1];
        case 3: return lfos[2];
        case 4: return env2;
        case 5: return currentModWheelValue;
        default: return 0.0f;
    }
}

float SynthVoice::renderWaveform(int waveform, float phase, float pulseWidth)
{
    const auto phaseNormalised = phase / twoPi;
    const auto width = juce::jlimit(0.05f, 0.95f, pulseWidth);

    switch (waveform)
    {
        case 1:
        {
            if (phaseNormalised < width)
                return juce::jmap(phaseNormalised, 0.0f, width, -1.0f, 1.0f);
            return juce::jmap(phaseNormalised, width, 1.0f, 1.0f, -1.0f);
        }

        case 2:
            return phaseNormalised < width ? 1.0f : -1.0f;

        case 3:
        {
            const auto warped = phaseNormalised < width
                ? juce::jmap(phaseNormalised, 0.0f, width, 0.0f, 0.5f)
                : juce::jmap(phaseNormalised, width, 1.0f, 0.5f, 1.0f);
            return 1.0f - 4.0f * std::abs(warped - 0.5f);
        }

        case 4:
            return random.nextFloat() * 2.0f - 1.0f;

        default:
        {
            const auto warp = (width - 0.5f) * 1.8f;
            return std::sin(phase + std::sin(phase) * warp);
        }
    }
}

float SynthVoice::saturate(float sample, float amount) const
{
    if (amount <= 0.0001f)
        return sample;

    const auto drive = 1.0f + amount * 4.0f;
    return std::tanh(sample * drive);
}

void SynthVoice::applyDestinationModulation(ModulationState& state, int destination, float scaled) const
{
    switch (destination)
    {
        case 1: state.pitchSemitones += scaled * 12.0f; break;
        case 2: state.cutoffHz += scaled * 10000.0f; break;
        case 3: state.resonance += scaled * 0.8f; break;
        case 4: state.ampGain += scaled * 0.8f; break;
        case 5: state.pwmOffsets[0] += scaled * 0.45f; break;
        case 6: state.pwmOffsets[1] += scaled * 0.45f; break;
        case 7: state.pwmOffsets[2] += scaled * 0.45f; break;
        case 8: state.pwmOffsets[3] += scaled * 0.45f; break;
        case 9: state.detuneCents += scaled * 25.0f; break;
        case 10: state.drive += scaled * 0.75f; break;
        default: break;
    }
}

void SynthVoice::applyMatrix(ModulationState& state,
                             float env1,
                             float env2,
                             const std::array<float, numLfos>& lfos,
                             const std::array<int, numMatrixSlots>& sources,
                             const std::array<int, numMatrixSlots>& destinations,
                             const std::array<float, numMatrixSlots>& amounts) const
{
    const auto pitchBend = juce::jmap(static_cast<float>(currentPitchWheelValue), 0.0f, 16383.0f, -1.0f, 1.0f);

    for (int slot = 0; slot < numMatrixSlots; ++slot)
    {
        const auto source = sources[static_cast<size_t>(slot)];
        const auto destination = destinations[static_cast<size_t>(slot)];
        const auto amount = amounts[static_cast<size_t>(slot)];

        if (source == 0 || destination == 0 || std::abs(amount) < 0.0001f)
            continue;

        float sourceValue = 0.0f;
        switch (source)
        {
            case 1: sourceValue = lfos[0]; break;
            case 2: sourceValue = lfos[1]; break;
            case 3: sourceValue = lfos[2]; break;
            case 4: sourceValue = env1; break;
            case 5: sourceValue = env2; break;
            case 6: sourceValue = currentModWheelValue; break;
            case 7: sourceValue = noteVelocity; break;
            case 8: sourceValue = pitchBend; break;
            default: break;
        }

        applyDestinationModulation(state, destination, sourceValue * amount);
    }
}

void SynthVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;

    ampParams.attack = processor.getParam("env1Attack");
    ampParams.decay = processor.getParam("env1Decay");
    ampParams.sustain = processor.getParam("env1Sustain");
    ampParams.release = processor.getParam("env1Release");
    ampAdsr.setParameters(ampParams);

    modParams.attack = processor.getParam("env2Attack");
    modParams.decay = processor.getParam("env2Decay");
    modParams.sustain = processor.getParam("env2Sustain");
    modParams.release = processor.getParam("env2Release");
    modAdsr.setParameters(modParams);

    const std::array<int, numOscillators> waveforms
    {
        static_cast<int>(processor.getParam("osc1Wave")),
        static_cast<int>(processor.getParam("osc2Wave")),
        static_cast<int>(processor.getParam("osc3Wave")),
        mapSubWaveformToMainIndex(static_cast<int>(processor.getParam("subWave")))
    };

    const std::array<float, numOscillators> levels
    {
        processor.getParam("osc1Level"),
        processor.getParam("osc2Level"),
        processor.getParam("osc3Level"),
        processor.getParam("subLevel")
    };

    const std::array<float, numOscillators> coarseSemitones
    {
        processor.getParam("osc1Coarse"),
        processor.getParam("osc2Coarse"),
        processor.getParam("osc3Coarse"),
        processor.getParam("subOctave") * 12.0f
    };

    const std::array<float, numOscillators> fineCents
    {
        processor.getParam("osc1Fine"),
        processor.getParam("osc2Fine"),
        processor.getParam("osc3Fine"),
        0.0f
    };

    const std::array<float, numOscillators> pulseWidths
    {
        processor.getParam("osc1PulseWidth"),
        processor.getParam("osc2PulseWidth"),
        processor.getParam("osc3PulseWidth"),
        processor.getParam("subPulseWidth")
    };

    const std::array<float, numOscillators> pwmAmounts
    {
        processor.getParam("osc1PwmAmount"),
        processor.getParam("osc2PwmAmount"),
        processor.getParam("osc3PwmAmount"),
        processor.getParam("subPwmAmount")
    };

    const std::array<int, numOscillators> pwmSources
    {
        static_cast<int>(processor.getParam("osc1PwmSource")),
        static_cast<int>(processor.getParam("osc2PwmSource")),
        static_cast<int>(processor.getParam("osc3PwmSource")),
        static_cast<int>(processor.getParam("subPwmSource"))
    };

    const std::array<int, numOscillators> syncSources
    {
        0,
        static_cast<int>(processor.getParam("osc2SyncSource")),
        static_cast<int>(processor.getParam("osc3SyncSource")),
        0
    };

    const std::array<int, numLfos> lfoShapes
    {
        static_cast<int>(processor.getParam("lfo1Shape")),
        static_cast<int>(processor.getParam("lfo2Shape")),
        static_cast<int>(processor.getParam("lfo3Shape"))
    };

    const std::array<float, numLfos> lfoRates
    {
        processor.getParam("lfo1Rate"),
        processor.getParam("lfo2Rate"),
        processor.getParam("lfo3Rate")
    };

    const std::array<float, numLfos> lfoDepths
    {
        processor.getParam("lfo1Depth"),
        processor.getParam("lfo2Depth"),
        processor.getParam("lfo3Depth")
    };

    const std::array<int, numMatrixSlots> matrixSources
    {
        static_cast<int>(processor.getParam("matrixSource1")),
        static_cast<int>(processor.getParam("matrixSource2")),
        static_cast<int>(processor.getParam("matrixSource3")),
        static_cast<int>(processor.getParam("matrixSource4"))
    };

    const std::array<int, numMatrixSlots> matrixDestinations
    {
        static_cast<int>(processor.getParam("matrixDest1")),
        static_cast<int>(processor.getParam("matrixDest2")),
        static_cast<int>(processor.getParam("matrixDest3")),
        static_cast<int>(processor.getParam("matrixDest4"))
    };

    const std::array<float, numMatrixSlots> matrixAmounts
    {
        processor.getParam("matrixAmount1"),
        processor.getParam("matrixAmount2"),
        processor.getParam("matrixAmount3"),
        processor.getParam("matrixAmount4")
    };

    const auto filterType = static_cast<int>(processor.getParam("filterType"));
    const auto cutoff = processor.getParam("cutoff");
    const auto resonance = processor.getParam("resonance");
    const auto drive = processor.getParam("drive");
    const auto filterAccent = processor.getParam("filterAccent");
    const auto env1ToFilter = processor.getParam("env1ToFilter");
    const auto env2ToFilter = processor.getParam("env2ToFilter");
    const auto env2ToPitch = processor.getParam("env2ToPitch");
    const auto pitchBendRange = processor.getParam("pitchBendRange");
    const auto stereoWidth = processor.getParam("stereoWidth");
    const auto unisonDetune = processor.getParam("unisonDetune");
    const auto velocitySensitivity = processor.getParam("velocitySensitivity");
    const auto velocityDestination = static_cast<int>(processor.getParam("velocityDestination"));
    const auto velocityAmount = processor.getParam("velocityAmount");
    const auto pitchBendNormalised = juce::jmap(static_cast<float>(currentPitchWheelValue), 0.0f, 16383.0f, -1.0f, 1.0f);
    const auto unisonCount = juce::jmax(1, static_cast<int>(oscillatorPhases.size()));

    switch (filterType)
    {
        case 1: filter.setType(juce::dsp::StateVariableTPTFilterType::bandpass); break;
        case 2: filter.setType(juce::dsp::StateVariableTPTFilterType::highpass); break;
        default: filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass); break;
    }

    while (--numSamples >= 0)
    {
        if (glideSamplesRemaining > 0)
        {
            currentNoteFrequency += noteFrequencyStep;
            --glideSamplesRemaining;

            if (glideSamplesRemaining <= 0)
            {
                currentNoteFrequency = targetNoteFrequency;
                noteFrequencyStep = 0.0f;
            }
        }

        const auto env1 = ampAdsr.getNextSample();
        const auto env2 = modAdsr.getNextSample();

        std::array<float, numLfos> lfoValues {};
        for (int lfo = 0; lfo < numLfos; ++lfo)
            lfoValues[static_cast<size_t>(lfo)] = getLfoValue(lfo, lfoShapes[static_cast<size_t>(lfo)], lfoRates[static_cast<size_t>(lfo)]) * lfoDepths[static_cast<size_t>(lfo)];

        ModulationState modulation;
        applyMatrix(modulation, env1, env2, lfoValues, matrixSources, matrixDestinations, matrixAmounts);

        if (velocityDestination > 0 && std::abs(velocityAmount) > 0.0001f)
            applyDestinationModulation(modulation, velocityDestination, noteVelocity * velocityAmount);

        const auto accent = filterAccent * juce::jlimit(0.0f, 1.0f, juce::jmap(noteVelocity, 0.45f, 1.0f, 0.0f, 1.0f));
        const auto pitchOffset = (pitchBendNormalised * pitchBendRange) + (env2 * env2ToPitch) + modulation.pitchSemitones;
        const auto dynamicCutoff = juce::jlimit(40.0f, 18000.0f, cutoff + (env1 * env1ToFilter * 10000.0f) + (env2 * env2ToFilter * 10000.0f) + modulation.cutoffHz + accent * 5200.0f);
        const auto dynamicResonance = juce::jlimit(0.1f, 1.8f, resonance + modulation.resonance + accent * 0.22f);
        const auto dynamicDrive = juce::jlimit(0.0f, 1.6f, drive + modulation.drive + accent * 0.28f);
        const auto dynamicDetune = juce::jmax(0.0f, unisonDetune + modulation.detuneCents);

        filter.setCutoffFrequency(dynamicCutoff);
        filter.setResonance(dynamicResonance);

        float left = 0.0f;
        float right = 0.0f;

        for (int voiceIndex = 0; voiceIndex < unisonCount; ++voiceIndex)
        {
            const auto previousWraps = previousWrapFlags[static_cast<size_t>(voiceIndex)];
            std::array<bool, numOscillators> currentWraps {};
            float mixed = 0.0f;

            for (int osc = 0; osc < numOscillators; ++osc)
            {
                auto& phase = oscillatorPhases[static_cast<size_t>(voiceIndex)][static_cast<size_t>(osc)];

                const auto syncChoice = syncSources[static_cast<size_t>(osc)];
                if (syncChoice > 0)
                {
                    const auto sourceIndex = syncChoice - 1;
                    if (sourceIndex >= 0 && sourceIndex < numOscillators && sourceIndex != osc && previousWraps[static_cast<size_t>(sourceIndex)])
                        phase = 0.0f;
                }

                const auto detuneSemitones = ((unisonPositions[static_cast<size_t>(voiceIndex)] * dynamicDetune) + unisonDrift[static_cast<size_t>(voiceIndex)]) * 0.01f;
                const auto totalSemitones = pitchOffset + coarseSemitones[static_cast<size_t>(osc)] + (fineCents[static_cast<size_t>(osc)] * 0.01f) + detuneSemitones;
                const auto frequency = juce::jlimit(10.0f, static_cast<float>(currentSampleRate * 0.45), currentNoteFrequency * std::pow(2.0f, totalSemitones / 12.0f));
                const auto increment = twoPi * (frequency / static_cast<float>(currentSampleRate));

                phase += increment;
                while (phase >= twoPi)
                {
                    phase -= twoPi;
                    currentWraps[static_cast<size_t>(osc)] = true;
                }

                const auto pwmSourceValue = getPwmSourceValue(pwmSources[static_cast<size_t>(osc)], env2, lfoValues);
                const auto pulseWidth = juce::jlimit(0.05f, 0.95f,
                                                     pulseWidths[static_cast<size_t>(osc)]
                                                     + (pwmAmounts[static_cast<size_t>(osc)] * pwmSourceValue * 0.45f)
                                                     + modulation.pwmOffsets[static_cast<size_t>(osc)]);

                mixed += renderWaveform(waveforms[static_cast<size_t>(osc)], phase, pulseWidth) * levels[static_cast<size_t>(osc)];
            }

            previousWrapFlags[static_cast<size_t>(voiceIndex)] = currentWraps;

            mixed = saturate(mixed, dynamicDrive);

            const auto pan = juce::jlimit(-1.0f, 1.0f, unisonPositions[static_cast<size_t>(voiceIndex)] * stereoWidth);
            const auto leftGain = std::sqrt(0.5f * (1.0f - pan));
            const auto rightGain = std::sqrt(0.5f * (1.0f + pan));

            left += mixed * leftGain;
            right += mixed * rightGain;
        }

        const auto stackCompensation = 1.0f / std::sqrt(static_cast<float>(unisonCount));
        const auto velocityGain = juce::jmap(velocitySensitivity, 1.0f, noteVelocity);
        const auto amp = velocityGain * env1 * stackCompensation * juce::jlimit(0.0f, 2.2f, 1.0f + modulation.ampGain + accent * 0.24f);

        left *= amp;
        right *= amp;

        if (outputBuffer.getNumChannels() > 1)
        {
            left = filter.processSample(0, left);
            right = filter.processSample(1, right);
            outputBuffer.addSample(0, startSample, left);
            outputBuffer.addSample(1, startSample, right);
        }
        else
        {
            const auto mono = filter.processSample(0, (left + right) * 0.5f);
            outputBuffer.addSample(0, startSample, mono);
        }

        ++startSample;
    }

    if (! ampAdsr.isActive() && ! modAdsr.isActive())
        clearCurrentNote();
}
