// BrnGuiEventDrawEventIcons.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX. BrnGui::GuiEventDrawEventIcons is the
// "draw event icons" GUI event payload (home: Gui/BrnGuiEventTypeDefs.h). Construct
// fills the event and copies in the caller's "icons to ignore" list; GetIgnoreIcons
// reads that list back out. Each runs non-fatal CGS_ASSERT guards (the X360 proceeds
// regardless). The X360-baked assert file/line are discarded per project convention;
// the stringized conditions match the X360 assert text.

#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGui
{

// @0x824EB218
//   stfs f1,0x28(this)   ; mfDisplayTime = lfDisplayTime
//   stb  r4,0x34(this)   ; mbFlag        = lbFlag
//   stw  r5,0x2C(this)   ; muIconSetId   = luIconSetId
//   stw  r8,0x30(this)   ; miNumIconsToIgnore = liNumIconsToIgnore
//   if (count != 0 && (lpuIconsToIgnore == 0 || count > 10)) <assert>
//   for (i=0; i < miNumIconsToIgnore; ++i)  this[i] = lpuIconsToIgnore[i]  (word copy @+0)
GuiEventDrawEventIcons* GuiEventDrawEventIcons::Construct(u8 lbFlag,
                                                         u32 luIconSetId,
                                                         f32 lfDisplayTime,
                                                         s32 /*liUnused*/,
                                                         const u32* lpuIconsToIgnore,
                                                         s32 liNumIconsToIgnore)
{
    mfDisplayTime      = lfDisplayTime;
    mbFlag             = lbFlag;
    muIconSetId        = luIconSetId;
    miNumIconsToIgnore = liNumIconsToIgnore;

    CGS_ASSERT( (miNumIconsToIgnore == 0) ||
                ((lpuIconsToIgnore != 0) && (miNumIconsToIgnore <= KI_MAX_ICONS_TO_IGNORE)),
                "(miNumIconsToIgnore == 0) || ((lpuIconsToIgnore != NULL) && (miNumIconsToIgnore <= KI_MAX_ICONS_TO_IGNORE) )" );

    for ( s32 liIndex = 0; liIndex < miNumIconsToIgnore; ++liIndex )
        maIgnoreIcons[liIndex] = lpuIconsToIgnore[liIndex];

    return this;
}

// @0x82443518
//   if (lpuIconsToIgnore   == 0) <assert "lpuIconsToIgnore">
//   if (lpiNumIconsToIgnore == 0) <assert "lpiNumIconsToIgnore">
//   *lpiNumIconsToIgnore = miNumIconsToIgnore         (stw *(this+0x30) -> *a3)
//   for (i=0; i < miNumIconsToIgnore; ++i) lpuIconsToIgnore[i] = maIgnoreIcons[i]  (word copy)
GuiEventDrawEventIcons* GuiEventDrawEventIcons::GetIgnoreIcons(u32* lpuIconsToIgnore,
                                                              s32* lpiNumIconsToIgnore) const
{
    CGS_ASSERT( lpuIconsToIgnore != 0, "lpuIconsToIgnore" );
    CGS_ASSERT( lpiNumIconsToIgnore != 0, "lpiNumIconsToIgnore" );

    *lpiNumIconsToIgnore = miNumIconsToIgnore;

    for ( s32 liIndex = 0; liIndex < miNumIconsToIgnore; ++liIndex )
        lpuIconsToIgnore[liIndex] = maIgnoreIcons[liIndex];

    // The X360 returns `this` (r3 preserved through r31); the const method returns it as
    // a non-const pointer to mirror the binary's return register.
    return const_cast<GuiEventDrawEventIcons*>( this );
}

} // namespace BrnGui
