#include "PluginEditor.h"

namespace
{
const juce::StringArray waveformItems { "Sine", "Saw", "Square", "Triangle", "Noise" };
const juce::StringArray subWaveformItems { "Sine", "Square", "Saw", "Triangle" };
const juce::StringArray filterItems { "Low Pass", "Band Pass", "High Pass", "Low Pass 24" };
const juce::StringArray lfoShapeItems { "Sine", "Triangle", "Square", "Saw", "S&H" };
const juce::StringArray pwmSourceItems { "Off", "LFO 1", "LFO 2", "LFO 3", "Env 2", "Mod Wheel" };
const juce::StringArray syncSourceItems { "Off", "Osc 1", "Osc 2", "Osc 3" };
const juce::StringArray arpModeItems { "Up", "Down", "Up/Down", "Random" };
const juce::StringArray gatePatternItems { "Straight", "Trance", "Syncopated", "Stutter" };
const juce::StringArray keyModeItems { "Poly", "Mono" };
const juce::StringArray portamentoModeItems { "Off", "Retrig", "Legato" };
const juce::StringArray timingDivisionItems { "1/32", "1/16", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/2", "1 Bar" };
const juce::StringArray matrixSourceItems { "Off", "LFO 1", "LFO 2", "LFO 3", "Env 1", "Env 2", "Mod Wheel", "Velocity", "Pitch Bend" };
const juce::StringArray matrixDestinationItems
{
    "Off", "Pitch", "Cutoff", "Resonance", "Amp",
    "Osc 1 PWM", "Osc 2 PWM", "Osc 3 PWM", "Sub PWM", "Detune", "Drive"
};
const juce::StringArray themeItems
{
    "Synthwave / Retro Future",
    "Horror / Dark Occult",
    "Metal / Industrial",
    "AI / Neural Core",
    "Minimal Pro"
};

constexpr float rotaryKnobDiameter = 44.0f;
constexpr int rotaryTextBoxWidth = 64;
constexpr int rotaryTextBoxHeight = 20;
constexpr int rotaryControlWidth = 78;
constexpr int rotaryControlHeight = 86;

juce::String formatSliderValue(const juce::String& id, double value)
{
    const auto roundedInt = juce::roundToInt(value);
    const auto isWhole = std::abs(value - static_cast<double>(roundedInt)) < 0.0001;

    const auto integerIds = juce::StringArray
    {
        "voiceCount", "unisonVoices", "pitchBendRange", "stepSeqSteps", "subOctave",
        "arpOctaves", "bitDepth", "bitDownsample"
    };

    if (integerIds.contains(id) || (isWhole && std::abs(value) >= 100.0))
        return juce::String(roundedInt);

    int decimals = 2;
    if (std::abs(value) >= 1000.0)
        decimals = 0;
    else if (std::abs(value) >= 100.0)
        decimals = 1;
    else if (std::abs(value) >= 10.0)
        decimals = isWhole ? 0 : 1;

    auto text = juce::String(value, decimals);
    while (text.contains(".") && (text.endsWith("0") || text.endsWith(".")))
    {
        if (text.endsWith("."))
        {
            text = text.dropLastCharacters(1);
            break;
        }

        text = text.dropLastCharacters(1);
    }

    return text;
}

juce::Rectangle<int> getPanelBody(juce::Rectangle<int> bounds)
{
    return bounds.reduced(14, 12).withTrimmedTop(44);
}

void configureLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(0xffd7d9dd));
    label.setFont(juce::Font(juce::FontOptions(9.75f, juce::Font::bold)));
}

void styleHeaderButton(juce::TextButton& button, const AISynthAudioProcessorEditor::ThemePalette& theme, bool active)
{
    button.setColour(juce::TextButton::buttonColourId, theme.panelTop.brighter(active ? 0.12f : 0.02f));
    button.setColour(juce::TextButton::buttonOnColourId, theme.accent.withAlpha(0.85f));
    button.setColour(juce::TextButton::textColourOffId, theme.text);
    button.setColour(juce::TextButton::textColourOnId, theme.backgroundTop);
}

void drawWoodStrip(juce::Graphics& g, juce::Rectangle<int> area, const AISynthAudioProcessorEditor::ThemePalette& theme)
{
    juce::ColourGradient side(theme.panelTop.brighter(0.08f), area.getTopLeft().toFloat(),
                              theme.panelBottom.darker(0.3f), area.getBottomRight().toFloat(), false);
    g.setGradientFill(side);
    g.fillRoundedRectangle(area.toFloat(), 8.0f);

    for (int i = 0; i < area.getHeight(); i += 18)
    {
        const auto alpha = 0.08f + 0.06f * std::sin(static_cast<float>(i) * 0.17f);
        g.setColour(theme.accent.withAlpha(alpha));
        g.drawLine(static_cast<float>(area.getX() + 4), static_cast<float>(area.getY() + i),
                   static_cast<float>(area.getRight() - 4), static_cast<float>(area.getY() + i + 6), 1.0f);
    }
}

void drawScrew(juce::Graphics& g, juce::Point<float> centre, const AISynthAudioProcessorEditor::ThemePalette& theme)
{
    g.setColour(theme.wornMetalKnobs ? theme.knobOuter.darker(0.4f) : theme.panelBottom.darker(0.5f));
    g.fillEllipse(centre.x - 6.0f, centre.y - 6.0f, 12.0f, 12.0f);
    g.setColour(theme.secondary.withAlpha(0.55f));
    g.drawEllipse(centre.x - 5.0f, centre.y - 5.0f, 10.0f, 10.0f, 1.2f);
    g.drawLine(centre.x - 3.0f, centre.y, centre.x + 3.0f, centre.y, 1.0f);
}

void drawPanel(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title, const juce::String& subtitle,
               const AISynthAudioProcessorEditor::ThemePalette& theme, float animationPhase)
{
    auto area = bounds.toFloat();
    juce::ColourGradient panelGradient(theme.panelTop, area.getTopLeft(),
                                       theme.panelBottom, area.getBottomLeft(), false);
    g.setGradientFill(panelGradient);
    g.fillRoundedRectangle(area, theme.showGlass ? 14.0f : 12.0f);

    if (theme.showGlass)
    {
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.fillRoundedRectangle(area.removeFromTop(area.getHeight() * 0.32f), 12.0f);
        area = bounds.toFloat();
    }

    g.setColour(theme.border.withAlpha(theme.flatPanels ? 0.65f : 0.9f));
    g.drawRoundedRectangle(area.reduced(1.0f), 12.0f, theme.flatPanels ? 1.0f : 1.6f);

    if (! theme.flatPanels)
    {
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.drawRoundedRectangle(area.reduced(2.0f), 12.0f, 2.2f);
    }

    auto accentBar = area.removeFromTop(4.0f);
    juce::ColourGradient accentGradient(theme.accent, { accentBar.getX(), accentBar.getY() },
                                        theme.secondary, { accentBar.getRight(), accentBar.getBottom() }, false);
    g.setGradientFill(accentGradient);
    g.fillRoundedRectangle(accentBar, 10.0f);

    if (theme.showWarningStripes)
    {
        auto stripeArea = bounds.removeFromBottom(10).removeFromRight(120);
        for (int x = stripeArea.getX() - stripeArea.getHeight(); x < stripeArea.getRight(); x += 16)
        {
            g.setColour(theme.accent.withAlpha(0.14f));
            g.drawLine(static_cast<float>(x), static_cast<float>(stripeArea.getBottom()),
                       static_cast<float>(x + stripeArea.getHeight()), static_cast<float>(stripeArea.getY()), 5.0f);
        }
    }

    if (theme.glowAmount > 0.0f && ! theme.flatPanels)
    {
        const auto pulse = 0.55f + 0.45f * std::sin(animationPhase * 1.7f);
        g.setColour(theme.accent.withAlpha(theme.glowAmount * 0.18f * pulse));
        g.drawRoundedRectangle(bounds.toFloat().expanded(1.5f), 12.0f, 2.6f);
    }

    g.setColour(theme.text);
    g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
    g.drawText(title, bounds.reduced(16, 10).removeFromTop(22), juce::Justification::centredLeft);

    g.setColour(theme.mutedText);
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.drawText(subtitle, bounds.reduced(16, 10).withTrimmedTop(20).removeFromTop(18), juce::Justification::centredLeft);

    drawScrew(g, { area.getX() + 12.0f, area.getY() + 12.0f }, theme);
    drawScrew(g, { area.getRight() - 12.0f, area.getY() + 12.0f }, theme);
    drawScrew(g, { area.getX() + 12.0f, area.getBottom() - 12.0f }, theme);
    drawScrew(g, { area.getRight() - 12.0f, area.getBottom() - 12.0f }, theme);
}

void drawSubSection(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title,
                    const AISynthAudioProcessorEditor::ThemePalette& theme)
{
    auto area = bounds.toFloat().reduced(4.0f);
    g.setColour(theme.overlay.withAlpha(0.5f));
    g.fillRoundedRectangle(area, 10.0f);
    g.setColour(theme.border.withAlpha(0.4f));
    g.drawRoundedRectangle(area, 10.0f, 1.0f);
    g.setColour(theme.text);
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    g.drawText(title, bounds.reduced(10, 6).removeFromTop(18), juce::Justification::centredLeft);
}
}

EnvelopeDisplay::EnvelopeDisplay(AISynthAudioProcessor& processorRef, juce::String prefixToUse, juce::String titleToUse)
    : processor(processorRef), prefix(std::move(prefixToUse)), title(std::move(titleToUse))
{
    startTimerHz(24);
}

void EnvelopeDisplay::timerCallback()
{
    repaint();
}

void EnvelopeDisplay::paint(juce::Graphics& g)
{
    juce::Colour accent(0xfff0bf5a);
    juce::Colour glow(0x44f7b55e);
    juce::Colour text(0xffeff1f3);

    switch (processor.getCurrentThemeIndex())
    {
        case 0: accent = juce::Colour(0xff00f0ff); glow = juce::Colour(0x44ff2e88); text = juce::Colour(0xffe6f1ff); break;
        case 1: accent = juce::Colour(0xff8b0000); glow = juce::Colour(0x443a0000); text = juce::Colour(0xffd6d6d6); break;
        case 2: accent = juce::Colour(0xffff6a00); glow = juce::Colour(0x44aaaaaa); text = juce::Colour(0xfff0f0f0); break;
        case 3: accent = juce::Colour(0xff00ffcc); glow = juce::Colour(0x443a86ff); text = juce::Colour(0xffe0f7ff); break;
        case 4: accent = juce::Colour(0xfff5a623); glow = juce::Colour(0x44f5a623); text = juce::Colour(0xffffffff); break;
        default: break;
    }

    auto area = getLocalBounds().toFloat().reduced(4.0f);
    g.setColour(juce::Colour(0x88070a0d));
    g.fillRoundedRectangle(area, 10.0f);

    g.setColour(juce::Colour(0x22ffffff));
    for (int i = 1; i < 8; ++i)
    {
        const auto x = area.getX() + (area.getWidth() * static_cast<float>(i) / 8.0f);
        g.drawVerticalLine(static_cast<int>(x), area.getY(), area.getBottom());
    }

    for (int i = 1; i < 4; ++i)
    {
        const auto y = area.getY() + (area.getHeight() * static_cast<float>(i) / 4.0f);
        g.drawHorizontalLine(static_cast<int>(y), area.getX(), area.getRight());
    }

    const auto attack = processor.getParam(prefix + "Attack");
    const auto decay = processor.getParam(prefix + "Decay");
    const auto sustain = processor.getParam(prefix + "Sustain");
    const auto release = processor.getParam(prefix + "Release");

    const auto sum = attack + decay + release + 0.55f;
    const auto attackX = area.getX() + area.getWidth() * (attack / sum);
    const auto decayX = attackX + area.getWidth() * (decay / sum);
    const auto releaseX = area.getRight() - area.getWidth() * (release / sum);

    juce::Path envelope;
    envelope.startNewSubPath(area.getX(), area.getBottom());
    envelope.lineTo(attackX, area.getY() + area.getHeight() * 0.05f);
    envelope.lineTo(decayX, area.getBottom() - area.getHeight() * sustain);
    envelope.lineTo(releaseX, area.getBottom() - area.getHeight() * sustain);
    envelope.lineTo(area.getRight(), area.getBottom());

    g.setColour(glow);
    g.strokePath(envelope, juce::PathStrokeType(6.0f));
    g.setColour(accent);
    g.strokePath(envelope, juce::PathStrokeType(2.2f));

    g.setColour(text);
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    g.drawText(title, getLocalBounds().reduced(10, 8).removeFromTop(18), juce::Justification::topLeft);
}

void AISynthAudioProcessorEditor::SynthLookAndFeel::setTheme(const ThemePalette& newTheme, float newAnimationPhase)
{
    theme = newTheme;
    animationPhase = newAnimationPhase;
}

