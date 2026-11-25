// Stereo looper at the end of the chain (records the processed sound).
//
// One footswitch: EMPTY -> REC -> PLAY -> OVERDUB -> PLAY ...
// STOP, CLEAR, UNDO/REDO (one level, swaps the last overdub in and out), reverse,
// half speed, overdub feedback and optional snap-to-beat loop length.
// Everything runs allocation-free on the audio thread; the undo snapshot is built
// incrementally so there are never CPU spikes.
#pragma once
#include "DSP.h"
#include <atomic>
#include <memory>

namespace rs
{
struct LoopParams
{
    bool on = true;
    float level = 1.0f, feedback = 1.0f;
    bool reverse = false, half = false, sync = false;
};

class Looper
{
public:
    enum State { Empty = 0, Recording, Playing, Overdubbing, Stopped };
    enum Command { Tap = 1, Stop, Clear, Undo };
    static constexpr int binSize = 1024;
    static constexpr float maxSeconds = 40.0f;

    void prepare (double sampleRate)
    {
        const int needed = (int) (sampleRate * maxSeconds);
        if (needed != maxLen)
        {
            maxLen = needed;
            for (int b = 0; b < 2; ++b)
                for (int c = 0; c < 2; ++c) buf[b][c].assign ((size_t) maxLen, 0.0f);
            epoch.assign ((size_t) maxLen, 0);
            numBins = maxLen / binSize + 1;
            for (auto& b : bins) b.reset (new std::atomic<float>[(size_t) numBins]);
            clear();
        }
        sr = sampleRate;
        fadeCoef = msToCoeff (8.0f, sr);
    }

    /** Queue a command (any thread). */
    void post (Command c)
    {
        int expected = 0;
        pending.compare_exchange_strong (expected, (int) c);
    }

    void process (float* L, float* R, int n, const LoopParams& p, double bpm)
    {
        if (maxLen == 0) return;
        lastSync = p.sync;
        if (const int c = pending.exchange (0)) handle ((Command) c, bpm);

        auto& mL = buf[main][0];
        auto& mR = buf[main][1];
        const double rate = (p.half ? 0.5 : 1.0) * (p.reverse ? -1.0 : 1.0);
        const bool audible = p.on && (state == Playing || state == Overdubbing);

        for (int i = 0; i < n; ++i)
        {
            const float inL = L[i], inR = R[i];
            outGain += ((audible ? p.level : 0.0f) - outGain) * fadeCoef;

            if (state == Recording)
            {
                mL[(size_t) recPos] = inL;
                mR[(size_t) recPos] = inR;
                updateBin (main, recPos, inL, inR);
                if (++recPos >= maxLen) closeLoop (bpm, p.sync);
                continue;
            }
            if (len <= 0 || (state != Playing && state != Overdubbing && outGain < 1.0e-4f)) continue;

            const int i0 = (int) playPos;
            const float frac = (float) (playPos - i0);
            const int i1 = i0 + 1 < len ? i0 + 1 : 0;
            const float pl = mL[(size_t) i0] + frac * (mL[(size_t) i1] - mL[(size_t) i0]);
            const float pr = mR[(size_t) i0] + frac * (mR[(size_t) i1] - mR[(size_t) i0]);

            if (state == Overdubbing && i0 != lastWrite)
            {
                snapshot (i0);
                mL[(size_t) i0] = mL[(size_t) i0] * p.feedback + inL;
                mR[(size_t) i0] = mR[(size_t) i0] * p.feedback + inR;
                updateBin (main, i0, mL[(size_t) i0], mR[(size_t) i0]);
                lastWrite = i0;
            }

            L[i] = inL + pl * outGain;
            R[i] = inR + pr * outGain;

            if (state == Playing || state == Overdubbing)
            {
                playPos += rate;
                if (playPos >= (double) len) playPos -= (double) len;
                if (playPos < 0.0) playPos += (double) len;
            }
        }

        // finish the undo snapshot in the background, a slice per block
        if (state != Recording && sweepPos < len)
        {
            const int end = std::min (len, sweepPos + 16384);
            for (; sweepPos < end; ++sweepPos) snapshot (sweepPos);
            if (sweepPos >= len && dubbedSinceSnapshot) undoReady.store (true);
        }

        uiState.store (state);
        uiLength.store (state == Recording ? recPos : len);
        uiPos.store (state == Recording ? recPos : (int) playPos);
    }

