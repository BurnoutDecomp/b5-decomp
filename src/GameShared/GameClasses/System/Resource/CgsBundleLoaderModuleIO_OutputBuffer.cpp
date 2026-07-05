#include "GameShared/GameClasses/System/Resource/CgsBundleLoaderModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsResource::BundleLoaderIO::OutputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies:
//
//   GetPool()          @ 0x828E21E0 -> &mPoolSendQueue             (this + 4),       write-lock (bit 3)  (:143)
//   GetLoadBundleResponseQueue()   const @ 0x828E2288 -> (this + 0x1014),  read-lock  (bit 4)  (:145)
//   GetLoadBundleResponseQueue()         @ 0x828E2330 -> (this + 0x1014),  write-lock (bit 3)  (:146)
//   GetUnloadBundleResponseQueue() const @ 0x828E23D8 -> (this + 0xA420),  read-lock  (bit 4)  (:148)
//   GetUnloadBundleResponseQueue()       @ 0x828E2480 -> (this + 0xA420),  write-lock (bit 3)  (:149)
//   GetStream()        const @ 0x828E2528 -> &mStreamRequestQueue  (this + 0x1342C), read-lock  (bit 4)  (:151)
//
// Each loads the 1-byte FlagSet status (lbz 0(this)) and tests its lock bit (read: extrwi ...,27 ==
// (status>>4)&1; write: extrwi ...,28 == (status>>3)&1), asserts when clear (the read/write assert
// strings carry a trailing \n), then returns the payload address. GetPool/GetStream are the
// historical names for mPoolSendQueue (+4) / mStreamRequestQueue (+0x1342C).

namespace CgsResource
{
namespace BundleLoaderIO
{
    void OutputBuffer::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer, mPoolSendQueue) == 0x0004, "mPoolSendQueue @0x0004");
        static_assert(offsetof(OutputBuffer, mStreamRequestQueue) == 0x1342C, "mStreamRequestQueue @0x1342C");
    }

    // X360 0x828E21E0: write-lock; return this + 4 (the pool-send queue).
    OutputBuffer::PoolStorage* OutputBuffer::GetPool()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");   // :143
        return &mPoolSendQueue;
    }

    // X360 0x828E2288: read-lock (bit 4); return &mLoadBundleResponseQueue (this + 0x1014).
    // (DWARF CgsBundleLoaderModuleIO.h:145, const overload.) Consumed by
    // CgsResource::ResourceModule::ProcessResourceResponses.
    const OutputBuffer::LoadBundleResponseQueue* OutputBuffer::GetLoadBundleResponseQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");   // :145
        return &mLoadBundleResponseQueue;
    }

    // X360 0x828E2330: write-lock (bit 3); return &mLoadBundleResponseQueue (this + 0x1014).
    OutputBuffer::LoadBundleResponseQueue* OutputBuffer::GetLoadBundleResponseQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");   // :146
        return &mLoadBundleResponseQueue;
    }

    // X360 0x828E23D8: read-lock (bit 4); return &mUnloadBundleResponseQueue (this + 0xA420).
    // (DWARF CgsBundleLoaderModuleIO.h:148, const overload.)
    const OutputBuffer::UnloadBundleResponseQueue* OutputBuffer::GetUnloadBundleResponseQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");   // :148
        return &mUnloadBundleResponseQueue;
    }

    // X360 0x828E2480: write-lock (bit 3); return &mUnloadBundleResponseQueue (this + 0xA420).
    // (DWARF CgsBundleLoaderModuleIO.h:149, non-const overload.) Consumed by
    // CgsResource::BundleLoaderModule::CheckForUnloads.
    OutputBuffer::UnloadBundleResponseQueue* OutputBuffer::GetUnloadBundleResponseQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");   // :149
        return &mUnloadBundleResponseQueue;
    }

    // X360 0x828E2528: read-lock; return this + 0x1342C (the stream-request queue).
    const OutputBuffer::StreamStorage* OutputBuffer::GetStream() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");   // :151
        return &mStreamRequestQueue;
    }
}
}