void AISynthAudioProcessorEditor::SynthLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                                                     float sliderPosProportional, float rotaryStartAngle,
                                                                     float rotaryEndAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)).reduced(4.0f, 2.0f);
    const auto diameter = juce::jmin(rotaryKnobDiameter, juce::jmin(bounds.getWidth(), bounds.getHeight()));
    auto knobBounds = juce::Rectangle<float>(diameter, diameter).withCentre(bounds.getCentre());
    const auto radius = knobBounds.getWidth() * 0.5f;
    const auto centre = knobBounds.getCentre();
    const auto angle = juce::jmap(sliderPosProportional, 0.0f, 1.0f, rotaryStartAngle, rotaryEndAngle);
    const auto glow = theme.glowAmount * (0.6f + 0.4f * std::sin(animationPhase * 2.0f));

    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.fillEllipse(knobBounds.translated(0.0f, 3.0f));

    if (theme.glowAmount > 0.0f)
    {
        g.setColour(theme.accent.withAlpha(glow * 0.2f));
        g.fillEllipse(knobBounds.expanded(6.0f));
    }

    g.setColour(theme.knobOuter);
    g.fillEllipse(knobBounds);
    g.setColour(theme.knobInner.darker(theme.wornMetalKnobs ? 0.2f : 0.0f));
    g.fillEllipse(knobBounds.reduced(radius * 0.13f));

    juce::ColourGradient knobFace(theme.knobHighlight, centre.x - radius * 0.3f, centre.y - radius * 0.7f,
                                  theme.knobInner, centre.x + radius * 0.4f, centre.y + radius * 0.7f, true);
    g.setGradientFill(knobFace);
    g.fillEllipse(knobBounds.reduced(radius * 0.18f));

    g.setColour(juce::Colours::white.withAlpha(theme.flatPanels ? 0.06f : 0.12f));
    g.fillEllipse(knobBounds.reduced(radius * 0.24f).withTrimmedBottom(radius * 0.8f));

    if (theme.wornMetalKnobs)
    {
        g.setColour(theme.secondary.withAlpha(0.12f));
        for (int groove = 0; groove < 12; ++groove)
        {
            const auto grooveAngle = juce::MathConstants<float>::twoPi * static_cast<float>(groove) / 12.0f;
            const auto p1 = centre + juce::Point<float>(std::cos(grooveAngle), std::sin(grooveAngle)) * radius * 0.65f;
            const auto p2 = centre + juce::Point<float>(std::cos(grooveAngle), std::sin(grooveAngle)) * radius * 0.9f;
            g.drawLine({ p1, p2 }, 1.0f);
        }
    }

    juce::Path marker;
    marker.addRoundedRectangle(-2.0f, -radius * 0.68f, 4.0f, radius * 0.34f, 2.0f);

    g.setColour(theme.accent);
    g.fillPath(marker, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

    g.setColour(theme.secondary.withAlpha(0.18f));
    for (int tick = 0; tick < 11; ++tick)
    {
        const auto tickPos = static_cast<float>(tick) / 10.0f;
        const auto tickAngle = juce::jmap(tickPos, 0.0f, 1.0f, rotaryStartAngle, rotaryEndAngle);
        const auto outer = centre + juce::Point<float>(std::cos(tickAngle), std::sin(tickAngle)) * radius * 1.02f;
        const auto inner = centre + juce::Point<float>(std::cos(tickAngle), std::sin(tickAngle)) * radius * 0.9f;
        g.drawLine({ inner, outer }, 1.0f);
    }
}

void AISynthAudioProcessorEditor::SynthLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                                                     float sliderPos, float minSliderPos, float maxSliderPos,
                                                                     const juce::Slider::SliderStyle style, juce::Slider&)
{
    juce::ignoreUnused(minSliderPos, maxSliderPos);

    auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)).reduced(8.0f);
    const auto isVertical = style == juce::Slider::LinearVertical || style == juce::Slider::LinearBarVertical;

    g.setColour(juce::Colour(0x44000000));
    g.fillRoundedRectangle(bounds, 8.0f);

    if (isVertical)
    {
        auto track = bounds.reduced(bounds.getWidth() * 0.34f, 8.0f);
        g.setColour(theme.sliderTrack);
        g.fillRoundedRectangle(track, track.getWidth() * 0.5f);

        auto fill = track.withTop(sliderPos);
        g.setColour(theme.accent);
        g.fillRoundedRectangle(fill, track.getWidth() * 0.5f);

        auto thumb = juce::Rectangle<float>(track.getX() - 7.0f, sliderPos - 8.0f, track.getWidth() + 14.0f, 16.0f);
        g.setColour(theme.sliderThumb);
        g.fillRoundedRectangle(thumb, 8.0f);
    }
    else
    {
        auto track = bounds.reduced(6.0f, bounds.getHeight() * 0.34f);
        g.setColour(theme.sliderTrack);
        g.fillRoundedRectangle(track, track.getHeight() * 0.5f);

        auto fill = track.withWidth(juce::jlimit(0.0f, track.getWidth(), sliderPos - track.getX()));
        g.setColour(theme.accent);
        g.fillRoundedRectangle(fill, track.getHeight() * 0.5f);

        g.setColour(theme.sliderThumb);
        g.fillEllipse(sliderPos - 8.0f, track.getCentreY() - 8.0f, 16.0f, 16.0f);
    }
}

void AISynthAudioProcessorEditor::SynthLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                                                 int buttonX, int buttonY, int buttonW, int buttonH,
                                                                 juce::ComboBox&)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    juce::ColourGradient gradient(theme.panelTop.brighter(0.08f), bounds.getTopLeft(),
                                  theme.panelBottom.darker(0.08f), bounds.getBottomLeft(), false);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(bounds, 8.0f);

    if (theme.glowAmount > 0.0f)
    {
        g.setColour(theme.secondary.withAlpha(theme.glowAmount * 0.18f));
        g.drawRoundedRectangle(bounds.expanded(1.0f), 8.0f, 2.0f);
    }

    g.setColour(theme.border);
    g.drawRoundedRectangle(bounds.reduced(1.0f), 8.0f, 1.2f);

    juce::Path arrow;
    arrow.startNewSubPath(static_cast<float>(buttonX + buttonW * 0.2f), static_cast<float>(buttonY + buttonH * 0.35f));
    arrow.lineTo(static_cast<float>(buttonX + buttonW * 0.5f), static_cast<float>(buttonY + buttonH * 0.65f));
    arrow.lineTo(static_cast<float>(buttonX + buttonW * 0.8f), static_cast<float>(buttonY + buttonH * 0.35f));

    g.setColour(theme.text);
    g.strokePath(arrow, juce::PathStrokeType(2.0f));
}

juce::Font AISynthAudioProcessorEditor::SynthLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return juce::Font(juce::FontOptions(11.0f, juce::Font::bold));
}

juce::Font AISynthAudioProcessorEditor::SynthLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    juce::ignoreUnused(buttonHeight);
    return juce::Font(juce::FontOptions(10.5f, juce::Font::bold));
}

juce::Label* AISynthAudioProcessorEditor::SynthLookAndFeel::createSliderTextBox(juce::Slider&)
{
    auto* label = new juce::Label();
    label->setColour(juce::Label::backgroundColourId, theme.panelBottom.darker(0.1f));
    label->setColour(juce::Label::outlineColourId, theme.border);
    label->setColour(juce::Label::textColourId, theme.text);
    label->setJustificationType(juce::Justification::centred);
    label->setBorderSize({ 1, 4, 1, 4 });
    label->setMinimumHorizontalScale(0.68f);
    label->setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    return label;
}

juce::Slider::SliderLayout AISynthAudioProcessorEditor::SynthLookAndFeel::getSliderLayout(juce::Slider& slider)
{
    auto layout = juce::LookAndFeel_V4::getSliderLayout(slider);
    const auto style = slider.getSliderStyle();
    const bool isRotary = style == juce::Slider::RotaryHorizontalVerticalDrag
        || style == juce::Slider::Rotary
        || style == juce::Slider::RotaryHorizontalDrag
        || style == juce::Slider::RotaryVerticalDrag;

    if (! isRotary || slider.getTextBoxPosition() != juce::Slider::TextBoxBelow)
        return layout;

    auto bounds = slider.getLocalBounds().reduced(4, 2);
    const auto textBoxHeight = juce::jmin(rotaryTextBoxHeight, juce::jmax(18, bounds.getHeight() / 4));
    const auto textBoxWidth = juce::jmin(rotaryTextBoxWidth, juce::jmax(50, bounds.getWidth() - 8));

    auto textBoxBounds = bounds.removeFromBottom(textBoxHeight);
    textBoxBounds = textBoxBounds.withSizeKeepingCentre(textBoxWidth, textBoxHeight);
    bounds.removeFromBottom(4);

    layout.sliderBounds = bounds;
    layout.textBoxBounds = textBoxBounds;
    return layout;
}

juce::String AISynthAudioProcessorEditor::getTopLevelGroupName(const juce::String& fullName)
{
    if (fullName.contains("/"))
        return fullName.upToFirstOccurrenceOf("/", false, false).trim();

    return fullName == "Init" ? "Factory" : "User";
}

juce::String AISynthAudioProcessorEditor::getLeafName(const juce::String& fullName)
{
    if (fullName.contains("/"))
        return fullName.fromLastOccurrenceOf("/", false, false).trim();

    return fullName.trim();
}

