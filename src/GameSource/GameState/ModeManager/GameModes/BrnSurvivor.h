#pragma once

#include "types.hpp"
#include "GameSource/GameState/ModeManager/GameModes/BrnOfflineGameMode.h"

namespace BrnGameState
{
// Marked Man: original offline event setup, threat ramp and results.
class SurvivorMode : public OfflineGameMode
{
public:
    void Start(const StartGameModeParams* lpStartGameModeParams, GameModeParams* lpGameModeParams,
               ScoringSystem* lpScoringSystem) override;
    void PreWorldUpdate(GameStateModuleIO::OutputBuffer* lpOutput,
                        const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                        const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCars,
                        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                        bool lbPaused, const ScoringSystem* lpScoringSystem) override;
    void OnPlayerInShortCut() override;
    bool ShouldExit(const ScoringSystem* lpScoringSystem) const override;
    void FillInGameModeSpecificResults(const ScoringSystem* lpScoringSystem,
                                      GameStateModuleIO::FinishedModeAction* lpAction) override;
    void OnPlayerUsesPaintShop() override;
    virtual const char* GetName() const;                               // slot 6,  X360 0x827E2560
    virtual f32         GetOutroTimeout() const;                       // slot 16, X360 0x827E2570

    f32 GetRampTimer() const              { return mfRampTimer; }
    f32 GetMaxRampTimer() const           { return mfMaxRampTimer; }
    s32 GetBroadcastOpponentCount() const { return miBroadcastOpponentCount; }

private:
    void UpdateOpponents(GameStateModuleIO::OutputBuffer* lpOutput, s32 liOpponentCount);

    // DWARF: BrnSurvivor.cpp:27. The mode's fixed outro timeout; the X360 GetOutroTimeout body
    // returns 0.0, so this constant is 0.0f for this build (same shape as RaceMode).
    static const f32 KF_OUTRO_TIME_SECONDS;

    // DecFIGS source names/order, with the three HUD fields independently pinned by
    // ARTIST at ModeManager +0x6D0/+0x6D4/+0x6D8.
    bool mbInShortcut;
    f32  mfRampTimer;
    f32  mfMaxRampTimer;
    s32  miBroadcastOpponentCount;
    s32  miMaxOpponentCount;
    f32  mfTimeInReverse;
};

// ---- VTABLE-BINDING TRIPWIRE (see the explanation in BrnOfflineGameMode.h) ----------------------
static_assert(sizeof(static_cast<const char* (SurvivorMode::*)() const>(&SurvivorMode::GetName)) != 0,
              "SurvivorMode::GetName must bind GameMode vtable slot 6");
static_assert(sizeof(static_cast<f32 (SurvivorMode::*)() const>(&SurvivorMode::GetOutroTimeout)) != 0,
              "SurvivorMode::GetOutroTimeout must bind GameMode vtable slot 16");
}
