// Compile-gate translation unit for BrnTrafficHull.h.
// Forces instantiation of the inline accessor BrnTraffic::Hull::GetSection so
// the header is exercised under MSVC cl /c, not merely parsed.

#include "SharedClasses/Traffic/BrnTrafficHull.h"

namespace
{
    const BrnTraffic::Section* EmbedCheck_GetSection(const BrnTraffic::Hull& lHull, u32 luIndex)
    {
        return lHull.GetSection(luIndex);
    }

    // Reference the symbol so the optimizer cannot strip the instantiation.
    const void* const gpEmbedCheck = reinterpret_cast<const void*>(&EmbedCheck_GetSection);
}
