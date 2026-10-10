// BrnGameState::GameStateInviteManager -- lifecycle (Construct / Prepare / Destruct).
//
// The console inlines all three into the owning GameStateModule (the manager sits at gsm+0x7F0):
// Construct right after PaybackManager::Construct in GameStateModule::Construct, Prepare as
// GameStateModule::Prepare's stage 17, Destruct right after ModeManager::Destruct in
// GameStateModule::Destruct.

#include "GameSource/GameState/InviteManager/BrnGameStateInviteManager.h"

namespace BrnGameState
{

// Builds the five queues, parks both state words on their COUNT sentinels (nothing in progress)
// and clears the per-module prepared bits.
void GameStateInviteManager::Construct()
{
    mGameEventQueue.Construct();
    mOutputBindRequestQueue.Construct();
    mOutputUnBindRequestQueue.Construct();
    mInputBindResultQueue.Construct();
    mInputUnBindResultQueue.Construct();

    mePrepareInviteSubstate = E_PREPARE_INVITE_SUBSTATE_COUNT;
    meInviteState           = E_INVITE_STATE_COUNT;
    mModulePreparedBitArray.UnSetAll();
}

// Prepares the game-event queue, clears the prepared bits, empties the queue. Always succeeds.
bool GameStateInviteManager::Prepare()
{
    mGameEventQueue.Prepare();
    mModulePreparedBitArray.UnSetAll();
    mGameEventQueue.Clear();
    return true;
}

// Clears the prepared bits, destructs the game-event queue and parks both state words again.
void GameStateInviteManager::Destruct()
{
    mModulePreparedBitArray.UnSetAll();
    mGameEventQueue.Destruct();

    mePrepareInviteSubstate = E_PREPARE_INVITE_SUBSTATE_COUNT;
    meInviteState           = E_INVITE_STATE_COUNT;
}

}
