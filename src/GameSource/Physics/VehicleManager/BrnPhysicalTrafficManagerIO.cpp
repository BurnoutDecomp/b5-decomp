#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManagerIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnPhysics::Vehicle::ArticulatedJointCreateBuffer -- the two frame-batch "flag a joint" methods.
// Reconstructed from BURNOUT_X360_ARTIST.XEX.

namespace BrnPhysics
{
namespace Vehicle
{
    // @0x825C23B0  ArticulatedJointCreateBuffer::FlagJointToBeCreated
    //   memcpy the 192-byte InAddJoint into maCreatedJointEvents[liJointIndex] (X360 dst =
    //   192*index + this + 0x10) and unconditionally set the create bit (X360 inlines the
    //   CgsBitArray bounds assert + the field*8 / (1<<(index&63)) or-into-word).
    void ArticulatedJointCreateBuffer::FlagJointToBeCreated(s32 liJointIndex,
                                                            const InAddJoint& lrAddJointEvent)
    {
        CGS_ASSERT(liJointIndex >= 0 && liJointIndex < KI_MAX_ARTICULATED_TRAFFIC_VEHICLES,
                   "liJointIndex >= 0 && liJointIndex < kiMaxArticulatedTrafficVehicles");

        maCreatedJointEvents[liJointIndex] = lrAddJointEvent;   // 192-byte copy
        mCreatedJointBitArray.SetBit(static_cast<u32>(liJointIndex));
    }

    // @0x825C24F8  ArticulatedJointCreateBuffer::FlagJointToBeRemoved
    //   A create+remove in the same frame cancels: skip the removal if this slot is already
    //   flagged for creation (X360 tests mCreatedJointBitArray first, `bne` skips). Otherwise store
    //   the 8-byte InRemoveJoint (single ld/stdx) and set the remove bit.
    void ArticulatedJointCreateBuffer::FlagJointToBeRemoved(s32 liJointIndex,
                                                            const InRemoveJoint& lrRemoveJointEvent)
    {
        CGS_ASSERT(liJointIndex >= 0 && liJointIndex < KI_MAX_ARTICULATED_TRAFFIC_VEHICLES,
                   "liJointIndex >= 0 && liJointIndex < kiMaxArticulatedTrafficVehicles");

        if (!mCreatedJointBitArray.IsBitSet(static_cast<u32>(liJointIndex)))
        {
            maRemovedJointEvents[liJointIndex] = lrRemoveJointEvent;   // single 8-byte store
            mRemovedJointBitArray.SetBit(static_cast<u32>(liJointIndex));
        }
    }
}
}
