#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptRenderHandler.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsGui::AptRenderHandler member functions, reconstructed from BURNOUT_X360_ARTIST.XEX.
// This TU (class:CgsGui::AptRenderHandler) bodies the three X360-emitted functions:
//
//   AptRenderHandler()    @ 0x827DFBD8
//   SetWhiteTexture(tex)  @ 0x828466E0
//   GetIm2dRendererType() @ 0x82846780
//
// X360 store-for-store notes (constructor @0x827DFBD8):
//   - Two leading dword runs zeroed (r11 == 0): guest +0x88..+0x98 (five stw, with the
//     compiler's duplicate-first-write of +0x88) and +0xA0..+0xB0 (five stw, duplicate-first
//     write of +0xA0). Reproduced as the two 5-element u32 arrays, fully zeroed.
//   - Two large slot arrays seeded with 0x7FFFFFFF: a do/while loop (r10 = 24 down through -1,
//     25 stores) writes 0x7FFFFFFF (`ori r7,r8,0xFFFF` over `lis r8,0x7FFF`) into the FIRST dword
//     of each 12-byte (3-dword) slot, advancing the slot pointer by 0xC each iteration. The first
//     array bases at guest +99780, the second at guest +104184. Only the leading marker word of
//     each slot is written; the other two dwords are untouched.
//   - Two flag bytes cleared (stb r11): guest +100072 (after array A) and +104476 (after array B).
//   - Two index sentinels set to -1 (stwx r8,-1): guest +104172 and +108576.
//   The X360 emits the stores interleaved with the loops; the order here (zero runs, array A +
//   its flag/sentinel, array B + its flag/sentinel) preserves the same observable end state.
//
// SetWhiteTexture (@0x828466E0): asserts the incoming texture pointer is non-null
// ("Invalid texture pointer sent to AptRenderHandler::SetWhiteTexture"), then stores it into the
// white-texture slot (`v3[32] = a2` == guest +0x80).
//
// GetIm2dRendererType (@0x82846780): asserts the renderer pointer is non-null
// ("mpImRenderers", guest +108588), then returns *mpImRenderers (the renderer's leading dword).
// The X360 `lwz r11,0(r31); lwz r3,0(r11)` is a member load followed by a SINGLE deref of that
// pointer -- in C++ the member access is the first load and `*mpImRenderers` is the second.
//
// The X360-baked CgsAptRender.h file/line cites (h:314, h:424) are discarded per project
// convention; the assert strings are X360 rodata, reproduced verbatim.

namespace CgsGui
{
    AptRenderHandler::AptRenderHandler()
    {
        // Two leading zeroed dword runs (guest +0x88..+0x98, +0xA0..+0xB0).
        for (u32 i = 0; i < 5; ++i)
        {
            maZeroRunA[i] = 0;
            maZeroRunB[i] = 0;
        }

        // Slot array A: seed each slot's leading marker word with the 0x7FFFFFFF "unset" value
        // (guest +99780, 25 slots, 12-byte stride). Trailing flag byte cleared, index sentinel -1.
        for (u32 i = 0; i < KU_NUM_SLOTS; ++i)
        {
            maSlotsA[i].miMarker = KI_UNSET;
        }
        mbFlagA      = 0;
        miEntryIndexA = KI_NO_ENTRY;

        // Slot array B (guest +104184, 25 slots). Trailing flag byte cleared, index sentinel -1.
        for (u32 i = 0; i < KU_NUM_SLOTS; ++i)
        {
            maSlotsB[i].miMarker = KI_UNSET;
        }
        mbFlagB      = 0;
        miEntryIndexB = KI_NO_ENTRY;
    }

    // X360 0x828466E0.
    void AptRenderHandler::SetWhiteTexture(void* lpWhiteTexture)
    {
        CGS_ASSERT(lpWhiteTexture != 0,
                   "Invalid texture pointer sent to AptRenderHandler::SetWhiteTexture");
        mpWhiteTexture = lpWhiteTexture;
    }

    // X360 0x82846780.
    int AptRenderHandler::GetIm2dRendererType() const
    {
        CGS_ASSERT(mpImRenderers != 0, "mpImRenderers");
        return *mpImRenderers;
    }
}
