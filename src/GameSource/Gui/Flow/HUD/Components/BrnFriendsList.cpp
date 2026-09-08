// BrnFriendsList.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The two BrnGui::FriendsListComponent
// methods the GUI flow reaches at attach/refresh time:
//   SetGuiCachePointer @0x82473580 -- latch the GuiCache pointer + cache its far field
//   SetDirty           @0x8241F120 -- snapshot the current selection + scroll indicator
// SetGuiCachePointer runs one non-fatal CGS_ASSERT pointer guard (the X360 proceeds
// regardless). The X360-baked assert file/line are discarded per project convention;
// the stringized condition matches the X360 assert text.

#include "GameSource/Gui/Flow/HUD/Components/BrnFriendsList.h"
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h"
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"
#include <cstdlib>
#include "BrnCommonTypes.h"                                     // VecFloat (MovieClipRef::SetPositionY arg)
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"    // CgsDev::StrStream (AttemptStateRestore's streamed assert)
#include "GameShared/GameClasses/Language/CgsLanguageManager.h" // FormatAndAddText + ParameterFormatType
#include "SharedClasses/DataLists/ChallengeList.h"
#include "SharedClasses/DataLists/ChallengeListEntry.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface full type
#include "GameSource/Gui/BrnGuiWorldDataController.h"                  // WorldDataController::GetFreeburnChallengeList
#include "GameShared/GameClasses/System/CgsHardwareInit.h"      // CgsSystem::HardwareInit::IsHardDiskAvailable
#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h" // IsRunning/IsShowingResults tier reads
#include "GameSource/Gui/Flow/HUD/Components/BrnFriendsListEntry.h" // LobbyNameCmp   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"     // one-shot deferred-call log

// EasyDrive lifecycle, navigation and row population restored from ARTIST.

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
    CgsNetwork::PlayerName s_amabSavedNames[1];   // unk_82FB00C0 (stride 0x84)
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
            s_amabSavedNames[0].Construct(
                maRecords[mi8SelectedIndex].mPlayerName.macName);
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
    // 0xB78 + count*132 is record[count-1].mbIsFullBuddy, not a parallel array.
    while (muNumEntries > 0)
    {
        SFriendRecord& last = maRecords[muNumEntries - 1];
        if (last.mbIsFullBuddy && last.mbIsOnline) break;
        maeDisplayTypes[muNumEntries - 1] = 0;
        memset(&last, 0, sizeof(last));
        --muNumEntries;
    }
}

// @0x824147A8 --------------------------------------------------------------------
int FriendsListComponent::BuddySortFunction(const void* lpA, const void* lpB)
{
    const SFriendRecord* lpRA = static_cast<const SFriendRecord*>(lpA);
    const SFriendRecord* lpRB = static_cast<const SFriendRecord*>(lpB);

    if (lpRA->mbIsFullBuddy != 0)
    {
        if (lpRB->mbIsFullBuddy == 0)
            return -1;
    }
    else
        return (lpRB->mbIsFullBuddy != 0) ? 1 : 0;

    const s32 laType = static_cast<s32>(lpRA->miInviteStatus);
    const s32 lbType = static_cast<s32>(lpRB->miInviteStatus);
    if (laType == 1)
        return (lbType == 1) ? LobbyNameCmp(lpRA->mPlayerName.macName, lpRB->mPlayerName.macName) : -1;
    if (lbType == 1)
        return 1;
    if (laType == 2)
        return (lbType == 2) ? LobbyNameCmp(lpRA->mPlayerName.macName, lpRB->mPlayerName.macName) : -1;
    if (lbType == 2)
        return 1;
    if (lpRA->mbIsOnline == 1)
        return (lpRB->mbIsOnline == 1) ? LobbyNameCmp(lpRA->mPlayerName.macName, lpRB->mPlayerName.macName) : -1;
    return (lpRB->mbIsOnline == 1) ? 1 : 0;
}

// @0x82414868 --------------------------------------------------------------------
bool FriendsListComponent::MoveHighlightDueToBranchOpen()
{
    u32 luIdx = 0;
    while (luIdx < muNumEntries &&
           LobbyNameCmp(mHighlightedName.macName, maRecords[luIdx].mPlayerName.macName) != 0)
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
        const s32 excess = static_cast<s32>(luIdx) - mi8SelectedRowIndex
                         - static_cast<s32>(muNumEntries) + KI_VISIBLE_ROWS;
        if (excess > 0) mi8SelectedRowIndex += static_cast<s8>(excess);
        return true;
    }

    meBranchState = 0;
    mHighlightedName.macName[0] = 0;
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
            if (!lbInLobby)                                         // @0x82414304..54
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
        if (mpGuiCache->mbIsOnlineHost && !mpGuiCache->mbOnlineRanked) // +0xA9DF @0x82414438
            maeAvailableShortcutOptions[muNumEntries++] = 19;       // @0x824144D0
        if (mpGuiCache->meOnlineGameMode == 11)                     // @0x824144EC..F8
            maeAvailableShortcutOptions[muNumEntries++] = 11;
        maeAvailableShortcutOptions[muNumEntries++] = 12;           // always @0x8241451C..
        maeAvailableShortcutOptions[muNumEntries++] = 18;           // ..58 block
        maeAvailableShortcutOptions[muNumEntries++] = 20;
        maeAvailableShortcutOptions[muNumEntries++] = 13;
    }

    // The final store is already counted by the append above.
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
    lrEntry.SetHighlightable(true);                                // entry vtable+4 @0x82422DB8
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
    if (liCount < static_cast<s32>(muNumEntries))
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
           LobbyNameCmp(maRecords[luIdx].mPlayerName.macName, lpcName) != 0)
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
        const s32 liFromBottom = static_cast<s32>(muNumEntries) - static_cast<s32>(luIdx) - 1;
        mi8SelectedRowIndex = liFromBottom >= KI_VISIBLE_ROWS ? 0 : static_cast<s8>(4 - liFromBottom);
    }
    UpdateAllFriendsEntryData();
}


// ===================================================================================
// [stuntrace F2 wave, 2026-08-27] the two per-frame/attach leaves the RACE_MAIN mount
// needs. mbFriendsList is 1 for E_MODE_STUNT_ATTACK, so BOTH of these execute on the
// mode-7 path: RaceMainHudState::UpdateWFInit @0x824805D4 calls AttemptStateRestore
// once at reveal, and ::UpdatePermenant @0x82480B78 calls UpdateAptVariables every
// frame (right after poking mabEntryFlags[1]).
// ===================================================================================

namespace
{
    // X360 .rdata tables, all read out of image.bin (offset == VA - 0x82000000,
    // big-endian) rather than re-typed from the pseudocode.

    // off_82F24D40[6] -- the panel-state timeline labels, indexed by the SNAPSHOT of
    // mePanelState (0 closed / 1 opening / 2 open / 5 dismissed). The run ends where the
    // 4-entry scroll-arrow label table @0x82F24D58 begins, which pins the count at 6.
    const char* const KAPC_PANEL_STATE_FRAMES[6] =
    {
        "invisible", "transIn", "idle", "nextPage", "prevPage", "transOut"
    };

    // off_82F24D68[13] -- the branch-state timeline labels, indexed by the SNAPSHOT of
    // meBranchState. Exactly 13 entries, a clean 1:1 with EFriendListBranchState
    // (E_FRIENDLISTBRANCH_COUNT == 13); the run ends at 0x82F24D9C where the
    // "$FRIENDSLIST_*" shortcut-label table begins.
    const char* const KAPC_BRANCH_STATE_FRAMES[13] =
    {
        "invisible",
        "oneBranchIn",     "oneBranchOut",
        "twoBranchIn",     "twoBranchOut",
        "threeBranchIn",   "threeBranchOut",
        "oneBranchFirst",
        "twoBranchFirst",  "twoBranchSecond",
        "threeBranchFirst", "threeBranchSecond", "threeBranchThird"
    };

