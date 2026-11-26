#include "Rack.h"
#include "Params.h"

namespace rs
{
namespace
{
    constexpr float revBaseMs[8] = { 31.3f, 37.9f, 41.3f, 47.9f, 53.1f, 61.7f, 67.3f, 73.7f };
    constexpr float revApMs[4]   = { 4.7f, 3.6f, 12.7f, 9.3f };
    constexpr float revSign[8]   = { 1, -1, 1, -1, 1, -1, 1, -1 };
    constexpr double syncBeats[] = { 0.0, 2.0, 1.0, 1.5, 2.0 / 3.0, 0.5, 0.75, 1.0 / 3.0, 0.25 };

    inline void storeMax (std::atomic<float>& a, float v)
    {
        if (v > a.load (std::memory_order_relaxed)) a.store (v, std::memory_order_relaxed);
    }
}

void Rack::prepare (double sampleRate)
{
    sr = sampleRate;
    tracker.prepare (sr);

    for (auto* f : { &dynFade, &octFade, &synFade, &wahFade, &drvFade, &eqFade, &harFade, &modFade, &dlyFade, &revFade, &genFade, &grmFade })
        f->prepare (sr);

    gateEnv.setup (0.3f, 40.0f, sr);
    octEnv.setup (1.0f, 60.0f, sr);
    octUpShift.prepare (sr, 60.0f);
    octUpShift.setWindowMs (25.0f, sr);
    synAmpEnv.setup (4.0f, 70.0f, sr);
    wahEnv.setup (3.0f, 140.0f, sr);

    os = std::make_unique<juce::dsp::Oversampling<float>> (1, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    os->initProcessing ((size_t) chunk);
    drvDc.setCutoff (18.0f, sr);
    drvPre.setCutoff (90.0f, sr);

    harShift1.prepare (sr, 80.0f);
    harShift2.prepare (sr, 80.0f);

    for (int c = 0; c < 2; ++c)
    {
        modDl[c].prepare ((int) (sr * 0.05));
        rotXover[c].set (800.0f, 0.5f, sr);
        dlyDl[c].prepare ((int) (sr * 2.7));
        dlyHp[c].setCutoff (110.0f, sr);
    }
    dlyDuckEnv.setup (5.0f, 260.0f, sr);

    revPre.prepare ((int) (sr * 0.22));
    for (int k = 0; k < 4; ++k)
    {
        revApLen[k] = revApMs[k] * 0.001f * (float) sr;
        revAp[k].prepare ((int) revApLen[k] + 4);
    }
    for (int i = 0; i < 8; ++i) revLine[i].prepare ((int) (sr * 0.16));
    shimmer.prepare (sr, 120.0f);
    shimmer.setWindowMs (90.0f, sr);
    shimHp.setCutoff (400.0f, sr);

    generator.prepare (sr);
    gremlins.prepare (sr);
    looper.prepare (sr);
    inGainSm.reset (sr, 0.03);
    outGainSm.reset (sr, 0.03);
    widthSm.reset (sr, 0.05);
    reset();
}

void Rack::reset()
{
    tracker.reset();
    gateEnv.reset(); octEnv.reset(); synAmpEnv.reset(); wahEnv.reset(); dlyDuckEnv.reset();
    gateGain = 1; compEnv = 0; gateOpen = true;
    octPre1.reset(); octPre2.reset(); octPost1.reset(); octPost2.reset(); octUpShift.reset();
    octHigh = ff1 = ff2 = false;
    synFilter.reset(); synAmp = 0; synPhase = synSubPhase = 0;
    wahFilter.reset(); wahSmooth = 0;
    if (os) os->reset();
    drvTone.reset(); drvDc.reset(); drvPre.reset();
    for (auto* b : { &eqHp1, &eqHp2, &eqBody, &eqMid, &eqAir }) b->reset();
    harShift1.reset(); harShift2.reset();
    for (int c = 0; c < 2; ++c)
    {
        modDl[c].reset(); dlyDl[c].reset(); dlyLp[c].reset(); dlyHp[c].reset(); rotXover[c].reset();
        modFbState[c] = 0;
        for (auto& s : apState[c]) s = 0;
    }
    revPre.reset();
    for (auto& a : revAp) a.reset();
    for (int i = 0; i < 8; ++i) { revLine[i].reset(); revDamp[i].reset(); }
    shimmer.reset(); shimmerFb = 0; shimHp.reset();
    limGain = 1.0f;
    dlySmooth = 0;
    generator.reset();
    gremlins.reset();
}

void Rack::updateFilters (const RackParams& p)
{
    const float now[5] = { p.eqLowCut, p.eqBody, p.eqMid, p.eqMidF, p.eqAir };
    if (std::equal (now, now + 5, lastEq)) return;
    std::copy (now, now + 5, lastEq);
    eqHp1.highPass (p.eqLowCut, 0.5412f, sr);
    eqHp2.highPass (p.eqLowCut, 1.3066f, sr);
    eqBody.peak (210.0f, 0.8f, p.eqBody, sr);
    eqMid.peak (p.eqMidF, 1.1f, p.eqMid, sr);
    eqAir.highShelf (7500.0f, p.eqAir, sr);
}

float Rack::harmonyShift (int interval, const RackParams& p) const
{
    if (interval == 0) return 0.0f;
    if (p.harMode == 1) return (float) interval;

    const float refOffset = 12.0f * std::log2 (p.tunerRef / 440.0f);
    const int note = (int) std::lround (trackedMidi - refOffset);
    const int keyConcert = ((p.harKey - params::writtenOffset (p.instrument)) % 12 + 12) % 12;
    const int rel = ((note - keyConcert) % 12 + 12) % 12;
    const auto& steps = params::scaleSteps (p.harScale);
    const int n = (int) steps.size();

    int idx = 0;
    for (int i = 0; i < n; ++i)
        if (steps[(size_t) i] <= rel) idx = i;

    const int target = idx + interval;
    const int oct = target >= 0 ? target / n : -((-target + n - 1) / n);
    const int ti = target - oct * n;
    return (float) (steps[(size_t) ti] + oct * 12 - steps[(size_t) idx]);
}

void Rack::process (const float* inL, const float* inR, float* outL, float* outR, int numSamples,
                    const RackParams& p, double bpm)
{
    inGainSm.setTargetValue (dbToGain (p.inGain));
    outGainSm.setTargetValue (dbToGain (p.outGain));
    widthSm.setTargetValue (p.outWidth);
    updateFilters (p);

    for (int pos = 0; pos < numSamples; pos += chunk)
    {
        const int n = std::min (chunk, numSamples - pos);
        for (int i = 0; i < n; ++i)
            mono[i] = inR != nullptr ? 0.5f * (inL[pos + i] + inR[pos + i]) : inL[pos + i];

        processChunk (n, p, bpm);

        for (int i = 0; i < n; ++i) { outL[pos + i] = L[i]; outR[pos + i] = R[i]; }
    }
}

void Rack::processChunk (int n, const RackParams& p, double bpm)
{
    const float refOffset = 12.0f * std::log2 (p.tunerRef / 440.0f);

    // ---- input gain + pitch tracking --------------------------------------------------
    float inPk = 0;
    for (int i = 0; i < n; ++i)
    {
        mono[i] *= inGainSm.getNextValue();
        tracker.push (mono[i]);
        rawIn[i] = mono[i];
        inPk = std::max (inPk, std::abs (mono[i]));
    }
    storeMax (meters.inPeak, inPk);
    const bool voiced = tracker.isVoiced();
    if (voiced) trackedMidi = tracker.getMidi();
    meters.voiced.store (voiced);
    meters.pitchMidi.store (tracker.getMidi() - refOffset);
    meters.pitchConfidence.store (tracker.getConfidence());

    // ---- dynamics: gate + opto-style compressor ---------------------------------------
    dynFade.set (p.dynOn);
    if (dynFade.active())
    {
        const float thr = -8.0f - p.compAmt * 30.0f;
        const float ratio = 1.5f + p.compAmt * 6.0f;
        const float slope = 1.0f - 1.0f / ratio;
        const float makeup = p.compAmt > 0.001f ? -thr * slope * 0.45f : 0.0f;
        const float atk = msToCoeff (juce::jmap (p.compSpeed, 30.0f, 1.5f), sr);
        const float rel = msToCoeff (juce::jmap (p.compSpeed, 450.0f, 60.0f), sr);
        const float gOpen = msToCoeff (0.8f, sr), gClose = msToCoeff (90.0f, sr);
        const bool gateOn = p.gateThr > -79.9f;
        float maxGr = 0;

        for (int i = 0; i < n; ++i)
        {
            const float x = mono[i];
            const float gdb = gainToDb (gateEnv.process (x));
            if (! gateOn) gateOpen = true;
            else if (gdb > p.gateThr) gateOpen = true;
            else if (gdb < p.gateThr - 5.0f) gateOpen = false;
            const float gt = gateOpen ? 1.0f : 0.0f;
            gateGain += (gt > gateGain ? gOpen : gClose) * (gt - gateGain);
            float y = x * gateGain;

            const float r = std::abs (y);
            compEnv += (r > compEnv ? atk : rel) * (r - compEnv);
            const float over = gainToDb (compEnv) - thr;
            float gr = 0;
            if (over > 3.0f) gr = over * slope;
            else if (over > -3.0f) gr = (over + 3.0f) * (over + 3.0f) / 12.0f * slope;
            y *= dbToGain (makeup - gr);
            maxGr = std::max (maxGr, gr * (p.compAmt > 0.001f ? 1.0f : 0.0f));

            const float f = dynFade.next();
            mono[i] = x + f * (y - x);
        }
        storeMax (meters.gainReduction, maxGr * dynFade.v);
    }

    const float f0 = midiToHz (trackedMidi);

    // ---- octaver: analog-style flip-flop divider + pitch-shifted octave up -------------
    octFade.set (p.octOn);
    if (octFade.active())
    {
        if (voiced) octUpShift.setWindowForPeriod ((float) sr / f0, 22.0f, sr);
        const float preCut = voiced ? juce::jlimit (40.0f, 900.0f, f0 * 1.25f) : 350.0f;
        octPre1.set (preCut, 0.6f, sr);
        octPre2.set (preCut, 0.6f, sr);
        const float postCut = voiced ? juce::jlimit (35.0f, 600.0f, f0) : 250.0f;
        octPost1.set (postCut, 0.7f, sr);
        octPost2.set (postCut * 0.5f, 0.7f, sr);

        for (int i = 0; i < n; ++i)
        {
            const float x = mono[i];
            const float xf = octPre2.lp (octPre1.lp (x));
            const float th = 0.12f * octEnv.process (xf);
            if (! octHigh && xf > th) { octHigh = true; ff1 = ! ff1; if (ff1) ff2 = ! ff2; }
            else if (octHigh && xf < -th) octHigh = false;

            const float s1 = octPost1.lp (xf * (ff1 ? 1.0f : -1.0f)) * 2.2f;
            const float s2 = octPost2.lp (xf * (ff2 ? 1.0f : -1.0f)) * 2.8f;
            const float up = octUpShift.process (x, 2.0f);
            const float y = x * p.octDry + s1 * p.octSub1 + s2 * p.octSub2 + up * p.octUp;
            const float f = octFade.next();
            mono[i] = x + f * (y - x);
        }
    }

    // ---- tracking synth: a mono synth voice that follows your pitch and breath ---------
    synFade.set (p.synOn);
    if (synFade.active())
    {
        const float targetHz = midiToHz (trackedMidi + (float) (p.synOct - 2) * 12.0f);
        const float glide = p.synGlide < 1.0f ? 1.0f : msToCoeff (p.synGlide * 0.5f, sr);
        const float gateCoef = msToCoeff (6.0f, sr);
        const float gateT = voiced ? 1.0f : 0.0f;
        const float q = 0.6f + p.synRes * p.synRes * 11.0f;

        for (int i = 0; i < n; ++i)
        {
            const float x = mono[i];
            synHz += (targetHz - synHz) * glide;
            synAmp += (gateT - synAmp) * gateCoef;
            const float dt = synHz / (float) sr;
            synPhase += dt;
            if (synPhase >= 1.0f) synPhase -= 1.0f;
            synSubPhase += dt * 0.5f;
            if (synSubPhase >= 1.0f) synSubPhase -= 1.0f;

            float v = 0;
            switch (p.synWave)
            {
                case 0: v = 2.0f * synPhase - 1.0f - polyBlep (synPhase, dt); break;
                case 1:
                {
                    float p2 = synPhase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
                    v = 0.6f * ((synPhase < 0.5f ? 1.0f : -1.0f) + polyBlep (synPhase, dt) - polyBlep (p2, dt));
                    break;
                }
                case 2:
                {
                    const float dts = dt * 0.5f;
                    float p2 = synSubPhase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
                    const float sq = (synSubPhase < 0.5f ? 1.0f : -1.0f) + polyBlep (synSubPhase, dts) - polyBlep (p2, dts);
                    v = 0.6f * (2.0f * synPhase - 1.0f - polyBlep (synPhase, dt)) + 0.45f * sq;
                    break;
                }
                default: v = std::sin (twoPi * synPhase) + 0.3f * std::sin (2.0f * twoPi * synPhase); break;
            }

            const float e = synAmpEnv.process (x);
            if ((i & 15) == 0)
                synFilter.set (p.synCut * std::pow (2.0f, p.synEnv * 4.5f * clamp01 (e * 6.0f)), q, sr);
            const float s = synFilter.lp (v) * e * 2.2f * synAmp;
            const float y = x + s * p.synLvl * 0.9f;
            const float f = synFade.next();
            mono[i] = x + f * (y - x);
        }
    }

    // ---- envelope filter / auto-wah ----------------------------------------------------
    wahFade.set (p.wahOn);
    if (wahFade.active())
    {
        const float sm = msToCoeff (4.0f, sr);
        const float sensGain = 0.5f + p.wahSens * p.wahSens * 14.0f;
        for (int i = 0; i < n; ++i)
        {
            const float x = mono[i];
            const float e = wahEnv.process (x);
            float m = 0;
            switch (p.wahMode)
            {
                case 0: m = clamp01 (e * sensGain); break;
                case 1: m = 1.0f - clamp01 (e * sensGain); break;
                case 2:
                    wahLfo += p.wahRate / (float) sr;
                    if (wahLfo >= 1.0f) wahLfo -= 1.0f;
                    m = 0.5f - 0.5f * std::cos (twoPi * wahLfo);
                    break;
                default: m = p.wahSens; break;   // Pedal: the SENS knob is the pedal position
            }
            wahSmooth += (m - wahSmooth) * sm;
            if ((i & 1) == 0)
                wahFilter.set (p.wahFreq * std::pow (2.0f, p.wahRange * wahSmooth), p.wahQ, sr);
            wahFilter.process (x);
            const float w = wahFilter.bpOut * wahFilter.k * 1.9f + wahFilter.lpOut * 0.3f;
            const float y = x + p.wahMix * (w - x);
            const float f = wahFade.next();
            mono[i] = x + f * (y - x);
        }
        meters.wahPos.store (wahSmooth);
    }

    // ---- drive (2x oversampled) -------------------------------------------------------
    drvFade.set (p.drvOn);
    if (drvFade.active())
    {
        std::copy (mono, mono + n, dryCopy);
        for (int i = 0; i < n; ++i) mono[i] = drvPre.hp (mono[i]);

        float* chans[1] = { mono };
        juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
        auto up = os->processSamplesUp (block);
        float* u = up.getChannelPointer (0);
        const int un = (int) up.getNumSamples();
        const float g = dbToGain (p.drvDrive * 42.0f);
        const float c = p.drvChar;

        // growl: the "hum while you blow" roughness, a wandering 25-55 Hz amplitude flutter
        growlRate += 0.08f * ((25.0f + 30.0f * c + 8.0f * noise.next()) - growlRate);
        const float growlInc = growlRate / (float) (sr * 2.0);

        for (int j = 0; j < un; ++j)
        {
            const float x = u[j];
            float s = 0;
            switch (p.drvType)
            {
                case 0:
                {
                    const float b = 0.1f + c * 0.5f;
                    s = std::tanh (g * x + b) - std::tanh (b);
                    break;
                }
                case 1:
                {
                    s = juce::jlimit (-0.8f, 0.8f, std::tanh (g * 2.5f * x) * 1.3f);
                    s += c * 0.85f * (1.6f * std::abs (s) - 0.6f - s);   // blend in octave-up fuzz
                    break;
                }
                case 2:
                {
                    growlPhase += growlInc;
                    if (growlPhase >= 1.0f) growlPhase -= 1.0f;
                    const float depth = 0.35f + 0.55f * c;
                    const float m = 1.0f - depth * (0.5f + 0.5f * std::sin (twoPi * growlPhase));
                    s = std::tanh (g * 1.5f * x * m * (1.0f + 0.2f * noise.next()));
                    break;
                }
                default:
                {
                    const float y = g * 0.7f * x;
                    s = 0.6366f * std::atan (y + c * 0.3f * y * std::abs (y) / (1.0f + std::abs (y)));
                    break;
                }
            }
            u[j] = s;
        }
        os->processSamplesDown (block);

        drvTone.set (700.0f * std::pow (2.0f, p.drvTone * 4.5f), 0.6f, sr);
        static constexpr float typeTrim[] = { 1.0f, 0.55f, 0.6f, 0.85f };
        const float comp = dbToGain (p.drvLevel) * typeTrim[juce::jlimit (0, 3, p.drvType)] / (1.0f + p.drvDrive * 3.0f);
        for (int i = 0; i < n; ++i)
        {
            const float dry = dryCopy[i];
            float y = drvTone.lp (drvDc.hp (mono[i])) * comp;
            y = dry + p.drvMix * (y - dry);
            const float f = drvFade.next();
            mono[i] = dry + f * (y - dry);
        }
    }

    // ---- EQ ------------------------------------------------------------------------------
    eqFade.set (p.eqOn);
    if (eqFade.active())
    {
        for (int i = 0; i < n; ++i)
        {
            const float x = mono[i];
            const float y = eqAir.process (eqMid.process (eqBody.process (eqHp2.process (eqHp1.process (x)))));
            const float f = eqFade.next();
            mono[i] = x + f * (y - x);
        }
    }

    // ---- morph generator (mono part): the sax becomes the zone's synth voice -------------
    genFade.set (p.gen.on);
    const float concertMidi = trackedMidi - refOffset;
    if (genFade.active())
        generator.processMorph (mono, n, voiced, trackedMidi, p.gen, genFade);

    // ---- harmonizer: mono -> stereo ----------------------------------------------------
    std::copy (mono, mono + n, L);
    std::copy (mono, mono + n, R);
    harFade.set (p.harOn);
    if (harFade.active())
    {
        if (voiced || p.harMode == 1)
        {
            harHeld1 = harmonyShift (p.harInt1, p);
            harHeld2 = harmonyShift (p.harInt2, p);
        }
        meters.harmShift1.store (harHeld1);
        meters.harmShift2.store (harHeld2);
        const float r1 = std::pow (2.0f, harHeld1 / 12.0f), r2 = std::pow (2.0f, harHeld2 / 12.0f);

        // pitch-synchronous window: half the window is a whole number of input periods
        if (voiced)
        {
            const float period = (float) sr / f0;
            harShift1.setWindowForPeriod (period, 30.0f, sr);
            harShift2.setWindowForPeriod (period, 30.0f, sr);
        }

        const float pan1 = -p.harSpread, pan2 = p.harSpread;
        const float l1 = std::cos ((pan1 + 1.0f) * pi * 0.25f) * 1.4142f, rr1 = std::sin ((pan1 + 1.0f) * pi * 0.25f) * 1.4142f;
        const float l2 = std::cos ((pan2 + 1.0f) * pi * 0.25f) * 1.4142f, rr2 = std::sin ((pan2 + 1.0f) * pi * 0.25f) * 1.4142f;
        const float coef = msToCoeff (20.0f, sr);

        for (int i = 0; i < n; ++i)
        {
            const float x = mono[i];
            harRatio1 += (r1 - harRatio1) * coef;
            harRatio2 += (r2 - harRatio2) * coef;
            const float v1 = harShift1.process (x, harRatio1) * p.harLvl1;
            const float v2 = harShift2.process (x, harRatio2) * p.harLvl2;
            const float f = harFade.next();
            L[i] = x + f * (v1 * l1 + v2 * l2);
            R[i] = x + f * (v1 * rr1 + v2 * rr2);
        }
    }

    // ---- generative bloom (stereo), triggered by the zone you play in -------------------
    generator.processBloom (L, R, n, voiced, concertMidi, p.gen, bpm, p.gen.on);

    // ---- gremlins: any sound at the input spawns peculiar creatures ----------------------
    grmFade.set (p.grm.on);
    gremlins.pollManual (voiced, midiToHz (trackedMidi), p.grm);
    if (grmFade.active() || p.grm.on)
        gremlins.process (rawIn, L, R, n, voiced, midiToHz (trackedMidi), p.grm, grmFade);

    // ---- modulation: chorus / flanger / phaser / rotary --------------------------------
    modFade.set (p.modOn);
    if (modFade.active())
    {
        const float ms = (float) sr * 0.001f;
        for (int i = 0; i < n; ++i)
        {
            modPhase += p.modRate / (float) sr;
            if (modPhase >= 1.0f) modPhase -= 1.0f;
            rotPhaseLo += p.modRate * 0.83f / (float) sr;
            if (rotPhaseLo >= 1.0f) rotPhaseLo -= 1.0f;
            const float f = modFade.next();

            for (int c = 0; c < 2; ++c)
            {
                float& io = c == 0 ? L[i] : R[i];
                const float in = io;
                float fx = in;
                switch (p.modType)
                {
                    case 0:
                    {
                        const float lfo = std::sin (twoPi * (modPhase + 0.25f * (float) c));
                        const float d = (14.0f + (0.5f + p.modDepth * 6.0f) * lfo) * ms;
                        fx = modDl[c].read (d);
                        modDl[c].push (in + fx * p.modFb * 0.3f);
                        break;
                    }
                    case 1:
                    {
                        const float lfo = 0.5f + 0.5f * std::sin (twoPi * (modPhase + 0.25f * (float) c));
                        const float d = (0.25f + p.modDepth * 3.5f * lfo) * ms;
                        const float wet = modDl[c].read (d);
                        modDl[c].push (in + wet * p.modFb * 0.92f);
                        fx = 0.6f * (in + wet);
                        break;
                    }
                    case 2:
                    {
                        const float lfo = 0.5f + 0.5f * std::sin (twoPi * (modPhase + 0.25f * (float) c));
                        const float fc = 180.0f * std::pow (2.0f, p.modDepth * 5.5f * lfo);
                        const float t = std::tan (pi * fc / (float) sr);
                        const float a = (t - 1.0f) / (t + 1.0f);
                        float y = in + modFbState[c] * p.modFb * 0.85f;
                        for (int s = 0; s < 6; ++s)
                        {
                            const float o = a * y + apState[c][s];
                            apState[c][s] = y - a * o;
                            y = o;
                        }
                        modFbState[c] = y;
                        fx = 0.6f * (in + y);
                        break;
                    }
                    default:
                    {
                        const float ph = twoPi * (modPhase + 0.5f * (float) c);
                        rotXover[c].process (in);
                        const float lo = rotXover[c].lpOut, hi = in - lo;
                        const float d = (1.0f + p.modDepth * 0.9f * (1.0f + std::sin (ph))) * ms;
                        const float wetHi = modDl[c].read (d);
                        modDl[c].push (hi);
                        const float amHi = 1.0f - 0.45f * p.modDepth * (0.5f + 0.5f * std::cos (ph));
                        const float amLo = 1.0f - 0.3f * p.modDepth * (0.5f + 0.5f * std::sin (twoPi * rotPhaseLo + pi * (float) c));
                        fx = wetHi * amHi + lo * amLo;
                        break;
                    }
                }
                const float y = in + p.modMix * (fx - in);
                io = in + f * (y - in);
            }
        }
    }

    // ---- delay: tempo-synced tape echo with ping-pong and ducking -------------------------
    dlyFade.set (p.dlyOn);
    if (dlyFade.active())
    {
        double ms = p.dlyTime;
        if (p.dlySync > 0) ms = 60000.0 / juce::jlimit (30.0, 300.0, bpm) * syncBeats[p.dlySync];
        const float target = (float) (juce::jlimit (5.0, 2500.0, ms) * sr * 0.001);
        if (dlySmooth <= 0.0f) dlySmooth = target;
        const float smooth = msToCoeff (150.0f, sr);
        const float wobSm = msToCoeff (300.0f, sr);
        const float fbG = p.dlyFb * 1.05f;
        for (int c = 0; c < 2; ++c) dlyLp[c].setCutoff (p.dlyTone, sr);

        for (int i = 0; i < n; ++i)
        {
            dlySmooth += (target - dlySmooth) * smooth;
            wobPhase += 0.55f / (float) sr;
            if (wobPhase >= 1.0f) wobPhase -= 1.0f;
            if (++wobCounter >= 2048) { wobCounter = 0; wobTarget = noise.next(); }
            wobRand += (wobTarget - wobRand) * wobSm;
            const float mod = p.dlyWobble * (float) sr * (0.0025f * std::sin (twoPi * wobPhase) + 0.0015f * wobRand);
            const float d = std::max (1.0f, dlySmooth + mod);

            const float rL = dlyDl[0].read (d), rR = dlyDl[1].read (d);
            const float fL = std::tanh (dlyLp[0].lp (dlyHp[0].hp (rL)) * fbG);
            const float fR = std::tanh (dlyLp[1].lp (dlyHp[1].hp (rR)) * fbG);
            const float inM = 0.5f * (L[i] + R[i]);
            if (p.dlyPP) { dlyDl[0].push (inM + fR); dlyDl[1].push (fL); }
            else         { dlyDl[0].push (L[i] + fL); dlyDl[1].push (R[i] + fR); }

            const float duck = 1.0f - p.dlyDuck * 0.92f * clamp01 (dlyDuckEnv.process (inM) * 7.0f);
            const float f = dlyFade.next() * p.dlyMix * duck;
            L[i] += f * rL;
            R[i] += f * rR;
        }
    }
    else
    {
        dlySmooth = 0.0f;
    }

    // ---- reverb: 8-line feedback delay network + shimmer ---------------------------------
    revFade.set (p.revOn);
    if (revFade.active())
    {
        const float scale = 0.4f + p.revSize * 1.35f;
        float gains[8];
        for (int k = 0; k < 8; ++k)
        {
            revLen[k] = revBaseMs[k] * scale * 0.001f * (float) sr;
            gains[k] = std::pow (10.0f, -3.0f * revLen[k] / ((float) sr * p.revDecay));
            revDamp[k].setCutoff (18000.0f * std::pow (2.0f, -p.revDamp * 5.0f), sr);
        }
        const float pre = std::max (1.0f, p.revPre * 0.001f * (float) sr);
        const float modDepth = 6.0f * (float) sr / 48000.0f;

        for (int i = 0; i < n; ++i)
        {
            revPre.push (std::tanh (0.5f * (L[i] + R[i]) + shimmerFb));
            float x = revPre.read (pre);
            for (int k = 0; k < 4; ++k)
            {
                const float delayed = revAp[k].at ((int) revApLen[k]);
                const float y = delayed - 0.6f * x;
                revAp[k].push (x + 0.6f * y);
                x = y;
            }

            revModPhase += 0.7f / (float) sr;
            if (revModPhase >= 1.0f) revModPhase -= 1.0f;
            float o[8], sum = 0;
            for (int k = 0; k < 8; ++k)
            {
                float d = revLen[k];
                if (k == 0) d += modDepth * std::sin (twoPi * revModPhase);
                if (k == 5) d += modDepth * std::cos (twoPi * revModPhase * 1.3f);
                o[k] = revDamp[k].lp (revLine[k].read (d));
                sum += o[k];
            }
            const float hh = sum * 0.25f;
            for (int k = 0; k < 8; ++k)
                revLine[k].push (x * revSign[k] * 0.4f + (o[k] - hh) * gains[k]);

            const float wl = 0.32f * (o[0] - o[2] + o[4] - o[6]);
            const float wr = 0.32f * (o[1] - o[3] + o[5] - o[7]);
            if (p.revShimmer > 0.001f)
                shimmerFb = shimHp.hp (shimmer.process (0.5f * (wl + wr), 2.0f)) * p.revShimmer * 0.6f;
            else
                shimmerFb = 0.0f;

            const float f = revFade.next() * p.revMix * 1.3f;
            L[i] += f * wl;
            R[i] += f * wr;
        }
    }

    // ---- looper (records the processed sound) ------------------------------------------
    looper.process (L, R, n, p.loop, bpm);

    // ---- output: width, gain, brickwall-ish safety limiter --------------------------------
    const float limRel = msToCoeff (120.0f, sr);
    float pkL = 0, pkR = 0;
    for (int i = 0; i < n; ++i)
    {
        const float w = widthSm.getNextValue();
        const float m = 0.5f * (L[i] + R[i]), s = 0.5f * (L[i] - R[i]) * w;
        const float g = outGainSm.getNextValue();
        float l = (m + s) * g, r = (m - s) * g;
        limGain += (1.0f - limGain) * limRel;
        const float pk = std::max (std::abs (l), std::abs (r));
        if (pk * limGain > 0.98f) limGain = 0.98f / pk;
        l *= limGain; r *= limGain;
        L[i] = l; R[i] = r;
        pkL = std::max (pkL, std::abs (l));
        pkR = std::max (pkR, std::abs (r));
    }
    storeMax (meters.outPeakL, pkL);
    storeMax (meters.outPeakR, pkR);

    if (! std::isfinite (pkL) || ! std::isfinite (pkR))
    {
        reset();
        std::fill (L, L + n, 0.0f);
        std::fill (R, R + n, 0.0f);
    }
}
} // namespace rs
