#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsID.h"                                    // CgsID (the road id SetupComponent formats)
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"                            // BrnFlapt::TextFieldRef (three embedded)
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptIconComponent.h"  // BrnGui::FlaptIconComponent (base)

// BrnGui::RoadRuleShotComponent - the road-rule "mugshot" HUD panel: when a road
// rule changes hands online, pose/show the smug-shot frame ("Ruler"/"Loser"),
// fill in the gamertag / road-id / won-lost lines, and drive the anim clip
// through its transin/pose/snap/showing/transout labels. Class shape / enums /
// member names / method set verbatim from the DecFIGS DWARF
// (BrnRoadRuleShotComponent.h:55/:59/:112/:122-:148); gated on the X360 ledger.
// This TU bodies the six exported functions; HandleLoadNotifications (cpp:110)
// is not X360-exported (folded) and is declared-only.
namespace BrnGui
{
    class GuiCache;   // reached through the state interface in Snap (BrnGuiCache.h)

    // DWARF BrnRoadRuleShotComponent.h:55.
    class RoadRuleShotComponent : public FlaptIconComponent
    {
    public:
        // DWARF h:59.
        enum EPlayerState
        {
            E_PLAYER_STATE_RACING   = 0,
            E_PLAYER_STATE_CRASHING = 1,
            E_PLAYER_STATE_COUNT    = 2
        };

        // DWARF h:112.
        enum EComponentState
        {
            E_COMPONENT_STATE_INVALID   = -1,
            E_COMPONENT_STATE_NEW_RULER = 0,
            E_COMPONENT_STATE_OLD_LOSER = 1,
            E_COMPONENT_STATE_COUNT     = 2
        };

        // @0x82424600 (this TU, DWARF h:72, cpp:58) -- the inlined base Construct
        // + the state latches (component state to INVALID) + the ref clears.
        // Parameter types follow the committed FlaptIconComponent virtual.
        virtual void Construct(const void* lpDEBUGName,
                               CgsGui::StateInterface* lpStateInterface,
                               const void* lpcParentName);

        // @0x82424690 (this TU, DWARF h:78, cpp:92) -- the base icon Prepare with
        // the parent prefix folded away (the DWARF's 2-param derived shape, the
        // FriendsListChangeIcon precedent).
        void Prepare(const char* lacName, const BrnFlapt::FileRef& lFile);

        // DWARF h:84, cpp:110 -- not X360-exported (folded); declared-only.
        void HandleLoadNotifications(const char* lpacComponentName);

        // @0x824151F0 (this TU, DWARF h:93, cpp:148) -- configure the shot: pick
        // the Ruler/Loser frame + the won/lost string from the ruler flag, resolve
        // the anim clip's three text fields, and fill the gamertag / road-id /
        // rule-state lines. (lePlayerState is validated but otherwise unused --
        // faithful to the X360 body.)
        void SetupComponent(EPlayerState lePlayerState, bool lbLocalPlayerNewRuler,
                            const char* lpcGamertag, CgsID lRoadId, bool lbTimeRule);

        // @0x82415598 (this TU, DWARF h:98, cpp:253) -- play "pose" or "transin".
        void Show(bool lbPose);

        // @0x82415620 (this TU, DWARF h:103, cpp:295) -- jump to "showing"/"snap"
        // and, when the captured-line gate is set, fill the gamertag line with
        // "CAPTURED_FOR <the shot opponent's player record's name>".
        void Snap(bool lbShowing);

        // @0x82415790 (this TU, DWARF h:107, cpp:349) -- play "transout" if the
        // component was ever set up.
        void Hide();

    private:
        // DWARF member layout (h:123-:148; X360 offsets in comments; access BY
        // NAME). KAPC_COMPONENT_STATE_FRAME_NAMES (h:122) lives in the .cpp.
        const char*            mpcComponentState;    // h:123 +0x14 (the live frame name; NULL until SetupComponent)
        EComponentState        meComponentState;     // h:124 +0x18
        BrnFlapt::MovieClipRef mAnimationMovieClip;  // h:144 +0x1C ("anim_mc")
        BrnFlapt::TextFieldRef mGamertagTextfield;   // h:146 +0x24
        BrnFlapt::TextFieldRef mRoadNameTextfield;   // h:147 +0x30
        BrnFlapt::TextFieldRef mRuleStateTextfield;  // h:148 +0x3C
    };
}
