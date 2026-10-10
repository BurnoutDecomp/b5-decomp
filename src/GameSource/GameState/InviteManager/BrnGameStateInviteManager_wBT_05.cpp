// BrnGameState::GameStateInviteManager -- the two inline requests GameStateModule::ProcessGameEvents
// makes of the invite manager (debug info BrnGameStateInviteManager.h). Neither has an
// out-of-line copy on the console: both are inlined into the request-invite and prepare-for-invite
// arms (cases 57 and 58), which is where these bodies are read from.

#include "GameSource/GameState/InviteManager/BrnGameStateInviteManager.h"

namespace BrnGameState
{

// Case 57 takes the event's parameter block by value and stores it whole (0x9C bytes) into
// mInviteOrJoinParams, before anything else the arm does.
void GameStateInviteManager::RequestInvite(BrnNetwork::BrnNetworkModuleIO::InviteOrJoinParams lParams)
{
    mInviteOrJoinParams = lParams;
}

// Cases 57 and 58 both make exactly these two stores: the invite restarts at START_INVITE (Update
// then hands the parameters to the other modules) with the controller re-bind sub-state at DONE.
void GameStateInviteManager::StartPrepareForInvite()
{
    meInviteState           = E_INVITE_STATE_START_INVITE;
    mePrepareInviteSubstate = E_PREPARE_INVITE_SUBSTATE_DONE;
}

}
