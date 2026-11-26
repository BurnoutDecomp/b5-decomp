// XDK size optimization: ARTIST compiled the RenderWare core filesys library for size
// (record-form cr0 null tests, e.g. AsyncOp::DoClose 0x82BBE028 `cmplwi r3,0`;
// Stream::readcallback 0x82BC0598 and Handle::~Handle 0x82BBD6A0 match only so).
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
// The RenderWare filesys operation lifecycle and handle results.
#include "SDKs/EATech/rwcore/filesys/device.h"  // Device / Manager / Allocator (AsyncOp dispatches here)

#include <cstring>   // memcpy
#include <new>       // placement new (Handle construction over allocator storage)
#include <xtl.h>     // MemoryBarrier

namespace rw
{
    namespace core
    {
        namespace filesys
        {
            // The X360 default completion / DoIo callback (a shared empty function the asm
            // patches in when the caller passes none). Modelled as a file-local no-op.
            static void DefaultCompletion(AsyncOp* /*lpOp*/)
            {
            }

            // GetSize @0x82BBD700:  ld r3, 0x18(r3); blr -- return the 64-bit size at +0x18.
            u64 GetSize(const Handle* lpHandle)
            {
                return lpHandle->mu64Size;
            }

            // =============================================================================
            // rw::core::filesys::AsyncOp -- the async-op class itself.
            // =============================================================================

            // -----------------------------------------------------------------------------
            // AsyncOp::AsyncOp @0x82BBD3E0 -- zero/-3 init of the named fields in the 0x48-byte object.
            //   mStatus = OPSTATUS_NEW; mpfnComplete = STUB; everything else 0.
            // -----------------------------------------------------------------------------
            AsyncOp::AsyncOp()
            {
                mStatus = OPSTATUS_NEW;
                mpNext = 0;
                mbCancelled = false;
                mbIsReadOp = false;
                miPriority = 0;
                mpFileHandle = 0;
                mpContext = 0;
                mpfnComplete = &DefaultCompletion;
                mu64Position = 0;
                mu64ByteCount = 0;
                mpBuffer = 0;
                mu64BytesDone = 0;
                mpDevice = 0;
                mpfnDoIo = 0;
            }

            // -----------------------------------------------------------------------------
            // AsyncOp::~AsyncOp @0x82BBD430 -- lwsync, then zero +0x10/+0x14/+0x30/+0x40
            // and the link +0x00 (the only fields the asm clears).
            // -----------------------------------------------------------------------------
            AsyncOp::~AsyncOp()
            {
                ::MemoryBarrier();      // lwsync
                mpFileHandle  = 0;    // result[4] (+0x10) = 0
                mpContext = 0;          // result[5] (+0x14) = 0
                mpBuffer  = 0;    // result[12] (+0x30) = 0
                mpDevice  = 0;    // result[16] (+0x40) = 0
                mpNext    = 0;    // *result (+0x00) = 0
            }

