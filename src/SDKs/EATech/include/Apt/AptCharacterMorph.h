#pragma once

// ===========================================================================
// EATech Apt -- AptCharacterMorph: the morph/tween character (the SWF
// DefineMorphShape analogue).
//
// A morph character crossfades between two shape characters as a 0..1 ratio
// sweeps: at ratio 0 the START shape is fully opaque and the END shape is
// transparent; at ratio 1 the reverse. The renderable AptRenderItemMorph reads
// these two sub-characters and draws each at the complementary alpha
// (AptRenderItemMorph::Render @0x82AFEEC0).
//
// MINIMAL LAYOUT RECOVERY: the X360 morph render path only ever reaches two
// members of this character beyond the AptCharacter base, both AptCharacter* and
// both handed straight to AptCharacter::render --
//     *(mpCharacter + 16)  ->  the start (ratio==0) shape character
//     *(mpCharacter + 20)  ->  the end   (ratio==1) shape character
// so they are reconstructed by name here (project rule: infer members from the
// offsets accessed). The full morph-character record (built by
// AptCharacterAnimation::Fixup) carries further fields that the render path does
// not touch; they are added if/when a TU needs them. The companion runtime
// instance is AptCharacterMorphInst (DWARF-attested), which references a
// character of this type.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptCharacter.h"   // AptCharacter base + render()

struct AptCharacterMorph : public AptCharacter
{
    AptCharacter* mpStartCharacter;   // [+16] the ratio==0 shape (fades out as ratio -> 1)
    AptCharacter* mpEndCharacter;     // [+20] the ratio==1 shape (fades in  as ratio -> 1)
};
