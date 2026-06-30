// ===================================================================================
// BrnNetwork::NetworkRoadRulesManager  -- owning header
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkRoadRulesManager.h
//
// The online road-rules (challenge high-score) sync manager embedded in BrnNetworkManager.
// It keeps a fixed table of up-to-7 per-player road-rules data slots (keyed by NetworkPlayerID),
// pulls the road-rules client-config (the score-key string + the periodic reset date) down at
// auto-login, and drives the download / upload of road-rules high scores through the server.
//
// SHAPE: no DecFIGS DWARF is published for this TU; the member set / byte layout below is
// recovered from the X360 ARTIST binary (the constructor @ 0x827E2BE8 and the GetRoadRules*
// /OnAutoLogin bodies, scratchpad/wave33). The committed sibling managers
// (BrnNetworkPlayerStatsManager / BrnNetworkAggressiveDrivingManager) supply the member-naming
// + sub-object-construction idiom mirrored here.
//
// LAYOUT (absolute X360 byte offsets, the spine of the recovered bodies):
//   +0x0000  maPlayerData[7]                  -- 7 PerPlayerData slots, stride 0x2B8 (696 B)
//                                                (each slot's key NetworkPlayerID sits at +0x2B0)
//   +0x3588  maRoadRulesScoreKey[16]          -- "ROAD_RULES_SKEY" client-config string buffer
//   +0x3610  miField3610 / mfField3614        -- score/time pair (ctor-cleared to 0 / 0.0f)
//   +0x3618  miField3618 / mfField361C        -- score/time pair (ctor-cleared to 0 / 0.0f)
//   +0x3634  miState                          -- auto-login state word (set to 5 once primed)
//   +0x3644  mpServerInterface                -- BrnServerInterface*  (server-info / config source)
//   +0x3648  mpNetworkModule                  -- BrnNetworkModule*     (event queue / managers)
//   +0x364C  mbHaveLocalRoadRulesScores       -- byte flag gating the local-scores download
//   +0x3650  mField3650Vtable                 -- embedded sub-object vtable slot (ctor off_820CE18C)
//
// No fixed-byte sizeof/offset static_assert is emitted: the PC gate targets x64, where the
// embedded pointers and per-slot sub-objects widen and shift every absolute byte offset relative
// to the X360 32-bit layout the bodies were read from. The by-name member walk the compiler emits
// is identical. The X360 absolute offsets are quoted in the .cpp only as provenance.
#pragma once

#include <cstddef>                                        // offsetof (uncalled _AssertLayout)

#include "types.hpp"
#include "SharedClasses/StreetData/BrnChallengeData.h"   // BrnStreetData::ChallengePlayerScoreEntry (HandleNewPersonalBest param)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"  // BrnNetwork::NetworkPlayerID

namespace BrnNetwork
{
    class BrnNetworkModule;    // pointer-only forward (mpNetworkModule)
    class BrnServerInterface;  // pointer-only forward (mpServerInterface)

    class NetworkRoadRulesManager
    {
    public:
        // The C++ constructor (@ 0x827E2BE8) -- installs the embedded per-slot sub-object vtables
        // and clears the manager's own score/time scalar pair. Distinct from any Brn lifecycle
        // Construct().
        NetworkRoadRulesManager();

        // Debug "Trigger Personal Best": feed a fabricated personal-best record through the normal
        // new-PB pipeline (the debug component builds the record on the stack and hands it over).
        void HandleNewPersonalBest( BrnStreetData::ChallengePlayerScoreEntry* lpScoreEntry );

        // Debug "Get Road Rules High Scores": kick off the server-side road-rules score download.
        void StartDownloadingRoadRulesScoresFromServer();

        // @ 0x82564338 -- auto-login hook. Once the auto-login process has been primed (miState != 0)
        // it just signals the network manager that the process completed; before then, when the
        // current game mode warrants it, it pulls the road-rules client-config (score-key + reset
        // date), posts the reset-date network event, and kicks off the high-score download/upload.
        void OnAutoLogin();

