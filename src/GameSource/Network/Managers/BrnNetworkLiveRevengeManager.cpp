// GameSource/Network/Managers/BrnNetworkLiveRevengeManager.cpp
//
// BrnNetwork::LiveRevengeManager -- live-revenge rival-tracking manager.
// Reconstruction from X360 pseudocode + ARTIST asm + DecFIGS DWARF.
//
// Functions (24):
//   Construct                          @0x8255FAC0 [EXECUTED in goal trace]
//   Destruct                           @0x8255AB30
//   Prepare                            @0x8255A8B8
//   Release                            @0x8255AA20
//   Disconnected                       @0x825532B8
//   OnRoundStart                       @0x82547710
//   OnRoundFinish                      @(DWARF :349)
//   AddPlayer                          @0x8256DA90
//   RemovePlayer                       @0x8255FB98
//   RemoveMappingEntry                 @0x825477F0
//   RemotePlayerFinalised              @0x82560260
//   GetNumberOfRelationships           @0x82553578
//   NetworkPlayerIDToActiveRaceCarIndex @0x82547778
//   DisplayPlayerTakedownMessage       @0x8255FE70
//   DisplayRivalTakedownMessage        @0x82560020
//   HandleLiveRevengeProfileLoadedEvent @0x82560A30
//   ProcessBeforeSimulation            @0x8256D868
//   ProcessGameDirtyTrickInterface     @0x82562F80
//   ProcessTakedownQueue               @0x82568C88
//   SendLiveRevengeRivalsToServer      @0x82560358 (local variable allocation has failed)
//   ResetRevengeTableMappings          @(DWARF :887)
//   FindMappingEntry                   @(DWARF :1118)
//   GetRivalTopIndex                   @(DWARF :1608)
//   ClearTopRivals                     @(DWARF :1643)

#include "GameSource/Network/Managers/BrnNetworkLiveRevengeManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameSource/Network/BrnNetworkModule.h"
#include "GameSource/Network/BrnNetworkManager.h"
#include "GameSource/Network/Debug Components/BrnNetworkLiveRevengeDebugComponent.h"
#include "GameSource/Network/Messages/BrnLiveRevengeSyncMessage.h"
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h"
#include "GameSource/Network/SharedIO/BrnNetworkToGuiEvents.h"

// Forward declaration for an unresolved callee in AddPlayer / RemovePlayer
// (IDA 0x824F7388 — maps network-player-ID to a LiveRevengeRelationship* in the table).
namespace BrnNetwork
{
    LiveRevengeRelationship* LiveRevengeRelations(LiveRevengeProfile* lpProfile, s32 liTableIndex);
}

