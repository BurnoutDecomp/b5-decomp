#include "GameSource/Replays/BrnReplayQuantisedVector4.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// BrnReplays::QuantisedVector4Compression, reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   ctor @ 0x8264C7E8: stores the four per-lane bit counts at +0..+3, asserts
//   lMin[i] < lMax[i] for each lane i (the X360 body computes each compare with a
//   vperm-extract of lane i from both vectors followed by vcmpgtfp comparing
//   lMax[i] > lMin[i]; the scalar reconstruction reads the named Vector4 lane), then
//   stores mMin at +16 and mMax at +32 (16-byte vector stores).
//
// The per-lane asserts cite GameSource/Replays/BrnReplayQuantisedVector4.h lines
// 132-135 in the X360 build; the CGS_ASSERT substitution carries __FILE__/__LINE__
// of this reconstruction instead (semantically equivalent -- the boot-trace milestone
// runs this ctor with valid ranges, so the asserts never fire).

namespace BrnReplays
{
    void QuantisedVector4Compression::_AssertLayout()
    {
        static_assert(offsetof(QuantisedVector4Compression, maBitCounts) == 0x00,
                      "maBitCounts @0x00");
        static_assert(offsetof(QuantisedVector4Compression, mMin) == 0x10, "mMin @0x10");
        static_assert(offsetof(QuantisedVector4Compression, mMax) == 0x20, "mMax @0x20");
        static_assert(sizeof(QuantisedVector4Compression) == 0x30,
                      "sizeof QuantisedVector4Compression == 0x30");
    }

    QuantisedVector4Compression::QuantisedVector4Compression(
        const Vector4& lrMin, const Vector4& lrMax,
        u8 luBitsX, u8 luBitsY, u8 luBitsZ, u8 luBitsW)
    {
        // +0..+3: stb r4..r7, 0..3(r31)
        maBitCounts[0] = luBitsX;
        maBitCounts[1] = luBitsY;
        maBitCounts[2] = luBitsZ;
        maBitCounts[3] = luBitsW;

        // Per-lane range sanity: lMin[i] < lMax[i] (X360: vcmpgtfp lMax[i] > lMin[i]).
        CGS_ASSERT(lrMin.x < lrMax.x, "lMin[0] < lMax[0]");
        CGS_ASSERT(lrMin.y < lrMax.y, "lMin[1] < lMax[1]");
        CGS_ASSERT(lrMin.z < lrMax.z, "lMin[2] < lMax[2]");
        CGS_ASSERT(lrMin.w < lrMax.w, "lMin[3] < lMax[3]");

        // +16 / +32: stvx128 v127(lMin)/v126(lMax)
        mMin = lrMin;
        mMax = lrMax;
    }
}
