#include "Pages.h"

namespace neb::ui
{
//==============================================================================
void LayerDisplay::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const auto c = colours::layer (layer);
    g.setColour (colours::bg.withAlpha (0.85f));
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);

    auto area = b.reduced (10.0f, 8.0f);
    auto titleArea = area.removeFromTop (16.0f);
    const int mode = (int) param ("mode");

    juce::String title = modeNames()[mode];
    if (mode == modeWavetable) title << "  /  " << wavetableNames()[(int) param ("wtTable")];
    if (mode == modeGranular || mode == modeSample)
    {
        const int srcIdx = (int) param ("src");
        auto name = srcIdx < (int) builtinSources().size() ? sourceNames()[srcIdx] : proc.getUserSampleName (layer);
        title << "  /  " << (name.isEmpty() ? juce::String ("(no sample loaded)") : name);
    }
    g.setColour (c);
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.drawText (title.toUpperCase(), titleArea, juce::Justification::centredLeft);

    g.setColour (colours::outline.withAlpha (0.5f));
    g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());

    juce::Path path;
    const int N = 300;
    auto plot = [&] (const std::function<float (double)>& fn)
    {
        for (int i = 0; i < N; ++i)
        {
            const double t = (double) i / (N - 1);
            const float x = area.getX() + area.getWidth() * (float) t;
            const float y = area.getCentreY() - fn (juce::jmin (t, 0.9999)) * area.getHeight() * 0.42f;
            if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
    };

    if (mode == modeGranular || mode == modeSample)
    {
        if (auto* sd = proc.getLayerSource (layer))
        {
            const auto& pk = sd->peaks;
            const float w = area.getWidth() / (float) pk.size();
            g.setColour (c.withAlpha (0.55f));
            for (size_t i = 0; i < pk.size(); ++i)
            {
                const float h = pk[i] * area.getHeight() * 0.48f;
                g.fillRect (area.getX() + (float) i * w, area.getCentreY() - h, juce::jmax (1.0f, w - 0.5f), h * 2);
            }
            if (mode == modeGranular)
            {
                const float pos = param ("grPos"), spray = param ("grSpray") * 0.5f;
                const float x0 = area.getX() + area.getWidth() * juce::jmax (0.0f, pos - spray);
                const float x1 = area.getX() + area.getWidth() * juce::jmin (1.0f, pos + spray);
                g.setColour (c.withAlpha (0.18f));
                g.fillRect (x0, area.getY(), juce::jmax (2.0f, x1 - x0), area.getHeight());
                g.setColour (colours::text);
                g.drawVerticalLine ((int) (area.getX() + area.getWidth() * pos), area.getY(), area.getBottom());
            }
            else
            {
                g.setColour (colours::text);
                g.drawVerticalLine ((int) (area.getX() + area.getWidth() * param ("smpStart")), area.getY(), area.getBottom());
            }
        }
        return;
    }

    if (mode == modeWavetable)
    {
        const auto& tables = wavetables();
        const auto& t = tables[(size_t) juce::jlimit (0, (int) tables.size() - 1, (int) param ("wtTable"))];
        const float pos = param ("wtPos");
        // ghost frames for a 3D feel
        for (int f = 0; f < 6; ++f)
        {
            juce::Path ghost;
            const float fp = (float) f / 5.0f;
            const float dx = (fp - 0.5f) * 30.0f, dy = (0.5f - fp) * 18.0f;
            for (int i = 0; i < 120; ++i)
            {
                const double ph = (double) i / 119.0 * 0.9999;
                const float x = area.getX() + area.getWidth() * 0.85f * (float) ph + area.getWidth() * 0.075f + dx;
                const float y = area.getCentreY() + dy - wtLookup (t, fp, ph, 0) * area.getHeight() * 0.25f;
                if (i == 0) ghost.startNewSubPath (x, y); else ghost.lineTo (x, y);
            }
            g.setColour (c.withAlpha (0.12f));
            g.strokePath (ghost, juce::PathStrokeType (1.0f));
        }
        plot ([&] (double ph) { return wtLookup (t, pos, ph, 0); });
    }
    else if (mode == modePluck)
    {
        const float tone = param ("plkTone"), decay = param ("plkDecay");
        juce::Random r (7);
        float lp = 0.0f;
        plot ([&] (double t)
        {
            const float env = std::exp (-(float) t * 4.0f / juce::jmax (0.2f, decay));
            const float raw = (float) std::sin (twoPi * t * 9.0) * 0.6f + (r.nextFloat() * 2.0f - 1.0f) * 0.4f;
            lp += (raw - lp) * (0.1f + 0.9f * tone);
            return lp * env;
        });
    }
    else if (mode == modeFM)
    {
        const double ratio = param ("fmRatio");
        const float index = param ("fmIndex");
        plot ([&] (double t) { return (float) std::sin (twoPi * t + index * std::sin (twoPi * t * ratio)); });
    }
    else
    {
        const float w = param ("wave");
        plot ([&] (double t) { return morphOsc (w, t, 1.0e-9) * 0.9f; });
    }

    g.setColour (c.withAlpha (0.25f));
    g.strokePath (path, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved));
    g.setColour (c);
    g.strokePath (path, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved));
}

