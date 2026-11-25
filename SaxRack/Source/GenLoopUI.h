// UI for the Morph Generator (frequency-zone map) and the Looper.
#pragma once
#include "Widgets.h"
#include "Generator.h"
#include "Looper.h"

namespace rs::ui
{
//==============================================================================
/** Frequency map: four coloured zones with draggable splits, your live pitch, triggers. */
class ZoneMap : public juce::Component
{
public:
    ZoneMap (APVTS& s, Generator& g) : state (s), gen (g) {}

    void update (bool voiced, float hz)
    {
        livePitch = voiced ? hz : -1.0f;
        const int t = gen.meters.triggers.load();
        if (t != lastTriggers) { lastTriggers = t; flash = 1.0f; flashZone = gen.meters.bloomZone.load(); }
        flash *= 0.9f;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::screen);
        g.fillRoundedRectangle (r, 6.0f);
        auto plot = r.reduced (6.0f, 4.0f).withTrimmedBottom (14.0f).withTrimmedTop (24.0f);
        plotArea = plot;

        const float s[3] = { split (1), split (2), split (3) };
        float sorted[3] = { s[0], s[1], s[2] };
        std::sort (sorted, sorted + 3);
        const int liveZone = livePitch > 0 ? Generator::zoneOf (livePitch, s) : -1;
        const float edges[5] = { lo, sorted[0], sorted[1], sorted[2], hi };

        for (int z = 0; z < 4; ++z)
        {
            const float x0 = xOf (edges[z]), x1 = xOf (edges[z + 1]);
            if (x1 <= x0) continue;
            auto zr = juce::Rectangle<float> (x0, plot.getY(), x1 - x0, plot.getHeight());
            const auto c = col::zone[z];
            const bool live = z == liveZone && gen.meters.morphAmount.load() >= 0.0f;
            g.setColour (c.withAlpha (live ? 0.30f : 0.12f));
            g.fillRect (zr);
            if (flash > 0.02f && z == flashZone)
            {
                g.setColour (c.withAlpha (0.55f * flash));
                g.fillRect (zr);
            }
            g.setColour (c.withAlpha (live ? 1.0f : 0.75f));
            g.fillRect (zr.removeFromBottom (3.0f));
            g.setFont (font (12.0f, true));
            const juce::String name = params::genSounds[soundOf (z)].toUpperCase();
            g.drawFittedText (name, zr.toNearestInt().reduced (2, 0).withTrimmedTop (6).withHeight (16), juce::Justification::centred, 1, 0.7f);
            g.setColour (col::dim);
            g.setFont (font (9.5f));
            g.drawText ("ZONE " + juce::String (z + 1), zr.withTrimmedTop (22).withHeight (12), juce::Justification::centred);
        }

        // note ticks
        g.setFont (font (9.0f));
        for (int m = 36; m <= 96; m += 12)
        {
            const float x = xOf (midiToHz ((float) m));
            if (x < plot.getX() || x > plot.getRight()) continue;
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (col::dim);
            g.drawText (noteName (m), juce::Rectangle<float> (x - 15, plot.getBottom() + 1, 30, 12), juce::Justification::centred);
        }

        // split handles
        for (int k = 0; k < 3; ++k)
        {
            const float x = xOf (s[k]);
            const bool hot = k == dragging || k == hover;
            g.setColour (hot ? col::text : col::dim.brighter (0.3f));
            g.drawLine (x, plot.getY(), x, plot.getBottom(), hot ? 2.0f : 1.2f);
            juce::Path tri;
            tri.addTriangle (x - 5, plot.getY() - 9, x + 5, plot.getY() - 9, x, plot.getY() - 1);
            g.fillPath (tri);
            if (hot)
            {
                g.setFont (font (10.0f, true));
                g.drawText (juce::String (juce::roundToInt (s[k])) + " Hz  " + noteName ((int) std::lround (hzToMidi (s[k]))),
                            juce::Rectangle<float> (x - 50, plot.getY() + plot.getHeight() * 0.62f, 100, 12), juce::Justification::centred);
            }
        }

