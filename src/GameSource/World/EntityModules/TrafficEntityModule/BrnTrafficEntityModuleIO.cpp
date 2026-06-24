#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// BrnTraffic::BrnTrafficIO::OutputBuffer_PostScene::GetTrafficAIInterface() (non-const, write)
//   @ 0x827111C0. Reconstructed from BURNOUT_X360_ARTIST.XEX. Asserts the buffer is locked for
// writing (status bit 3, IsBufferLockedForWriting()) then returns the embedded AI interface
// (the X360 returns `a1 + 16416`, i.e. &this->mTrafficAIInterface). The producer
// (ConvertSceneResultsToTrafficDataForAI @ 0x82728518) calls this, reads the entity count at the
// interface's offset 0 and memcpy's 176-byte TrafficAIEntity records onto the active list.

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // X360 0x827111C0: write-lock; return this + 16416.
    TrafficAIInterface* OutputBuffer_PostScene::GetTrafficAIInterface()
    {
        // The AI-interface offset is load-bearing for this getter (the X360 returns this+16416).
        // Pinned from inside the member so offsetof can see the private members. The coarse-query
        // queue precedes it: the X360 places the queue at status+4 (its status word is 4 bytes vs
        // our 1-byte IOBuffer FlagSet), but the 16-aligned mTrafficAIInterface still lands at 16416.
        static_assert(offsetof(OutputBuffer_PostScene, mTrafficAIInterface) == 16416,
                      "mTrafficAIInterface @16416");
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mTrafficAIInterface;
    }

    // ========================================================================
    // OutputBuffer_PostPhysics handle accessors (reconstructed from BURNOUT_X360_ARTIST.XEX).
    // Each tests the lock state then returns the member's pinned address. The return offsets
    // (+8 / +834784 / +834828) are the load-bearing facts -- pinned here so offsetof can see the
    // private members.
    void OutputBuffer_PostPhysics::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer_PostPhysics, mInterfaceAt8)      == 8,      "mInterfaceAt8 @8");
        static_assert(offsetof(OutputBuffer_PostPhysics, mInterfaceAt834784) == 834784, "mInterfaceAt834784 @834784");
        static_assert(offsetof(OutputBuffer_PostPhysics, mInterfaceAt834828) == 834828, "mInterfaceAt834828 @834828");
    }

    // X360 0x82711A48 (asm-line 392): write-lock; return this + 8.
    OutputBuffer_PostPhysics::InterfaceAt8Storage* OutputBuffer_PostPhysics::GetWriteInterfaceAt8()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mInterfaceAt8;
    }

    // X360 0x82711D90 (asm-line 407): write-lock; return this + 834784.
    OutputBuffer_PostPhysics::InterfaceAt834784Storage* OutputBuffer_PostPhysics::GetWriteInterfaceAt834784()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mInterfaceAt834784;
    }

    // X360 0x827A0E18 (asm-line 418): read-lock; return this + 834828.
    const OutputBuffer_PostPhysics::InterfaceAt834828Storage* OutputBuffer_PostPhysics::GetReadInterfaceAt834828() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mInterfaceAt834828;
    }

    // X360 0x82712030 (asm-line 419): write-lock; return this + 834828.
    OutputBuffer_PostPhysics::InterfaceAt834828Storage* OutputBuffer_PostPhysics::GetWriteInterfaceAt834828()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mInterfaceAt834828;
    }
}
}
