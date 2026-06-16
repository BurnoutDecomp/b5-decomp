#include "GameSource/GameState/BrnGameStateModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/GameState/ModeManager/BrnModeManager.h"            // BrnGameState::ModeManager::GetCurrentGameMode
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"     // BrnGameState::GameMode::IsOnline

namespace BrnGameState
{
// X360 @ 0x823116D0. Returns whether the currently-running game mode is one of the online modes. May
// only be called while the module is updating (asserts mbIsUpdating). Fetches the current game mode
// from the embedded ModeManager and forwards to GameMode::IsOnline(); if there is no current mode,
// returns false. (X360 reads the current-mode pointer inline as *(this + 0x1DB8) inside mModeManager
// and its mbIsOnline at *(mode + 172); de-inlined to the two logical calls.)
bool GameStateModule::IsOnlineGameMode()
{
    if (!mbIsUpdating)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "Can not use this function unless module is updating\n",
            "..\\..\\..\\GameSource\\GameState/BrnGameStateModule.h",
            1004);
        CgsDev::Assert::EndAssert();
    }

    const GameMode* lpCurrentGameMode = mModeManager.GetCurrentGameMode();
    if (lpCurrentGameMode != nullptr)
    {
        return lpCurrentGameMode->IsOnline();
    }
    return false;
}
}
