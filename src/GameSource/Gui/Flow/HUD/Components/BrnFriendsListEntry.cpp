#include "GameSource/Gui/Flow/HUD/Components/BrnFriendsListEntry.h"

#include "GameShared/GameClasses/Core/CgsStringUtils.h"                              // CgsCore::SnPrintf
#include "GameShared/GameClasses/Core/CgsAssert.h"                                    // CGS_ASSERT (SetEntryStatus)
#include "GameShared/GameClasses/System/CgsHardwareInit.h"                            // CgsSystem::HardwareInit::IsHardDiskAvailable
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"                                    // BrnFlapt::FileRef
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponentUtils.h"    // BrnGui::AttachToTextFieldComponent

// BrnGui::FriendsListEntry -- reconstructed from BURNOUT_X360_ARTIST.XEX (DWARF
// primary file GameSource/Gui/Flow/Hud/Components/BrnFriendsListEntry.cpp).
//
// Bodied here (4 ledger functions):
//   FriendsListEntry::Construct  @0x82414B28   FriendsListEntry::Prepare @0x824239B8
//   FriendsListEntry::Update     @0x82423B58   FriendsListEntry::Invalidate @0x82423C10

namespace BrnGui
{

// The bar-state frame labels (XEX .data @0x82F24EF8) and the two per-entry-state
// bar mappings (XEX .rodata: SELECTED @0x8204C278, UNSELECTED @0x8204C2F8).
const char* const FriendsListEntry::KAC_BAR_STATE_NAMES[FriendsListEntry::E_FRIENDLISTBARSTATE_COUNT] =
{
    "unselected",           "unselectedSCut",   "unselectedFBC",  "unselectedFBCDone",
    "unselectedEasy",       "unselectedMed",    "unselectedHard", "unselectedVHard",
    "unselectedDisconnected",
    "selected",             "selectedSCut",     "selectedFBC",    "selectedFBCDone",
    "selectedEasy",         "selectedMed",      "selectedHard",   "selectedVHard",
    "selectedDisconnected", "selectedDisabled", "invisible",
};
const char FriendsListEntry::macPlayerNameTextFieldName[15] = "playerName_txt";
const char FriendsListEntry::macStatusIconName[17]          = "inviteStatus_cpt";
const char FriendsListEntry::KAC_INDEX_TEXT_FIELD_NAME[10]  = "index_txt";
const char FriendsListEntry::macBarStateAnimatorName[18]    = "entryAnimator_cpt";

// The status-icon frame labels (XEX .data @0x82F24F48, immediately after
// KAC_BAR_STATE_NAMES[20]) and the per-entry-state -> icon-state mapping
// (KAE_FRIEND_LIST_ICON_STATES, dword_8204C1F0). Only KAC_STATUS_ICON_NAMES[0]
// ("invisible") is X360-attested (the off_82F24F48[0] rodata consumed by the
// no-HDD branch in SetEntryStatus); the remaining five labels and the whole
// 29-entry mapping table are reconstructed from the EFriendListIconState /
// EFriendListEntryState enum semantics -- the SetEntryStatus asm proves the
// indexing and the {RECEIVED, SENT_JOINABLE, JOINABLE} branch constants, not the
// individual cell values.
const char* const FriendsListEntry::KAC_STATUS_ICON_NAMES[FriendsListEntry::E_FRIENDLISTICONSTATE_COUNT] =
{
    "invisible",        // E_FRIENDLISTICONSTATE_INVISIBLE     (X360-attested)
    "received",         // E_FRIENDLISTICONSTATE_RECEIVED      (reconstructed)
    "sent",             // E_FRIENDLISTICONSTATE_SENT          (reconstructed)
    "sentJoinable",     // E_FRIENDLISTICONSTATE_SENT_JOINABLE (reconstructed)
    "joinable",         // E_FRIENDLISTICONSTATE_JOINABLE      (reconstructed)
    "fbcComplete",      // E_FRIENDLISTICONSTATE_FBC_COMPLETE  (reconstructed)
};

#define ICON(e) FriendsListEntry::E_FRIENDLISTICONSTATE_##e
const FriendsListEntry::EFriendListIconState
FriendsListEntry::KAE_FRIEND_LIST_ICON_STATES[FriendsListEntry::E_FRIENDLISTENTRYSTATE_COUNT] =
{
    ICON(INVISIBLE),        // INVISIBLE
    ICON(INVISIBLE),        // YOUOFFLINE
    ICON(INVISIBLE),        // NOFRIENDS
    ICON(INVISIBLE),        // DISABLED
    ICON(INVISIBLE),        // FRIENDINLOBBY
    ICON(INVISIBLE),        // FRIENDOFFLINE
    ICON(JOINABLE),         // FRIENDJOINABLE
    ICON(INVISIBLE),        // FRIENDNOTJOINABLE
    ICON(SENT_JOINABLE),    // FRIENDSENTJOINABLE
    ICON(SENT),             // FRIENDSENTNOTJOINABLE
    ICON(RECEIVED),         // FRIENDRECEIVED
    ICON(INVISIBLE),        // FRIENDNOTINVITABLENOTJOINABLE
    ICON(JOINABLE),         // FRIENDNOTINVITABLEJOINABLE
    ICON(INVISIBLE),        // NOMULTIPLAYERPRIVILEGE
    ICON(INVISIBLE),        // INVITEINPROGRESS
    ICON(INVISIBLE),        // REVOKEINPROGRESS
    ICON(INVISIBLE),        // DECLINEINPROGRESS
    ICON(INVISIBLE),        // SHORTCUT_BASIC
    ICON(INVISIBLE),        // FBC_BASIC
    ICON(INVISIBLE),        // FBC_NOT_DONE
    ICON(FBC_COMPLETE),     // FBC_DONE
    ICON(INVISIBLE),        // FBC_EASY_TODO
    ICON(FBC_COMPLETE),     // FBC_EASY_DONE
    ICON(INVISIBLE),        // FBC_MEDIUM_TODO
    ICON(FBC_COMPLETE),     // FBC_MEDIUM_DONE
    ICON(INVISIBLE),        // FBC_HARD_TODO
    ICON(FBC_COMPLETE),     // FBC_HARD_DONE
    ICON(INVISIBLE),        // FBC_VERY_HARD_TODO
    ICON(FBC_COMPLETE),     // FBC_VERY_HARD_DONE
};
#undef ICON

#define BAR(e) FriendsListEntry::E_FRIENDLISTBARSTATE_##e
const FriendsListEntry::EFriendListBarState
FriendsListEntry::KAE_FRIEND_LIST_SELECTED_BAR_STATES[FriendsListEntry::E_FRIENDLISTENTRYSTATE_COUNT] =
{
    BAR(INVISIBLE),               // INVISIBLE
    BAR(SELECTED),                // YOUOFFLINE
    BAR(SELECTED),                // NOFRIENDS
    BAR(SELECTED_DISABLED),       // DISABLED
    BAR(SELECTED),                // FRIENDINLOBBY
    BAR(SELECTED_DISCONNECTED),   // FRIENDOFFLINE
    BAR(SELECTED), BAR(SELECTED), BAR(SELECTED), BAR(SELECTED), BAR(SELECTED),
    BAR(SELECTED), BAR(SELECTED), BAR(SELECTED), BAR(SELECTED), BAR(SELECTED),
    BAR(SELECTED),                // ... every remaining friends state -> SELECTED
    BAR(SELECTED_SHORTCUT_BASIC), // SHORTCUT_BASIC
    BAR(SELECTED_FBC_BASIC),      // FBC_BASIC
    BAR(SELECTED_FBC_BASIC),      // FBC_NOT_DONE
    BAR(SELECTED_FBC_BASIC),      // FBC_DONE
    BAR(SELECTED_FBC_EASY),  BAR(SELECTED_FBC_EASY),        // FBC_EASY_TODO/DONE
    BAR(SELECTED_FBC_MEDIUM), BAR(SELECTED_FBC_MEDIUM),     // FBC_MEDIUM_TODO/DONE
    BAR(SELECTED_FBC_HARD),  BAR(SELECTED_FBC_HARD),        // FBC_HARD_TODO/DONE
    BAR(SELECTED_FBC_VERY_HARD), BAR(SELECTED_FBC_VERY_HARD), // FBC_VERY_HARD_TODO/DONE
};
const FriendsListEntry::EFriendListBarState
FriendsListEntry::KAE_FRIEND_LIST_UNSELECTED_BAR_STATES[FriendsListEntry::E_FRIENDLISTENTRYSTATE_COUNT] =
{
    BAR(INVISIBLE),                     // INVISIBLE
    BAR(UNSELECTED),                    // YOUOFFLINE
    BAR(UNSELECTED),                    // NOFRIENDS
    BAR(UNSELECTED_SHORTCUT_BASIC),     // DISABLED (X360 table value 1)
    BAR(UNSELECTED),                    // FRIENDINLOBBY
    BAR(UNSELECTED_DISCONNECTED),       // FRIENDOFFLINE
    BAR(UNSELECTED), BAR(UNSELECTED), BAR(UNSELECTED), BAR(UNSELECTED), BAR(UNSELECTED),
    BAR(UNSELECTED), BAR(UNSELECTED), BAR(UNSELECTED), BAR(UNSELECTED), BAR(UNSELECTED),
    BAR(UNSELECTED),                    // ... every remaining friends state -> UNSELECTED
    BAR(UNSELECTED_SHORTCUT_BASIC),     // SHORTCUT_BASIC
    BAR(UNSELECTED_FBC_BASIC),          // FBC_BASIC
    BAR(UNSELECTED_FBC_BASIC),          // FBC_NOT_DONE
    BAR(UNSELECTED_FBC_BASIC),          // FBC_DONE
    BAR(UNSELECTED_FBC_EASY),  BAR(UNSELECTED_FBC_EASY),          // FBC_EASY_TODO/DONE
    BAR(UNSELECTED_FBC_MEDIUM), BAR(UNSELECTED_FBC_MEDIUM),       // FBC_MEDIUM_TODO/DONE
    BAR(UNSELECTED_FBC_HARD),  BAR(UNSELECTED_FBC_HARD),          // FBC_HARD_TODO/DONE
    BAR(UNSELECTED_FBC_VERY_HARD), BAR(UNSELECTED_FBC_VERY_HARD), // FBC_VERY_HARD_TODO/DONE
};
#undef BAR

// @ 0x82414B28 -- the inlined BrnFlaptComponent::Construct (h:113 tripwire + the
// interface/ref adopt) first, then the selectable base, the gate resets (vtbl
// slots 0/2), the dirty raise, the two child Constructs (NULL names/parents on
// the X360), the ref invalidations, and the state seeds.
void FriendsListEntry::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                                 const char* lpacParentName, u64 luId)
{
    BrnFlaptComponent::Construct(lpStateInterface);   // inlined on the X360
    Selectable::Construct(lpacName, lpStateInterface, lpacParentName, luId);
    SetActive(false);       // vtbl slot 0
    SetSelectable(false);   // vtbl slot 2
    SetDirty();
    mBarStateAnimator.Construct(0, lpStateInterface, 0);
    mStatusIcon.Construct(0, lpStateInterface, 0);
    mBarArrowRef.SetInvalid();
    mPlayerNameTextField.SetInvalid();
    mIndexTextField.SetInvalid();
    meFriendListBarState = E_FRIENDLISTBARSTATE_INVISIBLE;
    meEntryStatus        = E_FRIENDLISTENTRYSTATE_INVISIBLE;
}

