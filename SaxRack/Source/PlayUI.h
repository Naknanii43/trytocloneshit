// The PLAY screen: the simple front of Reedstorm. Worlds, one Transform pad, a handful of big controls.
// Everything here drives the same engine as the EDIT pages; nothing is removed, only hidden.
#pragma once
#include "Widgets.h"
#include "Looper.h"
#include "Gremlins.h"
#include "PluginProcessor.h"

namespace rs::ui
{
struct World { const char* name; const char* preset; const char* vibe; juce::uint32 colour; };

inline const std::array<World, 8>& worlds()
{
    static const std::array<World, 8> w { {
        { "Clean",     "Studio Polish",       "polished, present, mix-ready",   0xffe2b45c },
        { "Jazz Club", "Smoky Jazz Club",     "dark, warm and close",           0xffc9875a },
        { "Funk",      "Funk Envelope Wah",   "quacky envelope wah",            0xffffd23f },
        { "Dream",     "Ambient Shimmer",     "endless shimmer cathedral",      0xff9cc2ff },
        { "Synth",     "Synth Horn Lead",     "your breath plays a synth",      0xffb36bff },
        { "Monster",   "Octave Fuzz Monster", "octave-down fuzz beast",         0xffff3d5e },
        { "Cartoon",   "Cartoon Sax",         "boings & whistles on your notes",0xff72d66b },
        { "Factory",   "Industrial Factory",  "clanks, grinders, crushed bits", 0xffdfe6ee },
    } };
    return w;
}

//==============================================================================
class WorldTile : public juce::Component
{
public:
    WorldTile (ReedstormProcessor& p, int idx) : proc (p), world (worlds()[(size_t) idx])
    {
        for (int i = 0; i < proc.getNumPrograms(); ++i)
            if (proc.getProgramName (i) == world.preset) presetIndex = i;
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTitle (juce::String ("World: ") + world.name);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mouseWasClicked() && presetIndex >= 0) proc.loadPreset (presetIndex);
    }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }

    void refresh()
    {
        const bool sel = proc.getCurrentPresetIndex() == presetIndex;
        if (sel != selected) { selected = sel; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        const auto c = juce::Colour (world.colour);
        auto r = getLocalBounds().toFloat().reduced (3.0f);
        juce::ColourGradient bg (c.withAlpha (selected ? 0.30f : 0.10f), r.getX(), r.getY(),
                                 col::panel2.withAlpha (0.95f), r.getRight(), r.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (selected ? c : (hover ? c.withAlpha (0.6f) : col::edge.brighter (0.2f)));
        g.drawRoundedRectangle (r, 10.0f, selected ? 2.0f : 1.0f);

        // a little emblem: concentric rings that fill in when chosen
        const auto centre = juce::Point<float> (r.getX() + 26.0f, r.getY() + 28.0f);
        for (int k = 3; k >= 1; --k)
        {
            const float rad = 5.0f * (float) k;
            g.setColour (c.withAlpha (selected ? 0.25f * (float) (4 - k) : 0.12f * (float) (4 - k)));
            g.fillEllipse (centre.x - rad, centre.y - rad, rad * 2, rad * 2);
        }
        g.setColour (c);
        g.fillEllipse (centre.x - 4, centre.y - 4, 8, 8);

        g.setColour (selected ? col::text : col::text.withAlpha (0.85f));
        g.setFont (font (19.0f, true));
        g.drawText (world.name, r.withTrimmedLeft (14).withTrimmedTop (r.getHeight() * 0.42f).withHeight (24), juce::Justification::centredLeft);
        g.setColour (col::dim);
        g.setFont (font (11.5f));
        g.drawFittedText (world.vibe, r.withTrimmedLeft (14).withTrimmedRight (8).withTrimmedTop (r.getHeight() * 0.42f + 24).toNearestInt(),
                          juce::Justification::topLeft, 2);
    }

private:
    ReedstormProcessor& proc;
    const World& world;
    int presetIndex = -1;
    bool hover = false, selected = false;
};

//==============================================================================
/** X: sax -> synth (Morph Gen). Y: clean -> chaos (Gremlins, fuzz, glitches). */
class TransformPad : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit TransformPad (ReedstormProcessor& p) : proc (p)
    {
        setTitle ("Transform pad");
        setTooltip ("Drag: left to right turns your sax into a synth, bottom to top goes from clean to chaos. Double-click to return to the pure World.");
    }

    void update (float level, bool voiced, int spawns)
    {
        glow = glow * 0.85f + juce::jlimit (0.0f, 1.0f, level * 3.0f) * 0.15f;
        if (spawns != lastSpawns) { lastSpawns = spawns; burst = 1.0f; }
        burst *= 0.88f;
        live = voiced;
        phase += 0.03f;
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (auto* id : { "perf_morph", "perf_chaos" }) if (auto* prm = proc.apvts.getParameter (id)) prm->beginChangeGesture();
        setFrom (e.position);
    }
    void mouseDrag (const juce::MouseEvent& e) override { setFrom (e.position); }
    void mouseUp (const juce::MouseEvent&) override
    {
        for (auto* id : { "perf_morph", "perf_chaos" }) if (auto* prm = proc.apvts.getParameter (id)) prm->endChangeGesture();
    }
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        for (auto* id : { "perf_morph", "perf_chaos" })
            if (auto* prm = proc.apvts.getParameter (id)) { prm->beginChangeGesture(); prm->setValueNotifyingHost (0.0f); prm->endChangeGesture(); }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto pad = r.reduced (2.0f);
        const float x = proc.apvts.getRawParameterValue ("perf_morph")->load();
        const float y = proc.apvts.getRawParameterValue ("perf_chaos")->load();

        // ground: four corners in four colours (sax brass, synth cyan, gremlin magenta, mutant red)
        g.setColour (col::screen);
        g.fillRoundedRectangle (pad, 14.0f);
        g.saveState();
        juce::Path clip; clip.addRoundedRectangle (pad, 14.0f);
        g.reduceClipRegion (clip);
        auto corner = [&] (juce::Point<float> at, juce::Colour c, float a)
        {
            juce::ColourGradient cg (c.withAlpha (a), at.x, at.y, c.withAlpha (0.0f), at.x + pad.getWidth() * 0.75f * (at.x < pad.getCentreX() ? 1.0f : -1.0f), at.y, true);
            g.setGradientFill (cg);
            g.fillRect (pad);
        };
        corner (pad.getBottomLeft(), col::brass, 0.28f);
        corner (pad.getBottomRight(), col::gen, 0.26f);
        corner (pad.getTopLeft(), col::grm, 0.26f);
        corner (pad.getTopRight(), col::drv, 0.30f);

        // grid
        g.setColour (juce::Colours::white.withAlpha (0.045f));
        for (int k = 1; k < 8; ++k)
        {
            g.drawVerticalLine ((int) (pad.getX() + pad.getWidth() * (float) k / 8.0f), pad.getY(), pad.getBottom());
            g.drawHorizontalLine ((int) (pad.getY() + pad.getHeight() * (float) k / 8.0f), pad.getX(), pad.getRight());
        }
        g.setColour (juce::Colours::white.withAlpha (0.09f));
        g.drawVerticalLine ((int) pad.getCentreX(), pad.getY(), pad.getBottom());
        g.drawHorizontalLine ((int) pad.getCentreY(), pad.getX(), pad.getRight());

        // corner names
        auto label = [&] (const juce::String& t, const juce::String& sub, juce::Rectangle<float> area, juce::Justification j, juce::Colour c)
        {
            g.setColour (c.withAlpha (0.95f));
            g.setFont (font (15.0f, true).withExtraKerningFactor (0.12f));
            g.drawText (t, area.removeFromTop (20), j);
            g.setColour (col::dim);
            g.setFont (font (11.0f));
            g.drawText (sub, area.removeFromTop (16), j);
        };
        auto in = pad.reduced (18.0f, 14.0f);
        label ("PURE SAX", "your horn, the World as it is", in.withTop (in.getBottom() - 38), juce::Justification::bottomLeft, col::brass);
        label ("SYNTH", "your notes play the synth", in.withTop (in.getBottom() - 38), juce::Justification::bottomRight, col::gen);
        label ("GREMLINS", "every sound wakes creatures", in.withHeight (38), juce::Justification::topLeft, col::grm);
        label ("MUTANT", "synth + chaos + glitches", in.withHeight (38), juce::Justification::topRight, col::drv);

        // the puck
        const auto pos = juce::Point<float> (pad.getX() + 24.0f + x * (pad.getWidth() - 48.0f), pad.getBottom() - 24.0f - y * (pad.getHeight() - 48.0f));
        const auto puckCol = col::brass.interpolatedWith (col::gen, x).interpolatedWith (col::grm, y * 0.7f);
        const float pulse = 22.0f + 26.0f * glow + 4.0f * std::sin (phase * 3.0f);
        juce::ColourGradient halo (puckCol.withAlpha (0.55f), pos.x, pos.y, puckCol.withAlpha (0.0f), pos.x + pulse * 2.2f, pos.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (pos.x - pulse * 2.2f, pos.y - pulse * 2.2f, pulse * 4.4f, pulse * 4.4f);
        if (burst > 0.05f)
        {
            g.setColour (col::grm.withAlpha (burst * 0.8f));
            for (int k = 0; k < 8; ++k)
            {
                const float a = juce::MathConstants<float>::twoPi * (float) k / 8.0f + phase;
                const float r0 = 20.0f + (1.0f - burst) * 50.0f;
                g.drawLine (pos.x + std::cos (a) * r0, pos.y + std::sin (a) * r0, pos.x + std::cos (a) * (r0 + 10), pos.y + std::sin (a) * (r0 + 10), 2.0f);
            }
        }
        g.setColour (puckCol);
        g.fillEllipse (pos.x - 13, pos.y - 13, 26, 26);
        g.setColour (juce::Colours::white.withAlpha (live ? 0.95f : 0.6f));
        g.drawEllipse (pos.x - 13, pos.y - 13, 26, 26, 2.0f);
        g.fillEllipse (pos.x - 4, pos.y - 4, 8, 8);
        g.restoreState();

        g.setColour (col::edge.brighter (0.25f));
        g.drawRoundedRectangle (pad, 14.0f, 1.2f);

        // axis captions
        g.setColour (col::dim);
        g.setFont (font (10.5f, true).withExtraKerningFactor (0.2f));
        g.drawText ("SAX  >  SYNTH  " + juce::String (juce::roundToInt (x * 100)) + "%", pad.withTop (pad.getBottom() - 16).translated (0, -2), juce::Justification::centred);
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, pad.getX() + 10, pad.getCentreY()));
        g.drawText ("CLEAN  >  CHAOS  " + juce::String (juce::roundToInt (y * 100)) + "%",
                    juce::Rectangle<float> (pad.getX() - 140 + 10, pad.getCentreY() - 8, 280, 16), juce::Justification::centred);
    }

