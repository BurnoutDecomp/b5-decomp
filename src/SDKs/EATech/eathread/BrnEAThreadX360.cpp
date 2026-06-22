// X360-faithful EA::Thread facade + parameter-struct ctors.
// Source of truth: per-addr exports under
// .ida-exports/BURNOUT_X360_ARTIST.XEX/ (X360 asm overrides DWARF).
// See BrnEAThreadX360.h for the committed-type / vendor-snapshot divergence FLAG.

#include <Windows.h>
#include <process.h>  // _beginthreadex
#include <string.h>   // strncpy
#include <new>        // operator new / operator delete

#include "SDKs/EATech/eathread/BrnEAThreadX360.h"

// The X360 dynamic-thread-handle table is the already-recovered sibling type.
// IDA labels it EA::Thread::DynamicThreadArray, but the recovered home placed it
// at top-level ::DynamicThreadArray with the single-base (this) access the
// binary uses. We drive a single static instance, matching the binary's fixed
// global &unk_8324FF10.
#include "SDKs/EATech/eathread/eathread_pc_dynamicthreadarray.h"

namespace
{
    // Module-global TLS index for the cached thread HANDLE.
    // X360: dword_82F862A8, initialized to -1 (TLS_OUT_OF_INDEXES).
    DWORD gdwThreadHandleTls = TLS_OUT_OF_INDEXES;

    // X360: &unk_8324FF10 -- the one DynamicThreadArray instance the facade uses.
    DynamicThreadArray gThreadHandleTable;

    // X360: the 24-slot static EAThreadDynamicData pool (&unk_8324F898, 68B each)
    // and its parallel 24-entry busy-flag table (&unk_8324FF90).
    EA::Thread::EAThreadDynamicData gThreadDynamicDataPool[EA::Thread::KI_THREAD_DYNAMIC_DATA_COUNT];
    volatile LONG                   gThreadDynamicDataBusy[EA::Thread::KI_THREAD_DYNAMIC_DATA_COUNT];

    // X360 module data globals touched by EA::Thread::Thread:
    //   dword_8324FF00 -- the latched global RunnableFunction user-wrapper.
    //   dword_8324FF0C -- a round-robin processor-assignment counter, bumped
    //                     atomically (lwarx/stwcx.) when Begin needs to pick the
    //                     next hardware thread.
    //   dword_82F862B0 -- the configured default processor (negative => "auto",
    //                     i.e. use/advance the round-robin counter).
    // FLAG: dword_82F862B0's writer lives outside this dossier; it is modeled
    // here as the EAThread default-processor global and initialized to -1 (auto),
    // which matches the X360 "no explicit processor" behavior.
    EA::Thread::RunnableFunctionUserWrapper gpGlobalRunnableFunctionUserWrapper = 0; // dword_8324FF00
    volatile LONG                            glProcessorRoundRobin = 0;              // dword_8324FF0C
    int                                      giDefaultProcessor    = -1;             // dword_82F862B0
}

namespace EA
{
namespace Thread
{
    // 0x82B42650
    BarrierParameters::BarrierParameters(int height, bool bIntraProcess, const char* pName)
    {
        mnHeight       = height;
        mbIntraProcess = bIntraProcess;
        if (pName)
        {
            strncpy(mName, pName, 15);
            mName[15] = 0;
        }
        else
        {
            mName[0] = 0;
        }
    }

    // 0x82B42910 -- NOTE: no mMaxCount on X360 (see header FLAG).
    SemaphoreParameters::SemaphoreParameters(int initialCount, bool bIntraProcess, const char* pName)
    {
        mInitialCount  = initialCount;
        mbIntraProcess = bIntraProcess;
        if (pName)
        {
            strncpy(mName, pName, 15);
            mName[15] = 0;
        }
        else
        {
            mName[0] = 0;
        }
    }

    // 0x82B42710 -- X360 copies pName (unlike the modern vendor ctor).
    MutexParameters::MutexParameters(bool bIntraProcess, const char* pName)
    {
        mbIntraProcess = bIntraProcess;
        if (pName)
        {
            strncpy(mName, pName, 15);
            mName[15] = 0;
        }
        else
        {
            mName[0] = 0;
        }
    }

