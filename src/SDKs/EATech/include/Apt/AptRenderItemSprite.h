#pragma once

// ===========================================================================
// EATech Apt -- AptRenderItemSprite: the renderable for a movie-clip (sprite).
//
// A sprite does not draw itself -- its child display-list items render through
// the render tree. So Render is empty; what the sprite contributes is its
// transform bracket (PushRenderData pushes the sprite's matrices before its
// children render, PopRenderData pops after) plus an optional render-data hook
// keyed on the instance name. It is the base of AptRenderItemButton and
// AptRenderItemAnimation.
//
// SHAPE + BODIES from the PS3 EXTERNAL ELF (19AptRenderItemSprite): ctor 0x810B6C,
// Render 0x7E49AC (empty), PushRenderData 0x7F249C, PopRenderData 0x7ECAD0,
// PushRenderDataAbsolute 0x7F2958. LAYOUT: AptRenderItem (52 bytes) +
// mInstanceName (the render-data tracking name) = 56 bytes.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderItem.h"
#include "SDKs/EATech/include/Apt/AptString/EAString.h"

struct AptRenderItemSprite : public AptRenderItem
{
    EAStringC mInstanceName;   // [13] -- render-data tracking name (default: empty)

    AptRenderItemSprite(AptCharacter* pCharacter, int nCreatedOnTick);   // @0x810B6C

    virtual void Render(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick) const;          // @0x7E49AC (empty)
    virtual void PushRenderData(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick) const;  // @0x7F249C
    virtual void PopRenderData(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick) const;   // @0x7ECAD0
    virtual void PushRenderDataAbsolute(AptRenderingContext* pCtx) const;                                  // @0x7F2958
};
