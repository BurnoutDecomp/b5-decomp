// =====================================================================================
// rw::core::filesys::Device -- the RenderWare core filesystem device scheduler.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU.
//
//   rw::core::filesys::Device::Device                 @0x82BBE170  (ctor)
//   rw::core::filesys::Device::~Device                @0x82BBE208  (dtor)
//   rw::core::filesys::Device::`scalar deleting dtor'  @0x82BBE3B0
//   rw::core::filesys::Device::Start                  @0x82BBF608
//   rw::core::filesys::Device::ThreadEntry            @0x82BBF3E8
//   rw::core::filesys::Device::InsertOp               @0x82BBF6B0
//   rw::core::filesys::Device::ChangeOpPriority       @0x82BBF7E8
//   rw::core::filesys::Device::CheckForOptimalReadOp  @0x82BBED28
//   rw::core::filesys::Device::GetInstance            @0x82BBEAC0
//
// The X360 EA::Thread::Mutex embedded at +0x18 / +0xB0 is, on the host, a Win32
// CRITICAL_SECTION (the disassembly labels both fields "CriticalSection"); the homed
// eathread_condition.cpp models the very same X360 mutex the same way. So:
//   EA::Thread::Mutex::Mutex(p,0,1) -> InitializeCriticalSection
//   EA::Thread::Mutex::Lock(p,&a)   -> EnterCriticalSection
//   EA::Thread::Mutex::Unlock(p)    -> LeaveCriticalSection
//   EA::Thread::Mutex::~Mutex(p)    -> DeleteCriticalSection   (the disasm's STUB calls)
//
// The two EA::Thread::Condition and the EA::Thread::Thread are the project's homed types.
// =====================================================================================

#include "SDKs/EATech/rwcore/filesys/device.h"

#include <windows.h> // MemoryBarrier (the host mapping of the X360 lwsync)
#include <cstring>   // strchr, strncpy

namespace rw
{
    namespace core
    {
        namespace filesys
        {
            // X360 off_8327F078 -- the global filesys Manager. Owned by the (not-yet-homed)
            // Manager TU; declared extern in device.h. A single definition is provided here
            // so this TU links standalone; the Manager TU will own the real instance.
            Manager* gpFileSysManager = nullptr;

            namespace
            {
                // The X360 hands &dword_82181154 / &unk_82181154 -- the EAThread
                // kTimeoutNone constant (0xFFFFFFFF == INFINITE) -- as the timeout pointer
                // to Condition::Wait and as the spin/attributes pointer to Mutex::Lock. The
                // homed Condition::Wait reads 0xFFFFFFFF as "block forever"; the host
                // EnterCriticalSection ignores it. Modelled as a file-scope const whose
                // address is handed to Wait, exactly as the asm hands &constant.
                const u32 KU_TIMEOUT_NONE = 0xFFFFFFFFu;
            }

            // -----------------------------------------------------------------------------
            // Device::Device @0x82BBE170
            //
            // Clears the registry link/flags/op-list, latches the external-thread bit
            // (lbExternalThread & 1), constructs both mutexes and both conditions and the
            // (unstarted) worker thread, then stores the driver and zeroes the read budget.
            // The X360 ctor calls EA::Thread::Mutex::Mutex(p,0,1) / Condition(p,0,1) and an
            // inlined Thread() (the disasm names it VideoRenderable::~VideoRenderable, a
            // symbol alias for the single `stw 0` that is Thread's default ctor).
            // -----------------------------------------------------------------------------
            Device::Device(DeviceDriver* lpDriver, bool lbExternalThread)
                : mpNextRegistered(nullptr)   // *a1 = 0
                , mbThreadRunning(0)           // *(a1+4) = 0
                , mbStopRequested(0)           // *(a1+5) = 0
                , mbExternalThread(lbExternalThread ? 1 : 0)  // *(a1+6) = a3 & 1
                , mWork(0, true)               // EA::Thread::Condition::Condition(a1+72, 0, 1)
                , mCompletion(0, true)         // EA::Thread::Condition::Condition(a1+224, 0, 1)
                , mpDriver(lpDriver)           // *(a1+320) = a2
                , mu64ReadBudget(0)            // *(a1+328) = 0
            {
                // *(a1+8) = *(a1+12) = *(a1+16) = 0 -- empty op list.
                mOpList.mpHead  = nullptr;
                mOpList.mpTail  = nullptr;
                mOpList.muCount = 0;

                // EA::Thread::Mutex::Mutex(a1+24, 0, 1) and (a1+176, 0, 1).
                ::InitializeCriticalSection(&mLock);
                ::InitializeCriticalSection(&mCompletionLock);
            }

