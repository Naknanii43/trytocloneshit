// Reedstorm UI widgets.
#pragma once
#include "LookAndFeel.h"
#include "Params.h"
#include "DSP.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <complex>

namespace rs::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

//==============================================================================
/** Rotary knob with a caption; shows the value while hovered or dragged. */
class Knob : public juce::Component
{
public:
    Knob (APVTS& state, const juce::String& paramId, const juce::String& caption, juce::Colour accent, bool bipolar = false)
        : name (caption), param (state.getParameter (paramId))
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        slider.getProperties().set ("accent", (juce::int64) accent.getARGB());
        slider.getProperties().set ("bipolar", bipolar);
        slider.setMouseDragSensitivity (220);
        slider.setVelocityBasedMode (false);
        slider.onValueChange = [this] { repaint(); };
        slider.addMouseListener (this, false);
        addAndMakeVisible (slider);
        attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
        if (param != nullptr)
            slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        accentColour = accent;
        if (param != nullptr) setTooltip (param->getName (64) + ". Double-click to reset.");
    }

    /** Optional custom value text (e.g. harmony intervals that depend on the mode). */
    std::function<juce::String()> valueText;

    void setTooltip (const juce::String& t) { slider.setTooltip (t); }

    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }

    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromBottom (16);
        const int s = juce::jmin (r.getWidth(), r.getHeight(), 70);
        slider.setBounds (r.withSizeKeepingCentre (s, s));
    }

    void paint (juce::Graphics&) override {}

    void paintOverChildren (juce::Graphics& g) override
    {
        auto r = getLocalBounds().removeFromBottom (16);
        const bool showValue = hover || slider.isMouseButtonDown();
        juce::String t = name;
        if (showValue && param != nullptr) t = valueText ? valueText() : param->getCurrentValueAsText();
        g.setFont (font (11.5f, true));
        g.setColour (showValue ? accentColour.brighter (0.3f) : col::dim.brighter (0.25f));
        g.drawText (t, r, juce::Justification::centred, true);
    }

    juce::Slider slider;

private:
    juce::String name;
    juce::RangedAudioParameter* param = nullptr;
    juce::Colour accentColour;
    bool hover = false;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

//==============================================================================
class Combo : public juce::Component
{
public:
    Combo (APVTS& state, const juce::String& paramId, const juce::String& caption, juce::Colour accent)
        : name (caption)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId)))
            box.addItemList (p->choices, 1);
        box.getProperties().set ("accent", (juce::int64) accent.getARGB());
        addAndMakeVisible (box);
        attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, paramId, box);
    }
    void resized() override { box.setBounds (getLocalBounds().withTrimmedTop (15)); }
    void paint (juce::Graphics& g) override
    {
        g.setColour (col::dim);
        g.setFont (font (10.5f, true));
        g.drawText (name.toUpperCase(), getLocalBounds().removeFromTop (14), juce::Justification::centredLeft);
    }
    juce::ComboBox box;

private:
    juce::String name;
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

//==============================================================================
class Switch : public juce::Component
{
public:
    Switch (APVTS& state, const juce::String& paramId, const juce::String& caption, juce::Colour accent)
    {
        button.setButtonText (caption);
        button.getProperties().set ("accent", (juce::int64) accent.getARGB());
        addAndMakeVisible (button);
        attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramId, button);
    }
    void resized() override { button.setBounds (compact ? getLocalBounds() : getLocalBounds().withTrimmedTop (15)); }
    juce::ToggleButton button;
    bool compact = false;

private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

//==============================================================================
/** One rack unit: title bar with power LED, a row of selectors, a grid of knobs. */
class ModulePanel : public juce::Component
{
public:
    ModulePanel (APVTS& s, const juce::String& title, const juce::String& subtitle, const juce::String& powerId,
                 juce::Colour accent, int columns)
        : state (s), titleText (title), subText (subtitle), accentColour (accent), cols (columns)
    {
        power.getProperties().set ("accent", (juce::int64) accent.getARGB());
        power.setTooltip ("Switch " + title + " on / off");
        addAndMakeVisible (power);
        powerAttachment = std::make_unique<APVTS::ButtonAttachment> (state, powerId, power);
        power.onStateChange = [this] { refreshActive(); };
    }

