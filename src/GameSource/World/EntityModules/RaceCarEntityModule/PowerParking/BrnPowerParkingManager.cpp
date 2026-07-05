#include "GameSource/World/EntityModules/RaceCarEntityModule/PowerParking/BrnPowerParkingManager.h"

#include "GameSource/Math/BrnMathUtils.h"           // BrnMath::GetPointToInfiniteLineDistance (declare-only, additive grow)
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"           // Vector3 operator+ (lVehiclePos + lVehicleDir)

#include <cmath>       // std::atan2 / std::fabs
#include <algorithm>   // std::min

// ============================================================================
// GameSource/World/EntityModules/RaceCarEntityModule/PowerParking/BrnPowerParkingManager.cpp
//
// BrnWorld::CheckVehicleForPowerPark  (X360 @ 0x822B1FA0)  -- free function, returns bool.
// Reconstructed from BURNOUT_X360_ARTIST.XEX at SEMANTIC PARITY (VMX/SIMD dropped to portable
// C++), mirroring the BrnMathUtils.cpp house style. The Hex-Rays 23-int signature is register-
// ABI noise: the four Vector3 params arrive in v1..v4 (playerPos=v120, playerDir=v2,
// vehiclePos=v121, vehicleDir=v119) and the four f32& refs in r3..r6 (r3=closest, r4=second,
// r28=angleDiff, r27=perpendicular).
//
// One-vehicle Power-Parking candidacy test. Ranks the target vehicle by squared XZ distance
// into a running closest / second-closest pair; only when it becomes the NEW closest does it
// record the heading-angle difference (folded into [0, HALF_PI]) and the perpendicular distance
// from the player to the vehicle's heading line.
//
// FLAG (VMX->portable): the XMVectorATan reciprocal-estimate + Newton refinement + quadrant
//   selects collapse to std::atan2(dir.x, dir.z); the squared-distance splat/madd collapses to
//   scalar dx*dx + dz*dz; the fsel folds collapse to std::min. Numerically exact, not placeholders.
// FLAG (angle constants): TWO_PI/PI/HALF_PI are the X360 rodata floats (6.2831855f / 3.1415927f /
//   1.5707964f), == RwMathFPU::TWO_PI / ::PI / ::HALF_PI, named to match the assert text.
// FLAG (GetPointToInfiniteLineDistance): a BrnMath:: free function with no committed body; homed
//   declare-only in BrnMathUtils.h (additive grow, mirroring BrnMath::IsPointInsideBox). Signature
//   (point, lineStart, lineEnd) inferred from the callsite register setup; returns f32 distance.
// ============================================================================

namespace BrnWorld
{
    bool CheckVehicleForPowerPark( Vector3 lPlayerPos,
                                   Vector3 lPlayerDir,
                                   Vector3 lVehiclePos,
                                   Vector3 lVehicleDir,
                                   f32& lfClosestDistanceSq,
                                   f32& lfSecondClosestDistanceSq,
                                   f32& lfClosestAngleDiff,
                                   f32& lfClosestPerpendicularDist )
    {
        // X360 rodata angle constants (== RwMathFPU::TWO_PI / ::PI / ::HALF_PI).
        const f32 KF_TWO_PI  = 6.2831855f;
        const f32 KF_PI      = 3.1415927f;
        const f32 KF_HALF_PI = 1.5707964f;

        // Squared distance in the XZ ground plane (flt_82018E3C == 225.0 == 15m^2).
        const f32 KF_MAX_POWER_PARK_DISTANCE_SQ = 225.0f;
        const f32 lfDeltaX = lVehiclePos.x - lPlayerPos.x;
        const f32 lfDeltaZ = lVehiclePos.z - lPlayerPos.z;
        const f32 lfDistanceSq = lfDeltaX * lfDeltaX + lfDeltaZ * lfDeltaZ;

        if (lfDistanceSq > KF_MAX_POWER_PARK_DISTANCE_SQ)
            return false;

        // Not closer than the current runner-up: a candidate, but nothing to record.
        if (lfDistanceSq >= lfSecondClosestDistanceSq)
            return true;

        // Between the closest and the runner-up: becomes the new runner-up.
        if (lfDistanceSq >= lfClosestDistanceSq)
        {
            lfSecondClosestDistanceSq = lfDistanceSq;
            return true;
        }

        // New closest vehicle: demote the old closest to runner-up, then record its geometry.
        lfSecondClosestDistanceSq = lfClosestDistanceSq;
        lfClosestDistanceSq       = lfDistanceSq;

        // Heading of each direction vector in the XZ plane (the inlined XMVectorATan == atan2).
        const f32 lfPlayerHeading  = std::atan2(lPlayerDir.x,  lPlayerDir.z);
        const f32 lfVehicleHeading = std::atan2(lVehicleDir.x, lVehicleDir.z);

        // Absolute heading difference, then fold into [0, HALF_PI].
        f32 lfAngleDiff = std::fabs(lfPlayerHeading - lfVehicleHeading);

        CGS_ASSERT(lfAngleDiff >= 0.0f && lfAngleDiff <= KF_TWO_PI,
                   "lfClosestAngleDiff >= 0.0f && lfClosestAngleDiff <= RwMathFPU::TWO_PI");
        lfAngleDiff = std::min(lfAngleDiff, KF_TWO_PI - lfAngleDiff);   // fold [0,TWO_PI] -> [0,PI]

        CGS_ASSERT(lfAngleDiff >= 0.0f && lfAngleDiff <= KF_PI,
                   "lfClosestAngleDiff >= 0.0f && lfClosestAngleDiff <= RwMathFPU::PI");
        lfAngleDiff = std::min(lfAngleDiff, KF_PI - lfAngleDiff);       // fold [0,PI] -> [0,HALF_PI]

        CGS_ASSERT(lfAngleDiff >= 0.0f && lfAngleDiff <= KF_HALF_PI,
                   "lfClosestAngleDiff >= 0.0f && lfClosestAngleDiff <= RwMathFPU::HALF_PI");

        lfClosestAngleDiff = lfAngleDiff;

        // Perpendicular distance from the player to the vehicle's heading line.
        lfClosestPerpendicularDist =
            BrnMath::GetPointToInfiniteLineDistance(lPlayerPos, lVehiclePos, lVehiclePos + lVehicleDir);

        return true;
    }
}