        // live pitch
        if (livePitch > 0)
        {
            const float x = xOf (livePitch);
            const auto c = liveZone >= 0 ? col::zone[liveZone] : col::text;
            g.setColour (c.withAlpha (0.35f));
            g.fillRect (x - 4.0f, plot.getY(), 8.0f, plot.getHeight());
            g.setColour (juce::Colours::white);
            g.fillRect (x - 1.0f, plot.getY(), 2.0f, plot.getHeight());
        }

        g.setColour (col::gen.withAlpha (0.85f));
        g.setFont (font (9.5f, true).withExtraKerningFactor (0.12f));
        g.drawText ("DRAG THE SPLITS", r.reduced (8, 2).removeFromTop (12), juce::Justification::centredLeft);
        g.drawText ("MORPH " + juce::String (juce::roundToInt (gen.meters.morphAmount.load() * 100)) + "%"
                        + (gen.meters.blooming.load() ? "   BLOOMING" : ""),
                    r.reduced (8, 2).removeFromTop (12), juce::Justification::centredRight);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int h = nearest (e.position.x);
        if (h != hover) { hover = h; repaint(); }
        setMouseCursor (h >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        dragging = nearest (e.position.x);
        if (dragging >= 0) if (auto* p = param (dragging + 1)) p->beginChangeGesture();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging < 0) return;
        if (auto* p = param (dragging + 1))
            p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (60.0f, 1600.0f, hzOf (e.position.x))));
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging >= 0) if (auto* p = param (dragging + 1)) p->endChangeGesture();
        dragging = -1;
    }

private:
    juce::RangedAudioParameter* param (int k) const { return state.getParameter ("gen_split" + juce::String (k)); }
    float split (int k) const { return state.getRawParameterValue ("gen_split" + juce::String (k))->load(); }
    int soundOf (int z) const { return (int) std::lround (state.getRawParameterValue ("gen_snd" + juce::String (z + 1))->load()); }
    float xOf (float hz) const { return plotArea.getX() + plotArea.getWidth() * std::log (hz / lo) / std::log (hi / lo); }
    float hzOf (float x) const { return lo * std::pow (hi / lo, (x - plotArea.getX()) / plotArea.getWidth()); }
    int nearest (float x) const
    {
        int best = -1;
        float bd = 8.0f;
        for (int k = 0; k < 3; ++k)
        {
            const float d = std::abs (xOf (split (k + 1)) - x);
            if (d < bd) { bd = d; best = k; }
        }
        return best;
    }

    APVTS& state;
    Generator& gen;
    juce::Rectangle<float> plotArea { 0, 0, 100, 50 };
    const float lo = 55.0f, hi = 1700.0f;
    float livePitch = -1, flash = 0;
    int lastTriggers = 0, flashZone = 0, hover = -1, dragging = -1;
};

//==============================================================================
class GeneratorPanel : public ModulePanel
{
public:
    GeneratorPanel (APVTS& s, Generator& g)
        : ModulePanel (s, "MORPH GEN", "your pitch picks the synth", "gen_on", col::gen, 4), zones (s, g), generator (g)
    {
        addAndMakeVisible (zones);
        for (int z = 0; z < 4; ++z)
        {
            zoneBoxes[z] = std::make_unique<Combo> (s, "gen_snd" + juce::String (z + 1), "Zone " + juce::String (z + 1), col::zone[z]);
            addAndMakeVisible (*zoneBoxes[z]);
        }
        pattern = std::make_unique<Combo> (s, "gen_pattern", "Bloom pattern", col::gen);
        rate = std::make_unique<Combo> (s, "gen_rate", "Bloom rate", col::gen);
        addAndMakeVisible (*pattern);
        addAndMakeVisible (*rate);

        knob ("gen_morph", "MORPH").setTooltip ("Blend from your sax (0%) to the zone's synth voice (100%)");
        knob ("gen_pmorph", "PITCH>MORPH", true).setTooltip ("Right: higher notes morph further into synth. Left: lower notes do.");
        knob ("gen_bloom", "BLOOM").setTooltip ("Level of the generative pattern a held note triggers in its zone's sound");
        knob ("gen_length", "LENGTH");
        knob ("gen_density", "DENSITY");
        knob ("gen_range", "RANGE").valueText = [this] { return juce::String ((int) std::lround (state.getRawParameterValue ("gen_range")->load())) + " oct"; };
        knob ("gen_hold", "HOLD").setTooltip ("How long a note must be held steady to trigger a bloom");

        fire.setButtonText ("BLOOM!");
        fire.setTooltip ("Trigger a bloom now (on the last note played)");
        fire.onClick = [this] { generator.manualFire.store (true); };
        addAndMakeVisible (fire);
    }

