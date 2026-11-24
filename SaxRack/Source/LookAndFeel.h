// Reedstorm styling: brass on smoked black, one accent colour per module.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace rs::ui
{
namespace col
{
    const juce::Colour bg        { 0xff0d0c0a };
    const juce::Colour panel     { 0xff1a1714 };
    const juce::Colour panel2    { 0xff12100e };
    const juce::Colour edge      { 0xff2e2922 };
    const juce::Colour brass     { 0xffe2b45c };
    const juce::Colour brassDark { 0xff7d5f2a };
    const juce::Colour text      { 0xfff1e8d8 };
    const juce::Colour dim       { 0xff8e8473 };
    const juce::Colour screen    { 0xff070807 };

    const juce::Colour dyn    { 0xffe2b45c };
    const juce::Colour oct    { 0xffff6b3d };
    const juce::Colour syn    { 0xffb36bff };
    const juce::Colour wah    { 0xffffd23f };
    const juce::Colour drv    { 0xffff3d5e };
    const juce::Colour eq     { 0xff4fd1c5 };
    const juce::Colour har    { 0xff72d66b };
    const juce::Colour mod    { 0xff4da3ff };
    const juce::Colour dly    { 0xffff8fd1 };
    const juce::Colour rev    { 0xff9cc2ff };
}

inline juce::Font font (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

inline juce::Colour accentOf (const juce::Component& c, juce::Colour fallback = col::brass)
{
    const auto v = c.getProperties()["accent"];
    return v.isVoid() ? fallback : juce::Colour ((juce::uint32) (juce::int64) v);
}

class ReedLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ReedLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff15130f));
        setColour (juce::PopupMenu::textColourId, col::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::brass.withAlpha (0.25f));
        setColour (juce::PopupMenu::highlightedTextColourId, col::brass);
        setColour (juce::PopupMenu::headerTextColourId, col::brass);
        setColour (juce::ComboBox::textColourId, col::text);
        setColour (juce::ComboBox::arrowColourId, col::brass);
        setColour (juce::Label::textColourId, col::text);
        setColour (juce::TextButton::textColourOffId, col::text);
        setColour (juce::TextButton::textColourOnId, col::brass);
        setColour (juce::Slider::textBoxTextColourId, col::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf0181511));
        setColour (juce::TooltipWindow::textColourId, col::text);
        setColour (juce::TooltipWindow::outlineColourId, col::brassDark);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider& s) override
    {
        const auto accent = accentOf (s);
        const bool bipolar = (bool) s.getProperties()["bipolar"];
        const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
        const float size = juce::jmin (area.getWidth(), area.getHeight());
        const auto c = area.getCentre();
        const float r = size * 0.5f;
        const float ang = a0 + pos * (a1 - a0);
        const float trackR = r - 3.0f;

        // track
        juce::Path track;
        track.addCentredArc (c.x, c.y, trackR, trackR, 0, a0, a1, true);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.strokePath (track, juce::PathStrokeType (4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xff2a2621));
        g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // value arc with glow
        const float from = bipolar ? (a0 + a1) * 0.5f : a0;
        if (std::abs (ang - from) > 0.01f)
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, trackR, trackR, 0, juce::jmin (from, ang), juce::jmax (from, ang), true);
            g.setColour (accent.withAlpha (0.18f));
            g.strokePath (arc, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (accent);
            g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // knob body: knurled metal
        const float br = r * 0.68f;
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (c.x - br, c.y - br + 2.5f, br * 2, br * 2);
        juce::ColourGradient body (juce::Colour (0xff3b352d), c.x, c.y - br, juce::Colour (0xff12100d), c.x, c.y + br, false);
        g.setGradientFill (body);
        g.fillEllipse (c.x - br, c.y - br, br * 2, br * 2);
        g.setColour (juce::Colour (0xff4a4238).withAlpha (0.8f));
        for (int i = 0; i < 24; ++i)
        {
            const float t = juce::MathConstants<float>::twoPi * (float) i / 24.0f;
            g.drawLine (c.x + std::sin (t) * br * 0.86f, c.y - std::cos (t) * br * 0.86f,
                        c.x + std::sin (t) * br * 0.98f, c.y - std::cos (t) * br * 0.98f, 1.0f);
        }
        const float ir = br * 0.74f;
        juce::ColourGradient cap (juce::Colour (0xff2c2721), c.x, c.y - ir, juce::Colour (0xff181512), c.x, c.y + ir, false);
        g.setGradientFill (cap);
        g.fillEllipse (c.x - ir, c.y - ir, ir * 2, ir * 2);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawEllipse (c.x - ir, c.y - ir, ir * 2, ir * 2, 1.0f);

        // pointer
        const juce::Point<float> p1 (c.x + std::sin (ang) * ir * 0.25f, c.y - std::cos (ang) * ir * 0.25f);
        const juce::Point<float> p2 (c.x + std::sin (ang) * ir * 0.95f, c.y - std::cos (ang) * ir * 0.95f);
        g.setColour (accent.withAlpha (0.35f));
        g.drawLine ({ p1, p2 }, 5.0f);
        g.setColour (accent.brighter (0.4f));
        g.drawLine ({ p1, p2 }, 2.0f);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        const auto accent = accentOf (box);
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (col::screen);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (box.isMouseOver (true) ? accent.withAlpha (0.8f) : col::edge.brighter (0.2f));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        juce::Path arrow;
        const float ax = (float) w - 13.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
        g.setColour (accent);
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.0f, true); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (4, 1, box.getWidth() - 22, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
        label.setJustificationType (juce::Justification::centredLeft);
    }

    juce::Font getPopupMenuFont() override { return font (14.0f); }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
        g.setColour (col::brassDark);
        g.drawRect (0, 0, w, h, 1);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (down ? col::brass.withAlpha (0.3f) : over ? juce::Colour (0xff2a251e) : juce::Colour (0xff1c1915));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (over ? col::brass : col::edge.brighter (0.3f));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return font ((float) h * 0.55f, true); }

    // Power LEDs and small switches
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        const auto accent = accentOf (b);
        const bool on = b.getToggleState();
        auto r = b.getLocalBounds().toFloat();

        if (b.getButtonText().isEmpty())     // round power LED
        {
            const float d = juce::jmin (r.getWidth(), r.getHeight()) - 4.0f;
            auto led = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
            if (on)
            {
                g.setColour (accent.withAlpha (0.25f));
                g.fillEllipse (led.expanded (4.0f));
            }
            g.setColour (on ? accent : juce::Colour (0xff2b2722));
            g.fillEllipse (led);
            g.setColour (on ? juce::Colours::white.withAlpha (0.55f) : juce::Colours::black.withAlpha (0.6f));
            g.drawEllipse (led, 1.0f);
            // power glyph
            juce::Path p;
            const auto cc = led.getCentre();
            const float pr = d * 0.26f;
            p.addCentredArc (cc.x, cc.y, pr, pr, 0, 0.75f, juce::MathConstants<float>::twoPi - 0.75f, true);
            p.startNewSubPath (cc.x, cc.y - pr * 1.25f);
            p.lineTo (cc.x, cc.y - pr * 0.2f);
            g.setColour (on ? juce::Colours::black.withAlpha (0.75f) : (over ? accent : col::dim));
            g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            return;
        }

        // pill switch with label
        auto pill = r.reduced (1.0f);
        g.setColour (on ? accent.withAlpha (0.22f) : col::screen);
        g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
        g.setColour (on ? accent : (over ? col::dim : col::edge.brighter (0.3f)));
        g.drawRoundedRectangle (pill, pill.getHeight() * 0.5f, 1.0f);
        g.setColour (on ? accent : col::dim);
        g.setFont (font (11.5f, true));
        g.drawText (b.getButtonText(), pill, juce::Justification::centred);
    }
};
} // namespace rs::ui
