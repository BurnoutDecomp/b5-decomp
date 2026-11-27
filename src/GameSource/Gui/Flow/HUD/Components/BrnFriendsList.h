#pragma once

// ===================================================================================
// BrnGui::FriendsListComponent  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/HUD/Components/BrnFriendsList.h
//
// The in-HUD friends-list component (the X360 asserts reference this header:
// SetGuiCachePointer @0x82473580 -> BrnFriendsList.h:665). It tracks a "current
// selection" cursor (a highlighted entry + two flag bytes + a count) and, when that
// selection changes, latches a *dirty snapshot* of it into a separate region for the
// renderer/animation layer to pick up, alongside a scroll-arrow indicator computed from
// the selected index vs the entry count.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (stores authoritative on width / which
// field is copied):
//   SetGuiCachePointer @ 0x82473580 -- latch the GuiCache pointer (+0x4100) and cache a
//       far cache field into muCachedCacheField (+0x870). Asserts the pointer non-NULL.
//   SetDirty           @ 0x8241F120 -- snapshot the current selection (+0x874..+0x882)
//       into the dirty region (+0x4108..+0x411A), raise the dirty flag (+0x4118 = 1) and
//       store a scroll-arrow indicator (+0x4114) computed from the selected index.
//
// LAYOUT NOTE: this is a large component (the X360 object spans well past +0x16000); only
// the fields the two reconstructed methods touch are modelled, as NAMED members grouped
// by role. The exact inter-field padding / the unrecovered bulk between the groups is
// NOT reconstructed (no DWARF / reference entry for this TU), so the groups are modelled
// directly rather than byte-padded to the X360 offsets -- this is a host build where
// pointer widening already breaks byte-exactness; the contract here is semantic parity
// with member-by-name access (the established BrnGuiKeyboard / CrashNavIconRenderer
// convention). The X360 byte offsets are cited per field for provenance only.
// ===================================================================================

#include "types.hpp"
#include "GameSource/Network/SharedIO/BrnBuddyInformation.h"
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"   // [friends wave] branch labels/title
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"  // [friends wave] arrow clips
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"      // Prepare(const FileRef&)
#include "GameSource/GameState/BrnCgsPlayerName.h"
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h" // [friends wave] real base (DWARF h:49)     // [friends wave] mHighlightedName
#include "GameSource/Gui/BrnGuiCache.h"   // BrnGui::GuiCache (the cache pointer + far field)
#include "GameSource/Gui/Flow/HUD/Components/BrnFriendsListEntry.h" // [friends wave] maEntries[5]

namespace BrnGui
{
    class GuiEventOnlineReceiveFriendInfo;

    const s32 KI_MAX_FRIEND_RECORDS = 100;   // maeDisplayTypes capacity (SortFullList bound)
    const s32 KI_VISIBLE_ROWS       = 5;

