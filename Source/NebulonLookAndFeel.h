#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace neb
{
namespace colours
{
    const juce::Colour bg       { 0xff0b0e17 };
    const juce::Colour panel    { 0xff141a28 };
    const juce::Colour panelHi  { 0xff1c2438 };
    const juce::Colour outline  { 0xff2a3450 };
    const juce::Colour text     { 0xffd8e0f0 };
    const juce::Colour textDim  { 0xff7f8aa6 };
    const juce::Colour layerA   { 0xff35d0e8 };   // cyan
    const juce::Colour layerB   { 0xffb06bff };   // violet
    const juce::Colour fx       { 0xffffb347 };   // amber
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour (juce::ComboBox::backgroundColourId, colours::panelHi);
        setColour (juce::ComboBox::outlineColourId, colours::outline);
        setColour (juce::ComboBox::textColourId, colours::text);
        setColour (juce::ComboBox::arrowColourId, colours::textDim);
        setColour (juce::PopupMenu::backgroundColourId, colours::panel);
        setColour (juce::PopupMenu::textColourId, colours::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::panelHi);
        setColour (juce::Label::textColourId, colours::textDim);
        setColour (juce::TextButton::buttonColourId, colours::panelHi);
        setColour (juce::TextButton::textColourOffId, colours::text);
        setColour (juce::BubbleComponent::backgroundColourId, colours::panelHi);
        setColour (juce::BubbleComponent::outlineColourId, colours::outline);
        setColour (juce::TooltipWindow::textColourId, colours::text);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
        auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const float track = juce::jmax (2.5f, radius * 0.13f);
        const float arcR = radius - track * 0.5f;

        // track
        juce::Path bgArc;
        bgArc.addCentredArc (c.x, c.y, arcR, arcR, 0, startAngle, endAngle, true);
        g.setColour (colours::outline);
        g.strokePath (bgArc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // value arc, bipolar params draw from the centre
        const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
        const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
        juce::Path valArc;
        valArc.addCentredArc (c.x, c.y, arcR, arcR, 0, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (accent.withAlpha (0.25f));
        g.strokePath (valArc, juce::PathStrokeType (track * 2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (valArc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // knob body
        const float bodyR = radius - track * 2.0f;
        juce::ColourGradient grad (colours::panelHi.brighter (0.25f), c.x, c.y - bodyR,
                                   colours::bg, c.x, c.y + bodyR, false);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2);
        g.setColour (colours::outline.brighter (0.2f));
        g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2, 1.0f);

        // pointer
        juce::Path ptr;
        ptr.addRoundedRectangle (-1.5f, -bodyR + 2.0f, 3.0f, bodyR * 0.55f, 1.5f);
        g.setColour (s.isMouseOverOrDragging() ? accent.brighter (0.4f) : colours::text);
        g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (c));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        const auto accent = b.findColour (juce::ToggleButton::tickColourId);
        auto r = b.getLocalBounds().toFloat().reduced (2.0f);
        auto pill = r.removeFromLeft (r.getHeight() * 1.8f);
        const bool on = b.getToggleState();
        g.setColour (on ? accent.withAlpha (0.3f) : colours::outline);
        g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
        const float d = pill.getHeight() - 4.0f;
        const float kx = on ? pill.getRight() - d - 2.0f : pill.getX() + 2.0f;
        g.setColour (on ? accent : colours::textDim.withAlpha (over ? 1.0f : 0.7f));
        g.fillEllipse (kx, pill.getY() + 2.0f, d, d);

        g.setColour (on ? colours::text : colours::textDim);
        g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (8), juce::Justification::centredLeft);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (13.0f)); }
    juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (14.0f)); }
};
} // namespace neb
