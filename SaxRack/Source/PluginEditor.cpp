#include "PluginEditor.h"

using namespace rs::ui;

namespace
{
juce::String intervalName (int steps, bool diatonic, int scale)
{
    if (steps == 0) return "Unison";
    const juce::String dir = steps > 0 ? juce::String (juce::CharPointer_UTF8 (" \xe2\x86\x91")) : juce::String (juce::CharPointer_UTF8 (" \xe2\x86\x93"));
    const int a = std::abs (steps);
    if (diatonic)
    {
        if (rs::params::scaleSteps (scale).size() != 7) return (steps > 0 ? "+" : "-") + juce::String (a) + " steps";
        static const char* names[] = { "", "2nd", "3rd", "4th", "5th", "6th", "7th", "Oct", "9th", "10th", "11th", "12th", "13th" };
        return names[a] + dir;
    }
    static const char* chrom[] = { "", "m2", "M2", "m3", "M3", "P4", "Tritone", "P5", "m6", "M6", "m7", "M7", "Oct" };
    return chrom[a] + dir;
}
}

ReedstormContent::ReedstormContent (ReedstormProcessor& p) : proc (p)
{
    setLookAndFeel (&lnf);
    auto& s = proc.apvts;

    // ---- header ------------------------------------------------------------------------
    buildPresetMenu();
    presetBox.getProperties().set ("accent", (juce::int64) col::brass.getARGB());
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx != proc.getCurrentPresetIndex()) proc.loadPreset (idx);
        shownPreset = -1;
    };
    addAndMakeVisible (presetBox);
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    addAndMakeVisible (tuner);
    addAndMakeVisible (trace);
    instrument = std::make_unique<Combo> (s, "tuner_inst", "Instrument", col::brass);
    instrument->box.setTooltip ("Your saxophone: the tuner, pitch trace and harmony key use its written pitch");
    addAndMakeVisible (*instrument);
    refSlider.setSliderStyle (juce::Slider::IncDecButtons);
    refSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 70, 20);
    refSlider.setIncDecButtonsMode (juce::Slider::incDecButtonsDraggable_Vertical);
    refSlider.setTooltip ("Tuner reference pitch (A4)");
    addAndMakeVisible (refSlider);
    refAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, "tuner_ref", refSlider);

    inGain = std::make_unique<Knob> (s, "in_gain", "INPUT", col::brass, true);
    outGain = std::make_unique<Knob> (s, "out_gain", "OUTPUT", col::brass, true);
    width = std::make_unique<Knob> (s, "out_width", "WIDTH", col::brass);
    for (auto* c : { (juce::Component*) inGain.get(), (juce::Component*) outGain.get(), (juce::Component*) width.get(),
                     (juce::Component*) &inMeter, (juce::Component*) &outMeterL, (juce::Component*) &outMeterR })
        addAndMakeVisible (c);

    // ---- modules (row 0 = mono core, row 1 = stereo) -------------------------------------
    auto& dyn = addPanel ("DYNAMICS", "gate + opto comp", "dyn_on", col::dyn, 2, 0);
    dyn.knob ("gate_thr", "GATE");
    dyn.knob ("comp_amt", "SQUEEZE");
    dyn.knob ("comp_speed", "SPEED");
    dyn.wantsSpareCell = true;
    dyn.addAndMakeVisible (grMeter);
    dynPanel = &dyn;

    auto& oct = addPanel ("OCTAVER", "analog divider", "oct_on", col::oct, 2, 0);
    oct.knob ("oct_sub1", "SUB 1");
    oct.knob ("oct_sub2", "SUB 2");
    oct.knob ("oct_up", "OCT UP");
    oct.knob ("oct_dry", "DRY");

    auto& syn = addPanel ("SYNTH", "pitch + breath tracking", "syn_on", col::syn, 3, 0);
    syn.combo ("syn_wave", "Wave");
    syn.combo ("syn_oct", "Octave");
    syn.knob ("syn_cut", "CUTOFF");
    syn.knob ("syn_res", "RESO");
    syn.knob ("syn_env", "ENV");
    syn.knob ("syn_glide", "GLIDE");
    syn.knob ("syn_lvl", "LEVEL");

    auto& wah = addPanel ("ENV FILTER", "auto-wah", "wah_on", col::wah, 3, 0);
    wah.combo ("wah_mode", "Mode");
    wah.knob ("wah_sens", "SENS/PEDAL").setTooltip ("Envelope sensitivity. In Pedal mode this is the pedal position: automate it!");
    wah.knob ("wah_freq", "FREQ");
    wah.knob ("wah_range", "RANGE");
    wah.knob ("wah_q", "RESO");
    wah.knob ("wah_rate", "LFO RATE");
    wah.knob ("wah_mix", "MIX");
    wahPanel = &wah;

    auto& drv = addPanel ("DRIVE", "2x oversampled", "drv_on", col::drv, 3, 0);
    drv.combo ("drv_type", "Type");
    drv.knob ("drv_drive", "DRIVE");
    drv.knob ("drv_char", "CHARACTER").setTooltip ("Tube: asymmetry  |  Fuzz: octave-up blend  |  Growl: depth & speed  |  Tape: even harmonics");
    drv.knob ("drv_tone", "TONE");
    drv.knob ("drv_level", "LEVEL", true);
    drv.knob ("drv_mix", "MIX");

    auto& eq = addPanel ("EQ", "sax-voiced", "eq_on", col::eq, 3, 1);
    eqCurve = std::make_unique<EqCurve> (s);
    eq.setTopDisplay (eqCurve.get());
    eq.knob ("eq_lowcut", "LOW CUT");
    eq.knob ("eq_body", "BODY", true);
    eq.knob ("eq_mid", "HONK", true);
    eq.knob ("eq_midf", "HONK FREQ");
    eq.knob ("eq_air", "AIR", true);

    auto& har = addPanel ("HARMONY", "smart 2-voice", "har_on", col::har, 3, 1);
    har.combo ("har_mode", "Mode");
    har.combo ("har_key", "Key");
    har.combo ("har_scale", "Scale");
    auto& i1 = har.knob ("har_int1", "VOICE 1", true);
    auto& i2 = har.knob ("har_int2", "VOICE 2", true);
    for (auto* k : { &i1, &i2 })
    {
        const juce::String id = k == &i1 ? "har_int1" : "har_int2";
        k->valueText = [this, id]
        {
            auto& st = proc.apvts;
            return intervalName ((int) std::lround (st.getRawParameterValue (id)->load()),
                                 st.getRawParameterValue ("har_mode")->load() < 0.5f,
                                 (int) std::lround (st.getRawParameterValue ("har_scale")->load()));
        };
        k->setTooltip ("Interval of this harmony voice. Diatonic: scale steps in the key (written pitch for your sax). Chromatic: semitones.");
    }
    har.knob ("har_lvl1", "LEVEL 1");
    har.knob ("har_lvl2", "LEVEL 2");
    har.knob ("har_spread", "SPREAD");
    harPanel = &har;

    auto& mod = addPanel ("MODULATION", "", "mod_on", col::mod, 2, 1);
    mod.combo ("mod_type", "Type");
    mod.knob ("mod_rate", "RATE");
    mod.knob ("mod_depth", "DEPTH");
    mod.knob ("mod_fb", "FEEDBACK");
    mod.knob ("mod_mix", "MIX");

    auto& dly = addPanel ("DELAY", "tape echo", "dly_on", col::dly, 3, 1);
    dly.combo ("dly_sync", "Sync");
    dly.toggle ("dly_pp", "PING-PONG");
    delayTimeKnob = &dly.knob ("dly_time", "TIME");
    delayTimeKnob->valueText = [this]
    {
        auto& st = proc.apvts;
        const int sync = (int) std::lround (st.getRawParameterValue ("dly_sync")->load());
        if (sync > 0) return rs::params::delaySyncs[sync] + " (synced)";
        return st.getParameter ("dly_time")->getCurrentValueAsText();
    };
    dly.knob ("dly_fb", "FEEDBACK");
    dly.knob ("dly_tone", "TONE");
    dly.knob ("dly_wobble", "WOBBLE");
    dly.knob ("dly_duck", "DUCK").setTooltip ("Echoes duck under your playing and bloom in the gaps");
    dly.knob ("dly_mix", "MIX");

    auto& rev = addPanel ("REVERB", "FDN + shimmer", "rev_on", col::rev, 3, 1);
    rev.knob ("rev_pre", "PRE-DELAY");
    rev.knob ("rev_size", "SIZE");
    rev.knob ("rev_decay", "DECAY");
    rev.knob ("rev_damp", "DAMP");
    rev.knob ("rev_shimmer", "SHIMMER").setTooltip ("Feeds an octave-up copy back into the tail: angelic ambient pads");
    rev.knob ("rev_mix", "MIX");

    chain = std::make_unique<ChainStrip> (s, std::vector<ChainStrip::Node> {
        { "DYN", "dyn_on", col::dyn }, { "OCT", "oct_on", col::oct }, { "SYNTH", "syn_on", col::syn },
        { "WAH", "wah_on", col::wah }, { "DRIVE", "drv_on", col::drv }, { "EQ", "eq_on", col::eq },
        { "HARM", "har_on", col::har }, { "MOD", "mod_on", col::mod }, { "DELAY", "dly_on", col::dly },
        { "VERB", "rev_on", col::rev } });
    addAndMakeVisible (*chain);

    // brushed-metal noise texture
    texture = juce::Image (juce::Image::ARGB, 256, 256, true);
    juce::Random rng (1234);
    for (int y = 0; y < 256; ++y)
    {
        float streak = rng.nextFloat();
        for (int x = 0; x < 256; ++x)
        {
            streak = 0.92f * streak + 0.08f * rng.nextFloat();
            texture.setPixelAt (x, y, juce::Colours::white.withAlpha (0.012f + 0.03f * streak));
        }
    }

    for (auto& panel : panels) panel->refreshActive();
    setSize (designW, designH);
    startTimerHz (30);
}

