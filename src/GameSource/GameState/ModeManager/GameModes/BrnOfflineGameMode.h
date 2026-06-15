#pragma once

#include "types.hpp"
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"

namespace BrnGameState
{
// Owning header for the OfflineGameMode base. Reconstructed from the DecFIGS DWARF
// (BrnOfflineGameMode.h): derives from GameMode and adds the two debug data members
// below. Its own virtuals (Construct(ModeManager*)/GetFrameRateType) belong to the
// OfflineGameMode TU and are added here when it is reconstructed.
class OfflineGameMode : public GameMode
{
private:
    bool mbDebugAlwaysRaceToSingleLocation;
    s32  miDebugDesignIndexOfLandmarkToAlwaysRaceTo;
};
}
