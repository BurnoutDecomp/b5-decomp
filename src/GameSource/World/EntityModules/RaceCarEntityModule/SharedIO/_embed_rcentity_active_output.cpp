// Gate embedder for RCEntityActiveRaceCarOutputInterface: forces the full layout
// (sizeof/offsetof) and exercises a representative set of the bodied accessors.
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameSource/BurnoutConstants.h"

using namespace BrnWorld::RaceCarEntityModuleIO;

void _EmbedRCEntityActiveRaceCarOutput(RCEntityActiveRaceCarOutputInterface& lrIface)
{
    lrIface.Clear();
    (void)lrIface.GetRaceCarColour(E_ACTIVE_RACE_CAR_INDEX_0);
    (void)lrIface.GetActiveRaceCarColourIndex(E_ACTIVE_RACE_CAR_INDEX_0);
    (void)lrIface.GetActiveRaceCarPaintFinishIndex(E_ACTIVE_RACE_CAR_INDEX_0);
    (void)lrIface.GetCarModelId(E_ACTIVE_RACE_CAR_INDEX_0);
    (void)lrIface.GetGlobalRaceCarIndex(E_ACTIVE_RACE_CAR_INDEX_0);
    (void)lrIface.GetRaceCarAISection(E_ACTIVE_RACE_CAR_INDEX_0);
    (void)lrIface.GetRivalId(E_ACTIVE_RACE_CAR_INDEX_0);
    (void)lrIface.HasCrashedIntoWater(E_ACTIVE_RACE_CAR_INDEX_0);
    (void)lrIface.IsRaceCarNetwork(E_ACTIVE_RACE_CAR_INDEX_0);
    (void)lrIface.IsRaceCarRival(E_ACTIVE_RACE_CAR_INDEX_0);
    lrIface.SetPlayerActiveRaceCarData(E_ACTIVE_RACE_CAR_INDEX_0,
                                       E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING);
    (void)lrIface.GetPlayerActiveRaceCarIndex();
    (void)lrIface.GetPlayerSpeedMPH();
    RCEntityActiveRaceCarOutputInterface lCopy(lrIface);
    (void)lCopy;
}
