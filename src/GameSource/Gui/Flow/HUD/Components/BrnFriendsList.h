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
// NOT reconstructed (no DWARF / leak entry for this TU), so the groups are modelled
// directly rather than byte-padded to the X360 offsets -- this is a host build where
// pointer widening already breaks byte-exactness; the contract here is semantic parity
// with member-by-name access (the established BrnGuiKeyboard / CrashNavIconRenderer
// convention). The X360 byte offsets are cited per field for provenance only.
// ===================================================================================

#include "types.hpp"
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"   // [friends wave] branch labels/title
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"  // [friends wave] arrow clips
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"      // Prepare(const FileRef&)
#include "GameSource/GameState/BrnCgsPlayerName.h"
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h" // [friends wave] real base (DWARF h:49)     // [friends wave] mHighlightedName
#include "GameSource/Gui/BrnGuiCache.h"   // BrnGui::GuiCache (the cache pointer + far field)
#include "GameSource/Gui/Flow/HUD/Components/BrnFriendsListEntry.h" // [friends wave] maEntries[5]

namespace BrnGui
{
    const s32 KI_MAX_FRIEND_RECORDS = 100;   // maeDisplayTypes capacity (SortFullList bound)
    const s32 KI_VISIBLE_ROWS       = 5;

    // Consumer-carved shortcut-option vocabulary (assert strings spell
    // E_SHORTCUT_OPTION_COUNT / maeAvailableShortcutOptions).
    enum EShortCutOption
    {
        E_SHORTCUT_OPTION_NONE  = -1,
        E_SHORTCUTOPTION_COUNT  = 21,
    };

    // [friends wave] base corrected: the X360 Construct stores the interface at +0x00
    // and invalidates the clip pair at +0x04/+0x08 -- the BrnFlaptComponent shape.
    class FriendsListComponent : public BrnFlaptComponent
    {
    public:
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

        // @ 0x82473580 -- latch the (non-NULL) GuiCache pointer and cache its far field.
        // Returns `this`.
        FriendsListComponent* SetGuiCachePointer(GuiCache* lpGuiCache);

        // @ 0x8241F120 -- snapshot the current selection into the dirty region, raise the
        // dirty flag, and store the scroll-arrow indicator. Returns `this`.
        FriendsListComponent* SetDirty();
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
        void ProcessNewEntryData(const void* lpFriendInfo);                             // 0x824430D0
        void RequestRefreshedData();                                                    // 0x82439248
        void SetTotalFriends(s32 liCount);                                              // 0x824392B8
        void AttemptStateRestore();                                                     // 0x82441D78
        void ReshowShortcuts();                                                         // 0x82441E78
        void ShowFriendsList();                                                         // 0x8243FF68
        void ShowShortcutList();                                                        // 0x824417E0
        void ShowChallengesList();                                                      // 0x824418D0
        void ShowSpecificFriend(const char* lpcName);                                   // 0x8242BBE8
        void ShowSpecificShortcut(s32 leOption);                                        // 0x82441C90
        void ShowSpecificChallenge(CgsID lu64Uid);                                      // 0x82441B78
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
        void Highlight(s32 liRow);                                                      // 0x82414960
        void SetEntryData(s32 liRow, const char* lpcText, s32 leStatus, bool lbLocalise); // 0x82422CB0
        void HandleNotConnected();                                                      // 0x8242B948
        void HandleNoFriends();                                                         // 0x8242BA40
        void HandleDPadLeft();                                                          // 0x82442868
        void HandleDPadRight();                                                         // 0x82442D98
        void HandleDPadRightFriends();                                                  // 0x824386B0
        void HandleDPadRightChallenges();                                               // 0x82438760
        void HandleDPadRightShortcuts();                                                // 0x82438DC0
        void HandleBranchDPadRightFriends();                                            // 0x82438938
        void HandleBranchDPadRightShortcuts();                                          // 0x82438DC0 sibling @0x82438938 pair -- see cpp map
        void HandleBranchInteraction(s32 liAction);                                     // 0x82439040
        void HandleTableInteraction(s32 liAction);                                      // 0x82442E50
        static void TransitionCompleteCallback(void* lpUserData);                       // 0x82414AA0
        static int  BuddySortFunction(const void* lpA, const void* lpB);                // 0x824147A8

    private:
        // ---- current selection cursor (X360 +0x870..+0x894) ----
        u32 muCachedCacheField;   // +0x870 -- cached GuiCache far field (set by SetGuiCachePointer)
        s32 meListType;           // +0x874 -- 1 friends / 2 shortcuts / 3 challenges (SaveCurrentState switch @0x82414A10)
        s32 mePanelState;         // +0x878 -- 0 closed / 1 opening / 2 open / 5 dismissed
        s32 meBranchState;        // +0x87C -- EFriendListBranchState
        s8  mi8FirstVisibleIndex; // +0x880 -- scroll-window top row
        s8  mi8SelectedRowIndex;  // +0x881 -- highlighted row within window
        s8  mi8SelectedIndex;     // +0x882 -- highlighted entry (signed); drives the scroll indicator
        u32 muNumEntries;         // +0x894 -- total entries (scroll-indicator bound)

