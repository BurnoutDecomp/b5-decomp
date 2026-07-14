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
    // Clone copy-ctor (@0x82AEC040): base copy + the instance name + the sprite
    // render-type flag (shared by AptRenderItemAnimation/Button's Clone).
    AptRenderItemSprite(const AptRenderItemSprite* pSource, int nCreatedOnTick, bool bCopyExtended);

    virtual AptRenderItem* Clone(int nCreatedOnTick, bool bCopyExtended) override;                         // @0x82AEC890
    virtual void Render(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick) const;          // @0x7E49AC (empty)
    virtual void PushRenderData(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick) const;  // @0x7F249C
    virtual void PopRenderData(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick) const;   // @0x7ECAD0
    virtual void PushRenderDataAbsolute(AptRenderingContext* pCtx) const;                                  // @0x7F2958

    // dtor (X360 vector deleting destructor @0x82AEC8E0): mInstanceName destructs
    // (its refcount drop) + the base ~AptRenderItem runs; the sized pool delete frees.
    virtual ~AptRenderItemSprite();

    // ---- render-properties string (the instance-name render-data key) --------
    // The sprite's "render properties string" IS mInstanceName; Set assigns through
    // EAStringC::operator= (refcount-shared copy).
    EAStringC* GetRenderPropertiesString();                                        // @0x82AD5030
    EAStringC& SetRenderPropertiesString(const EAStringC& rString);                // @0x82AE5A80
};
