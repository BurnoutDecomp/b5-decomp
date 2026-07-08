// =====================================================================================
// rw::core::filesys -- async-op list (intrusive) + GetSize accessor.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for these TUs.
//
//   rw::core::filesys::AsyncOpList::InsertAfter @0x82BBDE70  (template <AsyncOp> inst)
//   rw::core::filesys::AsyncOpList::Remove      @0x82BBDED0  (template <AsyncOp> inst)
//   rw::core::filesys::GetSize                  @0x82BBD700
//
// The list head is { mpHead@+0, mpTail@+4, muCount@+8 }; nodes link forward via
// AsyncOp::mpNext (node +0). The bodies are store-for-store ports of the pseudocode,
// with the raw _DWORD word-indices resolved to named members:
//   result/a1 -> AsyncOpList*   result[0]==mpHead  result[1]==mpTail  result[2]==muCount
//   a3 -> the node being inserted               a2 -> the predecessor / target node
// =====================================================================================

#include "SDKs/EATech/rwcore/filesys/device.h"  // Device / Manager / Allocator (AsyncOp dispatches here)

#include <cstring>   // memcpy
#include <cstdint>   // intptr_t
#include <new>       // placement new (Handle construction over allocator storage)
#include <windows.h> // MemoryBarrier (host mapping of the X360 lwsync)

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

            // &unk_82181154 -- the kTimeoutNone constant the result accessors hand to
            // Device::Wait as its timeout pointer (0xFFFFFFFF == block forever), mirroring
            // the KU_TIMEOUT_NONE pattern in device.cpp.
            static const u32 KU_TIMEOUT_NONE = 0xFFFFFFFFu;

            // GetSize @0x82BBD700:  ld r3, 0x18(r3); blr -- return the 64-bit size at +0x18.
            u64 GetSize(const AsyncOp* lpOp)
            {
                return lpOp->mu64Size;
            }

            // InsertAfter @0x82BBDE70.
            //   if (a2) { if (!a2->next) tail = a3; a3->next = a2->next; a2->next = a3; ++count; }
            //   else    { a3->next = head; v3 = count; head = a3; count = v3 + 1;
            //             if (!a3->next) tail = a3; }
            AsyncOpList* AsyncOpList::InsertAfter(AsyncOp* lpAfter, AsyncOp* lpNode)
            {
                if (lpAfter)
                {
                    if (!lpAfter->mpNext)
                        mpTail = lpNode;
                    lpNode->mpNext = lpAfter->mpNext;
                    lpAfter->mpNext = lpNode;
                    ++muCount;
                }
                else
                {
                    lpNode->mpNext = mpHead;
                    u32 luPrevCount = muCount;
                    mpHead = lpNode;
                    muCount = luPrevCount + 1;
                    if (!lpNode->mpNext)
                        mpTail = lpNode;
                }
                return this;
            }

            // Remove @0x82BBDED0.
            // lpFrom (X360 r5/a3) is a PREDECESSOR NODE cursor, not a pointer-to-link: the
            // walk follows lpFrom->mpNext (the asm's `*(r5)`) looking for lpNode. The head
            // case (lpNode == mpHead) is handled separately against the head/tail fields.
            //
            //   r10 = mpHead;
            //   if (lpNode != mpHead) {                       // 0x82BBDF1C
            //       if (mpHead) {
            //           if (!lpFrom) lpFrom = mpHead;          // start scan at head node
            //           if (lpFrom->mpNext) {
            //               while (lpFrom->mpNext && lpFrom->mpNext != lpNode)  // DF3C
            //                   lpFrom = lpFrom->mpNext;
            //               if (lpFrom->mpNext == lpNode) {    // DF58/DF64
            //                   --muCount; result = 1;
            //                   lpFrom->mpNext = lpNode->mpNext;   // DF7C/DF80
            //                   if (lpNode == mpTail) mpTail = lpFrom;  // DF84/DF90
            //               }
            //           }
            //       }
            //   } else {                                      // lpNode == mpHead
            //       result = 1; --muCount;
            //       if (lpNode == mpTail) { mpTail = 0; mpHead = 0; }   // DF04/DF08
            //       else mpHead = lpNode->mpNext;              // DF10/DF14
            //   }
            //   if (result) lpNode->mpNext = 0;               // DF9C
            int AsyncOpList::Remove(AsyncOp* lpNode, AsyncOp* lpFrom)
            {
                int liResult = 0;

                if (lpNode != mpHead)
                {
                    if (mpHead)
                    {
                        if (!lpFrom)
                            lpFrom = mpHead;
                        if (lpFrom->mpNext)
                        {
                            while (lpFrom->mpNext && lpFrom->mpNext != lpNode)
                                lpFrom = lpFrom->mpNext;

                            if (lpFrom->mpNext && lpFrom->mpNext == lpNode)
                            {
                                liResult = 1;
                                --muCount;
                                lpFrom->mpNext = lpNode->mpNext;
                                if (lpNode == mpTail)
                                    mpTail = lpFrom;
                            }
                        }
                    }
                }
                else
                {
                    liResult = 1;
                    AsyncOp* lpTail = mpTail;
                    --muCount;
                    if (lpNode == lpTail)
                    {
                        mpTail = nullptr;
                        mpHead = nullptr;
                    }
                    else
                    {
                        mpHead = lpNode->mpNext;
                    }
                }

                if (liResult)
                    lpNode->mpNext = nullptr;

                return liResult;
            }

            // =============================================================================
            // rw::core::filesys::AsyncOp -- the async-op class itself.
            // =============================================================================

            // -----------------------------------------------------------------------------
            // AsyncOp::AsyncOp @0x82BBD3E0 -- zero/-3 init of the whole 0x48-byte object.
            //   miResult = -3; mpfnComplete = STUB; everything else 0.
            // -----------------------------------------------------------------------------
            AsyncOp::AsyncOp()
                : mpNext(nullptr)               // *result = 0
                , miResult(-3)                  // *(result+4) = -3
                , mbReserved08(0)               // *(result+8) = 0
                , mbIsReadOp(0)                 // *(result+9) = 0
                , miPriority(0)                 // *(result+12) = 0
                , mpStream(nullptr)             // *(result+16) = 0
                , mUserData(0)                  // *(result+20) = 0
                , mpfnComplete(&DefaultCompletion) // *(result+24) = STUB
                , muReserved1C(0)
                , mu64Position(0)               // *(result+32) = 0
                , mu64ByteCount(0)              // *(result+40) = 0
                , mpBuffer(nullptr)             // *(result+48) = 0
                , muReserved34(0)
                , mu64BytesDone(0)              // *(result+56) = 0
                , mpDevice(nullptr)             // *(result+64) = 0
                , mpfnDoIo(nullptr)             // *(result+68) = 0
            {
            }

            // -----------------------------------------------------------------------------
            // AsyncOp::~AsyncOp @0x82BBD430 -- lwsync, then zero +0x10/+0x14/+0x30/+0x40
            // and the link +0x00 (the only fields the asm clears).
            // -----------------------------------------------------------------------------
            AsyncOp::~AsyncOp()
            {
                ::MemoryBarrier();      // lwsync
                mpStream  = nullptr;    // result[4] (+0x10) = 0
                mUserData = 0;          // result[5] (+0x14) = 0
                mpBuffer  = nullptr;    // result[12] (+0x30) = 0
                mpDevice  = nullptr;    // result[16] (+0x40) = 0
                mpNext    = nullptr;    // *result (+0x00) = 0
            }

            // -----------------------------------------------------------------------------
            // AsyncOp::Open @0x82BC00C0
            //
            // Fill an open op (read flag clear), resolve the path's Device, allocate a copy
            // of the path into mpBuffer, default the callback to STUB, set DoIo = DoOpen, and
            // queue the op on its device (the default device when the device is "shared").
            //   a1=this a2=path a3=flags a4=callback a5=userData a6=priority
            // -----------------------------------------------------------------------------
            s32 AsyncOp::Open(const char* lpcPath, u32 luFlags, CompletionCallback lpfnComplete,
                              uintptr_t luUserData, s32 liPriority)
            {
                ::MemoryBarrier();              // lwsync
                miResult = 0;                   // *(a1+4) = 0
                ::MemoryBarrier();              // lwsync

                mbReserved08  = 0;              // *(a1+8) = 0
                mbIsReadOp    = 0;              // *(a1+9) = 0
                miPriority    = liPriority;     // stw r8, 0xC
                mpStream      = nullptr;        // stw r11(=0), 0x10
                mUserData     = luUserData;     // stw r7, 0x14
                mu64Position  = luFlags;        // std clrldi(r5,32), 0x20  (flags zero-extended)
                mu64ByteCount = 0;              // std r11(=0), 0x28
                mu64BytesDone = 0;              // std r11(=0), 0x38

                Device* lpDevice = Device::GetInstance(lpcPath, nullptr);
                mpDevice = lpDevice;            // *(a1+64) = Instance

                // Allocate a copy of the path (length incl. NUL) and copy it into mpBuffer.
                const char* lpEnd = lpcPath;
                while (*lpEnd++)
                    ;
                u32 luLen = static_cast<u32>(lpEnd - lpcPath); // (v12 - a2) incl. NUL
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
                return lpQueue->InsertOp(this);
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
            s32 AsyncOp::Read(void* lpStream, void* lpBuffer, u64 lu64Position, u64 lu64Count,
                              CompletionCallback lpfnComplete, uintptr_t luUserData, s32 liPriority)
            {
                IoStream* lpStrm = static_cast<IoStream*>(lpStream);

                ::MemoryBarrier();              // lwsync
                miResult = 0;                   // *(a1+4) = 0
                ::MemoryBarrier();              // lwsync

                mbReserved08  = 0;              // *(a1+8) = 0
                mbIsReadOp    = 1;              // *(a1+9) = 1
                mpDevice      = lpStrm->mpDevice; // v10 = *(a2+12); *(a1+64) = v10
                miPriority    = liPriority;     // *(a1+12) = a7
                mpStream      = static_cast<OpStream*>(lpStream); // *(a1+16) = a2
                mu64Position  = lu64Position;   // std r6, 0x20
                mpBuffer      = lpBuffer;       // *(a1+48) = a3  (stw r5, 0x30)
                mu64ByteCount = lu64Count;      // std r7, 0x28
                mu64BytesDone = 0;              // std r3(=0), 0x38
                mUserData     = luUserData;     // *(a1+20) = a6

                if (!lpfnComplete)
                    lpfnComplete = &DefaultCompletion;
                mpfnComplete = lpfnComplete;    // *(a1+24) = a5
                mpfnDoIo = &AsyncOp::DoRead;    // *(a1+68) = DoRead

                return mpDevice->InsertOp(this);
            }

            s32 AsyncOp::Write(void* lpStream, void* lpBuffer, u64 lu64Position, u64 lu64Count,
                               CompletionCallback lpfnComplete, uintptr_t luUserData, s32 liPriority)
            {
                IoStream* lpStrm = static_cast<IoStream*>(lpStream);

                ::MemoryBarrier();              // lwsync
                miResult = 0;                   // *(a1+4) = 0
                ::MemoryBarrier();              // lwsync

                mbReserved08  = 0;              // *(a1+8) = 0
                mbIsReadOp    = 0;              // *(a1+9) = 0
                mpDevice      = lpStrm->mpDevice; // v10 = *(a2+12); *(a1+64) = v10
                miPriority    = liPriority;     // *(a1+12) = a7
                mpStream      = static_cast<OpStream*>(lpStream); // *(a1+16) = a2
                mu64Position  = lu64Position;   // std r6, 0x20
                mpBuffer      = lpBuffer;       // *(a1+48) = a3  (stw r5, 0x30)
                mu64ByteCount = lu64Count;      // std r7, 0x28
                mu64BytesDone = 0;              // std r3(=0), 0x38
                mUserData     = luUserData;     // *(a1+20) = a6

                if (!lpfnComplete)
                    lpfnComplete = &DefaultCompletion;
                mpfnComplete = lpfnComplete;    // *(a1+24) = a5
                mpfnDoIo = &AsyncOp::DoWrite;   // *(a1+68) = DoWrite

                return mpDevice->InsertOp(this);
            }

            // -----------------------------------------------------------------------------
            // AsyncOp::Close @0x82BBFB48
            //
            // Fill a close op, take the stream's Device from stream+0x0C, set DoIo = DoClose,
            // and queue. The callback defaults to STUB. (Close stores the stream at +0x10.)
            //   a1=this a2=stream a3=callback a4=userData a5=priority
            // -----------------------------------------------------------------------------
            s32 AsyncOp::Close(void* lpStream, CompletionCallback lpfnComplete,
                               uintptr_t luUserData, s32 liPriority)
            {
                IoStream* lpStrm = static_cast<IoStream*>(lpStream);

                ::MemoryBarrier();              // lwsync
                miResult = 0;                   // *(a1+4) = 0
                ::MemoryBarrier();              // lwsync

                mbReserved08  = 0;              // *(a1+8) = 0
                mbIsReadOp    = 0;              // *(a1+9) = 0
                miPriority    = liPriority;     // *(a1+12) = a5
                mpStream      = static_cast<OpStream*>(lpStream); // *(a1+16) = a2
                mUserData     = luUserData;     // *(a1+20) = a4

                if (!lpfnComplete)
                    lpfnComplete = &DefaultCompletion;
                mpfnComplete  = lpfnComplete;   // *(a1+24) = a3
                mu64Position  = 0;              // *(a1+32) = 0
                mu64ByteCount = 0;              // *(a1+40) = 0
                mpBuffer      = nullptr;        // *(a1+48) = 0
                mu64BytesDone = 0;              // *(a1+56) = 0

                Device* lpDevice = lpStrm->mpDevice; // v6 = *(a2+12)
                mpfnDoIo = &AsyncOp::DoClose;   // *(a1+68) = DoClose
                mpDevice = lpDevice;            // *(a1+64) = v6
                return lpDevice->InsertOp(this);
            }

            // -----------------------------------------------------------------------------
            // AsyncOp::DoClose @0x82BBE010 -- destroy the result handle (mpStream) and clear it.
            // -----------------------------------------------------------------------------
            s32 AsyncOp::DoClose(AsyncOp* lpOp)
            {
                Handle* lpHandle = reinterpret_cast<Handle*>(lpOp->mpStream); // *(a1+16)
                if (lpHandle)
                    HandleScalarDeletingDtor(lpHandle, 1);
                lpOp->mpStream = nullptr;       // *(a1+16) = 0
                return 1;
            }

            // -----------------------------------------------------------------------------
            // AsyncOp::DoOpen @0x82BBFA70
            //
            // Allocate a Handle (40 bytes), construct it from the path copy / position-hi /
            // device, free the path copy, then succeed (return 1) iff the handle opened
            // (mbIsOpen); otherwise destroy the half-built handle and return -2.
            // -----------------------------------------------------------------------------
            s32 AsyncOp::DoOpen(AsyncOp* lpOp)
            {
                void* lpRaw = gpFileSysAllocator->Alloc(0x28, "rw::core::filesystem::Manager::Allocate", 0, 4, 0);
                Handle* lpHandle = nullptr;
                if (lpRaw)
                {
                    // Handle::Handle(storage, path=mpBuffer(+0x30), openFlags=(u32)mu64Position(+0x24),
                    //                device=mpDevice(+0x40)). The asm `ld r5,0x20(r31)` passes the full
                    // doubleword; big-endian byte +0x24 is the LOW 32 bits, which Open stored via
                    // `std clrldi(r5,32)` = the open-flags arg zero-extended (high word 0, low word = flags).
                    lpHandle = new (lpRaw) Handle(static_cast<const char*>(lpOp->mpBuffer),
                                                  static_cast<u32>(lpOp->mu64Position),
                                                  lpOp->mpDevice);
                }
                lpOp->mpStream = reinterpret_cast<OpStream*>(lpHandle); // a1[4] (+0x10) = v3

                gpFileSysAllocator->Free(lpOp->mpBuffer, 0);  // free the path copy (a1[12] == +0x30)

                Handle* lpResult = reinterpret_cast<Handle*>(lpOp->mpStream);
                if (lpResult->mbIsOpen)         // *(v4+8)
                    return 1;
                if (lpResult)
                    HandleScalarDeletingDtor(lpResult, 1);
                lpOp->mpStream = nullptr;       // a1[4] = 0
                return -2;
            }

            // -----------------------------------------------------------------------------
            // AsyncOp::DoRead @0x82BBFBB8
            //
            // The read transfer callback. Walk the stream's IO interface: take the leading
            // position (slot 0x1C), clamp the requested count against the IO size ceiling
            // (slot 0x2C) and the Manager's per-read cap, perform the read (slot 0x14), then
            // advance buffer/position and decrement the remaining count. Returns 1 when the
            // op is complete (nothing left, or a short read); otherwise re-queues itself.
            //
            // The Stream / StreamIo layout is the X360-proven minimal shape modelled in
            // asyncop.h; every offset and vtable slot is taken from the asm.
            // -----------------------------------------------------------------------------
            s32 AsyncOp::DoRead(AsyncOp* lpOp)
            {
                s32 liDone = 0;

                IoStream* lpStream = reinterpret_cast<IoStream*>(lpOp->mpStream);
                StreamIo* lpIo = lpStream->mpIo;
                StreamContext* lpCtx = lpStream->mpContext;
                void* lpDriver = lpCtx->mpDevice->GetDriver();   // *(*(ctx+0xC))+0x140

                // Leading position (slot 0x1C); stash into the context.
                u64 lu64Lead = lpIo->mpVTable->mpfnSlot1C(lpIo, lpStream->mDriverKey,
                                                          lpOp->mu64Position, 0,
                                                          lpDriver, lpCtx->mDriverKey);
                lpCtx->mu64LeadPos = lu64Lead;

                // Clamp the remaining count to the IO size ceiling and the per-read cap.
                u32 luSizeCeil = lpIo->mpVTable->mpfnSlot2C(lpIo);
                u64 lu64Want = lpOp->mu64ByteCount;
                if (lu64Want >= luSizeCeil)
                    lu64Want = luSizeCeil;
                u32 luWant = static_cast<u32>(lu64Want);
                u32 luCap = static_cast<u32>(gpFileSysManager->miMaxReadSize); // *(off_8327F078+0x24)
                if (luWant >= luCap)
                    luWant = luCap;

                // The read transfer (slot 0x14) returns the bytes actually read.
                s32 liBytes = lpIo->mpVTable->mpfnSlot14(lpIo, lpStream->mDriverKey,
                                                         lpOp->mpBuffer, luWant,
                                                         lpDriver, lpCtx->mDriverKey);
                u64 lu64Bytes = static_cast<u32>(liBytes);

                // Advance: buffer += bytes, position += bytes, remaining -= bytes,
                // bytesDone += bytes.
                lpOp->mpBuffer       = static_cast<u8*>(lpOp->mpBuffer) + liBytes;
                u64 lu64Remaining    = lpOp->mu64ByteCount - lu64Bytes;
                lpOp->mu64Position   = lpOp->mu64Position + lu64Bytes;
                lpOp->mu64ByteCount  = lu64Remaining;
                lpOp->mu64BytesDone  = lpOp->mu64BytesDone + lu64Bytes;

                // cmpldi remaining,0 + ble -> "remaining == 0" (unsigned), OR a short read.
                if (lu64Remaining == 0 || static_cast<u32>(liBytes) != luWant)
                    liDone = 1;
                else
                    lpOp->mpDevice->InsertOp(lpOp);   // more to read -> re-queue

                return liDone;
            }

            // -----------------------------------------------------------------------------
            // AsyncOp::DoWrite @0x82BBD450
            //
            // The write transfer callback. Take the leading position (slot 0x1C), write the
            // whole buffer (slot 0x18), advance the context's position, fetch the trailing
            // position (slot 0x20) into the stream, and accumulate the written bytes. Always
            // returns 1 (a write completes in one shot).
            // -----------------------------------------------------------------------------
            s32 AsyncOp::DoWrite(AsyncOp* lpOp)
            {
                IoStream* lpStream = reinterpret_cast<IoStream*>(lpOp->mpStream);
                StreamIo* lpIo = lpStream->mpIo;
                StreamContext* lpCtx = lpStream->mpContext;
                void* lpDriver = lpCtx->mpDevice->GetDriver();

                // Leading position (slot 0x1C) -> context.
                u64 lu64Lead = lpIo->mpVTable->mpfnSlot1C(lpIo, lpStream->mDriverKey,
                                                          lpOp->mu64Position, 0,
                                                          lpDriver, lpCtx->mDriverKey);
                lpCtx->mu64LeadPos = lu64Lead;

                // The write transfer (slot 0x18) returns the bytes written.
                s32 liBytes = lpIo->mpVTable->mpfnSlot18(lpIo, lpStream->mDriverKey,
                                                         lpOp->mpBuffer, lpOp->mu64ByteCount,
                                                         lpDriver, lpCtx->mDriverKey);
                u64 lu64Bytes = static_cast<u32>(liBytes);
                lpOp->mu64ByteCount = lu64Bytes;                 // *(a1+40) = bytes
                lpCtx->mu64LeadPos = lpCtx->mu64LeadPos + lu64Bytes; // ctx position += bytes

                // Trailing position (slot 0x20) -> stream.
                u64 lu64Tail = lpIo->mpVTable->mpfnSlot20(lpIo, lpStream->mDriverKey);
                lpStream->mu64WritePos = lu64Tail;               // *(stream+0x18) = result

                lpOp->mu64BytesDone = lpOp->mu64BytesDone + lpOp->mu64ByteCount;
                return 1;
            }

            // -----------------------------------------------------------------------------
            // AsyncOp::GetResultHandle @0x82BBE0C0 -- block on the op, then return the result
            // handle (mpStream). The Device to wait on is the op's device, redirected to the
            // default device when that device is externally pumped (Device+0x06 != 0).
            // -----------------------------------------------------------------------------
            void* AsyncOp::GetResultHandle()
            {
                Device* lpDevice = mpDevice;             // *(a1+64)
                if (lpDevice->IsExternalThread())        // *(v2+6)
                    lpDevice = gpFileSysManager->mpDefaultDevice; // *(off_8327F078+10)
                lpDevice->Wait(this, &KU_TIMEOUT_NONE);  // &unk_82181154
                return mpStream;                         // *(a1+16)
            }

            // -----------------------------------------------------------------------------
            // AsyncOp::GetResultSize @0x82BBE118 -- block on the op, return the bytes done.
            // -----------------------------------------------------------------------------
            u64 AsyncOp::GetResultSize()
            {
                Device* lpDevice = mpDevice;             // *(a1+64)
                if (lpDevice->IsExternalThread())
                    lpDevice = gpFileSysManager->mpDefaultDevice;
                lpDevice->Wait(this, &KU_TIMEOUT_NONE);
                return mu64BytesDone;                    // *(a1+56)
            }

            // -----------------------------------------------------------------------------
            // AsyncOp::GetStatus @0x82BBE058 -- if *lpbWantWait, block on the op; return the
            // current result/status (miResult), publishing it with lwsync fences.
            // -----------------------------------------------------------------------------
            s32 AsyncOp::GetStatus(const s32* lpbWantWait)
            {
                ::MemoryBarrier();          // lwsync
                if (*lpbWantWait)
                {
                    Device* lpDevice = mpDevice;         // *(a1+64)
                    if (lpDevice->IsExternalThread())
                        lpDevice = gpFileSysManager->mpDefaultDevice;
                    lpDevice->Wait(this);                // 2-arg form (default timeout)
                }
                ::MemoryBarrier();          // lwsync
                return miResult;            // *(a1+4)
            }

            // -----------------------------------------------------------------------------
            // AsyncOp::SetPriority @0x82BBFD80 -- if the priority actually changes, re-queue
            // via Device::ChangeOpPriority (redirecting to the default device when shared).
            // The X360 returns `this` (r3) when unchanged, else the ChangeOpPriority result.
            // -----------------------------------------------------------------------------
            s32 AsyncOp::SetPriority(s32 liPriority)
            {
                if (miPriority != liPriority)            // *(result+12) != a2
                {
                    Device* lpDevice = mpDevice;         // *(result+64)
                    if (lpDevice->IsExternalThread())    // *(v2+6)
                        lpDevice = gpFileSysManager->mpDefaultDevice;
                    return lpDevice->ChangeOpPriority(this, liPriority);
                }
                return static_cast<s32>(reinterpret_cast<intptr_t>(this)); // r3 == this
            }
        }
    }
}