            // -----------------------------------------------------------------------------
            // Device::~Device @0x82BBE208
            //
            // If the worker is running: when this device owns its thread (not external),
            // request stop, wake the work condition, and spin-sleep until the worker clears
            // mbThreadRunning; then Close the driver. Tear down the conditions/thread/mutexes
            // (the disasm's STUB(a1+0xB0)/STUB(a1+0x18) are the two Mutex destructors), then
            // walk-and-clear the residual op list and zero the head/tail/count and link.
            // -----------------------------------------------------------------------------
            Device::~Device()
            {
                if (mbThreadRunning)
                {
                    if (!mbExternalThread)
                    {
                        mbStopRequested = 1;
                        mWork.Signal(false);            // Condition::Signal(a1+72, 0)
                        while (mbThreadRunning)
                        {
                            u32 luMs = 1;               // v9[0] = 1
                            EA::Thread::ThreadSleep(&luMs);
                        }
                    }
                    mpDriver->Close();                  // (*(driver+8))(driver)
                }

                mpDriver = nullptr;                     // *(a1+320) = 0

                // Destroy in the X360 order: completion condition, completion mutex, worker
                // thread, work condition, op-list mutex. The two embedded EA::Thread types
                // run their destructors automatically; the mutexes are DeleteCriticalSection.
                // (Member destructors fire in reverse-declaration order at scope exit, which
                // is the inverse of this list -- the observable teardown is identical: every
                // primitive is destroyed exactly once.)
                ::DeleteCriticalSection(&mCompletionLock);
                ::DeleteCriticalSection(&mLock);

                // Walk the residual op list, clearing each node's link until a null link is
                // found (the X360 stops on the first node whose mpNext is already null).
                AsyncOp* lpOp = mOpList.mpHead;
                if (lpOp)
                {
                    for (;;)
                    {
                        AsyncOp* lpNext = lpOp->mpNext;
                        if (!lpNext)
                            break;
                        lpOp->mpNext = nullptr;
                        lpOp = lpNext;
                    }
                }

                mOpList.mpTail   = nullptr;   // *(a1+12) = 0
                mOpList.mpHead   = nullptr;   // *(a1+8)  = 0
                mOpList.muCount  = 0;         // *(a1+16) = 0
                mpNextRegistered = nullptr;   // *a1      = 0
            }

            // -----------------------------------------------------------------------------
            // Device::Start @0x82BBF608
            //
            // Open the driver. If this device drives the pump externally (mbExternalThread),
            // just mark it running. Otherwise begin the worker thread on ThreadEntry(this)
            // with the Manager's thread parameters and global user-wrapper, then block
            // (1ms sleeps) until the worker sets mbThreadRunning.
            // -----------------------------------------------------------------------------
            int Device::Start()
            {
                int liResult = mpDriver->Open();   // (*(driver+4))(driver)

                if (mbExternalThread)
                {
                    mbThreadRunning = 1;           // *(a1+4) = 1
                }
                else
                {
                    // v3 = off_8327F078 (Manager*); v4 = sub_82B42D88() (the global runnable
                    // user-wrapper). Begin(thread, ThreadEntry, this, &mgr->params, wrapper).
                    EA::Thread::ThreadParameters* lpParams = &gpFileSysManager->mThreadParameters;
                    EA::Thread::RunnableFunctionUserWrapper lpWrapper =
                        EA::Thread::Thread::GetGlobalRunnableFunctionUserWrapper();  // sub_82B42D88

                    mThread.Begin(&Device::ThreadEntry, this, lpParams, lpWrapper);

                    while (!mbThreadRunning)
                    {
                        u32 luMs = 1;              // v5 = 1
                        EA::Thread::ThreadSleep(&luMs);
                    }
                    // result is the last ThreadSleep return in the asm; the function value is
                    // unused by every caller (InsertOp/Handle::Handle ignore it).
                    liResult = 0;
                }

                return liResult;
            }

