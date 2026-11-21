#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Params.h"

namespace neb
{
class Arpeggiator
{
public:
    struct Settings
    {
        bool on = false;
        int mode = arpUp;
        double stepBeats = 0.25;
        int octaves = 1;
        float gate = 0.8f, swing = 0.0f;
        bool latch = false;
    };

    std::atomic<int> displayStep { -1 };

    void prepare (double sampleRate) { sr = sampleRate; resetState(); }

    void process (juce::MidiBuffer& midi, int numSamples, const Settings& s,
                  double bpm, bool hostPlaying, double ppq)
    {
        if (! s.on)
        {
            if (wasOn)
            {
                juce::MidiBuffer out;
                releaseSounding (out, 0);
                for (const auto m : midi) out.addEvent (m.getMessage(), m.samplePosition);
                midi.swapWith (out);
                resetState();
            }
            return;
        }

        juce::MidiBuffer out;
        if (! wasOn) { resetState(); wasOn = true; }

        for (const auto m : midi)
        {
            const auto msg = m.getMessage();
            if (msg.isNoteOn())
            {
                if (s.latch && physicalDown == 0) notes.clear();
                notes.addIfNotAlreadyThere (msg.getNoteNumber());
                velocities[msg.getNoteNumber()] = msg.getVelocity();
                ++physicalDown;
            }
            else if (msg.isNoteOff())
            {
                physicalDown = juce::jmax (0, physicalDown - 1);
                if (! s.latch) notes.removeFirstMatchingValue (msg.getNoteNumber());
            }
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                notes.clear(); physicalDown = 0;
                out.addEvent (msg, m.samplePosition);
            }
            else
            {
                out.addEvent (msg, m.samplePosition);
            }
        }
        if (! s.latch && physicalDown == 0) notes.clear();

        if (notes.isEmpty())
        {
            releaseSounding (out, 0);
            toNext = 0; stepCount = 0; seqIndex = -1; dir = 1;
            displayStep = -1;
            midi.swapWith (out);
            return;
        }

        const double samplesPerBeat = 60.0 / juce::jmax (20.0, bpm) * sr;
        const double stepLen = s.stepBeats * samplesPerBeat;
        auto swingOff = [&] (long k) { return (k & 1) ? (double) s.swing * 0.33 * stepLen : 0.0; };

        if (hostPlaying)
        {
            long k = (long) std::ceil (ppq / s.stepBeats - 1.0e-6);
            const long kp = k - 1;
            if (s.swing > 0 && (kp & 1) && kp > lastFired && kp * s.stepBeats + swingOff (kp) / samplesPerBeat >= ppq)
                k = kp;
            toNext = (k * s.stepBeats - ppq) * samplesPerBeat + swingOff (k);
            stepCount = k;
        }

        for (int i = 0; i < numSamples; ++i)
        {
            if (! sounding.isEmpty())
            {
                offCountdown -= 1.0;
                if (offCountdown <= 0.0) releaseSounding (out, i);
            }
            if (toNext <= 0.0)
            {
                releaseSounding (out, i);
                fireStep (out, i, s);
                lastFired = stepCount;
                offCountdown = stepLen * s.gate;
                toNext += stepLen + swingOff (stepCount + 1) - swingOff (stepCount);
                ++stepCount;
            }
            toNext -= 1.0;
        }
        midi.swapWith (out);
    }

private:
    void resetState()
    {
        notes.clear(); sounding.clear();
        physicalDown = 0; toNext = 0; stepCount = 0; lastFired = -1; seqIndex = -1; dir = 1;
        wasOn = false; displayStep = -1;
    }

    void releaseSounding (juce::MidiBuffer& out, int pos)
    {
        for (int n : sounding) out.addEvent (juce::MidiMessage::noteOff (1, n), pos);
        sounding.clear();
    }

    void fireStep (juce::MidiBuffer& out, int pos, const Settings& s)
    {
        juce::Array<int> base = notes;
        if (s.mode != arpAsPlayed) base.sort();
        const int octs = juce::jlimit (1, 4, s.octaves);

        if (s.mode == arpChord)
        {
            const int oct = (int) (stepCount % octs);
            for (int n : base)
            {
                const int nn = n + 12 * oct;
                if (nn > 127) continue;
                out.addEvent (juce::MidiMessage::noteOn (1, nn, (juce::uint8) velocities[n]), pos);
                sounding.add (nn);
            }
        }
        else
        {
            juce::Array<int> seq;
            for (int o = 0; o < octs; ++o)
                for (int n : base)
                    if (n + 12 * o <= 127) seq.add (n + 12 * o);
            const int N = seq.size();
            if (N == 0) return;

            switch (s.mode)
            {
                case arpDown:   seqIndex = (seqIndex <= 0 || seqIndex >= N) ? N - 1 : seqIndex - 1; break;
                case arpUpDown:
                    if (N == 1) { seqIndex = 0; break; }
                    seqIndex += dir;
                    if (seqIndex >= N) { dir = -1; seqIndex = N - 2; }
                    if (seqIndex < 0)  { dir = 1; seqIndex = juce::jmin (1, N - 1); }
                    break;
                case arpRandom: seqIndex = rng.nextInt (N); break;
                default:        seqIndex = (seqIndex + 1) % N; break;
            }
            seqIndex = juce::jlimit (0, N - 1, seqIndex);
            const int nn = seq[seqIndex];
            const int baseNote = base[seqIndex % base.size()];
            out.addEvent (juce::MidiMessage::noteOn (1, nn, (juce::uint8) juce::jmax (1, velocities[baseNote])), pos);
            sounding.add (nn);
        }
        displayStep = (int) (stepCount % 16);
    }

    double sr = 44100.0;
    juce::Array<int> notes, sounding;
    int velocities[128] {};
    int physicalDown = 0, seqIndex = -1, dir = 1;
    double toNext = 0, offCountdown = 0;
    long stepCount = 0, lastFired = -1;
    bool wasOn = false;
    juce::Random rng;
};
} // namespace neb