            // -----------------------------------------------------------------------------
            // AsyncOp::Open @0x82BC00C0
            //
            // Fill an open op (read flag clear), resolve the path's Device, allocate a copy
            // of the path into mpBuffer, default the callback to STUB, set DoIo = DoOpen, and
            // queue the op on its device (the default device when the device is "shared").
            //   a1=this a2=path a3=flags a4=callback a5=userData a6=priority
            // -----------------------------------------------------------------------------
            void AsyncOp::Open(const char* lpcPath, u32 luFlags, CompletionCallback lpfnComplete,
                              void* lpContext, s32 liPriority)
            {
                ::MemoryBarrier();              // lwsync
                mStatus = OPSTATUS_PENDING;                   // *(a1+4) = 0
                ::MemoryBarrier();              // lwsync

                mbCancelled   = false;              // *(a1+8) = 0
                mbIsReadOp    = false;              // *(a1+9) = 0
                miPriority    = liPriority;     // stw r8, 0xC
                mpFileHandle  = 0;        // stw r11(=0), 0x10
                mpContext     = lpContext;     // stw r7, 0x14
                mu64Position  = luFlags;        // std clrldi(r5,32), 0x20  (flags zero-extended)
                mu64ByteCount = 0;              // std r11(=0), 0x28
                mu64BytesDone = 0;              // std r11(=0), 0x38

                Device* lpDevice = Device::GetInstance(lpcPath, 0);
                mpDevice = lpDevice;            // *(a1+64) = Instance

                // Allocate a copy of the path (length incl. NUL) and copy it into mpBuffer.
                // ARTIST 0x82BC0120..0x82BC0154 is the inlined strlen, then + 1 (PS3 DecFIGS
                // AsyncOp::Open: strlen(path) + 1 -> Manager::Allocate -> memcpy).
                const size_t luLen = ::strlen(lpcPath) + 1;
                void* lpPathCopy = gpFileSysAllocator->Alloc(luLen, "rw::core::filesystem::Manager::Allocate", 0, 4, 0);
                mpBuffer = lpPathCopy;          // *(a1+48) = v15
                ::memcpy(lpPathCopy, lpcPath, luLen);

                if (!lpfnComplete)
                    lpfnComplete = &DefaultCompletion;   // a4 = STUB
                mpfnComplete = lpfnComplete;    // *(a1+24) = a4
                mpfnDoIo = &AsyncOp::DoOpen;    // *(a1+68) = DoOpen

                Device* lpQueue = mpDevice;
                if (lpQueue->IsExternalThread()) // *(v16+6) -- external-thread device flag
                    lpQueue = gpFileSysManager->mpDefaultDevice;  // *(off_8327F078+10)
                lpQueue->InsertOp(this);
            }

            // -----------------------------------------------------------------------------
            // AsyncOp::Read @0x82BC01D0  /  AsyncOp::Write @0x82BBFCF0
            //
            // Identical store sequence; they differ only in mbIsReadOp (Read=1/Write=0) and
            // the DoIo callback (DoRead vs DoWrite). The owning Device is read from the
            // stream (stream+0x0C). The op is queued on its own device directly (no shared
            // redirect -- the asm reads *(a1+64) straight into r3).
            //   a1=this a2=stream a3=buffer a4=position a5=callback a6=userData a7=priority
            // -----------------------------------------------------------------------------
            void AsyncOp::Read(Handle* lpFileHandle, void* lpBuffer, u64 lu64Position, u64 lu64Count,
                              CompletionCallback lpfnComplete, void* lpContext, s32 liPriority)
            {
                ::MemoryBarrier();              // lwsync
                mStatus = OPSTATUS_PENDING;                   // *(a1+4) = 0
                ::MemoryBarrier();              // lwsync

                mbCancelled   = false;              // *(a1+8) = 0
                mbIsReadOp    = true;              // *(a1+9) = 1
                mpDevice      = lpFileHandle->mpDevice; // v10 = *(a2+12); *(a1+64) = v10
                miPriority    = liPriority;     // *(a1+12) = a7
                mpFileHandle  = lpFileHandle; // *(a1+16) = a2
                mu64Position  = lu64Position;   // std r6, 0x20
                mpBuffer      = lpBuffer;       // *(a1+48) = a3  (stw r5, 0x30)
                mu64ByteCount = lu64Count;      // std r7, 0x28
                mu64BytesDone = 0;              // std r3(=0), 0x38
                mpContext     = lpContext;     // *(a1+20) = a6

                if (!lpfnComplete)
                    lpfnComplete = &DefaultCompletion;
                mpfnComplete = lpfnComplete;    // *(a1+24) = a5
                mpfnDoIo = &AsyncOp::DoRead;    // *(a1+68) = DoRead

                mpDevice->InsertOp(this);
            }

