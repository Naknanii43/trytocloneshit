#include "Wavetables.h"
#include "DSP.h"
#include <juce_dsp/juce_dsp.h>
#include <functional>

namespace neb
{
namespace
{
constexpr int N = Wavetable::size; // file scope so lambdas can use it without capturing (MSVC)
using FrameFn = std::function<void (int frame, int frames, float* out /* size samples */)>;

void additive (float* out, const std::function<float (int h)>& amp, int maxH = 128)
{
    for (int i = 0; i < N; ++i) out[i] = 0.0f;
    for (int h = 1; h <= maxH; ++h)
    {
        const float a = amp (h);
        if (std::abs (a) < 1.0e-5f) continue;
        for (int i = 0; i < N; ++i)
            out[i] += a * (float) std::sin (twoPi * h * i / N);
    }
}

float formantGain (float hz, float f1, float f2, float f3)
{
    auto bump = [] (float x, float c, float w) { const float d = (x - c) / w; return std::exp (-d * d); };
    return 0.08f + bump (hz, f1, 90) + 0.7f * bump (hz, f2, 140) + 0.35f * bump (hz, f3, 220);
}

Wavetable build (const juce::String& name, int frames, const FrameFn& fn)
{
    constexpr int L = Wavetable::levels;
    Wavetable t;
    t.name = name;
    t.frames = frames;
    t.data.resize ((size_t) frames * L * (N + 1));

    juce::dsp::FFT fft (10); // 1024
    std::vector<float> time (N), spec (2 * N), work (2 * N);

    for (int f = 0; f < frames; ++f)
    {
        fn (f, frames, time.data());
        std::fill (spec.begin(), spec.end(), 0.0f);
        std::copy (time.begin(), time.end(), spec.begin());
        fft.performRealOnlyForwardTransform (spec.data());
        spec[0] = spec[1] = 0.0f; // remove DC

        float scale = 1.0f;
        for (int l = 0; l < L; ++l)
        {
            const int maxH = 512 >> l;
            work = spec;
            for (int k = maxH + 1; k <= N - maxH - 1; ++k) { work[(size_t) (2 * k)] = 0; work[(size_t) (2 * k + 1)] = 0; }
            fft.performRealOnlyInverseTransform (work.data());

            if (l == 0)
            {
                float peak = 0.0f;
                for (int i = 0; i < N; ++i) peak = juce::jmax (peak, std::abs (work[(size_t) i]));
                scale = peak > 1.0e-6f ? 0.95f / peak : 1.0f;
            }
            float* dst = t.data.data() + ((size_t) f * L + (size_t) l) * (N + 1);
            for (int i = 0; i < N; ++i) dst[i] = work[(size_t) i] * scale;
            dst[N] = dst[0];
        }
    }
    return t;
}

std::vector<Wavetable> buildAll()
{
    std::vector<Wavetable> v;

    v.push_back (build ("Classic Morph", 32, [] (int f, int F, float* o)
    {
        const float w = 3.0f * (float) f / (float) (F - 1);
        for (int i = 0; i < N; ++i) o[i] = morphOsc (w, (double) i / N, 1.0e-9);
    }));

    v.push_back (build ("Pulse Width", 32, [] (int f, int F, float* o)
    {
        const double duty = 0.5 - 0.46 * f / (F - 1);
        for (int i = 0; i < N; ++i) o[i] = ((double) i / N < duty) ? 1.0f : -1.0f;
    }));

    v.push_back (build ("Vowel Morph", 32, [] (int f, int F, float* o)
    {
        static const float fm[5][3] = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 },
                                        { 570, 840, 2410 },  { 300, 870, 2240 } };
        const float p = 4.0f * (float) f / (float) (F - 1);
        const int a = juce::jmin (3, (int) p);
        const float fr = p - (float) a;
        float F1 = fm[a][0] + (fm[a + 1][0] - fm[a][0]) * fr;
        float F2 = fm[a][1] + (fm[a + 1][1] - fm[a][1]) * fr;
        float F3 = fm[a][2] + (fm[a + 1][2] - fm[a][2]) * fr;
        additive (o, [=] (int h) { return formantGain (110.0f * (float) h, F1, F2, F3) / std::sqrt ((float) h); }, 120);
    }));

