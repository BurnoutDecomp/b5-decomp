#pragma once

// ===========================================================================
// EATech Apt -- AptRenderItemStaticText: the renderable for a static-text
// character (the SWF DefineText analogue: baked glyph runs, no editable buffer).
//
// AptRenderItemStaticText : AptRenderItem -- no extra data (X360 52 bytes, same as
// the base); all the text lives in its AptCharacterStaticText. SHAPE + BODIES from
// the X360 ARTIST.XEX:
//   Clone @0x82AEC950, Render @0x82AFEC18, vtable off_821458D0, render-type flag
//   0x280000. The ctor is inlined into the factory (base ctor + flag stamp),
//   de-optimised back to its own constructor here.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderItem.h"

struct AptCharacter;
class AptRenderingContext;
enum AptMaskRenderOperation : int;

struct AptRenderItemStaticText : public AptRenderItem
{
    // Factory ctor (Manager_CreateItem case 10 inlines: base ctor + the static-text
    // render-type flag).
    AptRenderItemStaticText(AptCharacter* pCharacter, int nCreatedOnTick);
    // Clone copy-ctor: base clone copy + the static-text render-type flag.
    AptRenderItemStaticText(const AptRenderItemStaticText* pSource, int nCreatedOnTick, bool bCopyExtended);

    virtual AptRenderItem* Clone(int nCreatedOnTick, bool bCopyExtended) override;   // @0x82AEC950
    virtual void Render(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick) const override;  // @0x82AFEC18
};
