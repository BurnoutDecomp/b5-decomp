// BrnFriendsList.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The two BrnGui::FriendsListComponent
// methods the GUI flow reaches at attach/refresh time:
//   SetGuiCachePointer @0x82473580 -- latch the GuiCache pointer + cache its far field
//   SetDirty           @0x8241F120 -- snapshot the current selection + scroll indicator
// SetGuiCachePointer runs one non-fatal CGS_ASSERT pointer guard (the X360 proceeds
// regardless). The X360-baked assert file/line are discarded per project convention;
// the stringized condition matches the X360 assert text.
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gui/Flow/HUD/Components/BrnFriendsList.cpp"
#include "GameSource/Input/GameInputActions.h"
#include "GameSource/Gui/Flow/HUD/Components/BrnFriendsList.h"
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h"
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"
#include <cstdlib>
#include "BrnCommonTypes.h"                                     // VecFloat (MovieClipRef::SetPositionY arg)
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
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

static const char KAC_ARTIST_ASSERT_FILE[] = "..\\..\\..\\GameSource\\Gui/Flow/HUD/Components/BrnFriendsList.h";

namespace BrnGui
{

// @ 0x82473580
//   if (lpGuiCache == 0) <assert "lpGuiCache">
//   stw lpGuiCache, 0x4100(this)             ; mpGuiCache = lpGuiCache
//   lwzx r, lpGuiCache, 0xAC74 ; stw r,0x870 ; miPlayerCount = cache far field
//   return this
// SetGuiCachePointer is header-inline (BrnFriendsList.h).

// @ 0x8241F120
//   snapshot current selection into the dirty region:
//     mSnapshotA(+0x4108) = meCurrentState(+0x878)
//     mSnapshotB(+0x410C) = meBranchState(+0x87C)
//     mSnapshotC(+0x4110) = meListType(+0x874)
//     miDirtyCurrentlyActiveSlots(+0x4119) = miCurrentlyActiveSlots(+0x880)
//     miDirtyCurrentlyHighlightedSlot(+0x411A) = miCurrentlyHighlightedSlot(+0x881)
//   raise dirty flag: mbDirty(+0x4118) = 1
//   scroll indicator (+0x4114), liIndex = (s8)miCurrentlyHighlightedOption(+0x882):
//     base = (liIndex > 0) ? 2 : 0
//     if (liIndex >= miTotalOptions - 1)  meDirtyArrowConfiguration = base
//     else                              meDirtyArrowConfiguration = base + 1
// DWARF h:494 / cpp:2435 (local leCurrentArrowState cpp:2437). PS3 0x39B3C8 computes 2 * (option > 0), then adds one while the option is not
// the last; ARTIST inlines it into SetDirty (0x8241F160..0x8241F184).
FriendsListComponent::EArrowConfigurations FriendsListComponent::GetArrowState()
{
    EArrowConfigurations leCurrentArrowState = E_ARROWS_NONE;
    if (miCurrentlyHighlightedOption > 0)
    {
        leCurrentArrowState = E_ARROWS_UP_ONLY;
    }
    if (miCurrentlyHighlightedOption < miTotalOptions - 1)
    {
        leCurrentArrowState = static_cast<EArrowConfigurations>(leCurrentArrowState + E_ARROWS_DOWN_ONLY);
    }
    return leCurrentArrowState;
}

// @ 0x82410958 -- post-increment on the friends-list branch enum: advance the referenced enum
// by one (writing it back), assert it stays <= E_FRIENDLISTBRANCH_THIRD_OF_THREE, and return the
// PRE-increment value. Free operator in namespace BrnGui (DWARF BrnFriendsList.h:767).
FriendsListComponent::EFriendListBranchState
operator++(FriendsListComponent::EFriendListBranchState& lreEnumIndex, int)
{
    const FriendsListComponent::EFriendListBranchState leOld = lreEnumIndex;
    lreEnumIndex = static_cast<FriendsListComponent::EFriendListBranchState>(lreEnumIndex + 1);
    CGS_ASSERT_AT(lreEnumIndex <= FriendsListComponent::E_FRIENDLISTBRANCH_THIRD_OF_THREE,
        "leEnumIndex <= FriendsListComponent::E_FRIENDLISTBRANCH_THIRD_OF_THREE",
            KAC_ARTIST_ASSERT_FILE, 603 /* ARTIST 0x82410984 */);

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
    CGS_ASSERT_AT(lreEnumIndex >= FriendsListComponent::E_FRIENDLISTBRANCH_FIRST_OF_ONE,
        "leEnumIndex >= FriendsListComponent::E_FRIENDLISTBRANCH_FIRST_OF_ONE",
            KAC_ARTIST_ASSERT_FILE, 603 /* ARTIST 0x824109E4 */);

    return leOld;
}

// The restore slots (DWARF class statics BrnFriendsList.h:439..443; ARTIST .bss).
bool FriendsListComponent::sbRestoreDataAvailable;                                  // 0x82FB27E0
FriendsListComponent::EListType FriendsListComponent::seStoredListType;            // 0x82FB27E4
CgsID FriendsListComponent::sStoredChallenge;                                       // 0x82FB27E8
CgsNetwork::PlayerName FriendsListComponent::sStoredFriend;                         // 0x82FB00C0
FriendsListComponent::EShortcutOptions FriendsListComponent::seShortcutOption;     // 0x82FB27F0

namespace
{
    bool   s_sbHddOverlayShown = false;                       // byte_82FB27E1 (X360-only HDD overlay; no DWARF name)
}

// @0x82422B20 --------------------------------------------------------------------
void FriendsListComponent::Construct(const char* /*lacName*/, CgsGui::StateInterface* lpStateInterface,
                                     const char* /*lpacParentName*/)
{
    // The header-inline base Construct (assert h:113): adopt the channel, drop the clip.
    BrnFlaptComponent::Construct(lpStateInterface);

    maeAvailableShortcutBranchOptions[0] = maeAvailableShortcutBranchOptions[1] = maeAvailableShortcutBranchOptions[2] = 0;
    for (s32 i = 0; i < KI_VISIBLE_ROWS; ++i)
        maEntries[i].Construct(0, lpStateInterface, 0, Selectable::K_INVALID_ID); // @0x82422BB8
    memset(maFullBuddyList, 0, 0x3390);                                // @0x82422BD0

    for (s32 i = 0; i < KI_MAX_FRIEND_RECORDS; ++i) maeFullBuddyEntryState[i] = 0;
    for (s32 i = 0; i < E_SHORTCUT_OPTIONS_COUNT; ++i)
        maeAvailableShortcutOptions[i] = E_SHORTCUT_OPTIONS_COUNT;

    maeAvailableShortcutBranchOptions[0] = maeAvailableShortcutBranchOptions[1] = maeAvailableShortcutBranchOptions[2] = 15;
    meListType = E_LIST_TYPE_FRIENDS;   meCurrentState = 0;   meBranchState = 0;
    miCurrentlyActiveSlots = 0;   miCurrentlyHighlightedSlot = 0;   miCurrentlyHighlightedOption = 0;
    miCurrentlyHighlightedBranch = 0;
    miTotalOptions = 0;
    mbVisible = mbCanSendInvites = 0;
    meDataCollectionState = 2;
    miPlayerCount = 0;
    mbDirty = 0;
    mpGuiCache = 0;
    mbTriggerHideFriendsList = 0;
}

// @0x82414960 --------------------------------------------------------------------
void FriendsListComponent::Highlight(s8 liSlotToHighlight)   // DWARF cpp:3535: liSlotToHighlight, liIndex
{
    for (s32 liIndex = 0; liIndex < KI_VISIBLE_ROWS; ++liIndex)
    {
        FriendsListEntry& lrEntry = maEntries[liIndex];
        if (liSlotToHighlight == liIndex)
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
    // The saving arm first, the clear in the else (ARTIST 0x824149F4 beq to 0x82414A8C).
    if (meCurrentState != 0)
    {
        sbRestoreDataAvailable = true;
        seStoredListType = meListType;
        switch (meListType)
        {
            case E_LIST_TYPE_FRIENDS:
                sStoredFriend.Construct(
                    maFullBuddyList[miCurrentlyHighlightedOption].mPlayerName.macName);
                break;
            case E_LIST_TYPE_SHORTCUTS:
                seShortcutOption = maeAvailableShortcutOptions[miCurrentlyHighlightedOption];
                break;
            case E_LIST_TYPE_CHALLENGES:
                sStoredChallenge = maChallengesAvailable[miCurrentlyHighlightedOption];
                break;
            default:
                break;
        }
    }
    else
    {
        sbRestoreDataAvailable = false;
    }
}

// @0x82414AA0 --------------------------------------------------------------------
void FriendsListComponent::TransitionCompleteCallback(void* lpUserData)
{
    CGS_ASSERT_AT(lpUserData != 0, "lpUserData", CGS_ARTIST_FILE, 3900 /* ARTIST 0x82414AD4 */);
    FriendsListComponent& lrSelf = *static_cast<FriendsListComponent*>(lpUserData);
    if (lrSelf.meCurrentState == 1)
        lrSelf.meCurrentState = 2;
    else if (lrSelf.meCurrentState == 5)
    {
        lrSelf.miCurrentlyHighlightedSlot = 0;
        lrSelf.miCurrentlyHighlightedOption = 0;
        lrSelf.meBranchState = 0;
        lrSelf.mbTriggerHideFriendsList = 1;
        lrSelf.meCurrentState = 0;
    }
}

// @0x824146C0 --------------------------------------------------------------------
void FriendsListComponent::RemoveUnneededFriends()
{
    // 0xB78 + count*132 is record[count-1].mbIsFullBuddy, not a parallel array.
    while (miTotalOptions > 0)
    {
        SFriendRecord& last = maFullBuddyList[miTotalOptions - 1];
        if (last.mbIsFullBuddy && last.mbIsOnline) break;
        maeFullBuddyEntryState[miTotalOptions - 1] = 0;
        memset(&last, 0, sizeof(last));
        --miTotalOptions;
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
    // DWARF local liIndex (cpp:3436) and the inlined PlayerName::IsEqualTo (ARTIST 0x8241489C:
    // LobbyNameCmp(mCurrentlySelectedName, record name)).
    int32_t liIndex;
    for (liIndex = 0; liIndex < miTotalOptions; ++liIndex)
    {
        if (mCurrentlySelectedName.IsEqualTo(maFullBuddyList[liIndex].mPlayerName))
            break;
    }

    if (liIndex != miTotalOptions)
    {
        miCurrentlyHighlightedOption = static_cast<s8>(liIndex);
        // The member cursor, not liIndex, is compared and subtracted (ARTIST 0x824148EC `extsb r8, r9`).
        if (miTotalOptions <= KI_VISIBLE_ROWS)
        {
            miCurrentlyHighlightedSlot = miCurrentlyHighlightedOption;
            return true;
        }
        if (miCurrentlyHighlightedOption <= miCurrentlyHighlightedSlot)
        {
            miCurrentlyHighlightedSlot = miCurrentlyHighlightedOption;
            return true;
        }
        // DWARF locals liSlotsBelow / liFriendsBelow / liNumberToShiftDown (cpp:3472/3475/3478).
        // Remaining difference (96.8): ARTIST sets the `true` before this arithmetic (0x82414900) and
        // its no-shift path joins the `return false` epilogue (0x82414958); the if/else-if, ||, early-
        // return and not-found-first spellings were tried.
        int32_t liSlotsBelow = KI_VISIBLE_ROWS - miCurrentlyHighlightedSlot;
        int32_t liFriendsBelow = miTotalOptions - miCurrentlyHighlightedOption;
        int32_t liNumberToShiftDown = liSlotsBelow - liFriendsBelow;
        if (liNumberToShiftDown > 0)
        {
            miCurrentlyHighlightedSlot += liNumberToShiftDown;
        }
        return true;
    }

    meBranchState = 0;
    mCurrentlySelectedName.Clear();
    if (miCurrentlyHighlightedOption >= static_cast<s32>(miTotalOptions))
        miCurrentlyHighlightedOption = static_cast<s8>(miTotalOptions - 1);
    if (miTotalOptions <= KI_VISIBLE_ROWS)
        miCurrentlyHighlightedSlot = miCurrentlyHighlightedOption;
    return false;
}

// ===================================================================================
// [friends wave -- BODY TRANCH 2] data producers + not-connected/no-friends arms.
// ===================================================================================

// @0x82414288 BuildShortcutOptions ---------------------------------------------------
void FriendsListComponent::BuildShortcutOptions()
{
    miTotalOptions = 0;
    maeAvailableShortcutOptions[0] = E_SHORTCUT_OPTIONS_SHOW_FRIENDS;      // always first @0x824142A0
    ++miTotalOptions;

    if (!mpGuiCache->IsOnlineStartInProgress())                     // offline arm @0x82414588
    {
        maeAvailableShortcutOptions[miTotalOptions++] = E_SHORTCUT_OPTIONS_FREEBURN;            // @0x82414594
        if (mpGuiCache->GetOfflineShortcutProgressGate())           // far byte +0x13B9A @0x824145B0
            maeAvailableShortcutOptions[miTotalOptions++] = E_SHORTCUT_OPTIONS_ROAD_RULES;        // @0x824145C0
        maeAvailableShortcutOptions[miTotalOptions++] = E_SHORTCUT_OPTIONS_IMAGE_GALLERY;            // @0x824145DC block
        maeAvailableShortcutOptions[miTotalOptions++] = E_SHORTCUT_OPTIONS_VIEW_CHALLENGES;
        maeAvailableShortcutOptions[miTotalOptions++] = E_SHORTCUT_OPTIONS_UNRANKED;
        maeAvailableShortcutOptions[miTotalOptions++] = E_SHORTCUT_OPTIONS_RANKED;
        maeAvailableShortcutOptions[miTotalOptions++] = E_SHORTCUT_OPTIONS_SCOREBOARDS;
        maeAvailableShortcutOptions[miTotalOptions++] = E_SHORTCUT_OPTIONS_NEWS;
    }
    else                                                            // online arm @0x824142C0
    {
        const FreeburnChallengeManager* lpMgr = mpGuiCache->GetFreeburnChallengeManager();
        const bool lbRunning  = lpMgr->IsRunning();                 // tier==3 @0x8241435C
        const bool lbResults  = lpMgr->IsShowingResults();          // tier==4 @0x8241437C

        if (!mpGuiCache->mbIsOnlineHost)                            // +0xB864 == 0 @0x8241444C
        {
            if (!(lbRunning || lbResults))
                maeAvailableShortcutOptions[miTotalOptions++] = E_SHORTCUT_OPTIONS_ROAD_RULES;    // @0x82414480
            if (!(mpGuiCache->GetOnlineGameModeOptions()->meGameMode == 15 ||
                  mpGuiCache->GetOnlineGameModeOptions()->meGameMode == 16))
                maeAvailableShortcutOptions[miTotalOptions++] = E_SHORTCUT_OPTIONS_VIEW_EVENT;   // @0x824144CC..D0
        }
        else                                                        // host arm @0x82414304
        {
            const bool lbInLobby = (mpGuiCache->GetOnlineGameModeOptions()->meGameMode == 15 ||
                                    mpGuiCache->GetOnlineGameModeOptions()->meGameMode == 16);
            if (!lbInLobby)                                         // @0x82414304..54
            {
                maeAvailableShortcutOptions[miTotalOptions++] = E_SHORTCUT_OPTIONS_LAUNCH;
                maeAvailableShortcutOptions[miTotalOptions++] = E_SHORTCUT_OPTIONS_VIEW_EVENT;
                maeAvailableShortcutOptions[miTotalOptions++] = E_SHORTCUT_OPTIONS_CANCEL_EVENT;
            }
            if (lbRunning)
                maeAvailableShortcutOptions[miTotalOptions++] = E_SHORTCUT_OPTIONS_CANCEL_CHALLENGE;   // @0x8241436C
            else if (!lbResults)
            {
                if (static_cast<s32>(mpGuiCache->muNumActivePlayers) > 1)   // +0xAC74
                    maeAvailableShortcutOptions[miTotalOptions++] = E_SHORTCUT_OPTIONS_SELECT_CHALLENGE;
                maeAvailableShortcutOptions[miTotalOptions++] = E_SHORTCUT_OPTIONS_ROAD_RULES;    // @0x824143D0
            }
            if (lbInLobby)
                maeAvailableShortcutOptions[miTotalOptions++] = E_SHORTCUT_OPTIONS_CREATE_EVENT;   // @0x82414414
        }
        if (mpGuiCache->mbIsOnlineHost && !mpGuiCache->GetOnlineGameModeOptions()->mbRanked) // +0xA9DF @0x82414438
            maeAvailableShortcutOptions[miTotalOptions++] = E_SHORTCUT_OPTIONS_CHANGE_SECURITY;       // @0x824144D0
        if (mpGuiCache->GetOnlineGameModeOptions()->meGameMode == 11)                     // @0x824144EC..F8
            maeAvailableShortcutOptions[miTotalOptions++] = E_SHORTCUT_OPTIONS_CHANGE_TEAMS;
        maeAvailableShortcutOptions[miTotalOptions++] = E_SHORTCUT_OPTIONS_VIEW_LOBBY;           // always @0x8241451C..
        maeAvailableShortcutOptions[miTotalOptions++] = E_SHORTCUT_OPTIONS_VIEW_MAP;           // ..58 block
        maeAvailableShortcutOptions[miTotalOptions++] = E_SHORTCUT_OPTIONS_SETTINGS;
        maeAvailableShortcutOptions[miTotalOptions++] = E_SHORTCUT_OPTIONS_LEAVE_GAME;
    }

    // The final store is already counted by the append above.
    for (s32 i = miTotalOptions; i < E_SHORTCUT_OPTIONS_COUNT; ++i)     // sentinel pad @0x82414684
        maeAvailableShortcutOptions[i] = E_SHORTCUT_OPTIONS_COUNT;
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
    miCurrentlyHighlightedBranch = 0;                              // @0x82423538
}

// @0x82422CB0 SetEntryData ---------------------------------------------------------------
void FriendsListComponent::SetEntryData(s8 liRow, const char* lpcText, s32 leStatus,
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
    if (meCurrentState == 0 || meListType != E_LIST_TYPE_FRIENDS)                    // @0x82439258..6C
        return;

    meDataCollectionState = 0;                                             // @0x82439288

    {
        GuiEventOnlineGetBuddyCount lBuddyCount;                 // DWARF cpp:2364
        mpStateInterface->OutputGuiEvent(lBuddyCount);           // @0x824392A4
    }
}

// @0x824392B8 SetTotalFriends -----------------------------------------------------------
void FriendsListComponent::SetTotalFriends(s32 liCount)
{
    if (liCount < static_cast<s32>(miTotalOptions))
    {
        memset(&maFullBuddyList[liCount], 0,
               (miTotalOptions - liCount) * sizeof(SFriendRecord));                // @0x824392F8
    }
    miTotalOptions = static_cast<u32>(liCount);                    // @0x82439308
    meDataCollectionState = 1;                                             // @0x8243931C

    GuiEventOnlineGetBuddies lBuddiesRequest;                    // DWARF cpp:2420
    mpStateInterface->OutputGuiEvent(lBuddiesRequest);           // @0x82439330
}

// @0x8242B830 BuildChallengeList ----------------------------------------------------------
void FriendsListComponent::BuildChallengeList()
{
    CGS_ASSERT(mpGuiCache->mpWorldDataController != 0, "mpWorldDataController");   // cpp:0x914
    const BrnResource::ChallengeList* lpList =
        mpGuiCache->mpWorldDataController->GetFreeburnChallengeList();

    const s32 leSlot = static_cast<s32>(mpGuiCache->muChallengeSlotMirror);           // +0xAC78 consumer-carved member
    miTotalOptions = 0;                                            // @0x8242B890

    const s32 liCount = lpList->GetChallengeCount();             // +0x32E0 @0x8242B894
    for (s32 i = 0; i < liCount; ++i)                            // @0x8242B8B0
    {
        const BrnResource::ChallengeListEntry* lpEntry = lpList->GetChallengeData(i);
        if ((static_cast<u8>(lpEntry->GetNumPlayers()) & 0xF) != leSlot)             // +0xD3 low nibble @0x8242B8C0
            continue;
        if (!lpList->IsChallengeContentBought(i))                // @0x8242B8D8
            continue;
        CGS_ASSERT(miTotalOptions < 256,
                   "KI_MAX_CHALLENGES_FOR_EACH_PLAYER_COUNT");   // cpp:0xBE5
        maChallengesAvailable[miTotalOptions++] = lpEntry->GetChallengeID();   // +0xC0 qword @0x8242B910
    }
}

// @0x8242B948 HandleNotConnected -------------------------------------------------------------
void FriendsListComponent::HandleNotConnected()
{
    miCurrentlyHighlightedSlot = 0;                                     // @0x8242B970
    miCurrentlyActiveSlots = 1;                                    // @0x8242B978
    miCurrentlyHighlightedOption = 0;                                        // @0x8242B97C
    meBranchState = 0;                                           // @0x8242B980
    SetEntryData(0, "FRIENDSLIST_NOT_CONNECTED",
                 FriendsListEntry::E_FRIENDLISTENTRYSTATE_YOUOFFLINE,
                 true);                                          // status 1 (`li r6, 1` @0x8242B96C)
    Highlight(miCurrentlyHighlightedSlot);

    for (s32 liRow = miCurrentlyActiveSlots; liRow < KI_VISIBLE_ROWS; ++liRow)
        maEntries[liRow].Invalidate();                           // @0x8242B9B4

    SetDirty();   // inlined by ARTIST (the snapshot tail, e.g. 0x8242B9CC)
}

// DWARF cpp:2086 (h:588); no ARTIST copy: HandleNoFriends and SortFullList inline it with its
// assert (ARTIST line 2089, 0x8242BA70; PS3 0x39C698 line 2088).
bool FriendsListComponent::HasMultiplayerPrivileges()
{
    CGS_ASSERT_AT(mpGuiCache, "mpGuiCache", CGS_ARTIST_FILE, 2089);
    return mpGuiCache->IsMultiplayerAllowed();
}

// @0x8242BA40 HandleNoFriends -------------------------------------------------------------------
void FriendsListComponent::HandleNoFriends()
{
    // The privilege test is HasMultiplayerPrivileges (DWARF cpp:2086), inlined with its
    // cpp:2089 "mpGuiCache" assert (ARTIST 0x8242BA60..0x8242BA8C).
    if (!mpGuiCache->IsOnline() && HasMultiplayerPrivileges() &&
        CgsSystem::HardwareInit::IsHardDiskAvailable())
    {
        // multiplayer-capable arm: NO_FRIENDS row + INSTANT_FREEBURN row (stores @0x8242BAC8..ADC)
        miCurrentlyHighlightedSlot = 1;
        miCurrentlyHighlightedOption = 1;
        miCurrentlyActiveSlots = 2;
        meBranchState = E_FRIENDLISTBRANCH_INVISIBLE;
        SetEntryData(0, "FRIENDSLIST_NO_FRIENDS",
                     FriendsListEntry::E_FRIENDLISTENTRYSTATE_NOFRIENDS,
                     true);
        SetEntryData(1, "FRIENDSLIST_INSTANT_FREEBURN",
                     FriendsListEntry::E_FRIENDLISTENTRYSTATE_FRIENDJOINABLE,
                     true);                                      // status 6 @0x8242BAFC
        Highlight(miCurrentlyHighlightedSlot);
    }
    else
    {
        // The single-row arm highlights nothing (ARTIST 0x8242BB40 goes straight to the loop).
        miCurrentlyHighlightedSlot = 0;                                 // @0x8242BB28
        miCurrentlyActiveSlots = 1;
        miCurrentlyHighlightedOption = 0;
        meBranchState = E_FRIENDLISTBRANCH_INVISIBLE;
        SetEntryData(0, "FRIENDSLIST_NO_FRIENDS",
                     FriendsListEntry::E_FRIENDLISTENTRYSTATE_NOFRIENDS,
                     true);                                      // @0x8242BB40
    }

    // DWARF local liScreenEntryIndex (cpp:3240).
    for (int32_t liScreenEntryIndex = miCurrentlyActiveSlots; liScreenEntryIndex < KI_VISIBLE_ROWS; ++liScreenEntryIndex)
        maEntries[liScreenEntryIndex].Invalidate();               // @0x8242BB64

    SetDirty();   // inlined by ARTIST (the snapshot tail)
}

// @0x8242BBE8 ShowSpecificFriend ------------------------------------------------------------------
void FriendsListComponent::ShowSpecificFriend(CgsNetwork::PlayerName* lpName)
{
    const s32 leMode = mpGuiCache->GetGameMode();
    if (leMode != -1 && leMode != 15)                            // @0x8242BC10
        return;

    // DWARF local liIndex (cpp:3682) and the inlined PlayerName::IsEqualTo (ARTIST 0x8242BC3C:
    // LobbyNameCmp(record name, lpName)); the found arm is the fall-through (0x8242BC64 `beq` to
    // the not-found stores placed last), as in ShowSpecificShortcut / ShowSpecificChallenge.
    int32_t liIndex;
    for (liIndex = 0; liIndex < miTotalOptions; ++liIndex)
    {
        if (maFullBuddyList[liIndex].mPlayerName.IsEqualTo(*lpName))
            break;
    }

    if (liIndex != miTotalOptions)
    {
        miCurrentlyHighlightedOption = static_cast<s8>(liIndex);                   // @0x8242BC70
        if (miTotalOptions <= KI_VISIBLE_ROWS)
        {
            miCurrentlyHighlightedSlot = miCurrentlyHighlightedOption;            // @0x8242BC94
        }
        else
        {
            // DWARF local liSlotsUsedAfterCurrent (cpp:3701).
            int32_t liSlotsUsedAfterCurrent = miTotalOptions - miCurrentlyHighlightedOption - 1;
            if (liSlotsUsedAfterCurrent >= KI_VISIBLE_ROWS)
                miCurrentlyHighlightedSlot = 0;
            else
                miCurrentlyHighlightedSlot = static_cast<s8>(4 - liSlotsUsedAfterCurrent);
        }
    }
    else
    {
        miCurrentlyHighlightedOption = 0;                                          // @0x8242BCA4
        miCurrentlyHighlightedSlot = 0;
    }
    UpdateAllFriendsEntryData();
}

// ===================================================================================
// [stuntrace F2 wave, 2026-08-27] the two per-frame/attach leaves the RACE_MAIN mount
// needs. mbFriendsList is 1 for E_MODE_STUNT_ATTACK, so BOTH of these execute on the
// mode-7 path: RaceMainHudState::UpdateWFInit @0x824805D4 calls AttemptStateRestore
// once at reveal, and ::UpdatePermenant @0x82480B78 calls UpdateAptVariables every
// frame (right after poking mbCanSendInvites).
// ===================================================================================

namespace
{
    // X360 .rdata tables, all read out of image.bin (offset == VA - 0x82000000,
    // big-endian) rather than re-typed from the pseudocode.

    // off_82F24D40[6] -- the panel-state timeline labels, indexed by the SNAPSHOT of
    // meCurrentState (0 closed / 1 opening / 2 open / 5 dismissed). The run ends where the
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
    const char* const KPC_FRIEND_LIST_TITLE_ID = "$FRIEND_LIST_TITLE" + 1;   // console's `+1` past the '$'

    // flt_8204C0A0 / flt_82051D60 -- the branch clip's row pitch and origin:
    // Y = row * 27.0f - 75.4f (rows 0..4 -> -75.4 / -48.4 / -21.4 / 5.6 / 32.6).
    const f32 KF_BRANCH_ROW_PITCH  = 27.0f;
    const f32 KF_BRANCH_ROW_ORIGIN = 75.400002f;

    // The snapshot of miCurrentlyHighlightedSlot is stored as a byte and sign-extended before the
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
// returns WITHOUT clearing sbRestoreDataAvailable / seStoredListType (@0x82441E6C falls
// straight into the epilogue at 0x82441E70) -- so a saved friends list is restored on
// every subsequent AttemptStateRestore until something else overwrites the slots. The
// other three arms all clear both. Do not "fix" this into a shared clear.
void FriendsListComponent::AttemptStateRestore()
{
    if (!sbRestoreDataAvailable)                                     // byte_82FB27E0 @0x82441D88
        return;

    if (seStoredListType == E_LIST_TYPE_FRIENDS)                                  // @0x82441D9C
    {
        ShowFriendsList();                                       // @0x82441E6C
        return;                                                  // no clear -- see banner
    }

    if (seStoredListType == E_LIST_TYPE_SHORTCUTS)                                  // @0x82441DA4
    {
        ShowSpecificShortcut(seShortcutOption);           // @0x82441E54 (dword_82FB27F0 kept)
    }
    else if (seStoredListType == E_LIST_TYPE_CHALLENGES)                             // @0x82441DAC
    {
        // `ld r4, qword_82FB27E8` -- ONE 64-bit CgsID argument. Hex-Rays renders it as the
        // (HIDWORD, LODWORD) pair a 32-bit ABI would use; the asm is a single ld.
        ShowSpecificChallenge(sStoredChallenge);            // @0x82441E30
        sStoredChallenge = 0;                              // @0x82441E38
    }
    else
    {
        // The console streams into the GLOBAL CgsDev::Assert::gpcMessageBuffer
        // (@0x82441DCC..0x82441DF8); this port uses the tree's stack-buffer StrStream
        // idiom and drops the X360-baked file/line (cpp:3873) per convention. Text verbatim.
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "Invalid list type : " << seStoredListType << "\n";
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);   // X360 cpp:3873
        CgsDev::Assert::EndAssert();
    }

    sbRestoreDataAvailable = false;                                      // @0x82441E3C / @0x82441E5C / @0x82441E18
    seStoredListType  = E_LIST_TYPE_NONE;                                      // @0x82441E40 / @0x82441E60 / @0x82441E1C
}

// @0x82423558 UpdateAptVariables ---------------------------------------------------
// Push the latched selection snapshot (raised by SetDirty) out to the apt movie: the
// panel's own timeline label, the branch container's label + Y position, the two scroll
// arrows' visibility and the list title. One-shot -- it consumes and clears mbDirty.
//
// Every value read here is the SNAPSHOT half of the pair (+0x4108..+0x411A), never the
// live cursor: meCurrentState/meBranchState/meListType/miCurrentlyHighlightedSlot are re-read from
// meDirtyCurrentState/meDirtyBranchState/meDirtyListType/miDirtyCurrentlyHighlightedSlot. Do not "simplify" to the live
// members -- the snapshot is what SetDirty froze at the moment of the change.
void FriendsListComponent::UpdateAptVariables()
{
    if (!mbDirty)                                                // +0x4118 @0x82423568
        return;

    const s32 leListType = static_cast<s32>(meDirtyListType);        // +0x4110 @0x82423578
    mbDirty = 0;                                                 // @0x82423580

    const char* lpcStringIDToUse;
    if (leListType == 3)                                         // challenges @0x8242357C
    {
        // The party player count goes into a "%d" scratch string, which is then printed
        // into the "EASY_DRIVE_CHALLENGES" loc-string's %1 marker and published as the
        // dynamic string "FRIEND_LIST_TITLE".
        char lacText[64];
        CgsCore::SPrintf(lacText, sizeof(lacText), "%d",
                         mpGuiCache->GetOnlinePlayerCount());      // cache +0xAC74 @0x824235A8
        lacText[sizeof(lacText) - 1] = 0;                        // @0x824235AC

        const char* const lkpcStringID = KAPC_LIST_TITLES[leListType];   // @0x824235D0
        mpStateInterface->GetLanguageManager()->FormatAndAddText(
            KPC_FRIEND_LIST_TITLE_ID,
            lkpcStringID,
            CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,    // r6 == 9
            1,                                                   // r7 == one positional parameter
            lacText,
            CgsLanguage::LanguageManager::E_FORMAT_INTEGER);     // r9 == 11
        lpcStringIDToUse = "$FRIEND_LIST_TITLE";           // @0x824235F8
    }
    else
    {
        lpcStringIDToUse = KAPC_LIST_TITLES[leListType];         // @0x8242360C
    }

    mAptRef.GotoAndPlayLabel(KAPC_PANEL_STATE_FRAMES[meDirtyCurrentState]);          // @0x82423628

    // (s8)miDirtyCurrentlyHighlightedSlot is the snapshot of miCurrentlyHighlightedSlot; the X360 sign-extends
    // the byte, converts it to double, then does a single fmsubs against the two .rdata
    // floats. SetPositionY takes a broadcast VecFloat (the console's vspltw of lane 0).
    const f32 lfPosition = static_cast<f32>(static_cast<s8>(miDirtyCurrentlyHighlightedSlot)) * KF_BRANCH_ROW_PITCH
                         - KF_BRANCH_ROW_ORIGIN;                             // @0x8242363C..0x82423664
    const VecFloat lvfPosition(lfPosition);
    mBranchRef.SetPositionY(lvfPosition);                                   // @0x82423670
    mBranchRef.GotoAndPlayLabel(KAPC_BRANCH_STATE_FRAMES[meDirtyBranchState]);     // @0x8242368C

    // The 0..3 scroll indicator SetDirty computed: bit 2 == an entry above the window,
    // bit 1 == an entry below it. Anything outside 1..3 hides both arrows.
    const s32 liIndicator = static_cast<s32>(meDirtyArrowConfiguration);             // +0x4114 @0x82423690
    const bool lbShowUp   = (liIndicator == KI_SCROLL_INDICATOR_UP_ONLY ||
                             liIndicator == KI_SCROLL_INDICATOR_BOTH);
    const bool lbShowDown = (liIndicator == KI_SCROLL_INDICATOR_DOWN_ONLY ||
                             liIndicator == KI_SCROLL_INDICATOR_BOTH);
    mUpArrowRef.SetVisible(lbShowUp);                                       // +0xBD0 @0x824236B4/CC/DC
    mDownArrowRef.SetVisible(lbShowDown);                                   // +0xBD8 @0x824236E8

    mListTitleTextfield.SetText(lpcStringIDToUse, false);                        // +0xBBC @0x824236F8
}

// @0x8243FF68 ShowFriendsList ------------------------------------------------------
// Re-open the panel on the FRIENDS list: park the cursor at the top of the list, latch
// the selection snapshot, post the "friends list shown" out-event, ask for fresh friend
// data and re-run the apt bind.
//
// The console INLINED SetDirty here: the seven stores at 0x8243FFA4..0x8243FFC4 are
// exactly SetDirty's, constant-folded against the cursor this function just wrote
//   (+0x4118 = 1, +0x4108 = meCurrentState(1), +0x410C = meBranchState(0),
//    +0x4119 = miCurrentlyActiveSlots (untouched here), +0x411A = miCurrentlyHighlightedSlot(0),
//    +0x4110 = meListType(1), +0x4114 = (miTotalOptions - 1 > 0) ? 1 : 0)
// -- and that last term is precisely SetDirty's indicator for a selected index of 0
// (base 0, `0 >= miTotalOptions - 1 ? 0 : 1`). Restored as the real call per house rule.
void FriendsListComponent::ShowFriendsList()
{
    miCurrentlyHighlightedSlot = 0;                                     // @0x8243FF8C
    miCurrentlyHighlightedOption = 0;                                        // @0x8243FF90
    meListType = E_LIST_TYPE_FRIENDS;                                              // @0x8243FF98 (friends)
    meCurrentState = 1;                                            // @0x8243FF9C (opening)
    meBranchState = E_FRIENDLISTBRANCH_INVISIBLE;                // @0x8243FFA0

    SetDirty();                                                  // inlined @0x8243FFA4..C4

    // DWARF local lFriendListShowingEvent (cpp:1993): GuiEventFriendListShowing (X360 id 94),
    // posted through the inlined OutputGuiEvent (@0x8243FFF8).
    GuiEventFriendListShowing lFriendListShowingEvent;
    lFriendListShowingEvent.mbNowShowing = true;
    mpStateInterface->OutputGuiEvent(lFriendListShowingEvent);

    RequestRefreshedData();                                      // @0x82440000

    Invalidate();
}

// @0x82441C90 ShowSpecificShortcut --------------------------------------------------
// Re-open the panel on the SHORTCUTS list with one named option pre-selected. The gate
// is the ShowSpecificFriend gate plus "the panel is closed": only the offline (-1) and
// online-lobby (15) game modes may re-open it.
void FriendsListComponent::ShowSpecificShortcut(EShortcutOptions leShortcutOption)
{
    const s32 leMode = mpGuiCache->GetGameMode();                // cache +0x9E58 @0x82441CB8
    if (leMode != -1 && leMode != 15)                            // @0x82441CBC..C8
        return;
    if (meCurrentState != 0)                                       // @0x82441CCC..D4
        return;

    ShowShortcutList();

    // DWARF local liIndex (cpp:3744); as ShowSpecificChallenge: the loop re-reads miTotalOptions
    // (ARTIST 0x82441D04), the option is the left operand (`cmpw r30, r7` 0x82441CFC).
    int32_t liIndex;
    for (liIndex = 0; liIndex < miTotalOptions; ++liIndex)
    {
        if (leShortcutOption == maeAvailableShortcutOptions[liIndex])
            break;
    }

    // The found arm is the fall-through (ARTIST 0x82441D1C `beq` to the not-found stores placed last).
    if (liIndex != miTotalOptions)
    {
        miCurrentlyHighlightedOption = static_cast<s8>(liIndex);             // extsb+stb @0x82441D20/28
        if (miTotalOptions <= KI_VISIBLE_ROWS)
        {
            miCurrentlyHighlightedSlot = miCurrentlyHighlightedOption;              // @0x82441D48
        }
        else
        {
            // DWARF local liSlotsUsedAfterCurrent (cpp:3763); the console re-sign-extends the
            // STORED byte before this subtract (`extsb r11, r11` @0x82441D30).
            int32_t liSlotsUsedAfterCurrent = miTotalOptions - miCurrentlyHighlightedOption - 1;   // @0x82441D34/38
            if (liSlotsUsedAfterCurrent >= KI_VISIBLE_ROWS)                 // @0x82441D3C/40
                miCurrentlyHighlightedSlot = 0;                         // @0x82441D54
            else
                miCurrentlyHighlightedSlot = static_cast<s8>(4 - liSlotsUsedAfterCurrent);    // subfic @0x82441D44
        }
    }
    else
    {
        miCurrentlyHighlightedOption = 0;                                    // @0x82441D50
        miCurrentlyHighlightedSlot = 0;                                 // @0x82441D54
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
void FriendsListComponent::ShowSpecificChallenge(CgsID lChallengeID)
{
    const s32 leMode = mpGuiCache->GetGameMode();                // cache +0x9E58 @0x82441BA0
    if (leMode != -1 && leMode != 15)                            // @0x82441BA4..B0
        return;
    if (meCurrentState != 0)                                       // @0x82441BB4..BC
        return;
    if (mpGuiCache->GetOnlinePlayerCount() <= 1)                  // cache +0xAC74 @0x82441BC8..D0
        return;

    ShowChallengesList();

    // DWARF local liIndex (cpp:3621); the loop re-reads miTotalOptions (ARTIST 0x82441C00) and the
    // id is the left operand (`cmpld r30, r7` 0x82441BF8). `ld r7, 0(r10)` from this+0x70, stride 8:
    // ONE 64-bit id per slot (Hex-Rays renders the `*(v8 + 4)` low half a 32-bit ABI would use).
    int32_t liIndex;
    for (liIndex = 0; liIndex < miTotalOptions; ++liIndex)
    {
        if (lChallengeID == maChallengesAvailable[liIndex])
            break;
    }

    // The found arm is the fall-through (ARTIST 0x82441C18 `beq` to the not-found stores placed last).
    if (liIndex != miTotalOptions)
    {
        miCurrentlyHighlightedOption = static_cast<s8>(liIndex);             // extsb+stb @0x82441C1C/24
        if (miTotalOptions <= KI_VISIBLE_ROWS)
        {
            miCurrentlyHighlightedSlot = miCurrentlyHighlightedOption;              // @0x82441C44
        }
        else
        {
            // DWARF local liSlotsUsedAfterCurrent (cpp:3640).
            int32_t liSlotsUsedAfterCurrent = miTotalOptions - miCurrentlyHighlightedOption - 1;   // @0x82441C2C..34
            if (liSlotsUsedAfterCurrent >= KI_VISIBLE_ROWS)                 // @0x82441C38/3C
                miCurrentlyHighlightedSlot = 0;                         // @0x82441C50
            else
                miCurrentlyHighlightedSlot = static_cast<s8>(4 - liSlotsUsedAfterCurrent);    // subfic @0x82441C40
        }
    }
    else
    {
        miCurrentlyHighlightedOption = 0;                                    // @0x82441C4C
        miCurrentlyHighlightedSlot = 0;                                 // @0x82441C50
    }

    UpdateAllChallengesEntryData();

    // `lbz +0x882 ; extsb ; addi 14 ; slwi 3 ; ldx ; std qword_82FB27E8` -- the cursor is
    // re-read from memory and (idx + 14) * 8 is &maChallengesAvailable[idx] (+0x70 == 14 * 8).
    sStoredChallenge = maChallengesAvailable[miCurrentlyHighlightedOption];               // @0x82441C5C..74
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
        clip = lFile.FindComponent(name);
        clip.SetVisible(false);
        parent = clip.GetParent();
        mBranchTextfields[i] = parent.FindChildTextField(KAPC_BRANCH_TEXT_FIELDS[i]);
    }
    for (s32 i = 0; i < KI_VISIBLE_ROWS; ++i)
    {
        char name[32];
        CgsCore::SPrintf(name, sizeof(name), "Entry%i", i);
        maEntries[i].Prepare(name, lFile, lacName);
    }
    BrnFlapt::MovieClipRef clip;
    clip = mAptRef.FindChildMovieClip("listTitle_mc");
    mListTitleTextfield = clip.FindChildTextField("listTitle_txt");
    mBranchRef = mAptRef.FindChildMovieClip("branch_mc");
    clip = mAptRef.FindChildMovieClip("upArrow_mc");
    mUpArrowRef = clip.FindChildMovieClip("arrow");
    clip = mAptRef.FindChildMovieClip("downArrow_mc");
    mDownArrowRef = clip.FindChildMovieClip("arrow");
}

// ARTIST 0x8242B440.
void FriendsListComponent::Invalidate()
{
    // DWARF cpp:2335 `int8_t liCurrentIndex` (ARTIST 0x8242B470 extsb).
    for (s8 liCurrentIndex = 0; liCurrentIndex < KI_VISIBLE_ROWS; ++liCurrentIndex)
        maEntries[liCurrentIndex].Invalidate();
}

// ARTIST 0x8242B628. +0x880 is the number of populated visible rows, despite
// the earlier consumer-only member name miCurrentlyActiveSlots.
void FriendsListComponent::UpdateAllShortcutsEntryData()
{
    BuildShortcutOptions();
    miCurrentlyActiveSlots = 0;
    s32 first = miCurrentlyHighlightedOption - miCurrentlyHighlightedSlot;
    if (miTotalOptions >= KI_VISIBLE_ROWS)
    {
        const s32 excess = first - static_cast<s32>(miTotalOptions) + KI_VISIBLE_ROWS;
        if (excess > 0)
        {
            first -= excess;
            miCurrentlyHighlightedOption -= static_cast<s8>(excess);
        }
    }
    for (s32 i = 0; i < KI_VISIBLE_ROWS; ++i)
    {
        if (i >= static_cast<s32>(miTotalOptions))
            maEntries[i].Invalidate();
        else
        {
            ++miCurrentlyActiveSlots;
            const s32 option = maeAvailableShortcutOptions[first + i];
            CGS_ASSERT_AT(option != E_SHORTCUT_OPTIONS_COUNT, "Trying to display an invalid option", CGS_ARTIST_FILE, 2768);
            const bool disabled = !CgsSystem::HardwareInit::IsHardDiskAvailable()
                               && option >= 2 && option <= 6;
            SetEntryData(i, KAPC_SHORTCUT_TEXT[option], disabled ? 3 : 17, true);
        }
    }
    while (miCurrentlyHighlightedSlot >= miCurrentlyActiveSlots)
    {
        --miCurrentlyHighlightedSlot;
        --miCurrentlyHighlightedOption;
    }
    Highlight(miCurrentlyHighlightedSlot);
    SetDirty();
}

namespace
{
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
    miCurrentlyHighlightedSlot   = 0;
    miCurrentlyHighlightedOption = 0;
    meListType = E_LIST_TYPE_SHORTCUTS;
    meCurrentState = 1;
    meBranchState = 0;
    SetDirty();
    GuiEventFriendListShowing lFriendListShowingEvent;   // DWARF local (e.g. cpp:2069)
    lFriendListShowingEvent.mbNowShowing = true;
    mpStateInterface->OutputGuiEvent(lFriendListShowingEvent);
    UpdateAllEntryData();
    if (!CgsSystem::HardwareInit::IsHardDiskAvailable() && !s_sbHddOverlayShown)
    {
        GuiOverlayRequest event;
        event.Construct(CgsIDCompress("OnReqHDD"));   // ARTIST 0x8244189C: CgsIDCompress, then the inline id/count/flags stores
        mpStateInterface->OutputGuiEvent(event);
        s_sbHddOverlayShown = true;
    }
}

// ARTIST 0x824418D0.
void FriendsListComponent::ShowChallengesList()
{
    miCurrentlyHighlightedSlot   = 0;
    miCurrentlyHighlightedOption = 0;
    meListType = E_LIST_TYPE_CHALLENGES;
    meCurrentState = 1;
    meBranchState = 0;
    SetDirty();
    GuiEventFriendListShowing lFriendListShowingEvent;   // DWARF local (e.g. cpp:2069)
    lFriendListShowingEvent.mbNowShowing = true;
    mpStateInterface->OutputGuiEvent(lFriendListShowingEvent);
    UpdateAllEntryData();
}

// ARTIST 0x82440020.
void FriendsListComponent::UpdateAllEntryData()
{
    switch (meListType)
    {
    case E_LIST_TYPE_FRIENDS: UpdateAllFriendsEntryData(); break;
    case E_LIST_TYPE_SHORTCUTS: UpdateAllShortcutsEntryData(); break;
    case E_LIST_TYPE_CHALLENGES: UpdateAllChallengesEntryData(); break;
    default: CGS_FAIL_AT("Unknown list type\n", CGS_ARTIST_FILE, 2656 /* ARTIST 0x82440060 */); break;
    }
}

// ARTIST 0x824397E8.
void FriendsListComponent::Close()
{
    if (meCurrentState == 0) return;
    if (meListType == E_LIST_TYPE_CHALLENGES)
    {
        CgsID lChallengeID = maChallengesAvailable[miCurrentlyHighlightedOption];
        GuiChallengeSelectedEvent lChallengeSelected;
        lChallengeSelected.mChallengeID = lChallengeID;
        lChallengeSelected.meFreeburnChallengeStyle = static_cast<s32>(
            mpGuiCache->GetFreeburnChallengeList()->GetChallengeData(lChallengeID)->GetChallengeStyle());
        lChallengeSelected.meAction = GuiChallengeSelectedEvent::E_ACTION_HIDDEN;
        mpStateInterface->OutputGuiEvent(lChallengeSelected);
        lChallengeSelected.meAction = GuiChallengeSelectedEvent::E_ACTION_CANCELED;
        mpStateInterface->OutputGuiEvent(lChallengeSelected);
    }
    meCurrentState = 5;
    SetDirty();
    Invalidate();
}

// ARTIST 0x82442868.
void FriendsListComponent::HandleDPadLeft()
{
    // ARTIST 0x82442880..0x824428BC: the closed state is tested first, then the open state;
    // the other lists close in place (DWARF lists the inlined SetDirty here, not Close).
    if (meCurrentState == 0 || meCurrentState != 2) return;
    // A switch on the list type: ARTIST tests 1 and 3 and runs the default (close in place)
    // inline, the challenge case after it and the shared ShowShortcutList tail last.
    switch (meListType)
    {
    case E_LIST_TYPE_FRIENDS:
        ShowShortcutList();
        break;
    case E_LIST_TYPE_CHALLENGES:
    {
        CgsID lChallengeID = maChallengesAvailable[miCurrentlyHighlightedOption];
        GuiChallengeSelectedEvent lChallengeSelected;
        lChallengeSelected.mChallengeID = lChallengeID;
        lChallengeSelected.meFreeburnChallengeStyle = static_cast<s32>(
            mpGuiCache->GetFreeburnChallengeList()->GetChallengeData(lChallengeID)->GetChallengeStyle());
        lChallengeSelected.meAction = GuiChallengeSelectedEvent::E_ACTION_HIDDEN;
        mpStateInterface->OutputGuiEvent(lChallengeSelected);
        lChallengeSelected.meAction = GuiChallengeSelectedEvent::E_ACTION_CANCELED;
        mpStateInterface->OutputGuiEvent(lChallengeSelected);
        ShowShortcutList();
        break;
    }
    default:
        meCurrentState = 5;
        SetDirty();
        Invalidate();
        break;
    }
}

// ARTIST 0x82441988.
bool FriendsListComponent::SelectPrevious()
{
    if (miCurrentlyHighlightedOption == 0 || miTotalOptions == 0) return false;
    if (miCurrentlyHighlightedSlot > 0)
    {
        --miCurrentlyHighlightedOption;
        --miCurrentlyHighlightedSlot;
        Highlight(miCurrentlyHighlightedSlot);
        if (meListType == E_LIST_TYPE_CHALLENGES)
            {
                CgsID lChallengeID = maChallengesAvailable[miCurrentlyHighlightedOption];
                GuiChallengeSelectedEvent lChallengeSelected;
                lChallengeSelected.mChallengeID = lChallengeID;
                lChallengeSelected.meAction = GuiChallengeSelectedEvent::E_ACTION_SHOWN;
                lChallengeSelected.meFreeburnChallengeStyle = static_cast<s32>(
                    mpGuiCache->GetFreeburnChallengeList()->GetChallengeData(lChallengeID)->GetChallengeStyle());
                mpStateInterface->OutputGuiEvent(lChallengeSelected);
            }
        return true;
    }
    if (miCurrentlyHighlightedSlot == 0 && miCurrentlyHighlightedOption > 0)
    {
        --miCurrentlyHighlightedOption;
        UpdateAllEntryData();
        return true;
    }
    return false;
}

// ARTIST 0x82441A80.
bool FriendsListComponent::SelectNext()
{
    // The last-option test comes first (ARTIST 0x82441AA8, then the empty-list test 0x82441AB0).
    if (static_cast<s32>(miTotalOptions) - 1 == miCurrentlyHighlightedOption || miTotalOptions == 0) return false;
    if (miCurrentlyHighlightedSlot < 4)
    {
        ++miCurrentlyHighlightedOption;
        ++miCurrentlyHighlightedSlot;
        Highlight(miCurrentlyHighlightedSlot);
        if (meListType == E_LIST_TYPE_CHALLENGES)
            {
                CgsID lChallengeID = maChallengesAvailable[miCurrentlyHighlightedOption];
                GuiChallengeSelectedEvent lChallengeSelected;
                lChallengeSelected.mChallengeID = lChallengeID;
                lChallengeSelected.meAction = GuiChallengeSelectedEvent::E_ACTION_SHOWN;
                lChallengeSelected.meFreeburnChallengeStyle = static_cast<s32>(
                    mpGuiCache->GetFreeburnChallengeList()->GetChallengeData(lChallengeID)->GetChallengeStyle());
                mpStateInterface->OutputGuiEvent(lChallengeSelected);
            }
        return true;
    }
    if (miCurrentlyHighlightedSlot == 4 && miCurrentlyHighlightedOption < static_cast<s32>(miTotalOptions) - 1)
    {
        ++miCurrentlyHighlightedOption;
        UpdateAllEntryData();
        return true;
    }
    return false;
}

// ARTIST 0x82442D98.
void FriendsListComponent::HandleDPadRight()
{
    if (meCurrentState == 0) { ShowShortcutList(); return; }
    switch (meListType)
    {
    case E_LIST_TYPE_FRIENDS: HandleDPadRightFriends(); break;
    case E_LIST_TYPE_SHORTCUTS: HandleDPadRightShortcuts(); break;
    case E_LIST_TYPE_CHALLENGES: HandleDPadRightChallenges(); break;
    default: CGS_FAIL_AT("unknown list type\n", CGS_ARTIST_FILE, 1066 /* ARTIST 0x82442DF8 */); break;
    }
}

// ARTIST 0x82442E50. Input action IDs are the original GUI D-pad actions.
void FriendsListComponent::HandleTableInteraction(s32 liAction)
{
    switch (liAction)
    {
    case E_GAMEINPUTACTIONS_GUI_DPAD_UP:
        if (meCurrentState == 2)
        {
            if (SelectPrevious())
            {
                SetDirty();
                GuiAudioTriggerEvent lEvent;
                lEvent.Construct(2, "", "EasyDriveEntry", "");
                mpStateInterface->OutputGuiEvent(lEvent);
            }
            if (meListType == E_LIST_TYPE_CHALLENGES)
                sStoredChallenge = maChallengesAvailable[miCurrentlyHighlightedOption];
        }
        break;
    case E_GAMEINPUTACTIONS_GUI_DPAD_DOWN:
        if (meCurrentState == 2)
        {
            if (SelectNext())
            {
                SetDirty();
                GuiAudioTriggerEvent lEvent;
                lEvent.Construct(2, "", "EasyDriveEntry", "");
                mpStateInterface->OutputGuiEvent(lEvent);
            }
            if (meListType == E_LIST_TYPE_CHALLENGES)
                sStoredChallenge = maChallengesAvailable[miCurrentlyHighlightedOption];
        }
        break;
    case E_GAMEINPUTACTIONS_GUI_DPAD_LEFT:
        HandleDPadLeft();
        break;
    case E_GAMEINPUTACTIONS_GUI_DPAD_RIGHT:
        HandleDPadRight();
        break;
    }
}

// ARTIST 0x82443280.
void FriendsListComponent::HandleControllerInput(const s32* input)
{
    CGS_ASSERT_AT(input != 0, "lpControllerInput", CGS_ARTIST_FILE, 849 /* ARTIST 0x824432B8 */);
    CGS_ASSERT_AT(mpGuiCache != 0, "mpGuiCache", CGS_ARTIST_FILE, 856 /* ARTIST 0x824432E0 */);
    // Host diagnostic: one record per GUI press, disabled in normal play.
    if (!mpGuiCache->GetGameplayHudReady() || (meListType == E_LIST_TYPE_FRIENDS && meDataCollectionState != 2)) return;
    if (meCurrentState == 2 && meBranchState != 0) HandleBranchInteraction(input[1]);
    else HandleTableInteraction(input[1]);
}

// ARTIST 0x82442C78.
void FriendsListComponent::Update()
{
    if (mbTriggerHideFriendsList)
    {
        GuiEventFriendListShowing lFriendListShowingEvent;   // DWARF local (e.g. cpp:2069)
        lFriendListShowingEvent.mbNowShowing = false;
        mpStateInterface->OutputGuiEvent(lFriendListShowingEvent);
        mbTriggerHideFriendsList = false;
    }
    for (s32 i = 0; i < KI_VISIBLE_ROWS; ++i) maEntries[i].Update();
    const s32 count = mpGuiCache->GetOnlinePlayerCount();
    if (count != static_cast<s32>(miPlayerCount))
    {
        if (meCurrentState != 0)
        {
            if (meListType == E_LIST_TYPE_SHORTCUTS && (count <= 1 || miPlayerCount <= 1)) ShowShortcutList();
            else if (meListType == E_LIST_TYPE_CHALLENGES)
            {
                if (count > 1) ShowChallengesList();
                else HandleDPadLeft();
            }
        }
        miPlayerCount = count;
    }
}

// ARTIST 0x82441E78.
void FriendsListComponent::ReshowShortcuts()
{
    if (meCurrentState == 0 || meListType != E_LIST_TYPE_SHORTCUTS) return;

    // DWARF locals leShortcutOption (cpp:3922), liIndex (cpp:3926), liSlotsUsedAfterCurrent
    // (cpp:3945): ShowSpecificShortcut's search over the rebuilt list (ARTIST 0x82441ED8..0x82441F34).
    EShortcutOptions leShortcutOption = maeAvailableShortcutOptions[miCurrentlyHighlightedOption];
    ShowShortcutList();

    int32_t liIndex;
    for (liIndex = 0; liIndex < miTotalOptions; ++liIndex)
    {
        if (leShortcutOption == maeAvailableShortcutOptions[liIndex])
            break;
    }

    if (liIndex != miTotalOptions)
    {
        miCurrentlyHighlightedOption = static_cast<s8>(liIndex);
        if (miTotalOptions <= KI_VISIBLE_ROWS)
        {
            miCurrentlyHighlightedSlot = miCurrentlyHighlightedOption;
        }
        else
        {
            int32_t liSlotsUsedAfterCurrent = miTotalOptions - miCurrentlyHighlightedOption - 1;
            if (liSlotsUsedAfterCurrent >= KI_VISIBLE_ROWS)
                miCurrentlyHighlightedSlot = 0;
            else
                miCurrentlyHighlightedSlot = static_cast<s8>(4 - liSlotsUsedAfterCurrent);
        }
    }
    else
    {
        miCurrentlyHighlightedOption = 0;
        miCurrentlyHighlightedSlot = 0;
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
    if (meCurrentState != 0 && miTotalOptions == 0 && !mpGuiCache->IsOnlineStartInProgress())
    {
        meBranchState = 0;
        miCurrentlyHighlightedSlot = 1;
        miCurrentlyActiveSlots = 2;
        miCurrentlyHighlightedOption = 1;
        SetEntryData(0, "FRIENDSLIST_NO_FRIENDS", 2, true);
        SetEntryData(1, "FRIENDSLIST_INSTANT_FREEBURN", 6, true);
        Highlight(miCurrentlyHighlightedSlot);
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
    CGS_ASSERT_AT(miCurrentlyHighlightedOption < E_SHORTCUT_OPTIONS_COUNT, "miCurrentlyHighlightedOption < E_SHORTCUT_OPTIONS_COUNT", CGS_ARTIST_FILE, 1835);
    const s32 option = maeAvailableShortcutOptions[miCurrentlyHighlightedOption];
    CGS_ASSERT_AT(option < E_SHORTCUT_OPTIONS_COUNT, "maeAvailableShortcutOptions[ miCurrentlyHighlightedOption ] < E_SHORTCUT_OPTIONS_COUNT", CGS_ARTIST_FILE, 1836);
    s32 count = 0;
    s32 first = -1;
    switch (option)
    {
    case 1: first = 0; break;
    case 2: first = 3; break;
    case 5: first = 6; break;
    case 6: first = 9; break;
    case 19:
        CGS_ASSERT(mpGuiCache->GetOnlineGameModeOptions()->meSecurity >= 0 && mpGuiCache->GetOnlineGameModeOptions()->meSecurity < 3, "Unknown security value");
        count = 2;
        maeAvailableShortcutBranchOptions[0] = mpGuiCache->GetOnlineGameModeOptions()->meSecurity == 0 ? 13 : 12;
        maeAvailableShortcutBranchOptions[1] = mpGuiCache->GetOnlineGameModeOptions()->meSecurity == 2 ? 13 : 14;
        break;
    default: CGS_FAIL_AT("Shortcut option doesn't have branches", CGS_ARTIST_FILE, 1952); break;
    }
    if (first >= 0)
    {
        count = 3;
        for (s32 i = 0; i < 3; ++i) maeAvailableShortcutBranchOptions[i] = first + i;
    }
    CGS_ASSERT(count >= 1 && count <= 3, "Invalid number of shortcut branches");
    meBranchState = count * 2 - 1;
    SetDirty();
    meBranchState = count == 1 ? 7 : count == 2 ? 8 : 10;
    mCurrentlySelectedName.macName[0] = 0;
    for (s32 i = 0; i < 3; ++i)
        mBranchTextfields[i].SetText(i < count ? KAPC_SHORTCUT_BRANCH_TEXT[maeAvailableShortcutBranchOptions[i]] : "", false);
    miCurrentlyHighlightedBranch = 0;
}

// ARTIST 0x82439040.
void FriendsListComponent::HandleBranchInteraction(s32 action)
{
    CGS_ASSERT_AT(meBranchState != 0, "E_FRIENDLISTBRANCH_INVISIBLE != meBranchState", CGS_ARTIST_FILE, 2109);
    if (action == 37 && (meBranchState == 9 || meBranchState == 11 || meBranchState == 12))
    {
        --meBranchState;
        --miCurrentlyHighlightedBranch;
        SetDirty();
        FriendsAudio(mpStateInterface);
    }
    else if (action == 38 && (meBranchState == 8 || meBranchState == 10 || meBranchState == 11))
    {
        ++meBranchState;
        ++miCurrentlyHighlightedBranch;
        SetDirty();
        FriendsAudio(mpStateInterface);
    }
    else if (action == 39) WithdrawBranches();
    else if (action == 40)
    {
        switch (meListType)
        {
        case E_LIST_TYPE_FRIENDS: HandleBranchDPadRightFriends(); break;
        case E_LIST_TYPE_SHORTCUTS: HandleBranchDPadRightShortcuts(); break;
        case E_LIST_TYPE_CHALLENGES: break;
        default: CGS_FAIL_AT("unknown list type", CGS_ARTIST_FILE, 2197); break;
        }
    }
    CGS_ASSERT_AT(miCurrentlyHighlightedBranch >= 0 && miCurrentlyHighlightedBranch < 3,
               "miCurrentlyHighlightedBranch in range", CGS_ARTIST_FILE, 2211);
}

// ARTIST 0x82438DC0.
void FriendsListComponent::HandleBranchDPadRightShortcuts()
{
    const s32 branch = maeAvailableShortcutBranchOptions[miCurrentlyHighlightedBranch];
    if (branch >= 0 && branch <= 2)
    {
        GuiEventRoadRuleModeRequest event = {};
        const s32 type = branch == 2 ? 1 : 0;
        memcpy(event.maData, &type, sizeof(type));
        event.maData[4] = branch != 0;
        mpStateInterface->OutputGuiEvent(event);
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
    else CGS_FAIL_AT("Unknown branch option", CGS_ARTIST_FILE, 1669);
    WithdrawBranches();
    meCurrentState = 5;
    SetDirty();
    Invalidate();
}

// ARTIST 0x82442938.
void FriendsListComponent::HandleDPadRightShortcuts()
{
    CGS_ASSERT_AT(miCurrentlyHighlightedOption < E_SHORTCUT_OPTIONS_COUNT, "miCurrentlyHighlightedOption < E_SHORTCUT_OPTIONS_COUNT", CGS_ARTIST_FILE, 1125);
    const s32 option = maeAvailableShortcutOptions[miCurrentlyHighlightedOption];
    CGS_ASSERT_AT(option < E_SHORTCUT_OPTIONS_COUNT, "maeAvailableShortcutOptions[ miCurrentlyHighlightedOption ] < E_SHORTCUT_OPTIONS_COUNT", CGS_ARTIST_FILE, 1126);
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
        mpStateInterface->OutputGuiEvent(event);
        break;
    }
    case 15:
        meCurrentState = 0;
        ShowSpecificChallenge(sStoredChallenge);
        return;
    default: CGS_FAIL_AT("Unknown shortcut option", CGS_ARTIST_FILE, 1313); break;
    }
    meCurrentState = 5;
    SetDirty();
    Invalidate();
}

// ARTIST 0x8242B498.
void FriendsListComponent::UpdateAllFriendsEntryData()
{
    // DWARF locals liStartFriend / liDifferenceToChange (cpp:2673/2674) and the loop's
    // liScreenEntryIndex (cpp:2690); the filled row is the tested arm (ARTIST 0x8242B508 `bge`
    // to the Invalidate placed last).
    int32_t liStartFriend;
    int32_t liDifferenceToChange;

    miCurrentlyActiveSlots = 0;
    liStartFriend = miCurrentlyHighlightedOption - miCurrentlyHighlightedSlot;
    if (miTotalOptions >= KI_VISIBLE_ROWS)
    {
        liDifferenceToChange = liStartFriend - static_cast<s32>(miTotalOptions) + KI_VISIBLE_ROWS;
        if (liDifferenceToChange > 0)
        {
            miCurrentlyHighlightedOption = static_cast<s8>(miCurrentlyHighlightedOption - liDifferenceToChange);
            liStartFriend -= liDifferenceToChange;
        }
    }
    for (int32_t liScreenEntryIndex = 0; liScreenEntryIndex < KI_VISIBLE_ROWS; ++liScreenEntryIndex)
    {
        if (liScreenEntryIndex < static_cast<s32>(miTotalOptions))
        {
            ++miCurrentlyActiveSlots;
            SetEntryData(static_cast<s8>(liScreenEntryIndex),
                         maFullBuddyList[liStartFriend + liScreenEntryIndex].mPlayerName.macName,
                         maeFullBuddyEntryState[liStartFriend + liScreenEntryIndex], false);
        }
        else
        {
            maEntries[liScreenEntryIndex].Invalidate();
        }
    }
    while (miCurrentlyHighlightedSlot >= miCurrentlyActiveSlots) { --miCurrentlyHighlightedSlot; --miCurrentlyHighlightedOption; }
    Highlight(miCurrentlyHighlightedSlot);
    SetDirty();
}

// ARTIST 0x82423708.
void FriendsListComponent::SortFullList()
{
    // DWARF locals (cpp:3062..3065, 3076, 3135); remaining difference (97.1): the second loop's index
    // and the hoisted GuiCache+0x9E58 offset take r29/r28 where ARTIST has r28/r29.
    // the privilege test is the inlined
    // HasMultiplayerPrivileges (ARTIST 0x82423890..0x824238B8, assert cpp:2089).
    int32_t liCurrentIndex;
    int32_t liBuddyIndex;
    int32_t liPlayerIndex;
    SFriendRecord* lpBuddy;

    for (liBuddyIndex = 0; liBuddyIndex < miTotalOptions; ++liBuddyIndex)
    {
        lpBuddy = &maFullBuddyList[liBuddyIndex];
        lpBuddy->mbIsInSameLobby = false;
        if (lpBuddy->mbIsOnline)
        {
            for (liPlayerIndex = 0; liPlayerIndex < 8; ++liPlayerIndex)   // KI_MAX_PLAYERS (stride 0x138 to 0x9C0)
            {
                const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerStatusData =
                    mpGuiCache->GetOnlinePlayerInfoByIndex(liPlayerIndex);
                if (lpPlayerStatusData->mNetworkPlayerID != -1 &&
                    lpPlayerStatusData->mPlayerName.IsEqualTo(lpBuddy->mPlayerName))
                {
                    lpBuddy->mbIsInSameLobby = true;
                    break;
                }
            }
        }
    }

    qsort(maFullBuddyList, miTotalOptions, sizeof(SFriendRecord), &BuddySortFunction);

    for (liCurrentIndex = 0, lpBuddy = maFullBuddyList; liCurrentIndex < miTotalOptions; ++liCurrentIndex, ++lpBuddy)
    {
        if (!lpBuddy->mbIsOnline)
        {
            maeFullBuddyEntryState[liCurrentIndex] = FriendsListEntry::E_FRIENDLISTENTRYSTATE_FRIENDOFFLINE;
        }
        else if (lpBuddy->mbIsInSameLobby == true)                                  // cmplwi 1 @0x82423880
        {
            maeFullBuddyEntryState[liCurrentIndex] = FriendsListEntry::E_FRIENDLISTENTRYSTATE_FRIENDINLOBBY;
        }
        // Two arms with the same store (ARTIST 0x824238C8 and 0x824238E0, each its own `b`).
        else if (!HasMultiplayerPrivileges())
        {
            maeFullBuddyEntryState[liCurrentIndex] = FriendsListEntry::E_FRIENDLISTENTRYSTATE_NOMULTIPLAYERPRIVILEGE;
        }
        else if (!CgsSystem::HardwareInit::IsHardDiskAvailable())
        {
            maeFullBuddyEntryState[liCurrentIndex] = FriendsListEntry::E_FRIENDLISTENTRYSTATE_NOMULTIPLAYERPRIVILEGE;
        }
        else if (lpBuddy->miInviteStatus == 2)                                      // E_INVITED_LOCAL_PLAYER_TO_GAME
        {
            maeFullBuddyEntryState[liCurrentIndex] = FriendsListEntry::E_FRIENDLISTENTRYSTATE_FRIENDRECEIVED;
        }
        else if (lpBuddy->miInviteStatus == 1)                                      // E_INVITED_TO_LOCAL_PLAYERS_GAME
        {
            if (lpBuddy->mbIsJoinable == true)
                maeFullBuddyEntryState[liCurrentIndex] = FriendsListEntry::E_FRIENDLISTENTRYSTATE_FRIENDSENTJOINABLE;
            else
                maeFullBuddyEntryState[liCurrentIndex] = FriendsListEntry::E_FRIENDLISTENTRYSTATE_FRIENDSENTNOTJOINABLE;
        }
        else
        {
            // The lobby test is the header-inline GsmIO::IsOnlineFreeBurnLobby (ARTIST materialises
            // its bool, 0x82423930..0x82423948).
            BrnGameState::GameStateModuleIO::EGameModeType leGameMode = mpGuiCache->GetGameMode();
            if (leGameMode == BrnGameState::GameStateModuleIO::E_MODE_NONE ||
                (BrnGameState::GameStateModuleIO::IsOnlineFreeBurnLobby(leGameMode) &&
                 !mpGuiCache->GetOnlineGameModeOptions()->mbRanked))
            {
                if (lpBuddy->mbIsJoinable == true)
                    maeFullBuddyEntryState[liCurrentIndex] = FriendsListEntry::E_FRIENDLISTENTRYSTATE_FRIENDJOINABLE;
                else
                    maeFullBuddyEntryState[liCurrentIndex] = FriendsListEntry::E_FRIENDLISTENTRYSTATE_FRIENDNOTJOINABLE;
            }
            else
            {
                if (lpBuddy->mbIsJoinable == true)
                    maeFullBuddyEntryState[liCurrentIndex] = FriendsListEntry::E_FRIENDLISTENTRYSTATE_FRIENDNOTINVITABLEJOINABLE;
                else
                    maeFullBuddyEntryState[liCurrentIndex] = FriendsListEntry::E_FRIENDLISTENTRYSTATE_FRIENDNOTINVITABLENOTJOINABLE;
            }
        }
    }
}

// ARTIST 0x824430D0.
void FriendsListComponent::ProcessNewEntryData(const GuiEventOnlineReceiveFriendInfo* lpFriendInfo)
{
    CGS_ASSERT_AT(lpFriendInfo != 0, "lpFriendInfo", CGS_ARTIST_FILE, 706 /* ARTIST 0x82443104 */);
    if (meListType != E_LIST_TYPE_FRIENDS) return;
    // DWARF (BrnFriendsList.cpp:736/:779) names the locals liEventIndex and lbShouldAnimateOut; no local holds the event.
    if (!lpFriendInfo->mbConnected) { HandleNotConnected(); meDataCollectionState = 2; return; }
    if (lpFriendInfo->miNumberOfBuddiesInEvent == 0) { HandleNoFriends(); meDataCollectionState = 2; return; }
    for (s32 liEventIndex = 0; liEventIndex < lpFriendInfo->miNumberOfBuddiesInEvent; ++liEventIndex)
        maFullBuddyList[lpFriendInfo->miIndexOfFirstBuddyInFullBuddyList + liEventIndex] = lpFriendInfo->mBuddyInformation[liEventIndex];
    if (lpFriendInfo->miIndexOfFirstBuddyInFullBuddyList + lpFriendInfo->miNumberOfBuddiesInEvent < static_cast<s32>(miTotalOptions)) return;
    meDataCollectionState = 2;
    SortFullList();
    RemoveUnneededFriends();
    if (miTotalOptions == 0) { HandleNoFriends(); return; }
    bool lbShouldAnimateOut = false;
    if (meBranchState != 0)
        lbShouldAnimateOut = MoveHighlightDueToBranchOpen();
    UpdateAllEntryData();
    if (lbShouldAnimateOut) { meBranchState = 0; HandleTableInteraction(40); }
    else if (sbRestoreDataAvailable && seStoredListType == E_LIST_TYPE_FRIENDS)
    {
        ShowSpecificFriend(&sStoredFriend);
        sbRestoreDataAvailable = false;
        seStoredListType = E_LIST_TYPE_NONE;
    }
}

// ARTIST 0x82422E18.
void FriendsListComponent::ShowFriendsListBranch()
{
    s32 status;
    switch (mpGuiCache->muFriendsPanelBranchMirror)
    {
    case 0: status = maEntries[miCurrentlyHighlightedSlot].GetEntryStatus(); break;
    case 1: status = 14; break;
    case 2: status = 15; break;
    case 3: status = 16; break;
    case 4: status = 0; break;
    default: CGS_FAIL("Invalid case in switch"); status = maEntries[miCurrentlyHighlightedSlot].GetEntryStatus(); break;
    }
    if (!CgsSystem::HardwareInit::IsHardDiskAvailable() && status != 2) status = 3;
    s32 count = 0;
    for (s32 i = 0; i < 3; ++i) if (KAA_FRIEND_BRANCHES[status][i]) ++count;
    if (count == 0) return;
    meBranchState = count * 2 - 1;
    SetDirty();
    meBranchState = count == 1 ? 7 : count == 2 ? 8 : 10;
    mCurrentlySelectedName.Construct(maFullBuddyList[miCurrentlyHighlightedOption].mPlayerName.macName);
    for (s32 i = 0; i < 3; ++i) mBranchTextfields[i].SetText(KAPC_FRIEND_BRANCH_TEXT[KAA_FRIEND_BRANCHES[status][i]], false);
    miCurrentlyHighlightedBranch = 0;
}

// ARTIST 0x824386B0.
void FriendsListComponent::HandleDPadRightFriends()
{
    if (meBranchState != 0) return;
    if (miTotalOptions != 0 || miCurrentlyHighlightedSlot != 1) { ShowFriendsListBranch(); return; }
    if (mpGuiCache->muFriendsPanelBranchMirror != 0) return;
    GuiEventOnlineInviteEvent event = {};
    event.meRequestedAction = 6;
    mpStateInterface->OutputGuiEvent(event);
    mpGuiCache->muFriendsPanelBranchMirror = 4;
    miCurrentlyActiveSlots = 1;
    miCurrentlyHighlightedSlot   = 0;
    miCurrentlyHighlightedOption = 0;
    maEntries[1].Invalidate();
    SetDirty();
}

// ARTIST 0x82438938.
void FriendsListComponent::HandleBranchDPadRightFriends()
{
    s32 status;
    switch (mpGuiCache->muFriendsPanelBranchMirror)
    {
    case 0: status = maEntries[miCurrentlyHighlightedSlot].GetEntryStatus(); break;
    case 1: status = 14; break;
    case 2: status = 15; break;
    case 3: status = 16; break;
    case 4: status = 0; break;
    default: CGS_FAIL("Invalid case in switch"); status = maEntries[miCurrentlyHighlightedSlot].GetEntryStatus(); break;
    }
    const s32 branch = KAA_FRIEND_BRANCHES[status][miCurrentlyHighlightedBranch];
    bool close = false;
    if (branch == 3)
    {
        GuiEventOnlineShowProfile event;
        event.mFriendToView.Construct(maEntries[miCurrentlyHighlightedSlot].GetNameField().GetText());
        mpStateInterface->OutputGuiEvent(event);
    }
    else if (branch == 2 || branch == 4 || branch == 5 || branch == 6 || branch == 7 || branch == 9)
    {
        if (branch != 4 || mbCanSendInvites)
        {
            GuiEventOnlineInviteEvent event = {};
            event.mFriendToAddress.Construct(branch == 2 ? "" : maEntries[miCurrentlyHighlightedSlot].GetNameField().GetText());
            event.meRequestedAction = branch == 2 ? 5 : branch == 4 ? 0 : branch == 5 ? 4 : branch == 6 ? 1 : branch == 7 ? 2 : 3;
            if (branch == 6) event.mbHasOnlineGameBeenStarted = mpGuiCache->IsOnlineStartInProgress();
            mpStateInterface->OutputGuiEvent(event);
            if (branch == 4) { mpGuiCache->muFriendsPanelBranchMirror = 1; mpGuiCache->mbOnlineGameOptionsChanged = true; }
            else if (branch == 6) mpGuiCache->muFriendsPanelBranchMirror = 2;
            else if (branch == 9) mpGuiCache->muFriendsPanelBranchMirror = 3;
        }
        close = branch == 4;
    }
    else CGS_ASSERT(branch == 1 || branch == 8 || branch == 10 || branch == 11 || branch == 12, "Invalid branch option used");
    WithdrawBranches();
    if (close) { meCurrentState = 5; SetDirty(); Invalidate(); }
}

// ARTIST 0x82439350.
void FriendsListComponent::UpdateAllChallengesEntryData()
{
    BuildChallengeList();
    miCurrentlyActiveSlots = 0;
    s32 first = miCurrentlyHighlightedOption - miCurrentlyHighlightedSlot;
    if (miTotalOptions >= 5)
    {
        const s32 excess = first - static_cast<s32>(miTotalOptions) + 5;
        if (excess > 0) { first -= excess; miCurrentlyHighlightedOption -= static_cast<s8>(excess); }
    }
    const BrnResource::ChallengeList* list = mpGuiCache->GetFreeburnChallengeList();
    for (s32 i = 0; i < 5; ++i)
    {
        if (i >= static_cast<s32>(miTotalOptions)) maEntries[i].Invalidate();
        else
        {
            ++miCurrentlyActiveSlots;
            const BrnResource::ChallengeListEntry* entry = list->GetChallengeData(maChallengesAvailable[first + i]);
            const s32 index = list->GetChallengeIndex(entry->GetChallengeID());
            const BrnGameState::GameStateModuleIO::CompletedFburnChallengesData::CompletedFburnChallenges* completed = mpGuiCache->GetFreeburnChallengeManager()->GetCompletedChallengesData()->GetLocalPlayerCompletionStatus();
            const bool done = completed->IsBitSet(index);
            SetEntryData(i, entry->GetTitleStringID(), done ? 20 : 19, true);
            maEntries[i].SetIndexText(first + i + 1);
        }
    }
    while (miCurrentlyHighlightedSlot >= miCurrentlyActiveSlots) { --miCurrentlyHighlightedSlot; --miCurrentlyHighlightedOption; }
    Highlight(miCurrentlyHighlightedSlot);
    {
        CgsID lChallengeID = maChallengesAvailable[miCurrentlyHighlightedOption];
        GuiChallengeSelectedEvent lChallengeSelected;
        lChallengeSelected.mChallengeID = lChallengeID;
        lChallengeSelected.meAction = GuiChallengeSelectedEvent::E_ACTION_SHOWN;
        lChallengeSelected.meFreeburnChallengeStyle = static_cast<s32>(
            mpGuiCache->GetFreeburnChallengeList()->GetChallengeData(lChallengeID)->GetChallengeStyle());
        mpStateInterface->OutputGuiEvent(lChallengeSelected);
    }
    SetDirty();
}

// ARTIST 0x82438760.
void FriendsListComponent::HandleDPadRightChallenges()
{
    // DWARF locals lChallengeID / lChallengeSelected (cpp:1339/1341) and lGameOptions (cpp:1353).
    CgsID lChallengeID = maChallengesAvailable[miCurrentlyHighlightedOption];
    GuiChallengeSelectedEvent lChallengeSelected;
    lChallengeSelected.mChallengeID = lChallengeID;
    lChallengeSelected.meAction = GuiChallengeSelectedEvent::E_ACTION_CHOSEN;
    lChallengeSelected.meFreeburnChallengeStyle = static_cast<s32>(
        mpGuiCache->GetFreeburnChallengeList()->GetChallengeData(lChallengeID)->GetChallengeStyle());
    mpStateInterface->OutputGuiEvent(lChallengeSelected);
    lChallengeSelected.meAction = GuiChallengeSelectedEvent::E_ACTION_HIDDEN;
    mpStateInterface->OutputGuiEvent(lChallengeSelected);
    if (mpGuiCache->GetOnlineGameModeOptions()->meGameMode != 15)
    {
        GuiEventNetworkGameParams lGameOptions = *mpGuiCache->GetOnlineGameModeOptions();
        lGameOptions.meGameMode = 15;
        mpStateInterface->OutputGuiEvent(lGameOptions);
    }
    meCurrentState = 5;
    SetDirty();
    Invalidate();
}

} // namespace BrnGui

#undef CGS_ARTIST_FILE
