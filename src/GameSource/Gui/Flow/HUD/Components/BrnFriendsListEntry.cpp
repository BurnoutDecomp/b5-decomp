#include "GameSource/Gui/Flow/HUD/Components/BrnFriendsListEntry.h"

#include "GameShared/GameClasses/Core/CgsStringUtils.h"                              // CgsCore::SnPrintf
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

}
