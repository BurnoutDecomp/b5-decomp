#ifndef GAMESOURCE_REPLAYS_SERIALISERS_BRNREPLAYDIRECTORBRIDGESERIALISER_H
#define GAMESOURCE_REPLAYS_SERIALISERS_BRNREPLAYDIRECTORBRIDGESERIALISER_H

#include "types.hpp"
#include "BrnCommonTypes.h"                             // Matrix44Affine (the CoM transforms)
#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT (the lpStatic tripwires)
#include "GameSource/Replays/BrnReplayBaseSerialiser.h" // BrnReplays::BaseSerialiser

// ============================================================================
// GameSource/Replays/Serialisers/BrnReplayDirectorBridgeSerialiser.h
//
// BrnReplays::DirectorBridgeSerialiser -- the replay channel for the
// world->director bridge snapshot: the active player, the takedown record, the
// eight per-vehicle activity bytes / info records / centre-of-mass transforms,
// and the player data block. Reconstructed from BURNOUT_X360_ARTIST.XEX; the
// asserts name the original files (the getters are header-inline --
// BrnReplayDirectorBridgeSerialiser.h:193/:215/:238/:287/:310/:383-385 -- and
// Read/Write live in BrnReplayDirectorBridgeSerialiser.cpp:149..).
//
// Bodied by this TU (8 ledger functions): the six inline getters below +
// Read @0x82657478 / Write @0x82657210 (the .cpp). GetStaticLayout is its own
// ledger function (declaration-only here, the batch-11 DirectorSerialiser
// pattern).
// ============================================================================

namespace BrnReplays
{
    // The 24320-byte bridge snapshot the static buffer holds (spans pinned by the
    // Write serialise list + the getter offsets; the per-vehicle info record and
    // the trailing blocks stay opaque -- their field surfaces belong to the
    // bridge's own TUs).
    struct DirectorBridgeSerialiserStaticLayout
    {
        s8 muActivePlayerIndex;              // +0x0000 (serialised, 1; SIGNED -- the getters extsb the -1 sentinel)
        s8 muPlayerKillerIndex;              // +0x0001 (serialised, 1 -- last in the stream; signed)
        u8 mbPlayerWasTakenDown;             // +0x0002 (serialised, 1)
        u8 mabVehicleActive[8];              // +0x0003 (serialised, 8)
        u8 maPad000B[0x10 - 0x0B];           // +0x000B .. +0x000F
        u8 maVehicleInfos[8][1264];          // +0x0010 (serialised, 10112; opaque records)
        u8 maBlock5BA0[48];                  // +0x5BA0 (serialised, 48; role not recovered)
        Matrix44Affine maCentreOfMassTransforms[8];   // +0x5BD0 (serialised, 512)
        u8 maPlayerData[296];                // +0x5DD0 (serialised, 296; the truncated-symbol block)
        u8 mBlock5EF8;                       // +0x5EF8 (serialised, 1)
        u8 maPad5EF9[3];                     // +0x5EF9 .. +0x5EFB
        u8 maBlock5EFC[4];                   // +0x5EFC (serialised, 4)
    };

    class DirectorBridgeSerialiser : public BaseSerialiser
    {
    public:
        // @0x8264C498 (this file's earlier slice, de-forked onto the real base) --
        // parameterise the shared BaseSerialiser::Construct with this stream's id
        // (3), the 0x8000 buffer pair and the channel name.
        s32 Construct();

        // Own ledger functions (declaration-only): the static-buffer view with its
        // size/null tripwires, and the game-action-queue stream step both Read and
        // Write chain after the ten spans (@0x82657724 / @0x82657450 call sites).
        DirectorBridgeSerialiserStaticLayout* GetStaticLayout();
        void SerialiseGameActionQueue();

        // ---- the header-inline getters (this TU; every one carries the
        // lpStatic tripwire at its own header line, all non-gating) ----

        // @0x823A86A8 (h:193) -- the active player slot.
        s32 GetActivePlayerIndex()
        {
            DirectorBridgeSerialiserStaticLayout* lpStatic = GetStaticLayout();
            CGS_ASSERT(lpStatic != 0, "lpStatic");   // :193 (non-gating)
            return lpStatic->muActivePlayerIndex;
        }

        // @0x823A8708 (h:215) -- one vehicle's activity byte.
        bool GetVehicleActive(s32 liVehicleIndex)
        {
            DirectorBridgeSerialiserStaticLayout* lpStatic = GetStaticLayout();
            CGS_ASSERT(lpStatic != 0, "lpStatic");   // :215 (non-gating)
            return lpStatic->mabVehicleActive[liVehicleIndex] != 0;
        }

        // @0x823A8770 (h:238) -- one vehicle's 1264-byte info record (opaque; the
        // consumers own the field surface).
        void* GetVehicleInfo(s32 liVehicleIndex)
        {
            DirectorBridgeSerialiserStaticLayout* lpStatic = GetStaticLayout();
            CGS_ASSERT(lpStatic != 0, "lpStatic");   // :238 (non-gating)
            return lpStatic->maVehicleInfos[liVehicleIndex];
        }

        // @0x823A87E0 (h:287) -- one vehicle's centre-of-mass transform.
        Matrix44Affine* GetCentreOfMassTransform(s32 liVehicleIndex)
        {
            DirectorBridgeSerialiserStaticLayout* lpStatic = GetStaticLayout();
            CGS_ASSERT(lpStatic != 0, "lpStatic");   // :287 (non-gating)
            return &lpStatic->maCentreOfMassTransforms[liVehicleIndex];
        }

        // @0x823A8850 (h:310) -- the player data block (FLAG: IDA truncates the
        // symbol at "GetPl"; the accessor name is completed from the block's
        // role. Opaque -- the consumers own the field surface).
        void* GetPlayerData()
        {
            DirectorBridgeSerialiserStaticLayout* lpStatic = GetStaticLayout();
            CGS_ASSERT(lpStatic != 0, "lpStatic");   // :310 (non-gating)
            return lpStatic->maPlayerData;
        }

        // @0x823A88A8 (h:383/:384/:385) -- the takedown record (was-taken-down +
        // the killer index, both out-params; the parameter NAMES are the assert
        // texts).
        void GetPlayerTakedownInfo(bool* lpbPlayerWasTakenDown, s32* lpePlayerKiller)
        {
            DirectorBridgeSerialiserStaticLayout* lpStatic = GetStaticLayout();
            CGS_ASSERT(lpStatic != 0, "lpStatic");                    // :383 (non-gating)
            CGS_ASSERT(lpbPlayerWasTakenDown != 0, "lpbPlayerWasTakenDown");   // :384
            CGS_ASSERT(lpePlayerKiller != 0, "lpePlayerKiller");      // :385
            *lpbPlayerWasTakenDown = lpStatic->mbPlayerWasTakenDown != 0;
            *lpePlayerKiller       = lpStatic->muPlayerKillerIndex;
        }

        // @0x82657478 / @0x82657210 (this TU, the .cpp) -- the per-frame stream
        // steps (see the .cpp).
        void Read();
        void Write();
    };
}

#endif // GAMESOURCE_REPLAYS_SERIALISERS_BRNREPLAYDIRECTORBRIDGESERIALISER_H
