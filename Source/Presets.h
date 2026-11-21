#pragma once

#include <vector>
#include <utility>

namespace neb
{
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values; // overrides on top of defaults
};

inline const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets {
        { "Init Atmosphere", {} },

        { "Glass Horizon Pad", {
            { "A_wave", 2.0f }, { "A_unison", 7 }, { "A_detune", 22 }, { "A_spread", 0.9f },
            { "A_cutoff", 1800 }, { "A_reso", 0.25f }, { "A_fenv", 0.2f },
            { "A_aA", 1.2f }, { "A_aD", 1.5f }, { "A_aS", 0.8f }, { "A_aR", 2.5f },
            { "A_fA", 2.0f }, { "A_fD", 3.0f }, { "A_fS", 0.4f }, { "A_fR", 2.5f },
            { "A_lfoRate", 0.15f }, { "A_lfoCut", 0.25f },
            { "B_on", 1 }, { "B_wave", 0.6f }, { "B_octave", 1 }, { "B_unison", 3 }, { "B_detune", 8 },
            { "B_level", 0.45f }, { "B_cutoff", 6000 }, { "B_aA", 2.0f }, { "B_aR", 3.0f },
            { "chorusMix", 0.45f }, { "delayMix", 0.25f }, { "delayTime", 450 }, { "delayFb", 0.45f },
            { "revSize", 0.92f }, { "revMix", 0.5f } } },

        { "Warm Analog Strings", {
            { "A_wave", 2.0f }, { "A_unison", 5 }, { "A_detune", 14 }, { "A_drift", 0.5f },
            { "A_ftype", 1 }, { "A_cutoff", 2500 }, { "A_reso", 0.1f }, { "A_fenv", 0.15f },
            { "A_aA", 0.35f }, { "A_aD", 0.8f }, { "A_aS", 0.85f }, { "A_aR", 1.2f },
            { "A_lfoRate", 5.0f }, { "A_lfoPitch", 0.08f },
            { "B_on", 1 }, { "B_wave", 3.0f }, { "B_octave", -1 }, { "B_level", 0.35f }, { "B_cutoff", 1400 },
            { "B_aA", 0.4f }, { "B_aR", 1.2f },
            { "chorusMix", 0.6f }, { "chorusDepth", 0.6f }, { "revMix", 0.35f } } },

        { "Dark Matter Drone", {
            { "A_wave", 2.6f }, { "A_octave", -1 }, { "A_unison", 7 }, { "A_detune", 35 }, { "A_drift", 0.8f },
            { "A_sub", 0.3f }, { "A_noise", 0.1f }, { "A_cutoff", 600 }, { "A_reso", 0.55f },
            { "A_drive", 0.4f }, { "A_aA", 3.0f }, { "A_aR", 5.0f },
            { "A_lfoRate", 0.07f }, { "A_lfoCut", 0.6f },
            { "B_on", 1 }, { "B_wave", 1.0f }, { "B_octave", -2 }, { "B_level", 0.35f }, { "B_cutoff", 300 },
            { "B_aA", 4.0f }, { "B_aR", 6.0f },
            { "revSize", 0.98f }, { "revMix", 0.55f }, { "delayMix", 0.3f }, { "delayTime", 900 }, { "delayFb", 0.6f } } },

        { "Crystal Pluck", {
            { "A_wave", 3.0f }, { "A_unison", 3 }, { "A_detune", 10 }, { "A_cutoff", 400 }, { "A_reso", 0.35f },
            { "A_fenv", 0.75f }, { "A_fA", 0.001f }, { "A_fD", 0.35f }, { "A_fS", 0.0f }, { "A_fR", 0.3f },
            { "A_aA", 0.001f }, { "A_aD", 0.6f }, { "A_aS", 0.0f }, { "A_aR", 0.5f },
            { "B_on", 1 }, { "B_wave", 0.0f }, { "B_octave", 2 }, { "B_level", 0.3f },
            { "B_aA", 0.001f }, { "B_aD", 0.3f }, { "B_aS", 0.0f }, { "B_aR", 0.4f },
            { "delayMix", 0.35f }, { "delayTime", 333 }, { "delayFb", 0.5f }, { "revMix", 0.3f } } },

        { "Analog Brass Stack", {
            { "A_wave", 2.0f }, { "A_unison", 3 }, { "A_detune", 9 }, { "A_ftype", 1 }, { "A_cutoff", 500 },
            { "A_reso", 0.15f }, { "A_fenv", 0.6f }, { "A_fA", 0.08f }, { "A_fD", 0.6f }, { "A_fS", 0.5f },
            { "A_aA", 0.05f }, { "A_aS", 0.9f }, { "A_aR", 0.25f }, { "A_drive", 0.3f },
            { "B_on", 1 }, { "B_wave", 2.0f }, { "B_semi", 7 }, { "B_level", 0.4f }, { "B_cutoff", 900 },
            { "chorusMix", 0.25f }, { "revMix", 0.2f } } },

        { "Ethereal Choir", {
            { "A_wave", 1.3f }, { "A_unison", 7 }, { "A_detune", 18 }, { "A_spread", 1.0f },
            { "A_ftype", 2 }, { "A_cutoff", 1100 }, { "A_reso", 0.5f }, { "A_lfoRate", 0.3f }, { "A_lfoCut", 0.35f },
            { "A_aA", 1.0f }, { "A_aR", 2.0f },
            { "B_on", 1 }, { "B_wave", 0.0f }, { "B_octave", 1 }, { "B_noise", 0.15f }, { "B_level", 0.35f },
            { "B_ftype", 2 }, { "B_cutoff", 2600 }, { "B_reso", 0.6f }, { "B_aA", 1.5f }, { "B_aR", 2.5f },
            { "chorusMix", 0.5f }, { "revSize", 0.95f }, { "revMix", 0.6f } } },

        { "Deep Sub Bass", {
            { "A_wave", 2.0f }, { "A_octave", -2 }, { "A_unison", 1 }, { "A_sub", 0.8f }, { "A_ftype", 1 },
            { "A_cutoff", 300 }, { "A_reso", 0.2f }, { "A_fenv", 0.35f }, { "A_fD", 0.25f }, { "A_fS", 0.1f },
            { "A_aA", 0.002f }, { "A_aS", 1.0f }, { "A_aR", 0.15f }, { "A_drive", 0.5f }, { "A_keytrack", 0.2f },
            { "B_on", 0 }, { "chorusMix", 0.0f }, { "delayMix", 0.0f }, { "revMix", 0.05f } } },

        { "Shimmer Keys", {
            { "A_wave", 0.5f }, { "A_unison", 2 }, { "A_detune", 6 }, { "A_cutoff", 5000 },
            { "A_aA", 0.003f }, { "A_aD", 1.8f }, { "A_aS", 0.3f }, { "A_aR", 1.2f },
            { "A_lfoRate", 4.5f }, { "A_lfoAmp", 0.15f },
            { "B_on", 1 }, { "B_wave", 1.0f }, { "B_octave", 1 }, { "B_unison", 5 }, { "B_detune", 15 },
            { "B_level", 0.3f }, { "B_aA", 0.6f }, { "B_aR", 2.0f },
            { "chorusMix", 0.4f }, { "delayMix", 0.3f }, { "delayTime", 500 }, { "revSize", 0.85f }, { "revMix", 0.45f } } },
    };
    return presets;
}
} // namespace neb
