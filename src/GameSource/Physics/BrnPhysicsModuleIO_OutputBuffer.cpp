#include "GameSource/Physics/BrnPhysicsModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// BrnPhysics::PhysicsModuleIO::OutputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the 7 X360-emitted OutputBuffer accessors:
//
//   GetVehicleOutputRequestInterface() @ 0x8259FF30 -> +16     (this+16),     write (bit 3)
//   GetVehicleOutputInterface() const  @ 0x8279F598 -> +44128  (this+44128),  read  (bit 4)
//   GetVehicleOutputInterface()        @ 0x825A0080 -> +44128  (this+44128),  write (bit 3)
//   GetPropManagerOutputInterface() const @ 0x8279F640 -> +71792 (this+71792), read (bit 4)
//   GetPropManagerOutputInterface()    @ 0x825C0DC8 -> +71792  (this+71792),  write (bit 3)
//   GetDeformationOutputInterface()    @ 0x825A0128 -> +148656 (this+148656), write (bit 3)
//   GetContactSpyInterface()           @ 0x825A0320 -> +998192 (this+998192), write (bit 3)
//
// The const (read) handle tests the read-lock bit (((*a1 >> 4) & 1)); the non-const (write)
// handles test the write-lock bit (((*a1 >> 3) & 1)) -- matching CgsModule::IOBuffer's
// IsBufferLockedForReading()/IsBufferLockedForWriting(). Each returns the member's address.

namespace BrnPhysics
{
namespace PhysicsModuleIO
{
    void OutputBuffer::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer, mVehicleOutputRequestInterface) == 16,     "mVehicleOutputRequestInterface @16");
        // ⭐ 2026-08-09 (conductor wave): the :378 manager seat, previously folded into
        // padding, is a real member now -- 16 + 41936 == 41952 (the request interface is
        // byte-identical on both targets, so this pin stays ABSOLUTE).
        static_assert(offsetof(OutputBuffer, mVehicleManagerOutputInterface) == 41952,  "mVehicleManagerOutputInterface @41952");
        // ⚠ From here down the buffer GROWS on the host (12- vs 16-byte console queue
        // headers inside the two vehicle interfaces -- see the header note), so the gates
        // are the console DELTAS between seats, not absolutes. Console deltas: 44128-41952
        // is sizeof(VMOI)'s console span (adjacent members, gated trivially by adjacency);
        // 148656-71792 == 76864; 159648-148656 == 11000... NO -- the deltas below are the
        // PAD-under-written spans, which this file's own pad arrays hold by construction:
        static_assert(offsetof(OutputBuffer, mDeformationOutputInterface)
                    - offsetof(OutputBuffer, mPropManagerOutputInterface)    == 148656 - 71792,
                      "prop -> deformation console delta");
        static_assert(offsetof(OutputBuffer, mDeformationOutputInterfaceForEntityModules)
                    - offsetof(OutputBuffer, mDeformationOutputInterface)    == 159648 - 148656,
                      "deformation -> entity-modules console delta");
        static_assert(offsetof(OutputBuffer, mSceneInputInterface)
                    - offsetof(OutputBuffer, mDeformationOutputInterfaceForEntityModules) == 179424 - 159648,
                      "entity-modules -> scene console delta");
        static_assert(offsetof(OutputBuffer, mContactSpyInterface)
                    - offsetof(OutputBuffer, mSceneInputInterface)           >= 998192 - 179424,
                      "scene -> contact-spy console delta (>=: the spy seat 8-aligns)");
    }

    // X360 0x8279F4F0 (read sibling block): read-lock; return this + 41952.
    const Vehicle::VehicleManagerOutputInterface* OutputBuffer::GetVehicleManagerOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mVehicleManagerOutputInterface;
    }

    // X360 0x8259FFD8: write-lock; return this + 41952. The accessor
    // PhysicsModule::Update @0x825B0640 calls five times per frame (DoCrashPrediction /
    // UpdateDrivers / ProcessResetEvents / ProcessContactSpies / WriteOut seats).
    Vehicle::VehicleManagerOutputInterface* OutputBuffer::GetVehicleManagerOutputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mVehicleManagerOutputInterface;
    }

    // X360 0x825A01D0 (DWARF :364): write-lock; return this + 159648 (`addis 2; addi 28576`).
    // Consumed by PhysicsModule::Update's OutputData leg.
    OutputBuffer::DeformationOutputInterfaceForEntityModulesStorage*
    OutputBuffer::GetDeformationOutputInterfaceForEntityModules()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mDeformationOutputInterfaceForEntityModules;
    }

    // X360 0x8259FF30: write-lock; return this + 16.
    OutputBuffer::VehicleOutputRequestInterfaceStorage* OutputBuffer::GetVehicleOutputRequestInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mVehicleOutputRequestInterface;
    }

    // X360 0x8279F598: read-lock; return this + 44128.
    const OutputBuffer::VehicleOutputInterfaceStorage* OutputBuffer::GetVehicleOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mVehicleOutputInterface;
    }

    // X360 0x825A0080: write-lock; return this + 44128.
    OutputBuffer::VehicleOutputInterfaceStorage* OutputBuffer::GetVehicleOutputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mVehicleOutputInterface;
    }

    // X360 0x8279F640: read-lock; return this + 71792.
    const OutputBuffer::PropOutputInterfaceStorage* OutputBuffer::GetPropManagerOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mPropManagerOutputInterface;
    }

    // X360 0x825C0DC8: write-lock; return this + 71792.
    OutputBuffer::PropOutputInterfaceStorage* OutputBuffer::GetPropManagerOutputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mPropManagerOutputInterface;
    }

    // X360 0x825A0128: write-lock; return this + 148656.
    OutputBuffer::DeformationOutputInterfaceStorage* OutputBuffer::GetDeformationOutputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mDeformationOutputInterface;
    }

    // X360 0x825A0320: write-lock; return this + 998192.
    // (Return RETYPED 2026-08-06 with the member promotion to the real ContactSpyInterface.)
    ContactSpy::ContactSpyInterface* OutputBuffer::GetContactSpyInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mContactSpyInterface;
    }

    // ---- wave5 ADDITIVE accessors (const twins + non-const scene-input) --------------
    // Assert strings match this file's existing convention (no trailing \n); the X360 rodata
    // carries \n but the committed bodies above omit it -- kept consistent within this file.

    // X360 0x8279F448 (DWARF :298): read-lock; return this + 16.
    const OutputBuffer::VehicleOutputRequestInterfaceStorage* OutputBuffer::GetVehicleOutputRequestInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mVehicleOutputRequestInterface;
    }

    // X360 0x8279F6E8 (DWARF :322): read-lock; return this + 148656.
    const OutputBuffer::DeformationOutputInterfaceStorage* OutputBuffer::GetDeformationOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mDeformationOutputInterface;
    }

    // X360 0x8279F838 (DWARF :366): read-lock; return this + 179424.
    const OutputBuffer::SceneInputInterfaceStorage* OutputBuffer::GetSceneInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mSceneInputInterface;
    }

    // X360 0x825A0278 (DWARF :337): write-lock; return this + 179424.
    OutputBuffer::SceneInputInterfaceStorage* OutputBuffer::GetSceneInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mSceneInputInterface;
    }
}
}