AISynthAudioProcessorEditor::AISynthAudioProcessorEditor(AISynthAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      ampEnvelopeDisplay(p, "env1", "AMP ENVELOPE"),
      modEnvelopeDisplay(p, "env2", "MOD ENVELOPE"),
      keyboardComponent(p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&synthLookAndFeel);
    setOpaque(true);
    setSize(1480, 880);

    for (int osc = 1; osc <= 3; ++osc)
    {
        const auto prefix = "osc" + juce::String(osc);
        addChoice(prefix + "Wave", "Wave", waveformItems, "Select the oscillator waveform.");
        addChoice(prefix + "PwmSource", "PWM Mod", pwmSourceItems, "Choose which modulator drives pulse width.");
        addToggle(prefix + "Bypass", "Bypass", "Mute this oscillator without changing its settings.");
        addSlider(prefix + "Level", "Level", "Set oscillator level.", juce::Slider::RotaryHorizontalVerticalDrag);
        addSlider(prefix + "Coarse", "Coarse", "Transpose this oscillator in semitones.", juce::Slider::RotaryHorizontalVerticalDrag);
        addSlider(prefix + "Fine", "Fine", "Fine tune this oscillator in cents.", juce::Slider::RotaryHorizontalVerticalDrag);
        addSlider(prefix + "PulseWidth", "Pulse Width", "Set the base pulse width / waveform skew.", juce::Slider::RotaryHorizontalVerticalDrag);
        addSlider(prefix + "PwmAmount", "PWM Amt", "Set how much the selected source moves pulse width.", juce::Slider::RotaryHorizontalVerticalDrag);

        if (osc > 1)
            addChoice(prefix + "SyncSource", "Sync", syncSourceItems, "Hard-sync this oscillator from another oscillator.");
    }

    addChoice("subWave", "Wave", subWaveformItems, "Select the sub oscillator waveform.");
    addChoice("subPwmSource", "PWM Mod", pwmSourceItems, "Choose which modulator drives sub pulse width.");
    addToggle("subBypass", "Bypass", "Mute the sub oscillator without changing its settings.");
    addSlider("subLevel", "Level", "Sub oscillator level.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("subOctave", "Octave", "Transpose the sub oscillator down by octaves.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("subPulseWidth", "Pulse Width", "Set the base pulse width / waveform skew for the sub oscillator.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("subPwmAmount", "PWM Amt", "Set how much the selected source moves sub pulse width.", juce::Slider::RotaryHorizontalVerticalDrag);

    addChoice("filterType", "Type", filterItems, "Choose the filter response.");
    addSlider("cutoff", "Cutoff", "Filter cutoff frequency.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("resonance", "Reso", "Filter resonance.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("drive", "Drive", "Push the voice into warm filter drive.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("filterAccent", "Accent", "TB-style accent response driven by note and step velocity.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("env1ToFilter", "Env 1 > Cut", "How much the amp envelope opens the filter.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("env2ToFilter", "Env 2 > Cut", "How much the second envelope drives the filter.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("masterGain", "Master", "Final output gain.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("pitchBendRange", "PB Range", "Pitch bend range in semitones.", juce::Slider::RotaryHorizontalVerticalDrag);
    addChoice("keyMode", "Key Mode", keyModeItems, "Choose polyphonic or monophonic key behavior.");
    addChoice("portamentoMode", "Glide Mode", portamentoModeItems, "Choose how portamento behaves when notes change.");
    addSlider("portamentoTime", "Glide", "Set the portamento glide time.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("voiceCount", "Polyphony", "Set the number of playable synth voices.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("unisonVoices", "Voices", "Number of stacked unison voices.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("unisonDetune", "Detune", "Spread unison voices in cents.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("stereoWidth", "Width", "Stereo width of the unison stack.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("velocitySensitivity", "Vel Sens", "Set how strongly note velocity affects loudness.", juce::Slider::RotaryHorizontalVerticalDrag);
    addChoice("velocityDestination", "Vel Dest", matrixDestinationItems, "Assign velocity to an extra destination.");
    addSlider("velocityAmount", "Vel Amt", "Set the depth of the dedicated velocity routing.", juce::Slider::RotaryHorizontalVerticalDrag);

    addSlider("env1Attack", "Attack", "Amp envelope attack.", juce::Slider::LinearVertical);
    addSlider("env1Decay", "Decay", "Amp envelope decay.", juce::Slider::LinearVertical);
    addSlider("env1Sustain", "Sustain", "Amp envelope sustain.", juce::Slider::LinearVertical);
    addSlider("env1Release", "Release", "Amp envelope release.", juce::Slider::LinearVertical);

    addSlider("env2Attack", "Attack", "Mod envelope attack.", juce::Slider::LinearVertical);
    addSlider("env2Decay", "Decay", "Mod envelope decay.", juce::Slider::LinearVertical);
    addSlider("env2Sustain", "Sustain", "Mod envelope sustain.", juce::Slider::LinearVertical);
    addSlider("env2Release", "Release", "Mod envelope release.", juce::Slider::LinearVertical);
    addSlider("env2ToPitch", "Env 2 > Pitch", "Route the second envelope into oscillator pitch.", juce::Slider::RotaryHorizontalVerticalDrag);

    for (int lfo = 1; lfo <= 3; ++lfo)
    {
        addChoice("lfo" + juce::String(lfo) + "Shape", "Shape", lfoShapeItems, "Choose the LFO waveform.");
        addSlider("lfo" + juce::String(lfo) + "Rate", "Rate", "LFO speed in Hz.", juce::Slider::RotaryHorizontalVerticalDrag);
        addSlider("lfo" + juce::String(lfo) + "Depth", "Depth", "LFO depth.", juce::Slider::RotaryHorizontalVerticalDrag);
    }

    addToggle("arpEnabled", "Arp On", "Enable the arpeggiator.");
    addToggle("arpLatch", "Latch", "Keep the arpeggiator running after keys are released.");
    addToggle("rhythmGateEnabled", "Gate On", "Enable the rhythm gate.");
    addChoice("arpMode", "Arp Mode", arpModeItems, "Choose the arpeggiator direction.");
    addChoice("rhythmGatePattern", "Gate Pattern", gatePatternItems, "Choose the rhythm gate pattern.");
    addSlider("arpRate", "Arp Rate", "Arpeggiator speed in steps per second.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("arpGate", "Arp Gate", "Set how long each arpeggiated note stays open.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("arpOctaves", "Octaves", "Add extra octaves to the arpeggiator.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("rhythmGateRate", "Gate Rate", "Rhythm gate speed in steps per second.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("rhythmGateDepth", "Gate Depth", "How hard the rhythm gate chops the sound.", juce::Slider::RotaryHorizontalVerticalDrag);

    addToggle("stepSeqEnabled", "Seq On", "Enable the step sequencer.");
    addToggle("stepSeqSync", "Host Sync", "Sync the step sequencer to the host transport.");
    addChoice("stepSeqLength", "Step Len", timingDivisionItems, "Choose the sequencer step length.");
    addSlider("stepSeqBpm", "Seq BPM", "Free-running BPM for the sequencer when host sync is off.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("stepSeqSteps", "Seq Steps", "Choose how many steps are active in the pattern.", juce::Slider::RotaryHorizontalVerticalDrag);

    for (int step = 1; step <= 8; ++step)
    {
        addSlider("seqStep" + juce::String(step) + "Note", "Pitch " + juce::String(step), "Step pitch offset in semitones.", juce::Slider::LinearVertical);
        addSlider("seqStep" + juce::String(step) + "Gate", "Gate " + juce::String(step), "Step gate amount.", juce::Slider::LinearVertical);
        addSlider("seqStep" + juce::String(step) + "Velocity", "Vel " + juce::String(step), "Step velocity depth.", juce::Slider::LinearVertical);
    }

    addToggle("delayBypass", "Bypass", "Disable the delay block.");
    addToggle("delaySync", "Delay Sync", "Sync delay timing to the host transport.");
    addChoice("delayDivision", "Delay Div", timingDivisionItems, "Choose the delay note value when sync is enabled.");
    addSlider("delayMix", "Delay Mix", "Delay dry/wet balance.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("delayFeedback", "Feedback", "Delay feedback amount.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("delayTime", "Delay Time", "Free-running delay time in seconds.", juce::Slider::RotaryHorizontalVerticalDrag);

    addToggle("chorusBypass", "Bypass", "Disable the chorus block.");
    addSlider("chorusMix", "Chorus Mix", "Chorus dry/wet balance.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("chorusRate", "Chorus Rate", "Chorus rate.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("chorusDepth", "Chorus Depth", "Chorus depth.", juce::Slider::RotaryHorizontalVerticalDrag);
    addToggle("driveBypass", "Bypass", "Disable the distortion and saturation block.");
    addSlider("distortionAmount", "Distortion", "Drive into distortion.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("saturationAmount", "Saturation", "Soft clipping and analogue-style thickening.", juce::Slider::RotaryHorizontalVerticalDrag);
    addToggle("compressorBypass", "Bypass", "Disable the compressor block.");
    addSlider("compressorThreshold", "Comp Thresh", "Compressor threshold in dB.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("compressorRatio", "Comp Ratio", "Compressor ratio.", juce::Slider::RotaryHorizontalVerticalDrag);
    addToggle("reverbBypass", "Bypass", "Disable the reverb block.");
    addSlider("reverbMix", "Reverb Mix", "Reverb dry/wet balance.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("reverbSize", "Reverb Size", "Reverb room size.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("reverbDamping", "Reverb Damp", "Reverb damping.", juce::Slider::RotaryHorizontalVerticalDrag);
    addToggle("bitcrusherBypass", "Bypass", "Disable the bitcrusher block.");
    addSlider("bitcrusherMix", "Bit Mix", "Bitcrusher dry/wet balance.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("bitDepth", "Bit Depth", "Bit depth of the crusher.", juce::Slider::RotaryHorizontalVerticalDrag);
    addSlider("bitDownsample", "Downsample", "Downsample factor for the crusher.", juce::Slider::RotaryHorizontalVerticalDrag);

    for (int slot = 1; slot <= 4; ++slot)
    {
        addChoice("matrixSource" + juce::String(slot), "Source " + juce::String(slot), matrixSourceItems, "Select the modulation source.");
        addChoice("matrixDest" + juce::String(slot), "Destination " + juce::String(slot), matrixDestinationItems, "Select the modulation destination.");
        addSlider("matrixAmount" + juce::String(slot), "Amount " + juce::String(slot), "Set positive or negative modulation depth.", juce::Slider::LinearHorizontal);
    }

    addAndMakeVisible(ampEnvelopeDisplay);
    addAndMakeVisible(modEnvelopeDisplay);

    keyboardComponent.setAvailableRange(24, 96);
    keyboardComponent.setKeyWidth(24.0f);
    keyboardComponent.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xfff2eadb));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xff1a1d21));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour(0xff4b5662));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour(0x55d59b45));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour(0x88f0bf5a));
    keyboardComponent.setWantsKeyboardFocus(true);
    addAndMakeVisible(keyboardComponent);

    configureLabel(presetNameLabel, "PRESET");
    addAndMakeVisible(presetNameLabel);

    configureLabel(presetGroupLabel, "GROUP");
    addAndMakeVisible(presetGroupLabel);

    configureLabel(themeLabel, "THEME");
    addAndMakeVisible(themeLabel);

    presetGroupCombo.setTooltip("Filter presets by bank or category.");
    presetGroupCombo.onChange = [this]
    {
        if (! updatingPresetCombo)
        {
            juce::Component::SafePointer<AISynthAudioProcessorEditor> safeThis(this);
            juce::Timer::callAfterDelay(80, [safeThis]
            {
                if (safeThis != nullptr)
                    safeThis->refreshPresetCombo(true, false);
            });
        }
    };
    addAndMakeVisible(presetGroupCombo);

    presetSearchEditor.setTooltip("Search presets by name.");
    presetSearchEditor.setFont(juce::Font(juce::FontOptions(11.5f)));
    presetSearchEditor.setTextToShowWhenEmpty("Search presets", currentTheme.mutedText);
    presetSearchEditor.onTextChange = [this]
    {
        if (! updatingPresetCombo)
            refreshPresetCombo(true, false);
    };
    addAndMakeVisible(presetSearchEditor);

    presetCombo.setTooltip("Load one of the factory or saved user presets.");
    presetCombo.onChange = [this]
    {
        if (updatingPresetCombo)
            return;

        const auto selectedIndex = presetCombo.getSelectedItemIndex();
        if (juce::isPositiveAndBelow(selectedIndex, filteredPresetNames.size()))
        {
            const auto presetName = filteredPresetNames[selectedIndex];
            juce::Component::SafePointer<AISynthAudioProcessorEditor> safeThis(this);
            juce::Timer::callAfterDelay(80, [safeThis, presetName]
            {
                if (safeThis == nullptr)
                    return;

                if (safeThis->audioProcessor.loadPresetByName(presetName))
                {
                    safeThis->presetNameEditor.setText(getLeafName(safeThis->audioProcessor.getCurrentPresetName()), juce::dontSendNotification);
                    safeThis->refreshPatternCombo(false);
                    safeThis->refreshMidiLearnDecorations();
                    safeThis->applyTheme();
                }
            });
        }
    };
    addAndMakeVisible(presetCombo);

    presetNameEditor.setTooltip("Name used when saving a new preset.");
    presetNameEditor.setFont(juce::Font(juce::FontOptions(11.5f)));
    presetNameEditor.setText(getLeafName(audioProcessor.getCurrentPresetName()), juce::dontSendNotification);
    presetNameEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff101317));
    presetNameEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff666d74));
    presetNameEditor.setColour(juce::TextEditor::textColourId, juce::Colour(0xffedf1f5));
    presetNameEditor.setTextToShowWhenEmpty("Save current sound as...", juce::Colour(0x88edf1f5));
    addAndMakeVisible(presetNameEditor);

    themeCombo.addItemList(themeItems, 1);
    themeCombo.setSelectedItemIndex(audioProcessor.getCurrentThemeIndex(), juce::dontSendNotification);
    themeCombo.onChange = [this]
    {
        audioProcessor.setCurrentThemeIndex(themeCombo.getSelectedItemIndex());
        applyTheme();
    };
    addAndMakeVisible(themeCombo);

    savePresetButton.setButtonText("Save");
    reloadPresetButton.setButtonText("Refresh");
    initPresetButton.setButtonText("Init");
    presetMenuButton.setButtonText("Preset Menu");
    presetMenuButton.setTooltip("Open preset actions, including clearing MIDI learn assignments for this preset.");
    savePresetButton.onClick = [this] { savePresetFromEditor(); };
    reloadPresetButton.onClick = [this]
    {
        audioProcessor.refreshPresetList();
        refreshPresetCombo(true);
    };
    initPresetButton.onClick = [this]
    {
        juce::Component::SafePointer<AISynthAudioProcessorEditor> safeThis(this);
        juce::Timer::callAfterDelay(80, [safeThis]
        {
            if (safeThis == nullptr)
                return;

            safeThis->audioProcessor.loadPresetByName("Init");
            safeThis->refreshPatternCombo(false);
            safeThis->refreshPresetCombo(false);
            safeThis->refreshMidiLearnDecorations();
            safeThis->applyTheme();
        });
    };
    presetMenuButton.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible(savePresetButton);
    addAndMakeVisible(reloadPresetButton);
    addAndMakeVisible(initPresetButton);
    addAndMakeVisible(presetMenuButton);

    configureLabel(patternLabel, "PATTERN");
    addAndMakeVisible(patternLabel);

    patternCombo.setTooltip("Load a factory or saved step-sequencer pattern.");
    patternCombo.onChange = [this]
    {
        if (updatingPatternCombo)
            return;

        const auto selectedIndex = patternCombo.getSelectedItemIndex();
        if (juce::isPositiveAndBelow(selectedIndex, filteredPatternNames.size()))
        {
            const auto patternName = filteredPatternNames[selectedIndex];
            juce::Component::SafePointer<AISynthAudioProcessorEditor> safeThis(this);
            juce::Timer::callAfterDelay(80, [safeThis, patternName]
            {
                if (safeThis == nullptr)
                    return;

                if (safeThis->audioProcessor.loadPatternByName(patternName))
                    safeThis->patternNameEditor.setText(getLeafName(safeThis->audioProcessor.getCurrentPatternName()), juce::dontSendNotification);
            });
        }
    };
    addAndMakeVisible(patternCombo);

    patternNameEditor.setTooltip("Name used when saving a sequencer pattern.");
    patternNameEditor.setFont(juce::Font(juce::FontOptions(11.5f)));
    patternNameEditor.setText(getLeafName(audioProcessor.getCurrentPatternName()), juce::dontSendNotification);
    patternNameEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff101317));
    patternNameEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff666d74));
    patternNameEditor.setColour(juce::TextEditor::textColourId, juce::Colour(0xffedf1f5));
    patternNameEditor.setTextToShowWhenEmpty("Save sequencer pattern as...", juce::Colour(0x88edf1f5));
    addAndMakeVisible(patternNameEditor);

    savePatternButton.setButtonText("Save Pattern");
    reloadPatternButton.setButtonText("Refresh");
    savePatternButton.onClick = [this] { savePatternFromEditor(); };
    reloadPatternButton.onClick = [this]
    {
        audioProcessor.refreshPatternList();
        refreshPatternCombo(true);
    };
    addAndMakeVisible(savePatternButton);
    addAndMakeVisible(reloadPatternButton);

    voiceTabButton.setButtonText("Voice");
    modulationTabButton.setButtonText("Mod");
    effectsTabButton.setButtonText("FX / Matrix");
    sequencerTabButton.setButtonText("Sequencer");
    osc1TabButton.setButtonText("OSC 1");
    osc2TabButton.setButtonText("OSC 2");
    osc3TabButton.setButtonText("OSC 3");
    subTabButton.setButtonText("SUB");
    delayFxTabButton.setButtonText("Delay");
    chorusFxTabButton.setButtonText("Chorus");
    driveFxTabButton.setButtonText("Drive");
    dynamicsFxTabButton.setButtonText("Dynamics");
    reverbFxTabButton.setButtonText("Reverb");
    crusherFxTabButton.setButtonText("Crusher");
    voiceFilterTabButton.setButtonText("Tone");
    voiceArpTabButton.setButtonText("Arp");

    for (auto* button : { &voiceTabButton, &modulationTabButton, &effectsTabButton, &sequencerTabButton,
                          &osc1TabButton, &osc2TabButton, &osc3TabButton, &subTabButton,
                          &delayFxTabButton, &chorusFxTabButton, &driveFxTabButton,
                          &dynamicsFxTabButton, &reverbFxTabButton, &crusherFxTabButton,
                          &voiceFilterTabButton, &voiceArpTabButton })
    {
        button->setClickingTogglesState(false);
        addAndMakeVisible(*button);
    }

    voiceTabButton.onClick = [this] { setActiveTab(ActiveTab::voice); };
    modulationTabButton.onClick = [this] { setActiveTab(ActiveTab::modulation); };
    effectsTabButton.onClick = [this] { setActiveTab(ActiveTab::effects); };
    sequencerTabButton.onClick = [this] { setActiveTab(ActiveTab::sequencer); };
    osc1TabButton.onClick = [this]
    {
        oscillatorDetailTab = OscillatorDetailTab::osc1;
        applyTheme();
        resized();
        repaint();
    };
    osc2TabButton.onClick = [this]
    {
        oscillatorDetailTab = OscillatorDetailTab::osc2;
        applyTheme();
        resized();
        repaint();
    };
    osc3TabButton.onClick = [this]
    {
        oscillatorDetailTab = OscillatorDetailTab::osc3;
        applyTheme();
        resized();
        repaint();
    };
    subTabButton.onClick = [this]
    {
        oscillatorDetailTab = OscillatorDetailTab::sub;
        applyTheme();
        resized();
        repaint();
    };
    delayFxTabButton.onClick = [this]
    {
        effectsDetailTab = EffectsDetailTab::delay;
        applyTheme();
        resized();
        repaint();
    };
    chorusFxTabButton.onClick = [this]
    {
        effectsDetailTab = EffectsDetailTab::chorus;
        applyTheme();
        resized();
        repaint();
    };
    driveFxTabButton.onClick = [this]
    {
        effectsDetailTab = EffectsDetailTab::drive;
        applyTheme();
        resized();
        repaint();
    };
    dynamicsFxTabButton.onClick = [this]
    {
        effectsDetailTab = EffectsDetailTab::dynamics;
        applyTheme();
        resized();
        repaint();
    };
    reverbFxTabButton.onClick = [this]
    {
        effectsDetailTab = EffectsDetailTab::reverb;
        applyTheme();
        resized();
        repaint();
    };
    crusherFxTabButton.onClick = [this]
    {
        effectsDetailTab = EffectsDetailTab::crusher;
        applyTheme();
        resized();
        repaint();
    };
    voiceFilterTabButton.onClick = [this]
    {
        voiceDetailTab = VoiceDetailTab::filter;
        applyTheme();
        resized();
        repaint();
    };
    voiceArpTabButton.onClick = [this]
    {
        voiceDetailTab = VoiceDetailTab::arp;
        applyTheme();
        resized();
        repaint();
    };

    stylePerformanceSlider(pitchWheelSlider, pitchWheelLabel, "PITCH", true);
    stylePerformanceSlider(modWheelSlider, modWheelLabel, "MOD", false);

    pitchWheelSlider.setRange(-1.0, 1.0, 0.001);
    pitchWheelSlider.setDoubleClickReturnValue(true, 0.0);
    pitchWheelSlider.onValueChange = [this] { sendPitchWheelMessage(); };
    pitchWheelSlider.onDragEnd = [this]
    {
        pitchWheelSlider.setValue(0.0, juce::sendNotificationSync);
        sendPitchWheelMessage();
    };

    modWheelSlider.setRange(0.0, 1.0, 0.001);
    modWheelSlider.setValue(audioProcessor.getCurrentModWheelValue(), juce::dontSendNotification);
    modWheelSlider.onValueChange = [this] { sendModWheelMessage(); };

    refreshPatternCombo(false);
    refreshPresetCombo(false);
    setActiveTab(ActiveTab::voice);
    applyTheme();
    startTimerHz(24);
    resized();
}

AISynthAudioProcessorEditor::~AISynthAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

juce::Slider& AISynthAudioProcessorEditor::addSlider(const juce::String& id, const juce::String& title, const juce::String& tooltip, juce::Slider::SliderStyle style)
{
    auto* slider = sliders.add(new MidiLearnSlider());
    slider->setSliderStyle(style);

    const auto isHorizontal = style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar;
    slider->setTextBoxStyle(isHorizontal ? juce::Slider::TextBoxRight : juce::Slider::TextBoxBelow,
                            false,
                            isHorizontal ? 60 : rotaryTextBoxWidth,
                            rotaryTextBoxHeight);
    slider->setTooltip(tooltip);
    slider->textFromValueFunction = [id] (double value)
    {
        return formatSliderValue(id, value);
    };
    slider->valueFromTextFunction = [] (const juce::String& text)
    {
        return text.getDoubleValue();
    };
    addAndMakeVisible(slider);

    if (auto* midiSlider = dynamic_cast<MidiLearnSlider*>(slider))
        midiSlider->onContextMenuRequested = [this, slider, id, title] { showSliderMidiLearnMenu(id, *slider, title); };

    auto* label = labels.add(new juce::Label());
    configureLabel(*label, title.toUpperCase());
    addAndMakeVisible(label);

    sliderAttachments.push_back(std::make_unique<SliderAttachment>(audioProcessor.apvts, id, *slider));
    sliderMetas.push_back({ id, slider, label, tooltip });
    return *slider;
}

juce::ComboBox& AISynthAudioProcessorEditor::addChoice(const juce::String& id, const juce::String& title, const juce::StringArray& items, const juce::String& tooltip)
{
    auto* combo = comboBoxes.add(new juce::ComboBox());
    combo->addItemList(items, 1);
    combo->setTooltip(tooltip);
    addAndMakeVisible(combo);

    auto* label = labels.add(new juce::Label());
    configureLabel(*label, title.toUpperCase());
    addAndMakeVisible(label);

    comboAttachments.push_back(std::make_unique<ComboAttachment>(audioProcessor.apvts, id, *combo));
    choiceMetas.push_back({ id, combo, label });
    return *combo;
}

juce::ToggleButton& AISynthAudioProcessorEditor::addToggle(const juce::String& id, const juce::String& title, const juce::String& tooltip)
{
    auto* button = toggles.add(new juce::ToggleButton(title));
    button->setTooltip(tooltip);
    button->setColour(juce::ToggleButton::textColourId, juce::Colour(0xffeceff3));
    addAndMakeVisible(button);

    buttonAttachments.push_back(std::make_unique<ButtonAttachment>(audioProcessor.apvts, id, *button));
    toggleMetas.push_back({ id, button });
    return *button;
}

AISynthAudioProcessorEditor::SliderMeta* AISynthAudioProcessorEditor::findSlider(const juce::String& id)
{
    for (auto& meta : sliderMetas)
        if (meta.id == id)
            return &meta;

    return nullptr;
}

AISynthAudioProcessorEditor::ChoiceMeta* AISynthAudioProcessorEditor::findChoice(const juce::String& id)
{
    for (auto& meta : choiceMetas)
        if (meta.id == id)
            return &meta;

    return nullptr;
}

AISynthAudioProcessorEditor::ToggleMeta* AISynthAudioProcessorEditor::findToggle(const juce::String& id)
{
    for (auto& meta : toggleMetas)
        if (meta.id == id)
            return &meta;

    return nullptr;
}

void AISynthAudioProcessorEditor::layoutKnobGrid(juce::Rectangle<int> area, std::initializer_list<const char*> ids, int columns,
                                                 int labelHeight, int cellPadding, float labelFontSize, int minimumRows)
{
    const auto count = static_cast<int>(ids.size());
    if (count == 0)
        return;

    const auto rows = juce::jmax(juce::jmax(1, (count + columns - 1) / columns), minimumRows);
    const auto cellW = area.getWidth() / columns;
    const auto cellH = area.getHeight() / rows;

    int index = 0;
    for (const auto* id : ids)
    {
        if (auto* meta = findSlider(id))
        {
            auto cell = juce::Rectangle<int>(area.getX() + (index % columns) * cellW,
                                             area.getY() + (index / columns) * cellH,
                                             cellW, cellH).reduced(cellPadding, 2);
            meta->slider->setVisible(true);
            meta->label->setVisible(true);
            meta->label->setFont(juce::Font(juce::FontOptions(labelFontSize, juce::Font::bold)));
            meta->label->setBounds(cell.removeFromTop(labelHeight));

            auto sliderBounds = cell.reduced(2, 0);
            const auto sliderWidth = juce::jmin(sliderBounds.getWidth(), rotaryControlWidth);
            const auto sliderHeight = juce::jmin(sliderBounds.getHeight(), rotaryControlHeight);
            meta->slider->setBounds(sliderBounds.withSizeKeepingCentre(sliderWidth, sliderHeight));
        }

        ++index;
    }
}

void AISynthAudioProcessorEditor::layoutChoiceRow(juce::Rectangle<int> area, std::initializer_list<const char*> ids)
{
    if (ids.size() == 0)
        return;

    const auto width = area.getWidth() / static_cast<int>(ids.size());
    int x = area.getX();

    for (const auto* id : ids)
    {
        if (auto* meta = findChoice(id))
        {
            auto cell = juce::Rectangle<int>(x, area.getY(), width, area.getHeight()).reduced(4);
            meta->combo->setVisible(true);
            meta->label->setVisible(true);
            meta->label->setBounds(cell.removeFromTop(15));
            meta->combo->setBounds(cell.removeFromTop(30));
        }

        x += width;
    }
}

void AISynthAudioProcessorEditor::layoutToggleRow(juce::Rectangle<int> area, std::initializer_list<const char*> ids)
{
    if (ids.size() == 0)
        return;

    const auto width = area.getWidth() / static_cast<int>(ids.size());
    int x = area.getX();

    for (const auto* id : ids)
    {
        if (auto* meta = findToggle(id))
        {
            meta->button->setVisible(true);
            meta->button->setBounds(juce::Rectangle<int>(x, area.getY(), width, area.getHeight()).reduced(4));
        }

        x += width;
    }
}

void AISynthAudioProcessorEditor::layoutVerticalSliderBank(juce::Rectangle<int> area, std::initializer_list<const char*> ids)
{
    if (ids.size() == 0)
        return;

    const auto width = area.getWidth() / static_cast<int>(ids.size());
    int x = area.getX();

    for (const auto* id : ids)
    {
        if (auto* meta = findSlider(id))
        {
            auto cell = juce::Rectangle<int>(x, area.getY(), width, area.getHeight()).reduced(4);
            meta->slider->setVisible(true);
            meta->label->setVisible(true);
            meta->label->setBounds(cell.removeFromTop(18));
            meta->slider->setBounds(cell);
        }

        x += width;
    }
}

void AISynthAudioProcessorEditor::layoutMatrixRows(juce::Rectangle<int> area)
{
    auto body = getPanelBody(area);
    const auto rowHeight = body.getHeight() / 4;

    for (int slot = 1; slot <= 4; ++slot)
    {
        auto row = body.removeFromTop(rowHeight).reduced(0, 2);
        const auto labelHeight = 18;
        const auto sourceWidth = static_cast<int>(row.getWidth() * 0.32f);
        const auto destWidth = static_cast<int>(row.getWidth() * 0.32f);

        auto sourceArea = row.removeFromLeft(sourceWidth).reduced(4, 0);
        auto destArea = row.removeFromLeft(destWidth).reduced(4, 0);
        auto amountArea = row.reduced(4, 0);

        if (auto* source = findChoice("matrixSource" + juce::String(slot)))
        {
            source->combo->setVisible(true);
            source->label->setVisible(true);
            source->label->setBounds(sourceArea.removeFromTop(labelHeight));
            source->combo->setBounds(sourceArea.removeFromTop(30));
        }

        if (auto* destination = findChoice("matrixDest" + juce::String(slot)))
        {
            destination->combo->setVisible(true);
            destination->label->setVisible(true);
            destination->label->setBounds(destArea.removeFromTop(labelHeight));
            destination->combo->setBounds(destArea.removeFromTop(30));
        }

        if (auto* amount = findSlider("matrixAmount" + juce::String(slot)))
        {
            amount->slider->setVisible(true);
            amount->label->setVisible(true);
            amount->label->setBounds(amountArea.removeFromTop(labelHeight));
            amount->slider->setBounds(amountArea.withTrimmedTop(2));
        }
    }
}

void AISynthAudioProcessorEditor::layoutOscillatorSections(juce::Rectangle<int> area)
{
    auto body = getPanelBody(area);
    oscillatorSectionBounds = {};
    oscillatorTabsBounds = body.removeFromTop(34);

    auto oscTabs = oscillatorTabsBounds.reduced(2, 2);
    const auto tabGap = 6;
    const auto tabWidth = (oscTabs.getWidth() - tabGap * 3) / 4;
    osc1TabButton.setBounds(oscTabs.removeFromLeft(tabWidth));
    oscTabs.removeFromLeft(tabGap);
    osc2TabButton.setBounds(oscTabs.removeFromLeft(tabWidth));
    oscTabs.removeFromLeft(tabGap);
    osc3TabButton.setBounds(oscTabs.removeFromLeft(tabWidth));
    oscTabs.removeFromLeft(tabGap);
    subTabButton.setBounds(oscTabs);

    body.removeFromTop(8);
    auto section = body;
    const auto activeIndex = [&]
    {
        switch (oscillatorDetailTab)
        {
            case OscillatorDetailTab::osc2: return 1;
            case OscillatorDetailTab::osc3: return 2;
            case OscillatorDetailTab::sub:  return 3;
            case OscillatorDetailTab::osc1:
            default:                        return 0;
        }
    }();

    oscillatorSectionBounds[static_cast<size_t>(activeIndex)] = section;
    auto inner = section.reduced(12, 26);

    if (oscillatorDetailTab == OscillatorDetailTab::osc1)
    {
        layoutToggleRow(inner.removeFromTop(28).removeFromRight(140), { "osc1Bypass" });
        inner.removeFromTop(4);
        layoutChoiceRow(inner.removeFromTop(54), { "osc1Wave", "osc1PwmSource" });
        inner.removeFromTop(6);
        layoutKnobGrid(inner, { "osc1Level", "osc1Coarse", "osc1Fine", "osc1PulseWidth", "osc1PwmAmount" }, 3, 16, 2, 10.0f, 2);
    }
    else if (oscillatorDetailTab == OscillatorDetailTab::osc2)
    {
        layoutToggleRow(inner.removeFromTop(28).removeFromRight(140), { "osc2Bypass" });
        inner.removeFromTop(4);
        layoutChoiceRow(inner.removeFromTop(54), { "osc2Wave", "osc2PwmSource" });
        inner.removeFromTop(4);
        layoutChoiceRow(inner.removeFromTop(50), { "osc2SyncSource" });
        inner.removeFromTop(6);
        layoutKnobGrid(inner, { "osc2Level", "osc2Coarse", "osc2Fine", "osc2PulseWidth", "osc2PwmAmount" }, 3, 16, 2, 10.0f, 2);
    }
    else if (oscillatorDetailTab == OscillatorDetailTab::osc3)
    {
        layoutToggleRow(inner.removeFromTop(28).removeFromRight(140), { "osc3Bypass" });
        inner.removeFromTop(4);
        layoutChoiceRow(inner.removeFromTop(54), { "osc3Wave", "osc3PwmSource" });
        inner.removeFromTop(4);
        layoutChoiceRow(inner.removeFromTop(50), { "osc3SyncSource" });
        inner.removeFromTop(6);
        layoutKnobGrid(inner, { "osc3Level", "osc3Coarse", "osc3Fine", "osc3PulseWidth", "osc3PwmAmount" }, 3, 16, 2, 10.0f, 2);
    }
    else
    {
        layoutToggleRow(inner.removeFromTop(28).removeFromRight(140), { "subBypass" });
        inner.removeFromTop(4);
        layoutChoiceRow(inner.removeFromTop(54), { "subWave", "subPwmSource" });
        inner.removeFromTop(60);
        layoutKnobGrid(inner, { "subLevel", "subOctave", "subPulseWidth", "subPwmAmount" }, 2, 16, 2, 10.0f, 2);
    }
}

void AISynthAudioProcessorEditor::layoutLfoSections(juce::Rectangle<int> area)
{
    auto body = getPanelBody(area);
    const auto gap = 8;
    const auto sectionWidth = (body.getWidth() - gap * 2) / 3;

    for (int index = 0; index < 3; ++index)
    {
        auto section = body.removeFromLeft(sectionWidth);
        lfoSectionBounds[static_cast<size_t>(index)] = section;
        if (index < 2)
            body.removeFromLeft(gap);

        auto inner = section.reduced(8, 26);
        if (index == 0)
        {
            layoutChoiceRow(inner.removeFromTop(54), { "lfo1Shape" });
            layoutKnobGrid(inner, { "lfo1Rate", "lfo1Depth" }, 2);
        }
        else if (index == 1)
        {
            layoutChoiceRow(inner.removeFromTop(54), { "lfo2Shape" });
            layoutKnobGrid(inner, { "lfo2Rate", "lfo2Depth" }, 2);
        }
        else
        {
            layoutChoiceRow(inner.removeFromTop(54), { "lfo3Shape" });
            layoutKnobGrid(inner, { "lfo3Rate", "lfo3Depth" }, 2);
        }
    }
}

void AISynthAudioProcessorEditor::layoutPresetPanel(juce::Rectangle<int> area)
{
    auto body = getPanelBody(area);
    auto presetArea = body.removeFromLeft(static_cast<int>(body.getWidth() * 0.56f));
    constexpr int controlRowHeight = 30;
    constexpr int actionButtonWidth = 92;

    auto topRow = presetArea.removeFromTop(controlRowHeight);
    presetGroupLabel.setBounds(topRow.removeFromLeft(70));
    presetGroupCombo.setBounds(topRow.removeFromLeft(170).reduced(2, 0));
    topRow.removeFromLeft(8);
    themeLabel.setBounds(topRow.removeFromLeft(66));
    themeCombo.setBounds(topRow.reduced(2, 0));

    presetArea.removeFromTop(6);

    auto middleRow = presetArea.removeFromTop(controlRowHeight);
    presetNameLabel.setBounds(middleRow.removeFromLeft(70));
    presetCombo.setBounds(middleRow.removeFromLeft(390).reduced(2, 0));
    middleRow.removeFromLeft(8);
    presetSearchEditor.setBounds(middleRow.reduced(2, 0));

    presetArea.removeFromTop(6);

    auto bottomRow = presetArea.removeFromTop(controlRowHeight);
    presetNameEditor.setBounds(bottomRow.removeFromLeft(296).reduced(2, 0));
    bottomRow.removeFromLeft(8);
    presetMenuButton.setBounds(bottomRow.removeFromLeft(actionButtonWidth).reduced(2, 0));
    bottomRow.removeFromLeft(4);
    savePresetButton.setBounds(bottomRow.removeFromLeft(actionButtonWidth).reduced(2, 0));
    bottomRow.removeFromLeft(4);
    reloadPresetButton.setBounds(bottomRow.removeFromLeft(actionButtonWidth).reduced(2, 0));
    bottomRow.removeFromLeft(4);
    initPresetButton.setBounds(bottomRow.removeFromLeft(actionButtonWidth).reduced(2, 0));

    body.removeFromLeft(12);
    auto wheelArea = body.removeFromLeft(150);
    auto pitchArea = wheelArea.removeFromLeft(wheelArea.getWidth() / 2).reduced(4);
    auto modArea = wheelArea.reduced(4);

    pitchWheelLabel.setBounds(pitchArea.removeFromTop(18));
    pitchWheelSlider.setBounds(pitchArea);
    modWheelLabel.setBounds(modArea.removeFromTop(18));
    modWheelSlider.setBounds(modArea);

    body.removeFromLeft(16);
    layoutChoiceRow(body.removeFromTop(54), { "keyMode", "portamentoMode" });
}

void AISynthAudioProcessorEditor::layoutSequencerPanel(juce::Rectangle<int> area)
{
    auto body = getPanelBody(area);

    patternLabel.setVisible(true);
    patternCombo.setVisible(true);
    patternNameEditor.setVisible(true);
    savePatternButton.setVisible(true);
    reloadPatternButton.setVisible(true);

    auto patternRow = body.removeFromTop(30);
    patternLabel.setBounds(patternRow.removeFromLeft(80));
    patternCombo.setBounds(patternRow.removeFromLeft(320).reduced(2, 0));
    patternRow.removeFromLeft(8);
    patternNameEditor.setBounds(patternRow.removeFromLeft(280).reduced(2, 0));
    patternRow.removeFromLeft(6);
    savePatternButton.setBounds(patternRow.removeFromLeft(112).reduced(2, 0));
    patternRow.removeFromLeft(4);
    reloadPatternButton.setBounds(patternRow.removeFromLeft(90).reduced(2, 0));

    body.removeFromTop(8);
    layoutToggleRow(body.removeFromTop(28), { "stepSeqEnabled", "stepSeqSync" });
    body.removeFromTop(8);
    layoutChoiceRow(body.removeFromTop(54), { "stepSeqLength" });
    body.removeFromTop(6);
    layoutKnobGrid(body.removeFromTop(104), { "stepSeqBpm", "stepSeqSteps" }, 2);
    body.removeFromTop(8);

    const auto rowLabelWidth = 60;
    auto rowArea = body;
    rowArea.removeFromLeft(rowLabelWidth);
    const auto rowHeight = rowArea.getHeight() / 3;
    const auto columnWidth = rowArea.getWidth() / 8;

    for (int step = 0; step < 8; ++step)
    {
        auto noteCell = juce::Rectangle<int>(rowArea.getX() + step * columnWidth, rowArea.getY(), columnWidth, rowHeight).reduced(4, 6);
        auto gateCell = juce::Rectangle<int>(rowArea.getX() + step * columnWidth, rowArea.getY() + rowHeight, columnWidth, rowHeight).reduced(4, 6);
        auto velocityCell = juce::Rectangle<int>(rowArea.getX() + step * columnWidth, rowArea.getY() + rowHeight * 2, columnWidth, rowHeight).reduced(4, 6);
        sequencerStepBounds[static_cast<size_t>(step)] = noteCell.getUnion(gateCell).getUnion(velocityCell);

        if (auto* note = findSlider("seqStep" + juce::String(step + 1) + "Note"))
        {
            note->slider->setVisible(true);
            note->label->setVisible(false);
            note->slider->setBounds(noteCell);
        }

        if (auto* gate = findSlider("seqStep" + juce::String(step + 1) + "Gate"))
        {
            gate->slider->setVisible(true);
            gate->label->setVisible(false);
            gate->slider->setBounds(gateCell);
        }

        if (auto* velocity = findSlider("seqStep" + juce::String(step + 1) + "Velocity"))
        {
            velocity->slider->setVisible(true);
            velocity->label->setVisible(false);
            velocity->slider->setBounds(velocityCell);
        }
    }
}

void AISynthAudioProcessorEditor::hideAllDynamicControls()
{
    for (auto& meta : sliderMetas)
    {
        meta.slider->setVisible(false);
        meta.label->setVisible(false);
    }

    for (auto& meta : choiceMetas)
    {
        meta.combo->setVisible(false);
        meta.label->setVisible(false);
    }

    for (auto& meta : toggleMetas)
        meta.button->setVisible(false);

    ampEnvelopeDisplay.setVisible(false);
    modEnvelopeDisplay.setVisible(false);
    patternLabel.setVisible(false);
    patternCombo.setVisible(false);
    patternNameEditor.setVisible(false);
    savePatternButton.setVisible(false);
    reloadPatternButton.setVisible(false);
    oscillatorSectionBounds = {};
    lfoSectionBounds = {};
    sequencerStepBounds = {};
}

AISynthAudioProcessorEditor::ThemePalette AISynthAudioProcessorEditor::createThemePalette(int themeIndex, float phase)
{
    switch (juce::jlimit(0, 4, themeIndex))
    {
        case 0:
            return { "Synthwave / Retro Future",
                     juce::Colour(0xff0b0f1a), juce::Colour(0xff040611),
                     juce::Colour(0xff121a2b), juce::Colour(0xff0f1624),
                     juce::Colour(0xffff2e88), juce::Colour(0xff00f0ff),
                     juce::Colour(0xffe6f1ff), juce::Colour(0xffaab9d2),
                     juce::Colour(0xff8f4dff), juce::Colour(0x1800f0ff),
                     juce::Colour(0xff5b5f88), juce::Colour(0xff121421), juce::Colour(0xff9299cc),
                     juce::Colour(0xff17243a), juce::Colour(0xffe6f1ff),
                     true, true, false, false, false, false, false, false, false,
                     0.55f };
        case 1:
            return { "Horror / Dark Occult",
                     juce::Colour(0xff0a0a0a), juce::Colour(0xff020202),
                     juce::Colour(0xff141414), juce::Colour(0xff0d0d0d),
                     juce::Colour(0xff8b0000), juce::Colour(0xff3a0000),
                     juce::Colour(0xffd6d6d6), juce::Colour(0xff9c9292),
                     juce::Colour(0xff4b1818), juce::Colour(0x12ffffff),
                     juce::Colour(0xff55504b), juce::Colour(0xff171514), juce::Colour(0xff80766b),
                     juce::Colour(0xff211313), juce::Colour(0xffd0cbc6),
                     false, false, true, false, false, true, false, true, false,
                     0.18f + 0.03f * std::sin(phase * 3.7f) };
        case 2:
            return { "Metal / Industrial",
                     juce::Colour(0xff1a1a1a), juce::Colour(0xff0d0d0d),
                     juce::Colour(0xff2a2a2a), juce::Colour(0xff1d1d1d),
                     juce::Colour(0xffff6a00), juce::Colour(0xffaaaaaa),
                     juce::Colour(0xfff0f0f0), juce::Colour(0xffbcbcbc),
                     juce::Colour(0xff686868), juce::Colour(0x18ffffff),
                     juce::Colour(0xffa5a7ab), juce::Colour(0xff1b1d1f), juce::Colour(0xff6d747c),
                     juce::Colour(0xff222222), juce::Colour(0xfff0f0f0),
                     false, false, false, true, false, false, false, true, false,
                     0.22f };
        case 3:
            return { "AI / Neural Core",
                     juce::Colour(0xff0a0f14), juce::Colour(0xff050b0f),
                     juce::Colour(0xff101820), juce::Colour(0xff0d1319),
                     juce::Colour(0xff00ffcc), juce::Colour(0xff3a86ff),
                     juce::Colour(0xffe0f7ff), juce::Colour(0xff8bb9c9),
                     juce::Colour(0xff4ecfdc), juce::Colour(0x1800ffcc),
                     juce::Colour(0xff6fa7b2), juce::Colour(0xff0b1118), juce::Colour(0xff79c7d2),
                     juce::Colour(0xff0f1c24), juce::Colour(0xffe0f7ff),
                     true, false, false, false, true, false, true, false, false,
                     0.4f + 0.08f * std::sin(phase * 2.1f) };
        default:
            return { "Minimal Pro",
                     juce::Colour(0xff1e1e1e), juce::Colour(0xff111111),
                     juce::Colour(0xff2a2a2a), juce::Colour(0xff202020),
                     juce::Colour(0xfff5a623), juce::Colour(0xffd6d6d6),
                     juce::Colour(0xffffffff), juce::Colour(0xffbbbbbb),
                     juce::Colour(0xff4f4f4f), juce::Colour(0x14000000),
                     juce::Colour(0xff808080), juce::Colour(0xff171717), juce::Colour(0xff5d5d5d),
                     juce::Colour(0xff1c1c1c), juce::Colour(0xffffffff),
                     false, false, false, false, false, false, false, false, true,
                     0.12f };
    }
}

void AISynthAudioProcessorEditor::applyTheme()
{
    currentTheme = createThemePalette(audioProcessor.getCurrentThemeIndex(), animationPhase);
    themeCombo.setSelectedItemIndex(audioProcessor.getCurrentThemeIndex(), juce::dontSendNotification);

    synthLookAndFeel.setTheme(currentTheme, animationPhase);

    for (auto* label : labels)
    {
        label->setColour(juce::Label::textColourId, currentTheme.text);
    }

    presetNameLabel.setColour(juce::Label::textColourId, currentTheme.text);
    presetGroupLabel.setColour(juce::Label::textColourId, currentTheme.text);
    themeLabel.setColour(juce::Label::textColourId, currentTheme.text);
    patternLabel.setColour(juce::Label::textColourId, currentTheme.text);
    pitchWheelLabel.setColour(juce::Label::textColourId, currentTheme.text);
    modWheelLabel.setColour(juce::Label::textColourId, currentTheme.text);

    presetNameEditor.setColour(juce::TextEditor::backgroundColourId, currentTheme.panelBottom.darker(0.06f));
    presetNameEditor.setColour(juce::TextEditor::outlineColourId, currentTheme.border);
    presetNameEditor.setColour(juce::TextEditor::textColourId, currentTheme.text);
    presetNameEditor.setTextToShowWhenEmpty("Save current sound as...", currentTheme.mutedText.withAlpha(0.8f));
    presetSearchEditor.setColour(juce::TextEditor::backgroundColourId, currentTheme.panelBottom.darker(0.06f));
    presetSearchEditor.setColour(juce::TextEditor::outlineColourId, currentTheme.border);
    presetSearchEditor.setColour(juce::TextEditor::textColourId, currentTheme.text);
    presetSearchEditor.setTextToShowWhenEmpty("Search presets", currentTheme.mutedText.withAlpha(0.8f));
    patternNameEditor.setColour(juce::TextEditor::backgroundColourId, currentTheme.panelBottom.darker(0.06f));
    patternNameEditor.setColour(juce::TextEditor::outlineColourId, currentTheme.border);
    patternNameEditor.setColour(juce::TextEditor::textColourId, currentTheme.text);
    patternNameEditor.setTextToShowWhenEmpty("Save sequencer pattern as...", currentTheme.mutedText.withAlpha(0.8f));

    for (auto* box : comboBoxes)
        box->setColour(juce::ComboBox::textColourId, currentTheme.text);

    presetCombo.setColour(juce::ComboBox::textColourId, currentTheme.text);
    presetGroupCombo.setColour(juce::ComboBox::textColourId, currentTheme.text);
    themeCombo.setColour(juce::ComboBox::textColourId, currentTheme.text);
    patternCombo.setColour(juce::ComboBox::textColourId, currentTheme.text);

    keyboardComponent.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, currentTheme.text.withAlpha(currentTheme.flatPanels ? 0.94f : 0.88f));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::blackNoteColourId, currentTheme.panelBottom.darker(0.6f));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, currentTheme.border.withAlpha(0.7f));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, currentTheme.secondary.withAlpha(0.35f));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, currentTheme.accent.withAlpha(0.45f));

    styleHeaderButton(savePresetButton, currentTheme, false);
    styleHeaderButton(reloadPresetButton, currentTheme, false);
    styleHeaderButton(initPresetButton, currentTheme, false);
    styleHeaderButton(presetMenuButton, currentTheme, false);
    styleHeaderButton(savePatternButton, currentTheme, false);
    styleHeaderButton(reloadPatternButton, currentTheme, false);
    styleHeaderButton(voiceTabButton, currentTheme, activeTab == ActiveTab::voice);
    styleHeaderButton(modulationTabButton, currentTheme, activeTab == ActiveTab::modulation);
    styleHeaderButton(effectsTabButton, currentTheme, activeTab == ActiveTab::effects);
    styleHeaderButton(sequencerTabButton, currentTheme, activeTab == ActiveTab::sequencer);
    styleHeaderButton(osc1TabButton, currentTheme, oscillatorDetailTab == OscillatorDetailTab::osc1);
    styleHeaderButton(osc2TabButton, currentTheme, oscillatorDetailTab == OscillatorDetailTab::osc2);
    styleHeaderButton(osc3TabButton, currentTheme, oscillatorDetailTab == OscillatorDetailTab::osc3);
    styleHeaderButton(subTabButton, currentTheme, oscillatorDetailTab == OscillatorDetailTab::sub);
    styleHeaderButton(delayFxTabButton, currentTheme, effectsDetailTab == EffectsDetailTab::delay);
    styleHeaderButton(chorusFxTabButton, currentTheme, effectsDetailTab == EffectsDetailTab::chorus);
    styleHeaderButton(driveFxTabButton, currentTheme, effectsDetailTab == EffectsDetailTab::drive);
    styleHeaderButton(dynamicsFxTabButton, currentTheme, effectsDetailTab == EffectsDetailTab::dynamics);
    styleHeaderButton(reverbFxTabButton, currentTheme, effectsDetailTab == EffectsDetailTab::reverb);
    styleHeaderButton(crusherFxTabButton, currentTheme, effectsDetailTab == EffectsDetailTab::crusher);
    styleHeaderButton(voiceFilterTabButton, currentTheme, voiceDetailTab == VoiceDetailTab::filter);
    styleHeaderButton(voiceArpTabButton, currentTheme, voiceDetailTab == VoiceDetailTab::arp);

    refreshMidiLearnDecorations();
    refreshBackgroundTexture();
    repaint();
}

