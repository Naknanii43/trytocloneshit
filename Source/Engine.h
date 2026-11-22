#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP.h"
#include "Params.h"
#include "Wavetables.h"
#include "Sources.h"

namespace neb
{
using Atom = std::atomic<float>*;

struct LayerParams
{
    Atom on{}, level{}, pan{}, mode{}, octave{}, semi{}, fine{},
         wave{}, wtTable{}, wtPos{}, fmRatio{}, fmIndex{}, fmFb{},
         src{}, grSize{}, grDensity{}, grPos{}, grSpray{}, grPitch{}, smpStart{}, smpLoop{},
         unison{}, detune{}, spread{}, harm1{}, harm2{},
         sub{}, noise{}, drift{}, drive{},
         ftype{}, cutoff{}, reso{}, fenv{}, keytrack{},
         aA{}, aD{}, aS{}, aR{}, fA{}, fD{}, fS{}, fR{}, velSens{},
         plkTone{}, plkDecay{}, plkBow{}, pitchEnv{}, pitchDecay{};

    void bind (juce::AudioProcessorValueTreeState& s, const juce::String& p)
    {
        auto g = [&] (const char* id)
        {
            auto* a = s.getRawParameterValue (p + id);
            jassert (a != nullptr);
            return a;
        };
        on = g ("on"); level = g ("level"); pan = g ("pan"); mode = g ("mode");
        octave = g ("octave"); semi = g ("semi"); fine = g ("fine");
        wave = g ("wave"); wtTable = g ("wtTable"); wtPos = g ("wtPos");
        fmRatio = g ("fmRatio"); fmIndex = g ("fmIndex"); fmFb = g ("fmFb");
        src = g ("src"); grSize = g ("grSize"); grDensity = g ("grDensity"); grPos = g ("grPos");
        grSpray = g ("grSpray"); grPitch = g ("grPitch"); smpStart = g ("smpStart"); smpLoop = g ("smpLoop");
        unison = g ("unison"); detune = g ("detune"); spread = g ("spread"); harm1 = g ("harm1"); harm2 = g ("harm2");
        sub = g ("sub"); noise = g ("noise"); drift = g ("drift"); drive = g ("drive");
        ftype = g ("ftype"); cutoff = g ("cutoff"); reso = g ("reso"); fenv = g ("fenv"); keytrack = g ("keytrack");
        aA = g ("aA"); aD = g ("aD"); aS = g ("aS"); aR = g ("aR");
        fA = g ("fA"); fD = g ("fD"); fS = g ("fS"); fR = g ("fR"); velSens = g ("velSens");
        plkTone = g ("plkTone"); plkDecay = g ("plkDecay"); plkBow = g ("plkBow");
        pitchEnv = g ("pitchEnv"); pitchDecay = g ("pitchDecay");
    }
};

struct GlobalParams
{
    Atom poly{}, mono{}, legato{}, glide{}, bend{};
    Atom macro[numMacros] {};
    Atom lfoRate[numLfos] {}, lfoShape[numLfos] {}, lfoSync[numLfos] {}, lfoDiv[numLfos] {}, lfoFade[numLfos] {}, lfoRetrig[numLfos] {};
    Atom me[numModEnvs][4] {};
    Atom mSrc[numSlots] {}, mDst[numSlots] {}, mLayer[numSlots] {}, mAmt[numSlots] {};

    void bind (juce::AudioProcessorValueTreeState& s)
    {
        auto g = [&] (const juce::String& id)
        {
            auto* a = s.getRawParameterValue (id);
            jassert (a != nullptr);
            return a;
        };
        poly = g ("poly"); mono = g ("mono"); legato = g ("legato"); glide = g ("glide"); bend = g ("bend");
        for (int i = 0; i < numMacros; ++i) macro[i] = g ("macro" + juce::String (i + 1));
        for (int i = 0; i < numLfos; ++i)
        {
            const juce::String p = "lfo" + juce::String (i + 1) + "_";
            lfoRate[i] = g (p + "rate"); lfoShape[i] = g (p + "shape"); lfoSync[i] = g (p + "sync");
            lfoDiv[i] = g (p + "div"); lfoFade[i] = g (p + "fade"); lfoRetrig[i] = g (p + "retrig");
        }
        for (int e = 0; e < numModEnvs; ++e)
        {
            const juce::String p = "me" + juce::String (e + 1) + "_";
            me[e][0] = g (p + "A"); me[e][1] = g (p + "D"); me[e][2] = g (p + "S"); me[e][3] = g (p + "R");
        }
        for (int i = 0; i < numSlots; ++i)
        {
            const juce::String p = "m" + juce::String (i + 1) + "_";
            mSrc[i] = g (p + "src"); mDst[i] = g (p + "dst"); mLayer[i] = g (p + "layer"); mAmt[i] = g (p + "amt");
        }
    }
};

// State shared between the processor and all voices.
struct Shared
{
    LayerParams layer[numLayers];
    GlobalParams global;
    std::atomic<float> bpm { 120.0f }, modWheel { 0.0f }, aftertouch { 0.0f };
    std::atomic<double> lfoPhase[numLfos];
    std::atomic<const SampleData*> userSample[numLayers];
    float lastPitch = -1.0f; // audio thread only

