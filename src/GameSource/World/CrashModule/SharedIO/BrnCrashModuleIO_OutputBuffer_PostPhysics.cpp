#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO.h"   // OutputBuffer_PostPhysics
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT

#include <cstddef>   // offsetof

// BrnWorld::CrashIO::OutputBuffer_PostPhysics::GetNetworkOutputInterface() @ 0x827BB9C0.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Asserts the buffer is locked for writing
// (status bit 3, IsBufferLockedForWriting()) -- the X360 streams the message
// "Not locked for writing\n" and fires CgsDev::Assert::FireAssert at BrnCrashModuleIO.h:208,
// a non-gating tripwire -- then returns the embedded network-output interface (the X360 returns
// `this + 0x10`, i.e. &this->mNetworkOutputInterface). The producer
// (CrashModule::GenerateOwnedTrafficUpdates @0x827C53F0) reads it and forwards each owned crashing
// traffic update to NetworkOutputInterface::AddOwnedTrafficUpdate.

namespace BrnWorld
{
namespace CrashIO
{
    NetworkOutputInterface* OutputBuffer_PostPhysics::GetNetworkOutputInterface()
    {
        // The interface offset is load-bearing for this getter (the X360 returns this+0x10).
        // Pinned from inside the member so offsetof can see the private member.
        static_assert(offsetof(OutputBuffer_PostPhysics, mNetworkOutputInterface) == 0x10,
                      "mNetworkOutputInterface @0x10");
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mNetworkOutputInterface;
    }
}
}