// @ 0x824239B8 -- bind this row's clip (the base Prepare resolves the composite
// "<parent>_<name>" key, binds mAptRef and resets its timeline; the X360 inlines
// it), prepare the two children under the composite name, then resolve the
// Entry/branch_arrow_mc clip chain and the two text fields.
void FriendsListEntry::Prepare(const char* lpacName, const BrnFlapt::FileRef& lrFile,
                               const char* lpacParentName)
{
    char lacComposite[160];   // X360 sp+0x80 local (formatted through a 128 cap)
    CgsCore::SnPrintf(lacComposite, 128, "%s_%s", lpacParentName, lpacName);
    lacComposite[127] = 0;

    BrnFlaptComponent::Prepare(lpacName, lrFile, lpacParentName);   // inlined on the X360

    mBarStateAnimator.Prepare(macBarStateAnimatorName, lrFile, lacComposite);
    mStatusIcon.Prepare(macStatusIconName, lrFile, lacComposite);

    BrnFlapt::MovieClipRef lEntryClip;
    BrnFlapt::MovieClipRef lArrowClip;
    mAptRef.FindChildMovieClip(&lEntryClip, "Entry");
    lEntryClip.FindChildMovieClip(&lArrowClip, "branch_arrow_mc");
    mBarArrowRef = lArrowClip;

    BrnFlapt::TextFieldRef lTextField;
    mPlayerNameTextField = *AttachToTextFieldComponent(
        &lTextField, macPlayerNameTextFieldName, "playerName_cpt", lacComposite, lrFile);
    mIndexTextField = *AttachToTextFieldComponent(
        &lTextField, KAC_INDEX_TEXT_FIELD_NAME, "index_mc", lacComposite, lrFile);
}

