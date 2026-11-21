#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using APVTS = juce::AudioProcessorValueTreeState;

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
juce::String hz   (float v, int) { return v < 1000.0f ? juce::String (v, 0) + " Hz" : juce::String (v / 1000.0f, 2) + " kHz"; }

juce::String waveName (float v, int)
{
    static const char* names[] = { "Sine", "Triangle", "Saw", "Square" };
    const int i = juce::jlimit (0, 3, (int) v);
    const float fr = v - (float) i;
    if (fr < 0.05f || i == 3) return names[i];
    if (fr > 0.95f) return names[i + 1];
    return juce::String (names[i]) + " > " + names[i + 1];
}

void addLayer (APVTS::ParameterLayout& layout, const juce::String& p, bool on)
{
    using F = juce::AudioParameterFloat;
    using I = juce::AudioParameterInt;
    auto id = [&] (const char* s) { return juce::ParameterID { p + s, 1 }; };
    const juce::String L = "Layer " + p.substring (0, 1) + " ";

    layout.add (std::make_unique<juce::AudioParameterBool> (id ("on"), L + "On", on));
    layout.add (std::make_unique<F> (id ("level"), L + "Level", juce::NormalisableRange<float> (0, 1), 0.8f, withText (pct)));
    layout.add (std::make_unique<F> (id ("pan"), L + "Pan", juce::NormalisableRange<float> (-1, 1), 0.0f));
    layout.add (std::make_unique<F> (id ("wave"), L + "Wave", juce::NormalisableRange<float> (0, 3), 2.0f, withText (waveName)));
    layout.add (std::make_unique<I> (id ("octave"), L + "Octave", -3, 3, 0));
    layout.add (std::make_unique<I> (id ("semi"), L + "Semitone", -12, 12, 0));
    layout.add (std::make_unique<F> (id ("fine"), L + "Fine", juce::NormalisableRange<float> (-100, 100), 0.0f,
                                     withText ([] (float v, int) { return juce::String (v, 0) + " ct"; })));
    layout.add (std::make_unique<I> (id ("unison"), L + "Unison", 1, 7, 5));
    layout.add (std::make_unique<F> (id ("detune"), L + "Detune", juce::NormalisableRange<float> (0, 100), 15.0f,
                                     withText ([] (float v, int) { return juce::String (v, 0) + " ct"; })));
    layout.add (std::make_unique<F> (id ("spread"), L + "Stereo Spread", juce::NormalisableRange<float> (0, 1), 0.7f, withText (pct)));
    layout.add (std::make_unique<F> (id ("sub"), L + "Sub", juce::NormalisableRange<float> (0, 1), 0.0f, withText (pct)));
    layout.add (std::make_unique<F> (id ("noise"), L + "Noise", juce::NormalisableRange<float> (0, 1), 0.0f, withText (pct)));
    layout.add (std::make_unique<F> (id ("drift"), L + "Analog Drift", juce::NormalisableRange<float> (0, 1), 0.3f, withText (pct)));
    layout.add (std::make_unique<juce::AudioParameterChoice> (id ("ftype"), L + "Filter Type",
                                                              juce::StringArray { "LP 12", "LP 24", "Band", "High" }, 1));
    layout.add (std::make_unique<F> (id ("cutoff"), L + "Cutoff", skewed (20, 20000, 1000), 4000.0f, withText (hz)));
    layout.add (std::make_unique<F> (id ("reso"), L + "Resonance", juce::NormalisableRange<float> (0, 1), 0.2f, withText (pct)));
    layout.add (std::make_unique<F> (id ("fenv"), L + "Filter Env Amt", juce::NormalisableRange<float> (-1, 1), 0.3f, withText (pct)));
    layout.add (std::make_unique<F> (id ("keytrack"), L + "Key Track", juce::NormalisableRange<float> (0, 1), 0.5f, withText (pct)));
    layout.add (std::make_unique<F> (id ("drive"), L + "Drive", juce::NormalisableRange<float> (0, 1), 0.1f, withText (pct)));

    const auto t = skewed (0.001f, 10.0f, 1.0f);
    layout.add (std::make_unique<F> (id ("aA"), L + "Amp Attack", t, 0.01f, withText (secs)));
    layout.add (std::make_unique<F> (id ("aD"), L + "Amp Decay", t, 0.4f, withText (secs)));
    layout.add (std::make_unique<F> (id ("aS"), L + "Amp Sustain", juce::NormalisableRange<float> (0, 1), 0.8f, withText (pct)));
    layout.add (std::make_unique<F> (id ("aR"), L + "Amp Release", t, 0.8f, withText (secs)));
    layout.add (std::make_unique<F> (id ("fA"), L + "Filter Attack", t, 0.01f, withText (secs)));
    layout.add (std::make_unique<F> (id ("fD"), L + "Filter Decay", t, 0.8f, withText (secs)));
    layout.add (std::make_unique<F> (id ("fS"), L + "Filter Sustain", juce::NormalisableRange<float> (0, 1), 0.5f, withText (pct)));
    layout.add (std::make_unique<F> (id ("fR"), L + "Filter Release", t, 0.8f, withText (secs)));

    layout.add (std::make_unique<F> (id ("lfoRate"), L + "LFO Rate", skewed (0.01f, 20.0f, 2.0f), 0.5f,
                                     withText ([] (float v, int) { return juce::String (v, 2) + " Hz"; })));
    layout.add (std::make_unique<F> (id ("lfoCut"), L + "LFO > Cutoff", juce::NormalisableRange<float> (0, 1), 0.0f, withText (pct)));
    layout.add (std::make_unique<F> (id ("lfoPitch"), L + "LFO > Pitch", juce::NormalisableRange<float> (0, 1), 0.0f, withText (pct)));
    layout.add (std::make_unique<F> (id ("lfoAmp"), L + "LFO > Amp", juce::NormalisableRange<float> (0, 1), 0.0f, withText (pct)));
}
} // namespace