    // 0x82B426B0
    ConditionParameters::ConditionParameters(bool bIntraProcess, const char* pName)
    {
        mbIntraProcess = bIntraProcess;
        if (pName)
        {
            strncpy(mName, pName, 15);
            mName[15] = 0;
        }
        else
        {
            mName[0] = 0;
        }
    }

    // 0x82B43CF8
    // Claims the first free slot in the fixed 24-entry pool (busy flag flipped
    // 0->1 atomically). If all 24 are taken, heap-allocates a fresh 68-byte
    // record and zero-inits the same fields the static-slot path leaves zeroed.
    //
    // FLAG: the X360 reservation is a PowerPC lwarx/stwcx. spin claiming the
    // busy word, plus a second lwarx/stwcx. that zeroes the per-record spinlock
    // word at +0x20. On the MSVC host we model the reservation with an
    // interlocked compare-and-swap -- a host mapping of an atomic primitive that
    // cannot be expressed by name; the store-for-store field writes are exact.
    EAThreadDynamicData* AllocateThreadDynamicData()
    {
        for (s32 liIndex = 0; liIndex < KI_THREAD_DYNAMIC_DATA_COUNT; ++liIndex)
        {
            // lwarx/stwcx.: read busy flag; if free (0), claim it (set 1).
            const LONG lPrev = InterlockedCompareExchange(&gThreadDynamicDataBusy[liIndex], 1, 0);
            if (lPrev == 0)
                return &gThreadDynamicDataPool[liIndex]; // claimed this static slot
        }

        // Pool exhausted: allocate a fresh record.
        EAThreadDynamicData* lpData =
            static_cast<EAThreadDynamicData*>(operator new(sizeof(EAThreadDynamicData)));
        if (!lpData)
            return 0;

        // X360 zeroes exactly +0x00,+0x04,+0x08,+0x0C,+0x1C and the +0x20
        // refcount word (the last via a lwarx/stwcx. store of 0). Other fields
        // are filled in by Begin before the thread is started.
        lpData->mhThread                = 0;  // +0x00
        lpData->mnThreadId              = 0;  // +0x04
        lpData->mnStatus                = 0;  // +0x08
        lpData->mnReturnValue           = 0;  // +0x0C
        lpData->mpBeginThreadUserWrapper = 0; // +0x1C
        // +0x20 refcount: X360 zeroes it via lwarx/stwcx.; plain store here.
        lpData->mnRefCount              = 0;
        return lpData;
    }

    // 0x82B43DD8
    // If pData points inside the static pool, release its busy slot. Otherwise it
    // was heap-allocated: CloseHandle its HANDLE (if any) and operator-delete it.
    // The static-slot path also CloseHandles a live HANDLE.
    void FreeThreadDynamicData(EAThreadDynamicData* pData)
    {
        if (!pData)
            return;

        EAThreadDynamicData* const lpPoolBegin = &gThreadDynamicDataPool[0];
        EAThreadDynamicData* const lpPoolEnd    = &gThreadDynamicDataPool[KI_THREAD_DYNAMIC_DATA_COUNT];

        if (pData >= lpPoolBegin && pData < lpPoolEnd)
        {
            // Static slot: release the busy flag (X360: lwarx/stwcx. store 0).
            const ptrdiff_t lIndex = pData - lpPoolBegin;
            InterlockedExchange(&gThreadDynamicDataBusy[lIndex], 0);

            HANDLE lhThread = pData->mhThread;
            if (lhThread)
                CloseHandle(lhThread);
        }
        else
        {
            // Heap record.
            HANDLE lhThread = pData->mhThread;
            if (lhThread)
                CloseHandle(lhThread);
            operator delete(pData);
        }
    }

