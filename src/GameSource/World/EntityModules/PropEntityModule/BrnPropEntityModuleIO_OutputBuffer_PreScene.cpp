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
        // ⭐⭐ REBASED 2026-08-12 (prop-BOOT wave, agent B8) -- same reasoning as the
        // OutputBuffer_Prepare twin: the four console offsets were only reproducible while the
        // members were opaque spans cut to them. The members are the real DWARF types now, each
        // wider on this LLP64 host, so the gate pins ORDER + NON-OVERLAP + no-shrink instead.
        static_assert(offsetof(OutputBuffer_PreScene, mResourceRequestInterface) == 4,
                      "mResourceRequestInterface must follow the status byte (console +4)");
        static_assert(offsetof(OutputBuffer_PreScene, mSceneInputInterface)
                          >= offsetof(OutputBuffer_PreScene, mResourceRequestInterface)
                             + sizeof(ResourceRequestInterfaceStorage),
                      "mSceneInputInterface overlaps mResourceRequestInterface");
        static_assert(offsetof(OutputBuffer_PreScene, mPropInputInterface)
                          >= offsetof(OutputBuffer_PreScene, mSceneInputInterface)
                             + sizeof(SceneInputInterfaceStorage),
                      "mPropInputInterface overlaps mSceneInputInterface");
        static_assert(offsetof(OutputBuffer_PreScene, mVisibleOverheadSignArray)
                          >= offsetof(OutputBuffer_PreScene, mPropInputInterface)
                             + sizeof(PropInputInterfaceStorage),
                      "mVisibleOverheadSignArray overlaps mPropInputInterface");
        static_assert(sizeof(SceneInputInterfaceStorage) >= 818768 - 16,
                      "scene-input interface smaller than the console span -- retype regressed");
        static_assert(sizeof(PropInputInterfaceStorage) >= 11264,
                      "prop-input interface smaller than the console span -- retype regressed");
        // The count word Construct zeroes sits at +1024 inside the array (console buffer+832128
        // == &mVisibleOverheadSignArray + 1024).
        static_assert(offsetof(VisibleOverheadSignArrayStorage, miCount) == 1024,
                      "overhead-sign count word @ +1024 (console buffer+832128)");
    }

    // ========================================================================
    // OutputBuffer_PreScene::Construct   @ 0x822EFB98   (DWARF :628)
    // ------------------------------------------------------------------------
    // ⭐ NEW 2026-08-12 (prop-BOOT wave, agent B8) -- same never-written hole as the Prepare
    // twin: `lpPropOutput_PreScene->Construct()` in WorldModule's pre-scene stage resolved to
    // the inherited CgsModule::IOBuffer::Construct, leaving all four members un-built.
    //
    // Store-for-store from the asm, in the console's call order:
    //   stb 1, 0(this)                                    -> IOBuffer::Construct()
    //   bl  InSceneUpdateInterface::Construct(+1056)       -> mSceneInputInterface.Construct()
    //   bl  AddPhysicalPropEvent_50_::Construct(+819824)    \
    //   bl  RemovePhysicalPropEvent_300_::Construct(+827856) |  mPropInputInterface.Construct()
    //   bl  RemovePhysicalPartEvent_100_::Construct(+830268) |  (inlined, four queues + flag)
    //   bl  AddPhysicalPartEvent_50_::Construct(+823840)     |
    //   stb 0, [+831088]                                    /
    //   bl  VariableEventQueue<1024,16>::Construct(+4)      \ mResourceRequestInterface.Construct()
    //   bl  VariableEventQueue<1024,16>::Clear(+4)          /
    //   stw 0, [+832128]                                    -> mVisibleOverheadSignArray.Clear()
    //   bl  VariableEventQueue<1024,16>::Clear(+4)          -> mResourceRequestInterface.Clear()
    //   bl  InSceneUpdateInterface::Clear(+1056)            -> mSceneInputInterface.Clear()
    //   stw 0, [+819832/+827864/+830276/+823848]; stb 0, [+831088] -> mPropInputInterface.Clear()
    //   stw 0, [+832128]                                    -> mVisibleOverheadSignArray.Clear()
    // (Hex-Rays renders the four prop-queue length stores as *(v3+2)/(+1006)/(+2010)/(+2613) on
    //  an int* -- i.e. byte +8 / +4024 / +8040 / +10452 from +819824, which are exactly the four
    //  miLength fields; that is what identifies the tail as PropInputInterface::Clear.)
    //
    // The Construct pass runs scene -> prop -> request -> overhead; the Clear pass runs
    // request -> scene -> prop -> overhead. Both orders are reproduced as the asm has them.
    // ========================================================================
    void OutputBuffer_PreScene::Construct()
    {
        CgsModule::IOBuffer::Construct();

        mSceneInputInterface.Construct();
        mPropInputInterface.Construct();
        mResourceRequestInterface.Construct();
        mVisibleOverheadSignArray.Clear();

        mResourceRequestInterface.Clear();
        mSceneInputInterface.Clear();
        mPropInputInterface.Clear();
        mVisibleOverheadSignArray.Clear();
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

    // X360 0x827A18C8 (IDA `sub_827A18C8`, :634): read-lock; return this + 1056.
    // The const twin of the write-lock getter below. Called by
    // WorldModule::BridgeEntityModulesToSceneModule_PreScene (the prop leg of the per-frame
    // scene merge) -- see the header note.
    const OutputBuffer_PreScene::SceneInputInterfaceStorage* OutputBuffer_PreScene::GetSceneInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mSceneInputInterface;
    }

    // X360 0x822B9738 (IDA `sub_822B9738`): write-lock; return this + 1056.
    // Reconstructed store-for-store: the body is the write-lock tripwire followed by
    // `return a1 + 1056`.
    OutputBuffer_PreScene::SceneInputInterfaceStorage* OutputBuffer_PreScene::GetSceneInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mSceneInputInterface;
    }
}
}
