// UI for the Morph Generator (frequency-zone map) and the Looper.
#pragma once
#include "Widgets.h"
#include "Generator.h"
#include "Looper.h"
#include "Gremlins.h"
#include <tuple>

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
inline juce::Colour trackColour (int t)
{
    static const juce::Colour c[4] = { juce::Colour (0xffc8f03c), juce::Colour (0xffffa53d), juce::Colour (0xff3ee8ff), juce::Colour (0xffff5fa2) };
    return c[t & 3];
}

inline juce::Colour loopStateColour (int st, juce::Colour play)
{
    return st == LoopTrack::Recording ? juce::Colour (0xffff4a4a)
         : st == LoopTrack::Overdubbing ? juce::Colour (0xffffa53d)
         : st == LoopTrack::Playing ? play : col::dim;
}

class LoopDisplay : public juce::Component
{
public:
    LoopDisplay (LoopStation& s, int index) : station (s), track (s.track (index)), colour (trackColour (index)) {}

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::screen);
        g.fillRoundedRectangle (r, 6.0f);
        const int state = track.uiState.load();
        const int len = track.uiLength.load();
        const int pos = track.uiPos.load();
        const double sr = track.getSampleRate();
        const auto stateCol = loopStateColour (state, colour);
        const int master = station.uiMasterLen.load();

        auto wave = r.reduced (8.0f, 5.0f).withTrimmedTop (13.0f);
        if (len > 0)
        {
            const int shownLen = state == LoopTrack::Recording ? juce::jmax (len, master > 0 ? master : (int) (sr * 4.0)) : len;
            // master-loop grid
            if (master > 0 && shownLen > master)
            {
                g.setColour (juce::Colours::white.withAlpha (0.12f));
                for (int k = master; k < shownLen; k += master)
                    g.drawVerticalLine ((int) (wave.getX() + wave.getWidth() * (float) k / (float) shownLen), wave.getY(), wave.getBottom());
            }
            const int nb = juce::jmax (1, juce::jmin (len, state == LoopTrack::Recording ? pos : len) / LoopTrack::binSize);
            const float bw = wave.getWidth() * (float) LoopTrack::binSize / (float) shownLen;
            juce::Path p;
            for (int b = 0; b < nb; ++b)
            {
                const float v = juce::jmin (1.0f, std::sqrt (track.bin (b)) * 1.1f) * wave.getHeight() * 0.5f;
                p.addRectangle (wave.getX() + (float) b * bw, wave.getCentreY() - v, juce::jmax (1.0f, bw * 0.8f), juce::jmax (1.0f, 2.0f * v));
            }
            g.setColour (stateCol.withAlpha (state == LoopTrack::Stopped ? 0.35f : 0.8f));
            g.fillPath (p);
            const float x = wave.getX() + wave.getWidth() * (float) pos / (float) shownLen;
            g.setColour (juce::Colours::white);
            g.fillRect (x - 1.0f, wave.getY(), 2.0f, wave.getHeight());
        }
        else
        {
            g.setColour (col::dim.withAlpha (0.5f));
            g.setFont (font (11.5f));
            g.drawText (master > 0 ? "REC to add a layer: it locks to the master loop" : "REC, play a phrase, press again to loop it",
                        wave, juce::Justification::centred);
        }

        static const char* names[] = { "EMPTY", "RECORDING", "PLAYING", "OVERDUBBING", "STOPPED" };
        auto head = r.reduced (8.0f, 2.0f).removeFromTop (13.0f);
        g.setColour (stateCol);
        g.fillEllipse (head.getX(), head.getCentreY() - 4, 8, 8);
        g.setFont (font (10.0f, true).withExtraKerningFactor (0.12f));
        juce::String st = names[juce::jlimit (0, 4, state)];
        if (track.uiWaiting.load()) st += "  (finishing the bar...)";
        g.drawText (st, head.withTrimmedLeft (13), juce::Justification::centredLeft);
        g.setColour (col::dim);
        juce::String t = juce::String ((double) len / sr, 1) + " s";
        if (master > 0 && len > 0 && state != LoopTrack::Recording) t = "x" + juce::String (juce::jmax (1, (len + master / 2) / master)) + "   " + t;
        if (track.undoReady.load()) t = "UNDO READY   " + t;
        g.drawText (t, head, juce::Justification::centredRight);
    }

