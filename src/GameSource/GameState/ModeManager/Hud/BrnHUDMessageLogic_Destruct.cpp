#include "GameSource/GameState/ModeManager/Hud/BrnHUDMessageLogic.h"

namespace BrnGameState
{
// ARTIST 0x8236F5E8 resets the team-change bits and the buffered-crash pool.
void HUDMessageLogic::Destruct()
{
    // ARTIST 0x8236F604/0x8236F614 stores the team bits twice and the pool's occupancy word again
    // after its Clear (0x8236F648): each container is cleared, then Destruct()ed.
    mTeamChangedBits.UnSetAll();
    mTeamChangedBits.Destruct();
    mBufferedCrashingCars.Clear();
    mBufferedCrashingCars.Destruct();
}
}
