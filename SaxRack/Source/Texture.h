// TEXTURE effects: Formant shifter, Talk box, Doubler, Tremolo/Auto-pan, Beat repeat, Spring reverb, Granular cloud.
#pragma once
#include "DSP.h"
#include <atomic>

namespace rs
{
//==============================================================================
struct FormantParams { bool on = false; float shift = 0, mix = 1; };

/** Pitch-synchronous (PSOLA-style) formant shifter: changes the "size" of the horn, keeps the pitch.
    Grains two periods long are emitted once per period (so the pitch stays), but each grain is
    read faster or slower, which moves the resonances up or down. Adds ~25 ms of latency while on. */
class FormantShifter
{
public:
    void prepare (double s)
    {
        sr = s;
        size = 1;
        while (size < (int) (sr * 0.25)) size <<= 1;
        in.assign ((size_t) size, 0.0f);
        out.assign ((size_t) size, 0.0f);
        lat = (int) (sr * 0.025);
        t = 0; nextEmit = 0; voicedSm = 0; period = (float) sr / 200.0f;
    }

    void process (float* mono, int n, bool voiced, float midi, const FormantParams& p, Fade& fade)
    {
        const float f = std::pow (2.0f, p.shift / 12.0f);
        if (voiced) period = juce::jlimit (32.0f, (float) lat * 0.9f, (float) sr / midiToHz (midi));
        const float vc = msToCoeff (20.0f, sr);
        const int mask = size - 1;
        for (int i = 0; i < n; ++i)
        {
            const float x = mono[i];
            in[(size_t) (t & mask)] = x;
            if (t >= nextEmit)
            {
                const int len = std::max (8, (int) (2.0f * period / f));
                const double srcStart = (double) (t - lat) - period;
                for (int k = 0; k < len; ++k)
                {
                    const double src = srcStart + (double) k * f;
                    const long long i0 = (long long) std::floor (src);
                    const float fr = (float) (src - (double) i0);
                    const float a = in[(size_t) (i0 & mask)], b = in[(size_t) ((i0 + 1) & mask)];
                    const float w = 0.5f - 0.5f * std::cos (twoPi * (float) k / (float) len);
                    out[(size_t) ((t + k) & mask)] += (a + fr * (b - a)) * w * f;
                }
                nextEmit = t + (long long) std::max (8.0f, period);
            }
            const float wet = out[(size_t) (t & mask)];
            out[(size_t) (t & mask)] = 0.0f;
            const float dry = in[(size_t) ((t - lat) & mask)];
            voicedSm += ((voiced ? 1.0f : 0.0f) - voicedSm) * vc;
            const float shifted = dry + voicedSm * (wet - dry);
            const float y = dry + p.mix * (shifted - dry);
            const float g = fade.next();
            mono[i] = x + g * (y - x);
            ++t;
        }
    }

private:
    double sr = 48000.0;
    std::vector<float> in, out;
    int size = 1, lat = 1200;
    long long t = 0, nextEmit = 0;
    float voicedSm = 0, period = 240;
};

//==============================================================================
struct TalkParams { bool on = false; int mode = 0; float vowel = 0.0f, rate = 1.0f, sens = 0.5f, reso = 0.5f, mix = 1.0f; };

/** Talk box: three resonant formant filters glide between the vowels A-E-I-O-U. */
class TalkBox
{
public:
    void prepare (double s) { sr = s; env.setup (5.0f, 150.0f, sr); pos = 0; lfo = 0; target = 0; timer = 0; for (auto& f : bp) f.reset(); }

