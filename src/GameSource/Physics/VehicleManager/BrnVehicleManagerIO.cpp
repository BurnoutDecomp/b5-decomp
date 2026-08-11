#include "GameSource/Physics/VehicleManager/BrnVehicleManagerIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// BrnPhysics::Vehicle::VehicleManagerOutputBuffer, reconstructed from
// BURNOUT_X360_ARTIST.XEX (accessors) + the DecFIGS DWARF / PS3 out-of-line pair
// (Construct/Destruct). See the header banner for the 2026-08-09 naming
// correction (the class/member/accessor names committed before this pass were
// invented; DWARF + PS3 mangles carry the real ones).
//
//   GetVehicleOutputRequestInterface() const @ 0x825A0FB0  read  (bit 4) -> +16 (:60)
//   GetVehicleOutputRequestInterface()       @ 0x825A1058  write (bit 3) -> +16 (:61)
//
// The const (read) handle tests the read-lock bit (((*a1 >> 4) & 1)); the
// non-const (write) handle tests the write-lock bit (((*a1 >> 3) & 1)) --
// matching CgsModule::IOBuffer's IsBufferLockedForReading()/-Writing(). Each
// asserts the lock (non-gating tripwire) and returns the member's address.

namespace BrnPhysics
{
namespace Vehicle
{
    void VehicleManagerOutputBuffer::_AssertLayout()
    {
        static_assert(offsetof(VehicleManagerOutputBuffer, mVehicleOutputRequestInterface) == 16,
                      "mVehicleOutputRequestInterface @16");
    }

    // DWARF :52 (PS3 out-of-line `VehicleManagerOutputBuffer::Construct`). The
    // X360 stack template CreateIOBuffer<VehicleManagerOutputBuffer> @0x8259DAF0
    // runs this after the alloc: raise the buffer status, construct the six
    // request queues through the interface's own Construct.
    void VehicleManagerOutputBuffer::Construct()
    {
        CgsModule::IOBuffer::Construct();
        mVehicleOutputRequestInterface.Construct();
    }

    // DWARF :56 (PS3 out-of-line sibling). The queues are POD-over-inline-storage;
    // teardown has no per-member work beyond the base.
    void VehicleManagerOutputBuffer::Destruct()
    {
        CgsModule::IOBuffer::Destruct();
    }

    // X360 0x825A0FB0: read-lock; return this + 16.
    const VehicleOutputRequestInterface*
    VehicleManagerOutputBuffer::GetVehicleOutputRequestInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mVehicleOutputRequestInterface;
    }

    // X360 0x825A1058: write-lock; return this + 16.
    VehicleOutputRequestInterface*
    VehicleManagerOutputBuffer::GetVehicleOutputRequestInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mVehicleOutputRequestInterface;
    }
}
}
