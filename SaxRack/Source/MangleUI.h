// UI for the MANGLE page: Whammy, Ring Mod, Freeze, Slicer.
#pragma once
#include "Widgets.h"
#include "Mangle.h"

namespace rs::ui
{
class WhammyPanel : public ModulePanel
{
public:
    WhammyPanel (APVTS& s, Whammy& w) : ModulePanel (s, "WHAMMY", "pitch bends & dive bombs", "whm_on", col::whm, 3), whammy (w)
    {
        combo ("whm_mode", "Mode");
        knob ("whm_amount", "AMOUNT", true).setTooltip ("How far to bend, in semitones (negative = down)");
        knob ("whm_pedal", "PEDAL").setTooltip ("Pedal mode: the bend position. Map it to an expression pedal!");
        knob ("whm_rate", "RATE").setTooltip ("LFO mode: sweep speed. Random mode: how often it jumps");
        knob ("whm_glide", "GLIDE").setTooltip ("Smoothness of the bend. Scoop mode: how long each note takes to slide in");
        knob ("whm_mix", "MIX");
    }
    void update()
    {
        const float sh = whammy.currentShift.load();
        setActivity (isOn() ? juce::jmin (1.0f, std::abs (sh) / 24.0f) : -1.0f);
        if (std::abs (sh - shown) > 0.05f) { shown = sh; repaint (getWidth() - 140, 0, 140, 40); }
    }
    void paintOverChildren (juce::Graphics& g) override
    {
        if (! isOn()) return;
        g.setColour (accent());
        g.setFont (font (14.0f, true));
        g.drawText ((shown >= 0 ? "+" : "") + juce::String (shown, 1) + " st", getLocalBounds().reduced (44, 8).removeFromTop (24),
                    juce::Justification::centredRight);
    }
private:
    Whammy& whammy;
    float shown = 0;
};

class RingPanel : public ModulePanel
{
public:
    explicit RingPanel (APVTS& s) : ModulePanel (s, "RING MOD", "metal, bells & robots", "rng_on", col::ring, 3)
    {
        combo ("rng_mode", "Carrier");
        combo ("rng_wave", "Wave");
        knob ("rng_freq", "FREQ").setTooltip ("Fixed mode: carrier frequency");
        knob ("rng_ratio", "RATIO").setTooltip ("Tracked mode: carrier = your note x ratio. Whole numbers ring like bells, odd ratios clang like metal");
        knob ("rng_rate", "SWEEP RATE");
        knob ("rng_sweep", "SWEEP").setTooltip ("Sweeps the carrier up and down (up to +-2 octaves)");
        knob ("rng_mix", "MIX");
    }
};

class FreezePanel : public ModulePanel
{
public:
    FreezePanel (APVTS& s, Freeze& f) : ModulePanel (s, "FREEZE", "infinite sustain", "frz_on", col::frz, 2), freeze (f)
    {
        combo ("frz_mode", "Mode");
        combo ("frz_pitch", "Pitch");
        toggle ("frz_stack", "STACK");
        switches[0]->button.setTooltip ("Manual: each press adds a layer instead of replacing the frozen sound");
        knob ("frz_decay", "DECAY").setTooltip ("0 = frozen forever. Higher values let frozen layers fade away");
        knob ("frz_attack", "ATTACK");
        knob ("frz_width", "WIDTH");
        knob ("frz_level", "LEVEL");
        big.setButtonText ("FREEZE");
        big.setClickingTogglesState (true);
        big.setTooltip ("Hold your note and press: the sound is frozen into an endless pad. Press again to release. "
                        "In Auto mode every steadily held note is frozen and stacked by itself.");
        addAndMakeVisible (big);
        holdAttachment = std::make_unique<APVTS::ButtonAttachment> (s, "frz_hold", big);
    }

    void update()
    {
        const int layers = freeze.layersActive.load();
        if (layers != shownLayers) { shownLayers = layers; repaint(); }
        big.setButtonText (big.getToggleState() ? "FROZEN" : "FREEZE");
    }

    void resized() override
    {
        auto r = contentArea();
        auto left = r.removeFromLeft (juce::jmin (210, r.getWidth() / 3));
        bigArea = left.reduced (4, 10);
        big.setBounds (bigArea.withTrimmedBottom (26));
        r.removeFromLeft (14);
        auto top = r.removeFromTop (42);
        const int w = (top.getWidth() - 16) / 3;
        selectors[0]->setBounds (top.removeFromLeft (w)); top.removeFromLeft (8);
        selectors[1]->setBounds (top.removeFromLeft (w)); top.removeFromLeft (8);
        switches[0]->setBounds (top);
        r.removeFromTop (6);
        extraCellBounds.clear();
        layoutKnobGrid (r, 4);
    }

