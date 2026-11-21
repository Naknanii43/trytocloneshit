#include "Sources.h"
#include "DSP.h"

namespace neb
{
namespace
{
constexpr double SR = 44100.0;
constexpr double root = 261.6256; // C4

std::unique_ptr<SampleData> make (const juce::String& name, double seconds,
                                  const std::function<void (std::vector<float>&)>& gen)
{
    auto s = std::make_unique<SampleData>();
    s->name = name;
    s->sampleRate = SR;
    s->data.assign ((size_t) (seconds * SR), 0.0f);
    gen (s->data);
    s->finalise();
    return s;
}

void formantVoice (std::vector<float>& d, float F1, float F2, float F3, float breath, unsigned seed)
{
    juce::Random r ((juce::int64) seed);
    Svf a, b, c;
    a.setFreq (F1, 0.12f, SR); b.setFreq (F2, 0.1f, SR); c.setFreq (F3, 0.08f, SR);
    double ph = 0, vib = 0;
    for (size_t i = 0; i < d.size(); ++i)
    {
        const double t = (double) i / SR;
        vib += 5.2 / SR;
        const double hz = root * (1.0 + 0.004 * std::sin (twoPi * vib) * juce::jmin (1.0, t * 2));
        ph += hz / SR; if (ph >= 1) ph -= 1;
        const float src = (float) (2 * ph - 1) * 0.6f + (r.nextFloat() * 2 - 1) * breath;
        float lp, bp1, bp2, bp3, hp;
        a.tick (src, lp, bp1, hp); b.tick (src, lp, bp2, hp); c.tick (src, lp, bp3, hp);
        d[i] = bp1 + 0.6f * bp2 + 0.3f * bp3;
    }
}

std::vector<std::unique_ptr<SampleData>> buildAll()
{
    std::vector<std::unique_ptr<SampleData>> v;

    v.push_back (make ("Glass Bells", 3.0, [] (auto& d)
    {
        const double ratios[] = { 1.0, 2.0, 2.76, 4.07, 5.4, 6.8 };
        for (size_t i = 0; i < d.size(); ++i)
        {
            const double t = (double) i / SR;
            double s = 0;
            for (int p = 0; p < 6; ++p)
                s += std::sin (twoPi * root * ratios[p] * t) * std::exp (-t * (0.8 + p * 1.3)) / (1 + p * 0.7);
            d[i] = (float) (s * juce::jmin (1.0, t * 400));
        }
    }));

    v.push_back (make ("Breath Choir", 3.0, [] (auto& d) { formantVoice (d, 730, 1090, 2440, 0.25f, 7); }));
    v.push_back (make ("Vocal Ooh", 3.0, [] (auto& d) { formantVoice (d, 300, 870, 2240, 0.12f, 9); }));

    v.push_back (make ("Bowed Metal", 3.0, [] (auto& d)
    {
        const double ratios[] = { 1.0, 1.004, 2.01, 2.98, 4.13, 5.21, 7.33 };
        for (size_t i = 0; i < d.size(); ++i)
        {
            const double t = (double) i / SR;
            double s = 0;
            for (int p = 0; p < 7; ++p)
                s += std::sin (twoPi * root * ratios[p] * t + p) * (0.6 + 0.4 * std::sin (twoPi * (0.3 + p * 0.17) * t)) / (1 + p);
            d[i] = (float) (s * juce::jmin (1.0, t * 3));
        }
    }));

    v.push_back (make ("Kalimba", 2.0, [] (auto& d)
    {
        juce::Random r (3);
        for (size_t i = 0; i < d.size(); ++i)
        {
            const double t = (double) i / SR;
            double s = std::sin (twoPi * root * t) * std::exp (-t * 2.5)
                     + 0.4 * std::sin (twoPi * root * 5.4 * t) * std::exp (-t * 9)
                     + 0.2 * std::sin (twoPi * root * 8.9 * t) * std::exp (-t * 15);
            if (i < 200) s += (r.nextFloat() * 2 - 1) * 0.5 * (1.0 - i / 200.0);
            d[i] = (float) s;
        }
    }));

    v.push_back (make ("Tape Strings", 3.0, [] (auto& d)
    {
        double ph[7] {};
        const double det[] = { -0.11, -0.07, -0.03, 0, 0.03, 0.07, 0.11 };
        Svf f; float lp, bp, hp;
        for (size_t i = 0; i < d.size(); ++i)
        {
            const double t = (double) i / SR;
            const double wow = 1.0 + 0.0015 * std::sin (twoPi * 0.6 * t);
            double s = 0;
            for (int u = 0; u < 7; ++u)
            {
                ph[u] += root * std::pow (2.0, det[u] / 12.0) * wow / SR;
                if (ph[u] >= 1) ph[u] -= 1;
                s += 2 * ph[u] - 1;
            }
            f.setFreq ((float) (1200 + 900 * std::sin (twoPi * 0.25 * t)), 1.6f, SR);
            f.tick ((float) s / 3.0f, lp, bp, hp);
            d[i] = lp * (float) juce::jmin (1.0, t * 4);
        }
    }));

    v.push_back (make ("Crystal Shimmer", 3.0, [] (auto& d)
    {
        juce::Random r (11);
        const double ratios[] = { 2, 3, 4, 6, 8 };
        double ph[5] {}; double amp[5] {}; double tgt[5] {};
        for (size_t i = 0; i < d.size(); ++i)
        {
            if (i % 2205 == 0) for (auto& tg : tgt) tg = r.nextFloat() < 0.4f ? r.nextFloat() : 0.0;
            double s = 0;
            for (int p = 0; p < 5; ++p)
            {
                amp[p] += (tgt[p] - amp[p]) * 0.0006;
                ph[p] += root * ratios[p] / SR; if (ph[p] >= 1) ph[p] -= 1;
                s += std::sin (twoPi * ph[p]) * amp[p];
            }
            d[i] = (float) s;
        }
    }));

    v.push_back (make ("Ocean Wash", 4.0, [] (auto& d)
    {
        juce::Random r (5);
        Svf f; float lp, bp, hp;
        for (size_t i = 0; i < d.size(); ++i)
        {
            const double t = (double) i / SR;
            const double swell = 0.5 - 0.5 * std::cos (twoPi * t / 4.0);
            f.setFreq ((float) (300 + 2500 * swell), 1.2f, SR);
            f.tick (r.nextFloat() * 2 - 1, lp, bp, hp);
            d[i] = lp * (float) (0.3 + swell);
        }
    }));

    v.push_back (make ("Vinyl Air", 3.0, [] (auto& d)
    {
        juce::Random r (21);
        Svf f; f.setFreq (3500, 1.0f, SR); float lp, bp, hp;
        for (size_t i = 0; i < d.size(); ++i)
        {
            f.tick (r.nextFloat() * 2 - 1, lp, bp, hp);
            float s = bp * 0.3f;
            if (r.nextFloat() < 0.0004f) s += (r.nextFloat() * 2 - 1) * 2.0f;
            d[i] = s;
        }
    }));

    return v;
}
} // namespace

const std::vector<std::unique_ptr<SampleData>>& builtinSources()
{
    static const auto s = buildAll();
    return s;
}

juce::StringArray sourceNames()
{
    juce::StringArray n { "Glass Bells", "Breath Choir", "Vocal Ooh", "Bowed Metal", "Kalimba",
                          "Tape Strings", "Crystal Shimmer", "Ocean Wash", "Vinyl Air" };
    n.add ("User Sample");
    return n;
}
} // namespace neb
