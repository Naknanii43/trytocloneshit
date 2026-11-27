// UI for the TEXTURE page.
#pragma once
#include "Widgets.h"
#include "Texture.h"

namespace rs::ui
{
class FormantPanel : public ModulePanel
{
public:
    explicit FormantPanel (APVTS& s) : ModulePanel (s, "FORMANT", "horn size", "fmt_on", col::fmt, 2)
    {
        knob ("fmt_shift", "SIZE", true).setTooltip ("Left: a giant bass sax. Right: a tiny sopranino. The pitch stays where you play it.");
        knob ("fmt_mix", "MIX");
    }
};

class TalkPanel : public ModulePanel
{
public:
    TalkPanel (APVTS& s, TalkBox& t) : ModulePanel (s, "TALK BOX", "vowels A-E-I-O-U", "tlk_on", col::tlk, 3), talk (t)
    {
        combo ("tlk_mode", "Mode");
        knob ("tlk_vowel", "VOWEL").setTooltip ("Manual mode: which vowel your sax sings");
        knob ("tlk_rate", "RATE");
        knob ("tlk_sens", "SENS").setTooltip ("Envelope mode: how much your playing level opens the mouth");
        knob ("tlk_reso", "RESO");
        knob ("tlk_mix", "MIX");
    }
    void update()
    {
        const float v = talk.currentVowel.load();
        setActivity (isOn() ? v : -1.0f);
        if (std::abs (v - shown) > 0.01f) { shown = v; repaint (getWidth() - 120, 0, 120, 40); }
    }
    void paintOverChildren (juce::Graphics& g) override
    {
        if (! isOn()) return;
        static const char* vw[] = { "A", "E", "I", "O", "U" };
        g.setColour (accent());
        g.setFont (font (20.0f, true));
        g.drawText (vw[juce::jlimit (0, 4, juce::roundToInt (shown * 4.0f))], getLocalBounds().reduced (44, 6).removeFromTop (26),
                    juce::Justification::centredRight);
    }
private:
    TalkBox& talk;
    float shown = 0;
};

class DoublerPanel : public ModulePanel
{
public:
    explicit DoublerPanel (APVTS& s) : ModulePanel (s, "DOUBLER", "one sax becomes a section", "dbl_on", col::dbl, 2)
    {
        combo ("dbl_players", "Players");
        knob ("dbl_detune", "DETUNE").setTooltip ("How far out of tune the other players drift");
        knob ("dbl_timing", "TIMING").setTooltip ("How late the other players come in");
        knob ("dbl_width", "WIDTH");
        knob ("dbl_mix", "MIX");
    }
};

class TremoloPanel : public ModulePanel
{
public:
    explicit TremoloPanel (APVTS& s) : ModulePanel (s, "TREMOLO", "pulse & pan", "trm_on", col::trm, 2)
    {
        combo ("trm_mode", "Mode");
        combo ("trm_shape", "Shape");
        combo ("trm_sync", "Sync");
        knob ("trm_rate", "RATE").setTooltip ("Speed when Sync is Free");
        knob ("trm_depth", "DEPTH");
    }
};

class BeatRepeatPanel : public ModulePanel
{
public:
    BeatRepeatPanel (APVTS& s, BeatRepeat& b) : ModulePanel (s, "BEAT REPEAT", "DJ-style stutter", "brp_on", col::brp, 4), repeat (b)
    {
        combo ("brp_mode", "Mode");
        combo ("brp_grid", "Grid");
        combo ("brp_length", "Auto length");
        knob ("brp_chance", "CHANCE").setTooltip ("Auto mode: probability of a repeat on each beat");
        knob ("brp_pitch", "PITCH", true).setTooltip ("Each repeat drops (or rises) this much: tape-stop dives and rising glitches");
        knob ("brp_decay", "DECAY");
        knob ("brp_mix", "MIX");
        big.setButtonText ("REPEAT");
        big.setTooltip ("Manual mode: hold to stutter the last slice you played");
        big.onStateChange = [this]
        {
            const bool down = big.isDown();
            if (down == lastDown) return;
            lastDown = down;
            if (auto* prm = state.getParameter ("brp_hold"))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (down ? 1.0f : 0.0f);
                prm->endChangeGesture();
            }
        };
        addAndMakeVisible (big);
    }
    void update()
    {
        const bool r = repeat.repeating.load();
        if (r != shown) { shown = r; big.setColour (juce::TextButton::textColourOffId, r ? accent() : col::text); repaint(); }
    }
    void resized() override
    {
        auto r = contentArea();
        auto left = r.removeFromLeft (juce::jmin (150, r.getWidth() / 4));
        big.setBounds (left.reduced (2, 8));
        r.removeFromLeft (12);
        auto top = r.removeFromTop (42);
        const int w = (top.getWidth() - 16) / 3;
        for (int k = 0; k < 3; ++k) { selectors[(size_t) k]->setBounds (top.removeFromLeft (w)); top.removeFromLeft (8); }
        r.removeFromTop (6);
        extraCellBounds.clear();
        layoutKnobGrid (r, 4);
    }
    void paint (juce::Graphics& g) override
    {
        ModulePanel::paint (g);
        if (shown && isOn())
        {
            g.setColour (accent().withAlpha (0.25f));
            g.fillRoundedRectangle (big.getBounds().toFloat().expanded (6), 10.0f);
        }
    }
    void refreshActive() override { ModulePanel::refreshActive(); big.setAlpha (isOn() ? 1.0f : 0.42f); }
private:
    BeatRepeat& repeat;
    juce::TextButton big;
    bool lastDown = false, shown = false;
};

class SpringPanel : public ModulePanel
{
public:
    explicit SpringPanel (APVTS& s) : ModulePanel (s, "SPRING", "boingy tank reverb", "spr_on", col::spr, 3)
    {
        knob ("spr_decay", "DECAY");
        knob ("spr_tone", "TONE");
        knob ("spr_tension", "TENSION").setTooltip ("Tight, short spring vs. loose, long and drippy");
        knob ("spr_drip", "DRIP").setTooltip ("How hard attacks splash the springs (the surf 'drip')");
        knob ("spr_mix", "MIX");
    }
};

class CloudPanel : public ModulePanel
{
public:
    explicit CloudPanel (APVTS& s) : ModulePanel (s, "CLOUD", "granular haze", "cld_on", col::cld, 4)
    {
        combo ("cld_pmode", "Pitch mode");
        knob ("cld_density", "DENSITY");
        knob ("cld_size", "SIZE");
        knob ("cld_spray", "SPRAY").setTooltip ("How far back in time grains are taken from (up to ~2 s)");
        knob ("cld_pitch", "PITCH").setTooltip ("Free: random detune. Octaves / Fifths: how often grains jump to those intervals");
        knob ("cld_reverse", "REVERSE");
        knob ("cld_width", "WIDTH");
        knob ("cld_mix", "MIX");
    }
};
} // namespace rs::ui