ReedstormContent::~ReedstormContent()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

ModulePanel& ReedstormContent::addPanel (const juce::String& title, const juce::String& sub, const juce::String& powerId,
                                         juce::Colour c, int cols, int row)
{
    panels.push_back (std::make_unique<ModulePanel> (proc.apvts, title, sub, powerId, c, cols));
    panelRow.push_back (row);
    panelCols.push_back (cols);
    addAndMakeVisible (*panels.back());
    return *panels.back();
}

void ReedstormContent::buildPresetMenu()
{
    presetBox.clear (juce::dontSendNotification);
    const auto& presets = rs::factoryPresets();
    juce::String lastCat;
    for (size_t i = 0; i < presets.size(); ++i)
    {
        if (lastCat != presets[i].category)
        {
            lastCat = presets[i].category;
            presetBox.addSectionHeading (lastCat);
        }
        presetBox.addItem (presets[i].name, (int) i + 1);
    }
}

void ReedstormContent::stepPreset (int delta)
{
    const int n = (int) rs::factoryPresets().size();
    proc.loadPreset ((proc.getCurrentPresetIndex() + delta + n) % n);
}

void ReedstormContent::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.fillAll (col::bg);
    juce::ColourGradient glow (col::brass.withAlpha (0.10f), r.getWidth() * 0.15f, 0, col::bg.withAlpha (0.0f), r.getWidth() * 0.15f, 420, true);
    g.setGradientFill (glow);
    g.fillRect (r);
    g.setTiledImageFill (texture, 0, 0, 1.0f);
    g.fillRect (r);

    // header plate
    auto header = juce::Rectangle<float> (12, 10, r.getWidth() - 24, 150);
    juce::ColourGradient plate (juce::Colour (0xff1e1a15), 0, header.getY(), juce::Colour (0xff110f0c), 0, header.getBottom(), false);
    g.setGradientFill (plate);
    g.fillRoundedRectangle (header, 10.0f);
    g.setColour (col::brassDark.withAlpha (0.6f));
    g.drawRoundedRectangle (header.reduced (0.5f), 10.0f, 1.0f);

    // logo: a lightning-bolt reed + wordmark
    auto logo = juce::Rectangle<float> (30, 24, 290, 60);
    juce::Path bolt;
    const float bx = logo.getX(), by = logo.getY();
    bolt.startNewSubPath (bx + 20, by + 2);
    bolt.lineTo (bx + 6, by + 32);
    bolt.lineTo (bx + 17, by + 32);
    bolt.lineTo (bx + 10, by + 58);
    bolt.lineTo (bx + 32, by + 22);
    bolt.lineTo (bx + 20, by + 22);
    bolt.lineTo (bx + 28, by + 2);
    bolt.closeSubPath();
    g.setColour (col::brass.withAlpha (0.25f));
    g.strokePath (bolt, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::ColourGradient boltFill (juce::Colour (0xfffff0c0), bx, by, col::brass, bx, by + 58, false);
    g.setGradientFill (boltFill);
    g.fillPath (bolt);

    const auto word = font (34.0f, true).withExtraKerningFactor (0.14f);
    juce::GlyphArrangement ga;
    ga.addLineOfText (word, "REEDSTORM", logo.getX() + 44, logo.getY() + 38);
    juce::Path text;
    ga.createPath (text);
    juce::ColourGradient brass (juce::Colour (0xfffff3d0), 0, logo.getY() + 8, juce::Colour (0xffb8862f), 0, logo.getY() + 42, false);
    brass.addColour (0.55, col::brass);
    g.setGradientFill (brass);
    g.fillPath (text);
    g.setColour (col::dim);
    g.setFont (font (10.5f, true).withExtraKerningFactor (0.32f));
    g.drawText ("SAXOPHONE EFFECTS RACK", juce::Rectangle<float> (logo.getX() + 46, logo.getY() + 42, 260, 14), juce::Justification::centredLeft);

    // preset description
    g.setColour (col::dim.brighter (0.2f));
    g.setFont (font (11.5f));
    g.drawFittedText (presetDescription, juce::Rectangle<int> (30, 124, 300, 32), juce::Justification::topLeft, 2);

    // labels in the output section
    g.setColour (col::brass.withAlpha (0.8f));
    g.setFont (font (10.0f, true).withExtraKerningFactor (0.15f));
    g.drawText ("METERS", juce::Rectangle<int> (1106, 18, 70, 16), juce::Justification::centredLeft);
}

