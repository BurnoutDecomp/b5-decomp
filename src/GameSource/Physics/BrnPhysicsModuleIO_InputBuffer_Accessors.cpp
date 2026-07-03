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