    // off_82F24E0C[4] -- the list titles, indexed by the SNAPSHOT of meListType
    // (0 none / 1 friends / 2 shortcuts / 3 challenges). Index 0 is unk_820046A7, the
    // shared empty string (a lone NUL in .rdata). Index 3 is NOT a display string: it is
    // the loc-string id the challenges arm formats the party player count into. The run
    // ends at 0x82F24E1C where the "branch_mc_branchOption*_mc" component names begin.
    const char* const KAPC_LIST_TITLES[4] =
    {
        "", "$HUD_FRIENDS", "$CN_TITLE_ONLINE", "EASY_DRIVE_CHALLENGES"
    };

    // off_82F256E0 -> 0x82051D64. The dynamic-string id the challenges arm publishes the
    // formatted title under. The console passes the pointer PAST the leading '$' as the
    // AddString key ("FRIEND_LIST_TITLE") and the pointer WITH it as the text to display,
    // which is why the two spellings differ by exactly one byte on X360
    // (`addi r28, r10, aFriendListTitl+1 - 0x82051D64`).
    const char KAC_FRIEND_LIST_TITLE_TEXT[] = "$FRIEND_LIST_TITLE";
    const char* const KPC_FRIEND_LIST_TITLE_ID = KAC_FRIEND_LIST_TITLE_TEXT + 1;   // console's `+1` past the '$'

    // flt_8204C0A0 / flt_82051D60 -- the branch clip's row pitch and origin:
    // Y = row * 27.0f - 75.4f (rows 0..4 -> -75.4 / -48.4 / -21.4 / 5.6 / 32.6).
    const f32 KF_BRANCH_ROW_PITCH  = 27.0f;
    const f32 KF_BRANCH_ROW_ORIGIN = 75.400002f;

    // The snapshot of mi8SelectedRowIndex is stored as a byte and sign-extended before the
    // multiply (`lbz r10, 0x411A ; extsb r11, r10 ; fcfid` @0x82423630..0x82423650).
    const s32 KI_SCROLL_INDICATOR_UP_ONLY   = 2;
    const s32 KI_SCROLL_INDICATOR_DOWN_ONLY = 1;
    const s32 KI_SCROLL_INDICATOR_BOTH      = 3;
}

// @0x82441D78 AttemptStateRestore --------------------------------------------------
// If SaveCurrentState latched a list before the panel closed, re-open it. The saved
// state lives in the file-local .bss slots at the top of this TU.
//
// CONSOLE QUIRK, PRESERVED: the friends arm (type 1) tail-calls ShowFriendsList and
// returns WITHOUT clearing s_abStateSavedFlag / s_aiSavedListType (@0x82441E6C falls
// straight into the epilogue at 0x82441E70) -- so a saved friends list is restored on
// every subsequent AttemptStateRestore until something else overwrites the slots. The
// other three arms all clear both. Do not "fix" this into a shared clear.
void FriendsListComponent::AttemptStateRestore()
{
    if (!s_abStateSavedFlag)                                     // byte_82FB27E0 @0x82441D88
        return;

    if (s_aiSavedListType == 1)                                  // @0x82441D9C
    {
        ShowFriendsList();                                       // @0x82441E6C
        return;                                                  // no clear -- see banner
    }

    if (s_aiSavedListType == 2)                                  // @0x82441DA4
    {
        ShowSpecificShortcut(s_aiSavedShortcutOption);           // @0x82441E54 (dword_82FB27F0 kept)
    }
    else if (s_aiSavedListType == 3)                             // @0x82441DAC
    {
        // `ld r4, qword_82FB27E8` -- ONE 64-bit CgsID argument. Hex-Rays renders it as the
        // (HIDWORD, LODWORD) pair a 32-bit ABI would use; the asm is a single ld.
        ShowSpecificChallenge(s_u64SavedChallengeUid);            // @0x82441E30
        s_u64SavedChallengeUid = 0;                              // @0x82441E38
    }
    else
    {
        // The console streams into the GLOBAL CgsDev::Assert::gpcMessageBuffer
        // (@0x82441DCC..0x82441DF8); this port uses the tree's stack-buffer StrStream
        // idiom and drops the X360-baked file/line (cpp:3873) per convention. Text verbatim.
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "Invalid list type : " << s_aiSavedListType << "\n";
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);   // X360 cpp:3873
        CgsDev::Assert::EndAssert();
    }

    s_abStateSavedFlag = 0;                                      // @0x82441E3C / @0x82441E5C / @0x82441E18
    s_aiSavedListType  = 0;                                      // @0x82441E40 / @0x82441E60 / @0x82441E1C
}

// @0x82423558 UpdateAptVariables ---------------------------------------------------
// Push the latched selection snapshot (raised by SetDirty) out to the apt movie: the
// panel's own timeline label, the branch container's label + Y position, the two scroll
// arrows' visibility and the list title. One-shot -- it consumes and clears mbDirty.
//
// Every value read here is the SNAPSHOT half of the pair (+0x4108..+0x411A), never the
// live cursor: mePanelState/meBranchState/meListType/mi8SelectedRowIndex are re-read from
// muSnapshotA/muSnapshotB/muSnapshotC/mbSnapshotFlagB. Do not "simplify" to the live
// members -- the snapshot is what SetDirty froze at the moment of the change.
void FriendsListComponent::UpdateAptVariables()
{
    if (!mbDirty)                                                // +0x4118 @0x82423568
        return;

    const s32 leListType = static_cast<s32>(muSnapshotC);        // +0x4110 @0x82423578
    mbDirty = 0;                                                 // @0x82423580
    if (std::getenv("BRN_EASYDRIVE_TRACE") && CgsDev::Log::gpDebugPrint)
        *CgsDev::Log::gpDebugPrint << "[easydrive-view] panel " << static_cast<s32>(muSnapshotA)
            << " list " << leListType << " branch " << static_cast<s32>(muSnapshotB)
            << " selected " << static_cast<s32>(mi8SelectedIndex)
            << " row " << static_cast<s32>(mi8SelectedRowIndex)
            << " count " << static_cast<s32>(muNumEntries) << "\n";


    const char* lpcStringIDToUse;
    if (leListType == 3)                                         // challenges @0x8242357C
    {
        // The party player count goes into a "%d" scratch string, which is then printed
        // into the "EASY_DRIVE_CHALLENGES" loc-string's %1 marker and published as the
        // dynamic string "FRIEND_LIST_TITLE".
        char lacText[64];
        CgsCore::SPrintf(lacText, sizeof(lacText), "%d",
                         mpGuiCache->GetNumActivePlayers());      // cache +0xAC74 @0x824235A8
        lacText[sizeof(lacText) - 1] = 0;                        // @0x824235AC

        const char* const lkpcStringID = KAPC_LIST_TITLES[leListType];   // @0x824235D0
        mpStateInterface->GetLanguageManager()->FormatAndAddText(
            KPC_FRIEND_LIST_TITLE_ID,
            lkpcStringID,
            CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,    // r6 == 9
            1,                                                   // r7 == one positional parameter
            lacText,
            CgsLanguage::LanguageManager::E_FORMAT_INTEGER);     // r9 == 11
        lpcStringIDToUse = KAC_FRIEND_LIST_TITLE_TEXT;           // @0x824235F8
    }
    else
    {
        lpcStringIDToUse = KAPC_LIST_TITLES[leListType];         // @0x8242360C
    }

    mAptRef.GotoAndPlayLabel(KAPC_PANEL_STATE_FRAMES[muSnapshotA]);          // @0x82423628

    // (s8)mbSnapshotFlagB is the snapshot of mi8SelectedRowIndex; the X360 sign-extends
    // the byte, converts it to double, then does a single fmsubs against the two .rdata
    // floats. SetPositionY takes a broadcast VecFloat (the console's vspltw of lane 0).
    const f32 lfPosition = static_cast<f32>(static_cast<s8>(mbSnapshotFlagB)) * KF_BRANCH_ROW_PITCH
                         - KF_BRANCH_ROW_ORIGIN;                             // @0x8242363C..0x82423664
    const VecFloat lvfPosition = { lfPosition, lfPosition, lfPosition, lfPosition };
    mBranchClip.SetPositionY(lvfPosition);                                   // @0x82423670
    mBranchClip.GotoAndPlayLabel(KAPC_BRANCH_STATE_FRAMES[muSnapshotB]);     // @0x8242368C

    // The 0..3 scroll indicator SetDirty computed: bit 2 == an entry above the window,
    // bit 1 == an entry below it. Anything outside 1..3 hides both arrows.
    const s32 liIndicator = static_cast<s32>(muScrollIndicator);             // +0x4114 @0x82423690
    const bool lbShowUp   = (liIndicator == KI_SCROLL_INDICATOR_UP_ONLY ||
                             liIndicator == KI_SCROLL_INDICATOR_BOTH);
    const bool lbShowDown = (liIndicator == KI_SCROLL_INDICATOR_DOWN_ONLY ||
                             liIndicator == KI_SCROLL_INDICATOR_BOTH);
    mUpArrowClip.SetVisible(lbShowUp);                                       // +0xBD0 @0x824236B4/CC/DC
    mDownArrowClip.SetVisible(lbShowDown);                                   // +0xBD8 @0x824236E8

    mListTitleField.SetText(lpcStringIDToUse, false);                        // +0xBBC @0x824236F8
}