        // ---- the GuiCache this component reads through (X360 +0x4100) ----
        GuiCache* mpGuiCache;     // +0x4100 -- set by SetGuiCachePointer

        // ---- dirty snapshot region the renderer picks up (X360 +0x4108..+0x411A) ----
        u32 muSnapshotA;          // +0x4108 = muSelectionB
        u32 muSnapshotB;          // +0x410C = muSelectionC
        u32 muSnapshotC;          // +0x4110 = muSelectionA
        u32 muScrollIndicator;    // +0x4114 -- 0..3, from selected index vs count
        u8  mbDirty;              // +0x4118 -- raised to 1 by SetDirty
        u8  mbSnapshotFlagA;      // +0x4119 = mbSelectionFlagA
        u8  mbSnapshotFlagB;      // +0x411A = mbSelectionFlagB
        // ---- one online-friend record [friends wave]: stride 0x84, base +0xBEC ----
        struct SFriendRecord
        {
            u32  muType;                     // +0x00 presence/class code (BuddySort 1/2 arms)
            u32  muPad04;
            u32  muPad08;
            char macName[16];                // +0x0C (LobbyNameCmp target)
            u8   mubClassA;                  // +0x1C (zero => "you" row -> state 5)
            u8   mubClassB;                  // +0x1D (secondary sort class)
            u8   mubMatchedLobbyName;        // +0x1E (set by SortFullList lobby scan)
            u8   mubJoinable;                // +0x1F (joinable/not-joinable state pairs)
            u8   maRest[0x64];               // +0x20..+0x83 (unwitnessed interior)
        };
    
        static_assert(sizeof(SFriendRecord) == 0x84, "record stride");
        static_assert(__builtin_offsetof(SFriendRecord, macName) == 0x0C &&
                      __builtin_offsetof(SFriendRecord, mubClassA) == 0x1C,
                      "SortFullList/BuddySort field witnesses");

    private:
        // ---- [friends wave] asm-pinned additions (offsets in comments) ----
        FriendsListEntry maEntries[KI_VISIBLE_ROWS];                // +0x8A0 (stride 0x98)
        s32  maeAvailableShortcutOptions[E_SHORTCUTOPTION_COUNT];   // +0x00C, sentinel NONE(21)
        s32  mauNumBranches[3];                                     // +0x060 (Construct seeds 15)
        CgsID mau64ChallengeIds[KI_MAX_FRIEND_RECORDS];             // +0x070 ((idx+14)*8 addressing)
        BrnFlapt::TextFieldRef  maBranchLabelFields[3];             // +0xB98 ("branchOptionOne_txt" chain)
        BrnFlapt::TextFieldRef  mListTitleField;                    // +0xBBC ("listTitle_mc"/"listTitle_txt")
        BrnFlapt::MovieClipRef  mUpArrowClip;                       // +0xBC8 ("upArrow_mc"/"arrow")
        BrnFlapt::MovieClipRef  mDownArrowClip;                     // +0xBD0 ("downArrow_mc")
        BrnFlapt::MovieClipRef  mThirdClip;                         // +0xBD8 (chain tail)
        SFriendRecord maRecords[KI_MAX_FRIEND_RECORDS];             // +0xBEC (memset span 0x3390)
        s32  maeDisplayTypes[KI_MAX_FRIEND_RECORDS];                // +0x3F70 (EFriendListEntryState)
        s8   mi8CurrentlyHighlightedBranch;                         // +0x883
        CgsNetwork::PlayerName mHighlightedName;                    // +0x884
        u8   mabEntryFlags[2];                                      // +0x898
        s32  meDataState;                                           // +0x89C (0 none/1 requested/2 ready)
        u8   mabRecordTailFlags[KI_MAX_FRIEND_RECORDS][2];          // +0xB78 (parallel pairs, stride 0x84)
        bool mbReopenAfterClose;                                    // +0x4104 (TransitionCompleteCallback)
    };

    // Free post-increment / post-decrement over the friends-list branch enum
    // (DWARF BrnFriendsList.h:767 operator++ / h:764 operator--). Each writes the
    // stepped value back and asserts it stays in range, returning the PRE-step value.
    FriendsListComponent::EFriendListBranchState
    operator++(FriendsListComponent::EFriendListBranchState& lreEnumIndex, int);   // @0x82410958
    FriendsListComponent::EFriendListBranchState
    operator--(FriendsListComponent::EFriendListBranchState& lreEnumIndex, int);   // @0x824109B8
}
