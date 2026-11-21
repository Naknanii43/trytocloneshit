#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP.h"
#include "Params.h"

namespace neb
{
// Distortion -> EQ -> Phaser -> Chorus -> Delay -> Reverb
class FxChain
{
public:
    void bind (juce::AudioProcessorValueTreeState& s)
    {
        auto g = [&] (const char* id) { auto* a = s.getRawParameterValue (id); jassert (a); return a; };
        distOn = g ("distOn"); distType = g ("distType"); distDrive = g ("distDrive"); distTone = g ("distTone"); distMix = g ("distMix");
        eqLow = g ("eqLow"); eqHigh = g ("eqHigh");
        phOn = g ("phOn"); phRate = g ("phRate"); phDepth = g ("phDepth"); phFb = g ("phFb"); phMix = g ("phMix");
        chOn = g ("chOn"); chRate = g ("chorusRate"); chDepth = g ("chorusDepth"); chMix = g ("chorusMix");
        dlOn = g ("dlOn"); dlTime = g ("delayTime"); dlSync = g ("delaySync"); dlDiv = g ("delayDiv");
        dlFb = g ("delayFb"); dlMix = g ("delayMix"); dlPing = g ("delayPing");
        rvOn = g ("rvOn"); rvSize = g ("revSize"); rvDamp = g ("revDamp"); rvWidth = g ("revWidth"); rvMix = g ("revMix");
    }

    void prepare (double sampleRate, int blockSize)
    {
        sr = sampleRate;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 2 };
        chorus.prepare (spec);
        chorus.setCentreDelay (10.0f);
        chorus.setFeedback (0.1f);
        phaser.prepare (spec);
        phaser.setCentreFrequency (900.0f);
        reverb.setSampleRate (sampleRate);
        reverb.reset();
        delayBuf.setSize (2, (int) (sampleRate * 4.1) + 1);
        delayBuf.clear();
        delayWrite = 0;
        dlLp[0] = dlLp[1] = 0;
        delaySamples.reset (sampleRate, 0.2);
        delaySamples.setCurrentAndTargetValue ((float) (0.375 * sampleRate));
        for (auto& b : eq) b.reset();
        lastLow = lastHigh = -100.0f;
        toneState[0] = toneState[1] = 0;
        crushHold[0] = crushHold[1] = 0;
        crushCount = 0;
    }

    void process (juce::AudioBuffer<float>& buffer, double bpm)
    {
        const int n = buffer.getNumSamples();
        float* L = buffer.getWritePointer (0);
        float* R = buffer.getWritePointer (1);

        // Distortion
        if (distOn->load() > 0.5f)
        {
            const int type = (int) distType->load();
            const float drive = 1.0f + distDrive->load() * 24.0f;
            const float mix = distMix->load();
            const float toneCoef = std::exp (-2.0f * (float) pi * (500.0f + 15000.0f * distTone->load()) / (float) sr);
            const float bits = std::pow (2.0f, 16.0f - 13.0f * distDrive->load());
            const int hold = 1 + (int) (distDrive->load() * 20.0f);
            for (int i = 0; i < n; ++i)
            {
                ++crushCount;
                for (int c = 0; c < 2; ++c)
                {
                    float* d = c == 0 ? L : R;
                    const float x = d[i];
                    float y;
                    switch (type)
                    {
                        case 0:  y = std::tanh (x * drive) / std::tanh (juce::jmin (drive, 3.0f)); break;
                        case 1:  y = juce::jlimit (-0.8f, 0.8f, x * drive); break;
                        case 2:  y = std::sin (x * drive * 1.5f) * 0.8f; break;
                        default:
                            if (crushCount % hold == 0) crushHold[c] = std::round (x * bits) / bits;
                            y = crushHold[c];
                            break;
                    }
                    toneState[c] = y + (toneState[c] - y) * toneCoef;
                    d[i] = x + (toneState[c] - x) * mix;
                }
            }
        }

        // EQ
        const float lo = eqLow->load(), hi = eqHigh->load();
        if (lo != lastLow || hi != lastHigh)
        {
            eq[0].shelf (false, sr, 220.0, lo); eq[1].shelf (false, sr, 220.0, lo);
            eq[2].shelf (true, sr, 4500.0, hi); eq[3].shelf (true, sr, 4500.0, hi);
            lastLow = lo; lastHigh = hi;
        }
        if (std::abs (lo) > 0.05f || std::abs (hi) > 0.05f)
            for (int i = 0; i < n; ++i)
            {
                L[i] = eq[2].process (eq[0].process (L[i]));
                R[i] = eq[3].process (eq[1].process (R[i]));
            }

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);

