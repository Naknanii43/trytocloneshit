// Offline test + demo renderer for Reedstorm.
//
//   ReedstormRenderTest <outDir> [--demos]
//
// * checks the pitch tracker on a synthetic saxophone over the whole sax range
// * checks the pitch shifter, the octaver and the diatonic harmony logic
// * renders every factory preset over a sax phrase (NaN / silence / clipping / CPU checks)
// * runs at 44.1, 48 and 96 kHz and with a mono input bus
// * snapshots the editor to PNG
// With --demos it also writes the dry phrase and one WAV per preset for the demo MP3s.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <chrono>
#include <functional>
#include <tuple>

static void writeWav (const juce::File& file, const juce::AudioBuffer<float>& buf, double sr)
{
    juce::WavAudioFormat wav;
    file.deleteFile();
    if (auto stream = file.createOutputStream())
        if (auto* w = wav.createWriterFor (stream.get(), sr, (unsigned) buf.getNumChannels(), 24, {}, 0))
        {
            stream.release();
            std::unique_ptr<juce::AudioFormatWriter> writer (w);
            writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
        }
}

//==============================================================================
// A simple additive "saxophone": formant-shaped harmonic spectrum that brightens with
// dynamics, breath noise, a scooped attack and delayed vibrato. Good enough to exercise
// every effect with a realistic, harmonically rich monophonic signal.
struct SaxNote { float midi; float beats; float vel; };

static juce::AudioBuffer<float> synthSax (const std::vector<SaxNote>& notes, double sr, double bpm, float level = 0.35f)
{
    double total = 0;
    for (auto& n : notes) total += n.beats;
    const int len = (int) (total * 60.0 / bpm * sr + sr * 1.5);
    juce::AudioBuffer<float> out (1, len);
    out.clear();
    rs::Noise noise;
    rs::Svf breath; breath.set (2200.0f, 1.2f, sr);
    rs::Svf body; body.set (5200.0f, 0.6f, sr);

    double t0 = 0;
    for (auto& n : notes)
    {
        const int start = (int) (t0 * 60.0 / bpm * sr);
        const double dur = n.beats * 60.0 / bpm * 0.94;
        t0 += n.beats;
        if (n.midi <= 0) continue;
        const int num = (int) (dur * sr) + (int) (0.09 * sr);
        const float f0 = rs::midiToHz (n.midi);
        const int H = juce::jmin (60, (int) (sr * 0.45 / f0));
        std::vector<double> ph ((size_t) H + 1, 0.0);
        std::vector<float> amp ((size_t) H + 1);
        for (int k = 1; k <= H; ++k)
        {
            const float f = f0 * (float) k;
            auto bump = [f] (float c, float bw, float g) { const float x = (f - c) / bw; return g / (1.0f + x * x); };
            amp[(size_t) k] = (1.0f / std::pow ((float) k, 0.55f)) * (0.35f + bump (520, 260, 1.0f) + bump (1500, 500, 0.7f) + bump (2900, 700, 0.35f));
        }
        for (int i = 0; i < num && start + i < len; ++i)
        {
            const float t = (float) i / (float) sr;
            const float att = juce::jmin (1.0f, t / 0.045f);
            const float rel = t > (float) dur ? juce::jmax (0.0f, 1.0f - (t - (float) dur) / 0.08f) : 1.0f;
            const float env = att * rel * n.vel;
            const float scoop = -0.35f * std::exp (-t / 0.03f);
            const float vib = t > 0.28f ? 0.12f * juce::jmin (1.0f, (t - 0.28f) / 0.3f) * std::sin (rs::twoPi * 5.3f * t) : 0.0f;
            const float f = rs::midiToHz (n.midi + scoop + vib);
            const float bright = 0.35f + 0.65f * env;
            float s = 0;
            for (int k = 1; k <= H; ++k)
            {
                ph[(size_t) k] += (double) f * k / sr;
                const float roll = std::exp (-(float) k * (1.0f - bright) * 0.25f);
                s += amp[(size_t) k] * roll * (float) std::sin (juce::MathConstants<double>::twoPi * ph[(size_t) k]);
            }
            s = body.lp (s) * 0.18f + breath.bpNorm (noise.next()) * 0.05f * env;
            out.addSample (0, start + i, s * env * level);
        }
    }
    return out;
}

