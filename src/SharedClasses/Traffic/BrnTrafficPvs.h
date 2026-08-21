#pragma once

// =============================================================================
// BrnTrafficPvs.h  (NEW OWNING HEADER)
//
// Home for BrnTraffic::Pvs -- the traffic potentially-visible-set grid that maps a
// world point to its traffic "hull" cell and gives the per-cell visible-hull set.
//
// LAYOUT is DWARF-authoritative
// (references/DecFIGS/dwarfdump/SharedClasses/Traffic/BrnTrafficPvs.h, struct @ :46):
//   Vector3                mGridMin;        // :104  +0x00 (16B SIMD lane)
//   Vector3                mCellSize;       // :105  +0x10
//   Vector3                mRecipCellSize;  // :106  +0x20
//   uint32_t               muNumCells_X;    // :107  +0x30
//   uint32_t               muNumCells_Z;    // :108  +0x34
//   uint32_t               muNumCells;      // :109  +0x38
//   Set<uint16_t,8u>*      mpaHullPvs;      // :111  +0x3C
//
// The X360 asm pins every offset used by the two owned accessors:
//   GetHullIndexForPoint @ 0x82208090 reads mGridMin@+0 (`lvx128 v0,r0,r3`),
//     mRecipCellSize@+0x20 (`lvx128 v13,r3,r11` r11=0x20), muNumCells_X@+0x30,
//     muNumCells_Z@+0x34, muNumCells@+0x38.
//   GetHullIndexForPoint(Vector3, s32&, s32&) @ 0x827106B8 reads the same five and
//     writes the two clamped coordinates back through its reference arguments.
//   GetHullPvs @ 0x82705FB0 reads muNumCells@+0x38, mpaHullPvs@+0x3C, and indexes
//     it with a 20-byte stride == sizeof(Set<uint16_t,8u>) (8*u16 + u32 length).
//
// The bake-in assert path "..\\..\\..\\SharedClasses\\Traffic/BrnTrafficPvs.h"
// (lines 141 / 204 of the original) fixes the home.
//
// X360 NOTE: pointers are 4 bytes on X360, 8 on host. mpaHullPvs is pinned BY NAME;
// the +0x3C absolute offset is not static_asserted across the pointer because the
// pointer width differs between target and host.
// =============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                 // Vector3
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsSet.h"       // Set<T,N>

namespace BrnTraffic
{
    // BrnTrafficPvs.h:46 -- the traffic PVS grid.
    class Pvs
    {
    public:
        // BrnTrafficPvs.h:57 -- map a world point to its cell/hull index.
        // X360 @ 0x82208090. asm:
        //   scaled = (lPoint - mGridMin) * mRecipCellSize;   (vsubfp + vmulfp128)
        //   liX = clamp((int)scaled.x, 0, muNumCells_X - 1);
        //   liZ = clamp((int)scaled.z, 0, muNumCells_Z - 1);
        //   luIndex = muNumCells_X * liZ + liX;
        //   assert(luIndex < muNumCells);
        //   return luIndex;
        u32 GetHullIndexForPoint(Vector3 lPoint) const;

