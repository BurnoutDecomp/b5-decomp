#include "rw/rwcore_structs.h"
#include "SDKs/EATech/eathread/eathread_atomic.h"

#include <cstdlib>
#include <new>

namespace rw { namespace shared_globals { namespace internal {
namespace {

// ARTIST 82BBC878/82BBC8D8: this manager is private to this source file.
// The list sentinel has the native OSGlobalNode shape; its original nested
// OSGlobalList declaration was not recovered. Only its links are initialized.
struct OSGlobalManager
{
    OSGlobalNode mOSGlobalList;
    EA::Thread::AtomicInt<unsigned int> mRefCount;
    CRITICAL_SECTION mcsLock;

    OSGlobalManager();
};

CGS_STATIC_ASSERT(sizeof(OSGlobalNode) == 0x10, "ARTIST global node");
CGS_STATIC_ASSERT(sizeof(CRITICAL_SECTION) == 0x1C, "XDK critical section");
CGS_STATIC_ASSERT(offsetof(OSGlobalManager, mRefCount) == 0x10, "ARTIST manager count");
CGS_STATIC_ASSERT(offsetof(OSGlobalManager, mcsLock) == 0x14, "ARTIST manager lock");
CGS_STATIC_ASSERT(sizeof(OSGlobalManager) == 0x30, "ARTIST global manager");

OSGlobalManager::OSGlobalManager()
{
    mOSGlobalList.mpNext = &mOSGlobalList;
    mOSGlobalList.mpPrev = &mOSGlobalList;
    mRefCount.SetValue(0);
    RtlInitializeCriticalSection(&mcsLock);
}

// ARTIST 82BBC8D8: retain the raw allocation immediately before the aligned block.
OSGlobalManager* AllocateAndConstructOSGlobalManager()
{
    void* lpRaw = HeapAlloc(GetProcessHeap(), 0, 0x43);
    const uintptr_t luAligned = (reinterpret_cast<uintptr_t>(lpRaw) + 0x13)
                              & ~static_cast<uintptr_t>(0xF);
    void* lpAligned = reinterpret_cast<void*>(luAligned);
    reinterpret_cast<void**>(lpAligned)[-1] = lpRaw;
    return new (lpAligned) OSGlobalManager;
}

OSGlobalManager* s_pOSGlobalManager = 0; // off_8327F024
HANDLE s_hGlobalSemaphore = 0;          // dword_8327F020
volatile LONG s_initRefCount = 0;       // unk_8327F05C

// ARTIST 82BBC918. This counter is distinct from the manager's share count.
void ShutdownOSGlobalSystem()
{
    if (_InterlockedDecrement(&s_initRefCount) == 0)
    {
        if (s_pOSGlobalManager)
        {
            if (s_pOSGlobalManager->mRefCount.Decrement() == 0)
            {
                void* lpRaw = reinterpret_cast<void**>(s_pOSGlobalManager)[-1];
                HeapFree(GetProcessHeap(), 0, lpRaw);
            }
            s_pOSGlobalManager = 0;
        }
        if (s_hGlobalSemaphore)
        {
            CloseHandle(s_hGlobalSemaphore);
            s_hGlobalSemaphore = 0;
        }
    }
}

// ARTIST 82BBD228. In particular, the manager increment has no null guard.
int InitOSGlobalSystem()
{
    if (!s_pOSGlobalManager)
    {
        HANDLE lhMutex = CreateMutexA(0, FALSE, "RwGlobalMgrMutex");
        if (!lhMutex)
            return 0;

        if (WaitForSingleObject(lhMutex, INFINITE) != static_cast<DWORD>(-1))
        {
            s_hGlobalSemaphore = CreateSemaphoreA(0, 0, 0x7FFFFFFF, "RwGlobalMgr");
            if (s_hGlobalSemaphore)
            {
                if (GetLastError() == ERROR_ALREADY_EXISTS)
                {
                    LONG liPrevCount;
                    if (ReleaseSemaphore(s_hGlobalSemaphore, 1, &liPrevCount))
                    {
                        WaitForSingleObject(s_hGlobalSemaphore, INFINITE);
                        s_pOSGlobalManager = reinterpret_cast<OSGlobalManager*>(
                            static_cast<uintptr_t>(static_cast<uint32_t>(liPrevCount) << 4));
                    }
                }
                else
                {
                    s_pOSGlobalManager = AllocateAndConstructOSGlobalManager();
                    ReleaseSemaphore(s_hGlobalSemaphore,
                        static_cast<LONG>(reinterpret_cast<uintptr_t>(s_pOSGlobalManager) >> 4), 0);
                }
            }
            s_pOSGlobalManager->mRefCount.Increment();
            ReleaseMutex(lhMutex);
            CloseHandle(lhMutex);
        }
        if (!s_pOSGlobalManager)
        {
            ShutdownOSGlobalSystem();
            return 0;
        }
    }
    _InterlockedIncrement(&s_initRefCount);
    return 1;
}

// The native CRT initializer 82C73EB0 calls Init even if it fails, then registers
// the Shutdown thunk with atexit. A static object's lifetime provides that order.
struct AutoinitOSGlobalManager
{
    AutoinitOSGlobalManager() { InitOSGlobalSystem(); }
    ~AutoinitOSGlobalManager() { ShutdownOSGlobalSystem(); }
};

AutoinitOSGlobalManager s_autoinitOSGlobalManager;

} // anonymous namespace
}}} // rw::shared_globals::internal