/** A bluesy phrase in concert Bb (sounds good on alto/tenor), ~12 s at 100 bpm. */
static std::vector<SaxNote> phrase()
{
    return {
        { 65, 0.5f, 0.8f }, { 68, 0.5f, 0.85f }, { 70, 1.0f, 0.95f }, { 0, 0.5f, 0 }, { 70, 0.25f, 0.7f }, { 72, 0.25f, 0.75f },
        { 73, 0.5f, 0.9f }, { 72, 0.5f, 0.8f }, { 70, 0.5f, 0.85f }, { 68, 0.5f, 0.75f }, { 65, 1.5f, 0.9f }, { 0, 0.5f, 0 },
        { 58, 0.5f, 0.8f }, { 61, 0.5f, 0.8f }, { 63, 0.5f, 0.85f }, { 64, 0.25f, 0.7f }, { 65, 0.75f, 0.95f }, { 63, 0.5f, 0.8f },
        { 61, 0.5f, 0.75f }, { 58, 2.0f, 0.9f }, { 0, 0.5f, 0 },
        { 77, 0.5f, 1.0f }, { 75, 0.5f, 0.9f }, { 73, 0.5f, 0.9f }, { 70, 0.5f, 0.85f }, { 72, 0.5f, 0.9f }, { 68, 0.5f, 0.8f },
        { 70, 3.0f, 1.0f } };
}

static juce::AudioBuffer<float> run (ReedstormProcessor& proc, const juce::AudioBuffer<float>& in, double sr, int block,
                                     bool monoIn, double& seconds)
{
    proc.setPlayConfigDetails (monoIn ? 1 : 2, 2, sr, block);
    proc.prepareToPlay (sr, block);
    const int len = in.getNumSamples();
    juce::AudioBuffer<float> out (2, len);
    juce::MidiBuffer midi;
    const auto t0 = std::chrono::high_resolution_clock::now();
    for (int pos = 0; pos < len; pos += block)
    {
        const int n = juce::jmin (block, len - pos);
        juce::AudioBuffer<float> buf (2, n);
        buf.copyFrom (0, 0, in, 0, pos, n);
        buf.copyFrom (1, 0, in, 0, pos, n);
        proc.processBlock (buf, midi);
        out.copyFrom (0, pos, buf, 0, 0, n);
        out.copyFrom (1, pos, buf, 1, 0, n);
    }
    seconds = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
    return out;
}