            // -----------------------------------------------------------------------------
            // Device::InsertOp @0x82BBF6B0
            //
            // Under the op-list lock: auto-Start the worker if idle, then find the insertion
            // point that keeps the list ordered by priority (descending), with a secondary
            // read-coalescing tie-break when the Manager has position-sorting enabled and
            // both the new op and the candidate are reads. Insert after the found node and
            // signal the work condition.
            // -----------------------------------------------------------------------------
            int Device::InsertOp(AsyncOp* lpOp)
            {
                ::EnterCriticalSection(&mLock);     // Mutex::Lock(a1+24, &attrs)

                if (!mbThreadRunning)
                    Start();

                AsyncOp* lpAfter = nullptr;         // v5
                u64 lu64NewEnd = 0;                 // v6
                if (lpOp->mbIsReadOp)
                    lu64NewEnd = mpDriver->GetBlockSize(lpOp->mpStream->mpDriverKey) + lpOp->mu64Position;

                for (AsyncOp* lpCur = mOpList.mpHead; lpCur; lpCur = lpCur->mpNext)
                {
                    if (lpOp->miPriority > lpCur->miPriority
                        || (lpOp->miPriority == lpCur->miPriority
                            && gpFileSysManager->mbSortByPosition == 1
                            && lpOp->mbIsReadOp
                            && lpCur->mbIsReadOp
                            && lu64NewEnd < mpDriver->GetBlockSize(lpCur->mpStream->mpDriverKey) + lpCur->mu64Position))
                    {
                        break;
                    }
                    lpAfter = lpCur;
                }

                mOpList.InsertAfter(lpAfter, lpOp); // AsyncOp_::InsertAfter(a1+8, v5, a2)

                ::LeaveCriticalSection(&mLock);     // Mutex::Unlock(v3)
                return mWork.Signal(false);         // Condition::Signal(a1+72, 0)
            }

            // -----------------------------------------------------------------------------
            // Device::ChangeOpPriority @0x82BBF7E8
            //
            // Under the op-list lock: remove the op from the list; if it was present, set its
            // new priority and re-insert it at the right ordered position. Returns the
            // Mutex::Unlock result (the X360 r3 at the unlock call).
            // -----------------------------------------------------------------------------
            int Device::ChangeOpPriority(AsyncOp* lpOp, int liPriority)
            {
                ::EnterCriticalSection(&mLock);     // Mutex::Lock(a1+24, &attrs)

                if (mOpList.Remove(lpOp, nullptr))  // AsyncOp_::Remove(a1+8, a2, 0)
                {
                    lpOp->miPriority = liPriority;  // *(a2+12) = a3
                    InsertOp(lpOp);
                }

                ::LeaveCriticalSection(&mLock);     // Mutex::Unlock(v4)
                return 0;                           // LeaveCriticalSection has no value
            }