    void paint (juce::Graphics& g) override
    {
        ModulePanel::paint (g);
        if (shownLayers > 0 && isOn())
        {
            g.setColour (accent().withAlpha (0.18f));
            g.fillRoundedRectangle (big.getBounds().toFloat().expanded (6), 10.0f);
        }
        auto dots = bigArea.removeFromBottom (18).toFloat();
        for (int k = 0; k < Freeze::numLayers; ++k)
        {
            auto d = juce::Rectangle<float> (12, 12).withCentre ({ dots.getCentreX() + (float) (k - 1) * 22.0f, dots.getCentreY() });
            g.setColour (k < shownLayers ? accent() : col::edge.brighter (0.3f));
            g.fillEllipse (d);
        }
    }

    void refreshActive() override
    {
        ModulePanel::refreshActive();
        big.setAlpha (isOn() ? 1.0f : 0.42f);
    }

private:
    Freeze& freeze;
    juce::TextButton big;
    std::unique_ptr<APVTS::ButtonAttachment> holdAttachment;
    juce::Rectangle<int> bigArea;
    int shownLayers = -1;
};

/** 16-step pattern view with the playing step highlighted. */
class StepGrid : public juce::Component
{
public:
    StepGrid (APVTS& s, Slicer& sl) : state (s), slicer (sl) {}
    void update()
    {
        const int st = slicer.currentStep.load();
        if (st != shownStep || ++frames % 15 == 0) { shownStep = st; repaint(); }
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::screen);
        g.fillRoundedRectangle (r, 5.0f);
        const int pat = (int) std::lround (state.getRawParameterValue ("gat_pattern")->load());
        const bool pp = state.getRawParameterValue ("gat_pp")->load() > 0.5f;
        auto cells = r.reduced (6.0f, 6.0f);
        const float w = cells.getWidth() / 16.0f;
        for (int k = 0; k < 16; ++k)
        {
            const bool on = pat == 6 ? slicer.randomStep (k) : Slicer::patternSteps (pat)[k] == 'x';
            auto c = juce::Rectangle<float> (cells.getX() + (float) k * w + 2.0f, cells.getY(), w - 4.0f, cells.getHeight());
            const bool now = k == shownStep;
            g.setColour (on ? col::gate.withAlpha (now ? 1.0f : 0.55f) : (now ? col::dim.withAlpha (0.5f) : juce::Colour (0xff1d1a16)));
            g.fillRoundedRectangle (c, 3.0f);
            if (on && pp)
            {
                g.setColour (juce::Colours::black.withAlpha (0.7f));
                g.setFont (font (9.0f, true));
                g.drawText (k % 2 == 0 ? "L" : "R", c, juce::Justification::centred);
            }
            if (k % 4 == 0)
            {
                g.setColour (col::dim.withAlpha (0.5f));
                g.fillRect (c.getX() - 2.0f, cells.getBottom() + 1.0f, 1.0f, 4.0f);
            }
        }
    }
private:
    APVTS& state;
    Slicer& slicer;
    int shownStep = -1, frames = 0;
};

class SlicerPanel : public ModulePanel
{
public:
    SlicerPanel (APVTS& s, Slicer& sl) : ModulePanel (s, "SLICER", "tempo-synced gate", "gat_on", col::gate, 3), grid (s, sl)
    {
        combo ("gat_pattern", "Pattern");
        combo ("gat_rate", "Rate");
        toggle ("gat_pp", "PING-PONG");
        knob ("gat_depth", "DEPTH");
        knob ("gat_smooth", "SMOOTH").setTooltip ("Short = hard chops, long = soft pulsing");
        knob ("gat_swing", "SWING");
        addAndMakeVisible (grid);
    }
    void update() { grid.update(); }
    void resized() override
    {
        auto r = contentArea();
        auto top = r.removeFromTop (42);
        const int w = (top.getWidth() - 16) / 3;
        selectors[0]->setBounds (top.removeFromLeft (w)); top.removeFromLeft (8);
        selectors[1]->setBounds (top.removeFromLeft (w)); top.removeFromLeft (8);
        switches[0]->setBounds (top);
        r.removeFromTop (8);
        grid.setBounds (r.removeFromTop (juce::jmax (34, r.getHeight() / 3)));
        r.removeFromTop (4);
        extraCellBounds.clear();
        layoutKnobGrid (r, 3);
    }
    void refreshActive() override { ModulePanel::refreshActive(); grid.setAlpha (isOn() ? 1.0f : 0.42f); }
private:
    StepGrid grid;
};
} // namespace rs::ui