    void resized() override
    {
        auto r = contentArea();
        auto left = r.removeFromLeft ((int) (r.getWidth() * 0.47f));
        r.removeFromLeft (12);
        auto zoneRow = left.removeFromBottom (40);
        left.removeFromBottom (6);
        zones.setBounds (left);
        const int zw = (zoneRow.getWidth() - 3 * 6) / 4;
        for (auto& z : zoneBoxes) { z->setBounds (zoneRow.removeFromLeft (zw)); zoneRow.removeFromLeft (6); }

        auto top = r.removeFromTop (42);
        const int cw = (top.getWidth() - 8) / 2;
        pattern->setBounds (top.removeFromLeft (cw));
        top.removeFromLeft (8);
        rate->setBounds (top);
        r.removeFromTop (4);
        extraCellBounds.clear();
        layoutKnobGrid (r, 4, 1);
        if (! extraCellBounds.empty()) fire.setBounds (extraCellBounds[0].withSizeKeepingCentre (juce::jmin (84, extraCellBounds[0].getWidth()), 30));
    }

    void refreshActive() override
    {
        ModulePanel::refreshActive();
        const float a = isOn() ? 1.0f : 0.42f;
        zones.setAlpha (a);
        for (auto& z : zoneBoxes) z->setAlpha (a);
        pattern->setAlpha (a); rate->setAlpha (a); fire.setAlpha (a);
    }

    ZoneMap zones;

private:
    Generator& generator;
    std::unique_ptr<Combo> zoneBoxes[4], pattern, rate;
    juce::TextButton fire;
};

//==============================================================================
class LoopDisplay : public juce::Component
{
public:
    explicit LoopDisplay (Looper& l) : looper (l) {}

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::screen);
        g.fillRoundedRectangle (r, 6.0f);
        const int state = looper.uiState.load();
        const int len = looper.uiLength.load();
        const int pos = looper.uiPos.load();
        const double sr = looper.getSampleRate();
        const juce::Colour stateCol = state == Looper::Recording ? juce::Colour (0xffff4a4a)
                                    : state == Looper::Overdubbing ? juce::Colour (0xffffa53d)
                                    : state == Looper::Playing ? col::loop : col::dim;

        auto wave = r.reduced (8.0f, 6.0f).withTrimmedTop (14.0f);
        if (len > 0)
        {
            const int shownLen = state == Looper::Recording ? juce::jmax (len, (int) (sr * 4.0)) : len;
            const int nb = juce::jmax (1, len / Looper::binSize);
            const float mid = wave.getCentreY();
            juce::Path p;
            const float bw = wave.getWidth() * (float) Looper::binSize / (float) shownLen;
            for (int b = 0; b < nb; ++b)
            {
                const float v = juce::jmin (1.0f, std::sqrt (looper.bin (b)) * 1.1f) * wave.getHeight() * 0.5f;
                p.addRectangle (wave.getX() + (float) b * bw, mid - v, juce::jmax (1.0f, bw * 0.8f), juce::jmax (1.0f, 2.0f * v));
            }
            g.setColour (stateCol.withAlpha (0.75f));
            g.fillPath (p);
            const float x = wave.getX() + wave.getWidth() * (float) pos / (float) shownLen;
            g.setColour (juce::Colours::white);
            g.fillRect (x - 1.0f, wave.getY(), 2.0f, wave.getHeight());
        }
        else
        {
            g.setColour (col::dim.withAlpha (0.6f));
            g.setFont (font (12.0f));
            g.drawText ("Press REC, play a phrase, press again to loop it", wave, juce::Justification::centred);
        }

        static const char* names[] = { "EMPTY", "RECORDING", "PLAYING", "OVERDUBBING", "STOPPED" };
        auto head = r.reduced (8.0f, 3.0f).removeFromTop (13.0f);
        g.setColour (stateCol);
        g.fillEllipse (head.getX(), head.getCentreY() - 4, 8, 8);
        g.setFont (font (10.0f, true).withExtraKerningFactor (0.12f));
        g.drawText (names[juce::jlimit (0, 4, state)], head.withTrimmedLeft (13), juce::Justification::centredLeft);
        g.setColour (col::dim);
        juce::String t = juce::String ((double) len / sr, 1) + " s";
        if (looper.undoReady.load()) t = "UNDO READY   " + t;
        g.drawText (t, head, juce::Justification::centredRight);
    }

