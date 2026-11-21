#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using APVTS = juce::AudioProcessorValueTreeState;
using namespace neb;

namespace
{
juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
{
    juce::NormalisableRange<float> r (lo, hi);
    r.setSkewForCentre (centre);
    return r;
}

juce::AudioParameterFloatAttributes withText (std::function<juce::String (float, int)> f)
{
    return juce::AudioParameterFloatAttributes().withStringFromValueFunction (std::move (f));
}

juce::String secs (float v, int) { return v < 1.0f ? juce::String (v * 1000.0f, 0) + " ms" : juce::String (v, 2) + " s"; }
juce::String pct  (float v, int) { return juce::String (v * 100.0f, 0) + "%"; }
juce::String hz   (float v, int) { return v < 1000.0f ? juce::String (v, v < 10.0f ? 2 : 0) + " Hz" : juce::String (v / 1000.0f, 2) + " kHz"; }
juce::String cents (float v, int) { return juce::String (v, 0) + " ct"; }
juce::String db   (float v, int) { return juce::String (v, 1) + " dB"; }

juce::String waveName (float v, int)
{
    static const char* names[] = { "Sine", "Triangle", "Saw", "Square" };
    const int i = juce::jlimit (0, 3, (int) v);
    const float fr = v - (float) i;
    if (fr < 0.05f || i == 3) return names[i];
    if (fr > 0.95f) return names[i + 1];
    return juce::String (names[i]) + " > " + names[i + 1];
}

using F = juce::AudioParameterFloat;
using I = juce::AudioParameterInt;
using B = juce::AudioParameterBool;
using C = juce::AudioParameterChoice;

void addLayer (APVTS::ParameterLayout& layout, int index)
{
    const juce::String p = layerPrefix (index);
    auto id = [&] (const char* s) { return juce::ParameterID { p + s, 1 }; };
    const juce::String L = "Layer " + p.substring (0, 1) + " ";
    const auto unit = juce::NormalisableRange<float> (0, 1);
    const auto t = skewed (0.001f, 10.0f, 1.0f);

    layout.add (std::make_unique<B> (id ("on"), L + "On", index == 0));
    layout.add (std::make_unique<F> (id ("level"), L + "Level", unit, 0.8f, withText (pct)));
    layout.add (std::make_unique<F> (id ("pan"), L + "Pan", juce::NormalisableRange<float> (-1, 1), 0.0f));
    layout.add (std::make_unique<C> (id ("mode"), L + "Engine", modeNames(), 0));
    layout.add (std::make_unique<I> (id ("octave"), L + "Octave", -3, 3, 0));
    layout.add (std::make_unique<I> (id ("semi"), L + "Semitone", -12, 12, 0));
    layout.add (std::make_unique<F> (id ("fine"), L + "Fine", juce::NormalisableRange<float> (-100, 100), 0.0f, withText (cents)));

    layout.add (std::make_unique<F> (id ("wave"), L + "Analog Shape", juce::NormalisableRange<float> (0, 3), 2.0f, withText (waveName)));
    layout.add (std::make_unique<C> (id ("wtTable"), L + "Wavetable", wavetableNames(), 0));
    layout.add (std::make_unique<F> (id ("wtPos"), L + "WT Position", unit, 0.0f, withText (pct)));
    layout.add (std::make_unique<F> (id ("fmRatio"), L + "FM Ratio", juce::NormalisableRange<float> (0.5f, 12.0f, 0.5f), 2.0f,
                                     withText ([] (float v, int) { return juce::String (v, 1) + "x"; })));
    layout.add (std::make_unique<F> (id ("fmIndex"), L + "FM Amount", juce::NormalisableRange<float> (0, 10), 2.0f,
                                     withText ([] (float v, int) { return juce::String (v, 2); })));
    layout.add (std::make_unique<F> (id ("fmFb"), L + "FM Feedback", unit, 0.0f, withText (pct)));

    layout.add (std::make_unique<C> (id ("src"), L + "Source", sourceNames(), 0));
    layout.add (std::make_unique<F> (id ("grSize"), L + "Grain Size", skewed (5.0f, 1000.0f, 120.0f), 120.0f,
                                     withText ([] (float v, int) { return juce::String (v, 0) + " ms"; })));
    layout.add (std::make_unique<F> (id ("grDensity"), L + "Grain Density", skewed (1.0f, 100.0f, 20.0f), 20.0f,
                                     withText ([] (float v, int) { return juce::String (v, 1) + " /s"; })));
    layout.add (std::make_unique<F> (id ("grPos"), L + "Grain Position", unit, 0.3f, withText (pct)));
    layout.add (std::make_unique<F> (id ("grSpray"), L + "Grain Spray", unit, 0.2f, withText (pct)));
    layout.add (std::make_unique<F> (id ("grPitch"), L + "Grain Pitch Rand", unit, 0.0f, withText (pct)));
    layout.add (std::make_unique<F> (id ("smpStart"), L + "Sample Start", unit, 0.0f, withText (pct)));
    layout.add (std::make_unique<B> (id ("smpLoop"), L + "Sample Loop", true));

    layout.add (std::make_unique<I> (id ("unison"), L + "Unison", 1, 7, 3));
    layout.add (std::make_unique<F> (id ("detune"), L + "Detune", juce::NormalisableRange<float> (0, 100), 12.0f, withText (cents)));
    layout.add (std::make_unique<F> (id ("spread"), L + "Stereo Width", unit, 0.7f, withText (pct)));
    layout.add (std::make_unique<I> (id ("harm1"), L + "Harmony 1", -24, 24, 0));
    layout.add (std::make_unique<I> (id ("harm2"), L + "Harmony 2", -24, 24, 0));

    layout.add (std::make_unique<F> (id ("sub"), L + "Sub", unit, 0.0f, withText (pct)));
    layout.add (std::make_unique<F> (id ("noise"), L + "Noise", unit, 0.0f, withText (pct)));
    layout.add (std::make_unique<F> (id ("drift"), L + "Analog Drift", unit, 0.3f, withText (pct)));
    layout.add (std::make_unique<F> (id ("drive"), L + "Drive", unit, 0.1f, withText (pct)));

    layout.add (std::make_unique<C> (id ("ftype"), L + "Filter Type", filterNames(), 1));
    layout.add (std::make_unique<F> (id ("cutoff"), L + "Cutoff", skewed (20, 20000, 1000), 5000.0f, withText (hz)));
    layout.add (std::make_unique<F> (id ("reso"), L + "Resonance", unit, 0.2f, withText (pct)));
    layout.add (std::make_unique<F> (id ("fenv"), L + "Filter Env Amt", juce::NormalisableRange<float> (-1, 1), 0.2f, withText (pct)));
    layout.add (std::make_unique<F> (id ("keytrack"), L + "Key Track", unit, 0.5f, withText (pct)));

    layout.add (std::make_unique<F> (id ("aA"), L + "Amp Attack", t, 0.01f, withText (secs)));
    layout.add (std::make_unique<F> (id ("aD"), L + "Amp Decay", t, 0.4f, withText (secs)));
    layout.add (std::make_unique<F> (id ("aS"), L + "Amp Sustain", unit, 0.8f, withText (pct)));
    layout.add (std::make_unique<F> (id ("aR"), L + "Amp Release", t, 0.6f, withText (secs)));
    layout.add (std::make_unique<F> (id ("fA"), L + "Filter Attack", t, 0.01f, withText (secs)));
    layout.add (std::make_unique<F> (id ("fD"), L + "Filter Decay", t, 0.8f, withText (secs)));
    layout.add (std::make_unique<F> (id ("fS"), L + "Filter Sustain", unit, 0.5f, withText (pct)));
    layout.add (std::make_unique<F> (id ("fR"), L + "Filter Release", t, 0.8f, withText (secs)));
    layout.add (std::make_unique<F> (id ("velSens"), L + "Velocity Sens", unit, 0.6f, withText (pct)));
}
} // namespace

