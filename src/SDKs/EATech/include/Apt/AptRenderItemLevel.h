#pragma once

// ===========================================================================
// EATech Apt -- AptRenderItemLevel: the renderable for the null-character "level"
// node (the stage/root render item that AptRenderItem::Manager_CreateItem builds
// when a character instance has no character, e.g. the movie root).
//
// AptRenderItemLevel : AptRenderItem -- no extra data (X360 52 bytes, same as the
// base); it draws nothing itself (it inherits the base's empty Render) and exists
// to anchor its children in the render tree, distinguished only by its render-type
// flag (0x3C0000) + vtable. SHAPE + BODY from the X360 ARTIST.XEX:
//   Clone @0x82AEC7B0, vtable off_8214587C. The ctor is inlined into the factory
//   (base ctor + flag stamp), de-optimised back to its own constructor here.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderItem.h"

struct AptCharacter;

struct AptRenderItemLevel : public AptRenderItem
{
    // Factory ctor (Manager_CreateItem null-character case inlines: base ctor +
    // the level render-type flag). pCharacter is null on that path.
    AptRenderItemLevel(AptCharacter* pCharacter, int nCreatedOnTick);
    // Clone copy-ctor: base clone copy + the level render-type flag.
    AptRenderItemLevel(const AptRenderItemLevel* pSource, int nCreatedOnTick, bool bCopyExtended);

    virtual AptRenderItem* Clone(int nCreatedOnTick, bool bCopyExtended) override;   // @0x82AEC7B0
};
