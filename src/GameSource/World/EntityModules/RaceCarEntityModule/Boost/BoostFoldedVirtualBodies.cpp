// ARTIST folds byte-identical BoostBurnout virtual overrides onto a single
// surviving body.  These definitions restore the source-level class surface
// using the concrete slot targets in the three X360 vtables:
//   BoostBurnout2 0x820CEC30, BoostBurnout3 0x820CECF8,
//   BoostBurnout5 0x820CEDC8.
// The non-empty bodies below are written from the assembly of the exact target
// selected by those slots; the empty overrides all point at 0x8284CB38.

#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout2.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"

namespace BrnWorld
{

// BoostBurnout2 slots 4, 9 and 11 all target the shared empty body.
void BoostBurnout2::OnTakenDownByAIOrPlayer() {}
void BoostBurnout2::OnSlammed() {}
void BoostBurnout2::OnShortcut() {}

// B2 slot 13 targets BoostBurnout5::OnStartCrashPlay @0x822D5330.
void BoostBurnout2::OnStartCrashPlay()
{
    mfMinBoostAllowedAmount = 1.1920929e-07f;
    miBoostLevel = miCombinedBoostLevel - 1;
    UpdateMaxBoost(false);
}

// B2 slot 26 targets the base implementation @0x822A5EE0.
bool BoostBurnout2::IsBoostFull() const
{
    return mbIsBoostFull;
}

// B3 slot 9 targets BoostBurnout5::OnSlammed @0x822A6A20.
void BoostBurnout3::OnSlammed()
{
    RemoveBoost(mfBeingSlammed);
}

// B3 slot 11 targets the shared empty body.
void BoostBurnout3::OnShortcut() {}

// B3 slot 12 targets BoostBurnout2::OnTrafficCheck @0x822A6358.  Unlike
// BoostBurnout5's override, this body does not set mbJustTrafficChecked.
void BoostBurnout3::OnTrafficCheck()
{
    AddBoost(mfTrafficCheck);
}

// B5 slot 4 targets the shared empty body.
void BoostBurnout5::OnTakenDownByAIOrPlayer() {}

// B5 slot 5 targets BoostBurnout2::OnPlayerAttacksRival @0x822A6E68.
void BoostBurnout5::OnPlayerAttacksRival(BrnPhysics::Vehicle::EImpactType leImpactType)
{
    switch (leImpactType)
    {
    case BrnPhysics::Vehicle::E_IMPACT_TRADING_PAINT:
        AddBoost(mfTradingPaintEarning);
        break;
    case BrnPhysics::Vehicle::E_IMPACT_SLAM:
    case BrnPhysics::Vehicle::E_IMPACT_BOOST_SLAM:
        AddBoost(mfSlamEarning);
        break;
    case BrnPhysics::Vehicle::E_IMPACT_SHUNT:
    case BrnPhysics::Vehicle::E_IMPACT_BOOST_SHUNT:
        AddBoost(mfShuntEarning);
        break;
    case BrnPhysics::Vehicle::E_IMPACT_GRINDING:
        AddBoost(mfGrindingEarning);
        break;
    case BrnPhysics::Vehicle::E_IMPACT_RUBBING:
        AddBoost(mfRubbingEarning);
        break;
    default:
        break;
    }
}

// B5 slot 6 targets BoostBurnout3::OnNearMiss @0x822A66A8.
void BoostBurnout5::OnNearMiss(ENearMissType leNearMissType)
{
    switch (leNearMissType)
    {
    case E_NEAR_MISS_NORMAL_TRAFFIC:
    case E_NEAR_MISS_NORMAL_OTHER_RACE_CAR:
        AddBoost(mfNearMissBoostEarning);
        break;
    case E_NEAR_MISS_CRASH_ESCAPE_TRAFFIC:
    case E_NEAR_MISS_CRASH_ESCAPE_OTHER_RACE_CAR:
        AddBoost(mfCrashEscapeBoostEarning);
        break;
    }
}

// B5 slot 8 targets BoostBurnout3::OnWrecked @0x822C2438.
void BoostBurnout5::OnWrecked(bool lbIsInOnlineGameMode)
{
    RemoveBoost(mfOnWrecked);

    if (mfBoostAmount < 0.0f)
    {
        mfBoostAmount = 0.0f;
    }
    if (!(mfBoostAmount <= mfMaxBoost))
    {
        mfBoostAmount = mfMaxBoost;
    }

    if (lbIsInOnlineGameMode && mfBoostAmount < mfMinBoostAllowedAmount)
    {
        f32 lfRestoredBoost = mfMinBoostAllowedAmount + 1.0f;
        if (lfRestoredBoost < 0.0f)
        {
            lfRestoredBoost = 0.0f;
        }
        if (!(lfRestoredBoost <= mfMaxBoost))
        {
            lfRestoredBoost = mfMaxBoost;
        }
        mfBoostAmount = lfRestoredBoost;
    }
}

} // namespace BrnWorld