//==============================================================================
MainPage::MainPage (NebulonProcessor& p, juce::Component* popup, std::function<void (int)> onEditLayer)
    : Page (p, popup), pad (p)
{
    addAndMakeVisible (pad);
    for (int l = 0; l < numLayers; ++l)
    {
        const auto pre = layerPrefix (l);
        const auto c = colours::layer (l);
        auto& e = editButtons[l];
        e.setButtonText ("EDIT");
        e.setColour (juce::TextButton::buttonOnColourId, c);
        e.onClick = [onEditLayer, l] { onEditLayer (l); };
        setWeight (e, 0.9f);
        e.getProperties().set ("maxH", 44);

        strips[l] = panel ("LAYER " + juce::String::charToString ((juce::juce_wchar) ('A' + l)), c);
        strips[l]->addRow ({ toggle (pre + "on", "ON", c, 0.9f), choice (pre + "mode", "Engine", 1.5f),
                             knob (pre + "level", "Level", c), knob (pre + "pan", "Pan", c),
                             knob (pre + "octave", "Octave", c), knob (pre + "cutoff", "Cutoff", c),
                             knob (pre + "reso", "Reso", c), knob (pre + "aA", "Attack", c),
                             knob (pre + "aR", "Release", c), &e });
    }

    macros = panel ("MACROS", colours::perf);
    auto* m1 = knob ("macro1", "TONE", colours::perf);
    auto* m2 = knob ("macro2", "MORPH", colours::perf);
    auto* m3 = knob ("macro3", "GRIT", colours::perf);
    auto* m4 = knob ("macro4", "SPREAD", colours::perf);
    for (auto* k : { m1, m2, m3, m4 }) k->slider.getProperties().set ("bipolar", true);
    macros->addRow ({ m1, m2, m3, m4 });

    perf = panel ("PERFORMANCE", colours::perf);
    perf->addRow ({ knob ("poly", "Voices", colours::perf), knob ("glide", "Glide", colours::perf),
                    knob ("bend", "Bend", colours::perf), knob ("master", "Master", colours::perf) });
    perf->addRow ({ toggle ("mono", "MONO", colours::perf), toggle ("legato", "LEGATO", colours::perf) });
}

void MainPage::resized()
{
    auto r = getLocalBounds();
    auto right = r.removeFromRight (330);
    r.removeFromRight (8);
    const int h = r.getHeight() / numLayers;
    for (int l = 0; l < numLayers; ++l)
        strips[l]->setBounds (r.removeFromTop (h).reduced (0, 3));
    pad.setBounds (right.removeFromTop (right.getHeight() * 46 / 100).reduced (0, 3));
    macros->setBounds (right.removeFromTop (right.getHeight() * 45 / 100).reduced (0, 3));
    perf->setBounds (right.reduced (0, 3));
}

