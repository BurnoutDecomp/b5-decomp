#pragma once

#include <cstddef>                                                                      // offsetof (_AssertLayout)

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                                      // CGS_ASSERT (ValidateProfile, inline accessors)
#include "GameShared/GameClasses/Containers/CgsArray.h"                                 // Array<T, N>
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"                          // CgsContainers::FastBitArray<10>
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                                // CgsModule::EventQueue<T,N>
#include "GameSource/CompilerDefines/gameshared_network_defines.h"                      // ::KI_MAX_NETWORK_PLAYERS
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h"              // BrnGameState::TakedownEvent
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                             // NetworkPlayerID, EActiveRaceCarIndex, EDirtyTrickStatus
#include "GameSource/Network/Debug Components/BrnNetworkLiveRevengeDebugComponent.h"   // BrnNetwork::LiveRevengeDebugComponent
#include "GameSource/Network/Managers/BrnNetworkLiveRevengeRelationship.h"             // BrnNetwork::LiveRevengeRelationship
#include "GameSource/Network/Messages/BrnLiveRevengeSyncMessage.h"                     // BrnNetwork::LiveRevengeSyncMessage

// Forward declarations for heavy pointer-only members.
namespace BrnNetwork
{
    class BrnNetworkManager;
    class BrnNetworkModule;

    namespace BrnNetworkModuleIO
    {
        struct OutputBuffer;                 // ProcessBeforeSimulation param
        struct PostSimulationInputBuffer;    // ProcessAfterSimulation param
        struct NetworkInPaybackIntialised;   // HandlePaybackInitialisedEvent param
        struct NetworkInPaybackSucceeded;    // HandlePaybackSucceededEvent param
    }
}
namespace BrnGameState { namespace GameStateModuleIO { struct OnlineRoundResults; } }   // HandleRoundResults param
namespace CgsMemory  { class  HeapMalloc; }
namespace CgsNetwork { struct ReliableMessage; struct SignalMessage; }                   // sync-message callbacks

// BrnNetwork::LiveRevengeManager + LiveRevengeProfile
// Recovered from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/Managers/BrnNetworkLiveRevengeManager.h),
// gated against the X360 ARTIST binary.
//
// The console offsets are pinned in a 32-bit build only (_AssertLayout); members are reached by
// name on the x64 host.

namespace BrnNetwork
{
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
        Array<LiveRevengeRelationship, KI_MAX_REVENGE_HISTORY>
            maRelationshipTable;                                        // +0x08  (250 * 120 + count; 8-aligned)

        // Header inline: Prepare and Release emit the version store and the table reset in place.
        void Clear()
        {
            miVersionNumber = KI_VERSION_NUMBER;
            maRelationshipTable.Clear();
        }

        // A profile is valid when it has the current version and every relationship in its
        // table validates. (On a version mismatch the console logs "Live Revenge Profile version
        // mismatch, expected <6>, got <version>" to the network dev-log stream, which has no home
        // in this tree.)
        bool ValidateProfile()
        {
            if (miVersionNumber != KI_VERSION_NUMBER)
            {
                return false;
            }

            CGS_ASSERT(maRelationshipTable.GetLength() < static_cast<u32>(KI_MAX_REVENGE_HISTORY),
                       "maRelationshipTable.GetLength() < static_cast<uint32_t>( KI_MAX_REVENGE_HISTORY )");
            for (u32 luIndex = 0; luIndex < maRelationshipTable.GetLength(); ++luIndex)
            {
                maRelationshipTable[luIndex].Validate();
            }
            return true;
        }
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

        void ProcessBeforeSimulation(BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer);
        void ProcessAfterSimulation(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInputBuffer);

        void AddPlayer(NetworkPlayerID lNetworkPlayerID);
        void RemovePlayer(NetworkPlayerID lPlayerID);
        void Disconnected();
        void OnRoundStart();
        void OnLeaveGame();
        void OnRoundFinish();
        void OnGameFinish();

        s32  GetNumberOfRivals();
        s32  GetNumberOfRelationships();