APVTS::ParameterLayout NebulonProcessor::createLayout()
{
    using F = juce::AudioParameterFloat;
    APVTS::ParameterLayout layout;
    addLayer (layout, "A_", true);
    addLayer (layout, "B_", true);

    auto id = [] (const char* s) { return juce::ParameterID { s, 1 }; };
    layout.add (std::make_unique<F> (id ("chorusMix"), "Chorus Mix", juce::NormalisableRange<float> (0, 1), 0.3f, withText (pct)));
    layout.add (std::make_unique<F> (id ("chorusRate"), "Chorus Rate", skewed (0.05f, 5.0f, 0.8f), 0.4f,
                                     withText ([] (float v, int) { return juce::String (v, 2) + " Hz"; })));
    layout.add (std::make_unique<F> (id ("chorusDepth"), "Chorus Depth", juce::NormalisableRange<float> (0, 1), 0.4f, withText (pct)));
    layout.add (std::make_unique<F> (id ("delayTime"), "Delay Time", skewed (10.0f, 2000.0f, 400.0f), 375.0f,
                                     withText ([] (float v, int) { return juce::String (v, 0) + " ms"; })));
    layout.add (std::make_unique<F> (id ("delayFb"), "Delay Feedback", juce::NormalisableRange<float> (0, 0.95f), 0.35f, withText (pct)));
    layout.add (std::make_unique<F> (id ("delayMix"), "Delay Mix", juce::NormalisableRange<float> (0, 1), 0.2f, withText (pct)));
    layout.add (std::make_unique<F> (id ("revSize"), "Reverb Size", juce::NormalisableRange<float> (0, 1), 0.75f, withText (pct)));
    layout.add (std::make_unique<F> (id ("revDamp"), "Reverb Damping", juce::NormalisableRange<float> (0, 1), 0.4f, withText (pct)));
    layout.add (std::make_unique<F> (id ("revMix"), "Reverb Mix", juce::NormalisableRange<float> (0, 1), 0.3f, withText (pct)));
    layout.add (std::make_unique<F> (id ("master"), "Master", juce::NormalisableRange<float> (0, 1), 0.7f, withText (pct)));
    return layout;
}

