// BrnFriendsList.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The two BrnGui::FriendsListComponent
// methods the GUI flow reaches at attach/refresh time:
//   SetGuiCachePointer @0x82473580 -- latch the GuiCache pointer + cache its far field
//   SetDirty           @0x8241F120 -- snapshot the current selection + scroll indicator
// SetGuiCachePointer runs one non-fatal CGS_ASSERT pointer guard (the X360 proceeds
// regardless). The X360-baked assert file/line are discarded per project convention;
// the stringized condition matches the X360 assert text.

#include "GameSource/Gui/Flow/HUD/Components/BrnFriendsList.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "SharedClasses/DataLists/ChallengeList.h"
#include "SharedClasses/DataLists/ChallengeListEntry.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface full type
#include "GameSource/Gui/BrnGuiWorldDataController.h"                  // WorldDataController::GetFreeburnChallengeList
#include "GameShared/GameClasses/System/CgsHardwareInit.h"      // CgsSystem::HardwareInit::IsHardDiskAvailable
#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h" // IsRunning/IsShowingResults tier reads
#include "GameSource/Gui/Flow/HUD/Components/BrnFriendsListEntry.h" // LobbyNameCmp   // CGS_ASSERT

// ===================================================================================
// ⏳ TU STATUS: INCOMPLETE BY DESIGN (friends wave, tranche 1 of N). 8 of the 47
// phantom-reviewed bodies land here (Construct/Highlight/SaveCurrentState/
// TransitionCompleteCallback/RemoveUnneededFriends/BuddySortFunction/
// MoveHighlightDueToBranchOpen). The remaining bodies are decoded and queued in
// scratch/friends_decode_notes.md -- they need two more GuiCache carves first
// (+0x23B9A offline-shortcut gate byte; FreeburnChallengeManager tier @mgr+0x04).
// Do NOT mark this TU done until the last body lands.
// ===================================================================================

