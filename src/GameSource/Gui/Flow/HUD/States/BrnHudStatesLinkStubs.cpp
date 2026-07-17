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
    const CgsGui::sResourceTuple RaceMainHudState::maResourcesToLoad[1] =
        { { 0u, CgsGui::E_GUI_RESOURCETYPE_START } };   // FLAG: unrecovered .rdata @0x82F25F88
    const u32 RaceMainHudState::muNumResourcesToLoad = 0;

    void RaceMainHudState::OnEnter() { LogUnreconstructedState("RaceMainHudState", "OnEnter"); }
    void RaceMainHudState::OnLeave() {}
    void RaceMainHudState::Update()  {}

    // (FBURN_MAIN is no longer stubbed here: BrnFBurnMainHudState.cpp now carries the
    // real reconstruction -- lifecycle, phase machine, and the 42-entry resource table
    // read from the XEX image.)

    // ---- CRASHEDSTNT ----------------------------------------------------------------
    const CgsGui::sResourceTuple CrashedStuntHudState::maResourcesToLoad[1] =
        { { 0u, CgsGui::E_GUI_RESOURCETYPE_START } };   // FLAG: unrecovered .rdata @0x82F26488
    const u32 CrashedStuntHudState::muNumResourcesToLoad = 0;

    void CrashedStuntHudState::OnEnter() { LogUnreconstructedState("CrashedStuntHudState", "OnEnter"); }
    void CrashedStuntHudState::OnLeave() {}
    void CrashedStuntHudState::Update()  {}
}
