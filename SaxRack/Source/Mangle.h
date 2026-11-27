// MANGLE effects: Whammy (pitch bend), Ring Mod, Freeze (granular infinite sustain), Slicer (tempo gate).
#pragma once
#include "DSP.h"
#include <atomic>

namespace rs
{
//==============================================================================
struct WhammyParams { bool on = false; int mode = 0; float amount = 12, pedal = 0, rate = 0.5f, glide = 80, mix = 1; };

/** Pitch bender: Pedal (expression), LFO (dive/rise), Scoop (each note slides in), Random (stepped jumps). */
class Whammy
{
public:
    void prepare (double s) { sr = s; shifter.prepare (sr, 80.0f); reset(); }
    void reset() { shifter.reset(); shift = 0; lfo = 0; scoop = 0; randTimer = 0; randTarget = 0; lastNote = -100; }

    void process (float* mono, int n, bool voiced, float midi, const WhammyParams& p, Fade& fade)
    {
        const int note = (int) std::lround (midi);
        if (voiced && (note != lastNote)) { scoop = 1.0f; lastNote = note; }
        if (! voiced) lastNote = -100;
        if (voiced) shifter.setWindowForPeriod ((float) sr / midiToHz (midi), 28.0f, sr);

        const float glideC = p.glide < 1.0f ? 1.0f : msToCoeff (p.glide, sr);
        const float scoopC = std::exp (-1.0f / (std::max (p.glide, 5.0f) * 0.001f * (float) sr));
        for (int i = 0; i < n; ++i)
        {
            float target = 0.0f;
            switch (p.mode)
            {
                case 0: target = p.amount * p.pedal; break;
                case 1:
                    lfo += p.rate / (float) sr;
                    if (lfo >= 1.0f) lfo -= 1.0f;
                    target = p.amount * (0.5f - 0.5f * std::cos (twoPi * lfo));
                    break;
                case 2: target = p.amount * scoop; scoop *= scoopC; break;
                default:
                    if (--randTimer <= 0)
                    {
                        randTimer = (int) (sr / std::max (0.05f, p.rate));
                        randTarget = std::round (rng.next() * std::abs (p.amount));
                    }
                    target = randTarget;
                    break;
            }
            shift += (target - shift) * (p.mode == 2 ? 1.0f : glideC);
            const float x = mono[i];
            const float s = shifter.process (x, std::pow (2.0f, shift / 12.0f));
            const float y = x + p.mix * (s - x);
            const float f = fade.next();
            mono[i] = x + f * (y - x);
        }
        currentShift.store (shift);
    }

    std::atomic<float> currentShift { 0 };

private:
    double sr = 48000.0;
    PitchShifter shifter;
    float shift = 0, lfo = 0, scoop = 0, randTarget = 0;
    int randTimer = 0, lastNote = -100;
    Noise rng;
};

//==============================================================================
struct RingParams { bool on = false; int mode = 0, wave = 0; float freq = 440, ratio = 1.5f, rate = 0.3f, sweep = 0, mix = 0.7f; };

/** Ring modulator: the carrier follows your pitch (musical bells) or sits at a fixed frequency (robots, metal). */
class RingMod
{
public:
    void prepare (double s) { sr = s; ph = lfo = 0; carrierSm = 440; }

    void process (float* mono, int n, bool voiced, float midi, const RingParams& p, Fade& fade)
    {
        const float base = p.mode == 0 ? (voiced ? midiToHz (midi) * p.ratio : carrierSm) : p.freq;
        const float c = msToCoeff (15.0f, sr);
        for (int i = 0; i < n; ++i)
        {
            lfo += p.rate / (float) sr;
            if (lfo >= 1.0f) lfo -= 1.0f;
            carrierSm += (base - carrierSm) * c;
            const float hz = carrierSm * std::pow (2.0f, p.sweep * 2.0f * std::sin (twoPi * lfo));
            ph += hz / (float) sr;
            ph -= std::floor (ph);
            float car;
            switch (p.wave)
            {
                case 1:  car = std::tanh (6.0f * std::sin (twoPi * ph)); break;            // soft square
                case 2:  car = 1.0f - 4.0f * std::abs (ph - 0.5f); break;                  // triangle
                default: car = std::sin (twoPi * ph); break;
            }
            const float x = mono[i];
            const float y = x + p.mix * (x * car * 1.3f - x);
            const float f = fade.next();
            mono[i] = x + f * (y - x);
        }
    }

private:
    double sr = 48000.0;
    float ph = 0, lfo = 0, carrierSm = 440;
};

//==============================================================================
struct FreezeParams { bool on = false, hold = false, stack = false; int mode = 0, pitch = 1; float decay = 0, width = 0.8f, level = 0.8f, attack = 300; };

/** Granular freeze: captures a slice of your sound and sustains it forever as a shimmering pad.
    Manual: hold FREEZE. Auto: every steadily held new note is captured and stacked (up to 3 layers). */
class Freeze
{
public:
    static constexpr int numLayers = 3, grainsPerLayer = 5;

