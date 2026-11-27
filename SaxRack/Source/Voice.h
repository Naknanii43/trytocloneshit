// Synth voice used by the Morph Generator: one engine, six characters.
// The same voice runs either "continuously" (the morph voice that follows your sax)
// or as a one-shot note with its own envelope (the generative bloom).
#pragma once
#include "DSP.h"

namespace rs
{
enum class GenSound { Bells, Pad, Bowed, Drone, Choir, Swarm };

struct SynthVoice
{
    // ---- setup ---------------------------------------------------------------------------
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        ks.prepare ((int) (sr / 20.0) + 8);
        reset();
    }
    void reset()
    {
        ks.reset();
        for (auto& p : ph) p = 0.0;
        fmPh = 0; env = 0; ksLast = 0; active = false; age = 0;
        for (auto& f : formant) f.reset();
        filt.reset();
    }

    // ---- one-shot notes (bloom) -------------------------------------------------------------
    void start (GenSound s, float midi, float velocity, float pan, int noteLenSamples)
    {
        sound = s;
        hz = midiToHz (midi);
        vel = velocity;
        gainL = fastCos2Pi ((pan + 1.0f) * 0.125f);
        gainR = fastSin2Pi ((pan + 1.0f) * 0.125f);
        attackLeft = (int) (attackSeconds() * sr);
        attackStep = attackLeft > 0 ? 1.0f / (float) attackLeft : 1.0f;
        holdLeft = noteLenSamples;
        decayCoef = fastExp2 (-1.4426950f * (1.0f / (releaseSeconds() * (float) sr)));
        env = attackLeft > 0 ? 0.0f : 1.0f;
        active = true;
        age = 0;
        vowel = (float) (rng.next() * 0.5f + 0.5f);
        if (sound == GenSound::Bowed) { pluckExcite = (int) (sr / hz); ks.reset(); ksLast = 0; }
    }

    /** One-shot render, adds into L/R. */
    void renderAdd (float& L, float& R)
    {
        if (! active) return;
        if (attackLeft > 0) { env = std::min (1.0f, env + attackStep); --attackLeft; }
        else if (holdLeft > 0) --holdLeft;
        else env *= decayCoef;
        if (env < 1.0e-4f && attackLeft <= 0 && holdLeft <= 0) { active = false; return; }

        float excite = 0.0f;
        if (sound == GenSound::Bowed && pluckExcite > 0) { excite = rng.next() * 0.9f; --pluckExcite; }
        const float s = tick (hz, 1.0f, excite, env) * env * vel;
        L += s * gainL;
        R += s * gainR;
    }

    /** Continuous render (morph voice): pitch and loudness come from the sax. */
    float renderContinuous (float freq, float amp, GenSound s)
    {
        sound = s;
        const float excite = sound == GenSound::Bowed ? rng.next() * 0.12f * std::min (1.0f, amp * 6.0f) : 0.0f;
        return tick (freq, amp, excite, std::min (1.0f, amp * 5.0f)) * amp;
    }

    bool active = false;
    int age = 0;

private:
    float attackSeconds() const
    {
        switch (sound)
        {
            case GenSound::Bells: return 0.002f;
            case GenSound::Pad:   return 0.45f;
            case GenSound::Bowed: return 0.0f;
            case GenSound::Drone: return 0.9f;
            case GenSound::Choir: return 0.35f;
            case GenSound::Swarm: default:              return 0.03f;
        }
    }
    float releaseSeconds() const
    {
        switch (sound)
        {
            case GenSound::Bells: return 0.9f;
            case GenSound::Pad:   return 1.1f;
            case GenSound::Bowed: return 0.6f;
            case GenSound::Drone: return 1.8f;
            case GenSound::Choir: return 0.9f;
            case GenSound::Swarm: default:              return 0.35f;
        }
    }

    float saw (int i, float f)
    {
        const float dt = f / (float) sr;
        ph[i] += dt;
        if (ph[i] >= 1.0) ph[i] -= 1.0;
        const float t = (float) ph[i];
        return 2.0f * t - 1.0f - polyBlep (t, dt);
    }
    float sine (int i, float f)
    {
        ph[i] += f / sr;
        if (ph[i] >= 1.0) ph[i] -= 1.0;
        return fastSin2Pi ((float) ph[i]);
    }

    /** Core oscillator. `bright` 0..1 opens the timbre (breath / envelope). */
    float tick (float f, float amp, float excite, float bright)
    {
        ++age;
        f = std::min (f, (float) sr * 0.2f);
        switch (sound)
        {
            case GenSound::Bells:
            {
                fmPh += 3.0 * f / sr;      // harmonic 1:3 ratio: glassy, and stays in tune
                if (fmPh >= 1.0) fmPh -= 1.0;
                const float idx = 0.6f + 2.4f * bright;
                ph[0] += f / sr;
                if (ph[0] >= 1.0) ph[0] -= 1.0;
                return 0.8f * fastSin2Pi ((float) ph[0] + idx * 0.15915494f * fastSin2Pi ((float) fmPh));
            }
            case GenSound::Pad:
            {
                const float v = saw (0, f * 0.9960f) + saw (1, f) + saw (2, f * 1.0040f);
                if ((age & 15) == 0) filt.set (std::min (14000.0f, f * 2.5f + 2500.0f * bright), 0.8f, sr);
                return 0.45f * filt.lp (v);
            }
            case GenSound::Bowed:
            {
                const float period = (float) sr / f;
                const float out = ks.read (std::max (1.0f, period - 1.5f));   // minus push + averaging delay
                const float y = 0.5f * (out + ksLast) * 0.996f;
                ksLast = out;
                ks.push (excite + y);
                (void) amp;
                return 1.4f * out;
            }
            case GenSound::Drone:
                return 0.75f * sine (0, f) + 0.35f * sine (1, f * 2.0f) + 0.45f * sine (2, f * 0.5f);
            case GenSound::Choir:
            {
                const float v = saw (0, f) + 0.7f * saw (1, f * 1.003f);
                if ((age & 31) == 0)
                {
                    const float o = 0.85f + 0.3f * vowel;   // each voice sings a slightly different "aah"
                    formant[0].set (700.0f * o, 9.0f, sr);
                    formant[1].set (1150.0f * o, 11.0f, sr);
                    formant[2].set (2800.0f, 14.0f, sr);
                }
                return 0.9f * (formant[0].bpNorm (v) + 0.6f * formant[1].bpNorm (v) + 0.25f * formant[2].bpNorm (v));
            }
            case GenSound::Swarm: default:
            {
                float v = 0;
                for (int i = 0; i < 4; ++i) v += saw (i, f * (1.0f + 0.009f * (float) (i - 1.5f)));
                const float pulse = sine (4, 5.5f) * 0.5f + 0.5f;
                if ((age & 15) == 0) filt.set (f * (2.0f + 6.0f * bright * pulse), 2.5f, sr);
                return 0.4f * filt.lp (v);
            }
        }
    }

    double sr = 48000.0;
    GenSound sound = GenSound::Bells;
    double ph[5] {};
    double fmPh = 0;
    float hz = 220, vel = 1, gainL = 0.7f, gainR = 0.7f, env = 0, attackStep = 1, decayCoef = 0.999f, vowel = 0.5f;
    int attackLeft = 0, holdLeft = 0, pluckExcite = 0;
    DelayLine ks;
    float ksLast = 0;
    Svf filt, formant[3];
    Noise rng;
};
} // namespace rs
