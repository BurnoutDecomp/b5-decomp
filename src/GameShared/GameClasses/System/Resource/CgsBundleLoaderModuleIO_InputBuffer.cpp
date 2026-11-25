#include "GameShared/GameClasses/System/Resource/CgsBundleLoaderModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"

#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\resource\\CgsBundleLoaderModuleIO.h"

#include <cstddef>   // offsetof

// CgsResource::BundleLoaderIO::InputBuffer_Update read-locked accessors, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies:
//
//   GetLoadBundleRequestQueue()   const @ 0x828E1D48 -> &mLoadBundleRequestQueue   (this + 4),      read-lock (bit 4)
//   GetUnloadBundleRequestQueue() const @ 0x828E1E98 -> &mUnloadBundleRequestQueue (this + 0x9410), read-lock (bit 4)  (:81)
//
// Each loads the 1-byte FlagSet status (lbz 0(this)), tests the read-lock bit (extrwi r11,r11,1,27
// == (status >> 4) & 1 == eStatusLockedForRead), asserts "Not locked for reading\n" when clear
// (rodata carries the trailing newline), then returns the named member's address. The write-locked overloads live in
// CgsBundleLoaderModuleIO_InputBuffer_Update.cpp.

namespace CgsResource
{
namespace BundleLoaderIO
{
    void InputBuffer_Update::Construct()
    {
        IOBuffer::Construct();
        mLoadBundleRequestQueue.Construct();
        mUnloadBundleRequestQueue.Construct();
    }

    void InputBuffer_Update::Destruct() { IOBuffer::Destruct(); }

    const InputBuffer_Update::LoadBundleRequestQueue* InputBuffer_Update::GetLoadBundleRequestQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), CGS_ASSERT_STREAM_MESSAGE_AT(
            "Not locked for reading\n", CGS_ARTIST_FILE, 78));
        return &mLoadBundleRequestQueue;
    }

    const InputBuffer_Update::EventQueueStorage* InputBuffer_Update::GetEventQueue() const
    {
        return GetLoadBundleRequestQueue();
    }

    // X360 0x828E1E98: read-lock; return &mUnloadBundleRequestQueue (this + 0x9410).
    // (DWARF CgsBundleLoaderModuleIO.h:81, const overload.) Consumed by
    // CgsResource::BundleLoaderModule::ProcessBundleLoadRequests.
    const InputBuffer_Update::UnloadBundleRequestQueue* InputBuffer_Update::GetUnloadBundleRequestQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), CGS_ASSERT_STREAM_MESSAGE_AT(
            "Not locked for reading\n", CGS_ARTIST_FILE, 81));
        return &mUnloadBundleRequestQueue;
    }
}
}

#undef CGS_ARTIST_FILE
