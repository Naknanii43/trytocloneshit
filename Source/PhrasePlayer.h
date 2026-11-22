#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace neb
{
// Musical material for the Preview button and the demo renderer:
// five little "songs", each with its own key, tempo, harmony and grooves.
struct PhraseEvent { double start, length; int note, vel; };
struct Phrase { std::vector<PhraseEvent> events; double loopBeats = 16.0; };

enum PhraseType { phChords, phComp, phBass, phLead, phPluck, phDrone, phDrums, numPhraseTypes };

inline PhraseType phraseTypeForCategory (const juce::String& c)
{
    if (c == "Keys")   return phComp;
    if (c == "Bass")   return phBass;
    if (c == "Leads")  return phLead;
    if (c == "Plucks") return phPluck;
    if (c == "Drones") return phDrone;
    if (c == "Drums")  return phDrums;
    return phChords;
}

struct Song
{
    const char* name;
    double bpm;
    int chords[4][4];
    int roots[4];
    std::vector<PhraseEvent> bass;   // one bar; note = interval above the root
    std::vector<PhraseEvent> comp;   // one bar of chord hits (note unused)
    double pluckStep;                // 0.5 = 8ths, 0.25 = 16ths
    std::vector<PhraseEvent> lead;   // 16 beats
};

inline const std::vector<Song>& songs()
{
    static const std::vector<Song> s {
        { "Dream", 84,
          { { 53, 57, 60, 64 }, { 55, 59, 62, 64 }, { 57, 60, 64, 71 }, { 52, 55, 59, 62 } }, { 53, 55, 57, 52 },
          { { 0, 0.6, 0, 110 }, { 0.75, 0.45, 0, 90 }, { 1.5, 0.4, 12, 100 }, { 2, 0.6, 0, 105 }, { 3, 0.4, 0, 95 }, { 3.5, 0.4, 7, 92 } },
          { { 0, 1.4, 0, 100 }, { 1.5, 0.9, 0, 78 }, { 2.5, 0.4, 0, 70 }, { 3, 0.9, 0, 86 } }, 0.5,
          { { 0, 1.5, 69, 100 }, { 1.5, 0.5, 67, 85 }, { 2, 1, 65, 90 }, { 3, 1, 64, 90 }, { 4, 2, 62, 100 }, { 6, 1, 67, 90 },
            { 7, 1, 71, 95 }, { 8, 1.5, 72, 105 }, { 9.5, 0.5, 71, 85 }, { 10, 2, 69, 95 }, { 12, 1, 67, 90 }, { 13, 1, 64, 88 },
            { 14, 1.9, 71, 100 } } },

        { "Synthwave", 104,
          { { 57, 60, 64, 69 }, { 57, 60, 65, 69 }, { 55, 60, 64, 67 }, { 55, 59, 62, 67 } }, { 57, 53, 48, 55 },
          { { 0, 0.4, 0, 115 }, { 0.5, 0.4, 0, 85 }, { 1, 0.4, 12, 100 }, { 1.5, 0.4, 0, 85 }, { 2, 0.4, 0, 110 }, { 2.5, 0.4, 0, 85 },
            { 3, 0.4, 12, 100 }, { 3.5, 0.4, 0, 85 } },
          { { 0, 1.9, 0, 100 }, { 2, 1.9, 0, 90 } }, 0.25,
          { { 0, 0.5, 69, 100 }, { 0.5, 0.5, 72, 90 }, { 1, 1, 76, 105 }, { 2, 1, 74, 95 }, { 3, 1, 72, 95 }, { 4, 1.5, 72, 100 },
            { 5.5, 0.5, 69, 85 }, { 6, 2, 65, 95 }, { 8, 0.5, 67, 95 }, { 8.5, 0.5, 72, 95 }, { 9, 1, 76, 105 }, { 10, 1, 79, 110 },
            { 11, 1, 76, 95 }, { 12, 1.5, 74, 100 }, { 13.5, 0.5, 72, 90 }, { 14, 1.9, 71, 100 } } },

        { "Lofi", 78,
          { { 53, 57, 60, 64 }, { 53, 57, 59, 64 }, { 52, 55, 59, 62 }, { 55, 58, 61, 64 } }, { 50, 55, 48, 57 },
          { { 0, 1.4, 0, 110 }, { 1.67, 0.3, 0, 80 }, { 2, 0.9, 0, 100 }, { 3.67, 0.3, 7, 85 } },
          { { 0, 0.9, 0, 95 }, { 1.67, 0.3, 0, 70 }, { 2, 0.9, 0, 88 }, { 3.67, 0.3, 0, 72 } }, 0.5,
          { { 0, 1, 69, 90 }, { 1, 0.5, 67, 80 }, { 1.5, 1.5, 65, 85 }, { 4, 0.5, 64, 85 }, { 4.5, 0.5, 65, 80 }, { 5, 2, 67, 90 },
            { 8, 1, 71, 95 }, { 9, 1, 69, 85 }, { 10, 2, 67, 90 }, { 12, 0.66, 64, 85 }, { 12.67, 0.66, 65, 80 },
            { 13.33, 0.66, 67, 85 }, { 14, 1.9, 69, 95 } } },

        { "Dark", 140,
          { { 55, 60, 63, 67 }, { 56, 60, 63, 68 }, { 55, 58, 63, 67 }, { 53, 58, 62, 65 } }, { 48, 56, 51, 58 },
          { { 0, 0.5, 0, 120 }, { 0.75, 0.25, 0, 95 }, { 1.5, 0.25, 0, 100 }, { 2.5, 0.5, 0, 110 }, { 3.25, 0.25, 12, 95 }, { 3.5, 0.4, 0, 100 } },
          { { 0, 0.25, 0, 110 }, { 0.75, 0.25, 0, 90 }, { 1.5, 0.25, 0, 95 }, { 3, 0.9, 0, 100 } }, 0.25,
          { { 0, 0.75, 72, 100 }, { 0.75, 0.75, 75, 95 }, { 1.5, 2.4, 79, 110 }, { 4, 0.5, 80, 100 }, { 4.5, 0.5, 79, 90 },
            { 5, 1, 75, 95 }, { 6, 1.9, 72, 100 }, { 8, 0.75, 70, 95 }, { 8.75, 0.75, 74, 95 }, { 9.5, 2.4, 75, 105 },
            { 12, 1, 74, 100 }, { 13, 1, 70, 90 }, { 14, 1.9, 67, 100 } } },

        { "Sunrise", 124,
          { { 55, 60, 64, 67 }, { 55, 59, 62, 67 }, { 57, 60, 64, 69 }, { 57, 60, 65, 69 } }, { 48, 55, 57, 53 },
          { { 0.5, 0.4, 0, 110 }, { 1.5, 0.4, 12, 105 }, { 2.5, 0.4, 0, 110 }, { 3.5, 0.4, 12, 105 } },
          { { 0.5, 0.3, 0, 105 }, { 1.5, 0.3, 0, 95 }, { 2.5, 0.3, 0, 105 }, { 3.5, 0.3, 0, 95 } }, 0.25,
          { { 0, 0.5, 76, 100 }, { 0.5, 0.5, 79, 95 }, { 1, 0.5, 76, 90 }, { 1.5, 1, 72, 95 }, { 2.5, 1.4, 74, 100 },
            { 4, 0.5, 74, 95 }, { 4.5, 0.5, 79, 100 }, { 5, 1, 74, 90 }, { 6, 1.9, 71, 95 }, { 8, 0.5, 72, 95 },
            { 8.5, 0.5, 76, 100 }, { 9, 1, 81, 110 }, { 10, 1.9, 79, 100 }, { 12, 0.5, 77, 95 }, { 12.5, 0.5, 76, 90 },
            { 13, 1, 74, 95 }, { 14, 1.9, 72, 100 } } },
    };
    return s;
}

inline int numSongs() { return (int) songs().size(); }

inline Phrase makePhrase (PhraseType t, int songIndex)
{
    const auto& s = songs()[(size_t) juce::jlimit (0, numSongs() - 1, songIndex)];
    Phrase p;
    for (int c = 0; c < 4; ++c)
    {
        const double b = c * 4.0;
        switch (t)
        {
            case phChords:
                for (int i = 0; i < 4; ++i) p.events.push_back ({ b, 3.9, s.chords[c][i], 92 - i * 4 });
                break;
            case phComp:
                for (auto& h : s.comp)
                    for (int i = 0; i < 4; ++i) p.events.push_back ({ b + h.start, h.length, s.chords[c][i], h.vel });
                break;
            case phBass:
                for (auto& h : s.bass) p.events.push_back ({ b + h.start, h.length, s.roots[c] + h.note, h.vel });
                break;
            case phPluck:
            {
                const int pattern[8] = { 0, 1, 2, 3, 4, 3, 2, 1 };
                const int steps = (int) (4.0 / s.pluckStep);
                for (int k = 0; k < steps; ++k)
                {
                    const int idx = pattern[k % 8];
                    const int note = idx == 4 ? s.chords[c][0] + 12 : s.chords[c][idx];
                    p.events.push_back ({ b + k * s.pluckStep, s.pluckStep * 0.9, note, k % 4 == 0 ? 110 : 82 });
                }
                break;
            }
            case phDrone:
                if (c % 2 == 0)
                    for (int n : { s.roots[c] - 12, s.roots[c] - 5, s.roots[c] + 4 })
                        p.events.push_back ({ b, 7.9, n, 88 });
                break;
            case phDrums:
            {
                const double hits[] = { 0, 0.75, 1.5, 2, 2.5, 3, 3.5, 3.75 };
                const int vels[] = { 120, 80, 95, 115, 85, 100, 90, 70 };
                for (int k = 0; k < 8; ++k) p.events.push_back ({ b + hits[k], 0.2, 48, vels[k] });
                break;
            }
            default: break;
        }
    }
    if (t == phLead) p.events = s.lead;
    return p;
}

inline const Phrase& phrase (PhraseType t, int songIndex = 0)
{
    static const std::vector<Phrase> all = []
    {
        std::vector<Phrase> v;
        for (int s = 0; s < numSongs(); ++s)
            for (int t2 = 0; t2 < numPhraseTypes; ++t2)
                v.push_back (makePhrase ((PhraseType) t2, s));
        return v;
    }();
    const int s = juce::jlimit (0, numSongs() - 1, songIndex);
    return all[(size_t) (s * numPhraseTypes + juce::jlimit (0, (int) numPhraseTypes - 1, (int) t))];
}

// Picks a song for a preset so neighbouring presets don't all play the same tune.
inline int songForPreset (const juce::String& name, const juce::String& category)
{
    if (category == "Drones" || category == "Textures") return 0;
    return (int) ((juce::uint32) name.hashCode() % (juce::uint32) numSongs());
}

// Loops a phrase in real time, writing MIDI into the processing buffer.
class PhrasePlayer
{
public:
    std::atomic<bool> enabled { false };
    std::atomic<int> type { phChords }, song { 0 };

    void prepare (double sampleRate) { sr = sampleRate; pos = 0; }
    double songTempo() const { return songs()[(size_t) juce::jlimit (0, numSongs() - 1, song.load())].bpm; }

    void process (juce::MidiBuffer& midi, int numSamples, double bpm)
    {
        const bool en = enabled.load();
        const int t = type.load() * 16 + song.load();
        if (! en || t != lastType)
        {
            for (int n : sounding) midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
            sounding.clear();
            pos = 0;
            lastType = t;
            if (! en) return;
        }

        const auto& ph = phrase ((PhraseType) type.load(), song.load());
        const double bps = bpm / 60.0 / sr; // beats per sample
        const double a = pos, b = pos + numSamples * bps;
        if (b <= ph.loopBeats)
        {
            emit (midi, ph, a, b, bps, 0.0, numSamples);
            pos = b;
        }
        else
        {
            emit (midi, ph, a, ph.loopBeats, bps, 0.0, numSamples);
            emit (midi, ph, 0.0, b - ph.loopBeats, bps, (ph.loopBeats - a) / bps, numSamples);
            pos = b - ph.loopBeats;
        }
    }

private:
    void emit (juce::MidiBuffer& midi, const Phrase& ph, double a, double b, double bps, double base, int n)
    {
        auto at = [&] (double beat) { return juce::jlimit (0, n - 1, (int) (base + (beat - a) / bps)); };
        for (const auto& e : ph.events)
        {
            double end = e.start + e.length;
            if (end >= ph.loopBeats) end -= ph.loopBeats;
            if (end >= a && end < b && sounding.contains (e.note))
            {
                midi.addEvent (juce::MidiMessage::noteOff (1, e.note), at (end));
                sounding.removeFirstMatchingValue (e.note);
            }
        }
        for (const auto& e : ph.events)
            if (e.start >= a && e.start < b)
            {
                midi.addEvent (juce::MidiMessage::noteOn (1, e.note, (juce::uint8) e.vel), at (e.start));
                sounding.addIfNotAlreadyThere (e.note);
            }
    }

    double sr = 44100.0, pos = 0.0;
    int lastType = -1;
    juce::Array<int> sounding;
};
} // namespace neb