namespace BrnGui
{

// @ 0x82473580
//   if (lpGuiCache == 0) <assert "lpGuiCache">
//   stw lpGuiCache, 0x4100(this)             ; mpGuiCache = lpGuiCache
//   lwzx r, lpGuiCache, 0xAC74 ; stw r,0x870 ; muCachedCacheField = cache far field
//   return this
FriendsListComponent* FriendsListComponent::SetGuiCachePointer(GuiCache* lpGuiCache)
{
    CGS_ASSERT( lpGuiCache != 0, "lpGuiCache" );

    mpGuiCache = lpGuiCache;
    muCachedCacheField = lpGuiCache->GetFriendsListCachedField();
    return this;
}

// @ 0x8241F120
//   snapshot current selection into the dirty region:
//     mSnapshotA(+0x4108) = mePanelState(+0x878)
//     mSnapshotB(+0x410C) = meBranchState(+0x87C)
//     mSnapshotC(+0x4110) = meListType(+0x874)
//     mbSnapshotFlagA(+0x4119) = mi8FirstVisibleIndex(+0x880)
//     mbSnapshotFlagB(+0x411A) = mi8SelectedRowIndex(+0x881)
//   raise dirty flag: mbDirty(+0x4118) = 1
//   scroll indicator (+0x4114), liIndex = (s8)mi8SelectedIndex(+0x882):
//     base = (liIndex > 0) ? 2 : 0
//     if (liIndex >= muNumEntries - 1)  muScrollIndicator = base
//     else                              muScrollIndicator = base + 1
FriendsListComponent* FriendsListComponent::SetDirty()
{
    const s32 liSelectedIndex = mi8SelectedIndex; // extsb -- sign-extended

    mbDirty     = 1;
    muSnapshotA = mePanelState;
    muSnapshotB = meBranchState;
    muSnapshotC = meListType;
    mbSnapshotFlagA = mi8FirstVisibleIndex;
    mbSnapshotFlagB = mi8SelectedRowIndex;

    s32 liBase = 0;
    if ( liSelectedIndex > 0 )
        liBase = 2;

    if ( liSelectedIndex >= static_cast<s32>( muNumEntries ) - 1 )
        muScrollIndicator = static_cast<u32>( liBase );
    else
        muScrollIndicator = static_cast<u32>( liBase + 1 );

    return this;
}

// @ 0x82410958 -- post-increment on the friends-list branch enum: advance the referenced enum
// by one (writing it back), assert it stays <= E_FRIENDLISTBRANCH_THIRD_OF_THREE, and return the
// PRE-increment value. Free operator in namespace BrnGui (DWARF BrnFriendsList.h:767).
FriendsListComponent::EFriendListBranchState
operator++(FriendsListComponent::EFriendListBranchState& lreEnumIndex, int)
{
    const FriendsListComponent::EFriendListBranchState leOld = lreEnumIndex;
    lreEnumIndex = static_cast<FriendsListComponent::EFriendListBranchState>(lreEnumIndex + 1);
    CGS_ASSERT(lreEnumIndex <= FriendsListComponent::E_FRIENDLISTBRANCH_THIRD_OF_THREE,
               "leEnumIndex <= FriendsListComponent::E_FRIENDLISTBRANCH_THIRD_OF_THREE");
    return leOld;
}

// @ 0x824109B8 -- post-decrement on the friends-list branch enum: step the referenced enum back
// by one (writing it back), assert it stays >= E_FRIENDLISTBRANCH_FIRST_OF_ONE, and return the
// PRE-decrement value. Free operator in namespace BrnGui (DWARF BrnFriendsList.h:764).
FriendsListComponent::EFriendListBranchState
operator--(FriendsListComponent::EFriendListBranchState& lreEnumIndex, int)
{
    const FriendsListComponent::EFriendListBranchState leOld = lreEnumIndex;
    lreEnumIndex = static_cast<FriendsListComponent::EFriendListBranchState>(lreEnumIndex - 1);
    CGS_ASSERT(lreEnumIndex >= FriendsListComponent::E_FRIENDLISTBRANCH_FIRST_OF_ONE,
               "leEnumIndex >= FriendsListComponent::E_FRIENDLISTBRANCH_FIRST_OF_ONE");
    return leOld;
}


namespace
{
    // Per-list static save slots (X360 .bss) backing SaveCurrentState/AttemptStateRestore.
    u8     s_abStateSavedFlag = 0;                            // byte_82FB27E0
    s32    s_aiSavedListType  = 0;                            // dword_82FB27E4
    CgsID  s_u64SavedChallengeUid = 0;                         // qword_82FB27E8
    s32    s_aiSavedShortcutOption = 0;                       // dword_82FB27F0
    CgsNetwork::PlayerName s_amabSavedNames[KI_MAX_FRIEND_RECORDS];   // unk_82FB00C0 (stride 0x84)
    bool   s_sbHddOverlayShown = false;                       // byte_82FB27E1
}

// @0x82422B20 --------------------------------------------------------------------
void FriendsListComponent::Construct(const char* /*lacName*/, CgsGui::StateInterface* lpStateInterface,
                                     const char* /*lpacParentName*/)
{
    CGS_ASSERT(lpStateInterface != 0, "lpStateInterface");       // cpp:0x71 (+ base h:113)
    // base Construct is header-inline on console (h:113): adopt channel + drop clip.
    mpStateInterface = lpStateInterface;
    mAptRef.SetInvalid();

    mauNumBranches[0] = mauNumBranches[1] = mauNumBranches[2] = 0;
    for (s32 i = 0; i < KI_VISIBLE_ROWS; ++i)
        maEntries[i].Construct(0, lpStateInterface, 0, Selectable::K_INVALID_ID); // @0x82422BB8
    memset(maRecords, 0, 0x3390);                                // @0x82422BD0

    for (s32 i = 0; i < KI_MAX_FRIEND_RECORDS; ++i) maeDisplayTypes[i] = 0;
    for (s32 i = 0; i < E_SHORTCUTOPTION_COUNT; ++i)
        maeAvailableShortcutOptions[i] = E_SHORTCUTOPTION_COUNT;

    mauNumBranches[0] = mauNumBranches[1] = mauNumBranches[2] = 15;
    meListType = 1;   mePanelState = 0;   meBranchState = 0;
    mi8FirstVisibleIndex = 0;   mi8SelectedRowIndex = 0;   mi8SelectedIndex = 0;
    mi8CurrentlyHighlightedBranch = 0;
    muNumEntries = 0;
    mabEntryFlags[0] = mabEntryFlags[1] = 0;
    meDataState = 2;
    muCachedCacheField = 0;
    mbDirty = 0;
    mpGuiCache = 0;
    mbReopenAfterClose = 0;
}

// @0x82414960 --------------------------------------------------------------------
void FriendsListComponent::Highlight(s32 liRow)
{
    for (s32 i = 0; i < KI_VISIBLE_ROWS; ++i)
    {
        FriendsListEntry& lrEntry = maEntries[i];
        if (i == liRow)
        {
            if (lrEntry.IsHighlightable() && !lrEntry.IsHighlighted())
                lrEntry.SetHighlighted(true);
        }
        else if (lrEntry.IsHighlighted())
            lrEntry.SetHighlighted(false);
    }
}

// @0x824149E8 --------------------------------------------------------------------
void FriendsListComponent::SaveCurrentState()
{
    if (mePanelState == 0)
    {
        s_abStateSavedFlag = 0;
        return;
    }
    s_abStateSavedFlag = 1;
    s_aiSavedListType = meListType;
    switch (meListType)
    {
        case 1:
            s_amabSavedNames[mi8SelectedIndex].Construct(
                maRecords[mi8SelectedIndex].macName);
            break;
        case 2:
            s_aiSavedShortcutOption = maeAvailableShortcutOptions[mi8SelectedIndex];
            break;
        case 3:
            s_u64SavedChallengeUid = mau64ChallengeIds[mi8SelectedIndex];
            break;
        default:
            break;
    }
}

// @0x82414AA0 --------------------------------------------------------------------
void FriendsListComponent::TransitionCompleteCallback(void* lpUserData)
{
    CGS_ASSERT(lpUserData != 0, "lpUserData");
    FriendsListComponent& lrSelf = *static_cast<FriendsListComponent*>(lpUserData);
    if (lrSelf.mePanelState == 1)
        lrSelf.mePanelState = 2;
    else if (lrSelf.mePanelState == 5)
    {
        lrSelf.mi8SelectedRowIndex = 0;
        lrSelf.mi8SelectedIndex = 0;
        lrSelf.meBranchState = 0;
        lrSelf.mbReopenAfterClose = 1;
        lrSelf.mePanelState = 0;
    }
}

// @0x824146C0 --------------------------------------------------------------------
void FriendsListComponent::RemoveUnneededFriends()
{
    while (muNumEntries > 0)
    {
        if (!(mabRecordTailFlags[muNumEntries][0] != 0 &&
              mabRecordTailFlags[muNumEntries][1] != 0))
            break;
        maeDisplayTypes[muNumEntries - 1] = 0;
        memset(reinterpret_cast<u8*>(&maRecords[muNumEntries]) - 0x84 + 0x60, 0, 0x84);
        --muNumEntries;
    }
}

// @0x824147A8 --------------------------------------------------------------------
int FriendsListComponent::BuddySortFunction(const void* lpA, const void* lpB)
{
    const SFriendRecord* lpRA = static_cast<const SFriendRecord*>(lpA);
    const SFriendRecord* lpRB = static_cast<const SFriendRecord*>(lpB);

    if (lpRA->mubClassA != 0)
    {
        if (lpRB->mubClassA == 0)
            return -1;
    }
    else
        return (lpRB->mubClassA != 0) ? 1 : 0;

    const s32 laType = static_cast<s32>(lpRA->muType);
    const s32 lbType = static_cast<s32>(lpRB->muType);
    if (laType == 1)
        return (lbType == 1) ? LobbyNameCmp(lpRA->macName, lpRB->macName) : -1;
    if (lbType == 1)
        return 1;
    if (laType == 2)
        return (lbType == 2) ? LobbyNameCmp(lpRA->macName, lpRB->macName) : -1;
    if (lbType == 2)
        return 1;
    if (lpRA->mubClassB == 1)
        return (lpRB->mubClassB == 1) ? LobbyNameCmp(lpRA->macName, lpRB->macName) : -1;
    return (lpRB->mubClassB == 1) ? 1 : 0;
}

// @0x82414868 --------------------------------------------------------------------
bool FriendsListComponent::MoveHighlightDueToBranchOpen()
{
    u32 luIdx = 0;
    while (luIdx < muNumEntries &&
           LobbyNameCmp(mHighlightedName.macName, maRecords[luIdx].macName) != 0)
        ++luIdx;

    if (luIdx != muNumEntries)
    {
        mi8SelectedIndex = static_cast<s8>(luIdx);
        if (muNumEntries <= KI_VISIBLE_ROWS)
        {
            mi8SelectedRowIndex = static_cast<s8>(luIdx);
            return true;
        }
        if (static_cast<s32>(luIdx) <= mi8SelectedRowIndex)
        {
            mi8SelectedRowIndex = static_cast<s8>(luIdx);
            return true;
        }
        const s32 liShift = static_cast<s32>(luIdx) - mi8SelectedRowIndex;
        if (static_cast<s32>(muNumEntries) - liShift > KI_VISIBLE_ROWS)
        {
            mi8SelectedRowIndex = static_cast<s8>(mi8SelectedRowIndex + liShift);
            return true;
        }
        return false;
    }

    meBranchState = 0;
    mabRecordTailFlags[0][0] = 0;
    if (mi8SelectedIndex >= static_cast<s32>(muNumEntries))
        mi8SelectedIndex = static_cast<s8>(muNumEntries - 1);
    if (muNumEntries <= KI_VISIBLE_ROWS)
        mi8SelectedRowIndex = mi8SelectedIndex;
    return false;
}


// ===================================================================================
// [friends wave -- BODY TRANCH 2] data producers + not-connected/no-friends arms.
// ===================================================================================

// @0x82414288 BuildShortcutOptions ---------------------------------------------------
void FriendsListComponent::BuildShortcutOptions()
{
    muNumEntries = 0;
    maeAvailableShortcutOptions[0] = E_SHORTCUTOPTION_FRIENDS;      // always first @0x824142A0
    ++muNumEntries;

    if (!mpGuiCache->IsOnlineStartInProgress())                     // offline arm @0x82414588
    {
        maeAvailableShortcutOptions[muNumEntries++] = 2;            // @0x82414594
        if (mpGuiCache->GetOfflineShortcutProgressGate())           // far byte +0x13B9A @0x824145B0
            maeAvailableShortcutOptions[muNumEntries++] = 1;        // @0x824145C0
        maeAvailableShortcutOptions[muNumEntries++] = 3;            // @0x824145DC block
        maeAvailableShortcutOptions[muNumEntries++] = 4;
        maeAvailableShortcutOptions[muNumEntries++] = 5;
        maeAvailableShortcutOptions[muNumEntries++] = 6;
        maeAvailableShortcutOptions[muNumEntries++] = 7;
        maeAvailableShortcutOptions[muNumEntries++] = 8;
    }
    else                                                            // online arm @0x824142C0
    {
        const FreeburnChallengeManager* lpMgr = mpGuiCache->GetFreeburnChallengeManager();
        const bool lbRunning  = lpMgr->IsRunning();                 // tier==3 @0x8241435C
        const bool lbResults  = lpMgr->IsShowingResults();          // tier==4 @0x8241437C

        if (!mpGuiCache->mbIsOnlineHost)                            // +0xB864 == 0 @0x8241444C
        {
            if (!(lbRunning || lbResults))
                maeAvailableShortcutOptions[muNumEntries++] = 1;    // @0x82414480
            if (!(mpGuiCache->meOnlineGameMode == 15 ||
                  mpGuiCache->meOnlineGameMode == 16))
                maeAvailableShortcutOptions[muNumEntries++] = 16;   // @0x824144CC..D0
        }
        else                                                        // host arm @0x82414304
        {
            const bool lbInLobby = (mpGuiCache->meOnlineGameMode == 15 ||
                                    mpGuiCache->meOnlineGameMode == 16);
            if (lbInLobby)                                          // @0x82414304..54
            {
                maeAvailableShortcutOptions[muNumEntries++] = 9;
                maeAvailableShortcutOptions[muNumEntries++] = 16;
                maeAvailableShortcutOptions[muNumEntries++] = 17;
            }
            if (lbRunning)
                maeAvailableShortcutOptions[muNumEntries++] = 14;   // @0x8241436C
            else if (!lbResults)
            {
                if (static_cast<s32>(mpGuiCache->muNumActivePlayers) > 1)   // +0xAC74
                    maeAvailableShortcutOptions[muNumEntries++] = 15;
                maeAvailableShortcutOptions[muNumEntries++] = 1;    // @0x824143D0
            }
            if (lbInLobby)
                maeAvailableShortcutOptions[muNumEntries++] = 10;   // @0x82414414
        }
        if (!mpGuiCache->mbOnlineRanked)                            // +0xA9DF @0x82414438
            maeAvailableShortcutOptions[muNumEntries++] = 19;       // @0x824144D0
        if (mpGuiCache->meOnlineGameMode == 11)                     // @0x824144EC..F8
            maeAvailableShortcutOptions[muNumEntries++] = 11;
        maeAvailableShortcutOptions[muNumEntries++] = 12;           // always @0x8241451C..
        maeAvailableShortcutOptions[muNumEntries++] = 18;           // ..58 block
        maeAvailableShortcutOptions[muNumEntries++] = 20;
        maeAvailableShortcutOptions[muNumEntries++] = 13;
    }

    ++muNumEntries;                                                 // @0x82414674
    for (s32 i = muNumEntries; i < E_SHORTCUTOPTION_COUNT; ++i)     // sentinel pad @0x82414684
        maeAvailableShortcutOptions[i] = E_SHORTCUTOPTION_COUNT;
}

// @0x824234B8 WithdrawBranches ---------------------------------------------------------
void FriendsListComponent::WithdrawBranches()
{
    s32 leTarget;
    switch (meBranchState)                                          // switch(me-7) @0x824234DC
    {
        case E_FRIENDLISTBRANCH_FIRST_OF_ONE:    leTarget = E_FRIENDLISTBRANCH_ONE_OUT;   break;
        case E_FRIENDLISTBRANCH_FIRST_OF_TWO:
        case E_FRIENDLISTBRANCH_SECOND_OF_TWO:   leTarget = E_FRIENDLISTBRANCH_TWO_OUT;   break;
        case E_FRIENDLISTBRANCH_FIRST_OF_THREE:
        case E_FRIENDLISTBRANCH_SECOND_OF_THREE:
        case E_FRIENDLISTBRANCH_THIRD_OF_THREE:  leTarget = E_FRIENDLISTBRANCH_THREE_OUT; break;
        default: return;
    }
    meBranchState = leTarget;                                       // @0x8242352C
    SetDirty();
    meBranchState = E_FRIENDLISTBRANCH_INVISIBLE;                   // @0x82423534
    mi8CurrentlyHighlightedBranch = 0;                              // @0x82423538
}

// @0x82422CB0 SetEntryData ---------------------------------------------------------------
void FriendsListComponent::SetEntryData(s32 liRow, const char* lpcText, s32 leStatus,
                                        bool lbLocalise)
{
    CGS_ASSERT(liRow >= 0 && liRow < KI_VISIBLE_ROWS, "Invalid index: \n");   // cpp:0x339
    CGS_ASSERT(lpcText != 0, "lpcPlayerName != NULL");                        // cpp:0x33A

    FriendsListEntry& lrEntry = maEntries[liRow];                   // +0x8A0 + row*0x98
    lrEntry.Invalidate();                                           // entry vtable+4 @0x82422DB8
    if (lbLocalise)
        lrEntry.GetNameField().SetLocalisedText(lpcText, 9);  // ID_LOOKUP @0x82422DE0
    else
        lrEntry.GetNameField().SetText(lpcText, false);       // @0x82422DFC
    lrEntry.SetEntryStatus(static_cast<FriendsListEntry::EFriendListEntryState>(leStatus));   // @0x82422E08
}







// ===================================================================================
// [friends wave -- BODY TRANCH 3] data producers, not-connected arms, specific-show.
// ===================================================================================

// @0x82439248 RequestRefreshedData -----------------------------------------------------
void FriendsListComponent::RequestRefreshedData()
{
    if (mePanelState == 0 || meListType != 1)                    // @0x82439258..6C
        return;

    meDataState = 0;                                             // @0x82439288

    struct RefreshRequest                                        // wrapper {1,97,12} + pad byte
    {
        s32 miOutEventSize;
        s32 miOutEventType;
        s32 miOutEventOffset;
        s32 miPayload;
    } lRequest;
    lRequest.miOutEventSize   = 1;                               // payload width
    lRequest.miOutEventType   = 97;                              // 0x61
    lRequest.miOutEventOffset = 12;
    lRequest.miPayload        = 0;
    mpStateInterface->GetOutputEventQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lRequest), 40, 16);          // @0x824392A4
}