void AISynthAudioProcessorEditor::timerCallback()
{
    animationPhase += 0.08f;
    if (animationPhase > juce::MathConstants<float>::twoPi)
        animationPhase -= juce::MathConstants<float>::twoPi;

    const auto themeIndex = audioProcessor.getCurrentThemeIndex();
    if (themeCombo.getSelectedItemIndex() != themeIndex)
    {
        applyTheme();
        return;
    }

    currentTheme = createThemePalette(themeIndex, animationPhase);
    synthLookAndFeel.setTheme(currentTheme, animationPhase);
    repaint();
}

void AISynthAudioProcessorEditor::refreshBackgroundTexture()
{
    backgroundTexture = juce::Image(juce::Image::ARGB, getWidth(), getHeight(), true);
    juce::Graphics g(backgroundTexture);

    juce::ColourGradient background(currentTheme.backgroundTop, 0.0f, 0.0f,
                                    currentTheme.backgroundBottom, 0.0f, static_cast<float>(getHeight()), false);
    g.setGradientFill(background);
    g.fillAll();

    if (currentTheme.showGrid)
    {
        g.setColour(currentTheme.secondary.withAlpha(0.05f));
        for (int x = 0; x < getWidth(); x += 40)
            g.drawVerticalLine(x, 0.0f, static_cast<float>(getHeight()));
        for (int y = 0; y < getHeight(); y += 40)
            g.drawHorizontalLine(y, 0.0f, static_cast<float>(getWidth()));
    }

    if (currentTheme.showCircuitLines)
    {
        g.setColour(currentTheme.secondary.withAlpha(0.08f));
        for (int i = 0; i < 9; ++i)
        {
            const auto y = 80 + i * 70;
            const auto phaseOffset = static_cast<int>((std::sin(animationPhase + i * 0.7f) + 1.0f) * 40.0f);
            juce::Path path;
            path.startNewSubPath(static_cast<float>(40 + phaseOffset), static_cast<float>(y));
            path.lineTo(static_cast<float>(240 + phaseOffset), static_cast<float>(y));
            path.lineTo(static_cast<float>(280 + phaseOffset), static_cast<float>(y + 22));
            path.lineTo(static_cast<float>(getWidth() - 120 - phaseOffset), static_cast<float>(y + 22));
            g.strokePath(path, juce::PathStrokeType(1.4f));
            g.fillEllipse(static_cast<float>(275 + phaseOffset), static_cast<float>(y + 17), 10.0f, 10.0f);
        }
    }

    juce::Random random(1977 + audioProcessor.getCurrentThemeIndex());
    for (int y = 0; y < getHeight(); ++y)
    {
        const auto tone = currentTheme.showScanlines ? 0.035f + 0.015f * std::sin(static_cast<float>(y) * 0.18f + animationPhase) : 0.01f;
        g.setColour(currentTheme.text.withAlpha(tone));
        g.drawHorizontalLine(y, 0.0f, static_cast<float>(getWidth()));
    }

    const auto noiseCount = currentTheme.showGrunge ? 5200 : 2200;
    for (int i = 0; i < noiseCount; ++i)
    {
        const auto colour = currentTheme.showGrunge
            ? currentTheme.accent.interpolatedWith(juce::Colours::white, random.nextFloat() * 0.08f).withAlpha(random.nextFloat() * 0.025f)
            : juce::Colours::white.withAlpha(random.nextFloat() * 0.03f);
        g.setColour(colour);
        g.fillRect(random.nextInt(getWidth()), random.nextInt(getHeight()), 1, 1);
    }

    if (currentTheme.showGrunge)
    {
        g.setColour(currentTheme.secondary.withAlpha(0.05f));
        for (int i = 0; i < 30; ++i)
        {
            const auto x = random.nextInt(getWidth());
            const auto y = random.nextInt(getHeight());
            g.drawLine(static_cast<float>(x), static_cast<float>(y),
                       static_cast<float>(x + random.nextInt(80) - 40), static_cast<float>(y + random.nextInt(20) - 10), 1.0f);
        }
    }

    if (currentTheme.showVignette)
    {
        juce::ColourGradient vignette(juce::Colours::transparentBlack, static_cast<float>(getWidth()) * 0.5f, static_cast<float>(getHeight()) * 0.5f,
                                      juce::Colours::black.withAlpha(0.45f), 0.0f, 0.0f, true);
        g.setGradientFill(vignette);
        g.fillAll();
    }
}

