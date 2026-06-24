#include "GameShared/Jobs/FrustumTest/FrustumTest.h"

#include <cstddef>  // offsetof
#include <cstring>  // std::memcpy (models the Xbox XMemCpy / VMX block copies)

// Byte-offset pins: the operator= asm (0x82BDF950) attests these exact region boundaries.
namespace
{
    static_assert(sizeof(CgsGeometric::Frustum) == 0x80, "Frustum must be 128 bytes");
    static_assert(offsetof(CgsSceneManager::FrustumJobQueryInfo, maViewProjection) == 0x500,
                  "maViewProjection must be at +0x500");
    static_assert(offsetof(CgsSceneManager::FrustumJobQueryInfo, max32EntityTypeFlags) == 0x780,
                  "max32EntityTypeFlags must be at +0x780");
    static_assert(offsetof(CgsSceneManager::FrustumJobQueryInfo, muNumQueries) == 0x7A8,
                  "muNumQueries must be at +0x7A8");
}

// CgsSceneManager::FrustumJobQueryInfo::operator= @ 0x82BDF950
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity). The X360 build lowers this
// assignment as a full, contiguous copy of the struct's 0x7AC live bytes, in three regions:
//
//   maFrustum[10]            (0x000..0x500): ten memcpy(dst, src, 0x80) calls
//                                            (addi r3/r4 by 0x80; li r5,0x80; bl memcpy x10)
//   maViewProjection[10]     (0x500..0x780): VMX lvx128/stvx128 16-byte lane copies (40 lanes;
//                                            10 Matrix44 * 4 Vector4 rows)
//   max32EntityTypeFlags[10] + muNumQueries  (0x780..0x7AC): eleven lwz/stw 32-bit word copies
//
// No field is skipped and no byte outside [0,0x7AC) is written (the 4 trailing alignment pad
// bytes at 0x7AC..0x7B0 are left untouched), so this is exactly a complete memberwise copy.
// Reproduced here as the three per-region member copies (memcpy stands in for the Xbox block /
// VMX intrinsic copies), returning *this -- matching the X360 `return this`.
//
// (This assignment operator is invoked by CgsSceneManager::FrustumTestJobData::operator=,
// which copies its embedded mQueryInfo member.)

namespace CgsSceneManager
{
    FrustumJobQueryInfo& FrustumJobQueryInfo::operator=(const FrustumJobQueryInfo& lrSource)
    {
        // Region 1 (0x000..0x500): the 10 swizzled frustums.
        std::memcpy(maFrustum, lrSource.maFrustum, sizeof(maFrustum));
        // Region 2 (0x500..0x780): the 10 view-projection matrices.
        std::memcpy(maViewProjection, lrSource.maViewProjection, sizeof(maViewProjection));
        // Region 3 (0x780..0x7A8): the 10 entity-type filter flag words.
        std::memcpy(max32EntityTypeFlags, lrSource.max32EntityTypeFlags, sizeof(max32EntityTypeFlags));
        // (0x7A8): the live query count.
        muNumQueries = lrSource.muNumQueries;
        return *this;
    }
}
