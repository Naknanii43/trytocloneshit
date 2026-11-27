// Monophonic YIN pitch tracker, tuned for saxophone (≈45 Hz – 2.2 kHz).
// Runs on a decimated copy of the input so it is cheap enough for the audio thread.
#pragma once
#include "DSP.h"

namespace rs
{
class PitchTracker
{
public:
    void prepare (double sampleRate)
    {
        decim = std::max (1, (int) std::round (sampleRate / 11025.0));
        dsr = sampleRate / decim;
        aa1.set ((float) dsr * 0.40f, 0.7071f, sampleRate);
        aa2.set ((float) dsr * 0.40f, 0.7071f, sampleRate);
        W = (int) (dsr * 0.024);
        tauMax = (int) (dsr / 45.0);
        tauMin = std::max (2, (int) (dsr / 2300.0));
        N = W + tauMax + 2;
        hop = std::max (16, (int) (dsr * 0.0055));
        ring.assign ((size_t) N, 0.0f);
        frame.assign ((size_t) N, 0.0f);
        diff.assign ((size_t) tauMax + 2, 0.0f);
        raw.assign ((size_t) tauMax + 2, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        pos = dc = hc = 0;
        aa1.reset(); aa2.reset();
        voiced = false;
        midi = 60.0f;
        hist[0] = hist[1] = hist[2] = 60.0f;
        unvoicedHops = 1000;
    }

    void push (float x)
    {
        const float y = aa2.lp (aa1.lp (x));
        if (++dc < decim) return;
        dc = 0;
        ring[(size_t) pos] = y;
        pos = (pos + 1) % N;
        if (++hc >= hop) { hc = 0; analyse(); }
    }

    bool isVoiced() const { return voiced; }
    float getMidi() const { return midi; }          // smoothed, fractional MIDI note (concert pitch, A=440)
    float getConfidence() const { return confidence; }
    int hopsSinceVoiced() const { return unvoicedHops; }

private:
    void analyse()
    {
        for (int i = 0; i < N; ++i) frame[(size_t) i] = ring[(size_t) ((pos + i) % N)];

        // energy gate on the newest window
        float e = 0.0f;
        for (int j = N - W; j < N; ++j) e += frame[(size_t) j] * frame[(size_t) j];
        const float rms = std::sqrt (e / (float) W);
        if (rms < 0.0035f) { markUnvoiced(); return; }

        const float* f = frame.data();
        diff[0] = 1.0f;
        float running = 0.0f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            // four independent accumulators: lets the compiler use SIMD (NEON on Apple Silicon)
            float a0 = 0, a1 = 0, a2 = 0, a3 = 0;
            const float* g = f + tau;
            int j = 0;
            for (; j + 3 < W; j += 4)
            {
                const float t0 = f[j] - g[j], t1 = f[j + 1] - g[j + 1], t2 = f[j + 2] - g[j + 2], t3 = f[j + 3] - g[j + 3];
                a0 += t0 * t0; a1 += t1 * t1; a2 += t2 * t2; a3 += t3 * t3;
            }
            for (; j < W; ++j) { const float t = f[j] - g[j]; a0 += t * t; }
            const float d = (a0 + a1) + (a2 + a3);
            running += d;
            raw[(size_t) tau] = d;
            diff[(size_t) tau] = running > 0.0f ? d * (float) tau / running : 1.0f;
        }

        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (diff[(size_t) tau] < 0.14f)
            {
                while (tau + 1 < tauMax && diff[(size_t) tau + 1] < diff[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        }
        if (best < 0)
        {
            int m = tauMin;
            for (int tau = tauMin; tau < tauMax; ++tau)
                if (diff[(size_t) tau] < diff[(size_t) m]) m = tau;
            if (diff[(size_t) m] < 0.28f) best = m;
        }
        if (best < 0) { markUnvoiced(); return; }

        float tauF = (float) best;
        if (best > 1 && best < tauMax)
        {
            // refine on the raw difference function (less biased than the normalised one)
            const float a = raw[(size_t) best - 1], b = raw[(size_t) best], c = raw[(size_t) best + 1];
            const float den = a - 2.0f * b + c;
            if (std::abs (den) > 1.0e-9f) tauF += 0.5f * (a - c) / den;
        }
        confidence = 1.0f - diff[(size_t) best];

        const float m = hzToMidi ((float) dsr / tauF);
        hist[0] = hist[1]; hist[1] = hist[2]; hist[2] = m;
        const float med = std::max (std::min (hist[0], hist[1]), std::min (std::max (hist[0], hist[1]), hist[2]));

        if (! voiced || std::abs (med - midi) > 0.7f) midi = med;     // new note: jump
        else                                          midi += 0.35f * (med - midi);
        voiced = true;
        unvoicedHops = 0;
    }

    void markUnvoiced()
    {
        if (++unvoicedHops > 6) voiced = false;
    }

    Svf aa1, aa2;
    int decim = 4, W = 256, tauMax = 245, tauMin = 5, N = 512, hop = 64;
    double dsr = 11025.0;
    std::vector<float> ring, frame, diff, raw;
    int pos = 0, dc = 0, hc = 0;
    bool voiced = false;
    float midi = 60.0f, confidence = 0.0f;
    float hist[3] {};
    int unvoicedHops = 1000;
};
} // namespace rs