void AISynthAudioProcessorEditor::refreshPresetCombo(bool preserveSelection, bool rebuildGroups)
{
    const auto selectedIndex = presetCombo.getSelectedItemIndex();
    const auto desiredName = preserveSelection && juce::isPositiveAndBelow(selectedIndex, filteredPresetNames.size())
        ? filteredPresetNames[selectedIndex]
        : audioProcessor.getCurrentPresetName();
    const auto desiredGroup = preserveSelection && presetGroupCombo.getSelectedItemIndex() >= 0
        ? presetGroupCombo.getText()
        : getTopLevelGroupName(desiredName);
    const auto query = presetSearchEditor.getText().trim().toLowerCase();

    updatingPresetCombo = true;
    filteredPresetNames.clear();

    if (rebuildGroups)
    {
        juce::StringArray groups;
        groups.add("All Presets");
        for (const auto& name : audioProcessor.getPresetNames())
            groups.addIfNotAlreadyThere(getTopLevelGroupName(name));

        presetGroupCombo.clear();
        presetGroupCombo.addItemList(groups, 1);

        auto selectedGroupId = groups.indexOf(desiredGroup) + 1;
        if (selectedGroupId <= 0)
            selectedGroupId = groups.indexOf(getTopLevelGroupName(audioProcessor.getCurrentPresetName())) + 1;
        if (selectedGroupId <= 0)
            selectedGroupId = 1;
        presetGroupCombo.setSelectedId(selectedGroupId, juce::dontSendNotification);
    }

    const auto activeGroup = presetGroupCombo.getText();
    presetCombo.clear();

    int itemId = 1;
    int selectedId = 0;

    for (const auto& name : audioProcessor.getPresetNames())
    {
        const auto group = getTopLevelGroupName(name);
        const auto leafName = getLeafName(name);

        if (activeGroup != "All Presets" && group != activeGroup)
            continue;

        if (query.isNotEmpty()
            && ! name.toLowerCase().contains(query)
            && ! leafName.toLowerCase().contains(query))
        {
            continue;
        }

        filteredPresetNames.add(name);
        const auto displayName = activeGroup == "All Presets" ? name : leafName;
        presetCombo.addItem(displayName, itemId);
        if (name == desiredName)
            selectedId = itemId;

        ++itemId;
    }

    if (selectedId == 0 && itemId > 1)
        selectedId = 1;

    if (selectedId > 0)
        presetCombo.setSelectedId(selectedId, juce::dontSendNotification);

    updatingPresetCombo = false;
    presetNameEditor.setText(getLeafName(selectedId > 0 ? filteredPresetNames[selectedId - 1] : audioProcessor.getCurrentPresetName()),
                             juce::dontSendNotification);
    repaint();
}

