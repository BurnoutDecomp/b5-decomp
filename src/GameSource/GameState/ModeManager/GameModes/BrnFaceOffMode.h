#pragma once

#include "types.hpp"
#include "GameSource/GameState/ModeManager/GameModes/BrnOfflineGameMode.h"

namespace BrnGameState
{
// FaceOffMode is a concrete offline game mode. Bases (OfflineGameMode -> GameMode) are #included
// from their owning headers, never forked.
//
// CONSOLE OVERRIDE SET -- vtable 0x820D0500, the same set the DWARF declares
// (BrnFaceOffMode.h:99-112, .cpp:81/:174/:195):
//   slot  2  PreWorldUpdate                 0x82330670
//   slot  5  Start                          0x82330440
//   slot  6  GetName                        0x827E24A8
//   slot  8  GetIntroDurationSeconds        0x827EAB30 (ICF-shared)
//   slot 15  FillInGameModeSpecificResults  0x82315CC0
//   slot 16  GetOutroTimeout                0x827E24B8
class FaceOffMode : public OfflineGameMode
{
public:
    virtual const char* GetName() const;
    virtual f32         GetOutroTimeout() const;

    // Slot 8 (vtbl+32), LANDED 2026-09-25 (crash parity FX-SCENARIOS). X360 0x827EAB30 -- a body
    // shared by ICF (IDA files it as MovieVideoRenderer::GetRenderingDelay): `lis/lfs f1,
    // flt_82001CC0 ; blr` == 0.0f. DWARF BrnFaceOffMode.h:99 declares the override. With 0.0
    // ModeManager::StartModeIntro's `mfDurationSeconds > 0.0f` (fcmpu/bgt) leaves mbDoIntro false:
    // a Face Off has NO intro. Until then it inherited the GameMode base's 6.0.
    virtual f32         GetIntroDurationSeconds() const;

    // Slot 2 (vtbl+8), X360 0x82330670. The whole body is `b GameMode::PreWorldUpdate`.
    virtual void PreWorldUpdate(GameStateModuleIO::OutputBuffer* lpOutput,
                                const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCars,
                                const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                                bool lbPaused,
                                const ScoringSystem* lpScoringSystem);

    // Slot 5 (vtbl+20), X360 0x82330440. Reads r3/r4/r5 only: the ScoringSystem* is never touched.
    virtual void Start(const StartGameModeParams* lpStartGameModeParams,
                       GameModeParams* lpGameModeParams,
                       ScoringSystem* lpScoringSystem);

    // Slot 15 (vtbl+60), X360 0x82315CC0. On this build the body is its two asserts and nothing else.
    virtual void FillInGameModeSpecificResults(const ScoringSystem* lpScoringSystem,
                                               GameStateModuleIO::FinishedModeAction* lpAction);

private:
    // DWARF: BrnFaceOffMode.cpp:27. The mode's fixed outro timeout; the X360 GetOutroTimeout body
    // returns 0.0, so this constant is 0.0f for this build (same shape as RaceMode).
    static const f32 KF_OUTRO_TIME_SECONDS;

    // DWARF BrnFaceOffMode.h:88, console +188: Start @0x823304D8 `stw r11(1), 0xBC(r29)`.
    s32  miNumRivalsInRace;
    // DWARF BrnFaceOffMode.h:89, console +192. No ARTIST export touches it (the record is 196 bytes,
    // ModeManager mFaceOff +268 -> mCrashMode +464, so the storage is real even though it is dead).
    bool mbPlayerBeatMainRival;
};

// ---- VTABLE-BINDING TRIPWIRE (see the explanation in BrnOfflineGameMode.h) ----------------------
static_assert(sizeof(static_cast<const char* (FaceOffMode::*)() const>(&FaceOffMode::GetName)) != 0,
              "FaceOffMode::GetName must bind GameMode vtable slot 6");
static_assert(sizeof(static_cast<f32 (FaceOffMode::*)() const>(&FaceOffMode::GetOutroTimeout)) != 0,
              "FaceOffMode::GetOutroTimeout must bind GameMode vtable slot 16");
static_assert(sizeof(static_cast<f32 (FaceOffMode::*)() const>(&FaceOffMode::GetIntroDurationSeconds)) != 0,
              "FaceOffMode::GetIntroDurationSeconds must bind GameMode vtable slot 8");
static_assert(sizeof(static_cast<void (FaceOffMode::*)(GameStateModuleIO::OutputBuffer*,
                                                       const GameStateModuleIO::PreWorldInputBuffer*,
                                                       const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface*,
                                                       const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*,
                                                       bool,
                                                       const ScoringSystem*)>(&FaceOffMode::PreWorldUpdate)) != 0,
              "FaceOffMode::PreWorldUpdate must bind GameMode vtable slot 2");
static_assert(sizeof(static_cast<void (FaceOffMode::*)(const StartGameModeParams*, GameModeParams*, ScoringSystem*)>(&FaceOffMode::Start)) != 0,
              "FaceOffMode::Start must bind GameMode vtable slot 5");
static_assert(sizeof(static_cast<void (FaceOffMode::*)(const ScoringSystem*, GameStateModuleIO::FinishedModeAction*)>(&FaceOffMode::FillInGameModeSpecificResults)) != 0,
              "FaceOffMode::FillInGameModeSpecificResults must bind GameMode vtable slot 15");
}
