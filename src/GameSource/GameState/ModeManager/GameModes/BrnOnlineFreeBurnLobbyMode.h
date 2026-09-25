#pragma once

#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineGameMode.h"
#include "GameSource/GameState/ModeManager/GameModes/BrnBurnoutSkillzManager.h"   // mBurnoutSkillzManager (by value)
#include "GameShared/GameClasses/Core/CgsAssert.h"                                // the two inline setters

namespace BrnGameState
{
// OnlineFreeBurnLobbyMode is a concrete game mode. The base types (OnlineGameMode -> GameMode) are #included from
// their own owning headers rather than forked locally.
//
// Mode 15 (E_MODE_ONLINE_FREE_BURN_LOBBY). Its one data member is the embedded BurnoutSkillzManager
// at console mode +0xB8: the GameMode base ends at +0xB8 and the manager is 0xA0 bytes, so the
// console sizeof is 0x158 (ModeManager places the lobby at +2952 and the next mode at +3296). On the
// host the member simply follows the base, by name.
class OnlineFreeBurnLobbyMode : public OnlineGameMode
{
    // ModeManager reaches the embedded manager directly: its PostWorldUpdate lobby arm reads the
    // manager's cached ScoringSystem and calls its private SetNewSkillIfGreater inline. The original
    // header keeps the member private and gives the class no accessor for it.
    friend class ModeManager;

public:
    // Slot 0. GameMode::Construct, mbIsOnline = 1, then the skillz manager's Construct.
    virtual void Construct(ModeManager* lpModeManager);

    // Slot 2. The base PreWorldUpdate, then the skillz manager's (not exiting the lobby).
    virtual void PreWorldUpdate(GameStateModuleIO::OutputBuffer* lpOutput,
                                const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCars,
                                const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                                bool lbPaused,
                                const ScoringSystem* lpScoringSystem);

    // Slot 3. Tail call into the skillz manager.
    virtual void PostWorldUpdate(const GameStateModuleIO::PostWorldInputBuffer* lpInput);

    virtual const char* GetName() const;                               // slot 6, X360 0x827E25E0

    // X360 0x82322338. Builds the online free-burn-lobby GameModeParams + copies the network ids.
    virtual void Start(const StartGameModeParams* lpStartGameModeParams,
                       GameModeParams* lpGameModeParams,
                       ScoringSystem* lpScoringSystem);                // slot 5

    // Slot 12. The lobby's own state machine (the same transitions as OnlineFreeBurnMode's).
    virtual void SendEvent(EGameModeEvent leEvent);

    // Slot 19. Flags the spawned car's skills for a network update, without the HUD message.
    virtual void PlayerHasSpawned(::EActiveRaceCarIndex leActiveRaceCarIndex);

    // Slot 20. Forwards the new road score to the skillz manager.
    virtual void ProcessNewRoadScore(GameStateModuleIO::OutputBuffer* lpOutput,
                                     BrnStreetData::ChallengePlayerScoreEntry lScoreEntry,
                                     BrnStreetData::ScoreType leScoreType,
                                     BrnStreetData::ChallengeIndex lChallengeIndex,
                                     ::EActiveRaceCarIndex leActiveRaceCarIndex);

    // Slot 21. Forwards the road change to the skillz manager.
    virtual void OnEnterRoad(BrnStreetData::RoadIndex lRoadIndex);

    // Slot 23 (vtbl+92). Folded leaf 0x827E2F38 (`li r3,0; blr`); the GameMode base is 0x82C296C8
    // (`li r3,1`). DWARF BrnOnlineFreeBurnLobbyMode.h:48 declares this override; ADDED 2026-08-26
    // with the 26-slot base.
    virtual bool RequiresStreaming() const;

    // Out of line on the console; ModeManager::ProcessNewRoadScore calls it for modes 2 and 16.
    // Buffers the score in the skillz manager until the lobby's next PreWorldUpdate.
    void BufferNewRoadScore(BrnStreetData::ChallengePlayerScoreEntry lChallengeScore,
                            BrnStreetData::ScoreType leScoreType,
                            BrnStreetData::ChallengeIndex lChallengeIndex);

    // Inlined into ModeManager::UpdateCurrentMode's online-showtime arm: only the skillz manager
    // ticks, flagged as exiting the lobby.
    void BurnoutSkillzOnlyPreWorldUpdate(GameStateModuleIO::OutputBuffer* lpOutput,
                                         const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                         const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCars,
                                         const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                                         bool lbPaused,
                                         const ScoringSystem* lpScoringSystem);

