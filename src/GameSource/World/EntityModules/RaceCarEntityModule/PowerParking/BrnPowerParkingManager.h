#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 (= rw::math::vpu::Vector3)
#include "GameSource/World/EntityModules/RaceCarEntityModule/PowerParking/BrnPowerParkingDebugComponent.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (CheckVehicleForPowerPark's :205/:209/:213 range asserts)
#include "GameSource/Math/BrnMathUtils.h"            // BrnMath::GetPointToInfiniteLineDistance
#include "rw/math/vpu/vector3_operation.h"           // rw::math::vpu::operator- / operator+
#include "rw/math/fpu/scalar_operation.h"            // rw::math::fpu::Min (the console's fsel Min)
#include "GameSource/GameState/BrnGameStateSharedIO.h"  // BrnGameState::GameStateModuleIO::EGameModeType (Update's by-value param)

#include <cmath>                                     // std::atan / std::fabs / std::copysign

// ============================================================================
// GameSource/World/EntityModules/RaceCarEntityModule/PowerParking/BrnPowerParkingManager.h
//
// BrnWorld Power-Parking mini-game scorer. This header homes:
//   - KF_POWER_PARK_NEARBY_RADIUS      (DWARF BrnPowerParkingManager.h:46)
//   - EPowerParkOutcome                (DWARF BrnPowerParkingManager.h:48)
//   - struct PowerParkingManager       (DWARF BrnPowerParkingManager.h:71)
//   - CheckVehicleForPowerPark         (DWARF BrnPowerParkingManager.h:182, X360 0x822B1FA0)
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
    // Update's pointer-only parameters -- forward declarations (the BoostStrategy.h precedent). The
    // queue's home is SharedIO/BrnRaceCarEntityModuleIOQueues.h (`struct GameEventQueue :
    // CgsModule::VariableEventQueue<1536,16>`); the DWARF spells the parameter
    // GameStateModuleIO::GameEventQueue* in the declaration and OutputBuffer_PrePhysics::GameEventQueue*
    // in the definition -- the same 1536/16 queue UpdatePowerParking takes off its output buffer.
    class ActiveRaceCar;
    struct PlayerVehicleControls;
    namespace RaceCarEntityModuleIO { struct GameEventQueue; }

    // DWARF BrnPowerParkingManager.h:46. The image carries only its square: CheckVehicleForPowerPark's
    // range test `lfDistanceSq > KF_POWER_PARK_NEARBY_RADIUS * KF_POWER_PARK_NEARBY_RADIUS` is folded
    // to the literal 225.0f (flt_82018E3C == 0x43610000, read at 0x822B2004/0x822B200C), and 15.0f is
    // the one positive binary32 value whose square rounds to it.
    const f32 KF_POWER_PARK_NEARBY_RADIUS = 15.0f;

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

        // DWARF :75 / :78. Both inlined on the console -- into RaceCarEntityModule::Construct
        // (0x822FDB14 / 0x822FDB1C) and ::Destruct (0x822F3DC0); bodies in the .cpp.
        void Construct();
        void Destruct();
        bool Prepare();
        // DWARF :89. X360 0x822F8400 (crash parity FX-RCEM4 2026-09-24).
        void Update(BrnGameState::GameStateModuleIO::EGameModeType leGameModeType, f32 lfSimTimerStep,
                    ActiveRaceCar* lpPlayerActiveRaceCar, PlayerVehicleControls* lpPlayerControls,
                    RaceCarEntityModuleIO::GameEventQueue* lpEventQueue);
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
        // DWARF :115 -- a header inline (no out-of-line symbol; ProcessPowerParking reads +0 twice).
        bool IsPowerParking() const { return mbPowerParkInProgress; }
        void UpdateScoring(f32 lfAngleChange, f32 lfPositionChange, f32 lfCurrentLinearVelocity);
    };

    // ---- the candidacy test (crash parity FX-RCEM4 2026-09-24) -------------------------------------
    // Transcribed from the ARTIST dump of 0x822B1FA0. (A 2026-07 reconstruction had fabricated a body
    // with no dump behind it and was rightly removed; this one is read instruction by instruction.)
    namespace PowerParkingDetail
    {
        // The three RwMathFPU constants the angle folds and their range asserts read.
        const f32 KF_HALF_PI = 1.5707964f;   // RwMathFPU::HALF_PI, flt_82014444 (0x3FC90FDB), 0x822B2068 / 0x822B2258
        const f32 KF_PI      = 3.1415927f;   // RwMathFPU::PI,      flt_8201443C (0x40490FDB), 0x822B206C
        const f32 KF_TWO_PI  = 6.2831855f;   // RwMathFPU::TWO_PI,  flt_82001C94 (0x40C90FDB), 0x822B21B8

        // rw::math::vpu::ATan2(a, b) -- the SDK's inlined vector arctangent (DWARF
        // trig_operation_inline.h:60; locals zeroVec / signBit / a_signBit / bNeg / bZero / res1 /
        // bRecip / res2 / b_signBit / newRes). CheckVehicleForPowerPark expands it twice
        // (0x822B2030..0x822B212C for the player, 0x822B20C4..0x822B218C for the vehicle):
        //   base = XMVectorATan(a * Reciprocal(b))       vrefp128 + one Newton step, vmaddcfp128, bl
        //   vcmpgtfp128(0, b) -> vsel: b <  0 -> base + copysign(pi,   a)   (vand a, 0x80000000 ; vor)
        //   vcmpeqfp128(0, b) -> vsel: b == 0 ->        copysign(pi/2, a)
        // NOT std::atan2: b == 0 answers +/-pi/2 even when a is also 0 (std::atan2(0, 0) is 0).
        // FLAG (PC-platform, numeric): the vrefp estimate + one Newton-Raphson step and XMVectorATan's
        // minimax polynomial are de-optimised to the exact divide and std::atan -- the standing
        // convention of this tree (CameraUtils.cpp's ATan2 transcribes the same SDK helper).
        // MOVE-WHEN: rw/math/vpu gains its trig_operation header; this is the SDK function, not game code.
        inline f32 ATan2(f32 a, f32 b)
        {
            if (b == 0.0f)
                return std::copysign(KF_HALF_PI, a);

            const f32 lfBase = std::atan(a / b);

            if (b < 0.0f)
                return lfBase + std::copysign(KF_PI, a);

            return lfBase;
        }
    }

    // DWARF BrnPowerParkingManager.h:182 (locals lPlayerToVehicle :184, lfDistanceSq :186,
    // lfPlayerAngle :199, lfTrafficAngle :201). X360 0x822B1FA0 is the ONE out-of-line copy of this
    // header inline; both ends of the Power Parking chain call it with the player's position and
    // direction and one candidate vehicle's:
    //   TrafficEntityModule::GenerateNearbyParkedTrafficOutput  0x8271FB90 (each parked traffic car)
    //   RaceCarEntityModule::ProcessPowerParking                0x822CE0C0 (each other race car)
    // Returns whether the vehicle is within the nearby radius; the caller counts the trues. A vehicle
    // that is the new closest also refreshes the angle and perpendicular-distance measurements.
    inline bool CheckVehicleForPowerPark(Vector3 lPlayerPos, Vector3 lPlayerDir,
                                         Vector3 lVehiclePos, Vector3 lVehicleDir,
                                         f32& lfClosestDistanceSq, f32& lfSecondClosestDistanceSq,
                                         f32& lfClosestAngleDiff, f32& lfClosestPerpendicularDist)
    {
        // 0x822B1FD4 vsubfp128 v0, v121(vehicle), v120(player); 0x822B1FE8..0x822B1FFC the ground-plane
        // square: vspltw lanes 2 and 0, vmulfp128 z*z (0x822B1FF8, rounded: ROUNDING_RULE 4), then vmaddfp
        // x*x + (z*z) (0x822B1FFC, raw D,A,B,C = v0, v0, v13, v0: ONE rounding, ROUNDING_RULE 3). Height
        // (lane 1) takes no part.
        const Vector3 lPlayerToVehicle = lVehiclePos - lPlayerPos;
        const f32 lfDistanceSq = std::fmaf(lPlayerToVehicle.x, lPlayerToVehicle.x,
                                           lPlayerToVehicle.z * lPlayerToVehicle.z);

        // 0x822B2010 fcmpu / bgt -> li r3, 0 (0x822B22B0). Not taken on a NaN distance.
        if (lfDistanceSq > KF_POWER_PARK_NEARBY_RADIUS * KF_POWER_PARK_NEARBY_RADIUS)
            return false;

        // 0x822B2020 and 0x822B202C are bge (bc 4, lt): taken on NaN. So a NaN distance counts as
        // nearby (returns true) and ranks nowhere; an equal distance is not closer.
        if (!(lfDistanceSq < lfSecondClosestDistanceSq))
            return true;                                                // 0x822B22A8

        if (!(lfDistanceSq < lfClosestDistanceSq))
        {
            lfSecondClosestDistanceSq = lfDistanceSq;                   // 0x822B22A4 stfs f13, 0(r4)
            return true;
        }

        lfSecondClosestDistanceSq = lfClosestDistanceSq;                // 0x822B2064 stfs f12, 0(r4)
        lfClosestDistanceSq       = lfDistanceSq;                       // 0x822B2084 stfs f13, 0(r3)

        {
            // Heading angles in the ground plane: ATan2(dir.z, dir.x) (vspltw128 lanes 2 and 0 of v2 /
            // v119), spilled to var_F0 / var_100 and subtracted at 0x822B2198.
            const f32 lfPlayerAngle  = PowerParkingDetail::ATan2(lPlayerDir.z, lPlayerDir.x);
            const f32 lfTrafficAngle = PowerParkingDetail::ATan2(lVehicleDir.z, lVehicleDir.x);

            lfClosestAngleDiff = std::fabs(lfPlayerAngle - lfTrafficAngle);   // 0x822B21A8 fabs ; 0x822B21AC stfs

            // :205 (li r5, 0xCD). blt 0.0 fires, ble TWO_PI skips -- so a NaN does NOT fire; the
            // negated compares keep that polarity (the plain `>= && <=` spelling would fire on NaN).
            CGS_ASSERT(!(lfClosestAngleDiff < 0.0f) && !(lfClosestAngleDiff > PowerParkingDetail::KF_TWO_PI),
                       "lfClosestAngleDiff >= 0.0f && lfClosestAngleDiff <= RwMathFPU::TWO_PI");

            // 0x822B21EC..0x822B2200: fsel(a - (2pi - a), 2pi - a, a) -- RwMathFPU::Min.
            lfClosestAngleDiff = rw::math::fpu::Min(lfClosestAngleDiff,
                                                    PowerParkingDetail::KF_TWO_PI - lfClosestAngleDiff);

            // :209 (li r5, 0xD1), same polarity.
            CGS_ASSERT(!(lfClosestAngleDiff < 0.0f) && !(lfClosestAngleDiff > PowerParkingDetail::KF_PI),
                       "lfClosestAngleDiff >= 0.0f && lfClosestAngleDiff <= RwMathFPU::PI");

            // 0x822B2238..0x822B224C: fsel(a - (pi - a), pi - a, a). An antiparallel car counts as aligned.
            lfClosestAngleDiff = rw::math::fpu::Min(lfClosestAngleDiff,
                                                    PowerParkingDetail::KF_PI - lfClosestAngleDiff);

            // :213 (li r5, 0xD5), same polarity.
            CGS_ASSERT(!(lfClosestAngleDiff < 0.0f) && !(lfClosestAngleDiff > PowerParkingDetail::KF_HALF_PI),
                       "lfClosestAngleDiff >= 0.0f && lfClosestAngleDiff <= RwMathFPU::HALF_PI");
        }

        // 0x822B2280..0x822B2298: v1 = player, v2 = vehicle, v3 = vehicle + its direction (vaddfp128).
        lfClosestPerpendicularDist = BrnMath::GetPointToInfiniteLineDistance(lPlayerPos, lVehiclePos,
                                                                             lVehiclePos + lVehicleDir);
        return true;                                                    // 0x822B229C li r3, 1
    }
}
