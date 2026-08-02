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

// The three online-screen reconstructions whose declared-but-undefined ctors/virtuals this TU
// has to satisfy for the exe to link at all -- see the DELETE-WHEN block at the bottom.
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnline.h"
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnlineMod.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards.h"
#include "GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkRouteInfo.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnHelpBar.h"

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
    // (CarSelectLivery's placeholder lifecycle is GONE -- the real class landed 2026-08-02.)

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

    // =====================================================================================
    // ⛔ LINK SCAFFOLD FOR THE THREE ONLINE-SCREEN RECONSTRUCTIONS -- ADDED 2026-08-02.
    //
    // WHY THIS BLOCK EXISTS. The `Decomp: OnlineScoreboards` / `Decomp: CrashNavEnterOnlineBase`
    // / `Decomp: OnlineGameOptions` commits (f551f7da / e1c0b31a / 6e583e47) grew these three
    // leaf headers to their full DWARF shape and, in doing so, DECLARED constructors and
    // virtuals whose bodies live in foreign ledger TUs that do not exist yet. Their own commit
    // messages say so outright ("...are defined nowhere yet ... the screen does not link. All
    // declared, none stubbed."). BrnScreenFlow.cpp instantiates all three through
    // `NewPoolState<T>`, so from those commits on the whole exe fails to link with 10
    // unresolved externals the moment BrnScreenFlow.cpp is recompiled -- which the incremental
    // build only deferred, not avoided. This block is the minimum that restores a linkable
    // tree; it changes no behaviour, because none of these three screens is reachable on this
    // build (they are online-only) and because a class with no user-declared constructor -- the
    // shape these were in before -- left exactly the same members uninitialised.
    //
    // ⚠️ DELETE-WHEN, PER SYMBOL. Four of the eleven bodies below already exist in the tree but
    // in UNMOUNTED TUs; mounting those TUs REQUIRES deleting the matching stub here, or the
    // link fails the other way (LNK2005):
    //     CrashNavEnterOnlineX360::OnLeave / ::ShowSignInUI
    //         -> src/GameSource/Gui/Flow/Screen/States/X360/BrnCrashNavEnterOnlineX360.cpp
    //     GuiNetworkRouteInfo::GuiNetworkRouteInfo
    //         -> src/GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkRouteInfo.cpp
    //     OnlineGameOptions::OnEnter
    //         -> src/GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions_wI_05.cpp:218
    // The other seven have NO definition anywhere in the tree (verified by grep over all of
    // src/), so they are genuine gaps, not mounting gaps. HelpBar::HelpBar is the loudest of
    // them: BrnHelpBar.cpp:46 carries its own "BLOCKED: left declared-but-undefined" note.
    // =====================================================================================

    // ---- CN_ENTER_ONLINE ----------------------------------------------------------------
    // ⚠️ DEFINING THE CTOR IS WHAT FORCES THE WHOLE VTABLE. MSVC emits a class's vtable in the
    // TU that defines its constructor, and the vtable references EVERY virtual -- so the
    // lifecycle set below is not optional padding, it is the cost of the ctor above it. That
    // is also why this block is bigger than the linker's first error list: each round of stubs
    // materialised the next vtable.
    // FLAG link scaffold: no definition anywhere in src/ (ctor + the @0x824E0680 Update).
    CrashNavEnterOnlineBase::CrashNavEnterOnlineBase() {}
    // FLAG link scaffold: no definition anywhere in src/.
    void CrashNavEnterOnlineBase::Update() {}
    // FLAG link scaffold: REAL BODY EXISTS, unmounted -- BrnCrashNavEnterOnline_wI_06.cpp:178.
    void CrashNavEnterOnlineBase::OnEnter()
    { LogUnreconstructedState("CrashNavEnterOnlineBase", "OnEnter"); }
    // FLAG link scaffold: no definition anywhere in src/ (@0x824CAB88, foreign ledger TU).
    void CrashNavEnterOnlineBase::OnLeave() {}
    // FLAG link scaffold: REAL BODY EXISTS, unmounted -- X360/BrnCrashNavEnterOnlineX360.cpp.
    void CrashNavEnterOnlineX360::OnLeave() {}
    // FLAG link scaffold: REAL BODY EXISTS, unmounted -- X360/BrnCrashNavEnterOnlineX360.cpp.
    // The console returns the sign-in UI's result code; 0 is its "no UI was shown" value.
    u32 CrashNavEnterOnlineX360::ShowSignInUI() { return 0; }

    // ---- ONLINE_GAME_OPTIONS ------------------------------------------------------------
    // (No ctor stub: OnlineGameOptions declares none, so the compiler generates it inline.
    //  The LNK2019 the linker reported "in NewPoolState<OnlineGameOptions>" was for the two
    //  BY-VALUE members that generated ctor calls -- GuiNetworkRouteInfo and HelpBar, both
    //  stubbed at the bottom of this block -- NOT for a missing OnlineGameOptions ctor.)
    // FLAG link scaffold: REAL BODY EXISTS, unmounted -- BrnOnlineGameOptions_wI_05.cpp:218.
    void OnlineGameOptions::OnEnter() { LogUnreconstructedState("OnlineGameOptions", "OnEnter"); }
    // FLAG link scaffold: no definition anywhere in src/ (cpp:443, foreign ledger TU).
    void OnlineGameOptions::OnLeave() {}
    // FLAG link scaffold: no definition anywhere in src/ (@0x824AF688, foreign ledger TU).
    void OnlineGameOptions::Update() {}

    // ---- ONLINE_SCOREBOARDS -------------------------------------------------------------
    // FLAG link scaffold: no definition anywhere in src/ (the X360 body is compiler-synthesised).
    OnlineScoreboards::OnlineScoreboards() {}
    // FLAG link scaffold: REAL BODY EXISTS, unmounted -- BrnOnlineScoreboards_wI_03.cpp:224.
    void OnlineScoreboards::Construct(CgsID, CgsFsm::ScriptedFsm*) {}
    // FLAG link scaffold: REAL BODY EXISTS, unmounted -- BrnOnlineScoreboards_wI_05.cpp:175.
    void OnlineScoreboards::OnEnter()
    { LogUnreconstructedState("OnlineScoreboards", "OnEnter"); }
    // FLAG link scaffold: REAL BODY EXISTS, unmounted -- BrnOnlineScoreboards_wI_05.cpp:246.
    void OnlineScoreboards::OnLeave() {}
    // FLAG link scaffold: no definition anywhere in src/.
    void OnlineScoreboards::Update() {}

    // ---- leaderboard components embedded BY VALUE in OnlineScoreboards -------------------
    // FLAG link scaffold: REAL BODY EXISTS, unmounted --
    //   Screen/Components/BrnLeaderboardTableComponent.cpp:39 / BrnLeaderboardColumnComponent.cpp:30.
    void LeaderboardColumnComponent::Construct(const char*, CgsGui::StateInterface*, const char*) {}
    // FLAG link scaffold: REAL BODY EXISTS, unmounted -- BrnLeaderboardTableComponent.cpp:39.
    void LeaderboardTableComponent::Construct(const char*, CgsGui::StateInterface*, const char*) {}

    // ---- shared components pulled in by the three screens above --------------------------
    // FLAG link scaffold: REAL BODY EXISTS, unmounted -- Screen/Components/BrnGuiNetworkRouteInfo.cpp.
    GuiNetworkRouteInfo::GuiNetworkRouteInfo() {}
    // FLAG link scaffold: no definition anywhere in src/ -- BrnHelpBar.cpp:46 declares
    // HelpBar::HelpBar @0x82515328 BLOCKED and deliberately leaves it undefined.
    HelpBar::HelpBar() {}
}