    Knob& knob (const juce::String& id, const juce::String& caption, bool bipolar = false)
    {
        knobs.push_back (std::make_unique<Knob> (state, id, caption, accentColour, bipolar));
        addAndMakeVisible (*knobs.back());
        return *knobs.back();
    }
    Combo& combo (const juce::String& id, const juce::String& caption)
    {
        selectors.push_back (std::make_unique<Combo> (state, id, caption, accentColour));
        addAndMakeVisible (*selectors.back());
        return *selectors.back();
    }
    Switch& toggle (const juce::String& id, const juce::String& caption)
    {
        switches.push_back (std::make_unique<Switch> (state, id, caption, accentColour));
        addAndMakeVisible (*switches.back());
        return *switches.back();
    }
    /** Reserve the selector row for a custom display (e.g. the EQ curve). */
    void setTopDisplay (juce::Component* c) { topDisplay = c; if (c) addAndMakeVisible (c); }
    /** Leave a free knob cell (for a meter, etc.) and return its bounds after layout. */
    juce::Rectangle<int> spareCell;

    juce::Colour accent() const { return accentColour; }
    bool isOn() const { return power.getToggleState(); }

    virtual void refreshActive()
    {
        const float a = isOn() ? 1.0f : 0.42f;
        for (auto& k : knobs) k->setAlpha (a);
        for (auto& c : selectors) c->setAlpha (a);
        for (auto& s : switches) s->setAlpha (a);
        if (topDisplay) topDisplay->setAlpha (a);
        repaint();
    }

    /** Thin activity bar under the title (0..1), e.g. wah position; negative hides it. */
    void setActivity (float v)
    {
        if (std::abs (v - activity) > 0.004f) { activity = v; repaint (0, 0, getWidth(), 34); }
    }

    /** Positions the power LED and returns the area below the title bar. */
    juce::Rectangle<int> contentArea()
    {
        auto r = getLocalBounds().reduced (10, 6);
        auto head = r.removeFromTop (28);
        power.setBounds (head.removeFromRight (26).withSizeKeepingCentre (24, 24));
        r.removeFromTop (6);
        return r;
    }

    void layoutKnobGrid (juce::Rectangle<int> r, int columns, int extraCells = 0)
    {
        const int total = (int) knobs.size() + extraCells;
        const int rows = juce::jmax (1, (total + columns - 1) / columns);
        const int cw = r.getWidth() / columns, ch = r.getHeight() / rows;
        for (int i = 0; i < total; ++i)
        {
            const auto cell = juce::Rectangle<int> (r.getX() + (i % columns) * cw, r.getY() + (i / columns) * ch, cw, ch);
            if (i < (int) knobs.size()) knobs[(size_t) i]->setBounds (cell.reduced (2));
            else extraCellBounds.push_back (cell.reduced (4));
        }
    }
    std::vector<juce::Rectangle<int>> extraCellBounds;

