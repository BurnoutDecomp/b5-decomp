#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82846528
//   (CgsGui::GuiHudMessageResource::FixUp)
//
// Behaviour-faithful to the X360 pseudocode. FixUp is a load-time pointer
// relocation: it rebases the resource's base pointer by `delta`, then walks the
// `count` 32-bit pointer entries that begin at that rebased base and adds `delta`
// to each.
//
//   count       = this[2]
//   this[0]    += delta            // rebase the base pointer
//   for i in [0, count):
//       *(base + i) += delta       // relocate each entry pointer
//   return this
//
// (The pseudocode re-reads this[2] as the loop bound each iteration; count is not
// mutated, so a fixed bound is equivalent.)

namespace CgsGui
{
    struct GuiHudMessageResource
    {
        u32  muBase;     // [0] base pointer (rebased in place)
        u32  muPad1;     // [1] untouched
        s32  miCount;    // [2] number of pointer entries at *muBase

        GuiHudMessageResource* FixUp(int liDelta);
    };

    GuiHudMessageResource* GuiHudMessageResource::FixUp(int liDelta)
    {
        const s32 liCount = miCount;
        muBase += static_cast<u32>(liDelta);

        if (liCount > 0)
        {
            u32* lpaEntries = reinterpret_cast<u32*>(muBase);
            for (s32 liEntry = 0; liEntry < liCount; ++liEntry)
                lpaEntries[liEntry] += static_cast<u32>(liDelta);
        }
        return this;
    }
}