// @0x8243FF68 ShowFriendsList ------------------------------------------------------
// Re-open the panel on the FRIENDS list: park the cursor at the top of the list, latch
// the selection snapshot, post the "friends list shown" out-event, ask for fresh friend
// data and re-run the apt bind.
//
// The console INLINED SetDirty here: the seven stores at 0x8243FFA4..0x8243FFC4 are
// exactly SetDirty's, constant-folded against the cursor this function just wrote
//   (+0x4118 = 1, +0x4108 = mePanelState(1), +0x410C = meBranchState(0),
//    +0x4119 = mi8FirstVisibleIndex (untouched here), +0x411A = mi8SelectedRowIndex(0),
//    +0x4110 = meListType(1), +0x4114 = (muNumEntries - 1 > 0) ? 1 : 0)
// -- and that last term is precisely SetDirty's indicator for a selected index of 0
// (base 0, `0 >= muNumEntries - 1 ? 0 : 1`). Restored as the real call per house rule.
void FriendsListComponent::ShowFriendsList()
{
    mi8SelectedRowIndex = 0;                                     // @0x8243FF8C
    mi8SelectedIndex = 0;                                        // @0x8243FF90
    meListType = 1;                                              // @0x8243FF98 (friends)
    mePanelState = 1;                                            // @0x8243FF9C (opening)
    meBranchState = E_FRIENDLISTBRANCH_INVISIBLE;                // @0x8243FFA0

    SetDirty();                                                  // inlined @0x8243FFA4..C4

    struct ShownRequest                                          // wrapper {1,94,12} + payload byte
    {
        s32 miOutEventSize;
        s32 miOutEventType;
        s32 miOutEventOffset;
        u8  mubPayload;                                          // stb 1, var_14 @0x8243FFE4
    } lRequest;
    lRequest.miOutEventSize   = 1;                               // payload width
    lRequest.miOutEventType   = 94;                              // 0x5E
    lRequest.miOutEventOffset = 12;
    lRequest.mubPayload       = 1;
    mpStateInterface->GetOutputEventQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lRequest), 40, 16);          // @0x8243FFF8

    RequestRefreshedData();                                      // @0x82440000

    Invalidate();
}

// @0x82441C90 ShowSpecificShortcut --------------------------------------------------
// Re-open the panel on the SHORTCUTS list with one named option pre-selected. The gate
// is the ShowSpecificFriend gate plus "the panel is closed": only the offline (-1) and
// online-lobby (15) game modes may re-open it.
void FriendsListComponent::ShowSpecificShortcut(s32 leOption)
{
    const s32 leMode = mpGuiCache->GetGameMode();                // cache +0x9E58 @0x82441CB8
    if (leMode != -1 && leMode != 15)                            // @0x82441CBC..C8
        return;
    if (mePanelState != 0)                                       // @0x82441CCC..D4
        return;

    ShowShortcutList();

    const s32 liNumEntries = static_cast<s32>(muNumEntries);     // +0x894, signed compares
    s32 liIndex = 0;                                             // @0x82441CE4
    while (liIndex < liNumEntries &&
           maeAvailableShortcutOptions[liIndex] != leOption)     // +0x00C stride 4 @0x82441CF8
        ++liIndex;

    if (liIndex == liNumEntries)                                 // not found @0x82441D18
    {
        mi8SelectedIndex = 0;                                    // @0x82441D50
        mi8SelectedRowIndex = 0;                                 // @0x82441D54
    }
    else
    {
        mi8SelectedIndex = static_cast<s8>(liIndex);             // extsb+stb @0x82441D20/28
        if (liNumEntries <= KI_VISIBLE_ROWS)
        {
            mi8SelectedRowIndex = mi8SelectedIndex;              // @0x82441D48
        }
        else
        {
            // The console re-sign-extends the STORED byte before this subtract
            // (`extsb r11, r11` @0x82441D30), so the truncated cursor is what feeds it.
            const s32 liFromBottom = liNumEntries - mi8SelectedIndex - 1;   // @0x82441D34/38
            if (liFromBottom >= KI_VISIBLE_ROWS)                 // @0x82441D3C/40
                mi8SelectedRowIndex = 0;                         // @0x82441D54
            else
                mi8SelectedRowIndex = static_cast<s8>(4 - liFromBottom);    // subfic @0x82441D44
        }
    }

    UpdateAllShortcutsEntryData();
}

// @0x82441B78 ShowSpecificChallenge -------------------------------------------------
// Re-open the panel on the CHALLENGES list with one challenge pre-selected. Same gate as
// ShowSpecificShortcut plus a party-size gate: freeburn challenges need more than one
// active player (cache +0xAC74 > 1), so a solo session never re-opens this list.
//
// CONSOLE QUIRK, PRESERVED: the tail re-latches the (possibly re-clamped) selection back
// into the saved-challenge .bss slot -- and it runs on BOTH arms, including the
// "id not found -> cursor 0" arm, so a stale saved uid is rewritten to whatever challenge
// now sits at index 0. AttemptStateRestore then zeroes the slot on its way out.
void FriendsListComponent::ShowSpecificChallenge(CgsID lu64Uid)
{
    const s32 leMode = mpGuiCache->GetGameMode();                // cache +0x9E58 @0x82441BA0
    if (leMode != -1 && leMode != 15)                            // @0x82441BA4..B0
        return;
    if (mePanelState != 0)                                       // @0x82441BB4..BC
        return;
    if (mpGuiCache->GetNumActivePlayers() <= 1)                  // cache +0xAC74 @0x82441BC8..D0
        return;

    ShowChallengesList();

    const s32 liNumEntries = static_cast<s32>(muNumEntries);     // +0x894 @0x82441BDC
    s32 liIndex = 0;
    // `ld r7, 0(r10)` from this+0x70, stride 8, compared with cmpld -- ONE 64-bit id per
    // slot. (Hex-Rays renders it as the `*(v8 + 4)` low half a 32-bit ABI would use.)
    while (liIndex < liNumEntries &&
           mau64ChallengeIds[liIndex] != lu64Uid)                // +0x070 @0x82441BF4..FC
        ++liIndex;

    if (liIndex == liNumEntries)                                 // not found @0x82441C14
    {
        mi8SelectedIndex = 0;                                    // @0x82441C4C
        mi8SelectedRowIndex = 0;                                 // @0x82441C50
    }
    else
    {
        mi8SelectedIndex = static_cast<s8>(liIndex);             // extsb+stb @0x82441C1C/24
        if (liNumEntries <= KI_VISIBLE_ROWS)
        {
            mi8SelectedRowIndex = mi8SelectedIndex;              // @0x82441C44
        }
        else
        {
            const s32 liFromBottom = liNumEntries - mi8SelectedIndex - 1;   // @0x82441C2C..34
            if (liFromBottom >= KI_VISIBLE_ROWS)                 // @0x82441C38/3C
                mi8SelectedRowIndex = 0;                         // @0x82441C50
            else
                mi8SelectedRowIndex = static_cast<s8>(4 - liFromBottom);    // subfic @0x82441C40
        }
    }

    UpdateAllChallengesEntryData();

    // `lbz +0x882 ; extsb ; addi 14 ; slwi 3 ; ldx ; std qword_82FB27E8` -- the cursor is
    // re-read from memory and (idx + 14) * 8 is &mau64ChallengeIds[idx] (+0x70 == 14 * 8).
    s_u64SavedChallengeUid = mau64ChallengeIds[mi8SelectedIndex];               // @0x82441C5C..74
}