    // [friends wave] base corrected: the X360 Construct stores the interface at +0x00
    // and invalidates the clip pair at +0x04/+0x08 -- the BrnFlaptComponent shape.
    class FriendsListComponent : public BrnFlaptComponent
    {
    public:
        // DWARF BrnFriendsList.h:117 (the asserts spell E_SHORTCUT_OPTIONS_COUNT).
        enum EShortcutOptions
        {
            E_SHORTCUT_OPTIONS_SHOW_FRIENDS     = 0,
            E_SHORTCUT_OPTIONS_ROAD_RULES       = 1,
            E_SHORTCUT_OPTIONS_FREEBURN         = 2,
            E_SHORTCUT_OPTIONS_IMAGE_GALLERY    = 3,
            E_SHORTCUT_OPTIONS_VIEW_CHALLENGES  = 4,
            E_SHORTCUT_OPTIONS_UNRANKED         = 5,
            E_SHORTCUT_OPTIONS_RANKED           = 6,
            E_SHORTCUT_OPTIONS_SCOREBOARDS      = 7,
            E_SHORTCUT_OPTIONS_NEWS             = 8,
            E_SHORTCUT_OPTIONS_LAUNCH           = 9,
            E_SHORTCUT_OPTIONS_CREATE_EVENT     = 10,
            E_SHORTCUT_OPTIONS_CHANGE_TEAMS     = 11,
            E_SHORTCUT_OPTIONS_VIEW_LOBBY       = 12,
            E_SHORTCUT_OPTIONS_LEAVE_GAME       = 13,
            E_SHORTCUT_OPTIONS_CANCEL_CHALLENGE = 14,
            E_SHORTCUT_OPTIONS_SELECT_CHALLENGE = 15,
            E_SHORTCUT_OPTIONS_VIEW_EVENT       = 16,
            E_SHORTCUT_OPTIONS_CANCEL_EVENT     = 17,
            E_SHORTCUT_OPTIONS_VIEW_MAP         = 18,
            E_SHORTCUT_OPTIONS_CHANGE_SECURITY  = 19,
            E_SHORTCUT_OPTIONS_SETTINGS         = 20,
            E_SHORTCUT_OPTIONS_COUNT            = 21,
        };

        // DWARF BrnFriendsList.h:306.
        enum EListType
        {
            E_LIST_TYPE_NONE       = 0,
            E_LIST_TYPE_FRIENDS    = 1,
            E_LIST_TYPE_SHORTCUTS  = 2,
            E_LIST_TYPE_CHALLENGES = 3,
            E_LIST_TYPE_COUNT      = 4,
        };

        // DWARF BrnFriendsList.h:13 -- friends-list branch/transition state; drives the
        // post-inc/post-dec operators (@0x82410958 / @0x824109B8) that step through it.
        enum EFriendListBranchState
        {
            E_FRIENDLISTBRANCH_INVISIBLE       = 0,
            E_FRIENDLISTBRANCH_ONE_IN          = 1,
            E_FRIENDLISTBRANCH_ONE_OUT         = 2,
            E_FRIENDLISTBRANCH_TWO_IN          = 3,
            E_FRIENDLISTBRANCH_TWO_OUT         = 4,
            E_FRIENDLISTBRANCH_THREE_IN        = 5,
            E_FRIENDLISTBRANCH_THREE_OUT       = 6,
            E_FRIENDLISTBRANCH_FIRST_OF_ONE    = 7,
            E_FRIENDLISTBRANCH_FIRST_OF_TWO    = 8,
            E_FRIENDLISTBRANCH_SECOND_OF_TWO   = 9,
            E_FRIENDLISTBRANCH_FIRST_OF_THREE  = 10,
            E_FRIENDLISTBRANCH_SECOND_OF_THREE = 11,
            E_FRIENDLISTBRANCH_THIRD_OF_THREE  = 12,
            E_FRIENDLISTBRANCH_COUNT           = 13,
        };

        // DWARF BrnFriendsList.h:115 -- the scroll-arrow indicator SetDirty snapshots.
        enum EArrowConfigurations
        {
            E_ARROWS_NONE      = 0,
            E_ARROWS_DOWN_ONLY = 1,
            E_ARROWS_UP_ONLY   = 2,
            E_ARROWS_BOTH      = 3,
            E_ARROWS_COUNT     = 4,
        };

        // @ 0x82473580 -- latch the (non-NULL) GuiCache pointer and cache its far field.
        // DWARF h:263 declares it void; the body is header-inline below (assert h:665).
        void SetGuiCachePointer(GuiCache* lpGuiCache);

        // DWARF h:494 -- the scroll-arrow indicator for the highlighted option (no ARTIST copy;
        // SetDirty inlines it, PS3 0x39B3C8).
        EArrowConfigurations GetArrowState();

