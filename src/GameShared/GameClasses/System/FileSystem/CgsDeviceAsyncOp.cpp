// Bodies for the one-shot async file operation, reconstructed from BURNOUT_X360_ARTIST.XEX:
//   AsyncOp            @ 0x828DE2F8
//   Open               @ 0x828F2300
//   Read               @ 0x828F23D8
//   Close              @ 0x828F24C0
//   Wait               @ 0x828DE338
//   CompletionCallback @ 0x828DE4A0
//
// Each issue routes through CgsFileSystem::GetDeviceManager() and registers CompletionCallback +
// this as the continuation; the device worker thread runs the op and posts the semaphore that Wait()
// blocks on. mbOperating enforces a single op in flight.

#include "GameShared/GameClasses/System/FileSystem/CgsDeviceAsyncOp.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace CgsFileSystem
{
    // @ 0x828DE2F8. The semaphore starts at count 0 (Wait blocks until the first completion Post);
    // no operation is in flight yet.
    AsyncOp::AsyncOp()
        : mSemaphore(0)
        , mbOperating(false)
    {
    }

    // @ 0x828F2300.
    void AsyncOp::Open(const char* lpcFileName, u32 luFlags, s32 liPriority)
    {
        CGS_ASSERT(!mbOperating, "Last operation has not been waited before\n");
        mbOperating = true;
        GetDeviceManager()->Open(lpcFileName, luFlags, &AsyncOp::CompletionCallback, this, liPriority);
    }

    // @ 0x828F23D8.
    void AsyncOp::Read(Handle lHandle, u64 luPosition, void* lpBuffer, u32 luBufferSize, s32 liPriority)
    {
        CGS_ASSERT(!mbOperating, "Last operation has not been waited before\n");
        mbOperating = true;
        GetDeviceManager()->Read(lHandle, luPosition, lpBuffer, luBufferSize, &AsyncOp::CompletionCallback, this, liPriority);
    }

    // @ 0x828F24C0.
    void AsyncOp::Close(Handle lHandle, s32 liPriority)
    {
        CGS_ASSERT(!mbOperating, "Last operation has not been waited before\n");
        mbOperating = true;
        GetDeviceManager()->Close(lHandle, &AsyncOp::CompletionCallback, this, liPriority);
    }

    // @ 0x828DE338. Block on the completion semaphore, then mark the op finished.
    void AsyncOp::Wait()
    {
        CGS_ASSERT(mbOperating, "No operation\n");
        mSemaphore.Wait();
        mbOperating = false;
    }

    // @ 0x828DE4A0. Device-worker continuation: record the outcome on the issuing AsyncOp and wake
    // the producer waiting in Wait().
    void AsyncOp::CompletionCallback(s32 liResult, Handle lHandle, u64 luSize, void* lpContext)
    {
        AsyncOp* lpOp = static_cast<AsyncOp*>(lpContext);
        lpOp->mHandle  = lHandle;
        lpOp->miResult = liResult;
        lpOp->muSize   = luSize;
        lpOp->mSemaphore.Post(1);
    }
}
