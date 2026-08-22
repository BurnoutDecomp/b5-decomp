// =================================================================================================
// b5-decomp/src/GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager_WriteOutVehicleStats.cpp
//
// PhysicalTrafficManager::WriteOutVehicleStats @0x825F0308 (481 insns) -- the TRAFFIC half of the
// per-frame physics publish. Its race-car half, VehicleManager::WriteOutVehicleStats @0x8263F460,
// is already real and mounted and tail-calls this one at 0x8263FA28
// (`addis r3,r18,1 ; addi r3,r3,-0x5120` == this + 44768 == &mPhysicalTrafficManager, r4 == the
// same VehicleOutputInterface). Its gate at BrnPhysicsConductorGates.cpp:374 RETIRES with this file.
//
// SIGNATURE from the asm prologue: r3 this, r4 the VehicleOutputInterface. The Hex-Rays `a3`/`a4`
// are stale register noise -- the only two registers the body reads are r3 (walked as v5) and the
// interface it hands to AddTrafficState.
//
// THE WALK. The whole function is one iteration over mUsedTrafficVehicles (X360 word base
// this+104552, capacity ku8TotalMaxNumPhysicalTraffic == 20), open-coded by the console as the
// usual `cntlzd`-over-64-bit-fields first/next-set-bit pair plus the CgsBitArray.h:203
// "invalid index : " bound assert. Per set bit:
//   v22 = *(this + 103604) + (i << 6)   == &mpaTrafficVehicles[i]   (stride 64)
//   v25 = *(this + 103360 + 4*i)        == maTrafficEntityIDs[i]
//   *(v22 + 28)                          == PhysicalTrafficVehicle::mpVehicleBody
//   -> four `lvx128` + `vcmpeqfp.` triples over body+0x10/+0x20/+0x30/+0x40 (mTransform's four
//      rows, lanes .x/.y/.z only) == a NaN check on the pose; on failure the console builds the
//      "Traffic has invalid transfrm, ID <id>, vehicle index <i>, physics ID <id>" message and
//      fires BrnPhysicalTrafficManager.cpp:2493. NON-GATING -- the publish happens either way.
//   -> VehicleOutputInterface::AddTrafficState(maTrafficEntityIDs[i], &mpaTrafficVehicles[i])
//
// TWO DELIBERATE SIMPLIFICATIONS, both named:
//  1. The console's assert message interpolates three values through CgsDev::StrStream, including
//     PhysicalTrafficManager::GetPhysicsEntityId(i) @0x825B4980, which exists ONLY to be printed.
//     CGS_ASSERT here takes a literal, so the interpolation (and that call) is dropped; the
//     predicate, its file and its line are unchanged.
//  2. The console emits the bit walk inline with a per-64-bit-field fast path. The BitArray<20u>
//     GetFirstNonZeroBit/GetNextNonZeroBit pair is that same walk by name, and is what the sibling
//     traffic walks in this manager already use (BrnVehicleManager_ReadUpdatedBodies.cpp:169).
// =================================================================================================

#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstdlib>   // getenv  ([T3-state] diagnostics only)

namespace BrnPhysics
{
namespace Vehicle
{
namespace
{
    // `vcmpeqfp. vX, vX, vX` + the CR6 all-lanes bit, applied to lanes .x/.y/.z of each row: a
    // NaN compares unequal to itself. The console tests three lanes per row and ignores .w.
    inline bool IsRowFinite(const Vector3& lvRow)
    {
        return (lvRow.x == lvRow.x) && (lvRow.y == lvRow.y) && (lvRow.z == lvRow.z);
    }

    inline bool IsTransformFinite(const Matrix44Affine& lrTransform)
    {
        return IsRowFinite(lrTransform.xAxis) && IsRowFinite(lrTransform.yAxis)
            && IsRowFinite(lrTransform.zAxis) && IsRowFinite(lrTransform.wAxis);
    }
}

// -------------------------------------------------------------------------------------------------
// @0x825F0308  PhysicalTrafficManager::WriteOutVehicleStats
// -------------------------------------------------------------------------------------------------
void PhysicalTrafficManager::WriteOutVehicleStats(Vehicle::VehicleOutputInterface* lpOutputInterface)
{
    // DIVERGENCE (named): the console has no null guard -- VehicleManager::WriteOutVehicleStats
    // always forwards the buffer it was handed.
    CGS_ASSERT(lpOutputInterface != 0, "lpOutputInterface != NULL");
    if (lpOutputInterface == 0)
    {
        return;
    }

    for (s32 liVehicle = mUsedTrafficVehicles.GetFirstNonZeroBit();
         liVehicle >= 0;
         liVehicle = mUsedTrafficVehicles.GetNextNonZeroBit(liVehicle))
    {
        CGS_ASSERT(static_cast<u32>(liVehicle) < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC,
                   "liVehicle < ku8TotalMaxNumPhysicalTraffic");                       // :730

        const PhysicalTrafficVehicle* lpVehicle = &mpaTrafficVehicles[liVehicle];
        const EntityId                lEntityId = maTrafficEntityIDs[liVehicle];

        CGS_ASSERT(lpVehicle->mpVehicleBody != 0, "lpVehicle->mpVehicleBody != NULL");

        if (lpVehicle->mpVehicleBody != 0)
        {
            // NON-GATING tripwire, BrnPhysicalTrafficManager.cpp:2493. See simplification 1.
            CGS_ASSERT(IsTransformFinite(lpVehicle->mpVehicleBody->GetTransform()),
                       "Traffic has invalid transfrm");                                // :2493
        }

        lpOutputInterface->AddTrafficState(lEntityId, lpVehicle);

        // [T3-state] the first PhysicalTrafficState this build ever publishes. DELETE-WHEN-STABLE.
        {
            static const bool skbTrafficDiag = (std::getenv("BRN_TRAFFIC_DIAG") != 0);
            static bool       sbLoggedFirstState = false;
            if (skbTrafficDiag && !sbLoggedFirstState && CgsDev::Log::gpDebugPrint != 0
                && lpVehicle->mpVehicleBody != 0)
            {
                sbLoggedFirstState = true;
                const Vector3 lvPos = lpVehicle->mpVehicleBody->GetPosition();
                *CgsDev::Log::gpDebugPrint
                    << "[T3-state] FIRST PhysicalTrafficState published: slot " << liVehicle
                    << " entity index " << static_cast<s32>((lEntityId.muValue >> 10) & 0x3FFFu)
                    << " at (" << lvPos.x << ", " << lvPos.y << ", " << lvPos.z << ")\n";
            }
        }
    }
}

}
}
