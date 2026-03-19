#include "../Source/PluginProcessor.h"

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <thread>

namespace
{
juce::StringArray chooseStressPresets(const juce::StringArray& presetNames)
{
    juce::StringArray selected;

    for (const auto& group : { "Bass /", "Lead /", "Pad /", "Motion /", "SFX /", "Voice /", "Keys /" })
    {
        for (const auto& presetName : presetNames)
        {
            if (presetName.startsWith(group))
            {
                selected.addIfNotAlreadyThere(presetName);
                break;
            }
        }
    }

    if (selected.isEmpty())
        selected = presetNames;

    return selected;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    AISynthAudioProcessor processor;
    processor.prepareToPlay(48000.0, 256);

    const auto stressPresets = chooseStressPresets(processor.getPresetNames());
    const auto patternNames = processor.getPatternNames();

    if (stressPresets.isEmpty())
    {
        std::cerr << "No presets available for stress test.\n";
        return 1;
    }

    processor.loadPresetByName(stressPresets[0]);

    std::atomic<bool> failed { false };
    std::atomic<int> processedBlocks { 0 };

    auto audioFuture = std::async(std::launch::async, [&]
    {
        juce::AudioBuffer<float> buffer(2, 256);
        juce::MidiBuffer midi;

        for (int block = 0; block < 2200 && ! failed.load(); ++block)
        {
            buffer.clear();
            midi.clear();

            const auto noteNumber = 48 + (block / 32) % 24;
            if (block % 32 == 0)
                midi.addEvent(juce::MidiMessage::noteOn(1, noteNumber, static_cast<juce::uint8>(100)), 0);
            else if (block % 32 == 16)
                midi.addEvent(juce::MidiMessage::noteOff(1, noteNumber), 0);

            processor.processBlock(buffer, midi);
            processedBlocks.store(block + 1);
        }
    });

    auto presetFuture = std::async(std::launch::async, [&]
    {
        for (int index = 0; index < 320 && ! failed.load(); ++index)
        {
            processor.loadPresetByName(stressPresets[index % stressPresets.size()]);

            if (! patternNames.isEmpty() && (index % 3) == 0)
                processor.loadPatternByName(patternNames[index % patternNames.size()]);

            std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }
    });

    const auto audioStatus = audioFuture.wait_for(std::chrono::seconds(25));
    const auto presetStatus = presetFuture.wait_for(std::chrono::seconds(25));

    if (audioStatus != std::future_status::ready || presetStatus != std::future_status::ready)
    {
        failed.store(true);
        std::cerr << "Stress test timed out after " << processedBlocks.load() << " audio blocks.\n";
        return 2;
    }

    audioFuture.get();
    presetFuture.get();
    processor.releaseResources();

    std::cout << "Stress test completed successfully after " << processedBlocks.load() << " audio blocks.\n";
    return 0;
}
