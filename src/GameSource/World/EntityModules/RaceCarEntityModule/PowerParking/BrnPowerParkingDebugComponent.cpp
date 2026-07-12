#include "GameSource/World/EntityModules/RaceCarEntityModule/PowerParking/BrnPowerParkingDebugComponent.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/PowerParking/BrnPowerParkingManager.h"  // PowerParkingManager (member access)
#include "GameShared/GameClasses/Core/CgsAssert.h"

// BrnWorld::PowerParkingDebugComponent member functions, reconstructed from BURNOUT_X360_ARTIST.XEX.
// The debug component for the Power Parking scorer; mirrors the committed sibling
// BrnCrashPlayDebugComponent.cpp store-for-store.

namespace BrnWorld
{
    // X360 0x822A75B8 -- indirect lwz through the off_... "PowerParking" rodata pointer.
    const char* PowerParkingDebugComponent::GetName() const
    {
        return "PowerParking";
    }

    // X360 0x822A75C8 -- register the manager's scoring fields with the debug menu. Read-only
    // diagnostics (nearby-car count + the closest-distance/angle/perp measurements) get SetReadOnly.
    void PowerParkingDebugComponent::OnActivate()
    {
        RegisterVariable(&mpPowerParkingManager->mfDistanceScore, "", "Distance Score");
        RegisterVariable(&mpPowerParkingManager->mfProximityScore, "", "Proximity Score");
        RegisterVariable(&mpPowerParkingManager->mfSpeedScore, "", "Speed Score");
        RegisterVariable(&mpPowerParkingManager->mfRotationScore, "", "Rotation Score");
        RegisterVariable(&mpPowerParkingManager->mfAngleAlignmentScore, "", "Angle Alignment Score");
        RegisterVariable(&mpPowerParkingManager->mfPositionAlignmentScore, "", "Position Alignment Score");
        RegisterVariable(&mpPowerParkingManager->miWeightedDistanceScore, "", "Weighted Distance Score");
        RegisterVariable(&mpPowerParkingManager->miWeightedProximityScore, "", "Weighted Proximity Score");
        RegisterVariable(&mpPowerParkingManager->miWeightedSpeedScore, "", "Weighted Speed Score");
        RegisterVariable(&mpPowerParkingManager->miWeightedRotationScore, "", "Weighted Rotation Score");
        RegisterVariable(&mpPowerParkingManager->miWeightedAngleAlignmentScore, "", "Weighted Angle Alignment Score");
        RegisterVariable(&mpPowerParkingManager->miWeightedPositionAlignmentScore, "", "Weighted Position Alignment Score");
        RegisterVariable(&mpPowerParkingManager->muNearbyParkedCarCount, "", "Nearby Parked Cars");
        SetReadOnly(&mpPowerParkingManager->muNearbyParkedCarCount, true);
        RegisterVariable(&mpPowerParkingManager->mfClosestDistanceSq, "", "Closest Distance Squared");
        SetReadOnly(&mpPowerParkingManager->mfClosestDistanceSq, true);
        RegisterVariable(&mpPowerParkingManager->mfSecondClosestDistanceSq, "", "Second Closest Distance Squared");
        SetReadOnly(&mpPowerParkingManager->mfSecondClosestDistanceSq, true);
        RegisterVariable(&mpPowerParkingManager->mfClosestAngleDiff, "", "Closest Angle Diff");
        SetReadOnly(&mpPowerParkingManager->mfClosestAngleDiff, true);
        RegisterVariable(&mpPowerParkingManager->mfClosestPerpendicularDist, "", "Closest Perpendicular Diff");
        SetReadOnly(&mpPowerParkingManager->mfClosestPerpendicularDist, true);
        RegisterVariable(&mpPowerParkingManager->mbDebugForcePowerPark, "", "FORCE PARKING (set ON then OFF)");
    }

    // X360 0x822A7570 -- per-frame update: only asserts the manager pointer is live.
    void PowerParkingDebugComponent::Update()
    {
        CGS_ASSERT(mpPowerParkingManager != 0, "mpPowerParkingManager != NULL");
    }
}
