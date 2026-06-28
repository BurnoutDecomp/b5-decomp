#pragma once

// ===========================================================================
// EATech Apt -- AptCharacterShapeInst: the per-instance render node for a SHAPE
// character (an .apt shape: the geometry/fill primitive, AptCharacter type tag 1
// in the AptCharacterAnimation::Fixup switch). It is one of the type-tagged
// leaves of the AptCharacterInst family produced by
// AptCharacterInst::CreateCharacterInst for a given AptCharacter tag.
//
// HEADER-ONLY LEAF. The X360 build emits only the compiler-generated vector
// deleting destructor for this class (@0x82AF8620): it runs the base
// ~AptCharacterInst, then -- when the low bit of the deletion flag is set --
// returns the object to its DOGMA pool with size 16. That deallocation size
// equals sizeof(AptCharacterInst) (the console 16-byte / 4-dword base layout),
// so this leaf adds NO data members of its own; it is purely a type tag plus the
// inherited render-item plumbing. The deleting-destructor thunk is a compiler
// artefact and is dropped -- only the empty destructor is written. It is NON-virtual
// to match the committed AptCharacterInst base, which models its console vtable as a
// manual `void* mpVTable_unused` member (a C++ `virtual` here would inject a second
// compiler vptr and break the 16-byte == sizeof(base) layout).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptCharacterInst.h"   // base: AptCharacterInst (16 bytes)

struct AptCharacterShapeInst : public AptCharacterInst
{
    // No additional data members: the deleting-destructor's DOGMA deallocate size
    // (16) matches sizeof(AptCharacterInst), so the leaf adds nothing to the base.
    ~AptCharacterShapeInst() {}   // non-virtual: matches the base's manual-vtable convention
};
