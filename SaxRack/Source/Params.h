// Parameter definitions shared by the processor, the editor and the presets.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace rs::params
{
inline const juce::StringArray wahModes      { "Env Up", "Env Down", "LFO", "Pedal" };
inline const juce::StringArray driveTypes    { "Tube", "Fuzz", "Growl", "Tape" };
inline const juce::StringArray synthWaves    { "Saw", "Square", "Saw+Sub", "Soft" };
inline const juce::StringArray synthOctaves  { "-2 oct", "-1 oct", "0", "+1 oct" };
inline const juce::StringArray harmModes     { "Diatonic", "Chromatic" };
inline const juce::StringArray keys          { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
inline const juce::StringArray scales        { "Major", "Minor", "Dorian", "Mixolydian", "Harm Minor", "Blues", "Maj Pent", "Min Pent" };
inline const juce::StringArray modTypes      { "Chorus", "Flanger", "Phaser", "Rotary" };
inline const juce::StringArray delaySyncs    { "Free", "1/2", "1/4", "1/4 dot", "1/4 trip", "1/8", "1/8 dot", "1/8 trip", "1/16" };
inline const juce::StringArray genSounds     { "Bells", "Pad", "Bowed", "Drone", "Choir", "Swarm" };
inline const juce::StringArray genPatterns   { "Arp Up", "Random Walk", "Chord Swell", "Cascade" };
inline const juce::StringArray genRates      { "1/4", "1/8", "1/8 trip", "1/16", "1/16 trip", "1/32" };
inline const juce::StringArray grmPalettes   { "All Creatures", "Cartoon", "Sci-Fi", "Voice Mangler", "Scrapyard" };
inline const juce::StringArray instruments   { "Concert C", "Soprano Bb", "Alto Eb", "Tenor Bb", "Bari Eb" };

/** Semitones from sounding (concert) pitch to the written note, per instrument. */
inline int writtenOffset (int instrument)
{
    static const int off[] = { 0, 2, 9, 14, 21 };
    return off[juce::jlimit (0, 4, instrument)];
}

/** Scale intervals for the diatonic harmonizer. */
inline const std::vector<int>& scaleSteps (int scale)
{
    static const std::vector<std::vector<int>> s {
        { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 2, 4, 5, 7, 9, 10 },
        { 0, 2, 3, 5, 7, 8, 11 }, { 0, 3, 5, 6, 7, 10 },    { 0, 2, 4, 7, 9 },        { 0, 3, 5, 7, 10 } };
    return s[(size_t) juce::jlimit (0, (int) s.size() - 1, scale)];
}

inline juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
{
    juce::NormalisableRange<float> r (lo, hi);
    r.setSkewForCentre (centre);
    return r;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    int v = 1;

    auto fl = [&] (const char* id, const char* name, NormalisableRange<float> r, float def, const char* unit = "", int decimals = 2)
    {
        const String u (unit);
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id, v }, name, r, def,
            AudioParameterFloatAttributes().withLabel (u).withStringFromValueFunction (
                [u, decimals] (float x, int)
                {
                    if (u == "%")  return String (roundToInt (x * 100.0f)) + "%";
                    if (u == "Hz" && x >= 1000.0f) return String (x / 1000.0f, 2) + " kHz";
                    if (u == "Hz" && x >= 100.0f)  return String (roundToInt (x)) + " Hz";
                    if (u == "ms" && x >= 1000.0f) return String (x / 1000.0f, 2) + " s";
                    if (u == "ms") return String (roundToInt (x)) + " ms";
                    if (u == "dB") return (x > 0 ? "+" : "") + String (x, 1) + " dB";
                    return String (x, decimals) + (u.isEmpty() ? "" : " " + u);
                })));
    };
    auto pct = [&] (const char* id, const char* name, float def) { fl (id, name, { 0.0f, 1.0f }, def, "%"); };
    auto bl = [&] (const char* id, const char* name, bool def) { layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, v }, name, def)); };
    auto ch = [&] (const char* id, const char* name, const StringArray& c, int def) { layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, v }, name, c, def)); };
    auto in = [&] (const char* id, const char* name, int lo, int hi, int def)
    {
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { id, v }, name, lo, hi, def,
            AudioParameterIntAttributes().withStringFromValueFunction ([] (int x, int) { return (x > 0 ? "+" : "") + String (x); })));
    };

    // Input & dynamics
    fl ("in_gain", "Input Gain", { -24.0f, 24.0f }, 0.0f, "dB");
    bl ("dyn_on", "Dynamics On", true);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "gate_thr", v }, "Gate Threshold",
        NormalisableRange<float> (-80.0f, -20.0f), -80.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (
            [] (float x, int) { return x <= -79.9f ? String ("Off") : String (x, 1) + " dB"; })));
    pct ("comp_amt", "Compression", 0.3f);
    pct ("comp_speed", "Comp Speed", 0.5f);

    // Octaver
    bl ("oct_on", "Octaver On", false);
    pct ("oct_sub1", "Sub Octave", 0.6f);
    pct ("oct_sub2", "Sub 2 Octaves", 0.0f);
    pct ("oct_up", "Octave Up", 0.0f);
    pct ("oct_dry", "Octave Dry", 1.0f);

    // Tracking synth
    bl ("syn_on", "Synth On", false);
    ch ("syn_wave", "Synth Wave", synthWaves, 0);
    ch ("syn_oct", "Synth Octave", synthOctaves, 1);
    fl ("syn_cut", "Synth Cutoff", skewed (80.0f, 12000.0f, 1200.0f), 1400.0f, "Hz");
    pct ("syn_res", "Synth Resonance", 0.3f);
    pct ("syn_env", "Synth Filter Env", 0.5f);
    fl ("syn_glide", "Synth Glide", skewed (0.0f, 400.0f, 60.0f), 25.0f, "ms");
    pct ("syn_lvl", "Synth Level", 0.5f);

    // Envelope filter / wah
    bl ("wah_on", "Wah On", false);
    ch ("wah_mode", "Wah Mode", wahModes, 0);
    pct ("wah_sens", "Wah Sensitivity", 0.6f);
    fl ("wah_freq", "Wah Frequency", skewed (150.0f, 2500.0f, 500.0f), 380.0f, "Hz");
    fl ("wah_range", "Wah Range", { 0.5f, 4.5f }, 2.5f, "oct", 1);
    fl ("wah_q", "Wah Resonance", skewed (0.7f, 14.0f, 4.0f), 4.5f, "", 1);
    fl ("wah_rate", "Wah LFO Rate", skewed (0.05f, 10.0f, 1.5f), 1.5f, "Hz");
    pct ("wah_mix", "Wah Mix", 1.0f);

    // Drive
    bl ("drv_on", "Drive On", false);
    ch ("drv_type", "Drive Type", driveTypes, 0);
    pct ("drv_drive", "Drive", 0.4f);
    pct ("drv_char", "Drive Character", 0.3f);
    pct ("drv_tone", "Drive Tone", 0.55f);
    fl ("drv_level", "Drive Level", { -24.0f, 12.0f }, 0.0f, "dB");
    pct ("drv_mix", "Drive Mix", 1.0f);

    // EQ
    bl ("eq_on", "EQ On", true);
    fl ("eq_lowcut", "Low Cut", skewed (20.0f, 500.0f, 90.0f), 40.0f, "Hz");
    fl ("eq_body", "Body", { -12.0f, 12.0f }, 0.0f, "dB");
    fl ("eq_mid", "Honk", { -12.0f, 12.0f }, 0.0f, "dB");
    fl ("eq_midf", "Honk Freq", skewed (300.0f, 5000.0f, 1200.0f), 1200.0f, "Hz");
    fl ("eq_air", "Air", { -12.0f, 12.0f }, 0.0f, "dB");

    // Harmonizer
    bl ("har_on", "Harmony On", false);
    ch ("har_mode", "Harmony Mode", harmModes, 0);
    ch ("har_key", "Harmony Key", keys, 0);
    ch ("har_scale", "Harmony Scale", scales, 0);
    in ("har_int1", "Voice 1 Interval", -12, 12, 2);
    in ("har_int2", "Voice 2 Interval", -12, 12, -3);
    pct ("har_lvl1", "Voice 1 Level", 0.6f);
    pct ("har_lvl2", "Voice 2 Level", 0.0f);
    pct ("har_spread", "Harmony Spread", 0.6f);

    // Modulation
    bl ("mod_on", "Mod On", false);
    ch ("mod_type", "Mod Type", modTypes, 0);
    fl ("mod_rate", "Mod Rate", skewed (0.03f, 8.0f, 1.0f), 0.8f, "Hz");
    pct ("mod_depth", "Mod Depth", 0.5f);
    pct ("mod_fb", "Mod Feedback", 0.3f);
    pct ("mod_mix", "Mod Mix", 0.5f);

    // Delay
    bl ("dly_on", "Delay On", false);
    ch ("dly_sync", "Delay Sync", delaySyncs, 6);
    fl ("dly_time", "Delay Time", skewed (20.0f, 2000.0f, 400.0f), 380.0f, "ms");
    pct ("dly_fb", "Delay Feedback", 0.35f);
    fl ("dly_tone", "Delay Tone", skewed (400.0f, 14000.0f, 3000.0f), 4000.0f, "Hz");
    pct ("dly_wobble", "Delay Wobble", 0.15f);
    pct ("dly_duck", "Delay Ducking", 0.3f);
    pct ("dly_mix", "Delay Mix", 0.3f);
    bl ("dly_pp", "Ping-Pong", true);

    // Reverb
    bl ("rev_on", "Reverb On", false);
    fl ("rev_pre", "Pre-Delay", { 0.0f, 200.0f }, 20.0f, "ms");
    pct ("rev_size", "Room Size", 0.6f);
    fl ("rev_decay", "Decay", skewed (0.2f, 15.0f, 2.5f), 2.5f, "s", 1);
    pct ("rev_damp", "Damping", 0.4f);
    pct ("rev_shimmer", "Shimmer", 0.0f);
    pct ("rev_mix", "Reverb Mix", 0.3f);

    // Morph generator: frequency zones -> synth sounds
    bl ("gen_on", "Morph Gen On", false);
    fl ("gen_split1", "Zone Split 1", skewed (60.0f, 1600.0f, 300.0f), 220.0f, "Hz");
    fl ("gen_split2", "Zone Split 2", skewed (60.0f, 1600.0f, 300.0f), 370.0f, "Hz");
    fl ("gen_split3", "Zone Split 3", skewed (60.0f, 1600.0f, 300.0f), 620.0f, "Hz");
    ch ("gen_snd1", "Zone 1 Sound", genSounds, 3);
    ch ("gen_snd2", "Zone 2 Sound", genSounds, 1);
    ch ("gen_snd3", "Zone 3 Sound", genSounds, 0);
    ch ("gen_snd4", "Zone 4 Sound", genSounds, 4);
    pct ("gen_morph", "Morph", 0.4f);
    fl ("gen_pmorph", "Pitch to Morph", { -1.0f, 1.0f }, 0.0f, "", 2);
    pct ("gen_bloom", "Bloom Level", 0.6f);
    ch ("gen_pattern", "Bloom Pattern", genPatterns, 0);
    ch ("gen_rate", "Bloom Rate", genRates, 3);
    fl ("gen_length", "Bloom Length", skewed (0.5f, 16.0f, 4.0f), 4.0f, "s", 1);
    pct ("gen_density", "Bloom Density", 0.75f);
    in ("gen_range", "Bloom Range", 1, 3, 2);
    fl ("gen_hold", "Trigger Hold", skewed (20.0f, 800.0f, 150.0f), 120.0f, "ms");

    // Gremlins
    bl ("grm_on", "Gremlins On", false);
    pct ("grm_sens", "Gremlin Sensitivity", 0.6f);
    pct ("grm_chance", "Gremlin Chance", 0.7f);
    pct ("grm_harsh", "Silly / Harsh", 0.3f);
    pct ("grm_dirt", "Gremlin Dirt", 0.2f);
    pct ("grm_follow", "Gremlin Pitch Follow", 0.5f);
    pct ("grm_swarm", "Gremlin Swarm", 0.3f);
    pct ("grm_level", "Gremlin Level", 0.6f);
    ch ("grm_palette", "Gremlin Palette", grmPalettes, 0);

    // Loop station (4 tracks)
    bl ("loop_on", "Loop Station On", true);
    bl ("loop_lock", "Loop Lock To Master", true);
    bl ("loop_snap", "Loop Snap to Beat", false);
    fl ("loop_fb", "Overdub Feedback", { 0.3f, 1.0f }, 1.0f, "%");
    for (int t = 1; t <= 4; ++t)
    {
        const juce::String n (t);
        const auto id = [&] (const char* suffix) { return juce::String ("loop") + n + "_" + suffix; };
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id ("level"), v }, "Track " + n + " Level",
            NormalisableRange<float> (0.0f, 1.5f), 1.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return String (roundToInt (x * 100.0f)) + "%"; })));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id ("pan"), v }, "Track " + n + " Pan",
            NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int)
            {
                if (std::abs (x) < 0.01f) return String ("C");
                return (x < 0 ? "L" : "R") + String (roundToInt (std::abs (x) * 100.0f));
            })));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id ("mute"), v }, "Track " + n + " Mute", false));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id ("rev"), v }, "Track " + n + " Reverse", false));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id ("half"), v }, "Track " + n + " Half Speed", false));
        // momentary footswitch parameters, MIDI-learnable in the host
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id ("tap"), v }, "Track " + n + " Rec/Play/Dub", false));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id ("stop"), v }, "Track " + n + " Stop", false));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id ("undo"), v }, "Track " + n + " Undo", false));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id ("clear"), v }, "Track " + n + " Clear", false));
    }
    bl ("loop_stopall", "Loop Stop All", false);
    bl ("loop_clearall", "Loop Clear All", false);

    // Output & tuner
    fl ("out_width", "Stereo Width", { 0.0f, 2.0f }, 1.0f, "%");
    fl ("out_gain", "Output Gain", { -24.0f, 12.0f }, 0.0f, "dB");
    ch ("tuner_inst", "Instrument", instruments, 2);
    fl ("tuner_ref", "Tuner Reference", { 430.0f, 450.0f, 0.5f }, 440.0f, "Hz", 1);

    return layout;
}
} // namespace rs::params
