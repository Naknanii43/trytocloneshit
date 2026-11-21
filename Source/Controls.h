#pragma once

#include "PluginProcessor.h"
#include "NebulonLookAndFeel.h"

namespace neb::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

inline void setWeight (juce::Component& c, float w) { c.getProperties().set ("w", w); }

class Knob : public juce::Component
{
public:
    Knob (APVTS& s, const juce::String& id, const juce::String& name, juce::Colour c, juce::Component* popupParent)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setPopupDisplayEnabled (true, true, popupParent);
        slider.setColour (juce::Slider::rotarySliderFillColourId, c);
        attachment = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
        if (auto* p = s.getParameter (id))
        {
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
            slider.setTooltip (p->getName (64));
        }
        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::Font (juce::FontOptions (11.5f)));
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (slider);
        addAndMakeVisible (label);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromBottom (15));
        const int d = juce::jmin (r.getWidth(), r.getHeight(), 64);
        slider.setBounds (r.withSizeKeepingCentre (d, d));
    }

    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

class Choice : public juce::Component
{
public:
    Choice (APVTS& s, const juce::String& id, const juce::String& name)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)))
            box.addItemList (p->choices, 1);
        attachment = std::make_unique<APVTS::ComboBoxAttachment> (s, id, box);
        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::Font (juce::FontOptions (11.5f)));
        addAndMakeVisible (box);
        addAndMakeVisible (label);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (3, 0);
        auto boxArea = r.withSizeKeepingCentre (r.getWidth(), juce::jmin (26, r.getHeight()));
        box.setBounds (boxArea);
        label.setBounds (r.withTrimmedTop (boxArea.getBottom() + 2).removeFromTop (15));
    }

    juce::ComboBox box;
    juce::Label label;
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

class Toggle : public juce::Component
{
public:
    Toggle (APVTS& s, const juce::String& id, const juce::String& name, juce::Colour c)
    {
        button.setButtonText (name);
        button.setColour (juce::ToggleButton::tickColourId, c);
        attachment = std::make_unique<APVTS::ButtonAttachment> (s, id, button);
        addAndMakeVisible (button);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        button.setBounds (r.withSizeKeepingCentre (r.getWidth(), juce::jmin (26, r.getHeight())));
    }

    juce::ToggleButton button;
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

// A titled box that lays its children out in rows; each child can carry a width weight "w".
class Panel : public juce::Component
{
public:
    Panel (juce::String t, juce::Colour c) : title (std::move (t)), colour (c) {}

    void addRow (std::initializer_list<juce::Component*> items)
    {
        rows.emplace_back (items);
        for (auto* c : items) addAndMakeVisible (c);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (colours::panelHi.withAlpha (0.55f));
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (colours::outline.withAlpha (0.8f));
        g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
        g.setColour (colour.withAlpha (0.9f));
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)).withExtraKerningFactor (0.08f));
        g.drawText (title, getLocalBounds().reduced (10, 5).removeFromTop (15), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6, 5);
        r.removeFromTop (17);
        if (rows.empty()) return;
        const int h = r.getHeight() / (int) rows.size();
        for (auto& row : rows)
        {
            auto rr = r.removeFromTop (h);
            float total = 0;
            for (auto* c : row) if (c->isVisible()) total += (float) c->getProperties().getWithDefault ("w", 1.0);
            if (total <= 0) continue;
            const float unitW = (float) rr.getWidth() / total;
            float x = (float) rr.getX();
            for (auto* c : row)
            {
                if (! c->isVisible()) continue;
                const float w = unitW * (float) c->getProperties().getWithDefault ("w", 1.0);
                auto cell = juce::Rectangle<float> (x, (float) rr.getY(), w, (float) rr.getHeight()).toNearestInt().reduced (2, 1);
                const int maxH = c->getProperties().getWithDefault ("maxH", 0);
                if (maxH > 0 && cell.getHeight() > maxH) cell = cell.withSizeKeepingCentre (cell.getWidth(), maxH);
                c->setBounds (cell);
                x += w;
            }
        }
    }

    juce::String title;
    juce::Colour colour;
    std::vector<std::vector<juce::Component*>> rows;
};

// Base class for the editor's tab pages: owns its controls.
class Page : public juce::Component
{
public:
    Page (NebulonProcessor& p, juce::Component* popup) : proc (p), popupParent (popup) {}
    virtual void refresh() {}

protected:
    template <typename T>
    T* own (std::unique_ptr<T> c)
    {
        auto* raw = c.get();
        owned.push_back (std::move (c));
        return raw;
    }

    Knob* knob (const juce::String& id, const juce::String& name, juce::Colour c, float w = 1.0f)
    {
        auto* k = own (std::make_unique<Knob> (proc.apvts, id, name, c, popupParent));
        setWeight (*k, w);
        return k;
    }

    Choice* choice (const juce::String& id, const juce::String& name, float w = 1.4f)
    {
        auto* k = own (std::make_unique<Choice> (proc.apvts, id, name));
        setWeight (*k, w);
        return k;
    }

    Toggle* toggle (const juce::String& id, const juce::String& name, juce::Colour c, float w = 1.2f)
    {
        auto* k = own (std::make_unique<Toggle> (proc.apvts, id, name, c));
        setWeight (*k, w);
        return k;
    }

    Panel* panel (const juce::String& title, juce::Colour c)
    {
        auto* p = own (std::make_unique<Panel> (title, c));
        addAndMakeVisible (p);
        return p;
    }

    NebulonProcessor& proc;
    juce::Component* popupParent;
    std::vector<std::unique_ptr<juce::Component>> owned;
};
} // namespace neb::ui