namespace {
// ARTIST 0x82f24e1c
const char* const KAPC_BRANCH_COMPONENTS[] = {
    "branch_mc_branchOptionOne_mc",
    "branch_mc_branchOptionTwo_mc",
    "branch_mc_branchOptionThree_mc",
};
// ARTIST 0x82f24e7c
const char* const KAPC_BRANCH_TEXT_FIELDS[] = {
    "branchOptionOne_txt",
    "branchOptionTwo_txt",
    "branchOptionThree_txt",
};
// ARTIST 0x82f24e28
const char* const KAPC_SHORTCUT_TEXT[] = {
    "ONLINE_GAME_SEARCH_OPTION_FRIENDS",
    "ONLINE_EASY_DRIVE_SHORTCUTS_ROAD_RULES",
    "ONLINE_EASY_DRIVE_SHORTCUTS_FREEBURN",
    "ONLINE_EASY_DRIVE_SHORTCUTS_IMAGE_GALLERY",
    "ONLINE_EASY_DRIVE_SHORTCUTS_VIEW_CHALLENGES",
    "ONLINE_EASY_DRIVE_SHORTCUTS_UNRANKED",
    "ONLINE_EASY_DRIVE_SHORTCUTS_RANKED",
    "ONLINE_EASY_DRIVE_SHORTCUTS_SCOREBOARDS",
    "ONLINE_EASY_DRIVE_SHORTCUTS_NEWS",
    "ONLINE_EASY_DRIVE_SHORTCUTS_LAUNCH",
    "ONLINE_EASY_DRIVE_SHORTCUTS_CREATE_EVENT",
    "ONLINE_EASY_DRIVE_SHORTCUTS_CHANGE_TEAMS",
    "ONLINE_EASY_DRIVE_SHORTCUTS_VIEW_PLAYERS",
    "ONLINE_EASY_DRIVE_SHORTCUTS_LEAVE_GAME",
    "ONLINE_EASY_DRIVE_SHORTCUTS_CANCEL_CHALLENGE",
    "ONLINE_EASY_DRIVE_SHORTCUTS_CHALLENGES",
    "ONLINE_EASY_DRIVE_SHORTCUTS_VIEW_EVENT",
    "ONLINE_EASY_DRIVE_SHORTCUTS_CANCEL_EVENT",
    "ONLINE_EASY_DRIVE_SHORTCUTS_VIEW_MAP",
    "ONLINE_EASY_DRIVE_SHORTCUTS_CHANGE_SECURITY",
    "ONLINE_EASY_DRIVE_SHORTCUTS_SETTINGS",
};
// ARTIST 0x82f24dd0
const char* const KAPC_SHORTCUT_BRANCH_TEXT[] = {
    "$ONLINE_EASY_DRIVE_SHORTCUTS_RR_OFF",
    "$ONLINE_EASY_DRIVE_SHORTCUTS_RR_TIME",
    "$ONLINE_EASY_DRIVE_SHORTCUTS_RR_CRASH",
    "$ONLINE_EASY_DRIVE_SHORTCUTS_QM",
    "$ONLINE_EASY_DRIVE_SHORTCUTS_CM",
    "$ONLINE_EASY_DRIVE_SHORTCUTS_CR",
    "$ONLINE_EASY_DRIVE_SHORTCUTS_QM",
    "$ONLINE_EASY_DRIVE_SHORTCUTS_CM",
    "$ONLINE_EASY_DRIVE_SHORTCUTS_CR",
    "$ONLINE_EASY_DRIVE_SHORTCUTS_QM",
    "$ONLINE_EASY_DRIVE_SHORTCUTS_CM",
    "$ONLINE_EASY_DRIVE_SHORTCUTS_CR",
    "$ONLINE_EASY_DRIVE_SHORTCUTS_AL",
    "$ONLINE_EASY_DRIVE_SHORTCUTS_FO",
    "$ONLINE_EASY_DRIVE_SHORTCUTS_IO",
};
}

// ARTIST 0x8242B188: bind the shipped EasyDrive movie and its five reusable rows.
void FriendsListComponent::Prepare(const char* lacName, const BrnFlapt::FileRef& lFile)
{
    BrnFlaptComponent::Prepare(lacName, lFile, 0);
    CGS_ASSERT(mAptRef.mpMovieClipInst != 0, "mpMovieClipInst");
    mAptRef.SetFrameTriggerCallback(reinterpret_cast<void*>(&TransitionCompleteCallback), this);
    for (s32 i = 0; i < 3; ++i)
    {
        char name[128];
        CgsCore::SnPrintf(name, 127, "%s_%s", lacName, KAPC_BRANCH_COMPONENTS[i]);
        name[127] = 0;
        BrnFlapt::MovieClipRef clip, parent;
        lFile.FindComponent(&clip, name);
        clip.SetVisible(false);
        clip.GetParent(&parent);
        parent.FindChildTextField(&maBranchLabelFields[i], KAPC_BRANCH_TEXT_FIELDS[i]);
    }
    for (s32 i = 0; i < KI_VISIBLE_ROWS; ++i)
    {
        char name[32];
        CgsCore::SPrintf(name, sizeof(name), "Entry%i", i);
        maEntries[i].Prepare(name, lFile, lacName);
    }
    BrnFlapt::MovieClipRef clip;
    mAptRef.FindChildMovieClip(&clip, "listTitle_mc");
    clip.FindChildTextField(&mListTitleField, "listTitle_txt");
    mAptRef.FindChildMovieClip(&mBranchClip, "branch_mc");
    mAptRef.FindChildMovieClip(&clip, "upArrow_mc");
    clip.FindChildMovieClip(&mUpArrowClip, "arrow");
    mAptRef.FindChildMovieClip(&clip, "downArrow_mc");
    clip.FindChildMovieClip(&mDownArrowClip, "arrow");
}

// ARTIST 0x8242B440.
void FriendsListComponent::Invalidate()
{
    for (s32 i = 0; i < KI_VISIBLE_ROWS; ++i)
        maEntries[i].Invalidate();
}

