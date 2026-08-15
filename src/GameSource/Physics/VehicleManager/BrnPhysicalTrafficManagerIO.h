#pragma once

// ============================================================================
// BrnPhysics::Vehicle::ArticulatedJointCreateBuffer
//   GameSource/Physics/VehicleManager/BrnPhysicalTrafficManagerIO.h
//   (DWARF home BrnPhysicalTrafficManagerIO.h:108-111 / :42-47)
//
// The IO buffer pushed onto the physics input stack by
// PhysicalTrafficManager::AllocateInternalBuffers (CreateIOBuffer<ArticulatedJointCreateBuffer>).
// It batches the articulation-joint create/remove requests for the frame: an InAddJoint per slot
// flagged for creation, an InRemoveJoint per slot flagged for removal, and two BitArray<10> masks
// tracking which slots are flagged. Reconstructed from BURNOUT_X360_ARTIST.XEX + the DecFIGS DWARF.
//
// LAYOUT (X360-attested offsets, member names/typedefs verbatim from the DWARF):
//   base   CgsModule::IOBuffer (1-byte status FlagSet, padded to +16 by alignas(16))
//   @0x0010 InAddJoint    maCreatedJointEvents[10]   (stride 192, 0xC0)
//   @0x0790 InRemoveJoint maRemovedJointEvents[10]   (stride 8)
//   @0x07E0 BitArray<10>  mCreatedJointBitArray      (u64[1])
//   @0x07E8 BitArray<10>  mRemovedJointBitArray      (u64[1])
//   sizeof == 0x7F0
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                     // CgsModule::IOBuffer
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                 // CgsContainers::BitArray<N>
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"  // CgsPhysics::PhysicsSimulationIO::InAddJoint / InRemoveJoint

namespace BrnPhysics
{
namespace Vehicle
{
    // The articulation-joint pool capacity asserted by FlagJointToBe* ("liJointIndex >= 0 &&
    // liJointIndex < kiMaxArticulatedTrafficVehicles"). DWARF-homes as kiMaxArticulatedTrafficVehicles
    // = 10 (BrnVehicleConstants.h); kept buffer-local here (buffer-only use).
    const s32 KI_MAX_ARTICULATED_TRAFFIC_VEHICLES = 10;

    class ArticulatedJointCreateBuffer : public CgsModule::IOBuffer
    {
    public:
        typedef CgsPhysics::PhysicsSimulationIO::InAddJoint    InAddJoint;    // :42
        typedef CgsPhysics::PhysicsSimulationIO::InRemoveJoint InRemoveJoint; // :43

        // Construct -- X360-attested by the CreateIOBuffer<ArticulatedJointCreateBuffer>
        // instantiation @0x82614AE8, which inlines it whole after `Alloc(this, 2032, name)`:
        //     li r10,1 ; stb r10,0(r11)      -- IOBuffer::Construct
        //     std r28,0x7E0(r11)             -- mCreatedJointBitArray = 0   (BitArray::Construct)
        //     std r28,0x7E8(r11)             -- mRemovedJointBitArray = 0   (BitArray::Construct)
        //     std r28,0x7E0(r11)             -- ...and again               (BitArray::Clear)
        //     std r28,0x7E8(r11)             -- ...and again               (BitArray::Clear)
        // (r28 == 0; the doubled stores are the console's own Construct-then-Clear pair, the
        // same idiom RequestInterface<N>::Construct uses. Offsets 0x7E0/0x7E8 are exactly the
        // two BitArray<10> members below.) Both request arrays are left uninitialised, as on
        // the console -- only the two masks decide which slots are live.
        // NOTE: nothing cleared these masks before, because the old PC CreateIOBuffer<T>
        // value-initialised the whole 2 KB buffer; without this body they would be garbage.
        void Construct()
        {
            CgsModule::IOBuffer::Construct();
            mCreatedJointBitArray.UnSetAll();
            mRemovedJointBitArray.UnSetAll();
        }

        // @0x825C23B0: stash an add-joint request in slot liJointIndex and flag the slot for
        // creation (unconditional SetBit).
        void FlagJointToBeCreated(s32 liJointIndex, const InAddJoint& lrAddJointEvent);

        // @0x825C24F8: stash a remove-joint request in slot liJointIndex and flag it for removal --
        // UNLESS the slot is already flagged for creation this frame (a create+remove cancels).
        void FlagJointToBeRemoved(s32 liJointIndex, const InRemoveJoint& lrRemoveJointEvent);

        // @0x825C26F0 (DWARF GetCreateJointEvent, BrnPhysicalTrafficManagerIO.h:90): asserted
        // accessor for the create-joint request stashed in slot liJointIndex by FlagJointToBeCreated.
        const InAddJoint& GetCreateJointEvent(s32 liJointIndex) const;

        // @0x825C2860 (DWARF GetRemoveJointEvent, BrnPhysicalTrafficManagerIO.h:95): asserted
        // accessor for the remove-joint request stashed in slot liJointIndex by FlagJointToBeRemoved.
        const InRemoveJoint& GetRemoveJointEvent(s32 liJointIndex) const;

        typedef CgsContainers::BitArray<10u> CreatedJointBitArray;  // DWARF :42
        typedef CgsContainers::BitArray<10u> RemovedJointBitArray;  // DWARF :43

        // DWARF BrnPhysicalTrafficManagerIO.h:99 / :103. The console emits no out-of-line symbol for
        // either: ArticulatedJointPool::SendCreateRemoveJointEvents @0x826013C0 inlines both to a
        // bare `buffer + 2016` / `buffer + 2024` and then walks the returned mask in place, which is
        // exactly where these two members sit. Header-inline here, matching that -- and it is what
        // keeps the pool's walk BY NAME instead of reaching through a friend or an offset.
        const CreatedJointBitArray* GetCreatedJointBitArray() const { return &mCreatedJointBitArray; }
        const RemovedJointBitArray* GetRemovedJointBitArray() const { return &mRemovedJointBitArray; }

    private:
        InAddJoint                             maCreatedJointEvents[KI_MAX_ARTICULATED_TRAFFIC_VEHICLES]; // @0x0010 (stride 192)
        InRemoveJoint                          maRemovedJointEvents[KI_MAX_ARTICULATED_TRAFFIC_VEHICLES]; // @0x0790 (stride 8)
        CreatedJointBitArray                   mCreatedJointBitArray;                                     // @0x07E0
        RemovedJointBitArray                   mRemovedJointBitArray;                                     // @0x07E8
    };
}
}
