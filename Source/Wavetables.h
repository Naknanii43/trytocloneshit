#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace neb
{
// Mip-mapped single-cycle wavetables. Level l keeps harmonics up to 512 >> l,
// so playback stays alias-free across the keyboard.
struct Wavetable
{
    static constexpr int size = 1024, levels = 10;
    juce::String name;
    int frames = 0;
    std::vector<float> data; // [frame][level][size + 1] (one guard sample)

    const float* get (int frame, int level) const
    {
        return data.data() + ((size_t) frame * levels + (size_t) level) * (size + 1);
    }
};

const std::vector<Wavetable>& wavetables();
juce::StringArray wavetableNames();

inline int wtLevelFor (double dt)
{
    const double maxH = 0.5 / juce::jmax (1.0e-9, dt);
    int l = 0;
    while (l < Wavetable::levels - 1 && (512 >> l) > maxH) ++l;
    return l;
}

inline float wtLookup (const Wavetable& t, float pos, double phase, int level)
{
    const float fp = juce::jlimit (0.0f, 1.0f, pos) * (float) (t.frames - 1);
    const int f0 = (int) fp;
    const int f1 = juce::jmin (f0 + 1, t.frames - 1);
    const float ff = fp - (float) f0;
    const double x = phase * Wavetable::size;
    const int i = (int) x;
    const float fr = (float) (x - i);
    const float* a = t.get (f0, level);
    const float* b = t.get (f1, level);
    const float va = a[i] + (a[i + 1] - a[i]) * fr;
    const float vb = b[i] + (b[i + 1] - b[i]) * fr;
    return va + (vb - va) * ff;
}
} // namespace neb
