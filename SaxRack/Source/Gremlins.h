// GREMLINS: an onset detector hears ANY sound at the input (a note, a clap, a key click,
// your voice) and spawns peculiar little sound creatures in response.
//
//  Silly:  Boing, Pew, Quack, Bubbles, Robot, Whistle, Kazoo
//  Harsh:  Clank, Grinder, Stutter, Ghost (reversed), Screech, Static
//
// SILLY<->HARSH weights the two families, DIRT drives the whole gremlin bus through an
// industrial clipper + bit crusher + sample-rate reducer, FOLLOW ties their pitch to yours.
#pragma once
#include "DSP.h"
#include <atomic>

namespace rs
{
struct GremlinParams
{
    bool on = false;
    float sens = 0.6f, chance = 0.7f, harsh = 0.3f, dirt = 0.2f, follow = 0.5f, swarm = 0.3f, level = 0.6f;
    int palette = 0;
};

class Gremlins
{
public:
    enum Type { Boing, Pew, Quack, Bubbles, Robot, Whistle, Kazoo,          // silly
                Clank, Grinder, Stutter, Ghost, Screech, Static, numTypes };  // harsh
    static constexpr int numSilly = 7;
    static constexpr int numVoices = 10;
    static constexpr int historySize = 16;

    static const char* typeName (int t)
    {
        static const char* n[] = { "BOING!", "PEW PEW", "QUACK", "blub blub", "BEEP-BOOP", "WHEEE", "BZZZT",
                                   "CLANK!", "GRRRZZT", "ST-ST-STUTTER", "ESREVER", "SKREEEE", "KSSHHH" };
        return n[juce::jlimit (0, numTypes - 1, t)];
    }

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        ringSize = 1;
        while (ringSize < (int) (sr * 2.0)) ringSize <<= 1;
        ring.assign ((size_t) ringSize, 0.0f);
        for (auto& v : voices) v.prepare (sr);
        fastEnv.setup (0.4f, 25.0f, sr);
        slowEnv.setup (60.0f, 300.0f, sr);
        reset();
    }

    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        ringPos = 0;
        for (auto& v : voices) v.active = false;
        fastEnv.reset(); slowEnv.reset();
        refractory = 0;
        for (auto& p : pendingSpawn) p.delay = -1;
        holdL = holdR = 0; holdCount = 0;
    }

    /** input: the raw (input-gain only) mono signal. Adds the gremlin bus to L/R. */
    void process (const float* input, float* L, float* R, int n, bool voiced, float trackedHz,
                  const GremlinParams& p, Fade& fade)
    {
        const float thresh = dbToGain (-28.0f - p.sens * 32.0f);      // -28 .. -60 dBFS
        const float ratio = 2.6f - p.sens * 1.3f;
        const int refrac = (int) (0.09 * sr);
        const float dirtSteps = std::pow (2.0f, 14.0f - p.dirt * 11.0f);
        const float dirtNorm = 1.6f / std::sqrt (1.0f + p.dirt * 18.0f);

        for (int i = 0; i < n; ++i)
        {
            const float x = input[i];
            ring[(size_t) ringPos] = x;
            ringPos = (ringPos + 1) & (ringSize - 1);

            // --- onset detection
            const float f = fastEnv.process (x), s = slowEnv.process (x);
            if (refractory > 0) --refractory;
            else if (f > thresh && f > s * ratio)
            {
                refractory = refrac;
                meters.onsets.fetch_add (1);
                if (p.on && dice() < p.chance) spawnSwarm (voiced, trackedHz, p);
            }

            // --- delayed swarm members
            for (auto& ps : pendingSpawn)
                if (ps.delay >= 0 && --ps.delay < 0) spawn (ps.type, ps.hz, p);

            // --- render
            float l = 0, r = 0;
            for (auto& v : voices)
                if (v.active) v.render (*this, l, r);

            // --- DIRT: clip -> bit crush -> sample-rate reduce
            if (p.dirt > 0.001f)
            {
                const float drive = 1.0f + p.dirt * 18.0f;
                l = fastTanh (l * drive) * dirtNorm;
                r = fastTanh (r * drive) * dirtNorm;
                const float steps = dirtSteps;
                l = std::round (l * steps) / steps;
                r = std::round (r * steps) / steps;
                const int hold = 1 + (int) (p.dirt * p.dirt * 10.0f);
                if (++holdCount >= hold) { holdCount = 0; holdL = l; holdR = r; }
                l = holdL; r = holdR;
            }
            const float g = fade.next() * p.level * 1.1f;
            L[i] += l * g;
            R[i] += r * g;
        }
    }

    struct Meters
    {
        std::atomic<int> onsets { 0 }, spawned { 0 };
        std::atomic<int> history[historySize] {};     // last spawned types, newest at spawned % historySize
    } meters;

    std::atomic<bool> manualSpawn { false };
    void pollManual (bool voiced, float hz, const GremlinParams& p)
    {
        if (manualSpawn.exchange (false)) spawnSwarm (voiced, hz, p);
    }

