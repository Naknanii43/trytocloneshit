#include "Engine.h"

namespace neb
{
//==============================================================================
void Voice::updateEnv (int l)
{
    const auto& p = sh.layer[l];
    st[l].amp.setParameters ({ p.aA->load(), p.aD->load(), p.aS->load(), p.aR->load() });
    st[l].fenv.setParameters ({ p.fA->load(), p.fD->load(), p.fS->load(), p.fR->load() });
}

void Voice::startNote (int midiNote, float velocity, juce::SynthesiserSound*, int pitchWheel)
{
    const double sr = getSampleRate();
    note = midiNote;
    vel = velocity;
    bend = (float) (pitchWheel - 8192) / 8192.0f;
    randomVal = rng.nextFloat() * 2.0f - 1.0f;
    noteTime = 0;
    held = true;

    const float glide = sh.global.glide->load();
    curPitch = (glide > 0.0005f && sh.lastPitch >= 0.0f) ? sh.lastPitch : (float) midiNote;
    targetPitch = (float) midiNote;
    sh.lastPitch = (float) midiNote;

    for (int k = 0; k < numLfos; ++k)
    {
        lfoPh[k] = sh.global.lfoRetrig[k]->load() > 0.5f ? 0.0 : sh.lfoPhase[k].load();
        shPrev[k] = rng.nextFloat() * 2 - 1;
        shCur[k] = rng.nextFloat() * 2 - 1;
    }

    for (int e = 0; e < numModEnvs; ++e)
    {
        modEnv[e].setSampleRate (sr);
        modEnv[e].setParameters ({ sh.global.me[e][0]->load(), sh.global.me[e][1]->load(),
                                   sh.global.me[e][2]->load(), sh.global.me[e][3]->load() });
        modEnv[e].reset();
        modEnv[e].noteOn();
    }

    const auto& builtins = builtinSources();
    for (int l = 0; l < numLayers; ++l)
    {
        auto& s = st[l];
        const auto& p = sh.layer[l];
        s.active = p.on->load() > 0.5f;
        if (! s.active) continue;

        s.amp.setSampleRate (sr);
        s.fenv.setSampleRate (sr);
        updateEnv (l);
        s.amp.reset();
        s.fenv.reset();

        const int srcIdx = (int) p.src->load();
        if (srcIdx < (int) builtins.size()) s.sample = builtins[(size_t) srcIdx].get();
        else s.sample = sh.userSample[l].load();
        if (s.sample == nullptr) s.sample = builtins[0].get();

        const double startPos = p.smpStart->load() * (double) s.sample->data.size();
        for (int u = 0; u < LayerState::maxSlots; ++u)
        {
            s.phase[u] = rng.nextDouble();
            s.modPhase[u] = 0;
            s.fmPrev[u] = 0;
            s.smpPos[u] = startPos;
            s.uniDrift[u] = rng.nextFloat() * 2.0f - 1.0f;
        }
        s.subPhase = 0;
        for (int c = 0; c < 2; ++c) { s.f1[c].reset(); s.f2[c].reset(); s.f3[c].reset(); s.comb[c].reset(); }
        for (auto& g : s.grains) g.active = false;
        s.grainTimer = 0;
        s.amp.noteOn();
        s.fenv.noteOn();
    }
    playing = true;
}

void Voice::legatoTo (int midiNote, float)
{
    note = midiNote;
    targetPitch = (float) midiNote;
    if (sh.global.glide->load() <= 0.0005f) curPitch = targetPitch;
    sh.lastPitch = (float) midiNote;
    held = true;
}

void Voice::stopNote (float, bool allowTailOff)
{
    held = false;
    if (allowTailOff)
    {
        for (auto& s : st) { s.amp.noteOff(); s.fenv.noteOff(); }
        for (auto& e : modEnv) e.noteOff();
    }
    else
    {
        for (auto& s : st) { s.amp.reset(); s.fenv.reset(); s.active = false; }
        playing = false;
        clearCurrentNote();
    }
}

//==============================================================================
void Voice::controlTick (int n)
{
    const double sr = getSampleRate();
    const auto& g = sh.global;

    // glide
    const float glide = g.glide->load();
    if (glide <= 0.0005f) curPitch = targetPitch;
    else curPitch += (targetPitch - curPitch) * (1.0f - std::exp (-(float) n / (glide * 0.25f * (float) sr)));

    // LFOs
    for (int k = 0; k < numLfos; ++k)
    {
        lfoPh[k] += sh.lfoHz (k) * n / sr;
        if (lfoPh[k] >= 1.0)
        {
            lfoPh[k] -= std::floor (lfoPh[k]);
            shPrev[k] = shCur[k];
            shCur[k] = rng.nextFloat() * 2.0f - 1.0f;
        }
        const double t = lfoPh[k];
        float v = 0;
        switch ((int) g.lfoShape[k]->load())
        {
            case 0: v = (float) std::sin (twoPi * t); break;
            case 1: v = (float) (1.0 - 4.0 * std::abs (t - 0.5)); break;
            case 2: v = (float) (2.0 * t - 1.0); break;
            case 3: v = (float) (1.0 - 2.0 * t); break;
            case 4: v = t < 0.5 ? 1.0f : -1.0f; break;
            case 5: v = shCur[k]; break;
            default:
            {
                const float s = (float) (t * t * (3.0 - 2.0 * t));
                v = shPrev[k] + (shCur[k] - shPrev[k]) * s;
                break;
            }
        }
        const float fade = g.lfoFade[k]->load();
        if (fade > 0.001f) v *= (float) juce::jmin (1.0, noteTime / fade);
        srcVals[srcLfo1 + k] = v;
    }

    // mod envelopes
    for (int e = 0; e < numModEnvs; ++e)
    {
        modEnv[e].setParameters ({ g.me[e][0]->load(), g.me[e][1]->load(), g.me[e][2]->load(), g.me[e][3]->load() });
        float v = modEnv[e].getNextSample();
        for (int i = 1; i < n; ++i) modEnv[e].getNextSample();
        srcVals[srcEnv1 + e] = v;
    }

    srcVals[srcNone] = 0;
    srcVals[srcVelocity] = vel;
    srcVals[srcModWheel] = sh.modWheel.load();
    srcVals[srcAftertouch] = sh.aftertouch.load();
    srcVals[srcKey] = juce::jlimit (-1.0f, 1.0f, (float) (note - 60) / 36.0f);
    srcVals[srcRandom] = randomVal;
    for (int i = 0; i < numMacros; ++i) srcVals[srcMacro1 + i] = g.macro[i]->load();

    // matrix
    std::memset (mods, 0, sizeof (mods));
    for (int i = 0; i < numSlots; ++i)
    {
        const int src = (int) g.mSrc[i]->load();
        const int dst = (int) g.mDst[i]->load();
        if (src <= srcNone || dst <= dstNone || src >= numSrc || dst >= numDst) continue;
        const float v = srcVals[src] * g.mAmt[i]->load();
        const int layer = (int) g.mLayer[i]->load();
        if (layer == 0) for (auto& m : mods) m[dst] += v;
        else if (layer <= numLayers) mods[layer - 1][dst] += v;
    }

    noteTime += n / sr;
}

//==============================================================================
void Voice::renderNextBlock (juce::AudioBuffer<float>& out, int start, int num)
{
    if (! playing) return;
    float* outL = out.getWritePointer (0);
    float* outR = out.getNumChannels() > 1 ? out.getWritePointer (1) : outL;

    int pos = start;
    const int end = start + num;
    while (pos < end)
    {
        const int n = juce::jmin (chunk, end - pos);
        controlTick (n);
        bool any = false;
        for (int l = 0; l < numLayers; ++l)
            any = renderLayer (l, outL, outR, pos, n) || any;
        if (! any)
        {
            playing = false;
            clearCurrentNote();
            return;
        }
        pos += n;
    }
}

bool Voice::renderLayer (int l, float* outL, float* outR, int pos, int n)
{
    auto& s = st[l];
    if (! s.active) return false;
    if (! s.amp.isActive()) { s.active = false; return false; }

    const double sr = getSampleRate();
    const auto& p = sh.layer[l];
    const float* m = mods[l];
    updateEnv (l);

    const int mode = (int) p.mode->load();
    const int uni = juce::jlimit (1, 7, (int) p.unison->load());
    const float detune = juce::jmax (0.0f, p.detune->load() + m[dstDetune] * 100.0f) / 100.0f;
    const float spread = p.spread->load();
    const float drift = p.drift->load();
    const float timbre = m[dstTimbre];

    // slow random drift
    s.driftCounter -= n;
    if (s.driftCounter <= 0)
    {
        s.driftCounter = (int) (sr * 0.05);
        s.driftTarget = rng.nextFloat() * 2.0f - 1.0f;
    }
    s.driftVal += (s.driftTarget - s.driftVal) * juce::jmin (1.0f, 0.0005f * (float) n);

    const double basePitch = curPitch + 12.0 * p.octave->load() + p.semi->load() + p.fine->load() / 100.0
                           + bend * sh.global.bend->load() + m[dstPitch] * 12.0 + s.driftVal * drift * 0.15;

    double groupSemis[3] = { 0, 0, 0 };
    int groups = 1;
    if ((int) p.harm1->load() != 0) groupSemis[groups++] = p.harm1->load();
    if ((int) p.harm2->load() != 0) groupSemis[groups++] = p.harm2->load();

    float tl[chunk] {}, tr[chunk] {};

    if (mode == modeGranular)
    {
        renderGranular (l, s, p, m, groupSemis, groups, basePitch, spread, tl, tr, n);
    }
    else
    {
        const int slots = groups * uni;
        double dt[LayerState::maxSlots];
        float gl[LayerState::maxSlots], gr[LayerState::maxSlots];
        const float norm = 1.0f / std::sqrt ((float) slots);
        for (int g = 0; g < groups; ++g)
            for (int u = 0; u < uni; ++u)
            {
                const int k = g * uni + u;
                const float posU = uni == 1 ? 0.0f : (float) u / (float) (uni - 1) * 2.0f - 1.0f;
                const double semis = basePitch + groupSemis[g] + posU * detune + s.uniDrift[k] * drift * 0.08;
                dt[k] = juce::jmin (0.49, midiToHz (semis) / sr);
                const float panPos = juce::jlimit (-1.0f, 1.0f, posU * spread + (g > 0 ? (g == 1 ? -0.3f : 0.3f) * spread : 0.0f));
                const float a = (panPos + 1.0f) * (float) pi * 0.25f;
                gl[k] = std::cos (a) * norm;
                gr[k] = std::sin (a) * norm;
            }

        switch (mode)
        {
            case modeWavetable:
            {
                const auto& tables = wavetables();
                const auto& tbl = tables[(size_t) juce::jlimit (0, (int) tables.size() - 1, (int) p.wtTable->load())];
                const float wpos = juce::jlimit (0.0f, 1.0f, p.wtPos->load() + timbre);
                for (int k = 0; k < slots; ++k)
                {
                    const int level = wtLevelFor (dt[k]);
                    double ph = s.phase[k];
                    for (int i = 0; i < n; ++i)
                    {
                        const float v = wtLookup (tbl, wpos, ph, level);
                        tl[i] += v * gl[k]; tr[i] += v * gr[k];
                        ph += dt[k]; if (ph >= 1.0) ph -= 1.0;
                    }
                    s.phase[k] = ph;
                }
                break;
            }
            case modeFM:
            {
                const double ratio = p.fmRatio->load();
                const float index = juce::jmax (0.0f, p.fmIndex->load() + timbre * 10.0f);
                const float fb = p.fmFb->load() * 1.5f;
                for (int k = 0; k < slots; ++k)
                {
                    double ph = s.phase[k], mph = s.modPhase[k];
                    float prev = s.fmPrev[k];
                    const double mdt = juce::jmin (0.49, dt[k] * ratio);
                    for (int i = 0; i < n; ++i)
                    {
                        const float mod = (float) std::sin (twoPi * mph + fb * prev);
                        prev = mod;
                        const float v = (float) std::sin (twoPi * ph + index * mod);
                        tl[i] += v * gl[k]; tr[i] += v * gr[k];
                        ph += dt[k]; if (ph >= 1.0) ph -= 1.0;
                        mph += mdt; if (mph >= 1.0) mph -= 1.0;
                    }
                    s.phase[k] = ph; s.modPhase[k] = mph; s.fmPrev[k] = prev;
                }
                break;
            }
            case modeSample:
            {
                const auto& data = s.sample->data;
                const double len = (double) data.size();
                if (len < 4) break;
                const bool loop = p.smpLoop->load() > 0.5f;
                const double ls = juce::jlimit (0.0, len * 0.9, p.smpStart->load() * len);
                const double le = len - 2;
                const double xf = juce::jmin (0.08 * s.sample->sampleRate, (le - ls) * 0.25);
                const double rootHz = midiToHz (s.sample->rootNote);
                auto read = [&] (double x)
                {
                    const int i0 = juce::jlimit (0, (int) len - 2, (int) x);
                    const float fr = (float) (x - i0);
                    return data[(size_t) i0] + (data[(size_t) i0 + 1] - data[(size_t) i0]) * fr;
                };
                for (int k = 0; k < slots; ++k)
                {
                    const double inc = dt[k] * sr / rootHz * s.sample->sampleRate / sr;
                    double x = s.smpPos[k];
                    for (int i = 0; i < n; ++i)
                    {
                        float v = 0.0f;
                        if (x < le)
                        {
                            v = read (x);
                            if (loop && x > le - xf)
                            {
                                const float w = (float) ((x - (le - xf)) / xf);
                                v = v * (1.0f - w) + read (ls + (x - (le - xf))) * w;
                            }
                        }
                        tl[i] += v * gl[k]; tr[i] += v * gr[k];
                        x += inc;
                        if (loop && x >= le) x = ls + xf + (x - le);
                    }
                    s.smpPos[k] = x;
                }
                break;
            }
            default:
            {
                const float w = juce::jlimit (0.0f, 3.0f, p.wave->load() + timbre * 3.0f);
                for (int k = 0; k < slots; ++k)
                {
                    double ph = s.phase[k];
                    for (int i = 0; i < n; ++i)
                    {
                        const float v = morphOsc (w, ph, dt[k]);
                        tl[i] += v * gl[k]; tr[i] += v * gr[k];
                        ph += dt[k]; if (ph >= 1.0) ph -= 1.0;
                    }
                    s.phase[k] = ph;
                }
                break;
            }
        }
    }

    // texture
    const float subLvl = juce::jlimit (0.0f, 1.0f, p.sub->load() + m[dstSub]);
    const float noiseLvl = juce::jlimit (0.0f, 1.0f, p.noise->load() + m[dstNoise]);
    const double subDt = midiToHz (basePitch - 12.0) / sr;
    for (int i = 0; i < n; ++i)
    {
        float add = 0.0f;
        if (subLvl > 0.0f)
        {
            add += (float) std::sin (twoPi * s.subPhase) * subLvl;
            s.subPhase += subDt; if (s.subPhase >= 1.0) s.subPhase -= 1.0;
        }
        if (noiseLvl > 0.0f) add += (rng.nextFloat() * 2.0f - 1.0f) * noiseLvl * 0.5f;
        tl[i] += add; tr[i] += add;
    }

    // filter
    const float fe = s.fenv.getNextSample();
    for (int i = 1; i < n; ++i) s.fenv.getNextSample();
    const int ftype = (int) p.ftype->load();
    const float reso = juce::jlimit (0.0f, 1.0f, p.reso->load() + m[dstReso]);
    const float fenvAmt = p.fenv->load() + m[dstFilterEnv];
    const double oct = fenvAmt * fe * 6.0 + m[dstCutoff] * 6.0 + p.keytrack->load() * (note - 60) / 12.0;
    const float maxFc = (float) juce::jmin (20000.0, sr * 0.45);
    const float fc = juce::jlimit (20.0f, maxFc, (float) (p.cutoff->load() * std::pow (2.0, oct)));
    const float k = 2.0f - 1.97f * reso;

    if (ftype == fVowel)
    {
        static const float fm[5][3] = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 },
                                        { 570, 840, 2410 },  { 300, 870, 2240 } };
        const float vp = juce::jlimit (0.0f, 4.0f, (float) (std::log2 (fc / 20.0f) / std::log2 (1000.0f)) * 4.0f);
        const int a = juce::jmin (3, (int) vp);
        const float fr = vp - (float) a;
        const float kv = 0.6f - 0.5f * reso;
        for (int c = 0; c < 2; ++c)
        {
            s.f1[c].setFreq (fm[a][0] + (fm[a + 1][0] - fm[a][0]) * fr, kv, sr);
            s.f2[c].setFreq (fm[a][1] + (fm[a + 1][1] - fm[a][1]) * fr, kv, sr);
            s.f3[c].setFreq (fm[a][2] + (fm[a + 1][2] - fm[a][2]) * fr, kv, sr);
        }
    }
    else
    {
        const float g = (float) std::tan (pi * fc / sr);
        for (int c = 0; c < 2; ++c) { s.f1[c].set (g, k); s.f2[c].set (g, k); }
    }

    const float drive = juce::jlimit (0.0f, 1.0f, p.drive->load() + m[dstDrive]);
    const float dr = 1.0f + drive * 5.0f;
    const float dn = 1.0f / std::tanh (dr);
    const float combDelay = juce::jlimit (2.0f, (float) Comb::size - 4.0f, (float) (sr / fc));
    const float combFb = (ftype == fCombNeg ? -1.0f : 1.0f) * (0.3f + 0.68f * reso);

    // level / pan
    const float level = p.level->load() * juce::jlimit (0.0f, 2.0f, 1.0f + m[dstLevel]);
    const float pan = juce::jlimit (-1.0f, 1.0f, p.pan->load() + m[dstPan]);
    const float pa = (pan + 1.0f) * (float) pi * 0.25f;
    const float panL = std::cos (pa) * 1.4142f, panR = std::sin (pa) * 1.4142f;
    const float velGain = 1.0f - p.velSens->load() * (1.0f - vel);

    for (int i = 0; i < n; ++i)
    {
        float y[2] = { std::tanh (tl[i] * dr) * dn, std::tanh (tr[i] * dr) * dn };
        for (int c = 0; c < 2; ++c)
        {
            float lp, bp, hp;
            switch (ftype)
            {
                case fLP12:  s.f1[c].tick (y[c], lp, bp, hp); y[c] = lp; break;
                case fLP24:  { s.f1[c].tick (y[c], lp, bp, hp); float l2, b2, h2; s.f2[c].tick (lp, l2, b2, h2); y[c] = l2; break; }
                case fBand:  s.f1[c].tick (y[c], lp, bp, hp); y[c] = bp * 1.5f; break;
                case fHigh:  s.f1[c].tick (y[c], lp, bp, hp); y[c] = hp; break;
                case fNotch: s.f1[c].tick (y[c], lp, bp, hp); y[c] = lp + hp; break;
                case fCombPos:
                case fCombNeg: y[c] = s.comb[c].process (y[c], combDelay, combFb); break;
                default:
                {
                    float b1, b2, b3;
                    s.f1[c].tick (y[c], lp, b1, hp);
                    s.f2[c].tick (y[c], lp, b2, hp);
                    s.f3[c].tick (y[c], lp, b3, hp);
                    y[c] = (b1 + 0.7f * b2 + 0.4f * b3) * 1.6f;
                    break;
                }
            }
        }
        const float gain = s.amp.getNextSample() * level * velGain * 0.16f;
        outL[pos + i] += y[0] * gain * panL;
        outR[pos + i] += y[1] * gain * panR;
    }
    return true;
}

