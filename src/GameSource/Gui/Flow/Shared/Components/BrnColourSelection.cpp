// ===================================================================================
// BrnGui::ColourSelection  -- implementation
//   class:BrnGui::ColourSelection
//
// GetUint32ColourFromVector4 @ 0x824E8528
//   Packs a normalised RGBA colour vector into a 0x00RRGGBB integer. The X360 asserts the
//   pointer is non-NULL (BrnColourSelection.h line 131), then for each of R,G,B multiplies
//   the channel by 255.0 (flt_82010C20, fmuls single-precision) and truncates toward zero
//   (fctidz -> stfiwx keeps the low 32 bits), packing them as ((R<<8 | G)<<8 | B). The
//   alpha lane is not read. Reconstructed store-for-store from the X360 asm.
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/Components/BrnColourSelection.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnGui
{
    // @ 0x824E8528
    s32 ColourSelection::GetUint32ColourFromVector4(const f32* lpv4Colour)
    {
        CGS_ASSERT(lpv4Colour != 0, "lpv4Colour");                  // @0x824E8540 (cmplwi r31,0)

        // Per channel: scale by 255.0 (single-precision) then truncate toward zero.
        const s32 liRed   = static_cast<s32>(lpv4Colour[0] * 255.0f);   // fmuls/fctidz @0x824E857C
        const s32 liGreen = static_cast<s32>(lpv4Colour[1] * 255.0f);   // fmuls/fctidz @0x824E8580
        const s32 liBlue  = static_cast<s32>(lpv4Colour[2] * 255.0f);   // fmuls/fctidz @0x824E8584

        return ((liRed << 8) | liGreen) << 8 | liBlue;              // slwi/or chain @0x824E85A0..0x824E85C0
    }
}
