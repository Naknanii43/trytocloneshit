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
                writeWav (outDir.getChildFile (juce::String (i + 1).paddedLeft ('0', 2) + "_" + juce::String (presets[i].name).replaceCharacters (" /", "__") + ".wav"), out, sr);
        }
    }
    std::printf ("Worst CPU: %.2f%% of one core in realtime\n", worstRt * 100.0);

    //------------------------------------------------------------------------------ editor
    {
        ReedstormProcessor proc;
        proc.loadPreset (11);   // Horn Section: lights up the harmony trace
        const double sr = 48000.0;
        proc.setPlayConfigDetails (2, 2, sr, 512);
        proc.prepareToPlay (sr, 512);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (ReedstormContent::designW, ReedstormContent::designH);
        const auto in = synthSax (phrase(), sr, 100.0);
        juce::MidiBuffer midi;
        const int stop = (int) (sr * 6.15);
        for (int pos = 0; pos < stop; pos += 512)
        {
            juce::AudioBuffer<float> buf (2, 512);
            buf.copyFrom (0, 0, in, 0, pos, 512);
            buf.copyFrom (1, 0, in, 0, pos, 512);
            proc.processBlock (buf, midi);
            if ((pos / 512) % 3 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (33);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
        const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto f = outDir.getChildFile ("ui.png");
        f.deleteFile();
        if (auto s = f.createOutputStream()) png.writeImageToStream (img, *s);
        std::printf ("\nEditor snapshot: %s\n", f.getFullPathName().toRawUTF8());

        // also check the editor survives a resize and every preset switch
        ed->setSize (ReedstormContent::designW / 2, ReedstormContent::designH / 2);
        for (int i = 0; i < proc.getNumPrograms(); ++i) proc.setCurrentProgram (i);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