void Voice::renderGranular (int, LayerState& s, const LayerParams& p, const float* m,
                            const double* groupSemis, int groups, double basePitch, float spread,
                            float* tl, float* tr, int n)
{
    const double sr = getSampleRate();
    const auto& data = s.sample->data;
    const double len = (double) data.size();
    if (len < 16) return;

    const double size = juce::jlimit (0.005, 2.0, p.grSize->load() / 1000.0 * std::pow (2.0, m[dstGrainSize] * 3.0));
    const double density = juce::jlimit (0.5, 200.0, p.grDensity->load() * std::pow (2.0, m[dstGrainDensity] * 3.0));
    double gpos = p.grPos->load() + m[dstTimbre];
    gpos -= std::floor (gpos);
    const float spray = p.grSpray->load();
    const float pitchRand = p.grPitch->load();
    const double rootHz = midiToHz (s.sample->rootNote);
    const float overlap = (float) (density * size);
    const float norm = 1.0f / juce::jmax (1.0f, std::sqrt (overlap));

    for (int i = 0; i < n; ++i)
    {
        s.grainTimer -= 1.0;
        if (s.grainTimer <= 0.0)
        {
            s.grainTimer += sr / density * (0.7 + 0.6 * rng.nextDouble());
            for (auto& g : s.grains)
            {
                if (g.active) continue;
                g.active = true;
                g.age = 0;
                g.len = juce::jmax (8, (int) (size * sr));
                double startPos = gpos + spray * (rng.nextDouble() * 2.0 - 1.0) * 0.5;
                startPos -= std::floor (startPos);
                g.pos = startPos * len;
                const double semis = basePitch + groupSemis[rng.nextInt (groups)]
                                   + pitchRand * 12.0 * (rng.nextDouble() * 2.0 - 1.0);
                g.inc = midiToHz (semis) / rootHz * s.sample->sampleRate / sr;
                const float pp = (rng.nextFloat() * 2.0f - 1.0f) * spread;
                const float a = (pp + 1.0f) * (float) pi * 0.25f;
                g.gl = std::cos (a) * norm;
                g.gr = std::sin (a) * norm;
                break;
            }
        }

        float l = 0, r = 0;
        for (auto& g : s.grains)
        {
            if (! g.active) continue;
            const float w = 0.5f - 0.5f * (float) std::cos (twoPi * g.age / g.len);
            const int i0 = (int) g.pos;
            const float fr = (float) (g.pos - i0);
            const float a = data[(size_t) i0], b = data[(size_t) ((i0 + 1) % (int) len)];
            const float v = (a + (b - a) * fr) * w;
            l += v * g.gl; r += v * g.gr;
            g.pos += g.inc;
            if (g.pos >= len - 1) g.pos -= len - 1;
            if (++g.age >= g.len) g.active = false;
        }
        tl[i] += l * 1.4f;
        tr[i] += r * 1.4f;
    }
}