//==============================================================================
LayerPage::LayerPage (NebulonProcessor& p, juce::Component* popup, int l)
    : Page (p, popup), layer (l), display (p, l)
{
    const auto pre = layerPrefix (l);
    const auto c = colours::layer (l);

    auto* mode = choice (pre + "mode", "Engine", 1.3f);
    auto* table = choice (pre + "wtTable", "Wavetable", 1.8f);
    auto* src = choice (pre + "src", "Source", 1.8f);
    loadButton.setColour (juce::TextButton::buttonOnColourId, c);
    setWeight (loadButton, 1.3f);
    loadButton.getProperties().set ("maxH", 30);
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load a sample into layer " + juce::String::charToString ((juce::juce_wchar) ('A' + layer)),
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile() && ! proc.loadUserSample (layer, f))
                                      juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                              "Nebulon", "Couldn't read that audio file.");
                              });
    };

    auto* wave = knob (pre + "wave", "Shape", c);
    auto* wtPos = knob (pre + "wtPos", "Position", c);
    auto* fmR = knob (pre + "fmRatio", "Ratio", c);
    auto* fmI = knob (pre + "fmIndex", "Amount", c);
    auto* fmF = knob (pre + "fmFb", "Feedback", c);
    auto* gS = knob (pre + "grSize", "Size", c);
    auto* gD = knob (pre + "grDensity", "Density", c);
    auto* gP = knob (pre + "grPos", "Position", c);
    auto* gY = knob (pre + "grSpray", "Spray", c);
    auto* gR = knob (pre + "grPitch", "Pitch Rnd", c);
    auto* sS = knob (pre + "smpStart", "Start", c);
    auto* sL = toggle (pre + "smpLoop", "LOOP", c);
    auto* pT = knob (pre + "plkTone", "Brightness", c);
    auto* pD = knob (pre + "plkDecay", "Decay", c);
    auto* pB = knob (pre + "plkBow", "Bow", c);

    analogCtl = { wave };
    wtCtl = { table, wtPos };
    fmCtl = { fmR, fmI, fmF };
    granCtl = { gS, gD, gP, gY, gR };
    smpCtl = { sS, sL };
    plkCtl = { pT, pD, pB };
    sourceCtl = { src, &loadButton };

    osc = panel ("OSCILLATOR", c);
    osc->addRow ({ mode, table, src, &loadButton });
    osc->addRow ({ wave, wtPos, fmR, fmI, fmF, gS, gD, gP, gY, gR, sS, sL, pT, pD, pB });

    addAndMakeVisible (display);

    pitch = panel ("PITCH & UNISON", c);
    pitch->addRow ({ knob (pre + "octave", "Octave", c), knob (pre + "semi", "Semi", c),
                     knob (pre + "fine", "Fine", c), knob (pre + "unison", "Unison", c) });
    pitch->addRow ({ knob (pre + "detune", "Detune", c), knob (pre + "spread", "Width", c),
                     knob (pre + "harm1", "Harmony 1", c), knob (pre + "harm2", "Harmony 2", c) });

    texture = panel ("TEXTURE", c);
    texture->addRow ({ knob (pre + "sub", "Sub", c), knob (pre + "noise", "Noise", c), knob (pre + "drift", "Drift", c),
                       knob (pre + "drive", "Drive", c), knob (pre + "pitchEnv", "Pitch Env", c), knob (pre + "pitchDecay", "P.Env Time", c) });

    filter = panel ("FILTER", c);
    filter->addRow ({ choice (pre + "ftype", "Type"), knob (pre + "cutoff", "Cutoff", c), knob (pre + "reso", "Reso", c),
                      knob (pre + "fenv", "Env Amt", c), knob (pre + "keytrack", "Key", c) });

    output = panel ("OUTPUT", c);
    output->addRow ({ toggle (pre + "on", "LAYER ON", c, 1.5f), knob (pre + "level", "Level", c),
                      knob (pre + "pan", "Pan", c), knob (pre + "velSens", "Vel Sens", c) });

    ampEnv = panel ("AMP ENVELOPE", c);
    ampEnv->addRow ({ knob (pre + "aA", "Attack", c), knob (pre + "aD", "Decay", c),
                      knob (pre + "aS", "Sustain", c), knob (pre + "aR", "Release", c) });

    filtEnv = panel ("FILTER ENVELOPE", c);
    filtEnv->addRow ({ knob (pre + "fA", "Attack", c), knob (pre + "fD", "Decay", c),
                       knob (pre + "fS", "Sustain", c), knob (pre + "fR", "Release", c) });

    refresh();
}

