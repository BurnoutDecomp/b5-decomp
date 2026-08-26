#pragma once

#include "types.hpp"
#include "GameSource/GameState/ModeManager/GameModes/BrnOfflineGameMode.h"

namespace BrnGameState
{
// StuntAttackMode is a concrete offline game mode ("Stunt Race"). Bases (OfflineGameMode ->
// GameMode) are #included from their owning headers, never forked.
//
// ===================================================================================================
// CONSOLE OVERRIDE SET -- vtable 0x820D0720 (identified by decoding its slot-6 GetName leaf
// 0x827E2528, which returns "Stunt Race" @0x82029ADC). Re-dumped from image.bin 2026-08-26; the
// DWARF (BrnStuntAttackMode.h:45-78) declares exactly the same ten names.
//   slot  2  PreWorldUpdate           0x82344EE0   declared + bodied (2026-08-26)
//   slot  5  Start                    0x82331E98   declared + bodied (2026-08-26)
//   slot  6  GetName                  0x827E2528   declared
//   slot  8  GetIntroDurationSeconds  0x827E2538   declared  (lfs [0x82021240] == 6.0f)
//   slot  9  HasTimedIntro            folded onto the base's `li r3,1` leaf -- invisible, inherited
//   slot 11  ShouldCountdownEnd       0x827E2558   declared  (`lbz r3,0xD4(r3); blr`)
//   slot 13  ShouldExit               0x827E2F38   declared  (`li r3,0; blr` -- never idle-exits)
//   slot 14  ShouldFinish             0x823162B8   declared + bodied (2026-08-26)
//   slot 16  GetOutroTimeout          0x827E2548   declared  (lfs [0x82021244] == 0.0f, AUTHORED)
//   slot 23  RequiresStreaming        0x827E2F38   declared  (`li r3,0; blr`)
//   slot 24  HasLoadingScreen         inherits the base's 0x827E2F38 (FALSE) -- a stunt race takes
//                                     NEITHER the loading-screen path NOR the streaming path.
//
// [x] FRONTIER CLOSED 2026-08-26 (wave-B closure round). All three overrides below are now DECLARED
// (in the landed 26-slot base signatures) and BODIED in BrnStuntAttackMode.cpp:
//   * StuntAttackMode::Start          @0x82331E98 -- slot 5.  Builds the mutable GameModeParams.
//   * StuntAttackMode::PreWorldUpdate @0x82344EE0 -- slot 2.  The SOLE writer of
//     mbPlayerPointingInStartDirection, which ShouldCountdownEnd returns.
//   * StuntAttackMode::ShouldFinish   @0x823162B8 -- slot 14. Ends the event on the idle timers
//     unless a stunt combo is still running.
// The two ProgressionManager callees Start needs (GetProgressionRankForGameMode @0x8237B4E8 and
// GetStuntRunScoreTarget @0x8237B6B0) were DECLARE-ONLY when the three landed; both are now BODIED
// in BrnProgressionManager.cpp (with their shared callee GetRankThresholdForEvent @0x82370260), so
// Start's score/time targets are real values rather than link holes. Nothing on this path is
// declare-only any more.
// ===================================================================================================
class StuntAttackMode : public OfflineGameMode
{
public:
    virtual const char* GetName() const;                               // slot 6,  X360 0x827E2528
    virtual f32         GetIntroDurationSeconds() const;               // slot 8,  X360 0x827E2538
    virtual f32         GetOutroTimeout() const;                       // slot 16, X360 0x827E2548
    virtual bool        ShouldCountdownEnd() const;                    // slot 11, X360 0x827E2558

    // Slot 13 (vtbl+52). Folded leaf 0x827E2F38 (`li r3,0; blr`) -- a stunt race NEVER auto-exits
    // on the shared idle timers; it ends through ShouldFinish (slot 14) instead. ADDED 2026-08-26
    // with the 26-slot base: GameMode::ShouldExit is now wired to the real ScoringSystem idle
    // timers, and standing still lining up a stunt is normal play, so without this override a
    // stunt race would exit itself 4 s after the player stops.
    virtual bool        ShouldExit(const ScoringSystem* lpScoringSystem) const;

    // Slot 23 (vtbl+92). Folded leaf 0x827E2F38 (`li r3,0; blr`); the base is 0x82C296C8
    // (`li r3,1`). SetupGameMode @0x8234B158 gates the WaitForStreaming path on this, so the
    // campaign's "a stunt race is not blocked on GameStateModule::WaitForStreaming" claim lives
    // here. ADDED 2026-08-26 with the 26-slot base.
    virtual bool        RequiresStreaming() const;

    // ===============================================================================================
    // THE THREE PAYOFF OVERRIDES (landed 2026-08-26 by the wave-B closure round).
    // Each is written in the EXACT landed base signature from BrnGameMode.h -- a drifted parameter
    // list would MINT A NEW SLOT instead of overriding, silently, with nothing a compile-only gate
    // can see. The member-pointer static_asserts at the foot of this header are the tripwire.
    // ===============================================================================================

    // Slot 2 (vtbl+8), X360 0x82344EE0. Six parameters, exactly as the base declares them
    // (ModeManager::UpdateCurrentMode @0x82350EC8 dispatches all six). r7 == lpActiveRaceCars is
    // the one this body reads; the base call forwards all six unchanged
    // (asm 0x82344FE8..0x82345004 `mr r9,r24 / r8,r25 / r7,r30 / r6,r26 / r5,r27 / r4,r29`).
    virtual void        PreWorldUpdate(GameStateModuleIO::OutputBuffer* lpOutput,
                                       const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                       const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCars,
                                       const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                                       bool lbPaused,
                                       const ScoringSystem* lpScoringSystem);

