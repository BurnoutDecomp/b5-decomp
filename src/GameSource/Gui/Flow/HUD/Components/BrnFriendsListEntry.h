#pragma once

#include "types.hpp"
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"                          // BrnFlapt::MovieClipRef
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"                          // BrnFlapt::TextFieldRef
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectable.h"                // BrnGui::Selectable (first base)
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h"    // BrnFlaptComponent (second base chain)
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptIconComponent.h" // FlaptIconComponent / FlaptAnimatorComponent

// BrnGui::FriendsListEntry - one row of the friends-list HUD panel: a selectable
// Flapt component mirroring a friend's presence/challenge state into the entry
// bar animator, the invite-status icon, and the gamertag/index text fields.
// Class shape / enums / member names / method set verbatim from the DecFIGS
// DWARF (BrnFriendsListEntry.h:54/:72/:75/:126/:153/:232-:256). The X360 layout
// pins the MI bases (Selectable @+0x00, flags @+0x0C; the BrnFlaptComponent
// chain @+0x18) with meEntryStatus @+0x24, meFriendListBarState @+0x28,
// mBarStateAnimator @+0x2C, mStatusIcon @+0x64, mBarArrowRef @+0x78,
// mPlayerNameTextField @+0x80, mIndexTextField @+0x8C. This TU bodies
// Construct/Prepare/Update/Invalidate; the rest of the surface is declared-only.
namespace BrnGui
{
    // DWARF BrnFriendsListEntry.h:54 -- the shared row base (adds nothing).
    struct BaseFriendsListEntry : public BrnFlaptComponent
    {
    };

    struct FriendsListEntry : public Selectable, public BaseFriendsListEntry
    {
        // DWARF :75.
        enum EFriendListEntryState
        {
            E_FRIENDLISTENTRYSTATE_INVISIBLE                    = 0,
            E_FRIENDLISTENTRYSTATE_YOUOFFLINE                   = 1,
            E_FRIENDLISTENTRYSTATE_NOFRIENDS                    = 2,
            E_FRIENDLISTENTRYSTATE_DISABLED                     = 3,
            E_FRIENDLISTENTRYSTATE_FRIENDINLOBBY                = 4,
            E_FRIENDLISTENTRYSTATE_FRIENDOFFLINE                = 5,
            E_FRIENDLISTENTRYSTATE_FRIENDJOINABLE               = 6,
            E_FRIENDLISTENTRYSTATE_FRIENDNOTJOINABLE            = 7,
            E_FRIENDLISTENTRYSTATE_FRIENDSENTJOINABLE           = 8,
            E_FRIENDLISTENTRYSTATE_FRIENDSENTNOTJOINABLE        = 9,
            E_FRIENDLISTENTRYSTATE_FRIENDRECEIVED               = 10,
            E_FRIENDLISTENTRYSTATE_FRIENDNOTINVITABLENOTJOINABLE = 11,
            E_FRIENDLISTENTRYSTATE_FRIENDNOTINVITABLEJOINABLE   = 12,
            E_FRIENDLISTENTRYSTATE_NOMULTIPLAYERPRIVILEGE       = 13,
            E_FRIENDLISTENTRYSTATE_INVITEINPROGRESS             = 14,
            E_FRIENDLISTENTRYSTATE_REVOKEINPROGRESS             = 15,
            E_FRIENDLISTENTRYSTATE_DECLINEINPROGRESS            = 16,
            E_FRIENDLISTENTRYSTATE_SHORTCUT_BASIC               = 17,
            E_FRIENDLISTENTRYSTATE_FBC_BASIC                    = 18,
            E_FRIENDLISTENTRYSTATE_FBC_NOT_DONE                 = 19,
            E_FRIENDLISTENTRYSTATE_FBC_DONE                     = 20,
            E_FRIENDLISTENTRYSTATE_FBC_EASY_TODO                = 21,
            E_FRIENDLISTENTRYSTATE_FBC_EASY_DONE                = 22,
            E_FRIENDLISTENTRYSTATE_FBC_MEDIUM_TODO              = 23,
            E_FRIENDLISTENTRYSTATE_FBC_MEDIUM_DONE              = 24,
            E_FRIENDLISTENTRYSTATE_FBC_HARD_TODO                = 25,
            E_FRIENDLISTENTRYSTATE_FBC_HARD_DONE                = 26,
            E_FRIENDLISTENTRYSTATE_FBC_VERY_HARD_TODO           = 27,
            E_FRIENDLISTENTRYSTATE_FBC_VERY_HARD_DONE           = 28,
            E_FRIENDLISTENTRYSTATE_COUNT                        = 29,
            E_FRIENDLISTENTRYSTATE_FIRST                        = 0,
            E_FRIENDLISTENTRYSTATE_FRINEDS_FIRST                = 0,   // (DWARF spelling)
            E_FRIENDLISTENTRYSTATE_FRINEDS_LAST                 = 16,
            E_FRIENDLISTENTRYSTATE_FRINEDS_COUNT                = 17,
            E_FRIENDLISTENTRYSTATE_SHORTCUT_FIRST               = 17,
            E_FRIENDLISTENTRYSTATE_SHORTCUT_LAST                = 17,
            E_FRIENDLISTENTRYSTATE_SHORTCUT_COUNT               = 1,
            E_FRIENDLISTENTRYSTATE_FBC_FIRST                    = 18,
            E_FRIENDLISTENTRYSTATE_FBC_LAST                     = 26,
            E_FRIENDLISTENTRYSTATE_FBC_COUNT                    = 9,
        };

