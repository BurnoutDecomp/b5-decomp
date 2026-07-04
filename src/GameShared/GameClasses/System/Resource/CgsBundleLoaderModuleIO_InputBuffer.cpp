#include "GameShared/GameClasses/System/Resource/CgsBundleLoaderModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsResource::BundleLoaderIO::InputBuffer member function, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the one X360-emitted read-locked accessor:
//
//   GetEventQueue() const @ 0x828E1D48  -> &mEventQueue (this + 4), read-locked (bit 4)
//
// The X360 body loads the 1-byte FlagSet status (lbz 0(this)), tests the read-lock bit
// (extrwi r11,r11,1,27 == (status >> 4) & 1, i.e. CgsModule::IOBuffer::eStatusLockedForRead),
// asserts "Not locked for reading\n" when clear, then returns this + 4. We return the named
// member's address (semantic parity, not byte-match). Assert path CgsBundleLoaderModuleIO.h:78.
// Consumed by CgsResource::BundleLoaderModule::ProcessBundleLoadRequests.

namespace CgsResource
{
namespace BundleLoaderIO
{
    void InputBuffer::_AssertLayout()
    {
        static_assert(offsetof(InputBuffer, mEventQueue) == 0x0004, "mEventQueue @0x0004");
    }

    // X360 0x828E1D48: read-lock; return this + 4.
    const InputBuffer::EventQueueStorage* InputBuffer::GetEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mEventQueue;
    }
}
}
