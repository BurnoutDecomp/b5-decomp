#ifndef CGS_DEVICE_ASYNC_OP_H
#define CGS_DEVICE_ASYNC_OP_H

#include "types.hpp"
#include "GameShared/GameClasses/System/FileSystem/CgsDeviceManager.h"  // CgsFileSystem::Handle, GetDeviceManager, DeviceManager async ops
#include "eathread/eathread_semaphore.h"                                // EA::Thread::Semaphore

// CgsFileSystem::AsyncOp - a one-shot async file operation. The producer issues exactly one
// Open/Read/Write/Close, then blocks on Wait() until the device worker thread fires the completion
// callback (which records the result/handle/size and posts the semaphore). Recovered from
// BURNOUT_X360_ARTIST.XEX + the DecFIGS DWARF (CgsDeviceAsyncOp.h). The "one operation in flight"
// rule is enforced by mbOperating: issuing a second op before Wait()-ing the first asserts.

namespace CgsFileSystem
{
    struct AsyncOp
    {
        AsyncOp();   // @ 0x828DE2F8

        // Issue an async operation; each asserts no prior op is still in flight, latches mbOperating,
        // and hands the op to the device manager with CompletionCallback / this as the continuation.
        void Open(const char* lpcFileName, u32 luFlags, s32 liPriority);                                  // @ 0x828F2300
        void Read(Handle lHandle, u64 luPosition, void* lpBuffer, u32 luBufferSize, s32 liPriority);      // @ 0x828F23D8
        void Close(Handle lHandle, s32 liPriority);                                                       // @ 0x828F24C0

        // Block until the in-flight operation completes (asserts one is in flight), then clear it.
        void Wait();   // @ 0x828DE338

        // Results of the last completed operation (trivial inline accessors; inlined at the X360
        // call sites).
        u64    GetLastOperationSize()   const { return muSize; }
        Handle GetLastOperationHandle() const { return mHandle; }
        s32    GetLastOperationResult() const { return miResult; }

    private:
        // The device worker's completion continuation: record the op outcome and wake Wait().
        static void CompletionCallback(s32 liResult, Handle lHandle, u64 luSize, void* lpContext);   // @ 0x828DE4A0

        EA::Thread::Semaphore mSemaphore;     // posted by CompletionCallback, waited by Wait()
        Handle                mHandle;        // handle from the last completed op
        volatile s32          miResult;       // result code from the last completed op
        volatile u64          muSize;         // byte count from the last completed op
        volatile bool         mbOperating;    // true while an op is in flight (gates re-issue / Wait)
    };
}

#endif // CGS_DEVICE_ASYNC_OP_H
