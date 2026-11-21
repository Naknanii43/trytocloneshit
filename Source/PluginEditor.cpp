#include "PluginEditor.h"

using namespace neb;

NebulonEditor::NebulonEditor (NebulonProcessor& p)
    : AudioProcessorEditor (&p), proc (p), scope (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);

    buildLayer (0, "A_", colours::layerA);
    buildLayer (1, "B_", colours::layerB);

    auto fx = [this] (const char* title, std::initializer_list<std::pair<const char*, const char*>> items)
    {
        Section s { title, colours::fx, {}, nullptr, {} };
        for (auto& [id, name] : items) s.knobs.push_back (makeKnob (id, name, colours::fx));
        fxSections.push_back (std::move (s));
    };
    fx ("CHORUS", { { "chorusMix", "Mix" }, { "chorusRate", "Rate" }, { "chorusDepth", "Depth" } });
    fx ("DELAY",  { { "delayTime", "Time" }, { "delayFb", "Feedback" }, { "delayMix", "Mix" } });
    fx ("REVERB", { { "revSize", "Size" }, { "revDamp", "Damp" }, { "revMix", "Mix" } });
    fx ("MASTER", { { "master", "Volume" } });

    // presets
    addAndMakeVisible (presetBox);
    refreshPresetBox();
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedItemIndex();
        if (idx >= 0 && idx != proc.getCurrentProgram()) proc.setCurrentProgram (idx);
    };
    for (auto* b : { &prevBtn, &nextBtn }) addAndMakeVisible (*b);
    auto step = [this] (int d)
    {
        const int n = proc.getNumPrograms();
        proc.setCurrentProgram ((proc.getCurrentProgram() + d + n) % n);
        refreshPresetBox();
    };
    prevBtn.onClick = [step] { step (-1); };
    nextBtn.onClick = [step] { step (1); };

    addAndMakeVisible (scope);

    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffcfd6e6));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, colours::panel);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, colours::layerA.withAlpha (0.7f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, colours::layerB.withAlpha (0.3f));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, colours::outline);
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    keyboard.setLowestVisibleKey (36);
    addAndMakeVisible (keyboard);

    setResizable (true, true);
    setResizeLimits (1000, 700, 1800, 1260);
    getConstrainer()->setFixedAspectRatio (1140.0 / 790.0);
    setSize (1140, 790);
    startTimerHz (30);
}

NebulonEditor::~NebulonEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

NebulonEditor::Knob* NebulonEditor::makeKnob (const juce::String& paramId, const juce::String& name, juce::Colour accent)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    k->slider.setPopupDisplayEnabled (true, true, this);
    k->slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k->slider);
    if (auto* param = proc.apvts.getParameter (paramId))
    {
        k->slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        k->slider.setTooltip (param->getName (64));
    }

    k->label.setText (name, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (juce::Font (juce::FontOptions (11.5f)));
    k->label.setInterceptsMouseClicks (false, false);

    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    knobs.push_back (std::move (k));
    return knobs.back().get();
}

