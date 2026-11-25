#pragma once

// Serialized list of polygon soups and their parallel four-box bounds.
// FixUp rebases the pointer table, the bounds array and every soup.

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox4.h"

namespace CgsGeometric
{
    struct PolygonSoup;   // CgsPolygonSoup.h -- referenced through the pointer table

    struct PolygonSoupList
    {
        float            mOverallAabb[8]; // +0x00  min/max vector lanes
        PolygonSoup**   mpapPolySoups;    // +0x20
        AxisAlignedBox4* mpaPolySoupBoxes; // +0x24
        s32              miNumPolySoups;   // +0x28
        s32              miDataSize;       // +0x2C  total serialised byte size

        // Inlined in DrawPolySoups, ARTIST 828C0B28..828C0BCC; declaration
        // names from DecFIGS CgsPolygonSoupList.h:101..119.
        s32 GetNumPolygonSoups() const { return miNumPolySoups; }
        const PolygonSoup* GetPolygonSoup(s32 liPolySoupIndex) const
        {
            CGS_ASSERT(liPolySoupIndex >= 0 && liPolySoupIndex < GetNumPolygonSoups(),
                       "liPolySoupIndex >= 0 && liPolySoupIndex < GetNumPolygonSoups()");
            return mpapPolySoups[liPolySoupIndex];
        }
        // The binary's bound is the soup count rounded up to four, without
        // dividing by four (828C0B90..98). Keep that original accessor behavior.
        s32 GetNum4Boxes() const { return ((miNumPolySoups + 3) / 4) * 4; }
        const AxisAlignedBox4* Get4Boxes(s32 li4BoxIndex) const
        {
            CGS_ASSERT(li4BoxIndex >= 0 && li4BoxIndex < GetNum4Boxes(),
                       "li4BoxIndex >= 0 && li4BoxIndex < GetNum4Boxes()");
            return mpaPolySoupBoxes + li4BoxIndex;
        }

        // FixUp -- load-time relocation; rebases the table, every soup pointer, and
        // the two pointer fields inside every soup. Bodied in CgsPolygonSoupList.cpp.
        void FixUp(const void* lpLoadBase);
    };
}
