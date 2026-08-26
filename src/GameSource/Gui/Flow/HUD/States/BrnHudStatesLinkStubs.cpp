// ===========================================================================
// BrnHudStatesLinkStubs.cpp -- link scaffold for the IN-GAME HUD-flow states the
// BrnHudFlow 14-state pool instantiates but the boot/menu slice never enters
// (RACE_MAIN / FBURN_MAIN / CRASHEDSTNT). Their real per-frame bodies live in
// their own TUs (BrnRaceMainHudState.cpp / BrnFBurnMainHudState.cpp /
// BrnCrashedStuntHudState.cpp) against TU-local record types that predate the
// shared headers; homing them onto the header types is the "faithful all
// states" follow-on. Until then:
//   * the .rdata resource-tuple tables (no exported values -- IDA exports are
//     function-only) are empty FLAG'd placeholders;
//   * the lifecycle virtuals log once and return, so an early FSM handoff into
//     an un-reconstructed state leaves the game up and the gap visible.
// FLAG link scaffold: every body below is a stand-in, not a reconstruction.
// ===========================================================================

#include <cstdio>   // std::snprintf (the one-shot gap log)

#include "GameSource/Gui/Flow/HUD/States/BrnRaceMainHudState.h"
#include "GameSource/Gui/Flow/HUD/States/BrnFBurnMainHudState.h"
#include "GameSource/Gui/Flow/HUD/States/BrnCrashedStuntHudState.h"
#include "GameSource/Gui/Flow/PreEvent/States/BrnPreRaceFlyBy.h"   // PRE_FLY_BY scaffold (see block below)
#include "GameSource/Gui/Flow/HUD/Components/BrnChallengeSelector.h"     // HUD-component ctor cluster (see block below)
#include "GameSource/Gui/Flow/HUD/Components/BrnPaybackComponent.h"      // HUD-component ctor cluster
#include "GameSource/Gui/Flow/HUD/Components/BrnRoadRuleShotComponent.h" // HUD-component ctor cluster
#include "GameSource/Gui/SatNav/BrnMapManager.h"                         // MapManager ctor gate (see block below)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::WriteToLog

namespace
{
    void LogUnreconstructedState(const char* lpacState, const char* lpacHook)
    {
        char lac[128];
        // (one line per state entry; these states are post-boot territory)
        std::snprintf(lac, sizeof(lac), "[HudFlow] %s::%s -- un-reconstructed state (FLAG).\n",
                      lpacState, lpacHook);
        CgsDev::Log::WriteToLog(lac);
    }
}

namespace BrnGui
{
    // ---- RACE_MAIN ------------------------------------------------------------------
    // PAIRED-EDIT NOTE (2026-08-26): the REAL 21-entry table @0x82F25F88 is now recovered in
    // BrnRaceMainHudState.cpp (unmounted). Mounting that TU REQUIRES deleting these two
    // definitions in the same change, or LNK2005; until it mounts, these inert stubs are what
    // the exe links and the recovery is deliberately dormant.
    const CgsGui::sResourceTuple RaceMainHudState::maResourcesToLoad[1] =
        { { 0u, CgsGui::E_GUI_RESOURCETYPE_START } };   // stub; real table recovered, see note
    const u32 RaceMainHudState::muNumResourcesToLoad = 0;

    void RaceMainHudState::OnEnter() { LogUnreconstructedState("RaceMainHudState", "OnEnter"); }
    void RaceMainHudState::OnLeave() {}
    void RaceMainHudState::Update()  {}