            void AsyncOp::Write(Handle* lpFileHandle, const void* lpBuffer, u64 lu64Position, u64 lu64Count,
                               CompletionCallback lpfnComplete, void* lpContext, s32 liPriority)
            {
                ::MemoryBarrier();              // lwsync
                mStatus = OPSTATUS_PENDING;                   // *(a1+4) = 0
                ::MemoryBarrier();              // lwsync

                mbCancelled   = false;              // *(a1+8) = 0
                mbIsReadOp    = false;              // *(a1+9) = 0
                mpDevice      = lpFileHandle->mpDevice; // v10 = *(a2+12); *(a1+64) = v10
                miPriority    = liPriority;     // *(a1+12) = a7
                mpFileHandle  = lpFileHandle; // *(a1+16) = a2
                mu64Position  = lu64Position;   // std r6, 0x20
                mpBuffer      = const_cast<void*>(lpBuffer);       // *(a1+48) = a3  (stw r5, 0x30)
                mu64ByteCount = lu64Count;      // std r7, 0x28
                mu64BytesDone = 0;              // std r3(=0), 0x38
                mpContext     = lpContext;     // *(a1+20) = a6

                if (!lpfnComplete)
                    lpfnComplete = &DefaultCompletion;
                mpfnComplete = lpfnComplete;    // *(a1+24) = a5
                mpfnDoIo = &AsyncOp::DoWrite;   // *(a1+68) = DoWrite

                mpDevice->InsertOp(this);
            }

            // -----------------------------------------------------------------------------
            // AsyncOp::Close @0x82BBFB48
            //
            // Fill a close op, take the stream's Device from stream+0x0C, set DoIo = DoClose,
            // and queue. The callback defaults to STUB. (Close stores the stream at +0x10.)
            //   a1=this a2=stream a3=callback a4=userData a5=priority
            // -----------------------------------------------------------------------------
            void AsyncOp::Close(Handle* lpFileHandle, CompletionCallback lpfnComplete,
                               void* lpContext, s32 liPriority)
            {
                ::MemoryBarrier();              // lwsync
                mStatus = OPSTATUS_PENDING;                   // *(a1+4) = 0
                ::MemoryBarrier();              // lwsync

                mbCancelled   = false;              // *(a1+8) = 0
                mbIsReadOp    = false;              // *(a1+9) = 0
                miPriority    = liPriority;     // *(a1+12) = a5
                mpFileHandle  = lpFileHandle; // *(a1+16) = a2
                mpContext     = lpContext;     // *(a1+20) = a4

                if (!lpfnComplete)
                    lpfnComplete = &DefaultCompletion;
                mpfnComplete  = lpfnComplete;   // *(a1+24) = a3
                mu64Position  = 0;              // *(a1+32) = 0
                mu64ByteCount = 0;              // *(a1+40) = 0
                mpBuffer      = 0;        // *(a1+48) = 0
                mu64BytesDone = 0;              // *(a1+56) = 0

                mpDevice = lpFileHandle->mpDevice; // *(a1+64) = *(a2+12)
                mpfnDoIo = &AsyncOp::DoClose;   // *(a1+68) = DoClose
                mpDevice->InsertOp(this);
            }

            // -----------------------------------------------------------------------------
            // AsyncOp::DoClose @0x82BBE010 -- destroy the result handle (mpFileHandle) and clear it.
            // -----------------------------------------------------------------------------
            AsyncOp::OpStatus AsyncOp::DoClose(AsyncOp* lpOp)
            {
                delete lpOp->mpFileHandle;      // Handle::`scalar deleting destructor'
                lpOp->mpFileHandle = 0;       // *(a1+16) = 0
                return OPSTATUS_COMPLETE;
            }

