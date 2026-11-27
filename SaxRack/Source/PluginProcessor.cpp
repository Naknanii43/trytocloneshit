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
    static const char* suffix[9] = { "level", "pan", "mute", "rev", "half", "tap", "stop", "undo", "clear" };
    for (int t = 0; t < 4; ++t)
        for (int k = 0; k < 9; ++k)
            loopIds[(size_t) t][(size_t) k] = "loop" + std::to_string (t + 1) + "_" + suffix[k];

    // warm the address cache on this (message) thread so the audio thread never inserts
    warmingUp = true;
    byAddress.reserve (512);
    (void) snapshot();
    for (auto& t : loopIds) for (auto& id : t) (void) get (id.c_str());
    (void) get ("loop_stopall"); (void) get ("loop_clearall");
    warmingUp = false;
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
    // Hot path: every id used on the audio thread is a string literal (or a stable member string),
    // so after warm-up the lookup is a hash of the pointer, not of the text. Never allocates there.
    if (auto it = byAddress.find (id); it != byAddress.end())
        return it->second->load (std::memory_order_relaxed);
    auto it = raw.find (id);
    jassert (it != raw.end());
    if (it == raw.end()) return 0.0f;
    if (warmingUp) byAddress[id] = it->second;
    return it->second->load (std::memory_order_relaxed);
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
    auto& g = p.gen;
    g.on = b ("gen_on");
    g.split[0] = get ("gen_split1"); g.split[1] = get ("gen_split2"); g.split[2] = get ("gen_split3");
    g.sound[0] = i ("gen_snd1"); g.sound[1] = i ("gen_snd2"); g.sound[2] = i ("gen_snd3"); g.sound[3] = i ("gen_snd4");
    g.morph = get ("gen_morph"); g.pitchMorph = get ("gen_pmorph"); g.bloom = get ("gen_bloom");
    g.pattern = i ("gen_pattern"); g.rate = i ("gen_rate"); g.length = get ("gen_length");
    g.density = get ("gen_density"); g.range = i ("gen_range"); g.hold = get ("gen_hold");
    g.scale = p.harScale;
    g.keyConcert = ((p.harKey - rs::params::writtenOffset (i ("tuner_inst"))) % 12 + 12) % 12;
    auto& gr = p.grm;
    gr.on = b ("grm_on"); gr.sens = get ("grm_sens"); gr.chance = get ("grm_chance"); gr.harsh = get ("grm_harsh");
    gr.dirt = get ("grm_dirt"); gr.follow = get ("grm_follow"); gr.swarm = get ("grm_swarm"); gr.level = get ("grm_level"); gr.palette = i ("grm_palette");
    p.loop.on = b ("loop_on"); p.loop.lock = b ("loop_lock"); p.loop.snap = b ("loop_snap"); p.loop.feedback = get ("loop_fb");
    for (int t = 0; t < 4; ++t)
    {
        const auto& ids = loopIds[(size_t) t];
        auto& tp = p.loop.track[t];
        tp.level = get (ids[0].c_str()); tp.pan = get (ids[1].c_str());
        tp.mute = get (ids[2].c_str()) > 0.5f; tp.reverse = get (ids[3].c_str()) > 0.5f; tp.half = get (ids[4].c_str()) > 0.5f;
    }
    p.whm.on = b ("whm_on"); p.whm.mode = i ("whm_mode"); p.whm.amount = get ("whm_amount"); p.whm.pedal = get ("whm_pedal");
    p.whm.rate = get ("whm_rate"); p.whm.glide = get ("whm_glide"); p.whm.mix = get ("whm_mix");
    p.ring.on = b ("rng_on"); p.ring.mode = i ("rng_mode"); p.ring.wave = i ("rng_wave"); p.ring.freq = get ("rng_freq");
    p.ring.ratio = get ("rng_ratio"); p.ring.rate = get ("rng_rate"); p.ring.sweep = get ("rng_sweep"); p.ring.mix = get ("rng_mix");
    p.frz.on = b ("frz_on"); p.frz.hold = b ("frz_hold"); p.frz.mode = i ("frz_mode"); p.frz.pitch = i ("frz_pitch");
    p.frz.stack = b ("frz_stack"); p.frz.decay = get ("frz_decay"); p.frz.attack = get ("frz_attack");
    p.frz.width = get ("frz_width"); p.frz.level = get ("frz_level");
    p.gate.on = b ("gat_on"); p.gate.pattern = i ("gat_pattern"); p.gate.rate = i ("gat_rate"); p.gate.depth = get ("gat_depth");
    p.gate.smooth = get ("gat_smooth"); p.gate.swing = get ("gat_swing"); p.gate.pingPong = b ("gat_pp");
    p.fmt.on = b ("fmt_on"); p.fmt.shift = get ("fmt_shift"); p.fmt.mix = get ("fmt_mix");
    p.talk.on = b ("tlk_on"); p.talk.mode = i ("tlk_mode"); p.talk.vowel = get ("tlk_vowel"); p.talk.rate = get ("tlk_rate");
    p.talk.sens = get ("tlk_sens"); p.talk.reso = get ("tlk_reso"); p.talk.mix = get ("tlk_mix");
    p.dbl.on = b ("dbl_on"); p.dbl.players = i ("dbl_players") + 1; p.dbl.detune = get ("dbl_detune"); p.dbl.timing = get ("dbl_timing");
    p.dbl.width = get ("dbl_width"); p.dbl.mix = get ("dbl_mix");
    p.trem.on = b ("trm_on"); p.trem.mode = i ("trm_mode"); p.trem.shape = i ("trm_shape"); p.trem.sync = i ("trm_sync");
    p.trem.rate = get ("trm_rate"); p.trem.depth = get ("trm_depth");
    p.brp.on = b ("brp_on"); p.brp.hold = b ("brp_hold"); p.brp.mode = i ("brp_mode"); p.brp.grid = i ("brp_grid");
    p.brp.length = i ("brp_length"); p.brp.chance = get ("brp_chance"); p.brp.pitch = get ("brp_pitch");
    p.brp.decay = get ("brp_decay"); p.brp.mix = get ("brp_mix");
    p.spring.on = b ("spr_on"); p.spring.decay = get ("spr_decay"); p.spring.tone = get ("spr_tone");
    p.spring.tension = get ("spr_tension"); p.spring.drip = get ("spr_drip"); p.spring.mix = get ("spr_mix");
    p.cloud.on = b ("cld_on"); p.cloud.pitchMode = i ("cld_pmode"); p.cloud.density = get ("cld_density"); p.cloud.size = get ("cld_size");
    p.cloud.spray = get ("cld_spray"); p.cloud.pitch = get ("cld_pitch"); p.cloud.reverse = get ("cld_reverse");
    p.cloud.width = get ("cld_width"); p.cloud.mix = get ("cld_mix");
    p.outWidth = get ("out_width"); p.outGain = get ("out_gain");
    applyPerformMacros (p);
    p.instrument = i ("tuner_inst"); p.tunerRef = get ("tuner_ref");
    return p;
}