        // DWARF :126.
        enum EFriendListBarState
        {
            E_FRIENDLISTBARSTATE_UNSELECTED                = 0,
            E_FRIENDLISTBARSTATE_UNSELECTED_SHORTCUT_BASIC = 1,
            E_FRIENDLISTBARSTATE_UNSELECTED_FBC_BASIC      = 2,
            E_FRIENDLISTBARSTATE_UNSELECTED_FBC_DONE       = 3,
            E_FRIENDLISTBARSTATE_UNSELECTED_FBC_EASY       = 4,
            E_FRIENDLISTBARSTATE_UNSELECTED_FBC_MEDIUM     = 5,
            E_FRIENDLISTBARSTATE_UNSELECTED_FBC_HARD       = 6,
            E_FRIENDLISTBARSTATE_UNSELECTED_FBC_VERY_HARD  = 7,
            E_FRIENDLISTBARSTATE_UNSELECTED_DISCONNECTED   = 8,
            E_FRIENDLISTBARSTATE_SELECTED                  = 9,
            E_FRIENDLISTBARSTATE_SELECTED_SHORTCUT_BASIC   = 10,
            E_FRIENDLISTBARSTATE_SELECTED_FBC_BASIC        = 11,
            E_FRIENDLISTBARSTATE_SELECTED_FBC_DONE         = 12,
            E_FRIENDLISTBARSTATE_SELECTED_FBC_EASY         = 13,
            E_FRIENDLISTBARSTATE_SELECTED_FBC_MEDIUM       = 14,
            E_FRIENDLISTBARSTATE_SELECTED_FBC_HARD         = 15,
            E_FRIENDLISTBARSTATE_SELECTED_FBC_VERY_HARD    = 16,
            E_FRIENDLISTBARSTATE_SELECTED_DISCONNECTED     = 17,
            E_FRIENDLISTBARSTATE_SELECTED_DISABLED         = 18,
            E_FRIENDLISTBARSTATE_INVISIBLE                 = 19,
            E_FRIENDLISTBARSTATE_COUNT                     = 20,
            E_FRIENDLISTBARSTATE_FIRST                     = 0,
        };

