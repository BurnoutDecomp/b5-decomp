#include "GameSource/World/EntityModules/RaceCarEntityModule/PowerParking/BrnPowerParkingDebugComponent.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/PowerParking/BrnPowerParkingManager.h"  // PowerParkingManager (member access)
#include "GameShared/GameClasses/Core/CgsAssert.h"

// BrnWorld::PowerParkingDebugComponent member functions, reconstructed from BURNOUT_X360_ARTIST.XEX.
// The debug component for the Power Parking scorer; mirrors the committed sibling
// BrnCrashPlayDebugComponent.cpp store-for-store.

namespace BrnWorld
{
    // DWARF :42. No X360 symbol: PowerParkingManager::Construct inlines it into RaceCarEntityModule::
    // Construct as the one store 0x822FDB1C `stw r10, 0x90(r10)` (r10 == module + 0x18250, the
    // manager; +0x90 is this component's +0x0C). PS3 0x126C34 `stw r4, 0xC(r3)` -- nothing else.
    void PowerParkingDebugComponent::Construct(PowerParkingManager* lpPowerParkingManager)
    {
        mpPowerParkingManager = lpPowerParkingManager;
    }

    // DWARF :54. No X360 symbol: inlined into RaceCarEntityModule::Destruct @0x822F3DC0 on module
    // + 0x182E0 -- the "mpPowerParkingManager != NULL" tripwire (BrnPowerParkingDebugComponent.cpp:56),
    // the pointer nulled, then the base Destruct. PS3 0x126D14 is the same three steps.
    void PowerParkingDebugComponent::Destruct()
    {
        CGS_ASSERT(mpPowerParkingManager != 0, "mpPowerParkingManager != NULL");
        mpPowerParkingManager = 0;
        CgsDev::DebugComponent::Destruct();
    }

    // X360 0x822A75B8 -- indirect lwz through the off_... "PowerParking" rodata pointer.
    const char* PowerParkingDebugComponent::GetName() const
    {
        return "PowerParking";
    }

    // DWARF :95 -- X360 vtable 0x820CDF20 slot 4 = 0x82312480 (the folded "Gameplay" returner).
    const char* PowerParkingDebugComponent::GetPath() const
    {
        return "Gameplay";
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
