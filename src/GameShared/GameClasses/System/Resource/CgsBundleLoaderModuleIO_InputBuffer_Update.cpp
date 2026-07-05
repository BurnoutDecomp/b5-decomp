#include "GameShared/GameClasses/System/Resource/CgsBundleLoaderModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsResource::BundleLoaderIO::InputBuffer_Update write-locked accessors, reconstructed from
// BURNOUT_X360_ARTIST.XEX. Bodies the two X360-emitted WRITE-locked request-queue accessors of
// the update input buffer (DWARF CgsBundleLoaderModuleIO.h:66 class):
//
//   GetLoadBundleRequestQueue()    @ 0x828E1DF0 -> &mLoadBundleRequestQueue   (this + 4),      write-lock (bit 3)  (DWARF :79)
//   GetUnloadBundleRequestQueue()  @ 0x828E1F40 -> &mUnloadBundleRequestQueue (this + 0x9410), write-lock (bit 3)  (DWARF :82)
//
// Each loads the 1-byte FlagSet status (lbz 0(this)), tests the write-lock bit (extrwi ...,28 ==
// (status>>3)&1 == eStatusLockedForWrite), asserts "Not locked for writing\n" (rodata carries the
// trailing newline) when clear, then returns the named queue's address. Both drained by
// CgsResource::ResourceModule::ProcessResourceRequests. The const read-locked overloads (:78 / :81)
// are bodied in CgsBundleLoaderModuleIO_InputBuffer.cpp.
//
// No offsetof pins: the queues embed BaseEventQueue's mpEvents pointer, which widens 4->8 on the
// 64-bit host, so the X360 32-bit member offsets are not host-assertable.

namespace CgsResource
{
namespace BundleLoaderIO
{
    // X360 0x828E1DF0: write-lock; return &mLoadBundleRequestQueue (this + 4).
    InputBuffer_Update::LoadBundleRequestQueue* InputBuffer_Update::GetLoadBundleRequestQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");   // :79
        return &mLoadBundleRequestQueue;
    }

    // X360 0x828E1F40: write-lock; return &mUnloadBundleRequestQueue (this + 0x9410).
    InputBuffer_Update::UnloadBundleRequestQueue* InputBuffer_Update::GetUnloadBundleRequestQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");   // :82
        return &mUnloadBundleRequestQueue;
    }
}
}