    void process (float* mono, int n, const TalkParams& p, Fade& fade)
    {
        static const float F[5][3] = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };
        const float q = 4.0f + p.reso * 16.0f;
        const float sm = msToCoeff (25.0f, sr);
        for (int i = 0; i < n; ++i)
        {
            const float x = mono[i];
            float v = p.vowel;
            switch (p.mode)
            {
                case 1: lfo += p.rate / (float) sr; if (lfo >= 1.0f) lfo -= 1.0f; v = 0.5f - 0.5f * std::cos (twoPi * lfo); break;
                case 2: v = clamp01 (env.process (x) * (1.0f + p.sens * p.sens * 20.0f)); break;
                case 3:
                    if (--timer <= 0) { timer = (int) (sr / std::max (0.1f, p.rate)); target = 0.5f + 0.5f * rng.next(); }
                    v = target; break;
                default: break;
            }
            pos += (v - pos) * sm;
            if ((i & 7) == 0)
            {
                const float idx = pos * 3.999f;
                const int a = (int) idx;
                const float fr = idx - (float) a;
                for (int k = 0; k < 3; ++k) bp[k].set (F[a][k] + fr * (F[a + 1][k] - F[a][k]), q * (k == 0 ? 0.7f : 1.0f), sr);
            }
            const float w = (bp[0].bpNorm (x) + 0.7f * bp[1].bpNorm (x) + 0.35f * bp[2].bpNorm (x)) * 3.4f;
            const float y = x + p.mix * (w - x);
            const float g = fade.next();
            mono[i] = x + g * (y - x);
        }
        currentVowel.store (pos);
    }

    std::atomic<float> currentVowel { 0 };

private:
    double sr = 48000.0;
    EnvFollower env;
    Svf bp[3];
    float pos = 0, lfo = 0, target = 0;
    int timer = 0;
    Noise rng;
};

//==============================================================================
struct DoublerParams { bool on = false; int players = 1; float detune = 0.4f, timing = 0.4f, width = 0.8f, mix = 0.6f; };

/** ADT-style doubler: 1-3 extra "players", each slightly late and slowly drifting in pitch. */
class Doubler
{
public:
    void prepare (double s)
    {
        sr = s;
        for (auto& d : dl) d.prepare ((int) (sr * 0.12));
        for (int v = 0; v < 3; ++v) { ph[v] = 0.3f * (float) v; drift[v] = 0; driftT[v] = 0; }
    }

    void process (float* L, float* R, int n, const DoublerParams& p, Fade& fade)
    {
        const int voices = juce::jlimit (1, 3, p.players);
        const float ms = (float) sr * 0.001f;
        static const float pans[3] = { -1.0f, 1.0f, -0.25f };
        for (int i = 0; i < n; ++i)
        {
            const float x = 0.5f * (L[i] + R[i]);
            dl[0].push (x);
            float wl = 0, wr = 0;
            for (int v = 0; v < voices; ++v)
            {
                const float rate = 0.23f + 0.11f * (float) v;
                ph[v] += rate / (float) sr;
                if (ph[v] >= 1.0f) ph[v] -= 1.0f;
                if (++counter[v] > (int) (sr * 0.5)) { counter[v] = 0; driftT[v] = rng.next(); }
                drift[v] += (driftT[v] - drift[v]) * 0.00005f;
                // pitch wander in cents -> delay modulation depth
                const float depthSamples = (p.detune * 25.0f / 1731.0f) * (float) sr / (twoPi * rate);
                const float base = (8.0f + p.timing * (12.0f + 14.0f * (float) v)) * ms;
                const float d = base + depthSamples * (0.7f * std::sin (twoPi * ph[v]) + 0.3f * drift[v]) + depthSamples + 2.0f;
                const float s = dl[0].read (d);
                const float pan = pans[v] * p.width;
                wl += s * std::cos ((pan + 1.0f) * pi * 0.25f);
                wr += s * std::sin ((pan + 1.0f) * pi * 0.25f);
            }
            const float g = fade.next() * p.mix * (voices == 1 ? 1.1f : 0.85f);
            L[i] += wl * g;
            R[i] += wr * g;
        }
    }

private:
    double sr = 48000.0;
    DelayLine dl[1];
    float ph[3] {}, drift[3] {}, driftT[3] {};
    int counter[3] {};
    Noise rng;
};

//==============================================================================
struct TremoloParams { bool on = false; int mode = 0, shape = 0, sync = 0; float rate = 5.0f, depth = 0.6f; };

/** Tremolo, auto-pan, and vintage "harmonic" tremolo (lows and highs pulse in opposition). */
class Tremolo
{
public:
    void prepare (double s) { sr = s; ph = 0; for (auto& x : xo) x.set (700.0f, 0.6f, sr); }

