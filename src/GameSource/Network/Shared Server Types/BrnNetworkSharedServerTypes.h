// ===================================================================================
// BrnNetwork::ServerGeneratedTypes  -- owning header
//   b5-decomp/src/GameSource/Network/Shared Server Types/BrnNetworkSharedServerTypes.h
//
// The server-generated wire record types shared between the game and the back-end
// custom-command protocol. Reconstructed (member names/types/order) from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/Shared Server Types/BrnNetworkSharedServerTypes.h),
// gated against the X360 binary.
//
// OfflineProgressionT in particular is the OFFPROG payload uploaded by
// ServerInterfaceCustomCommands::UploadOfflineProgress (@0x82590908): that body packs this
// 64-byte record with the field pattern "lllwwwbbb13s13s13sb" (3 longs, 3 words, 3 bytes,
// three 13-char strings, one trailing byte) at packed size 64 -- exactly the member layout
// below. Only the type's existence + this field set are needed by the homing TUs; this is
// the first reconstruction.
//
// No fixed-byte sizeof static_assert is emitted (the PC gate targets x64; the layout here is
// natural-aligned and packs identically, but absolute-offset pinning is reserved for when a
// homing TU proves it).
#pragma once

#include "types.hpp"

namespace BrnNetwork
{
    namespace ServerGeneratedTypes
    {
        // DWARF BrnNetworkSharedServerTypes.h:70 -- one player's accumulated offline-play
        // progress, uploaded to the server on reconnect.
        struct OfflineProgressionT
        {
            s32  miTotalTime;            // +0x00
            s32  miTotalDistance;        // +0x04
            s32  miTakedowns;            // +0x08
            s16  mi16Jumps;              // +0x0C
            s16  mi16Smashes;            // +0x0E
            s16  mi16Stunts;             // +0x10
            s8   mi8CarsCollected;       // +0x12
            s8   mi8TimeRoadRulesWon;    // +0x13
            s8   mi8CrashRoadRulesWon;   // +0x14
            char macFavouriteCar[13];    // +0x15
            char macForgottenCar[13];    // +0x22
            char macNemesis[13];         // +0x2F
            s8   mi8AchievementsEarnt;   // +0x3C  (packed size 64 with trailing alignment)
        };

        // DWARF BrnNetworkSharedServerTypes.h:88 -- one rival relationship record (uploaded by
        // ServerInterfaceCustomCommands::UploadLiveRevengeData). Declared for completeness of
        // this header's type family.
        struct RivalDataT
        {
            s32  miRivalryStatus;        // +0x00
            s32  miTimesBattled;         // +0x04
            s32  miTakedownsFor;         // +0x08
            s32  miTakedownsAgainst;     // +0x0C
            s32  miMugshotsFor;          // +0x10
            s32  miMugshotsAgainst;      // +0x14
            s32  miMarksFor;             // +0x18
            s32  miMarksAgainst;         // +0x1C
            s32  miPaybacksScoredFor;    // +0x20
            s32  miPaybacksScoredAgainst;// +0x24
            s32  miPaybacksDealtFor;     // +0x28
            s32  miPaybacksDealtAgainst; // +0x2C
            char macPers[16];            // +0x30
        };
    }
}
