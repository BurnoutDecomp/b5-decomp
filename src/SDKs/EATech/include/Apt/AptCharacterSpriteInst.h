#pragma once

// ===========================================================================
// EATech Apt -- AptCharacterSpriteInst: the concrete per-instance render node
// for a plain SPRITE / movie-clip character (AptCharacter type tag 5). It is one
// of the type-tagged leaves of the AptCharacterInst family produced by
// AptCharacterInst::CreateCharacterInst (@0x82AFFF70: `if (mnType == 5)` ->
// Allocate(36) -> the sprite-base ctor). Its sibling AptCharacterAnimationInst
// also derives from the shared AptCharacterSpriteInstBase; this leaf is the
// non-animation sprite.
//
// HEADER-ONLY LEAF. The X360 build emits only the compiler-generated SCALAR
// deleting destructor for this class (@0x82B004F0): it runs the base
// ~AptCharacterSpriteInstBase, then -- when the low bit of the deletion flag is
// set -- returns the object to its DOGMA pool (off_8324D808, the shared Apt
// non-GC pool the base ctor also allocates from) with size 36 / 0x24
// (asm `li r5,0x24`). That deallocation size equals sizeof(AptCharacterSpriteInstBase)
// (the committed sprite base: AptCharacterInst 16 bytes + mnGotoFrame/mnClipActionFlags/
// mnCurrentFrame/mDisplayList/mnLastActionFrame = 36), so this leaf adds NO data
// members of its own -- it is purely a type tag plus the inherited movie-clip /
// render-item plumbing. The deleting-destructor thunk is a compiler artefact and
// is dropped per project policy -- only the empty destructor is written, and the
// real teardown (the embedded display list + the AptCharacterInst base) runs via
// the implicitly chained ~AptCharacterSpriteInstBase().
//
// NON-virtual destructor: it must match the committed AptCharacterSpriteInstBase /
// AptCharacterInst convention, which models the console vtable as a manual
// `void* mpVTable_unused` member at offset 0 (non-polymorphic). A C++ `virtual`
// here would inject a second, compiler-synthesised vptr and break the
// 36-byte == sizeof(base) layout (a layout fork -> FAIL).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptCharacterSpriteInstBase.h"   // base: AptCharacterSpriteInstBase (36 bytes)

struct AptCharacterSpriteInst : public AptCharacterSpriteInstBase
{
    // No additional data members: the scalar deleting destructor's DOGMA
    // deallocate size (36 / 0x24) matches sizeof(AptCharacterSpriteInstBase), so
    // the leaf adds nothing beyond the shared sprite base.
    ~AptCharacterSpriteInst() {}   // non-virtual: matches the base's manual-vtable convention
};
