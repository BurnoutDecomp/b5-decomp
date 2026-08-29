// ===========================================================================
// BrnScreenStatesDataLinkStubs.cpp -- link scaffold for the SCREEN/PostEvent
// state surface the BrnScreenFlow closure pulls in but whose per-state TUs are
// only partially reconstructed (2026-07-12). Two kinds of content:
//
//   * The per-state static RESOURCE TABLES (maResourcesToLoad /
//     maResourceTuplesToLoad / KA_RESOURCES_TO_LOAD + their counts). The IDA
//     exports are function-only, so every table below was read STRAIGHT FROM
//     THE DECRYPTED XEX at the address its declaring header documents -- these
//     are real reconstructions, not placeholders (every entry requests type
//     4 == CgsGui::E_GUI_RESOURCETYPE_APT). (PauseScreen's table, once the odd
//     one out, is now attested + defined by its own TU, BrnPauseScreen.cpp.)
//
//   * The LIFECYCLE VIRTUALS (OnEnter/OnLeave/Update/GetResourcesToLoad and
//     one menu hook) of states whose own TUs have not landed (or landed
//     partial -- per the campaign rule the partial TUs are NOT edited; their
//     missing pieces live here). These log once on entry and are otherwise
//     inert, the BrnHudStatesLinkStubs pattern. FLAG link scaffold: every
//     function body below is a stand-in, not a reconstruction (the real X360
//     bodies are noted where their ledger addresses are known).
// ===========================================================================

#include <cstdio>   // std::snprintf (the one-shot gap log)

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::WriteToLog

#include "GameSource/Gui/Flow/Screen/States/BrnBrnDebug.h"
#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectOnlineEnd.h"
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavAccountManagement.h"
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavColourCalibrate.h"
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavDriverDetails.h"
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnline.h"
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnlineMod.h"
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavOptions.h"
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavSettings.h"
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavStats.h"
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavTrax.h"
#include "GameSource/Gui/Flow/Screen/States/BrnCredits.h"
#include "GameSource/Gui/Flow/Screen/States/BrnImageGallery.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineCreateFreeburn.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineCustomMatch.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptionsSummary.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineLoading.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineNews.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlinePause.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlinePlay.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlinePreEvent.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineQuickCustomCreate.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineQuickMatch.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineRivals.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineSelectRoute.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineStats.h"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineViewChallenges.h"
#include "GameSource/Gui/Flow/Screen/States/OnlineYouWin.h"
#include "GameSource/Gui/Flow/PostEvent/States/Offline/BrnCompletedGame.h"
#include "GameSource/Gui/Flow/PostEvent/States/Offline/BrnOfflineInstantResults.h"
#include "GameSource/Gui/Flow/PostEvent/States/Offline/BrnOfflineRivalShutdown.h"
#include "GameSource/Gui/Flow/PostEvent/States/Offline/BrnOfflineTrophyCarUnlock.h"
#include "GameSource/Gui/Flow/PostEvent/States/Online/BrnOnlineInstantResults.h"
#include "GameSource/Gui/Flow/PostEvent/States/Showtime/BrnShowtimeInstantResults.h"
#include "GameSource/Gui/Flow/Screen/Components/BrnImageGallerySelectable.h"

namespace
{
    void LogUnreconstructedState(const char* lpacState, const char* lpacHook)
    {
        char lac[128];
        // (one line per state entry; these states are post-boot territory)
        std::snprintf(lac, sizeof(lac), "[ScreenFlow] %s::%s -- un-reconstructed state (FLAG).\n",
                      lpacState, lpacHook);
        CgsDev::Log::WriteToLog(lac);
    }
}

namespace BrnGui
{
    // =======================================================================
    //  Static resource tables -- values read from the decrypted XEX at the
    //  address each declaring header documents. Type 4 ==
    //  CgsGui::E_GUI_RESOURCETYPE_APT throughout.
    // =======================================================================

