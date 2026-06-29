#pragma once

// ===================================================================================
// BrnNetwork::StateManager::CurrentPlayerXUIDs -- owning header
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkStateManager.h
//
// MINIMAL SLICE: only the nested CurrentPlayerXUIDs player-id<->XUID lookup table is
// modelled here. The enclosing BrnNetwork::StateManager is a large manager that is not
// reconstructed by this TU; CurrentPlayerXUIDs is its self-contained nested helper.
//
// CurrentPlayerXUIDs is a fixed 8-slot table mapping a network player id to that
// player's 64-bit XUID. The X360 bodies (SetXUID @ 0x82542540, GetXUID @ 0x825425C0,
// both members of BrnNetwork::StateManager::CurrentPlayerXUIDs, cited against
// GameSource/Network/Managers/BrnNetworkStateManager.h:600/608/620) walk the table with
// a 16-byte (0x10) slot stride:
//
//   slot layout (16 bytes; SetXUID stores via `stw r4,0(slot)` + `std r5,8(slot)`):
//     +0x00 (4)  miPlayerId   network player id; free slot == KI_FREE_SLOT (-1)
//     +0x04 (4)  pad          (XUID is 8-aligned at +0x08)
//     +0x08 (8)  mu64XUID     the player's 64-bit XUID
//
//   maSlots[8]  -> 8 * 16 == 128 bytes
//
// SetXUID @ 0x82542540: linear-scans maSlots for the first slot whose miPlayerId is the
//   free sentinel (-1; the asm `lwz 0(slot); cmpwi -1; beq`), asserting "No free slots!\n"
//   (h:600) if all 8 are used, then stores the player id (`stw r4,0(slot)`) and the 64-bit
//   XUID (`std r5,8(slot)`). The Hex-Rays `*(v5+1) = *(&a3+4)` is the big-endian high-half
//   alias of that single 64-bit store -- it is ONE std of the whole XUID, not a half-store.
//
// GetXUID @ 0x825425C0: asserts lpXUID != null ("lpXUID", h:608), linear-scans for the slot
//   whose miPlayerId == liPlayerId (asserting "Couldn't find player!\n", h:620, after 8
//   misses), then writes that slot's 64-bit XUID to *lpXUID (`ld 8(slot); std 0(lpXUID)`).
//
// Both return `this` (the table pointer; r3 preserved across the body).
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (SetXUID/GetXUID guards)

namespace BrnNetwork
{
    struct StateManager
    {
        // Whether the currently-selected game mode has enough populated teams to launch
        // (the launch "locking" gate). ADDITIVE GROW (BrnNetworkLaunchManager TU):
        // declared-only; body is the StateManager's own TU
        // (GameSource/Network/Managers/BrnNetworkStateManager.cpp).
        bool GameModeHasEnoughTeams();

        // Nested fixed-size player-id -> XUID lookup table (8 slots).
        struct CurrentPlayerXUIDs
        {
            // Number of player slots (the asm scans exactly 8 entries before the
            // "No free slots!" / "Couldn't find player!" asserts fire).
            static const s32 KI_NUM_SLOTS  = 8;
            // Free-slot sentinel stored in miPlayerId until a player claims the slot.
            static const s32 KI_FREE_SLOT  = -1;

            struct Slot
            {
                s32 miPlayerId;   // +0x00 (free == KI_FREE_SLOT)
                s32 miPad;        // +0x04 (XUID 8-byte alignment)
                u64 mu64XUID;     // +0x08
            };

            // @ 0x82542540 -- claim the first free slot for liPlayerId/lqXUID.
            CurrentPlayerXUIDs* SetXUID(s32 liPlayerId, u64 lqXUID);
            // @ 0x825425C0 -- find liPlayerId and write its XUID to *lpXUID.
            CurrentPlayerXUIDs* GetXUID(s32 liPlayerId, u64* lpXUID);

            Slot maSlots[KI_NUM_SLOTS];   // 8 * 16 == 128 bytes
        };
    };
} // namespace BrnNetwork
