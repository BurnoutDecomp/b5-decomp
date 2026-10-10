// BrnGameState::GameStateModule::OnEnterOnline -- ProcessGameEvents case 29 (E_EVENT_PREPARE_FOR_ONLINE).
// Before the online session takes over: leave a running offline mode, leave the junkyard, lift a
// training pause, reset the player's crash state and turn the road rules to their online mode.

#include "GameSource/GameState/BrnGameStateModule.h"

#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // AddEvent
#include "GameSource/GameState/BrnGameStateModuleIO.h"                   // OutputBuffer
#include "GameSource/GameState/BrnGameActions.h"                         // ResetCrashingAction (9)
#include "GameSource/GameState/ModeManager/BrnModeManager.h"             // IsInGameMode / IsOnlineGameMode / ExitCurrentMode
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"          // IsInJunkyard / ForceExitJunkyard
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"     // ForceUnpause
#include "GameSource/GameState/RoadRules/BrnRoadRulesManager.h"          // SetRoadRulesMode

namespace BrnGameState
{

void GameStateModule::OnEnterOnline(GameStateModuleIO::OutputBuffer*    lpOutput,
                                    GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    // An offline mode is quit outright; the next-mode argument is the 18 "no successor" value.
    if (mModeManager.IsInGameMode() && !mModeManager.IsOnlineGameMode())
    {
        mModeManager.ExitCurrentMode(lpOutput, true,
                                     static_cast<GameStateModuleIO::EGameModeType>(ModeManager::KI_GAME_MODE_SLOTS));
    }

    if (mCarSelectManager.IsInJunkyard())
    {
        mCarSelectManager.ForceExitJunkyard(lpActionQueue, false);
    }

    // Inlined here on the console: action 151 when the latched tip pauses the game.
    mpTrainingManager->ForceUnpause(lpActionQueue);

    const GameStateModuleIO::ResetCrashingAction lResetCrashingAction = {};
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResetCrashingAction),
                            GameStateModuleIO::E_ACTION_RESET_CRASHING,
                            static_cast<s32>(sizeof(lResetCrashingAction)));

    mRoadRulesManager.SetRoadRulesMode(lpOutput, true);
}

}
