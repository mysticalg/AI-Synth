#include "PluginEditor.h"

EnvelopeDisplay::EnvelopeDisplay(AISynthAudioProcessor& processorRef) : processor(processorRef)
{
    startTimerHz(24);
}

void EnvelopeDisplay::timerCallback()
{
    repaint();
}

void EnvelopeDisplay::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced(8.0f);
    g.fillAll(juce::Colour(0xff111111));
    g.setColour(juce::Colours::darkgrey);
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    const auto attack = processor.getParam("attack");
    const auto decay = processor.getParam("decay");
    const auto sustain = processor.getParam("sustain");
    const auto release = processor.getParam("release");
    const auto curve = processor.getParam("envCurve");

    const auto sum = attack + decay + release + 0.5f;
    const auto attackX = area.getX() + area.getWidth() * (attack / sum);
    const auto decayX = attackX + area.getWidth() * (decay / sum);
    const auto releaseX = area.getRight() - area.getWidth() * (release / sum);

    auto shaped = [curve] (float v)
    {
        if (curve >= 0.0f)
            return std::pow(v, 1.0f + curve * 4.0f);
        return 1.0f - std::pow(1.0f - v, 1.0f + (-curve * 4.0f));
    };

    juce::Path p;
    p.startNewSubPath(area.getX(), area.getBottom());
    p.lineTo(attackX, area.getBottom() - area.getHeight() * shaped(1.0f));
    p.lineTo(decayX, area.getBottom() - area.getHeight() * shaped(sustain));
    p.lineTo(releaseX, area.getBottom() - area.getHeight() * shaped(sustain));
    p.lineTo(area.getRight(), area.getBottom());

    g.setColour(juce::Colours::deepskyblue);
    g.strokePath(p, juce::PathStrokeType(2.0f));

    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.setFont(12.0f);
    g.drawText("ADSR (curved / stepped preview)", getLocalBounds().reduced(12), juce::Justification::topLeft);
}

AISynthAudioProcessorEditor::AISynthAudioProcessorEditor(AISynthAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), envelopeDisplay(p)
{
    setSize(1200, 720);

    waveform.addItemList({ "Sine", "Saw", "Square", "Noise" }, 1);
    waveform.setTooltip("Main oscillator waveform selector.");
    addAndMakeVisible(waveform);
    waveformAttachment = std::make_unique<ComboAttachment>(audioProcessor.apvts, "waveform", waveform);

    auto add = [this] (const juce::String& id, const juce::String& title, const juce::String& tip)
    {
        addKnob(id, title, tip);
    };

    add("attack", "A", "Attack time in seconds.");
    add("decay", "D", "Decay time in seconds.");
    add("sustain", "S", "Sustain level.");
    add("release", "R", "Release time in seconds.");
    add("envCurve", "Curve", "Envelope curve shape from log to exp.");
    add("envSteps", "Steps", "Step count for stepped ADSR mode.");

    add("cutoff", "Cutoff", "Low-pass filter cutoff.");
    add("resonance", "Reso", "Filter resonance/Q.");
    add("lfoRate", "LFO Hz", "LFO rate.");
    add("lfoDepth", "LFO D", "LFO amount.");
    add("modLfoToFilter", "LFO>F", "Mod matrix slot: LFO to filter cutoff.");
    add("modEnvToFilter", "ENV>F", "Mod matrix slot: envelope to filter cutoff.");

    add("fmMix", "FM Mix", "Mix FM oscillator with main oscillator.");
    add("fmAmount", "FM Amt", "Frequency modulation amount.");
    add("fmRatio", "FM Ratio", "FM oscillator ratio.");

    add("unisonVoices", "Unison", "Up to 128 unison voices.");
    add("unisonSpread", "Spread", "Pitch spread in Hz (supports extreme 20kHz offsets).");
    add("unisonPan", "Pan", "Stereo distribution of unison voices.");

    add("delayMix", "Delay", "Delay dry/wet.");
    add("delayTime", "D Time", "Delay time in milliseconds.");
    add("delayFeedback", "D FB", "Delay feedback.");
    add("phaserRate", "Ph Hz", "Phaser rate.");
    add("phaserDepth", "Ph D", "Phaser depth.");
    add("flangerRate", "Fl Hz", "Flanger rate.");
    add("flangerDepth", "Fl D", "Flanger depth.");
    add("drive", "Drive", "Distortion / overdrive intensity.");

    steppedEnvButton.setTooltip("Enable stepped ADSR editing/playback.");
    addAndMakeVisible(steppedEnvButton);
    steppedEnvAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "envStepped", steppedEnvButton);

    addAndMakeVisible(envelopeDisplay);
}

juce::Slider& AISynthAudioProcessorEditor::addKnob(const juce::String& id, const juce::String& title, const juce::String& tooltip)
{
    auto* knob = knobs.add(new juce::Slider());
    knob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    knob->setTooltip(tooltip);
    addAndMakeVisible(knob);

    auto* label = labels.add(new juce::Label());
    label->setText(title, juce::dontSendNotification);
    label->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);

    sliderAttachments.push_back(std::make_unique<SliderAttachment>(audioProcessor.apvts, id, *knob));

    return *knob;
}

void AISynthAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));
    g.setColour(juce::Colours::white);
    g.setFont(22.0f);
    g.drawText("🎛 AI Synth — Cubase-ready VSTi", 14, 8, getWidth() - 28, 28, juce::Justification::centredLeft);
    g.setFont(13.0f);
    g.setColour(juce::Colours::lightgrey);
    g.drawText("Tooltips included for every control. Hover to learn what each section does.", 14, 36, getWidth() - 20, 20, juce::Justification::centredLeft);
}

void AISynthAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(12);
    auto top = area.removeFromTop(54);
    juce::ignoreUnused(top);

    auto waveformArea = area.removeFromTop(28);
    waveform.setBounds(waveformArea.removeFromLeft(200));
    steppedEnvButton.setBounds(waveformArea.removeFromLeft(140));

    auto envArea = area.removeFromTop(190);
    envelopeDisplay.setBounds(envArea.reduced(4));

    auto grid = area;
    const int columns = 8;
    const int cellW = grid.getWidth() / columns;
    const int cellH = 140;

    for (int i = 0; i < knobs.size(); ++i)
    {
        const int row = i / columns;
        const int col = i % columns;
        auto cell = juce::Rectangle<int>(grid.getX() + col * cellW, grid.getY() + row * cellH, cellW, cellH);

        labels[i]->setBounds(cell.removeFromTop(16));
        knobs[i]->setBounds(cell.reduced(8));
    }
}
