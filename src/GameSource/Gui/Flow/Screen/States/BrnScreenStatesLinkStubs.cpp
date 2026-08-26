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
    // ⭐⭐ PARTIAL RECONSTRUCTION (pause wave, 2026-08-26), NOT a stub any more -- and the
    // reason it is worth doing before the map itself exists is that ENTERING THE OFFLINE MAP
    // *IS* THE OFFLINE PAUSE. Two banners are RETRACTED by this:
    //   * hudscope_log's "OFFLINE THERE IS NO PAUSE MENU ... Map does NOT freeze the sim on
    //     console" -- false.
    //   * the memory note "activating the map UNpauses; the deactivate pauses" -- the first
    //     half is false.
    // CrashNavMapMain::OnEnter @0x824CC9E8, read as ASM (not pseudocode; the stack slots
    // var_40..var_34 at 0x824CCA2C-0x824CCA50) posts TWO records:
    //     { 8, 191, 12, 0, 0 } ch 40 size 20  == GuiEventActivateCrashNav(FALSE)
    //     { 4,  45, 12, 1    } ch 40 size 16  == GuiEventNetworkSuspension(TRUE)
    // The first is byte-identical to BrnOnlinePlay.cpp:172's "Deactivate CrashNav"; its
    // ACTIVATE twin { 8, 191, 12, 1, 0 } is what BrnInGame.cpp:381 posts. Both are reached
    // here BY NAME through the types this tree already owns, not as hand-built records.
    // The 191{0} is what GameBridgeGUIToX_GameState's case 191 turns into game event 93
    // payload 1 -> RequestPause(4) -> action 86 -> mbSimPaused = 1.
    //
    // ⛔ STILL PARKED: the CrashNavMap BASE half (the class really derives from CrashNavMap,
    // whose 19 written bodies are UNMOUNTED and whose ctor/OnLeave/PlaceCursorOnPlayer/
    // SetFilterFromPanel do not exist). NO MAP IS DRAWN. This state currently freezes the
    // world and shows the frozen frame; the map quad is Wave B/C.
    // The 19 observed GUI event ids, read out of .rdata at dword_82066358 (the exact pointer
    // CrashNavMapMain::OnEnter @0x824CCA0C and OnLeave @0x824CCA98 both hand to
    // (Un)RegisterForEvents with a count of 19). Id 6 == KI_EVENT_CONTROLLER is the first entry
    // and is the one the exit arm below needs.
    const s32 CrashNavMapMain::maiEventToObserve[19] =
    {
        6, 7, 8, 14, 16, 43, 44, 202, 224, 213, 199, 64, 436, 334, 516, 438, 189, 344, 332
    };
    const s32 CrashNavMapMain::miNumEventsObserved = 19;

    void CrashNavMapMain::OnEnter()
    {
        // 0x824CCA0C -- REGISTER FIRST. ⭐⭐ Omitting this was a MEASURED defect and
        // not a cosmetic one: run cc_pause2 paused correctly and then could never come back,
        // because a state that observes nothing receives nothing, so the exit arm in Update
        // below was dead code no matter how faithfully it was written. The pause was ONE-WAY.
        // ⭐ The lesson, which this campaign keeps re-learning: A BODIED CONSUMER IS NOT A
        // REACHED CONSUMER. Check what DELIVERS to it, not just that it exists.
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // 0x824CCA44/4C/50 -- the deactivate record.
        GuiEventActivateCrashNav lDeactivate(false);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lDeactivate), 40,
            static_cast<s32>(sizeof(GuiEventActivateCrashNav)));

        // The second record: { 4, 45, 12, 1 } == GuiEvent<45> == GuiEventNetworkSuspension(true).
        CgsGui::GuiEventNetworkSuspension lSuspend(true);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lSuspend), 40,
            static_cast<s32>(sizeof(CgsGui::GuiEventNetworkSuspension)));

        // ⛔ NOT reproduced: the console's `*(this+24928) = 1` and `*(this+56) = 1` both land
        // past sizeof(CgsGui::State) == 56, i.e. inside the CrashNavMap base this declaration
        // does not have. Writing them here would corrupt whatever follows the object.
        //
        // One-shot: the base half is not reconstructed, so say so rather than look complete.
        static bool sbLoggedBasePark = false;
        if (!sbLoggedBasePark)
        {
            sbLoggedBasePark = true;
            LogUnreconstructedState("CrashNavMapMain", "OnEnter[CrashNavMap base half PARKED -- no map drawn]");
        }
    }

    // @0x824CCA98 -- PARTIAL. The console does three things; the first is reproduced and the
    // other two belong to the parked halves:
    //     UnRegisterForEvents(maiEventToObserve, 19)          <- reproduced (symmetric with OnEnter)
    //     CrashNavPanel::StoreSettings(this + 1760, 0)        <- ⛔ PARKED (CrashNavPanel is 4-of-~20)
    //     CrashNavMap::OnLeave(this)                          <- ⛔ PARKED (the base half)
    // The unregister is NOT optional bookkeeping: the observer table is 4 slots wide
    // (CgsGui::KI_MAX_OBSERVERS), so registering on every entry without releasing would run it
    // out after four visits to the map.
    void CrashNavMapMain::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
    }

    // The EXIT arm, from CrashNavMapMain::HandleCrashNavInputPressed @0x824CCAE8, cases
    // 0x2D (45) and 0x32 (50). The console reaches it from the base spine's event walk; this
    // partial drains the in-queue directly for the ONE event that matters
    // (6 == KI_EVENT_CONTROLLER, action sub-id at payload +4, the same layout
    // InGame::HandleControllerInput reads) and runs the console's four steps in order.
    void CrashNavMapMain::Update()
    {
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        if (lpInQueue == 0)
            return;

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
            if (liAction != 45 && liAction != 50)
                continue;

            // 0x824CCAE8 case 0x2D / 0x32, in the console's own order.
            CgsGui::GuiEventNetworkSuspension lResume(false);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lResume), 40,
                static_cast<s32>(sizeof(CgsGui::GuiEventNetworkSuspension)));

            GuiEventActivateCrashNav lActivate(true);          // <- THE UNPAUSE
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lActivate), 40,
                static_cast<s32>(sizeof(GuiEventActivateCrashNav)));

            // { 1, 533, 12 } ch 40 size 16.
            CgsGui::GuiEvent<533> lDone(1, 12);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lDone), 40, 16);

            SendStateEvent("GO_BACK");                         // CN_MAP_MAIN(5) -> INGAME(4)
            break;
        }

        // The base spine clears its own queue at the end of the walk.
        lpInQueue->Clear();
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

    // =====================================================================================
    // ⛔ THE SAME REGRESSION, A THIRD TIME -- ON_CUST_MAT, ADDED 2026-08-03.
    //
    // b5-decomp a1fec0e9 ("Gui components: grow thirteen thin header slices to their DWARF
    // shapes") grew BrnOnlineCustomMatch.h from a one-accessor slice to the full DWARF class
    // and, in doing so, DECLARED OnEnter/OnLeave/Update virtual. Its own trailing comments say
    // where the bodies are ("FOREIGN TU ... defined nowhere yet"), and grep over all of src/
    // confirms it: there is no BrnOnlineCustomMatch.cpp in the tree at all, so ALL THREE are
    // undefined. BrnScreenFlow.cpp:178 instantiates the class through NewPoolState<T>, which
    // materialises the vtable, and the vtable references every virtual -- so from that commit
    // on the exe failed to link with LNK2001 x3 (plus the Table::Table() LNK2019 the same
    // header's by-value BrnGui::Table member introduced; that one is a MOUNT, see the build
    // script). Identical shape to the CN_ENTER_ONLINE / ONLINE_GAME_OPTIONS /
    // ONLINE_SCOREBOARDS block above, same minimum fix, same zero behaviour change: ON_CUST_MAT
    // is online-only and unreachable on this build, and the pre-a1fec0e9 shape inherited these
    // three from CgsFsm::State, whose bodies are empty.
    //
    // ⚠️ DELETE-WHEN, PER SYMBOL. The moment a TU defining any of these three lands AND IS
    // MOUNTED, the matching stub here MUST be deleted or the link fails the other way
    // (LNK2005). One of the three already has its real body in the tree -- see below. The
    // console addresses are on the declarations in BrnOnlineCustomMatch.h (OnEnter
    // @0x82496C10, OnLeave @0x824970D0, Update @0x824AC808).
    // =====================================================================================

    // ---- ON_CUST_MAT --------------------------------------------------------------------
    // FLAG link scaffold: REAL BODY EXISTS, unmounted -- b5-decomp 62f56950 landed it at
    // BrnOnlineCustomMatch_wJ_06.cpp:214 while this stub was being written. Mounting that TU
    // (and its five siblings) REQUIRES deleting this line.
    void OnlineCustomMatch::OnEnter() { LogUnreconstructedState("OnlineCustomMatch", "OnEnter"); }
    // FLAG link scaffold: no definition anywhere in src/ (@0x824970D0, foreign ledger TU).
    void OnlineCustomMatch::OnLeave() {}
    // FLAG link scaffold: no definition anywhere in src/ (@0x824AC808, foreign ledger TU).
    void OnlineCustomMatch::Update() {}

    // =====================================================================================
    // ⛔ THE SAME REGRESSION, A FOURTH TIME -- ONLINE_PLAY / ON_SELECT_ROUTE, ADDED 2026-08-07
    // (the dev->physics merge).
    //
    // dev 4ee3195e ("Gui: restore two screen headers a merge resolved to the wrong side")
    // grew BrnOnlinePlay.h / BrnOnlineSelectRoute.h back to their full shapes, declaring
    // out-of-line ctors (and, for OnlinePlay, the OnEnter/OnLeave/Update virtuals).
    // BrnScreenFlow.cpp's NewPoolState<T> materialises both, so the exe needs the symbols.
    // Real reconstructions exist -- BrnOnlinePlay.cpp (dev 2b505a01) and
    // BrnOnlineSelectRoute.cpp -- but NEITHER TU closes at link (2026-08-07, trial-mounted):
    //     BrnOnlinePlay.cpp     -> OnlinePlay::Update + OnlinePlay::ShowFriendsMenu have no
    //                              definition anywhere in src/ (the header's "body links from
    //                              another slice" note is not yet true), and it also needs
    //                              GuiCache::IsMultiplayerAllowed, NetworkPlayerStats::Construct
    //                              and MenuComponent::AppendExpectedAptComponent, all undefined.
    //     BrnOnlineSelectRoute.cpp -> its ctor writes through eight BrnGui::gp*VTable image
    //                              globals (gpOnlineSelectRouteVTable et al.) that no TU
    //                              defines, and runs the unmounted MapManager ctor.
    // Same rationale as the three blocks above, same zero behaviour change: both screens are
    // online-only and unreachable on this build, and their pre-4ee3195e shapes had no
    // user-declared ctor. Both states' static resource tables already live in
    // BrnScreenStatesDataLinkStubs.cpp (no data stubs needed here).
    //
    // ⚠️ DELETE-WHEN, PER SYMBOL (LNK2005 otherwise): mounting BrnOnlinePlay.cpp requires
    // deleting the four OnlinePlay stubs below; mounting BrnOnlineSelectRoute.cpp requires
    // deleting the OnlineSelectRoute ctor stub.
    // =====================================================================================

    // ---- ONLINE_PLAY --------------------------------------------------------------------
    // FLAG link scaffold: REAL BODY EXISTS, unmounted -- BrnOnlinePlay.cpp:126 (@0x82508B40).
    // Base + member default-construction is the recovered effect of the real ctor anyway.
    OnlinePlay::OnlinePlay() {}
    // FLAG link scaffold: REAL BODY EXISTS, unmounted -- BrnOnlinePlay.cpp:136 (@0x8249BC18).
    void OnlinePlay::OnEnter() { LogUnreconstructedState("OnlinePlay", "OnEnter"); }
    // FLAG link scaffold: REAL BODY EXISTS, unmounted -- BrnOnlinePlay.cpp:181 (@0x8249BDA8).
    void OnlinePlay::OnLeave() {}
    // FLAG link scaffold: no definition anywhere in src/ (declared-only in BrnOnlinePlay.h).
    void OnlinePlay::Update() {}

    // ---- ON_SELECT_ROUTE ----------------------------------------------------------------
    // FLAG link scaffold: REAL BODY EXISTS, unmounted -- BrnOnlineSelectRoute.cpp:87
    // (@0x8251AE30; blocked on the undefined gp*VTable image globals it stores).
    OnlineSelectRoute::OnlineSelectRoute() {}
}
