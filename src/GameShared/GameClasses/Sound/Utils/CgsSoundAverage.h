#ifndef CGS_SOUND_UTILS_AVERAGE_H
#define CGS_SOUND_UTILS_AVERAGE_H

#include "types.hpp"

// ============================================================================
// GameShared/GameClasses/Sound/Utils/CgsSoundAverage.h  (minimal owning home)
//
// CgsSound::Utils::Average<N, T> -- a fixed-window rolling mean over the last N
// recorded samples. Stores samples in a circular buffer indexed by a wrapping
// write cursor, and on every Record() recomputes the arithmetic mean of the
// whole window into a cached member.
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX. No prior source.
// One generic is homed here; each window size the ARTIST build emitted is a thin
// EXPLICIT INSTANTIATION below (NOT a per-N fork).
//
// LAYOUT (X360-authoritative, proven by the Record asm offsets, e.g. <3,float>
// @0x826A8138):
//   maf32Samples[N]   +0x000          (N T-values; for <3,float> +0x000..+0x008)
//   mu8Index          +4*N            (write cursor; lbz/stb -- a single byte)
//   <pad to 4>
//   mf32Average       +4*N+4          (cached window mean; stfs)
//
// Record(sample) for window N:
//   - writes sample into slot mu8Index (stfsx f1, index*4, this);
//   - advances mu8Index := (u8)(mu8Index + 1) % N  (the X360 emits the %N as a
//     reciprocal-multiply; reconstructed here as the equivalent modulo);
//   - seeds mf32Average := 0.0f, then sums every slot maf32Samples[0..N-1] into it;
//   - stores the mean mf32Average := sum * (1/N). The X360 uses a rounded literal
//     for 1/N (e.g. 0.33333334f for N==3, 0.039999999f for N==25); expressed here
//     as 1/N so the generic is correct for any window, while each instantiation's
//     codegen reproduces its own constant.
// ============================================================================

namespace CgsSound
{
namespace Utils
{
    template <u32 N, typename T>
    struct Average
    {
        Average() { Reset(); }

        void Reset()
        {
            for (u32 lu = 0; lu < N; ++lu)
            {
                maf32Samples[lu] = static_cast<T>(0);
            }
            mu8Index    = 0;
            mf32Average = static_cast<T>(0);
        }

        // Record a new sample and refresh the cached window mean.
        // @0x826A8138 (<3,float>) / 0x826A8580 (<4>) / 0x826A80A8 (<5>) /
        // 0x826A8348 (<9>) / 0x826A8218 (<10>) / 0x826A83D8 (<25>).
        void Record(T aSample)
        {
            // samples[index] = sample  (stfsx f1, index*4, this)
            maf32Samples[mu8Index] = aSample;

            // index = (u8)(index + 1) % N   (lbz/stb -- byte-wide cursor)
            mu8Index = static_cast<u8>(static_cast<u8>(mu8Index + 1) % N);

            // average = 0, then sum all N slots (seed +0x...: flt 0.0f)
            mf32Average = static_cast<T>(0);
            T lSum = static_cast<T>(0);
            for (u32 lu = 0; lu < N; ++lu)
            {
                lSum += maf32Samples[lu];
                mf32Average = lSum;
            }

            // mean = sum * (1/N)
            mf32Average = lSum * (static_cast<T>(1) / static_cast<T>(N));
        }

        T GetAverage() const { return mf32Average; }

    private:
        T   maf32Samples[N]; //          +0x000
        u8  mu8Index;        //          +4*N
        T   mf32Average;     //          +4*N+4
    };

    // Explicit instantiations the X360 ARTIST build emitted (Record bodies):
    template struct Average<3u,  f32>; // @0x826A8138
    template struct Average<4u,  f32>; // @0x826A8580
    template struct Average<5u,  f32>; // @0x826A80A8
    template struct Average<9u,  f32>; // @0x826A8348
    template struct Average<10u, f32>; // @0x826A8218
    template struct Average<25u, f32>; // @0x826A83D8

} // namespace Utils
} // namespace CgsSound

#endif // CGS_SOUND_UTILS_AVERAGE_H
