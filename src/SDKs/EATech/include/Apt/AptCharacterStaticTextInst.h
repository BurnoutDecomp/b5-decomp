#pragma once

// ===========================================================================
// EATech Apt -- AptCharacterStaticTextInst: the per-instance render node for a
// STATIC-TEXT character (an .apt text field with no dynamic/editable content).
// It is one of the type-tagged leaves of the AptCharacterInst family produced by
// AptCharacterInst::CreateCharacterInst for a given AptCharacter tag.
//
// HEADER-ONLY LEAF. The X360 build emits only the compiler-generated scalar
// deleting destructor for this class (@0x82AF8678): it runs the base
// ~AptCharacterInst, then -- when the low bit of the deletion flag is set --
// returns the object to its DOGMA pool with size 16. That deallocation size
// equals sizeof(AptCharacterInst) (the console 16-byte / 4-dword base layout),
// so this leaf adds NO data members of its own; it is purely a type tag plus the
// inherited render-item plumbing. The deleting-destructor thunk is a compiler
// artefact and is dropped -- only the empty virtual destructor is written.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptCharacterInst.h"   // base: AptCharacterInst (16 bytes)

struct AptCharacterStaticTextInst : public AptCharacterInst
{
    // No additional data members: the deleting-destructor's DOGMA deallocate size
    // (16) matches sizeof(AptCharacterInst), so the leaf adds nothing to the base.
    // NON-virtual to match the committed AptCharacterInst base (manual-vtable
    // convention -- a C++ `virtual` would inject a second vptr and break parity).
    ~AptCharacterStaticTextInst() {}
};