void AISynthAudioProcessorEditor::refreshPatternCombo(bool preserveSelection)
{
    const auto selectedIndex = patternCombo.getSelectedItemIndex();
    const auto desiredName = preserveSelection && juce::isPositiveAndBelow(selectedIndex, filteredPatternNames.size())
        ? filteredPatternNames[selectedIndex]
        : audioProcessor.getCurrentPatternName();

    updatingPatternCombo = true;
    filteredPatternNames.clear();
    patternCombo.clear();

    int itemId = 1;
    int selectedId = 0;
    for (const auto& name : audioProcessor.getPatternNames())
    {
        filteredPatternNames.add(name);
        patternCombo.addItem(name, itemId);
        if (name == desiredName)
            selectedId = itemId;

        ++itemId;
    }

    if (selectedId == 0 && itemId > 1 && desiredName.isEmpty())
        selectedId = 1;

    if (selectedId > 0)
        patternCombo.setSelectedId(selectedId, juce::dontSendNotification);

    updatingPatternCombo = false;
    patternNameEditor.setText(getLeafName(selectedId > 0 ? filteredPatternNames[selectedId - 1] : desiredName),
                              juce::dontSendNotification);
    repaint();
}

void AISynthAudioProcessorEditor::refreshMidiLearnDecorations()
{
    for (auto& meta : sliderMetas)
    {
        const auto mappingText = audioProcessor.getMidiMappingDescription(meta.id);
        const auto learning = audioProcessor.isMidiLearnActiveFor(meta.id);

        juce::String tooltip = meta.baseTooltip;
        tooltip << "\nRight-click to MIDI learn this control.";
        if (learning)
            tooltip << "\nLearning: move a MIDI controller now.";
        else if (mappingText.isNotEmpty())
            tooltip << "\nMapped to " << mappingText << " for this preset.";

        meta.slider->setTooltip(tooltip);

        auto labelColour = currentTheme.text;
        if (learning)
            labelColour = currentTheme.secondary;
        else if (mappingText.isNotEmpty())
            labelColour = currentTheme.accent;

        meta.label->setColour(juce::Label::textColourId, labelColour);
    }
}

void AISynthAudioProcessorEditor::sendPitchWheelMessage()
{
    const auto wheelValue = juce::jlimit(0, 16383, static_cast<int>(std::round(juce::jmap(static_cast<float>(pitchWheelSlider.getValue()), -1.0f, 1.0f, 0.0f, 16383.0f))));
    audioProcessor.getKeyboardState().processNextMidiEvent(juce::MidiMessage::pitchWheel(1, wheelValue));
}

void AISynthAudioProcessorEditor::sendModWheelMessage()
{
    const auto controllerValue = juce::jlimit(0, 127, static_cast<int>(std::round(modWheelSlider.getValue() * 127.0)));
    audioProcessor.getKeyboardState().processNextMidiEvent(juce::MidiMessage::controllerEvent(1, 1, controllerValue));
}

void AISynthAudioProcessorEditor::savePresetFromEditor()
{
    auto presetName = presetNameEditor.getText().trim();
    if (presetName.isEmpty())
        presetName = getLeafName(audioProcessor.getCurrentPresetName());

    if (! presetName.contains("/"))
        presetName = "User / " + presetName;

    if (audioProcessor.saveUserPreset(presetName))
    {
        refreshPresetCombo(false);
        presetNameEditor.setText(getLeafName(audioProcessor.getCurrentPresetName()), juce::dontSendNotification);
    }
}

