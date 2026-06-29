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
// The trailing 4-byte word at +4 is the record's STATE -- now attested by the
// AptSavedInputCheckpoints helpers (CanLinkPendingFiles @0x82AEE938 /
// CanContinueSavedInputs @0x82AEE980 / Checkpoint @0x82AFD3D0), which read & write
// *(element+4) with the small integers 1..4. Those bodies pin its meaning:
// Checkpoint promotes LOADED(2) -> LINKED(3) and appends LINKED(3)/PENDING(1); the
// gates accept {LOADED,LINKED} (linkable) or {LOADED,PLAYED} (replayable). So +4 is
// an E_APT_SAVED_INPUT_STATE enum, not the earlier unattested placeholder.
//
// Console layout is 8 bytes (2 x 32-bit); the named members let the x64 PC build
// compute the correct (wider) sizeof/offsets -- the pervasive PC-port FLAG also
// used by AptFile (named members, not literal console offsets).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

#include "SDKs/EATech/include/Apt/AptString/EAString.h"   // EAStringC mInputName member

// Saved-input record state (the +4 word). Values are the literals the X360
// AptSavedInputCheckpoints helpers store/compare (1..4).
enum E_APT_SAVED_INPUT_STATE : int32_t
{
    E_APT_SAVED_INPUT_PENDING = 1,   // queued; its file is not loaded yet
    E_APT_SAVED_INPUT_LOADED  = 2,   // its file is loaded (ready to link)
    E_APT_SAVED_INPUT_LINKED  = 3,   // linked into the movie
    E_APT_SAVED_INPUT_PLAYED  = 4,   // replayed (consumed by the saved-input tick)
};

struct AptFileSavedInputState
{
    // +0 (console): the saved input's name (ref-counted). MUST stay the first
    // member -- the X360 deleting destructor releases this element's string by
    // calling EAStringC::DecreaseInternalRefCount on *(this+0); ~EAStringC()
    // produces exactly that, so the embedded member's own destructor is the
    // faithful equivalent.
    EAStringC mInputName;

    // +4 (console): the record's state (E_APT_SAVED_INPUT_*). The checkpoint
    // helpers read/write it; the deleting destructor ignores it (trivial enum).
    E_APT_SAVED_INPUT_STATE meState;

    // Out-of-line destructor: its only work is releasing mInputName, which the
    // compiler does via the embedded EAStringC member's own destructor. Declaring
    // it out-of-line lets the host compiler synthesise the scalar/vector deleting
    // destructor thunk the X360 emitted (see AptFileSavedInputState.cpp).
    ~AptFileSavedInputState();
};

#endif // APT_FILE_SAVED_INPUT_STATE_H