void LayerPage::refresh()
{
    const int mode = (int) proc.apvts.getRawParameterValue (layerPrefix (layer) + "mode")->load();
    if (mode != lastMode)
    {
        lastMode = mode;
        auto show = [] (std::vector<juce::Component*>& v, bool b) { for (auto* c : v) c->setVisible (b); };
        show (analogCtl, mode == modeAnalog);
        show (wtCtl, mode == modeWavetable);
        show (fmCtl, mode == modeFM);
        show (granCtl, mode == modeGranular);
        show (smpCtl, mode == modeSample);
        show (plkCtl, mode == modePluck);
        show (sourceCtl, mode == modeGranular || mode == modeSample);
        osc->resized();
    }
    display.repaint();
}

void LayerPage::resized()
{
    auto r = getLocalBounds();
    const int gap = 6;
    auto top = r.removeFromTop (r.getHeight() * 42 / 100);
    r.removeFromTop (gap);
    auto mid = r.removeFromTop ((r.getHeight() - gap) / 2);
    r.removeFromTop (gap);
    auto bottom = r;

    osc->setBounds (top.removeFromLeft (top.getWidth() * 46 / 100));
    top.removeFromLeft (gap);
    display.setBounds (top.removeFromLeft (top.getWidth() * 45 / 100));
    top.removeFromLeft (gap);
    pitch->setBounds (top);

    texture->setBounds (mid.removeFromLeft (mid.getWidth() * 38 / 100));
    mid.removeFromLeft (gap);
    filter->setBounds (mid.removeFromLeft (mid.getWidth() * 52 / 100));
    mid.removeFromLeft (gap);
    output->setBounds (mid);

    ampEnv->setBounds (bottom.removeFromLeft ((bottom.getWidth() - gap) / 2));
    bottom.removeFromLeft (gap);
    filtEnv->setBounds (bottom);
}

//==============================================================================
MatrixRow::MatrixRow (NebulonProcessor& p, int s, juce::Component* popup) : slot (s)
{
    const juce::String pre = "m" + juce::String (s + 1) + "_";
    src.addItemList (modSrcNames(), 1);
    dst.addItemList (modDstNames(), 1);
    lay.addItemList (modLayerNames(), 1);
    srcAtt = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, pre + "src", src);
    dstAtt = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, pre + "dst", dst);
    layAtt = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, pre + "layer", lay);
    amount.setSliderStyle (juce::Slider::LinearHorizontal);
    amount.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    amount.setPopupDisplayEnabled (true, true, popup);
    amount.setColour (juce::Slider::trackColourId, colours::mod);
    amount.setDoubleClickReturnValue (true, 0.0);
    amtAtt = std::make_unique<APVTS::SliderAttachment> (p.apvts, pre + "amt", amount);
    for (auto* c : { (juce::Component*) &src, (juce::Component*) &dst, (juce::Component*) &lay, (juce::Component*) &amount })
        addAndMakeVisible (c);
}

void MatrixRow::paint (juce::Graphics& g)
{
    g.setColour (colours::textDim);
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.drawText (juce::String (slot + 1), getLocalBounds().removeFromLeft (22), juce::Justification::centred);
}

