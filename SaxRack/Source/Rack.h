// The Reedstorm signal chain.
//
//   IN ─ DYNAMICS ─ OCTAVER ─ SYNTH ─ WAH ─ DRIVE ─ EQ ─┬─ HARMONY ─ MOD ─ DELAY ─ REVERB ─ OUT
//   (mono core: a saxophone is a single voice)            └─ stereo from here on
//
// A YIN pitch tracker listens to the input and feeds the tuner, the octaver's filter,
// the diatonic harmonizer and the tracking synth.
#pragma once
#include "DSP.h"
#include "PitchTracker.h"
#include "Generator.h"
#include "Looper.h"
#include "Gremlins.h"
#include "Mangle.h"
#include "Texture.h"
#include <juce_dsp/juce_dsp.h>
#include <atomic>

namespace rs
{
struct RackParams
{
    float inGain = 0;
    bool dynOn = true; float gateThr = -80, compAmt = 0.3f, compSpeed = 0.5f;
    bool octOn = false; float octSub1 = 0.6f, octSub2 = 0, octUp = 0, octDry = 1;
    bool synOn = false; int synWave = 0, synOct = 1; float synCut = 1400, synRes = 0.3f, synEnv = 0.5f, synGlide = 25, synLvl = 0.5f;
    bool wahOn = false; int wahMode = 0; float wahSens = 0.6f, wahFreq = 380, wahRange = 2.5f, wahQ = 4.5f, wahRate = 1.5f, wahMix = 1;
    bool drvOn = false; int drvType = 0; float drvDrive = 0.4f, drvChar = 0.3f, drvTone = 0.55f, drvLevel = 0, drvMix = 1;
    bool eqOn = true; float eqLowCut = 40, eqBody = 0, eqMid = 0, eqMidF = 1200, eqAir = 0;
    bool harOn = false; int harMode = 0, harKey = 0, harScale = 0, harInt1 = 2, harInt2 = -3; float harLvl1 = 0.6f, harLvl2 = 0, harSpread = 0.6f;
    bool modOn = false; int modType = 0; float modRate = 0.8f, modDepth = 0.5f, modFb = 0.3f, modMix = 0.5f;
    bool dlyOn = false; int dlySync = 6; float dlyTime = 380, dlyFb = 0.35f, dlyTone = 4000, dlyWobble = 0.15f, dlyDuck = 0.3f, dlyMix = 0.3f; bool dlyPP = true;
    bool revOn = false; float revPre = 20, revSize = 0.6f, revDecay = 2.5f, revDamp = 0.4f, revShimmer = 0, revMix = 0.3f;
    GenParams gen;
    GremlinParams grm;
    WhammyParams whm;
    RingParams ring;
    FreezeParams frz;
    SlicerParams gate;
    FormantParams fmt;
    TalkParams talk;
    DoublerParams dbl;
    TremoloParams trem;
    RepeatParams brp;
    SpringParams spring;
    CloudParams cloud;
    LoopParams loop;
    float outWidth = 1, outGain = 0;
    int instrument = 2; float tunerRef = 440;
};

/** Values the editor reads (written once per block by the audio thread). */
struct RackMeters
{
    std::atomic<float> inPeak { 0 }, outPeakL { 0 }, outPeakR { 0 }, gainReduction { 0 };
    std::atomic<float> pitchMidi { 60 }, pitchConfidence { 0 };
    std::atomic<bool> voiced { false };
    std::atomic<float> wahPos { 0 }, harmShift1 { 0 }, harmShift2 { 0 };
};

class Rack
{
public:
    static constexpr int chunk = 64;

    void prepare (double sampleRate);
    void reset();
    /** inR may be null for a mono input. outL/outR may alias inL/inR. */
    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples,
                  const RackParams& p, double bpm, double hostBeat = -1.0);

    RackMeters meters;
    Generator generator;
    Gremlins gremlins;
    Whammy whammy;
    RingMod ringMod;
    Freeze freeze;
    Slicer slicer;
    FormantShifter formant;
    TalkBox talkBox;
    Doubler doubler;
    Tremolo tremolo;
    BeatRepeat beatRepeat;
    SpringReverb springRev;
    GranularCloud cloud;
    LoopStation looper;

    /** Harmony interval (scale steps or semitones) -> semitones for the current note. Public for tests. */
    float harmonyShift (int interval, const RackParams& p) const;
    void setTrackedMidiForTest (float m) { trackedMidi = m; }

private:
    void processChunk (int n, const RackParams& p, double bpm);
    void updateFilters (const RackParams& p);

    double sr = 48000.0;
    float mono[chunk] {}, dryCopy[chunk] {}, L[chunk] {}, R[chunk] {};

    PitchTracker tracker;
    float trackedMidi = 60.0f;   // concert pitch, held through short gaps

    // dynamics
    Fade dynFade; EnvFollower gateEnv; float gateGain = 1, compEnv = 0; bool gateOpen = true;
    // octaver
    Fade octFade; Svf octPre1, octPre2; Svf octPost1, octPost2; EnvFollower octEnv;
    bool octHigh = false, ff1 = false, ff2 = false; PitchShifter octUpShift;
    // synth
    Fade synFade; float synPhase = 0, synSubPhase = 0, synHz = 110; EnvFollower synAmpEnv; Svf synFilter; float synAmp = 0;
    // wah
    Fade wahFade; EnvFollower wahEnv; Svf wahFilter; float wahLfo = 0, wahSmooth = 0;
    // drive
    Fade drvFade; std::unique_ptr<juce::dsp::Oversampling<float>> os; Svf drvTone; OnePole drvDc, drvPre;
    float growlPhase = 0, growlRate = 32; Noise noise;
    // eq
    Fade eqFade; Biquad eqHp1, eqHp2, eqBody, eqMid, eqAir; float lastEq[5] { -1, -1, -1, -1, -1 };
    // harmony
    Fade harFade; PitchShifter harShift1, harShift2; float harRatio1 = 1, harRatio2 = 1;
    float harHeld1 = 0, harHeld2 = 0;
    // modulation
    Fade modFade; DelayLine modDl[2]; float modPhase = 0, rotPhaseLo = 0; float apState[2][6] {}; float modFbState[2] {}; float phaserA[2] { -0.9f, -0.9f };
    Svf rotXover[2];
    // delay
    Fade dlyFade; DelayLine dlyDl[2]; float dlySmooth = 0; OnePole dlyLp[2], dlyHp[2]; EnvFollower dlyDuckEnv;
    float wobPhase = 0, wobRand = 0, wobTarget = 0; int wobCounter = 0;
    // reverb (8-line FDN with shimmer)
    Fade revFade; DelayLine revPre; DelayLine revAp[4]; float revApLen[4] {}; DelayLine revLine[8];
    float revLen[8] {}; OnePole revDamp[8]; float revModPhase = 0; PitchShifter shimmer; float shimmerFb = 0; OnePole shimHp;
    // generator
    Fade genFade, grmFade, whmFade, rngFade, frzFade, gatFade, fmtFade, tlkFade, dblFade, trmFade, brpFade, sprFade, cldFade;
    float crushHold = 0; int crushCount = 0;
    double beatPos = 0;
    float rawIn[chunk] {};
    // output
    float limGain = 1.0f;
    juce::LinearSmoothedValue<float> inGainSm, outGainSm, widthSm;
};
} // namespace rs
