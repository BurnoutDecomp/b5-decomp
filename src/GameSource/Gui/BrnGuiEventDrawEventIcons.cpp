// BrnGuiEventDrawEventIcons.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX. BrnGui::GuiEventDrawEventIcons is the
// "draw event icons" GUI event payload (home: Gui/BrnGuiEventTypeDefs.h). Construct
// fills the event and copies in the caller's "icons to ignore" list; GetIgnoreIcons
// reads that list back out. Each runs non-fatal CGS_ASSERT guards (the X360 proceeds
// regardless). The X360-baked assert file/line are discarded per project convention;
// the stringized conditions match the X360 assert text.
//
// ⛔⛔ MOUNT CONTRACT -- READ BEFORE SCORING THIS SYMBOL AS LANDED.
// This TU is NOT in build_game_exe.bat / build/game/obj/build.rsp, which is the ONLY reason
// the full link measured `BrnGui::GuiEventDrawEventIcons::GetIgnoreIcons(u32*, int*) const`
// as an unresolved external: the body below has been correct and complete all along, and it
// was re-verified store-for-store against the X360 export @0x82443518 on 2026-08-29 (wave
// G3) -- the two null asserts with their BrnGuiEventTypeDefs.h:2815/:2816 lines, the count
// read at this+0x30, and the word-by-word copy out of the ignore list at this+0x00.
// Its consumer, CrashNavIconRenderer::RecvEvent @0x82456168 (case 554), IS mounted.
// ⇒ THE FIX IS A BAT MOUNT, NOT A NEW BODY. Add
//     %SRC%\GameSource\Gui\BrnGuiEventDrawEventIcons.cpp
// to build_game_exe.bat beside the other GameSource\Gui event TUs. Verified safe:
// GuiEventDrawEventIcons::Construct and ::GetIgnoreIcons are defined NOWHERE ELSE in the
// tree (no LinkStubs / LinkGates entry either), so the mount cannot raise LNK2005, and this
// TU introduces no undefined externals of its own -- it calls nothing but CGS_ASSERT.
// Writing the body a second time in a mounted TU would have been the wrong fix: it would
// have armed exactly that LNK2005 for whoever mounts this file next.

#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGui
{

// @0x824EB218
//   stfs f1,0x28(this)   ; mfFadeTime         = lfFadeTime
//   stb  r4,0x34(this)   ; mbDrawIcons        = lbDrawIcons
//   stw  r5,0x2C(this)   ; meIconDisplayType  = leIconDisplayType
//   stw  r8,0x30(this)   ; miNumIconsToIgnore = liNumIconsToIgnore
//   if (count != 0 && (lpuIconsToIgnore == 0 || count > 10)) <assert>
//   for (i=0; i < miNumIconsToIgnore; ++i)  this[i] = lpuIconsToIgnore[i]  (word copy @+0)
//
// PPC ARGUMENT NOTE: the parameters are r3=this, r4=bool, r5=EIconDisplayType, f1=float,
// r7=list, r8=count -- r6 is NEVER read, because a float argument takes f1 and SKIPS its
// GPR slot. There is no 4th integer parameter (an earlier revision of this file carried a
// phantom `s32 liUnused` for the dead r6). The DWARF signature (BrnGuiEventTypeDefs.h:2680)
// has exactly these five parameters and returns void; r3 is clobbered by the assert calls
// below and never restored, so the old `return this` was a decompiler artifact.
void GuiEventDrawEventIcons::Construct(bool lbDrawIcons,
                                       EIconDisplayType leIconDisplayType,
                                       f32 lfFadeTime,
                                       u32* lpuIconsToIgnore,
                                       s32 liNumIconsToIgnore)
{
    mfFadeTime         = lfFadeTime;
    mbDrawIcons        = lbDrawIcons;
    meIconDisplayType  = leIconDisplayType;
    miNumIconsToIgnore = liNumIconsToIgnore;

    CGS_ASSERT( (miNumIconsToIgnore == 0) ||
                ((lpuIconsToIgnore != 0) && (miNumIconsToIgnore <= KI_MAX_ICONS_TO_IGNORE)),
                "(miNumIconsToIgnore == 0) || ((lpuIconsToIgnore != NULL) && (miNumIconsToIgnore <= KI_MAX_ICONS_TO_IGNORE) )" );

    for ( s32 liIndex = 0; liIndex < miNumIconsToIgnore; ++liIndex )
        mauIconsToIgnore[liIndex] = lpuIconsToIgnore[liIndex];
}

// @0x82443518
//   if (lpuIconsToIgnore   == 0) <assert "lpuIconsToIgnore">
//   if (lpiNumIconsToIgnore == 0) <assert "lpiNumIconsToIgnore">
//   *lpiNumIconsToIgnore = miNumIconsToIgnore         (stw *(this+0x30) -> *a3)
//   for (i=0; i < miNumIconsToIgnore; ++i) lpuIconsToIgnore[i] = mauIconsToIgnore[i]  (word copy)
//
// void per the DWARF (BrnGuiEventTypeDefs.h:2688): `this` lives in r31, and r3 is
// overwritten by the two CgsDev::Assert::FireAssert argument setups and never restored,
// so the binary has no meaningful return value.
void GuiEventDrawEventIcons::GetIgnoreIcons(u32* lpuIconsToIgnore,
                                            s32* lpiNumIconsToIgnore) const
{
    CGS_ASSERT( lpuIconsToIgnore != 0, "lpuIconsToIgnore" );
    CGS_ASSERT( lpiNumIconsToIgnore != 0, "lpiNumIconsToIgnore" );

    *lpiNumIconsToIgnore = miNumIconsToIgnore;

    for ( s32 liIndex = 0; liIndex < miNumIconsToIgnore; ++liIndex )
        lpuIconsToIgnore[liIndex] = mauIconsToIgnore[liIndex];
}

} // namespace BrnGui
