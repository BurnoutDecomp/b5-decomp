#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineGameMode.h"

#include "GameSource/GameState/ModeManager/BrnModeManager.h"

namespace BrnGameState
{
// X360: BrnGameState::OnlineGameMode::Construct (0x8232FE98). Forwards to the GameMode
// base (which stores the ModeManager* and zero-inits the mode flags) and then marks this
// mode constructed. The base Construct takes the ModeManager* (DWARF-attested shape
// virtual void Construct(ModeManager*)); the Hex-Rays pseudocode renders the forwarded
// call with no visible argument and a leftover `int result`, both artifacts of a void
// function -- the reconstructed body forwards the argument and returns nothing.
void OnlineGameMode::Construct(ModeManager* lpModeManager)
{
    GameMode::Construct(lpModeManager);
    mbConstructed = true;
}

// X360: BrnGameState::OnlineGameMode::SendEvent (0x8232FED0). Drives the online mode's
// state machine in response to a game-mode event. SetCurrentState and SendEvent both
// return void in the DWARF-attested base API; the pseudocode's `result = ...` / `return
// result` / `return (int)this` are register-reuse artifacts of a void function and are
// dropped. State ids (0..7) are GameStateModuleIO::EGameModeState values; that enum is not
// yet reconstructed (out of scope for this de-fork), so they are passed as raw s32 to the
// base SetCurrentState, matching the X360 body.
void OnlineGameMode::SendEvent(EGameModeEvent leEvent)
{
    if (leEvent == E_GME_ABORT)
    {
        SetCurrentState(5);
        return;
    }
    if (leEvent == E_GME_RESTART)
    {
        SetCurrentState(6);
        return;
    }

    switch (meCurrentState)
    {
        case 0:
            if (leEvent == E_GME_NEXT)
            {
                SetCurrentState(2);
            }
            break;
        case 1:
            if (leEvent == E_GME_NEXT)
            {
                SetCurrentState(7);
            }
            else if (leEvent == E_GME_USER_ACCEPT)
            {
                SetCurrentState(0);
            }
            break;
        case 2:
            if (leEvent == E_GME_NEXT)
            {
                SetCurrentState(3);
                mbFinalStandingsShown = true;
            }
            break;
        case 3:
            if (leEvent == E_GME_NEXT)
            {
                SetCurrentState(4);
                mpModeManager->TellGuiToShowOnlineFinalStandings();
            }
            break;
        case 4:
            if (leEvent == E_GME_USER_ACCEPT)
            {
                SetCurrentState(5);
            }
            break;
        case 6:
            if (leEvent == E_GME_NEXT)
            {
                SetCurrentState(leEvent);
            }
            break;
        case 7:
            if (leEvent == E_GME_NEXT)
            {
                SetCurrentState(0);
            }
            break;
        default:
            break;
    }
}
}
