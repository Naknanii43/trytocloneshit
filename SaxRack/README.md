# Reedstorm: Saxophone Effects Rack (VST3 / AU / Standalone)

A 10-module effects rack built specifically for **saxophone**. It listens to your horn with a
pitch tracker, so it can show a **tuner** in your instrument's written key, add **in-key
harmonies**, drive a **synth voice that follows your playing**, and keep the octaver locked to your note.

![Reedstorm](docs/ui.png)

## 🎧 Listen

Each clip is the same synthetic sax phrase run through one factory preset.
Start with the **[▶ dry phrase](demos/00_Dry_Sax.mp3)**.

| | | |
|---|---|---|
| [Studio Polish](demos/02_Studio_Polish.mp3) | [Smoky Jazz Club](demos/03_Smoky_Jazz_Club.mp3) | [Bright Pop Lead](demos/04_Bright_Pop_Lead.mp3) |
| [Funk Envelope Wah](demos/05_Funk_Envelope_Wah.mp3) | [Robot Talk Wah](demos/06_Robot_Talk_Wah.mp3) | [Octave Fuzz Monster](demos/07_Octave_Fuzz_Monster.mp3) |
| [Blues Shouter Growl](demos/08_Blues_Shouter_Growl.mp3) | [Sub Bass Sax](demos/09_Sub_Bass_Sax.mp3) | [Synth Horn Lead](demos/10_Synth_Horn_Lead.mp3) |
| [Square Wave Sax](demos/11_Square_Wave_Sax.mp3) | [Horn Section](demos/12_Horn_Section.mp3) | [Octave Unison](demos/13_Octave_Unison.mp3) |
| [Ethio Minor Section](demos/14_Ethio_Minor_Section.mp3) | [Ambient Shimmer](demos/15_Ambient_Shimmer.mp3) | [Dub Echo Chamber](demos/16_Dub_Echo_Chamber.mp3) |
| [Jet Flanger Space](demos/17_Jet_Flanger_Space.mp3) | [Rotary Club](demos/18_Rotary_Club.mp3) | [Lo-Fi Tape Memory](demos/19_Lo-Fi_Tape_Memory.mp3) |

(The input is a synthesized saxophone, so expect the effects to sound better on a real horn.)

## The rack

```
IN ─ DYNAMICS ─ OCTAVER ─ SYNTH ─ ENV FILTER ─ DRIVE ─ EQ ─┬─ HARMONY ─ MODULATION ─ DELAY ─ REVERB ─ OUT
     └──────────── mono core (one voice) ───────────────┘  └──────────────── stereo ─────────────────┘
```

Click any module in the chain strip (or its power LED) to switch it on or off. Modules crossfade
in and out, so switching never clicks.

| Module | What it does |
|---|---|
| **Dynamics** | Noise gate for key clicks and breath between phrases, plus an opto-style compressor (*Squeeze*, *Speed*) with auto make-up and a GR meter |
| **Octaver** | Analog-style flip-flop divider (−1 and −2 octaves). Its input filter follows the tracked pitch, so it locks on cleanly. Also adds a pitch-synchronous octave up |
| **Synth** | A mono synth voice (Saw / Square / Saw+Sub / Soft) that follows your **pitch** and your **breath**: your dynamics drive the amp and the filter. Octave −2…+1, glide, resonance |
| **Env Filter** | Auto-wah: *Env Up*, *Env Down*, *LFO* or *Pedal* mode. In Pedal mode the SENS knob becomes the pedal position, so you can map it to an expression pedal or automate it |
| **Drive** | 2× oversampled **Tube**, **Fuzz** (with octave-up blend), **Growl** (the rough "hum while you blow" texture) and **Tape** |
| **EQ** | Sax-voiced: 24 dB/oct low cut, *Body* (210 Hz), *Honk* (sweepable 300 Hz to 5 kHz), *Air* shelf, with a live curve |
| **Harmony** | Two pitch-shifted voices. **Diatonic** mode stays in key (pick key and scale: Major, Minor, Dorian, Mixolydian, Harmonic Minor, Blues, both Pentatonics); **Chromatic** mode uses fixed intervals. Voices are panned with *Spread* |
| **Modulation** | Chorus, Flanger, Phaser (6-stage) and Rotary speaker |
| **Delay** | Tape echo, free or synced to the host tempo (1/2 to 1/16, dotted, triplet). Ping-pong, wobble (wow and flutter), a filtered and saturated feedback loop, and **ducking** so echoes stay out of the way while you play |
| **Reverb** | 8-line feedback-delay-network reverb with pre-delay, size, decay, damping and **Shimmer** (an octave-up feedback path for ambient pads) |

