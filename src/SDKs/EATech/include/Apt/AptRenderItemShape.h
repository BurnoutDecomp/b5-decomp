#pragma once

// ===========================================================================
// EATech Apt -- AptRenderItemShape: the renderable for a vector-shape character.
//
// The simplest AptRenderItem subtype: no extra data (52 bytes, same as the base);
// it just renders its AptCharacter's shape geometry. SHAPE + BODIES from the PS3
// EXTERNAL ELF (18AptRenderItemShape): ctor 0x80FDF4, Render 0x8113C0.
//
// Render pushes the item's matrices onto the render context, draws the shape
// geometry (AptCharacter::render), then pops -- the canonical leaf render.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderItem.h"

struct AptRenderItemShape : public AptRenderItem
{
    AptRenderItemShape(AptCharacter* pCharacter, int nCreatedOnTick);   // @0x80FDF4
    // Clone copy-ctor: base copy + the shape render-type flag.
    AptRenderItemShape(const AptRenderItemShape* pSource, int nCreatedOnTick, bool bCopyExtended);

    virtual AptRenderItem* Clone(int nCreatedOnTick, bool bCopyExtended) override;                 // @0x82AECB10
    virtual void Render(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick) const;  // @0x8113C0
};