    void prepare (double s)
    {
        sr = s;
        ringSize = 1;
        while (ringSize < (int) (sr * 1.0)) ringSize <<= 1;
        ring.assign ((size_t) ringSize, 0.0f);
        capLen = (int) (sr * 0.4);
        for (auto& l : layers) l.buf.assign ((size_t) capLen, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        ringPos = 0;
        for (auto& l : layers) { l.active = false; l.gain = 0; }
        wasHeld = false; stableSamples = 0; candidate = -1; lastFrozen = -1;
    }

    /** input: the mono core signal (recorded continuously). Adds the frozen pad to L/R. */
    void process (const float* input, float* L, float* R, int n, bool voiced, float midi, const FreezeParams& p, Fade& fade)
    {
        for (int i = 0; i < n; ++i) { ring[(size_t) ringPos] = input[i]; ringPos = (ringPos + 1) & (ringSize - 1); }

        if (p.on)
        {
            if (p.mode == 0)
            {
                if (p.hold && ! wasHeld) capture (p, ! p.stack);
                if (! p.hold && wasHeld) for (auto& l : layers) l.releasing = true;
            }
            else if (voiced)
            {
                const int note = (int) std::lround (midi);
                if (note != candidate) { candidate = note; stableSamples = 0; }
                stableSamples += n;
                if (stableSamples > (int) (0.35 * sr) && note != lastFrozen) { lastFrozen = note; capture (p, false); }
            }
            else { stableSamples = 0; candidate = -1; }
        }
        wasHeld = p.hold;

        static const float pitchRatio[] = { 0.5f, 1.0f, 1.4983f, 2.0f };
        const float rate = pitchRatio[juce::jlimit (0, 3, p.pitch)];
        const float attackStep = 1.0f / (std::max (p.attack, 10.0f) * 0.001f * (float) sr);
        const float releaseCoef = std::exp (-1.0f / (1.2f * (float) sr));
        const float decayCoef = p.decay <= 0.001f ? 1.0f : std::exp (-1.0f / ((0.5f + 12.0f * (1.0f - p.decay)) * (float) sr));
        int activeCount = 0;
        float fv[64];
        jassert (n <= 64);
        for (int i = 0; i < n; ++i) fv[i] = fade.next();

        for (auto& l : layers)
        {
            if (! l.active) continue;
            ++activeCount;
            for (int i = 0; i < n; ++i)
            {
                if (l.releasing || ! p.on) l.gain *= releaseCoef;
                else
                {
                    if (l.attacking) { l.peak += attackStep; if (l.peak >= 1.0f) { l.peak = 1.0f; l.attacking = false; } }
                    else l.peak *= decayCoef;
                    l.gain = l.peak;
                }
                float sl = 0, sr2 = 0;
                for (auto& g : l.grains)
                {
                    const float t = (float) g.age / (float) grainLen;
                    const float w = 0.5f - 0.5f * std::cos (twoPi * t);
                    const float pos = g.start + (float) g.age * rate * g.detune;
                    const int i0 = std::min (capLen - 2, (int) pos);
                    const float fr = pos - (float) i0;
                    const float v = (l.buf[(size_t) i0] + fr * (l.buf[(size_t) i0 + 1] - l.buf[(size_t) i0])) * w;
                    sl += v * g.gl; sr2 += v * g.gr;
                    if (++g.age >= grainLen) respawn (g, rate, p.width);
                }
                const float f = fv[i] * l.gain * p.level * 0.55f;
                L[i] += sl * f;
                R[i] += sr2 * f;
            }
            if (l.gain < 1.0e-4f && (l.releasing || l.peak < 1.0e-4f)) l.active = false;
        }
        layersActive.store (activeCount);
        frozen.store (activeCount > 0);
    }

    std::atomic<int> layersActive { 0 };
    std::atomic<bool> frozen { false };

private:
    struct Grain { float start = 0, detune = 1, gl = 0.7f, gr = 0.7f; int age = 0; };
    struct Layer
    {
        std::vector<float> buf;
        std::array<Grain, grainsPerLayer> grains;
        bool active = false, releasing = false, attacking = false;
        float gain = 0, peak = 0;
        int born = 0;
    };

    void capture (const FreezeParams& p, bool replaceAll)
    {
        if (replaceAll) for (auto& l : layers) if (l.active) l.releasing = true;
        Layer* target = nullptr;
        for (auto& l : layers) if (! l.active) { target = &l; break; }
        if (target == nullptr)
        {
            target = &layers[0];
            for (auto& l : layers) if (l.born < target->born) target = &l;
        }
        for (int k = 0; k < capLen; ++k)
            target->buf[(size_t) k] = ring[(size_t) ((ringPos - capLen + k) & (ringSize - 1))];
        // normalise the slice so quiet and loud notes freeze at a similar level
        float pk = 0;
        for (float v : target->buf) pk = std::max (pk, std::abs (v));
        if (pk > 1.0e-4f) for (auto& v : target->buf) v *= 0.5f / pk;
        grainLen = (int) (sr * 0.11);
        static const float pitchRatio[] = { 0.5f, 1.0f, 1.4983f, 2.0f };
        const float rate = pitchRatio[juce::jlimit (0, 3, p.pitch)];
        int k = 0;
        for (auto& g : target->grains)
        {
            respawn (g, rate, p.width);
            g.age = (grainLen * k++) / grainsPerLayer;
        }
        target->active = true;
        target->releasing = false;
        target->attacking = true;
        target->gain = target->peak = 0;
        target->born = ++births;
    }

    void respawn (Grain& g, float rate, float width)
    {
        g.age = 0;
        const float span = (float) grainLen * rate * 1.02f;
        g.start = (0.5f + 0.5f * rng.next()) * std::max (0.0f, (float) capLen - span - 4.0f);
        g.detune = 1.0f + 0.004f * rng.next();
        const float pan = rng.next() * width;
        g.gl = std::cos ((pan + 1.0f) * pi * 0.25f);
        g.gr = std::sin ((pan + 1.0f) * pi * 0.25f);
    }

    double sr = 48000.0;
    std::vector<float> ring;
    int ringSize = 1, ringPos = 0, capLen = 1, grainLen = 4096, births = 0;
    std::array<Layer, numLayers> layers;
    bool wasHeld = false;
    int stableSamples = 0, candidate = -1, lastFrozen = -1;
    Noise rng;
};

//==============================================================================
struct SlicerParams { bool on = false, pingPong = false; int pattern = 0, rate = 1; float depth = 1, smooth = 5, swing = 0; };

/** Tempo-synced gate driven by 16-step patterns. */
class Slicer
{
public:
    static const char* patternSteps (int p)
    {
        static const char* pats[] = { "x.xxx.x.x.xxx.x.", "..x...x...x...x.", "xxxx....xx.x....", "xxxxxx......xx..",
                                      "x.x.x.x.x.x.x.x.", "xx.x..xxx.x.xx.." };
        return pats[juce::jlimit (0, 5, p)];
    }