            // -----------------------------------------------------------------------------
            // Device::CheckForOptimalReadOp @0x82BBED28
            //
            // Given the op the worker just popped (lpOp), and only when it is a read op and
            // the Manager has position-sorting enabled: while the running read budget allows,
            // scan forward for a later same-stream read op whose end stays within the budget
            // ceiling; if one is found, splice lpOp back onto the list head, unlink the better
            // candidate, and service that one instead. Updates the running budget either way.
            // -----------------------------------------------------------------------------
            AsyncOp* Device::CheckForOptimalReadOp(AsyncOp* lpOp)
            {
                AsyncOp* lpChosen = lpOp;           // v3

                if (lpOp->mbIsReadOp && gpFileSysManager->mbSortByPosition == 1)
                {
                    u32 luCost = mpDriver->GetBlockSize(lpOp->mpStream->mpDriverKey);  // v5
                    u64 lu64Budget = mu64ReadBudget;                                   // v6
                    u64 lu64End = static_cast<u64>(luCost) + lpChosen->mu64Position;   // v2

                    if (lu64Budget && lu64End < lu64Budget)
                    {
                        AsyncOp* lpCur  = mOpList.mpHead;   // v7
                        AsyncOp* lpPrev = nullptr;          // v8
                        u64 lu64CandEnd = 0;                // v9
                        bool lbFound = false;               // v10

                        while (lpCur && lpCur->mbIsReadOp && lpCur->miPriority == lpChosen->miPriority)
                        {
                            lu64CandEnd = mpDriver->GetBlockSize(lpCur->mpStream->mpDriverKey) + lpCur->mu64Position;
                            if (mu64ReadBudget < lu64CandEnd)
                            {
                                lbFound = true;
                                break;
                            }
                            lpPrev = lpCur;
                            lpCur  = lpCur->mpNext;
                        }

                        if (lbFound)
                        {
                            // Splice lpChosen back onto the head (re-queue it), then unlink
                            // the chosen candidate lpCur to be serviced now.
                            lpChosen->mpNext = mOpList.mpHead;   // *v3 = *v11
                            u32 luCount = mOpList.muCount;       // v12
                            mOpList.mpHead = lpChosen;           // *v11 = v3
                            mOpList.muCount = luCount + 1;       // *(a1+16) = v12 + 1
                            if (!lpChosen->mpNext)
                                mOpList.mpTail = lpChosen;       // *(a1+12) = v3

                            lpChosen = lpCur;                    // v3 = v7
                            lu64End  = lu64CandEnd;              // v2 = v9
                            mOpList.Remove(lpCur, lpPrev);       // AsyncOp_::Remove(a1+8, v7, v8)
                        }
                    }

                    mu64ReadBudget = lu64End;   // *(a1+328) = v2
                }

                return lpChosen;
            }

            // -----------------------------------------------------------------------------
            // Device::ThreadEntry @0x82BBF3E8  (EAThread RunnableFunction)
            //
            // The worker loop. Mark running, take the op-list lock, then repeatedly:
            //   * pop the head op (inline unlink against head/tail/count),
            //   * if there is one: pick the optimal read op, drop the lock, run its DoIo;
            //     if DoIo produced a result, fire the op's completion callback under the
            //     completion lock (publishing the result with an lwsync before the call) and
            //     broadcast the completion condition; re-take the op-list lock,
            //   * else wait on the work condition (breaking out if the wait errors),
            // until stop is requested. Then release the lock and clear mbThreadRunning.
            // -----------------------------------------------------------------------------
            intptr_t Device::ThreadEntry(void* lpContext)
            {
                Device* lpThis = static_cast<Device*>(lpContext);

                lpThis->mbThreadRunning = 1;               // *(a1+4) = 1
                ::EnterCriticalSection(&lpThis->mLock);    // Mutex::Lock(a1+24)

                do
                {
                    AsyncOp* lpOp = lpThis->mOpList.mpHead; // v4 = *v3
                    if (lpOp)
                    {
                        // Inline head unlink (NOT AsyncOpList::Remove): the asm folds it.
                        if (lpOp == lpThis->mOpList.mpTail)
                        {
                            lpThis->mOpList.mpHead = nullptr;
                            lpThis->mOpList.mpTail = nullptr;
                        }
                        else
                        {
                            lpThis->mOpList.mpHead = lpOp->mpNext;
                        }
                        --lpThis->mOpList.muCount;
                        lpOp->mpNext = nullptr;            // *v4 = 0
                    }

                    if (lpOp)
                    {
                        AsyncOp* lpServe = lpThis->CheckForOptimalReadOp(lpOp);  // v5
                        ::LeaveCriticalSection(&lpThis->mLock);                   // Mutex::Unlock(v2)

                        int liResult = lpServe->mpfnDoIo(lpServe);                // (*(v5+68))(v5)
                        if (liResult)
                        {
                            ::EnterCriticalSection(&lpThis->mCompletionLock);     // Mutex::Lock(a1+176)
                            CompletionCallback lpfnComplete = lpServe->mpfnComplete; // v7 = *(v5+24)
                            lpServe->miResult = liResult;                         // *(v5+4) = v6
                            ::MemoryBarrier();                                     // lwsync: publish before callback
                            lpfnComplete(lpServe);                                 // v7(v5)
                            lpThis->mCompletion.Signal(true);                      // Condition::Signal(a1+224, 1)
                            ::LeaveCriticalSection(&lpThis->mCompletionLock);      // Mutex::Unlock(a1+176)
                        }

                        ::EnterCriticalSection(&lpThis->mLock);                   // Mutex::Lock(v2)
                    }
                    else if (lpThis->mWork.Wait(&lpThis->mLock, &KU_TIMEOUT_NONE) != 0)
                    {
                        break;
                    }
                }
                while (!lpThis->mbStopRequested);          // while (!*(a1+5))

                ::LeaveCriticalSection(&lpThis->mLock);    // Mutex::Unlock(v2)
                lpThis->mbThreadRunning = 0;               // *(a1+4) = 0
                return 0;
            }

