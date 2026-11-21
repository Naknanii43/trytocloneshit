#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include <array>

namespace neb
{
constexpr double twoPi = juce::MathConstants<double>::twoPi;
constexpr double pi    = juce::MathConstants<double>::pi;

inline double midiToHz (double note) { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }

//==============================================================================
// Zero-delay-feedback state variable filter (Simper/Cytomic topology).
struct Svf
{
    float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1;

    void set (float g, float kk)
    {
        k = kk;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void setFreq (float hz, float kk, double sr)
    {
        set ((float) std::tan (pi * juce::jlimit (10.0, sr * 0.45, (double) hz) / sr), kk);
    }

    void reset() { ic1 = ic2 = 0; }

    inline void tick (float x, float& lp, float& bp, float& hp)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
};

//==============================================================================
// Feedback comb filter for metallic / resonator tones.
struct Comb
{
    static constexpr int size = 4096, mask = size - 1;
    std::array<float, size> buf {};
    int w = 0;

    void reset() { buf.fill (0.0f); w = 0; }

    inline float process (float x, float delay, float fb)
    {
        float rp = (float) w - delay;
        if (rp < 0) rp += (float) size;
        const int i0 = (int) rp;
        const float fr = rp - (float) i0;
        const float d = buf[(size_t) (i0 & mask)] + (buf[(size_t) ((i0 + 1) & mask)] - buf[(size_t) (i0 & mask)]) * fr;
        const float y = x + fb * d;
        buf[(size_t) w] = std::tanh (y);
        w = (w + 1) & mask;
        return y * 0.6f;
    }
};

//==============================================================================
// Simple RBJ biquad (used for the master EQ)
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }

    void shelf (bool high, double sr, double freq, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = twoPi * freq / sr;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / 2.0 * std::sqrt (2.0);
        const double sA = 2.0 * std::sqrt (A) * alpha;
        double B0, B1, B2, A0, A1, A2;
        if (high)
        {
            B0 = A * ((A + 1) + (A - 1) * cw + sA);
            B1 = -2 * A * ((A - 1) + (A + 1) * cw);
            B2 = A * ((A + 1) + (A - 1) * cw - sA);
            A0 = (A + 1) - (A - 1) * cw + sA;
            A1 = 2 * ((A - 1) - (A + 1) * cw);
            A2 = (A + 1) - (A - 1) * cw - sA;
        }
        else
        {
            B0 = A * ((A + 1) - (A - 1) * cw + sA);
            B1 = 2 * A * ((A - 1) - (A + 1) * cw);
            B2 = A * ((A + 1) - (A - 1) * cw - sA);
            A0 = (A + 1) + (A - 1) * cw + sA;
            A1 = -2 * ((A - 1) + (A + 1) * cw);
            A2 = (A + 1) + (A - 1) * cw - sA;
        }
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }

    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

//==============================================================================
// Band-limited analog shapes with continuous morph Sine -> Tri -> Saw -> Square.
inline float polyBlep (double t, double dt)
{
    if (t < dt)       { t /= dt;            return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
    return 0.0f;
}

inline float analogShape (int s, double t, double dt)
{
    switch (s)
    {
        case 0:  return (float) std::sin (twoPi * t);
        case 1:  return (float) (t < 0.25 ? 4.0 * t : (t < 0.75 ? 2.0 - 4.0 * t : 4.0 * t - 4.0));
        case 2:  return (float) (2.0 * t - 1.0) - polyBlep (t, dt);
        default:
        {
            double t2 = t + 0.5; if (t2 >= 1.0) t2 -= 1.0;
            return (t < 0.5 ? 1.0f : -1.0f) + polyBlep (t, dt) - polyBlep (t2, dt);
        }
    }
}

inline float morphOsc (float w, double t, double dt)
{
    int i = (int) w;
    float fr = w - (float) i;
    if (i >= 3) { i = 3; fr = 0.0f; }
    const float a = analogShape (i, t, dt);
    if (fr <= 0.0001f) return a;
    return a + (analogShape (i + 1, t, dt) - a) * fr;
}
} // namespace neb
