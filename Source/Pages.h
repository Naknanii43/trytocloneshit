#pragma once

#include "Controls.h"

namespace neb::ui
{
//==============================================================================
class LayerDisplay : public juce::Component
{
public:
    LayerDisplay (NebulonProcessor& p, int l) : proc (p), layer (l) {}
    void paint (juce::Graphics&) override;

private:
    float param (const char* id) const { return proc.apvts.getRawParameterValue (layerPrefix (layer) + id)->load(); }
    NebulonProcessor& proc;
    int layer;
};

//==============================================================================
class MainPage : public Page
{
public:
    MainPage (NebulonProcessor&, juce::Component* popup, std::function<void (int)> onEditLayer);
    void resized() override;

private:
    Panel* strips[numLayers] {};
    Panel* macros = nullptr;
    Panel* perf = nullptr;
    juce::TextButton editButtons[numLayers];
};

//==============================================================================
class LayerPage : public Page
{
public:
    LayerPage (NebulonProcessor&, juce::Component* popup, int layer);
    void resized() override;
    void refresh() override;

private:
    int layer;
    int lastMode = -1;
    Panel *osc{}, *pitch{}, *texture{}, *filter{}, *output{}, *ampEnv{}, *filtEnv{};
    LayerDisplay display;
    juce::TextButton loadButton { "Load Sample..." };
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<juce::Component*> analogCtl, wtCtl, fmCtl, granCtl, smpCtl, sourceCtl;
};

//==============================================================================
class MatrixRow : public juce::Component
{
public:
    MatrixRow (NebulonProcessor&, int slot, juce::Component* popup);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    int slot;
    juce::ComboBox src, dst, lay;
    juce::Slider amount;
    std::unique_ptr<APVTS::ComboBoxAttachment> srcAtt, dstAtt, layAtt;
    std::unique_ptr<APVTS::SliderAttachment> amtAtt;
};

class ModPage : public Page
{
public:
    ModPage (NebulonProcessor&, juce::Component* popup);
    void resized() override;

private:
    Panel* lfos[numLfos] {};
    Panel* envs[numModEnvs] {};
    Panel* matrix = nullptr;
};

//==============================================================================
class StepDisplay : public juce::Component
{
public:
    explicit StepDisplay (NebulonProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;

private:
    NebulonProcessor& proc;
};

class ArpPage : public Page
{
public:
    ArpPage (NebulonProcessor&, juce::Component* popup);
    void resized() override;
    void refresh() override { steps.repaint(); }

private:
    Panel* arpPanel = nullptr;
    StepDisplay steps;
};

//==============================================================================
class FxPage : public Page
{
public:
    FxPage (NebulonProcessor&, juce::Component* popup);
    void resized() override;

private:
    Panel *dist{}, *eq{}, *phaser{}, *chorus{}, *delay{}, *reverb{};
};
} // namespace neb::ui
