#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsArray.h"                                 // Array<T, N>
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"                          // CgsContainers::FastBitArray<10>
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                                // CgsModule::EventQueue<T,N>
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h"              // BrnGameState::TakedownEvent
#include "GameSource/Network/Debug Components/BrnNetworkLiveRevengeDebugComponent.h"   // BrnNetwork::LiveRevengeDebugComponent
#include "GameSource/Network/Managers/BrnNetworkLiveRevengeRelationship.h"             // BrnNetwork::LiveRevengeRelationship
#include "GameSource/Network/Messages/BrnLiveRevengeSyncMessage.h"                     // BrnNetwork::LiveRevengeSyncMessage

// Forward declarations for heavy pointer-only members.
namespace BrnNetwork { struct BrnNetworkManager; struct BrnNetworkModule; }
namespace CgsMemory  { class  HeapMalloc; }

// BrnNetwork::LiveRevengeManager + LiveRevengeProfile
// Recovered from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/Managers/BrnNetworkLiveRevengeManager.h),
// gated against the X360 ARTIST binary.
//
// The PC gate targets x64 (8-byte pointers) so no fixed-byte sizeof/offset assert is used.

namespace BrnNetwork
{
    // Alias used by the mapping-entry (BrnNetworkLiveRevengeManager.h:74 DWARF).
    // Multiple DWARF compilations spell this namespace differently
    // (RoadRulesRecvData / GuiEventNetworkLaunching); in practice it is the same
    // s32 typedef as BrnNetwork::NetworkPlayerID from BrnNetworkSharedIO.h.
    typedef s32 NetworkPlayerID;

    // BrnNetworkLiveRevengeManager.h:89 (DWARF). The saved/loaded live-revenge profile:
    // a version word followed by the fixed 250-entry relationship-history table.
    // X360-AUTHORITATIVE: the RegisterAll / UnregisterAll walkers read the table at
    // profile+8 (lwz 0x934(manager) -> profile; addi profile, 8 -> &maRelationshipTable),
    // so the table begins at +0x08 -- the version word at +0x00 plus a 4-byte alignment
    // gap. (DecFIGS lists KI_MAX_REVENGE_HISTORY == 250 and KI_VERSION_NUMBER == 6 as
    // compile-time constants, and miVersionNumber + maRelationshipTable as the two members.)
    struct LiveRevengeProfile
    {
        static const s32 KI_MAX_REVENGE_HISTORY = 250;
        static const s32 KI_VERSION_NUMBER      = 6;

        s32 miVersionNumber;                                            // +0x00
        u8  mPad_Version[4];                                            // +0x04  (align table to +0x08)
        Array<LiveRevengeRelationship, KI_MAX_REVENGE_HISTORY>
            maRelationshipTable;                                        // +0x08  (250 * 120 + count)

        void Clear();                       // BrnNetworkLiveRevengeManager.cpp
        bool IsIncorrectVersion() const;    // BrnNetworkLiveRevengeManager.cpp
        bool IsUpgradable() const;          // BrnNetworkLiveRevengeManager.cpp
        bool ValidateProfile(const LiveRevengeProfile* lpNewProfile, s32 liMaxEntries) const; // own TU
    };

    // BrnNetworkLiveRevengeManager.h:74 (DWARF). Per-active-player mapping entry:
    // binds a network player ID to a slot in the 250-entry relationship table and
    // owns the two live-sync messages (send + recv) for that player.
    struct LiveRevengeMappingEntry
    {
        NetworkPlayerID         mPlayerID;          // DWARF :74
        s32                     miRevengeTableIndex; // DWARF :75
        LiveRevengeSyncMessage  mSendMessage;        // DWARF :76
        LiveRevengeSyncMessage  mRecvMessage;        // DWARF :77
    };

    // 2-word payback network-event payload (X360: word0 @+0, word1 @+4). Consumed by
    // HandlePayback{Initialised,Succeeded}Event -> UpdatePaybacksData. X360 attests only
    // two 4-byte words (lwz 0(r31), lwz 4(r31)); field names inferred from UpdatePaybacksData
    // arg1/arg2.
    struct PaybackEvent
    {
        s32 miNetworkPlayerID;  // +0  (UpdatePaybacksData arg1)
        s32 miAggressorIndex;   // +4  (UpdatePaybacksData arg2)
    };

    // Suppress the minimal-slice forward definition in BrnNetworkManager.h when the full
    // class definition is already visible (see BrnNetworkManager.h comment).
#define BRNETWORK_LIVEREVENGEMANAGER_DEFINED

    // BrnNetworkLiveRevengeManager.h:133 (DWARF).
    struct LiveRevengeManager
    {
        // BrnNetworkLiveRevengeManager.h:281 (DWARF).
        enum ELiveRevengeUploadStatus
        {
            E_LIVE_REVENGE_UPLOAD_STATUS_PENDING     = 0,
            E_LIVE_REVENGE_UPLOAD_STATUS_IN_PROGRESS = 1,
            E_LIVE_REVENGE_UPLOAD_STATUS_IDLE        = 2,
            E_LIVE_REVENGE_UPLOAD_STATUS_COUNT       = 3,
        };

        // Nested QSort comparison data (BrnNetworkLiveRevengeManager.cpp:1582).
        struct LiveRevengeQSortData
        {
            s32 miTableIndex;
            s32 miRivalScore;
        };

        // ---- public interface ----
        void Construct(BrnNetworkModule* lpNetworkModule);
        bool Prepare(CgsMemory::HeapMalloc* lpHeapMalloc);
        bool Release();
        void Destruct();