APVTS::ParameterLayout NebulonProcessor::createLayout()
{
    APVTS::ParameterLayout layout;
    for (int l = 0; l < numLayers; ++l) addLayer (layout, l);

    auto id = [] (const juce::String& s) { return juce::ParameterID { s, 1 }; };
    const auto unit = juce::NormalisableRange<float> (0, 1);
    const auto t = skewed (0.001f, 10.0f, 1.0f);

    // performance
    layout.add (std::make_unique<I> (id ("poly"), "Polyphony", 1, 16, 16));
    layout.add (std::make_unique<B> (id ("mono"), "Mono", false));
    layout.add (std::make_unique<B> (id ("legato"), "Legato", true));
    layout.add (std::make_unique<F> (id ("glide"), "Glide", skewed (0.0f, 2.0f, 0.3f), 0.0f, withText (secs)));
    layout.add (std::make_unique<I> (id ("bend"), "Bend Range", 1, 24, 2));
    for (int i = 1; i <= numMacros; ++i)
        layout.add (std::make_unique<F> (id ("macro" + juce::String (i)), "Macro " + juce::String (i), unit, 0.0f, withText (pct)));

    // modulation
    for (int i = 1; i <= numLfos; ++i)
    {
        const juce::String p = "lfo" + juce::String (i) + "_", n = "LFO " + juce::String (i) + " ";
        layout.add (std::make_unique<F> (id (p + "rate"), n + "Rate", skewed (0.01f, 30.0f, 2.0f), 1.0f, withText (hz)));
        layout.add (std::make_unique<C> (id (p + "shape"), n + "Shape", lfoShapeNames(), 0));
        layout.add (std::make_unique<B> (id (p + "sync"), n + "Sync", false));
        layout.add (std::make_unique<C> (id (p + "div"), n + "Division", divisionNames(), 6));
        layout.add (std::make_unique<F> (id (p + "fade"), n + "Fade In", skewed (0.0f, 10.0f, 1.0f), 0.0f, withText (secs)));
        layout.add (std::make_unique<B> (id (p + "retrig"), n + "Retrigger", true));
    }
    for (int e = 1; e <= numModEnvs; ++e)
    {
        const juce::String p = "me" + juce::String (e) + "_", n = "Mod Env " + juce::String (e) + " ";
        layout.add (std::make_unique<F> (id (p + "A"), n + "Attack", t, 0.5f, withText (secs)));
        layout.add (std::make_unique<F> (id (p + "D"), n + "Decay", t, 1.0f, withText (secs)));
        layout.add (std::make_unique<F> (id (p + "S"), n + "Sustain", unit, 0.5f, withText (pct)));
        layout.add (std::make_unique<F> (id (p + "R"), n + "Release", t, 1.0f, withText (secs)));
    }
    for (int i = 1; i <= numSlots; ++i)
    {
        const juce::String p = "m" + juce::String (i) + "_", n = "Mod " + juce::String (i) + " ";
        // slots 9-12 come pre-wired to the four macros: Tone, Morph, Grit, Spread
        static const int macroDst[4] = { dstCutoff, dstTimbre, dstDrive, dstDetune };
        static const float macroAmt[4] = { 0.5f, 0.5f, 0.7f, 0.4f };
        const int mi = i - 9;
        const bool isMacro = mi >= 0 && mi < 4;
        layout.add (std::make_unique<C> (id (p + "src"), n + "Source", modSrcNames(), isMacro ? srcMacro1 + mi : 0));
        layout.add (std::make_unique<C> (id (p + "dst"), n + "Destination", modDstNames(), isMacro ? macroDst[mi] : 0));
        layout.add (std::make_unique<C> (id (p + "layer"), n + "Layer", modLayerNames(), 0));
        layout.add (std::make_unique<F> (id (p + "amt"), n + "Amount", juce::NormalisableRange<float> (-1, 1),
                                         isMacro ? macroAmt[mi] : 0.0f, withText (pct)));
    }

    // arpeggiator
    layout.add (std::make_unique<B> (id ("arpOn"), "Arp On", false));
    layout.add (std::make_unique<C> (id ("arpMode"), "Arp Mode", arpModeNames(), 0));
    layout.add (std::make_unique<C> (id ("arpDiv"), "Arp Rate", divisionNames(), 12));
    layout.add (std::make_unique<I> (id ("arpOct"), "Arp Octaves", 1, 4, 1));
    layout.add (std::make_unique<F> (id ("arpGate"), "Arp Gate", juce::NormalisableRange<float> (0.05f, 1.0f), 0.7f, withText (pct)));
    layout.add (std::make_unique<F> (id ("arpSwing"), "Arp Swing", unit, 0.0f, withText (pct)));
    layout.add (std::make_unique<B> (id ("arpLatch"), "Arp Latch", false));

    // effects
    layout.add (std::make_unique<B> (id ("distOn"), "Distortion On", false));
    layout.add (std::make_unique<C> (id ("distType"), "Distortion Type", distTypeNames(), 0));
    layout.add (std::make_unique<F> (id ("distDrive"), "Distortion Drive", unit, 0.3f, withText (pct)));
    layout.add (std::make_unique<F> (id ("distTone"), "Distortion Tone", unit, 0.6f, withText (pct)));
    layout.add (std::make_unique<F> (id ("distMix"), "Distortion Mix", unit, 0.5f, withText (pct)));
    layout.add (std::make_unique<F> (id ("eqLow"), "EQ Low", juce::NormalisableRange<float> (-12, 12), 0.0f, withText (db)));
    layout.add (std::make_unique<F> (id ("eqHigh"), "EQ High", juce::NormalisableRange<float> (-12, 12), 0.0f, withText (db)));
    layout.add (std::make_unique<B> (id ("phOn"), "Phaser On", false));
    layout.add (std::make_unique<F> (id ("phRate"), "Phaser Rate", skewed (0.02f, 5.0f, 0.5f), 0.3f, withText (hz)));
    layout.add (std::make_unique<F> (id ("phDepth"), "Phaser Depth", unit, 0.6f, withText (pct)));
    layout.add (std::make_unique<F> (id ("phFb"), "Phaser Feedback", juce::NormalisableRange<float> (-0.9f, 0.9f), 0.4f, withText (pct)));
    layout.add (std::make_unique<F> (id ("phMix"), "Phaser Mix", unit, 0.5f, withText (pct)));
    layout.add (std::make_unique<B> (id ("chOn"), "Chorus On", true));
    layout.add (std::make_unique<F> (id ("chorusRate"), "Chorus Rate", skewed (0.05f, 5.0f, 0.8f), 0.4f, withText (hz)));
    layout.add (std::make_unique<F> (id ("chorusDepth"), "Chorus Depth", unit, 0.4f, withText (pct)));
    layout.add (std::make_unique<F> (id ("chorusMix"), "Chorus Mix", unit, 0.3f, withText (pct)));
    layout.add (std::make_unique<B> (id ("dlOn"), "Delay On", true));
    layout.add (std::make_unique<F> (id ("delayTime"), "Delay Time", skewed (10.0f, 2000.0f, 400.0f), 375.0f,
                                     withText ([] (float v, int) { return juce::String (v, 0) + " ms"; })));
    layout.add (std::make_unique<B> (id ("delaySync"), "Delay Sync", true));
    layout.add (std::make_unique<C> (id ("delayDiv"), "Delay Division", divisionNames(), 8));
    layout.add (std::make_unique<F> (id ("delayFb"), "Delay Feedback", juce::NormalisableRange<float> (0, 0.95f), 0.35f, withText (pct)));
    layout.add (std::make_unique<F> (id ("delayMix"), "Delay Mix", unit, 0.2f, withText (pct)));
    layout.add (std::make_unique<B> (id ("delayPing"), "Delay Ping-Pong", true));
    layout.add (std::make_unique<B> (id ("rvOn"), "Reverb On", true));
    layout.add (std::make_unique<F> (id ("revSize"), "Reverb Size", unit, 0.75f, withText (pct)));
    layout.add (std::make_unique<F> (id ("revDamp"), "Reverb Damping", unit, 0.4f, withText (pct)));
    layout.add (std::make_unique<F> (id ("revWidth"), "Reverb Width", unit, 1.0f, withText (pct)));
    layout.add (std::make_unique<F> (id ("revMix"), "Reverb Mix", unit, 0.3f, withText (pct)));
    layout.add (std::make_unique<F> (id ("master"), "Master", unit, 0.7f, withText (pct)));
    return layout;
}

