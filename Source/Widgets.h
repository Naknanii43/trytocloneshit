#pragma once

#include "PluginProcessor.h"
#include "NebulonLookAndFeel.h"

namespace neb::ui
{
//==============================================================================
// Rounded button with a drawn icon (play / stop / dice) and a label.
class IconButton : public juce::Button
{
public:
    enum Icon { play, dice };

    IconButton (const juce::String& text, Icon i, juce::Colour c) : juce::Button (text), icon (i), accent (c) {}

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const bool on = getToggleState();
        const float glow = on ? 0.55f + 0.25f * std::sin ((float) juce::Time::getMillisecondCounter() * 0.006f) : 0.0f;
        g.setColour (on ? accent.withAlpha (0.18f + 0.12f * glow) : colours::panelHi.brighter (over ? 0.18f : 0.0f).darker (down ? 0.2f : 0.0f));
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (on || over ? accent : colours::outline.brighter (0.3f));
        g.drawRoundedRectangle (r, 7.0f, on ? 1.6f : 1.0f);

        auto ic = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.28f);
        g.setColour (accent);
        if (icon == play)
        {
            juce::Path p;
            if (on) { p.addRectangle (ic.reduced (ic.getWidth() * 0.1f)); }
            else p.addTriangle (ic.getX(), ic.getY(), ic.getX(), ic.getBottom(), ic.getRight(), ic.getCentreY());
            g.fillPath (p);
        }
        else
        {
            g.drawRoundedRectangle (ic, 2.5f, 1.5f);
            const float d = ic.getWidth() * 0.18f;
            for (auto pt : { ic.getCentre(), ic.getTopLeft().translated (ic.getWidth() * 0.27f, ic.getHeight() * 0.27f),
                             ic.getBottomRight().translated (-ic.getWidth() * 0.27f, -ic.getHeight() * 0.27f) })
                g.fillEllipse (pt.x - d / 2, pt.y - d / 2, d, d);
        }

        g.setColour (on ? accent.brighter (0.3f) : colours::text);
        g.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold)).withExtraKerningFactor (0.06f));
        g.drawText (getButtonText(), r.withTrimmedRight (6), juce::Justification::centred);
    }

private:
    Icon icon;
    juce::Colour accent;
};

