# Nebulon — Hybrid Atmosphere Synthesizer

> **Also in this repo:** [**Reedstorm**](SaxRack/README.md), a saxophone effects rack (VST3 / AU) with a tuner, an in-key harmonizer, a tracking synth, an octaver, an auto-wah, drive, delay, shimmer reverb and more.

An original, open-source polyphonic synth plugin (VST3 / AU / Standalone) for cinematic pads,
evolving textures, granular soundscapes, keys, basses, leads and arps. It's in the same
category as big "hybrid" synth workstations, but all of its code, sound engines, UI and
presets are its own.

![Main page](docs/main.png)

## 🎧 Listen first

Every sound below was rendered straight from the plugin's engine, drums included. No samples from other products, no post-processing.

**Tracks:** five short pieces in different styles, keys and tempos, each built only from Nebulon presets:

| Track | Style | Built from |
|---|---|---|
| **[▶ Synthwave Drive](demos/00_Track_Synthwave_Drive.mp3)** | 104 BPM, A minor | Analog Kick, Punch Snare, Room Clap, hats, Velvet Dusk, Round Pluck Bass, Night Drive Arp, Solar Flare Lead |
| **[▶ Lofi Night](demos/01_Track_Lofi_Night.mp3)** | 78 BPM, swung, jazzy D minor | Lofi Dream Piano, Pluck Bass, Nylon Guitar, Vinyl Ghosts, drums |
| **[▶ Dark Pulse](demos/02_Track_Dark_Pulse.mp3)** | 140 BPM half-time, C minor | 808 Boom, Tension Strings, Glass Sequencer, Bowed Cello, Epic Impact, trap hats |
| **[▶ Sunrise House](demos/03_Track_Sunrise_House.mp3)** | 124 BPM, C major | four-on-the-floor kick, clap, Rubber Bass, Saw Stab, Steel String, Hyper Saw Lead |
| **[▶ Ambient Horizons](demos/04_Track_Ambient_Horizons.mp3)** | 84 BPM, F major | Aurora Veil, Ocean Memory, Music Box, Koto Garden, Hope Rising |
| **[▶ Drum Machine Beat](demos/05_Drum_Machine_Beat.mp3)** | 96 BPM | every Drums preset plus 808 Boom |

**More:** [▶ Preset Tour](demos/98_Preset_Tour_All_Presets.mp3) (all 83 presets, each playing its own song) ·
[▶ Inspire Roulette](demos/99_Inspire_Roulette.mp3) (ten patches invented by the INSPIRE button)

**Individual presets:**
[Glass Horizon](demos/10_Glass_Horizon.mp3) ·
[Aurora Veil](demos/11_Aurora_Veil.mp3) ·
[Warm Analog Strings](demos/12_Warm_Analog_Strings.mp3) ·
[Lofi Dream Piano](demos/13_Lofi_Dream_Piano.mp3) ·
[FM Electric Piano](demos/14_FM_Electric_Piano.mp3) ·
[Funky Clav](demos/15_Funky_Clav.mp3) ·
[Perc Organ](demos/16_Perc_Organ.mp3) ·
[Music Box](demos/17_Music_Box.mp3) ·
[Nylon Guitar](demos/18_Nylon_Guitar.mp3) ·
[Steel String](demos/19_Steel_String.mp3) ·
[Koto Garden](demos/20_Koto_Garden.mp3) ·
[Bowed Cello](demos/21_Bowed_Cello.mp3) ·
[Reese Growl](demos/22_Reese_Growl.mp3) ·
[Wobble Monster](demos/23_Wobble_Monster.mp3) ·
[808 Boom](demos/24_808_Boom.mp3) ·
[Hyper Saw Lead](demos/25_Hyper_Saw_Lead.mp3) ·
[Vowel Talker](demos/26_Vowel_Talker.mp3) ·
[Square Lead](demos/27_Square_Lead.mp3) ·
[Crystal Pluck](demos/28_Crystal_Pluck.mp3) ·
[Dream Harp](demos/29_Dream_Harp.mp3) ·
[Starlight Arp](demos/30_Starlight_Arp.mp3) ·
[Chip Arp](demos/31_Chip_Arp.mp3) ·
[Deep Space Station](demos/32_Deep_Space_Station.mp3) ·
[Rain on Glass](demos/33_Rain_on_Glass.mp3) ·
[Four Worlds](demos/34_Four_Worlds.mp3) ·
[Tension Strings](demos/35_Tension_Strings.mp3)