        // @ 0x8241F120 -- snapshot the current selection into the dirty region, raise the
        // dirty flag, and store the scroll-arrow indicator (DWARF h:500, void).
        void SetDirty();
        // ---- [friends wave] remaining reconstructed surface ----
        void Construct(const char* lacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName);                                     // 0x82422B20
        void Prepare(const char* lacName, const BrnFlapt::FileRef& lFile);              // 0x8242B188
        void Invalidate();                                                              // 0x8242B440
        void Update();                                                                  // 0x82442C78
        void Close();                                                                   // 0x824397E8
        void EndWait();                                                                 // 0x82442FF0
        bool SelectPrevious();                                                          // 0x82441988
        bool SelectNext();                                                              // 0x82441A80
        void HandleControllerInput(const s32* lpiInput);                                // 0x82443280
        void ProcessNewEntryData(const GuiEventOnlineReceiveFriendInfo* lpFriendInfo);                             // 0x824430D0
        void RequestRefreshedData();                                                    // 0x82439248
        void SetTotalFriends(s32 liCount);                                              // 0x824392B8
        void AttemptStateRestore();                                                     // 0x82441D78
        void ReshowShortcuts();                                                         // 0x82441E78
        void ShowFriendsList();                                                         // 0x8243FF68
        void ShowShortcutList();                                                        // 0x824417E0
        void ShowChallengesList();                                                      // 0x824418D0
        void ShowSpecificFriend(CgsNetwork::PlayerName* lpName);                                 // 0x8242BBE8 (DWARF cpp:3674)
        void ShowSpecificShortcut(EShortcutOptions leShortcutOption);                   // 0x82441C90
        void ShowSpecificChallenge(CgsID lChallengeID);                                    // 0x82441B78
        void UpdateAllEntryData();                                                      // 0x82440020
        void UpdateAllFriendsEntryData();                                               // 0x8242B498
        void UpdateAllShortcutsEntryData();                                             // 0x8242B628
        void UpdateAllChallengesEntryData();                                            // 0x82439350
        void BuildChallengeList();                                                      // 0x8242B830
        void BuildShortcutOptions();                                                    // 0x82414288
        void SortFullList();                                                            // 0x82423708
        void RemoveUnneededFriends();                                                   // 0x824146C0
        bool MoveHighlightDueToBranchOpen();                                            // 0x82414868
        void SaveCurrentState();                                                        // 0x824149E8
        void UpdateAptVariables();                                                      // 0x82423558
        void WithdrawBranches();                                                        // 0x824234B8
        void ShowFriendsListBranch();                                                   // 0x82422E18
        void ShowShortcutsBranch();                                                     // 0x82423120
        void Highlight(s8 li8Row);                                                      // 0x82414960 (DWARF h: Highlight(int8_t))
        void SetEntryData(s8 liRow, const char* lpcText, s32 leStatus, bool lbLocalise); // 0x82422CB0 (DWARF int8_t index)
        bool HasMultiplayerPrivileges();                                                // DWARF h:588 (no ARTIST copy)
        void HandleNotConnected();                                                      // 0x8242B948
        void HandleNoFriends();                                                         // 0x8242BA40
        void HandleDPadLeft();                                                          // 0x82442868
        void HandleDPadRight();                                                         // 0x82442D98
        void HandleDPadRightFriends();                                                  // 0x824386B0
        void HandleDPadRightChallenges();                                               // 0x82438760
        void HandleDPadRightShortcuts();                                                // 0x82442938
        void HandleBranchDPadRightFriends();                                            // 0x82438938
        void HandleBranchDPadRightShortcuts();                                          // 0x82438DC0
        void HandleBranchInteraction(s32 liAction);                                     // 0x82439040
        void HandleTableInteraction(s32 liAction);                                      // 0x82442E50
        static void TransitionCompleteCallback(void* lpUserData);                       // 0x82414AA0
        static int  BuddySortFunction(const void* lpA, const void* lpB);                // 0x824147A8