private:
    Looper& looper;
};

class LooperPanel : public ModulePanel
{
public:
    LooperPanel (APVTS& s, Looper& l) : ModulePanel (s, "LOOPER", "records the whole rack", "loop_on", col::loop, 3), display (l), looper (l)
    {
        addAndMakeVisible (display);
        knob ("loop_level", "LEVEL");
        knob ("loop_fb", "FEEDBACK").setTooltip ("How much of the existing loop survives each overdub pass");
        toggle ("loop_rev", "REVERSE");
        toggle ("loop_half", "HALF");
        toggle ("loop_sync", "SNAP");
        switches[2]->button.setTooltip ("Snap the loop length to whole beats of the host tempo");

        auto setup = [this] (juce::TextButton& b, const juce::String& text, Looper::Command c, const juce::String& tip)
        {
            b.setButtonText (text);
            b.setTooltip (tip);
            b.onClick = [this, c] { looper.post (c); };
            addAndMakeVisible (b);
        };
        setup (tap, "REC", Looper::Tap, "Footswitch: record, then play, then overdub. Also a host parameter (Loop Rec/Play/Dub) for MIDI pedals.");
        setup (stop, "STOP", Looper::Stop, "Stop playback (press REC/PLAY to restart from the top)");
        setup (undo, "UNDO", Looper::Undo, "Undo the last overdub. Press again to redo.");
        setup (clear, "CLEAR", Looper::Clear, "Erase the loop");
    }

    void update()
    {
        static const char* next[] = { "REC", "PLAY", "DUB", "PLAY", "PLAY" };
        const int st = looper.uiState.load();
        tap.setButtonText (next[juce::jlimit (0, 4, st)]);
        const juce::Colour c = st == Looper::Recording ? juce::Colour (0xffff4a4a)
                             : st == Looper::Overdubbing ? juce::Colour (0xffffa53d) : col::loop;
        tap.setColour (juce::TextButton::textColourOffId, c);
        undo.setEnabled (looper.undoReady.load());
        display.repaint();
    }

    void resized() override
    {
        auto r = contentArea();
        display.setBounds (r.removeFromTop ((int) (r.getHeight() * 0.42f)));
        r.removeFromTop (6);
        auto buttons = r.removeFromTop (32);
        const int bw = (buttons.getWidth() - 3 * 6) / 5;
        tap.setBounds (buttons.removeFromLeft (bw * 2));
        for (auto* b : { &stop, &undo, &clear }) { buttons.removeFromLeft (6); b->setBounds (buttons.removeFromLeft (bw)); }
        r.removeFromTop (4);
        auto knobsArea = r.removeFromLeft (r.getWidth() / 2);
        layoutKnobGrid (knobsArea, 2);
        r.removeFromLeft (6);
        const int sh = juce::jmin (30, r.getHeight() / 3);
        r = r.withSizeKeepingCentre (r.getWidth(), sh * 3);
        for (auto& sw : switches) { sw->compact = true; sw->setBounds (r.removeFromTop (sh).reduced (0, 3)); }
    }

private:
    LoopDisplay display;
    Looper& looper;
    juce::TextButton tap, stop, undo, clear;
};
} // namespace rs::ui