        // Header inline: the takedown messages and RemotePlayerFinalised reach the relationship
        // through this const view of the per-player lookup.
        const LiveRevengeRelationship* GetRevengeRelationship(NetworkPlayerID lNetworkPlayerID)
        {
            return GetNonConstRevengeRelationship(lNetworkPlayerID);
        }

        // Header inline (the rival search reaches it out of line): one table row by index.
        const LiveRevengeRelationship* GetRevengeRelationshipByIndex(s32 liIndex)
        {
            CGS_ASSERT(mpLiveRevengeProfile, "mpLiveRevengeProfile");
            return &mpLiveRevengeProfile->maRelationshipTable[static_cast<u32>(liIndex)];
        }

        void RemotePlayerFinalised(NetworkPlayerID lNetworkPlayerID);

        void GetUniqueIDByName(CgsNetwork::PlayerName* lpPlayerName,
                               LiveRevengeRelationship::UniquePlayerID* lpUniqueID);

        void UpdateTopRivals();
        void SendLiveRevengeRivalsToServer();

        // Header inlines on the console (emitted out of line at the network state manager's
        // event dispatch); bodied in BrnNetworkLiveRevengeManager.cpp here.
        void HandlePaybackInitialisedEvent(const BrnNetworkModuleIO::NetworkInPaybackIntialised* lpPaybackEvent);
        void HandlePaybackSucceededEvent(const BrnNetworkModuleIO::NetworkInPaybackSucceeded* lpPaybackEvent);

        void HandleLiveRevengeProfileLoadedEvent(const LiveRevengeProfile* lpLiveRevengeProfile);
        void HandleRoundResults(const BrnGameState::GameStateModuleIO::OnlineRoundResults* lpResults);

        // The per-player relationship lookup. Private in the reference class; the network manager
        // (player status output) and the aggressive-driving manager reach it as well.
        LiveRevengeRelationship* GetNonConstRevengeRelationship(NetworkPlayerID lNetworkPlayerID);
        // Transitional spelling kept for the two outside callers
        // (BrnNetworkManager::OutputPlayerStatusInfo, NetworkAggressiveDrivingManager::AddTakedownEvent);
        // delete once they call GetNonConstRevengeRelationship.
        LiveRevengeRelationship* GetNonConstRevengeRelation(NetworkPlayerID lNetworkPlayerID)
        {
            return GetNonConstRevengeRelationship(lNetworkPlayerID);
        }

        // Private in the reference class; the debug component and the rival search read it.
        LiveRevengeProfile* GetProfile() { return mpLiveRevengeProfile; }

    private:
        void DisplayRivalTakedownMessage(BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer,
                                         EActiveRaceCarIndex leAggressorActiveRaceCarIndex,
                                         EActiveRaceCarIndex leVictimActiveRaceCarIndex);
        void DisplayPlayerTakedownMessage(BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer,
                                          EActiveRaceCarIndex leAggressorActiveRaceCarIndex,
                                          EActiveRaceCarIndex leVictimActiveRaceCarIndex);
        s32   FindPlayerInTableByName(const char* lpcName);
        s32   AddNewTableEntry(const LiveRevengeRelationship::UniquePlayerID* lpUniqueID);
        void  ProcessTakedownQueue(BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer);
        void  UpdateLiveRevengeRelationShip(EActiveRaceCarIndex leAggressorActiveRaceCarIndex,
                                            EActiveRaceCarIndex leVictimActiveRaceCarIndex,
                                            bool lbMarkedManTakedown,
                                            BrnNetworkModuleIO::OutputBuffer* lpOutputBuffer);
        void  UpdatePaybacksData(EActiveRaceCarIndex leAggressorActiveRaceCarIndex,
                                 EActiveRaceCarIndex leVictimActiveRaceCarIndex,
                                 EDirtyTrickStatus leDirtyTrickStatus);
        void  ResetRevengeTableMappings();
        EActiveRaceCarIndex NetworkPlayerIDToActiveRaceCarIndex(NetworkPlayerID lNetworkPlayerID);
        void  AddMappingEntry(NetworkPlayerID lNetworkPlayerID, s32 liRevengeTableIndex);
        void  RemoveMappingEntry(NetworkPlayerID lNetworkPlayerID);
        LiveRevengeMappingEntry* FindMappingEntry(NetworkPlayerID lNetworkPlayerID);
        s32   GetRivalTopIndex(s32 liTableIndex) const;
        void  ClearTopRivals();
        void  ProcessGameDirtyTrickInterface();
        void  SyncMessageArrivedCallback(LiveRevengeSyncMessage* lpMessage, NetworkPlayerID lRemotePlayerID);
        void  AutoSaveLiveRevengeProfile();
        void  UpdateMarkedManInfo();
        bool  IsTableValid();

