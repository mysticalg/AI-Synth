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

ClassroomScene::ClassroomScene(AISynthAudioProcessor& processorRef) : processor(processorRef)
{
    students[0].position = { 70.0f, 165.0f };
    students[1].position = { 160.0f, 165.0f };
    students[2].position = { 250.0f, 165.0f };
    students[3].position = { 340.0f, 165.0f };
    startTimerHz(5);
}

void ClassroomScene::timerCallback()
{
    static const std::array<juce::String, 6> speechOptions {
        "I think the answer is 12!",
        "🙂 Let's solve it together.",
        "Teacher, can I explain?",
        "👍 Great idea!",
        "🤔 Maybe try another method?",
        "🎯 I can show my working."
    };

    static const std::array<juce::String, 4> thoughtOptions {
        "(daydreaming about lunch...)",
        "(thinking about homework)",
        "(imagining snowball teams)",
        "(planning football strategy)"
    };

    for (auto& student : students)
    {
        student.speaking = juce::Random::getSystemRandom().nextBool();
        if (student.speaking)
            student.bubbleText = speechOptions[static_cast<size_t>(juce::Random::getSystemRandom().nextInt(static_cast<int>(speechOptions.size())))];
        else
            student.bubbleText = thoughtOptions[static_cast<size_t>(juce::Random::getSystemRandom().nextInt(static_cast<int>(thoughtOptions.size())))];
    }

    if (activityLog.isEmpty())
        activityLog = "Activity Log: Students answer teacher questions, share ideas with emoticons, and react to classmates.";

    if (getWeatherName() == "Windy")
        windShift = std::sin(juce::Time::getMillisecondCounterHiRes() * 0.006) * 8.0f;
    else
        windShift = 0.0f;

    repaint();
}

juce::String ClassroomScene::getWeatherName() const
{
    const auto seconds = juce::Time::getCurrentTime().toMilliseconds() / 1000;
    const auto weeks = seconds / (7 * 24 * 60 * 60);
    switch (weeks % 4)
    {
        case 0: return "Sunny";
        case 1: return "Rainy";
        case 2: return "Snowy";
        default: return "Windy";
    }
}

juce::String ClassroomScene::getWeatherReaction() const
{
    const auto weather = getWeatherName();
    if (weather == "Rainy")
        return "Students choose to stay inside while it rains.";
    if (weather == "Snowy")
        return "Students want snowball fights during break.";
    if (weather == "Sunny")
        return "Students want to play football outside.";
    return "Students just want to run around in the wind.";
}

void ClassroomScene::triggerActionKey()
{
    activityLog = "Activity Log: [E] action key used at urinals. Toilets are interactable.";
    repaint();
}