    private:
        // Number of per-player road-rules data slots (the maPlayerData array length).
        static const s32 KI_NUMBER_OF_PLAYER_DATA_SLOTS = 7;

        // One per-player road-rules data slot (X360 stride 0x2B8 == 696 B). The recovered ctor
        // installs four embedded sub-object vtables inside each slot (slot-relative +0x000/+0x120/
        // +0x240/+0x278); the slot's key NetworkPlayerID lives at slot-relative +0x2B0. The
        // concrete sub-object/score types are not published by any dossier/DWARF, so the slot is
        // modelled honestly as opaque storage with only the grounded key exposed by name; the
        // sub-object vtable installs are left to those sub-objects' own (inlined) constructors,
        // matching the committed-sibling sub-object-construction idiom.
        struct PerPlayerData
        {
            u8              maOpaqueBefore[0x2B0];   // four sub-object vtable slots + score payload
            NetworkPlayerID mPlayerID;               // slot-relative +0x2B0 (key; -1 == free slot)
            u8              maOpaqueAfter[0x2B8 - 0x2B0 - sizeof(NetworkPlayerID)];
        };

        // X360 0x82549F60 -- return the first free (mPlayerID == -1) slot, or nullptr after asserting
        // when the table is full. Reached by AddPlayer.
        PerPlayerData* GetNextFreeRoadRulesDataEntry();

        // X360 0x82549ED0 -- return the slot keyed by lPlayerID (asserts lPlayerID is valid first),
        // or nullptr when the player has no slot. Reached by RemovePlayer.
        PerPlayerData* GetRoadRulesDataForPlayer( NetworkPlayerID lPlayerID );

        // ---- helpers homed in THIS TU's .cpp; declared (callees of OnAutoLogin) ---------------
        // Bodies belong to this manager's own behavioural TUs (no ground-truth asm in this TU),
        // so they are declared-only here and as sibling forwards in the .cpp.
        u32  AttemptToDownloadLocalRoadRulesHighScores();
        u32  AttemptToDownloadRoadRulesHighScores();
        s32  AttemptToUploadNewRoadRulesScores();

        // ---- data layout (X360 absolute offsets above) ----------------------------------------
        PerPlayerData maPlayerData[KI_NUMBER_OF_PLAYER_DATA_SLOTS];  // +0x0000
        char          maRoadRulesScoreKey[16];                      // +0x3588 ("ROAD_RULES_SKEY")
        s32           miField3610;                                  // +0x3610 (ctor 0)
        f32           mfField3614;                                  // +0x3614 (ctor 0.0f)
        s32           miField3618;                                  // +0x3618 (ctor 0)
        f32           mfField361C;                                  // +0x361C (ctor 0.0f)
        u8            maPadToState[0x3634 - 0x3620];                // -> +0x3634
        s32           miState;                                      // +0x3634
        u8            maPadToServerInterface[0x3644 - 0x3638];      // -> +0x3644
        BrnServerInterface* mpServerInterface;                      // +0x3644
        BrnNetworkModule*   mpNetworkModule;                        // +0x3648
        bool          mbHaveLocalRoadRulesScores;                  // +0x364C
        u8            maPadToVtable[0x3650 - 0x364D];               // -> +0x3650
        void*         mField3650Vtable;                            // +0x3650 (ctor off_820CE18C)

        // Uncalled layout pin. PerPlayerData holds no pointers, so its stride (X360 slot stride
        // 0x2B8) and the key offset (slot-relative +0x2B0) are pointer-INVARIANT and hold on the
        // LLP64 gate host -- they are the spine of the GetRoadRules* slot scans.
        static void _AssertLayout()
        {
            static_assert( sizeof(PerPlayerData) == 0x2B8,
                           "PerPlayerData stride must be 696 (X360 0x2B8)" );
            static_assert( offsetof(PerPlayerData, mPlayerID) == 0x2B0,
                           "PerPlayerData key must sit at slot-relative +0x2B0" );
        }
    };
}
