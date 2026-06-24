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

        // BrnTrafficPvs.h:75 -- the visible-hull set for a cell index.
        // X360 @ 0x82705FB0. asm: assert(luIndex < muNumCells);
        //   return mpaHullPvs[luIndex];   (20-byte stride)
        const Set<u16, 8>& GetHullPvs(u32 luIndex) const;

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