void MatrixRow::resized()
{
    auto r = getLocalBounds().withSizeKeepingCentre (getWidth(), juce::jmin (getHeight(), 28));
    r.removeFromLeft (24);
    const int w = r.getWidth();
    src.setBounds (r.removeFromLeft (w * 27 / 100).reduced (2, 0));
    dst.setBounds (r.removeFromLeft (w * 29 / 100).reduced (2, 0));
    lay.setBounds (r.removeFromLeft (w * 18 / 100).reduced (2, 0));
    amount.setBounds (r.reduced (6, 0));
}

ModPage::ModPage (NebulonProcessor& p, juce::Component* popup) : Page (p, popup)
{
    const auto c = colours::mod;
    for (int i = 0; i < numLfos; ++i)
    {
        const juce::String pre = "lfo" + juce::String (i + 1) + "_";
        lfos[i] = panel ("LFO " + juce::String (i + 1), c);
        lfos[i]->addRow ({ choice (pre + "shape", "Shape"), choice (pre + "div", "Sync Rate") });
        lfos[i]->addRow ({ knob (pre + "rate", "Rate", c), knob (pre + "fade", "Fade In", c) });
        lfos[i]->addRow ({ toggle (pre + "sync", "SYNC", c), toggle (pre + "retrig", "RETRIG", c) });
    }
    for (int e = 0; e < numModEnvs; ++e)
    {
        const juce::String pre = "me" + juce::String (e + 1) + "_";
        envs[e] = panel ("MOD ENVELOPE " + juce::String (e + 1), c);
        envs[e]->addRow ({ knob (pre + "A", "Attack", c), knob (pre + "D", "Decay", c),
                           knob (pre + "S", "Sustain", c), knob (pre + "R", "Release", c) });
    }
    matrix = panel ("MOD MATRIX   -   source  >  destination  >  layer  >  amount     (slots 9-12 drive the macros)", c);
    std::vector<MatrixRow*> rows;
    for (int s = 0; s < numSlots; ++s)
        rows.push_back (own (std::make_unique<MatrixRow> (p, s, popup)));
    for (int r = 0; r < numSlots / 2; ++r)
        matrix->addRow ({ rows[(size_t) r], rows[(size_t) (r + numSlots / 2)] });
}

void ModPage::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (r.getHeight() * 44 / 100);
    r.removeFromTop (6);
    const int w = top.getWidth() / numLfos;
    for (int i = 0; i < numLfos; ++i) lfos[i]->setBounds (top.removeFromLeft (w).reduced (3, 0));
    auto left = r.removeFromLeft (360);
    r.removeFromLeft (6);
    envs[0]->setBounds (left.removeFromTop (left.getHeight() / 2).reduced (3));
    envs[1]->setBounds (left.reduced (3));
    matrix->setBounds (r.reduced (3));
}

//==============================================================================
void StepDisplay::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (colours::panelHi.withAlpha (0.55f));
    g.fillRoundedRectangle (b, 7.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (b.reduced (0.5f), 7.0f, 1.0f);

    const bool on = proc.apvts.getRawParameterValue ("arpOn")->load() > 0.5f;
    const int step = proc.arp.displayStep.load();
    auto area = b.reduced (16.0f, 20.0f);
    const float w = area.getWidth() / 16.0f;
    for (int i = 0; i < 16; ++i)
    {
        auto cell = juce::Rectangle<float> (area.getX() + w * (float) i, area.getY(), w, area.getHeight()).reduced (4.0f, 0.0f);
        const bool lit = on && i == step;
        g.setColour (lit ? colours::layerA : (i % 4 == 0 ? colours::outline.brighter (0.2f) : colours::outline));
        g.fillRoundedRectangle (cell, 5.0f);
        if (lit)
        {
            g.setColour (colours::layerA.withAlpha (0.3f));
            g.fillRoundedRectangle (cell.expanded (3.0f), 7.0f);
        }
    }
    g.setColour (colours::textDim);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText (on ? "Hold a chord - the arpeggiator follows your DAW tempo (120 BPM when standalone)"
                   : "Arpeggiator is off - switch it on above",
                getLocalBounds().removeFromBottom (18), juce::Justification::centred);
}