            // -----------------------------------------------------------------------------
            // AsyncOp::DoOpen @0x82BBFA70
            //
            // Allocate a Handle (40 bytes), construct it from the path copy, open flags and
            // device, free the path copy, then succeed (return 1) iff the handle opened
            // (mpFile); otherwise destroy the half-built handle and return -2.
            // -----------------------------------------------------------------------------
            AsyncOp::OpStatus AsyncOp::DoOpen(AsyncOp* lpOp)
            {
                void* lpRaw = gpFileSysAllocator->Alloc(sizeof(Handle), "rw::core::filesystem::Manager::Allocate", 0, 4, 0);
                // Handle::Handle(storage, path=mpBuffer(+0x30), openFlags=(u32)mu64Position(+0x24),
                //                device=mpDevice(+0x40)). The asm `ld r5,0x20(r31)` passes the full
                // doubleword; big-endian byte +0x24 is the LOW 32 bits, which Open stored via
                // `std clrldi(r5,32)` = the open-flags arg zero-extended (high word 0, low word = flags).
                // The non-throwing placement new tests the storage itself (ARTIST 0x82BBFAB8:
                // `cmplwi r3,0` ... `li r3,0`).
                lpOp->mpFileHandle = new (lpRaw) Handle(static_cast<const char*>(lpOp->mpBuffer),
                                                        static_cast<u32>(lpOp->mu64Position),
                                                        lpOp->mpDevice);

                gpFileSysAllocator->Free(lpOp->mpBuffer, 0);  // free the path copy (a1[12] == +0x30)

                if (lpOp->mpFileHandle->mpFile)  // *(v4+8)
                    return OPSTATUS_COMPLETE;
                delete lpOp->mpFileHandle;      // still null-tested: ARTIST 0x82BBFB10
                lpOp->mpFileHandle = 0;         // a1[4] = 0
                return OPSTATUS_FAILED;
            }

            // The linked Handle owns the file position. Reload the handle after
            // each virtual DeviceDriver call, as ARTIST does in both transfers.
            AsyncOp::OpStatus AsyncOp::DoRead(AsyncOp* lpOp)
            {
                OpStatus lStatus = OPSTATUS_PENDING;

                Handle* lpHandle = lpOp->mpFileHandle;
                const u64 lu64Position = lpHandle->mpDriver->Seek(
                    lpHandle->mpFile, lpOp->mu64Position, 0,
                    lpHandle->mpOwnerHandle->mpDevice->GetDriver(), lpHandle->mpOwnerHandle->mpFile);
                lpOp->mpFileHandle->mpOwnerHandle->mu64Position = lu64Position;

                const u32 luMaxRead = lpOp->mpFileHandle->mpDriver->GetMaxReadSize();
                const u32 luMaxManagerRead = static_cast<u32>(gpFileSysManager->miMaxReadSize);
                u32 luRequest = static_cast<u32>(lpOp->mu64ByteCount < luMaxRead ? lpOp->mu64ByteCount : luMaxRead);
                if (luRequest >= luMaxManagerRead)
                    luRequest = luMaxManagerRead;

                lpHandle = lpOp->mpFileHandle;
                const u32 luRead = lpHandle->mpDriver->Read(
                    lpHandle->mpFile, lpOp->mpBuffer, luRequest,
                    lpHandle->mpOwnerHandle->mpDevice->GetDriver(), lpHandle->mpOwnerHandle->mpFile);
                lpOp->mpBuffer = static_cast<u8*>(lpOp->mpBuffer) + luRead;
                lpOp->mu64Position += luRead;
                lpOp->mu64ByteCount -= luRead;
                lpOp->mu64BytesDone += luRead;

                if (lpOp->mu64ByteCount > 0 && luRead == luRequest)
                    lpOp->mpDevice->InsertOp(lpOp);
                else
                    lStatus = OPSTATUS_COMPLETE;
                return lStatus;
            }