    void prepare (double s) { sr = s; gL = gR = 1; for (auto& b : randomBar) b = true; lastBar = -1; }

    /** beat: position in quarter notes at the first sample; beatsPerSample: tempo increment. */
    void process (float* L, float* R, int n, double beat, double beatsPerSample, const SlicerParams& p, Fade& fade)
    {
        static const double stepBeats[] = { 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
        const double sb = stepBeats[juce::jlimit (0, 4, p.rate)];
        const float c = msToCoeff (std::max (1.0f, p.smooth), sr);
        for (int i = 0; i < n; ++i)
        {
            const double pos = (beat + (double) i * beatsPerSample) / sb;
            double pair = pos * 0.5;
            const double inPair = pair - std::floor (pair);
            const double split = 0.5 + 0.25 * (double) p.swing;          // swing pushes the off-step later
            const long long pairIdx = (long long) std::floor (pair);
            const int step = (int) (((pairIdx * 2 + (inPair < split ? 0 : 1)) % 16 + 16) % 16);

            const long long bar = (long long) std::floor (pos / 16.0);
            if (p.pattern == 6 && bar != lastBar)
            {
                lastBar = bar;
                for (auto& b : randomBar) b = rng.next() > -0.2f;
            }
            const bool on = p.pattern == 6 ? randomBar[(size_t) step] : patternSteps (p.pattern)[step] == 'x';
            const float closed = 1.0f - p.depth;
            float tl = on ? 1.0f : closed, tr = tl;
            if (p.pingPong && on) { if (step % 2 == 0) tr = closed; else tl = closed; }
            gL += (tl - gL) * c;
            gR += (tr - gR) * c;
            const float f = fade.next();
            L[i] *= 1.0f + f * (gL - 1.0f);
            R[i] *= 1.0f + f * (gR - 1.0f);
            currentStep.store (step);
        }
    }

    std::atomic<int> currentStep { 0 };
    bool randomStep (int s) const { return randomBar[(size_t) (s & 15)]; }

private:
    double sr = 48000.0;
    float gL = 1, gR = 1;
    std::array<bool, 16> randomBar {};
    long long lastBar = -1;
    Noise rng;
};
} // namespace rs
