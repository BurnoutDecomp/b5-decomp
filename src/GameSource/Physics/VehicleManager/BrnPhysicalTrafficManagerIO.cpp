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

    // @0x825C26F0  ArticulatedJointCreateBuffer::GetCreateJointEvent (DWARF name/signature,
    //   BrnPhysicalTrafficManagerIO.h:90; returns const InAddJoint&, ABI-returned as the element
    //   pointer -- Hex-Rays shows `192*liJointIndex + this + 16`, i.e. &maCreatedJointEvents[idx]).
    //   Two X360 bounds checks precede the payload read: a bare "liJointIndex >= 0 &&
    //   liJointIndex < kiMaxArticulatedTrafficVehicles" tripwire (BrnPhysicalTrafficManagerIO.h:210)
    //   and the inlined CgsBitArray formatted-message bounds assert (CgsBitArray.h:203) -- both are
    //   the SAME logical [0,10) bound, collapsed to one CGS_ASSERT here (matching the
    //   FlagJointToBeCreated/FlagJointToBeRemoved convention above). Then asserts the slot is
    //   actually flagged for creation ("mCreatedJointBitArray.IsBitSet( liJointIndex )",
    //   BrnPhysicalTrafficManagerIO.h:211) before returning the stashed request.
    const ArticulatedJointCreateBuffer::InAddJoint&
    ArticulatedJointCreateBuffer::GetCreateJointEvent(s32 liJointIndex) const
    {
        CGS_ASSERT(liJointIndex >= 0 && liJointIndex < KI_MAX_ARTICULATED_TRAFFIC_VEHICLES,
                   "liJointIndex >= 0 && liJointIndex < kiMaxArticulatedTrafficVehicles");
        CGS_ASSERT(mCreatedJointBitArray.IsBitSet(static_cast<u32>(liJointIndex)),
                   "mCreatedJointBitArray.IsBitSet( liJointIndex )");
        return maCreatedJointEvents[liJointIndex];
    }

    // @0x825C2860  ArticulatedJointCreateBuffer::GetRemoveJointEvent (DWARF name/signature,
    //   BrnPhysicalTrafficManagerIO.h:95; returns const InRemoveJoint&). Hex-Rays shows
    //   `8*(liJointIndex + 242) + this` == this + 0x790 + 8*liJointIndex, i.e.
    //   &maRemovedJointEvents[idx]. Same bounds-check collapse as GetCreateJointEvent above
    //   (BrnPhysicalTrafficManagerIO.h:221 tripwire + CgsBitArray.h:203 inlined bounds assert ->
    //   one CGS_ASSERT), then asserts the slot is flagged for removal
    //   ("mRemovedJointBitArray.IsBitSet( liJointIndex )", BrnPhysicalTrafficManagerIO.h:222).
    const ArticulatedJointCreateBuffer::InRemoveJoint&
    ArticulatedJointCreateBuffer::GetRemoveJointEvent(s32 liJointIndex) const
    {
        CGS_ASSERT(liJointIndex >= 0 && liJointIndex < KI_MAX_ARTICULATED_TRAFFIC_VEHICLES,
                   "liJointIndex >= 0 && liJointIndex < kiMaxArticulatedTrafficVehicles");
        CGS_ASSERT(mRemovedJointBitArray.IsBitSet(static_cast<u32>(liJointIndex)),
                   "mRemovedJointBitArray.IsBitSet( liJointIndex )");
        return maRemovedJointEvents[liJointIndex];
    }
}
}
