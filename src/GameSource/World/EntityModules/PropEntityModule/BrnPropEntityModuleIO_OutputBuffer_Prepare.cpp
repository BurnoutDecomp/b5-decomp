#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// BrnWorld::PropEntityIO::OutputBuffer_Prepare member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the 3 X360-emitted accessors:
//
//   GetResourceRequestInterface() @ 0x822B9690  -> &mResourceRequestInterface (+4),     write (bit 3)
//   GetPropInputInterface() const @ 0x827A1778  -> &mPropInputInterface (+819824),       read  (bit 4)
//   GetPropInputInterface()       @ 0x822B95E8  -> &mPropInputInterface (+819824),       write (bit 3)
//
// The const (read) handle tests the read-lock bit (((*a1 >> 4) & 1), `extrwi r11,r11,1,27`); the
// non-const (write) handles test the write-lock bit (((*a1 >> 3) & 1), `extrwi r11,r11,1,28`) --
// matching CgsModule::IOBuffer's IsBufferLockedForReading()/IsBufferLockedForWriting(). The X360
// asserts the lock state (streaming "Not locked for reading/writing\n", a non-gating tripwire at
// BrnPropEntityModuleIO.h:607/610/613), then returns the member's address (this + 4 / this + 819824
// via `addis rN,0xD; addi rN,-0x7FFC` == +4 and `addis rN,0xD; addi rN,-0x7D90` == +819824).

namespace BrnWorld
{
namespace PropEntityIO
{
    void OutputBuffer_Prepare::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer_Prepare, mResourceRequestInterface) == 4,      "mResourceRequestInterface @4");
        static_assert(offsetof(OutputBuffer_Prepare, mSceneInputInterface)      == 1056,   "mSceneInputInterface @1056");
        static_assert(offsetof(OutputBuffer_Prepare, mPropInputInterface)       == 819824, "mPropInputInterface @819824");
    }

    // X360 0x827A1820 (R, :605) -- read-lock; return the resource-request interface (this+4).
    // Called by WorldModule::BridgePropResourceRequestsToOutput_Prepare.
    const OutputBuffer_Prepare::ResourceRequestInterfaceStorage* OutputBuffer_Prepare::GetResourceRequestInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mResourceRequestInterface;
    }

    // X360 0x827A16D0 (R, :599) -- read-lock; return the scene-input interface (this+1056).
    // Called by WorldModule::BridgePropModuleToSceneModule_Prepare.
    const OutputBuffer_Prepare::SceneInputInterfaceStorage* OutputBuffer_Prepare::GetSceneInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mSceneInputInterface;
    }

    // X360 0x822B9540 (W, :602) -- write-lock; return the scene-input interface (this+1056).
    // Called by BrnWorld::PropEntityModule::InitializePropPhysicsData.
    OutputBuffer_Prepare::SceneInputInterfaceStorage* OutputBuffer_Prepare::GetSceneInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mSceneInputInterface;
    }

    // X360 0x822B9690: write-lock; return this + 4.
    OutputBuffer_Prepare::ResourceRequestInterfaceStorage* OutputBuffer_Prepare::GetResourceRequestInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mResourceRequestInterface;
    }

    // X360 0x827A1778: read-lock; return this + 819824.
    const OutputBuffer_Prepare::PropInputInterfaceStorage* OutputBuffer_Prepare::GetPropInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mPropInputInterface;
    }

    // X360 0x822B95E8: write-lock; return this + 819824.
    OutputBuffer_Prepare::PropInputInterfaceStorage* OutputBuffer_Prepare::GetPropInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mPropInputInterface;
    }
}
}
