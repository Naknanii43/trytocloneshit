#pragma once

#include <juce_core/juce_core.h>
#include <array>

namespace neb
{
constexpr int numLayers = 4, numLfos = 4, numModEnvs = 2, numSlots = 12, numMacros = 4;

inline juce::String layerPrefix (int l) { return juce::String::charToString ((juce::juce_wchar) ('A' + l)) + "_"; }

enum Mode { modeAnalog, modeWavetable, modeFM, modeGranular, modeSample };
inline juce::StringArray modeNames() { return { "Analog", "Wavetable", "FM", "Granular", "Sample" }; }

enum FilterType { fLP12, fLP24, fBand, fHigh, fNotch, fCombPos, fCombNeg, fVowel };
inline juce::StringArray filterNames() { return { "LP 12", "LP 24", "Band", "High", "Notch", "Comb +", "Comb -", "Vowel" }; }

inline juce::StringArray lfoShapeNames() { return { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H", "Drift" }; }

struct Division { const char* name; double beats; };
inline const std::array<Division, 15>& divisions()
{
    static const std::array<Division, 15> d { {
        { "4 bars", 16.0 }, { "2 bars", 8.0 }, { "1/1", 4.0 }, { "1/2", 2.0 }, { "1/2T", 4.0 / 3.0 },
        { "1/4D", 1.5 }, { "1/4", 1.0 }, { "1/4T", 2.0 / 3.0 }, { "1/8D", 0.75 }, { "1/8", 0.5 },
        { "1/8T", 1.0 / 3.0 }, { "1/16D", 0.375 }, { "1/16", 0.25 }, { "1/16T", 1.0 / 6.0 }, { "1/32", 0.125 } } };
    return d;
}
inline juce::StringArray divisionNames()
{
    juce::StringArray s;
    for (auto& d : divisions()) s.add (d.name);
    return s;
}
inline double divisionBeats (int i) { return divisions()[(size_t) juce::jlimit (0, (int) divisions().size() - 1, i)].beats; }

enum ModSrc { srcNone, srcLfo1, srcLfo2, srcLfo3, srcLfo4, srcEnv1, srcEnv2, srcVelocity, srcModWheel,
              srcAftertouch, srcKey, srcRandom, srcMacro1, srcMacro2, srcMacro3, srcMacro4, numSrc };
inline juce::StringArray modSrcNames()
{
    return { "-", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "Mod Env 1", "Mod Env 2", "Velocity", "Mod Wheel",
             "Aftertouch", "Key", "Random", "Macro 1", "Macro 2", "Macro 3", "Macro 4" };
}

enum ModDst { dstNone, dstPitch, dstCutoff, dstReso, dstLevel, dstPan, dstTimbre, dstDetune, dstDrive,
              dstSub, dstNoise, dstGrainSize, dstGrainDensity, dstFilterEnv, numDst };
inline juce::StringArray modDstNames()
{
    return { "-", "Pitch", "Cutoff", "Resonance", "Level", "Pan", "Timbre", "Detune", "Drive",
             "Sub", "Noise", "Grain Size", "Grain Density", "Filter Env" };
}
inline juce::StringArray modLayerNames() { return { "All", "A", "B", "C", "D" }; }

enum ArpMode { arpUp, arpDown, arpUpDown, arpRandom, arpAsPlayed, arpChord };
inline juce::StringArray arpModeNames() { return { "Up", "Down", "Up/Down", "Random", "As Played", "Chord" }; }

inline juce::StringArray distTypeNames() { return { "Soft", "Hard", "Fold", "Crush" }; }
} // namespace neb
