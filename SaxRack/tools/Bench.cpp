// ReedstormBench: CPU profile per module + reliability stress test.
//
//   ReedstormBench [--quick]
//
// 1. CPU: every module on its own, everything at once, and every preset, at 48 kHz with
//    64- and 512-sample blocks. Reports average % of one core and the worst single block
//    (a real-time glitch happens when one block takes longer than its duration).
// 2. Stress: random automation of every parameter (including looper footswitches and preset
//    changes), random block sizes 1..4096, all common sample rates, mono/stereo inputs,
//    silence / noise / full-scale square / sax input. Fails on NaN/Inf or overs.
// 3. Denormal check: loud input then long silence with everything on; silence must not get slower.
#include "../Source/PluginProcessor.h"
#include <chrono>
#include <random>

#include <time.h>
using Clock = std::chrono::steady_clock;

/** CPU time of this thread only: not disturbed by the OS running other things in between. */
static double threadSeconds()
{
    timespec ts {};
    clock_gettime (CLOCK_THREAD_CPUTIME_ID, &ts);
    return (double) ts.tv_sec + (double) ts.tv_nsec * 1e-9;
}

namespace
{
juce::AudioBuffer<float> makeSax (double sr, double seconds)
{
    const int n = (int) (sr * seconds);
    juce::AudioBuffer<float> b (1, n);
    const float notes[] = { 58, 61, 63, 65, 68, 70, 72, 70, 65, 63, 61, 58, 77, 75, 73, 70 };
    rs::Noise nz;
    rs::Svf lp; lp.set (3500.0f, 0.7f, sr);
    double ph = 0;
    for (int i = 0; i < n; ++i)
    {
        const double t = (double) i / sr;
        const int idx = (int) (t / 0.45);
        const double inNote = t - idx * 0.45;
        const float env = inNote < 0.4 ? (float) std::min (1.0, inNote / 0.03) : 0.0f;
        const float f = rs::midiToHz (notes[idx % 16] + 0.1f * (float) std::sin (6.28 * 5.3 * t));
        ph += f / sr; ph -= std::floor (ph);
        const float saw = (float) (2.0 * ph - 1.0);
        b.setSample (0, i, (lp.lp (saw) * 0.8f + nz.next() * 0.03f) * env * 0.3f);
    }
    return b;
}

void setParam (ReedstormProcessor& p, const juce::String& id, float realValue)
{
    if (auto* prm = p.apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (realValue));
}

const char* moduleIds[] = { "dyn_on", "oct_on", "syn_on", "wah_on", "drv_on", "rng_on", "eq_on", "gen_on", "whm_on", "fmt_on",
                            "tlk_on", "grm_on", "frz_on", "cld_on", "har_on", "dbl_on", "mod_on", "trm_on", "gat_on", "brp_on",
                            "dly_on", "spr_on", "rev_on" };

void allOff (ReedstormProcessor& p)
{
    p.loadPreset (0);
    for (auto* id : moduleIds) setParam (p, id, 0.0f);
}

void everythingOn (ReedstormProcessor& p)
{
    for (auto* id : moduleIds) setParam (p, id, 1.0f);
    setParam (p, "gen_bloom", 1.0f); setParam (p, "gen_morph", 0.6f);
    setParam (p, "grm_chance", 1.0f); setParam (p, "grm_swarm", 1.0f); setParam (p, "grm_harsh", 0.5f);
    setParam (p, "frz_mode", 1); setParam (p, "cld_density", 1.0f); setParam (p, "brp_mode", 1); setParam (p, "brp_chance", 1.0f);
    setParam (p, "har_lvl2", 0.5f); setParam (p, "dbl_players", 2); setParam (p, "rev_shimmer", 0.5f);
    setParam (p, "drv_type", 1);
}

struct Result { double avgPct = 0, worstBlockPct = 0, worstAt = 0; bool finite = true; float peak = 0; };

Result run (ReedstormProcessor& p, const juce::AudioBuffer<float>& in, double sr, int block, bool loopsToo = false)
{
    p.setPlayConfigDetails (2, 2, sr, block);
    p.prepareToPlay (sr, block);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    Result r;
    double total = 0;
    const int n = in.getNumSamples();
    int blocks = 0;
    for (int pos = 0; pos + block <= n; pos += block, ++blocks)
    {
        buf.copyFrom (0, 0, in, 0, pos, block);
        buf.copyFrom (1, 0, in, 0, pos, block);
        if (loopsToo)
        {
            const int b = pos / block;
            const int span = (int) (sr * 1.5) / block;
            for (int t = 0; t < 4; ++t)
                if (b == (t + 1) * span || b == (t + 2) * span) p.looper().post (t, rs::LoopTrack::Tap);
        }
        const double t0 = threadSeconds();
        p.processBlock (buf, midi);
        const double dt = threadSeconds() - t0;
        if (pos > n / 10)   // skip warm-up
        {
            total += dt;
            const double pct = 100.0 * dt / ((double) block / sr);
            if (pct > r.worstBlockPct) { r.worstBlockPct = pct; r.worstAt = (double) pos / sr; }
        }
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < block; ++i)
            {
                const float v = buf.getSample (c, i);
                if (! std::isfinite (v)) r.finite = false;
                r.peak = std::max (r.peak, std::abs (v));
            }
    }
    r.avgPct = 100.0 * total / ((double) n * 0.9 / sr);
    return r;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const bool quick = argc > 1 && juce::String (argv[1]) == "--quick";
    int failures = 0;
    auto fail = [&] (const juce::String& m) { std::printf ("  FAIL: %s\n", m.toRawUTF8()); ++failures; };
    const double sr = 48000.0;
    const auto sax = makeSax (sr, quick ? 6.0 : 12.0);