    void process (float* L, float* R, int n, double beat, double beatsPerSample, const TremoloParams& p, Fade& fade)
    {
        static const double cycleBeats[] = { 0, 2.0, 1.0, 0.5, 1.0 / 3.0, 0.25 };
        for (int i = 0; i < n; ++i)
        {
            float phase;
            if (p.sync > 0)
            {
                const double b = (beat + (double) i * beatsPerSample) / cycleBeats[juce::jlimit (1, 5, p.sync)];
                phase = (float) (b - std::floor (b));
            }
            else
            {
                ph += p.rate / (float) sr;
                if (ph >= 1.0f) ph -= 1.0f;
                phase = ph;
            }
            auto wave = [&] (float x)
            {
                x -= std::floor (x);
                switch (p.shape)
                {
                    case 1:  return 1.0f - 4.0f * std::abs (x - 0.5f);                  // triangle
                    case 2:  return std::tanh (8.0f * std::sin (twoPi * x));            // soft square
                    default: return std::sin (twoPi * x);
                }
            };
            const float m = wave (phase);
            const float f = fade.next();
            float gl, gr;
            if (p.mode == 1)        // auto-pan
            {
                const float pan = m * p.depth;
                gl = std::cos ((pan + 1.0f) * pi * 0.25f) * 1.4142f;
                gr = std::sin ((pan + 1.0f) * pi * 0.25f) * 1.4142f;
                L[i] *= 1.0f + f * (gl - 1.0f);
                R[i] *= 1.0f + f * (gr - 1.0f);
            }
            else if (p.mode == 2)   // harmonic tremolo
            {
                for (int c = 0; c < 2; ++c)
                {
                    float& io = c == 0 ? L[i] : R[i];
                    xo[c].process (io);
                    const float lo = xo[c].lpOut, hi = io - lo;
                    const float y = lo * (1.0f - p.depth * 0.5f * (1.0f + m)) + hi * (1.0f - p.depth * 0.5f * (1.0f - m));
                    io += f * (y * 1.3f - io);
                }
            }
            else                     // classic tremolo
            {
                const float g = 1.0f - p.depth * 0.5f * (1.0f - m);
                L[i] *= 1.0f + f * (g - 1.0f);
                R[i] *= 1.0f + f * (g - 1.0f);
            }
        }
    }

private:
    double sr = 48000.0;
    float ph = 0;
    Svf xo[2];
};

//==============================================================================
struct RepeatParams { bool on = false, hold = false; int mode = 0, grid = 2, length = 1; float chance = 0.3f, pitch = 0, decay = 0.85f, mix = 1.0f; };

/** Beat repeat: grabs the last slice (1/4 to 1/32, tempo-synced) and stutters it, optionally
    pitching each repeat down or up and fading. Manual (hold the button) or Auto (random, on the beat). */
class BeatRepeat
{
public:
    void prepare (double s)
    {
        sr = s;
        size = 1;
        while (size < (int) (sr * 4.0)) size <<= 1;
        for (auto& b : buf) b.assign ((size_t) size, 0.0f);
        w = 0; active = false; amt = 0; lastBeat = -1; wasHeld = false;
    }

