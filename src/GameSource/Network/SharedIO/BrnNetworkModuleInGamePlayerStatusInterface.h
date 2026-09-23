// ===================================================================================
// BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData / InGamePlayerStatusInterface
//   -- owning header (promoted from BrnNetworkModuleInGamePlayerStatusInterface.cpp)
//   b5-decomp/src/GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h
//
// SHAPE authoritative from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h:63/117),
// gated against the X360 binary (member byte offsets / copy strides):
//   InGamePlayerStatusData::operator=          @ 0x823628C8  (memberwise; 312-byte stride)
//   InGamePlayerStatusInterface::operator=     @ 0x8236B020  (8 records + name + counts)
//   InGamePlayerStatusInterface::GetPlayerStatusDataForWriting @ 0x8230FF60 (stride 312)
//   InGamePlayerStatusData::Clear              @ 0x823555A8
//
// LAYOUT (X360-AUTHORITATIVE byte offsets; sizeof(InGamePlayerStatusData) == 312):
//   +0    NetworkPlayerStats      mPlayerStats               (136B; committed BrnNetworkPlayerStats.h)
//   +136  LiveRevengeRelationship mLiveRevengeRelationship   (120B; committed BrnNetworkLiveRevengeRelationship.h)
//   +256  PlayerName              mPlayerName                (16B; committed BrnNetworkSharedIO.h)
//   +272  NetworkPlayerID         mNetworkPlayerID           (s32)
//   +276  EActiveRaceCarIndex     meActiveRaceCarIndex       (s32)
//   +280  s32                     meVOIPStatus               (CgsNetwork::ENetworkHeadsetPlayerStatus; un-homed -> s32)
//   +284  ECameraStatus           meCameraStatus             (s32)
//   +288  NetworkPlayerID         mMarkedManPlayerID         (s32)
//   +292  EActiveRaceCarIndex     meMarkedManActiveRaceCarIndex (s32)
//   +296  s32                     meDistrict                 (BrnWorld::EDistrict; un-homed -> s32)
//   +300  bool                    mbMarkedMan
//   +301  bool                    mbIsHost
//   +302  bool                    mbIsLocalPlayer
//   +303  bool                    mbIsInLocalGameWorld
//   +304..+312 trailing pad to the 312-byte record stride
//
// The X360 InGamePl copy (0x823628C8) block-copies 120 bytes for mLiveRevengeRelationship
// (+136..+256), which is why LiveRevengeRelationship's committed home was GROWN to 120 bytes
// (see BrnNetworkLiveRevengeRelationship.h) -- placing mPlayerName at +256 and mNetworkPlayerID
// at +272 exactly as the X360 stores them.
//
// NOTE on un-homed enum types: the DWARF spells meVOIPStatus as
// CgsNetwork::ENetworkHeadsetPlayerStatus and meDistrict as BrnWorld::EDistrict. Neither has a
// committed shared home, so each is modelled as its underlying 4-byte s32 (the X360 stores
// plain words at +280/+296). meCameraStatus's enum is defined locally below. Replace the s32
// fields with the real enum typedefs when those subsystems land (offsets must not move).
#pragma once

#include "types.hpp"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"          // BrnNetwork::PlayerName(16B), NetworkPlayerID, EActiveRaceCarIndex(NONE=-1)
#include "GameSource/BurnoutConstants.h"                             // ::EActiveRaceCarIndex (0..8 : INVALID/_0../_COUNT) for MarkedManInterface
#include "GameSource/Network/Managers/BrnNetworkPlayerStats.h"       // BrnNetwork::NetworkPlayerStats (136B, committed; operator= @0x82355C50)
#include "GameSource/Network/Managers/BrnNetworkLiveRevengeRelationship.h" // BrnNetwork::LiveRevengeRelationship (120B, committed)

namespace BrnNetwork
{
    // DWARF spells this BrnNetwork::ECameraStatus; no committed shared home, so it is defined
    // here (4-byte enum). The X360 cleared record stores 4 ("default") at +284.
    // ADDITIVE GROW (BrnMugshotManager TU): the X360 cleared in-game player record stores 4 at the
    // camera-status word (KI_CAMERA_STATUS_DEFAULT below), and MugshotManager::DoesPlayerHaveACamera
    // asserts `meCameraStatus != E_CAMERA_STATUS_COUNT` then treats status 0 as "no camera". So the
    // enum spans [NONE..COUNT] with COUNT == 4 (the cleared-record sentinel). FLAG: additive only.
    enum ECameraStatus : s32
    {
        E_CAMERA_STATUS_NONE      = 0,
        E_CAMERA_STATUS_AVAILABLE = 1,
        E_CAMERA_STATUS_IN_USE    = 2,
        E_CAMERA_STATUS_DISABLED  = 3,
        E_CAMERA_STATUS_COUNT     = 4,
    };

    namespace BrnNetworkModuleIO
    {
        // Default camera status (X360 stores 4 at +284 in the cleared record).
        static const s32 KI_CAMERA_STATUS_DEFAULT = 4;

