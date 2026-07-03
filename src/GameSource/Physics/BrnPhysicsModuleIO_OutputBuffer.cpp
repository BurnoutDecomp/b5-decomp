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
        static_assert(offsetof(OutputBuffer, mVehicleOutputInterface)        == 44128,  "mVehicleOutputInterface @44128");
        static_assert(offsetof(OutputBuffer, mPropManagerOutputInterface)    == 71792,  "mPropManagerOutputInterface @71792");
        static_assert(offsetof(OutputBuffer, mDeformationOutputInterface)    == 148656, "mDeformationOutputInterface @148656");
        static_assert(offsetof(OutputBuffer, mSceneInputInterface)           == 179424, "mSceneInputInterface @179424");
        static_assert(offsetof(OutputBuffer, mContactSpyInterface)           == 998192, "mContactSpyInterface @998192");
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
    OutputBuffer::ContactSpyInterfaceStorage* OutputBuffer::GetContactSpyInterface()
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