void ClassroomScene::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(6.0f);
    g.fillAll(juce::Colour(0xff0f1218));

    auto outside = bounds.removeFromTop(95.0f);
    const auto weather = getWeatherName();

    g.setColour(juce::Colour(0xff37445f));
    g.fillRect(outside);

    // Sun / rain / snow / wind visual cues for weekly rotating weather.
    if (weather == "Sunny")
    {
        g.setColour(juce::Colours::yellow);
        g.fillEllipse(outside.getRight() - 62.0f, outside.getY() + 12.0f, 32.0f, 32.0f);
    }
    else if (weather == "Rainy")
    {
        g.setColour(juce::Colours::lightblue);
        for (int i = 0; i < 16; ++i)
            g.drawLine(outside.getX() + i * 28.0f, outside.getY() + 8.0f, outside.getX() + i * 28.0f - 5.0f, outside.getBottom() - 6.0f, 1.4f);
    }
    else if (weather == "Snowy")
    {
        g.setColour(juce::Colours::white);
        for (int i = 0; i < 28; ++i)
            g.fillEllipse(outside.getX() + i * 16.0f, outside.getY() + 10.0f + (i % 5) * 12.0f, 4.0f, 4.0f);
    }
    else
    {
        g.setColour(juce::Colours::whitesmoke);
        for (int i = 0; i < 4; ++i)
            g.drawArrow({ outside.getX() + 40.0f + i * 90.0f, outside.getY() + 22.0f, 42.0f + i * 90.0f, outside.getY() + 10.0f }, 1.6f, 8.0f, 7.0f);
    }

    // Tree line around the school; sways in windy weather.
    for (int i = 0; i < 10; ++i)
    {
        const auto x = outside.getX() + 12.0f + i * 44.0f + windShift;
        g.setColour(juce::Colours::saddlebrown);
        g.fillRect(x + 8.0f, outside.getBottom() - 22.0f, 5.0f, 20.0f);
        g.setColour(juce::Colours::forestgreen);
        g.fillEllipse(x, outside.getBottom() - 38.0f, 22.0f, 18.0f);
    }

    auto classroom = bounds;
    g.setColour(juce::Colour(0xff1d2330));
    g.fillRoundedRectangle(classroom, 8.0f);

    // Toilet corner with urinals and action-key hint.
    auto toiletArea = classroom.removeFromRight(140.0f).reduced(6.0f);
    g.setColour(juce::Colour(0xff141920));
    g.fillRoundedRectangle(toiletArea, 6.0f);
    g.setColour(juce::Colours::lightgrey);
    g.drawText("Toilets", toiletArea.removeFromTop(20.0f).toNearestInt(), juce::Justification::centred);
    for (int i = 0; i < 2; ++i)
        g.fillRoundedRectangle(toiletArea.getX() + 14.0f + i * 56.0f, toiletArea.getY() + 18.0f, 30.0f, 54.0f, 7.0f);
    g.setColour(juce::Colours::deepskyblue.withAlpha(0.9f));
    g.drawText("Press [E] action key\nto use urinals", toiletArea.removeFromBottom(38.0f).toNearestInt(), juce::Justification::centred);

    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.setFont(12.0f);
    g.drawText(activityLog, classroom.removeFromTop(18.0f).toNearestInt(), juce::Justification::left);
    g.drawText("Weather: " + weather + "  •  " + getWeatherReaction(), classroom.removeFromTop(18.0f).toNearestInt(), juce::Justification::left);

    for (const auto& student : students)
    {
        const auto x = student.position.x + windShift;
        const auto y = student.position.y;

        g.setColour(juce::Colours::lightgreen);
        g.fillEllipse(x, y, 20.0f, 20.0f);

        auto bubbleArea = juce::Rectangle<float>(x - 36.0f, y - 44.0f, 96.0f, 28.0f);
        g.setColour(student.speaking ? juce::Colours::white : juce::Colours::lightgoldenrodyellow);
        g.fillRoundedRectangle(bubbleArea, 8.0f);
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.setFont(10.0f);
        g.drawFittedText(student.bubbleText, bubbleArea.toNearestInt().reduced(3), juce::Justification::centred, 2);
    }
}

AISynthAudioProcessorEditor::AISynthAudioProcessorEditor(AISynthAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), envelopeDisplay(p), classroomScene(p)
{
    setSize(1200, 860);

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
    add("sfxLevel", "SFX", "Class ambience and weather sound effect level.");

    steppedEnvButton.setTooltip("Enable stepped ADSR editing/playback.");
    addAndMakeVisible(steppedEnvButton);
    steppedEnvAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "envStepped", steppedEnvButton);

    addAndMakeVisible(envelopeDisplay);
    addAndMakeVisible(classroomScene);
    setWantsKeyboardFocus(true);
    grabKeyboardFocus();
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

bool AISynthAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    if (key.getTextCharacter() == 'e' || key.getTextCharacter() == 'E')
    {
        classroomScene.triggerActionKey();
        return true;
    }
    return false;
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

    auto sceneArea = area.removeFromTop(220);
    classroomScene.setBounds(sceneArea.reduced(4));

    auto grid = area;
    const int columns = 8;
    const int cellW = grid.getWidth() / columns;
    const int cellH = 130;

    for (int i = 0; i < knobs.size(); ++i)
    {
        const int row = i / columns;
        const int col = i % columns;
        auto cell = juce::Rectangle<int>(grid.getX() + col * cellW, grid.getY() + row * cellH, cellW, cellH);

        labels[i]->setBounds(cell.removeFromTop(16));
        knobs[i]->setBounds(cell.reduced(8));
    }
}
