#pragma once

#include "types.hpp"
#include "GameSource/GameState/ModeManager/GameModes/BrnOfflineGameMode.h"

namespace BrnGameState
{
// Original offline Burning Route setup and player tracking.
class BurningRouteMode : public OfflineGameMode
{
public:
    void Start(const StartGameModeParams* lpStartGameModeParams, GameModeParams* lpGameModeParams,
               ScoringSystem* lpScoringSystem) override;
    void PreWorldUpdate(GameStateModuleIO::OutputBuffer* lpOutput,
                        const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                        const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCars,
                        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                        bool lbPaused, const ScoringSystem* lpScoringSystem) override;
    virtual const char* GetName() const;                               // slot 6,  X360 0x827E2508
    virtual f32         GetOutroTimeout() const;                       // slot 16, X360 0x827E2518

    // Slot 23 (vtbl+92). Folded leaf 0x827E2F38 (`li r3,0; blr`); the base is 0x82C296C8
    // (`li r3,1`). DWARF BrnBurningRoute.h:81 declares this override. ADDED 2026-08-26 with the
    // 26-slot base: without it, SetupGameMode would put a burning route through WaitForStreaming.
    virtual bool RequiresStreaming() const;

private:
    // DWARF: BrnBurningRoute.cpp:27. The mode's fixed outro timeout; the X360 GetOutroTimeout body
    // returns 0.0, so this constant is 0.0f for this build (same shape as RaceMode).
    static const f32 KF_OUTRO_TIME_SECONDS;
    ::EActiveRaceCarIndex mePlayerActiveIndex; // DecFIGS :97; ARTIST PreWorldUpdate stores +0xD4.
};

// ---- VTABLE-BINDING TRIPWIRE (see the explanation in BrnOfflineGameMode.h) ----------------------
static_assert(sizeof(static_cast<const char* (BurningRouteMode::*)() const>(&BurningRouteMode::GetName)) != 0,
              "BurningRouteMode::GetName must bind GameMode vtable slot 6");
static_assert(sizeof(static_cast<f32 (BurningRouteMode::*)() const>(&BurningRouteMode::GetOutroTimeout)) != 0,
              "BurningRouteMode::GetOutroTimeout must bind GameMode vtable slot 16");
static_assert(sizeof(static_cast<bool (BurningRouteMode::*)() const>(&BurningRouteMode::RequiresStreaming)) != 0,
              "BurningRouteMode::RequiresStreaming must bind GameMode vtable slot 23");
}