        // ⭐⭐ ADDITIVE 2026-08-21 (wave T1 round 2, cluster R2A). THE OVERLOAD THAT UNBLOCKS
        // TrafficEntityModule::UpdateRaceCarHulls @0x82721460 (blocker B1 of round 1).
        //
        // BrnTrafficPvs.h:64 -- the SAME mapping as the one-argument form, but it also HANDS
        // BACK the clamped grid coordinates, which is what lets a caller walk a rectangle of
        // cells instead of resolving a single point. Round 1 recorded it as "an unnamed helper
        // at X360 0x827106B8"; it is not unnamed at all -- the DecFIGS DWARF
        // (dwarfdump/SharedClasses/Traffic/BrnTrafficPvs.h) declares
        //     uint32_t GetHullIndexForPoint(Vector3, int32_t&, int32_t&) const;   // :64
        // and IDA simply left the X360 body as `sub_827106B8` because the symbol did not
        // survive. Identification is exact, not by resemblance:
        //   * same instruction sequence as the :57 form (lvx mGridMin@+0 / vsubfp / lvx
        //     mRecipCellSize@+0x20 / vmulfp128 / two fctiwz), same clamps against
        //     muNumCells_X@+0x30 and muNumCells_Z@+0x34, same `muNumCells_X * z + x`;
        //   * the ONLY differences are the two `stfiwx f0, 0, r4/r5` stores through the two
        //     extra pointer arguments and the write-back of each clamp (`stw r11, 0(r4)` at
        //     0x8271070C / 0x82710724 and `stw r11, 0(r5)` at 0x82710738 / 0x82710750), so the
        //     caller sees the CLAMPED indices, not the raw ones;
        //   * the baked assert line differs exactly as two adjacent inline bodies in one
        //     header must: the :57 form fires "luIndex < muNumCells" at BrnTrafficPvs.h:141
        //     (0x82208140 `li r5, 0x8D`), this one at :171 (0x82710778 `li r5, 0xAB`).
        //
        // Signature note: the DWARF spells the two outputs as `int32_t&`; Hex-Rays renders
        // them as `int*` because that is how a reference is passed. References here.
        u32 GetHullIndexForPoint(Vector3 lPoint, s32& lriOutX, s32& lriOutZ) const;

        // ⭐ ADDITIVE 2026-08-21 (wave T1 round 2, cluster R2A).
        // BrnTrafficPvs.h:70 -- the already-decomposed form: given grid coordinates, produce
        // the cell/hull index. It has NO standalone X360 symbol (it is not in
        // progress/identity.json) because every caller inlines it, so this is an INLINING
        // REVERSAL, not a new invention -- AGENTS.md's "Inlining reversal" de-optimisation.
        //
        // The recovered call site is TrafficEntityModule::UpdateRaceCarHulls' rectangle walk,
        // 0x827216A4..0x827216D4:
        //     lwz  r11, 8(r3)            ; mpData->mpPvs
        //     lwz  r10, 0x30(r11)        ; muNumCells_X
        //     lwz  r9,  0x38(r11)        ; muNumCells
        //     mullw r11, r10, r31        ; muNumCells_X * liZ
        //     add  r30, r11, r29         ;               + liX
        //     cmplw r30, r9 ; blt ->     ; assert(luIndex < muNumCells)
        //     FireAssert("luIndex < muNumCells", "..\\..\\..\\SharedClasses\\Traffic/BrnTrafficPvs.h", 0xBC == 188)
        // -- the :188 assert line is this body's own, sitting between the :171 form above and
        // GetHullPvs' :204 below, which is the order the DWARF declares them in (:64/:70/:75).
        // Note there is NO clamp here: the caller has already clamped.
        u32 GetHullIndexForIndices(s32 liX, s32 liZ) const;

        // BrnTrafficPvs.h:75 -- the visible-hull set for a cell index.
        // X360 @ 0x82705FB0. asm: assert(luIndex < muNumCells);
        //   return mpaHullPvs[luIndex];   (20-byte stride)
        const Set<u16, 8>& GetHullPvs(u32 luIndex) const;

        // BrnTrafficPvs.h:85 / :90 -- load-time pointer relocation. X360 @0x827623E8 /
        // @0x827624A0: exactly ONE pointer slot (mpaHullPvs), plus the `this` 16-byte
        // alignment guard (BrnTrafficPvs.cpp:45) and a debug walk of every cell's Set
        // asserting it was Constructed. Called from TrafficData::FixUp / FixDown.
        void FixUp(const void* lpBaseData);
        void FixDown(const void* lpBaseData);

    private:
        Vector3        mGridMin;        // :104  +0x00
        Vector3        mCellSize;       // :105  +0x10
        Vector3        mRecipCellSize;  // :106  +0x20
        u32            muNumCells_X;    // :107  +0x30
        u32            muNumCells_Z;    // :108  +0x34
        u32            muNumCells;      // :109  +0x38
        Set<u16, 8>*   mpaHullPvs;      // :111  +0x3C
    };
}