    private:
        // ADDITIVE GRANT ([stuntrace wS2] wave, 2026-08-27) -- the same friend-grant pattern
        // BrnDistrictMarker.h and BrnRoadRuleComponent.h already use for FBurnMainHudState.
        // RaceMainHudState::UpdatePermenant @0x824806E8 pokes this component's second entry
        // flag byte INLINE, with no accessor anywhere in the ledger:
        //     0x82480B70  stb r11, 0x25F1(r29)   ; r29 == the state, 0x25F1 - 0x1D58 == +0x899
        //     0x82480B78  bl  FriendsListComponent::UpdateAptVariables
        // (r11 == 1 when meGameModeType is 15 or 16, else 0). Friendship rather than a
        // fabricated setter, because the byte's semantics are not recovered.
        friend struct RaceMainHudState;
        friend struct FBurnMainHudState;

        // ---- one online-friend record [friends wave]: stride 0x84, base +0xBEC ----
        typedef BrnNetwork::BrnNetworkModuleIO::BuddyInformation SFriendRecord;


    private:
        // ---- data members: DWARF BrnFriendsList.h:132 order and names (the X360 offsets each
        // one lands on are the ones the ARTIST bodies use; widths follow the DWARF types) ----
        EShortcutOptions maeAvailableShortcutOptions[E_SHORTCUT_OPTIONS_COUNT]; // +0x00C (DWARF h:383; sentinel COUNT)
        s32   maeAvailableShortcutBranchOptions[3];                 // +0x060 EShortcutBranchOptions[3] (Construct seeds 15)
        CgsID maChallengesAvailable[256];                           // +0x070 ((idx+14)*8 addressing)
        s32   miPlayerCount;                                        // +0x870 (SetGuiCachePointer: cache +0xAC74)
        EListType meListType;                                       // +0x874 (DWARF h:389)
        s32   meCurrentState;                                       // +0x878 EFriendListState: 0 invisible / 1 transin / 2 idle / 5 transout
        s32   meBranchState;                                        // +0x87C EFriendListBranchState
        s8    miCurrentlyActiveSlots;                               // +0x880
        s8    miCurrentlyHighlightedSlot;                           // +0x881
        s8    miCurrentlyHighlightedOption;                         // +0x882 (signed; drives the scroll indicator)
        s8    miCurrentlyHighlightedBranch;                         // +0x883
        CgsNetwork::PlayerName mCurrentlySelectedName;              // +0x884
        s32   miTotalOptions;                                       // +0x894
        bool  mbVisible;                                            // +0x898
        bool  mbCanSendInvites;                                     // +0x899 (poked inline by RaceMainHudState/FBurnMainHudState)
        s32   meDataCollectionState;                                // +0x89C EDataCollection: 0 wait-number / 1 wait-data / 2 running
        FriendsListEntry maEntries[KI_VISIBLE_ROWS];                // +0x8A0 (stride 0x98)
        BrnFlapt::TextFieldRef mBranchTextfields[3];                // +0xB98 ("branchOptionOne_txt" chain)
        BrnFlapt::TextFieldRef mListTitleTextfield;                 // +0xBBC ("listTitle_mc"/"listTitle_txt")
        // Prepare @0x8242B188 binds these in this order, which pins each one by its apt name:
        //   @0x8242B3A0  mAptRef.FindChildMovieClip("branch_mc")                        -> +0xBC8
        //   @0x8242B3D4/E4 mAptRef["upArrow_mc"].FindChildMovieClip("arrow")            -> +0xBD0
        //   @0x8242B410/20 mAptRef["downArrow_mc"].FindChildMovieClip("arrow")          -> +0xBD8
        // UpdateAptVariables @0x82423558 corroborates it: +0xBC8 takes the BRANCH-state frame
        // label and the highlighted branch row's Y position, +0xBD0/+0xBD8 are the two
        // SetVisible targets of the 0..3 arrow configuration.
        BrnFlapt::MovieClipRef mBranchRef;                          // +0xBC8 ("branch_mc")
        BrnFlapt::MovieClipRef mUpArrowRef;                         // +0xBD0 ("upArrow_mc"/"arrow")
        BrnFlapt::MovieClipRef mDownArrowRef;                       // +0xBD8 ("downArrow_mc"/"arrow")
        SFriendRecord maFullBuddyList[KI_MAX_FRIEND_RECORDS];       // +0xBE0 (stride 0x84; memset span 0x3390)
        s32   maeFullBuddyEntryState[KI_MAX_FRIEND_RECORDS];        // +0x3F70 FriendsListEntry::EFriendListEntryState
        GuiCache* mpGuiCache;                                       // +0x4100 (set by SetGuiCachePointer)
        bool  mbTriggerHideFriendsList;                             // +0x4104 (TransitionCompleteCallback)
        // ---- the dirty snapshot the renderer picks up (SetDirty) ----
        s32   meDirtyCurrentState;                                  // +0x4108
        s32   meDirtyBranchState;                                   // +0x410C
        EListType meDirtyListType;                                  // +0x4110 (DWARF h:432)
        EArrowConfigurations meDirtyArrowConfiguration;             // +0x4114 (DWARF type)
        bool  mbDirty;                                              // +0x4118
        s8    miDirtyCurrentlyActiveSlots;                          // +0x4119
        s8    miDirtyCurrentlyHighlightedSlot;                      // +0x411A