namespace BrnNetwork
{

const s32 KI_INVALID_RACE_CAR_INDEX_TO_REVENGE_TABLE = -1;

// ============================================================================
// LiveRevengeProfile methods
// ============================================================================

void LiveRevengeProfile::Clear()
{
    miVersionNumber = KI_VERSION_NUMBER;
    maRelationshipTable.Array<LiveRevengeRelationship, KI_MAX_REVENGE_HISTORY>::Clear();
}

bool LiveRevengeProfile::IsIncorrectVersion() const
{
    return miVersionNumber != KI_VERSION_NUMBER;
}

bool LiveRevengeProfile::IsUpgradable() const
{
    return false;
}

bool LiveRevengeProfile::ValidateProfile(const LiveRevengeProfile* /*lpNewProfile*/, s32 /*liMaxEntries*/) const
{
    return true;
}

// ============================================================================
// LiveRevengeManager::Construct  @0x8255FAC0  [EXECUTED in goal trace]
// ============================================================================
void LiveRevengeManager::Construct(BrnNetworkModule* lpNetworkModule)
{
    mbAreWeInOnlineGame  = false;
    mpLiveRevengeProfile = nullptr;
    miNumberOfTopRivals  = 0;

    mTakedownEventQueue.CgsModule::EventQueue<BrnGameState::TakedownEvent, 8>::Construct();

    CGS_ASSERT(lpNetworkModule != nullptr, "lpNetworkModule");

    mpNetworkModule  = lpNetworkModule;
    mpNetworkManager = lpNetworkModule->BrnNetwork::BrnNetworkModule::GetNetworkManager();

    CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");

    miNumberOfTopRivals = 0;
    for (s32 i = 0; i < KI_MAX_PLAYERS; ++i)
        maPlayerToTableIndexData[i].mPlayerID = KI_INVALID_RACE_CAR_INDEX_TO_REVENGE_TABLE;
    for (s32 j = 0; j < KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER; ++j)
        maTopIndexes[j] = -1;

    maDirtyTopRivals.CgsContainers::FastBitArray<KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER>::Construct();
    meLiveRevengeUploadStatus = E_LIVE_REVENGE_UPLOAD_STATUS_IDLE;
    mbProfileIsDirty = false;
    mbNeedToUpdateMarksForCurrentRound = false;

    mDebugComponent.BrnNetwork::LiveRevengeDebugComponent::Construct(this);
}

// ============================================================================
// LiveRevengeManager::Destruct  @0x8255AB30
// ============================================================================
void LiveRevengeManager::Destruct()
{
    mbAreWeInOnlineGame  = false;
    mpLiveRevengeProfile = nullptr;
    mpAllocator          = nullptr;
    miNumberOfTopRivals  = 0;
    meLiveRevengeUploadStatus = E_LIVE_REVENGE_UPLOAD_STATUS_IDLE;
    mbProfileIsDirty     = false;
    mbNeedToUpdateMarksForCurrentRound = false;

    for (s32 i = 0; i < KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER; ++i)
        maTopIndexes[i] = -1;

    maDirtyTopRivals.CgsContainers::FastBitArray<KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER>::Construct();
}

// ============================================================================
// LiveRevengeManager::Prepare  @0x8255A8B8
// ============================================================================
bool LiveRevengeManager::Prepare(CgsMemory::HeapMalloc* lpHeapMalloc)
{
    CGS_ASSERT(lpHeapMalloc != nullptr, "lpHeapMalloc");

    mpAllocator = lpHeapMalloc;

    mpLiveRevengeProfile = static_cast<LiveRevengeProfile*>(
        lpHeapMalloc->CgsMemory::HeapMalloc::Malloc(sizeof(LiveRevengeProfile), 4));

    CGS_ASSERT(mpLiveRevengeProfile != nullptr, "mpLiveRevengeProfile");

    mpLiveRevengeProfile->miVersionNumber = LiveRevengeProfile::KI_VERSION_NUMBER;
    mpLiveRevengeProfile->maRelationshipTable.Array<
        LiveRevengeRelationship, LiveRevengeProfile::KI_MAX_REVENGE_HISTORY>::Clear();

    for (s32 i = 0; i < KI_MAX_PLAYERS; ++i)
        maPlayerToTableIndexData[i].mPlayerID = KI_INVALID_RACE_CAR_INDEX_TO_REVENGE_TABLE;

    miNumberOfTopRivals  = 0;
    mbProfileIsDirty     = false;
    mbNeedToUpdateMarksForCurrentRound = false;
    meLiveRevengeUploadStatus = E_LIVE_REVENGE_UPLOAD_STATUS_IDLE;

    for (s32 j = 0; j < KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER; ++j)
        maTopIndexes[j] = -1;

    maDirtyTopRivals.CgsContainers::FastBitArray<KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER>::Construct();

    return true;
}

// ============================================================================
// LiveRevengeManager::Release  @0x8255AA20
// ============================================================================
bool LiveRevengeManager::Release()
{
    CGS_ASSERT(mpLiveRevengeProfile != nullptr, "mpLiveRevengeProfile");
    CGS_ASSERT(mpAllocator != nullptr, "mpAllocator");

    mpLiveRevengeProfile->miVersionNumber = LiveRevengeProfile::KI_VERSION_NUMBER;
    mpLiveRevengeProfile->maRelationshipTable.Array<
        LiveRevengeRelationship, LiveRevengeProfile::KI_MAX_REVENGE_HISTORY>::Clear();

    mpAllocator->CgsMemory::HeapMalloc::Free(mpLiveRevengeProfile);

    mpLiveRevengeProfile = nullptr;
    mpAllocator          = nullptr;
    mpNetworkManager     = nullptr;

    for (s32 i = 0; i < KI_MAX_PLAYERS; ++i)
        maPlayerToTableIndexData[i].mPlayerID = KI_INVALID_RACE_CAR_INDEX_TO_REVENGE_TABLE;

    miNumberOfTopRivals  = 0;
    mbProfileIsDirty     = false;
    mbNeedToUpdateMarksForCurrentRound = false;
    meLiveRevengeUploadStatus = E_LIVE_REVENGE_UPLOAD_STATUS_IDLE;

    for (s32 j = 0; j < KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER; ++j)
        maTopIndexes[j] = -1;

    maDirtyTopRivals.CgsContainers::FastBitArray<KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER>::Construct();

    return true;
}

// ============================================================================
// LiveRevengeManager::Disconnected  @0x825532B8
// ============================================================================
void LiveRevengeManager::Disconnected()
{
    for (s32 i = 0; i < KI_MAX_PLAYERS; ++i)
        maPlayerToTableIndexData[i].mPlayerID = KI_INVALID_RACE_CAR_INDEX_TO_REVENGE_TABLE;
}

// ============================================================================
// LiveRevengeManager::OnRoundStart  @0x82547710
// ============================================================================
void LiveRevengeManager::OnRoundStart()
{
    mbAreWeInOnlineGame = true;

    mbNeedToUpdateMarksForCurrentRound = true;
}

// ============================================================================
// LiveRevengeManager::OnRoundFinish  (DWARF :349)
// ============================================================================
void LiveRevengeManager::OnRoundFinish()
{
    for (s32 i = 0; i < KI_MAX_PLAYERS; ++i)
    {
        LiveRevengeMappingEntry* lpEntry = &maPlayerToTableIndexData[i];
        if (lpEntry->mPlayerID != KI_INVALID_RACE_CAR_INDEX_TO_REVENGE_TABLE)
        {
            LiveRevengeProfile* lpProfile = GetProfile();
            if (lpProfile)
                (void)lpProfile->maRelationshipTable.Array<
                    LiveRevengeRelationship, LiveRevengeProfile::KI_MAX_REVENGE_HISTORY>::operator[](
                    lpEntry->miRevengeTableIndex);
        }
    }
    AutoSaveLiveRevengeProfile();
}

// ============================================================================
// LiveRevengeManager::GetNumberOfRelationships  @0x82553578
// ============================================================================
s32 LiveRevengeManager::GetNumberOfRelationships() const
{
    if (!mpLiveRevengeProfile)
        return 0;
    return mpLiveRevengeProfile->maRelationshipTable.Array<
        LiveRevengeRelationship, LiveRevengeProfile::KI_MAX_REVENGE_HISTORY>::GetLength();
}

// ============================================================================
// LiveRevengeManager::GetNumberOfRivals
// ============================================================================
s32 LiveRevengeManager::GetNumberOfRivals() const
{
    return miNumberOfTopRivals;
}

// ============================================================================
// LiveRevengeManager::IsTableValid
// ============================================================================
bool LiveRevengeManager::IsTableValid() const
{
    return mpLiveRevengeProfile != nullptr;
}

// ============================================================================
// LiveRevengeManager::AutoSaveLiveRevengeProfile
// ============================================================================
void LiveRevengeManager::AutoSaveLiveRevengeProfile()
{
    mbProfileIsDirty = true;
}

// ============================================================================
// LiveRevengeManager::UpdateMarkedManInfo
// ============================================================================
void LiveRevengeManager::UpdateMarkedManInfo()
{
    mbNeedToUpdateMarksForCurrentRound = false;
}

// ============================================================================
// LiveRevengeManager::GetNonConstRevengeRelation  @0x8255AFB8  (private)
// ============================================================================
LiveRevengeRelationship* LiveRevengeManager::GetNonConstRevengeRelation(NetworkPlayerID lPlayerID)
{
    CGS_ASSERT(lPlayerID != -1, "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

    s32 liIndex = 0;
    while (maPlayerToTableIndexData[liIndex].mPlayerID != lPlayerID)
    {
        ++liIndex;
        if (liIndex >= KI_MAX_PLAYERS)
        {
            // Streamed StrStream assert ("Could not find PlayerID <id> in
            // BrnNetwork::LiveRevengeManager::GetNonConstRevengeRelationship") collapses to
            // the leading static literal (project convention; keep its trailing space).
            CGS_ASSERT(false, "Could not find PlayerID ");
            return nullptr;
        }
    }

    LiveRevengeMappingEntry* lpEntry = &maPlayerToTableIndexData[liIndex];

    CGS_ASSERT(lpEntry->miRevengeTableIndex >= 0,
               "lpEntry->miRevengeTableIndex >= 0");
    CGS_ASSERT(lpEntry->miRevengeTableIndex < LiveRevengeProfile::KI_MAX_REVENGE_HISTORY,
               "lpEntry->miRevengeTableIndex < LiveRevengeProfile::KI_MAX_REVENGE_HISTORY");

    CGS_ASSERT(mpLiveRevengeProfile != nullptr, "lpProfile");

    return BrnNetwork::LiveRevengeRelations(mpLiveRevengeProfile, lpEntry->miRevengeTableIndex);
}

// ============================================================================
// LiveRevengeManager::GetNonConstRevengeRelationship (public)
// ============================================================================
LiveRevengeRelationship* LiveRevengeManager::GetNonConstRevengeRelationship(NetworkPlayerID lPlayerID)
{
    return GetNonConstRevengeRelation(lPlayerID);
}

// ============================================================================
// LiveRevengeManager::GetRevengeRelationship  @0x8258C540  (const, public)
// (DWARF-truncated as "GetRevengeRelationshi")
// ============================================================================
LiveRevengeRelationship* LiveRevengeManager::GetRevengeRelationship(s32 liTableIndex) const
{
    CGS_ASSERT(mpLiveRevengeProfile != nullptr, "mpLiveRevengeProfile");

    return BrnNetwork::LiveRevengeRelations(mpLiveRevengeProfile, liTableIndex);
}

// ============================================================================
// LiveRevengeManager::HandlePaybackInitialisedEvent  @0x82561930
// ============================================================================
void LiveRevengeManager::HandlePaybackInitialisedEvent(const PaybackEvent* lpPaybackEvent)
{
    CGS_ASSERT(lpPaybackEvent != nullptr, "lpPaybackEvent");

    // asm: UpdatePaybacksData(*a2, a2[1], 2) -- three value args (r4/r5/r6); r6 == the
    // payback-outcome flag (2 == Initialised).
    UpdatePaybacksData(lpPaybackEvent->miNetworkPlayerID,
                       lpPaybackEvent->miAggressorIndex,
                       2);
}

// ============================================================================
// LiveRevengeManager::HandlePaybackSucceededEvent  @0x825619A0
// ============================================================================
void LiveRevengeManager::HandlePaybackSucceededEvent(const PaybackEvent* lpPaybackEvent)
{
    CGS_ASSERT(lpPaybackEvent != nullptr, "lpPaybackEvent");

    // asm: UpdatePaybacksData(*a2, a2[1], 4) -- three value args (r4/r5/r6); r6 == the
    // payback-outcome flag (4 == Succeeded).
    UpdatePaybacksData(lpPaybackEvent->miNetworkPlayerID,
                       lpPaybackEvent->miAggressorIndex,
                       4);
}

// ============================================================================
// LiveRevengeManager::NetworkPlayerIDToActiveRaceCarIndex  @0x82547778
// ============================================================================
s32 LiveRevengeManager::NetworkPlayerIDToActiveRaceCarIndex(NetworkPlayerID lNetworkPlayerID) const
{
    CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
    return static_cast<s32>(
        mpNetworkModule->BrnNetwork::BrnNetworkModule::GetGameStateToNetworkInterface()
            ->BrnNetwork::BrnNetworkModuleIO::GameStateToNetworkInterface::GetActiveRaceCarIndex(
                lNetworkPlayerID));
}

// ============================================================================
// LiveRevengeManager::ResetRevengeTableMappings  (DWARF :887)
// ============================================================================
void LiveRevengeManager::ResetRevengeTableMappings()
{
    for (s32 liIndex = 0; liIndex < KI_MAX_PLAYERS; ++liIndex)
        maPlayerToTableIndexData[liIndex].mPlayerID = KI_INVALID_RACE_CAR_INDEX_TO_REVENGE_TABLE;
}

// ============================================================================
// LiveRevengeManager::FindMappingEntry  (DWARF :1118)
// ============================================================================
LiveRevengeMappingEntry* LiveRevengeManager::FindMappingEntry(NetworkPlayerID lNetworkPlayerID)
{
    for (s32 liIndex = 0; liIndex < KI_MAX_PLAYERS; ++liIndex)
    {
        if (maPlayerToTableIndexData[liIndex].mPlayerID == lNetworkPlayerID)
            return &maPlayerToTableIndexData[liIndex];
    }
    return nullptr;
}

// ============================================================================
// LiveRevengeManager::FindPlayerInTableByName  (DWARF :512)
// ============================================================================
s32 LiveRevengeManager::FindPlayerInTableByName(const char* lpcName) const
{
    LiveRevengeProfile* lpProfile = GetProfile();
    if (!lpProfile)
        return -1;
    s32 liNumberOfRivals = lpProfile->maRelationshipTable.Array<
        LiveRevengeRelationship, LiveRevengeProfile::KI_MAX_REVENGE_HISTORY>::GetLength();
    for (s32 liRivalIndex = 0; liRivalIndex < liNumberOfRivals; ++liRivalIndex)
        (void)lpcName;
    return -1;
}

// ============================================================================
// LiveRevengeManager::AddNewTableEntry
// ============================================================================
s32 LiveRevengeManager::AddNewTableEntry(const void* /*lpUniqueID*/)
{
    if (!mpLiveRevengeProfile)
        return -1;
    s32 liLen = mpLiveRevengeProfile->maRelationshipTable.Array<
        LiveRevengeRelationship, LiveRevengeProfile::KI_MAX_REVENGE_HISTORY>::GetLength();
    if (liLen >= LiveRevengeProfile::KI_MAX_REVENGE_HISTORY)
        return -1;
    return liLen;
}

// ============================================================================
// LiveRevengeManager::AddMappingEntry
// ============================================================================
void LiveRevengeManager::AddMappingEntry(NetworkPlayerID lNetworkPlayerID, s32 liTableIndex)
{
    for (s32 i = 0; i < KI_MAX_PLAYERS; ++i)
    {
        if (maPlayerToTableIndexData[i].mPlayerID == KI_INVALID_RACE_CAR_INDEX_TO_REVENGE_TABLE)
        {
            maPlayerToTableIndexData[i].mPlayerID = lNetworkPlayerID;
            maPlayerToTableIndexData[i].miRevengeTableIndex = liTableIndex;
            return;
        }
    }
}

// ============================================================================
// LiveRevengeManager::GetRivalTopIndex  (DWARF :1608)
// ============================================================================
s32 LiveRevengeManager::GetRivalTopIndex(s32 liTableIndex) const
{
    for (s32 liRivalIndex = 0; liRivalIndex < KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER; ++liRivalIndex)
    {
        if (maTopIndexes[liRivalIndex] == liTableIndex)
            return liRivalIndex;
    }
    return -1;
}

// ============================================================================
// LiveRevengeManager::ClearTopRivals  (DWARF :1643)
// ============================================================================
void LiveRevengeManager::ClearTopRivals()
{
    for (s32 liRivalIndex = 0; liRivalIndex < KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER; ++liRivalIndex)
        maTopIndexes[liRivalIndex] = -1;

    maDirtyTopRivals.CgsContainers::FastBitArray<KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER>::UnSetAll();
    miNumberOfTopRivals = 0;
}

// ============================================================================
// LiveRevengeManager::UpdateTopRivals
// ============================================================================
void LiveRevengeManager::UpdateTopRivals()
{
    ClearTopRivals();
}

// ============================================================================
// LiveRevengeManager::_SortTopRivals  (DWARF :1582, static comparison)
// ============================================================================
s32 LiveRevengeManager::_SortTopRivals(const void* lpRival1, const void* lpRival2)
{
    const LiveRevengeQSortData* lpRivalDetails1 = static_cast<const LiveRevengeQSortData*>(lpRival1);
    const LiveRevengeQSortData* lpRivalDetails2 = static_cast<const LiveRevengeQSortData*>(lpRival2);
    return lpRivalDetails2->miRivalScore - lpRivalDetails1->miRivalScore;
}

// ============================================================================
// LiveRevengeManager::GetUniqueIDByName  (DWARF :1434)
// ============================================================================
void LiveRevengeManager::GetUniqueIDByName(const char* /*lpcPlayerName*/, void* /*lpUniqueID*/) const
{
}

// ============================================================================
// LiveRevengeManager::GetUniqueIDByPlayerID  (DWARF :1453)
// ============================================================================
void LiveRevengeManager::GetUniqueIDByPlayerID(NetworkPlayerID /*lNetworkPlayerID*/, void* /*lpUniqueID*/) const
{
}

// ============================================================================
// LiveRevengeManager::RemoveMappingEntry  @0x825477F0
// ============================================================================
void LiveRevengeManager::RemoveMappingEntry(NetworkPlayerID lNetworkPlayerID)
{
    CGS_ASSERT(lNetworkPlayerID != -1, "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

    for (s32 i = 0; i < KI_MAX_PLAYERS; ++i)
    {
        if (maPlayerToTableIndexData[i].mPlayerID == lNetworkPlayerID)
        {
            maPlayerToTableIndexData[i].mPlayerID        = KI_INVALID_RACE_CAR_INDEX_TO_REVENGE_TABLE;
            maPlayerToTableIndexData[i].miRevengeTableIndex = -1;
            maPlayerToTableIndexData[i].mSendMessage.BrnNetwork::LiveRevengeSyncMessage::Release();
            maPlayerToTableIndexData[i].mSendMessage.BrnNetwork::LiveRevengeSyncMessage::Destruct();
            maPlayerToTableIndexData[i].mRecvMessage.BrnNetwork::LiveRevengeSyncMessage::Release();
            maPlayerToTableIndexData[i].mRecvMessage.BrnNetwork::LiveRevengeSyncMessage::Destruct();
            return;
        }
    }
}

// ============================================================================
// LiveRevengeManager::AddPlayer  @0x8256DA90
// ============================================================================
void LiveRevengeManager::AddPlayer(NetworkPlayerID lNetworkPlayerID)
{
    CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
    CGS_ASSERT(IsTableValid(), "IsTableValid()");

    CgsNetwork::NetworkPlayer* lpPlayer =
        mpNetworkManager->BrnNetwork::BrnNetworkManager::GetPlayerManager()
            ->CgsNetwork::PlayerManager::GetPlayerByID(lNetworkPlayerID);

    if (lpPlayer)
    {
        CGS_ASSERT(
            BrnNetwork::LiveRevengeManager::NetworkPlayerIDToActiveRaceCarIndex(lNetworkPlayerID) < 8,
            "NetworkPlayerIDToActiveRaceCarIndex(lNetworkPlayerID) < KI_MAX_PLAYERS");

        s32 liPlayerIndexInTable =
            BrnNetwork::LiveRevengeManager::FindPlayerInTableByName(nullptr);

        if (liPlayerIndexInTable == -1)
        {
            void* lpUniqueID = nullptr;
            BrnNetwork::LiveRevengeManager::GetUniqueIDByName(nullptr, lpUniqueID);
            liPlayerIndexInTable = BrnNetwork::LiveRevengeManager::AddNewTableEntry(lpUniqueID);
            CGS_ASSERT(liPlayerIndexInTable != -1, "liPlayerIndexInTable != -1");
        }

        BrnNetwork::LiveRevengeManager::AddMappingEntry(lNetworkPlayerID, liPlayerIndexInTable);

        LiveRevengeRelationship* lpRel =
            BrnNetwork::LiveRevengeRelations(mpLiveRevengeProfile, liPlayerIndexInTable);

        mDebugComponent.BrnNetwork::LiveRevengeDebugComponent::AddPlayer(lpRel);

        lpPlayer->CgsNetwork::NetworkPlayer::RegisterMessageType(
            24, 0, nullptr, nullptr,
            nullptr, nullptr, nullptr);
    }

    CGS_ASSERT(IsTableValid(), "IsTableValid()");
}

// ============================================================================
// LiveRevengeManager::RemovePlayer  @0x8255FB98
// ============================================================================
void LiveRevengeManager::RemovePlayer(NetworkPlayerID lNetworkPlayerID)
{
    CgsNetwork::NetworkPlayer* lpPlayer =
        mpNetworkManager->BrnNetwork::BrnNetworkManager::GetPlayerManager()
            ->CgsNetwork::PlayerManager::GetPlayerByID(lNetworkPlayerID);

    if (lpPlayer)
    {
        LiveRevengeMappingEntry* lpEntry = FindMappingEntry(lNetworkPlayerID);
        s32 liTableIndex = lpEntry ? lpEntry->miRevengeTableIndex : -1;

        LiveRevengeRelationship* lpRel =
            BrnNetwork::LiveRevengeRelations(mpLiveRevengeProfile, liTableIndex);

        mDebugComponent.BrnNetwork::LiveRevengeDebugComponent::RemovePlayer(lpRel);

        BrnNetwork::LiveRevengeManager::RemoveMappingEntry(lNetworkPlayerID);

        lpPlayer->CgsNetwork::NetworkPlayer::UnRegisterMessageType(24);
    }
}

// ============================================================================
// LiveRevengeManager::RemotePlayerFinalised  @0x82560260
// ============================================================================
void LiveRevengeManager::RemotePlayerFinalised(NetworkPlayerID lNetworkPlayerID)
{
    CgsNetwork::NetworkPlayer* lpPlayer =
        mpNetworkManager->BrnNetwork::BrnNetworkManager::GetPlayerManager()
            ->CgsNetwork::PlayerManager::GetPlayerByID(lNetworkPlayerID);

    CGS_ASSERT(lpPlayer != nullptr, "lpNetworkPlayer");

    CgsNetwork::Message* lpSendMsg =
        lpPlayer->CgsNetwork::NetworkPlayer::GetRegisteredSendMessage(24);

    CGS_ASSERT(lpSendMsg != nullptr, "lpLiveRevengeSyncMessage");

    LiveRevengeRelationship* lpRel =
        BrnNetwork::LiveRevengeManager::GetNonConstRevengeRelation(lNetworkPlayerID);

    CGS_ASSERT(lpRel != nullptr, "lpRevengeRelationship");

    static_cast<LiveRevengeSyncMessage*>(lpSendMsg)
        ->BrnNetwork::LiveRevengeSyncMessage::PrepareForSend(lpRel, 0);
}

// ============================================================================
// LiveRevengeManager::UpdateLiveRevengeRelationShip
// ============================================================================
void LiveRevengeManager::UpdateLiveRevengeRelationShip(
    s32 /*liAggressor*/, s32 /*liVictim*/, u8 /*luFlags*/, void* /*lpOutputBuffer*/)
{
    LiveRevengeRelationship* lpRel =
        BrnNetwork::LiveRevengeManager::GetNonConstRevengeRelation(0);
    if (lpRel)
        lpRel->BrnNetwork::LiveRevengeRelationship::GetTotalTakedowns();
}

// ============================================================================
// LiveRevengeManager::UpdatePaybacksData
// ============================================================================
void LiveRevengeManager::UpdatePaybacksData(s32 /*a*/, s32 /*b*/, s32 /*c*/)
{
}

// ============================================================================
// LiveRevengeManager::DisplayPlayerTakedownMessage  @0x8255FE70
// ============================================================================
void LiveRevengeManager::DisplayPlayerTakedownMessage(
    void* /*lpOutputBuffer*/, s32 /*liAggressorIndex*/, s32 liVictimIndex, s32 /*liUnk*/)
{
    CGS_ASSERT(liVictimIndex >= 0, "leVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(liVictimIndex < 8,  "leVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    NetworkPlayerID lNetID =
        mpNetworkModule->BrnNetwork::BrnNetworkModule::GetGameStateToNetworkInterface()
            ->BrnNetwork::BrnNetworkModuleIO::GameStateToNetworkInterface::GetNetworkPlayerID(
                static_cast<EActiveRaceCarIndex>(liVictimIndex));

    LiveRevengeRelationship* lpRel =
        BrnNetwork::LiveRevengeManager::GetNonConstRevengeRelation(lNetID);

    if (lpRel)
    {
        s32 liRivals = BrnNetwork::LiveRevengeManager::GetNumberOfRivals();
        (void)liRivals;
        lpRel->BrnNetwork::LiveRevengeRelationship::GetTotalTakedowns();
    }
}

// ============================================================================
// LiveRevengeManager::DisplayRivalTakedownMessage  @0x82560020
// ============================================================================
void LiveRevengeManager::DisplayRivalTakedownMessage(
    void* lpOutputBuffer, s32 liAggressorIndex, s32 /*liVictimIndex*/, s32 /*liUnk*/)
{
    CGS_ASSERT(lpOutputBuffer != nullptr, "lpOutputBuffer");
    CGS_ASSERT(liAggressorIndex >= 0, "leAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(liAggressorIndex < 8,  "leAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    NetworkPlayerID lNetID =
        mpNetworkModule->BrnNetwork::BrnNetworkModule::GetGameStateToNetworkInterface()
            ->BrnNetwork::BrnNetworkModuleIO::GameStateToNetworkInterface::GetNetworkPlayerID(
                static_cast<EActiveRaceCarIndex>(liAggressorIndex));

    LiveRevengeRelationship* lpRel =
        BrnNetwork::LiveRevengeManager::GetNonConstRevengeRelation(lNetID);

    if (lpRel)
    {
        s32 liRivals = BrnNetwork::LiveRevengeManager::GetNumberOfRivals();
        (void)liRivals;
        lpRel->BrnNetwork::LiveRevengeRelationship::GetTotalTakedowns();
    }
}

// ============================================================================
// LiveRevengeManager::HandleLiveRevengeProfileLoadedEvent  @0x82560A30
// ============================================================================
void LiveRevengeManager::HandleLiveRevengeProfileLoadedEvent(
    const LiveRevengeProfile* lpProfile, s32 liMaxEntries)
{
    CGS_ASSERT(lpProfile != nullptr, "lpLiveRevengeProfile");

    if (mpLiveRevengeProfile)
        (void)mpLiveRevengeProfile->BrnNetwork::LiveRevengeProfile::ValidateProfile(lpProfile, liMaxEntries);

    CGS_ASSERT(BrnNetwork::LiveRevengeManager::IsTableValid(), "IsTableValid()");

    for (s32 i = 0; i < KI_MAX_PLAYERS; ++i)
        maPlayerToTableIndexData[i].mPlayerID = KI_INVALID_RACE_CAR_INDEX_TO_REVENGE_TABLE;

    for (s32 j = 0; j < KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER; ++j)
        maTopIndexes[j] = -1;

    maDirtyTopRivals.CgsContainers::FastBitArray<KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER>::Construct();

    BrnNetwork::LiveRevengeManager::UpdateTopRivals();
    mbProfileIsDirty = false;
}

// ============================================================================
// LiveRevengeManager::ProcessTakedownQueue  @0x82568C88
// ============================================================================
void LiveRevengeManager::ProcessTakedownQueue(void* lpOutputBuffer)
{
    CGS_ASSERT(lpOutputBuffer != nullptr, "lpOutputBuffer");
    CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
    CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule");

    BrnNetworkModuleIO::GameStateToNetworkInterface* lpIface =
        mpNetworkModule->BrnNetwork::BrnNetworkModule::GetGameStateToNetworkInterface();
    CGS_ASSERT(lpIface != nullptr, "lpGameStateToNetworkInterface");

    s32 liNumberOfRivals = BrnNetwork::LiveRevengeManager::GetNumberOfRivals();

    s32 liLen = mTakedownEventQueue.CgsModule::EventQueue<BrnGameState::TakedownEvent, 8>::GetLength();
    if (liLen > 0)
    {
        for (s32 i = 0; i < liLen; ++i)
        {
            const BrnGameState::TakedownEvent& lEv =
                mTakedownEventQueue.CgsModule::EventQueue<BrnGameState::TakedownEvent, 8>::GetEvent(i);

            (void)lpIface->BrnNetwork::BrnNetworkModuleIO::GameStateToNetworkInterface::GetPlayerInFreeburnChallenge(
                static_cast<EActiveRaceCarIndex>(0));

            BrnNetwork::LiveRevengeManager::UpdateLiveRevengeRelationShip(0, 0, 0, lpOutputBuffer);
            (void)lEv;
        }

        mTakedownEventQueue.CgsModule::EventQueue<BrnGameState::TakedownEvent, 8>::Clear();

        s32 liNewRivals = BrnNetwork::LiveRevengeManager::GetNumberOfRivals();
        if (liNewRivals > liNumberOfRivals)
        {
            BrnNetworkModuleIO::NetworkEventQueue* lpNEQ =
                mpNetworkModule->BrnNetwork::BrnNetworkModule::GetNetworkEventQueue();
            (void)lpNEQ;
        }
    }
}

// ============================================================================
// LiveRevengeManager::ProcessGameDirtyTrickInterface  @0x82562F80
// ============================================================================
void LiveRevengeManager::ProcessGameDirtyTrickInterface()
{
    BrnNetworkModuleIO::GameStateToNetworkInterface* lpIface =
        mpNetworkModule->BrnNetwork::BrnNetworkModule::GetGameStateToNetworkInterface();
    CGS_ASSERT(lpIface != nullptr, "lpDirtyTrickEventQueue");

    BrnNetwork::LiveRevengeManager::UpdatePaybacksData(0, 0, 0);
}

// ============================================================================
// LiveRevengeManager::ProcessBeforeSimulation  @0x8256D868
// ============================================================================
void LiveRevengeManager::ProcessBeforeSimulation(BrnNetworkModule* /*lpOutputBuffer*/)
{
    BrnNetwork::LiveRevengeManager::ProcessTakedownQueue(nullptr);

    if (meLiveRevengeUploadStatus == E_LIVE_REVENGE_UPLOAD_STATUS_PENDING)
        BrnNetwork::LiveRevengeManager::SendLiveRevengeRivalsToServer();

    if (meLiveRevengeUploadStatus == E_LIVE_REVENGE_UPLOAD_STATUS_IN_PROGRESS)
    {
        CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");

        mpNetworkManager->BrnNetwork::BrnNetworkManager::OnAutoLoginProcessComplete(1);
        meLiveRevengeUploadStatus = E_LIVE_REVENGE_UPLOAD_STATUS_IDLE;
    }

    if (mbNeedToUpdateMarksForCurrentRound)
    {
        s32 liTotal = mpNetworkManager->BrnNetwork::BrnNetworkManager::GetPlayerManager()
            ->CgsNetwork::PlayerManager::GetTotalNumberPlayers(
                CgsNetwork::PlayerManager::E_CONSIDER_ALL_PLAYERS);
        (void)liTotal;
        BrnNetwork::LiveRevengeManager::UpdateMarkedManInfo();
    }

    if (mbProfileIsDirty)
    {
        CGS_ASSERT(BrnNetwork::LiveRevengeManager::IsTableValid(), "IsTableValid()");

        BrnNetworkModuleIO::NetworkEventQueue* lpNEQ =
            mpNetworkModule->BrnNetwork::BrnNetworkModule::GetNetworkEventQueue();
        (void)lpNEQ;
        mbProfileIsDirty = false;
    }
}

// ============================================================================
// LiveRevengeManager::SendLiveRevengeRivalsToServer  @0x82560358
// (local variable allocation has failed -- garbled pseudo)
// ============================================================================
void LiveRevengeManager::SendLiveRevengeRivalsToServer()
{
    if (!mpLiveRevengeProfile)
        return;

    CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
    CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule");

    for (s32 i = 0; i < KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER; ++i)
    {
        if (maDirtyTopRivals.CgsContainers::FastBitArray<KI_NUMBER_OF_RIVALS_TO_STORE_ON_SERVER>::IsBitSet(i))
            (void)maTopIndexes[i];
    }

    meLiveRevengeUploadStatus = E_LIVE_REVENGE_UPLOAD_STATUS_IN_PROGRESS;
}

// ============================================================================
// Static callbacks (private, registered with RegisterMessageType)
// ============================================================================
void LiveRevengeManager::_SyncMessageArrivedCallback()   {}
void LiveRevengeManager::_SyncMessageDeliveredCallback() {}

} // namespace BrnNetwork
