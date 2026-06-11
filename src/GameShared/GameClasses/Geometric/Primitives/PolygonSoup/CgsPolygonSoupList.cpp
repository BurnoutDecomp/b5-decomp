#include "types.hpp"
#include <cstdint>

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x... (CgsGeometric::PolygonSoupList::FixUp)
// First-pass reconstruction: behaviour-faithful to the X360 pseudocode. FixUp is a
// load-time relocation: it rebases the entry table and each entry by `delta`, then
// rebases two pointer fields inside every entry object. Field offsets named below
// are from the 32-bit source and are not byte-matched on a 64-bit host.

namespace CgsGeometric
{
    struct PolygonSoupEntry
    {
        u32       _0[4];   // bytes 0..15
        uintptr_t f16;     // [16]: relocated pointer
        uintptr_t f20;     // [20]: relocated pointer
    };

    struct PolygonSoupList
    {
        u32       _0[8];   // dwords 0..7
        uintptr_t entries; // [8]: base of the entry-pointer table
        uintptr_t field9;  // [9]: a second relocated pointer
        u32       count;   // [10]: number of entries

        PolygonSoupList* FixUp(int delta);
    };

    PolygonSoupList* PolygonSoupList::FixUp(int delta)
    {
        field9  += delta;
        entries += delta;

        uintptr_t* table = reinterpret_cast<uintptr_t*>(entries);
        for (u32 i = 0; i < count; ++i)
        {
            table[i] += delta;  // rebase the entry pointer
            PolygonSoupEntry* e = reinterpret_cast<PolygonSoupEntry*>(table[i]);
            e->f16 += delta;
            e->f20 += delta;
        }
        return this;
    }
}
