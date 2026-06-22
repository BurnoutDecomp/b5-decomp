// Compile-gate translation unit for WheelList.h.
// Forces the grown header surface to be parsed + ODR-instantiated under MSVC cl /c:
// the WheelListEntry layout (sizeof 72, macName @ +0x08), the WheelListResource
// entry-array accessor, and the WheelList name-lookup / wheel-data accessors that
// this group bodied. Not merely parsed -- the symbols below are referenced so the
// optimizer cannot strip them.

#include "SharedClasses/DataLists/WheelList.h"
#include <cstddef>   // offsetof

namespace
{
    // X360-proven layout facts (BrnResource::WheelList::GetWheelData @ 0x822CD3E8,
    // FindWheelIndexFromName @ 0x822CD4D8): WheelListEntry stride is 72 and the name
    // C-string sits at byte +0x08. Static-assert them so a future regrow cannot drift.
    static_assert(sizeof(BrnResource::WheelListEntry) == 72,
        "WheelListEntry footprint must stay 72 (GetWheelData stride)");
    static_assert(offsetof(BrnResource::WheelListEntry, macName) == 8,
        "WheelListEntry name must stay at +0x08 (stricmp target)");

    s32 EmbedCheck_FindWheel(const BrnResource::WheelList& lrList, const char* lpcName)
    {
        return lrList.FindWheelIndexFromName(lpcName);
    }

    const BrnResource::WheelListEntry* EmbedCheck_GetWheelData(
        const BrnResource::WheelList& lrList, s32 liIndex)
    {
        return lrList.GetWheelData(liIndex);
    }

    // Reference the symbols so the optimizer cannot strip the instantiations.
    const void* const gpEmbedCheckFind = reinterpret_cast<const void*>(&EmbedCheck_FindWheel);
    const void* const gpEmbedCheckData = reinterpret_cast<const void*>(&EmbedCheck_GetWheelData);
}
