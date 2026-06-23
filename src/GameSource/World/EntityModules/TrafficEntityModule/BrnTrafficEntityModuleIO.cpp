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
}
}