    // ---- PRE_FLY_BY (2026-08-26 fork retirement scaffold) -----------------------------
    // BrnHudFlow.cpp now includes the REAL PreEvent header (the empty HUD-shell fork is
    // deleted), so NewPoolState<PreRaceFlyByState> allocates the full host object and the
    // 4160-byte undersized-slot hazard is closed -- but the seven real body partfiles
    // (Flow/PreEvent/States/BrnPreRaceFlyBy_wJ_0*.cpp) cannot mount yet: their closure still
    // needs BrnGui::MainMapComponent::{Construct,Prepare,Update,SetZoom,SetDesiredWorldCentre,
    // SetStickMapToScreenEdges}, MapTransform::CalculateZoomFactor, GuiCache::
    // GetLandmarkInfoFromIndex and MapManager::MapManager (measured 2026-08-26 verify wave).
    // This scaffold satisfies the link meanwhile. DELETE THIS WHOLE BLOCK (statics + 4 bodies)
    // when the wJ partfile set + the MainMap/MapTransform closure mount -- the real
    // definitions live there and the two must never coexist in one build.
    // The statics carry the REAL recovered values (header comments @0x82F26BE0/@0x82F26BE8):
    // one FLAPT HD bundle, id 200 (the BRNPRERACEFLYBY* family).
    const CgsGui::sResourceTuple PreRaceFlyByState::maResourcesToLoad[1] =
        { { 200u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE } };
    const u32 PreRaceFlyByState::muNumResourcesToLoad = 1;

    PreRaceFlyByState::PreRaceFlyByState() {}   // console ctor @0x82514E58 = vtable stores +
                                                // default member construction only (wJ_01)
    void PreRaceFlyByState::OnEnter() { LogUnreconstructedState("PreRaceFlyByState", "OnEnter"); }
    void PreRaceFlyByState::OnLeave() {}
    void PreRaceFlyByState::Update()  {}

    // (FBURN_MAIN is no longer stubbed here: BrnFBurnMainHudState.cpp now carries the
    // real reconstruction -- lifecycle, phase machine, and the 42-entry resource table
    // read from the XEX image.)


    // ================================================================================
    // THE HUD-COMPONENT ctor CLUSTER (stuntrace waveB mount closure, 2026-08-26).
    //
    // The grown BrnRaceMainHudState.h holds these components BY VALUE, so BrnHudFlow.obj's
    // NewPoolState<RaceMainHudState> and this file's own RaceMainHudState bodies emit
    // references to every one of their Construct virtuals. FOUR of them need a scaffold;
    // two of the six-symbol cluster are MOUNT REQUESTS instead, measured to cost ZERO new
    // unresolved externals:
    //     BrnPositionIndicator.cpp                        (0 new externals -- mounted)
    //     BrnFreeburnChallengeStartComponent.cpp          (0 new externals -- mounted)
    // The "BrnSatNavTile.cpp + BrnMapManager.cpp PAIR closes MapManager::MapManager at 0 new
    // externals" claim was REFUTED BY TRIAL LINK (mount-closure verify 2026-08-26):
    // BrnMapManager.cpp:20/:30 hand-declare extern "C" `_vector_constructor_iterator_` and
    // `rw__Resource___default_constructor_closure_` with NO definition anywhere (a Hex-Rays
    // transliteration artifact that should become real C++ array construction). Until that
    // TU is rewritten, MapManager::MapManager() is scaffolded below instead.
    //
    // The three below each have a REAL, STANDALONE-COMPILING TU in the tree that is NOT mounted
    // because mounting it was MEASURED to open more holes than it closes (cl /c + dumpbin
    // /SYMBOLS of the obj, diffed against the defined-symbol set of every obj in the last
    // build). Each scaffold names its TU and the exact prerequisite symbols.
    //
    // Inert is safe: RACE_MAIN is never entered on this build (RaceMainHudState::OnEnter above
    // is the un-reconstructed-state log), so no component in that state is ever Constructed,
    // Prepared, Updated or drawn. A Construct that does nothing leaves the by-value member in
    // its default-constructed state, which is exactly where it already was.
    //
    // PAIRED-EDIT / DELETE-WHEN, per component: mounting that component's TU REQUIRES deleting
    // its scaffold in the SAME change (LNK2005 otherwise, and `cl /c` cannot see it).
    // ================================================================================