// @0x824392B8 SetTotalFriends -----------------------------------------------------------
void FriendsListComponent::SetTotalFriends(s32 liCount)
{
    if (liCount >= static_cast<s32>(muNumEntries))
    {
        memset(&maRecords[liCount], 0,
               (muNumEntries - liCount) * sizeof(SFriendRecord));                // @0x824392F8
    }
    muNumEntries = static_cast<u32>(liCount);                    // @0x82439308
    meDataState = 1;                                             // @0x8243931C

    struct TallyRequest                                          // wrapper {1,98,12}
    {
        s32 miOutEventSize;
        s32 miOutEventType;
        s32 miOutEventOffset;
        s32 miPayload;
    } lRequest;
    lRequest.miOutEventSize   = 1;
    lRequest.miOutEventType   = 98;                              // 0x62
    lRequest.miOutEventOffset = 12;
    lRequest.miPayload        = 0;
    mpStateInterface->GetOutputEventQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lRequest), 40, 16);          // @0x82439330
}

// @0x8242B830 BuildChallengeList ----------------------------------------------------------
void FriendsListComponent::BuildChallengeList()
{
    CGS_ASSERT(mpGuiCache->mpWorldDataController != 0, "mpWorldDataController");   // cpp:0x914
    const BrnResource::ChallengeList* lpList =
        mpGuiCache->mpWorldDataController->GetFreeburnChallengeList();

    const s32 leSlot = static_cast<s32>(mpGuiCache->muChallengeSlotMirror);           // +0xAC78 consumer-carved member
    muNumEntries = 0;                                            // @0x8242B890

    const s32 liCount = lpList->GetChallengeCount();             // +0x32E0 @0x8242B894
    for (s32 i = 0; i < liCount; ++i)                            // @0x8242B8B0
    {
        const BrnResource::ChallengeListEntry* lpEntry = lpList->GetChallengeData(i);
        if ((static_cast<u8>(lpEntry->GetNumPlayers()) & 0xF) != leSlot)             // +0xD3 low nibble @0x8242B8C0
            continue;
        if (!lpList->IsChallengeContentBought(i))                // @0x8242B8D8
            continue;
        CGS_ASSERT(muNumEntries < 256,
                   "KI_MAX_CHALLENGES_FOR_EACH_PLAYER_COUNT");   // cpp:0xBE5
        mau64ChallengeIds[muNumEntries++] = lpEntry->GetChallengeID();   // +0xC0 qword @0x8242B910
    }
}

