#ifndef BRN_ONLINE_LOADING_PLAYER_H
#define BRN_ONLINE_LOADING_PLAYER_H

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"     // CgsGui::GuiComponent / StateInterface
#include "GameSource/Gui/BrnGuiTextField.h"                             // BrnGui::TextField
#include "GameSource/Gui/Flow/Shared/Components/BrnIcon.h"              // BrnGui::IconComponent
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                         // BrnGui::GuiFlow
#include "GameSource/GameState/BrnGameStateSharedIO.h"                  // BrnGameState::GameStateModuleIO::EPlayerTeam

// BrnGui::OnlineLoadingPlayer - one row of the online "loading players" list. It owns a
// gamertag text field plus five status icons (camera-connected, crowned/leader,
// live-revenge, team, VOIP-talking) and drives them off named apt clips.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (DWARF primary file
// GameSource/Gui/Flow/Screen/Components/BrnOnlineLoadingPlayer.cpp):
//   OnlineLoadingPlayer::Construct                  @0x82419CD0
//   OnlineLoadingPlayer::HandleLoadNotification     @0x82419DD0
//   OnlineLoadingPlayer::AppendExpectedAptComponent @0x82419F68
//   OnlineLoadingPlayer::Hide                       @0x82419FF8
// (Show / Set* accessors are their own ledger functions and are OMITTED here.)
//
// Guest layout (base CgsGui::GuiComponent occupies +0x00..+0x8B):
//   +0x8C mGamertag (TextField, 0x128), +0x1B4 mCameraIcon, +0x248 mCrownIcon,
//   +0x2DC mLiveRevengeIcon, +0x370 mTeamIcon, +0x404 mVOIPIcon (IconComponent, stride
//   0x94), +0x498 meTeam, +0x49C mbCrowned, +0x49D mbGamertagSet, +0x49E mbVOIPActive.
// Members carry embedded StateInterface* pointers that widen on the 64-bit host, so the
// guest byte offsets past the base are NOT statically pinned (member access is by name).

namespace BrnGui
{
    class GuiCache;   // BrnGui::GuiCache (AppendExpectedAptComponent target; full def in BrnGuiCache.h)

    class OnlineLoadingPlayer : public CgsGui::GuiComponent
    {
    public:
        // @0x82419CD0 -- base construct, build the gamertag field + five status icons,
        // and clear the team / crowned / gamertag-set / VOIP-active state.
        virtual void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName);

        // @0x82419DD0 -- a named apt clip finished loading: if it belongs to this row,
        // re-assert the matching child (gamertag text / crown icon / VOIP icon).
        bool HandleLoadNotification(const char* lpacNotification);

        // @0x82419F68 -- register this row and its six children as expected apt
        // components on the given flow layer.
        void AppendExpectedAptComponent(GuiFlow leFlow, GuiCache* lpGuiCache);

        // @0x82419FF8 -- hide the row (push the "invisible" apt state).
        void Hide();

    private:
        // File-scope apt-clip names for the children (DWARF KAC_* constants; the X360
        // stores them as .rodata literals inlined into Construct). Defined in the .cpp.
        static const char* const KAC_GAMERTAG_TEXTFIELD_NAME;   // "gamertag_mc"
        static const char* const KAC_CAMERA_ICON_NAME;          // "CameraIcon_mc"
        static const char* const KAC_CROWN_ICON_NAME;           // "CrownIcon_mc"
        static const char* const KAC_LIVE_REVENGE_ICON_NAME;    // "LiveRevengeIcon_mc"
        static const char* const KAC_TEAM_ICON_NAME;            // "TeamIcon_mc"
        static const char* const KAC_VOIP_ICON_NAME;            // "VOIPIcon_mc"

        BrnGui::TextField     mGamertag;         // +0x8C
        BrnGui::IconComponent mCameraIcon;       // +0x1B4
        BrnGui::IconComponent mCrownIcon;        // +0x248
        BrnGui::IconComponent mLiveRevengeIcon;  // +0x2DC
        BrnGui::IconComponent mTeamIcon;         // +0x370
        BrnGui::IconComponent mVOIPIcon;         // +0x404

        BrnGameState::GameStateModuleIO::EPlayerTeam meTeam;  // +0x498
        bool mbCrowned;       // +0x49C
        bool mbGamertagSet;   // +0x49D
        bool mbVOIPActive;    // +0x49E
    };
}

#endif