    void resized() override
    {
        auto r = contentArea();

        if (! selectors.empty() || ! switches.empty() || topDisplay != nullptr)
        {
            auto row = r.removeFromTop (42);
            if (topDisplay != nullptr) topDisplay->setBounds (row.withTrimmedTop (2));
            else
            {
                const int n = (int) (selectors.size() + switches.size());
                const int w = (row.getWidth() - (n - 1) * 8) / n;
                for (auto& s : selectors) { s->setBounds (row.removeFromLeft (w)); row.removeFromLeft (8); }
                for (auto& s : switches) { s->setBounds (row.removeFromLeft (w)); row.removeFromLeft (8); }
            }
            r.removeFromTop (6);
        }

        const int total = (int) knobs.size() + (wantsSpareCell ? 1 : 0);
        const int rows = juce::jmax (1, (total + cols - 1) / cols);
        const int cw = r.getWidth() / cols, ch = r.getHeight() / rows;
        for (int i = 0; i < total; ++i)
        {
            const auto cell = juce::Rectangle<int> (r.getX() + (i % cols) * cw, r.getY() + (i / cols) * ch, cw, ch);
            if (i < (int) knobs.size()) knobs[(size_t) i]->setBounds (cell.reduced (2));
            else spareCell = cell.reduced (8);
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const bool on = isOn();

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (r.translated (0, 3), 9.0f);
        juce::ColourGradient grad (col::panel, r.getX(), r.getY(), col::panel2, r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, 9.0f);
        if (on)
        {
            juce::ColourGradient glow (accentColour.withAlpha (0.10f), r.getX(), r.getY(), accentColour.withAlpha (0.0f), r.getX(), r.getY() + 90, false);
            g.setGradientFill (glow);
            g.fillRoundedRectangle (r, 9.0f);
        }
        g.setColour (on ? accentColour.withAlpha (0.55f) : col::edge);
        g.drawRoundedRectangle (r, 9.0f, 1.2f);

        // rack screws
        for (auto pt : { r.getTopLeft().translated (9, 9), r.getBottomLeft().translated (9, -9), r.getBottomRight().translated (-9, -9) })
        {
            g.setColour (juce::Colour (0xff2c2823));
            g.fillEllipse (pt.x - 3, pt.y - 3, 6, 6);
            g.setColour (juce::Colours::black.withAlpha (0.7f));
            g.drawLine (pt.x - 2, pt.y - 2, pt.x + 2, pt.y + 2, 1.0f);
        }

        auto head = getLocalBounds().reduced (10, 6).removeFromTop (28).toFloat();
        head.removeFromLeft (8);
        g.setColour (on ? accentColour : col::dim);
        g.fillRoundedRectangle (head.getX(), head.getCentreY() - 8, 3.0f, 16.0f, 1.5f);
        g.setFont (font (15.0f, true).withExtraKerningFactor (0.12f));
        g.setColour (on ? col::text : col::dim);
        auto titleArea = head.withTrimmedLeft (10);
        g.drawText (titleText, titleArea, juce::Justification::centredLeft);
        const float tw = juce::GlyphArrangement::getStringWidth (font (15.0f, true).withExtraKerningFactor (0.12f), titleText);
        auto subArea = titleArea.withTrimmedLeft (tw + 10).withTrimmedRight (30);
        if (juce::GlyphArrangement::getStringWidth (font (10.5f), subText) < subArea.getWidth())
        {
            g.setFont (font (10.5f));
            g.setColour (col::dim.withAlpha (on ? 0.9f : 0.6f));
            g.drawText (subText, subArea, juce::Justification::centredLeft, false);
        }

        if (activity >= 0.0f && on)
        {
            auto bar = juce::Rectangle<float> (head.getX() + 10, head.getBottom() + 1, head.getWidth() - 46, 3);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillRoundedRectangle (bar, 1.5f);
            g.setColour (accentColour);
            g.fillRoundedRectangle (bar.withWidth (juce::jmax (3.0f, bar.getWidth() * activity)), 1.5f);
        }
    }

    bool wantsSpareCell = false;

protected:
    APVTS& state;
    juce::String titleText, subText;
    juce::Colour accentColour;
    int cols;
    juce::ToggleButton power;
    std::unique_ptr<APVTS::ButtonAttachment> powerAttachment;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Combo>> selectors;
    std::vector<std::unique_ptr<Switch>> switches;
    juce::Component* topDisplay = nullptr;
    float activity = -1.0f;
};

//==============================================================================
/** Segmented vertical level meter with peak hold. */
class Meter : public juce::Component
{
public:
    explicit Meter (juce::String l, bool reduction = false) : label (std::move (l)), isGr (reduction) {}

