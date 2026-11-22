// Offline smoke test: renders every factory preset to WAV, checks for silence / NaNs,
// measures CPU cost, loads a user sample, and snapshots each editor page to PNG.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

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

static juce::AudioBuffer<float> renderChord (NebulonProcessor& proc, double sr, int block, float& peak, bool& bad)
{
    proc.prepareToPlay (sr, block);
    const int total = (int) (sr * 6);
    juce::AudioBuffer<float> out (2, total);
    out.clear();
    const int chord[] = { 48, 55, 60, 64, 67 };
    peak = 0; bad = false;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = juce::jmin (block, total - pos);
        juce::AudioBuffer<float> buf (2, n);
        juce::MidiBuffer midi;
        if (pos == 0) for (int note : chord) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        if (pos <= (int) (sr * 3.5) && pos + n > (int) (sr * 3.5))
            for (int note : chord) midi.addEvent (juce::MidiMessage::noteOff (1, note), (int) (sr * 3.5) - pos);
        proc.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float v = buf.getSample (c, i);
                if (! std::isfinite (v)) bad = true;
                peak = juce::jmax (peak, std::abs (v));
                out.setSample (c, pos + i, v);
            }
    }
    return out;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? argv[1] : "render-out");
    outDir.createDirectory();

    NebulonProcessor proc;
    const double sr = 48000.0;
    const int block = 512;
    proc.setPlayConfigDetails (0, 2, sr, block);
    int failures = 0;
    double worstRt = 0;

    for (int p = 0; p < proc.getNumPrograms(); ++p)
    {
        proc.setCurrentProgram (p);
        float peak; bool bad;
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto out = renderChord (proc, sr, block, peak, bad);
        const double rt = (juce::Time::getMillisecondCounterHiRes() - t0) / 6000.0; // fraction of real time
        worstRt = juce::jmax (worstRt, rt);
        const auto name = proc.getProgramName (p);
        const bool silent = peak < 0.01f;
        std::printf ("%-10s %-22s peak=%.3f cpu=%4.1f%% %s\n", proc.getPresetCategory (p).toRawUTF8(), name.toRawUTF8(),
                     peak, rt * 100.0, bad ? "NaN/Inf!" : (silent ? "SILENT!" : "ok"));
        if (bad || silent) ++failures;
        writeWav (outDir.getChildFile (name.replaceCharacter (' ', '_') + ".wav"), out, sr);
    }
    std::printf ("worst-case render cost: %.1f%% of one core (5-note chord)\n", worstRt * 100.0);

    // user sample: write a test tone to disk, load it into layer A, granulate it
    {
        juce::AudioBuffer<float> tone (1, 48000);
        for (int i = 0; i < tone.getNumSamples(); ++i)
            tone.setSample (0, i, 0.5f * (float) std::sin (neb::twoPi * 261.6256 * i / 48000.0) * (1.0f + 0.3f * (float) std::sin (i * 0.0005)));
        auto f = outDir.getChildFile ("test_tone.wav");
        writeWav (f, tone, 48000.0);
        proc.setCurrentProgram (0);
        const bool ok = proc.loadUserSample (0, f);
        proc.apvts.getParameter ("A_mode")->setValueNotifyingHost (proc.apvts.getParameter ("A_mode")->convertTo0to1 (3));
        float peak; bool bad;
        renderChord (proc, sr, block, peak, bad);
        std::printf ("user sample load: %s, granular render peak=%.3f %s\n", ok ? "ok" : "FAILED", peak, bad ? "NaN!" : "");
        if (! ok || bad || peak < 0.01f) ++failures;
    }

    // arpeggiator: hold a chord with the arp on, count note-ons coming out
    {
        proc.setCurrentProgram (0);
        proc.apvts.getParameter ("arpOn")->setValueNotifyingHost (1.0f);
        proc.prepareToPlay (sr, block);
        int noteOns = 0;
        for (int b = 0; b < (int) (sr * 2 / block); ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0) for (int n : { 60, 64, 67 }) midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
            juce::MidiBuffer copy (midi);
            neb::Arpeggiator::Settings s; s.on = true; s.stepBeats = 0.25;
            proc.arp.process (copy, block, s, 120.0, false, 0.0);
            for (const auto m : copy) if (m.getMessage().isNoteOn()) ++noteOns;
        }
        std::printf ("arp: %d note-ons in 2s at 1/16 @120bpm (expect ~16)\n", noteOns);
        if (noteOns < 14 || noteOns > 18) ++failures;
    }

    // Pluck engine tuning: play single notes, check the fundamental
    {
        juce::dsp::FFT fft (16);
        for (int noteNum : { 45, 57, 69, 81 })
        {
            proc.setCurrentProgram (0);
            proc.apvts.getParameter ("A_mode")->setValueNotifyingHost (proc.apvts.getParameter ("A_mode")->convertTo0to1 (5));
            proc.apvts.getParameter ("A_plkDecay")->setValueNotifyingHost (proc.apvts.getParameter ("A_plkDecay")->convertTo0to1 (8.0f));
            proc.prepareToPlay (sr, block);
            std::vector<float> data (2 * 65536, 0.0f);
            int w = 0;
            for (int b = 0; b < 140; ++b)
            {
                juce::AudioBuffer<float> buf (2, block);
                juce::MidiBuffer midi;
                if (b == 0) midi.addEvent (juce::MidiMessage::noteOn (1, noteNum, (juce::uint8) 100), 0);
                proc.processBlock (buf, midi);
                if (b >= 12) for (int i = 0; i < block && w < 65536; ++i) data[(size_t) w++] = buf.getSample (0, i) * (0.5f - 0.5f * std::cos (6.2831853f * (float) w / 65536.0f));
            }
            fft.performFrequencyOnlyForwardTransform (data.data());
            const double expect = neb::midiToHz (noteNum);
            // strongest peak near the expected fundamental (+/- half an octave)
            int best = 0; float bestMag = 0;
            for (int k = (int) (expect / 1.41 * 65536 / sr); k < (int) (expect * 1.41 * 65536 / sr); ++k)
                if (data[(size_t) k] > bestMag) { bestMag = data[(size_t) k]; best = k; }
            const double got = best * sr / 65536.0;
            const double cents = 1200.0 * std::log2 (got / expect);
            std::printf ("pluck tuning: note %d expect %.1f Hz got %.1f Hz (%+.0f cents) %s\n", noteNum, expect, got, cents,
                         std::abs (cents) < 25 ? "ok" : "OUT OF TUNE!");
            if (std::abs (cents) >= 25) ++failures;
        }
    }

    // preview phrase player: no MIDI in, should still play notes
    {
        proc.setCurrentProgram (1);
        proc.preview.enabled.store (true);
        float peak; bool bad;
        juce::AudioBuffer<float> buf (2, block);
        proc.prepareToPlay (sr, block);
        peak = 0; bad = false;
        for (int b = 0; b < (int) (sr * 4 / block); ++b)
        {
            juce::MidiBuffer midi;
            proc.processBlock (buf, midi);
            peak = juce::jmax (peak, buf.getMagnitude (0, block));
        }
        proc.preview.enabled.store (false);
        std::printf ("preview: peak=%.3f %s\n", peak, peak > 0.01f ? "ok" : "SILENT!");
        if (peak < 0.01f) ++failures;
    }

    // inspire: 20 random patches must all make sound
    {
        int silent = 0;
        for (int i = 0; i < 20; ++i)
        {
            proc.inspire();
            float peak; bool bad;
            renderChord (proc, sr, block, peak, bad);
            if (peak < 0.01f || bad) { ++silent; std::printf ("  inspire FAILED: %s peak=%.3f\n", proc.currentPresetName.toRawUTF8(), peak); }
        }
        std::printf ("inspire: %d/20 random patches OK\n", 20 - silent);
        failures += silent;
    }

    // UI snapshots of every page
    proc.setCurrentProgram (2);
    {
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        for (int i = 0; i < 60; ++i) { proc.processBlock (buf, midi); midi.clear(); }
    }
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        auto* ne = dynamic_cast<NebulonEditor*> (ed.get());
        const char* names[] = { "main", "layerA", "layerB", "layerC", "layerD", "mod", "arp", "fx" };
        for (int pg = 0; pg < 8; ++pg)
        {
            ne->showPage (pg);
            const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::PNGImageFormat png;
            auto f = outDir.getChildFile (juce::String ("ui_") + names[pg] + ".png");
            f.deleteFile();
            if (auto s = f.createOutputStream()) png.writeImageToStream (img, *s);
        }
    }

    std::printf (failures ? "FAILED (%d)\n" : "ALL OK\n", failures);
    return failures ? 1 : 0;
}