//==============================================================================
void NebSynth::configure (int maxVoices, bool monoMode, bool legatoMode)
{
    if (monoMode != mono)
    {
        allNotesOff (0, true);
        heldStack.clear();
        monoNote = -1;
    }
    polyLimit = juce::jlimit (1, getNumVoices(), maxVoices);
    mono = monoMode;
    legato = legatoMode;
}

void NebSynth::noteOn (int ch, int midiNote, float velocity)
{
    if (! mono) { Synthesiser::noteOn (ch, midiNote, velocity); return; }

    const juce::ScopedLock sl (lock);
    heldStack.removeFirstMatchingValue (midiNote);
    heldStack.add (midiNote);
    auto* v = static_cast<Voice*> (getVoice (0));
    if (legato && v->isVoiceActive() && v->isHeld())
        v->legatoTo (midiNote, velocity);
    else
        startVoice (v, getSound (0).get(), ch, midiNote, velocity);
    monoNote = midiNote;
}

void NebSynth::noteOff (int ch, int midiNote, float velocity, bool allowTailOff)
{
    if (! mono) { Synthesiser::noteOff (ch, midiNote, velocity, allowTailOff); return; }

    const juce::ScopedLock sl (lock);
    heldStack.removeFirstMatchingValue (midiNote);
    if (midiNote != monoNote) return;
    auto* v = static_cast<Voice*> (getVoice (0));
    if (! heldStack.isEmpty())
    {
        const int prev = heldStack.getLast();
        if (legato && v->isVoiceActive()) v->legatoTo (prev, velocity);
        else startVoice (v, getSound (0).get(), ch, prev, 0.8f);
        monoNote = prev;
    }
    else
    {
        stopVoice (v, velocity, allowTailOff);
        monoNote = -1;
    }
}

void NebSynth::allNotesOff (int ch, bool allowTailOff)
{
    heldStack.clear();
    monoNote = -1;
    Synthesiser::allNotesOff (ch, allowTailOff);
}

juce::SynthesiserVoice* NebSynth::findFreeVoice (juce::SynthesiserSound* sound, int ch, int midiNote, bool steal) const
{
    int active = 0;
    for (auto* v : voices)
        if (v->isVoiceActive()) ++active;
    if (active >= polyLimit)
    {
        // steal the oldest voice, preferring ones already released
        juce::SynthesiserVoice* best = nullptr;
        for (int pass = 0; pass < 2 && best == nullptr; ++pass)
            for (auto* v : voices)
                if (v->isVoiceActive() && (pass == 1 || ! v->isKeyDown()))
                    if (best == nullptr || v->wasStartedBefore (*best)) best = v;
        juce::ignoreUnused (sound, ch, midiNote);
        return best;
    }
    return Synthesiser::findFreeVoice (sound, ch, midiNote, steal);
}
} // namespace neb