        void ProcessBeforeSimulation(BrnNetworkModule* lpOutputBuffer);
        void ProcessTakedownQueue(void* lpOutputBuffer);
        void ProcessGameDirtyTrickInterface();

        void AddPlayer(NetworkPlayerID lNetworkPlayerID);
        void RemovePlayer(NetworkPlayerID lNetworkPlayerID);
        void Disconnected();
        void OnRoundStart();
        void OnRoundFinish();

        s32  GetNumberOfRivals() const;
        s32  GetNumberOfRelationships() const;

        LiveRevengeRelationship* GetNonConstRevengeRelationship(NetworkPlayerID lPlayerID);

        // @ 0x8258C540 -- const table-index accessor (DWARF-truncated "GetRevengeRelationshi").
        // Caller: BrnNetwork::GameSearchParamsBase::FillInRivals.
        LiveRevengeRelationship* GetRevengeRelationship(s32 liTableIndex) const;

        // Payback network-event handlers (callers: BrnNetwork::StateManager::ProcessNetworkEvents).
        void HandlePaybackInitialisedEvent(const PaybackEvent* lpPaybackEvent);   // @ 0x82561930
        void HandlePaybackSucceededEvent(const PaybackEvent* lpPaybackEvent);     // @ 0x825619A0

        void DisplayPlayerTakedownMessage(void* lpOutputBuffer, s32 liAggressorIndex, s32 liVictimIndex, s32 liUnk);
        void DisplayRivalTakedownMessage(void* lpOutputBuffer, s32 liAggressorIndex, s32 liVictimIndex, s32 liUnk);

        void HandleLiveRevengeProfileLoadedEvent(const LiveRevengeProfile* lpProfile, s32 liMaxEntries);

        void RemotePlayerFinalised(NetworkPlayerID lNetworkPlayerID);

        void SendLiveRevengeRivalsToServer();
        void UpdateTopRivals();

        // Accessor for the debug component.
        LiveRevengeProfile* GetProfile() const { return mpLiveRevengeProfile; }

    private:
        // -- private helpers --
        void  ResetRevengeTableMappings();
        LiveRevengeMappingEntry* FindMappingEntry(NetworkPlayerID lNetworkPlayerID);
        s32   FindPlayerInTableByName(const char* lpcName) const;
        s32   AddNewTableEntry(const void* lpUniqueID);
        void  AddMappingEntry(NetworkPlayerID lNetworkPlayerID, s32 liTableIndex);
        void  RemoveMappingEntry(NetworkPlayerID lNetworkPlayerID);
        s32   NetworkPlayerIDToActiveRaceCarIndex(NetworkPlayerID lNetworkPlayerID) const;
        s32   GetRivalTopIndex(s32 liTableIndex) const;
        void  ClearTopRivals();
        bool  IsTableValid() const;
        void  AutoSaveLiveRevengeProfile();
        void  UpdateMarkedManInfo();
        void  UpdateLiveRevengeRelationShip(s32 liAggressor, s32 liVictim, u8 luFlags, void* lpOutputBuffer);
        // The X360 payback handlers pass (networkPlayerID, aggressorIndex, flag) as three
        // value args (r4/r5/r6), where the 3rd is a raw payback-outcome flag (2 == Initialised,
        // 4 == Succeeded; NOT EPaybackType, whose values are 0..3).
        void  UpdatePaybacksData(s32 liNetworkPlayerID, s32 liAggressorIndex, s32 liPaybackFlag);
        void  GetUniqueIDByName(const char* lpcPlayerName, void* lpUniqueID) const;
        void  GetUniqueIDByPlayerID(NetworkPlayerID lNetworkPlayerID, void* lpUniqueID) const;
        LiveRevengeRelationship* GetNonConstRevengeRelation(NetworkPlayerID lPlayerID);

        static s32 _SortTopRivals(const void* lpRival1, const void* lpRival2);
        static void _SyncMessageArrivedCallback();
        static void _SyncMessageDeliveredCallback();

        // ---- members (DWARF order, :291..:319) ----
        LiveRevengeDebugComponent mDebugComponent;      // DWARF :291 (by-value, first member)

        // DWARF :302  (7 per-player mapping slots; each 328B on X360)
        static const s32 KI_MAX_PLAYERS = 7;
        LiveRevengeMappingEntry   maPlayerToTableIndexData[KI_MAX_PLAYERS]; // DWARF :302

        // DWARF :304..306
        static const s32 KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER = 10;
        s32  maTopIndexes[KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER]; // DWARF :305
        s32  miNumberOfTopRivals;                                   // DWARF :306

        // DWARF :308..319
        LiveRevengeProfile*                             mpLiveRevengeProfile;   // DWARF :308
        CgsModule::EventQueue<BrnGameState::TakedownEvent, 8>
                                                        mTakedownEventQueue;    // DWARF :309
        BrnNetworkManager*                              mpNetworkManager;       // DWARF :310
        BrnNetworkModule*                               mpNetworkModule;        // DWARF :311
        CgsMemory::HeapMalloc*                          mpAllocator;            // DWARF :312
        ELiveRevengeUploadStatus                        meLiveRevengeUploadStatus; // DWARF :314
        CgsContainers::FastBitArray<KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER>
                                                        maDirtyTopRivals;       // DWARF :315
        bool                                            mbAreWeInOnlineGame;    // DWARF :317
        bool                                            mbProfileIsDirty;       // DWARF :318
        bool                                            mbNeedToUpdateMarksForCurrentRound; // DWARF :319
    };
}
