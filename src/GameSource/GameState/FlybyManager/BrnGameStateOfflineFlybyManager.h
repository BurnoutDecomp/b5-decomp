#pragma once

#include "GameSource/GameState/FlybyManager/BrnGameStateFlybyManager.h"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"

namespace BrnGameState
{
// DecFIGS BrnGameStateOfflineFlybyManager.h:42. ARTIST constructs this owner at
// GameStateModule+0x2D630; its message set follows the FlybyManager base at +0x2A0.
class OfflineFlybyManager : public FlybyManager
{
public:
    void Construct(GameStateModule* lpGameStateModule, ScoringSystem* lpScoringSystem);
    bool Prepare();

    const GameStateModuleIO::FlybyData* CalculateFlybyRivals() ;
    s32 CalculateNumberOfCarsInFlyby() ;

private:
    friend class GameStateModule;
    CgsContainers::BitArray<2> mStatShownMessages;
};
}
