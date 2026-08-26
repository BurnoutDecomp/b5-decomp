#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                              // Vector3
#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineGameMode.h"  // OnlineGameMode base
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"  // StartGameModeParams / GameModeParams / LightTriggerId / ScoringSystem fwd
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                    // CgsNumeric::Random (mRandom, embedded by value)

namespace BrnGameState
{

// OnlineStuntRunMode is a concrete online game mode ("OnlineStuntRun"). The base types
// (OnlineGameMode -> GameMode) are #included from their own owning headers rather than forked
// locally. This header declares the six X360-attested functions this TU owns; the base virtuals
// (GetName / PreWorldUpdate / GetOutroTimeout / ShouldFinish / Start) bind to the GameMode vtable
// by signature (BrnGameMode.h declares the slots). GetBestStartGridID is a private non-virtual
// helper Start calls to pick the best-aligned start light.
//
// LAYOUT NOTE: byte offsets are NOT X360-faithful on the x64 PC gate (the GameMode base carries
// 8-byte pointers + opaque contained-state padding). The two own data members below stand in for
// the X360 fields the asm reads at this+0xF0 (mfTimeRemaining) and this+0xF4 (mbHasCheckedFinish);
// mRandom stands in for the per-event start-grid shuffle generator the asm embeds at this+0xC0.
// Named access for semantic parity, not exact placement.
class OnlineStuntRunMode : public OnlineGameMode
{
public:
    // X360: BrnGameState::OnlineStuntRunMode::GetName (0x827E25B0). Overrides GameMode::GetName.
    virtual const char* GetName() const;

    // X360: BrnGameState::OnlineStuntRunMode::GetOutroTimeout (0x823162A8). Overrides
    // GameMode::GetOutroTimeout. Returns the fixed outro hold (flt_82021230 == 15.0).
    virtual f32 GetOutroTimeout() const;

    // X360: BrnGameState::OnlineStuntRunMode::PreWorldUpdate (0x82331A30). Overrides GameMode
    // vtable slot 2 (vtbl+8).
    //
    // SIGNATURE WIDENED 2026-08-26 (wave-B fix round). It was declared no-arg, matching the
    // then-committed base; the console base takes SIX arguments (UpdateCurrentMode @0x82350EC8
    // dispatches `(*(**(a1+3480)+8))(mode, a2, a3, a8, a28, a30, a1+3504)`), so once the base was
    // corrected the no-arg form would have MINTED A NEW SLOT instead of binding to slot 2.
    virtual void PreWorldUpdate(GameStateModuleIO::OutputBuffer* lpOutput,
                                const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCars,
                                const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                                bool lbPaused,
                                const ScoringSystem* lpScoringSystem);

    // Slot 24 (vtbl+96). 0x82C296C8 (`li r3,1; blr`) at slot 24 of vtable 0x820D08E0, against the
    // GameMode base's 0x827E2F38 (`li r3,0`). SetupGameMode @0x8234B158 reads it twice and
    // HandleLoadingScreenLoaded @0x8234B8A8 once. ADDED 2026-08-26 with the 26-slot base.
    // NOTE: this is the ONLINE stunt run. The OFFLINE StuntAttackMode ("Stunt Race", the campaign's
    // target) inherits the base FALSE -- the two must not be conflated.
    virtual bool HasLoadingScreen() const;

    // X360: BrnGameState::OnlineStuntRunMode::ShouldFinish (0x8233A3F0). Overrides
    // GameMode::ShouldFinish. True once the stunt-run countdown has run out.
    virtual bool ShouldFinish(ScoringSystem* lpScoringSystem);

    // X360: BrnGameState::OnlineStuntRunMode::Start (0x82339E70). Overrides GameMode::Start.
    // Builds the online stunt-run GameModeParams, seeds the start-grid shuffle, picks the best
    // start light and arms the countdown timer.
    virtual void Start(const StartGameModeParams* lpStartGameModeParams,
                       GameModeParams*            lpGameModeParams,
                       ScoringSystem*             lpScoringSystem);

private:
    // X360: BrnGameState::OnlineStuntRunMode::GetBestStartGridID (0x82331708). Walks the current
    // track's TrafficData hull light-trigger start blocks for the junction encoded in luTriggerId
    // and returns the packed light-trigger id whose start direction best aligns (minimum angle)
    // with lv3Reference.
    LightTriggerId GetBestStartGridID(LightTriggerId             luTriggerId,
                                      const StartGameModeParams* lpStartGameModeParams,
                                      Vector3                    lv3Reference);

    // ---- own data members (X360 fields this+0xC0 / +0xF0 / +0xF4) -----------------------------
    CgsNumeric::Random mRandom;             // X360 this+0xC0 -- per-event start-grid shuffle generator
    f32                mfTimeRemaining;     // X360 this+0xF0 -- stunt-run countdown (armed by Start)
    bool               mbHasCheckedFinish;  // X360 this+0xF4 -- ShouldFinish has run at least once
};

// ---- VTABLE-BINDING TRIPWIRE (see the explanation in BrnOfflineGameMode.h) ----------------------
static_assert(sizeof(static_cast<const char* (OnlineStuntRunMode::*)() const>(&OnlineStuntRunMode::GetName)) != 0,
              "OnlineStuntRunMode::GetName must bind GameMode vtable slot 6");
static_assert(sizeof(static_cast<f32 (OnlineStuntRunMode::*)() const>(&OnlineStuntRunMode::GetOutroTimeout)) != 0,
              "OnlineStuntRunMode::GetOutroTimeout must bind GameMode vtable slot 16");
static_assert(sizeof(static_cast<void (OnlineStuntRunMode::*)(GameStateModuleIO::OutputBuffer*,
                                                              const GameStateModuleIO::PreWorldInputBuffer*,
                                                              const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface*,
                                                              const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*,
                                                              bool,
                                                              const ScoringSystem*)>(&OnlineStuntRunMode::PreWorldUpdate)) != 0,
              "OnlineStuntRunMode::PreWorldUpdate must bind GameMode vtable slot 2");
static_assert(sizeof(static_cast<bool (OnlineStuntRunMode::*)(ScoringSystem*)>(&OnlineStuntRunMode::ShouldFinish)) != 0,
              "OnlineStuntRunMode::ShouldFinish must bind GameMode vtable slot 14");
static_assert(sizeof(static_cast<void (OnlineStuntRunMode::*)(const StartGameModeParams*, GameModeParams*, ScoringSystem*)>(&OnlineStuntRunMode::Start)) != 0,
              "OnlineStuntRunMode::Start must bind GameMode vtable slot 5");
static_assert(sizeof(static_cast<bool (OnlineStuntRunMode::*)() const>(&OnlineStuntRunMode::HasLoadingScreen)) != 0,
              "OnlineStuntRunMode::HasLoadingScreen must bind GameMode vtable slot 24");
}