    v.push_back (build ("Harmonic Sweep", 32, [] (int f, int F, float* o)
    {
        const float top = 1.0f + 63.0f * (float) f / (float) (F - 1);
        additive (o, [=] (int h) { return h <= top ? 1.0f / (float) h : (h < top + 1 ? (top + 1 - (float) h) / (float) h : 0.0f); }, 64);
    }));

    v.push_back (build ("Hard Sync", 32, [] (int f, int F, float* o)
    {
        const double ratio = 1.0 + 7.0 * f / (F - 1);
        for (int i = 0; i < N; ++i)
        {
            const double t = (double) i / N * ratio;
            o[i] = (float) (2.0 * (t - std::floor (t)) - 1.0);
        }
    }));

    v.push_back (build ("Digital Bells", 32, [] (int f, int F, float* o)
    {
        const float m = (float) f / (float) (F - 1);
        additive (o, [=] (int h)
        {
            static const int partials[] = { 1, 4, 7, 11, 17, 23, 31, 41 };
            for (int i = 0; i < 8; ++i)
                if (partials[i] == h)
                    return std::pow (0.75f - 0.5f * m, (float) i) * (i == 0 ? 1.0f : 0.5f + m);
            return 0.0f;
        }, 41);
    }));

    v.push_back (build ("Drawbar Organ", 32, [] (int f, int F, float* o)
    {
        static const float bars[4][6] = { { 1, 0.8f, 0, 0, 0, 0 }, { 1, 1, 0.8f, 0.6f, 0, 0 },
                                          { 1, 0.5f, 1, 0.3f, 0.7f, 0 }, { 1, 1, 1, 1, 1, 1 } };
        static const int harms[6] = { 1, 2, 3, 4, 6, 8 };
        const float p = 3.0f * (float) f / (float) (F - 1);
        const int a = juce::jmin (2, (int) p);
        const float fr = p - (float) a;
        additive (o, [=] (int h)
        {
            for (int i = 0; i < 6; ++i)
                if (harms[i] == h) return bars[a][i] + (bars[a + 1][i] - bars[a][i]) * fr;
            return 0.0f;
        }, 8);
    }));

    v.push_back (build ("Spectral Growl", 32, [] (int f, int F, float* o)
    {
        const float m = (float) f / (float) (F - 1);
        additive (o, [=] (int h)
        {
            const float comb = 0.5f + 0.5f * std::cos ((float) twoPi * ((float) h * (0.07f + 0.25f * m) + m));
            return comb * comb / std::pow ((float) h, 0.7f);
        }, 96);
    }));

    v.push_back (build ("Crystal Dust", 32, [] (int f, int F, float* o)
    {
        juce::Random r (1234 + f / 4);
        juce::Random r2 (1234 + f / 4 + 1);
        const float fr = (float) (f % 4) / 4.0f;
        float amps[97];
        for (int h = 1; h <= 96; ++h)
        {
            const float a = r.nextFloat() < 0.18f ? r.nextFloat() : 0.0f;
            const float b = r2.nextFloat() < 0.18f ? r2.nextFloat() : 0.0f;
            amps[h] = (a + (b - a) * fr) / std::sqrt ((float) h);
        }
        amps[1] = 1.0f;
        additive (o, [&] (int h) { return amps[h]; }, 96);
        (void) F;
    }));

    return v;
}
} // namespace

const std::vector<Wavetable>& wavetables()
{
    static const std::vector<Wavetable> tables = buildAll();
    return tables;
}

juce::StringArray wavetableNames()
{
    return { "Classic Morph", "Pulse Width", "Vowel Morph", "Harmonic Sweep", "Hard Sync",
             "Digital Bells", "Drawbar Organ", "Spectral Growl", "Crystal Dust" };
}
} // namespace neb
