// Small, allocation-free DSP building blocks used by the rack.
#pragma once
#include <juce_core/juce_core.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace rs
{
constexpr float pi = 3.14159265358979f;
constexpr float twoPi = 2.0f * pi;

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float midiToHz (float m) { return 440.0f * std::pow (2.0f, (m - 69.0f) / 12.0f); }
inline float hzToMidi (float f) { return 69.0f + 12.0f * std::log2 (std::max (f, 1.0f) / 440.0f); }
inline float msToCoeff (float ms, double sr)
{
    return 1.0f - std::exp (-1.0f / (std::max (ms, 0.01f) * 0.001f * (float) sr));
}
inline float clamp01 (float x) { return std::min (1.0f, std::max (0.0f, x)); }

//==============================================================================
// Fast approximations for per-sample use. All are branch-light, NaN-safe for finite input,
// and accurate far beyond audibility for the ranges used here.

/** 2^x, relative error < 2e-5 (|x| < 120). */
inline float fastExp2 (float x)
{
    x = std::min (120.0f, std::max (-120.0f, x));
    const float fl = std::floor (x);
    const float f = x - fl;
    // minimax polynomial for 2^f on [0,1)
    const float p = 1.0f + f * (0.69314718f + f * (0.24022650f + f * (0.05550411f + f * (0.00961813f + f * 0.00133336f))));
    return std::ldexp (p, (int) fl);
}

/** log2(x) for x > 0, absolute error < 1e-4. */
inline float fastLog2 (float x)
{
    int e = 0;
    const float m = std::frexp (std::max (x, 1.0e-30f), &e);    // m in [0.5, 1)
    const float t = (m - 0.70710678f) / (m + 0.70710678f);
    const float t2 = t * t;
    const float ln = 2.0f * t * (1.0f + t2 * (0.33333333f + t2 * (0.2f + t2 * 0.14285714f)));
    return (float) e - 0.5f + ln * 1.44269504f;
}

inline float fastDbToGain (float db) { return fastExp2 (db * 0.16609640f); }
inline float fastGainToDb (float g)  { return 6.0205999f * fastLog2 (std::max (g, 1.0e-9f)); }

/** sin(2*pi*x) for any x (phase in cycles), absolute error < 1e-6. */
inline float fastSin2Pi (float x)
{
    x -= std::floor (x + 0.5f);                    // [-0.5, 0.5)
    const float y = x * twoPi;
    const float y2 = y * y;
    return y * (1.0f + y2 * (-0.16666667f + y2 * (0.0083333310f + y2 * (-0.00019840874f + y2 * (2.7525562e-06f - y2 * 2.3889859e-08f)))));
}
inline float fastCos2Pi (float x) { return fastSin2Pi (x + 0.25f); }
inline float fastSin (float radians) { return fastSin2Pi (radians * 0.15915494f); }

/** tanh(x), relative error < 1e-4, saturates cleanly. */
inline float fastTanh (float x)
{
    if (x > 9.0f) return 1.0f;
    if (x < -9.0f) return -1.0f;
    const float e = fastExp2 (2.8853901f * x);     // e^(2x)
    return (e - 1.0f) / (e + 1.0f);
}

//==============================================================================
struct OnePole
{
    float a = 1.0f, z = 0.0f;
    void setCutoff (float hz, double sr) { a = 1.0f - std::exp (-twoPi * std::min (hz, (float) sr * 0.49f) / (float) sr); }
    float lp (float x) { z += a * (x - z); return z; }
    float hp (float x) { return x - lp (x); }
    void reset() { z = 0.0f; }
};

/** Peak envelope follower with separate attack and release. */
struct EnvFollower
{
    float att = 0.1f, rel = 0.01f, env = 0.0f;
    void setup (float attackMs, float releaseMs, double sr) { att = msToCoeff (attackMs, sr); rel = msToCoeff (releaseMs, sr); }
    float process (float x)
    {
        const float r = std::abs (x);
        env += (r > env ? att : rel) * (r - env);
        return env;
    }
    void reset() { env = 0.0f; }
};

/** Topology-preserving state variable filter (Zavalishin / Simper). */
struct Svf
{
    float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    float lpOut = 0, bpOut = 0, hpOut = 0;

    void set (float cutoff, float q, double sr)
    {
        cutoff = std::min (std::max (cutoff, 10.0f), (float) sr * 0.47f);
        g = std::tan (pi * cutoff / (float) sr);
        k = 1.0f / std::max (q, 0.05f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void process (float x)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lpOut = v2; bpOut = v1; hpOut = x - k * v1 - v2;
    }
    float lp (float x) { process (x); return lpOut; }
    float hp (float x) { process (x); return hpOut; }
    float bpNorm (float x) { process (x); return bpOut * k; }
    void reset() { ic1 = ic2 = 0.0f; }
};

/** RBJ biquad (direct form I, coefficient-only updates, no allocation). */
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;

    void norm (float B0, float B1, float B2, float A0, float A1, float A2)
    {
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    void highPass (float f, float q, double sr)
    {
        const float w = twoPi * f / (float) sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        norm ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void peak (float f, float q, float db, double sr)
    {
        const float A = std::pow (10.0f, db / 40.0f), w = twoPi * f / (float) sr;
        const float c = std::cos (w), al = std::sin (w) / (2 * q);
        norm (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void highShelf (float f, float db, double sr)
    {
        const float A = std::pow (10.0f, db / 40.0f), w = twoPi * f / (float) sr;
        const float c = std::cos (w), s = std::sin (w), al = s / 2 * std::sqrt (2.0f), sq = 2 * std::sqrt (A) * al;
        norm (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
              (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
    float process (float x)
    {
        const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
    void reset() { x1 = x2 = y1 = y2 = 0; }
};

//==============================================================================
/** Power-of-two circular delay with cubic (Hermite) fractional reads. */
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;

    void prepare (int maxDelay)
    {
        int size = 16;
        while (size < maxDelay + 8) size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        w = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    float at (int d) const { return buf[(size_t) ((w - 1 - d) & mask)]; }
    float maxDelay() const { return (float) (mask - 4); }
    float read (float d) const
    {
        d = std::min (std::max (d, 1.0f), maxDelay());
        const int i = (int) d;
        const float f = d - (float) i;
        const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
};

/** Two-tap rotating-delay pitch shifter with Hann crossfades.
    For a clean, in-tune result on a monophonic source, give it the input's period with
    setWindowForPeriod(): when half the window is a whole number of periods the two taps
    stay phase-aligned, so there is no beating or drift between crossfades. */
struct PitchShifter
{
    DelayLine dl;
    float d = 0.0f, window = 1024.0f, targetWindow = 1024.0f, maxWindow = 2048.0f;

    void prepare (double sr, float maxWindowMs)
    {
        maxWindow = (float) sr * maxWindowMs * 0.001f;
        dl.prepare ((int) maxWindow + 8);
        window = targetWindow = maxWindow * 0.6f;
        d = 0.0f;
    }
    void setWindowMs (float ms, double sr)
    {
        targetWindow = std::min (maxWindow, std::max (64.0f, ms * 0.001f * (float) sr));
    }
    /** Window = 2 * k * period, with k chosen so the window lands near `aroundMs`. */
    void setWindowForPeriod (float periodSamples, float aroundMs, double sr)
    {
        const float half = aroundMs * 0.0005f * (float) sr;
        const float k = std::max (1.0f, std::round (half / std::max (1.0f, periodSamples)));
        float w = 2.0f * k * periodSamples;
        while (w > maxWindow && w > 2.0f * periodSamples) w -= 2.0f * periodSamples;
        targetWindow = std::min (maxWindow, std::max (64.0f, w));
    }
    void snapWindow() { window = targetWindow; }
    void reset() { dl.reset(); d = 0.0f; }

    float process (float x, float ratio)
    {
        dl.push (x);
        window += 0.002f * (targetWindow - window);
        d += 1.0f - ratio;
        if (d >= window) d -= window * std::floor (d / window);
        if (d < 0.0f)    d += window * std::ceil (-d / window);
        float d2 = d + 0.5f * window;
        if (d2 >= window) d2 -= window;
        const float g1 = 0.5f - 0.5f * fastCos2Pi (d / window);
        return dl.read (1.0f + d) * g1 + dl.read (1.0f + d2) * (1.0f - g1);
    }
};

/** Smooth 0..1 crossfade used when a module is switched on or off (no clicks). */
struct Fade
{
    float v = 0.0f, step = 0.001f;
    bool target = false;
    void prepare (double sr) { step = 1.0f / (0.02f * (float) sr); }
    void set (bool on) { target = on; }
    bool active() const { return target || v > 0.0f; }
    float next()
    {
        if (target) v = std::min (1.0f, v + step);
        else        v = std::max (0.0f, v - step);
        return v;
    }
};

/** PolyBLEP correction for band-limited saw / square oscillators. */
inline float polyBlep (float t, float dt)
{
    if (t < dt)        { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

/** Cheap xorshift noise in [-1, 1]. */
struct Noise
{
    uint32_t s = 0x9E3779B9u;
    float next()
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) (int32_t) s * (1.0f / 2147483648.0f);
    }
};
} // namespace rs
