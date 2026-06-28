#ifndef APT_FILE_SAVED_INPUT_STATE_H
#define APT_FILE_SAVED_INPUT_STATE_H

// ===========================================================================
// EATech Apt -- AptFileSavedInputState: one saved-input record for an Apt movie
// file. Held in an EASTL-style vector<AptFileSavedInputState> whose growth is
// driven by StringAsVectorPolicy<AptFileSavedInputState>::ReAlloc -- the same
// policy that also drives the array teardown (the vector deleting destructor in
// AptFileSavedInputState.cpp).
//
// SHAPE recovered from the X360 ARTIST `vector deleting destructor' @0x82AEC728:
//   - The walk strides 8 bytes per element (`slwi r10, r11, 3` = count * 8, and
//     the loop steps the cursor back two DWORDs each iteration), so
//     sizeof(AptFileSavedInputState) == 8 (two 32-bit words on the console).
//   - The only per-element work is EAStringC::DecreaseInternalRefCount on the
//     FIRST word (`lwz r3, 0(r31); bl EAStringC__DecreaseInternalRefCount`),
//     i.e. releasing an embedded EAStringC member at offset 0 -- the exact body
//     the compiler synthesises for ~EAStringC(). That pins offset 0 as an owned
//     EAStringC; the destructor does nothing else.
//
// The trailing 4-byte word is NOT attested: nothing in the X360 ledger reads or
// writes it (the only AptFileSavedInputState function the build emits is the
// deleting destructor, which touches only the EAStringC), and there is no DWARF
// / Feb-2007 / wiki entry for this type. It is therefore kept as a documented,
// sized placeholder so the named EAStringC member lands at offset 0 and the
// element stride stays 8 -- it is intentionally NOT given an invented role.
//
// Console layout is 8 bytes (2 x 32-bit); the named EAStringC member lets the
// x64 PC build compute the correct (wider) sizeof/offsets -- the pervasive
// PC-port FLAG also used by AptFile (named members, not literal console offsets).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptString/EAString.h"   // EAStringC mInputName member

struct AptFileSavedInputState
{
    // +0 (console): the saved input's name (ref-counted). MUST stay the first
    // member -- the X360 deleting destructor releases this element's string by
    // calling EAStringC::DecreaseInternalRefCount on *(this+0); ~EAStringC()
    // produces exactly that, so the embedded member's own destructor is the
    // faithful equivalent.
    EAStringC mInputName;

    // +4 (console): a single unattested 32-bit word. No X360-emitted code reads
    // or writes it and there is no DWARF/Feb-2007 reference, so its role is
    // unknown -- modeled as a sized placeholder (NOT an invented field) purely to
    // hold the attested 8-byte element stride. (FLAG: name/type pending the TU
    // that first populates the vector.)
    uint8_t mauUnknownPad4[4];

    // Out-of-line destructor: its only work is releasing mInputName, which the
    // compiler does via the embedded EAStringC member's own destructor. Declaring
    // it out-of-line lets the host compiler synthesise the scalar/vector deleting
    // destructor thunk the X360 emitted (see AptFileSavedInputState.cpp).
    ~AptFileSavedInputState();
};

#endif // APT_FILE_SAVED_INPUT_STATE_H
