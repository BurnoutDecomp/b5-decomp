#ifndef CGS_SOUND_PLAYBACK_VOICE_PROFILING_DATA_H
#define CGS_SOUND_PLAYBACK_VOICE_PROFILING_DATA_H

#include "types.hpp"

// ============================================================================
// GameShared/GameClasses/Sound/Playback/CgsVoiceProfilingData.h  (DWARF home)
//
// CgsSound::Playback::VoiceProfilingData<N> -- a fixed-window rolling timing
// statistic. It keeps the last N submitted sample times in a history ring, tracks
// the running min/max/most-recent, and reports the mean over the window.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The bodied member this TU owns is the
// explicit instantiation VoiceProfilingData<128>::SubmitNewTime @ 0x8268EDB0 (called
// by BrnSound::Debug::Statistics::RenderHUD). VoiceProfilingData<128> is an EXPLICIT
// INSTANTIATION of this generic -- not a fork; the generic is defined here and
// instantiated below.
//
// LAYOUT (X360-authoritative, DWARF CgsVoiceProfilingData.h:100..105, proven by the
// SubmitNewTime asm offsets for N==128):
//   mafAveragingHistory[N]   +0x000        (N floats; for N==128, +0x000..+0x1FC)
//   mfMaximumTime            +0x200 (=4*N) lfs/stfs 0x200(r3)
//   mfMinimumTime            +0x204        lfs/stfs 0x204(r3)
//   mfAverageTime            +0x208        stfs     0x208(r3)
//   mfMostRecentTime         +0x20C        stfs     0x20C(r3)
//
// SubmitNewTime(start, scale) computes time = start*scale, then:
//   - records it as most-recent (mfMostRecentTime, +0x20C);
//   - raises mfMaximumTime if time > current max;
//   - lowers mfMinimumTime if time < current min, OR if min is exactly 0.0 (the
//     uninitialized/first-sample sentinel -- X360 compares min against flt_82001CC0
//     == 0.0f and treats a zero min as "no min yet");
//   - shifts the history ring down by one (each slot copies its predecessor) while
//     accumulating the running sum, writes the new time into slot 0, and stores the
//     window mean (sum * 1/N) into mfAverageTime.
//
// NOTE: 1/N == 1/128 == 0.0078125f is the X360 literal flt_820AE0B8 the average
// multiply uses; expressed generically here as 1.0f / N so the generic is correct
// for any window size while the <128> instantiation reproduces the exact constant.
// ============================================================================

namespace CgsSound
{
namespace Playback
{
    // CgsVoiceProfilingData.h:44 (DWARF). N == the averaging-window length.
    template <u32 N>
    struct VoiceProfilingData
    {
        // CgsVoiceProfilingData.h:47. Zero-initialise the window + stats.
        VoiceProfilingData() { Reset(); }

        // CgsVoiceProfilingData.h:62.
        void Reset()
        {
            for (u32 lu = 0; lu < N; ++lu)
            {
                mafAveragingHistory[lu] = 0.0f;
            }
            mfMaximumTime    = 0.0f;
            mfMinimumTime    = 0.0f;
            mfAverageTime    = 0.0f;
            mfMostRecentTime = 0.0f;
        }

        // CgsVoiceProfilingData.h:114 @ 0x8268EDB0 (<128> instantiation).
        // af32Start * af32Scale == the elapsed time for this sample.
        void SubmitNewTime(f32 af32Start, f32 af32Scale)
        {
            const f32 lf32Time = af32Start * af32Scale;

            // Record the most-recent sample (+0x20C). (X360 stores this BEFORE the
            // min/max compares; functionally order-independent.)
            mfMostRecentTime = lf32Time;

            // Raise the running maximum (+0x200) if exceeded.
            if (lf32Time > mfMaximumTime)
            {
                mfMaximumTime = lf32Time;
            }

            // Lower the running minimum (+0x204) if undercut, OR seed it on the first
            // sample (a min of exactly 0.0f is the "no min yet" sentinel).
            if (lf32Time < mfMinimumTime || mfMinimumTime == 0.0f)
            {
                mfMinimumTime = lf32Time;
            }

            // Shift the history ring down one slot (slot i := slot i-1) from the top
            // down to slot 1, accumulating the running sum as we go; the new time is
            // folded into the sum and written into slot 0. (X360: r11 walks from
            // this+0x1FC downward for N-1 iterations summing *(p-1) into the running
            // total seeded with lf32Time, copying *(p-1) into *p.)
            f32 lf32Sum = lf32Time;
            for (u32 lu = N - 1; lu != 0; --lu)
            {
                const f32 lf32Prev = mafAveragingHistory[lu - 1];
                mafAveragingHistory[lu] = lf32Prev;
                lf32Sum += lf32Prev;
            }
            mafAveragingHistory[0] = lf32Time;

            // Window mean (+0x208): sum * (1/N).
            mfAverageTime = lf32Sum * (1.0f / static_cast<f32>(N));
        }

        // CgsVoiceProfilingData.h:156/163/170/149/178 -- accessor family.
        f32 GetMaximumTime() const            { return mfMaximumTime; }
        f32 GetMinimumTime() const            { return mfMinimumTime; }
        f32 GetAverageTime() const            { return mfAverageTime; }
        f32 GetMostRecentTime() const         { return mfMostRecentTime; }
        f32 GetTimeByIndex(u32 au32Index) const { return mafAveragingHistory[au32Index]; }

    private:
        f32 mafAveragingHistory[N]; // :100  +0x000
        f32 mfMaximumTime;          // :102  +4*N
        f32 mfMinimumTime;          // :103
        f32 mfAverageTime;          // :104
        f32 mfMostRecentTime;       // :105
    };

    // Explicit instantiation the X360 ARTIST build emitted (SubmitNewTime @0x8268EDB0).
    // The single bodied function this TU owns lives on this instantiation.
    template struct VoiceProfilingData<128u>;

    // Explicit instantiation emitted for the <16> window (SubmitNewTime @0x8268EE28,
    // called by BrnSound::Debug::Statistics::RenderHUD). N==16 => history result[0..15],
    // mfMaximumTime=result[16], mfMinimumTime=result[17], mfAverageTime=result[18],
    // mfMostRecentTime=result[19]; window mean multiply is the X360 literal 0.0625f==1/16.
    template struct VoiceProfilingData<16u>;

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_VOICE_PROFILING_DATA_H