    void push (float linearOrDb)
    {
        const float v = isGr ? juce::jlimit (0.0f, 1.0f, linearOrDb / 20.0f)
                              : juce::jlimit (0.0f, 1.0f, (gainToDb (linearOrDb) + 60.0f) / 60.0f);
        level = v > level ? v : level * 0.86f + v * 0.14f;
        if (v >= peak) { peak = v; hold = 40; }
        else if (--hold < 0) peak = juce::jmax (0.0f, peak - 0.015f);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto lab = r.removeFromBottom (14);
        g.setColour (col::dim);
        g.setFont (font (10.0f, true));
        g.drawText (label, lab, juce::Justification::centred);

        g.setColour (col::screen);
        g.fillRoundedRectangle (r, 3.0f);
        auto inner = r.reduced (3.0f);
        const int segs = 26;
        const float sh = inner.getHeight() / (float) segs;
        for (int i = 0; i < segs; ++i)
        {
            const float t = (float) (i + 1) / (float) segs;
            auto seg = juce::Rectangle<float> (inner.getX(), isGr ? inner.getY() + (float) i * sh : inner.getBottom() - (float) (i + 1) * sh,
                                               inner.getWidth(), sh - 1.5f);
            juce::Colour c = isGr ? col::brass
                                  : (t > 0.93f ? juce::Colour (0xffff4040) : t > 0.8f ? juce::Colour (0xffffc23d) : juce::Colour (0xff59d98e));
            const bool lit = t <= level + 0.001f;
            const bool pk = std::abs (t - peak) < 1.0f / (float) segs && peak > 0.02f;
            g.setColour (lit ? c : pk ? c.withAlpha (0.8f) : c.withAlpha (0.07f));
            g.fillRect (seg);
        }
    }

private:
    juce::String label;
    bool isGr;
    float level = 0, peak = 0;
    int hold = 0;
};

//==============================================================================
inline juce::String dot() { return juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  ")); }

inline juce::String noteName (int midi)
{
    return params::keys[((midi % 12) + 12) % 12] + juce::String (midi / 12 - 1);
}

/** Chromatic tuner showing the written note for the chosen saxophone. */
class TunerDisplay : public juce::Component
{
public:
    void set (bool v, float concertMidi, int instrument, float ref)
    {
        voiced = v;
        if (v)
        {
            concert = concertMidi;
            written = concertMidi + (float) params::writtenOffset (instrument);
            smoothedCents += 0.35f * (centsOf (written) - smoothedCents);
            lastSeen = 0;
        }
        else ++lastSeen;
        refHz = ref;
        inst = instrument;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::screen);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (col::edge);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

        const bool show = voiced || lastSeen < 25;
        const float alpha = voiced ? 1.0f : juce::jmax (0.0f, 1.0f - (float) lastSeen / 25.0f) * 0.6f;
        const float cents = smoothedCents;
        const bool inTune = std::abs (cents) < 5.0f;
        const auto tuneCol = inTune ? juce::Colour (0xff59d98e) : std::abs (cents) < 15.0f ? juce::Colour (0xffffc23d) : juce::Colour (0xffff5a4a);

        // cents arc
        auto arcArea = r.reduced (14.0f, 10.0f);
        const auto c = juce::Point<float> (arcArea.getCentreX(), arcArea.getBottom() - 10.0f);
        const float rad = juce::jmin (arcArea.getWidth() * 0.5f, arcArea.getHeight() - 14.0f);
        const float span = 1.1f;
        for (int i = -10; i <= 10; ++i)
        {
            const float a = span * (float) i / 10.0f;
            const float len = i % 5 == 0 ? 10.0f : 5.0f;
            const juce::Point<float> p1 (c.x + std::sin (a) * rad, c.y - std::cos (a) * rad);
            const juce::Point<float> p2 (c.x + std::sin (a) * (rad - len), c.y - std::cos (a) * (rad - len));
            g.setColour (i == 0 ? juce::Colour (0xff59d98e) : col::dim.withAlpha (0.5f));
            g.drawLine ({ p1, p2 }, i == 0 ? 2.0f : 1.0f);
        }
        g.setFont (font (9.5f));
        g.setColour (col::dim);
        g.drawText ("-50", juce::Rectangle<float> (c.x - rad - 6, c.y - 26, 30, 12), juce::Justification::centredLeft);
        g.drawText ("+50", juce::Rectangle<float> (c.x + rad - 24, c.y - 26, 30, 12), juce::Justification::centredRight);

        if (show)
        {
            const float a = span * juce::jlimit (-50.0f, 50.0f, cents) / 50.0f;
            const juce::Point<float> tip (c.x + std::sin (a) * (rad - 2), c.y - std::cos (a) * (rad - 2));
            g.setColour (tuneCol.withAlpha (0.3f * alpha));
            g.drawLine ({ c, tip }, 6.0f);
            g.setColour (tuneCol.withAlpha (alpha));
            g.drawLine ({ c, tip }, 2.0f);
        }
        g.setColour (col::brassDark);
        g.fillEllipse (c.x - 4, c.y - 4, 8, 8);

        // note name
        const int note = (int) std::lround (written);
        const juce::String name = show ? params::keys[((note % 12) + 12) % 12] : juce::String ("--");
        auto nameArea = juce::Rectangle<float> (c.x - 60, c.y - rad * 0.78f, 120, 44);
        if (show && inTune && voiced)
        {
            g.setColour (tuneCol.withAlpha (0.12f));
            g.fillEllipse (nameArea.withSizeKeepingCentre (76, 52));
        }
        g.setColour ((show ? (voiced ? col::text : col::dim) : col::dim.withAlpha (0.5f)));
        g.setFont (font (40.0f, true));
        g.drawText (name, nameArea, juce::Justification::centred);
        if (show)
        {
            g.setFont (font (13.0f, true));
            g.setColour (col::dim);
            const float nw = juce::GlyphArrangement::getStringWidth (font (40.0f, true), name);
            g.drawText (juce::String (note / 12 - 1), juce::Rectangle<float> (nameArea.getCentreX() + nw * 0.5f + 2.0f, nameArea.getY() + 16, 30, 20),
                        juce::Justification::centredLeft);
        }

        g.setFont (font (10.5f));
        g.setColour (col::dim);
        juce::String info = "A4 = " + juce::String (refHz, std::abs (refHz - std::floor (refHz)) < 0.01f ? 0 : 1) + " Hz";
        if (show)
        {
            info = (cents >= 0 ? "+" : "") + juce::String (juce::roundToInt (cents)) + " ct" + dot() + info;
            if (inst != 0) info += dot() + "sounds " + noteName ((int) std::lround (concert));
        }
        g.drawText (info, r.removeFromBottom (16).reduced (8, 0), juce::Justification::centred);
        g.setColour (col::brass.withAlpha (0.8f));
        g.setFont (font (10.0f, true).withExtraKerningFactor (0.15f));
        g.drawText ("TUNER", r.removeFromTop (18).reduced (10, 0), juce::Justification::centredLeft);
    }

private:
    static float centsOf (float m) { return (m - std::round (m)) * 100.0f; }
    bool voiced = false;
    float concert = 60, written = 60, smoothedCents = 0, refHz = 440;
    int inst = 0, lastSeen = 1000;
};

//==============================================================================
/** Scrolling piano-roll of your pitch (and the harmony voices). */
class PitchTrace : public juce::Component
{
public:
    PitchTrace() { for (auto& h : hist) h = { NAN, NAN, NAN }; }