    // Slot 5 (vtbl+20), X360 0x82331E98. The console body takes only THREE registers (r3 this,
    // r4 lpStartGameModeParams, r5 lpGameModeParams) and never touches r6 -- the ScoringSystem*
    // the base slot passes is simply unused here, exactly as in the committed RaceMode /
    // PursuitMode / BurningRouteMode Start bodies.
    virtual void        Start(const StartGameModeParams* lpStartGameModeParams,
                              GameModeParams* lpGameModeParams,
                              ScoringSystem* lpScoringSystem);

    // Slot 14 (vtbl+56), X360 0x823162B8. NON-const and takes a MUTABLE ScoringSystem* (it resets
    // the two idle timers on the "combo still running" arm) -- that is the base's declared shape.
    virtual bool        ShouldFinish(ScoringSystem* lpScoringSystem);

private:
    // DWARF member layout (BrnStuntAttackMode.h:97-100). Named members (no raw-offset access).
    // mbPlayerPointingInStartDirection is the gate ShouldCountdownEnd returns -- console +212
    // (0xD4), which is exactly the byte the folded leaf 0x827E2558 loads (`lbz r3,0xD4(r3)`).
    // It is recomputed each frame by PreWorldUpdate (declared above, bodied in the .cpp).
    // CONSOLE OFFSETS, all re-derived from Start @0x82331E98 and PreWorldUpdate @0x82344EE0 this
    // pass: mStartDir +0xC0 (a 16-byte `stvx128` slot), mfCountdownTimer +0xD0
    // (`stfs f31, 0xD0(r25)` in Start, `stfs f0, 0xD0(r31)` in PreWorldUpdate),
    // mbPlayerPointingInStartDirection +0xD4 (`stb r11, 0xD4(r31)`),
    // mbNeedToFillBoost +0xD5 (`stb r11(1), 0xD5(r25)` in Start, cleared at 0x82344FE4).
    Vector3 mStartDir;
    f32     mfCountdownTimer;
    bool    mbPlayerPointingInStartDirection;
    bool    mbNeedToFillBoost;

    // BrnStuntAttackMode.cpp:25 (DWARF). GetIntroDurationSeconds returns 6.0 for this build
    // (0x827E2538 loads flt_82021240, re-dumped from image.bin == 6.0f).
    static const f32 KF_INTRO_TIME_SECONDS;
};

// ---- VTABLE-BINDING TRIPWIRE (see the explanation in BrnOfflineGameMode.h) ----------------------
// This is the block the whole fix round exists for: agent 5 dispatches Start through vtbl+20 and
// agent 6 polls ShouldFinish through vtbl+56, and before the 26-slot rewrite the committed header
// put Start at index 9 and had no slot 14 at all.
static_assert(sizeof(static_cast<const char* (StuntAttackMode::*)() const>(&StuntAttackMode::GetName)) != 0,
              "StuntAttackMode::GetName must bind GameMode vtable slot 6");
static_assert(sizeof(static_cast<f32 (StuntAttackMode::*)() const>(&StuntAttackMode::GetIntroDurationSeconds)) != 0,
              "StuntAttackMode::GetIntroDurationSeconds must bind GameMode vtable slot 8");
static_assert(sizeof(static_cast<f32 (StuntAttackMode::*)() const>(&StuntAttackMode::GetOutroTimeout)) != 0,
              "StuntAttackMode::GetOutroTimeout must bind GameMode vtable slot 16");
static_assert(sizeof(static_cast<bool (StuntAttackMode::*)() const>(&StuntAttackMode::ShouldCountdownEnd)) != 0,
              "StuntAttackMode::ShouldCountdownEnd must bind GameMode vtable slot 11");
static_assert(sizeof(static_cast<bool (StuntAttackMode::*)(const ScoringSystem*) const>(&StuntAttackMode::ShouldExit)) != 0,
              "StuntAttackMode::ShouldExit must bind GameMode vtable slot 13");
static_assert(sizeof(static_cast<bool (StuntAttackMode::*)() const>(&StuntAttackMode::RequiresStreaming)) != 0,
              "StuntAttackMode::RequiresStreaming must bind GameMode vtable slot 23");
// Slot 2 / 5 / 14 -- now asserted against the DERIVED declarations above (they were asserted
// through the inherited base ones while the three were on the frontier). If a future edit drifts
// any parameter list, the cast stops compiling here instead of silently minting a new slot.
static_assert(sizeof(static_cast<void (StuntAttackMode::*)(GameStateModuleIO::OutputBuffer*,
                                                           const GameStateModuleIO::PreWorldInputBuffer*,
                                                           const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface*,
                                                           const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*,
                                                           bool,
                                                           const ScoringSystem*)>(&StuntAttackMode::PreWorldUpdate)) != 0,
              "StuntAttackMode::PreWorldUpdate must bind GameMode vtable slot 2 (X360 0x82344EE0)");
static_assert(sizeof(static_cast<void (StuntAttackMode::*)(const StartGameModeParams*, GameModeParams*, ScoringSystem*)>(&StuntAttackMode::Start)) != 0,
              "StuntAttackMode::Start must bind GameMode vtable slot 5 (X360 0x82331E98)");
static_assert(sizeof(static_cast<bool (StuntAttackMode::*)(ScoringSystem*)>(&StuntAttackMode::ShouldFinish)) != 0,
              "StuntAttackMode::ShouldFinish must bind GameMode vtable slot 14 (X360 0x823162B8)");
static_assert(sizeof(static_cast<bool (StuntAttackMode::*)() const>(&StuntAttackMode::HasLoadingScreen)) != 0,
              "StuntAttackMode::HasLoadingScreen must bind GameMode vtable slot 24 (inherited FALSE)");
}
