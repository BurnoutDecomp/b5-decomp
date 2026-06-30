#pragma once

// ===========================================================================
// EATech Apt -- AptRenderItemMorph: the renderable for a morph/tween character.
//
// AptRenderItemMorph : AptRenderItem -- it adds a single morph "ratio" (0..1) and
// draws its AptCharacterMorph's two shapes crossfaded by that ratio: the start
// shape at alpha (1-ratio)*255, the end shape at alpha ratio*255 (each alpha
// clamped to [-255,255]), bracketed by a colour-transform push/pop so the fade
// does not leak into siblings (AptRenderItemMorph::Render @0x82AFEEC0).
//
// LAYOUT: AptRenderItem (X360 52 bytes) + mfRatio (the morph blend ratio at
// console +0x34) = X360 56 bytes. Reconstructed with NAMED members; the X360 byte
// offsets are documentation only (the base widens on the x64 PC gate -- project
// semantic-parity-by-named-members rule).
//
// SHAPE + BODIES from the X360 ARTIST.XEX, cross-checked against the PS3 EXTERNAL
// ELF DWARF (18AptRenderItemMorph):
//   GetRatio @0x82AD5058 / Render @0x82AFEEC0 / Clone @0x82AECA30 /
//   `scalar deleting destructor' @0x82AECAA8 (compiler thunk; the base sized
//   operator delete frees the pool block). vtable off_821458EC. The morph
//   render-type flag is 0x00200000 (mFlags bits 18..23, mask 0x00FC0000) -- cf.
//   shape 0x040000 / sprite 0x140000 / static-text 0x280000 / level 0x3C0000.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "types.hpp"   // f32

#include "SDKs/EATech/include/Apt/AptRenderItem.h"

struct AptCharacter;
class AptRenderingContext;
enum AptMaskRenderOperation : int;

struct AptRenderItemMorph : public AptRenderItem
{
    f32 mfRatio;   // [+0x34] morph blend ratio (0 = start shape, 1 = end shape)

    // Normal ctor (factory case 8 inlines: base render item + the morph
    // render-type flag). The console leaves mfRatio for the per-tick morph update;
    // zeroed here to avoid reading an uninitialised member.
    AptRenderItemMorph(AptCharacter* pCharacter, int nCreatedOnTick);

    // Clone copy-ctor (@0x82AECA30 inlines this): base clone copy + the morph
    // render-type flag. (mfRatio is likewise set by the per-tick update.)
    AptRenderItemMorph(const AptRenderItemMorph* pSource, int nCreatedOnTick, bool bCopyExtended);

    virtual AptRenderItem* Clone(int nCreatedOnTick, bool bCopyExtended) override;   // @0x82AECA30
    virtual void Render(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick) const override;  // @0x82AFEEC0
    virtual ~AptRenderItemMorph();   // @0x82AECAA8 (trivial; base frees the pool block)

    // GetRatio @0x82AD5058 -- the current morph blend ratio.
    f32 GetRatio() const;
};
