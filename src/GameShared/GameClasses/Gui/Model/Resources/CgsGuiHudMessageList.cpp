#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82846580
//   (CgsGui::GuiHudMessageListResource::FixUp)
//
// Load-time pointer relocation (same idiom as GuiHudMessageResource::FixUp, but
// the base pointer lives at offset 8 and the entry count at offset 4):
//
//   count       = this[1]            (*(this+4))
//   this[2]    += delta              (*(this+8) rebased)
//   for i in [0, count):
//       *(base + i) += delta         (relocate each 32-bit entry pointer)
//   return this

namespace CgsGui
{
    struct GuiHudMessageListResource
    {
        u32  muPad0;     // [0] untouched
        s32  miCount;    // [1] number of pointer entries
        u32  muBase;     // [2] base pointer (rebased in place)

        GuiHudMessageListResource* FixUp(int liDelta);
    };

    GuiHudMessageListResource* GuiHudMessageListResource::FixUp(int liDelta)
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
