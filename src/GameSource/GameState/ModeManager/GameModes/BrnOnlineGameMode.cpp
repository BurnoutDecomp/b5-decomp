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
//
// [!] NAME CORRECTED 2026-08-26 (wave-B fix round): the `*(this+172) = 1` store was transcribed
// as `mbConstructed`. It is mbIsOnline -- OfflineGameMode::Construct @0x8232FE78 stores 0 into the
// same byte, and ModeManager::ProcessEvent @0x82340AF4 reads it (`lbz r11,0xAC(r11)`) to choose
// the ONLINE stunt scorer over the OFFLINE one. Asm: `li r11,1; stb r11,0xAC(r31)` @0x8232FEB0.
void OnlineGameMode::Construct(ModeManager* lpModeManager)
{
    GameMode::Construct(lpModeManager);
    mbIsOnline = true;
}

// X360 vtable slot 7 (vtbl+28), folded leaf 0x827DF718 == `li r3,2; blr`, identical in all seven
// online mode vtables. See the restoration note in BrnOnlineGameMode.h.
CgsSystem::EFrameRateManagerType OnlineGameMode::GetFrameRateType() const
{
    return CgsSystem::E_FRAMERATEMANAGER_MULTIPLE_UNCAPPED;
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
                // [!] NAME CORRECTED 2026-08-26 (wave-B fix round): this store was transcribed as
                // `mbFinalStandingsShown`. The asm is `li r11,1; stb r11,0xAF(r31)` @0x8232FFA0,
                // i.e. *(this+175) -- and +175 is mbShowResultsRequested (GameMode::Construct
                // zeroes 0xAF, GameMode::Initialise clears it with the rest of the 173..178 latch
                // run). Every byte of 172..179 is claimed by a proven reader/writer, so there is
                // no console home for a separate `mbFinalStandingsShown`; the name is retired.
                // See the +160..+179 table in BrnGameMode.h.
                mbShowResultsRequested = true;
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