/** The PLAY page: five macros that bend whatever World (preset) is loaded.
    All at their neutral positions (morph 0, chaos 0, space 0.5, tone 0, harmony "World") change nothing. */
void ReedstormProcessor::applyPerformMacros (rs::RackParams& p) const
{
    auto clamp01 = [] (float v) { return juce::jlimit (0.0f, 1.0f, v); };
    const float morph = get ("perf_morph"), chaos = get ("perf_chaos"), space = get ("perf_space"), tone = get ("perf_tone");
    const int harm = (int) std::lround (get ("perf_harm"));

    // TRANSFORM x: sax -> synth (the Morph Gen zones take over, held notes start to bloom)
    if (morph > 0.02f)
    {
        const bool was = p.gen.on;
        p.gen.on = true;
        p.gen.morph = std::max (was ? p.gen.morph : 0.0f, morph * 0.95f);
        p.gen.bloom = std::max (was ? p.gen.bloom : 0.0f, clamp01 ((morph - 0.4f) * 1.3f));
    }

    // TRANSFORM y: clean -> chaos (silly gremlins first, then harsh ones, fuzz, then glitches)
    if (chaos > 0.02f)
    {
        const bool was = p.grm.on;
        p.grm.on = true;
        p.grm.chance = std::max (was ? p.grm.chance : 0.0f, 0.3f + 0.7f * chaos);
        p.grm.swarm = std::max (was ? p.grm.swarm : 0.0f, chaos * 0.8f);
        p.grm.harsh = std::max (was ? p.grm.harsh : 0.0f, chaos * chaos);
        p.grm.dirt = std::max (was ? p.grm.dirt : 0.0f, chaos * chaos * 0.8f);
        p.grm.level = std::max (was ? p.grm.level : 0.0f, 0.25f + 0.5f * chaos);
        if (chaos > 0.5f)
        {
            const float c = (chaos - 0.5f) * 2.0f;
            if (! p.drvOn) { p.drvOn = true; p.drvType = 1; p.drvDrive = c * 0.6f; p.drvMix = c; p.drvTone = 0.5f; p.drvChar = 0.3f; }
            else p.drvDrive = std::min (1.0f, p.drvDrive + c * 0.3f);
        }
        if (chaos > 0.75f)
        {
            const bool brpWas = p.brp.on;
            p.brp.on = true;
            if (! brpWas) { p.brp.mode = 1; p.brp.grid = 2; p.brp.pitch = -1.0f; p.brp.chance = 0.0f; p.brp.mix = 1.0f; p.brp.decay = 0.88f; }
            p.brp.chance = std::max (p.brp.chance, (chaos - 0.75f) * 2.4f);
        }
    }

    // SPACE: below the middle dries the World out, above it adds room, echo, then shimmer
    if (space < 0.5f)
    {
        const float k = space * 2.0f;
        p.revMix *= k; p.dlyMix *= k; p.spring.mix *= k;
    }
    else if (space > 0.5f)
    {
        const float e = (space - 0.5f) * 2.0f;
        if (! p.revOn) { p.revOn = true; p.revMix = 0.0f; p.revDamp = 0.35f; p.revPre = 25.0f; }
        p.revMix = std::max (p.revMix, e * 0.75f);
        p.revSize = std::max (p.revSize, 0.5f + 0.5f * e);
        p.revDecay = std::max (p.revDecay, 1.5f + 10.0f * e * e);
        p.revShimmer = std::max (p.revShimmer, clamp01 ((e - 0.6f) * 1.6f));
        if (e > 0.3f)
        {
            if (! p.dlyOn) { p.dlyOn = true; p.dlySync = 6; p.dlyFb = 0.4f; p.dlyDuck = 0.5f; p.dlyMix = 0.0f; p.dlyTone = 3500.0f; }
            p.dlyMix = std::max (p.dlyMix, (e - 0.3f) * 0.4f);
        }
    }

    // TONE: dark <-> bright
    if (std::abs (tone) > 0.01f)
    {
        p.eqOn = true;
        p.eqAir = juce::jlimit (-12.0f, 12.0f, p.eqAir + tone * 8.0f);
        p.eqBody = juce::jlimit (-12.0f, 12.0f, p.eqBody - tone * 3.0f);
        if (tone > 0) p.eqLowCut = std::max (p.eqLowCut, 40.0f + tone * 140.0f);
    }

    // HARMONY style
    if (harm == 1) p.harOn = false;
    else if (harm >= 2)
    {
        p.harOn = true; p.harSpread = 0.6f;
        switch (harm)
        {
            case 2:  p.harMode = 0; p.harInt1 = 2; p.harLvl1 = 0.6f; p.harLvl2 = 0.0f; break;
            case 3:  p.harMode = 0; p.harInt1 = 2; p.harInt2 = 4; p.harLvl1 = 0.55f; p.harLvl2 = 0.45f; break;
            case 4:  p.harMode = 1; p.harInt1 = -12; p.harInt2 = 12; p.harLvl1 = 0.6f; p.harLvl2 = 0.4f; break;
            default: p.harMode = 1; p.harInt1 = 7; p.harLvl1 = 0.6f; p.harLvl2 = 0.0f; break;
        }
    }

    // the big FREEZE button works whatever the World
    if (p.frz.hold && ! p.frz.on) { p.frz.on = true; p.frz.mode = 0; p.frz.level = 0.85f; p.frz.width = 0.9f; }
}

void ReedstormProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels();
    const int outCh = getTotalNumOutputChannels();
    if (n == 0 || outCh == 0) return;

    double bpm = 120.0, hostBeat = -1.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (pos->getIsPlaying())
                if (auto ppq = pos->getPpqPosition()) hostBeat = *ppq;
        }

    // loop-station footswitch parameters (MIDI-learnable in the host): act on the rising edge
    static const rs::LoopTrack::Command cmds[4] = { rs::LoopTrack::Tap, rs::LoopTrack::Stop, rs::LoopTrack::Undo, rs::LoopTrack::Clear };
    for (int t = 0; t < 4; ++t)
        for (int k = 0; k < 4; ++k)
        {
            const bool down = get (loopIds[(size_t) t][(size_t) (5 + k)].c_str()) > 0.5f;
            if (down && ! footDown[t][k]) rack.looper.post (t, cmds[k]);
            footDown[t][k] = down;
        }
    const bool sa = get ("loop_stopall") > 0.5f, ca = get ("loop_clearall") > 0.5f;
    if (sa && ! stopAllDown) rack.looper.stopAll();
    if (ca && ! clearAllDown) rack.looper.clearAll();
    stopAllDown = sa; clearAllDown = ca;

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
            rack.process (inL + pos, inR != nullptr ? inR + pos : nullptr, outL + pos, tmp, m, p, bpm, hostBeat >= 0 ? hostBeat + pos * bpm / (60.0 * getSampleRate()) : -1.0);
            for (int i = 0; i < m; ++i) outL[pos + i] = 0.5f * (outL[pos + i] + tmp[i]);
        }
        return;
    }
    rack.process (inL, inR, outL, outR, n, snapshot(), bpm, hostBeat);
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
            if (id.startsWith ("tuner_") || id.startsWith ("loop") || id.startsWith ("perf_") || id == "frz_hold" || id == "brp_hold" || id == "in_gain" || id == "out_gain") continue;
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