// @ 0x82423B58 -- consume the dirty flag; re-derive the bar state through the
// highlighted/unhighlighted mapping and, when it changed, run the animator on the
// new bar frame and re-derive the arrow's visibility (hidden while DISABLED).
void FriendsListEntry::Update()
{
    if (!IsDirty())
        return;
    ClearFlag(E_FLAG_DIRTY);

    const EFriendListBarState leBarState = IsHighlighted()
        ? KAE_FRIEND_LIST_SELECTED_BAR_STATES[meEntryStatus]
        : KAE_FRIEND_LIST_UNSELECTED_BAR_STATES[meEntryStatus];
    if (leBarState != meFriendListBarState)
    {
        meFriendListBarState = leBarState;
        mBarStateAnimator.Run(KAC_BAR_STATE_NAMES[leBarState]);
        mBarArrowRef.SetVisible(meEntryStatus != E_FRIENDLISTENTRYSTATE_DISABLED);
    }
}

// @ 0x82423C10 -- drop the row back to an empty slot: gates off (vtbl slots 3
// then 1, the asm's call order), blank the gamertag, reset the status, and run
// the animator on the (now invisible) unhighlighted bar frame.
void FriendsListEntry::Invalidate()
{
    SetHighlighted(false);      // vtbl slot 3
    SetHighlightable(false);    // vtbl slot 1
    mPlayerNameTextField.SetText("", false);
    SetEntryStatus(E_FRIENDLISTENTRYSTATE_INVISIBLE);
    mBarStateAnimator.Run(KAC_BAR_STATE_NAMES[KAE_FRIEND_LIST_UNSELECTED_BAR_STATES[meEntryStatus]]);
}

