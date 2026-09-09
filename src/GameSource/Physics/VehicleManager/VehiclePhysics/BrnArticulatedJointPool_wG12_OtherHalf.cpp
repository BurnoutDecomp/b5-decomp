// ============================================================================
// GameSource/Physics/VehicleManager/VehiclePhysics/BrnArticulatedJointPool_wG12_OtherHalf.cpp
//
// ArticulatedJointPool::GetIndexOfOtherHalf, plus the three packed-id readers the console inlines
// into it (declared in BrnArticulatedJoint.h, bodied here).
//
// The id ArticulatedJointId::Set packs: bits[63:32] the CAB's physics EntityId, bits[31:16] the
// TRAILER's 14-bit entity index, bits[15:0] the joint's own pool index. Asking a CAB for its
// other half returns the trailer index straight out of the packed field; asking a TRAILER returns
// the cab index by unpacking the stored EntityId. That asymmetry is the console's, not a
// transcription slip. All four asserts are non-gating tripwires, and the CAB path really does
// fetch the joint twice.
// ============================================================================

#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnArticulatedJointPool.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnPhysics
{
namespace Vehicle
{
    // ---- the inlined packed-id readers (declared in BrnArticulatedJoint.h) -------------------

    ArticulatedJointId ArticulatedJoint::GetJointId() const
    {
        return mJointId;
    }

    u16 ArticulatedJointId::GetJointPoolIndex() const
    {
        return static_cast<u16>(mu64RawId & 0xFFFFull);
    }

    u16 ArticulatedJointId::GetTrailerVehicleIndex() const
    {
        return static_cast<u16>((mu64RawId >> 16) & 0xFFFFull);
    }

    // FLAG: leArticulatedType is the caller's PhysicalTrafficVehicle::EArticulatedVehicleType
    // taken as s32 -- that enum lives in BrnPhysicalTrafficManager.h, which includes this pool's
    // header, so naming it here would close an include cycle. Live values: CAB(1), TRAILER(2).
    s32 ArticulatedJointPool::GetIndexOfOtherHalf(s32 liJointIndex, s32 liArticulatedType)
    {
        CGS_ASSERT(IsJointInUse(liJointIndex), "IsJointInUse( liJointIndex )");            // :359
        CGS_ASSERT(liArticulatedType > 0 && liArticulatedType < 3,
                   "leArticulatedType > PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_NONE && leArticulatedType < PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_COUNT");  // :361

        if (liArticulatedType == 1)   // E_ARTICULATE_VEHICLE_CAB
        {
            CGS_ASSERT(static_cast<s32>(GetJoint(liJointIndex)->GetJointId().GetJointPoolIndex())
                           == liJointIndex,
                       "GetJointId( liJointIndex ).GetJointPoolIndex() == liJointIndex");   // :365
            return static_cast<s32>(GetJoint(liJointIndex)->GetJointId().GetTrailerVehicleIndex());
        }

        CGS_ASSERT(liArticulatedType == 2,
                   "leArticulatedType == PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_TRAILER"); // :371

        const EntityId lCabEntityId = GetJoint(liJointIndex)->GetJointId().GetCabEntityId();
        return static_cast<s32>((lCabEntityId.muValue >> 10) & 0x3FFFu);
    }
}
}
