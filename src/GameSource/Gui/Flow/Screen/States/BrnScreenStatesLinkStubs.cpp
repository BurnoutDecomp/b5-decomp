// ===========================================================================
// BrnScreenStatesLinkStubs.cpp -- lifecycle bodies for the SCREEN-flow placeholder
// states (see the header for the per-class evidence notes). The BrnHudStatesLinkStubs
// pattern: OnEnter logs once so an FSM handoff into an un-reconstructed screen leaves
// the game up and the gap visible; OnLeave/Update are empty; resource queries fall
// through to the CgsGui::State empty default.
// FLAG link scaffold: every body below is a stand-in, not a reconstruction.
// ===========================================================================

#include <cstdio>   // std::snprintf (the one-shot gap log)

#include "GameSource/Gui/Flow/Screen/States/BrnScreenStatesLinkStubs.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::WriteToLog

namespace
{
    void LogUnreconstructedState(const char* lpacState, const char* lpacHook)
    {
        char lac[128];
        // (one line per state entry; these screens are post-boot territory)
        std::snprintf(lac, sizeof(lac), "[ScreenFlow] %s::%s -- un-reconstructed state (FLAG).\n",
                      lpacState, lpacHook);
        CgsDev::Log::WriteToLog(lac);
    }
}

namespace BrnGui
{
    // ---- NULL -------------------------------------------------------------------------
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed NullState.
    void NullState::OnEnter() { LogUnreconstructedState("NullState", "OnEnter"); }
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed NullState.
    void NullState::OnLeave() {}
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed NullState.
    void NullState::Update()  {}

    // ---- CS_UNLOCK --------------------------------------------------------------------
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed CarSelectUnlock.
    void CarSelectUnlock::OnEnter() { LogUnreconstructedState("CarSelectUnlock", "OnEnter"); }
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed CarSelectUnlock.
    void CarSelectUnlock::OnLeave() {}
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed CarSelectUnlock.
    void CarSelectUnlock::Update()  {}

    // ---- CS_LIVERY --------------------------------------------------------------------
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed CarSelectLivery.
    void CarSelectLivery::OnEnter() { LogUnreconstructedState("CarSelectLivery", "OnEnter"); }
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed CarSelectLivery.
    void CarSelectLivery::OnLeave() {}
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed CarSelectLivery.
    void CarSelectLivery::Update()  {}

    // ---- CN_MAP_EVENT -----------------------------------------------------------------
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed CrashNavMapEvent.
    void CrashNavMapEvent::OnEnter() { LogUnreconstructedState("CrashNavMapEvent", "OnEnter"); }
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed CrashNavMapEvent.
    void CrashNavMapEvent::OnLeave() {}
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed CrashNavMapEvent.
    void CrashNavMapEvent::Update()  {}

    // ---- CN_MAP_MAIN ------------------------------------------------------------------
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed CrashNavMapMain.
    void CrashNavMapMain::OnEnter() { LogUnreconstructedState("CrashNavMapMain", "OnEnter"); }
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed CrashNavMapMain.
    void CrashNavMapMain::OnLeave() {}
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed CrashNavMapMain.
    void CrashNavMapMain::Update()  {}

    // ---- CN_PROFILE ---------------------------------------------------------------------
    // FLAG PC-platform leaf: placeholder for the un-reconstructed CrashNavProfile's wider
    // X360 Construct(id, fsm, ProfileManager&) -- forwards to the base 2-arg Construct and
    // deliberately never touches the profile manager (its backing object may be an
    // un-reconstructed shell).
    void CrashNavProfile::Construct(CgsID lId, CgsFsm::ScriptedFsm* lpFsm,
                                    ProfileManager& lrProfileManager)
    {
        (void)lrProfileManager;
        CgsGui::State::Construct(lId, lpFsm);
    }
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed CrashNavProfile.
    void CrashNavProfile::OnEnter() { LogUnreconstructedState("CrashNavProfile", "OnEnter"); }
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed CrashNavProfile.
    void CrashNavProfile::OnLeave() {}
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed CrashNavProfile.
    void CrashNavProfile::Update()  {}

    // ---- ON_GAME_ROOM -------------------------------------------------------------------
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed
    // OnlineGameRoomPlayerInfo (renamed +State here; see the header's ODR note).
    void OnlineGameRoomPlayerInfoState::OnEnter() { LogUnreconstructedState("OnlineGameRoomPlayerInfoState", "OnEnter"); }
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed OnlineGameRoomPlayerInfo.
    void OnlineGameRoomPlayerInfoState::OnLeave() {}
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed OnlineGameRoomPlayerInfo.
    void OnlineGameRoomPlayerInfoState::Update()  {}

    // ---- ON_TEAMS -----------------------------------------------------------------------
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed OnlineTeamSelection.
    void OnlineTeamSelection::OnEnter() { LogUnreconstructedState("OnlineTeamSelection", "OnEnter"); }
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed OnlineTeamSelection.
    void OnlineTeamSelection::OnLeave() {}
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed OnlineTeamSelection.
    void OnlineTeamSelection::Update()  {}

    // ---- RE_CLIPS -----------------------------------------------------------------------
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed ReplayClips.
    void ReplayClips::OnEnter() { LogUnreconstructedState("ReplayClips", "OnEnter"); }
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed ReplayClips.
    void ReplayClips::OnLeave() {}
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed ReplayClips.
    void ReplayClips::Update()  {}

    // ---- RE_CLIPS_ON --------------------------------------------------------------------
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed ReplayClipsOnline.
    void ReplayClipsOnline::OnEnter() { LogUnreconstructedState("ReplayClipsOnline", "OnEnter"); }
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed ReplayClipsOnline.
    void ReplayClipsOnline::OnLeave() {}
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed ReplayClipsOnline.
    void ReplayClipsOnline::Update()  {}

    // ---- RE_OPTIONS ---------------------------------------------------------------------
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed ReplayOptions.
    void ReplayOptions::OnEnter() { LogUnreconstructedState("ReplayOptions", "OnEnter"); }
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed ReplayOptions.
    void ReplayOptions::OnLeave() {}
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed ReplayOptions.
    void ReplayOptions::Update()  {}

    // ---- RE_INTRO -----------------------------------------------------------------------
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed ReplayIntro.
    void ReplayIntro::OnEnter() { LogUnreconstructedState("ReplayIntro", "OnEnter"); }
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed ReplayIntro.
    void ReplayIntro::OnLeave() {}
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed ReplayIntro.
    void ReplayIntro::Update()  {}

    // ---- RE_CREDITS ---------------------------------------------------------------------
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed ReplayCredits.
    void ReplayCredits::OnEnter() { LogUnreconstructedState("ReplayCredits", "OnEnter"); }
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed ReplayCredits.
    void ReplayCredits::OnLeave() {}
    // FLAG PC-platform leaf: placeholder lifecycle for the un-reconstructed ReplayCredits.
    void ReplayCredits::Update()  {}
}