            AsyncOp::OpStatus AsyncOp::DoWrite(AsyncOp* lpOp)
            {
                Handle* lpHandle = lpOp->mpFileHandle;
                const u64 lu64Position = lpHandle->mpDriver->Seek(
                    lpHandle->mpFile, lpOp->mu64Position, 0,
                    lpHandle->mpOwnerHandle->mpDevice->GetDriver(), lpHandle->mpOwnerHandle->mpFile);
                lpOp->mpFileHandle->mpOwnerHandle->mu64Position = lu64Position;

                lpHandle = lpOp->mpFileHandle;
                const u32 luWritten = lpHandle->mpDriver->Write(
                    lpHandle->mpFile, lpOp->mpBuffer, static_cast<u32>(lpOp->mu64ByteCount),
                    lpHandle->mpOwnerHandle->mpDevice->GetDriver(), lpHandle->mpOwnerHandle->mpFile);
                lpOp->mu64ByteCount = luWritten;
                Handle* lpOwner = lpOp->mpFileHandle->mpOwnerHandle;
                lpOwner->mu64Position = lpOp->mu64ByteCount + lpOwner->mu64Position;

                lpHandle = lpOp->mpFileHandle;
                lpOp->mpFileHandle->mu64Size = lpHandle->mpDriver->GetSize(lpHandle->mpFile);
                lpOp->mu64BytesDone += lpOp->mu64ByteCount;
                return OPSTATUS_COMPLETE;
            }

            // -----------------------------------------------------------------------------
            // AsyncOp::GetResultHandle @0x82BBE0C0 -- block on the op, then return the result
            // handle (mpFileHandle). The Device to wait on is the op's device, redirected to the
            // default device when that device is externally pumped (Device+0x06 != 0).
            // -----------------------------------------------------------------------------
            Handle* AsyncOp::GetResultHandle() const
            {
                Device* lpDevice = mpDevice;             // *(a1+64)
                if (lpDevice->IsExternalThread())        // *(v2+6)
                    lpDevice = gpFileSysManager->mpDefaultDevice; // *(off_8327F078+10)
                lpDevice->Wait(this, EA::Thread::kTimeoutNone);
                return mpFileHandle;                         // *(a1+16)
            }

            // -----------------------------------------------------------------------------
            // AsyncOp::GetResultSize @0x82BBE118 -- block on the op, return the bytes done.
            // -----------------------------------------------------------------------------
            u64 AsyncOp::GetResultSize() const
            {
                Device* lpDevice = mpDevice;             // *(a1+64)
                if (lpDevice->IsExternalThread())
                    lpDevice = gpFileSysManager->mpDefaultDevice;
                lpDevice->Wait(this, EA::Thread::kTimeoutNone);
                return mu64BytesDone;                    // *(a1+56)
            }

            // -----------------------------------------------------------------------------
            // AsyncOp::GetStatus -- unless the timeout is immediate, block on the
            // op until it completes or the absolute timeout passes (the timeout is handed
            // straight through to Device::Wait); return the current result/status
            // with lwsync fences.
            // -----------------------------------------------------------------------------
            AsyncOp::OpStatus AsyncOp::GetStatus(const EA::Thread::ThreadTime& lTimeoutAbsolute)
            {
                ::MemoryBarrier();          // lwsync
                if (lTimeoutAbsolute != EA::Thread::kTimeoutImmediate)
                {
                    Device* lpDevice = mpDevice;         // *(a1+64)
                    if (lpDevice->IsExternalThread())
                        lpDevice = gpFileSysManager->mpDefaultDevice;
                    lpDevice->Wait(this, lTimeoutAbsolute);
                }
                ::MemoryBarrier();          // lwsync
                return mStatus;            // *(a1+4)
            }

            // -----------------------------------------------------------------------------
            // AsyncOp::SetPriority @0x82BBFD80 -- if the priority actually changes, re-queue
            // via Device::ChangeOpPriority (redirecting to the default device when shared).
            // An unchanged priority leaves the current queue entry alone.
            // -----------------------------------------------------------------------------
            void AsyncOp::SetPriority(s32 liPriority)
            {
                if (miPriority != liPriority)            // *(result+12) != a2
                {
                    Device* lpDevice = mpDevice;         // *(result+64)
                    if (lpDevice->IsExternalThread())    // *(v2+6)
                        lpDevice = gpFileSysManager->mpDefaultDevice;
                    lpDevice->ChangeOpPriority(this, liPriority);
                }
            }
        }
    }
}
