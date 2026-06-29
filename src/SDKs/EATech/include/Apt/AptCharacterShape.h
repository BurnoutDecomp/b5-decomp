#pragma once

// ===========================================================================
// EATech Apt -- AptCharacterShape: a leaf vector-shape (and static-text) character.
//
// MINIMAL LAYOUT RECOVERY: the only field the X360 bounds path reaches beyond the
// AptCharacter base is the character's own bounding rectangle, at +0x10 (the first
// dword after the 16-byte AptCharacter base) -- AptCIH::GetBoundingRect's
// shape(type 1)/static-text(type 10) arm reads `*(AptCharacter + 0x10)` as an
// AptRect and hands it to AptRenderingContext::expandBoundingRect (the
// `v14 + 4`-elements == +0x10 byte access). Both the shape and static-text leaf
// characters carry their authored bounds at this offset, so this minimal home
// covers both (downcast guarded by the AptCharacter::mnType == 1 || 10 test). The
// rest of the leaf-character record is added if/when a TU needs it.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptCharacter.h"        // AptCharacter base (mnType + 16-byte base)
#include "SDKs/EATech/include/Apt/AptStd/AptRect.h"       // AptRect mBounds

struct AptCharacterShape : public AptCharacter
{
    AptRect mBounds;   // [+0x10] authored bounding rectangle (left/top/right/bottom)
};