// ARTIST 0x8242B628. +0x880 is the number of populated visible rows, despite
// the earlier consumer-only member name mi8FirstVisibleIndex.
void FriendsListComponent::UpdateAllShortcutsEntryData()
{
    BuildShortcutOptions();
    mi8FirstVisibleIndex = 0;
    s32 first = mi8SelectedIndex - mi8SelectedRowIndex;
    if (muNumEntries >= KI_VISIBLE_ROWS)
    {
        const s32 excess = first - static_cast<s32>(muNumEntries) + KI_VISIBLE_ROWS;
        if (excess > 0)
        {
            first -= excess;
            mi8SelectedIndex -= static_cast<s8>(excess);
        }
    }
    for (s32 i = 0; i < KI_VISIBLE_ROWS; ++i)
    {
        if (i >= static_cast<s32>(muNumEntries))
            maEntries[i].Invalidate();
        else
        {
            ++mi8FirstVisibleIndex;
            const s32 option = maeAvailableShortcutOptions[first + i];
            CGS_ASSERT(option != E_SHORTCUTOPTION_COUNT, "Trying to display an invalid option");
            const bool disabled = !CgsSystem::HardwareInit::IsHardDiskAvailable()
                               && option >= 2 && option <= 6;
            SetEntryData(i, KAPC_SHORTCUT_TEXT[option], disabled ? 3 : 17, true);
        }
    }
    while (mi8SelectedRowIndex >= mi8FirstVisibleIndex)
    {
        --mi8SelectedRowIndex;
        --mi8SelectedIndex;
    }
    Highlight(mi8SelectedRowIndex);
    SetDirty();
}

namespace
{
    // Original StateInterface::OutputGuiEvent wraps raw payloads on channel 40.
    template<class T> void FriendsOutput(CgsGui::StateInterface* state, T& event)
    {
        CgsGui::GuiEventWrapper<T, 40> wrapper(event);
        state->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&wrapper), 40, sizeof(wrapper));
    }

    void FriendsShown(CgsGui::StateInterface* state, bool shown)
    {
        struct Record { s32 size, type, offset; u8 shown; } record = {1, 94, 12, shown};
        state->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&record), 40, sizeof(record));
    }

    void FriendsChallenge(CgsGui::StateInterface* state, GuiCache* cache, CgsID id, s32 action)
    {
        const BrnResource::ChallengeListEntry* entry = cache->GetFreeburnChallengeList()->GetChallengeData(id);
        GuiChallengeSelectedEvent event = {id, action, static_cast<s32>(entry->GetChallengeStyle())};
        FriendsOutput(state, event);
    }

    void FriendsAudio(CgsGui::StateInterface* state)
    {
        GuiAudioTriggerEvent event;
        event.Construct(2, "", "EasyDriveEntry");
        state->OutputGuiEvent(event);
    }
}

// ARTIST 0x824417E0.
void FriendsListComponent::ShowShortcutList()
{
    const s32 mode = mpGuiCache->GetGameMode();
    if (mode != -1 && mode != 15) return;
    mi8SelectedRowIndex = mi8SelectedIndex = 0;
    meListType = 2;
    mePanelState = 1;
    meBranchState = 0;
    SetDirty();
    FriendsShown(mpStateInterface, true);
    UpdateAllEntryData();
    if (!CgsSystem::HardwareInit::IsHardDiskAvailable() && !s_sbHddOverlayShown)
    {
        GuiOverlayRequest event;
        event.Construct("OnReqHDD");
        FriendsOutput(mpStateInterface, event);
        s_sbHddOverlayShown = true;
    }
}

// ARTIST 0x824418D0.
void FriendsListComponent::ShowChallengesList()
{
    mi8SelectedRowIndex = mi8SelectedIndex = 0;
    meListType = 3;
    mePanelState = 1;
    meBranchState = 0;
    SetDirty();
    FriendsShown(mpStateInterface, true);
    UpdateAllEntryData();
}

// ARTIST 0x82440020.
void FriendsListComponent::UpdateAllEntryData()
{
    switch (meListType)
    {
    case 1: UpdateAllFriendsEntryData(); break;
    case 2: UpdateAllShortcutsEntryData(); break;
    case 3: UpdateAllChallengesEntryData(); break;
    default: CGS_ASSERT(false, "Unknown list type"); break;
    }
}

// ARTIST 0x824397E8.
void FriendsListComponent::Close()
{
    if (mePanelState == 0) return;
    if (meListType == 3)
    {
        FriendsChallenge(mpStateInterface, mpGuiCache, mau64ChallengeIds[mi8SelectedIndex], 3);
        FriendsChallenge(mpStateInterface, mpGuiCache, mau64ChallengeIds[mi8SelectedIndex], 1);
    }
    mePanelState = 5;
    SetDirty();
    Invalidate();
}

// ARTIST 0x82442868.
void FriendsListComponent::HandleDPadLeft()
{
    if (mePanelState != 2) return;
    if (meListType == 1) ShowShortcutList();
    else if (meListType == 3)
    {
        FriendsChallenge(mpStateInterface, mpGuiCache, mau64ChallengeIds[mi8SelectedIndex], 3);
        FriendsChallenge(mpStateInterface, mpGuiCache, mau64ChallengeIds[mi8SelectedIndex], 1);
        ShowShortcutList();
    }
    else Close();
}

// ARTIST 0x82441988.
bool FriendsListComponent::SelectPrevious()
{
    if (mi8SelectedIndex == 0 || muNumEntries == 0) return false;
    if (mi8SelectedRowIndex > 0)
    {
        --mi8SelectedIndex;
        --mi8SelectedRowIndex;
        Highlight(mi8SelectedRowIndex);
        if (meListType == 3)
            FriendsChallenge(mpStateInterface, mpGuiCache, mau64ChallengeIds[mi8SelectedIndex], 2);
        return true;
    }
    if (mi8SelectedRowIndex == 0 && mi8SelectedIndex > 0)
    {
        --mi8SelectedIndex;
        UpdateAllEntryData();
        return true;
    }
    return false;
}

// ARTIST 0x82441A80.
bool FriendsListComponent::SelectNext()
{
    if (muNumEntries == 0 || mi8SelectedIndex == static_cast<s32>(muNumEntries) - 1) return false;
    if (mi8SelectedRowIndex < 4)
    {
        ++mi8SelectedIndex;
        ++mi8SelectedRowIndex;
        Highlight(mi8SelectedRowIndex);
        if (meListType == 3)
            FriendsChallenge(mpStateInterface, mpGuiCache, mau64ChallengeIds[mi8SelectedIndex], 2);
        return true;
    }
    if (mi8SelectedRowIndex == 4 && mi8SelectedIndex < static_cast<s32>(muNumEntries) - 1)
    {
        ++mi8SelectedIndex;
        UpdateAllEntryData();
        return true;
    }
    return false;
}

// ARTIST 0x82442D98.
void FriendsListComponent::HandleDPadRight()
{
    if (mePanelState == 0) { ShowShortcutList(); return; }
    switch (meListType)
    {
    case 1: HandleDPadRightFriends(); break;
    case 2: HandleDPadRightShortcuts(); break;
    case 3: HandleDPadRightChallenges(); break;
    default: CGS_ASSERT(false, "unknown list type"); break;
    }
}

// ARTIST 0x82442E50. Input action IDs are the original GUI D-pad actions.
void FriendsListComponent::HandleTableInteraction(s32 action)
{
    if (action == 37 || action == 38)
    {
        if (mePanelState != 2) return;
        if (action == 37 ? SelectPrevious() : SelectNext())
        {
            SetDirty();
            FriendsAudio(mpStateInterface);
        }
        if (meListType == 3) s_u64SavedChallengeUid = mau64ChallengeIds[mi8SelectedIndex];
    }
    else if (action == 39) HandleDPadLeft();
    else if (action == 40) HandleDPadRight();
}

