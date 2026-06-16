#include "GameSource/GameState/ModeManager/GameModes/BrnCrashMode.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnGameState
{
// X360: BrnGameState::CrashMode::SendEvent (0x82330A58). Maps an inbound EGameModeEvent to a
// state transition. ABORT -> Quit(5); RESTART -> Intro(1); otherwise the per-state flow
// Intro(1)->InProgress(2)->Outro(3)->Results(4)->Quit(5). Showtime has no countdown phase, so any
// other current state is a programmer error (the X360-baked assert). SetCurrentState returns void;
// the pseudocode's result/return-result are register artifacts and are dropped. State ids are
// GameStateModuleIO::EGameModeState values passed as raw s32 to the base SetCurrentState.
void CrashMode::SendEvent(EGameModeEvent leEvent)
{
    if (leEvent == E_GME_ABORT)
    {
        SetCurrentState(5);   // -> Quit
        return;
    }
    if (leEvent == E_GME_RESTART)
    {
        SetCurrentState(1);   // -> Intro
        return;
    }

    switch (meCurrentState)
    {
        case 1:   // Intro
            if (leEvent == E_GME_NEXT || leEvent == E_GME_USER_ACCEPT)
            {
                SetCurrentState(2);   // -> InProgress
            }
            break;
        case 2:   // InProgress
            if (leEvent == E_GME_NEXT)
            {
                SetCurrentState(3);   // -> Outro
            }
            break;
        case 3:   // Outro
            if (leEvent == E_GME_NEXT)
            {
                SetCurrentState(4);   // -> Results
            }
            break;
        case 4:   // Results
            if (leEvent == E_GME_USER_ACCEPT || leEvent == E_GME_NEXT)
            {
                SetCurrentState(5);   // -> Quit
                return;
            }
            break;
        case 5:   // Quit -- terminal
            return;
        default:
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "Should not be in this state in Showtime mode!",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/ModeManager/GameModes/BrnCrashMode.cpp",
                151);
            CgsDev::Assert::EndAssert();
            break;
    }
}
}