private:
    LoopStation& station;
    LoopTrack& track;
    juce::Colour colour;
};

/** One loop track: transport, switches, level/pan and its waveform. */
class TrackLane : public juce::Component
{
public:
    TrackLane (APVTS& s, LoopStation& st, int index)
        : station (st), track (st.track (index)), idx (index), colour (trackColour (index)), display (st, index)
    {
        const juce::String n (index + 1);
        addAndMakeVisible (display);
        auto setup = [this] (juce::TextButton& b, const juce::String& text, LoopTrack::Command c, const juce::String& tip)
        {
            b.setButtonText (text);
            b.setTooltip (tip);
            b.onClick = [this, c] { station.post (idx, c); };
            addAndMakeVisible (b);
        };
        setup (tap, "REC", LoopTrack::Tap, "Record, then play, then overdub. Host parameter: Track " + n + " Rec/Play/Dub (MIDI-learn it to a pedal).");
        setup (stop, "STOP", LoopTrack::Stop, "Stop this track (REC/PLAY restarts it in sync)");
        setup (undo, "UNDO", LoopTrack::Undo, "Undo the last overdub on this track; press again to redo");
        setup (clear, "CLR", LoopTrack::Clear, "Erase this track");
        for (auto [sw, id, label] : { std::tuple<std::unique_ptr<Switch>*, const char*, const char*> { &mute, "mute", "MUTE" },
                                      { &rev, "rev", "REV" }, { &half, "half", "HALF" } })
        {
            *sw = std::make_unique<Switch> (s, "loop" + n + "_" + id, label, colour);
            (*sw)->compact = true;
            addAndMakeVisible (**sw);
        }
        level = std::make_unique<Knob> (s, "loop" + n + "_level", "LEVEL", colour);
        pan = std::make_unique<Knob> (s, "loop" + n + "_pan", "PAN", colour, true);
        addAndMakeVisible (*level);
        addAndMakeVisible (*pan);
    }

    void update()
    {
        static const char* next[] = { "REC", "PLAY", "DUB", "PLAY", "PLAY" };
        const int st = track.uiState.load();
        tap.setButtonText (next[juce::jlimit (0, 4, st)]);
        tap.setColour (juce::TextButton::textColourOffId, st == LoopTrack::Recording ? juce::Colour (0xffff4a4a)
                                                          : st == LoopTrack::Overdubbing ? juce::Colour (0xffffa53d) : colour);
        undo.setEnabled (track.undoReady.load());
        display.repaint();
        const float pk = track.uiPeak.load();
        if (std::abs (pk - shownPeak) > 0.01f) { shownPeak = pk; repaint (0, 0, 40, getHeight()); }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff15130f));
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (colour.withAlpha (0.35f));
        g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
        auto badge = juce::Rectangle<float> (8, 8, 26, r.getHeight() - 16);
        g.setColour (colour.withAlpha (0.15f + 0.6f * juce::jmin (1.0f, shownPeak * 3.0f)));
        g.fillRoundedRectangle (badge, 5.0f);
        g.setColour (colour);
        g.drawRoundedRectangle (badge, 5.0f, 1.2f);
        g.setFont (font (18.0f, true));
        g.drawText (juce::String (idx + 1), badge, juce::Justification::centred);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6);
        r.removeFromLeft (36);
        auto controls = r.removeFromLeft (430);
        r.removeFromLeft (8);
        display.setBounds (r);

        auto col1 = controls.removeFromLeft (150);
        auto tapArea = col1.removeFromTop (col1.getHeight() / 2 + 6).reduced (0, 3);
        tap.setBounds (tapArea);
        const int bw = (col1.getWidth() - 8) / 3;
        stop.setBounds (col1.removeFromLeft (bw).reduced (0, 3));
        col1.removeFromLeft (4);
        undo.setBounds (col1.removeFromLeft (bw).reduced (0, 3));
        col1.removeFromLeft (4);
        clear.setBounds (col1.reduced (0, 3));

        controls.removeFromLeft (10);
        auto sw = controls.removeFromLeft (90);
        const int sh = sw.getHeight() / 3;
        mute->setBounds (sw.removeFromTop (sh).reduced (0, 3));
        rev->setBounds (sw.removeFromTop (sh).reduced (0, 3));
        half->setBounds (sw.reduced (0, 3));
        controls.removeFromLeft (8);
        const int kw = controls.getWidth() / 2;
        level->setBounds (controls.removeFromLeft (kw));
        pan->setBounds (controls);
    }

