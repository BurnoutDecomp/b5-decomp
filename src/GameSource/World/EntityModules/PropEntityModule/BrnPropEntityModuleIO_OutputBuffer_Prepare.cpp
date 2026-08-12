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
        // ⭐⭐ REBASED 2026-08-12 (prop-BOOT wave, agent B8). This gate used to pin all three
        // members at their CONSOLE byte offsets (+4 / +1056 / +819824). It could only do that
        // because the members were opaque byte spans cut to exactly those offsets -- i.e. the
        // gate was pinning the bug, not the layout. The members are the real DWARF types now
        // (see the header banner), each wider than its console span on this LLP64 host, so the
        // console offsets are PROVENANCE ONLY and every access goes through the named member.
        //
        // What is still gated, and still has teeth -- these are exactly the two properties
        // whose violation produced the boot's access violation:
        //   * member ORDER is unchanged, and
        //   * the members DO NOT OVERLAP, i.e. each one is genuinely large enough to hold the
        //     type the consumers cast it to (the old 1-byte mPropInputInterface was not).
        static_assert(offsetof(OutputBuffer_Prepare, mResourceRequestInterface) == 4,
                      "mResourceRequestInterface must follow the status byte (console +4)");
        static_assert(offsetof(OutputBuffer_Prepare, mSceneInputInterface)
                          >= offsetof(OutputBuffer_Prepare, mResourceRequestInterface)
                             + sizeof(ResourceRequestInterfaceStorage),
                      "mSceneInputInterface overlaps mResourceRequestInterface");
        static_assert(offsetof(OutputBuffer_Prepare, mPropInputInterface)
                          >= offsetof(OutputBuffer_Prepare, mSceneInputInterface)
                             + sizeof(SceneInputInterfaceStorage),
                      "mPropInputInterface overlaps mSceneInputInterface");
        // Neither interface may silently shrink back below its console extent (the console
        // spans are 1056-4 == 1052 and 819824-1056 == 818768).
        static_assert(sizeof(ResourceRequestInterfaceStorage) >= 1024,
                      "resource-request interface smaller than its console payload -- retype regressed");
        static_assert(sizeof(SceneInputInterfaceStorage) >= 818768 - 16,
                      "scene-input interface smaller than the console span -- retype regressed");
        static_assert(sizeof(PropInputInterfaceStorage) >= 11264,
                      "prop-input interface smaller than the console span -- retype regressed");
    }

    // ========================================================================
    // OutputBuffer_Prepare::Construct   @ 0x822EFC58   (DWARF :593)
    // ------------------------------------------------------------------------
    // ⭐ NEW 2026-08-12 (prop-BOOT wave, agent B8). THIS FUNCTION WAS NEVER WRITTEN, which is
    // why the first boot to reach WorldModule::Prepare fired 21 "Not Constructed" asserts and
    // then took an access violation writing 0x0 inside InSceneUpdateInterface::
    // SetCullingGroupPair: `lpPropOutput->Construct()` at BrnWorldModule.cpp resolved to the
    // inherited two-line CgsModule::IOBuffer::Construct, so the three nested interfaces --
    // which are ALL this function is -- were never built.
    //
    // Store-for-store from the asm (0x822EFC58..0x822EFCE8):
    //   stb  1, 0(this)                                  -> IOBuffer::Construct()   (inlined)
    //   bl   VariableEventQueue<1024,16>::Construct(+4)   \ mResourceRequestInterface.Construct()
    //   bl   VariableEventQueue<1024,16>::Clear(+4)       /  (RequestInterface<N>::Construct is
    //                                                        `queue.Construct(); queue.Clear();`)
    //   bl   AddPhysicalPropEvent_50_::Construct(+819824)     \
    //   bl   RemovePhysicalPropEvent_300_::Construct(+827856)  |  mPropInputInterface.Construct()
    //   bl   RemovePhysicalPartEvent_100_::Construct(+830268)  |  (inlined -- its four queues in
    //   bl   AddPhysicalPartEvent_50_::Construct(+823840)      |   member order, then the flag)
    //   stb  0, 0x2C00(+819824)                               /
    //   bl   InSceneUpdateInterface::Construct(+1056)      -> mSceneInputInterface.Construct()
    //   bl   VariableEventQueue<1024,16>::Clear(+4)        -> mResourceRequestInterface.Clear()
    //   stw  0, 8/0x1F68/0x28D4/0xFB8(+819824); stb 0, 0x2C00  -> mPropInputInterface.Clear()
    //   bl   InSceneUpdateInterface::Clear(+1056)          -> mSceneInputInterface.Clear()
    //
    // So: construct all three (request, prop, scene -- that call order), then clear all three
    // (request, prop, scene -- the same order). The DWARF declares no Clear() on this buffer,
    // so the second pass is written out at this level rather than folded into an invented
    // helper. The apparent double-clear is the console's own: each interface's Construct
    // already ends in its own Clear, and the buffer clears them again.
    // ========================================================================
    void OutputBuffer_Prepare::Construct()
    {
        CgsModule::IOBuffer::Construct();

        mResourceRequestInterface.Construct();
        mPropInputInterface.Construct();
        mSceneInputInterface.Construct();

        mResourceRequestInterface.Clear();
        mPropInputInterface.Clear();
        mSceneInputInterface.Clear();
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
