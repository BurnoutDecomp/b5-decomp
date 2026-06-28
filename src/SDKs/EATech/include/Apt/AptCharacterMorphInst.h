#pragma once

// ===========================================================================
// EATech Apt -- AptCharacterMorphInst: the "morph" variant of the per-instance
// character data (the AptCIH -> AptCharacterInst -> AptRenderItem render spine).
//
// One of the type-tagged AptCharacterInst subtypes the CreateCharacterInst
// factory builds per character kind (sprite / button / text / morph / ...). A
// morph shape character (a Flash morph/shape-tween) gets this instance type so
// the per-instance render data carries the extra morph state alongside the base
// instance's render item + property hash.
//
// NO Feb-2007 source and NO DecFIGS DWARF exist for this class (X360-only, like
// the sibling AptCIHNone / AptNone singletons). SHAPE is therefore reconstructed
// STRICTLY from the X360 ARTIST.XEX. The X360 ledger attests exactly ONE symbol
// for the class:
//     AptCharacterMorphInst::`scalar deleting destructor'  @ 0x82AF86D0
//                                              (compiler thunk -- DROPPED)
// i.e. it is a HEADER-ONLY leaf: no ctor body, no accessors, no other method is
// emitted, so nothing is hand-written beyond this declaration. (Construction is
// the inlined base AptCharacterInst ctor folded into CreateCharacterInst; the
// morph state is populated elsewhere, in TUs not yet recovered.)
//
// BASE: AptCharacterInst (its render item / type flags / property-hash members).
//
// LAYOUT (console): the deleting-destructor thunk frees the object with the
// non-GC pool at the EXACT size 20 -- `DOGMA_PoolManager::Deallocate(off_8324D808,
// this, 20)` (asm `li r5,0x14`; off_8324D808 == gpNonGCPoolManager, the same
// non-GC pool the base AptCharacterInst::CreateCharacterInst allocates from). The
// base AptCharacterInst is 16 bytes (4 console dwords: vtable + mpRenderItem +
// mTypeFlags + mpProperties), so AptCharacterMorphInst adds exactly ONE 4-byte
// dword of its own. The deleting-destructor thunk is the only attested function
// and it touches no members (just chained ~AptCharacterInst + the sized free), so
// the extra dword's name/role is NOT recoverable from the X360 ledger, Feb-2007,
// the DWARF, or the wiki. It is reserved here as a single explicit, NAMED
// placeholder so sizeof(AptCharacterMorphInst) == 20 matches the binary's sized
// operator delete (semantic parity); access stays by name -- no offset poke.
// (FLAG: rename this field to the real morph member when a TU that reads/writes it
// -- e.g. the morph CreateCharacterInst branch or render path -- is recovered.)
//
// The deleting destructor (@0x82AF86D0) is `delete this` codegen: run
// ~AptCharacterInst, then -- iff the delete flag (low bit of the second arg) is
// set -- gpNonGCPoolManager->Deallocate(this, 20). It is regenerated from the
// (non-virtual) ~AptCharacterMorphInst() + the inherited non-GC pool free, so per
// project policy (deleting-destructor thunks are dropped) it is not written here.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

#include "SDKs/EATech/include/Apt/AptCharacterInst.h"   // AptCharacterInst base

struct AptCharacterMorphInst : public AptCharacterInst
{
    // FLAG: the one extra console dword beyond the 16-byte AptCharacterInst base
    // (object sizeof is 20, per the deleting destructor's sized non-GC free). The
    // morph instance's extra member -- name/type/role unattested by the X360
    // ledger, Feb-2007, the DWARF, or the wiki (the only emitted function is the
    // dropped deleting-destructor thunk, which never reads it). Reserved by name
    // to keep sizeof == 20; rename when a TU that uses it is recovered.
    uint32_t mMorphState_unknown;   // [4] (console dword) -- placeholder

    // Trivial teardown: AptCharacterMorphInst adds only the reserved dword above
    // (no owned resources), so destruction is just the chained ~AptCharacterInst
    // (which the C++ destructor emits, along with the sized non-GC pool free via
    // the deleting-destructor thunk the compiler regenerates). NON-virtual to match
    // the committed AptCharacterInst base's manual-vtable convention.
    ~AptCharacterMorphInst() {}
};