void AISynthAudioProcessorEditor::savePatternFromEditor()
{
    auto patternName = patternNameEditor.getText().trim();
    if (patternName.isEmpty())
        patternName = getLeafName(audioProcessor.getCurrentPatternName());

    if (! patternName.contains("/"))
        patternName = "User / " + patternName;

    if (audioProcessor.saveSequencerPattern(patternName))
    {
        refreshPatternCombo(false);
        patternNameEditor.setText(getLeafName(audioProcessor.getCurrentPatternName()), juce::dontSendNotification);
    }
}

void AISynthAudioProcessorEditor::showSliderMidiLearnMenu(const juce::String& parameterId, juce::Slider& slider, const juce::String& title)
{
    juce::PopupMenu menu;
    menu.addSectionHeader(title.toUpperCase());
    menu.addItem(1, "Learn MIDI CC");
    menu.addItem(2, "Cancel MIDI Learn", audioProcessor.getMidiLearnTargetParameter().isNotEmpty());

    const auto mappingDescription = audioProcessor.getMidiMappingDescription(parameterId);
    if (mappingDescription.isNotEmpty())
    {
        menu.addSeparator();
        menu.addItem(3, "Current Mapping: " + mappingDescription, false, false);
    }

    juce::Component::SafePointer<AISynthAudioProcessorEditor> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&slider),
                       [safeThis, parameterId] (int result)
                       {
                           if (safeThis == nullptr)
                               return;

                           if (result == 1)
                           {
                               safeThis->audioProcessor.beginMidiLearn(parameterId);
                               safeThis->refreshMidiLearnDecorations();
                               safeThis->repaint();
                           }
                           else if (result == 2)
                           {
                               safeThis->audioProcessor.cancelMidiLearn();
                               safeThis->refreshMidiLearnDecorations();
                               safeThis->repaint();
                           }
                       });
}

void AISynthAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader("Preset Actions");
    menu.addItem(1, "Clear MIDI Assignments for Current Preset");
    menu.addItem(2, "Cancel MIDI Learn", audioProcessor.getMidiLearnTargetParameter().isNotEmpty());

    juce::Component::SafePointer<AISynthAudioProcessorEditor> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetMenuButton),
                       [safeThis] (int result)
                       {
                           if (safeThis == nullptr)
                               return;

                           if (result == 1)
                           {
                               safeThis->audioProcessor.clearMidiMappingsForCurrentPreset();
                               safeThis->refreshMidiLearnDecorations();
                               safeThis->repaint();
                           }
                           else if (result == 2)
                           {
                               safeThis->audioProcessor.cancelMidiLearn();
                               safeThis->refreshMidiLearnDecorations();
                               safeThis->repaint();
                           }
                       });
}

void AISynthAudioProcessorEditor::setActiveTab(ActiveTab newTab)
{
    activeTab = newTab;
    voiceTabButton.setToggleState(activeTab == ActiveTab::voice, juce::dontSendNotification);
    modulationTabButton.setToggleState(activeTab == ActiveTab::modulation, juce::dontSendNotification);
    effectsTabButton.setToggleState(activeTab == ActiveTab::effects, juce::dontSendNotification);
    sequencerTabButton.setToggleState(activeTab == ActiveTab::sequencer, juce::dontSendNotification);
    applyTheme();
    resized();
    repaint();
}

void AISynthAudioProcessorEditor::stylePerformanceSlider(juce::Slider& slider, juce::Label& label, const juce::String& text, bool centreOnRelease)
{
    slider.setSliderStyle(juce::Slider::LinearVertical);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, rotaryTextBoxHeight);
    slider.setVelocityBasedMode(false);
    slider.setMouseDragSensitivity(180);
    slider.setTooltip(centreOnRelease ? "Spring-loaded pitch wheel." : "Mod wheel controller.");
    addAndMakeVisible(slider);

    configureLabel(label, text);
    addAndMakeVisible(label);
}

void AISynthAudioProcessorEditor::updateKeyboardWidth(const juce::Rectangle<int>& area)
{
    int whiteKeyCount = 0;
    for (int note = 24; note <= 96; ++note)
        if (! juce::MidiMessage::isMidiNoteBlack(note))
            ++whiteKeyCount;

    keyboardComponent.setLowestVisibleKey(24);
    keyboardComponent.setKeyWidth(static_cast<float>(area.getWidth()) / static_cast<float>(juce::jmax(1, whiteKeyCount)));
    keyboardComponent.setBounds(area);
}

void AISynthAudioProcessorEditor::paint(juce::Graphics& g)
{
    if (backgroundTexture.isNull() || backgroundTexture.getWidth() != getWidth() || backgroundTexture.getHeight() != getHeight())
        refreshBackgroundTexture();

    g.drawImageAt(backgroundTexture, 0, 0);

    drawWoodStrip(g, { 10, 18, 18, getHeight() - 36 }, currentTheme);
    drawWoodStrip(g, { getWidth() - 28, 18, 18, getHeight() - 36 }, currentTheme);

    auto headerArea = headerBounds.toFloat();
    juce::ColourGradient headerGradient(currentTheme.panelTop.brighter(currentTheme.showGlass ? 0.1f : 0.02f), headerArea.getTopLeft(),
                                        currentTheme.panelBottom.darker(0.08f), headerArea.getBottomLeft(), false);
    g.setGradientFill(headerGradient);
    g.fillRoundedRectangle(headerArea, 12.0f);
    g.setColour(currentTheme.border);
    g.drawRoundedRectangle(headerArea.reduced(1.0f), 12.0f, 1.2f);

    if (currentTheme.glowAmount > 0.0f)
    {
        g.setColour(currentTheme.secondary.withAlpha(currentTheme.glowAmount * 0.16f));
        g.drawRoundedRectangle(headerArea.expanded(1.0f), 12.0f, 2.0f);
    }

    g.setColour(currentTheme.text);
    g.setFont(juce::Font(juce::FontOptions(30.0f, juce::Font::bold)));
    g.drawText("AI SYNTH", headerBounds.reduced(20, 10).removeFromTop(30), juce::Justification::centredLeft);

    g.setColour(currentTheme.accent);
    g.setFont(juce::Font(juce::FontOptions(13.5f, juce::Font::bold)));
    g.drawText(currentTheme.name.toUpperCase() + " THEME ACTIVE",
               headerBounds.reduced(20, 10).withTrimmedTop(30).removeFromTop(18),
               juce::Justification::centredLeft);

    g.setColour(currentTheme.mutedText);
    g.setFont(juce::Font(juce::FontOptions(12.5f)));
    g.drawText("Switch tabs to keep the layout compact while the keyboard stays full-width across the bottom.",
               headerBounds.reduced(20, 10).withTrimmedTop(48), juce::Justification::centredLeft);

    const auto drawTabGlow = [&] (const juce::TextButton& button, bool active)
    {
        if (! active)
            return;

        auto glowArea = button.getBounds().toFloat().expanded(2.0f, 4.0f);
        g.setColour(currentTheme.accent.withAlpha(currentTheme.glowAmount * 0.18f));
        g.fillRoundedRectangle(glowArea.withTrimmedTop(glowArea.getHeight() * 0.45f), 8.0f);
    };

    drawTabGlow(voiceTabButton, activeTab == ActiveTab::voice);
    drawTabGlow(modulationTabButton, activeTab == ActiveTab::modulation);
    drawTabGlow(effectsTabButton, activeTab == ActiveTab::effects);
    drawTabGlow(sequencerTabButton, activeTab == ActiveTab::sequencer);
    drawTabGlow(osc1TabButton, activeTab == ActiveTab::voice && oscillatorDetailTab == OscillatorDetailTab::osc1);
    drawTabGlow(osc2TabButton, activeTab == ActiveTab::voice && oscillatorDetailTab == OscillatorDetailTab::osc2);
    drawTabGlow(osc3TabButton, activeTab == ActiveTab::voice && oscillatorDetailTab == OscillatorDetailTab::osc3);
    drawTabGlow(subTabButton, activeTab == ActiveTab::voice && oscillatorDetailTab == OscillatorDetailTab::sub);
    drawTabGlow(delayFxTabButton, activeTab == ActiveTab::effects && effectsDetailTab == EffectsDetailTab::delay);
    drawTabGlow(chorusFxTabButton, activeTab == ActiveTab::effects && effectsDetailTab == EffectsDetailTab::chorus);
    drawTabGlow(driveFxTabButton, activeTab == ActiveTab::effects && effectsDetailTab == EffectsDetailTab::drive);
    drawTabGlow(dynamicsFxTabButton, activeTab == ActiveTab::effects && effectsDetailTab == EffectsDetailTab::dynamics);
    drawTabGlow(reverbFxTabButton, activeTab == ActiveTab::effects && effectsDetailTab == EffectsDetailTab::reverb);
    drawTabGlow(crusherFxTabButton, activeTab == ActiveTab::effects && effectsDetailTab == EffectsDetailTab::crusher);
    drawTabGlow(voiceFilterTabButton, activeTab == ActiveTab::voice && voiceDetailTab == VoiceDetailTab::filter);
    drawTabGlow(voiceArpTabButton, activeTab == ActiveTab::voice && voiceDetailTab == VoiceDetailTab::arp);

    drawPanel(g, presetPanelBounds, "Presets / Performance", "Grouped browser, search, save/load, wheels, theme, and quick controls", currentTheme, animationPhase);

    if (activeTab == ActiveTab::voice)
    {
        drawPanel(g, oscillatorPanelBounds, "Oscillator Bank", "Three main oscillators plus sub, sync and PWM routing", currentTheme, animationPhase);
        if (! oscillatorTabsBounds.isEmpty())
        {
            auto tabArea = oscillatorTabsBounds.toFloat();
            g.setColour(currentTheme.overlay.withAlpha(0.18f));
            g.fillRoundedRectangle(tabArea, 10.0f);
            g.setColour(currentTheme.border.withAlpha(0.75f));
            g.drawRoundedRectangle(tabArea, 10.0f, 1.0f);
        }
        if (! voiceDetailTabsBounds.isEmpty())
        {
            auto tabArea = voiceDetailTabsBounds.toFloat();
            g.setColour(currentTheme.overlay.withAlpha(0.18f));
            g.fillRoundedRectangle(tabArea, 10.0f);
            g.setColour(currentTheme.border.withAlpha(0.75f));
            g.drawRoundedRectangle(tabArea, 10.0f, 1.0f);
        }

        if (! filterPanelBounds.isEmpty())
            drawPanel(g, filterPanelBounds, "Filter / Tone", "Tone shaping, envelope drive and TB-style accent response", currentTheme, animationPhase);

        if (! arpPanelBounds.isEmpty())
            drawPanel(g, arpPanelBounds, "Arp / Rhythm Gate", "Pattern sequencing, latch and chopper timing before the sequencer tab", currentTheme, animationPhase);
    }
    else if (activeTab == ActiveTab::modulation)
    {
        drawPanel(g, env1PanelBounds, "Envelope 1", "Dedicated amp ADSR sliders", currentTheme, animationPhase);
        drawPanel(g, env2PanelBounds, "Envelope 2", "Second envelope plus dedicated velocity routing", currentTheme, animationPhase);
        drawPanel(g, lfoPanelBounds, "LFO Bank", "Three independent modulators ready for PWM and sync movement", currentTheme, animationPhase);
    }
    else if (activeTab == ActiveTab::effects)
    {
        drawPanel(g, fxPanelBounds, "Effects Rack", "Host-sync delay plus chorus, drive, dynamics, reverb and crush", currentTheme, animationPhase);
        if (! fxTabsBounds.isEmpty())
        {
            auto tabArea = fxTabsBounds.toFloat();
            g.setColour(currentTheme.overlay.withAlpha(0.18f));
            g.fillRoundedRectangle(tabArea, 10.0f);
            g.setColour(currentTheme.border.withAlpha(0.75f));
            g.drawRoundedRectangle(tabArea, 10.0f, 1.0f);
        }
        drawPanel(g, matrixPanelBounds, "Modulation Matrix", "Four flexible routing lanes for pitch, filter, PWM and drive", currentTheme, animationPhase);
    }
    else if (activeTab == ActiveTab::sequencer)
    {
        drawPanel(g, sequencerPanelBounds, "Step Sequencer", "Pattern browser plus eight-step pitch, gate and velocity lanes with free or host-synced timing", currentTheme, animationPhase);
    }

    drawPanel(g, keyboardPanelBounds, "Keyboard", "Click or use your typing keyboard to trigger notes", currentTheme, animationPhase);

    if (activeTab == ActiveTab::voice)
    {
        const std::array<juce::String, 4> oscTitles { "OSC 1", "OSC 2", "OSC 3", "SUB" };
        for (size_t i = 0; i < oscillatorSectionBounds.size(); ++i)
            if (! oscillatorSectionBounds[i].isEmpty())
                drawSubSection(g, oscillatorSectionBounds[i], oscTitles[i], currentTheme);
    }
    else if (activeTab == ActiveTab::modulation)
    {
        const std::array<juce::String, 3> lfoTitles { "LFO 1", "LFO 2", "LFO 3" };
        for (size_t i = 0; i < lfoSectionBounds.size(); ++i)
            if (! lfoSectionBounds[i].isEmpty())
                drawSubSection(g, lfoSectionBounds[i], lfoTitles[i], currentTheme);
    }
    else if (activeTab == ActiveTab::sequencer && ! sequencerPanelBounds.isEmpty())
    {
        auto sequencerBody = getPanelBody(sequencerPanelBounds);
        sequencerBody.removeFromTop(30);
        sequencerBody.removeFromTop(8);
        sequencerBody.removeFromTop(28);
        sequencerBody.removeFromTop(8);
        sequencerBody.removeFromTop(54);
        sequencerBody.removeFromTop(6);
        sequencerBody.removeFromTop(104);
        sequencerBody.removeFromTop(8);

        auto rowLabelArea = sequencerBody.removeFromLeft(60);
        const auto rowHeight = sequencerBody.getHeight() / 3;
        const std::array<juce::String, 3> rowTitles { "NOTE", "GATE", "VELOCITY" };
        const auto enabledSteps = juce::jlimit(1, 8, static_cast<int>(std::round(audioProcessor.getParam("stepSeqSteps"))));
        const auto activeStep = audioProcessor.getCurrentSequencerStep();

        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        for (int row = 0; row < 3; ++row)
        {
            g.setColour(currentTheme.mutedText);
            g.drawText(rowTitles[static_cast<size_t>(row)],
                       rowLabelArea.removeFromTop(rowHeight).reduced(0, 6),
                       juce::Justification::centredRight);
        }

        g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
        for (int step = 0; step < static_cast<int>(sequencerStepBounds.size()); ++step)
        {
            const auto bounds = sequencerStepBounds[static_cast<size_t>(step)];
            if (bounds.isEmpty())
                continue;

            const auto isEnabled = step < enabledSteps;
            const auto isActive = isEnabled && step == activeStep;

            if (! isEnabled)
            {
                g.setColour(currentTheme.overlay.withAlpha(0.38f));
                g.fillRoundedRectangle(bounds.toFloat().expanded(2.0f, 0.0f), 10.0f);
            }
            else if (isActive)
            {
                g.setColour(currentTheme.accent.withAlpha(0.12f));
                g.fillRoundedRectangle(bounds.toFloat().expanded(2.0f, 0.0f), 10.0f);
                g.setColour(currentTheme.secondary.withAlpha(0.45f));
                g.drawRoundedRectangle(bounds.toFloat().expanded(2.0f, 0.0f), 10.0f, 1.4f);
            }

            g.setColour(isEnabled ? currentTheme.text : currentTheme.mutedText.withAlpha(0.55f));
            g.drawText(juce::String(step + 1),
                       juce::Rectangle<int>(bounds.getX(), bounds.getY() - 20, bounds.getWidth(), 18),
                       juce::Justification::centred);
        }

        const auto syncEnabled = audioProcessor.getParam("stepSeqSync") > 0.5f;
        juce::String modeText = syncEnabled ? "MODE: HOST SYNC" : "MODE: FREE RUN";
        if (! syncEnabled)
            modeText << "  " << juce::String(audioProcessor.getParam("stepSeqBpm"), 1) << " BPM";

        g.setColour(currentTheme.mutedText);
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        g.drawText(modeText,
                   sequencerPanelBounds.reduced(18, 12).removeFromRight(280).removeFromTop(24),
                   juce::Justification::centredRight);
    }

    g.setColour(currentTheme.mutedText);
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawText("CURRENT PRESET: " + audioProcessor.getCurrentPresetName().toUpperCase(),
               headerBounds.reduced(20, 10).removeFromRight(420),
               juce::Justification::centredRight);

    const auto learnTarget = audioProcessor.getMidiLearnTargetParameter();
    if (learnTarget.isNotEmpty())
    {
        juce::String learnTitle = learnTarget.toUpperCase();
        for (const auto& meta : sliderMetas)
        {
            if (meta.id == learnTarget)
            {
                learnTitle = meta.label->getText();
                break;
            }
        }

        g.setColour(currentTheme.secondary);
        g.setFont(juce::Font(juce::FontOptions(12.5f, juce::Font::bold)));
        g.drawText("MIDI LEARN: MOVE A CONTROLLER FOR " + learnTitle,
                   headerBounds.reduced(20, 10).removeFromRight(760).withTrimmedTop(50),
                   juce::Justification::centredRight);
    }
}