        // DWARF :153.
        enum EFriendListIconState
        {
            E_FRIENDLISTICONSTATE_INVISIBLE     = 0,
            E_FRIENDLISTICONSTATE_RECEIVED      = 1,
            E_FRIENDLISTICONSTATE_SENT          = 2,
            E_FRIENDLISTICONSTATE_SENT_JOINABLE = 3,
            E_FRIENDLISTICONSTATE_JOINABLE      = 4,
            E_FRIENDLISTICONSTATE_FBC_COMPLETE  = 5,
            E_FRIENDLISTICONSTATE_COUNT         = 6,
            E_FRIENDLISTICONSTATE_FIRST         = 0,
        };

        // @0x82414B28 (this TU, DWARF cpp:184) -- adopt the state channel, run both
        // base Constructs, reset the gates, construct the two child components, and
        // invalidate every clip/text ref.
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName, u64 luId);

        // @0x824239B8 (this TU, DWARF cpp:234) -- bind the row's clip (the base
        // Prepare), prepare the animator/icon children under the composite name, and
        // resolve the arrow clip + the two text fields.
        void Prepare(const char* lpacName, const BrnFlapt::FileRef& lrFile,
                     const char* lpacParentName);

        // @0x82423B58 (this TU, DWARF cpp:355) -- consume the dirty flag; when the
        // highlighted/unhighlighted bar mapping changes, run the animator on the new
        // bar frame and re-derive the arrow's visibility.
        virtual void Update();

        // @0x82423C10 (this TU, DWARF cpp:399) -- drop the row back to an empty slot.
        void Invalidate();

        // DWARF cpp:274/:297 / h:277/:292/:308 / cpp:419 -- declared-only (their own
        // ledger functions / X360 inlines not exported).
        const char* GetPlayerName() const;
        EFriendListEntryState GetEntryStatus() const;
        void SetText(const char* lpacText, bool lbLocalise);
        void SetEntryStatus(EFriendListEntryState leStatus);
        virtual void Select();
        void SetIndexText(s32 liIndex);

    private:
        // DWARF cpp:24-:129 -- the frame-label / clip-name / mapping tables
        // (XEX-recovered where consumed; definitions in the .cpp; the icon tables are
        // declared-only pending their consumers).
        static const char* const KAC_BAR_STATE_NAMES[E_FRIENDLISTBARSTATE_COUNT];        // cpp:24 (@0x82F24EF8)
        static const char        macPlayerNameTextFieldName[15];                          // cpp:59
        static const char        macStatusIconName[17];                                   // cpp:60
        static const char* const KAC_STATUS_ICON_NAMES[E_FRIENDLISTICONSTATE_COUNT];      // cpp:48 (decl-only)
        static const char        KAC_INDEX_TEXT_FIELD_NAME[10];                           // cpp:61 (DWARF spells [9]; "index_txt" needs 10 w/ NUL)
        static const char        macBarStateAnimatorName[18];                             // cpp:58
        static const EFriendListIconState KAE_FRIEND_LIST_ICON_STATES[E_FRIENDLISTENTRYSTATE_COUNT];        // cpp:63 (decl-only)
        static const EFriendListBarState  KAE_FRIEND_LIST_SELECTED_BAR_STATES[E_FRIENDLISTENTRYSTATE_COUNT];   // cpp:96 (@0x8204C278)
        static const EFriendListBarState  KAE_FRIEND_LIST_UNSELECTED_BAR_STATES[E_FRIENDLISTENTRYSTATE_COUNT]; // cpp:129 (@0x8204C2F8)

        // DWARF :232-:256 (X360 offsets in the file header).
        EFriendListEntryState  meEntryStatus;          // :232 (+0x24)
        EFriendListBarState    meFriendListBarState;   // :234 (+0x28)
        FlaptAnimatorComponent mBarStateAnimator;      // :250 (+0x2C)
        FlaptIconComponent     mStatusIcon;            // :251 (+0x64)
        BrnFlapt::MovieClipRef mBarArrowRef;           // :253 (+0x78)
        BrnFlapt::TextFieldRef mPlayerNameTextField;   // :255 (+0x80)
        BrnFlapt::TextFieldRef mIndexTextField;        // :256 (+0x8C)
    };
}
