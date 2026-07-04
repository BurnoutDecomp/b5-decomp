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
    struct FloatQuantiser
    {
        static void Pack(float lfValue, float lfMin, float lfMax, s32 liNumBits,
                         u32* lpuPackedValue);
        static void UnPack(float* lpfValue, float lfMin, float lfMax, s32 liNumBits,
                           u32 luPackedValue);
        static s32  GetNumBits(float lfMin, float lfMax, float lfResolution);
        static bool PackAndUnPackTest(float lfValue, float lfMin, float lfMax,
                                      s32 liNumBits);

    private:
        static float GetIncrement(float lfMin, float lfMax, s32 liNumBits);
    };
}
