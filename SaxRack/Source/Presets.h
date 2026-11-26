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

        { "Studio Polish", "Basics", "Gentle leveling, a little air and a small plate. Ready for any mix.",
          { { "comp_amt", 0.4f }, { "comp_speed", 0.55f }, { "eq_lowcut", 85 }, { "eq_body", 1.0f },
            { "eq_mid", -1.5f }, { "eq_midf", 650 }, { "eq_air", 2.5f },
            { "rev_on", 1 }, { "rev_size", 0.35f }, { "rev_decay", 1.3f }, { "rev_damp", 0.35f }, { "rev_mix", 0.18f }, { "rev_pre", 12 } } },

        { "Smoky Jazz Club", "Basics", "Warm, dark and close: rounded highs, fat body, a small room late at night.",
          { { "comp_amt", 0.3f }, { "comp_speed", 0.35f }, { "eq_lowcut", 60 }, { "eq_body", 2.5f }, { "eq_air", -3.0f },
            { "eq_mid", -2.0f }, { "eq_midf", 2200 },
            { "drv_on", 1 }, { "drv_type", 3 }, { "drv_drive", 0.15f }, { "drv_tone", 0.45f }, { "drv_mix", 0.6f }, { "drv_level", 2.5f },
            { "rev_on", 1 }, { "rev_size", 0.45f }, { "rev_decay", 1.7f }, { "rev_damp", 0.6f }, { "rev_mix", 0.24f }, { "rev_pre", 18 } } },

        { "Bright Pop Lead", "Basics", "Cutting, glossy 80s-style alto lead: hard compression, upper-mid bite, chorus shine and an echo.",
          { { "comp_amt", 0.6f }, { "comp_speed", 0.8f }, { "eq_lowcut", 130 }, { "eq_mid", 4.0f }, { "eq_midf", 1900 }, { "eq_air", 4.0f },
            { "mod_on", 1 }, { "mod_type", 0 }, { "mod_rate", 0.6f }, { "mod_depth", 0.3f }, { "mod_mix", 0.22f },
            { "dly_on", 1 }, { "dly_sync", 2 }, { "dly_fb", 0.25f }, { "dly_mix", 0.16f }, { "dly_duck", 0.5f },
            { "rev_on", 1 }, { "rev_size", 0.7f }, { "rev_decay", 2.4f }, { "rev_mix", 0.22f } } },

        { "Funk Envelope Wah", "Funk", "Quacky envelope filter that opens as you dig in. Play short and staccato.",
          { { "comp_amt", 0.5f }, { "comp_speed", 0.85f },
            { "wah_on", 1 }, { "wah_mode", 0 }, { "wah_sens", 0.62f }, { "wah_freq", 330 }, { "wah_range", 2.9f }, { "wah_q", 5.5f },
            { "drv_on", 1 }, { "drv_type", 0 }, { "drv_drive", 0.25f }, { "drv_mix", 0.7f }, { "drv_level", 4.0f },
            { "dly_on", 1 }, { "dly_sync", 6 }, { "dly_fb", 0.2f }, { "dly_mix", 0.14f } } },

        { "Robot Talk Wah", "Funk", "Tempo-free LFO wah with high resonance: a talking, vowel-like sweep.",
          { { "wah_on", 1 }, { "wah_mode", 2 }, { "wah_rate", 2.6f }, { "wah_freq", 300 }, { "wah_range", 2.6f }, { "wah_q", 8.0f },
            { "mod_on", 1 }, { "mod_type", 2 }, { "mod_rate", 0.3f }, { "mod_depth", 0.4f }, { "mod_mix", 0.3f },
            { "rev_on", 1 }, { "rev_size", 0.4f }, { "rev_mix", 0.15f } } },

        { "Octave Fuzz Monster", "Heavy", "One octave down, a hint of two, into a fuzz with octave-up grit. Huge.",
          { { "comp_amt", 0.45f }, { "oct_on", 1 }, { "oct_sub1", 0.85f }, { "oct_sub2", 0.3f }, { "oct_dry", 0.8f },
            { "drv_on", 1 }, { "drv_type", 1 }, { "drv_drive", 0.7f }, { "drv_char", 0.35f }, { "drv_tone", 0.5f }, { "drv_level", 2 },
            { "eq_lowcut", 35 }, { "eq_mid", 3.5f }, { "eq_midf", 900 },
            { "rev_on", 1 }, { "rev_size", 0.3f }, { "rev_decay", 0.9f }, { "rev_mix", 0.15f } } },

        { "Blues Shouter Growl", "Heavy", "Throaty growl and tube crunch for honking R&B and roadhouse blues.",
          { { "comp_amt", 0.5f }, { "drv_on", 1 }, { "drv_type", 2 }, { "drv_drive", 0.45f }, { "drv_char", 0.55f }, { "drv_tone", 0.5f }, { "drv_level", -2.0f },
            { "eq_lowcut", 90 }, { "eq_mid", 3.0f }, { "eq_midf", 1100 },
            { "dly_on", 1 }, { "dly_sync", 0 }, { "dly_time", 110 }, { "dly_fb", 0.18f }, { "dly_mix", 0.2f }, { "dly_pp", 0 }, { "dly_tone", 2500 },
            { "rev_on", 1 }, { "rev_size", 0.35f }, { "rev_decay", 1.1f }, { "rev_mix", 0.18f } } },

        { "Sub Bass Sax", "Heavy", "Tracking synth one octave down plus a clean sub: tenor turns into a bass section.",
          { { "comp_amt", 0.5f }, { "oct_on", 1 }, { "oct_sub1", 0.7f }, { "oct_dry", 0.7f },
            { "syn_on", 1 }, { "syn_wave", 2 }, { "syn_oct", 1 }, { "syn_cut", 600 }, { "syn_res", 0.2f }, { "syn_env", 0.5f }, { "syn_lvl", 0.55f },
            { "eq_lowcut", 30 }, { "eq_body", 3 } } },

        { "Synth Horn Lead", "Synth", "A saw lead that follows your pitch and breath, layered with your tone.",
          { { "comp_amt", 0.45f }, { "syn_on", 1 }, { "syn_wave", 0 }, { "syn_oct", 2 }, { "syn_cut", 1500 }, { "syn_res", 0.45f },
            { "syn_env", 0.65f }, { "syn_glide", 35 }, { "syn_lvl", 0.6f },
            { "mod_on", 1 }, { "mod_type", 0 }, { "mod_rate", 0.7f }, { "mod_depth", 0.45f }, { "mod_mix", 0.35f },
            { "dly_on", 1 }, { "dly_sync", 5 }, { "dly_fb", 0.3f }, { "dly_mix", 0.18f },
            { "rev_on", 1 }, { "rev_size", 0.6f }, { "rev_decay", 2.2f }, { "rev_mix", 0.22f } } },

        { "Square Wave Sax", "Synth", "Hollow square-wave voice an octave up, filtered by your breath. Chiptune horn.",
          { { "syn_on", 1 }, { "syn_wave", 1 }, { "syn_oct", 3 }, { "syn_cut", 900 }, { "syn_res", 0.3f }, { "syn_env", 0.8f },
            { "syn_glide", 10 }, { "syn_lvl", 0.5f },
            { "dly_on", 1 }, { "dly_sync", 7 }, { "dly_fb", 0.4f }, { "dly_mix", 0.2f } } },

        { "Horn Section", "Harmony", "Instant three-part section: a diatonic third above and a fourth below, spread wide.",
          { { "comp_amt", 0.4f }, { "har_on", 1 }, { "har_mode", 0 }, { "har_key", 0 }, { "har_scale", 0 },
            { "har_int1", 2 }, { "har_int2", -3 }, { "har_lvl1", 0.6f }, { "har_lvl2", 0.5f }, { "har_spread", 0.75f },
            { "eq_air", 1.5f }, { "rev_on", 1 }, { "rev_size", 0.55f }, { "rev_decay", 1.8f }, { "rev_mix", 0.22f } } },

        { "Octave Unison", "Harmony", "Clean octave below and octave above. Think big-band unison lines.",
          { { "har_on", 1 }, { "har_mode", 1 }, { "har_int1", -12 }, { "har_int2", 12 }, { "har_lvl1", 0.55f }, { "har_lvl2", 0.3f },
            { "har_spread", 0.4f }, { "rev_on", 1 }, { "rev_size", 0.5f }, { "rev_mix", 0.2f } } },

        { "Ethio Minor Section", "Harmony", "Harmonic-minor thirds over a low octave, with a slap echo. Moody and hypnotic.",
          { { "har_on", 1 }, { "har_mode", 0 }, { "har_key", 9 }, { "har_scale", 4 }, { "har_int1", 2 }, { "har_int2", -7 },
            { "har_lvl1", 0.55f }, { "har_lvl2", 0.35f }, { "har_spread", 0.6f },
            { "drv_on", 1 }, { "drv_type", 3 }, { "drv_drive", 0.25f }, { "drv_mix", 0.7f },
            { "dly_on", 1 }, { "dly_sync", 5 }, { "dly_fb", 0.25f }, { "dly_mix", 0.18f },
            { "rev_on", 1 }, { "rev_size", 0.5f }, { "rev_decay", 1.6f }, { "rev_mix", 0.2f } } },

        { "Ambient Shimmer", "Space", "Endless octave-up shimmer hall with a ducked dotted echo. Play long tones.",
          { { "comp_amt", 0.35f }, { "eq_lowcut", 120 },
            { "mod_on", 1 }, { "mod_type", 0 }, { "mod_rate", 0.25f }, { "mod_depth", 0.6f }, { "mod_mix", 0.35f },
            { "dly_on", 1 }, { "dly_sync", 3 }, { "dly_fb", 0.55f }, { "dly_mix", 0.35f }, { "dly_duck", 0.6f }, { "dly_tone", 3000 }, { "dly_wobble", 0.25f },
            { "rev_on", 1 }, { "rev_size", 1.0f }, { "rev_decay", 9.0f }, { "rev_damp", 0.3f }, { "rev_shimmer", 0.55f }, { "rev_mix", 0.65f }, { "rev_pre", 40 } } },

        { "Dub Echo Chamber", "Space", "Dark, wobbly ping-pong tape echo that keeps on repeating. Leave space.",
          { { "eq_lowcut", 150 }, { "eq_mid", 2 }, { "eq_midf", 1000 },
            { "dly_on", 1 }, { "dly_sync", 6 }, { "dly_fb", 0.72f }, { "dly_tone", 1800 }, { "dly_wobble", 0.45f }, { "dly_duck", 0.25f }, { "dly_mix", 0.5f },
            { "rev_on", 1 }, { "rev_size", 0.6f }, { "rev_decay", 2.0f }, { "rev_damp", 0.6f }, { "rev_mix", 0.2f } } },

        { "Jet Flanger Space", "Space", "Slow, deep jet flange into a long echo and a big hall.",
          { { "mod_on", 1 }, { "mod_type", 1 }, { "mod_rate", 0.12f }, { "mod_depth", 0.85f }, { "mod_fb", 0.7f }, { "mod_mix", 0.6f },
            { "dly_on", 1 }, { "dly_sync", 2 }, { "dly_fb", 0.45f }, { "dly_mix", 0.25f },
            { "rev_on", 1 }, { "rev_size", 0.85f }, { "rev_decay", 4.5f }, { "rev_mix", 0.35f } } },

        { "Zone Morph", "Morph", "Low notes rumble as a drone, the middle is a pad, the top rings like bells, altissimo sings as a choir. Held notes bloom chord swells.",
          { { "comp_amt", 0.4f }, { "gen_on", 1 }, { "gen_morph", 0.55f }, { "gen_snd1", 3 }, { "gen_snd2", 1 }, { "gen_snd3", 0 }, { "gen_snd4", 4 },
            { "gen_bloom", 0.5f }, { "gen_pattern", 2 }, { "gen_rate", 1 }, { "gen_length", 5 }, { "gen_density", 0.6f },
            { "dly_on", 1 }, { "dly_sync", 3 }, { "dly_fb", 0.4f }, { "dly_mix", 0.22f }, { "dly_duck", 0.5f },
            { "rev_on", 1 }, { "rev_size", 0.85f }, { "rev_decay", 4.5f }, { "rev_mix", 0.35f } } },

        { "Bloom Bells", "Morph", "Your tone stays mostly sax; every held note sets off a glittering bell arpeggio in key.",
          { { "comp_amt", 0.4f }, { "gen_on", 1 }, { "gen_morph", 0.12f }, { "gen_snd1", 2 }, { "gen_snd2", 0 }, { "gen_snd3", 0 }, { "gen_snd4", 5 },
            { "gen_bloom", 0.6f }, { "gen_pattern", 0 }, { "gen_rate", 3 }, { "gen_length", 3.5f }, { "gen_density", 0.85f }, { "gen_range", 2 },
            { "dly_on", 1 }, { "dly_sync", 6 }, { "dly_fb", 0.45f }, { "dly_mix", 0.3f }, { "dly_duck", 0.6f },
            { "rev_on", 1 }, { "rev_size", 0.8f }, { "rev_decay", 3.5f }, { "rev_mix", 0.32f } } },

        { "Choir Ascension", "Morph", "The higher you play, the more the sax turns into a choir. Long notes bloom swelling choir chords into a shimmer hall.",
          { { "eq_lowcut", 100 }, { "gen_on", 1 }, { "gen_morph", 0.3f }, { "gen_pmorph", 0.8f }, { "gen_snd1", 1 }, { "gen_snd2", 1 }, { "gen_snd3", 4 }, { "gen_snd4", 4 },
            { "gen_bloom", 0.55f }, { "gen_pattern", 2 }, { "gen_rate", 0 }, { "gen_length", 8 }, { "gen_density", 0.5f }, { "gen_hold", 250 },
            { "rev_on", 1 }, { "rev_size", 1.0f }, { "rev_decay", 8.0f }, { "rev_shimmer", 0.4f }, { "rev_mix", 0.5f } } },

        { "Swarm Machine", "Morph", "Aggressive: sax turns into a pulsing synth swarm in the middle register, with random-walk sequences and tape grit.",
          { { "comp_amt", 0.5f }, { "gen_on", 1 }, { "gen_morph", 0.75f }, { "gen_snd1", 3 }, { "gen_snd2", 5 }, { "gen_snd3", 5 }, { "gen_snd4", 0 },
            { "gen_bloom", 0.6f }, { "gen_pattern", 1 }, { "gen_rate", 3 }, { "gen_length", 4 }, { "gen_density", 0.8f },
            { "drv_on", 1 }, { "drv_type", 3 }, { "drv_drive", 0.35f },
            { "dly_on", 1 }, { "dly_sync", 5 }, { "dly_fb", 0.35f }, { "dly_mix", 0.2f } } },

        { "Generative Ambient", "Morph", "Barely-there morph, long wandering blooms, ducked echoes and shimmer. Play a few notes and let it grow; loop it.",
          { { "eq_lowcut", 110 }, { "gen_on", 1 }, { "gen_morph", 0.2f }, { "gen_snd1", 3 }, { "gen_snd2", 1 }, { "gen_snd3", 2 }, { "gen_snd4", 0 },
            { "gen_bloom", 0.8f }, { "gen_pattern", 1 }, { "gen_rate", 1 }, { "gen_length", 10 }, { "gen_density", 0.55f }, { "gen_range", 3 }, { "gen_hold", 300 },
            { "mod_on", 1 }, { "mod_type", 0 }, { "mod_rate", 0.2f }, { "mod_depth", 0.5f }, { "mod_mix", 0.3f },
            { "dly_on", 1 }, { "dly_sync", 3 }, { "dly_fb", 0.6f }, { "dly_mix", 0.35f }, { "dly_duck", 0.7f }, { "dly_wobble", 0.3f },
            { "rev_on", 1 }, { "rev_size", 1.0f }, { "rev_decay", 10.0f }, { "rev_shimmer", 0.45f }, { "rev_mix", 0.55f } } },

        { "Gremlin Party", "Gremlins", "Every note, breath and key click wakes silly creatures: boings, quacks, bubbles, robots. Pure mischief.",
          { { "comp_amt", 0.35f }, { "grm_on", 1 }, { "grm_harsh", 0.12f }, { "grm_chance", 0.85f }, { "grm_swarm", 0.5f },
            { "grm_follow", 0.6f }, { "grm_dirt", 0.05f }, { "grm_level", 0.7f },
            { "dly_on", 1 }, { "dly_sync", 6 }, { "dly_fb", 0.3f }, { "dly_mix", 0.2f },
            { "rev_on", 1 }, { "rev_size", 0.5f }, { "rev_decay", 1.8f }, { "rev_mix", 0.22f } } },

        { "Industrial Factory", "Gremlins", "Harsh: every sound sets off clanks, grinders, stutters and screeches, crushed to bits, into a fuzzed sax.",
          { { "comp_amt", 0.55f }, { "grm_on", 1 }, { "grm_harsh", 0.95f }, { "grm_chance", 0.9f }, { "grm_swarm", 0.6f },
            { "grm_follow", 0.4f }, { "grm_dirt", 0.75f }, { "grm_level", 0.75f },
            { "drv_on", 1 }, { "drv_type", 1 }, { "drv_drive", 0.55f }, { "drv_char", 0.4f }, { "drv_level", 1.0f },
            { "eq_lowcut", 70 }, { "eq_mid", 3 }, { "eq_midf", 1500 },
            { "rev_on", 1 }, { "rev_size", 0.75f }, { "rev_decay", 2.5f }, { "rev_damp", 0.2f }, { "rev_mix", 0.3f } } },

        { "Cartoon Sax", "Gremlins", "Clean sax with sound effects that play along in your key: slide whistles, boings and laser pews on your notes.",
          { { "comp_amt", 0.4f }, { "grm_on", 1 }, { "grm_harsh", 0.0f }, { "grm_chance", 1.0f }, { "grm_swarm", 0.2f },
            { "grm_follow", 1.0f }, { "grm_dirt", 0.0f }, { "grm_level", 0.6f },
            { "rev_on", 1 }, { "rev_size", 0.35f }, { "rev_decay", 1.2f }, { "rev_mix", 0.18f } } },

        { "Broken Robot", "Gremlins", "Half silly, half broken machine, all bit-crushed, through a flanger and a ping-pong echo.",
          { { "grm_on", 1 }, { "grm_harsh", 0.5f }, { "grm_chance", 0.8f }, { "grm_swarm", 0.4f }, { "grm_dirt", 0.45f },
            { "grm_follow", 0.5f }, { "grm_level", 0.65f },
            { "mod_on", 1 }, { "mod_type", 1 }, { "mod_rate", 0.2f }, { "mod_depth", 0.7f }, { "mod_fb", 0.6f }, { "mod_mix", 0.45f },
            { "dly_on", 1 }, { "dly_sync", 5 }, { "dly_fb", 0.4f }, { "dly_mix", 0.25f } } },

        { "Rotary Club", "Vintage", "Spinning rotary speaker and tape warmth. Organ-trio soul.",
          { { "comp_amt", 0.35f }, { "drv_on", 1 }, { "drv_type", 3 }, { "drv_drive", 0.3f }, { "drv_tone", 0.45f }, { "drv_level", 3.5f },
            { "mod_on", 1 }, { "mod_type", 3 }, { "mod_rate", 5.8f }, { "mod_depth", 0.75f }, { "mod_mix", 0.85f },
            { "rev_on", 1 }, { "rev_size", 0.4f }, { "rev_decay", 1.2f }, { "rev_mix", 0.18f } } },

        { "Lo-Fi Tape Memory", "Vintage", "Narrow band, wow and flutter, soft tape saturation: a dusty old cassette.",
          { { "comp_amt", 0.55f }, { "drv_on", 1 }, { "drv_type", 3 }, { "drv_drive", 0.45f }, { "drv_tone", 0.3f }, { "drv_level", 3.5f },
            { "eq_lowcut", 260 }, { "eq_air", -8 }, { "eq_mid", 3 }, { "eq_midf", 1400 },
            { "mod_on", 1 }, { "mod_type", 0 }, { "mod_rate", 0.45f }, { "mod_depth", 0.35f }, { "mod_mix", 0.6f }, { "mod_fb", 0 },
            { "dly_on", 1 }, { "dly_sync", 2 }, { "dly_fb", 0.4f }, { "dly_tone", 1600 }, { "dly_wobble", 0.75f }, { "dly_mix", 0.25f },
            { "out_width", 0.6f } } },
    };
    return presets;
}
} // namespace rs