// @0x8242B948 HandleNotConnected -------------------------------------------------------------
void FriendsListComponent::HandleNotConnected()
{
    mi8SelectedRowIndex = 0;                                     // @0x8242B970
    mi8FirstVisibleIndex = 1;                                    // @0x8242B978
    mi8SelectedIndex = 0;                                        // @0x8242B97C
    meBranchState = 0;                                           // @0x8242B980
    SetEntryData(0, "FRIENDSLIST_NOT_CONNECTED",
                 FriendsListEntry::E_FRIENDLISTENTRYSTATE_NOFRIENDS,
                 true);                                          // status 1 @0x8242B984
    Highlight(mi8SelectedRowIndex);

    for (s32 liRow = mi8FirstVisibleIndex; liRow < KI_VISIBLE_ROWS; ++liRow)
        maEntries[liRow].Invalidate();                           // @0x8242B9B4

    mbDirty = 1;                                                 // snapshot tail @0x8242B9CC
    muSnapshotA = mePanelState;
    muSnapshotB = static_cast<u32>(meBranchState);
    muSnapshotC = static_cast<u32>(meListType);
    mbSnapshotFlagA = static_cast<u8>(mi8FirstVisibleIndex);
    mbSnapshotFlagB = static_cast<u8>(mi8SelectedRowIndex);
    u32 luIndicator = 0;
    if (mi8SelectedIndex > 0)
        luIndicator = 2;
    if (mi8SelectedIndex <= static_cast<s32>(muNumEntries) - 1)
        ++luIndicator;
    muScrollIndicator = luIndicator;
}

