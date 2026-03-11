#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class EnvelopeDisplay final : public juce::Component, private juce::Timer
{
public:
    explicit EnvelopeDisplay(AISynthAudioProcessor& processorRef);

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    AISynthAudioProcessor& processor;
    juce::Path envelopePath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeDisplay)
};

// Classroom visualiser that renders speech/thought bubbles, an activity log and weather-driven behaviour cues.
class ClassroomScene final : public juce::Component, private juce::Timer
{
public:
    explicit ClassroomScene(AISynthAudioProcessor& processorRef);

    void paint(juce::Graphics& g) override;
    void triggerActionKey();

private:
    struct StudentState
    {
        juce::Point<float> position;
        bool speaking { true };
        juce::String bubbleText;
    };

    void timerCallback() override;
    juce::String getWeatherName() const;
    juce::String getWeatherReaction() const;

    AISynthAudioProcessor& processor;
    std::array<StudentState, 4> students;
    juce::String activityLog;
    float windShift { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClassroomScene)
};

class AISynthAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit AISynthAudioProcessorEditor(AISynthAudioProcessor&);
    ~AISynthAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    juce::Slider& addKnob(const juce::String& id, const juce::String& title, const juce::String& tooltip);

    AISynthAudioProcessor& audioProcessor;
    juce::TooltipWindow tooltipWindow { this, 500 };

    juce::ComboBox waveform;
    std::unique_ptr<ComboAttachment> waveformAttachment;

    juce::OwnedArray<juce::Slider> knobs;
    juce::OwnedArray<juce::Label> labels;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;

    juce::ToggleButton steppedEnvButton { "✂ Step Env" };
    std::unique_ptr<ButtonAttachment> steppedEnvAttachment;

    EnvelopeDisplay envelopeDisplay;
    ClassroomScene classroomScene;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AISynthAudioProcessorEditor)
};
