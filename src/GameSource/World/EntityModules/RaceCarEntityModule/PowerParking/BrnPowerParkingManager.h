#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 (= rw::math::vpu::Vector3)
#include "GameSource/World/EntityModules/RaceCarEntityModule/PowerParking/BrnPowerParkingDebugComponent.h"

// ============================================================================
// GameSource/World/EntityModules/RaceCarEntityModule/PowerParking/BrnPowerParkingManager.h
//
// BrnWorld Power-Parking mini-game scorer. This header homes:
//   - EPowerParkOutcome                (DWARF BrnPowerParkingManager.h:48)
//   - struct PowerParkingManager       (DWARF BrnPowerParkingManager.h:71)
//
// The PowerParkingManager LAYOUT is grounded in the X360 ARTIST asm (member offsets pinned
// by the store displacements in Prepare @0x822C24B8 / Update @0x822F8400 / DetermineOutcome
// @0x822A74A0 / UpdateScoring @0x822A7140) cross-checked against the DecFIGS DWARF member list.
// Offsets are the X360 (32-bit) form; on the PC/x64 build the embedded debug component widens
// (pointer members), so members are accessed BY NAME -- no X360 byte-offset is pinned.
//
// NOTE: the struct previously lived (forked, minimal, mis-ordered) inside
// BrnPowerParkingDebugComponent.h; it is homed here now and that header forward-declares it.
// ============================================================================
namespace BrnWorld
{
    // DWARF BrnPowerParkingManager.h:48 -- outcome of a completed power-park attempt.
    enum EPowerParkOutcome
    {
        E_PPO_TO_BE_DETERMINED = 0,
        E_PPO_SUCCESS          = 1,
        E_PPO_FAILURE          = 2,
        E_PPO_COUNT            = 3,
    };

    // DWARF BrnPowerParkingManager.h:71. Member order/offsets pinned by the ARTIST asm.
    // X360 byte offsets are noted for provenance only (PC/x64 widths differ once the embedded
    // debug component is accounted for).
    struct PowerParkingManager
    {
        bool              mbPowerParkInProgress;              // 0x00
        EPowerParkOutcome mePowerParkOutcome;                // 0x04
        f32               mfTimeUntilDisplayOutcome;         // 0x08
        s32               miOverallRating;                   // 0x0C

        f32               mfProximityScore;                  // 0x10
        f32               mfRotationScore;                   // 0x14
        f32               mfDistanceScore;                   // 0x18
        f32               mfSpeedScore;                      // 0x1C
        f32               mfPositionAlignmentScore;          // 0x20
        f32               mfAngleAlignmentScore;             // 0x24

        s32               miWeightedDistanceScore;           // 0x28
        s32               miWeightedProximityScore;          // 0x2C
        s32               miWeightedSpeedScore;              // 0x30
        s32               miWeightedRotationScore;           // 0x34
        s32               miWeightedPositionAlignmentScore;  // 0x38
        s32               miWeightedAngleAlignmentScore;     // 0x3C

        Vector3           mvPositionLastFrame;               // 0x40
        Vector3           mvFacingLastFrame;                 // 0x50
        f32               mfLowestSpeedThisPark;             // 0x60

        s32               miContactTrafficCount;             // 0x64
        s32               miNearTrafficCount;                // 0x68
        u32               muNearbyParkedCarCount;            // 0x6C
        u32               muNearbyParkedPlayerCount;         // 0x70

        f32               mfClosestDistanceSq;               // 0x74
        f32               mfSecondClosestDistanceSq;         // 0x78
        f32               mfClosestAngleDiff;                // 0x7C
        f32               mfClosestPerpendicularDist;        // 0x80

        PowerParkingDebugComponent mPowerParkingDebugComponent;  // 0x84 (X360)
        bool              mbDebugForcePowerPark;             // 0x94 (X360)

        void Construct();
        void Destruct();
        bool Prepare();
        void ClearData();
        void DetermineOutcome();
        void AddNearTraffic(u32 luEntityId);
        void AddContactTraffic(u32 luEntityId);
        void SetNearbyParkedTrafficData(u32 luNearbyParkedCarCount,
                                        u32 luNearbyParkedPlayerCount,
                                        f32 lfClosestDistanceSq,
                                        f32 lfSecondClosestDistanceSq,
                                        f32 lfClosestAngleDiff,
                                        f32 lfClosestPerpendicularDist);
        bool IsPowerParking() const;
        void UpdateScoring(f32 lfAngleChange, f32 lfPositionChange, f32 lfCurrentLinearVelocity);

        // NOTE: Update(EGameModeType, f32, ActiveRaceCar*, PlayerVehicleControls*, GameEventQueue*)
        // is X360-attested but declared alongside its body (it needs BrnGameState / ActiveRaceCar /
        // PlayerVehicleControls / GameEventQueue types); left out of this additive home for now.
    };
}