// @ 0x82410850 -- record the new entry status, drive the invite-status icon to
// the matching frame label (forced to "invisible" when the hard disk is absent
// and the mapped icon is a network-presence one: JOINABLE / SENT_JOINABLE /
// RECEIVED), and raise the dirty gate so the next Update re-derives the bar.
void FriendsListEntry::SetEntryStatus(EFriendListEntryState leNewStatus)
{
    CGS_ASSERT(leNewStatus < E_FRIENDLISTENTRYSTATE_COUNT, "leNewStatus < E_FRIENDLISTENTRYSTATE_COUNT");
    CGS_ASSERT(leNewStatus >= E_FRIENDLISTENTRYSTATE_FIRST, "leNewStatus >= E_FRIENDLISTENTRYSTATE_FIRST");

    meEntryStatus = leNewStatus;

    const EFriendListIconState leIconState = KAE_FRIEND_LIST_ICON_STATES[meEntryStatus];
    if (!CgsSystem::HardwareInit::IsHardDiskAvailable()
        && (leIconState == E_FRIENDLISTICONSTATE_JOINABLE
            || leIconState == E_FRIENDLISTICONSTATE_SENT_JOINABLE
            || leIconState == E_FRIENDLISTICONSTATE_RECEIVED))
    {
        mStatusIcon.SetState(KAC_STATUS_ICON_NAMES[E_FRIENDLISTICONSTATE_INVISIBLE]);
    }
    else
    {
        mStatusIcon.SetState(KAC_STATUS_ICON_NAMES[leIconState]);
    }

    SetDirty();
}

}