**Header:** factory preset browser, **tuner**, **pitch trace** (a scrolling piano roll of what you play; with Harmony on it also draws the harmony voices in green), input gain, output gain, stereo width, and IN / L / R meters. A safety limiter on the output stops overs.

### Tuner and transposition
Pick your horn in **Instrument** (Concert C, Soprano B♭, Alto E♭, Tenor B♭, Bari E♭). The tuner and pitch trace then show **written** notes (with the sounding note underneath), and the Harmony **Key** is also in written pitch. On alto, "C major" means the C major you read. The reference pitch (A4) is adjustable from 430 to 450 Hz.

### Tips
- Use a close mic or a clip-on mic for best tracking. Feed the plugin a **dry**, single-voice signal.
- Harmony and Synth follow the note you play. Very breathy or growled notes can't be tracked, so the harmony keeps its last interval.
- Use the Delay's *Duck* control: a lot of echo between phrases while staying clean while you play.
- Most knobs show their value while hovered. Double-click any knob to reset it.

## Factory presets (19)
**Basics:** Init, Studio Polish, Smoky Jazz Club, Bright Pop Lead ·
**Funk:** Funk Envelope Wah, Robot Talk Wah ·
**Heavy:** Octave Fuzz Monster, Blues Shouter Growl, Sub Bass Sax ·
**Synth:** Synth Horn Lead, Square Wave Sax ·
**Harmony:** Horn Section, Octave Unison, Ethio Minor Section ·
**Space:** Ambient Shimmer, Dub Echo Chamber, Jet Flanger Space ·
**Vintage:** Rotary Club, Lo-Fi Tape Memory

Presets are level-matched and also appear in your DAW's program list. Your input gain, output
gain and tuner settings are kept when you switch presets.

## Getting the plugin
GitHub Actions builds Windows, macOS and Linux binaries on every push. Open the **Actions** tab, choose the latest
**Build Reedstorm** run, and download `Reedstorm-<OS>` from *Artifacts*. Copy `Reedstorm.vst3` to:

| OS | VST3 folder |
|---|---|
| Windows | `C:\Program Files\Common Files\VST3\` |
| macOS | `~/Library/Audio/Plug-Ins/VST3/` (AU: `~/Library/Audio/Plug-Ins/Components/`) |
| Linux | `~/.vst3/` |

On macOS the build is unsigned. If macOS blocks it, run `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Reedstorm.vst3`.
The **Standalone** app lets you plug a mic into your audio interface and play without a DAW.

## Building it yourself
You need CMake 3.22 or newer and a C++17 compiler. JUCE 8 is downloaded automatically.

```bash
cd SaxRack
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The output goes to `build/Reedstorm_artefacts/Release/`. Pass `-DJUCE_DIR=/path/to/JUCE` to use a local JUCE.
Add `-DREEDSTORM_BUILD_TESTS=ON` to also build `ReedstormRenderTest <dir> [--demos]`, which:

- checks the pitch tracker over the full sax range (bari low A to soprano altissimo) at 44.1, 48 and 96 kHz
- checks that the pitch shifter and harmony voices are in tune (within a few cents), the octaver divides correctly, and the diatonic and transposed-key logic gives the right intervals
- renders every preset (NaN, silence, clipping and CPU checks; uses mono and stereo inputs) and snapshots the UI
- with `--demos`, writes the WAVs behind `demos/`

## Source layout
```
Source/Rack.*            the signal chain (all DSP modules)
Source/PitchTracker.h    YIN pitch tracker (decimated, cheap enough for the audio thread)
Source/DSP.h             SVF, biquad, delay line, pitch-synchronous pitch shifter, helpers
Source/Params.h          parameters, scales, instrument transpositions
Source/Presets.h         factory presets
Source/PluginProcessor.* plugin wrapper, presets, state
Source/PluginEditor.*    window layout (scales to any window size)
Source/Widgets.h         knobs, module panels, tuner, pitch trace, EQ curve, chain strip, meters
Source/LookAndFeel.h     styling
tools/RenderTest.cpp     tests and demo renderer
```

## License note
Built on [JUCE](https://juce.com), which is dual-licensed under AGPLv3 and a commercial license. If you distribute binaries,
keep this project open source under AGPLv3 or get a JUCE license.