        static void _SyncMessageArrivedCallback(CgsNetwork::ReliableMessage* lpMessage,
                                                NetworkPlayerID lSendingPlayerID, void* lpData);
        static void _SyncMessageDeliveredCallback(bool lbSuccess, bool lbFakeNack,
                                                  CgsNetwork::SignalMessage* lpAck,
                                                  NetworkPlayerID lRecvingPlayerID, void* lpData);
        static int  _SortTopRivals(const void* lpRival1, const void* lpRival2);

        void  AddRelationshipToDebugMenu(s32 liLiveRevengeTableIndex);
        void  RemoveRelationshipFromDebugMenu(s32 liLiveRevengeTableIndex);

        // ---- members (DWARF order, :291..:319) ----
        LiveRevengeDebugComponent mDebugComponent;      // DWARF :291 (by-value, first member)

        // One mapping slot per remote player (0x148 console bytes each).
        LiveRevengeMappingEntry   maPlayerToTableIndexData[KI_MAX_NETWORK_PLAYERS];

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

        // Console layout (0xAA8 bytes), pinned in a 32-bit build; inert on the x64 host. The
        // mapping-entry stride is 0x148 once LiveRevengeSyncMessage reproduces its 0xA0 console
        // bytes; until then the members past the table are pinned relative to the entry size.
        static void _AssertLayout();
    };

    inline void LiveRevengeManager::_AssertLayout()
    {
        static_assert(sizeof(void*) != 4 || offsetof(LiveRevengeMappingEntry, mSendMessage) == 0x8, "LiveRevengeMappingEntry::mSendMessage @ +0x8");
        static_assert(sizeof(void*) != 4 || sizeof(LiveRevengeMappingEntry) == 8 + 2 * sizeof(LiveRevengeSyncMessage), "LiveRevengeMappingEntry is two ids plus two messages");
        static_assert(sizeof(void*) != 4 || offsetof(LiveRevengeManager, maPlayerToTableIndexData) == 0x10, "maPlayerToTableIndexData @ +0x10");
#define BRN_LRM_AT(member, off)         static_assert(sizeof(void*) != 4 || offsetof(LiveRevengeManager, member) == 0x10 + KI_MAX_NETWORK_PLAYERS * sizeof(LiveRevengeMappingEntry) + (off), #member)
        BRN_LRM_AT(maTopIndexes,              0x00);   // +0x908
        BRN_LRM_AT(mpLiveRevengeProfile,      0x2C);   // +0x934
        BRN_LRM_AT(mTakedownEventQueue,       0x30);   // +0x938
        BRN_LRM_AT(mpNetworkManager,          0x180);  // +0xA88
        BRN_LRM_AT(meLiveRevengeUploadStatus, 0x18C);  // +0xA94
        BRN_LRM_AT(maDirtyTopRivals,          0x190);  // +0xA98
        BRN_LRM_AT(mbAreWeInOnlineGame,       0x198);  // +0xAA0
#undef BRN_LRM_AT
        static_assert(sizeof(void*) != 4 || sizeof(LiveRevengeManager) == 0x10 + KI_MAX_NETWORK_PLAYERS * sizeof(LiveRevengeMappingEntry) + 0x1A0, "LiveRevengeManager tail is 0x1A0 bytes");
        static_assert(sizeof(void*) != 4 || offsetof(LiveRevengeProfile, maRelationshipTable) == 0x8, "LiveRevengeProfile::maRelationshipTable @ +0x8");
        static_assert(sizeof(void*) != 4 || sizeof(LiveRevengeProfile) == 0x7540, "sizeof(LiveRevengeProfile) == 0x7540");
    }
}
