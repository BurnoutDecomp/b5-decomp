#include "GameShared/GameClasses/System/Resource/CgsBundleLoaderModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsResource::BundleLoaderIO::InputBuffer_Update read-locked accessors, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies:
//
//   GetEventQueue()               const @ 0x828E1D48 -> &mLoadBundleRequestQueue   (this + 4),      read-lock (bit 4)
//   GetUnloadBundleRequestQueue() const @ 0x828E1E98 -> &mUnloadBundleRequestQueue (this + 0x9410), read-lock (bit 4)  (:81)
//
// Each loads the 1-byte FlagSet status (lbz 0(this)), tests the read-lock bit (extrwi r11,r11,1,27
// == (status >> 4) & 1 == eStatusLockedForRead), asserts "Not locked for reading\n" when clear
// (rodata carries the trailing newline), then returns the named member's address. GetEventQueue is
// the historical name for the load-bundle request queue at +4. The write-locked overloads live in
// CgsBundleLoaderModuleIO_InputBuffer_Update.cpp.

namespace CgsResource
{
namespace BundleLoaderIO
{
    void InputBuffer_Update::_AssertLayout()
    {
        static_assert(offsetof(InputBuffer_Update, mLoadBundleRequestQueue) == 0x0004,
                      "mLoadBundleRequestQueue @0x0004");
    }

    // X360 0x828E1D48: read-lock; return this + 4 (the load-bundle request queue).
    const InputBuffer_Update::EventQueueStorage* InputBuffer_Update::GetEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mLoadBundleRequestQueue;
    }

    // X360 0x828E1E98: read-lock; return &mUnloadBundleRequestQueue (this + 0x9410).
    // (DWARF CgsBundleLoaderModuleIO.h:81, const overload.) Consumed by
    // CgsResource::BundleLoaderModule::ProcessBundleLoadRequests.
    const InputBuffer_Update::UnloadBundleRequestQueue* InputBuffer_Update::GetUnloadBundleRequestQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");   // :81
        return &mUnloadBundleRequestQueue;
    }
}
}