//==============================================================================
NebulonProcessor::NebulonProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NEBULON", createLayout())
{
    formatManager.registerBasicFormats();
    for (int l = 0; l < numLayers; ++l) shared.layer[l].bind (apvts, layerPrefix (l));
    shared.global.bind (apvts);
    fx.bind (apvts);

    // build tables up-front so the first note doesn't stall
    wavetables();
    builtinSources();

    for (int i = 0; i < 16; ++i)
        synth.addVoice (new Voice (shared));
    synth.addSound (new Sound());

    setCurrentProgram (1);
}

bool NebulonProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void NebulonProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    synth.setCurrentPlaybackSampleRate (sampleRate);
    fx.prepare (sampleRate, samplesPerBlock);
    arp.prepare (sampleRate);
}

void NebulonProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int n = buffer.getNumSamples();

    // tempo / transport
    double bpm = 120.0, ppq = 0.0;
    bool hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) ppq = *p;
            hostPlaying = pos->getIsPlaying() && pos->getPpqPosition().hasValue();
        }
    shared.bpm.store ((float) bpm);

    // free-running LFO phases (used by LFOs with retrigger off)
    for (int k = 0; k < numLfos; ++k)
    {
        double ph = shared.lfoPhase[k].load() + shared.lfoHz (k) * n / sr;
        shared.lfoPhase[k].store (ph - std::floor (ph));
    }

    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        if (msg.isControllerOfType (1)) shared.modWheel.store (msg.getControllerValue() / 127.0f);
        else if (msg.isChannelPressure()) shared.aftertouch.store (msg.getChannelPressureValue() / 127.0f);
        else if (msg.isAftertouch()) shared.aftertouch.store (msg.getAfterTouchValue() / 127.0f);
    }

    Arpeggiator::Settings as;
    as.on = apvts.getRawParameterValue ("arpOn")->load() > 0.5f;
    as.mode = (int) apvts.getRawParameterValue ("arpMode")->load();
    as.stepBeats = divisionBeats ((int) apvts.getRawParameterValue ("arpDiv")->load());
    as.octaves = (int) apvts.getRawParameterValue ("arpOct")->load();
    as.gate = apvts.getRawParameterValue ("arpGate")->load();
    as.swing = apvts.getRawParameterValue ("arpSwing")->load();
    as.latch = apvts.getRawParameterValue ("arpLatch")->load() > 0.5f;
    arp.process (midi, n, as, bpm, hostPlaying, ppq);

    synth.configure ((int) shared.global.poly->load(), shared.global.mono->load() > 0.5f,
                     shared.global.legato->load() > 0.5f);
    synth.renderNextBlock (buffer, midi, 0, n);

    fx.process (buffer, bpm);

    const float master = apvts.getRawParameterValue ("master")->load() * 1.4f;
    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);
    int w = scopeWrite.load();
    for (int i = 0; i < n; ++i)
    {
        l[i] = std::tanh (l[i] * master);
        r[i] = std::tanh (r[i] * master);
        scope[(size_t) w] = 0.5f * (l[i] + r[i]);
        w = (w + 1) % scopeSize;
    }
    scopeWrite.store (w);
}

