#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <cmath>

namespace neb
{
constexpr double twoPi = juce::MathConstants<double>::twoPi;

//==============================================================================
struct LayerParams
{
    std::atomic<float>* on{}, *level{}, *pan{}, *wave{}, *octave{}, *semi{}, *fine{},
        *unison{}, *detune{}, *spread{}, *sub{}, *noise{}, *drift{},
        *ftype{}, *cutoff{}, *reso{}, *fenv{}, *keytrack{}, *drive{},
        *aA{}, *aD{}, *aS{}, *aR{}, *fA{}, *fD{}, *fS{}, *fR{},
        *lfoRate{}, *lfoCut{}, *lfoPitch{}, *lfoAmp{};

    void bind (juce::AudioProcessorValueTreeState& s, const juce::String& p)
    {
        auto g = [&] (const char* id) { return s.getRawParameterValue (p + id); };
        on = g ("on"); level = g ("level"); pan = g ("pan"); wave = g ("wave");
        octave = g ("octave"); semi = g ("semi"); fine = g ("fine");
        unison = g ("unison"); detune = g ("detune"); spread = g ("spread");
        sub = g ("sub"); noise = g ("noise"); drift = g ("drift");
        ftype = g ("ftype"); cutoff = g ("cutoff"); reso = g ("reso");
        fenv = g ("fenv"); keytrack = g ("keytrack"); drive = g ("drive");
        aA = g ("aA"); aD = g ("aD"); aS = g ("aS"); aR = g ("aR");
        fA = g ("fA"); fD = g ("fD"); fS = g ("fS"); fR = g ("fR");
        lfoRate = g ("lfoRate"); lfoCut = g ("lfoCut");
        lfoPitch = g ("lfoPitch"); lfoAmp = g ("lfoAmp");
    }
};

struct SynthParams
{
    LayerParams layer[2];
    std::atomic<float> modWheel { 0.0f };
};

//==============================================================================
class Sound : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

//==============================================================================
// Zero-delay-feedback state variable filter (Cytomic / Simper topology).
struct Svf
{
    float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1;