    // Inlined into ModeManager::SendModeStopMessages.
    void OnModeEnd(bool lbExitingFreeburnLobby);

    // Inlined into ModeManager::Construct, each firing its own assert ahead of the manager's.
    void SetStreetManager(StreetManager* lpStreetManager)
    {
        CGS_ASSERT(lpStreetManager, "lpStreetManager");
        mBurnoutSkillzManager.SetStreetManager(lpStreetManager);
    }

    void SetMugshotManager(MugshotManager* lpMugshotManager)
    {
        CGS_ASSERT(lpMugshotManager, "lpMugshotManager");
        mBurnoutSkillzManager.SetMugshotManager(lpMugshotManager);
    }

private:
    // Console mode +0xB8, 0xA0 bytes.
    BurnoutSkillzManager mBurnoutSkillzManager;
};

// ---- VTABLE-BINDING TRIPWIRE (see the explanation in BrnOfflineGameMode.h) ----------------------
static_assert(sizeof(static_cast<void (OnlineFreeBurnLobbyMode::*)(ModeManager*)>(&OnlineFreeBurnLobbyMode::Construct)) != 0,
              "OnlineFreeBurnLobbyMode::Construct must bind GameMode vtable slot 0");
static_assert(sizeof(static_cast<void (OnlineFreeBurnLobbyMode::*)(GameStateModuleIO::OutputBuffer*, const GameStateModuleIO::PreWorldInputBuffer*, const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface*, const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*, bool, const ScoringSystem*)>(&OnlineFreeBurnLobbyMode::PreWorldUpdate)) != 0,
              "OnlineFreeBurnLobbyMode::PreWorldUpdate must bind GameMode vtable slot 2");
static_assert(sizeof(static_cast<void (OnlineFreeBurnLobbyMode::*)(const GameStateModuleIO::PostWorldInputBuffer*)>(&OnlineFreeBurnLobbyMode::PostWorldUpdate)) != 0,
              "OnlineFreeBurnLobbyMode::PostWorldUpdate must bind GameMode vtable slot 3");
static_assert(sizeof(static_cast<const char* (OnlineFreeBurnLobbyMode::*)() const>(&OnlineFreeBurnLobbyMode::GetName)) != 0,
              "OnlineFreeBurnLobbyMode::GetName must bind GameMode vtable slot 6");
static_assert(sizeof(static_cast<void (OnlineFreeBurnLobbyMode::*)(const StartGameModeParams*, GameModeParams*, ScoringSystem*)>(&OnlineFreeBurnLobbyMode::Start)) != 0,
              "OnlineFreeBurnLobbyMode::Start must bind GameMode vtable slot 5");
static_assert(sizeof(static_cast<void (OnlineFreeBurnLobbyMode::*)(EGameModeEvent)>(&OnlineFreeBurnLobbyMode::SendEvent)) != 0,
              "OnlineFreeBurnLobbyMode::SendEvent must bind GameMode vtable slot 12");
static_assert(sizeof(static_cast<void (OnlineFreeBurnLobbyMode::*)(::EActiveRaceCarIndex)>(&OnlineFreeBurnLobbyMode::PlayerHasSpawned)) != 0,
              "OnlineFreeBurnLobbyMode::PlayerHasSpawned must bind GameMode vtable slot 19");
static_assert(sizeof(static_cast<void (OnlineFreeBurnLobbyMode::*)(GameStateModuleIO::OutputBuffer*, BrnStreetData::ChallengePlayerScoreEntry, BrnStreetData::ScoreType, BrnStreetData::ChallengeIndex, ::EActiveRaceCarIndex)>(&OnlineFreeBurnLobbyMode::ProcessNewRoadScore)) != 0,
              "OnlineFreeBurnLobbyMode::ProcessNewRoadScore must bind GameMode vtable slot 20");
static_assert(sizeof(static_cast<void (OnlineFreeBurnLobbyMode::*)(BrnStreetData::RoadIndex)>(&OnlineFreeBurnLobbyMode::OnEnterRoad)) != 0,
              "OnlineFreeBurnLobbyMode::OnEnterRoad must bind GameMode vtable slot 21");
static_assert(sizeof(static_cast<bool (OnlineFreeBurnLobbyMode::*)() const>(&OnlineFreeBurnLobbyMode::RequiresStreaming)) != 0,
              "OnlineFreeBurnLobbyMode::RequiresStreaming must bind GameMode vtable slot 23");
}
