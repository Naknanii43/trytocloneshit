// Offline smoke test: renders each factory preset to a WAV file and snapshots the editor to PNG.
#include "../Source/PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>

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

    for (int p = 0; p < proc.getNumPrograms(); ++p)
    {
        proc.setCurrentProgram (p);
        proc.prepareToPlay (sr, block);
        const int total = (int) (sr * 6);
        juce::AudioBuffer<float> out (2, total);
        out.clear();
        const int chord[] = { 48, 55, 60, 64, 67 };
        float peak = 0; bool bad = false;

        for (int pos = 0; pos < total; pos += block)
        {
            const int n = juce::jmin (block, total - pos);
            juce::AudioBuffer<float> buf (2, n);
            juce::MidiBuffer midi;
            if (pos == 0) for (int note : chord) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
            if (pos <= (int) (sr * 3) && pos + n > (int) (sr * 3))
                for (int note : chord) midi.addEvent (juce::MidiMessage::noteOff (1, note), (int) (sr * 3) - pos);
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

        const auto name = proc.getProgramName (p);
        std::printf ("%-22s peak=%.3f %s\n", name.toRawUTF8(), peak, bad ? "NaN/Inf!" : (peak < 0.01f ? "SILENT!" : "ok"));
        if (bad || peak < 0.01f) ++failures;

        juce::WavAudioFormat wav;
        auto file = outDir.getChildFile (name.replaceCharacter (' ', '_') + ".wav");
        file.deleteFile();
        if (auto stream = file.createOutputStream())
            if (auto* w = wav.createWriterFor (stream.get(), sr, 2, 24, {}, 0))
            {
                stream.release();
                std::unique_ptr<juce::AudioFormatWriter> writer (w);
                writer->writeFromAudioSampleBuffer (out, 0, total);
            }
    }

    // UI snapshot (render one more burst of audio so the scope has a waveform)
    proc.setCurrentProgram (1);
    {
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        for (int i = 0; i < 40; ++i) { proc.processBlock (buf, midi); midi.clear(); }
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    juce::PNGImageFormat png;
    auto pngFile = outDir.getChildFile ("ui.png");
    pngFile.deleteFile();
    if (auto s = pngFile.createOutputStream()) png.writeImageToStream (img, *s);
    ed.reset();

    std::printf (failures ? "FAILED (%d)\n" : "ALL OK\n", failures);
    return failures ? 1 : 0;
}