private:
    void setFrom (juce::Point<float> p)
    {
        auto pad = getLocalBounds().toFloat().reduced (26.0f);
        const float nx = juce::jlimit (0.0f, 1.0f, (p.x - pad.getX()) / pad.getWidth());
        const float ny = juce::jlimit (0.0f, 1.0f, 1.0f - (p.y - pad.getY()) / pad.getHeight());
        if (auto* prm = proc.apvts.getParameter ("perf_morph")) prm->setValueNotifyingHost (nx);
        if (auto* prm = proc.apvts.getParameter ("perf_chaos")) prm->setValueNotifyingHost (ny);
        repaint();
    }

    ReedstormProcessor& proc;
    float glow = 0, burst = 0, phase = 0;
    int lastSpawns = 0;
    bool live = false;
};

//==============================================================================
class PlayPage : public juce::Component
{
public:
    explicit PlayPage (ReedstormProcessor& p) : proc (p), pad (p)
    {
        for (int i = 0; i < 8; ++i)
        {
            tiles[(size_t) i] = std::make_unique<WorldTile> (p, i);
            addAndMakeVisible (*tiles[(size_t) i]);
        }
        addAndMakeVisible (pad);

        space = std::make_unique<Knob> (p.apvts, "perf_space", "SPACE", col::rev, true);
        tone = std::make_unique<Knob> (p.apvts, "perf_tone", "TONE", col::wah, true);
        space->maxSize = 104; tone->maxSize = 104;
        space->setTooltip ("Left: drier than the World. Right: room, then echoes, then an endless shimmer");
        tone->setTooltip ("Left: darker and rounder. Right: brighter and more cutting");
        addAndMakeVisible (*space);
        addAndMakeVisible (*tone);

        for (int k = 0; k < 6; ++k)
        {
            auto& b = harm[(size_t) k];
            b.setButtonText (params::perfHarmonies[k]);
            b.setClickingTogglesState (false);
            b.onClick = [this, k]
            {
                if (auto* prm = proc.apvts.getParameter ("perf_harm"))
                {
                    prm->beginChangeGesture();
                    prm->setValueNotifyingHost (prm->convertTo0to1 ((float) k));
                    prm->endChangeGesture();
                }
            };
            addAndMakeVisible (b);
        }

        freeze.setButtonText ("FREEZE");
        freeze.setClickingTogglesState (true);
        freeze.setTooltip ("Hold a note and press: it becomes an endless pad. Press again to let go.");
        addAndMakeVisible (freeze);
        freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "frz_hold", freeze);