void ReedstormContent::resized()
{
    // header
    presetBox.setBounds (30, 92, 236, 28);
    prevButton.setBounds (270, 92, 26, 28);
    nextButton.setBounds (298, 92, 26, 28);
    tuner.setBounds (342, 18, 238, 134);
    trace.setBounds (590, 18, 400, 134);
    instrument->setBounds (1000, 22, 96, 40);
    refSlider.setBounds (1000, 72, 96, 22);
    inGain->setBounds (1000, 98, 96, 60);
    inMeter.setBounds (1108, 36, 16, 118);
    outMeterL.setBounds (1132, 36, 16, 118);
    outMeterR.setBounds (1150, 36, 16, 118);
    outGain->setBounds (1172, 26, 90, 64);
    width->setBounds (1172, 90, 90, 64);
    chain->setBounds (12, 164, getWidth() - 24, 36);

    // panels
    const int top = 206, bottom = getHeight() - 10, gap = 8;
    const int rowH = (bottom - top - gap) / 2;
    for (int row = 0; row < 2; ++row)
    {
        int totalCols = 0;
        for (size_t i = 0; i < panels.size(); ++i) if (panelRow[i] == row) totalCols += panelCols[i];
        int count = 0;
        for (size_t i = 0; i < panels.size(); ++i) if (panelRow[i] == row) ++count;
        const int avail = getWidth() - 24 - (count - 1) * gap;
        int x = 12;
        for (size_t i = 0; i < panels.size(); ++i)
        {
            if (panelRow[i] != row) continue;
            const int w = avail * panelCols[i] / totalCols;
            panels[i]->setBounds (x, top + row * (rowH + gap), w, rowH);
            x += w + gap;
        }
    }
    grMeter.setBounds (dynPanel->spareCell.withSizeKeepingCentre (18, juce::jmin (dynPanel->spareCell.getHeight(), 84)));
}

