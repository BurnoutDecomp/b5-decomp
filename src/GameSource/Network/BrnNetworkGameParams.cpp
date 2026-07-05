#include "GameSource/Network/BrnNetworkGameParams.h"

#include <cstring>   // std::memcpy

// =============================================================================
// BrnNetwork::GameParams -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnNetworkGameParams.h for the
// corrected base (CgsNetwork::ServerInterfaceGameParamsX360) + the opaque leaf
// layout (maBlock150 @ +0x150, seven 0xA0-stride maPlayerSlots @ +0x170).
//
// This TU SHIPS:
//   ~GameParams  (the `scalar deleting destructor' @ 0x82567338 forwards to it)
//   operator=    @ 0x82566A00
// =============================================================================

namespace BrnNetwork
{

// X360 @ 0x82567338 (`scalar deleting destructor'). As the object tears down,
// the Xenon codegen reinstalls the shared ServerInterfaceStructureInterface base
// vtable (off_8207C88C) into each of the seven embedded 0xA0-stride sub-objects
// (this+0x170 .. this+0x530) and then the primary vtable at this+0, before the
// delete-expression's conditional operator delete. That whole vtable walk + the
// conditional free are compiler-synthesised from this trivial out-of-line virtual
// destructor; only the empty body is hand-written, matching the committed
// convention (BrnNetwork::GameResults::~GameResults @ 0x827DFB60). Defining it
// out-of-line here also anchors this class's vtable to this TU.
GameParams::~GameParams()
{
}

// X360 @ 0x82566A00. Member-wise copy: chain to the X360 base operator=
// (0x82558DB0), then copy the game leaf's own storage in the exact order and
// byte ranges the Xenon codegen emits -- the 0x20-byte block at +0x150 followed
// by the seven 0xA0-stride sub-object records at +0x170, each copied over its
// full live span [+4 .. +0xA0) (only the +0 vtable slot is left untouched, as an
// operator= must not copy vtables). The leaf field layout is not attested, so
// (as in BrnNetwork::GameResults) the records are reached by raw byte offset
// from `this`/`other` rather than by fabricated member names.
GameParams& GameParams::operator=(const GameParams& lrOther)
{
    CgsNetwork::ServerInterfaceGameParamsX360::operator=(lrOther);

    u8* lpDst = reinterpret_cast<u8*>(this);
    const u8* lpSrc = reinterpret_cast<const u8*>(&lrOther);

    // +0x150: 8-word (32-byte) block, copied word-wise (mtctr 8).
    std::memcpy(lpDst + 0x150, lpSrc + 0x150, 8 * sizeof(u32));

    // +0x170: seven 0xA0-byte sub-object records. Each copies its full live span
    // [+4 .. +0xA0): a 0x40-byte byte run, seven words, a 0x24-byte run, then
    // seven more words -- contiguous, leaving only the leading +0 vtable slot.
    for (s32 liSlot = 0; liSlot < KI_GAMEPARAMS_PLAYER_SLOTS; ++liSlot)
    {
        const s32 liBase = 0x170 + liSlot * KI_GAMEPARAMS_PLAYER_SLOT_STRIDE;
        std::memcpy(lpDst + liBase + 0x04, lpSrc + liBase + 0x04, 0x40);            // [+0x04,+0x44)
        std::memcpy(lpDst + liBase + 0x44, lpSrc + liBase + 0x44, 7 * sizeof(u32)); // [+0x44,+0x60)
        std::memcpy(lpDst + liBase + 0x60, lpSrc + liBase + 0x60, 0x24);            // [+0x60,+0x84)
        std::memcpy(lpDst + liBase + 0x84, lpSrc + liBase + 0x84, 7 * sizeof(u32)); // [+0x84,+0xA0)
    }

    return *this;
}

} // namespace BrnNetwork