        loop.setButtonText ("REC");
        loop.setTooltip ("One button looper (track 1 of the Loop Station): record, then play, then layer on top");
        loop.onClick = [this] { proc.looper().post (0, LoopTrack::Tap); };
        clear.setButtonText ("CLEAR");
        clear.onClick = [this] { proc.looper().clearAll(); };
        undo.setButtonText ("UNDO");
        undo.onClick = [this] { proc.looper().post (0, LoopTrack::Undo); };
        for (auto* b : { &loop, &clear, &undo }) addAndMakeVisible (b);
    }

    void update()
    {
        for (auto& t : tiles) t->refresh();
        auto& m = proc.meters();
        pad.update (std::max (m.outPeakL.load(), m.outPeakR.load()), m.voiced.load(), proc.gremlins().meters.spawned.load());

        const int h = (int) std::lround (proc.apvts.getRawParameterValue ("perf_harm")->load());
        for (int k = 0; k < 6; ++k) harm[(size_t) k].setToggleState (k == h, juce::dontSendNotification);

        auto& tr = proc.looper().track (0);
        const int st = tr.uiState.load();
        static const char* next[] = { "REC", "PLAY", "LAYER", "PLAY", "PLAY" };
        loop.setButtonText (next[juce::jlimit (0, 4, st)]);
        loop.setColour (juce::TextButton::textColourOffId, st == LoopTrack::Recording ? juce::Colour (0xffff4a4a)
                                                          : st == LoopTrack::Overdubbing ? juce::Colour (0xffffa53d) : col::loop);
        undo.setEnabled (tr.undoReady.load());
        freeze.setButtonText (freeze.getToggleState() ? "FROZEN" : "FREEZE");
        loopState = st;
        loopLen = tr.uiLength.load();
        loopPos = tr.uiPos.load();
        loopSr = tr.getSampleRate();
        repaint (loopArea.expanded (4));
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto left = r.removeFromLeft (300);
        r.removeFromLeft (16);
        auto right = r.removeFromRight (318);
        r.removeFromRight (16);

        left.removeFromTop (26);
        const int th = left.getHeight() / 4, tw = left.getWidth() / 2;
        for (int i = 0; i < 8; ++i)
            tiles[(size_t) i]->setBounds (left.getX() + (i % 2) * tw, left.getY() + (i / 2) * th, tw, th);

        r.removeFromTop (26);
        pad.setBounds (r);

        right.removeFromTop (26);
        auto knobs = right.removeFromTop (150);
        space->setBounds (knobs.removeFromLeft (knobs.getWidth() / 2));
        tone->setBounds (knobs);
        right.removeFromTop (30);
        auto hrow = right.removeFromTop (76);
        const int hw = (hrow.getWidth() - 12) / 3;
        for (int k = 0; k < 6; ++k)
            harm[(size_t) k].setBounds (hrow.getX() + (k % 3) * (hw + 6), hrow.getY() + (k / 3) * 40, hw, 34);
        right.removeFromTop (18);
        freeze.setBounds (right.removeFromTop (92));
        right.removeFromTop (34);
        loopArea = right.removeFromBottom (34);
        auto lrow = right;
        loop.setBounds (lrow.removeFromLeft ((int) (lrow.getWidth() * 0.55f)).withTrimmedBottom (8));
        lrow.removeFromLeft (8);
        undo.setBounds (lrow.removeFromTop (lrow.getHeight() / 2 - 4));
        lrow.removeFromTop (8);
        clear.setBounds (lrow.withTrimmedBottom (8));
    }

    void paint (juce::Graphics& g) override
    {
        auto caption = [&] (juce::Rectangle<int> area, const juce::String& t)
        {
            g.setColour (col::brass.withAlpha (0.85f));
            g.setFont (font (11.0f, true).withExtraKerningFactor (0.25f));
            g.drawText (t, area, juce::Justification::centredLeft);
        };
        caption ({ 0, 0, 300, 22 }, "1   CHOOSE A WORLD");
        caption ({ pad.getX(), 0, 400, 22 }, "2   TRANSFORM IT AS YOU PLAY");
        caption ({ getWidth() - 318, 0, 318, 22 }, "3   SHAPE IT");
        caption ({ getWidth() - 318, harm[0].getY() - 24, 318, 20 }, "HARMONY");
        caption ({ getWidth() - 318, loop.getY() - 24, 318, 20 }, "LOOP");

        // loop progress strip
        auto a = loopArea.toFloat();
        g.setColour (col::screen);
        g.fillRoundedRectangle (a, 6.0f);
        if (loopLen > 0 && loopState != LoopTrack::Empty)
        {
            const float f = loopState == LoopTrack::Recording ? 1.0f : juce::jlimit (0.0f, 1.0f, (float) loopPos / (float) loopLen);
            const auto c = loopState == LoopTrack::Recording ? juce::Colour (0xffff4a4a)
                         : loopState == LoopTrack::Overdubbing ? juce::Colour (0xffffa53d) : col::loop;
            g.setColour (c.withAlpha (0.35f));
            g.fillRoundedRectangle (a.withWidth (a.getWidth() * f), 6.0f);
        }
        g.setColour (col::dim);
        g.setFont (font (11.5f, true));
        static const char* names[] = { "empty: press REC and play", "recording...", "playing", "layering on top", "stopped" };
        g.drawText (juce::String (names[juce::jlimit (0, 4, loopState)]) + (loopLen > 0 ? "   " + juce::String ((double) loopLen / loopSr, 1) + " s" : ""),
                    a.reduced (10, 0), juce::Justification::centredLeft);
    }

private:
    ReedstormProcessor& proc;
    std::array<std::unique_ptr<WorldTile>, 8> tiles;
    TransformPad pad;
    std::unique_ptr<Knob> space, tone;
    std::array<juce::TextButton, 6> harm;
    juce::TextButton freeze, loop, clear, undo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;
    juce::Rectangle<int> loopArea;
    int loopState = 0, loopLen = 0, loopPos = 0;
    double loopSr = 48000.0;
};
} // namespace rs::ui
