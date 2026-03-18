#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class EnvelopeDisplay final : public juce::Component, private juce::Timer
{
public:
    EnvelopeDisplay(AISynthAudioProcessor& processorRef, juce::String prefixToUse, juce::String titleToUse);

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    AISynthAudioProcessor& processor;
    juce::String prefix;
    juce::String title;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeDisplay)
};

class AISynthAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AISynthAudioProcessorEditor(AISynthAudioProcessor&);
    ~AISynthAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    enum class ActiveTab
    {
        voice,
        modulation,
        effects,
        sequencer
    };

    struct ThemePalette
    {
        juce::String name;
        juce::Colour backgroundTop;
        juce::Colour backgroundBottom;
        juce::Colour panelTop;
        juce::Colour panelBottom;
        juce::Colour accent;
        juce::Colour secondary;
        juce::Colour text;
        juce::Colour mutedText;
        juce::Colour border;
        juce::Colour overlay;
        juce::Colour knobOuter;
        juce::Colour knobInner;
        juce::Colour knobHighlight;
        juce::Colour sliderTrack;
        juce::Colour sliderThumb;
        bool showGrid { false };
        bool showScanlines { false };
        bool showVignette { false };
        bool showWarningStripes { false };
        bool showGlass { false };
        bool showGrunge { false };
        bool showCircuitLines { false };
        bool wornMetalKnobs { false };
        bool flatPanels { false };
        float glowAmount { 0.25f };
    };

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    class SynthLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        void setTheme(const ThemePalette& newTheme, float newAnimationPhase);
        void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                              float sliderPosProportional, float rotaryStartAngle,
                              float rotaryEndAngle, juce::Slider&) override;
        void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height,
                              float sliderPos, float minSliderPos, float maxSliderPos,
                              const juce::Slider::SliderStyle, juce::Slider&) override;
        void drawComboBox(juce::Graphics&, int width, int height, bool,
                          int buttonX, int buttonY, int buttonW, int buttonH,
                          juce::ComboBox&) override;
        juce::Font getComboBoxFont(juce::ComboBox&) override;
        juce::Label* createSliderTextBox(juce::Slider&) override;

    private:
        ThemePalette theme;
        float animationPhase { 0.0f };
    };

    class MidiLearnSlider final : public juce::Slider
    {
    public:
        std::function<void()> onContextMenuRequested;

        void mouseDown(const juce::MouseEvent& event) override
        {
            if (event.mods.isPopupMenu() && onContextMenuRequested)
            {
                onContextMenuRequested();
                return;
            }

            juce::Slider::mouseDown(event);
        }
    };

    struct SliderMeta
    {
        juce::String id;
        juce::Slider* slider {};
        juce::Label* label {};
        juce::String baseTooltip;
    };

    struct ChoiceMeta
    {
        juce::String id;
        juce::ComboBox* combo {};
        juce::Label* label {};
    };

    struct ToggleMeta
    {
        juce::String id;
        juce::ToggleButton* button {};
    };

    juce::Slider& addSlider(const juce::String& id, const juce::String& title, const juce::String& tooltip, juce::Slider::SliderStyle style);
    juce::ComboBox& addChoice(const juce::String& id, const juce::String& title, const juce::StringArray& items, const juce::String& tooltip);
    juce::ToggleButton& addToggle(const juce::String& id, const juce::String& title, const juce::String& tooltip);

    SliderMeta* findSlider(const juce::String& id);
    ChoiceMeta* findChoice(const juce::String& id);
    ToggleMeta* findToggle(const juce::String& id);

    void layoutKnobGrid(juce::Rectangle<int> area, std::initializer_list<const char*> ids, int columns);
    void layoutChoiceRow(juce::Rectangle<int> area, std::initializer_list<const char*> ids);
    void layoutToggleRow(juce::Rectangle<int> area, std::initializer_list<const char*> ids);
    void layoutVerticalSliderBank(juce::Rectangle<int> area, std::initializer_list<const char*> ids);
    void layoutMatrixRows(juce::Rectangle<int> area);
    void layoutOscillatorSections(juce::Rectangle<int> area);
    void layoutLfoSections(juce::Rectangle<int> area);
    void layoutPresetPanel(juce::Rectangle<int> area);
    void layoutSequencerPanel(juce::Rectangle<int> area);
    void hideAllDynamicControls();
    void timerCallback() override;
    void applyTheme();
    void refreshBackgroundTexture();
    void refreshPresetCombo(bool preserveSelection);
    void refreshPatternCombo(bool preserveSelection);
    void refreshMidiLearnDecorations();
    void sendPitchWheelMessage();
    void sendModWheelMessage();
    void savePresetFromEditor();
    void savePatternFromEditor();
    void showSliderMidiLearnMenu(const juce::String& parameterId, juce::Slider& slider, const juce::String& title);
    void showPresetMenu();
    void setActiveTab(ActiveTab newTab);
    void stylePerformanceSlider(juce::Slider& slider, juce::Label& label, const juce::String& text, bool centreOnRelease);
    void updateKeyboardWidth(const juce::Rectangle<int>& area);
    static ThemePalette createThemePalette(int themeIndex, float animationPhase);
    static juce::String getTopLevelGroupName(const juce::String& fullName);
    static juce::String getLeafName(const juce::String& fullName);

    AISynthAudioProcessor& audioProcessor;
    SynthLookAndFeel synthLookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 250 };

    EnvelopeDisplay ampEnvelopeDisplay;
    EnvelopeDisplay modEnvelopeDisplay;
    juce::MidiKeyboardComponent keyboardComponent;
    juce::Image backgroundTexture;

    juce::ComboBox presetCombo;
    juce::ComboBox presetGroupCombo;
    juce::TextEditor presetSearchEditor;
    juce::TextEditor presetNameEditor;
    juce::Label presetNameLabel;
    juce::Label presetGroupLabel;
    juce::ComboBox themeCombo;
    juce::Label themeLabel;
    juce::TextButton savePresetButton;
    juce::TextButton reloadPresetButton;
    juce::TextButton initPresetButton;
    juce::TextButton presetMenuButton;
    juce::ComboBox patternCombo;
    juce::TextEditor patternNameEditor;
    juce::Label patternLabel;
    juce::TextButton savePatternButton;
    juce::TextButton reloadPatternButton;
    juce::TextButton voiceTabButton;
    juce::TextButton modulationTabButton;
    juce::TextButton effectsTabButton;
    juce::TextButton sequencerTabButton;
    bool updatingPresetCombo { false };
    bool updatingPatternCombo { false };
    ActiveTab activeTab { ActiveTab::voice };
    ThemePalette currentTheme;
    float animationPhase { 0.0f };
    juce::StringArray filteredPresetNames;
    juce::StringArray filteredPatternNames;

    juce::OwnedArray<juce::Slider> sliders;
    juce::OwnedArray<juce::ComboBox> comboBoxes;
    juce::OwnedArray<juce::ToggleButton> toggles;
    juce::OwnedArray<juce::Label> labels;

    std::vector<SliderMeta> sliderMetas;
    std::vector<ChoiceMeta> choiceMetas;
    std::vector<ToggleMeta> toggleMetas;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ComboAttachment>> comboAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;

    juce::Slider pitchWheelSlider;
    juce::Slider modWheelSlider;
    juce::Label pitchWheelLabel;
    juce::Label modWheelLabel;

    juce::Rectangle<int> headerBounds;
    juce::Rectangle<int> presetPanelBounds;
    juce::Rectangle<int> oscillatorPanelBounds;
    juce::Rectangle<int> filterPanelBounds;
    juce::Rectangle<int> arpPanelBounds;
    juce::Rectangle<int> env1PanelBounds;
    juce::Rectangle<int> env2PanelBounds;
    juce::Rectangle<int> lfoPanelBounds;
    juce::Rectangle<int> fxPanelBounds;
    juce::Rectangle<int> matrixPanelBounds;
    juce::Rectangle<int> sequencerPanelBounds;
    juce::Rectangle<int> keyboardPanelBounds;

    std::array<juce::Rectangle<int>, 4> oscillatorSectionBounds {};
    std::array<juce::Rectangle<int>, 3> lfoSectionBounds {};
    std::array<juce::Rectangle<int>, 8> sequencerStepBounds {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AISynthAudioProcessorEditor)
};
