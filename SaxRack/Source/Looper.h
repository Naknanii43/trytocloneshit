// LOOP STATION: four stereo loop tracks at the end of the chain (they record the processed sound).
//
// Each track: one footswitch EMPTY -> REC -> PLAY -> OVERDUB -> PLAY ..., STOP, CLEAR,
// UNDO/REDO (one level), mute, reverse, half speed, level and pan.
// LOCK: the first loop closed becomes the master; every other track's length is rounded up
// to a whole multiple of it and stays phase-locked, so layers never drift.
// SNAP: the master length snaps to whole beats of the host tempo.
// Everything is allocation-free on the audio thread; the undo snapshot is built incrementally.
#pragma once
#include "DSP.h"
#include <atomic>
#include <memory>

namespace rs
{
struct TrackParams
{
    float level = 1.0f, pan = 0.0f;
    bool mute = false, reverse = false, half = false;
};

struct LoopParams
{
    bool on = true, lock = true, snap = false;
    float feedback = 1.0f;
    TrackParams track[4];
};

/** Shared timing between tracks for LOCK mode. */
struct LoopSync
{
    bool lock = true;
    int masterLen = 0;        // 0 = no master yet
    int globalPos = 0;        // master phase at the start of the current block
    int newMasterLen = 0, resetAt = -1;
    int phaseAt (int i) const { return masterLen > 0 ? (globalPos + i) % masterLen : 0; }
};

class LoopTrack
{
public:
    enum State { Empty = 0, Recording, Playing, Overdubbing, Stopped };
    enum Command { Tap = 1, Stop, Clear, Undo };
    static constexpr int binSize = 1024;

    void prepare (double sampleRate, int maxSamples)
    {
        sr = sampleRate;
        fadeCoef = msToCoeff (8.0f, sr);
        if (maxSamples == maxLen) return;      // keep the loop across host re-prepares
        maxLen = maxSamples;
        for (int b = 0; b < 2; ++b)
            for (int c = 0; c < 2; ++c) buf[b][c].assign ((size_t) maxLen, 0.0f);
        epoch.assign ((size_t) maxLen, 0);
        numBins = maxLen / binSize + 1;
        for (auto& b : bins) b.reset (new std::atomic<float>[(size_t) numBins]);
        clear();
    }

    void post (Command c)
    {
        int expected = 0;
        pending.compare_exchange_strong (expected, (int) c);
    }

    /** Records from in, ADDS playback to out. */
    void process (const float* inL, const float* inR, float* outL, float* outR, int n,
                  const TrackParams& p, float feedback, LoopSync& sync, double bpm, bool snap)
    {
        if (maxLen == 0) return;
        if (const int c = pending.exchange (0)) handle ((Command) c, sync, bpm, snap, 0);
        if (state == Empty && outGain < 1.0e-6f) { uiState.store (state); uiLength.store (0); uiPos.store (0); return; }   // idle track: nothing to do

        const double rate = (p.half ? 0.5 : 1.0) * (p.reverse ? -1.0 : 1.0);
        const bool audible = ! p.mute && (state == Playing || state == Overdubbing);
        const float gl = std::cos ((p.pan + 1.0f) * pi * 0.25f) * 1.4142f;
        const float gr = std::sin ((p.pan + 1.0f) * pi * 0.25f) * 1.4142f;
        float peak = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            auto& mL = buf[main][0];
            auto& mR = buf[main][1];
            outGain += ((audible ? p.level : 0.0f) - outGain) * fadeCoef;

            if (state == Recording)
            {
                mL[(size_t) recPos] = inL[i];
                mR[(size_t) recPos] = inR[i];
                updateBin (main, recPos, inL[i], inR[i]);
                ++recPos;
                if ((targetLen > 0 && recPos >= targetLen) || recPos >= maxLen)
                    closeLoop (sync, bpm, snap, i + 1);
                continue;
            }
            if (len <= 0 || (state != Playing && state != Overdubbing && outGain < 1.0e-4f)) continue;

            const int i0 = std::min ((int) playPos, len - 1);
            const float frac = (float) (playPos - i0);
            const int i1 = i0 + 1 < len ? i0 + 1 : 0;
            const float pl = mL[(size_t) i0] + frac * (mL[(size_t) i1] - mL[(size_t) i0]);
            const float pr = mR[(size_t) i0] + frac * (mR[(size_t) i1] - mR[(size_t) i0]);

            if (state == Overdubbing && i0 != lastWrite)
            {
                snapshot (i0);
                mL[(size_t) i0] = mL[(size_t) i0] * feedback + inL[i];
                mR[(size_t) i0] = mR[(size_t) i0] * feedback + inR[i];
                updateBin (main, i0, mL[(size_t) i0], mR[(size_t) i0]);
                lastWrite = i0;
            }

            outL[i] += pl * outGain * gl;
            outR[i] += pr * outGain * gr;
            peak = std::max (peak, std::abs (pl * outGain));

            if (state == Playing || state == Overdubbing)
            {
                playPos += rate;
                if (playPos >= (double) len) playPos -= (double) len;
                if (playPos < 0.0) playPos += (double) len;
            }
        }