private:
    float dice() { return 0.5f + 0.5f * rng.next(); }

    /** Palettes give each preset its own cast of creatures. */
    static bool inPalette (int type, int palette)
    {
        switch (palette)
        {
            case 1: return type == Boing || type == Whistle || type == Bubbles || type == Quack;        // Cartoon
            case 2: return type == Pew || type == Robot || type == Screech || type == Static;           // Sci-Fi
            case 3: return type == Kazoo || type == Stutter || type == Ghost || type == Quack;          // Voice Mangler
            case 4: return type == Clank || type == Grinder || type == Static || type == Stutter;       // Scrapyard
            default: return true;                                                                        // All
        }
    }

    int pickType (float harsh, int palette)
    {
        float w[numTypes], total = 0;
        for (int t = 0; t < numTypes; ++t)
        {
            w[t] = inPalette (t, palette) ? (t < numSilly ? 1.0f - harsh : harsh) / (t < numSilly ? 7.0f : 6.0f) : 0.0f;
            total += w[t];
        }
        if (total <= 1.0e-6f)      // palette has nothing in the chosen family: use the whole palette
            for (int t = 0; t < numTypes; ++t) { w[t] = inPalette (t, palette) ? 1.0f : 0.0f; total += w[t]; }
        float r = dice() * total;
        for (int t = 0; t < numTypes; ++t)
        {
            r -= w[t];
            if (r <= 0.0f && w[t] > 0.0f) return t;
        }
        for (int t = numTypes - 1; t >= 0; --t) if (w[t] > 0.0f) return t;
        return 0;
    }

    void spawnSwarm (bool voiced, float hz, const GremlinParams& p)
    {
        const int count = 1 + (int) (p.swarm * 3.0f * dice() + p.swarm);
        for (int k = 0; k < count; ++k)
        {
            float f = 0.0f;
            const bool follow = voiced && dice() < p.follow;
            if (follow) f = hz;
            const int t = pickType (p.harsh, p.palette);
            // creatures that chew on your sound wait until they have heard enough of it
            const double capture = t == Stutter ? 0.2 : (t == Kazoo || t == Ghost) ? 0.42 : 0.0;
            if (k == 0 && capture <= 0.0) spawn (t, f, p);
            else
                for (auto& ps : pendingSpawn)
                    if (ps.delay < 0)
                    {
                        ps.delay = (int) (sr * (capture + (k == 0 ? 0.0 : 0.05 + 0.35 * dice())));
                        ps.type = t; ps.hz = f;
                        break;
                    }
        }
    }

    void spawn (int type, float followHz, const GremlinParams&)
    {
        Voice* v = nullptr;
        for (auto& c : voices) if (! c.active) { v = &c; break; }
        if (v == nullptr)
        {
            v = &voices[0];
            for (auto& c : voices) if (c.t > v->t) v = &c;
        }
        v->start (type, followHz, *this);
        const int idx = meters.spawned.fetch_add (1);
        meters.history[(size_t) (idx % historySize)].store (type);
    }

    //==========================================================================================
    struct Voice
    {
        void prepare (double s)
        {
            sr = (float) s;
            comb.prepare ((int) (s / 30.0) + 8);
            slice.assign ((size_t) (s * 0.45), 0.0f);
        }

        void start (int type, float followHz, Gremlins& g)
        {
            kind = type;
            t = 0;
            active = true;
            pan = g.rng.next() * 0.8f;
            for (auto& p : ph) p = 0;
            for (auto& m : mode) m = {};
            env = 1.0f;
            const float d = g.dice(), e = g.dice();
            auto pick = [&] (float lo, float hi) { return followHz > 0 ? followHz : lo * std::pow (hi / lo, d); };

            switch (type)
            {
                case Boing:   f0 = pick (90, 300) * (followHz > 0 ? 0.5f : 1.0f); dur = 0.7f; break;
                case Pew:     f0 = pick (500, 1400) * (followHz > 0 ? 2.0f : 1.0f); dur = 0.16f + 0.1f * e; reps = 1 + (int) (e * 2.99f); break;
                case Quack:   f0 = pick (160, 320); dur = 0.17f; reps = 1 + (int) (e * 2.99f); break;
                case Bubbles: f0 = pick (300, 900); dur = 0.5f + 0.4f * e; break;
                case Robot:   f0 = pick (300, 1200); dur = 0.6f + 0.6f * e; break;
                case Whistle: f0 = pick (500, 1000); dur = 0.75f; break;
                case Kazoo:   f0 = pick (180, 400); dur = 0.45f; grabSlice (g, 0.4f); break;
                case Clank:   f0 = pick (70, 220) * (followHz > 0 ? 0.25f : 1.0f); dur = 1.1f; initModes(); break;
                case Grinder: f0 = pick (40, 120) * (followHz > 0 ? 0.25f : 1.0f); dur = 0.5f + 0.6f * e; comb.reset(); break;
                case Stutter: dur = 0.0f; grabSlice (g, 0.06f + 0.12f * e); reps = 4 + (int) (d * 9.0f); dur = sliceLen / sr * (float) reps; break;
                case Ghost:   grabSlice (g, 0.42f); dur = sliceLen / sr; f0 = fastExp2 ((float) ((int) (d * 4.0f) - 2) * 0.5f); break;
                case Screech: f0 = (followHz > 0 ? followHz * (float) (3 + (int) (d * 4.0f)) : 1500.0f + 2000.0f * d); dur = 0.8f; break;
                default:      dur = 0.25f + 0.35f * e; break;
            }
            f0 = std::min (f0, sr * 0.2f);
            durS = (int) (dur * sr);
            stepTimer = 0;
            stepHz = f0;
        }

        void render (Gremlins& g, float& L, float& R)
        {
            const float tt = (float) t / sr;
            float s = 0.0f;
            switch (kind)
            {
                case Boing:   // cartoon spring: decaying pitch wobble
                {
                    const float f = f0 * (1.0f + 0.45f * fastExp2 (-1.4426950f * (tt * 4.0f)) * fastSin2Pi (11.0f * tt));
                    s = osc (0, f) * fastExp2 (-1.4426950f * (tt * 5.0f)) * 0.8f;
                    s += 0.25f * osc (1, f * 2.0f) * fastExp2 (-1.4426950f * (tt * 9.0f));
                    break;
                }
                case Pew:     // laser zaps
                {
                    const float seg = dur / (float) reps;
                    const float lt = std::fmod (tt, seg);
                    const float f = f0 * 0.25f + f0 * 3.0f * fastExp2 (-1.4426950f * (lt * 22.0f));
                    s = sq (0, f) * fastExp2 (-1.4426950f * (lt * 14.0f)) * 0.5f;
                    break;
                }
                case Quack:   // buzzy formant with a pitch drop, repeated
                {
                    const float lt = std::fmod (tt, dur / (float) reps);
                    const float f = f0 * (1.0f - 0.35f * lt / (dur / (float) reps));
                    const float buzz = saw (0, f);
                    if ((t & 15) == 0) { form[0].set (900.0f - 500.0f * lt * 6.0f, 6.0f, sr); form[1].set (1300.0f, 7.0f, sr); }
                    s = (form[0].bpNorm (buzz) + 0.6f * form[1].bpNorm (buzz)) * std::min (1.0f, lt * 150.0f) * fastExp2 (-1.4426950f * (lt * 9.0f)) * 1.3f;
                    break;
                }
                case Bubbles: // rising sine blips
                {
                    if (--stepTimer <= 0) { stepTimer = (int) (sr * (0.025f + 0.05f * g.dice())); stepHz = f0 * (0.6f + g.dice()); blip = 0; }
                    const float bt = (float) blip++ / sr;
                    s = osc (0, stepHz * (1.0f + bt * 60.0f)) * fastExp2 (-1.4426950f * (bt * 45.0f)) * 0.7f;
                    break;
                }
                case Robot:   // bit-crushed random beeps, sometimes sliding
                {
                    if (--stepTimer <= 0)
                    {
                        stepTimer = (int) (sr * (0.035f + 0.07f * g.dice()));
                        static const float steps[] = { 1.0f, 1.25f, 1.5f, 2.0f, 0.75f, 1.333f };
                        target = f0 * steps[(int) (g.dice() * 5.99f)];
                        slide = g.dice() < 0.3f;
                        if (! slide) stepHz = target;
                    }
                    if (slide) stepHz += (target - stepHz) * 0.002f;
                    s = std::round (sq (0, stepHz) * 3.0f) / 3.0f * 0.35f;
                    break;
                }
                case Whistle: // slide whistle up and back, with vibrato
                {
                    const float shape = fastSin2Pi (0.5f * (std::min (1.0f, tt / dur)));
                    const float f = f0 * (1.0f + 1.6f * shape) * (1.0f + 0.02f * fastSin2Pi (6.0f * tt));
                    s = osc (0, f) * shape * 0.45f;
                    break;
                }
                case Kazoo:   // your last phrase, buzzed through a kazoo
                {
                    const float x = sliceAt (tt * sr);
                    const float buzz = saw (0, f0) * 0.5f + 0.5f;
                    if ((t & 15) == 0) { form[0].set (650.0f, 5.0f, sr); form[1].set (1700.0f, 6.0f, sr); }
                    const float y = x * buzz * 3.0f;
                    s = (form[0].bpNorm (y) + form[1].bpNorm (y)) * fastSin2Pi (0.5f * (tt / dur)) * 1.5f;
                    break;
                }
                case Clank:   // free-bar metal modes, hard clipped
                {
                    static const float ratios[6] = { 1.0f, 2.76f, 5.40f, 8.93f, 13.34f, 18.64f };
                    const float exc = t < 40 ? g.rng.next() : 0.0f;
                    for (int k = 0; k < 6; ++k)
                    {
                        auto& m = mode[k];
                        if (t == 0)
                        {
                            const float w = twoPi * std::min (f0 * ratios[k], sr * 0.45f) / sr;
                            const float rr = std::exp (-1.0f / (sr * (0.9f / (1.0f + (float) k * 0.6f))));
                            m.a1 = 2.0f * rr * std::cos (w); m.a2 = -rr * rr;
                        }
                        const float y = exc + m.a1 * m.y1 + m.a2 * m.y2;
                        m.y2 = m.y1; m.y1 = y;
                        s += y * (k == 0 ? 1.0f : 0.6f);
                    }
                    s = juce::jlimit (-0.7f, 0.7f, s * 0.4f);
                    break;
                }
                case Grinder: // comb-resonated noise + sub saw, folded and rattling
                {
                    const float period = sr / std::max (f0, 20.0f);
                    const float c = comb.read (period);
                    comb.push (g.rng.next() * 0.3f + c * 0.9f);
                    float y = c * 0.7f + saw (0, f0 * 0.5f) * 0.5f;
                    y *= 0.6f + 0.4f * fastSin2Pi (27.0f * tt);
                    y = fastSin (y * 3.5f);            // wavefolder
                    s = y * std::min (1.0f, tt * 40.0f) * fastExp2 (-1.4426950f * (tt * 1.8f)) * 0.5f;
                    break;
                }
                case Stutter: // a slice of what you just played, retriggered and crushed
                {
                    const float pos = std::fmod (tt * sr, sliceLen);
                    const int rep = (int) (tt * sr / sliceLen);
                    const float x = sliceAt (pos * (1.0f + 0.06f * (float) rep));
                    const float steps = fastExp2 (8.0f - (float) rep * 0.5f);
                    s = std::round (x * steps) / steps * 1.6f * gateEdge (pos, sliceLen);
                    break;
                }
                case Ghost:   // reversed and re-pitched, ring-modulated
                {
                    const float pos = sliceLen - 1.0f - std::fmod (tt * sr * f0, sliceLen);
                    s = sliceAt (pos) * osc (0, 90.0f) * 2.0f * fastSin2Pi (0.5f * (std::min (1.0f, tt / dur)));
                    break;
                }
                case Screech: // runaway feedback tone
                {
                    const float grow = std::min (1.0f, tt * 3.0f);
                    const float f = f0 * (1.0f + 0.01f * fastSin2Pi (7.0f * tt));
                    s = fastTanh (osc (0, f) * (1.0f + grow * 6.0f)) * grow * std::min (1.0f, (dur - tt) * 8.0f) * 0.32f;
                    break;
                }
                default:      // static bursts
                {
                    const float n = g.rng.next();
                    const float crackle = g.dice() < 0.02f ? (g.rng.next() > 0 ? 0.9f : -0.9f) : 0.0f;
                    s = (std::round (n * 4.0f) / 4.0f * 0.25f + crackle) * (0.5f + 0.5f * fastSin2Pi (13.0f * tt)) * 0.6f;
                    break;
                }
            }
            if (++t >= durS) active = false;
            const float gl = fastCos2Pi ((pan + 1.0f) * 0.125f), gr = fastSin2Pi ((pan + 1.0f) * 0.125f);
            L += s * gl;
            R += s * gr;
        }

        float osc (int i, float f) { ph[i] += f / sr; ph[i] -= std::floor (ph[i]); return fastSin2Pi (ph[i]); }
        float saw (int i, float f) { ph[i] += f / sr; ph[i] -= std::floor (ph[i]); return 2.0f * ph[i] - 1.0f; }
        float sq (int i, float f) { ph[i] += f / sr; ph[i] -= std::floor (ph[i]); return ph[i] < 0.5f ? 1.0f : -1.0f; }
        static float gateEdge (float pos, float len) { return std::min (1.0f, std::min (pos, len - pos) / 64.0f); }

        void grabSlice (Gremlins& g, float seconds)
        {
            sliceLen = std::min ((float) slice.size(), seconds * sr);
            const int n = (int) sliceLen;
            for (int k = 0; k < n; ++k)
                slice[(size_t) k] = g.ring[(size_t) ((g.ringPos - n + k) & (g.ringSize - 1))];
        }
        float sliceAt (float pos) const
        {
            const int i = juce::jlimit (0, (int) sliceLen - 1, (int) pos);
            return slice[(size_t) i];
        }
        void initModes() { for (auto& m : mode) m = {}; }

        struct Mode { float a1 = 0, a2 = 0, y1 = 0, y2 = 0; };
        float sr = 48000.0f;
        bool active = false;
        int kind = 0, t = 0, durS = 1, reps = 1, stepTimer = 0, blip = 0;
        float f0 = 440, dur = 0.5f, pan = 0, env = 1, stepHz = 440, target = 440, sliceLen = 1;
        bool slide = false;
        float ph[2] {};
        Mode mode[6];
        Svf form[2];
        DelayLine comb;
        std::vector<float> slice;
    };

    struct Pending { int delay = -1; int type = 0; float hz = 0; };

    double sr = 48000.0;
    std::vector<float> ring;
    int ringSize = 1, ringPos = 0;
    std::array<Voice, numVoices> voices;
    std::array<Pending, 8> pendingSpawn;
    EnvFollower fastEnv, slowEnv;
    int refractory = 0, holdCount = 0;
    float holdL = 0, holdR = 0;
    Noise rng;
};
} // namespace rs
