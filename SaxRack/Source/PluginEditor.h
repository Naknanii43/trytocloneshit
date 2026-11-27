#pragma once
#include "PluginProcessor.h"
#include "Widgets.h"
#include "GenLoopUI.h"
#include "MangleUI.h"
#include "TextureUI.h"

/** All UI at a fixed design size; the editor scales it to whatever size the window is. */
class ReedstormContent : public juce::Component, private juce::Timer
{
public:
    static constexpr int designW = 1280, designH = 840;

    explicit ReedstormContent (ReedstormProcessor&);
    ~ReedstormContent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void buildPresetMenu();
    void stepPreset (int delta);
    rs::ui::ModulePanel& addPanel (const juce::String& title, const juce::String& sub, const juce::String& powerId,
                                   juce::Colour c, int cols, int row);

    ReedstormProcessor& proc;
    rs::ui::ReedLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };
    juce::String presetDescription;

    rs::ui::TunerDisplay tuner;
    rs::ui::PitchTrace trace;
    std::unique_ptr<rs::ui::Combo> instrument;
    juce::Slider refSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> refAttachment;

    rs::ui::Meter inMeter { "IN" }, outMeterL { "L" }, outMeterR { "R" }, grMeter { "GR", true };
    std::unique_ptr<rs::ui::Knob> inGain, outGain, width;
    std::unique_ptr<rs::ui::ChainStrip> chain;
    std::unique_ptr<rs::ui::EqCurve> eqCurve;

    std::vector<std::unique_ptr<rs::ui::ModulePanel>> panels;
    std::vector<int> panelRow, panelCols;
    rs::ui::ModulePanel* dynPanel = nullptr;
    rs::ui::ModulePanel* wahPanel = nullptr;
    rs::ui::ModulePanel* harPanel = nullptr;
    rs::ui::Knob* delayTimeKnob = nullptr;
    rs::ui::GeneratorPanel* genPanel = nullptr;
    rs::ui::LoopStationPanel* loopPanel = nullptr;
    rs::ui::GremlinsPanel* grmPanel = nullptr;
    std::vector<int> panelPage;
    juce::TextButton tabs[5];
    rs::ui::TalkPanel* tlkPanel = nullptr;
    rs::ui::BeatRepeatPanel* brpPanel = nullptr;
    rs::ui::WhammyPanel* whmPanel = nullptr;
    rs::ui::FreezePanel* frzPanel = nullptr;
    rs::ui::SlicerPanel* gatPanel = nullptr;
    int page = 0;

public:
    void setPage (int p);

    juce::Image texture;
    int shownPreset = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReedstormContent)
};

class ReedstormEditor : public juce::AudioProcessorEditor
{
public:
    explicit ReedstormEditor (ReedstormProcessor&);
    void setPage (int p) { content.setPage (p); }
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (rs::ui::col::bg); }

private:
    ReedstormContent content;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReedstormEditor)
};
