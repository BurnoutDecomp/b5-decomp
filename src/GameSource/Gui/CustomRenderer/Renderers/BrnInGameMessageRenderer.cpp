#include "BrnInGameMessageRenderer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Language/CgsLanguageManager.h" // CgsLanguage::LanguageManager

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGui::InGameMessageRenderer::SetLanguageManager @ 0x82443FB0
//
// Stores the shared language manager, then re-positions the in-game message banner for
// one specific (wide-glyph) language. The guest reads the active language as the
// manager's leading field and compares it to 16; on a match it overwrites two mutable
// data-segment floats (the banner's X / Y layout globals) with 28.0 and 629.0
// respectively. Those globals are the renderer's shared on-screen message position;
// modelled here as named file-scope layout state with the constants the asm stores.

namespace BrnGui
{
namespace
{
    // The wide-glyph language id the X360 special-cases (guest: `*pLanguageMgr == 16`).
    const s32 KI_WideGlyphLanguage = 16;

    // The banner-position constants the guest writes for the wide-glyph language. The X360
    // loads them from rodata (flt_82038B1C / flt_8205540C) and stores them into the mutable
    // message-position globals (flt_82F25818 / flt_82F2581C).
    const f32 KF_WideGlyphMessageX = 28.0f;  // -> flt_82F25818
    const f32 KF_WideGlyphMessageY = 629.0f; // -> flt_82F2581C

    // The mutable message-position globals the renderer draws the banner at. Shared (one
    // copy per build, not per renderer) so they are file-scope here, matching the guest's
    // data-segment globals flt_82F25818 / flt_82F2581C.
    f32 gfMessagePositionX = 0.0f; // flt_82F25818
    f32 gfMessagePositionY = 0.0f; // flt_82F2581C
}

void* InGameMessageRenderer::SetLanguageManager(CgsLanguage::LanguageManager* lpLanguageManager)
{
    CGS_ASSERT(lpLanguageManager != 0, "lpLanguageManager");

    // Guest: stw r31, 0x1388(r30) -- store the manager pointer.
    mpLanguageManager = lpLanguageManager;

    // Guest: lwz r11, 0(r31); cmpwi r11, 0x10 -- if the active language is the wide-glyph
    // language, move the banner's on-screen position.
    if (lpLanguageManager->GetCurrentLanguage() == KI_WideGlyphLanguage)
    {
        gfMessagePositionX = KF_WideGlyphMessageX;
        gfMessagePositionY = KF_WideGlyphMessageY;
    }

    // The X360 leaves `this` (r30/r3) in the result on the non-assert path.
    return this;
}
}