        // ---- the restore slots SaveCurrentState / AttemptStateRestore keep (DWARF h:439..443) ----
        static bool sbRestoreDataAvailable;                         // ARTIST .bss 0x82FB27E0
        static EListType seStoredListType;                          // 0x82FB27E4
        static CgsID sStoredChallenge;                              // 0x82FB27E8
        static CgsNetwork::PlayerName sStoredFriend;                // 0x82FB00C0
        static EShortcutOptions seShortcutOption;                   // 0x82FB27F0
    };

    // DWARF: defined in the header (listed without a line in the BrnGuiComponentUnity CU, as the
    // header-inline SetGuiCachePointer is in BrnGuiHUDUnity); ARTIST keeps a COMDAT 0x8241F120 for
    // the sites that call it (Close 0x8243987C). In ARTIST's store order (0x8241F148..0x8241F15C):
    // the flag, the two states, the two slot bytes, then the list type.
    inline void FriendsListComponent::SetDirty()
    {
        mbDirty                         = true;
        meDirtyCurrentState             = meCurrentState;
        meDirtyBranchState              = meBranchState;
        miDirtyCurrentlyActiveSlots     = miCurrentlyActiveSlots;
        miDirtyCurrentlyHighlightedSlot = miCurrentlyHighlightedSlot;
        meDirtyListType                 = meListType;
        meDirtyArrowConfiguration       = GetArrowState();
    }

    // DWARF: defined in the header (ARTIST's COMDAT 0x82473580 asserts BrnFriendsList.h:665).
    inline void FriendsListComponent::SetGuiCachePointer(GuiCache* lpGuiCache)
    {
        CGS_ASSERT_AT(lpGuiCache, "lpGuiCache",
                      "..\\..\\..\\GameSource\\Gui/Flow/HUD/Components/BrnFriendsList.h", 665);
        mpGuiCache = lpGuiCache;
        miPlayerCount = lpGuiCache->GetFriendsListCachedField();
    }

    // Free post-increment / post-decrement over the friends-list branch enum
    // (DWARF BrnFriendsList.h:767 operator++ / h:764 operator--). Each writes the
    // stepped value back and asserts it stays in range, returning the PRE-step value.
    FriendsListComponent::EFriendListBranchState
    operator++(FriendsListComponent::EFriendListBranchState& lreEnumIndex, int);   // @0x82410958
    FriendsListComponent::EFriendListBranchState
    operator--(FriendsListComponent::EFriendListBranchState& lreEnumIndex, int);   // @0x824109B8
}
