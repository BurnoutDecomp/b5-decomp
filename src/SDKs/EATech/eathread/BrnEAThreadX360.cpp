// X360-faithful EA::Thread facade + parameter-struct ctors.
// Source of truth: per-addr exports under
// .ida-exports/BURNOUT_X360_ARTIST.XEX/ (X360 asm overrides DWARF).
// See BrnEAThreadX360.h for the committed-type / vendor-snapshot divergence FLAG.

#include <Windows.h>
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

        lpData->mhThread   = 0;  // +0x00
        lpData->muField04  = 0;  // +0x04
        lpData->muField08  = 0;  // +0x08
        lpData->muField0C  = 0;  // +0x0C
        lpData->muField1C  = 0;  // +0x1C
        // +0x20 spinlock word: X360 zeroes it via lwarx/stwcx.; plain store here.
        lpData->miSpinLock = 0;
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
}
}
