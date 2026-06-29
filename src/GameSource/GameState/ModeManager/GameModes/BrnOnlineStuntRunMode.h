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

    // X360: BrnGameState::OnlineStuntRunMode::PreWorldUpdate (0x82331A30). Overrides
    // GameMode::PreWorldUpdate.
    virtual void PreWorldUpdate();

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
}
