#include "GameSource/Physics/VehicleManager/BrnVehicleManagerIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// BrnPhysics::Vehicle::VehicleManagerIO lock-guarded accessors, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the two X360-emitted accessors:
//
//   GetVehicleManagerOutputInterface() const  @ 0x825A0FB0  read  (bit 4)  -> +16  (:60)
//   GetVehicleManagerOutputInterface()        @ 0x825A1058  write (bit 3)  -> +16  (:61)
//
// The const (read) handle tests the read-lock bit (((*a1 >> 4) & 1)); the non-const (write)
// handle tests the write-lock bit (((*a1 >> 3) & 1)) -- matching CgsModule::IOBuffer's
// IsBufferLockedForReading()/IsBufferLockedForWriting(). Each asserts the lock (non-gating
// tripwire) and returns the member's address (this + 16).

namespace BrnPhysics
{
namespace Vehicle
{
    void VehicleManagerIO::_AssertLayout()
    {
        static_assert(offsetof(VehicleManagerIO, mVehicleManagerOutputInterface) == 16,
                      "mVehicleManagerOutputInterface @16");
    }

    // X360 0x825A0FB0: read-lock; return this + 16.
    const VehicleManagerIO::VehicleManagerOutputInterfaceStorage*
    VehicleManagerIO::GetVehicleManagerOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mVehicleManagerOutputInterface;
    }

    // X360 0x825A1058: write-lock; return this + 16.
    VehicleManagerIO::VehicleManagerOutputInterfaceStorage*
    VehicleManagerIO::GetVehicleManagerOutputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mVehicleManagerOutputInterface;
    }
}
}
