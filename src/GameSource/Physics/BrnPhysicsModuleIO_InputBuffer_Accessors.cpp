#include "GameSource/Physics/BrnPhysicsModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnPhysics::PhysicsModuleIO::InputBuffer accessors owned by the bare PhysicsModuleIO group,
// reconstructed from BURNOUT_X360_ARTIST.XEX. This TU bodies the four deep-member accessors:
//
//   GetVehicleDriverInterface() const       @ 0x8259F948 read  (bit 4) -> +142544 (DWARF :278)
//   GetVehicleEffectsInputInterface() const @ 0x8259F9F0 read  (bit 4) -> +147840 (DWARF :281)
//   GetGameActionQueue() const              @ 0x8259FE88 read  (bit 4) -> +338496 (DWARF :304)
//   GetGameActionQueue()                    @ 0x8279F3A0 write (bit 3) -> +338496 (DWARF :305)
//
// The const (read) handles test the read-lock bit; the non-const (write) handle tests the
// write-lock bit. Lock strings carry the trailing \n per X360 rodata. The remaining twelve
// InputBuffer accessors live in the sibling BrnPhysicsModuleIO_InputBuffer.cpp.

namespace BrnPhysics
{
namespace PhysicsModuleIO
{
    // X360 0x8259F948 (DWARF :278): read-lock; return &mVehicleDriverInterface (this+142544).
    const InputBuffer::VehicleDriverInputInterfaceStorage* InputBuffer::GetVehicleDriverInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mVehicleDriverInterface;
    }

    // X360 0x8259F9F0 (DWARF :281): read-lock; return &mVehicleEffectsInputInterface (this+147840).
    const InputBuffer::VehicleEffectsInputInterfaceStorage* InputBuffer::GetVehicleEffectsInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mVehicleEffectsInputInterface;
    }

    // X360 0x8259FE88 (DWARF :304): read-lock; return &mGameActionQueue (this+338496).
    const InputBuffer::GameActionQueueStorage* InputBuffer::GetGameActionQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mGameActionQueue;
    }

    // X360 0x8279F3A0 (DWARF :305): write-lock; return &mGameActionQueue (this+338496).
    InputBuffer::GameActionQueueStorage* InputBuffer::GetGameActionQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mGameActionQueue;
    }
}
}

// ⭐ ADDED 2026-08-09 (conductor wave): the two queue getters PhysicsModule::Update
// @0x825B0640 consumes -- the potential-contact queue feeds
// PotentialContactInterface::SetConstQueue, the overlap-pairs queue feeds
// Start/EndVehicleContactGeneration. Same lock-tripwire pattern as every getter above.
namespace BrnPhysics
{
namespace PhysicsModuleIO
{
    // X360 0x8259FB40 (DWARF :289): read-lock; return &mPotentialContactQueue (console
    // this+160208 -- `addis r3,r28,2 ; addi r3,r3,0x71D0`).
    const CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::PotentialContact, 2048>*
    InputBuffer::GetPotentialContactQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mPotentialContactQueue;
    }

    // X360 0x8259FBE8 (DWARF :292): read-lock; return &mOverlapPairsQueue (console
    // this+324064 -- `addis r3,r28,5 ; addi r3,r3,-0xE20`).
    const CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::OutOverlapPair, 128>*
    InputBuffer::GetOverlapPairsQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mOverlapPairsQueue;
    }
}
}