// ARTIST 0x82443280.
void FriendsListComponent::HandleControllerInput(const s32* input)
{
    CGS_ASSERT(input != 0, "lpControllerInput");
    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");
    // Host diagnostic: one record per GUI press, disabled in normal play.
    if (std::getenv("BRN_EASYDRIVE_TRACE") && CgsDev::Log::gpDebugPrint)
        *CgsDev::Log::gpDebugPrint << "[easydrive-input] action " << input[1]
            << " ready " << (mpGuiCache->GetGameplayHudReady() ? 1 : 0)
            << " panel " << mePanelState << " list " << meListType << " mode " << mpGuiCache->GetGameMode() << " data " << meDataState << "\n";
    if (!mpGuiCache->GetGameplayHudReady() || (meListType == 1 && meDataState != 2)) return;
    if (mePanelState == 2 && meBranchState != 0) HandleBranchInteraction(input[1]);
    else HandleTableInteraction(input[1]);
}

// ARTIST 0x82442C78.
void FriendsListComponent::Update()
{
    if (mbReopenAfterClose)
    {
        FriendsShown(mpStateInterface, false);
        mbReopenAfterClose = false;
    }
    for (s32 i = 0; i < KI_VISIBLE_ROWS; ++i) maEntries[i].Update();
    const s32 count = mpGuiCache->GetNumActivePlayers();
    if (count != static_cast<s32>(muCachedCacheField))
    {
        if (mePanelState != 0)
        {
            if (meListType == 2 && (count <= 1 || muCachedCacheField <= 1)) ShowShortcutList();
            else if (meListType == 3)
            {
                if (count > 1) ShowChallengesList();
                else HandleDPadLeft();
            }
        }
        muCachedCacheField = count;
    }
}

// ARTIST 0x82441E78.
void FriendsListComponent::ReshowShortcuts()
{
    if (mePanelState == 0 || meListType != 2) return;
    const s32 option = maeAvailableShortcutOptions[mi8SelectedIndex];
    ShowShortcutList();
    s32 index = 0;
    while (index < static_cast<s32>(muNumEntries) && maeAvailableShortcutOptions[index] != option) ++index;
    mi8SelectedRowIndex = 0;
    if (index == static_cast<s32>(muNumEntries)) mi8SelectedIndex = 0;
    else
    {
        mi8SelectedIndex = static_cast<s8>(index);
        if (muNumEntries <= 5) mi8SelectedRowIndex = static_cast<s8>(index);
        else if (static_cast<s32>(muNumEntries) - index - 1 < 5)
            mi8SelectedRowIndex = static_cast<s8>(4 - (static_cast<s32>(muNumEntries) - index - 1));
    }
    UpdateAllShortcutsEntryData();
}

// ARTIST 0x82442FF0.
void FriendsListComponent::EndWait()
{
    mpGuiCache->muFriendsPanelBranchMirror = 0;
    if (meBranchState != 0)
    {
        meBranchState = 0;
        HandleTableInteraction(40);
    }
    if (mePanelState != 0 && muNumEntries == 0 && !mpGuiCache->IsOnlineStartInProgress())
    {
        meBranchState = 0;
        mi8SelectedRowIndex = mi8SelectedIndex = 1;
        mi8FirstVisibleIndex = 2;
        SetEntryData(0, "FRIENDSLIST_NO_FRIENDS", 2, true);
        SetEntryData(1, "FRIENDSLIST_INSTANT_FREEBURN", 6, true);
        Highlight(mi8SelectedRowIndex);
    }
}

namespace {
// ARTIST 0x82F24D9C / 0x82F24E88 (17 friend-entry states).
const char* const KAPC_FRIEND_BRANCH_TEXT[] = {
    "",
    "$FRIENDSLIST_ALREADY_IN_GAME",
    "$FRIENDSLIST_SIGN_IN",
    "$FRIENDSLIST_GAMER_PROFILE",
    "$FRIENDSLIST_SEND_INVITE",
    "$FRIENDSLIST_JOIN_GAME",
    "$FRIENDSLIST_REVOKE_INVITE",
    "$FRIENDSLIST_ACCEPT_INVITE",
    "$FRIENDSLIST_CANNOT_INVITE",
    "$FRIENDSLIST_DECLINE_INVITE",
    "$FRIENDSLIST_INVITE_IN_PROGRESS",
    "$FRIENDSLIST_REVOKE_IN_PROGRESS",
    "$FRIENDSLIST_DECLINE_IN_PROGRESS",
};
const s32 KAA_FRIEND_BRANCHES[17][3] = {
    {0, 0, 0},
    {2, 0, 0},
    {0, 0, 0},
    {3, 0, 0},
    {3, 1, 0},
    {3, 0, 0},
    {4, 5, 3},
    {4, 3, 0},
    {5, 3, 0},
    {3, 0, 0},
    {7, 9, 3},
    {8, 3, 0},
    {5, 8, 3},
    {3, 0, 0},
    {10, 0, 0},
    {11, 0, 0},
    {12, 0, 0},
};
}

// ARTIST 0x82423120.
void FriendsListComponent::ShowShortcutsBranch()
{
    CGS_ASSERT(mi8SelectedIndex < E_SHORTCUTOPTION_COUNT, "miCurrentlyHighlightedOption < E_SHORTCUT_OPTIONS_COUNT");
    const s32 option = maeAvailableShortcutOptions[mi8SelectedIndex];
    CGS_ASSERT(option < E_SHORTCUTOPTION_COUNT, "maeAvailableShortcutOptions[ miCurrentlyHighlightedOption ] < E_SHORTCUT_OPTIONS_COUNT");
    s32 count = 0;
    s32 first = -1;
    switch (option)
    {
    case 1: first = 0; break;
    case 2: first = 3; break;
    case 5: first = 6; break;
    case 6: first = 9; break;
    case 19:
        CGS_ASSERT(mpGuiCache->meOnlineSecurity >= 0 && mpGuiCache->meOnlineSecurity < 3, "Unknown security value");
        count = 2;
        mauNumBranches[0] = mpGuiCache->meOnlineSecurity == 0 ? 13 : 12;
        mauNumBranches[1] = mpGuiCache->meOnlineSecurity == 2 ? 13 : 14;
        break;
    default: CGS_ASSERT(false, "Shortcut option doesn't have branches"); break;
    }
    if (first >= 0)
    {
        count = 3;
        for (s32 i = 0; i < 3; ++i) mauNumBranches[i] = first + i;
    }
    CGS_ASSERT(count >= 1 && count <= 3, "Invalid number of shortcut branches");
    meBranchState = count * 2 - 1;
    SetDirty();
    meBranchState = count == 1 ? 7 : count == 2 ? 8 : 10;
    mHighlightedName.macName[0] = 0;
    for (s32 i = 0; i < 3; ++i)
        maBranchLabelFields[i].SetText(i < count ? KAPC_SHORTCUT_BRANCH_TEXT[mauNumBranches[i]] : "", false);
    mi8CurrentlyHighlightedBranch = 0;
}

// ARTIST 0x82439040.
void FriendsListComponent::HandleBranchInteraction(s32 action)
{
    CGS_ASSERT(meBranchState != 0, "E_FRIENDLISTBRANCH_INVISIBLE != meBranchState");
    if (action == 37 && (meBranchState == 9 || meBranchState == 11 || meBranchState == 12))
    {
        --meBranchState;
        --mi8CurrentlyHighlightedBranch;
        SetDirty();
        FriendsAudio(mpStateInterface);
    }
    else if (action == 38 && (meBranchState == 8 || meBranchState == 10 || meBranchState == 11))
    {
        ++meBranchState;
        ++mi8CurrentlyHighlightedBranch;
        SetDirty();
        FriendsAudio(mpStateInterface);
    }
    else if (action == 39) WithdrawBranches();
    else if (action == 40)
    {
        switch (meListType)
        {
        case 1: HandleBranchDPadRightFriends(); break;
        case 2: HandleBranchDPadRightShortcuts(); break;
        case 3: break;
        default: CGS_ASSERT(false, "unknown list type"); break;
        }
    }
    CGS_ASSERT(mi8CurrentlyHighlightedBranch >= 0 && mi8CurrentlyHighlightedBranch < 3,
               "miCurrentlyHighlightedBranch in range");
}