    // 0x82B424C8
    void SetCurrentThreadHandle(HANDLE hThread, bool bDynamic)
    {
        if (gdwThreadHandleTls == TLS_OUT_OF_INDEXES)
        {
            gdwThreadHandleTls = TlsAlloc();
            if (gdwThreadHandleTls == TLS_OUT_OF_INDEXES)
                return;
        }

        gThreadHandleTable.CheckDynamicThreadArray(false);

        if (bDynamic)
        {
            if (hThread)
            {
                // Explicit handle supplied: register it as a dynamic handle.
                gThreadHandleTable.AddDynamicThreadHandle(hThread, true);
            }
            else
            {
                // No handle supplied: if one is already cached, drop it (the
                // caller is clearing the current thread's registration).
                HANDLE lhCached = TlsGetValue(gdwThreadHandleTls);
                if (lhCached)
                    gThreadHandleTable.AddDynamicThreadHandle(lhCached, false);
            }
        }

        TlsSetValue(gdwThreadHandleTls, hThread);
    }

    // 0x82B42560
    ThreadId GetThreadId()
    {
        if (gdwThreadHandleTls != TLS_OUT_OF_INDEXES)
        {
            void* lpCached = TlsGetValue(gdwThreadHandleTls);
            if (lpCached)
                return lpCached; // previously cached HANDLE for this thread
        }

        // Not created by EAThread (or not yet cached): duplicate the pseudo
        // handle for the current thread into a real, stable HANDLE and cache it.
        // X360 call: DuplicateHandle(-1, -2, -1, &handle, 0, 1, 2).
        HANDLE lhThread = 0;
        if (DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(),
                            &lhThread, 0, TRUE, DUPLICATE_SAME_ACCESS))
        {
            SetCurrentThreadHandle(lhThread, true);
        }
        return lhThread;
    }

    // 0x82B425D0 -- NO priority remapping on X360 (see header FLAG).
    int GetThreadPriority()
    {
        return ::GetThreadPriority(GetCurrentThread());
    }

    // 0x82B425D8 -- NO priority remapping on X360.
    bool SetThreadPriority(int nPriority)
    {
        return ::SetThreadPriority(GetCurrentThread(), nPriority) != 0;
    }

    // 0x82B42610
    u32 ThreadSleep(const u32* lpuMilliseconds)
    {
        const u32 luMilliseconds = *lpuMilliseconds;
        if (luMilliseconds)
            return SleepEx(luMilliseconds, TRUE);
        return SwitchToThread();
    }

    // ---- EA::Thread::Thread (this TU) --------------------------------------

    // sub_82B43EC8 -- the static thread-entry trampoline _beginthreadex jumps to.
    // Not part of this dossier's public 7 funcs, but Begin must hand its address
    // to _beginthreadex, so it is reconstructed here store-for-store from its own
    // per-addr export. It dispatches the runnable, records the return value/status,
    // and releases the dynamic-data refcount when the thread exits.
    //
    // FLAG: _beginthreadex's start routine is unsigned(__stdcall*)(void*); the
    // X360 routine's intptr_t return is folded into that ABI on the host.
    static unsigned __stdcall ThreadEntryTrampoline(void* pArg)
    {
        EAThreadDynamicData* const lpData = static_cast<EAThreadDynamicData*>(pArg);

        // lwsync: acquire the fields published by Begin before we start.
        MemoryBarrier();

        RunnableFunction            lpFunction = reinterpret_cast<RunnableFunction>(lpData->mpStartContext[0]);
        void*                       lpContext  = lpData->mpStartContext[1];
        SetCurrentThreadHandle(lpData->mhThreadHandleCopy, false);

        RunnableFunctionUserWrapper lpWrapper  =
            reinterpret_cast<RunnableFunctionUserWrapper>(lpData->mpBeginThreadUserWrapper);

        lpData->mnStatus = Thread::kStatusRunning;

        intptr_t liReturn;
        if (lpWrapper)
            liReturn = lpWrapper(lpFunction, lpContext);
        else
            liReturn = lpFunction(lpContext);

        lpData->mnReturnValue = static_cast<s32>(liReturn);

        SetCurrentThreadHandle(0, false);
        lpData->mnStatus = Thread::kStatusEnded;

        // Release the begin-side refcount (lwarx/stwcx. decrement); free at zero.
        if (InterlockedDecrement(reinterpret_cast<volatile LONG*>(&lpData->mnRefCount)) == 0)
            FreeThreadDynamicData(lpData);

        return static_cast<unsigned>(liReturn);
    }