        if (state != Recording && sweepPos < len)
        {
            const int end = std::min (len, sweepPos + 16384);
            for (; sweepPos < end; ++sweepPos) snapshot (sweepPos);
            if (sweepPos >= len && dubbedSinceSnapshot) undoReady.store (true);
        }

        uiState.store (state);
        uiLength.store (state == Recording ? std::max (recPos, targetLen) : len);
        uiPos.store (state == Recording ? recPos : (int) playPos);
        uiPeak.store (std::max (peak, uiPeak.load() * 0.9f));
        uiWaiting.store (state == Recording && targetLen > 0);
    }

    bool isEmpty() const { return state == Empty; }
    int length() const { return len; }

    // ---- UI access ----------------------------------------------------------------------
    std::atomic<int> uiState { Empty }, uiLength { 0 }, uiPos { 0 }, uiMain { 0 };
    std::atomic<bool> undoReady { false }, uiWaiting { false };
    std::atomic<float> uiPeak { 0 };
    double getSampleRate() const { return sr; }
    float bin (int i) const
    {
        const int m = uiMain.load();
        return (i >= 0 && i < numBins && bins[m]) ? bins[m][(size_t) i].load (std::memory_order_relaxed) : 0.0f;
    }

private:
    void handle (Command c, LoopSync& sync, double bpm, bool snap, int i)
    {
        switch (c)
        {
            case Tap:
                if (state == Empty)             startRecording (sync, i);
                else if (state == Recording)    requestClose (sync, bpm, snap, i);
                else if (state == Playing)      startOverdub();
                else if (state == Overdubbing)  { state = Playing; lastWrite = -1; }
                else if (state == Stopped)      startPlaying (sync, i);
                break;
            case Stop:
                if (state == Recording) { requestClose (sync, bpm, snap, i); if (state == Recording) break; }
                if (state != Empty) { state = Stopped; lastWrite = -1; }
                break;
            case Clear:
                clear();
                break;
            case Undo:
                if (state == Overdubbing) { state = Playing; lastWrite = -1; }
                if (undoReady.load() && sweepPos >= len)
                {
                    main = 1 - main;
                    uiMain.store (main);
                    ++epochId;
                    sweepPos = len;
                }
                break;
        }
    }

    void startRecording (const LoopSync& sync, int i)
    {
        recPos = 0;
        targetLen = 0;
        phase = sync.lock ? sync.phaseAt (i) : 0;
        clearBins (main);
        state = Recording;
    }

    /** With a master loop, keep recording up to the next whole multiple of it. */
    void requestClose (LoopSync& sync, double bpm, bool snap, int i)
    {
        if (sync.lock && sync.masterLen > 0)
        {
            const int m = sync.masterLen;
            int mult = std::max (1, (recPos + m - 1) / m);
            while (mult > 1 && mult * m > maxLen) --mult;
            targetLen = std::min (maxLen, mult * m);
            if (recPos >= targetLen) closeLoop (sync, bpm, snap, i);
        }
        else closeLoop (sync, bpm, snap, i);
    }

    void startPlaying (const LoopSync& sync, int i)
    {
        playPos = 0;
        if (sync.lock && sync.masterLen > 0 && len > 0)
        {
            const int m = sync.masterLen;
            playPos = (double) (((sync.phaseAt (i) - phase) % m + m) % m % len);
        }
        state = Playing;
    }

    void startOverdub()
    {
        ++epochId;
        if (epochId == 0) { std::fill (epoch.begin(), epoch.end(), 0); epochId = 1; }
        sweepPos = 0;
        undoReady.store (false);
        dubbedSinceSnapshot = true;
        for (int b = 0; b < numBins; ++b) bins[1 - main][(size_t) b].store (bins[main][(size_t) b].load());
        state = Overdubbing;
        lastWrite = -1;
    }

    void snapshot (int idx)
    {
        if (epoch[(size_t) idx] == epochId) return;
        epoch[(size_t) idx] = epochId;
        buf[1 - main][0][(size_t) idx] = buf[main][0][(size_t) idx];
        buf[1 - main][1][(size_t) idx] = buf[main][1][(size_t) idx];
    }

    void closeLoop (LoopSync& sync, double bpm, bool snap, int i)
    {
        int newLen = std::max (1, recPos);
        const bool becomesMaster = sync.lock && sync.masterLen == 0 && sync.newMasterLen == 0;
        if (targetLen > 0) newLen = targetLen;
        else if (snap && bpm > 0 && (becomesMaster || ! sync.lock))
        {
            const double beat = 60.0 / bpm * sr;
            const int beatsCount = std::max (1, (int) std::lround (newLen / beat));
            newLen = std::min (maxLen, (int) std::lround (beatsCount * beat));
        }
        for (int c = 0; c < 2; ++c)
            for (int k = recPos; k < newLen; ++k) buf[main][c][(size_t) k] = 0.0f;
        len = newLen;

        const int fade = std::min (192, len / 4);
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < fade; ++k)
            {
                const float g = (float) k / (float) fade;
                buf[main][c][(size_t) k] *= g;
                buf[main][c][(size_t) (len - 1 - k)] *= g;
            }