// @0x8242BA40 HandleNoFriends -------------------------------------------------------------------
void FriendsListComponent::HandleNoFriends()
{
    const bool lbBlocked = mpGuiCache->IsOnlineStartInProgress() ||
                           !mpGuiCache->IsMultiplayerAllowed() ||
                           !CgsSystem::HardwareInit::IsHardDiskAvailable();

    if (!lbBlocked)
    {
        // multiplayer-capable arm: NO_FRIENDS row + INSTANT_FREEBURN row
        mi8SelectedRowIndex = 1;                                 // @0x8242BAB8..AC8
        mi8FirstVisibleIndex = 2;
        mi8SelectedIndex = 1;
        meBranchState = 0;
        SetEntryData(0, "FRIENDSLIST_NO_FRIENDS",
                     FriendsListEntry::E_FRIENDLISTENTRYSTATE_NOFRIENDS,
                     true);
        SetEntryData(1, "FRIENDSLIST_INSTANT_FREEBURN",
                     FriendsListEntry::E_FRIENDLISTENTRYSTATE_FRIENDJOINABLE,
                     true);                                      // status 6 @0x82442BAFC
        Highlight(mi8SelectedRowIndex);
    }
    else
    {
        mi8SelectedRowIndex = 0;                                 // @0x8242BB28
        mi8FirstVisibleIndex = 1;
        mi8SelectedIndex = 0;
        meBranchState = 0;
        SetEntryData(0, "FRIENDSLIST_NO_FRIENDS",
                     FriendsListEntry::E_FRIENDLISTENTRYSTATE_NOFRIENDS,
                     true);                                      // @0x8242BB40
        Highlight(mi8SelectedRowIndex);
    }

    for (s32 liRow = mi8FirstVisibleIndex; liRow < KI_VISIBLE_ROWS; ++liRow)
        maEntries[liRow].Invalidate();                           // @0x8242BB64

    mbDirty = 1;                                                 // @0x8242BB9C
    muSnapshotA = mePanelState;
    muSnapshotB = static_cast<u32>(meBranchState);
    muSnapshotC = static_cast<u32>(meListType);
    mbSnapshotFlagA = static_cast<u8>(mi8FirstVisibleIndex);
    mbSnapshotFlagB = static_cast<u8>(mi8SelectedRowIndex);
    u32 luIndicator = 0;
    if (mi8SelectedIndex > 0)
        luIndicator = 2;
    if (mi8SelectedIndex <= static_cast<s32>(muNumEntries) - 1)
        ++luIndicator;
    muScrollIndicator = luIndicator;
}

