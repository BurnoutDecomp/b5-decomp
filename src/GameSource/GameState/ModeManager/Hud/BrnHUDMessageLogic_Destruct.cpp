#include "GameSource/GameState/ModeManager/Hud/BrnHUDMessageLogic.h"

namespace BrnGameState
{
// ARTIST 0x8236F5E8 resets the team-change bits and the buffered-crash pool.
void HUDMessageLogic::Destruct()
{
    mTeamChangedBits.UnSetAll();
    mBufferedCrashingCars.Clear();
}
}
