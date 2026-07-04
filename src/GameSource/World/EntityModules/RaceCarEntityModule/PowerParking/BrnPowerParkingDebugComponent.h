#ifndef BRN_POWER_PARKING_DEBUG_COMPONENT_H
#define BRN_POWER_PARKING_DEBUG_COMPONENT_H

#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"

// BrnWorld::PowerParkingDebugComponent -- the in-game debug component for the Power Parking
// mini-game scorer (path "Gameplay", name "PowerParking"). Mirrors the committed sibling
// BrnCrashPlayDebugComponent: it holds a pointer to the (un-homed) PowerParkingManager and
// registers that manager's scoring fields with the debug menu on activation.
//
// PowerParkingManager has no committed home in src (no BrnPowerParkingManager.h), so -- following
// the CrashPlay sibling pattern -- it is modelled inline here with DWARF-attested named fields.
// The OnActivate registrations (BrnPowerParkingDebugComponent.cpp @ 0x822A75C8) pin the field set
// and their types (f32 scores, s32 weighted scores, u32 count, bool force flag).

namespace BrnWorld
{
    struct PowerParkingManager
    {
        // Raw per-frame scoring inputs.
        f32 mfDistanceScore;
        f32 mfProximityScore;
        f32 mfSpeedScore;
        f32 mfRotationScore;
        f32 mfAngleAlignmentScore;
        f32 mfPositionAlignmentScore;

        // Weighted (integerised) scores.
        s32 miWeightedDistanceScore;
        s32 miWeightedProximityScore;
        s32 miWeightedSpeedScore;
        s32 miWeightedRotationScore;
        s32 miWeightedAngleAlignmentScore;
        s32 miWeightedPositionAlignmentScore;

        // Read-only diagnostics.
        u32 muNearbyParkedCarCount;
        f32 mfClosestDistanceSq;
        f32 mfSecondClosestDistanceSq;
        f32 mfClosestAngleDiff;
        f32 mfClosestPerpendicularDist;

        // Debug override.
        bool mbDebugForcePowerPark;
    };

    class PowerParkingDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Update() override;

    protected:
        const char* GetName() const override;
        void        OnActivate() override;

    private:
        PowerParkingManager* mpPowerParkingManager;
    };
}

#endif