    // 0x82B43E88
    Thread::~Thread()
    {
        EAThreadDynamicData* const lpData = mThreadData.mpData;
        if (lpData)
        {
            // lwarx/stwcx. decrement of the refcount; free when it hits 0.
            if (InterlockedDecrement(reinterpret_cast<volatile LONG*>(&lpData->mnRefCount)) == 0)
                FreeThreadDynamicData(lpData);
        }
    }

    // 0x82B42EF8
    Thread::Status Thread::GetStatus(intptr_t* pThreadReturnValue)
    {
        MemoryBarrier(); // lwsync

        EAThreadDynamicData* const lpData = mThreadData.mpData;
        if (!lpData)
            return kStatusNone;

        if (lpData->mhThread)
        {
            DWORD ldwExitCode = 0;
            if (GetExitCodeThread(lpData->mhThread, &ldwExitCode))
            {
                if (ldwExitCode == STILL_ACTIVE) // 259
                    return kStatusRunning;

                // Thread has exited: close & forget the handle.
                CloseHandle(lpData->mhThread);
                lpData->mhThread = 0;
            }
        }

        if (pThreadReturnValue)
            *pThreadReturnValue = lpData->mnReturnValue;

        lpData->mnStatus = kStatusEnded;
        return kStatusEnded;
    }

    // 0x82B42FB8
    ThreadId Thread::GetId() const
    {
        EAThreadDynamicData* const lpData = mThreadData.mpData;
        if (lpData)
            return lpData->mhThread;
        return 0;
    }

    // 0x82B42FD8 -- 0x80000000 sentinel when the thread was never started.
    int Thread::GetPriority() const
    {
        EAThreadDynamicData* const lpData = mThreadData.mpData;
        if (lpData)
            return ::GetThreadPriority(lpData->mhThread);
        return static_cast<int>(0x80000000u);
    }

    // 0x82B43000
    void Thread::SetName(const char* pName)
    {
        EAThreadDynamicData* const lpData = mThreadData.mpData;
        if (lpData && pName)
        {
            strncpy(lpData->mName, pName, 32);
            lpData->mName[31] = 0; // stb 0 @ +0x43 (mName[31])

            if (lpData->mhThread)
            {
                // Raise the Visual Studio SetThreadName debugger exception.
                ULONG_PTR lArguments[4];
                lArguments[0] = 0x1000;                                       // dwType
                lArguments[1] = reinterpret_cast<ULONG_PTR>(lpData->mName);    // szName
                lArguments[2] = static_cast<ULONG_PTR>(lpData->mnThreadId);   // dwThreadID (asm reads +0x04, the OS thread id)
                lArguments[3] = 0;                                            // dwFlags
                RaiseException(0x406D1388u, 0, 4, lArguments);
            }
        }
    }

    // 0x82B42D98 -- latch once: only sets the global if it is still null.
    void Thread::SetGlobalRunnableFunctionUserWrapper(RunnableFunctionUserWrapper pUserWrapper)
    {
        if (!gpGlobalRunnableFunctionUserWrapper)
            gpGlobalRunnableFunctionUserWrapper = pUserWrapper;
    }