    void push (bool voiced, float writtenMidi, bool harmOn, float shift1, float shift2, bool v1, bool v2)
    {
        Point p { NAN, NAN, NAN };
        if (voiced)
        {
            p.main = writtenMidi;
            if (harmOn && v1) p.h1 = writtenMidi + shift1;
            if (harmOn && v2) p.h2 = writtenMidi + shift2;
            centre += (voiced ? 0.08f : 0.0f) * (writtenMidi - centre);
        }
        hist[(size_t) head] = p;
        head = (head + 1) % size;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::screen);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (col::edge);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        auto plot = r.reduced (8.0f, 6.0f).withTrimmedLeft (28.0f).withTrimmedTop (12.0f);

        const float range = 5.5f;
        const float lo = std::round (centre) - range, hi = std::round (centre) + range;
        auto yOf = [&] (float m) { return plot.getBottom() - (m - lo) / (hi - lo) * plot.getHeight(); };

        g.saveState();
        g.reduceClipRegion (r.toNearestInt());
        for (int m = (int) lo; m <= (int) hi; ++m)
        {
            const int pc = ((m % 12) + 12) % 12;
            const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
            const float y = yOf ((float) m);
            if (black)
            {
                g.setColour (juce::Colours::white.withAlpha (0.025f));
                g.fillRect (plot.getX(), y - plot.getHeight() / (hi - lo) * 0.5f, plot.getWidth(), plot.getHeight() / (hi - lo));
            }
            g.setColour (pc == 0 ? col::brassDark.withAlpha (0.6f) : juce::Colours::white.withAlpha (0.05f));
            g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
            if (! black)
            {
                g.setColour (pc == 0 ? col::brass : col::dim.withAlpha (0.8f));
                g.setFont (font (9.5f, pc == 0));
                g.drawText (noteName (m), juce::Rectangle<float> (r.getX() + 4, y - 6, 30, 12), juce::Justification::centredLeft);
            }
        }