void AISynthAudioProcessorEditor::resized()
{
    hideAllDynamicControls();

    auto area = getLocalBounds().reduced(18);
    headerBounds = area.removeFromTop(84);

    const auto gap = 8;
    area.removeFromTop(gap);
    presetPanelBounds = area.removeFromTop(154);
    area.removeFromTop(gap);
    keyboardPanelBounds = area.removeFromBottom(154);
    area.removeFromBottom(gap);

    auto contentArea = area;

    auto headerControls = headerBounds.reduced(18, 14);
    auto tabsArea = headerControls.removeFromRight(500);
    const auto tabGap = 6;
    const auto tabWidth = (tabsArea.getWidth() - tabGap * 3) / 4;
    voiceTabButton.setBounds(tabsArea.removeFromLeft(tabWidth));
    tabsArea.removeFromLeft(tabGap);
    modulationTabButton.setBounds(tabsArea.removeFromLeft(tabWidth));
    tabsArea.removeFromLeft(tabGap);
    effectsTabButton.setBounds(tabsArea.removeFromLeft(tabWidth));
    tabsArea.removeFromLeft(tabGap);
    sequencerTabButton.setBounds(tabsArea);

    layoutPresetPanel(presetPanelBounds);

    oscillatorPanelBounds = {};
    oscillatorTabsBounds = {};
    voiceDetailTabsBounds = {};
    filterPanelBounds = {};
    arpPanelBounds = {};
    env1PanelBounds = {};
    env2PanelBounds = {};
    lfoPanelBounds = {};
    fxPanelBounds = {};
    fxTabsBounds = {};
    matrixPanelBounds = {};
    sequencerPanelBounds = {};

    const auto showingVoiceControls = activeTab == ActiveTab::voice;
    const auto showingEffectsControls = activeTab == ActiveTab::effects;
    osc1TabButton.setVisible(showingVoiceControls);
    osc2TabButton.setVisible(showingVoiceControls);
    osc3TabButton.setVisible(showingVoiceControls);
    subTabButton.setVisible(showingVoiceControls);
    delayFxTabButton.setVisible(showingEffectsControls);
    chorusFxTabButton.setVisible(showingEffectsControls);
    driveFxTabButton.setVisible(showingEffectsControls);
    dynamicsFxTabButton.setVisible(showingEffectsControls);
    reverbFxTabButton.setVisible(showingEffectsControls);
    crusherFxTabButton.setVisible(showingEffectsControls);
    voiceFilterTabButton.setVisible(showingVoiceControls);
    voiceArpTabButton.setVisible(showingVoiceControls);

    if (activeTab == ActiveTab::voice)
    {
        auto rightColumn = contentArea;
        oscillatorPanelBounds = rightColumn.removeFromLeft(static_cast<int>(contentArea.getWidth() * 0.56f));
        rightColumn.removeFromLeft(gap);

        voiceDetailTabsBounds = rightColumn.removeFromTop(34);
        auto detailTabs = voiceDetailTabsBounds.reduced(2, 2);
        const auto detailGap = 6;
        const auto detailWidth = (detailTabs.getWidth() - detailGap) / 2;
        voiceFilterTabButton.setBounds(detailTabs.removeFromLeft(detailWidth));
        detailTabs.removeFromLeft(detailGap);
        voiceArpTabButton.setBounds(detailTabs);
        rightColumn.removeFromTop(gap);

        layoutOscillatorSections(oscillatorPanelBounds);

        if (voiceDetailTab == VoiceDetailTab::filter)
        {
            filterPanelBounds = rightColumn;
            auto filterBody = getPanelBody(filterPanelBounds);
            auto globalBody = filterBody.removeFromTop(92);
            layoutKnobGrid(globalBody, { "masterGain", "pitchBendRange", "voiceCount", "portamentoTime",
                                         "unisonVoices", "unisonDetune", "stereoWidth" }, 7, 14, 1, 7.8f, 1);
            filterBody.removeFromTop(8);
            layoutChoiceRow(filterBody.removeFromTop(50), { "filterType" });
            filterBody.removeFromTop(8);
            layoutKnobGrid(filterBody, { "cutoff", "resonance", "drive", "filterAccent", "env1ToFilter", "env2ToFilter" }, 3, 18, 4, 10.5f, 2);
        }
        else
        {
            arpPanelBounds = rightColumn;
            auto arpBody = getPanelBody(arpPanelBounds);
            layoutChoiceRow(arpBody.removeFromTop(54), { "arpMode", "rhythmGatePattern" });
            layoutToggleRow(arpBody.removeFromTop(28), { "arpEnabled", "arpLatch", "rhythmGateEnabled" });
            arpBody.removeFromTop(6);
            layoutKnobGrid(arpBody, { "arpRate", "arpGate", "arpOctaves", "rhythmGateRate", "rhythmGateDepth" }, 3, 16, 1, 11.0f);
        }
    }
    else if (activeTab == ActiveTab::modulation)
    {
        const auto colWidth = (contentArea.getWidth() - gap * 2) / 3;
        env1PanelBounds = contentArea.removeFromLeft(colWidth);
        contentArea.removeFromLeft(gap);
        env2PanelBounds = contentArea.removeFromLeft(colWidth);
        contentArea.removeFromLeft(gap);
        lfoPanelBounds = contentArea;

        ampEnvelopeDisplay.setVisible(true);
        modEnvelopeDisplay.setVisible(true);

        auto env1Body = getPanelBody(env1PanelBounds);
        ampEnvelopeDisplay.setBounds(env1Body.removeFromTop(126));
        env1Body.removeFromTop(4);
        layoutVerticalSliderBank(env1Body, { "env1Attack", "env1Decay", "env1Sustain", "env1Release" });

        auto env2Body = getPanelBody(env2PanelBounds);
        modEnvelopeDisplay.setBounds(env2Body.removeFromTop(126));
        env2Body.removeFromTop(4);
        layoutVerticalSliderBank(env2Body.removeFromTop(160), { "env2Attack", "env2Decay", "env2Sustain", "env2Release" });
        layoutChoiceRow(env2Body.removeFromTop(54), { "velocityDestination" });
        env2Body.removeFromTop(4);
        layoutKnobGrid(env2Body, { "env2ToPitch", "velocitySensitivity", "velocityAmount" }, 3);

        layoutLfoSections(lfoPanelBounds);
    }
    else if (activeTab == ActiveTab::effects)
    {
        fxPanelBounds = contentArea.removeFromLeft(static_cast<int>(contentArea.getWidth() * 0.6f));
        contentArea.removeFromLeft(gap);
        matrixPanelBounds = contentArea;

        auto fxBody = getPanelBody(fxPanelBounds);
        fxTabsBounds = fxBody.removeFromTop(34);
        auto fxTabs = fxTabsBounds.reduced(2, 2);
        const auto fxTabGap = 6;
        const auto fxTabWidth = (fxTabs.getWidth() - fxTabGap * 5) / 6;
        delayFxTabButton.setBounds(fxTabs.removeFromLeft(fxTabWidth));
        fxTabs.removeFromLeft(fxTabGap);
        chorusFxTabButton.setBounds(fxTabs.removeFromLeft(fxTabWidth));
        fxTabs.removeFromLeft(fxTabGap);
        driveFxTabButton.setBounds(fxTabs.removeFromLeft(fxTabWidth));
        fxTabs.removeFromLeft(fxTabGap);
        dynamicsFxTabButton.setBounds(fxTabs.removeFromLeft(fxTabWidth));
        fxTabs.removeFromLeft(fxTabGap);
        reverbFxTabButton.setBounds(fxTabs.removeFromLeft(fxTabWidth));
        fxTabs.removeFromLeft(fxTabGap);
        crusherFxTabButton.setBounds(fxTabs);

        fxBody.removeFromTop(gap);

        if (effectsDetailTab == EffectsDetailTab::delay)
        {
            layoutToggleRow(fxBody.removeFromTop(28).removeFromLeft(280), { "delayBypass", "delaySync" });
            fxBody.removeFromTop(6);
            layoutChoiceRow(fxBody.removeFromTop(48), { "delayDivision" });
            fxBody.removeFromTop(6);
            layoutKnobGrid(fxBody, { "delayMix", "delayFeedback", "delayTime" }, 3, 16, 2, 10.0f, 1);
        }
        else if (effectsDetailTab == EffectsDetailTab::chorus)
        {
            layoutToggleRow(fxBody.removeFromTop(28).removeFromLeft(140), { "chorusBypass" });
            fxBody.removeFromTop(6);
            layoutKnobGrid(fxBody, { "chorusMix", "chorusRate", "chorusDepth" }, 3, 16, 2, 10.0f, 1);
        }
        else if (effectsDetailTab == EffectsDetailTab::drive)
        {
            layoutToggleRow(fxBody.removeFromTop(28).removeFromLeft(140), { "driveBypass" });
            fxBody.removeFromTop(6);
            layoutKnobGrid(fxBody, { "distortionAmount", "saturationAmount" }, 2, 16, 2, 10.0f, 1);
        }
        else if (effectsDetailTab == EffectsDetailTab::dynamics)
        {
            layoutToggleRow(fxBody.removeFromTop(28).removeFromLeft(160), { "compressorBypass" });
            fxBody.removeFromTop(6);
            layoutKnobGrid(fxBody, { "compressorThreshold", "compressorRatio" }, 2, 16, 2, 10.0f, 1);
        }
        else if (effectsDetailTab == EffectsDetailTab::reverb)
        {
            layoutToggleRow(fxBody.removeFromTop(28).removeFromLeft(140), { "reverbBypass" });
            fxBody.removeFromTop(6);
            layoutKnobGrid(fxBody, { "reverbMix", "reverbSize", "reverbDamping" }, 3, 16, 2, 10.0f, 1);
        }
        else
        {
            layoutToggleRow(fxBody.removeFromTop(28).removeFromLeft(160), { "bitcrusherBypass" });
            fxBody.removeFromTop(6);
            layoutKnobGrid(fxBody, { "bitcrusherMix", "bitDepth", "bitDownsample" }, 3, 16, 2, 10.0f, 1);
        }

        layoutMatrixRows(matrixPanelBounds);
    }
    else if (activeTab == ActiveTab::sequencer)
    {
        sequencerPanelBounds = contentArea;
        layoutSequencerPanel(sequencerPanelBounds);
    }

    auto keyboardBody = getPanelBody(keyboardPanelBounds).reduced(4, 2);
    updateKeyboardWidth(keyboardBody);
}