        if (becomesMaster) { sync.newMasterLen = len; sync.resetAt = i; phase = 0; }
        // the next sample continues from the top of the loop, which is exactly in phase
        playPos = 0;
        lastWrite = -1;
        sweepPos = len;
        dubbedSinceSnapshot = false;
        undoReady.store (false);
        targetLen = 0;
        state = Playing;
    }

    void clear()
    {
        state = Empty;
        len = recPos = targetLen = 0;
        playPos = 0;
        lastWrite = -1;
        sweepPos = 0;
        dubbedSinceSnapshot = false;
        undoReady.store (false);
        clearBins (0);
        clearBins (1);
    }

    void clearBins (int b)
    {
        if (! bins[b]) return;
        for (int k = 0; k < numBins; ++k) bins[b][(size_t) k].store (0.0f);
    }

    void updateBin (int b, int idx, float l, float r)
    {
        const float v = std::max (std::abs (l), std::abs (r));
        auto& slot = bins[b][(size_t) (idx / binSize)];
        if (idx % binSize == 0) slot.store (v, std::memory_order_relaxed);
        else if (v > slot.load (std::memory_order_relaxed)) slot.store (v, std::memory_order_relaxed);
    }

    double sr = 48000.0;
    int maxLen = 0, numBins = 0;
    std::vector<float> buf[2][2];
    std::vector<uint16_t> epoch;
    uint16_t epochId = 1;
    std::unique_ptr<std::atomic<float>[]> bins[2];
    int main = 0;
    State state = Empty;
    int len = 0, recPos = 0, targetLen = 0, lastWrite = -1, sweepPos = 0, phase = 0;
    double playPos = 0;
    bool dubbedSinceSnapshot = false;
    float outGain = 0, fadeCoef = 0.01f;
    std::atomic<int> pending { 0 };
};

class LoopStation
{
public:
    static constexpr int numTracks = 4;
    static constexpr int maxSamples = 1200000;   // 25 s at 48 kHz per track (12.5 s at 96 kHz)

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const int len = std::min (maxSamples, (int) (sampleRate * 25.0));
        for (auto& t : tracks) t.prepare (sampleRate, len);
        if (len != preparedLen) { sync.masterLen = 0; sync.globalPos = 0; preparedLen = len; }
    }

    void process (float* L, float* R, int n, const LoopParams& p, double bpm)
    {
        handleGlobal();
        sync.lock = p.lock;
        sync.newMasterLen = 0;
        sync.resetAt = -1;
        bool anyContent = false;
        for (auto& t : tracks) anyContent |= ! t.isEmpty();
        if (! anyContent) sync.masterLen = 0;

        // record what the rack produced (before the loops are mixed in)
        float inL[256], inR[256];
        for (int pos = 0; pos < n; pos += 256)
        {
            const int m = std::min (256, n - pos);
            std::copy (L + pos, L + pos + m, inL);
            std::copy (R + pos, R + pos + m, inR);
            if (p.on)
                for (int k = 0; k < numTracks; ++k)
                    tracks[(size_t) k].process (inL, inR, L + pos, R + pos, m, p.track[k], p.feedback, sync, bpm, p.snap);
            if (sync.newMasterLen > 0)
            {
                sync.masterLen = sync.newMasterLen;
                sync.globalPos = (m - sync.resetAt) % sync.masterLen;
                sync.newMasterLen = 0;
                sync.resetAt = -1;
            }
            else if (sync.masterLen > 0)
                sync.globalPos = (sync.globalPos + m) % sync.masterLen;
        }
        uiMasterLen.store (sync.masterLen);
        uiMasterPos.store (sync.globalPos);
    }

    void post (int track, LoopTrack::Command c) { if (track >= 0 && track < numTracks) tracks[(size_t) track].post (c); }
    void stopAll()  { globalCmd.store (1); }
    void clearAll() { globalCmd.store (2); }

    LoopTrack& track (int i) { return tracks[(size_t) i]; }
    std::atomic<int> uiMasterLen { 0 }, uiMasterPos { 0 };

private:
    void handleGlobal()
    {
        const int g = globalCmd.exchange (0);
        if (g == 1) for (auto& t : tracks) t.post (LoopTrack::Stop);
        if (g == 2) for (auto& t : tracks) t.post (LoopTrack::Clear);
    }

    double sr = 48000.0;
    int preparedLen = -1;
    std::array<LoopTrack, numTracks> tracks;
    LoopSync sync;
    std::atomic<int> globalCmd { 0 };
};
} // namespace rs
