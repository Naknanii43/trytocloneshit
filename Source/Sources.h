#pragma once

#include <juce_core/juce_core.h>
#include <memory>
#include <vector>

namespace neb
{
// A mono sound source for the Sample and Granular engines.
struct SampleData
{
    juce::String name;
    std::vector<float> data;
    double sampleRate = 44100.0;
    int rootNote = 60;
    std::vector<float> peaks; // 512 overview peaks for the UI

    void finalise()
    {
        float pk = 0.0f;
        for (auto v : data) pk = juce::jmax (pk, std::abs (v));
        if (pk > 1.0e-6f) for (auto& v : data) v *= 0.85f / pk;
        peaks.assign (512, 0.0f);
        const size_t n = data.size();
        for (size_t i = 0; i < n; ++i)
        {
            auto& p = peaks[i * 512 / juce::jmax ((size_t) 1, n)];
            p = juce::jmax (p, std::abs (data[i]));
        }
    }
};

const std::vector<std::unique_ptr<SampleData>>& builtinSources();
juce::StringArray sourceNames(); // built-ins + "User Sample"
} // namespace neb
