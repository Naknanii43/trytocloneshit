// Factory presets. Each preset starts from the defaults (everything off except
// Dynamics and a flat EQ) and only lists what it changes. Values are in real units;
// choices are indices. The tuner instrument / reference are never touched by presets.
#pragma once
#include <vector>
#include <utility>

namespace rs
{
struct Preset
{
    const char* name;
    const char* category;
    const char* description;
    std::vector<std::pair<const char*, float>> values;
};

inline const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets {
        { "Init", "Basics", "Clean starting point: light compression, flat EQ, nothing else.", {} },

        { "Studio Polish", "Basics", "Modern, glossy and present: firm leveling, airy top, tight low end and a short bright plate.",
          { { "comp_amt", 0.55f }, { "comp_speed", 0.6f }, { "eq_lowcut", 110 }, { "eq_body", -1.0f },
            { "eq_mid", 2.0f }, { "eq_midf", 3200 }, { "eq_air", 5.0f },
            { "rev_on", 1 }, { "rev_size", 0.3f }, { "rev_decay", 0.9f }, { "rev_damp", 0.1f }, { "rev_mix", 0.24f }, { "rev_pre", 10 },
            { "out_width", 1.2f } } },

        { "Smoky Jazz Club", "Basics", "Dark, warm and close: rolled-off highs, fat body, tape warmth, a small dim room late at night.",
          { { "comp_amt", 0.3f }, { "comp_speed", 0.3f }, { "eq_lowcut", 55 }, { "eq_body", 4.5f }, { "eq_air", -7.0f },
            { "eq_mid", -3.0f }, { "eq_midf", 2400 },
            { "drv_on", 1 }, { "drv_type", 3 }, { "drv_drive", 0.3f }, { "drv_tone", 0.3f }, { "drv_mix", 0.8f }, { "drv_level", 3.0f },
            { "rev_on", 1 }, { "rev_size", 0.3f }, { "rev_decay", 1.1f }, { "rev_damp", 0.8f }, { "rev_mix", 0.28f }, { "rev_pre", 8 },
            { "out_width", 0.7f } } },

        { "Bright Pop Lead", "Basics", "Glossy 80s alto lead: squashed, razor-bright, wide chorus, dotted echo and a big hall.",
          { { "comp_amt", 0.8f }, { "comp_speed", 0.9f }, { "eq_lowcut", 190 }, { "eq_mid", 6.5f }, { "eq_midf", 2700 }, { "eq_air", 7.5f },
            { "drv_on", 1 }, { "drv_type", 0 }, { "drv_drive", 0.2f }, { "drv_tone", 0.9f }, { "drv_mix", 0.5f },
            { "mod_on", 1 }, { "mod_type", 0 }, { "mod_rate", 0.9f }, { "mod_depth", 0.55f }, { "mod_mix", 0.42f },
            { "dly_on", 1 }, { "dly_sync", 3 }, { "dly_fb", 0.3f }, { "dly_mix", 0.26f }, { "dly_duck", 0.4f }, { "dly_tone", 9000 },
            { "rev_on", 1 }, { "rev_size", 0.85f }, { "rev_decay", 3.2f }, { "rev_damp", 0.15f }, { "rev_mix", 0.3f }, { "out_width", 1.7f } } },

        { "Funk Envelope Wah", "Funk", "Quacky envelope filter that opens as you dig in. Play short and staccato.",
          { { "comp_amt", 0.5f }, { "comp_speed", 0.85f },
            { "wah_on", 1 }, { "wah_mode", 0 }, { "wah_sens", 0.62f }, { "wah_freq", 330 }, { "wah_range", 2.9f }, { "wah_q", 5.5f },
            { "drv_on", 1 }, { "drv_type", 0 }, { "drv_drive", 0.25f }, { "drv_mix", 0.7f }, { "drv_level", 4.0f },
            { "dly_on", 1 }, { "dly_sync", 6 }, { "dly_fb", 0.2f }, { "dly_mix", 0.14f } } },

        { "Robot Talk Wah", "Funk", "Tempo-free LFO wah with high resonance: a talking, vowel-like sweep.",
          { { "wah_on", 1 }, { "wah_mode", 2 }, { "wah_rate", 2.6f }, { "wah_freq", 300 }, { "wah_range", 2.6f }, { "wah_q", 8.0f },
            { "mod_on", 1 }, { "mod_type", 2 }, { "mod_rate", 0.3f }, { "mod_depth", 0.4f }, { "mod_mix", 0.3f },
            { "rev_on", 1 }, { "rev_size", 0.4f }, { "rev_mix", 0.15f } } },

        { "Octave Fuzz Monster", "Heavy", "A raspy, octave-up fuzz with one sub octave underneath: a buzzing, snarling monster.",
          { { "comp_amt", 0.5f }, { "oct_on", 1 }, { "oct_sub1", 0.7f }, { "oct_sub2", 0.0f }, { "oct_dry", 0.7f },
            { "drv_on", 1 }, { "drv_type", 1 }, { "drv_drive", 0.9f }, { "drv_char", 0.95f }, { "drv_tone", 0.6f }, { "drv_level", 2 },
            { "eq_lowcut", 60 }, { "eq_mid", 5 }, { "eq_midf", 1800 },
            { "rev_on", 1 }, { "rev_size", 0.2f }, { "rev_decay", 0.6f }, { "rev_mix", 0.1f } } },

        { "Blues Shouter Growl", "Heavy", "Throaty growl, nasal honk and a roadhouse slapback. No low end, all bark.",
          { { "comp_amt", 0.55f }, { "comp_speed", 0.8f }, { "drv_on", 1 }, { "drv_type", 2 }, { "drv_drive", 0.8f }, { "drv_char", 1.0f },
            { "drv_tone", 0.55f }, { "drv_level", -2.0f },
            { "eq_lowcut", 230 }, { "eq_mid", 7.5f }, { "eq_midf", 1250 }, { "eq_air", -1.5f },
            { "dly_on", 1 }, { "dly_sync", 0 }, { "dly_time", 95 }, { "dly_fb", 0.15f }, { "dly_mix", 0.35f }, { "dly_pp", 0 }, { "dly_tone", 2800 },
            { "out_width", 0.5f } } },

        { "Sub Bass Sax", "Heavy", "Your horn becomes a bass section: a fat tracking saw-and-sub synth an octave down plus a clean analog sub. Smooth, no distortion.",
          { { "comp_amt", 0.6f }, { "oct_on", 1 }, { "oct_sub1", 0.9f }, { "oct_sub2", 0.25f }, { "oct_dry", 0.45f },
            { "syn_on", 1 }, { "syn_wave", 2 }, { "syn_oct", 1 }, { "syn_cut", 420 }, { "syn_res", 0.25f }, { "syn_env", 0.6f }, { "syn_lvl", 0.7f },
            { "eq_lowcut", 25 }, { "eq_body", 4 }, { "eq_air", -5 }, { "out_width", 0.6f } } },

        { "Synth Horn Lead", "Synth", "A big, soft saw lead that follows your pitch and breath, with slow glide, wide chorus and a long hall.",
          { { "comp_amt", 0.45f }, { "syn_on", 1 }, { "syn_wave", 0 }, { "syn_oct", 2 }, { "syn_cut", 2800 }, { "syn_res", 0.15f },
            { "syn_env", 0.4f }, { "syn_glide", 70 }, { "syn_lvl", 0.85f },
            { "mod_on", 1 }, { "mod_type", 0 }, { "mod_rate", 0.5f }, { "mod_depth", 0.65f }, { "mod_mix", 0.5f },
            { "dly_on", 1 }, { "dly_sync", 2 }, { "dly_fb", 0.3f }, { "dly_mix", 0.18f },
            { "rev_on", 1 }, { "rev_size", 0.9f }, { "rev_decay", 3.5f }, { "rev_mix", 0.32f }, { "out_width", 1.6f } } },

        { "Square Wave Sax", "Synth", "8-bit horn: a squelchy square an octave up takes over from your sax. Thin, dry and bleepy.",
          { { "oct_on", 1 }, { "oct_sub1", 0.0f }, { "oct_dry", 0.15f },
            { "syn_on", 1 }, { "syn_wave", 1 }, { "syn_oct", 3 }, { "syn_cut", 2600 }, { "syn_res", 0.45f }, { "syn_env", 0.5f },
            { "syn_glide", 0 }, { "syn_lvl", 1.0f }, { "eq_lowcut", 320 }, { "comp_amt", 0.6f },
            { "drv_on", 1 }, { "drv_type", 3 }, { "drv_drive", 0.25f }, { "drv_tone", 0.95f }, { "drv_level", 12 },
            { "dly_on", 1 }, { "dly_sync", 7 }, { "dly_fb", 0.45f }, { "dly_mix", 0.3f }, { "dly_tone", 6000 }, { "out_width", 0.5f } } },

        { "Horn Section", "Harmony", "Instant three-part section: a diatonic third above and a fourth below, spread wide.",
          { { "comp_amt", 0.4f }, { "har_on", 1 }, { "har_mode", 0 }, { "har_key", 0 }, { "har_scale", 0 },
            { "har_int1", 2 }, { "har_int2", -3 }, { "har_lvl1", 0.6f }, { "har_lvl2", 0.5f }, { "har_spread", 0.75f },
            { "eq_air", 1.5f } } },

        { "Octave Unison", "Harmony", "Clean big-band unison: octave below on the left, octave above on the right, your horn in the middle, in a wooden hall.",
          { { "comp_amt", 0.45f }, { "har_on", 1 }, { "har_mode", 1 }, { "har_int1", -12 }, { "har_int2", 12 }, { "har_lvl1", 0.8f }, { "har_lvl2", 0.5f },
            { "har_spread", 1.0f }, { "eq_body", 2 },
            { "rev_on", 1 }, { "rev_size", 0.6f }, { "rev_decay", 1.6f }, { "rev_damp", 0.55f }, { "rev_mix", 0.22f }, { "out_width", 1.5f } } },

        { "Ethio Minor Section", "Harmony", "Harmonic-minor thirds over a low octave, swirling through a deep slow phaser with a triplet echo. Moody, hypnotic.",
          { { "har_on", 1 }, { "har_mode", 0 }, { "har_key", 9 }, { "har_scale", 4 }, { "har_int1", 2 }, { "har_int2", -7 },
            { "har_lvl1", 0.65f }, { "har_lvl2", 0.55f }, { "har_spread", 0.85f },
            { "eq_air", -4 }, { "eq_body", 2 },
            { "mod_on", 1 }, { "mod_type", 2 }, { "mod_rate", 0.15f }, { "mod_depth", 0.9f }, { "mod_fb", 0.8f }, { "mod_mix", 0.9f },
            { "dly_on", 1 }, { "dly_sync", 7 }, { "dly_fb", 0.35f }, { "dly_mix", 0.25f } } },

        { "Ambient Shimmer", "Space", "Angelic: ghostly fifths and octaves float above you into an endless shimmer cathedral, extra wide. Play long tones.",
          { { "comp_amt", 0.35f }, { "eq_lowcut", 160 }, { "eq_air", 4 },
            { "har_on", 1 }, { "har_mode", 1 }, { "har_int1", 7 }, { "har_int2", 12 }, { "har_lvl1", 0.3f }, { "har_lvl2", 0.25f }, { "har_spread", 1.0f },
            { "mod_on", 1 }, { "mod_type", 0 }, { "mod_rate", 0.2f }, { "mod_depth", 0.6f }, { "mod_mix", 0.35f },
            { "rev_on", 1 }, { "rev_size", 1.0f }, { "rev_decay", 14.0f }, { "rev_damp", 0.1f }, { "rev_shimmer", 0.75f }, { "rev_mix", 0.9f }, { "rev_pre", 60 },
            { "out_width", 1.9f } } },

        { "Dub Echo Chamber", "Space", "Telephone-band sax thrown into a wide ping-pong tape echo that keeps repeating and wobbling. All echo.",
          { { "eq_lowcut", 420 }, { "eq_mid", 7 }, { "eq_midf", 1100 }, { "eq_air", -8 },
            { "dly_on", 1 }, { "dly_sync", 6 }, { "dly_fb", 0.85f }, { "dly_tone", 1300 }, { "dly_wobble", 0.5f }, { "dly_duck", 0.1f }, { "dly_mix", 0.75f },
            { "out_width", 1.0f } } },

        { "Jet Flanger Space", "Space", "Jet engine: an octave-up whine and a crunchy edge sucked through a screaming, deep flanger. All whoosh.",
          { { "eq_air", 3 }, { "eq_lowcut", 120 },
            { "har_on", 1 }, { "har_mode", 1 }, { "har_int1", 12 }, { "har_lvl1", 0.45f }, { "har_lvl2", 0 }, { "har_spread", 0.0f },
            { "drv_on", 1 }, { "drv_type", 0 }, { "drv_drive", 0.45f }, { "drv_tone", 0.75f }, { "drv_level", -6 },
            { "mod_on", 1 }, { "mod_type", 1 }, { "mod_rate", 0.22f }, { "mod_depth", 1.0f }, { "mod_fb", 0.9f }, { "mod_mix", 1.0f },
            { "out_width", 1.4f } } },

        { "Zone Morph", "Morph", "Your sax mostly BECOMES the synth: drone down low, pad in the middle, bells up top, choir in altissimo.",
          { { "comp_amt", 0.4f }, { "gen_on", 1 }, { "gen_morph", 0.82f }, { "gen_snd1", 3 }, { "gen_snd2", 1 }, { "gen_snd3", 0 }, { "gen_snd4", 4 },
            { "gen_bloom", 0.25f }, { "gen_pattern", 2 }, { "gen_rate", 1 }, { "gen_length", 5 }, { "gen_density", 0.5f },
            { "rev_on", 1 }, { "rev_size", 0.7f }, { "rev_decay", 3.0f }, { "rev_mix", 0.3f } } },

        { "Bloom Bells", "Morph", "Pure sax up front; every held note sets off a fast, glittering 3-octave bell arpeggio in key.",
          { { "comp_amt", 0.4f }, { "eq_air", 3 }, { "gen_on", 1 }, { "gen_morph", 0.0f }, { "gen_snd1", 0 }, { "gen_snd2", 0 }, { "gen_snd3", 0 }, { "gen_snd4", 0 },
            { "gen_bloom", 0.6f }, { "gen_pattern", 0 }, { "gen_rate", 4 }, { "gen_length", 3.0f }, { "gen_density", 0.95f }, { "gen_range", 3 }, { "gen_hold", 90 },
            { "dly_on", 1 }, { "dly_sync", 6 }, { "dly_fb", 0.5f }, { "dly_mix", 0.32f }, { "dly_duck", 0.7f }, { "dly_tone", 9000 } } },

        { "Choir Ascension", "Morph", "The higher you play, the more your sax turns into a choir, with swelling choir chords and an octave-up halo. Dry-ish and close.",
          { { "eq_lowcut", 200 }, { "eq_air", 3 }, { "gen_on", 1 }, { "gen_morph", 0.5f }, { "gen_pmorph", 0.9f },
            { "gen_snd1", 4 }, { "gen_snd2", 4 }, { "gen_snd3", 4 }, { "gen_snd4", 4 },
            { "gen_bloom", 0.7f }, { "gen_pattern", 2 }, { "gen_rate", 0 }, { "gen_length", 6 }, { "gen_density", 0.6f }, { "gen_hold", 200 },
            { "har_on", 1 }, { "har_mode", 1 }, { "har_int1", 12 }, { "har_lvl1", 0.35f }, { "har_lvl2", 0.0f }, { "har_spread", 0.3f },
            { "rev_on", 1 }, { "rev_size", 0.35f }, { "rev_decay", 1.2f }, { "rev_damp", 0.3f }, { "rev_mix", 0.2f } } },

        { "Swarm Machine", "Morph", "Aggressive: sax turns into a pulsing synth swarm in the middle register, with random-walk sequences and tape grit.",
          { { "comp_amt", 0.5f }, { "gen_on", 1 }, { "gen_morph", 0.75f }, { "gen_snd1", 3 }, { "gen_snd2", 5 }, { "gen_snd3", 5 }, { "gen_snd4", 0 },
            { "gen_bloom", 0.6f }, { "gen_pattern", 1 }, { "gen_rate", 3 }, { "gen_length", 4 }, { "gen_density", 0.8f },
            { "drv_on", 1 }, { "drv_type", 3 }, { "drv_drive", 0.35f },
            { "dly_on", 1 }, { "dly_sync", 5 }, { "dly_fb", 0.35f }, { "dly_mix", 0.2f } } },

        { "Generative Ambient", "Morph", "Dark and slow: bowed strings and drones wander around your notes in long random walks. No shimmer, just dusk.",
          { { "eq_lowcut", 80 }, { "eq_air", -5 }, { "gen_on", 1 }, { "gen_morph", 0.1f }, { "gen_snd1", 3 }, { "gen_snd2", 2 }, { "gen_snd3", 2 }, { "gen_snd4", 3 },
            { "gen_bloom", 0.8f }, { "gen_pattern", 1 }, { "gen_rate", 2 }, { "gen_length", 12 }, { "gen_density", 0.45f }, { "gen_range", 2 }, { "gen_hold", 300 },
            { "dly_on", 1 }, { "dly_sync", 1 }, { "dly_fb", 0.68f }, { "dly_mix", 0.42f }, { "dly_tone", 1500 }, { "dly_wobble", 0.55f },
            { "rev_on", 1 }, { "rev_size", 0.6f }, { "rev_decay", 2.5f }, { "rev_damp", 0.85f }, { "rev_mix", 0.22f }, { "out_width", 0.8f } } },

        { "Gremlin Party", "Gremlins", "Every creature at once: a bouncing, chorused zoo of boings, lasers, quacks, robots and the odd clank, echoing around the room.",
          { { "comp_amt", 0.35f }, { "grm_on", 1 }, { "grm_palette", 0 }, { "grm_harsh", 0.2f }, { "grm_chance", 1.0f }, { "grm_swarm", 0.8f },
            { "grm_follow", 0.4f }, { "grm_dirt", 0.0f }, { "grm_level", 0.6f }, { "eq_air", 3 },
            { "mod_on", 1 }, { "mod_type", 0 }, { "mod_rate", 1.2f }, { "mod_depth", 0.7f }, { "mod_mix", 0.45f },
            { "dly_on", 1 }, { "dly_sync", 5 }, { "dly_fb", 0.45f }, { "dly_mix", 0.35f },
            { "rev_on", 1 }, { "rev_size", 0.5f }, { "rev_decay", 1.6f }, { "rev_mix", 0.25f }, { "out_width", 1.8f } } },

        { "Industrial Factory", "Gremlins", "Scrapyard: clanks, grinders, static and stutters, crushed to bits, behind a fuzzed sax in a metal hall.",
          { { "comp_amt", 0.55f }, { "grm_on", 1 }, { "grm_palette", 4 }, { "grm_harsh", 1.0f }, { "grm_chance", 0.95f }, { "grm_swarm", 0.65f },
            { "grm_follow", 0.3f }, { "grm_dirt", 0.8f }, { "grm_level", 0.8f },
            { "drv_on", 1 }, { "drv_type", 1 }, { "drv_drive", 0.6f }, { "drv_char", 0.2f }, { "drv_tone", 0.35f },
            { "eq_lowcut", 60 }, { "eq_mid", 4 }, { "eq_midf", 700 }, { "eq_air", -3 },
            { "rev_on", 1 }, { "rev_size", 0.8f }, { "rev_decay", 3.0f }, { "rev_damp", 0.0f }, { "rev_mix", 0.32f } } },

        { "Cartoon Sax", "Gremlins", "Saturday-morning cartoon: slide whistles, boings, bubbles and quacks pop out on YOUR notes, loud and proud.",
          { { "comp_amt", 0.4f }, { "grm_on", 1 }, { "grm_palette", 1 }, { "grm_harsh", 0.0f }, { "grm_chance", 1.0f }, { "grm_swarm", 0.55f },
            { "grm_follow", 1.0f }, { "grm_dirt", 0.0f }, { "grm_level", 0.7f }, { "eq_air", 2 }, { "eq_body", -2 },
            { "dly_on", 1 }, { "dly_sync", 5 }, { "dly_fb", 0.2f }, { "dly_mix", 0.15f } } },

        { "Broken Robot", "Gremlins", "Sci-fi malfunction: your sax gets a fast robotic resonant wobble while lasers, beeps and screeches answer.",
          { { "grm_on", 1 }, { "grm_palette", 2 }, { "grm_harsh", 0.5f }, { "grm_chance", 0.85f }, { "grm_swarm", 0.45f }, { "grm_dirt", 0.5f },
            { "grm_follow", 0.6f }, { "grm_level", 0.7f },
            { "wah_on", 1 }, { "wah_mode", 2 }, { "wah_rate", 7.5f }, { "wah_freq", 500 }, { "wah_range", 2.2f }, { "wah_q", 11.0f }, { "wah_mix", 0.85f },
            { "mod_on", 1 }, { "mod_type", 1 }, { "mod_rate", 0.3f }, { "mod_depth", 0.6f }, { "mod_fb", 0.7f }, { "mod_mix", 0.4f },
            { "dly_on", 1 }, { "dly_sync", 8 }, { "dly_fb", 0.4f }, { "dly_mix", 0.2f } } },

        { "Whammy Dive Bomb", "Mangle", "Every phrase slowly dives an octave down and climbs back up, through crunchy fuzz. Metal sax.",
          { { "comp_amt", 0.5f }, { "whm_on", 1 }, { "whm_mode", 1 }, { "whm_amount", -12 }, { "whm_rate", 0.22f }, { "whm_glide", 30 },
            { "drv_on", 1 }, { "drv_type", 1 }, { "drv_drive", 0.55f }, { "drv_char", 0.2f }, { "drv_tone", 0.55f },
            { "eq_lowcut", 70 }, { "rev_on", 1 }, { "rev_size", 0.35f }, { "rev_decay", 1.0f }, { "rev_mix", 0.15f } } },

        { "Jazz Scoops", "Mangle", "Every note slides up into pitch from a minor third below, like an exaggerated bebop scoop. Warm and close.",
          { { "comp_amt", 0.35f }, { "whm_on", 1 }, { "whm_mode", 2 }, { "whm_amount", -3 }, { "whm_glide", 140 },
            { "eq_body", 3 }, { "eq_air", -3 }, { "rev_on", 1 }, { "rev_size", 0.3f }, { "rev_decay", 0.9f }, { "rev_damp", 0.6f }, { "rev_mix", 0.2f } } },

        { "Ring Mod Robot", "Mangle", "A fixed low carrier turns your sax into a metallic, monotone robot voice, with a short slapback.",
          { { "comp_amt", 0.5f }, { "rng_on", 1 }, { "rng_mode", 1 }, { "rng_freq", 115 }, { "rng_wave", 0 }, { "rng_mix", 0.95f },
            { "eq_lowcut", 140 }, { "eq_mid", 4 }, { "eq_midf", 1800 },
            { "dly_on", 1 }, { "dly_sync", 0 }, { "dly_time", 70 }, { "dly_fb", 0.2f }, { "dly_mix", 0.25f }, { "dly_pp", 0 } } },

        { "Bell Ring", "Mangle", "A pitch-tracked ring modulator a fifth above your note: every phrase rings like struck bells in a big hall.",
          { { "rng_on", 1 }, { "rng_mode", 0 }, { "rng_ratio", 1.5f }, { "rng_wave", 0 }, { "rng_sweep", 0.05f }, { "rng_rate", 0.2f }, { "rng_mix", 0.6f },
            { "eq_air", 2 }, { "rev_on", 1 }, { "rev_size", 0.9f }, { "rev_decay", 4.0f }, { "rev_damp", 0.1f }, { "rev_mix", 0.35f } } },

        { "Freeze & Solo", "Mangle", "Auto Freeze: hold a note and it becomes a pad under you. Each new held note stacks on top, then slowly fades. Solo over yourself.",
          { { "comp_amt", 0.4f }, { "frz_on", 1 }, { "frz_mode", 1 }, { "frz_decay", 0.35f }, { "frz_attack", 600 }, { "frz_width", 1.0f }, { "frz_level", 0.9f },
            { "rev_on", 1 }, { "rev_size", 0.8f }, { "rev_decay", 3.5f }, { "rev_mix", 0.3f } } },

        { "Trance Gate Sax", "Mangle", "Long notes get chopped into a 16th-note trance pattern, with dotted echoes and a wide hall. Hold notes and let it pump.",
          { { "comp_amt", 0.5f }, { "gat_on", 1 }, { "gat_pattern", 0 }, { "gat_rate", 1 }, { "gat_depth", 1.0f }, { "gat_smooth", 3 },
            { "dly_on", 1 }, { "dly_sync", 6 }, { "dly_fb", 0.4f }, { "dly_mix", 0.3f },
            { "rev_on", 1 }, { "rev_size", 0.8f }, { "rev_decay", 2.5f }, { "rev_mix", 0.28f }, { "out_width", 1.5f } } },

        { "Rotary Club", "Vintage", "A fast-spinning rotary speaker and warm tube grit, wide and organ-like. Organ-trio soul.",
          { { "comp_amt", 0.35f }, { "drv_on", 1 }, { "drv_type", 0 }, { "drv_drive", 0.35f }, { "drv_tone", 0.5f }, { "drv_level", -4 },
            { "eq_body", 2 },
            { "mod_on", 1 }, { "mod_type", 3 }, { "mod_rate", 6.5f }, { "mod_depth", 0.95f }, { "mod_mix", 1.0f },
            { "rev_on", 1 }, { "rev_size", 0.4f }, { "rev_decay", 1.2f }, { "rev_mix", 0.18f }, { "out_width", 1.5f } } },

        { "Lo-Fi Tape Memory", "Vintage", "A worn cassette: dull top end, warm saturated body, seasick wow and flutter, almost mono. No echo.",
          { { "comp_amt", 0.65f }, { "drv_on", 1 }, { "drv_type", 3 }, { "drv_drive", 0.65f }, { "drv_tone", 0.15f }, { "drv_level", 3.5f },
            { "eq_lowcut", 90 }, { "eq_air", -12 }, { "eq_body", 4 },
            { "mod_on", 1 }, { "mod_type", 0 }, { "mod_rate", 0.35f }, { "mod_depth", 1.0f }, { "mod_mix", 1.0f }, { "mod_fb", 0 },
            { "out_width", 0.15f } } },
    };
    return presets;
}
} // namespace rs