    Shared()
    {
        for (auto& p : lfoPhase) p.store (0.0);
        for (auto& u : userSample) u.store (nullptr);
    }

    double lfoHz (int k) const
    {
        if (global.lfoSync[k]->load() > 0.5f)
            return bpm.load() / 60.0 / divisionBeats ((int) global.lfoDiv[k]->load());
        return global.lfoRate[k]->load();
    }
};

//==============================================================================
class Sound : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

//==============================================================================
struct Grain
{
    bool active = false;
    double pos = 0, inc = 0;
    int age = 0, len = 1;
    float gl = 0, gr = 0;
};

struct LayerState
{
    static constexpr int maxSlots = 21, maxGrains = 48;
    double phase[maxSlots] {}, modPhase[maxSlots] {}, smpPos[maxSlots] {};
    float fmPrev[maxSlots] {}, uniDrift[maxSlots] {};
    double subPhase = 0;
    Svf f1[2], f2[2], f3[2];
    Comb comb[2];
    juce::ADSR amp, fenv;
    float driftVal = 0, driftTarget = 0;
    int driftCounter = 0;
    bool active = false;
    const SampleData* sample = nullptr;
    Grain grains[maxGrains];
    double grainTimer = 0;

    // Karplus-Strong strings for the Pluck engine (one per unison voice)
    static constexpr int maxStrings = 7, stringLen = 4096;
    std::vector<float> strings = std::vector<float> ((size_t) (maxStrings * stringLen), 0.0f);
    int strWrite[maxStrings] {};
    float strLp[maxStrings] {}, exciteLp[maxStrings] {};
    int exciteLeft = 0;
};

//==============================================================================
class Voice : public juce::SynthesiserVoice
{
public:
    explicit Voice (Shared& s) : sh (s) {}

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<Sound*> (s) != nullptr; }
    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote (float, bool allowTailOff) override;
    void pitchWheelMoved (int v) override { bend = (float) (v - 8192) / 8192.0f; }
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>&, int start, int num) override;

    void legatoTo (int midiNote, float velocity);
    bool isHeld() const { return held; }

private:
    static constexpr int chunk = 16;

    void controlTick (int n);
    bool renderLayer (int l, float* outL, float* outR, int pos, int n);
    void renderGranular (int l, LayerState& s, const LayerParams& p, const float* m,
                         const double* groupSemis, int groups, double basePitch, float spread,
                         float* tl, float* tr, int n);
    void updateEnv (int l);

    Shared& sh;
    LayerState st[numLayers];
    juce::ADSR modEnv[numModEnvs];
    double lfoPh[numLfos] {};
    float shCur[numLfos] {}, shPrev[numLfos] {};
    float mods[numLayers][numDst] {};
    float srcVals[numSrc] {};

    int note = 60;
    float vel = 1.0f, bend = 0.0f, randomVal = 0.0f;
    float curPitch = 60.0f, targetPitch = 60.0f;
    double noteTime = 0;
    bool playing = false, held = false;
    juce::Random rng;
};

//==============================================================================
// Synthesiser with polyphony limit plus mono / legato note handling.
class NebSynth : public juce::Synthesiser
{
public:
    void configure (int maxVoices, bool monoMode, bool legatoMode);
    void noteOn (int ch, int midiNote, float velocity) override;
    void noteOff (int ch, int midiNote, float velocity, bool allowTailOff) override;
    void allNotesOff (int ch, bool allowTailOff) override;

protected:
    juce::SynthesiserVoice* findFreeVoice (juce::SynthesiserSound*, int ch, int midiNote, bool steal) const override;

private:
    int polyLimit = 16;
    bool mono = false, legato = false;
    juce::Array<int> heldStack;
    int monoNote = -1;
};
} // namespace neb
