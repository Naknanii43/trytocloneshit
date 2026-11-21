#pragma once

#include "PluginProcessor.h"
#include "NebulonLookAndFeel.h"

class NebulonEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NebulonEditor (NebulonProcessor&);
    ~NebulonEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Section
    {
        juce::String title;
        juce::Colour colour;
        std::vector<Knob*> knobs;
        juce::Component* lead = nullptr;   // optional non-knob control placed first (e.g. a combo box)
        juce::Rectangle<int> bounds;
    };

    // Animated output display
    class Scope : public juce::Component
    {
    public:
        explicit Scope (NebulonProcessor& p) : proc (p) {}
        void paint (juce::Graphics&) override;
        float phase = 0.0f;
    private:
        NebulonProcessor& proc;
    };

    struct LayerUI
    {
        juce::ToggleButton power;
        juce::ComboBox filterType;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterAtt;
        juce::Rectangle<int> bounds;
        std::vector<Section> rows[4];
    };

    Knob* makeKnob (const juce::String& paramId, const juce::String& name, juce::Colour accent);
    void buildLayer (int index, const juce::String& prefix, juce::Colour accent);
    void layoutSection (Section&);
    void refreshPresetBox();
    void timerCallback() override;

    NebulonProcessor& proc;
    neb::LookAndFeel lnf;

    std::vector<std::unique_ptr<Knob>> knobs;
    LayerUI layers[2];
    std::vector<Section> fxSections;

    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
    Scope scope;
    juce::MidiKeyboardComponent keyboard;
    juce::Rectangle<int> headerArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NebulonEditor)
};