## ✨ Instant fun: no manual needed

- **▶ PREVIEW**: one click and the current patch plays a musical phrase that suits it (chords for pads, a bassline for basses, a melody for leads, broken chords for plucks, a groove for drums). There are five built-in songs (Dream, Synthwave, Lofi, Dark, Sunrise), each with its own key, tempo and rhythm, so different presets don't all play the same tune. It keeps playing while you browse presets or turn knobs.
- **🎲 INSPIRE**: invents a brand-new, playable patch with a name like *"~ Lunar Orchard"*. It picks a sound type (pad, pluck, keys, bass, lead, texture or arp), engines, sources, filters, modulation and effects within musical ranges. Like it? Press **SAVE**.
- **Nebula Pad**: an animated, audio-reactive XY surface on the MAIN page. Drag it while notes play: left/right = **Tone**, up/down = **Morph**. It glows and pulses with the output.
- **Info bar**: hover any control to see its name and value; otherwise it describes the current preset and shows tips.
- **Play with your computer keyboard**: `A W S E D F T G Y H U J K` play C to C.
- **Macros** (Tone, Morph, Grit, Spread) start in the centre, so you can push any preset in either direction.

## What it can do

### 4 layers × 6 sound engines
Each of the four layers (A–D) is a complete synth with its own engine, filter and envelopes:

| Engine | What it does |
|---|---|
| **Analog** | Anti-aliased oscillator that morphs Sine → Triangle → Saw → Square |
| **Wavetable** | 9 built-in morphing tables (Classic, Pulse Width, Vowel, Harmonic Sweep, Hard Sync, Digital Bells, Drawbar Organ, Spectral Growl, Crystal Dust), mip-mapped for alias-free playback |
| **FM** | Two-operator phase modulation with ratio, amount and feedback |
| **Granular** | Grain clouds with size, density, position, spray and pitch randomisation |
| **Sample** | Pitched playback with start point and crossfaded looping |
| **Pluck** | Physical-model string (Karplus-Strong) with brightness, decay and a *bow* control: guitars, koto, clav, harp, cello |

Granular and Sample can use **9 built-in source sounds** (Glass Bells, Breath Choir, Vocal Ooh,
Bowed Metal, Kalimba, Tape Strings, Crystal Shimmer, Ocean Wash, Vinyl Air) **or any audio file
you load** (WAV / AIFF / FLAC / MP3 / OGG, up to 60 s). Loaded samples are remembered in your
DAW project and in saved presets.

![Layer page](docs/layerA.png)

### Per layer
- Unison up to 7 voices with detune and stereo width
- **Two harmony voices** (±24 semitones) for instant chords, fifths and octaves
- Sub oscillator, noise, *analog drift* and drive
- **Pitch envelope** (±48 semitones) for kicks, toms, zaps, blips and plucky attacks
- **8 filter types**: LP12, LP24, Band, High, Notch, Comb +, Comb −, Vowel (formant)
- Amp and filter ADSR envelopes, velocity sensitivity, level and pan

### Modulation
- **4 LFOs**: 7 shapes (incl. S&H and smooth random drift), tempo sync, fade-in, retrigger or free-run
- **2 mod envelopes**
- **12-slot mod matrix**. Sources: LFOs, mod envelopes, velocity, mod wheel, aftertouch, key,
  per-note random, 4 macros. Destinations: pitch, cutoff, resonance, level, pan, *timbre*
  (wave shape / wavetable position / FM amount / grain position), detune, drive, sub,
  noise, grain size, grain density, filter-env amount. Each slot targets all layers or one.