        auto drawLine = [&] (int which, juce::Colour c, float thick)
        {
            juce::Path p;
            bool drawing = false;
            for (int i = 0; i < size; ++i)
            {
                const auto& pt = hist[(size_t) ((head + i) % size)];
                const float m = which == 0 ? pt.main : which == 1 ? pt.h1 : pt.h2;
                const float x = plot.getX() + plot.getWidth() * (float) i / (float) (size - 1);
                if (std::isnan (m)) { drawing = false; continue; }
                const float y = juce::jlimit (plot.getY() - 4, plot.getBottom() + 4, yOf (m));
                if (! drawing) { p.startNewSubPath (x, y); drawing = true; }
                else p.lineTo (x, y);
            }
            g.setColour (c.withAlpha (0.18f));
            g.strokePath (p, juce::PathStrokeType (thick * 3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        drawLine (2, col::har.withAlpha (0.55f), 1.5f);
        drawLine (1, col::har.withAlpha (0.8f), 1.5f);
        drawLine (0, col::brass, 2.2f);

        const auto& last = hist[(size_t) ((head + size - 1) % size)];
        if (! std::isnan (last.main))
        {
            const float y = yOf (last.main);
            g.setColour (col::brass.withAlpha (0.3f));
            g.fillEllipse (plot.getRight() - 7, y - 7, 14, 14);
            g.setColour (juce::Colours::white);
            g.fillEllipse (plot.getRight() - 3, y - 3, 6, 6);
        }
        g.restoreState();

        g.setColour (col::brass.withAlpha (0.8f));
        g.setFont (font (10.0f, true).withExtraKerningFactor (0.15f));
        g.drawText ("PITCH TRACE", r.removeFromTop (18).reduced (10, 0), juce::Justification::centredLeft);
        g.setColour (col::dim);
        g.setFont (font (9.5f));
        g.drawText ("written pitch" + dot() + "green = harmony voices", getLocalBounds().toFloat().removeFromTop (18).reduced (10, 0), juce::Justification::centredRight);
    }

private:
    struct Point { float main, h1, h2; };
    static constexpr int size = 220;
    std::array<Point, size> hist {};
    int head = 0;
    float centre = 70.0f;
};

//==============================================================================
/** Live response curve of the EQ module. */
class EqCurve : public juce::Component
{
public:
    explicit EqCurve (APVTS& s) : state (s) {}

    void update()
    {
        float v[5];
        const char* ids[5] = { "eq_lowcut", "eq_body", "eq_mid", "eq_midf", "eq_air" };
        for (int i = 0; i < 5; ++i) v[i] = state.getRawParameterValue (ids[i])->load();
        if (std::equal (v, v + 5, last)) return;
        std::copy (v, v + 5, last);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::screen);
        g.fillRoundedRectangle (r, 5.0f);
        const double sr = 48000.0;
        Biquad f[5];
        f[0].highPass (last[0], 0.5412f, sr); f[1].highPass (last[0], 1.3066f, sr);
        f[2].peak (210.0f, 0.8f, last[1], sr); f[3].peak (last[3], 1.1f, last[2], sr); f[4].highShelf (7500.0f, last[4], sr);

        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (float hz : { 100.0f, 1000.0f, 10000.0f })
        {
            const float x = r.getX() + r.getWidth() * std::log (hz / 20.0f) / std::log (1000.0f);
            g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        }
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());

        juce::Path p;
        const int n = 120;
        for (int i = 0; i < n; ++i)
        {
            const float hz = 20.0f * std::pow (1000.0f, (float) i / (float) (n - 1));
            const std::complex<double> z1 = std::polar (1.0, -2.0 * juce::MathConstants<double>::pi * hz / sr);
            const std::complex<double> z2 = z1 * z1;
            double mag = 1.0;
            for (auto& b : f)
                mag *= std::abs (((double) b.b0 + (double) b.b1 * z1 + (double) b.b2 * z2) / (1.0 + (double) b.a1 * z1 + (double) b.a2 * z2));
            const float db = juce::jlimit (-18.0f, 18.0f, (float) (20.0 * std::log10 (std::max (mag, 1e-6))));
            const float x = r.getX() + r.getWidth() * (float) i / (float) (n - 1);
            const float y = r.getCentreY() - db / 18.0f * (r.getHeight() * 0.5f - 3.0f);
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        juce::Path fill (p);
        fill.lineTo (r.getRight(), r.getBottom()); fill.lineTo (r.getX(), r.getBottom()); fill.closeSubPath();
        g.setColour (col::eq.withAlpha (0.12f));
        g.fillPath (fill);
        g.setColour (col::eq);
        g.strokePath (p, juce::PathStrokeType (1.8f));
    }

private:
    APVTS& state;
    float last[5] { -1, -1, -1, -1, -1 };
};

//==============================================================================
/** The signal-flow strip: every module as a clickable pill, lit when on. */
class ChainStrip : public juce::Component
{
public:
    struct Node { juce::String label; juce::String paramId; juce::Colour colour; };

