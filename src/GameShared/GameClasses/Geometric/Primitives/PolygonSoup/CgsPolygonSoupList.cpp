#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupList.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x... (CgsGeometric::PolygonSoupList::FixUp)
// First-pass reconstruction: behaviour-faithful to the X360 pseudocode. FixUp is a
// load-time relocation: it rebases the PolySoup pointer table and each PolySoup by
// `delta`, then rebases two pointer fields inside every PolySoup object. Member
// names/types per burnout.wiki (Polygon Soup List -> CgsGeometric::PolygonSoupList);
// offsets are from the 32-bit source and are not byte-matched on a 64-bit host, so
// the relocation arithmetic keeps the pointer fields as uintptr_t.
//
// ⭐ FORK RETIRED 2026-08-10 (spatial-partition wave): the struct definitions that used
// to sit here -- duplicated token-for-token in CgsPolygonSoupListResourceType.cpp, and
// about to be copied a third time by the spatial-partition build TU -- now live in the
// single home CgsPolygonSoupList.h, included above. No layout change: the header is a
// verbatim lift of what was here.

namespace CgsGeometric
{
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
