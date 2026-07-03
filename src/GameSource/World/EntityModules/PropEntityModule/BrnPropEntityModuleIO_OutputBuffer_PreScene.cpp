#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// BrnWorld::PropEntityIO::OutputBuffer_PreScene member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the 3 X360-emitted accessors:
//
//   GetResourceRequestInterface() @ 0x822B9888  -> &mResourceRequestInterface (+4),     write (bit 3)
//   GetPropInputInterface() const @ 0x827A1970  -> &mPropInputInterface (+819824),       read  (bit 4)
//   GetPropInputInterface()       @ 0x822B97E0  -> &mPropInputInterface (+819824),       write (bit 3)
//
// The const (read) handle tests the read-lock bit (((*a1 >> 4) & 1)); the non-const (write)
// handles test the write-lock bit (((*a1 >> 3) & 1)) -- matching CgsModule::IOBuffer's
// IsBufferLockedForReading()/IsBufferLockedForWriting(). The X360 asserts the lock state,
// then returns the member's address (this + 4 / this + 819824).

namespace BrnWorld
{
namespace PropEntityIO
{
    void OutputBuffer_PreScene::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer_PreScene, mResourceRequestInterface) == 4,      "mResourceRequestInterface @4");
        static_assert(offsetof(OutputBuffer_PreScene, mSceneInputInterface)      == 1056,   "mSceneInputInterface @1056");
        static_assert(offsetof(OutputBuffer_PreScene, mPropInputInterface)       == 819824, "mPropInputInterface @819824");
        static_assert(offsetof(OutputBuffer_PreScene, mVisibleOverheadSignArray) == 831104, "mVisibleOverheadSignArray @831104");
    }

    // X360 0x827A1A18 (R, :640) -- read-lock; return the resource-request interface (this+4).
    // Called by WorldModule::BridgePropToOutput_PreScene.
    const OutputBuffer_PreScene::ResourceRequestInterfaceStorage* OutputBuffer_PreScene::GetResourceRequestInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mResourceRequestInterface;
    }

    // X360 0x827A1AC0 (R, :643) -- read-lock; return the visible-overhead-sign array (this+831104).
    // Called by WorldModule::BridgePropToOutput_PreScene.
    const OutputBuffer_PreScene::VisibleOverheadSignArrayStorage* OutputBuffer_PreScene::GetVisibleOverheadSignArray() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mVisibleOverheadSignArray;
    }

    // X360 0x822B9930 (W, :644) -- write-lock; return the visible-overhead-sign array (this+831104).
    // Called by BrnWorld::PropEntityModule::PreSceneUpdate.
    OutputBuffer_PreScene::VisibleOverheadSignArrayStorage* OutputBuffer_PreScene::GetVisibleOverheadSignArray()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mVisibleOverheadSignArray;
    }

    // X360 0x822B9888: write-lock; return this + 4.
    OutputBuffer_PreScene::ResourceRequestInterfaceStorage* OutputBuffer_PreScene::GetResourceRequestInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mResourceRequestInterface;
    }

    // X360 0x827A1970: read-lock; return this + 819824.
    const OutputBuffer_PreScene::PropInputInterfaceStorage* OutputBuffer_PreScene::GetPropInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mPropInputInterface;
    }

    // X360 0x822B97E0: write-lock; return this + 819824.
    OutputBuffer_PreScene::PropInputInterfaceStorage* OutputBuffer_PreScene::GetPropInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mPropInputInterface;
    }
}
}