ArpPage::ArpPage (NebulonProcessor& p, juce::Component* popup) : Page (p, popup), steps (p)
{
    const auto c = colours::layerA;
    arpPanel = panel ("ARPEGGIATOR", c);
    arpPanel->addRow ({ toggle ("arpOn", "ARP ON", c), toggle ("arpLatch", "LATCH", c),
                        choice ("arpMode", "Mode"), choice ("arpDiv", "Rate") });
    arpPanel->addRow ({ knob ("arpOct", "Octaves", c), knob ("arpGate", "Gate", c), knob ("arpSwing", "Swing", c),
                        knob ("glide", "Glide", colours::perf) });
    addAndMakeVisible (steps);
}

void ArpPage::resized()
{
    auto r = getLocalBounds().withSizeKeepingCentre (juce::jmin (900, getWidth()), getHeight());
    arpPanel->setBounds (r.removeFromTop (r.getHeight() * 58 / 100).reduced (0, 10));
    r.removeFromTop (6);
    steps.setBounds (r.reduced (0, 10));
}

//==============================================================================
FxPage::FxPage (NebulonProcessor& p, juce::Component* popup) : Page (p, popup)
{
    const auto c = colours::fx;
    dist = panel ("DISTORTION", c);
    dist->addRow ({ toggle ("distOn", "ON", c), choice ("distType", "Type") });
    dist->addRow ({ knob ("distDrive", "Drive", c), knob ("distTone", "Tone", c), knob ("distMix", "Mix", c) });

    eq = panel ("EQ  &  OUTPUT", c);
    eq->addRow ({ knob ("eqLow", "Low", c), knob ("eqHigh", "High", c) });
    eq->addRow ({ knob ("master", "Master", c) });

    phaser = panel ("PHASER", c);
    phaser->addRow ({ toggle ("phOn", "ON", c) });
    phaser->addRow ({ knob ("phRate", "Rate", c), knob ("phDepth", "Depth", c), knob ("phFb", "Feedback", c), knob ("phMix", "Mix", c) });

    chorus = panel ("CHORUS", c);
    chorus->addRow ({ toggle ("chOn", "ON", c) });
    chorus->addRow ({ knob ("chorusRate", "Rate", c), knob ("chorusDepth", "Depth", c), knob ("chorusMix", "Mix", c) });

    delay = panel ("DELAY", c);
    delay->addRow ({ toggle ("dlOn", "ON", c), toggle ("delaySync", "SYNC", c), toggle ("delayPing", "PING-PONG", c, 1.5f) });
    delay->addRow ({ knob ("delayTime", "Time", c), choice ("delayDiv", "Sync Time", 1.2f), knob ("delayFb", "Feedback", c), knob ("delayMix", "Mix", c) });

    reverb = panel ("REVERB", c);
    reverb->addRow ({ toggle ("rvOn", "ON", c) });
    reverb->addRow ({ knob ("revSize", "Size", c), knob ("revDamp", "Damping", c), knob ("revWidth", "Width", c), knob ("revMix", "Mix", c) });
}

void FxPage::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop ((r.getHeight() - 6) / 2);
    r.removeFromTop (6);
    const int w = top.getWidth();
    dist->setBounds (top.removeFromLeft (w * 34 / 100).reduced (3, 0));
    phaser->setBounds (top.removeFromLeft (w * 40 / 100).reduced (3, 0));
    eq->setBounds (top.reduced (3, 0));
    chorus->setBounds (r.removeFromLeft (w * 28 / 100).reduced (3, 0));
    delay->setBounds (r.removeFromLeft (w * 38 / 100).reduced (3, 0));
    reverb->setBounds (r.reduced (3, 0));
}
} // namespace neb::ui