    // ---- UI access -------------------------------------------------------------------------
    std::atomic<int> uiState { Empty }, uiLength { 0 }, uiPos { 0 };
    std::atomic<bool> undoReady { false };
    std::atomic<int> uiMain { 0 };
    int getMaxLength() const { return maxLen; }
    double getSampleRate() const { return sr; }
    float bin (int i) const
    {
        const int m = uiMain.load();
        return (i >= 0 && i < numBins && bins[m]) ? bins[m][(size_t) i].load (std::memory_order_relaxed) : 0.0f;
    }

private:
    void handle (Command c, double bpm)
    {
        switch (c)
        {
            case Tap:
                if (state == Empty)            { recPos = 0; clearBins (main); state = Recording; }
                else if (state == Recording)   closeLoop (bpm, lastSync);
                else if (state == Playing)     startOverdub();
                else if (state == Overdubbing) { state = Playing; lastWrite = -1; }
                else if (state == Stopped)     { playPos = 0; state = Playing; }
                break;
            case Stop:
                if (state == Recording) closeLoop (bpm, lastSync);
                if (state != Empty) { state = Stopped; playPos = 0; lastWrite = -1; }
                break;
            case Clear:
                clear();
                break;
            case Undo:
                if (state == Overdubbing) { state = Playing; lastWrite = -1; }
                if (undoReady.load() && sweepPos >= len)
                {
                    main = 1 - main;                 // swap layers: undo, and press again to redo
                    uiMain.store (main);
                    ++epochId;                       // both buffers are complete copies; re-sync on next dub
                    sweepPos = len;
                }
                break;
        }
    }

    void startOverdub()
    {
        ++epochId;                 // everything must be re-snapshotted before it is overwritten
        if (epochId == 0) { std::fill (epoch.begin(), epoch.end(), 0); epochId = 1; }
        sweepPos = 0;
        undoReady.store (false);
        dubbedSinceSnapshot = true;
        for (int i = 0; i < numBins; ++i) bins[1 - main][(size_t) i].store (bins[main][(size_t) i].load());
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

    void closeLoop (double bpm, bool sync)
    {
        int newLen = std::max (1, recPos);
        if (sync && bpm > 0)
        {
            const double beat = 60.0 / bpm * sr;
            const int beatsCount = std::max (1, (int) std::lround (newLen / beat));
            newLen = std::min (maxLen, (int) std::lround (beatsCount * beat));
            for (int c = 0; c < 2; ++c)
                for (int i = recPos; i < newLen; ++i) buf[main][c][(size_t) i] = 0.0f;
        }
        len = newLen;
        const int fade = std::min (256, len / 4);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < fade; ++i)
            {
                const float g = (float) i / (float) fade;
                buf[main][c][(size_t) i] *= g;
                buf[main][c][(size_t) (len - 1 - i)] *= g;
            }
        playPos = 0;
        lastWrite = -1;
        sweepPos = len;
        dubbedSinceSnapshot = false;
        undoReady.store (false);
        state = Playing;
    }

    void clear()
    {
        state = Empty;
        len = recPos = 0;
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
        for (int i = 0; i < numBins; ++i) bins[b][(size_t) i].store (0.0f);
    }

    void updateBin (int b, int idx, float l, float r)
    {
        const int bi = idx / binSize;
        const float v = std::max (std::abs (l), std::abs (r));
        auto& slot = bins[b][(size_t) bi];
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
    int len = 0, recPos = 0, lastWrite = -1, sweepPos = 0;
    double playPos = 0;
    bool dubbedSinceSnapshot = false;
    float outGain = 0, fadeCoef = 0.01f;
    std::atomic<int> pending { 0 };
public:
    bool lastSync = false;   // sync setting seen at the last block (used when a Tap closes the loop)
};
} // namespace rs
