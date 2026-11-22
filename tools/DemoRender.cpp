// Renders demo audio straight from the synth engine:
//   - genre tracks with drums (Synthwave, Lofi, Dark, House) + an ambient showcase
//   - individual preset demos, each playing its own Preview song
//   - a tour through every factory preset, and a run of "Inspire" patches
#include "../Source/PluginProcessor.h"
#include "../Source/Presets.h"
#include <juce_audio_formats/juce_audio_formats.h>

using namespace neb;
static constexpr double sr = 48000.0;
static constexpr int block = 512;

// A fake host transport so arps, synced LFOs and delays follow each song's tempo.
struct DemoPlayHead : juce::AudioPlayHead
{
    double bpm = 120.0;
    juce::int64 samplePos = 0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setIsPlaying (true);
        p.setTimeInSamples (samplePos);
        p.setPpqPosition ((double) samplePos / sr * bpm / 60.0);
        return p;
    }
};

static void writeWav (const juce::File& file, const juce::AudioBuffer<float>& buf)
{
    juce::WavAudioFormat wav;
    file.deleteFile();
    if (auto stream = file.createOutputStream())
        if (auto* w = wav.createWriterFor (stream.get(), sr, 2, 24, {}, 0))
        {
            stream.release();
            std::unique_ptr<juce::AudioFormatWriter> writer (w);
            writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
        }
    std::printf ("  wrote %s (%.1fs)\n", file.getFileName().toRawUTF8(), buf.getNumSamples() / sr);
}

using Events = std::vector<PhraseEvent>;

static Events repeatPhrase (const Phrase& p, int fromCycle, int toCycle, double maxLen = 1e9)
{
    Events out;
    for (int c = fromCycle; c < toCycle; ++c)
        for (auto e : p.events)
            if (e.start < maxLen)
                out.push_back ({ e.start + c * p.loopBeats, juce::jmin (e.length, maxLen - e.start), e.note, e.vel });
    return out;
}

// One bar of drum hits {beat, velocity}, repeated over bars [fromBar, toBar)
static Events drumPattern (std::initializer_list<std::pair<double, int>> bar, int fromBar, int toBar, int note = 48, double len = 0.2)
{
    Events out;
    for (int b = fromBar; b < toBar; ++b)
        for (auto& [beat, vel] : bar) out.push_back ({ b * 4.0 + beat, len, note, vel });
    return out;
}

static Events operator+ (Events a, const Events& b) { a.insert (a.end(), b.begin(), b.end()); return a; }

static juce::AudioBuffer<float> renderEvents (NebulonProcessor& proc, const Events& events, double bpm,
                                              double totalBeats, double tailSec)
{
    DemoPlayHead ph;
    ph.bpm = bpm;
    proc.setPlayHead (&ph);
    proc.prepareToPlay (sr, block);
    auto toSample = [bpm] (double beat) { return (int) (beat * 60.0 / bpm * sr); };

    struct Ev { int pos; juce::MidiMessage msg; };
    std::vector<Ev> evs;
    for (auto& e : events)
    {
        evs.push_back ({ toSample (e.start + e.length), juce::MidiMessage::noteOff (1, e.note) });
        evs.push_back ({ toSample (e.start), juce::MidiMessage::noteOn (1, e.note, (juce::uint8) juce::jlimit (1, 127, e.vel)) });
    }
    std::stable_sort (evs.begin(), evs.end(), [] (const Ev& a, const Ev& b) { return a.pos < b.pos; });

    const int total = toSample (totalBeats) + (int) (tailSec * sr);
    juce::AudioBuffer<float> out (2, total);
    size_t next = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = juce::jmin (block, total - pos);
        juce::AudioBuffer<float> buf (2, n);
        juce::MidiBuffer midi;
        while (next < evs.size() && evs[next].pos < pos + n)
        {
            midi.addEvent (evs[next].msg, juce::jmax (0, evs[next].pos - pos));
            ++next;
        }
        ph.samplePos = pos;
        proc.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, buf, c, 0, n);
    }
    proc.setPlayHead (nullptr);
    return out;
}

static void normalise (juce::AudioBuffer<float>& b, float target = 0.89f)
{
    const float pk = b.getMagnitude (0, b.getNumSamples());
    if (pk > 1.0e-4f) b.applyGain (target / pk);
}

static void fadeOut (juce::AudioBuffer<float>& b, double secs)
{
    const int n = juce::jmin (b.getNumSamples(), (int) (secs * sr));
    b.applyGainRamp (b.getNumSamples() - n, n, 1.0f, 0.0f);
}

