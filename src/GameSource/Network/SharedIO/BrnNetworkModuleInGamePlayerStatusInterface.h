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
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"          // BrnNetwork::PlayerName(16B), NetworkPlayerID, EActiveRaceCarIndex
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
        E_CAMERA_STATUS_NONE  = 0,
        E_CAMERA_STATUS_COUNT = 4,
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
            u8                      maReservedPadTo312[8];         // +304..+312

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

            // ---- declared-only API (bodies are separate TUs) ----
            const InGamePlayerStatusData* GetPlayerStatusData(s32 liIndex) const;
            const InGamePlayerStatusData* GetPlayerStatusDataByActiveRaceCarIndex(EActiveRaceCarIndex leIndex) const;
            const InGamePlayerStatusData* GetPlayerStatusDataByPlayerID(NetworkPlayerID lPlayerID) const;
            s32                           GetNumPlayers() const;
            void                          Clear();
            void                          SetNumPlayers(s32 liNumPlayers);
            const char*                   GetGameName() const;
            void                          SetGameName(const char* lpcName);
            bool                          GetLocalPlayerIsHost() const;
            void                          SetLocalPlayerIsHost(bool lbIsHost);
            NetworkPlayerID               GetNetworkIDFromPlayerName(PlayerName lName) const;

        private:
            InGamePlayerStatusData maInGamePlayerData[8]; // +0     (8 * 312 == 2496 bytes)
            char                   macGameName[36];        // +2496
            s32                    miNumPlayers;           // +2532 (In() bounds-checks against this)
            bool                   mbLocalPlayerIsHost;    // +2536
            // +2540 trailing pad word follows mbLocalPlayerIsHost.
        };
    } // namespace BrnNetworkModuleIO
} // namespace BrnNetwork