    std::printf ("=== CPU per module (48 kHz, %% of one core; worst = slowest single block) ===\n");
    std::printf ("%-14s %10s %10s %10s %10s\n", "module", "avg@64", "worst@64", "avg@512", "worst@512");
    {
        ReedstormProcessor p;
        allOff (p);
        auto a = run (p, sax, sr, 64), b = run (p, sax, sr, 512);
        std::printf ("%-14s %9.2f%% %9.1f%% %9.2f%% %9.1f%%\n", "(all off)", a.avgPct, a.worstBlockPct, b.avgPct, b.worstBlockPct);
    }
    for (auto* id : moduleIds)
    {
        ReedstormProcessor p;
        allOff (p);
        setParam (p, id, 1.0f);
        if (juce::String (id) == "grm_on") { setParam (p, "grm_chance", 1.0f); setParam (p, "grm_swarm", 1.0f); }
        if (juce::String (id) == "gen_on") setParam (p, "gen_bloom", 1.0f);
        if (juce::String (id) == "frz_on") setParam (p, "frz_mode", 1);
        if (juce::String (id) == "brp_on") { setParam (p, "brp_mode", 1); setParam (p, "brp_chance", 1.0f); }
        if (juce::String (id) == "har_on") setParam (p, "har_lvl2", 0.5f);
        auto a = run (p, sax, sr, 64), b = run (p, sax, sr, 512);
        std::printf ("%-14s %9.2f%% %9.1f%% %9.2f%% %9.1f%%\n", juce::String (id).upToFirstOccurrenceOf ("_", false, false).toRawUTF8(),
                     a.avgPct, a.worstBlockPct, b.avgPct, b.worstBlockPct);
        if (! a.finite || ! b.finite) fail (juce::String (id) + " produced NaN/Inf");
    }
    {
        ReedstormProcessor p;
        everythingOn (p);
        auto a = run (p, sax, sr, 64, true), b = run (p, sax, sr, 512, true);
        std::printf ("%-14s %9.2f%% %9.1f%% %9.2f%% %9.1f%%   (worst blocks at %.2f s / %.2f s)\n", "EVERYTHING+4L", a.avgPct, a.worstBlockPct, b.avgPct, b.worstBlockPct, a.worstAt, b.worstAt);
        if (! a.finite || ! b.finite) fail ("everything-on produced NaN/Inf");
        if (a.peak > 1.0f || b.peak > 1.0f) fail ("everything-on exceeded 0 dBFS");
    }

    std::printf ("\n=== Presets (48 kHz, 128-sample blocks) ===\n");
    double worstPreset = 0; juce::String worstName;
    {
        ReedstormProcessor probe;
        for (int i = 0; i < probe.getNumPrograms(); ++i)
        {
            ReedstormProcessor p;
            p.loadPreset (i);
            auto r = run (p, sax, sr, 128);
            if (r.avgPct > worstPreset) { worstPreset = r.avgPct; worstName = p.getProgramName (i); }
            if (! r.finite) fail (p.getProgramName (i) + " NaN/Inf");
            if (r.peak > 1.0f) fail (p.getProgramName (i) + " exceeded 0 dBFS");
        }
        std::printf ("heaviest preset: %s at %.2f%% of one core\n", worstName.toRawUTF8(), worstPreset);
    }