    // 0x82B43F80
    ThreadId Thread::Begin(RunnableFunction            pFunction,
                           void*                        pContext,
                           const ThreadParameters*      pParameters,
                           RunnableFunctionUserWrapper  pUserWrapper)
    {
        // If a prior thread record is still attached, release our reference to it
        // (lwarx/stwcx. decrement); free if that was the last reference.
        EAThreadDynamicData* lpOld = mThreadData.mpData;
        if (lpOld)
        {
            if (InterlockedDecrement(reinterpret_cast<volatile LONG*>(&lpOld->mnRefCount)) == 0)
                FreeThreadDynamicData(lpOld);
        }

        // Acquire a fresh dynamic-data record. Begin zeroes status/return/start
        // context/wrapper and the refcount, then publishes its fields.
        EAThreadDynamicData* lpData = AllocateThreadDynamicData();
        if (lpData)
        {
            lpData->mhThread                 = 0; // +0x00
            lpData->mnThreadId               = 0; // +0x04 (asm zero block stores +0x00/+0x04/+0x08/+0x0C/+0x1C/+0x20)
            lpData->mnStatus                 = 0; // +0x08
            lpData->mnReturnValue            = 0; // +0x0C
            lpData->mpBeginThreadUserWrapper = 0; // +0x1C
            lpData->mnRefCount               = 0; // +0x20 (lwarx/stwcx. store 0)
        }

        mThreadData.mpData = lpData;
        if (!lpData)
        {
            // (Out-of-pool with failed heap alloc: the X360 code still falls
            // through with a null record; the two refcount loops below operate on
            // the published null and are no-ops in that degenerate case.)
        }

        // Two atomic increments of the refcount: one for the Thread object's own
        // reference, one for the reference handed to the running thread.
        if (lpData)
        {
            InterlockedIncrement(reinterpret_cast<volatile LONG*>(&lpData->mnRefCount));
            InterlockedIncrement(reinterpret_cast<volatile LONG*>(&mThreadData.mpData->mnRefCount));
        }

        // Publish the start parameters (store order from the asm).
        EAThreadDynamicData* const lpPub = mThreadData.mpData;
        lpPub->mhThread                 = 0;                          // +0x00 (stw r26,0(r11))
        lpPub->mpStartContext[0]        = reinterpret_cast<void*>(pFunction); // +0x10
        lpPub->mpStartContext[1]        = pContext;                   // +0x14
        lpPub->mpBeginThreadUserWrapper = reinterpret_cast<void*>(pUserWrapper); // +0x1C

        unsigned int luStackSize = pParameters ? static_cast<unsigned int>(pParameters->mnStackSize) : 0u;

        // _beginthreadex(security=0, stack, start=trampoline, arg=lpPub, flags=4
        // (CREATE_SUSPENDED), thrdaddr = &lpPub->mnThreadId). The X360 passes ThrdAddr =
        // record+4 (asm 0x82B4409C addi r8,r6,4) so the OS thread id is written INTO the
        // record's +0x04 field, where SetName later reads it; the HANDLE return goes to mhThread.
        uintptr_t luHandle = _beginthreadex(0, luStackSize, ThreadEntryTrampoline,
                                            lpPub, 4 /*CREATE_SUSPENDED*/,
                                            reinterpret_cast<unsigned*>(&lpPub->mnThreadId));
        lpPub->mhThread = reinterpret_cast<HANDLE>(luHandle);

        if (lpPub->mhThread)
        {
            if (pParameters)
                SetName(pParameters->mpName);

            lpPub->mhThreadHandleCopy = lpPub->mhThread; // +0x18

            int liProcessor;
            if (pParameters && pParameters->mnProcessor >= 0)
            {
                liProcessor = pParameters->mnProcessor;
                // If the requested processor equals the round-robin cursor,
                // advance the cursor so the next auto-assignment differs.
                if (liProcessor == glProcessorRoundRobin)
                    InterlockedIncrement(&glProcessorRoundRobin);
            }
            else
            {
                liProcessor = giDefaultProcessor;
                if (giDefaultProcessor < 0)
                    liProcessor = InterlockedIncrement(&glProcessorRoundRobin);
            }

            if (lpPub->mhThread)
            {
                if (liProcessor < 0)
                    liProcessor = 0;
                // X360 calls XSetThreadProcessor(handle, hwThread) to pin the
                // thread to one of the 6 hardware threads. FLAG: XSetThreadProcessor
                // is an X360-only XDK primitive with no Win32 equivalent; on the
                // MSVC host we model it with SetThreadIdealProcessor (the closest
                // affinity primitive). The "% 6" hardware-thread fold is exact.
                SetThreadIdealProcessor(lpPub->mhThread, static_cast<DWORD>(liProcessor % 6));
            }

            ResumeThread(lpPub->mhThread);
            return lpPub->mhThread;
        }

        // _beginthreadex failed: release both references (decrement twice; free
        // at zero each time, matching the asm's two lwarx/stwcx. dec loops).
        if (InterlockedDecrement(reinterpret_cast<volatile LONG*>(&lpPub->mnRefCount)) == 0)
            FreeThreadDynamicData(lpPub);
        if (InterlockedDecrement(reinterpret_cast<volatile LONG*>(&mThreadData.mpData->mnRefCount)) == 0)
            FreeThreadDynamicData(mThreadData.mpData);

        mThreadData.mpData = 0;
        return 0;
    }
}
}