// ARTIST 0x82438DC0.
void FriendsListComponent::HandleBranchDPadRightShortcuts()
{
    const s32 branch = mauNumBranches[mi8CurrentlyHighlightedBranch];
    if (branch >= 0 && branch <= 2)
    {
        GuiEventRoadRuleModeRequest event = {};
        const s32 type = branch == 2 ? 1 : 0;
        memcpy(event.maData, &type, sizeof(type));
        event.maData[4] = branch != 0;
        FriendsOutput(mpStateInterface, event);
    }
    else if (branch >= 3 && branch <= 11)
    {
        static const s32 options[] = {0, 1, 2, 5, 6, 7, 8, 9, 10};
        GuiEventPerformOnlineMainMenuOption event = {};
        memcpy(event.maData, &options[branch - 3], 4);
        mpStateInterface->OutputInternalState(event);
    }
    else if (branch >= 12 && branch <= 14)
    {
        GuiEventPerformOnlinePauseOption event = {};
        const s32 option = branch - 2;
        memcpy(event.maData, &option, 4);
        mpStateInterface->OutputInternalState(event);
    }
    else CGS_ASSERT(false, "Unknown branch option");
    WithdrawBranches();
    mePanelState = 5;
    SetDirty();
    Invalidate();
}

// ARTIST 0x82442938.
void FriendsListComponent::HandleDPadRightShortcuts()
{
    CGS_ASSERT(mi8SelectedIndex < E_SHORTCUTOPTION_COUNT, "miCurrentlyHighlightedOption < E_SHORTCUT_OPTIONS_COUNT");
    const s32 option = maeAvailableShortcutOptions[mi8SelectedIndex];
    CGS_ASSERT(option < E_SHORTCUTOPTION_COUNT, "maeAvailableShortcutOptions[ miCurrentlyHighlightedOption ] < E_SHORTCUT_OPTIONS_COUNT");
    if (!CgsSystem::HardwareInit::IsHardDiskAvailable() && option >= 2 && option <= 6) return;
    switch (option)
    {
    case 0: ShowFriendsList(); return;
    case 1: case 2: case 5: case 6: case 19: ShowShortcutsBranch(); return;
    case 3: case 4: case 7: case 8:
    {
        const s32 value = option <= 4 ? option : option + 4;
        GuiEventPerformOnlineMainMenuOption event = {};
        memcpy(event.maData, &value, 4);
        mpStateInterface->OutputInternalState(event);
        break;
    }
    case 9: case 10: case 11: case 12: case 13: case 16: case 17: case 18: case 20:
    {
        const s32 value = option <= 13 ? option - 9 : option == 20 ? 13 : option - 10;
        GuiEventPerformOnlinePauseOption event = {};
        memcpy(event.maData, &value, 4);
        mpStateInterface->OutputInternalState(event);
        break;
    }
    case 14:
    {
        const BrnResource::ChallengeListEntry* entry = mpGuiCache->GetFreeburnChallengeManager()->GetCurrentChallenge();
        GuiChallengeSelectedEvent event = {entry->GetChallengeID(), 1, static_cast<s32>(entry->GetChallengeStyle())};
        FriendsOutput(mpStateInterface, event);
        break;
    }
    case 15:
        mePanelState = 0;
        ShowSpecificChallenge(s_u64SavedChallengeUid);
        return;
    default: CGS_ASSERT(false, "Unknown shortcut option"); break;
    }
    mePanelState = 5;
    SetDirty();
    Invalidate();
}

// ARTIST 0x8242B498.
void FriendsListComponent::UpdateAllFriendsEntryData()
{
    mi8FirstVisibleIndex = 0;
    s32 first = mi8SelectedIndex - mi8SelectedRowIndex;
    if (muNumEntries >= 5)
    {
        const s32 excess = first - static_cast<s32>(muNumEntries) + 5;
        if (excess > 0) { first -= excess; mi8SelectedIndex -= static_cast<s8>(excess); }
    }
    for (s32 i = 0; i < 5; ++i)
    {
        if (i >= static_cast<s32>(muNumEntries)) maEntries[i].Invalidate();
        else
        {
            ++mi8FirstVisibleIndex;
            SetEntryData(i, maRecords[first + i].mPlayerName.macName, maeDisplayTypes[first + i], false);
        }
    }
    while (mi8SelectedRowIndex >= mi8FirstVisibleIndex) { --mi8SelectedRowIndex; --mi8SelectedIndex; }
    Highlight(mi8SelectedRowIndex);
    SetDirty();
}

// ARTIST 0x82423708.
void FriendsListComponent::SortFullList()
{
    for (s32 i = 0; i < static_cast<s32>(muNumEntries); ++i)
    {
        SFriendRecord& record = maRecords[i];
        record.mbIsInSameLobby = false;
        if (record.mbIsOnline)
            for (s32 player = 0; player < 8; ++player)
            {
                const auto* info = mpGuiCache->GetOnlinePlayerInfo(player);
                if (info->mNetworkPlayerID != -1 && LobbyNameCmp(info->mPlayerName.macName, record.mPlayerName.macName) == 0)
                { record.mbIsInSameLobby = true; break; }
            }
    }
    qsort(maRecords, muNumEntries, sizeof(SFriendRecord), &BuddySortFunction);
    for (s32 i = 0; i < static_cast<s32>(muNumEntries); ++i)
    {
        const SFriendRecord& record = maRecords[i];
        s32 status;
        if (!record.mbIsOnline) status = 5;
        else if (record.mbIsInSameLobby) status = 4;
        else if (!mpGuiCache->IsMultiplayerAllowed() || !CgsSystem::HardwareInit::IsHardDiskAvailable()) status = 13;
        else if (record.miInviteStatus == 2) status = 10;
        else if (record.miInviteStatus == 1) status = record.mbIsJoinable ? 8 : 9;
        else
        {
            const s32 mode = mpGuiCache->GetGameMode();
            const bool canInvite = mode == -1 || ((mode == 15 || mode == 16) && !mpGuiCache->mbOnlineRanked);
            status = canInvite ? (record.mbIsJoinable ? 6 : 7) : (record.mbIsJoinable ? 12 : 11);
        }
        maeDisplayTypes[i] = status;
    }
}

// ARTIST 0x824430D0.
void FriendsListComponent::ProcessNewEntryData(const void* data)
{
    CGS_ASSERT(data != 0, "lpFriendInfo");
    if (meListType != 1) return;
    const GuiEventOnlineReceiveFriendInfo& event = *static_cast<const GuiEventOnlineReceiveFriendInfo*>(data);
    if (!event.mbConnected) { HandleNotConnected(); meDataState = 2; return; }
    if (event.miNumberOfBuddiesInEvent == 0) { HandleNoFriends(); meDataState = 2; return; }
    for (s32 i = 0; i < event.miNumberOfBuddiesInEvent; ++i)
        maRecords[event.miIndexOfFirstBuddyInFullBuddyList + i] = event.mBuddyInformation[i];
    if (event.miIndexOfFirstBuddyInFullBuddyList + event.miNumberOfBuddiesInEvent < static_cast<s32>(muNumEntries)) return;
    meDataState = 2;
    SortFullList();
    RemoveUnneededFriends();
    if (muNumEntries == 0) { HandleNoFriends(); return; }
    const bool reopen = meBranchState != 0 && MoveHighlightDueToBranchOpen();
    UpdateAllEntryData();
    if (reopen) { meBranchState = 0; HandleTableInteraction(40); }
    else if (s_abStateSavedFlag && s_aiSavedListType == 1)
    {
        ShowSpecificFriend(s_amabSavedNames[0].macName);
        s_abStateSavedFlag = 0;
        s_aiSavedListType = 0;
    }
}

