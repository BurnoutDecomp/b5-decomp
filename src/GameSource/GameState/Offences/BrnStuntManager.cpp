#include "GameSource/GameState/Offences/BrnStuntManager.h"
#include "GameShared/GameClasses/World/CgsWorldMap2D.h"   // CgsWorld::WorldMap2D
#include "SharedClasses/Trigger/BrnGenericRegion.h"       // BrnTrigger::GenericRegion
#include "SharedClasses/Trigger/BrnTriggerBase.h"
#include "SharedClasses/Trigger/BrnRegion.h"              // BoxRegion::GetPosition2D
#include "SharedClasses/World/BrnWorldRegion.h"           // BrnWorld::WorldRegion / ECounty / EDistrict

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGameState::StuntManager::FindTriggersCounty  @ 0x8236B310
namespace BrnGameState
{

// X360 0x8236B310. The pseudocode copies the trigger box centre (the first 12
// bytes of the GenericRegion == TriggerRegion::mBoxRegion position xyz) into a
// stack vector, then a vperm shuffles it into the (x, z) lanes the 2D world map
// wants -- i.e. BoxRegion::GetPosition2D(). The map sample is taken on the
// mWorldMap2D member (the `a1 + 880` the disassembly calls GetValue() on).
BrnWorld::ECounty
StuntManager::FindTriggersCounty(const BrnTrigger::GenericRegion* lpRegion)
{
    const Vector2 lPosition2D = lpRegion->GetBoxRegion()->GetPosition2D();

    const uint8_t luDistrictValue = mWorldMap2D.GetValue(lPosition2D);
    if (luDistrictValue == CgsWorld::KU_INVALID_WORLD_MAP_VALUE)   // 255
    {
        // Position falls outside the district grid -> no county.
        return BrnWorld::E_COUNTY_INVALID;   // == E_COUNTY_VALID_COUNT == 5 (the X360 `return 5`)
    }

    // The grid byte is a district id; WorldRegion::Construct() converts it to its
    // owning county (and stores meCounty @ +0, the `return v8[0]`).
    BrnWorld::WorldRegion lRegion;
    lRegion.Construct(static_cast<BrnWorld::EDistrict>(luDistrictValue));
    return lRegion.GetCounty();
}

}
