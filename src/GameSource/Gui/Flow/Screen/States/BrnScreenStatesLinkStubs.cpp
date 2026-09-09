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
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                          // GuiEventActivateCrashNav
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface, GuiEventNetworkSuspension
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnline.h"
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnlineMod.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineCustomMatch.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlinePlay.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineSelectRoute.h"
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

    // ---- the PC bring-up ESCAPE HATCH (used by OnlinePlay below) ----------------------
    // FLAG link scaffold: a placeholder that observes nothing traps the player in an
    // invisible state. The hatch registers for the controller event and drains 45/50 ->
    // "GO_BACK", 54/55 -> "TOGGLE_LEFT"/"TOGGLE_RIGHT". The FSM owns where those lead;
    // InGame::OnEnter posts the ActivateCrashNav(true) resume, so a bare GO_BACK is safe.
    namespace
    {
        typedef CgsModule::VariableEventQueue<18432, 16> HatchInputQueue;
        const s32 KAI_HATCH_EVENTS[] = { 6 };   // KI_EVENT_CONTROLLER

        // First hatch-relevant action id (45/50/54/55) in the queue, else 0; Clear()s it.
        s32 HatchDrain(InputBuffer::GuiEventQueue* lpInQueueRaw)
        {
            HatchInputQueue* lpInQueue = reinterpret_cast<HatchInputQueue*>(lpInQueueRaw);
            if (lpInQueue == 0)
                return 0;

            s32 liMatched = 0;
            const CgsModule::Event* lpEvent = 0;
            s32 liSize = 0;
            for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
                 lpEvent != 0;
                 liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
            {
                if (liEventId != 6)
                    continue;
                const s32 liAction =
                    *reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpEvent) + 4);
                if (liMatched == 0 &&
                    (liAction == 45 || liAction == 50 || liAction == 54 || liAction == 55))
                {
                    liMatched = liAction;
                }
            }
            lpInQueue->Clear();
            return liMatched;
        }
    }

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
    // Link scaffold for the online screens BrnScreenFlow.cpp instantiates through
    // NewPoolState<T> (CrashNavEnterOnline, OnlineGameOptions, OnlineScoreboards): their
    // headers declare ctors and virtuals whose bodies are unmounted or absent. Defining a
    // ctor forces the class vtable, which references every virtual, hence the full lifecycle
    // set per class. All online-only, unreachable on this build.
    //
    // DELETE-WHEN, per symbol: mounting any of these TUs requires deleting the matching
    // stub here (LNK2005 otherwise):
    //     CrashNavEnterOnlineX360::OnLeave / ::ShowSignInUI
    //         -> src/GameSource/Gui/Flow/Screen/States/X360/BrnCrashNavEnterOnlineX360.cpp
    //     GuiNetworkRouteInfo::GuiNetworkRouteInfo
    //         -> src/GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkRouteInfo.cpp
    //     OnlineGameOptions::OnEnter
    //         -> src/GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions_wI_05.cpp:218
    // The rest have no definition anywhere in src/.
    // =====================================================================================

    // ---- CN_ENTER_ONLINE ----------------------------------------------------------------
    // FLAG link scaffold: no definition anywhere in src/ (ctor and Update).
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
    // (No ctor stub: OnlineGameOptions declares none; its by-value GuiNetworkRouteInfo and
    //  HelpBar members are stubbed at the bottom of this block.)
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

    // =====================================================================================
    // ONLINE_PLAY / ON_SELECT_ROUTE: BrnScreenFlow.cpp's NewPoolState<T> needs the symbols.
    // Real reconstructions exist (BrnOnlinePlay.cpp / BrnOnlineSelectRoute.cpp) but neither
    // TU closes at link:
    //     BrnOnlinePlay.cpp     -> OnlinePlay::Update + OnlinePlay::ShowFriendsMenu have no
    //                              definition anywhere in src/, and it also needs
    //                              NetworkPlayerStats::Construct and
    //                              MenuComponent::AppendExpectedAptComponent, both undefined.
    //     BrnOnlineSelectRoute.cpp -> its ctor writes through eight BrnGui::gp*VTable image
    //                              globals (gpOnlineSelectRouteVTable et al.) that no TU
    //                              defines, and runs the unmounted MapManager ctor.
    // Both screens are online-only and unreachable on this build.
    //
    // DELETE-WHEN (LNK2005 otherwise): mounting BrnOnlinePlay.cpp requires deleting the four
    // OnlinePlay stubs below; mounting BrnOnlineSelectRoute.cpp requires deleting the
    // OnlineSelectRoute ctor stub.
    // =====================================================================================

    // ---- ONLINE_PLAY --------------------------------------------------------------------
    // FLAG link scaffold: REAL BODY EXISTS, unmounted -- BrnOnlinePlay.cpp:126 (@0x82508B40).
    // Base + member default-construction is the recovered effect of the real ctor anyway.
    OnlinePlay::OnlinePlay() {}
    // FLAG link scaffold: REAL BODY EXISTS, unmounted -- BrnOnlinePlay.cpp:136 (@0x8249BC18).
    void OnlinePlay::OnEnter()
    {
        // ESCAPE HATCH: ON_PLAY sits on the offline CrashNav tab ring (CN_SETTINGS <-TOGGLE->
        // ON_PLAY <-TOGGLE-> CN_MAP_MAIN), so the stub must observe the controller or tabbing
        // onto it is a one-way trap.
        mpStateInterface->RegisterForEvents(KAI_HATCH_EVENTS, 1);
        LogUnreconstructedState("OnlinePlay", "OnEnter[escape hatch armed -- no screen drawn]");
    }
    // FLAG link scaffold: REAL BODY EXISTS, unmounted -- BrnOnlinePlay.cpp:181 (@0x8249BDA8).
    void OnlinePlay::OnLeave() { mpStateInterface->UnRegisterForEvents(KAI_HATCH_EVENTS, 1); }
    // FLAG link scaffold: no definition anywhere in src/ (declared-only in BrnOnlinePlay.h).
    void OnlinePlay::Update()
    {
        switch (HatchDrain(mpInGuiEventQueue))
        {
        case 45: case 50: SendStateEvent("GO_BACK");      break;
        case 54:          SendStateEvent("TOGGLE_LEFT");  break;
        case 55:          SendStateEvent("TOGGLE_RIGHT"); break;
        default:          break;
        }
    }

    // ---- ON_SELECT_ROUTE ----------------------------------------------------------------
    // FLAG link scaffold: REAL BODY EXISTS, unmounted -- BrnOnlineSelectRoute.cpp:87
    // (@0x8251AE30; blocked on the undefined gp*VTable image globals it stores).
    OnlineSelectRoute::OnlineSelectRoute() {}
}