    ChainStrip (APVTS& s, std::vector<Node> n) : state (s), nodes (std::move (n)) {}

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        layout (r);
        g.setFont (font (9.5f, true));

        auto cap = [&] (juce::Rectangle<float> b, const juce::String& t)
        {
            g.setColour (col::brassDark);
            g.drawRoundedRectangle (b.reduced (1), b.getHeight() * 0.5f - 1, 1.0f);
            g.setColour (col::brass);
            g.drawText (t, b, juce::Justification::centred);
        };
        cap (inBox, "IN");
        cap (outBox, "OUT");

        auto arrow = [&] (float x0, float x1, float y, bool lit)
        {
            g.setColour (lit ? col::brass.withAlpha (0.7f) : col::edge.brighter (0.2f));
            g.drawLine (x0 + 2, y, x1 - 4, y, 1.4f);
            juce::Path a; a.addTriangle (x1 - 6, y - 3.5f, x1 - 6, y + 3.5f, x1 - 1, y);
            g.fillPath (a);
        };

        float prevRight = inBox.getRight();
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            const auto& b = boxes[i];
            const bool on = isOn (i);
            arrow (prevRight, b.getX(), b.getCentreY(), true);
            prevRight = b.getRight();
            if (on)
            {
                g.setColour (nodes[i].colour.withAlpha (0.2f));
                g.fillRoundedRectangle (b.expanded (2), b.getHeight() * 0.5f + 2);
                g.setColour (nodes[i].colour.withAlpha (0.9f));
                g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
                g.setColour (juce::Colours::black.withAlpha (0.85f));
            }
            else
            {
                g.setColour (i == (size_t) hoverIndex ? juce::Colour (0xff27231d) : juce::Colour (0xff17150f));
                g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
                g.setColour (col::edge.brighter (0.3f));
                g.drawRoundedRectangle (b, b.getHeight() * 0.5f, 1.0f);
                g.setColour (col::dim);
            }
            g.drawText (nodes[i].label, b, juce::Justification::centred);
            if (nodes[i].label == "TALK")   // stereo split marker
            {
                g.setColour (col::dim.withAlpha (0.7f));
                g.setFont (font (9.0f));
                g.drawText ("mono | stereo", juce::Rectangle<float> (b.getRight() - 10, b.getBottom() + 0, 80, 10), juce::Justification::centredLeft);
                g.setFont (font (9.5f, true));
            }
        }
        arrow (prevRight, outBox.getX(), outBox.getCentreY(), true);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int h = hit (e.position);
        if (h != hoverIndex) { hoverIndex = h; repaint(); }
        setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseExit (const juce::MouseEvent&) override { hoverIndex = -1; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int h = hit (e.position);
        if (h < 0) return;
        if (auto* p = state.getParameter (nodes[(size_t) h].paramId))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (isOn ((size_t) h) ? 0.0f : 1.0f);
            p->endChangeGesture();
        }
        repaint();
    }

    void refresh()
    {
        uint32_t mask = 0;
        for (size_t i = 0; i < nodes.size(); ++i) if (isOn (i)) mask |= 1u << i;
        if (mask != lastMask) { lastMask = mask; repaint(); }
    }

private:
    bool isOn (size_t i) const { return state.getRawParameterValue (nodes[i].paramId)->load() > 0.5f; }

    void layout (juce::Rectangle<float> r)
    {
        r = r.reduced (4, 6);
        const float h = 22.0f;
        const float y = r.getY();
        const float capW = 34.0f, gap = 8.0f;
        const float n = (float) nodes.size();
        const float w = (r.getWidth() - 2 * capW - (n + 1) * gap) / n;
        inBox = { r.getX(), y, capW, h };
        float x = inBox.getRight() + gap;
        boxes.resize (nodes.size());
        for (auto& b : boxes) { b = { x, y, w, h }; x += w + gap; }
        outBox = { x, y, capW, h };
    }

    int hit (juce::Point<float> p) const
    {
        for (size_t i = 0; i < boxes.size(); ++i) if (boxes[i].contains (p)) return (int) i;
        return -1;
    }

    APVTS& state;
    std::vector<Node> nodes;
    std::vector<juce::Rectangle<float>> boxes;
    juce::Rectangle<float> inBox, outBox;
    int hoverIndex = -1;
    uint32_t lastMask = 0xffffffff;
};
} // namespace rs::ui
