// MORPH GEN: your sax morphs into a synth whose sound depends on the frequency you play.
//
// The sax range is split into four frequency ZONES, each with its own synth character.
//  * MORPH: a voice that follows your pitch and breath, crossfading smoothly between zone
//    sounds as you move across the splits. The MORPH knob blends sax -> synth, and
//    PITCH->MORPH makes high (or low) notes more synthetic than the rest.
//  * BLOOM: hold a note and the zone it falls in triggers a generative pattern in that
//    zone's sound, built from the Harmony key and scale around the note you played.
#pragma once
#include "Voice.h"
#include "Params.h"
#include <atomic>

namespace rs
{
struct GenParams
{
    bool on = false;
    float split[3] { 220.0f, 370.0f, 620.0f };
    int sound[4] { 3, 1, 0, 4 };
    float morph = 0.5f, pitchMorph = 0.0f, bloom = 0.6f;
    int pattern = 0, rate = 3;
    float length = 4.0f, density = 0.75f, hold = 120.0f;
    int range = 2;
    int keyConcert = 0, scale = 0;
};

class Generator
{
public:
    static constexpr int numVoices = 10;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& v : morphVoice) v.prepare (sr);
        for (auto& v : voices) v.prepare (sr);
        env.setup (4.0f, 90.0f, sr);
        reset();
    }

    void reset()
    {
        for (auto& v : morphVoice) v.reset();
        for (auto& v : voices) v.reset();
        env.reset();
        gate = 0; xf = 1; curZone = 0; prevSound = curSound = 0; freqSm = 220;
        candidate = lastTriggered = -1; stableSamples = 0; unvoicedSamples = 0;
        bloomLeft = 0; stepCountdown = 0; step = 0;
    }

    static int zoneOf (float hz, const float* split)
    {
        float s[3] = { split[0], split[1], split[2] };
        std::sort (s, s + 3);
        int z = 0;
        while (z < 3 && hz >= s[z]) ++z;
        return z;
    }

    /** Mono stage: morph the sax into the zone voice. */
    void processMorph (float* mono, int n, bool voiced, float concertMidi, const GenParams& p, Fade& fade)
    {
        const float target = midiToHz (concertMidi);
        const int zone = zoneOf (target, p.split);
        if (voiced && zone != curZone)
        {
            prevSound = curSound;
            curZone = zone;
            xf = 0.0f;
            std::swap (morphVoice[0], morphVoice[1]);   // old voice keeps ringing while we fade
        }
        curSound = p.sound[curZone];
        meters.zone.store (curZone);

        const float pos = clamp01 (std::log2 (std::max (target, 50.0f) / 110.0f) / std::log2 (10.0f));
        const float m = clamp01 (p.morph + p.pitchMorph * (pos - 0.5f) * 2.0f);
        meters.morphAmount.store (m);
        const float gateT = voiced ? 1.0f : 0.0f;
        const float gc = msToCoeff (8.0f, sr), fc = msToCoeff (12.0f, sr), xc = 1.0f / (0.15f * (float) sr);

        for (int i = 0; i < n; ++i)
        {
            const float x = mono[i];
            const float e = env.process (x);
            gate += (gateT - gate) * gc;
            if (voiced) freqSm += (target - freqSm) * fc;
            xf = std::min (1.0f, xf + xc);
            const float a = e * gate * 1.6f;
            float v = morphVoice[0].renderContinuous (freqSm, a, (GenSound) curSound) * xf;
            if (xf < 1.0f) v += morphVoice[1].renderContinuous (freqSm, a, (GenSound) prevSound) * (1.0f - xf);
            const float y = x + m * (v - x);
            const float f = fade.next();
            mono[i] = x + f * (y - x);
        }
    }

    /** Stereo stage: generative bloom voices, added on top. */
    void processBloom (float* L, float* R, int n, bool voiced, float concertMidi, const GenParams& p, double bpm, bool enabled)
    {
        // --- trigger: a note held steady for HOLD ms starts a bloom in its zone's sound
        if (enabled && voiced)
        {
            unvoicedSamples = 0;
            const int note = (int) std::lround (concertMidi);
            if (note != candidate) { candidate = note; stableSamples = 0; }
            stableSamples += n;
            if (stableSamples >= (int) (p.hold * 0.001f * (float) sr) && note != lastTriggered)
                trigger (note, p);
        }
        else
        {
            unvoicedSamples += n;
            if (unvoicedSamples > (int) (0.12 * sr)) { lastTriggered = -1; candidate = -1; }
        }
        if (manualFire.exchange (false) && enabled) trigger (lastRoot >= 0 ? lastRoot : (int) std::lround (concertMidi), p);

        static const double beats[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
        const int stepLen = std::max (64, (int) (60.0 / juce::jlimit (30.0, 300.0, bpm) * beats[juce::jlimit (0, 5, p.rate)] * sr));

        float level = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            if (bloomLeft > 0 && enabled)
            {
                --bloomLeft;
                if (--stepCountdown <= 0) { doStep (p, stepLen); stepCountdown = stepLen; }
            }
            float l = 0, r = 0;
            for (auto& v : voices) v.renderAdd (l, r);
            const float g = 0.32f * p.bloom;
            L[i] += l * g;
            R[i] += r * g;
            level = std::max (level, std::abs (l * g));
        }
        meters.bloomLevel.store (level);
        meters.blooming.store (bloomLeft > 0);
    }

    struct Meters
    {
        std::atomic<int> zone { 0 }, triggers { 0 }, bloomZone { 0 };
        std::atomic<float> morphAmount { 0 }, bloomLevel { 0 };
        std::atomic<bool> blooming { false };
    } meters;

    std::atomic<bool> manualFire { false };