    // .rdata @0x8205E714 / count @0x8205E724
    const CgsGui::sResourceTuple CarSelectOnlineEnd::maResourcesToLoad[] =
        { { 151, CgsGui::E_GUI_RESOURCETYPE_APT }, { 94, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const u32 CarSelectOnlineEnd::muNumResourcesToLoad = 2;

    // .rdata @0x82F26FDC / count @0x82F26FE4
    const CgsGui::sResourceTuple CrashNavAccountManagement::maResourcesToLoad[] =
        { { 142, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const u32 CrashNavAccountManagement::muNumResourcesToLoad = 1;

    // .rdata @0x82F27008 / count @0x82F27018
    const CgsGui::sResourceTuple CrashNavColourCalibrate::maResourcesToLoad[] =
        { { 143, CgsGui::E_GUI_RESOURCETYPE_APT }, { 34, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const u32 CrashNavColourCalibrate::muNumResourcesToLoad = 2;

    // CrashNavDriverDetails's resource table has MOVED to its own TU
    // (BrnCrashNavDriverDetails.cpp, pause wave 2026-08-28) along with the rest of the
    // screen. The values here were right -- {144, APT}, {63, APT} -- and are carried over
    // unchanged; only the home changed, so this stand-in would now be a duplicate symbol.

    // .rdata @0x82066114 / count @0x8206611C
    const CgsGui::sResourceTuple CrashNavEnterOnlineBase::maResourceTuplesToLoad[] =
        { { 165, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const s32 CrashNavEnterOnlineBase::miNumResourcesToLoad = 1;

    // .rdata @0x82F26F50 / count @0x82F26F58
    const CgsGui::sResourceTuple CrashNavOptions::maResourcesToLoad[1] =
        { { 141, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const u32 CrashNavOptions::muNumResourcesToLoad = 1;

    // .rdata @0x820663C0 / count @0x820663C8
    const CgsGui::sResourceTuple CrashNavSettings::maResourceTuplesToLoad[] =
        { { 139, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const s32 CrashNavSettings::miNumResourcesToLoad = 1;

    // .rdata @0x82F26D88 / count @0x82F26D90
    const CgsGui::sResourceTuple CrashNavStats::maResourcesToLoad[] =
        { { 137, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const u32 CrashNavStats::muNumResourcesToLoad = 1;

    // .rdata @0x82F27278 / count @0x82F27288
    const CgsGui::sResourceTuple CrashNavTrax::maResourcesToLoad[] =
        { { 145, CgsGui::E_GUI_RESOURCETYPE_APT }, { 86, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const u32 CrashNavTrax::muNumResourcesToLoad = 2;

    // .rdata @0x82066654 / count @0x8206665C
    const CgsGui::sResourceTuple Credits::maResourcesToLoad[] =
        { { 154, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const u32 Credits::muNumResourcesToLoad = 1;

    // .rodata @0x8205E608 (the fixed one-entry list; no count static)
    const CgsGui::sResourceTuple ImageGalleryState::KA_RESOURCES_TO_LOAD[1] =
        { { 164, CgsGui::E_GUI_RESOURCETYPE_APT } };

    // (InstantResultsState's own table @0x82F26AFC is already defined by its
    // committed TU, BrnOfflineInstantResults.cpp -- not duplicated here.)

    // .rdata @0x82F27318 / count @0x82066898
    const CgsGui::sResourceTuple OfflineRivalShutdown::maResourcesToLoad[] =
        { { 224, CgsGui::E_GUI_RESOURCETYPE_APT }, { 59, CgsGui::E_GUI_RESOURCETYPE_APT },
          {  29, CgsGui::E_GUI_RESOURCETYPE_APT }, { 55, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const u32 OfflineRivalShutdown::muNumResourcesToLoad = 4;

    // .rdata @0x82F27338 / count @0x82066928
    const CgsGui::sResourceTuple OfflineTrophyCarUnlock::maResourcesToLoad[] =
        { { 225, CgsGui::E_GUI_RESOURCETYPE_APT }, { 59, CgsGui::E_GUI_RESOURCETYPE_APT },
          {  29, CgsGui::E_GUI_RESOURCETYPE_APT }, { 55, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const u32 OfflineTrophyCarUnlock::muNumResourcesToLoad = 4;

    // .rdata @0x8205DE98 / count @0x8205DEA0
    const CgsGui::sResourceTuple OnlineInstantResultsState::maResourceTuplesToLoad[] =
        { { 226, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const s32 OnlineInstantResultsState::miNumResourcesToLoad = 1;

    // .rdata @0x8205F880 / count @0x8205F890
    const CgsGui::sResourceTuple OnlineCreateFreeburn::maResourceTuplesToLoad[] =
        { { 176, CgsGui::E_GUI_RESOURCETYPE_APT }, { 191, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const s32 OnlineCreateFreeburn::miNumResourcesToLoad = 2;

    // .rdata @0x8205E77C / count @0x8205E784
    const CgsGui::sResourceTuple OnlineCustomMatch::maResourceTuplesToLoad[] =
        { { 175, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const s32 OnlineCustomMatch::miNumResourcesToLoad = 1;

    // .rdata @0x8205F004 / count @0x8205F014
    const CgsGui::sResourceTuple OnlineGameOptions::maResourceTuplesToLoad[] =
        { { 176, CgsGui::E_GUI_RESOURCETYPE_APT }, { 191, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const s32 OnlineGameOptions::miNumResourcesToLoad = 2;

    // .rdata @0x8205F1FC / count @0x8205F20C
    const CgsGui::sResourceTuple OnlineGameOptionsSummary::maResourceTuplesToLoad[] =
        { { 177, CgsGui::E_GUI_RESOURCETYPE_APT }, { 191, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const s32 OnlineGameOptionsSummary::miNumResourcesToLoad = 2;

    // .rdata @0x8205EE70 / count @0x8205EE80
    const CgsGui::sResourceTuple OnlineLoading::maResourceTuplesToLoad[] =
        { { 168, CgsGui::E_GUI_RESOURCETYPE_APT }, { 191, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const s32 OnlineLoading::miNumResourcesToLoad = 2;

    // .rdata @0x8205F810 / count @0x8205F818
    const CgsGui::sResourceTuple OnlineNews::maResourceTuplesToLoad[] =
        { { 181, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const s32 OnlineNews::miNumResourcesToLoad = 1;

    // .rdata @0x8205EF4C / count @0x8205EF54
    const CgsGui::sResourceTuple OnlinePause::maResourceTuplesToLoad[] =
        { { 169, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const s32 OnlinePause::miNumResourcesToLoad = 1;

    // .rdata @0x8205EF88 / count @0x8205EFA0
    const CgsGui::sResourceTuple OnlinePlay::maResourceTuplesToLoad[] =
        { { 172, CgsGui::E_GUI_RESOURCETYPE_APT }, { 190, CgsGui::E_GUI_RESOURCETYPE_APT },
          { 189, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const s32 OnlinePlay::miNumResourcesToLoad = 3;

    // .rdata @0x8205F994 / count @0x8205F99C
    const CgsGui::sResourceTuple OnlinePreEvent::maResourcesToLoad[] =
        { { 185, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const u32 OnlinePreEvent::muNumResourcesToLoad = 1;

    // .rdata @0x8205F9C8 / count @0x8205F9D8
    const CgsGui::sResourceTuple OnlineQuickCustomCreate::maResourcesToLoad[] =
        { { 173, CgsGui::E_GUI_RESOURCETYPE_APT }, { 190, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const u32 OnlineQuickCustomCreate::muNumResourcesToLoad = 2;

    // .rdata @0x8205F71C / count @0x8205F724
    const CgsGui::sResourceTuple OnlineQuickMatch::maResourceTuplesToLoad[] =
        { { 174, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const s32 OnlineQuickMatch::miNumResourcesToLoad = 1;

    // .rdata @0x8205F854 / count @0x8205F85C
    const CgsGui::sResourceTuple OnlineRivals::maResourceTuplesToLoad[] =
        { { 180, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const s32 OnlineRivals::miNumResourcesToLoad = 1;

    // .rdata @0x8205F67C / count @0x8205F684
    const CgsGui::sResourceTuple OnlineScoreboards::maResourcesToLoad[] =
        { { 182, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const u32 OnlineScoreboards::muNumResourcesToLoad = 1;

    // .rdata @0x8205F338 / count @0x8205F348
    const CgsGui::sResourceTuple OnlineSelectRoute::maResourceTuplesToLoad[] =
        { { 178, CgsGui::E_GUI_RESOURCETYPE_APT }, { 191, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const s32 OnlineSelectRoute::miNumResourcesToLoad = 2;

    // .rdata @0x8205FA1C / count @0x8205FA24
    const CgsGui::sResourceTuple OnlineStats::maResourcesToLoad[] =
        { { 186, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const u32 OnlineStats::muNumResourcesToLoad = 1;

    // .rdata @0x8205FAEC / count @0x8205FAF4
    const CgsGui::sResourceTuple OnlineViewChallenges::maResourceTuplesToLoad[] =
        { { 187, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const s32 OnlineViewChallenges::miNumResourcesToLoad = 1;

    // .rdata @0x8205FB64 / count @0x8205FB70
    const CgsGui::sResourceTuple OnlineYouWin::maResourcesToLoad[] =
        { { 171, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const u32 OnlineYouWin::muNumResourcesToLoad = 1;

    // .rdata @0x82F26BB8 / count @0x82F26BD0
    const CgsGui::sResourceTuple ShowtimeInstantResultsState::maResourcesToLoad[] =
        { { 217, CgsGui::E_GUI_RESOURCETYPE_APT }, { 70, CgsGui::E_GUI_RESOURCETYPE_APT },
          {  55, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const u32 ShowtimeInstantResultsState::muNumResourcesToLoad = 3;

    // =======================================================================
    //  Lifecycle scaffold -- FLAG stand-ins for the not-yet-landed bodies
    //  (real X360 addresses noted where the ledger names them).
    // =======================================================================

    // ---- BrnDebug (BrnBrnDebug.cpp is partial: OnEnter/OnLeave landed) ----------------
    void BrnDebug::Update() {}
    void BrnDebug::GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                      u32* lpuNumberOfResources) const
    {
        // FLAG: unrecovered resource list (DWARF h:60); empty until the real body lands.
        *lppResourceTuples    = 0;
        *lpuNumberOfResources = 0;
    }

    // ---- CarSelectOnlineEnd -----------------------------------------------------------
    void CarSelectOnlineEnd::OnEnter() { LogUnreconstructedState("CarSelectOnlineEnd", "OnEnter"); }
    void CarSelectOnlineEnd::OnLeave() {}
    void CarSelectOnlineEnd::Update()  {}

    // ---- CompletedGame ----------------------------------------------------------------
    void CompletedGame::OnEnter() { LogUnreconstructedState("CompletedGame", "OnEnter"); }
    void CompletedGame::OnLeave() {}
    void CompletedGame::Update()  {}

    // ---- CrashNavAccountManagement ----------------------------------------------------
    void CrashNavAccountManagement::OnEnter() { LogUnreconstructedState("CrashNavAccountManagement", "OnEnter"); }
    void CrashNavAccountManagement::OnLeave() {}
    void CrashNavAccountManagement::Update()  {}

    // ---- CrashNavColourCalibrate: the three lifecycle virtuals moved to the state's own
    //      TU (BrnCrashNavColourCalibrate.cpp, post-fx step 11). Only its static resource
    //      table stays here, with the rest of the measured .rdata block above.

    // ---- CrashNavEnterOnline variants (Mod TU landed NoTitle only) --------------------
    void CrashNavEnterOnlineFull::OnEnter() { LogUnreconstructedState("CrashNavEnterOnlineFull", "OnEnter"); }
    void CrashNavEnterOnlineX360::OnEnter() { LogUnreconstructedState("CrashNavEnterOnlineX360", "OnEnter"); }

    // ---- Credits (BrnCredits.cpp is partial: OnEnter landed) --------------------------
    void Credits::OnLeave() {}
    void Credits::Update()  {}

    // ---- InstantResultsState: THE THREE LIFECYCLE STUBS ARE GONE -------------------
    // ⭐⭐ OnEnter / OnLeave / Update are now REAL, in
    // GameSource/Gui/Flow/PostEvent/States/Offline/BrnOfflineInstantResults.cpp. Those three
    // stubs were why finishing an offline event produced no pixels: the whole chain in front
    // of them (FinishCurrentMode -> ShowModeResults -> action 37 -> GUI 291 -> InGame::Update
    // case 291 -> SendStateEvent("TO_OFF_POST")) succeeded and then landed on a log line.
    //
    // What stays here is the OTHER half of the same lesson. The class has 32 functions; this
    // wave bodied 17. The remaining 15 are declared in BrnOfflineInstantResults.h so the
    // reconstructed dispatch can name them, and they are bodied HERE -- logged, not silent --
    // so a run that reaches one says so in BrnGame.log instead of quietly doing nothing.
    // ⛔ DO NOT "tidy" these into empty bodies. A silent no-op is exactly what made the
    // original defect invisible to the link, to the ledger and to four run logs.
    //
    // NONE of these is on the path that puts the results movie on screen -- that is Update's
    // E_RESULTS_STATE_LOADING_RESOURCES arm (two PlayAptMovie calls), which runs before any
    // sub-state does. These are the sub-state PRESENTATIONS and the component fill.
    void InstantResultsState::SetupComponents()  { LogUnreconstructedState("InstantResultsState", "SetupComponents"); }
    void InstantResultsState::HandleAptTriggers(const void*)   { LogUnreconstructedState("InstantResultsState", "HandleAptTriggers"); }
    void InstantResultsState::HandleControllerInput(const void*) { LogUnreconstructedState("InstantResultsState", "HandleControllerInput"); }
    void InstantResultsState::UpdateEventResults()      { LogUnreconstructedState("InstantResultsState", "UpdateEventResults"); }
    void InstantResultsState::UpdateSecondResultsPage() { LogUnreconstructedState("InstantResultsState", "UpdateSecondResultsPage"); }
    void InstantResultsState::UpdateTakePhotoPage()     { LogUnreconstructedState("InstantResultsState", "UpdateTakePhotoPage"); }
    void InstantResultsState::UpdateRankUp()            { LogUnreconstructedState("InstantResultsState", "UpdateRankUp"); }
    void InstantResultsState::UpdateLicense()           { LogUnreconstructedState("InstantResultsState", "UpdateLicense"); }
    void InstantResultsState::UpdateCarUnlock()         { LogUnreconstructedState("InstantResultsState", "UpdateCarUnlock"); }
    void InstantResultsState::UpdateFreeCarUnlock()     { LogUnreconstructedState("InstantResultsState", "UpdateFreeCarUnlock"); }
    void InstantResultsState::UpdateShowingRivals()     { LogUnreconstructedState("InstantResultsState", "UpdateShowingRivals"); }
    void InstantResultsState::UpdateLeaving()           { LogUnreconstructedState("InstantResultsState", "UpdateLeaving"); }
    void InstantResultsState::UpdatePhoto()             { LogUnreconstructedState("InstantResultsState", "UpdatePhoto"); }
    // ⚠️ RETURN VALUES ARE NOT NEUTRAL, so both are stated rather than left to a bare `false`:
    //  * IsXSCarInUnlockedArray false  => SelectSubstates does not raise CAR_UNLOCK. That is
    //    the ordinary case (no XS car unlocked), so it degrades to "no car-unlock page".
    //  * WillShowCredits false => the results screen ADVANCEs instead of handing over to the
    //    credits. Wrong ONLY on the run that completes the final licence rank. The real body
    //    needs BrnGui::WorldDataController::GetProgressionData, which has no home in the tree.
    bool InstantResultsState::IsXSCarInUnlockedArray()  { LogUnreconstructedState("InstantResultsState", "IsXSCarInUnlockedArray"); return false; }
    bool InstantResultsState::WillShowCredits()         { LogUnreconstructedState("InstantResultsState", "WillShowCredits"); return false; }

    // ---- OnlineGameOptionsSummary ------------------------------------------------------
    void OnlineGameOptionsSummary::OnEnter() { LogUnreconstructedState("OnlineGameOptionsSummary", "OnEnter"); }
    void OnlineGameOptionsSummary::OnLeave() {}
    void OnlineGameOptionsSummary::Update()  {}

    // ---- OnlineLoading -----------------------------------------------------------------
    void OnlineLoading::OnEnter() { LogUnreconstructedState("OnlineLoading", "OnEnter"); }
    void OnlineLoading::OnLeave() {}
    void OnlineLoading::Update()  {}

    // ---- OnlineNews --------------------------------------------------------------------
    void OnlineNews::OnEnter() { LogUnreconstructedState("OnlineNews", "OnEnter"); }
    void OnlineNews::OnLeave() {}
    void OnlineNews::Update()  {}

    // ---- OnlineQuickCustomCreate menu hook ----------------------------------------------
    void OnlineQuickCustomCreate::ProcessSelectedMenuOption(EMainMenuOptions /*leOption*/)
    {
        LogUnreconstructedState("OnlineQuickCustomCreate", "ProcessSelectedMenuOption");
    }

    // (PauseScreen's full surface -- OnEnter/OnLeave/Update/GetResourcesToLoad --
    //  landed in its own TU, BrnPauseScreen.cpp; nothing of it lives here any more.)

    // ---- ShowtimeInstantResultsState (BrnShowtimeInstantResults.cpp is partial) -------
    void ShowtimeInstantResultsState::OnEnter() { LogUnreconstructedState("ShowtimeInstantResultsState", "OnEnter"); }
    void ShowtimeInstantResultsState::OnLeave() {}
    void ShowtimeInstantResultsState::Update()  {}

    // ---- ImageGallerySelectable::Select (component; BrnImageGallerySelectable.cpp is
    //      partial -- Construct/Update/HandleLoadNotifications landed, the Select
    //      override has no X360 export of its own) ------------------------------------
    void ImageGallerySelectable::Select() {}
}