//==============================================================================
NebulonProcessor::NebulonProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NEBULON", createLayout())
{
    params.layer[0].bind (apvts, "A_");
    params.layer[1].bind (apvts, "B_");

    for (int i = 0; i < 16; ++i)
        synth.addVoice (new neb::Voice (params));
    synth.addSound (new neb::Sound());

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

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    chorus.prepare (spec);
    chorus.setCentreDelay (12.0f);
    chorus.setFeedback (0.15f);

    reverb.setSampleRate (sampleRate);
    reverb.reset();

    delayBuf.setSize (2, (int) (sampleRate * 2.1) + 1);
    delayBuf.clear();
    delayWrite = 0;
    delayLpState[0] = delayLpState[1] = 0.0f;
    delaySamples.reset (sampleRate, 0.15);
    delaySamples.setCurrentAndTargetValue (apvts.getRawParameterValue ("delayTime")->load() * 0.001f * (float) sampleRate);
}

void NebulonProcessor::processDelay (juce::AudioBuffer<float>& buffer)
{
    const float mix = apvts.getRawParameterValue ("delayMix")->load();
    const float fb  = apvts.getRawParameterValue ("delayFb")->load();
    delaySamples.setTargetValue (apvts.getRawParameterValue ("delayTime")->load() * 0.001f * (float) sr);

    const int size = delayBuf.getNumSamples();
    auto* dl = delayBuf.getWritePointer (0);
    auto* dr = delayBuf.getWritePointer (1);
    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float d = delaySamples.getNextValue();
        float rp = (float) delayWrite - d;
        while (rp < 0) rp += (float) size;
        const int i0 = (int) rp;
        const int i1 = (i0 + 1) % size;
        const float fr = rp - (float) i0;
        const float wl = dl[i0] + (dl[i1] - dl[i0]) * fr;
        const float wr = dr[i0] + (dr[i1] - dr[i0]) * fr;

        // darkening feedback for a tape-like tail
        delayLpState[0] += (wl - delayLpState[0]) * 0.35f;
        delayLpState[1] += (wr - delayLpState[1]) * 0.35f;

        // ping-pong: input feeds left, channels cross in the feedback path
        const float in = 0.5f * (l[i] + r[i]);
        dl[delayWrite] = in + delayLpState[1] * fb;
        dr[delayWrite] = delayLpState[0] * fb;

        l[i] += wl * mix;
        r[i] += wr * mix;
        if (++delayWrite >= size) delayWrite = 0;
    }
}

void NebulonProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        if (msg.isControllerOfType (1))
            params.modWheel.store (msg.getControllerValue() / 127.0f);
    }

    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    // Chorus
    chorus.setRate (apvts.getRawParameterValue ("chorusRate")->load());
    chorus.setDepth (apvts.getRawParameterValue ("chorusDepth")->load());
    chorus.setMix (apvts.getRawParameterValue ("chorusMix")->load() * 0.5f);
    juce::dsp::AudioBlock<float> block (buffer);
    chorus.process (juce::dsp::ProcessContextReplacing<float> (block));

    // Delay
    processDelay (buffer);

    // Reverb
    const float revMix = apvts.getRawParameterValue ("revMix")->load();
    juce::Reverb::Parameters rp;
    rp.roomSize = apvts.getRawParameterValue ("revSize")->load();
    rp.damping  = apvts.getRawParameterValue ("revDamp")->load();
    rp.width    = 1.0f;
    rp.wetLevel = revMix;
    rp.dryLevel = 1.0f - revMix * 0.5f;
    reverb.setParameters (rp);
    reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), buffer.getNumSamples());

    // Master + soft clip, and feed the oscilloscope
    const float master = apvts.getRawParameterValue ("master")->load() * 1.5f;
    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);
    int w = scopeWrite.load();
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        l[i] = std::tanh (l[i] * master);
        r[i] = std::tanh (r[i] * master);
        scope[(size_t) w] = 0.5f * (l[i] + r[i]);
        w = (w + 1) % scopeSize;
    }
    scopeWrite.store (w);
}

//==============================================================================
int NebulonProcessor::getNumPrograms() { return (int) neb::factoryPresets().size(); }

const juce::String NebulonProcessor::getProgramName (int index)
{
    const auto& p = neb::factoryPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? juce::String (p[(size_t) index].name) : juce::String();
}

void NebulonProcessor::setCurrentProgram (int index)
{
    const auto& presets = neb::factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    currentProgram = index;

    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& [pid, value] : presets[(size_t) index].values)
        if (auto* rp = apvts.getParameter (pid))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
}

void NebulonProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
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
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* NebulonProcessor::createEditor() { return new NebulonEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NebulonProcessor(); }