    void set (float g, float kk)
    {
        k = kk;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void reset() { ic1 = ic2 = 0; }

    inline void tick (float x, float& lp, float& bp, float& hp)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
};

//==============================================================================
// Band-limited oscillator with continuous morph Sine -> Tri -> Saw -> Square.
inline float polyBlep (double t, double dt)
{
    if (t < dt)        { t /= dt;               return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt)  { t = (t - 1.0) / dt;    return (float) (t * t + t + t + 1.0); }
    return 0.0f;
}

inline float shape (int s, double t, double dt)
{
    switch (s)
    {
        case 0:  return (float) std::sin (twoPi * t);
        case 1:  return (float) (t < 0.25 ? 4.0 * t : (t < 0.75 ? 2.0 - 4.0 * t : 4.0 * t - 4.0));
        case 2:  return (float) (2.0 * t - 1.0) - polyBlep (t, dt);
        default:
        {
            double t2 = t + 0.5; if (t2 >= 1.0) t2 -= 1.0;
            return (t < 0.5 ? 1.0f : -1.0f) + polyBlep (t, dt) - polyBlep (t2, dt);
        }
    }
}

inline float morphOsc (float w, double t, double dt)
{
    int i = (int) w;
    float fr = w - (float) i;
    if (i >= 3) { i = 3; fr = 0.0f; }
    const float a = shape (i, t, dt);
    if (fr <= 0.0001f) return a;
    return a + (shape (i + 1, t, dt) - a) * fr;
}

//==============================================================================
struct LayerState
{
    static constexpr int maxUni = 7;
    double phase[maxUni] {};
    float uniDrift[maxUni] {};
    double subPhase = 0, lfoPhase = 0;
    Svf f1[2], f2[2];
    juce::ADSR amp, fenv;
    float driftVal = 0, driftTarget = 0;
    int driftCounter = 0;
    bool active = false;
};

//==============================================================================
class Voice : public juce::SynthesiserVoice
{
public:
    explicit Voice (SynthParams& p) : params (p) {}

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<Sound*> (s) != nullptr; }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int pitchWheel) override
    {
        note = midiNote;
        vel = velocity;
        bend = (float) (pitchWheel - 8192) / 8192.0f;

        for (int l = 0; l < 2; ++l)
        {
            auto& s = st[l];
            const bool wasActive = playing && s.active;
            s.active = params.layer[l].on->load() > 0.5f;
            if (! s.active) continue;

            s.amp.setSampleRate (getSampleRate());
            s.fenv.setSampleRate (getSampleRate());
            updateEnv (l);

            if (! wasActive)
            {
                s.amp.reset(); s.fenv.reset();
                for (int u = 0; u < LayerState::maxUni; ++u)
                {
                    s.phase[u] = rng.nextDouble();      // free-running "analog" phases
                    s.uniDrift[u] = rng.nextFloat() * 2.0f - 1.0f;
                }
                s.subPhase = 0;
                s.lfoPhase = rng.nextDouble();
                for (int c = 0; c < 2; ++c) { s.f1[c].reset(); s.f2[c].reset(); }
            }
            s.amp.noteOn();
            s.fenv.noteOn();
        }
        playing = true;
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            for (auto& s : st) { s.amp.noteOff(); s.fenv.noteOff(); }
        }
        else
        {
            for (auto& s : st) { s.amp.reset(); s.fenv.reset(); s.active = false; }
            playing = false;
            clearCurrentNote();
        }
    }

    void pitchWheelMoved (int v) override { bend = (float) (v - 8192) / 8192.0f; }
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
    {
        if (! playing) return;

        const double sr = getSampleRate();
        float* outL = out.getWritePointer (0);
        float* outR = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;
        const float depthScale = 1.0f + params.modWheel.load();
        const float velGain = 0.35f + 0.65f * vel;
        bool anyActive = false;

        for (int l = 0; l < 2; ++l)
        {
            auto& s = st[l];
            if (! s.active) continue;
            if (! s.amp.isActive()) { s.active = false; continue; }
            anyActive = true;

            const auto& p = params.layer[l];
            updateEnv (l);

            const int   uni      = juce::jlimit (1, LayerState::maxUni, (int) p.unison->load());
            const float detune   = p.detune->load() / 100.0f;   // semitones
            const float spread   = p.spread->load();
            const float wave     = p.wave->load();
            const float subLvl   = p.sub->load();
            const float noiseLvl = p.noise->load();
            const float level    = p.level->load();
            const float pan      = p.pan->load();
            const float cutoff   = p.cutoff->load();
            const float reso     = p.reso->load();
            const float fenvAmt  = p.fenv->load();
            const float keytrack = p.keytrack->load();
            const float drive    = p.drive->load();
            const int   ftype    = (int) p.ftype->load();
            const float lfoRate  = p.lfoRate->load();
            const float lfoCut   = p.lfoCut->load() * depthScale;
            const float lfoPitch = p.lfoPitch->load() * depthScale;
            const float lfoAmp   = juce::jmin (1.0f, p.lfoAmp->load() * depthScale);
            const float drift    = p.drift->load();

            const float k = 2.0f - 1.97f * reso;
            const double baseSemis = (note - 69) + 12.0 * p.octave->load() + p.semi->load()
                                   + p.fine->load() / 100.0 + bend * 2.0;

            const float panAngle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            const float layerL = std::cos (panAngle) * 1.4142f, layerR = std::sin (panAngle) * 1.4142f;

            double ratio[LayerState::maxUni];
            float gl[LayerState::maxUni], gr[LayerState::maxUni];
            const float norm = 1.0f / std::sqrt ((float) uni);
            for (int u = 0; u < uni; ++u)
            {
                const float pos = uni == 1 ? 0.0f : (float) u / (float) (uni - 1) * 2.0f - 1.0f;
                const double semis = pos * detune + s.uniDrift[u] * drift * 0.08;
                ratio[u] = std::pow (2.0, semis / 12.0);
                const float a = (pos * spread + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                gl[u] = std::cos (a) * norm;
                gr[u] = std::sin (a) * norm;
            }

            const double keyOct = keytrack * (note - 60) / 12.0;
            const float maxFc = (float) juce::jmin (20000.0, sr * 0.45);

            for (int i = 0; i < num; ++i)
            {
                // slow random pitch drift
                if (--s.driftCounter <= 0)
                {
                    s.driftCounter = (int) (sr * 0.05);
                    s.driftTarget = rng.nextFloat() * 2.0f - 1.0f;
                }
                s.driftVal += (s.driftTarget - s.driftVal) * 0.0005f;

                const float lfo = (float) std::sin (twoPi * s.lfoPhase);
                s.lfoPhase += lfoRate / sr;
                if (s.lfoPhase >= 1.0) s.lfoPhase -= 1.0;

                const float fe = s.fenv.getNextSample();
                const float ae = s.amp.getNextSample();

                const double semis = baseSemis + lfo * lfoPitch + s.driftVal * drift * 0.15;
                const double hz = 440.0 * std::pow (2.0, semis / 12.0);

                float xl = 0, xr = 0;
                for (int u = 0; u < uni; ++u)
                {
                    const double dt = juce::jmin (0.49, hz * ratio[u] / sr);
                    const float v = morphOsc (wave, s.phase[u], dt);
                    xl += v * gl[u];
                    xr += v * gr[u];
                    s.phase[u] += dt;
                    if (s.phase[u] >= 1.0) s.phase[u] -= 1.0;
                }

                const float subV = (float) std::sin (twoPi * s.subPhase) * subLvl;
                s.subPhase += hz * 0.5 / sr;
                if (s.subPhase >= 1.0) s.subPhase -= 1.0;
                const float nV = (rng.nextFloat() * 2.0f - 1.0f) * noiseLvl * 0.5f;
                xl += subV + nV;
                xr += subV + nV;

                if ((i & 7) == 0)
                {
                    const double oct = fenvAmt * fe * 6.0 + lfoCut * lfo * 3.0 + keyOct;
                    const float fc = juce::jlimit (20.0f, maxFc, (float) (cutoff * std::pow (2.0, oct)));
                    const float g = (float) std::tan (juce::MathConstants<double>::pi * fc / sr);
                    for (int c = 0; c < 2; ++c) { s.f1[c].set (g, k); s.f2[c].set (g, k); }
                }

                const float dr = 1.0f + drive * 5.0f;
                const float dn = 1.0f / std::tanh (dr);
                float y[2] = { std::tanh (xl * dr) * dn, std::tanh (xr * dr) * dn };
                for (int c = 0; c < 2; ++c)
                {
                    float lp, bp, hp;
                    s.f1[c].tick (y[c], lp, bp, hp);
                    switch (ftype)
                    {
                        case 0: y[c] = lp; break;
                        case 1: { float lp2, bp2, hp2; s.f2[c].tick (lp, lp2, bp2, hp2); y[c] = lp2; break; }
                        case 2: y[c] = bp * 1.5f; break;
                        default: y[c] = hp; break;
                    }
                }

                const float trem = 1.0f - lfoAmp * 0.5f * (1.0f + lfo);
                const float gain = ae * level * velGain * trem * 0.18f;
                outL[start + i] += y[0] * gain * layerL;
                if (outR != nullptr) outR[start + i] += y[1] * gain * layerR;
            }
        }

        if (! anyActive)
        {
            playing = false;
            clearCurrentNote();
        }
    }

private:
    void updateEnv (int l)
    {
        const auto& p = params.layer[l];
        st[l].amp.setParameters ({ p.aA->load(), p.aD->load(), p.aS->load(), p.aR->load() });
        st[l].fenv.setParameters ({ p.fA->load(), p.fD->load(), p.fS->load(), p.fR->load() });
    }

    SynthParams& params;
    LayerState st[2];
    int note = 60;
    float vel = 1.0f, bend = 0.0f;
    bool playing = false;
    juce::Random rng;
};

} // namespace neb
