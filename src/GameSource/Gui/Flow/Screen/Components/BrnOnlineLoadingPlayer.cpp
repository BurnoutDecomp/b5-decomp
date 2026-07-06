// ===========================================================================
// BrnGui::OnlineLoadingPlayer -- one row of the online "loading players" list.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (DWARF primary file
// GameSource/Gui/Flow/Screen/Components/BrnOnlineLoadingPlayer.cpp):
//   OnlineLoadingPlayer::Construct                  @0x82419CD0
//   OnlineLoadingPlayer::HandleLoadNotification     @0x82419DD0
//   OnlineLoadingPlayer::AppendExpectedAptComponent @0x82419F68
//   OnlineLoadingPlayer::Hide                       @0x82419FF8
// ===========================================================================

#include "GameSource/Gui/Flow/Screen/Components/BrnOnlineLoadingPlayer.h"
#include "GameSource/Gui/BrnGuiCache.h"

#include <cstring>   // std::strstr / std::strcmp

namespace BrnGui
{

// File-scope apt-clip names (X360 .rodata literals inlined into Construct).
const char* const OnlineLoadingPlayer::KAC_GAMERTAG_TEXTFIELD_NAME = "gamertag_mc";
const char* const OnlineLoadingPlayer::KAC_CAMERA_ICON_NAME        = "CameraIcon_mc";
const char* const OnlineLoadingPlayer::KAC_CROWN_ICON_NAME         = "CrownIcon_mc";
const char* const OnlineLoadingPlayer::KAC_LIVE_REVENGE_ICON_NAME  = "LiveRevengeIcon_mc";
const char* const OnlineLoadingPlayer::KAC_TEAM_ICON_NAME          = "TeamIcon_mc";
const char* const OnlineLoadingPlayer::KAC_VOIP_ICON_NAME          = "VOIPIcon_mc";

// @0x82419CD0 -----------------------------------------------------------------------
// Base component construct, then build the six children (gamertag text field via its
// virtual Construct; the five status icons with no state-identifier table). Each child
// is parented under this component's own name and wired to the shared state interface
// (base mpStateInterface @+0x88). Finally clear the team / crowned / gamertag-set /
// VOIP-active state.
void OnlineLoadingPlayer::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                                    const char* lpacParentName)
{
    CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);

    // Virtual dispatch in the X360 (TextField::Construct is virtual).
    mGamertag.Construct(KAC_GAMERTAG_TEXTFIELD_NAME, mpStateInterface, GetName());

    mCameraIcon.Construct(KAC_CAMERA_ICON_NAME,            mpStateInterface, 0, GetName());
    mCrownIcon.Construct(KAC_CROWN_ICON_NAME,              mpStateInterface, 0, GetName());
    mLiveRevengeIcon.Construct(KAC_LIVE_REVENGE_ICON_NAME, mpStateInterface, 0, GetName());
    mTeamIcon.Construct(KAC_TEAM_ICON_NAME,                mpStateInterface, 0, GetName());
    mVOIPIcon.Construct(KAC_VOIP_ICON_NAME,                mpStateInterface, 0, GetName());

    meTeam        = BrnGameState::GameStateModuleIO::E_PLAYER_TEAM_NONE;  // stw 0, 0x498
    mbCrowned     = false;                                                // stb 0, 0x49C
    mbGamertagSet = false;                                                // stb 0, 0x49D
    mbVOIPActive  = false;                                                // stb 0, 0x49E
}

// @0x82419DD0 -----------------------------------------------------------------------
// A named apt clip has finished loading (lpacNotification). If the clip belongs to
// this row (its name contains this component's name) re-assert whichever child the
// clip corresponds to: re-push the gamertag's stored text, or re-push the crown / VOIP
// icon's current visual state to the freshly-loaded clip. Returns true once the
// notification is claimed by this component.
bool OnlineLoadingPlayer::HandleLoadNotification(const char* lpacNotification)
{
    // Not one of this component's clips -> not handled.
    if (!std::strstr(lpacNotification, GetName()))
        return false;

    // Gamertag clip loaded: re-push the stored text (guest reads mGamertag.macText at
    // field+0xA4 and feeds it back through SetText).
    if (std::strcmp(lpacNotification, mGamertag.GetName()) == 0)
        mGamertag.SetText(mGamertag.GetText());

    // Crown clip loaded: re-assert the crown icon to its current state -- "crown" when
    // set / "invisible" when clear (X360 0x82419E7C..0x82419EC8; bool normalise inlined).
    if (std::strcmp(lpacNotification, mCrownIcon.GetName()) == 0)
    {
        const bool lbCrowned = (mbCrowned != false);
        mbCrowned = lbCrowned;
        mCrownIcon.SetState(lbCrowned ? "crown" : "invisible");
    }

    // VOIP clip loaded: re-assert the VOIP icon to its current state -- "on" when active
    // / "invisible" when inactive (X360 0x82419F00..0x82419F58).
    if (std::strcmp(lpacNotification, mVOIPIcon.GetName()) == 0)
    {
        const bool lbVoipActive = (mbVOIPActive != false);
        mbVOIPActive = lbVoipActive;
        mVOIPIcon.SetState(lbVoipActive ? "on" : "invisible");
    }

    return true;
}

// @0x82419F68 -----------------------------------------------------------------------
// Register this player row and each of its six child components (the gamertag text
// field and the five status icons) as "expected" apt components on the given flow
// layer, so the cache waits for every one to finish initialising. The X360 forwards
// each component's own name string (component base + 0x4 == GuiComponent::macName ==
// GetName()) to the cache's name-taking AppendExpectedAptComponent entry (@0x824F87C0).
void OnlineLoadingPlayer::AppendExpectedAptComponent(GuiFlow leFlow, GuiCache* lpGuiCache)
{
    lpGuiCache->AppendExpectedAptComponent(leFlow, GetName());                  // r31+4    (self)
    lpGuiCache->AppendExpectedAptComponent(leFlow, mGamertag.GetName());        // r31+0x90
    lpGuiCache->AppendExpectedAptComponent(leFlow, mCameraIcon.GetName());      // r31+0x1B8
    lpGuiCache->AppendExpectedAptComponent(leFlow, mCrownIcon.GetName());       // r31+0x24C
    lpGuiCache->AppendExpectedAptComponent(leFlow, mLiveRevengeIcon.GetName()); // r31+0x2E0
    lpGuiCache->AppendExpectedAptComponent(leFlow, mTeamIcon.GetName());        // r31+0x374
    lpGuiCache->AppendExpectedAptComponent(leFlow, mVOIPIcon.GetName());        // r31+0x408
}

// @0x82419FF8 -----------------------------------------------------------------------
// Hide the whole player row by pushing the "invisible" apt state (deferred output).
void OnlineLoadingPlayer::Hide()
{
    AddOutputAptViewState("apt_state", "invisible", false);
}

}