private:
    void trigger (int note, const GenParams& p)
    {
        lastTriggered = note;
        lastRoot = note;
        root = note;
        bloomZone = zoneOf (midiToHz ((float) note), p.split);
        bloomSound = p.sound[bloomZone];
        bloomLeft = (int) (p.length * (float) sr);
        stepCountdown = 0;
        step = 0;
        walk = 0;
        meters.bloomZone.store (bloomZone);
        meters.triggers.fetch_add (1);
    }

    /** Scale-degree offset from the root -> MIDI note (concert). */
    float degreeNote (int deg, const GenParams& p) const
    {
        const auto& steps = params::scaleSteps (p.scale);
        const int n = (int) steps.size();
        const int rel = ((root - p.keyConcert) % 12 + 12) % 12;
        int idx = 0;
        for (int i = 0; i < n; ++i) if (steps[(size_t) i] <= rel) idx = i;
        const int anchor = root - rel;
        const int t = idx + deg;
        const int oct = t >= 0 ? t / n : -((-t + n - 1) / n);
        return (float) (anchor + steps[(size_t) (t - oct * n)] + 12 * oct);
    }

    void play (int deg, const GenParams& p, int stepLen, float vel)
    {
        const auto snd = (GenSound) bloomSound;
        float midi = degreeNote (deg, p);
        if (snd == GenSound::Drone) midi -= 12.0f;
        while (midi > 100.0f) midi -= 12.0f;
        const bool sustained = snd == GenSound::Pad || snd == GenSound::Drone || snd == GenSound::Choir;
        const int len = sustained ? stepLen * 3 : stepLen / 2;

        SynthVoice* target = nullptr;
        for (auto& v : voices) if (! v.active) { target = &v; break; }
        if (target == nullptr)
        {
            target = &voices[0];
            for (auto& v : voices) if (v.age > target->age) target = &v;
        }
        target->start (snd, midi, vel, rng.next() * 0.75f, len);
    }

    void doStep (const GenParams& p, int stepLen)
    {
        const int n = (int) params::scaleSteps (p.scale).size();
        const int chordLen = n == 7 ? 4 : 3;
        auto chordDeg = [&] (int i) { return (n == 7 ? 2 : 1) * (i % chordLen) + n * (i / chordLen); };
        const int span = chordLen * std::max (1, p.range);
        const float vel = 0.55f + 0.45f * (0.5f + 0.5f * rng.next());
        const bool hit = 0.5f + 0.5f * rng.next() < p.density || step == 0;

        switch (p.pattern)
        {
            case 0:     // arpeggio up through the chord tones
                if (hit) play (chordDeg (step % span), p, stepLen, vel);
                break;
            case 1:     // random walk on the scale
            {
                static const int moves[] = { -2, -1, 1, 1, 2, 3 };
                if (step > 0) walk += moves[(int) ((0.5f + 0.5f * rng.next()) * 5.99f)];
                walk = juce::jlimit (-2, n * std::max (1, p.range), walk);
                if (hit) play (walk, p, stepLen, vel);
                break;
            }
            case 2:     // chord swells with sparkles on top
                if (step % 4 == 0)
                {
                    const int inv = (step / 4) % 3;
                    for (int i = 0; i < chordLen; ++i) play (chordDeg (i) + (i < inv ? n : 0), p, stepLen * 2, vel * 0.8f);
                }
                else if (hit && rng.next() > 0.3f) play (chordDeg (chordLen + (int) ((0.5f + 0.5f * rng.next()) * (float) chordLen)), p, stepLen, vel * 0.7f);
                break;
            default:    // cascade down from the top
                if (hit) play (chordDeg (span - 1 - (step % span)), p, stepLen, vel);
                break;
        }
        ++step;
    }

    double sr = 48000.0;
    SynthVoice morphVoice[2];
    std::array<SynthVoice, numVoices> voices;
    EnvFollower env;
    float gate = 0, xf = 1, freqSm = 220;
    int curZone = 0, curSound = 0, prevSound = 0;
    int candidate = -1, lastTriggered = -1, lastRoot = -1, stableSamples = 0, unvoicedSamples = 0;
    int root = 60, bloomZone = 0, bloomSound = 0, bloomLeft = 0, stepCountdown = 0, step = 0, walk = 0;
    Noise rng;
};
} // namespace rs