void ReedstormContent::timerCallback()
{
    auto& m = proc.meters();
    auto& s = proc.apvts;

    inMeter.push (m.inPeak.exchange (0.0f));
    outMeterL.push (m.outPeakL.exchange (0.0f));
    outMeterR.push (m.outPeakR.exchange (0.0f));
    grMeter.push (m.gainReduction.exchange (0.0f));

    const bool voiced = m.voiced.load();
    const int inst = (int) std::lround (s.getRawParameterValue ("tuner_inst")->load());
    const float concert = m.pitchMidi.load();
    tuner.set (voiced, concert, inst, s.getRawParameterValue ("tuner_ref")->load());

    const bool harmOn = s.getRawParameterValue ("har_on")->load() > 0.5f;
    trace.push (voiced, concert + (float) rs::params::writtenOffset (inst), harmOn,
                m.harmShift1.load(), m.harmShift2.load(),
                s.getRawParameterValue ("har_lvl1")->load() > 0.01f, s.getRawParameterValue ("har_lvl2")->load() > 0.01f);

    chain->refresh();
    eqCurve->update();
    for (auto& p : panels) p->refreshActive();
    wahPanel->setActivity (m.wahPos.load());
    delayTimeKnob->setAlpha (s.getRawParameterValue ("dly_sync")->load() > 0.5f ? 0.5f : 1.0f);
    if (! panels[8]->isOn()) delayTimeKnob->setAlpha (0.42f);

    const int cur = proc.getCurrentPresetIndex();
    if (cur != shownPreset)
    {
        shownPreset = cur;
        presetBox.setSelectedId (cur + 1, juce::dontSendNotification);
        presetDescription = rs::factoryPresets()[(size_t) cur].description;
        repaint (20, 120, 320, 40);
    }
}

//==============================================================================
ReedstormEditor::ReedstormEditor (ReedstormProcessor& p) : AudioProcessorEditor (p), content (p)
{
    addAndMakeVisible (content);
    setResizable (true, true);
    setResizeLimits (ReedstormContent::designW / 2, ReedstormContent::designH / 2,
                     ReedstormContent::designW * 2, ReedstormContent::designH * 2);
    getConstrainer()->setFixedAspectRatio ((double) ReedstormContent::designW / ReedstormContent::designH);
    setSize ((int) (ReedstormContent::designW * 0.9), (int) (ReedstormContent::designH * 0.9));
}

void ReedstormEditor::resized()
{
    const float scale = (float) getWidth() / (float) ReedstormContent::designW;
    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, ReedstormContent::designW, ReedstormContent::designH);
}
