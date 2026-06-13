#include "types.hpp"
#include <cstdint>

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x... (CgsGeometric::PolygonSoupList::FixUp)
// First-pass reconstruction: behaviour-faithful to the X360 pseudocode. FixUp is a
// load-time relocation: it rebases the PolySoup pointer table and each PolySoup by
// `delta`, then rebases two pointer fields inside every PolySoup object. Member
// names/types per burnout.wiki (Polygon Soup List -> CgsGeometric::PolygonSoupList);
// offsets are from the 32-bit source and are not byte-matched on a 64-bit host, so
// the relocation arithmetic keeps the pointer fields as uintptr_t.

namespace CgsGeometric
{
    struct PolygonSoupEntry // a PolygonSoup
    {
        u32       _0[4];     // 0x00 leading data
        uintptr_t mpField16; // 0x10 relocated pointer
        uintptr_t mpField20; // 0x14 relocated pointer
    };

    struct PolygonSoupList
    {
        float     mOverallAabb[8];  // 0x00 AxisAlignedBox
        uintptr_t mpapPolySoups;    // 0x20 PolygonSoup** (base of the pointer table)
        uintptr_t mpaPolySoupBoxes; // 0x24 AxisAlignedBox4*
        s32       miNumPolySoups;   // 0x28
        s32       miDataSize;       // 0x2C

        PolygonSoupList* FixUp(int delta);
    };

    PolygonSoupList* PolygonSoupList::FixUp(int delta)
    {
        mpaPolySoupBoxes += delta;
        mpapPolySoups    += delta;

        uintptr_t* lpaTable = reinterpret_cast<uintptr_t*>(mpapPolySoups);
        for (s32 i = 0; i < miNumPolySoups; ++i)
        {
            lpaTable[i] += delta;  // rebase the PolySoup pointer
            PolygonSoupEntry* lpEntry = reinterpret_cast<PolygonSoupEntry*>(lpaTable[i]);
            lpEntry->mpField16 += delta;
            lpEntry->mpField20 += delta;
        }
        return this;
    }
}
