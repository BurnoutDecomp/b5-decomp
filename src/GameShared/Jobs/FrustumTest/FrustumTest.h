#pragma once

// CgsSceneManager::FrustumJobQueryInfo — the per-frame batch of frustum-cull queries the
// FrustumTest job runs (up to KU_FRUSTUM_JOB_MAX_NUM_QUERIES == 10): for each query, the
// swizzled view frustum, its view-projection matrix, and the entity-type filter flags, plus
// the live query count. Layout + member names recovered from the DecFIGS DWARF
// (FrustumTest.h:49-54).
//
// Byte layout (validated against the FrustumJobQueryInfo::operator= block copy @0x82BDF950,
// which copies the whole 0x7AC-byte struct):
//   +0x000  Frustum   maFrustum[10]            ; 10 * 0x80  -> 10 memcpy(0x80) blocks (0..0x500)
//   +0x500  Matrix44  maViewProjection[10]     ; 10 * 0x40  -> VMX lvx128/stvx128 copies (0x500..0x780)
//   +0x780  uint32_t  max32EntityTypeFlags[10] ; 10 * 4     -> 10 lwz/stw word copies (0x780..0x7A8)
//   +0x7A8  uint32_t  muNumQueries             ; 1  * 4     -> the 11th lwz/stw word copy (0x7A8)
// sizeof == 0x7B0 (0x7AC rounded up to the 16-byte alignment of the embedded vec4/matrix lanes).

#include "types.hpp"
#include "BrnCommonTypes.h"  // Matrix44 (64-byte / 16-aligned)
#include "GameShared/GameClasses/Geometric/Primitives/CgsFrustum.h"  // CgsGeometric::Frustum (128 bytes)

namespace CgsSceneManager
{
    // FrustumTest.h:46 (DWARF).
    const u32 KU_FRUSTUM_JOB_MAX_NUM_QUERIES = 10;

    struct FrustumJobQueryInfo
    {
        CgsGeometric::Frustum maFrustum[KU_FRUSTUM_JOB_MAX_NUM_QUERIES];            // +0x000
        Matrix44              maViewProjection[KU_FRUSTUM_JOB_MAX_NUM_QUERIES];     // +0x500
        u32                   max32EntityTypeFlags[KU_FRUSTUM_JOB_MAX_NUM_QUERIES]; // +0x780
        u32                   muNumQueries;                                         // +0x7A8

        // operator= @ 0x82BDF950 — full memberwise copy of all 0x7AC live bytes (the X360
        // lowers it as memcpy(0x80) x10 over maFrustum, then VMX lvx128/stvx128 lane copies
        // over maViewProjection, then 11 lwz/stw word copies over max32EntityTypeFlags +
        // muNumQueries). No field is skipped; semantic parity == a complete struct copy.
        FrustumJobQueryInfo& operator=(const FrustumJobQueryInfo& lrSource);
    };
}
