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
    // ---- RACE_MAIN: SCAFFOLD RETIRED 2026-08-27 (stunt-race UI wave) ------------------
    // The real TU set is MOUNTED: BrnRaceMainHudState.cpp (the recovered 21-entry table +
    // OnEnter/OnLeave/UpdateSetupState family) + partfiles _wS2/_wS3/_wS4. The placeholder
    // statics and log-and-return lifecycle that stood here are deleted -- LNK2005 otherwise.

    // ---- PRE_FLY_BY: SCAFFOLD RETIRED 2026-08-27 (stunt-race UI wave) -----------------
    // The seven real body partfiles (Flow/PreEvent/States/BrnPreRaceFlyBy_wJ_0*.cpp) are
    // MOUNTED, with their measured closure: BrnMainMap.cpp (Construct/Prepare/RecvEvent +
    // the header-inline setters + the zoom tables) and BrnMainMapLinkGates.cpp (FLAG gates
    // for the siblings that are provably dead on the stunt path -- IsMapApplicableToGameMode
    // returns FALSE for STUNT_ATTACK, so the fly-by legitimately shows titles + description
    // and no minimap). The statics, ctor and lifecycle stubs that stood here are deleted --
    // the real definitions live in the wJ set and the two must never coexist in one build.

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

    // ---- the RACE_MAIN mount's measured residual gates (2026-08-27 trial links) -------
    // Each symbol below is referenced by the mounted BrnRaceMainHudState TU set but its
    // owning TU was TRIED and opens more holes than it closes (see the bat's SatNav/HUD
    // block notes). Every one is RUNTIME-DEAD on the STUNT_ATTACK path: UpdateSetupState
    // turns Compass / PlayerPositionTable / Payback OFF for mode 7, so the gated calls sit
    // behind component-enable flags that are false. One-shot logs make any future live hit
    // attributable.
    //
    // Compass (real TU BrnCompassComponent.cpp, all three bodied there; its own residual =
    // ShowPositionOnCompass + FormatDirectionLetters + ChallengeListEntryAction::
    // GetNumLocations). DELETE-WHEN BrnCompassComponent.cpp mounts.
    void CompassComponent::Construct(const char* /*lpacName*/, CgsGui::StateInterface* /*lpStateInterface*/,
                                     const char* /*lpacParentName*/, s32 /*liParentAptLayerIndex*/)
    {
        LogUnreconstructedState("CompassComponent", "Construct");
    }
    void CompassComponent::Prepare(const char* /*lpacName*/, const BrnFlapt::FileRef& /*lrFile*/)
    {
        LogUnreconstructedState("CompassComponent", "Prepare");
    }
    void CompassComponent::SetVisibility(bool /*lbVisible*/, bool /*lbImmediate*/)
    {
        LogUnreconstructedState("CompassComponent", "SetVisibility");
    }

    // PlayerPositionTable (real TU BrnPlayerPositionTable.cpp, SetCache @0x82473458 +
    // SetupGameMode @0x8243E6F8 bodied there; its residual = SingleComponent::SetCache,
    // SetTitleText @0x82437AD0, SetSkillsText @0x82413B30, CgsNetwork::UsernameCompare,
    // BurnoutSkillsManager::GetCurrentSkill). DELETE-WHEN the table pair mounts.
    void PlayerPositionTableComponent::SetCache(GuiCache* /*lpCache*/)
    {
        LogUnreconstructedState("PlayerPositionTableComponent", "SetCache");
    }
    void PlayerPositionTableComponent::SetupGameMode()
    {
        LogUnreconstructedState("PlayerPositionTableComponent", "SetupGameMode");
    }

    // Payback (real TU BrnPaybackComponent.cpp, unmounted -- see the Construct scaffold
    // above; these two join it). DELETE-WHEN BrnPaybackComponent.cpp mounts.
    void PaybackComponent::Initialize(GuiCache* /*lpCache*/)
    {
        LogUnreconstructedState("PaybackComponent", "Initialize");
    }
    void PaybackComponent::ShowAvailableInstantly(BrnNetwork::EPaybackType /*lePaybackType*/,
                                                  EActiveRaceCarIndex /*leCarIndex*/)
    {
        LogUnreconstructedState("PaybackComponent", "ShowAvailableInstantly");
    }

    // ---- RoadRuleShotComponent: SCAFFOLD RETIRED 2026-08-27 (stunt-race UI wave). The
    //      real TU BrnRoadRuleShotComponent.cpp was fully bodied all along and is MOUNTED;
    //      its two-accessor link residual (GetRoadRuleShotCapturedLineGate /
    //      GetRoadRuleShotOpponentARCI) landed in BrnGuiCache_wS1.cpp.

    // ---- MapManager ctor gate (mount-closure verify 2026-08-26) -----------------------
    //      MapManager::MapManager() SCAFFOLD RETIRED 2026-08-27 (map arm): BrnMapManager.cpp
    //      was rewritten member-by-name (the two undefined CRT-shaped helper externs died
    //      with the raw-offset ctor) and is MOUNTED -- its real ctor + Construct + RecvEvent
    //      now link from there. Re-adding a body here is LNK2005.

    // ---- CRASHEDSTNT: SCAFFOLD RETIRED 2026-08-27 (crashed-stunt HUD wave) -------------
    // The real TU is MOUNTED: BrnCrashedStuntHudState.cpp carries the recovered 4-entry
    // resource table (read from the XEX image at 0x82F26488, count 4 at 0x82F264A8 -- the
    // FLAG placeholder that stood here is paid off), the 12-entry event table @0x8205B17C,
    // the real OnEnter/OnLeave/Update lifecycle and the COMPLETE UpdatePermenant including
    // the "END_CSTNT" exit arm that returns the FSM to RACE_MAIN. The placeholder statics
    // and log-and-return lifecycle that stood here are deleted -- LNK2005 otherwise.
}