//==============================================================================
// Animated, audio-reactive XY surface. X drives Macro 1 (Tone), Y drives Macro 2 (Morph).
class NebulaPad : public juce::Component, private juce::Timer
{
public:
    explicit NebulaPad (NebulonProcessor& p) : proc (p)
    {
        juce::Random r (42);
        for (auto& s : stars) s = { r.nextFloat(), r.nextFloat(), 0.3f + r.nextFloat() * 0.7f, r.nextFloat() * 6.28f };
        startTimerHz (40);
        setRepaintsOnMouseActivity (true);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const float lvl = level;
        juce::Path clip;
        clip.addRoundedRectangle (b, 10.0f);
        g.saveState();
        g.reduceClipRegion (clip);

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1f3a), b.getCentreX(), b.getCentreY(),
                                                 colours::bg, b.getX(), b.getY(), true));
        g.fillRect (b);

        // drifting star field
        for (auto& s : stars)
        {
            const float tw = 0.5f + 0.5f * std::sin (s.phase);
            g.setColour (juce::Colours::white.withAlpha (0.12f + 0.35f * tw * s.z));
            const float d = 1.0f + 1.4f * s.z;
            g.fillEllipse (b.getX() + s.x * b.getWidth(), b.getY() + s.y * b.getHeight(), d, d);
        }

        // grid
        g.setColour (colours::outline.withAlpha (0.35f));
        for (int i = 1; i < 6; ++i)
        {
            g.drawVerticalLine ((int) (b.getX() + b.getWidth() * i / 6.0f), b.getY(), b.getBottom());
            g.drawHorizontalLine ((int) (b.getY() + b.getHeight() * i / 6.0f), b.getX(), b.getRight());
        }

        const auto pt = posToPoint (macroX(), macroY());

        // trail
        for (size_t i = 0; i < trail.size(); ++i)
        {
            const auto& t = trail[i];
            const float a = t.life;
            g.setColour (colours::layerB.interpolatedWith (colours::layerA, (float) i / (float) juce::jmax ((size_t) 1, trail.size())).withAlpha (a * 0.5f));
            const float d = 4.0f + 10.0f * a;
            g.fillEllipse (t.p.x - d / 2, t.p.y - d / 2, d, d);
        }

        // cross-hair
        g.setColour (colours::layerA.withAlpha (0.25f));
        g.drawVerticalLine ((int) pt.x, b.getY(), b.getBottom());
        g.drawHorizontalLine ((int) pt.y, b.getX(), b.getRight());

        // pulsing orb-like glow at the cursor, reacting to output level
        for (int ring = 4; ring >= 1; --ring)
        {
            const float rr = (10.0f + 34.0f * lvl) * (float) ring * 0.55f;
            g.setColour (colours::layerA.interpolatedWith (colours::layerB, macroY()).withAlpha (0.07f * (5 - ring) * (0.4f + lvl)));
            g.fillEllipse (pt.x - rr, pt.y - rr, rr * 2, rr * 2);
        }
        g.setColour (juce::Colours::white);
        g.fillEllipse (pt.x - 5, pt.y - 5, 10, 10);
        g.setColour (colours::layerA);
        g.drawEllipse (pt.x - 9, pt.y - 9, 18, 18, 1.5f);
        g.restoreState();

        g.setColour (isMouseOverOrDragging() ? colours::layerA.withAlpha (0.8f) : colours::outline);
        g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.2f);

        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)).withExtraKerningFactor (0.1f));
        g.setColour (colours::perf.withAlpha (0.9f));
        g.drawText ("NEBULA PAD", b.reduced (10, 8).removeFromTop (14), juce::Justification::centredLeft);
        g.setColour (colours::textDim);
        g.drawText ("TONE  >", b.reduced (10, 6).removeFromBottom (14), juce::Justification::centredRight);
        g.drawText ("MORPH  ^", b.reduced (10, 6).removeFromBottom (14), juce::Justification::centredLeft);
        if (! everTouched)
        {
            g.setColour (colours::text.withAlpha (0.55f + 0.3f * std::sin (anim * 2.0f)));
            g.setFont (juce::Font (juce::FontOptions (12.0f)));
            g.drawText ("drag me while you play", b.withTrimmedTop (b.getHeight() * 0.55f), juce::Justification::centredTop);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        everTouched = true;
        for (auto* p : params()) p->beginChangeGesture();
        mouseDrag (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto b = getLocalBounds().toFloat().reduced (8.0f);
        const float x = juce::jlimit (0.0f, 1.0f, (e.position.x - b.getX()) / b.getWidth());
        const float y = juce::jlimit (0.0f, 1.0f, 1.0f - (e.position.y - b.getY()) / b.getHeight());
        auto ps = params();
        ps[0]->setValueNotifyingHost (x);
        ps[1]->setValueNotifyingHost (y);
        trail.push_back ({ e.position, 1.0f });
        if (trail.size() > 40) trail.erase (trail.begin());
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        for (auto* p : params()) p->endChangeGesture();
    }

private:
    struct Star { float x, y, z, phase; };
    struct TrailPt { juce::Point<float> p; float life; };

    std::array<juce::RangedAudioParameter*, 2> params()
    {
        return { proc.apvts.getParameter ("macro1"), proc.apvts.getParameter ("macro2") };
    }
    float macroX() const { return proc.apvts.getRawParameterValue ("macro1")->load(); }
    float macroY() const { return proc.apvts.getRawParameterValue ("macro2")->load(); }

    juce::Point<float> posToPoint (float x, float y) const
    {
        auto b = getLocalBounds().toFloat().reduced (8.0f);
        return { b.getX() + x * b.getWidth(), b.getY() + (1.0f - y) * b.getHeight() };
    }

    void timerCallback() override
    {
        anim += 0.025f;
        float peak = 0.0f;
        const int w = proc.scopeWrite.load();
        for (int i = 1; i <= 512; ++i)
            peak = juce::jmax (peak, std::abs (proc.scope[(size_t) ((w - i + NebulonProcessor::scopeSize) % NebulonProcessor::scopeSize)]));
        level += (juce::jmin (1.0f, peak * 1.3f) - level) * 0.25f;

        for (auto& s : stars)
        {
            s.x += 0.0004f * s.z * (1.0f + 4.0f * level);
            if (s.x > 1.0f) s.x -= 1.0f;
            s.phase += 0.05f + 0.1f * s.z;
        }
        for (auto& t : trail) t.life -= 0.03f;
        while (! trail.empty() && trail.front().life <= 0.0f) trail.erase (trail.begin());
        repaint();
    }

    NebulonProcessor& proc;
    std::array<Star, 90> stars;
    std::vector<TrailPt> trail;
    float level = 0.0f, anim = 0.0f;
    bool everTouched = false;
};
} // namespace neb::ui