    void process (float* L, float* R, int n, double beat, double beatsPerSample, const RepeatParams& p, Fade& fade)
    {
        static const double gridBeats[] = { 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
        static const double lengthBeats[] = { 0.5, 1.0, 2.0, 4.0 };
        const double spb = 1.0 / std::max (1.0e-9, beatsPerSample);
        const int sliceLen = std::max (64, std::min (size / 2, (int) (gridBeats[juce::jlimit (0, 5, p.grid)] * spb)));
        const float ac = msToCoeff (3.0f, sr);
        const int mask = size - 1;

        if (p.on && p.mode == 0)
        {
            if (p.hold && ! wasHeld) trigger (sliceLen, -1);
            if (! p.hold && wasHeld) active = false;
        }
        wasHeld = p.hold;

        for (int i = 0; i < n; ++i)
        {
            buf[0][(size_t) (w & mask)] = L[i];
            buf[1][(size_t) (w & mask)] = R[i];
            ++w;

            const double b = beat + (double) i * beatsPerSample;
            const long long whole = (long long) std::floor (b);
            if (p.on && p.mode == 1 && whole != lastBeat)
            {
                if (lastBeat >= 0 && ! active && 0.5f + 0.5f * rng.next() < p.chance)
                    trigger (sliceLen, (long long) (lengthBeats[juce::jlimit (0, 3, p.length)] * spb));
                lastBeat = whole;
            }

            float rl = 0, rr = 0;
            if (active)
            {
                const double rd = start + pos;
                const long long i0 = (long long) std::floor (rd);
                const float fr = (float) (rd - (double) i0);
                const float edge = std::min (1.0f, (float) std::min (pos, (double) len - pos) / 48.0f);
                const float gain = std::pow (p.decay, (float) repeat) * edge;
                rl = (buf[0][(size_t) (i0 & mask)] + fr * (buf[0][(size_t) ((i0 + 1) & mask)] - buf[0][(size_t) (i0 & mask)])) * gain;
                rr = (buf[1][(size_t) (i0 & mask)] + fr * (buf[1][(size_t) ((i0 + 1) & mask)] - buf[1][(size_t) (i0 & mask)])) * gain;
                const double speed = juce::jlimit (0.25, 4.0, std::pow (2.0, (double) (p.pitch * (float) repeat) / 12.0));
                pos += speed;
                if (pos >= (double) len) { pos -= (double) len; ++repeat; }
                if (remaining > 0 && --remaining == 0) active = false;
            }
            amt += ((active ? 1.0f : 0.0f) - amt) * ac;
            const float f = fade.next() * amt;
            L[i] += f * (rl - L[i] * p.mix);
            R[i] += f * (rr - R[i] * p.mix);
        }
        repeating.store (active);
    }

    std::atomic<bool> repeating { false };

private:
    void trigger (int sliceLen, long long lengthSamples)
    {
        len = sliceLen;
        start = (double) (w - sliceLen);
        pos = 0;
        repeat = 0;
        remaining = lengthSamples;
        active = true;
    }

    double sr = 48000.0;
    std::vector<float> buf[2];
    int size = 1, len = 1, repeat = 0;
    long long w = 0, remaining = -1, lastBeat = -1;
    double start = 0, pos = 0;
    bool active = false, wasHeld = false;
    float amt = 0;
    Noise rng;
};

//==============================================================================
struct SpringParams { bool on = false; float decay = 0.5f, tone = 0.5f, tension = 0.5f, drip = 0.4f, mix = 0.35f; };

/** Spring reverb: a chain of dispersive allpasses inside a feedback delay gives the classic "boing" chirp. */
class SpringReverb
{
public:
    static constexpr int stages = 28;
    void prepare (double s)
    {
        sr = s;
        for (auto& d : dl) d.prepare ((int) (sr * 0.12));
        for (auto& c : ap) for (auto& z : c) z = { 0, 0 };
        for (auto& l : lp) l.reset();
        for (auto& h : hp) h.setCutoff (900.0f, sr);
    }

    void process (float* L, float* R, int n, const SpringParams& p, Fade& fade)
    {
        const float a = -(0.42f + 0.33f * p.tension);
        const float fb = 0.5f + 0.42f * p.decay;
        const float baseMs = 30.0f + 25.0f * (1.0f - p.tension);
        for (int c = 0; c < 2; ++c) lp[c].setCutoff (1200.0f * std::pow (2.0f, p.tone * 3.0f), sr);
        for (int i = 0; i < n; ++i)
        {
            const float f = fade.next();
            float wet[2];
            for (int c = 0; c < 2; ++c)
            {
                const float x = c == 0 ? L[i] : R[i];
                const float in = x + p.drip * 2.5f * hp[c].hp (x);
                const float d = baseMs * (c == 0 ? 1.0f : 1.13f) * 0.001f * (float) sr;
                const float back = dl[c].read (d);
                float v = in * 0.5f + fb * lp[c].lp (back);
                for (int k = 0; k < stages; ++k)
                {
                    auto& z = ap[(size_t) c][(size_t) k];
                    const float y = a * v + z.x1 - a * z.y1;
                    z.x1 = v; z.y1 = y;
                    v = y;
                }
                dl[c].push (std::tanh (v));
                wet[c] = back;
            }
            L[i] += f * p.mix * wet[0] * 0.9f;
            R[i] += f * p.mix * wet[1] * 0.9f;
        }
    }

private:
    struct Z { float x1, y1; };
    double sr = 48000.0;
    DelayLine dl[2];
    std::array<std::array<Z, stages>, 2> ap {};
    OnePole lp[2], hp[2];
};

//==============================================================================
struct CloudParams { bool on = false; int pitchMode = 0; float density = 0.5f, size = 120, spray = 0.5f, pitch = 0.3f, reverse = 0.3f, width = 0.8f, mix = 0.5f; };

/** Granular cloud: sprays short, re-pitched, sometimes reversed fragments of the last couple of seconds. */
class GranularCloud
{
public:
    static constexpr int numGrains = 28;
    void prepare (double s)
    {
        sr = s;
        size = 1;
        while (size < (int) (sr * 4.0)) size <<= 1;
        ring.assign ((size_t) size, 0.0f);
        w = 0; acc = 0;
        for (auto& g : grains) g.active = false;
    }