        // ===================================================================
        // InGamePlayerStatusData (DWARF BrnNetworkModuleInGamePlayerStatusInterface.h:63)
        //   One player's in-game status record. 312-byte stride (X360 In() returns 312*idx+base).
        // ===================================================================
        struct InGamePlayerStatusData
        {
            NetworkPlayerStats      mPlayerStats;                  // +0
            LiveRevengeRelationship mLiveRevengeRelationship;      // +136
            PlayerName              mPlayerName;                   // +256
            NetworkPlayerID         mNetworkPlayerID;              // +272
            EActiveRaceCarIndex     meActiveRaceCarIndex;          // +276
            s32                     meVOIPStatus;                  // +280 (CgsNetwork::ENetworkHeadsetPlayerStatus)
            ECameraStatus           meCameraStatus;                // +284
            NetworkPlayerID         mMarkedManPlayerID;            // +288
            EActiveRaceCarIndex     meMarkedManActiveRaceCarIndex; // +292
            s32                     meDistrict;                    // +296 (BrnWorld::EDistrict)
            bool                    mbMarkedMan;                   // +300
            bool                    mbIsHost;                      // +301
            bool                    mbIsLocalPlayer;               // +302
            bool                    mbIsInLocalGameWorld;          // +303
            // X360 record stride is 312 bytes (the In() accessor / array indexing uses 312*idx, and
            // the InGamePl copy reaches +304); natural C++ alignment ends the struct at +304, so an
            // 8-byte trailing reserved pad pins the array stride to the X360-authoritative 312. The
            // X360 copy stores the +304 word as inert padding; left default here.
            // +304: a byte the console build carries beyond the reference member list.
            // BrnNetworkManager::OutputPlayerStatusInfo stores false for the local player and the
            // remote BrnNetworkPlayer's eliminated flag otherwise. FLAG: named from that producer.
            bool                    mbIsEliminated;                // +304
            u8                      maReservedPadTo312[7];         // +305..+312

            void Clear();                                          // @ 0x823555A8 (body in this TU's .cpp)

            // Memberwise copy assignment @ 0x823628C8 (forwards mPlayerStats to
            // NetworkPlayerStats::operator= @0x82355C50, block-copies LiveRevenge/PlayerName,
            // copies the scalar tail). Body in this TU's .cpp.
            InGamePlayerStatusData& operator=(const InGamePlayerStatusData& lOther);
        };

        // ===================================================================
        // InGamePlayerStatusInterface (DWARF BrnNetworkModuleInGamePlayerStatusInterface.h:117)
        //   maInGamePlayerData[8] (8 * 312), macGameName[36], miNumPlayers, mbLocalPlayerIsHost.
        // ===================================================================
        struct InGamePlayerStatusInterface
        {
        public:
            // @ 0x8230FF60 -- returns &maInGamePlayerData[liIndex] (stride 312); asserts the
            // index is in [0, miNumPlayers). Body in BrnNetworkModuleIO.cpp.
            InGamePlayerStatusData* GetPlayerStatusDataForWriting(s32 liIndex);

            // @ 0x8236B020 -- memberwise copy of all 8 records (via InGamePlayerStatusData::operator=,
            // stride 312), then macGameName[36], miNumPlayers, mbLocalPlayerIsHost. Body in BrnNetworkModuleIO.cpp.
            InGamePlayerStatusInterface& operator=(const InGamePlayerStatusInterface& lOther);

            // Header-inline on the console: every reader (ModeManager, MugshotManager, OnlineFlyby-
            // Manager, MarkedManInterface, the GameModule's gui bridge) carries the two index asserts
            // and the stride-312 address computation in its own body.
            const InGamePlayerStatusData* GetPlayerStatusData(s32 liIndex) const
            {
                CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
                CGS_ASSERT(liIndex < miNumPlayers, "liIndex < miNumPlayers");
                return &maInGamePlayerData[liIndex];
            }

            // Header-inline on the console (callers read the +0x9E4 word directly, no assert).
            s32 GetNumPlayers() const { return miNumPlayers; }