// ARTIST 0x82422E18.
void FriendsListComponent::ShowFriendsListBranch()
{
    s32 status;
    switch (mpGuiCache->muFriendsPanelBranchMirror)
    {
    case 0: status = maEntries[mi8SelectedRowIndex].GetEntryStatus(); break;
    case 1: status = 14; break;
    case 2: status = 15; break;
    case 3: status = 16; break;
    case 4: status = 0; break;
    default: CGS_ASSERT(false, "Invalid case in switch"); status = maEntries[mi8SelectedRowIndex].GetEntryStatus(); break;
    }
    if (!CgsSystem::HardwareInit::IsHardDiskAvailable() && status != 2) status = 3;
    s32 count = 0;
    for (s32 i = 0; i < 3; ++i) if (KAA_FRIEND_BRANCHES[status][i]) ++count;
    if (count == 0) return;
    meBranchState = count * 2 - 1;
    SetDirty();
    meBranchState = count == 1 ? 7 : count == 2 ? 8 : 10;
    mHighlightedName.Construct(maRecords[mi8SelectedIndex].mPlayerName.macName);
    for (s32 i = 0; i < 3; ++i) maBranchLabelFields[i].SetText(KAPC_FRIEND_BRANCH_TEXT[KAA_FRIEND_BRANCHES[status][i]], false);
    mi8CurrentlyHighlightedBranch = 0;
}

// ARTIST 0x824386B0.
void FriendsListComponent::HandleDPadRightFriends()
{
    if (meBranchState != 0) return;
    if (muNumEntries != 0 || mi8SelectedRowIndex != 1) { ShowFriendsListBranch(); return; }
    if (mpGuiCache->muFriendsPanelBranchMirror != 0) return;
    GuiEventOnlineInviteEvent event = {};
    event.meRequestedAction = 6;
    FriendsOutput(mpStateInterface, event);
    mpGuiCache->muFriendsPanelBranchMirror = 4;
    mi8FirstVisibleIndex = 1;
    mi8SelectedRowIndex = mi8SelectedIndex = 0;
    maEntries[1].Invalidate();
    SetDirty();
}

// ARTIST 0x82438938.
void FriendsListComponent::HandleBranchDPadRightFriends()
{
    s32 status;
    switch (mpGuiCache->muFriendsPanelBranchMirror)
    {
    case 0: status = maEntries[mi8SelectedRowIndex].GetEntryStatus(); break;
    case 1: status = 14; break;
    case 2: status = 15; break;
    case 3: status = 16; break;
    case 4: status = 0; break;
    default: CGS_ASSERT(false, "Invalid case in switch"); status = maEntries[mi8SelectedRowIndex].GetEntryStatus(); break;
    }
    const s32 branch = KAA_FRIEND_BRANCHES[status][mi8CurrentlyHighlightedBranch];
    bool close = false;
    if (branch == 3)
    {
        GuiEventOnlineShowProfile event;
        event.mFriendToView.Construct(maEntries[mi8SelectedRowIndex].GetNameField().GetText());
        FriendsOutput(mpStateInterface, event);
    }
    else if (branch == 2 || branch == 4 || branch == 5 || branch == 6 || branch == 7 || branch == 9)
    {
        if (branch != 4 || mabEntryFlags[1])
        {
            GuiEventOnlineInviteEvent event = {};
            event.mFriendToAddress.Construct(branch == 2 ? "" : maEntries[mi8SelectedRowIndex].GetNameField().GetText());
            event.meRequestedAction = branch == 2 ? 5 : branch == 4 ? 0 : branch == 5 ? 4 : branch == 6 ? 1 : branch == 7 ? 2 : 3;
            if (branch == 6) event.mbHasOnlineGameBeenStarted = mpGuiCache->IsOnlineStartInProgress();
            FriendsOutput(mpStateInterface, event);
            if (branch == 4) { mpGuiCache->muFriendsPanelBranchMirror = 1; mpGuiCache->mbOnlineGameOptionsChanged = true; }
            else if (branch == 6) mpGuiCache->muFriendsPanelBranchMirror = 2;
            else if (branch == 9) mpGuiCache->muFriendsPanelBranchMirror = 3;
        }
        close = branch == 4;
    }
    else CGS_ASSERT(branch == 1 || branch == 8 || branch == 10 || branch == 11 || branch == 12, "Invalid branch option used");
    WithdrawBranches();
    if (close) { mePanelState = 5; SetDirty(); Invalidate(); }
}

// ARTIST 0x82439350.
void FriendsListComponent::UpdateAllChallengesEntryData()
{
    BuildChallengeList();
    mi8FirstVisibleIndex = 0;
    s32 first = mi8SelectedIndex - mi8SelectedRowIndex;
    if (muNumEntries >= 5)
    {
        const s32 excess = first - static_cast<s32>(muNumEntries) + 5;
        if (excess > 0) { first -= excess; mi8SelectedIndex -= static_cast<s8>(excess); }
    }
    const auto* list = mpGuiCache->GetFreeburnChallengeList();
    for (s32 i = 0; i < 5; ++i)
    {
        if (i >= static_cast<s32>(muNumEntries)) maEntries[i].Invalidate();
        else
        {
            ++mi8FirstVisibleIndex;
            const auto* entry = list->GetChallengeData(mau64ChallengeIds[first + i]);
            const s32 index = list->GetChallengeIndex(entry->GetChallengeID());
            const auto* completed = mpGuiCache->GetFreeburnChallengeManager()->GetCompletedChallengesData()->GetLocalPlayerCompletionStatus();
            CGS_ASSERT(static_cast<u32>(index) < 2000, "Challenge index out of range");
            const bool done = (completed->maxBits[index / 64] & (u64(1) << (index % 64))) != 0;
            SetEntryData(i, entry->GetTitleStringID(), done ? 20 : 19, true);
            maEntries[i].SetIndexText(first + i + 1);
        }
    }
    while (mi8SelectedRowIndex >= mi8FirstVisibleIndex) { --mi8SelectedRowIndex; --mi8SelectedIndex; }
    Highlight(mi8SelectedRowIndex);
    FriendsChallenge(mpStateInterface, mpGuiCache, mau64ChallengeIds[mi8SelectedIndex], 2);
    SetDirty();
}

// ARTIST 0x82438760.
void FriendsListComponent::HandleDPadRightChallenges()
{
    const CgsID id = mau64ChallengeIds[mi8SelectedIndex];
    FriendsChallenge(mpStateInterface, mpGuiCache, id, 0);
    FriendsChallenge(mpStateInterface, mpGuiCache, id, 3);
    if (mpGuiCache->meOnlineGameMode != 15)
    {
        GuiEventNetworkGameParams event;
        memcpy(event.maEvents, mpGuiCache->maOnlineGameModeOptionsStorage, sizeof(event.maEvents));
        event.meGameMode = 15;
        event.mePreviousGameMode = mpGuiCache->meOnlinePreviousGameMode;
        event.meSecurity = mpGuiCache->meOnlineSecurity;
        event.meBoostType = mpGuiCache->meOnlineBoostType;
        event.meVehicleChoice = mpGuiCache->miOnlineHostGameState;
        event.miTimeLimit = mpGuiCache->miOnlineTimeLimit;
        event.miNumRounds = mpGuiCache->miOnlineNumRounds;
        event.miVehicleClass = mpGuiCache->miOnlineVehicleClass;
        event.miNumRunnerCrashes = mpGuiCache->miOnlineNumRunnerCrashes;
        event.mbInfiniteBoost = mpGuiCache->mbOnlineInfiniteBoost;
        event.mbTrafficOn = mpGuiCache->mbOnlineTrafficOn;
        event.mbTrafficCheckingOn = mpGuiCache->mbOnlineTrafficCheckingOn;
        event.mbRanked = mpGuiCache->mbOnlineRanked;
        FriendsOutput(mpStateInterface, event);
    }
    mePanelState = 5;
    SetDirty();
    Invalidate();
}

} // namespace BrnGui