    void process (float* L, float* R, int n, const CloudParams& p, Fade& fade)
    {
        const int mask = size - 1;
        const float rate = 2.0f + p.density * p.density * 60.0f;          // grains per second
        const int glen = std::max (64, (int) (p.size * 0.001f * (float) sr));
        const float norm = 0.8f / std::sqrt (std::max (1.0f, rate * (float) glen / (float) sr));
        for (int i = 0; i < n; ++i)
        {
            ring[(size_t) (w & mask)] = 0.5f * (L[i] + R[i]);
            ++w;
            if (p.on)
            {
                acc += rate / (float) sr;
                while (acc >= 1.0f) { acc -= 1.0f; spawn (glen, p); }
            }
            float sl = 0, sr2 = 0;
            for (auto& g : grains)
            {
                if (! g.active) continue;
                const float t = (float) g.age / (float) g.len;
                const float win = 0.5f - 0.5f * std::cos (twoPi * t);
                const double rd = g.start + (double) g.age * g.speed * (g.reverse ? -1.0 : 1.0);
                const long long i0 = (long long) std::floor (rd);
                const float fr = (float) (rd - (double) i0);
                const float a = ring[(size_t) (i0 & mask)], b = ring[(size_t) ((i0 + 1) & mask)];
                const float v = (a + fr * (b - a)) * win;
                sl += v * g.gl; sr2 += v * g.gr;
                if (++g.age >= g.len) g.active = false;
            }
            const float f = fade.next() * p.mix * norm;
            L[i] += sl * f;
            R[i] += sr2 * f;
        }
    }

private:
    struct Grain { bool active = false, reverse = false; double start = 0, speed = 1; int len = 1, age = 0; float gl = 0.7f, gr = 0.7f; };

    void spawn (int glen, const CloudParams& p)
    {
        Grain* g = nullptr;
        for (auto& c : grains) if (! c.active) { g = &c; break; }
        if (g == nullptr) return;
        float semis = 0;
        const float r = rng.next();
        switch (p.pitchMode)
        {
            case 1: { static const float o[] = { -12, 0, 0, 12 }; semis = std::abs (r) < p.pitch ? o[(int) ((0.5f + 0.5f * rng.next()) * 3.99f)] : 0.0f; break; }
            case 2: { static const float o[] = { -12, -5, 0, 7, 12 }; semis = std::abs (r) < p.pitch ? o[(int) ((0.5f + 0.5f * rng.next()) * 4.99f)] : 0.0f; break; }
            default: semis = r * p.pitch * 12.0f; break;
        }
        g->speed = std::pow (2.0, (double) semis / 12.0);
        g->len = glen;
        g->age = 0;
        g->reverse = 0.5f + 0.5f * rng.next() < p.reverse;
        const double need = (double) glen * g->speed + 4.0;
        const double back = need + (0.5 + 0.5 * (double) rng.next()) * (double) p.spray * 1.8 * sr;
        g->start = (double) w - back + (g->reverse ? need : 0.0);
        const float pan = rng.next() * p.width;
        g->gl = std::cos ((pan + 1.0f) * pi * 0.25f);
        g->gr = std::sin ((pan + 1.0f) * pi * 0.25f);
        g->active = true;
    }

    double sr = 48000.0;
    std::vector<float> ring;
    int size = 1;
    long long w = 0;
    float acc = 0;
    std::array<Grain, numGrains> grains;
    Noise rng;
};
} // namespace rs