- **4 bipolar macro knobs** (Tone, Morph, Grit, Spread), pre-wired through matrix slots 9–12 and re-routable; also driven by the Nebula Pad

![Modulation page](docs/mod.png)

### Arpeggiator
Up, Down, Up/Down, Random, As Played, Chord · 1–4 octaves · rates from 4 bars to 1/32
(incl. dotted and triplet) · gate · swing · latch · locks to your DAW's tempo and bar grid.

### Effects rack
Distortion (Soft / Hard / Fold / Bitcrush) → 2-band EQ → Phaser → Chorus →
tempo-synced ping-pong Delay → Reverb → soft-clipping master.

![Effects page](docs/fx.png)

### Performance
Polyphony 1–16 · Mono and Legato modes · Glide · pitch-bend range up to ±24 · on-screen keyboard.

### Presets
84 factory patches in 10 categories (Pads, Keys, Bass, Leads, Plucks, Arps, **Drums**, Drones,
Textures, Cinematic), each with a one-line description. Effects are off unless a preset uses
them, so basses, keys, leads and drums stay dry and punchy, while pads and atmospheres stay lush. **Save your own** with the SAVE button. They go to `Documents/Nebulon/Presets`
and show up under "User" in the browser. Factory patches also appear in your DAW's program list.

## Getting the plugin

Every push builds Windows, macOS and Linux binaries with GitHub Actions.
Open the repo's **Actions** tab → latest *Build* run → download `Nebulon-Windows`,
`Nebulon-macOS` or `Nebulon-Linux` from *Artifacts*. Then copy `Nebulon.vst3` to:

| OS | VST3 folder |
|---|---|
| Windows | `C:\Program Files\Common Files\VST3\` |
| macOS | `~/Library/Audio/Plug-Ins/VST3/` (AU: `~/Library/Audio/Plug-Ins/Components/`) |
| Linux | `~/.vst3/` |

On macOS the build is unsigned; if it's blocked, run
`xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Nebulon.vst3`.
The **Standalone** app runs without a DAW (on-screen keyboard or a MIDI controller).

## Building yourself

Requires CMake ≥ 3.22 and a C++17 compiler; JUCE 8 is downloaded automatically.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Output lands in `build/Nebulon_artefacts/Release/`. Pass `-DJUCE_DIR=/path/to/JUCE` to use a
local JUCE. Add `-DNEBULON_BUILD_TESTS=ON` to also build:

- `NebulonRenderTest`: renders every preset (checking for silence / NaNs and reporting CPU
  cost) and tests sample loading, the arpeggiator, Pluck-engine tuning, Preview, 20 Inspire
  patches and every UI page.
- `NebulonDemoRender <dir>`: re-renders all the demo audio in `demos/`.

## Project layout

```
Source/Engine.*              voice: 5 engines, unison/harmony, filters, envelopes, LFOs, mod matrix;
                             synth with polyphony limit, mono/legato, glide
Source/Wavetables.*          wavetable generation (FFT mip-maps)
Source/Sources.*             built-in granular/sample source sounds
Source/Arpeggiator.h         tempo-synced arpeggiator
Source/PhrasePlayer.h        five songs + phrase generator for PREVIEW and the demos
Source/FxChain.h             effects rack
Source/DSP.h                 SVF, comb, biquad, PolyBLEP oscillators
Source/Params.h              shared enums and name lists
Source/PluginProcessor.*     parameters, presets, sample loading, state
Source/Presets.h             factory patches
Source/PluginEditor.*        window, header, tabs, preset browser, scope
Source/Pages.*               Main / Layer / Modulation / Arp / Effects pages
Source/Controls.h            knob, choice, toggle and panel widgets
Source/Widgets.h             Nebula Pad and icon buttons
Source/NebulonLookAndFeel.h  styling
tools/RenderTest.cpp         offline test
tools/DemoRender.cpp         demo audio renderer
```

## License note

Built on [JUCE](https://juce.com), which is dual-licensed under AGPLv3 / commercial.
If you distribute binaries, either keep this project open source under AGPLv3 or obtain a
JUCE license.