static void append (juce::AudioBuffer<float>& dst, int& writePos, const juce::AudioBuffer<float>& src, int overlap)
{
    const int start = juce::jmax (0, writePos - overlap);
    if (dst.getNumSamples() < start + src.getNumSamples())
        dst.setSize (2, start + src.getNumSamples(), true, true);
    for (int c = 0; c < 2; ++c) dst.addFrom (c, start, src, c, 0, src.getNumSamples());
    writePos = start + src.getNumSamples();
}

static int presetIndex (NebulonProcessor& p, const juce::String& name)
{
    for (int i = 0; i < p.getNumPrograms(); ++i)
        if (p.getProgramName (i) == name) return i;
    std::printf ("unknown preset %s\n", name.toRawUTF8());
    std::exit (1);
}

static juce::AudioBuffer<float> renderPreset (const juce::String& name, const Events& ev, double bpm, double beats, double tail)
{
    NebulonProcessor proc;
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.setCurrentProgram (presetIndex (proc, name));
    return renderEvents (proc, ev, bpm, beats, tail);
}

struct Part { const char* preset; Events events; float gain; };

static void renderTrack (const juce::File& file, double bpm, double beats, const std::vector<Part>& parts)
{
    juce::AudioBuffer<float> mix;
    for (auto& p : parts)
    {
        auto b = renderPreset (p.preset, p.events, bpm, beats, 4.0);
        if (mix.getNumSamples() == 0) { mix.setSize (2, b.getNumSamples()); mix.clear(); }
        for (int c = 0; c < 2; ++c) mix.addFrom (c, 0, b, c, 0, juce::jmin (b.getNumSamples(), mix.getNumSamples()), p.gain);
    }
    for (int c = 0; c < 2; ++c)
    {
        auto* d = mix.getWritePointer (c);
        for (int i = 0; i < mix.getNumSamples(); ++i) d[i] = std::tanh (d[i] * 1.15f);
    }
    normalise (mix);
    fadeOut (mix, 2.5);
    writeWav (file, mix);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? argv[1] : "demos");
    outDir.createDirectory();
    const int SYNTH = 1, LOFI = 2, DARK = 3, HOUSE = 4, DREAM = 0;

    std::printf ("Genre tracks...\n");
    // 1. Synthwave, 104 BPM, A minor
    renderTrack (outDir.getChildFile ("00_Track_Synthwave_Drive.wav"), 104, 64, {
        { "Analog Kick", drumPattern ({ { 0, 120 }, { 1, 110 }, { 2, 120 }, { 3, 110 } }, 0, 16), 0.9f },
        { "Punch Snare", drumPattern ({ { 1, 115 }, { 3, 120 } }, 0, 16), 0.55f },
        { "Room Clap",   drumPattern ({ { 1, 100 }, { 3, 105 } }, 4, 16), 0.35f },
        { "Closed Hat",  drumPattern ({ { 0, 70 }, { 0.5, 100 }, { 1, 70 }, { 1.5, 100 }, { 2, 70 }, { 2.5, 100 }, { 3, 70 }, { 3.5, 100 } }, 0, 16), 0.35f },
        { "Open Hat",    drumPattern ({ { 3.5, 90 } }, 8, 16), 0.25f },
        { "Velvet Dusk", repeatPhrase (phrase (phChords, SYNTH), 0, 4), 0.35f },
        { "Round Pluck Bass", repeatPhrase (phrase (phBass, SYNTH), 0, 4), 0.75f },
        { "Night Drive Arp", repeatPhrase (phrase (phChords, SYNTH), 1, 4), 0.35f },
        { "Solar Flare Lead", repeatPhrase (phrase (phLead, SYNTH), 2, 4), 0.4f },
    });

    // 2. Lofi, 78 BPM, D minor, swung
    renderTrack (outDir.getChildFile ("01_Track_Lofi_Night.wav"), 78, 48, {
        { "Analog Kick", drumPattern ({ { 0, 115 }, { 1.67, 90 }, { 2.5, 105 } }, 0, 12), 0.8f },
        { "Punch Snare", drumPattern ({ { 1, 105 }, { 3, 110 } }, 0, 12), 0.5f },
        { "Closed Hat",  drumPattern ({ { 0, 80 }, { 0.67, 60 }, { 1, 80 }, { 1.67, 60 }, { 2, 80 }, { 2.67, 60 }, { 3, 80 }, { 3.67, 65 } }, 0, 12), 0.3f },
        { "Lofi Dream Piano", repeatPhrase (phrase (phComp, LOFI), 0, 3), 0.55f },
        { "Pluck Bass", repeatPhrase (phrase (phBass, LOFI), 0, 3), 0.7f },
        { "Vinyl Ghosts", repeatPhrase (phrase (phChords, LOFI), 0, 3), 0.2f },
        { "Nylon Guitar", repeatPhrase (phrase (phLead, LOFI), 1, 3), 0.5f },
    });

    // 3. Dark half-time, 140 BPM, C minor
    renderTrack (outDir.getChildFile ("02_Track_Dark_Pulse.wav"), 140, 64, {
        { "Analog Kick", drumPattern ({ { 0, 120 }, { 2.75, 100 } }, 0, 16), 0.85f },
        { "Punch Snare", drumPattern ({ { 2, 125 } }, 0, 16), 0.6f },
        { "Closed Hat",  drumPattern ({ { 0, 90 }, { 0.25, 50 }, { 0.5, 75 }, { 0.75, 50 }, { 1, 90 }, { 1.25, 50 }, { 1.5, 75 }, { 1.75, 60 },
                                        { 2, 90 }, { 2.25, 50 }, { 2.5, 75 }, { 2.75, 50 }, { 3, 90 }, { 3.125, 60 }, { 3.25, 70 }, { 3.5, 75 }, { 3.75, 55 } }, 0, 16), 0.3f },
        { "808 Boom", repeatPhrase (phrase (phBass, DARK), 0, 4), 0.8f },
        { "Tension Strings", repeatPhrase (phrase (phChords, DARK), 0, 4), 0.3f },
        { "Glass Sequencer", repeatPhrase (phrase (phChords, DARK), 1, 4), 0.25f },
        { "Bowed Cello", repeatPhrase (phrase (phLead, DARK), 2, 4), 0.55f },
        { "Epic Impact", Events { { 0, 2, 36, 120 }, { 32, 2, 36, 120 } }, 0.4f },
    });

    // 4. House, 124 BPM, C major
    renderTrack (outDir.getChildFile ("03_Track_Sunrise_House.wav"), 124, 64, {
        { "Analog Kick", drumPattern ({ { 0, 120 }, { 1, 120 }, { 2, 120 }, { 3, 120 } }, 0, 16), 0.9f },
        { "Room Clap",   drumPattern ({ { 1, 110 }, { 3, 115 } }, 0, 16), 0.45f },
        { "Open Hat",    drumPattern ({ { 0.5, 95 }, { 1.5, 90 }, { 2.5, 95 }, { 3.5, 90 } }, 0, 16), 0.3f },
        { "Closed Hat",  drumPattern ({ { 0.25, 60 }, { 0.75, 60 }, { 1.25, 60 }, { 1.75, 60 }, { 2.25, 60 }, { 2.75, 60 }, { 3.25, 60 }, { 3.75, 60 } }, 4, 16), 0.25f },
        { "Rubber Bass", repeatPhrase (phrase (phBass, HOUSE), 0, 4), 0.65f },
        { "Saw Stab", repeatPhrase (phrase (phComp, HOUSE), 0, 4), 0.4f },
        { "Steel String", repeatPhrase (phrase (phPluck, HOUSE), 1, 4), 0.4f },
        { "Hyper Saw Lead", repeatPhrase (phrase (phLead, HOUSE), 2, 4), 0.35f },
    });

    // 5. Ambient showcase, 84 BPM (no drums)
    renderTrack (outDir.getChildFile ("04_Track_Ambient_Horizons.wav"), 84, 68, {
        { "Aurora Veil", repeatPhrase (phrase (phChords, DREAM), 0, 4) + Events { { 64, 4, 53, 85 }, { 64, 4, 57, 85 }, { 64, 4, 60, 85 }, { 64, 4, 64, 85 } }, 0.55f },
        { "Ocean Memory", repeatPhrase (phrase (phDrone, DREAM), 0, 4), 0.35f },
        { "Music Box", repeatPhrase (phrase (phPluck, DREAM), 1, 4), 0.3f },
        { "Koto Garden", repeatPhrase (phrase (phLead, DREAM), 2, 4), 0.45f },
        { "Hope Rising", repeatPhrase (phrase (phChords, DREAM), 3, 4), 0.4f },
    });

    // 6. Drum machine: every drum preset in one beat
    renderTrack (outDir.getChildFile ("05_Drum_Machine_Beat.wav"), 96, 32, {
        { "Analog Kick", drumPattern ({ { 0, 125 }, { 0.75, 95 }, { 2.5, 115 } }, 0, 8), 0.9f },
        { "Punch Snare", drumPattern ({ { 1, 120 }, { 3, 120 } }, 0, 8), 0.6f },
        { "Room Clap",   drumPattern ({ { 3, 110 } }, 2, 8), 0.4f },
        { "Closed Hat",  drumPattern ({ { 0, 90 }, { 0.5, 70 }, { 1, 90 }, { 1.5, 70 }, { 2, 90 }, { 2.5, 70 }, { 3, 90 } }, 0, 8), 0.35f },
        { "Open Hat",    drumPattern ({ { 3.5, 100 } }, 0, 8), 0.3f },
        { "Deep Tom",    Events { { 28, 0.2, 55, 110 }, { 28.5, 0.2, 52, 110 }, { 29, 0.2, 48, 115 }, { 29.5, 0.2, 45, 120 } }, 0.6f },
        { "Laser Zap",   Events { { 15.5, 0.3, 72, 100 }, { 31.5, 0.3, 72, 100 } }, 0.3f },
        { "808 Boom",    Events { { 0, 1.5, 36, 115 }, { 2.5, 1, 36, 110 }, { 8, 1.5, 39, 115 }, { 10.5, 1, 39, 110 },
                                  { 16, 1.5, 36, 115 }, { 18.5, 1, 36, 110 }, { 24, 1.5, 34, 115 }, { 26.5, 1, 34, 110 } }, 0.7f },
    });

    // ---------------------------------------------------------------- individual demos
    std::printf ("Preset demos...\n");
    const char* featured[] = { "Glass Horizon", "Aurora Veil", "Warm Analog Strings", "Lofi Dream Piano", "FM Electric Piano",
                               "Funky Clav", "Perc Organ", "Music Box", "Nylon Guitar", "Steel String", "Koto Garden",
                               "Bowed Cello", "Reese Growl", "Wobble Monster", "808 Boom", "Hyper Saw Lead", "Vowel Talker",
                               "Square Lead", "Crystal Pluck", "Dream Harp", "Starlight Arp", "Chip Arp", "Deep Space Station",
                               "Rain on Glass", "Four Worlds", "Tension Strings" };
    {
        NebulonProcessor tmp;
        int n = 10;
        for (auto* name : featured)
        {
            const auto cat = tmp.getPresetCategory (presetIndex (tmp, name));
            const int song = songForPreset (name, cat);
            const auto& ph = phrase (phraseTypeForCategory (cat), song);
            const int cycles = (cat == "Bass" || cat == "Leads" || cat == "Plucks" || cat == "Arps") ? 2 : 1;
            auto b = renderPreset (name, repeatPhrase (ph, 0, cycles), songs()[(size_t) song].bpm, ph.loopBeats * cycles, 3.5);
            normalise (b);
            fadeOut (b, 1.5);
            writeWav (outDir.getChildFile (juce::String (n++) + "_" + juce::String (name).replaceCharacter (' ', '_') + ".wav"), b);
        }
    }

    // ---------------------------------------------------------------- tour of every preset
    std::printf ("Preset tour...\n");
    {
        NebulonProcessor proc;
        proc.setPlayConfigDetails (0, 2, sr, block);
        juce::AudioBuffer<float> tour (2, 0);
        int writePos = 0;
        for (int i = 1; i < proc.getNumPrograms(); ++i)
        {
            proc.setCurrentProgram (i);
            const auto cat = proc.getPresetCategory (i);
            const int song = songForPreset (proc.getProgramName (i), cat);
            const double bpm = songs()[(size_t) song].bpm;
            const double beats = juce::jmin (16.0, std::round (4.5 * bpm / 60.0 / 4.0) * 4.0); // ~4.5 seconds
            auto b = renderEvents (proc, repeatPhrase (phrase (phraseTypeForCategory (cat), song), 0, 1, beats), bpm, beats, 1.0);
            normalise (b, 0.8f);
            b.applyGainRamp (0, (int) (0.02 * sr), 0.0f, 1.0f);
            fadeOut (b, 1.0);
            append (tour, writePos, b, (int) (0.25 * sr));
        }
        writeWav (outDir.getChildFile ("98_Preset_Tour_All_Presets.wav"), tour);
    }

    // ---------------------------------------------------------------- inspire roulette
    std::printf ("Inspire roulette...\n");
    {
        NebulonProcessor proc;
        proc.setPlayConfigDetails (0, 2, sr, block);
        juce::AudioBuffer<float> out (2, 0);
        int writePos = 0;
        for (int i = 0; i < 10; ++i)
        {
            proc.inspire();
            const int song = songForPreset (proc.currentPresetName, proc.currentCategory);
            const double bpm = songs()[(size_t) song].bpm;
            const double beats = std::round (5.0 * bpm / 60.0 / 4.0) * 4.0;
            auto b = renderEvents (proc, repeatPhrase (phrase (phraseTypeForCategory (proc.currentCategory), song), 0, 1, beats), bpm, beats, 1.2);
            normalise (b, 0.8f);
            fadeOut (b, 1.2);
            append (out, writePos, b, (int) (0.3 * sr));
            std::printf ("  %s (%s, %s song)\n", proc.currentPresetName.toRawUTF8(), proc.currentCategory.toRawUTF8(), songs()[(size_t) song].name);
        }
        writeWav (outDir.getChildFile ("99_Inspire_Roulette.wav"), out);
    }
    return 0;
}