//==============================================================================
int NebulonProcessor::getNumPrograms() { return (int) factoryPresets().size(); }

const juce::String NebulonProcessor::getProgramName (int index)
{
    const auto& p = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? juce::String (p[(size_t) index].name) : juce::String();
}

juce::String NebulonProcessor::getPresetCategory (int index) const
{
    const auto& p = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? juce::String (p[(size_t) index].category) : juce::String();
}

void NebulonProcessor::resetToDefaults()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());
}

void NebulonProcessor::setCurrentProgram (int index)
{
    const auto& presets = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    currentProgram = index;
    currentPresetName = presets[(size_t) index].name;

    resetToDefaults();
    for (const auto& [pid, value] : presets[(size_t) index].values)
    {
        auto* rp = apvts.getParameter (pid);
        jassert (rp != nullptr); // unknown id in a preset
        if (rp != nullptr) rp->setValueNotifyingHost (rp->convertTo0to1 (value));
    }
}

juce::File NebulonProcessor::getUserPresetFolder() const
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Nebulon").getChildFile ("Presets");
}

juce::Array<juce::File> NebulonProcessor::getUserPresets() const
{
    auto files = getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.nebpreset");
    files.sort();
    return files;
}

bool NebulonProcessor::saveUserPreset (const juce::String& name)
{
    auto folder = getUserPresetFolder();
    if (! folder.createDirectory()) return false;
    auto state = apvts.copyState();
    for (int l = 0; l < numLayers; ++l) state.removeProperty (layerPrefix (l) + "samplePath", nullptr);
    for (int l = 0; l < numLayers; ++l)
        if (apvts.state.hasProperty (layerPrefix (l) + "samplePath"))
            state.setProperty (layerPrefix (l) + "samplePath", apvts.state.getProperty (layerPrefix (l) + "samplePath"), nullptr);
    auto xml = state.createXml();
    if (xml == nullptr) return false;
    const auto file = folder.getChildFile (juce::File::createLegalFileName (name) + ".nebpreset");
    if (! xml->writeTo (file)) return false;
    currentPresetName = name;
    return true;
}