            // -----------------------------------------------------------------------------
            // Device::GetInstance @0x82BBEAC0
            //
            // Resolve a device-relative path (lpcPath) into the search-path working buffer
            // (lpScratch) and return the registered Device whose driver scheme matches the
            // path's leading "scheme:" prefix; when the resolved path carries no scheme the
            // Manager's default device is returned.
            //
            // Path-normalisation (only when lpScratch != null):
            //   * if lpcPath already contains ':' -> copy lpcPath verbatim into lpScratch;
            //   * else build lpScratch = currentDir-prefix + path, dropping a leading "./"
            //     or ".\\" and, for an absolute leading '/'/'\\', truncating the prefix at
            //     its ':' so only "scheme:" survives.
            // Then the scheme is the text up to ':' (copied into a small stack buffer) and the
            // device list is walked, matching that scheme case-insensitively against each
            // device's driver name (DeviceDriver::macName at +4).
            // -----------------------------------------------------------------------------
            Device* Device::GetInstance(const char* lpcPath, char* lpScratch)
            {
                const char* lpcSrc = lpcPath;   // v3 (walks the source path)
                char*       lpDst  = lpScratch; // v2 (the working/scheme buffer)

                if (lpScratch)
                {
                    if (::strchr(lpcPath, ':'))
                    {
                        // Path already has a scheme: copy it verbatim into lpScratch.
                        const char* lpcS = lpcPath;
                        char lcCh;
                        do
                        {
                            lcCh = *lpcS;
                            lpScratch[lpcS - lpcPath] = lcCh;
                            ++lpcS;
                        }
                        while (lcCh);
                    }
                    else
                    {
                        // Seed lpScratch with the Manager's current-directory prefix.
                        char* lpd = lpScratch;
                        const char* lpc = gpFileSysManager->mpCurrentDir;
                        char lcCh;
                        do
                        {
                            lcCh = *lpc++;
                            *lpd++ = lcCh;
                        }
                        while (lcCh);

                        char* lpColon = ::strchr(lpScratch, ':');

                        // Drop a leading "./" or ".\\" off the source path.
                        if (*lpcSrc == '.')
                        {
                            char lcNext = lpcSrc[1];
                            if (lcNext == '/' || lcNext == '\\')
                                lpcSrc += 2;
                        }

                        char lcLead = *lpcSrc;
                        if (lcLead == '/' || lcLead == '\\')
                        {
                            // Absolute path: keep only "scheme:" of the prefix.
                            lpColon[1] = '\0';
                        }
                        else
                        {
                            // Relative path: append the directory separator string (the X360
                            // appends the &unk_82181180 literal -- the path separator "/") at
                            // the end of the seeded prefix.
                            const char* lpSep = "/";
                            char* lpEnd = lpScratch;
                            while (*lpEnd++)
                                ;
                            --lpEnd;
                            char lc;
                            do
                            {
                                lc = *lpSep++;
                                *lpEnd++ = lc;
                            }
                            while (lc);
                        }

                        // Append the (normalised) source path after the prefix.
                        char* lpAppend = lpScratch;
                        while (*lpAppend++)
                            ;
                        --lpAppend;
                        char lc2;
                        do
                        {
                            lc2 = *lpcSrc++;
                            *lpAppend++ = lc2;
                        }
                        while (lc2);
                    }
                    lpDst = lpScratch;
                }
                else
                {
                    lpDst = const_cast<char*>(lpcPath);  // v2 = a1
                }

                // Extract the scheme (text up to ':') into a small stack buffer. The X360
                // pre-zeroes the first 16 bytes (`std v33[0]; std v33[1]`); zero the whole
                // buffer here so the scheme is NUL-terminated for the compare below.
                char lacScheme[64];   // _QWORD v33[8]
                ::memset(lacScheme, 0, sizeof(lacScheme));

                Device* lpMatch = nullptr;   // v24
                const char* lpColon = ::strchr(lpDst, ':');   // v23
                if (lpColon)
                    ::strncpy(lacScheme, lpDst, static_cast<size_t>(lpColon - lpDst) + 1);

                if (lacScheme[0] == '\0')
                {
                    // No scheme -> the Manager's default device.
                    lpMatch = gpFileSysManager->mpDefaultDevice;   // *(off_8327F078 + 10) == +0x28
                    return lpMatch;
                }

                // Walk the registered-device list, comparing the scheme to each device's
                // driver name case-insensitively.
                for (Device* lpDev = gpFileSysManager->mpDeviceListHead; lpDev; lpDev = lpDev->mpNextRegistered)
                {
                    const char* lpcName = (lpDev->mpDriver) ? lpDev->mpDriver->macName : nullptr;  // v26 = v25[80]
                    if (!lpcName)
                        continue;

                    const char* lpcA = lpcName;     // device driver name
                    const char* lpcB = lacScheme;   // parsed scheme
                    bool lbEqual = false;
                    for (;;)
                    {
                        char lcA = *lpcA;
                        char lcB = *lpcB++;

                        // Case-fold A toward B's case for the compare (the X360 only folds
                        // when exactly one side is upper and the other lower).
                        if (lcA >= 'A' && lcA <= 'Z' && lcB >= 'a' && lcB <= 'z')
                            lcA = static_cast<char>(lcA + 32);
                        else if (lcA >= 'a' && lcA <= 'z' && lcB >= 'A' && lcB <= 'Z')
                            lcA = static_cast<char>(lcA - 32);

                        if (lcA == '\0' || lcB == '\0' || lcA != lcB)
                        {
                            if (lcA == lcB)        // both NUL and equal -> full match
                                lbEqual = true;
                            break;
                        }
                        ++lpcA;
                    }

                    if (lbEqual)
                    {
                        lpMatch = lpDev;
                        break;
                    }
                }

                return lpMatch;
            }

            // -----------------------------------------------------------------------------
            // Device::`scalar deleting destructor' @0x82BBE3B0
            //   ~Device(this);
            //   if (flags & 1)
            //       (*(*gpFileSysAllocator + 0xC))(gpFileSysAllocator, this, 0);  // Free
            //   return this;
            // -----------------------------------------------------------------------------
            Device* DeviceScalarDeletingDtor(Device* lpDevice, char lcFlags)
            {
                lpDevice->~Device();
                if (lcFlags & 1)
                    gpFileSysAllocator->Free(lpDevice, 0);
                return lpDevice;
            }
        }
    }
}
