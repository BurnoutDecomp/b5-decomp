#ifndef CGS_UNIQUE_PLAYER_ID_X360_H
#define CGS_UNIQUE_PLAYER_ID_X360_H

#include "types.hpp"

#include "GameSource/GameState/BrnCgsPlayerName.h"   // CgsNetwork::PlayerName (16B macName home)

// ===========================================================================
// CgsNetwork::UniquePlayerIDX360
//   Home: GameShared/GameClasses/Network/Players/X360/CgsUniquePlayerIDX360.{h,cpp}
//
// The X360 unique-player identifier: a CgsNetwork::PlayerName (the 16-byte macName home)
// extended by the player's 64-bit XUID. Constructed from a name string + an XUID by
// ScoreboardManager when it resolves a player.
//
// LAYOUT (X360 asm @ 0x82541A10 Construct):
//     +0x00  (PlayerName base)  char macName[16]   -- copied via PlayerName::Construct
//     +0x10  mqXuid             (u64)              -- `std r28, 0x10(this)` (64-bit store)
//
// Construct validates the name (non-null + strlen < KI_USERNAME_LENGTH == 16, both vacuous
// CGS_ASSERT guards), delegates the name copy to PlayerName::Construct(this, lpcPlayerName),
// then stores the XUID at +0x10. The XUID arg is a single 64-bit register (r5 -> std r28);
// the Hex-Rays `LODWORD(v3) = a3` is the usual 64-bit-register-as-int misread of that store.
// ===========================================================================

namespace CgsNetwork
{
    struct UniquePlayerIDX360 : public PlayerName
    {
        // CgsUniquePlayerIDX360.h -- KI_USERNAME_LENGTH bound used by Construct's strlen guard
        // (shared with the PlayerName base; mirrored here so the X360 spelling resolves).
        static const s32 KI_USERNAME_LENGTH = CgsNetwork::KI_USERNAME_LENGTH;

        // @ 0x82541A10 -- validate lpcPlayerName, copy it into the PlayerName base, then store
        // the 64-bit XUID at +0x10. Returns `this` (the X360 returns PlayerName::Construct's r3).
        UniquePlayerIDX360* Construct(const char* lpcPlayerName, u64 lqXuid);

        u64 mqXuid;   // +0x10
    };
}

#endif // CGS_UNIQUE_PLAYER_ID_X360_H