void NebulonEditor::buildLayer (int index, const juce::String& prefix, juce::Colour accent)
{
    auto& L = layers[index];
    L.power.setButtonText (index == 0 ? "LAYER A" : "LAYER B");
    L.power.setColour (juce::ToggleButton::tickColourId, accent);
    L.powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, prefix + "on", L.power);
    addAndMakeVisible (L.power);

    L.filterType.addItemList ({ "LP 12", "LP 24", "Band", "High" }, 1);
    L.filterAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, prefix + "ftype", L.filterType);
    addAndMakeVisible (L.filterType);

    auto sec = [&] (int row, const char* title, std::initializer_list<std::pair<const char*, const char*>> items,
                    juce::Component* lead = nullptr)
    {
        Section s { title, accent, {}, lead, {} };
        for (auto& [id, name] : items) s.knobs.push_back (makeKnob (prefix + id, name, accent));
        L.rows[row].push_back (std::move (s));
    };

    sec (0, "OSCILLATOR", { { "wave", "Shape" }, { "octave", "Octave" }, { "semi", "Semi" }, { "fine", "Fine" },
                            { "unison", "Unison" }, { "detune", "Detune" }, { "spread", "Width" } });
    sec (1, "TEXTURE", { { "sub", "Sub" }, { "noise", "Noise" }, { "drift", "Drift" }, { "drive", "Drive" } });
    sec (1, "FILTER", { { "cutoff", "Cutoff" }, { "reso", "Reso" }, { "fenv", "Env" }, { "keytrack", "Key" } }, &L.filterType);
    sec (2, "AMP ENVELOPE", { { "aA", "Attack" }, { "aD", "Decay" }, { "aS", "Sustain" }, { "aR", "Release" } });
    sec (2, "FILTER ENVELOPE", { { "fA", "Attack" }, { "fD", "Decay" }, { "fS", "Sustain" }, { "fR", "Release" } });
    sec (3, "LFO", { { "lfoRate", "Rate" }, { "lfoCut", "> Cutoff" }, { "lfoPitch", "> Pitch" }, { "lfoAmp", "> Amp" } });
    sec (3, "OUTPUT", { { "level", "Level" }, { "pan", "Pan" } });
}

void NebulonEditor::refreshPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedItemIndex (proc.getCurrentProgram(), juce::dontSendNotification);
}

//==============================================================================
void NebulonEditor::layoutSection (Section& s)
{
    auto r = s.bounds.reduced (6, 4);
    r.removeFromTop (16); // title
    const int n = (int) s.knobs.size() + (s.lead != nullptr ? 1 : 0);
    if (n == 0) return;

    // a lead control (combo box) gets a wider cell than a knob
    const float units = (float) s.knobs.size() + (s.lead != nullptr ? 1.6f : 0.0f);
    const int cellW = juce::roundToInt ((float) r.getWidth() / units);

    if (s.lead != nullptr)
    {
        auto cell = r.removeFromLeft (r.getWidth() - cellW * (int) s.knobs.size()).reduced (3, 0);
        s.lead->setBounds (cell.withSizeKeepingCentre (cell.getWidth(), 24));
    }
    for (auto* k : s.knobs)
    {
        auto cell = r.removeFromLeft (cellW);
        k->label.setBounds (cell.removeFromBottom (15));
        const int d = juce::jmin (cell.getWidth(), cell.getHeight());
        k->slider.setBounds (cell.withSizeKeepingCentre (d, d));
    }
}

void NebulonEditor::resized()
{
    const float scale = (float) getWidth() / 1140.0f;
    auto area = getLocalBounds().reduced (juce::roundToInt (12 * scale));
    const auto gap = juce::roundToInt (8 * scale);

    headerArea = area.removeFromTop (juce::roundToInt (78 * scale));
    area.removeFromTop (gap);
    keyboard.setBounds (area.removeFromBottom (juce::roundToInt (72 * scale)));
    keyboard.setKeyWidth (keyboard.getWidth() / 36.0f); // 3 octaves
    area.removeFromBottom (gap);
    auto fxArea = area.removeFromBottom (juce::roundToInt (118 * scale));
    area.removeFromBottom (gap);

    // header: title | scope | presets
    {
        auto h = headerArea.reduced (14, 10);
        auto presetArea = h.removeFromRight (juce::roundToInt (300 * scale));
        h.removeFromLeft (juce::roundToInt (230 * scale));
        scope.setBounds (h.reduced (12, 0));
        auto row = presetArea.withSizeKeepingCentre (presetArea.getWidth(), 28).translated (0, 8);
        prevBtn.setBounds (row.removeFromLeft (28));
        nextBtn.setBounds (row.removeFromRight (28));
        presetBox.setBounds (row.reduced (4, 0));
    }

    // layer panels
    auto left = area.removeFromLeft ((area.getWidth() - gap) / 2);
    area.removeFromLeft (gap);
    juce::Rectangle<int> panels[2] = { left, area };

    for (int l = 0; l < 2; ++l)
    {
        auto& L = layers[l];
        L.bounds = panels[l];
        auto r = panels[l].reduced (6);
        L.power.setBounds (r.removeFromTop (28).removeFromLeft (160).reduced (4, 2));
        r.removeFromTop (2);
        const int rowH = r.getHeight() / 4;

        // relative widths per row, weighted by control count
        for (int row = 0; row < 4; ++row)
        {
            auto rr = r.removeFromTop (rowH);
            auto& secs = L.rows[row];
            int total = 0;
            for (auto& s : secs) total += (int) s.knobs.size() + (s.lead ? 2 : 0);
            const int fullW = rr.getWidth();
            for (size_t i = 0; i < secs.size(); ++i)
            {
                auto& s = secs[i];
                const int cnt = (int) s.knobs.size() + (s.lead ? 2 : 0);
                const int w = i + 1 == secs.size() ? rr.getWidth() : fullW * cnt / total;
                s.bounds = rr.removeFromLeft (w).reduced (3);
                layoutSection (s);
            }
        }
    }

    // fx strip
    {
        int total = 0;
        for (auto& s : fxSections) total += (int) s.knobs.size();
        const int fullW = fxArea.getWidth();
        for (size_t i = 0; i < fxSections.size(); ++i)
        {
            auto& s = fxSections[i];
            const int w = i + 1 == fxSections.size() ? fxArea.getWidth() : fullW * (int) s.knobs.size() / total;
            s.bounds = fxArea.removeFromLeft (w).reduced (3, 0);
            layoutSection (s);
        }
    }
}