    // ---- ChallengeSelector (real TU: BrnChallengeSelector.cpp + BrnChallengeSelector_wL_01.cpp,
    //      X360 Construct @0x824158F0). The pair must mount TOGETHER -- .cpp:18 says so in-tree
    //      ("SelectAvailableChallenge ... its body lives in the sibling part-file ... DO NOT add
    //      a second definition here"). Measured residual after mounting BOTH plus the already-
    //      mounted ChallengeList.cpp: THREE data-list accessors, all declared-only today --
    //          BrnResource::ChallengeList::GetChallengeCount() const
    //          BrnResource::ChallengeListEntry::GetNumPlayers() const
    //          BrnResource::ChallengeListEntry::GetDescriptionStringID() const
    //      (the last is the documented trivial twin of the already-inline GetTitleStringID,
    //      ChallengeListEntry.h:427-428: "@+0xA0 is the identical shape; left declared-only
    //      until a caller needs it" -- a caller needs it now. The other two live in
    //      ChallengeList.cpp / ChallengeListEntry.cpp, the DataLists wave's files, which is why
    //      they are reported rather than written here.)
    void ChallengeSelector::Construct(const char* /*lpacName*/, CgsGui::StateInterface* /*lpStateInterface*/,
                                      const char* /*lpacParentName*/)
    {
        LogUnreconstructedState("ChallengeSelector", "Construct");
    }

    // ---- PaybackComponent (real TU: BrnPaybackComponent.cpp, X360 Construct @0x8242E3A8).
    //      Measured residual: ONE symbol, and it is the component's OWN private leaf --
    //          BrnGui::PaybackComponent::SendAwardTriggerableEvent()   (declared h:85, DWARF
    //          :491, called from BrnPaybackComponent.cpp:240; NO body anywhere in src/).
    //      So this one is not an accessor gap: it needs a real reconstruction of the
    //      award-triggerable gui-event post before its TU can mount.
    void PaybackComponent::Construct(const char* /*lpacName*/, CgsGui::StateInterface* /*lpStateInterface*/,
                                     const char* /*lpacParentName*/)
    {
        LogUnreconstructedState("PaybackComponent", "Construct");
    }

    // ---- RoadRuleShotComponent (real TU: BrnRoadRuleShotComponent.cpp, X360 Construct
    //      @0x82424600). Measured residual: TWO GuiCache accessors, both declared with their
    //      member and console offset already pinned in BrnGuiCache.h --
    //          BrnGui::GuiCache::GetRoadRuleShotCapturedLineGate() const  (h:792,
    //              mbRoadRuleShotCapturedLineGate @+0xAC5A)
    //          BrnGui::GuiCache::GetRoadRuleShotOpponentARCI() const      (h:786,
    //              meRoadRuleShotOpponentARCI @+0xAC48)
    //      Their home is BrnGuiCache.cpp (mounted, and the event-GUI wave's hot file), so they
    //      are reported rather than written here. NOTE the signature: this component's virtual
    //      takes `const void*` name parameters, following the committed FlaptIconComponent
    //      virtual (BrnRoadRuleShotComponent.h:43-46) -- not `const char*` like its siblings.
    void RoadRuleShotComponent::Construct(const void* /*lpDEBUGName*/,
                                          CgsGui::StateInterface* /*lpStateInterface*/,
                                          const void* /*lpcParentName*/)
    {
        LogUnreconstructedState("RoadRuleShotComponent", "Construct");
    }

    // ---- MapManager ctor gate (mount-closure verify 2026-08-26) -----------------------
    //      The by-value SatNav components reach BrnGui::MapManager::MapManager(); its real
    //      TU BrnMapManager.cpp CANNOT mount (two undefined hand-declared CRT-shaped helpers,
    //      see the cluster banner above). Inert is safe on the same RACE_MAIN-never-entered
    //      argument; members default-construct. DELETE-WHEN BrnMapManager.cpp is rewritten
    //      with real array construction and mounted.
    MapManager::MapManager() {}

    // ---- CRASHEDSTNT ----------------------------------------------------------------
    const CgsGui::sResourceTuple CrashedStuntHudState::maResourcesToLoad[1] =
        { { 0u, CgsGui::E_GUI_RESOURCETYPE_START } };   // FLAG: unrecovered .rdata @0x82F26488
    const u32 CrashedStuntHudState::muNumResourcesToLoad = 0;

    void CrashedStuntHudState::OnEnter() { LogUnreconstructedState("CrashedStuntHudState", "OnEnter"); }
    void CrashedStuntHudState::OnLeave() {}
    void CrashedStuntHudState::Update()  {}
}