    std::printf ("\n=== Stress: random automation, block sizes, sample rates, inputs ===\n");
    {
        std::mt19937 rng (1234);
        const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
        const int rounds = quick ? 6 : 20;
        long long blocksDone = 0;
        float peak = 0; bool finite = true;
        for (int round = 0; round < rounds; ++round)
        {
            ReedstormProcessor p;
            const double fs = rates[(size_t) round % 5];
            const bool monoIn = round % 3 == 2;
            const int maxBlock = 4096;
            p.setPlayConfigDetails (monoIn ? 1 : 2, 2, fs, maxBlock);
            p.prepareToPlay (fs, maxBlock);
            auto params = p.getParameters();
            const auto src = makeSax (fs, 2.0);
            juce::AudioBuffer<float> buf (2, maxBlock);
            juce::MidiBuffer midi;
            long long t = 0;
            const long long total = (long long) (fs * (quick ? 3.0 : 8.0));
            std::uniform_int_distribution<int> blockDist (1, maxBlock);
            std::uniform_real_distribution<float> u (0.0f, 1.0f);
            while (t < total)
            {
                const int bs = u (rng) < 0.3f ? (int) (1 + u (rng) * 64) : blockDist (rng);
                // automate: a burst of random parameter changes, sometimes a preset change, sometimes a re-prepare
                for (int k = 0; k < 6; ++k) params[(int) (u (rng) * (float) params.size()) % params.size()]->setValueNotifyingHost (u (rng));
                if (u (rng) < 0.01f) p.loadPreset ((int) (u (rng) * (float) p.getNumPrograms()) % p.getNumPrograms());
                if (u (rng) < 0.002f) p.prepareToPlay (fs, maxBlock);
                const int kind = (int) (t / (long long) (fs * 0.7)) % 4;
                buf.setSize (2, bs, false, false, true);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < bs; ++i)
                    {
                        const long long s = t + i;
                        float v = 0;
                        switch (kind)
                        {
                            case 0: v = src.getSample (0, (int) (s % src.getNumSamples())); break;
                            case 1: v = (u (rng) * 2.0f - 1.0f) * 0.8f; break;
                            case 2: v = ((s / 50) % 2 ? 1.0f : -1.0f); break;            // full-scale square
                            default: v = 0.0f; break;
                        }
                        buf.setSample (c, i, v);
                    }
                p.processBlock (buf, midi);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < bs; ++i)
                    {
                        const float v = buf.getSample (c, i);
                        if (! std::isfinite (v)) finite = false;
                        peak = std::max (peak, std::abs (v));
                    }
                t += bs;
                ++blocksDone;
            }
        }
        std::printf ("%lld random blocks across 5 sample rates: %s, peak %.3f\n", blocksDone, finite ? "all finite" : "NaN/Inf FOUND", peak);
        if (! finite) fail ("stress produced NaN/Inf");
        if (peak > 1.0f) fail ("stress exceeded 0 dBFS");
    }

    std::printf ("\n=== Denormal check: loud then 20 s of silence, everything on ===\n");
    {
        ReedstormProcessor p;
        everythingOn (p);
        setParam (p, "frz_on", 0); setParam (p, "loop_on", 0);
        juce::AudioBuffer<float> in (1, (int) (sr * 22.0));
        in.clear();
        in.copyFrom (0, 0, sax, 0, 0, (int) (sr * 2.0));
        const int block = 256;
        p.setPlayConfigDetails (2, 2, sr, block);
        p.prepareToPlay (sr, block);
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        double loudT = 0, quietT = 0; int loudN = 0, quietN = 0;
        for (int pos = 0; pos + block <= in.getNumSamples(); pos += block)
        {
            buf.copyFrom (0, 0, in, 0, pos, block);
            buf.copyFrom (1, 0, in, 0, pos, block);
            const auto t0 = Clock::now();
            p.processBlock (buf, midi);
            const double dt = std::chrono::duration<double> (Clock::now() - t0).count();
            if (pos > sr * 0.5 && pos < sr * 2.0) { loudT += dt; ++loudN; }
            if (pos > sr * 15.0) { quietT += dt; ++quietN; }
        }
        const double ratio = (quietT / quietN) / (loudT / loudN);
        std::printf ("silence costs %.2fx the CPU of playing (>1.5x would mean denormals)\n", ratio);
        if (ratio > 1.5) fail ("denormal slowdown in silence");
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "RELIABILITY OK" : "PROBLEMS FOUND", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