private:
    LoopStation& station;
    LoopTrack& track;
    int idx;
    juce::Colour colour;
    LoopDisplay display;
    juce::TextButton tap, stop, undo, clear;
    std::unique_ptr<Switch> mute, rev, half;
    std::unique_ptr<Knob> level, pan;
    float shownPeak = 0;
};

class LoopStationPanel : public ModulePanel
{
public:
    LoopStationPanel (APVTS& s, LoopStation& st)
        : ModulePanel (s, "LOOP STATION", "4 tracks, phase-locked to the first loop", "loop_on", col::loop, 4), station (st)
    {
        for (int t = 0; t < 4; ++t)
        {
            lanes[t] = std::make_unique<TrackLane> (s, st, t);
            addAndMakeVisible (*lanes[t]);
        }
        toggle ("loop_lock", "LOCK");
        toggle ("loop_snap", "SNAP TO BEAT");
        switches[0]->button.setTooltip ("Lock every track to whole multiples of the first loop, so layers never drift");
        switches[1]->button.setTooltip ("Round the first loop's length to whole beats of the host tempo");
        for (auto& sw : switches) sw->compact = true;
        knob ("loop_fb", "FEEDBACK").setTooltip ("How much of the old layers survives each overdub pass");
        stopAll.setButtonText ("STOP ALL");
        clearAll.setButtonText ("CLEAR ALL");
        stopAll.onClick = [this] { station.stopAll(); };
        clearAll.onClick = [this] { station.clearAll(); };
        addAndMakeVisible (stopAll);
        addAndMakeVisible (clearAll);
    }

    void update() { for (auto& l : lanes) l->update(); repaint (masterBar.toNearestInt().expanded (2)); }

    void resized() override
    {
        auto r = contentArea();
        auto bar = r.removeFromTop (40);
        switches[0]->setBounds (bar.removeFromLeft (80).reduced (0, 7));
        bar.removeFromLeft (8);
        switches[1]->setBounds (bar.removeFromLeft (120).reduced (0, 7));
        bar.removeFromLeft (8);
        clearAll.setBounds (bar.removeFromRight (96).reduced (0, 5));
        bar.removeFromRight (8);
        stopAll.setBounds (bar.removeFromRight (96).reduced (0, 5));
        bar.removeFromRight (8);
        extraCellBounds.clear();
        layoutKnobGrid (bar.removeFromLeft (160).withTrimmedTop (-6).withTrimmedBottom (-10), 1);
        bar.removeFromLeft (8);
        masterBar = bar.toFloat().reduced (0, 10);
        r.removeFromTop (6);
        const int h = (r.getHeight() - 3 * 6) / 4;
        for (auto& l : lanes) { l->setBounds (r.removeFromTop (h)); r.removeFromTop (6); }
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        const int m = station.uiMasterLen.load();
        auto b = masterBar;
        g.setColour (col::screen);
        g.fillRoundedRectangle (b, 4.0f);
        if (m > 0)
        {
            const float ph = (float) station.uiMasterPos.load() / (float) m;
            g.setColour (col::loop.withAlpha (0.35f));
            g.fillRoundedRectangle (b.withWidth (juce::jmax (4.0f, b.getWidth() * ph)), 4.0f);
            g.setColour (col::loop);
            g.fillRect (b.getX() + b.getWidth() * ph - 1.5f, b.getY(), 3.0f, b.getHeight());
            for (int k = 1; k < 4; ++k)
            {
                g.setColour (juce::Colours::black.withAlpha (0.5f));
                g.drawVerticalLine ((int) (b.getX() + b.getWidth() * (float) k / 4.0f), b.getY(), b.getBottom());
            }
        }
        g.setColour (m > 0 ? col::text : col::dim);
        g.setFont (font (10.5f, true));
        g.drawText (m > 0 ? "MASTER  " + juce::String ((double) m / station.track (0).getSampleRate(), 2) + " s" : "NO MASTER LOOP YET",
                    b.reduced (8, 0), juce::Justification::centredRight);
    }

private:
    LoopStation& station;
    std::unique_ptr<TrackLane> lanes[4];
    juce::TextButton stopAll, clearAll;
    juce::Rectangle<float> masterBar;
};