bool NebulonProcessor::loadUserPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
    auto tree = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (tree);
    for (int l = 0; l < numLayers; ++l)
    {
        const auto path = tree.getProperty (layerPrefix (l) + "samplePath").toString();
        if (path.isNotEmpty()) loadUserSample (l, juce::File (path));
    }
    currentPresetName = file.getFileNameWithoutExtension();
    return true;
}

//==============================================================================
bool NebulonProcessor::loadUserSample (int layer, const juce::File& file)
{
    if (! juce::isPositiveAndBelow (layer, numLayers)) return false;
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr) return false;

    const auto maxLen = (juce::int64) (reader->sampleRate * 60.0);
    const int len = (int) juce::jmin (reader->lengthInSamples, maxLen);
    if (len < 64) return false;

    juce::AudioBuffer<float> buf ((int) reader->numChannels, len);
    reader->read (&buf, 0, len, 0, true, true);

    auto sd = std::make_unique<SampleData>();
    sd->name = file.getFileNameWithoutExtension();
    sd->sampleRate = reader->sampleRate;
    sd->data.assign ((size_t) len, 0.0f);
    for (int c = 0; c < buf.getNumChannels(); ++c)
    {
        const float* d = buf.getReadPointer (c);
        for (int i = 0; i < len; ++i) sd->data[(size_t) i] += d[i] / (float) buf.getNumChannels();
    }
    sd->finalise();

    {
        const juce::ScopedLock sl (sampleLock);
        shared.userSample[layer].store (sd.get());
        loadedSamples.push_back (std::move (sd));
    }

    apvts.state.setProperty (layerPrefix (layer) + "samplePath", file.getFullPathName(), nullptr);
    if (auto* src = apvts.getParameter (layerPrefix (layer) + "src"))
        src->setValueNotifyingHost (src->convertTo0to1 ((float) builtinSources().size()));
    return true;
}

const SampleData* NebulonProcessor::getLayerSource (int layer) const
{
    const int idx = (int) apvts.getRawParameterValue (layerPrefix (layer) + "src")->load();
    const auto& b = builtinSources();
    if (idx < (int) b.size()) return b[(size_t) idx].get();
    return shared.userSample[layer].load();
}

juce::String NebulonProcessor::getUserSampleName (int layer) const
{
    if (auto* s = shared.userSample[layer].load()) return s->name;
    return {};
}

//==============================================================================
void NebulonProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("presetName", currentPresetName, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NebulonProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = tree.getProperty ("program", 0);
            currentPresetName = tree.getProperty ("presetName", "").toString();
            apvts.replaceState (tree);
            for (int l = 0; l < numLayers; ++l)
            {
                const auto path = tree.getProperty (layerPrefix (l) + "samplePath").toString();
                if (path.isNotEmpty()) loadUserSample (l, juce::File (path));
            }
        }
}

juce::AudioProcessorEditor* NebulonProcessor::createEditor() { return new NebulonEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NebulonProcessor(); }
