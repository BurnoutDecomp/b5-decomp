#pragma once

#include "types.hpp"

// CgsNetwork::FloatQuantiser
//
// Canonical class home for the bounded-float bit quantiser. A value in the
// inclusive range [lfMin, lfMax] is packed into liNumBits by mapping it onto an
// integer increment across the range; the inverse recovers the float from the
// packed increment. Callee-declaration home for CgsSmartBitStream::GetQuantisedFloat
// (@ 0x8264AFA0), which calls UnPack(lpfValue, lfMin, lfMax, liNumBits, packed).
//
// Members/signatures are DWARF-shaped (CgsFloatQuantiser.h); the bodies live in the
// FloatQuantiser TU (not homed by this wave). Only the declarations are needed for
// the compile-only gate.
//
//   Pack   -- clamp value to [min,max] and emit the packed increment + bit width.
//   UnPack -- reconstruct *value from (min, max, numBits, packed).
//   GetNumBits / GetIncrement -- range<->bit-width helpers (GetIncrement private).

namespace CgsNetwork
{
    // The console's float-to-integer conversions (fctiwz / fctidz) saturate: a value past the
    // integer range becomes the nearest bound and NaN becomes the minimum. A plain C++ cast of
    // an out-of-range value is undefined (x64 yields the minimum for both). The resolution-form
    // quantiser counts the steps of an unbounded range (max FLT_MAX) through them.
    inline s32 SaturateToS32(double ldValue)
    {
        if (!(ldValue >= -2147483648.0))
            return -2147483647 - 1;
        if (ldValue >= 2147483647.0)
            return 0x7FFFFFFF;
        return static_cast<s32>(ldValue);
    }

    inline s64 SaturateToS64(double ldValue)
    {
        if (!(ldValue >= -9223372036854775808.0))
            return -9223372036854775807ll - 1;
        if (ldValue >= 9223372036854775807.0)
            return 0x7FFFFFFFFFFFFFFFll;
        return static_cast<s64>(ldValue);
    }

    struct FloatQuantiser
    {
        static void Pack(float lfValue, float lfMin, float lfMax, s32 liNumBits,
                         u32* lpuPackedValue);
        static void UnPack(float* lpfValue, float lfMin, float lfMax, s32 liNumBits,
                           u32 luPackedValue);
        // Resolution form: one step is 2 * lfResolution, and the bit count is the smallest
        // that holds every step across [lfMin, lfMax] (reported through lpiNumBitsUsed).
        static void Pack(float lfValue, float lfMin, float lfMax, float lfResolution,
                         u32* lpuPackedValue, s32* lpiNumBitsUsed);
        static void UnPack(float* lpfValue, float lfMin, float lfMax, float lfResolution,
                           u32 luPackedValue);
        static s32  GetNumBits(float lfMin, float lfMax, float lfResolution);
        static bool PackAndUnPackTest(float lfValue, float lfMin, float lfMax,
                                      s32 liNumBits);

    private:
        static float GetIncrement(float lfMin, float lfMax, s32 liNumBits);
    };
}