//==============================================================================
static void drawSection (juce::Graphics& g, const juce::String& title, juce::Colour c, juce::Rectangle<int> b)
{
    auto r = b.toFloat();
    g.setColour (colours::panelHi.withAlpha (0.55f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::outline.withAlpha (0.8f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (c.withAlpha (0.85f));
    g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    g.drawText (title, b.reduced (9, 4).removeFromTop (14), juce::Justification::centredLeft);
}

void NebulonEditor::paint (juce::Graphics& g)
{
    // background with soft nebula glows
    g.fillAll (colours::bg);
    const auto w = (float) getWidth(), h = (float) getHeight();
    g.setGradientFill (juce::ColourGradient (colours::layerA.withAlpha (0.10f), w * 0.15f, h * 0.1f,
                                             juce::Colours::transparentBlack, w * 0.55f, h * 0.6f, true));
    g.fillAll();
    g.setGradientFill (juce::ColourGradient (colours::layerB.withAlpha (0.10f), w * 0.85f, h * 0.2f,
                                             juce::Colours::transparentBlack, w * 0.4f, h * 0.7f, true));
    g.fillAll();

    // header
    {
        auto hb = headerArea.toFloat();
        g.setColour (colours::panel.withAlpha (0.85f));
        g.fillRoundedRectangle (hb, 10.0f);
        g.setColour (colours::outline);
        g.drawRoundedRectangle (hb.reduced (0.5f), 10.0f, 1.0f);

        auto t = headerArea.reduced (20, 10).removeFromLeft (juce::roundToInt (240 * w / 1140.0f));
        juce::ColourGradient tg (colours::layerA, (float) t.getX(), 0, colours::layerB, (float) t.getRight() - 30, 0, false);
        g.setGradientFill (tg);
        g.setFont (juce::Font (juce::FontOptions (34.0f * w / 1140.0f, juce::Font::bold)).withExtraKerningFactor (0.18f));
        g.drawText ("NEBULON", t.removeFromTop (t.getHeight() * 2 / 3), juce::Justification::bottomLeft);
        g.setColour (colours::textDim);
        g.setFont (juce::Font (juce::FontOptions (11.0f)).withExtraKerningFactor (0.12f));
        g.drawText ("HYBRID ATMOSPHERE SYNTHESIZER", t, juce::Justification::topLeft);

        auto pa = headerArea.reduced (14, 10).removeFromRight (juce::roundToInt (300 * w / 1140.0f));
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        g.drawText ("PATCH", pa.removeFromTop (16).reduced (4, 0), juce::Justification::centredLeft);
    }

    // layer panels
    for (int l = 0; l < 2; ++l)
    {
        auto& L = layers[l];
        const auto c = l == 0 ? colours::layerA : colours::layerB;
        auto pb = L.bounds.toFloat();
        g.setColour (colours::panel.withAlpha (0.9f));
        g.fillRoundedRectangle (pb, 10.0f);
        g.setColour ((L.power.getToggleState() ? c : colours::outline).withAlpha (0.6f));
        g.drawRoundedRectangle (pb.reduced (0.5f), 10.0f, 1.2f);
        // accent strip
        g.setColour (c.withAlpha (L.power.getToggleState() ? 0.9f : 0.25f));
        g.fillRoundedRectangle (pb.getRight() - 120.0f, pb.getY() + 14.0f, 100.0f, 3.0f, 1.5f);

        for (auto& row : L.rows)
            for (auto& s : row)
                drawSection (g, s.title, s.colour, s.bounds);
    }

    for (auto& s : fxSections)
        drawSection (g, s.title, s.colour, s.bounds);
}

void NebulonEditor::timerCallback()
{
    scope.phase += 0.02f;
    scope.repaint();
    if (presetBox.getSelectedItemIndex() != proc.getCurrentProgram())
        presetBox.setSelectedItemIndex (proc.getCurrentProgram(), juce::dontSendNotification);

    // dim disabled layers
    for (int l = 0; l < 2; ++l)
    {
        const bool on = layers[l].power.getToggleState();
        for (auto& row : layers[l].rows)
            for (auto& s : row)
            {
                for (auto* k : s.knobs) k->slider.setAlpha (on ? 1.0f : 0.35f);
                if (s.lead) s.lead->setAlpha (on ? 1.0f : 0.35f);
            }
    }
    repaint (layers[0].bounds.getUnion (layers[1].bounds).withHeight (40));
}

//==============================================================================
void NebulonEditor::Scope::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (colours::bg.withAlpha (0.8f));
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);

    // grid
    g.setColour (colours::outline.withAlpha (0.4f));
    for (int i = 1; i < 8; ++i)
    {
        const float x = b.getX() + b.getWidth() * (float) i / 8.0f;
        g.drawVerticalLine ((int) x, b.getY() + 4, b.getBottom() - 4);
    }
    g.drawHorizontalLine ((int) b.getCentreY(), b.getX() + 4, b.getRight() - 4);

    // snapshot with rising-edge trigger for a stable display
    constexpr int N = NebulonProcessor::scopeSize;
    std::array<float, N> buf;
    const int w = proc.scopeWrite.load();
    for (int i = 0; i < N; ++i) buf[(size_t) i] = proc.scope[(size_t) ((w + i) % N)];

    const int view = 512;
    int startIdx = N - view - 1;
    for (int i = N - view - 2; i > N / 4; --i)
        if (buf[(size_t) i] <= 0.0f && buf[(size_t) i + 1] > 0.0f) { startIdx = i; break; }

    float peak = 0.0f;
    for (int i = 0; i < view; ++i) peak = juce::jmax (peak, std::abs (buf[(size_t) (startIdx + i)]));
    const float gain = peak > 0.0001f ? juce::jmin (4.0f, 0.9f / peak) : 1.0f;

    juce::Path path;
    const float midY = b.getCentreY(), amp = b.getHeight() * 0.42f;
    for (int i = 0; i < view; ++i)
    {
        const float x = b.getX() + 6 + (b.getWidth() - 12) * (float) i / (float) (view - 1);
        const float y = midY - buf[(size_t) (startIdx + i)] * gain * amp;
        if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
    }

    juce::ColourGradient grad (colours::layerA, b.getX(), 0, colours::layerB, b.getRight(), 0, false);
    g.setGradientFill (grad);
    g.setOpacity (0.25f);
    g.strokePath (path, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved));
    g.setOpacity (1.0f);
    g.strokePath (path, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));

    if (peak < 0.0005f)
    {
        g.setColour (colours::textDim.withAlpha (0.6f + 0.3f * std::sin (phase)));
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText ("play a note", getLocalBounds(), juce::Justification::centred);
    }
}