            // ---- declared-only API (bodies are separate TUs) ----
            const InGamePlayerStatusData* GetPlayerStatusDataByActiveRaceCarIndex(EActiveRaceCarIndex leIndex) const;
            const InGamePlayerStatusData* GetPlayerStatusDataByPlayerID(NetworkPlayerID lPlayerID) const;
            // Header-inline on the console (the output buffer's Construct carries it): clear every
            // record, empty the game name, zero the player count. +0x9E8 / +0x9EC are left alone.
            void Clear()
            {
                for (s32 i = 0; i < 8; ++i)
                {
                    maInGamePlayerData[i].Clear();
                }
                macGameName[0] = 0;
                miNumPlayers = 0;
            }
            // Header-inline on the console: OutputPlayerStatusInfo stores the +0x9E4 word directly.
            void                          SetNumPlayers(s32 liNumPlayers) { miNumPlayers = liNumPlayers; }
            // Header-inline on the console: the gui bridge reads +0x9C0 / +0x9E8 directly.
            const char*                   GetGameName() const { return macGameName; }
            s32                           GetTotalNumberPlayers() const { return miTotalNumberPlayers; }
            // Header-inline on the console: OutputPlayerStatusInfo stores the +0x9E8 word directly.
            // FLAG: the reference lists no setter for this console-only member; named after its getter.
            void                          SetTotalNumberPlayers(s32 liTotalNumberPlayers) { miTotalNumberPlayers = liTotalNumberPlayers; }
            void                          SetGameName(const char* lpcName);
            // Header-inline on the console: ModeManager::PreWorldUpdate reads the +0x9EC byte
            // directly (plain lbz, no assert) and passes it to ChallengeManager::PreWorldUpdate.
            bool                          GetLocalPlayerIsHost() const { return mbLocalPlayerIsHost; }
            // Header-inline on the console: OutputPlayerStatusInfo stores the +0x9EC byte directly.
            void                          SetLocalPlayerIsHost(bool lbIsHost) { mbLocalPlayerIsHost = lbIsHost; }
            NetworkPlayerID               GetNetworkIDFromPlayerName(PlayerName lName) const;

        private:
            InGamePlayerStatusData maInGamePlayerData[8]; // +0     (8 * 312 == 2496 bytes)
            char                   macGameName[36];        // +2496
            s32                    miNumPlayers;           // +2532 (In() bounds-checks against this)
            // +2536: a second player count the console build carries beyond the reference member
            // list. BrnNetworkManager::OutputPlayerStatusInfo stores
            // CgsNetwork::PlayerManager::GetTotalNumberPlayers() here, and the copy-assignment
            // copies it as a word. Named from that producer; no reader is reconstructed yet.
            s32                    miTotalNumberPlayers;   // +2536
            // +2540: OutputPlayerStatusInfo stores ServerInterfaceGames::IsLocalPlayerHost() here
            // (stb), so this is mbLocalPlayerIsHost, not the +2536 word.
            bool                   mbLocalPlayerIsHost;    // +2540 (console sizeof 0x9F0)
        };

        // ===================================================================
        // MarkedManInterface  (no DWARF; shape wholly from the X360 asm of its two members)
        //   CheckForMarkedManTakedown    @ 0x82355758  (this[aggressorSlot] == victimSlot)
        //   SetFromPlayerStatusInterface @ 0x82355670  (this[player.meActiveRaceCarIndex] =
        //                                                player.meMarkedManActiveRaceCarIndex)
        //
        // This SharedIO header is the correct home: the X360 assert rodata for BOTH members carries
        // the path GameSource\Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h. Both
        // members index the table by an active-race-car slot with a 4-byte stride (slwi ...,2), so the
        // table is an 8-entry array of s32 race-car slots -- one marked-man victim slot per aggressor
        // slot. Consumers reached from BrnGameState::TakedownManager::ProcessTakedownEvent.
        // ===================================================================
        struct MarkedManInterface
        {
            // Reset every aggressor slot to INVALID. The X360 inlines this fill at the one site that
            // builds the table on the stack (TakedownManager::ProcessTakedownEvent @0x82393DE0: an
            // 8-count `stw r10(-1)` loop over the 32-byte local, immediately before
            // SetFromPlayerStatusInterface) -- slots no in-game player occupies stay INVALID. Additive
            // (no DWARF for this type); header inline.
            void Construct()
            {
                for (s32 liSlot = 0; liSlot < ::E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
                {
                    maMarkedManActiveRaceCarIndex[liSlot] = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
                }
            }

            // X360 0x82355758: does maMarkedManActiveRaceCarIndex[leAggressor] == leVictim?
            // (both slots range-asserted (> INVALID && < COUNT)). Body in this TU's .cpp.
            bool CheckForMarkedManTakedown(::EActiveRaceCarIndex leAggressorActiveRaceCarIndex,
                                           ::EActiveRaceCarIndex leVictimActiveRaceCarIndex) const;

            // X360 0x82355670: rebuild this table from every in-game player's status record
            // (this[player.meActiveRaceCarIndex] = player.meMarkedManActiveRaceCarIndex). Returns *this.
            // Body in this TU's .cpp.
            MarkedManInterface& SetFromPlayerStatusInterface(
                    const InGamePlayerStatusInterface& lPlayerStatusInterface);

        private:
            // Indexed by an aggressor's active-race-car slot (0..7); value is that aggressor's currently
            // marked victim slot. X360 stride is 4 bytes (slwi idx,2); 8 entries.
            ::EActiveRaceCarIndex maMarkedManActiveRaceCarIndex[::E_ACTIVE_RACE_CAR_INDEX_COUNT]; // +0 (8 * 4 == 32)
        };
        static_assert(sizeof(MarkedManInterface) == ::E_ACTIVE_RACE_CAR_INDEX_COUNT * 4,
                      "MarkedManInterface = 8 x s32 race-car slots (0x20)");
    } // namespace BrnNetworkModuleIO
} // namespace BrnNetwork
