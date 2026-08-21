#pragma once

// =============================================================================
// BrnTraffic::Pvs -- the traffic potentially-visible-set grid: maps a world point
// to its traffic "hull" cell and gives the per-cell visible-hull set. Layout is
// DWARF-authoritative (dwarfdump/SharedClasses/Traffic/BrnTrafficPvs.h @ :46,
// members :104-:111); each member below carries its console offset.
//
// The asm pins every offset the accessors use: GetHullIndexForPoint @0x82208090
// reads mGridMin@+0, mRecipCellSize@+0x20, muNumCells_X@+0x30, muNumCells_Z@+0x34,
// muNumCells@+0x38; GetHullPvs @0x82705FB0 indexes mpaHullPvs@+0x3C with a 20-byte
// stride == sizeof(Set<uint16_t,8u>). The baked assert path
// "..\\..\\..\\SharedClasses\\Traffic/BrnTrafficPvs.h" fixes the home.
//
// X360 pointers are 4 bytes, host pointers 8, so mpaHullPvs is pinned BY NAME and
// its +0x3C offset is not static_asserted.
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

        // BrnTrafficPvs.h:64. X360 @ 0x827106B8 (IDA leaves it as sub_827106B8; the
        // DWARF names it). The same mapping as the :57 form, but it also hands back the
        // clamped grid coordinates, which lets a caller walk a rectangle of cells --
        // TrafficEntityModule::UpdateRaceCarHulls @0x82721460 is that caller. Assert
        // baked at BrnTrafficPvs.h:171 (the :57 form fires at :141).
        u32 GetHullIndexForPoint(Vector3 lPoint, s32& lriOutX, s32& lriOutZ) const;

        // BrnTrafficPvs.h:70 -- grid coordinates to cell/hull index. NO standalone X360
        // symbol: every caller inlines it, so this is an inlining reversal recovered from
        // UpdateRaceCarHulls' rectangle walk at 0x827216A4..0x827216D4. Assert baked at
        // BrnTrafficPvs.h:188, between the :171 form above and GetHullPvs' :204 below,
        // which is the order the DWARF declares the three in (:64 / :70 / :75).
        u32 GetHullIndexForIndices(s32 liX, s32 liZ) const;

        // BrnTrafficPvs.h:75 -- the visible-hull set for a cell index.
        // X360 @ 0x82705FB0. asm: assert(luIndex < muNumCells);
        //   return mpaHullPvs[luIndex];   (20-byte stride)
        const Set<u16, 8>& GetHullPvs(u32 luIndex) const;

        // BrnTrafficPvs.h:85 / :90. X360 @0x827623E8 / @0x827624A0. Relocates exactly
        // one pointer slot (mpaHullPvs), plus the `this` 16-byte alignment guard (assert
        // baked at BrnTrafficPvs.cpp:45) and a debug walk asserting every cell's Set was
        // Constructed. Called from TrafficData::FixUp / FixDown.
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