// @0x8242BBE8 ShowSpecificFriend ------------------------------------------------------------------
void FriendsListComponent::ShowSpecificFriend(const char* lpcName)
{
    const s32 leMode = mpGuiCache->GetGameMode();
    if (leMode != -1 && leMode != 15)                            // @0x8242BC10
        return;

    u32 luIdx = 0;
    while (luIdx < muNumEntries &&
           LobbyNameCmp(maRecords[luIdx].macName, lpcName) != 0)
        ++luIdx;                                                 // @0x8242BC34

    if (luIdx == muNumEntries)                                   // @0x8242BCA4
    {
        mi8SelectedIndex = 0;
        mi8SelectedRowIndex = 0;
        UpdateAllFriendsEntryData();
        return;
    }

    mi8SelectedIndex = static_cast<s8>(luIdx);                   // @0x8242BC6C
    if (muNumEntries <= KI_VISIBLE_ROWS)
    {
        mi8SelectedRowIndex = static_cast<s8>(luIdx);            // @0x82442BC90
    }
    else
    {
        const s32 liFromBottom = static_cast<s32>(luIdx)
                              - static_cast<s32>(muNumEntries) - 1;
        if (liFromBottom >= KI_VISIBLE_ROWS)                     // @0x82442BC88
        {
            UpdateAllFriendsEntryData();
            return;
        }
        mi8SelectedRowIndex = static_cast<s8>(4 - liFromBottom); // subfic @0x82442BC8C
    }
    UpdateAllFriendsEntryData();
}

} // namespace BrnGui
