// Renders demo audio straight from the synth engine:
//   - a multi-instrument showcase track
//   - individual preset demos (each playing its Preview phrase)
//   - a tour through every factory preset
//   - a run of freshly invented "Inspire" patches
#include "../Source/PluginProcessor.h"
#include "../Source/Presets.h"
#include <juce_audio_formats/juce_audio_formats.h>

using namespace neb;
static constexpr double sr = 48000.0, bpm = 120.0;
static constexpr int block = 512;

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
    std::printf ("  wrote %s\n", file.getFileName().toRawUTF8());
}

static int beatToSample (double beat) { return (int) (beat * 60.0 / bpm * sr); }

static std::vector<PhraseEvent> repeatPhrase (const Phrase& p, int fromCycle, int toCycle, double maxLen = 1e9)
{
    std::vector<PhraseEvent> out;
    for (int c = fromCycle; c < toCycle; ++c)
        for (auto e : p.events)
            if (e.start < maxLen)
                out.push_back ({ e.start + c * p.loopBeats, juce::jmin (e.length, maxLen - e.start), e.note, e.vel });
    return out;
}

static juce::AudioBuffer<float> renderEvents (NebulonProcessor& proc, const std::vector<PhraseEvent>& events,
                                              double totalBeats, double tailSec)
{
    proc.prepareToPlay (sr, block);
    struct Ev { int pos; juce::MidiMessage msg; };
    std::vector<Ev> evs;
    for (auto& e : events)
    {
        evs.push_back ({ beatToSample (e.start + e.length), juce::MidiMessage::noteOff (1, e.note) });
        evs.push_back ({ beatToSample (e.start), juce::MidiMessage::noteOn (1, e.note, (juce::uint8) e.vel) });
    }
    std::stable_sort (evs.begin(), evs.end(), [] (const Ev& a, const Ev& b) { return a.pos < b.pos; });

    const int total = beatToSample (totalBeats) + (int) (tailSec * sr);
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
        proc.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, buf, c, 0, n);
    }
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

static juce::AudioBuffer<float> renderPreset (const juce::String& name, const std::vector<PhraseEvent>& ev, double beats, double tail)
{
    NebulonProcessor proc;
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.setCurrentProgram (presetIndex (proc, name));
    return renderEvents (proc, ev, beats, tail);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? argv[1] : "demos");
    outDir.createDirectory();

    // ---------------------------------------------------------------- showcase track
    std::printf ("Showcase track...\n");
    {
        const double beats = 84;
        auto pad   = renderPreset ("Aurora Veil", [] { auto e = repeatPhrase (phrase (phChords), 0, 5);
                                                       for (int n : { 53, 57, 60, 64 }) e.push_back ({ 80, 4, n, 85 }); return e; }(), beats, 6);
        auto tex   = renderPreset ("Ocean Memory", repeatPhrase (phrase (phDrone), 0, 5), beats, 6);
        auto arp   = renderPreset ("Night Drive Arp", repeatPhrase (phrase (phChords), 1, 5), beats, 6);
        auto bass  = renderPreset ("Round Pluck Bass", repeatPhrase (phrase (phBass), 2, 5), beats, 6);
        auto lead  = renderPreset ("Solar Flare Lead", repeatPhrase (phrase (phLead), 3, 5), beats, 6);
        auto hope  = renderPreset ("Hope Rising", repeatPhrase (phrase (phChords), 4, 5), beats, 6);
        auto hit   = renderPreset ("Epic Impact", { { 80, 2, 41, 120 }, { 80, 2, 48, 110 } }, beats, 6);

        juce::AudioBuffer<float> mix (2, pad.getNumSamples());
        mix.clear();
        auto add = [&] (const juce::AudioBuffer<float>& b, float g)
        { for (int c = 0; c < 2; ++c) mix.addFrom (c, 0, b, c, 0, juce::jmin (b.getNumSamples(), mix.getNumSamples()), g); };
        add (pad, 0.55f); add (tex, 0.35f); add (arp, 0.38f); add (bass, 0.6f); add (lead, 0.42f); add (hope, 0.4f); add (hit, 0.5f);
        for (int c = 0; c < 2; ++c)
        {
            auto* d = mix.getWritePointer (c);
            for (int i = 0; i < mix.getNumSamples(); ++i) d[i] = std::tanh (d[i] * 1.2f);
        }
        normalise (mix);
        fadeOut (mix, 3.0);
        writeWav (outDir.getChildFile ("00_Nebulon_Showcase.wav"), mix);
    }

    // ---------------------------------------------------------------- individual demos
    std::printf ("Preset demos...\n");
    const char* featured[] = { "Glass Horizon", "Nebula Choir", "Aurora Veil", "Warm Analog Strings", "Lofi Dream Piano",
                               "FM Electric Piano", "Dream Harp", "Reese Growl", "Wobble Monster", "Hyper Saw Lead",
                               "Vowel Talker", "Crystal Pluck", "Ice Drops", "Starlight Arp", "Night Drive Arp",
                               "Deep Space Station", "Abyssal Choir", "Rain on Glass", "Four Worlds", "Tension Strings" };
    {
        NebulonProcessor tmp;
        int n = 1;
        for (auto* name : featured)
        {
            const auto cat = tmp.getPresetCategory (presetIndex (tmp, name));
            const auto& ph = phrase (phraseTypeForCategory (cat));
            const int cycles = (cat == "Bass" || cat == "Leads" || cat == "Plucks" || cat == "Arps") ? 2 : 1;
            auto b = renderPreset (name, repeatPhrase (ph, 0, cycles), ph.loopBeats * cycles, 4.0);
            normalise (b);
            fadeOut (b, 1.5);
            writeWav (outDir.getChildFile (juce::String (n++).paddedLeft ('0', 2) + "_" + juce::String (name).replaceCharacter (' ', '_') + ".wav"), b);
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
            const auto& ph = phrase (phraseTypeForCategory (proc.getPresetCategory (i)));
            auto b = renderEvents (proc, repeatPhrase (ph, 0, 1, 8.0), 8.0, 1.0);
            normalise (b, 0.8f);
            b.applyGainRamp (0, (int) (0.02 * sr), 0.0f, 1.0f);
            fadeOut (b, 1.2);
            append (tour, writePos, b, (int) (0.3 * sr));
            std::printf ("  %2d %s\n", i, proc.getProgramName (i).toRawUTF8());
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
            const auto& ph = phrase (phraseTypeForCategory (proc.currentCategory));
            auto b = renderEvents (proc, repeatPhrase (ph, 0, 1, 8.0), 8.0, 1.2);
            normalise (b, 0.8f);
            fadeOut (b, 1.2);
            append (out, writePos, b, (int) (0.3 * sr));
            std::printf ("  %s (%s)\n", proc.currentPresetName.toRawUTF8(), proc.currentCategory.toRawUTF8());
        }
        writeWav (outDir.getChildFile ("99_Inspire_Roulette.wav"), out);
    }
    return 0;
}
