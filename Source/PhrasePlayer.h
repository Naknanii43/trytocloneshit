#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace neb
{
// Musical phrases used by the Preview button and the demo renderer.
// All phrases follow the same 16-beat progression: Fmaj7 - G6 - Am9 - Em7.
struct PhraseEvent { double start, length; int note, vel; };
struct Phrase { std::vector<PhraseEvent> events; double loopBeats = 16.0; };

enum PhraseType { phChords, phComp, phBass, phLead, phPluck, phDrone, numPhraseTypes };

inline PhraseType phraseTypeForCategory (const juce::String& c)
{
    if (c == "Keys")   return phComp;
    if (c == "Bass")   return phBass;
    if (c == "Leads")  return phLead;
    if (c == "Plucks") return phPluck;
    if (c == "Drones") return phDrone;
    return phChords;
}

inline const Phrase& phrase (PhraseType t)
{
    static const std::vector<Phrase> all = []
    {
        static const int chords[4][4] = { { 53, 57, 60, 64 }, { 55, 59, 62, 64 }, { 57, 60, 64, 71 }, { 52, 55, 59, 62 } };
        static const int roots[4] = { 53, 55, 57, 52 };
        std::vector<Phrase> v ((size_t) numPhraseTypes);

        for (int c = 0; c < 4; ++c)
        {
            const double b = c * 4.0;
            for (int i = 0; i < 4; ++i)
            {
                v[phChords].events.push_back ({ b, 3.9, chords[c][i], 92 - i * 4 });
                // keys: a rhythmic comping pattern
                v[phComp].events.push_back ({ b, 1.4, chords[c][i], 100 });
                v[phComp].events.push_back ({ b + 1.5, 0.9, chords[c][i], 78 });
                v[phComp].events.push_back ({ b + 2.5, 0.4, chords[c][i], 70 });
                v[phComp].events.push_back ({ b + 3.0, 0.9, chords[c][i], 86 });
            }
            const int r = roots[c];
            for (auto e : std::vector<PhraseEvent> { { 0, 0.6, r, 110 }, { 0.75, 0.45, r, 90 }, { 1.5, 0.4, r + 12, 100 },
                                                     { 2, 0.6, r, 105 }, { 3, 0.4, r, 95 }, { 3.5, 0.4, r + 7, 92 } })
                v[phBass].events.push_back ({ b + e.start, e.length, e.note, e.vel });

            const int tones[8] = { 0, 1, 2, 3, 4, 3, 2, 1 };
            for (int s = 0; s < 8; ++s)
            {
                const int idx = tones[s];
                const int note = idx == 4 ? chords[c][0] + 12 : chords[c][idx];
                v[phPluck].events.push_back ({ b + s * 0.5, 0.45, note, s == 0 ? 110 : 80 + (s % 2) * 12 });
            }
        }

        v[phLead].events = { { 0, 1.5, 69, 100 }, { 1.5, 0.5, 67, 85 }, { 2, 1, 65, 90 }, { 3, 1, 64, 90 },
                             { 4, 2, 62, 100 }, { 6, 1, 67, 90 }, { 7, 1, 71, 95 },
                             { 8, 1.5, 72, 105 }, { 9.5, 0.5, 71, 85 }, { 10, 2, 69, 95 },
                             { 12, 1, 67, 90 }, { 13, 1, 64, 88 }, { 14, 1.9, 71, 100 } };

        v[phDrone].events = { { 0, 7.9, 41, 90 }, { 0, 7.9, 48, 80 }, { 0, 7.9, 57, 70 },
                              { 8, 7.9, 45, 90 }, { 8, 7.9, 52, 80 }, { 8, 7.9, 60, 70 } };
        return v;
    }();
    return all[(size_t) juce::jlimit (0, (int) numPhraseTypes - 1, (int) t)];
}

// Loops a phrase in real time, writing MIDI into the processing buffer.
class PhrasePlayer
{
public:
    std::atomic<bool> enabled { false };
    std::atomic<int> type { phChords };

    void prepare (double sampleRate) { sr = sampleRate; pos = 0; }

    void process (juce::MidiBuffer& midi, int numSamples, double bpm)
    {
        const bool en = enabled.load();
        const int t = type.load();
        if (! en || t != lastType)
        {
            for (int n : sounding) midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
            sounding.clear();
            pos = 0;
            lastType = t;
            if (! en) return;
        }

        const auto& ph = phrase ((PhraseType) t);
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