static float detectHz (const juce::AudioBuffer<float>& b, int ch, double sr, int from, int to = -1)
{
    rs::PitchTracker tr;
    tr.prepare (sr);
    if (to < 0) to = b.getNumSamples();
    double sum = 0; int count = 0;
    for (int i = 0; i < to; ++i)
    {
        tr.push (b.getSample (ch, i));
        if (i >= from && (i % 64) == 0 && tr.isVoiced()) { sum += tr.getMidi(); ++count; }
    }
    return count > 0 ? rs::midiToHz ((float) (sum / count)) : 0.0f;   // average over the stretch
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File ("render-out"));
    const bool demos = argc > 2 && juce::String (argv[2]) == "--demos";
    outDir.createDirectory();
    int failures = 0;
    auto fail = [&] (const juce::String& m) { std::printf ("  FAIL: %s\n", m.toRawUTF8()); ++failures; };

    //------------------------------------------------------------------------------ tracker
    std::printf ("Pitch tracker over the saxophone range (bari low A to soprano altissimo):\n");
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        float worst = 0;
        int missed = 0;
        for (int m = 45; m <= 92; m += 1)
        {
            rs::PitchTracker tr;
            tr.prepare (sr);
            auto tone = synthSax ({ { (float) m, 2.0f, 0.9f } }, sr, 120.0);
            int voicedFrames = 0;
            float err = 0;
            for (int i = 0; i < tone.getNumSamples(); ++i)
            {
                tr.push (tone.getSample (0, i));
                if (i > (int) (0.12 * sr) && i < (int) (0.25 * sr) && tr.isVoiced())
                {
                    ++voicedFrames;
                    // vibrato is +-0.12 st after 0.28 s; check before it
                    err = juce::jmax (err, std::abs (tr.getMidi() - (float) m));
                }
            }
            if (voicedFrames == 0) ++missed;
            if (err > 0.1f) std::printf ("    note %d: error %.3f\n", m, err);
            worst = juce::jmax (worst, err);
        }
        std::printf ("  %6.0f Hz: worst error %.3f semitones, %d notes missed\n", sr, worst, missed);
        if (worst > 0.15f || missed > 0) fail ("pitch tracker accuracy at " + juce::String (sr));
    }

    //------------------------------------------------------------------------------ shifter
    {
        const double sr = 48000.0;
        rs::PitchShifter ps;
        ps.prepare (sr, 80.0f);
        ps.setWindowForPeriod ((float) sr / 220.0f, 30.0f, sr);
        ps.snapWindow();
        juce::AudioBuffer<float> b (1, (int) sr);
        for (int i = 0; i < b.getNumSamples(); ++i)
            b.setSample (0, i, ps.process (0.4f * std::sin (rs::twoPi * 220.0f * (float) i / (float) sr), std::pow (2.0f, 7.0f / 12.0f)));
        juce::AudioBuffer<float> ref (1, (int) sr);
        for (int i = 0; i < ref.getNumSamples(); ++i) ref.setSample (0, i, 0.4f * std::sin (rs::twoPi * 329.63f * (float) i / (float) sr));
        std::printf ("Tracker on a pure 329.63 Hz sine -> %.2f Hz\n", detectHz (ref, 0, sr, 0));
        const float hz = detectHz (b, 0, sr, (int) (0.2 * sr));
        std::printf ("Pitch shifter +7 st on 220 Hz -> %.1f Hz (expect 329.6)\n", hz);
        if (std::abs (hz - 329.6f) > 3.0f) fail ("pitch shifter");
    }

    //------------------------------------------------------------------------------ harmony logic
    {
        rs::Rack rack;
        rack.prepare (48000.0);
        rs::RackParams p;
        p.harMode = 0; p.harKey = 0; p.harScale = 0; p.instrument = 0;
        struct Case { float note; int steps; float expect; } cases[] = {
            { 60, 2, 4 },  { 62, 2, 3 },  { 64, 2, 3 },  { 65, 2, 4 },  { 67, 2, 4 },  { 71, 2, 3 },
            { 60, -3, -5 }, { 64, -3, -5 }, { 65, -3, -5 }, { 60, 7, 12 }, { 60, -7, -12 }, { 61, 2, 4 } };
        int bad = 0;
        for (auto& c : cases)
        {
            rack.setTrackedMidiForTest (c.note);
            const float got = rack.harmonyShift (c.steps, p);
            if (std::abs (got - c.expect) > 0.01f) { ++bad; std::printf ("  note %g steps %d -> %g (expect %g)\n", c.note, c.steps, got, c.expect); }
        }
        // alto in written C major = concert Eb major: concert Eb (63) + third -> G (+4)
        p.instrument = 2;
        rack.setTrackedMidiForTest (63);
        if (std::abs (rack.harmonyShift (2, p) - 4.0f) > 0.01f) { ++bad; std::printf ("  transposed key failed\n"); }
        std::printf ("Diatonic harmony logic: %d / %d cases wrong\n", bad, (int) std::size (cases) + 1);
        if (bad) fail ("harmony logic");
    }

    //------------------------------------------------------------------------------ octaver
    {
        const double sr = 48000.0;
        ReedstormProcessor proc;
        auto set = [&] (const char* id, float v) { auto* pr = proc.apvts.getParameter (id); pr->setValueNotifyingHost (pr->convertTo0to1 (v)); };
        set ("oct_on", 1); set ("oct_sub1", 1); set ("oct_dry", 0); set ("eq_on", 0); set ("dyn_on", 0);
        auto in = synthSax ({ { 57, 4.0f, 0.9f } }, sr, 120.0);   // A3 = 220 Hz
        double secs;
        auto out = run (proc, in, sr, 256, false, secs);
        const float hz = detectHz (out, 0, sr, (int) (0.3 * sr), (int) (1.5 * sr));
        std::printf ("Octaver sub-1 on 220 Hz -> %.1f Hz (expect 110)\n", hz);
        if (std::abs (hz - 110.0f) > 3.0f) fail ("octaver");
    }

    //------------------------------------------------------------------------------ harmony in tune
    {
        const double sr = 48000.0;
        for (int interval : { 7, -5, 4, 12 })
        {
            ReedstormProcessor proc;
            auto set = [&] (const char* id, float v) { auto* pr = proc.apvts.getParameter (id); pr->setValueNotifyingHost (pr->convertTo0to1 (v)); };
            set ("har_on", 1); set ("har_mode", 1); set ("har_int1", (float) interval); set ("har_lvl1", 1); set ("har_lvl2", 0);
            set ("har_spread", 1); set ("eq_on", 0); set ("dyn_on", 0);
            auto in = synthSax ({ { 57, 4.0f, 0.9f } }, sr, 120.0);
            double secs;
            auto out = run (proc, in, sr, 256, false, secs);
            juce::AudioBuffer<float> voice (1, out.getNumSamples());      // hard-left voice = L - R
            for (int i = 0; i < out.getNumSamples(); ++i) voice.setSample (0, i, out.getSample (0, i) - out.getSample (1, i));
            const float hz = detectHz (voice, 0, sr, (int) (0.4 * sr), (int) (1.7 * sr));
            const float expect = 220.0f * std::pow (2.0f, (float) interval / 12.0f);
            const float cents = 1200.0f * std::log2 (std::max (hz, 1.0f) / expect);
            std::printf ("Harmony voice %+d st on A3 -> %.1f Hz (expect %.1f, %+.1f cents)\n", interval, hz, expect, cents);
            if (std::abs (cents) > 6.0f) fail ("harmony voice out of tune");
        }
    }

    //------------------------------------------------------------------------------ morph generator
    {
        const double sr = 48000.0;
        const float splits[3] = { 220.0f, 370.0f, 620.0f };
        const bool zonesOk = rs::Generator::zoneOf (150, splits) == 0 && rs::Generator::zoneOf (300, splits) == 1
                          && rs::Generator::zoneOf (500, splits) == 2 && rs::Generator::zoneOf (900, splits) == 3;
        std::printf ("Zone mapping: %s\n", zonesOk ? "ok" : "WRONG");
        if (! zonesOk) fail ("zone mapping");

        // full morph: each synth character must follow the sax pitch
        for (int snd = 0; snd < 6; ++snd)
        {
            ReedstormProcessor proc;
            auto set = [&] (const char* id, float v) { auto* pr = proc.apvts.getParameter (id); pr->setValueNotifyingHost (pr->convertTo0to1 (v)); };
            set ("gen_on", 1); set ("gen_morph", 1); set ("gen_bloom", 0); set ("gen_snd2", (float) snd); set ("eq_on", 0);
            auto in = synthSax ({ { 64, 4.0f, 0.9f } }, sr, 120.0);   // E4 = 329.6 Hz -> zone 2
            double secs;
            auto out = run (proc, in, sr, 256, false, secs);
            // harmonicity at the played pitch: energy on multiples of f0 vs. halfway between them
            // (formant-heavy timbres fool time-domain trackers, the spectrum does not)
            auto goertzel = [&] (double hz)
            {
                const int a = (int) (0.6 * sr), b = (int) (1.8 * sr);
                const double w = juce::MathConstants<double>::twoPi * hz / sr;
                double s1 = 0, s2 = 0;
                for (int i = a; i < b; ++i)
                {
                    const double win = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (i - a) / (b - a));
                    const double s0 = out.getSample (0, i) * win + 2.0 * std::cos (w) * s1 - s2;
                    s2 = s1; s1 = s0;
                }
                return s1 * s1 + s2 * s2 - 2.0 * std::cos (w) * s1 * s2;
            };
            const double f0 = snd == 3 ? 164.81 : 329.63;     // Drone adds a sub-octave
            double on = 0, off = 0;
            for (int k = 1; k <= 8; ++k) { on += goertzel (f0 * k); off += goertzel (f0 * (k + 0.5)); }
            const double ratioDb = 10.0 * std::log10 (on / std::max (off, 1e-12));
            std::printf ("Morph voice %-6s on E4: harmonics of the played pitch %.1f dB above in-between\n", rs::params::genSounds[snd].toRawUTF8(), ratioDb);
            if (ratioDb < 15.0) fail ("morph voice " + rs::params::genSounds[snd] + " does not follow the pitch");
        }

        // bloom: a held note triggers a generative pattern in its zone's sound
        int zoneHits[4] = {};
        for (int note : { 50, 60, 69, 79 })   // D3, C4, A4, G5: one per zone
        {
            ReedstormProcessor proc;
            auto set = [&] (const char* id, float v) { auto* pr = proc.apvts.getParameter (id); pr->setValueNotifyingHost (pr->convertTo0to1 (v)); };
            set ("gen_on", 1); set ("gen_morph", 0); set ("gen_bloom", 1); set ("gen_length", 2);
            auto in = synthSax ({ { (float) note, 3.0f, 0.9f } }, sr, 120.0);
            double secs;
            auto withGen = run (proc, in, sr, 512, false, secs);
            set ("gen_on", 0);
            auto dry = run (proc, in, sr, 512, false, secs);
            double diff = 0;
            for (int i = 0; i < in.getNumSamples(); ++i) diff += std::pow (withGen.getSample (0, i) - dry.getSample (0, i), 2.0);
            const int triggers = proc.generator().meters.triggers.load();
            const int z = proc.generator().meters.bloomZone.load();
            if (triggers > 0 && diff > 1.0) zoneHits[z]++;
            std::printf ("Bloom on %s: %d trigger(s), zone %d, added energy %.1f\n", rs::ui::noteName (note).toRawUTF8(), triggers, z + 1, diff);
        }
        if (zoneHits[0] != 1 || zoneHits[1] != 1 || zoneHits[2] != 1 || zoneHits[3] != 1) fail ("bloom per zone");
    }

    //------------------------------------------------------------------------------ loop station
    {
        const double sr = 48000.0;
        rs::LoopStation st;
        st.prepare (sr);
        rs::LoopParams prm;
        const int block = 128;
        float L[block], R[block];
        long long now = 0;
        auto feed = [&] (int samples, std::function<float (long long)> src, std::vector<float>* capture = nullptr)
        {
            for (int done = 0; done < samples; done += block)
            {
                for (int i = 0; i < block; ++i) { L[i] = R[i] = src (now + i); }
                st.process (L, R, block, prm, 120.0);
                if (capture) capture->insert (capture->end(), L, L + block);
                now += block;
            }
        };
        auto sine = [&] (float hz) { return [hz, sr] (long long t) { return 0.3f * std::sin (rs::twoPi * hz * (float) t / (float) sr); }; };
        auto silence = [] (long long) { return 0.0f; };
        const int M = 48000;
        auto& t0 = st.track (0);

        st.post (0, rs::LoopTrack::Tap);  feed (M, sine (440));          // record 1 s of A on track 1
        st.post (0, rs::LoopTrack::Tap);  std::vector<float> pass1; feed (M * 2, silence, &pass1);
        const int len = t0.uiLength.load();
        double err = 0, energy = 0;
        for (int i = 4800; i < M; ++i) { err += std::abs (pass1[(size_t) i] - pass1[(size_t) (i + len)]); energy += std::abs (pass1[(size_t) i]); }
        std::printf ("Loop track 1: length %d, master %d, repeats exactly: %s\n", len, st.uiMasterLen.load(), err < 1e-3 ? "yes" : "NO");
        if (len != M || st.uiMasterLen.load() != M || err > 1e-3 || energy / M < 0.08) fail ("loop record/play");

        st.post (0, rs::LoopTrack::Tap);  feed (M, sine (660));          // overdub E
        st.post (0, rs::LoopTrack::Tap);  feed (M * 2, silence);
        std::vector<float> dubbed; feed (M, silence, &dubbed);
        const bool undoReady = t0.undoReady.load();
        st.post (0, rs::LoopTrack::Undo); std::vector<float> undone; feed (M, silence, &undone);
        st.post (0, rs::LoopTrack::Undo); std::vector<float> redone; feed (M, silence, &redone);
        double eDub = 0, eUndo = 0, dRedo = 0, dUndo = 0;
        for (int i = 0; i < M; ++i)
        {
            eDub += dubbed[(size_t) i] * dubbed[(size_t) i];
            eUndo += undone[(size_t) i] * undone[(size_t) i];
            dRedo += std::abs (redone[(size_t) i] - dubbed[(size_t) i]);
            if (i >= 4800) dUndo += std::abs (undone[(size_t) i] - pass1[(size_t) i]);
        }
        std::printf ("Overdub energy %.1f -> undo %.1f (matches first take: %s) -> redo matches overdub: %s\n",
                     eDub, eUndo, dUndo < 1.0 ? "yes" : "NO", dRedo < 1.0 ? "yes" : "NO");
        if (! undoReady || eDub < eUndo * 1.5 || dUndo > 1.0 || dRedo > 1.0) fail ("loop overdub/undo/redo");

        // track 2: started off the beat, closed after 1.4 master loops -> must become exactly 2x and stay phase-locked
        prm.track[0].mute = true;
        feed (13 * block, silence);
        st.post (1, rs::LoopTrack::Tap);
        const long long impulseAt = now + 7000;
        feed ((int) (M * 1.4) / block * block, [&] (long long t) { return t == impulseAt ? 1.0f : 0.0f; });
        st.post (1, rs::LoopTrack::Tap);
        feed (M, silence);                                      // finishes the second bar by itself
        auto& t1 = st.track (1);
        std::vector<float> out2; const long long cap2 = now; feed (M * 4, silence, &out2);
        auto lockedOk = [&] (const std::vector<float>& v, long long base, int period)
        {
            int hits = 0, bad = 0;
            for (size_t i = 0; i < v.size(); ++i)
                if (std::abs (v[i]) > 0.5f) { ++hits; if (((base + (long long) i - impulseAt) % period + period) % period != 0) ++bad; }
            return std::make_pair (hits, bad);
        };
        auto [hits, bad] = lockedOk (out2, cap2, t1.uiLength.load());
        std::printf ("Track 2: length %d (= %.2f x master), impulse repeats %d times, %d off-grid\n",
                     t1.uiLength.load(), (double) t1.uiLength.load() / M, hits, bad);
        if (t1.uiLength.load() != 2 * M || hits < 2 || bad != 0) fail ("loop lock");

        st.post (1, rs::LoopTrack::Stop);    feed (M / 3 / block * block, silence);
        st.post (1, rs::LoopTrack::Tap);     std::vector<float> out3; const long long cap3 = now; feed (M * 4, silence, &out3);
        auto [hits3, bad3] = lockedOk (out3, cap3, M);
        std::printf ("Track 2 after stop/restart: %d hits, %d off the master grid\n", hits3, bad3);
        if (hits3 < 1 || bad3 != 0) fail ("loop restart in sync");

        st.clearAll(); feed (block * 2, silence);
        if (st.track (0).uiState.load() != rs::LoopTrack::Empty || st.uiMasterLen.load() != 0) fail ("loop clear all");
    }

    //------------------------------------------------------------------------------ gremlins
    {
        const double sr = 48000.0;
        auto runGremlins = [&] (float harsh, int seconds, int* counts, float& peak, bool& finite)
        {
            rs::Gremlins g;
            g.prepare (sr);
            rs::GremlinParams gp;
            gp.on = true; gp.harsh = harsh; gp.chance = 1.0f; gp.swarm = 1.0f; gp.dirt = 0.5f; gp.level = 1.0f;
            rs::Fade fade; fade.prepare (sr); fade.set (true);
            rs::Noise nz;
            const int block = 256;
            float in[block], L[block], R[block];
            int seen = 0;
            peak = 0; finite = true;
            for (long long t = 0; t < (long long) seconds * (long long) sr; t += block)
            {
                for (int i = 0; i < block; ++i)
                {
                    const long long k = (t + i) % (long long) (sr * 0.3);     // a clap every 300 ms
                    in[i] = k < 600 ? nz.next() * 0.6f * (1.0f - (float) k / 600.0f) : 0.0f;
                    L[i] = R[i] = 0.0f;
                }
                g.process (in, L, R, block, true, 220.0f, gp, fade);
                for (int i = 0; i < block; ++i)
                {
                    if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) finite = false;
                    peak = juce::jmax (peak, std::abs (L[i]), std::abs (R[i]));
                }
                const int sp = g.meters.spawned.load();
                for (; seen < sp; ++seen) counts[g.meters.history[(size_t) (seen % rs::Gremlins::historySize)].load()]++;
            }
            return g.meters.onsets.load();
        };
        int silly[rs::Gremlins::numTypes] {}, harshC[rs::Gremlins::numTypes] {}, mixed[rs::Gremlins::numTypes] {};
        float pk1, pk2, pk3; bool f1, f2, f3;
        const int onsets = runGremlins (0.0f, 12, silly, pk1, f1);
        runGremlins (1.0f, 12, harshC, pk2, f2);
        runGremlins (0.5f, 40, mixed, pk3, f3);
        int sillyWrong = 0, harshWrong = 0, missing = 0, total = 0;
        for (int t = 0; t < rs::Gremlins::numTypes; ++t)
        {
            if (t >= rs::Gremlins::numSilly) sillyWrong += silly[t]; else harshWrong += harshC[t];
            if (mixed[t] == 0) ++missing;
            total += mixed[t];
        }
        std::printf ("Gremlins: %d onsets heard from 40 claps; silly mode spawned %d harsh, harsh mode spawned %d silly; "
                     "mixed run: %d creatures, %d of %d kinds never appeared; peaks %.2f %.2f %.2f\n",
                     onsets, sillyWrong, harshWrong, total, missing, rs::Gremlins::numTypes, pk1, pk2, pk3);
        if (onsets < 36 || onsets > 44) fail ("gremlin onset detection");
        if (sillyWrong || harshWrong || missing) fail ("gremlin silly/harsh selection");
        if (! (f1 && f2 && f3) || pk3 > 4.0f || pk3 < 0.05f) fail ("gremlin output");
    }

    //------------------------------------------------------------------------------ presets
    std::printf ("\nRendering every factory preset:\n");
    const auto& presets = rs::factoryPresets();
    double worstRt = 0;
    for (double sr : { 48000.0, 44100.0, 96000.0 })
    {
        const auto in = synthSax (phrase(), sr, 100.0);
        if (demos && sr == 48000.0)
        {
            juce::AudioBuffer<float> st (2, in.getNumSamples());
            st.copyFrom (0, 0, in, 0, 0, in.getNumSamples());
            st.copyFrom (1, 0, in, 0, 0, in.getNumSamples());
            writeWav (outDir.getChildFile ("00_Dry_Sax.wav"), st, sr);
        }
        for (size_t i = 0; i < presets.size(); ++i)
        {
            ReedstormProcessor proc;
            proc.loadPreset ((int) i);
            double secs = 0;
            const bool monoIn = (i % 3) == 1;
            const int block = sr == 48000.0 ? 512 : (sr == 44100.0 ? 441 : 1024);
            auto out = run (proc, in, sr, block, monoIn, secs);
            bool finite = true;
            float peak = 0, rms = 0;
            for (int c = 0; c < 2; ++c)
                for (int s = 0; s < out.getNumSamples(); ++s)
                {
                    const float v = out.getSample (c, s);
                    if (! std::isfinite (v)) finite = false;
                    peak = juce::jmax (peak, std::abs (v));
                    rms += v * v;
                }
            rms = std::sqrt (rms / (2.0f * (float) out.getNumSamples()));
            const double rt = secs / ((double) out.getNumSamples() / sr);
            worstRt = juce::jmax (worstRt, rt);
            if (sr == 48000.0)
                std::printf ("  %-22s peak %.2f  rms %5.1f dB  cpu %.2f%% of realtime\n", presets[i].name, peak, rs::gainToDb (rms), rt * 100.0);
            if (! finite) fail (juce::String (presets[i].name) + " produced NaN/Inf");
            if (rms < 0.005f) fail (juce::String (presets[i].name) + " is (nearly) silent");
            if (peak > 0.99f) fail (juce::String (presets[i].name) + " clips");

            if (demos && sr == 48000.0)
                writeWav (outDir.getChildFile (juce::String (i + 1).paddedLeft ('0', 2) + "_" + juce::String (presets[i].name).replace ("&", "and").replaceCharacters (" /", "__") + ".wav"), out, sr);
        }
    }
    std::printf ("Worst CPU: %.2f%% of one core in realtime\n", worstRt * 100.0);

    //------------------------------------------------------------------------------ editor
    {
        ReedstormProcessor proc;
        int showcase = 0;
        for (int i = 0; i < proc.getNumPrograms(); ++i) if (proc.getProgramName (i) == "Zone Morph") showcase = i;
        proc.loadPreset (showcase);
        auto* harm = proc.apvts.getParameter ("har_on");    // also light up the harmony lines in the trace
        harm->setValueNotifyingHost (1.0f);
        proc.apvts.getParameter ("har_lvl2")->setValueNotifyingHost (proc.apvts.getParameter ("har_lvl2")->convertTo0to1 (0.4f));
        const double sr = 48000.0;
        proc.setPlayConfigDetails (2, 2, sr, 512);
        proc.prepareToPlay (sr, 512);
        proc.apvts.getParameter ("grm_on")->setValueNotifyingHost (1.0f);
        proc.apvts.getParameter ("grm_harsh")->setValueNotifyingHost (0.45f);
        proc.apvts.getParameter ("grm_swarm")->setValueNotifyingHost (0.6f);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        auto* red = dynamic_cast<ReedstormEditor*> (ed.get());
        ed->setSize (ReedstormContent::designW, ReedstormContent::designH);
        red->setPage (0);
        const auto in = synthSax (phrase(), sr, 100.0);
        juce::MidiBuffer midi;
        const int stop = (int) (sr * 6.15);
        auto& ls = proc.looper();
        for (int pos = 0; pos < stop; pos += 512)
        {
            juce::AudioBuffer<float> buf (2, 512);
            buf.copyFrom (0, 0, in, 0, pos, 512);
            buf.copyFrom (1, 0, in, 0, pos, 512);
            const int blk = pos / 512;
            if (blk == 4)   ls.post (0, rs::LoopTrack::Tap);      // track 1: master loop
            if (blk == 190) ls.post (0, rs::LoopTrack::Tap);
            if (blk == 200) ls.post (1, rs::LoopTrack::Tap);      // track 2: locks to 2x
            if (blk == 300) ls.post (1, rs::LoopTrack::Tap);
            if (blk == 420) ls.post (2, rs::LoopTrack::Tap);      // track 3: recording when we look
            if (blk == 495) red->setPage (2);
            proc.processBlock (buf, midi);
            if (blk % 3 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (33);
        }
        auto snap = [&] (const char* name)
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
            const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::PNGImageFormat png;
            auto f = outDir.getChildFile (name);
            f.deleteFile();
            if (auto s2 = f.createOutputStream()) png.writeImageToStream (img, *s2);
            std::printf ("Editor snapshot: %s\n", f.getFullPathName().toRawUTF8());
        };
        std::printf ("\n");
        snap ("ui_generate.png");
        red->setPage (3); snap ("ui_loops.png");
        red->setPage (0); snap ("ui.png");

        // MANGLE page with all four effects running
        for (auto* id : { "whm_on", "rng_on", "frz_on", "gat_on" }) proc.apvts.getParameter (id)->setValueNotifyingHost (1.0f);
        proc.apvts.getParameter ("whm_mode")->setValueNotifyingHost (proc.apvts.getParameter ("whm_mode")->convertTo0to1 (1));
        proc.apvts.getParameter ("rng_mix")->setValueNotifyingHost (0.3f);
        proc.apvts.getParameter ("frz_mode")->setValueNotifyingHost (1.0f);
        red->setPage (1);
        for (int pos = 0; pos < (int) (sr * 3.0); pos += 512)
        {
            juce::AudioBuffer<float> buf (2, 512);
            buf.copyFrom (0, 0, in, 0, pos, 512);
            buf.copyFrom (1, 0, in, 0, pos, 512);
            proc.processBlock (buf, midi);
            if ((pos / 512) % 3 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (33);
        }
        snap ("ui_mangle.png");

        // also check the editor survives a resize and every preset switch
        ed->setSize (ReedstormContent::designW / 2, ReedstormContent::designH / 2);
        for (int i = 0; i < proc.getNumPrograms(); ++i) proc.setCurrentProgram (i);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