        if (phOn->load() > 0.5f)
        {
            phaser.setRate (phRate->load());
            phaser.setDepth (phDepth->load());
            phaser.setFeedback (phFb->load());
            phaser.setMix (phMix->load());
            phaser.process (ctx);
        }

        if (chOn->load() > 0.5f)
        {
            chorus.setRate (chRate->load());
            chorus.setDepth (chDepth->load());
            chorus.setMix (chMix->load() * 0.5f);
            chorus.process (ctx);
        }

        if (dlOn->load() > 0.5f) processDelay (buffer, bpm);

        if (rvOn->load() > 0.5f)
        {
            juce::Reverb::Parameters rp;
            const float mix = rvMix->load();
            rp.roomSize = rvSize->load();
            rp.damping = rvDamp->load();
            rp.width = rvWidth->load();
            rp.wetLevel = mix;
            rp.dryLevel = 1.0f - mix * 0.5f;
            reverb.setParameters (rp);
            reverb.processStereo (L, R, n);
        }
    }

private:
    void processDelay (juce::AudioBuffer<float>& buffer, double bpm)
    {
        const float mix = dlMix->load();
        const float fb = dlFb->load();
        const bool ping = dlPing->load() > 0.5f;
        double secs = dlTime->load() * 0.001;
        if (dlSync->load() > 0.5f) secs = divisionBeats ((int) dlDiv->load()) * 60.0 / juce::jmax (20.0, bpm);
        const int size = delayBuf.getNumSamples();
        delaySamples.setTargetValue ((float) juce::jlimit (1.0, (double) size - 2, secs * sr));

        auto* dl = delayBuf.getWritePointer (0);
        auto* dr = delayBuf.getWritePointer (1);
        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getWritePointer (1);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float d = delaySamples.getNextValue();
            float rp = (float) delayWrite - d;
            while (rp < 0) rp += (float) size;
            const int i0 = (int) rp;
            const int i1 = (i0 + 1) % size;
            const float fr = rp - (float) i0;
            const float wl = dl[i0] + (dl[i1] - dl[i0]) * fr;
            const float wr = dr[i0] + (dr[i1] - dr[i0]) * fr;
            dlLp[0] += (wl - dlLp[0]) * 0.35f;
            dlLp[1] += (wr - dlLp[1]) * 0.35f;

            if (ping)
            {
                const float in = 0.5f * (l[i] + r[i]);
                dl[delayWrite] = in + dlLp[1] * fb;
                dr[delayWrite] = dlLp[0] * fb;
            }
            else
            {
                dl[delayWrite] = l[i] + dlLp[0] * fb;
                dr[delayWrite] = r[i] + dlLp[1] * fb;
            }
            l[i] += wl * mix;
            r[i] += wr * mix;
            if (++delayWrite >= size) delayWrite = 0;
        }
    }

    using Atom = std::atomic<float>*;
    Atom distOn{}, distType{}, distDrive{}, distTone{}, distMix{}, eqLow{}, eqHigh{},
         phOn{}, phRate{}, phDepth{}, phFb{}, phMix{}, chOn{}, chRate{}, chDepth{}, chMix{},
         dlOn{}, dlTime{}, dlSync{}, dlDiv{}, dlFb{}, dlMix{}, dlPing{},
         rvOn{}, rvSize{}, rvDamp{}, rvWidth{}, rvMix{};

    double sr = 44100.0;
    juce::dsp::Chorus<float> chorus;
    juce::dsp::Phaser<float> phaser;
    juce::Reverb reverb;
    Biquad eq[4];
    float lastLow = -100.0f, lastHigh = -100.0f;
    float toneState[2] {}, crushHold[2] {};
    long crushCount = 0;
    juce::AudioBuffer<float> delayBuf;
    int delayWrite = 0;
    float dlLp[2] {};
    juce::SmoothedValue<float> delaySamples;
};
} // namespace neb