//==============================================================================
/** A little terrarium: every creature the gremlins spawn floats up with its name. */
class CritterView : public juce::Component
{
public:
    explicit CritterView (Gremlins& g) : gremlins (g) {}

    void update (bool enabled)
    {
        const int spawned = gremlins.meters.spawned.load();
        if (lastSpawned < 0) lastSpawned = spawned;
        for (int k = std::max (lastSpawned, spawned - Gremlins::historySize); k < spawned; ++k)
        {
            Critter c;
            c.type = gremlins.meters.history[(size_t) (k % Gremlins::historySize)].load();
            c.x = 0.08f + 0.84f * rnd.nextFloat();
            c.wobble = rnd.nextFloat() * 6.0f;
            critters.push_back (c);
        }
        lastSpawned = spawned;
        const int on = gremlins.meters.onsets.load();
        if (on != lastOnsets) { lastOnsets = on; heard = 1.0f; }
        heard *= 0.85f;
        for (auto& c : critters) c.age += 1.0f / 30.0f;
        critters.erase (std::remove_if (critters.begin(), critters.end(), [] (const Critter& c) { return c.age > 2.6f; }), critters.end());
        if (critters.size() > 24) critters.erase (critters.begin(), critters.begin() + (long) (critters.size() - 24));
        active = enabled;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        juce::ColourGradient bg (juce::Colour (0xff0b0910), r.getX(), r.getY(), juce::Colour (0xff120a12), r.getX(), r.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (juce::Colours::white.withAlpha (0.035f));
        for (float x = r.getX() + 12; x < r.getRight(); x += 24) g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        for (float y = r.getY() + 12; y < r.getBottom(); y += 24) g.drawHorizontalLine ((int) y, r.getX(), r.getRight());

        // "heard you" ear flash
        auto ear = juce::Rectangle<float> (r.getRight() - 96, r.getY() + 8, 88, 18);
        g.setColour (juce::Colour (0xffff4fd8).withAlpha (0.15f + 0.85f * heard));
        g.fillRoundedRectangle (ear, 9.0f);
        g.setColour (heard > 0.3f ? juce::Colours::black : col::dim);
        g.setFont (font (10.0f, true));
        g.drawText ("HEARD YOU", ear, juce::Justification::centred);

        if (critters.empty())
        {
            g.setColour (col::dim.withAlpha (0.6f));
            g.setFont (font (12.0f));
            g.drawText (active ? "make a sound: play, clap, talk, click the keys..." : "switch GREMLINS on, then make any sound",
                        r, juce::Justification::centred);
        }

        for (const auto& c : critters)
        {
            const bool harsh = c.type >= Gremlins::numSilly;
            const float a = juce::jlimit (0.0f, 1.0f, 2.6f - c.age) * juce::jmin (1.0f, c.age * 8.0f);
            const float x = r.getX() + r.getWidth() * c.x + 6.0f * std::sin (c.age * 5.0f + c.wobble);
            const float y = r.getBottom() - 26.0f - (r.getHeight() - 50.0f) * juce::jmin (1.0f, c.age / 2.2f);
            const float size = 22.0f + 6.0f * std::sin (c.age * 9.0f);
            const auto colr = harsh ? juce::Colour::fromHSV (0.0f + 0.03f * (float) (c.type % 3), 0.75f, 0.95f, 1.0f)
                                    : juce::Colour::fromHSV (0.12f + 0.12f * (float) c.type, 0.65f, 1.0f, 1.0f);
            auto body = juce::Rectangle<float> (size, size).withCentre ({ x, y });
            if (harsh)
            {
                juce::Path spiky;
                for (int k = 0; k < 14; ++k)
                {
                    const float ang = juce::MathConstants<float>::twoPi * (float) k / 14.0f + c.age * 2.0f;
                    const float rad = (k % 2 == 0 ? 0.62f : 0.38f) * size;
                    const juce::Point<float> pt (x + std::cos (ang) * rad, y + std::sin (ang) * rad);
                    if (k == 0) spiky.startNewSubPath (pt); else spiky.lineTo (pt);
                }
                spiky.closeSubPath();
                g.setColour (colr.withAlpha (0.85f * a));
                g.fillPath (spiky);
                g.setColour (juce::Colours::black.withAlpha (a));
                g.drawLine (x - 6, y - 4, x - 2, y - 1, 2.0f);     // angry eyebrows
                g.drawLine (x + 6, y - 4, x + 2, y - 1, 2.0f);
                g.fillEllipse (x - 5, y, 3, 3);
                g.fillEllipse (x + 2, y, 3, 3);
            }
            else
            {
                g.setColour (colr.withAlpha (0.25f * a));
                g.fillEllipse (body.expanded (5));
                g.setColour (colr.withAlpha (0.9f * a));
                g.fillEllipse (body);
                g.setColour (juce::Colours::white.withAlpha (a));
                g.fillEllipse (x - 7, y - 5, 6, 7);
                g.fillEllipse (x + 1, y - 5, 6, 7);
                g.setColour (juce::Colours::black.withAlpha (a));
                g.fillEllipse (x - 5, y - 2, 3, 3);
                g.fillEllipse (x + 3, y - 2, 3, 3);
            }
            g.setColour (colr.withAlpha (a));
            g.setFont (font (harsh ? 13.0f : 12.0f, true).withExtraKerningFactor (harsh ? 0.15f : 0.0f));
            g.drawText (Gremlins::typeName (c.type), juce::Rectangle<float> (x - 70, y + size * 0.5f + 1, 140, 15), juce::Justification::centred);
        }
    }

private:
    struct Critter { int type = 0; float x = 0.5f, age = 0, wobble = 0; };
    Gremlins& gremlins;
    std::vector<Critter> critters;
    int lastSpawned = -1, lastOnsets = 0;
    float heard = 0;
    bool active = false;
    juce::Random rnd;
};

class GremlinsPanel : public ModulePanel
{
public:
    GremlinsPanel (APVTS& s, Gremlins& g) : ModulePanel (s, "GREMLINS", "any sound you make spawns creatures", "grm_on", col::grm, 4), view (g), gremlins (g)
    {
        addAndMakeVisible (view);
        knob ("grm_sens", "SENS").setTooltip ("How easily a sound at the input wakes the gremlins");
        knob ("grm_chance", "CHANCE").setTooltip ("Probability that a detected sound spawns something");
        auto& h = knob ("grm_harsh", "SILLY<>HARSH", true);
        h.valueText = [this]
        {
            const int v = juce::roundToInt (state.getRawParameterValue ("grm_harsh")->load() * 100.0f);
            return "silly " + juce::String (100 - v) + " / harsh " + juce::String (v);
        };
        h.setTooltip ("Left: boing, pew, quack, bubbles, robot, whistle, kazoo. Right: clank, grinder, stutter, reverse ghost, screech, static.");
        knob ("grm_dirt", "DIRT").setTooltip ("Industrial clipper + bit crusher + sample-rate reducer on the whole gremlin bus");
        knob ("grm_follow", "FOLLOW").setTooltip ("How often creatures take their pitch from the note you are playing");
        knob ("grm_swarm", "SWARM").setTooltip ("How many creatures each sound wakes up");
        knob ("grm_level", "LEVEL");
        spawn.setButtonText ("SPAWN!");
        spawn.onClick = [this] { gremlins.manualSpawn.store (true); };
        addAndMakeVisible (spawn);
    }

    void update() { view.update (isOn()); }

    void resized() override
    {
        auto r = contentArea();
        auto left = r.removeFromLeft ((int) (r.getWidth() * 0.42f));
        r.removeFromLeft (12);
        view.setBounds (r);
        extraCellBounds.clear();
        layoutKnobGrid (left, 4, 1);
        if (! extraCellBounds.empty()) spawn.setBounds (extraCellBounds[0].withSizeKeepingCentre (juce::jmin (90, extraCellBounds[0].getWidth()), 32));
    }

    void refreshActive() override
    {
        ModulePanel::refreshActive();
        spawn.setAlpha (isOn() ? 1.0f : 0.42f);
    }

private:
    CritterView view;
    Gremlins& gremlins;
    juce::TextButton spawn;
};
} // namespace rs::ui
