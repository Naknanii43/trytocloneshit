#pragma once

#include <vector>
#include <utility>

namespace neb
{
// Index cheat-sheet used below:
//  mode: 0 Analog 1 Wavetable 2 FM 3 Granular 4 Sample
//  src : 0 Glass Bells 1 Breath Choir 2 Vocal Ooh 3 Bowed Metal 4 Kalimba 5 Tape Strings
//        6 Crystal Shimmer 7 Ocean Wash 8 Vinyl Air
//  wtTable: 0 Classic 1 Pulse 2 Vowel 3 HarmSweep 4 HardSync 5 DigiBells 6 Drawbar 7 Growl 8 CrystalDust
//  ftype: 0 LP12 1 LP24 2 Band 3 High 4 Notch 5 Comb+ 6 Comb- 7 Vowel
//  mod src: 1-4 LFO  5,6 ModEnv  7 Vel  8 ModWheel  9 AT  10 Key  11 Random  12-15 Macro
//  mod dst: 1 Pitch 2 Cutoff 3 Reso 4 Level 5 Pan 6 Timbre 7 Detune 8 Drive 9 Sub 10 Noise
//           11 GrainSize 12 GrainDensity 13 FilterEnv
//  mod layer: 0 All 1 A 2 B 3 C 4 D
//  lfo shape: 0 Sine 1 Tri 2 SawUp 3 SawDn 4 Square 5 S&H 6 Drift
//  div: 6 1/4  8 1/8D  9 1/8  10 1/8T  12 1/16  13 1/16T  2 1/1  1 2bars  0 4bars
struct Preset
{
    const char* name;
    const char* category;
    std::vector<std::pair<const char*, float>> values; // overrides on top of defaults
};

inline const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets {
        { "Init", "Init", {} },

        // ------------------------------------------------------------------ PADS
        { "Glass Horizon", "Pads", {
            { "A_wave", 2 }, { "A_unison", 7 }, { "A_detune", 22 }, { "A_spread", 0.9f }, { "A_cutoff", 1800 },
            { "A_reso", 0.25f }, { "A_aA", 1.2f }, { "A_aD", 1.5f }, { "A_aS", 0.8f }, { "A_aR", 2.5f },
            { "A_fA", 2.0f }, { "A_fD", 3.0f }, { "A_fS", 0.4f },
            { "B_on", 1 }, { "B_mode", 3 }, { "B_src", 0 }, { "B_octave", 1 }, { "B_grSize", 220 }, { "B_grDensity", 25 },
            { "B_grSpray", 0.5f }, { "B_level", 0.5f }, { "B_aA", 2.0f }, { "B_aR", 3.0f }, { "B_cutoff", 7000 },
            { "lfo1_rate", 0.12f }, { "m1_src", 1 }, { "m1_dst", 2 }, { "m1_amt", 0.2f },
            { "chorusMix", 0.45f }, { "delayMix", 0.25f }, { "delayFb", 0.45f }, { "revSize", 0.92f }, { "revMix", 0.5f } } },

        { "Nebula Choir", "Pads", {
            { "A_mode", 3 }, { "A_src", 1 }, { "A_grSize", 260 }, { "A_grDensity", 30 }, { "A_grPos", 0.4f }, { "A_grSpray", 0.4f },
            { "A_unison", 1 }, { "A_harm1", 7 }, { "A_cutoff", 4500 }, { "A_aA", 1.0f }, { "A_aR", 2.5f },
            { "B_on", 1 }, { "B_mode", 1 }, { "B_wtTable", 2 }, { "B_wtPos", 0.2f }, { "B_unison", 5 }, { "B_detune", 15 },
            { "B_level", 0.45f }, { "B_aA", 1.5f }, { "B_aR", 2.5f },
            { "lfo1_rate", 0.08f }, { "lfo1_shape", 6 }, { "m1_src", 1 }, { "m1_dst", 6 }, { "m1_layer", 2 }, { "m1_amt", 0.4f },
            { "lfo2_rate", 0.15f }, { "m2_src", 2 }, { "m2_dst", 6 }, { "m2_layer", 1 }, { "m2_amt", 0.25f },
            { "revSize", 0.95f }, { "revMix", 0.55f }, { "chorusMix", 0.4f } } },

        { "Evolving Wavescape", "Pads", {
            { "A_mode", 1 }, { "A_wtTable", 3 }, { "A_unison", 5 }, { "A_detune", 18 }, { "A_cutoff", 3000 },
            { "A_aA", 1.5f }, { "A_aR", 3.0f },
            { "B_on", 1 }, { "B_mode", 1 }, { "B_wtTable", 7 }, { "B_octave", -1 }, { "B_unison", 3 }, { "B_level", 0.5f },
            { "B_aA", 2.5f }, { "B_aR", 3.0f },
            { "lfo1_rate", 0.05f }, { "lfo1_shape", 1 }, { "m1_src", 1 }, { "m1_dst", 6 }, { "m1_layer", 1 }, { "m1_amt", 0.5f },
            { "lfo2_rate", 0.07f }, { "lfo2_shape", 6 }, { "m2_src", 2 }, { "m2_dst", 6 }, { "m2_layer", 2 }, { "m2_amt", 0.5f },
            { "lfo3_rate", 0.2f }, { "m3_src", 3 }, { "m3_dst", 5 }, { "m3_amt", 0.4f },
            { "revSize", 0.9f }, { "revMix", 0.45f }, { "delayMix", 0.2f } } },

        { "Warm Analog Strings", "Pads", {
            { "A_unison", 5 }, { "A_detune", 14 }, { "A_drift", 0.5f }, { "A_cutoff", 2500 }, { "A_reso", 0.1f },
            { "A_fenv", 0.15f }, { "A_aA", 0.35f }, { "A_aD", 0.8f }, { "A_aS", 0.85f }, { "A_aR", 1.2f },
            { "B_on", 1 }, { "B_mode", 4 }, { "B_src", 5 }, { "B_level", 0.45f }, { "B_aA", 0.4f }, { "B_aR", 1.2f },
            { "lfo1_rate", 5.0f }, { "lfo1_fade", 1.0f }, { "m1_src", 1 }, { "m1_dst", 1 }, { "m1_amt", 0.008f },
            { "chorusMix", 0.6f }, { "chorusDepth", 0.6f }, { "revMix", 0.35f } } },

        { "Frozen Lake", "Pads", {
            { "A_mode", 3 }, { "A_src", 6 }, { "A_grSize", 400 }, { "A_grDensity", 18 }, { "A_grSpray", 0.8f },
            { "A_harm1", 12 }, { "A_harm2", 19 }, { "A_ftype", 3 }, { "A_cutoff", 400 }, { "A_aA", 2.0f }, { "A_aR", 4.0f },
            { "B_on", 1 }, { "B_mode", 0 }, { "B_wave", 0.3f }, { "B_unison", 3 }, { "B_level", 0.45f }, { "B_octave", -1 },
            { "B_aA", 1.5f }, { "B_aR", 4.0f },
            { "lfo1_rate", 0.1f }, { "m1_src", 1 }, { "m1_dst", 11 }, { "m1_layer", 1 }, { "m1_amt", 0.4f },
            { "revSize", 0.98f }, { "revMix", 0.6f }, { "delayMix", 0.3f }, { "delayFb", 0.6f } } },

        { "Solar Wind", "Pads", {
            { "A_mode", 1 }, { "A_wtTable", 4 }, { "A_wtPos", 0.3f }, { "A_unison", 7 }, { "A_detune", 25 },
            { "A_cutoff", 2200 }, { "A_aA", 0.8f }, { "A_aR", 2.0f },
            { "B_on", 1 }, { "B_mode", 3 }, { "B_src", 7 }, { "B_level", 0.5f }, { "B_ftype", 2 }, { "B_cutoff", 1500 },
            { "lfo1_rate", 0.18f }, { "m1_src", 1 }, { "m1_dst", 6 }, { "m1_layer", 1 }, { "m1_amt", 0.3f },
            { "phOn", 1 }, { "phRate", 0.1f }, { "revMix", 0.45f } } },

        // ------------------------------------------------------------------ KEYS
        { "Shimmer Keys", "Keys", {
            { "A_wave", 0.5f }, { "A_unison", 2 }, { "A_detune", 6 }, { "A_cutoff", 5000 },
            { "A_aA", 0.003f }, { "A_aD", 1.8f }, { "A_aS", 0.3f }, { "A_aR", 1.2f },
            { "B_on", 1 }, { "B_mode", 4 }, { "B_src", 0 }, { "B_octave", 1 }, { "B_level", 0.35f },
            { "B_aA", 0.001f }, { "B_aD", 2.0f }, { "B_aS", 0.0f }, { "B_aR", 1.5f },
            { "lfo1_rate", 4.5f }, { "m1_src", 1 }, { "m1_dst", 4 }, { "m1_amt", 0.12f },
            { "chorusMix", 0.4f }, { "delayMix", 0.3f }, { "revSize", 0.85f }, { "revMix", 0.45f } } },

        { "FM Electric Piano", "Keys", {
            { "A_mode", 2 }, { "A_fmRatio", 1 }, { "A_fmIndex", 2.5f }, { "A_unison", 1 }, { "A_cutoff", 8000 },
            { "A_aA", 0.002f }, { "A_aD", 2.5f }, { "A_aS", 0.15f }, { "A_aR", 0.5f },
            { "B_on", 1 }, { "B_mode", 2 }, { "B_fmRatio", 14 }, { "B_fmIndex", 1.5f }, { "B_unison", 1 }, { "B_level", 0.25f },
            { "B_aA", 0.001f }, { "B_aD", 0.25f }, { "B_aS", 0.0f }, { "B_aR", 0.2f },
            { "me1_A", 0.001f }, { "me1_D", 1.2f }, { "me1_S", 0.1f },
            { "m1_src", 5 }, { "m1_dst", 6 }, { "m1_layer", 1 }, { "m1_amt", 0.3f },
            { "m2_src", 7 }, { "m2_dst", 6 }, { "m2_layer", 1 }, { "m2_amt", 0.25f },
            { "phOn", 1 }, { "phMix", 0.3f }, { "chorusMix", 0.3f }, { "revMix", 0.25f } } },

        { "Digital Bell Tower", "Keys", {
            { "A_mode", 1 }, { "A_wtTable", 5 }, { "A_wtPos", 0.4f }, { "A_unison", 2 }, { "A_detune", 5 },
            { "A_aA", 0.001f }, { "A_aD", 3.0f }, { "A_aS", 0.0f }, { "A_aR", 2.5f }, { "A_cutoff", 9000 },
            { "B_on", 1 }, { "B_mode", 2 }, { "B_fmRatio", 3.5f }, { "B_fmIndex", 3 }, { "B_octave", 1 }, { "B_level", 0.3f },
            { "B_aA", 0.001f }, { "B_aD", 1.5f }, { "B_aS", 0.0f }, { "B_aR", 2.0f },
            { "delayMix", 0.3f }, { "revSize", 0.9f }, { "revMix", 0.5f } } },

        { "Kalimba Dreams", "Keys", {
            { "A_mode", 4 }, { "A_src", 4 }, { "A_smpLoop", 0 }, { "A_unison", 1 }, { "A_aA", 0.001f }, { "A_aS", 1.0f }, { "A_aR", 1.5f },
            { "B_on", 1 }, { "B_mode", 3 }, { "B_src", 4 }, { "B_grPos", 0.05f }, { "B_grSize", 80 }, { "B_grDensity", 12 },
            { "B_level", 0.3f }, { "B_octave", 1 }, { "B_aA", 0.3f }, { "B_aR", 2.5f },
            { "delayMix", 0.35f }, { "delayDiv", 9 }, { "revMix", 0.4f } } },

        { "Drawbar Organ", "Keys", {
            { "A_mode", 1 }, { "A_wtTable", 6 }, { "A_wtPos", 0.6f }, { "A_unison", 1 }, { "A_cutoff", 12000 },
            { "A_aA", 0.005f }, { "A_aS", 1.0f }, { "A_aR", 0.08f }, { "A_drive", 0.3f }, { "A_velSens", 0 },
            { "lfo1_rate", 6.5f }, { "m1_src", 1 }, { "m1_dst", 1 }, { "m1_amt", 0.004f },
            { "chOn", 1 }, { "chorusMix", 0.6f }, { "chorusRate", 1.2f }, { "dlOn", 0 }, { "revMix", 0.2f } } },

        // ------------------------------------------------------------------ BASS
        { "Deep Sub Bass", "Bass", {
            { "A_octave", -2 }, { "A_unison", 1 }, { "A_sub", 0.8f }, { "A_cutoff", 300 }, { "A_reso", 0.2f },
            { "A_fenv", 0.35f }, { "A_fD", 0.25f }, { "A_fS", 0.1f }, { "A_aA", 0.002f }, { "A_aS", 1.0f }, { "A_aR", 0.15f },
            { "A_drive", 0.5f }, { "A_keytrack", 0.2f }, { "mono", 1 }, { "glide", 0.05f },
            { "chOn", 0 }, { "dlOn", 0 }, { "revMix", 0.05f } } },

        { "Wobble Monster", "Bass", {
            { "A_mode", 1 }, { "A_wtTable", 7 }, { "A_octave", -2 }, { "A_unison", 3 }, { "A_detune", 10 }, { "A_sub", 0.5f },
            { "A_cutoff", 400 }, { "A_reso", 0.4f }, { "A_aA", 0.003f }, { "A_aS", 1.0f }, { "A_aR", 0.15f }, { "A_drive", 0.6f },
            { "lfo1_sync", 1 }, { "lfo1_div", 10 }, { "lfo1_retrig", 1 },
            { "m1_src", 1 }, { "m1_dst", 2 }, { "m1_amt", 0.45f }, { "m2_src", 1 }, { "m2_dst", 6 }, { "m2_amt", 0.4f },
            { "mono", 1 }, { "distOn", 1 }, { "distDrive", 0.4f }, { "chOn", 0 }, { "dlOn", 0 }, { "revMix", 0.1f } } },

        { "Reese Growl", "Bass", {
            { "A_octave", -1 }, { "A_unison", 2 }, { "A_detune", 30 }, { "A_cutoff", 900 }, { "A_reso", 0.25f },
            { "A_aA", 0.005f }, { "A_aS", 1.0f }, { "A_aR", 0.2f }, { "A_drive", 0.5f },
            { "B_on", 1 }, { "B_octave", -2 }, { "B_wave", 0 }, { "B_unison", 1 }, { "B_level", 0.6f }, { "B_aR", 0.2f },
            { "lfo1_rate", 0.3f }, { "m1_src", 1 }, { "m1_dst", 2 }, { "m1_layer", 1 }, { "m1_amt", 0.2f },
            { "mono", 1 }, { "distOn", 1 }, { "distType", 0 }, { "chOn", 0 }, { "dlOn", 0 }, { "revMix", 0.08f } } },

        { "FM Slap Bass", "Bass", {
            { "A_mode", 2 }, { "A_octave", -2 }, { "A_fmRatio", 1 }, { "A_fmIndex", 1 }, { "A_fmFb", 0.3f }, { "A_unison", 1 },
            { "A_aA", 0.001f }, { "A_aD", 0.6f }, { "A_aS", 0.4f }, { "A_aR", 0.1f }, { "A_cutoff", 6000 },
            { "me1_A", 0.001f }, { "me1_D", 0.15f }, { "me1_S", 0 },
            { "m1_src", 5 }, { "m1_dst", 6 }, { "m1_amt", 0.4f }, { "mono", 1 }, { "chOn", 0 }, { "dlOn", 0 }, { "revMix", 0.08f } } },

        { "Acid Line", "Bass", {
            { "A_octave", -1 }, { "A_wave", 2.8f }, { "A_unison", 1 }, { "A_ftype", 1 }, { "A_cutoff", 250 }, { "A_reso", 0.75f },
            { "A_fenv", 0.6f }, { "A_fA", 0.001f }, { "A_fD", 0.25f }, { "A_fS", 0 }, { "A_aA", 0.001f }, { "A_aS", 1 }, { "A_aR", 0.08f },
            { "A_drive", 0.6f }, { "mono", 1 }, { "legato", 1 }, { "glide", 0.06f },
            { "arpOn", 1 }, { "arpMode", 2 }, { "arpDiv", 12 }, { "arpOct", 2 }, { "arpGate", 0.5f }, { "arpSwing", 0.2f },
            { "distOn", 1 }, { "distDrive", 0.35f }, { "delayMix", 0.15f }, { "revMix", 0.12f } } },

        // ------------------------------------------------------------------ LEADS
        { "Hyper Saw Lead", "Leads", {
            { "A_unison", 7 }, { "A_detune", 28 }, { "A_cutoff", 6000 }, { "A_aA", 0.005f }, { "A_aS", 1.0f }, { "A_aR", 0.3f },
            { "B_on", 1 }, { "B_octave", 1 }, { "B_unison", 3 }, { "B_level", 0.35f },
            { "lfo1_rate", 5.5f }, { "lfo1_fade", 0.6f }, { "m1_src", 8 }, { "m1_dst", 1 }, { "m1_amt", 0.02f },
            { "m2_src", 1 }, { "m2_dst", 1 }, { "m2_amt", 0.006f },
            { "mono", 1 }, { "legato", 1 }, { "glide", 0.08f }, { "delayMix", 0.25f }, { "revMix", 0.3f } } },

        { "Vowel Talker", "Leads", {
            { "A_unison", 3 }, { "A_detune", 10 }, { "A_ftype", 7 }, { "A_cutoff", 300 }, { "A_reso", 0.6f },
            { "A_aA", 0.01f }, { "A_aS", 1.0f }, { "A_aR", 0.3f },
            { "lfo1_rate", 1.2f }, { "lfo1_shape", 1 }, { "m1_src", 1 }, { "m1_dst", 2 }, { "m1_amt", 0.25f },
            { "m2_src", 8 }, { "m2_dst", 2 }, { "m2_amt", 0.4f },
            { "mono", 1 }, { "glide", 0.05f }, { "delayMix", 0.25f }, { "revMix", 0.25f } } },

        { "Sync Screamer", "Leads", {
            { "A_mode", 1 }, { "A_wtTable", 4 }, { "A_unison", 3 }, { "A_detune", 8 }, { "A_cutoff", 9000 },
            { "A_aA", 0.003f }, { "A_aS", 1.0f }, { "A_aR", 0.25f },
            { "me1_A", 0.01f }, { "me1_D", 0.8f }, { "me1_S", 0.2f },
            { "m1_src", 5 }, { "m1_dst", 6 }, { "m1_amt", 0.7f }, { "m2_src", 8 }, { "m2_dst", 6 }, { "m2_amt", 0.4f },
            { "mono", 1 }, { "glide", 0.04f }, { "distOn", 1 }, { "distDrive", 0.25f }, { "delayMix", 0.3f } } },

        { "Glass Flute", "Leads", {
            { "A_wave", 0.2f }, { "A_unison", 1 }, { "A_noise", 0.12f }, { "A_cutoff", 3500 },
            { "A_aA", 0.08f }, { "A_aS", 0.9f }, { "A_aR", 0.4f },
            { "B_on", 1 }, { "B_mode", 3 }, { "B_src", 1 }, { "B_octave", 1 }, { "B_level", 0.25f }, { "B_grSize", 60 },
            { "lfo1_rate", 5.0f }, { "lfo1_fade", 0.8f }, { "m1_src", 1 }, { "m1_dst", 1 }, { "m1_amt", 0.008f },
            { "mono", 1 }, { "glide", 0.05f }, { "delayMix", 0.25f }, { "revMix", 0.4f } } },

        // ------------------------------------------------------------------ PLUCKS
        { "Crystal Pluck", "Plucks", {
            { "A_wave", 3 }, { "A_unison", 3 }, { "A_detune", 10 }, { "A_cutoff", 400 }, { "A_reso", 0.35f },
            { "A_fenv", 0.75f }, { "A_fA", 0.001f }, { "A_fD", 0.35f }, { "A_fS", 0 }, { "A_fR", 0.3f },
            { "A_aA", 0.001f }, { "A_aD", 0.6f }, { "A_aS", 0 }, { "A_aR", 0.5f },
            { "B_on", 1 }, { "B_wave", 0 }, { "B_octave", 2 }, { "B_level", 0.3f },
            { "B_aA", 0.001f }, { "B_aD", 0.3f }, { "B_aS", 0 }, { "B_aR", 0.4f },
            { "delayMix", 0.35f }, { "delayFb", 0.5f }, { "revMix", 0.3f } } },

        { "Comb Harp", "Plucks", {
            { "A_unison", 1 }, { "A_noise", 0.8f }, { "A_wave", 0 }, { "A_ftype", 5 }, { "A_cutoff", 262 }, { "A_keytrack", 1.0f },
            { "A_reso", 0.92f }, { "A_aA", 0.001f }, { "A_aD", 1.5f }, { "A_aS", 0 }, { "A_aR", 1.2f },
            { "delayMix", 0.25f }, { "revMix", 0.4f } } },

        { "Pizzicato Dust", "Plucks", {
            { "A_mode", 1 }, { "A_wtTable", 8 }, { "A_unison", 2 }, { "A_cutoff", 900 }, { "A_fenv", 0.6f },
            { "A_fA", 0.001f }, { "A_fD", 0.2f }, { "A_fS", 0 }, { "A_aA", 0.001f }, { "A_aD", 0.35f }, { "A_aS", 0 }, { "A_aR", 0.3f },
            { "m1_src", 11 }, { "m1_dst", 6 }, { "m1_amt", 0.4f }, { "m2_src", 11 }, { "m2_dst", 5 }, { "m2_amt", 0.5f },
            { "delayMix", 0.3f }, { "delayDiv", 10 }, { "revMix", 0.35f } } },

        { "Kalimba Comb", "Plucks", {
            { "A_mode", 4 }, { "A_src", 4 }, { "A_smpLoop", 0 }, { "A_unison", 1 }, { "A_ftype", 6 }, { "A_cutoff", 523 },
            { "A_keytrack", 1 }, { "A_reso", 0.6f }, { "A_aA", 0.001f }, { "A_aS", 1 }, { "A_aR", 1.0f },
            { "delayMix", 0.3f }, { "revMix", 0.35f } } },

        // ------------------------------------------------------------------ ARPS
        { "Starlight Arp", "Arps", {
            { "A_wave", 2.5f }, { "A_unison", 3 }, { "A_cutoff", 1200 }, { "A_fenv", 0.5f }, { "A_fA", 0.001f }, { "A_fD", 0.2f },
            { "A_fS", 0.1f }, { "A_aA", 0.001f }, { "A_aD", 0.3f }, { "A_aS", 0.2f }, { "A_aR", 0.3f },
            { "arpOn", 1 }, { "arpMode", 0 }, { "arpDiv", 12 }, { "arpOct", 3 }, { "arpGate", 0.5f },
            { "lfo1_rate", 0.1f }, { "m1_src", 1 }, { "m1_dst", 2 }, { "m1_amt", 0.3f },
            { "delayMix", 0.35f }, { "delayDiv", 8 }, { "revMix", 0.35f } } },

        { "Bell Cascade", "Arps", {
            { "A_mode", 2 }, { "A_fmRatio", 3.5f }, { "A_fmIndex", 2 }, { "A_unison", 1 }, { "A_aA", 0.001f }, { "A_aD", 0.9f },
            { "A_aS", 0 }, { "A_aR", 1.0f },
            { "arpOn", 1 }, { "arpMode", 2 }, { "arpDiv", 13 }, { "arpOct", 2 }, { "arpGate", 0.4f },
            { "m1_src", 11 }, { "m1_dst", 5 }, { "m1_amt", 0.6f },
            { "delayMix", 0.35f }, { "revSize", 0.9f }, { "revMix", 0.5f } } },

        { "Random Circuit", "Arps", {
            { "A_mode", 1 }, { "A_wtTable", 1 }, { "A_unison", 1 }, { "A_cutoff", 2000 }, { "A_reso", 0.4f },
            { "A_aA", 0.001f }, { "A_aD", 0.2f }, { "A_aS", 0.3f }, { "A_aR", 0.15f },
            { "lfo1_shape", 5 }, { "lfo1_sync", 1 }, { "lfo1_div", 12 }, { "lfo1_retrig", 0 },
            { "m1_src", 1 }, { "m1_dst", 2 }, { "m1_amt", 0.35f }, { "m2_src", 1 }, { "m2_dst", 6 }, { "m2_amt", 0.5f },
            { "arpOn", 1 }, { "arpMode", 3 }, { "arpDiv", 12 }, { "arpOct", 2 }, { "arpGate", 0.6f },
            { "delayMix", 0.3f }, { "revMix", 0.25f } } },

        { "Chord Stabber", "Arps", {
            { "A_unison", 5 }, { "A_detune", 18 }, { "A_cutoff", 1500 }, { "A_fenv", 0.4f }, { "A_fD", 0.15f }, { "A_fS", 0 },
            { "A_aA", 0.001f }, { "A_aD", 0.2f }, { "A_aS", 0 }, { "A_aR", 0.2f },
            { "arpOn", 1 }, { "arpMode", 5 }, { "arpDiv", 8 }, { "arpGate", 0.3f },
            { "delayMix", 0.3f }, { "revMix", 0.3f } } },

        // ------------------------------------------------------------------ DRONES
        { "Dark Matter Drone", "Drones", {
            { "A_wave", 2.6f }, { "A_octave", -1 }, { "A_unison", 7 }, { "A_detune", 35 }, { "A_drift", 0.8f },
            { "A_sub", 0.3f }, { "A_noise", 0.1f }, { "A_cutoff", 600 }, { "A_reso", 0.55f }, { "A_drive", 0.4f },
            { "A_aA", 3.0f }, { "A_aR", 5.0f },
            { "B_on", 1 }, { "B_mode", 3 }, { "B_src", 3 }, { "B_octave", -1 }, { "B_level", 0.4f }, { "B_grSize", 500 },
            { "B_aA", 4.0f }, { "B_aR", 6.0f },
            { "lfo1_rate", 0.07f }, { "m1_src", 1 }, { "m1_dst", 2 }, { "m1_amt", 0.3f },
            { "revSize", 0.98f }, { "revMix", 0.55f }, { "delayMix", 0.3f }, { "delayFb", 0.6f } } },

        { "Deep Space Station", "Drones", {
            { "A_mode", 3 }, { "A_src", 7 }, { "A_octave", -1 }, { "A_grSize", 700 }, { "A_grDensity", 10 }, { "A_grSpray", 1.0f },
            { "A_aA", 4.0f }, { "A_aR", 6.0f },
            { "B_on", 1 }, { "B_mode", 2 }, { "B_octave", -2 }, { "B_fmRatio", 1.5f }, { "B_fmIndex", 1.5f }, { "B_level", 0.4f },
            { "B_aA", 5.0f }, { "B_aR", 6.0f },
            { "C_on", 1 }, { "C_mode", 3 }, { "C_src", 6 }, { "C_octave", 1 }, { "C_level", 0.3f }, { "C_aA", 6.0f }, { "C_aR", 6.0f },
            { "lfo1_rate", 0.03f }, { "lfo1_shape", 6 }, { "m1_src", 1 }, { "m1_dst", 6 }, { "m1_amt", 0.6f },
            { "revSize", 1.0f }, { "revMix", 0.65f }, { "delayMix", 0.3f }, { "delayFb", 0.7f } } },

        { "Cathedral Metal", "Drones", {
            { "A_mode", 4 }, { "A_src", 3 }, { "A_unison", 3 }, { "A_detune", 8 }, { "A_aA", 2.0f }, { "A_aR", 5.0f },
            { "B_on", 1 }, { "B_ftype", 5 }, { "B_noise", 0.6f }, { "B_wave", 0 }, { "B_cutoff", 130 }, { "B_reso", 0.85f },
            { "B_level", 0.4f }, { "B_aA", 3.0f }, { "B_aR", 5.0f },
            { "lfo1_rate", 0.05f }, { "m1_src", 1 }, { "m1_dst", 2 }, { "m1_layer", 2 }, { "m1_amt", 0.2f },
            { "revSize", 1.0f }, { "revMix", 0.7f } } },

        // ------------------------------------------------------------------ TEXTURES
        { "Ocean Memory", "Textures", {
            { "A_mode", 3 }, { "A_src", 7 }, { "A_grSize", 300 }, { "A_grDensity", 30 }, { "A_grSpray", 0.7f },
            { "A_ftype", 2 }, { "A_cutoff", 1200 }, { "A_reso", 0.3f }, { "A_aA", 1.5f }, { "A_aR", 3.0f },
            { "B_on", 1 }, { "B_mode", 3 }, { "B_src", 2 }, { "B_grSize", 180 }, { "B_harm1", 7 }, { "B_level", 0.4f },
            { "B_aA", 2.0f }, { "B_aR", 3.0f },
            { "lfo1_rate", 0.1f }, { "m1_src", 1 }, { "m1_dst", 6 }, { "m1_amt", 0.3f },
            { "revSize", 0.95f }, { "revMix", 0.6f } } },

        { "Vinyl Ghosts", "Textures", {
            { "A_mode", 3 }, { "A_src", 2 }, { "A_grSize", 450 }, { "A_grDensity", 8 }, { "A_grSpray", 0.6f }, { "A_grPitch", 0.1f },
            { "A_aA", 1.0f }, { "A_aR", 2.5f },
            { "B_on", 1 }, { "B_mode", 4 }, { "B_src", 8 }, { "B_level", 0.35f }, { "B_unison", 1 }, { "B_aR", 2.0f },
            { "distOn", 1 }, { "distType", 3 }, { "distDrive", 0.3f }, { "distMix", 0.35f }, { "eqHigh", -5 },
            { "revMix", 0.45f } } },

        { "Granular Rain", "Textures", {
            { "A_mode", 3 }, { "A_src", 6 }, { "A_grSize", 40 }, { "A_grDensity", 60 }, { "A_grSpray", 1.0f },
            { "A_grPitch", 0.5f }, { "A_aA", 0.5f }, { "A_aR", 2.0f },
            { "lfo1_rate", 0.25f }, { "m1_src", 1 }, { "m1_dst", 12 }, { "m1_amt", 0.4f },
            { "delayMix", 0.3f }, { "revMix", 0.5f } } },

        { "Broken Radio", "Textures", {
            { "A_mode", 1 }, { "A_wtTable", 8 }, { "A_unison", 1 }, { "A_ftype", 2 }, { "A_cutoff", 1500 }, { "A_reso", 0.6f },
            { "A_noise", 0.3f }, { "A_aA", 0.2f }, { "A_aR", 1.5f },
            { "lfo1_shape", 5 }, { "lfo1_rate", 6 }, { "m1_src", 1 }, { "m1_dst", 2 }, { "m1_amt", 0.3f },
            { "lfo2_shape", 6 }, { "lfo2_rate", 0.3f }, { "m2_src", 2 }, { "m2_dst", 6 }, { "m2_amt", 0.5f },
            { "distOn", 1 }, { "distType", 3 }, { "distDrive", 0.6f }, { "eqLow", -8 }, { "revMix", 0.35f } } },

        // ------------------------------------------------------------------ CINEMATIC (4 layers)
        { "Four Worlds", "Cinematic", {
            { "A_unison", 7 }, { "A_detune", 20 }, { "A_cutoff", 1500 }, { "A_aA", 1.5f }, { "A_aR", 3 },
            { "B_on", 1 }, { "B_mode", 1 }, { "B_wtTable", 2 }, { "B_octave", 1 }, { "B_level", 0.45f }, { "B_aA", 2 }, { "B_aR", 3 },
            { "C_on", 1 }, { "C_mode", 3 }, { "C_src", 1 }, { "C_level", 0.45f }, { "C_aA", 2.5f }, { "C_aR", 3 },
            { "D_on", 1 }, { "D_mode", 2 }, { "D_octave", -1 }, { "D_fmRatio", 2 }, { "D_fmIndex", 1 }, { "D_level", 0.5f },
            { "D_aA", 1 }, { "D_aR", 3 },
            { "lfo1_rate", 0.06f }, { "m1_src", 1 }, { "m1_dst", 6 }, { "m1_layer", 2 }, { "m1_amt", 0.5f },
            { "lfo2_rate", 0.11f }, { "m2_src", 2 }, { "m2_dst", 2 }, { "m2_layer", 1 }, { "m2_amt", 0.3f },
            { "revSize", 0.95f }, { "revMix", 0.55f }, { "delayMix", 0.25f } } },

        { "Trailer Riser", "Cinematic", {
            { "A_unison", 7 }, { "A_detune", 40 }, { "A_noise", 0.4f }, { "A_cutoff", 200 }, { "A_aA", 0.5f }, { "A_aR", 2.0f },
            { "B_on", 1 }, { "B_mode", 3 }, { "B_src", 3 }, { "B_level", 0.5f }, { "B_grDensity", 50 },
            { "me1_A", 8.0f }, { "me1_D", 1.0f }, { "me1_S", 1.0f },
            { "m1_src", 5 }, { "m1_dst", 2 }, { "m1_amt", 0.9f }, { "m2_src", 5 }, { "m2_dst", 1 }, { "m2_amt", 0.6f },
            { "m3_src", 5 }, { "m3_dst", 12 }, { "m3_amt", 0.6f },
            { "distOn", 1 }, { "distDrive", 0.2f }, { "revSize", 0.95f }, { "revMix", 0.5f } } },

        { "Brass Stack", "Cinematic", {
            { "A_unison", 3 }, { "A_detune", 9 }, { "A_cutoff", 500 }, { "A_reso", 0.15f }, { "A_fenv", 0.6f },
            { "A_fA", 0.08f }, { "A_fD", 0.6f }, { "A_fS", 0.5f }, { "A_aA", 0.05f }, { "A_aS", 0.9f }, { "A_aR", 0.25f }, { "A_drive", 0.3f },
            { "B_on", 1 }, { "B_semi", 7 }, { "B_level", 0.4f }, { "B_cutoff", 900 },
            { "C_on", 1 }, { "C_octave", -1 }, { "C_wave", 3 }, { "C_level", 0.4f }, { "C_cutoff", 700 },
            { "m1_src", 7 }, { "m1_dst", 2 }, { "m1_amt", 0.3f }, { "chorusMix", 0.25f }, { "revMix", 0.25f } } },

        { "Heartbeat Pulse", "Cinematic", {
            { "A_octave", -2 }, { "A_wave", 0 }, { "A_unison", 1 }, { "A_sub", 0.6f }, { "A_cutoff", 200 }, { "A_aS", 1 }, { "A_aR", 1 },
            { "B_on", 1 }, { "B_mode", 3 }, { "B_src", 7 }, { "B_level", 0.35f }, { "B_aA", 1 }, { "B_aR", 2 },
            { "lfo1_sync", 1 }, { "lfo1_div", 6 }, { "lfo1_shape", 3 }, { "lfo1_retrig", 0 },
            { "m1_src", 1 }, { "m1_dst", 4 }, { "m1_layer", 1 }, { "m1_amt", 0.9f },
            { "revMix", 0.4f } } },
    };
    return presets;
}
} // namespace neb
