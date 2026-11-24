#include "PluginProcessor.h"
#include "PluginEditor.h"

ReedstormProcessor::ReedstormProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "REEDSTORM", rs::params::createLayout())
{
    for (auto* p : getParameters())
        if (auto* wp = dynamic_cast<juce::RangedAudioParameter*> (p))
            raw[wp->getParameterID().toStdString()] = apvts.getRawParameterValue (wp->getParameterID());
}

bool ReedstormProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void ReedstormProcessor::prepareToPlay (double sampleRate, int)
{
    rack.prepare (sampleRate);
}

float ReedstormProcessor::get (const char* id) const
{
    auto it = raw.find (id);
    jassert (it != raw.end());
    return it != raw.end() ? it->second->load (std::memory_order_relaxed) : 0.0f;
}

rs::RackParams ReedstormProcessor::snapshot() const
{
    rs::RackParams p;
    auto b = [this] (const char* id) { return get (id) > 0.5f; };
    auto i = [this] (const char* id) { return (int) std::lround (get (id)); };

    p.inGain = get ("in_gain");
    p.dynOn = b ("dyn_on"); p.gateThr = get ("gate_thr"); p.compAmt = get ("comp_amt"); p.compSpeed = get ("comp_speed");
    p.octOn = b ("oct_on"); p.octSub1 = get ("oct_sub1"); p.octSub2 = get ("oct_sub2"); p.octUp = get ("oct_up"); p.octDry = get ("oct_dry");
    p.synOn = b ("syn_on"); p.synWave = i ("syn_wave"); p.synOct = i ("syn_oct"); p.synCut = get ("syn_cut");
    p.synRes = get ("syn_res"); p.synEnv = get ("syn_env"); p.synGlide = get ("syn_glide"); p.synLvl = get ("syn_lvl");
    p.wahOn = b ("wah_on"); p.wahMode = i ("wah_mode"); p.wahSens = get ("wah_sens"); p.wahFreq = get ("wah_freq");
    p.wahRange = get ("wah_range"); p.wahQ = get ("wah_q"); p.wahRate = get ("wah_rate"); p.wahMix = get ("wah_mix");
    p.drvOn = b ("drv_on"); p.drvType = i ("drv_type"); p.drvDrive = get ("drv_drive"); p.drvChar = get ("drv_char");
    p.drvTone = get ("drv_tone"); p.drvLevel = get ("drv_level"); p.drvMix = get ("drv_mix");
    p.eqOn = b ("eq_on"); p.eqLowCut = get ("eq_lowcut"); p.eqBody = get ("eq_body"); p.eqMid = get ("eq_mid");
    p.eqMidF = get ("eq_midf"); p.eqAir = get ("eq_air");
    p.harOn = b ("har_on"); p.harMode = i ("har_mode"); p.harKey = i ("har_key"); p.harScale = i ("har_scale");
    p.harInt1 = i ("har_int1"); p.harInt2 = i ("har_int2"); p.harLvl1 = get ("har_lvl1"); p.harLvl2 = get ("har_lvl2");
    p.harSpread = get ("har_spread");
    p.modOn = b ("mod_on"); p.modType = i ("mod_type"); p.modRate = get ("mod_rate"); p.modDepth = get ("mod_depth");
    p.modFb = get ("mod_fb"); p.modMix = get ("mod_mix");
    p.dlyOn = b ("dly_on"); p.dlySync = i ("dly_sync"); p.dlyTime = get ("dly_time"); p.dlyFb = get ("dly_fb");
    p.dlyTone = get ("dly_tone"); p.dlyWobble = get ("dly_wobble"); p.dlyDuck = get ("dly_duck"); p.dlyMix = get ("dly_mix");
    p.dlyPP = b ("dly_pp");
    p.revOn = b ("rev_on"); p.revPre = get ("rev_pre"); p.revSize = get ("rev_size"); p.revDecay = get ("rev_decay");
    p.revDamp = get ("rev_damp"); p.revShimmer = get ("rev_shimmer"); p.revMix = get ("rev_mix");
    p.outWidth = get ("out_width"); p.outGain = get ("out_gain");
    p.instrument = i ("tuner_inst"); p.tunerRef = get ("tuner_ref");
    return p;
}

void ReedstormProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels();
    const int outCh = getTotalNumOutputChannels();
    if (n == 0 || outCh == 0) return;

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = *b;

    const float* inL = buffer.getReadPointer (0);
    const float* inR = inCh > 1 ? buffer.getReadPointer (1) : nullptr;
    float* outL = buffer.getWritePointer (0);
    float* outR = outCh > 1 ? buffer.getWritePointer (1) : outL;

    if (outCh == 1)
    {
        // mono out: render stereo into scratch, fold down
        const auto p = snapshot();
        float tmp[256];
        for (int pos = 0; pos < n; pos += 256)
        {
            const int m = juce::jmin (256, n - pos);
            rack.process (inL + pos, inR != nullptr ? inR + pos : nullptr, outL + pos, tmp, m, p, bpm);
            for (int i = 0; i < m; ++i) outL[pos + i] = 0.5f * (outL[pos + i] + tmp[i]);
        }
        return;
    }
    rack.process (inL, inR, outL, outR, n, snapshot(), bpm);
    for (int c = 2; c < outCh; ++c) buffer.clear (c, 0, n);
}

const juce::String ReedstormProcessor::getProgramName (int index)
{
    const auto& p = rs::factoryPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? juce::String (p[(size_t) index].name) : juce::String();
}

void ReedstormProcessor::loadPreset (int index)
{
    const auto& presets = rs::factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    currentPreset = index;

    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto id = rp->getParameterID();
            if (id.startsWith ("tuner_") || id == "in_gain" || id == "out_gain") continue;
            rp->setValueNotifyingHost (rp->getDefaultValue());
        }

    for (const auto& [id, value] : presets[(size_t) index].values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));

    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

void ReedstormProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void ReedstormProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentPreset = (int) tree.getProperty ("preset", 0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* ReedstormProcessor::createEditor() { return new ReedstormEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ReedstormProcessor(); }
