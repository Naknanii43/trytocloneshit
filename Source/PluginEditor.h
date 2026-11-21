#pragma once

#include "PluginProcessor.h"
#include "NebulonLookAndFeel.h"
#include "Pages.h"

class NebulonEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NebulonEditor (NebulonProcessor&);
    ~NebulonEditor() override;

    void resized() override;
    void showPage (int index);

    static constexpr int designW = 1200, designH = 820;

private:
    class Scope : public juce::Component
    {
    public:
        explicit Scope (NebulonProcessor& p) : proc (p) {}
        void paint (juce::Graphics&) override;
    private:
        NebulonProcessor& proc;
    };

    // Everything is laid out at a fixed design size and scaled as a whole.
    class Content : public juce::Component
    {
    public:
        explicit Content (NebulonEditor& e) : owner (e) {}
        void paint (juce::Graphics&) override;
        void resized() override;
    private:
        NebulonEditor& owner;
    };

    void refreshPresetBox();
    void savePresetDialog();
    void timerCallback() override;

    NebulonProcessor& proc;
    neb::LookAndFeel lnf;
    Content content { *this };

    std::vector<std::unique_ptr<neb::ui::Page>> pages;
    std::vector<std::unique_ptr<juce::TextButton>> tabs;
    int currentPage = 0;

    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" };
    neb::ui::IconButton previewBtn { "PREVIEW", neb::ui::IconButton::play, neb::colours::layerA };
    neb::ui::IconButton inspireBtn { "INSPIRE", neb::ui::IconButton::dice, neb::colours::layerB };
    juce::String infoText, lastPresetSeen;
    juce::Rectangle<int> infoArea;
    double presetChangedAt = 0;
    int tipIndex = 0;
    bool focusGrabbed = false;
    void updateInfo();
    juce::Array<juce::File> userPresetFiles;
    Scope scope { proc };
    juce::MidiKeyboardComponent keyboard;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NebulonEditor)
};
