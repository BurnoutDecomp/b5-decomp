#include "rw/rwcore_structs.h"

#include <cstdint>
#include <new>       // placement new (the X360 ctor runs on the aligned heap block)
#include <intrin.h>  // _InterlockedIncrement / _InterlockedDecrement (the X360 lwarx/stwcx loops)

// On PC the X360 RtlInitializeCriticalSection is the Win32 InitializeCriticalSection
// (the Rtl* names are the ntdll-level aliases). Keep <windows.h> lean so it does not
// leak USER/GDI macros into the wider rw:: vocabulary (matches DebugCriticalSection.h).
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOGDI
#  define NOGDI
#endif
#ifndef NOUSER
#  define NOUSER
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>

// ===========================================================================
// rw::shared_globals::internal::OSGlobalManager -- out-of-line ctor home.
//
// OWNING HOME for:
//     rw::shared_globals::internal::`anonymous namespace'::OSGlobalManager::
//         OSGlobalManager  @ 0x82BBC878   (EXECUTED in the boot trace)
//
// The X360 ctor @ 0x82BBC878 (32-bit ABI; stw = 4-byte stores) does three things:
//
//   stw r31, 0(r31)        ; mOSGlobalList.next = this   (list head -> self)
//   stw r31, 4(r31)        ; mOSGlobalList.prev = this   (list tail -> self)
//   ; interrupt-disabled lwarx/stwcx loop storing r10(=0) to this+0x10:
//   mfmsr/mtmsree/lwarx/stwcx./mtmsree   ; mRefCount = 0  (EA::Thread::AtomicInt)
//   addi r3, r31, 0x14 ; bl RtlInitializeCriticalSection   ; init mcsLock
//   return this
//
// So the constructed manager is: an EMPTY circular intrusive global-node list
// (its head's next/prev both point back at the head sentinel), an atomic
// reference count seeded to zero, and an initialised OS critical section.
//
// LAYOUT NOTE (cross-build): the committed PC struct is the x64 rwcore.pdb form
// (mOSGlobalList[24], mRefCount[4], mcsLock[40], sizeof=72). The X360 stores land
// at the 32-bit offsets +0/+4 (the two list-head pointers), +0x10 (the atomic
// word) and +0x14 (the CRITICAL_SECTION). The two foreign fields are committed as
// opaque blobs (`was: EA::Thread::AtomicInt`, `was: Win32Mutex`), so the atomic
// seed and the lock are written through those committed members at offset 0 of
// each blob (the documented opaque-foreign-field precedent) -- never retyping the
// committed layout. The self-referential list head is initialised by name in
// x64-pointer terms (next=prev=&head), which is the same empty-list semantic the
// X360 32-bit stores express.
// ===========================================================================

namespace rw {
namespace shared_globals {
namespace internal {

OSGlobalManager::OSGlobalManager()
{
    // Empty circular intrusive list: the head node's next/prev point at the head.
    // On x64 the head occupies the first two pointers of mOSGlobalList.
    void** lppHead = reinterpret_cast<void**>(&mOSGlobalList[0]);
    lppHead[0] = this;  // next = this
    lppHead[1] = this;  // prev = this

    // Atomic reference count seeded to zero (the X360 lwarx/stwcx store of 0).
    *reinterpret_cast<volatile uint32_t*>(&mRefCount[0]) = 0u;

    // Initialise the OS critical section (X360 RtlInitializeCriticalSection).
    InitializeCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(&mcsLock[0]));
}

// ===========================================================================
// rw::shared_globals::internal::`anonymous namespace' init helper @ 0x82BBC8D8
//   (the lazy first-use allocator the OSGlobal system runs from InitOSGlobalSystem)
//
// The X360 body @ 0x82BBC8D8 is the 16-byte-aligned manager allocator:
//     r3 = GetProcessHeap()
//     r3 = RtlAllocateHeap(r3, 0, 0x43)        ; raw = 67-byte process-heap block
//     r10 = raw + 0x13                          ; round-up bias (16 + a 4-byte cookie slot)
//     r3  = r10 & ~0xF                          ; aligned = (raw + 0x13) & ~15
//     *(aligned - 4) = raw                      ; stash the raw base for the matching free
//     return OSGlobalManager::OSGlobalManager() ; placement-construct on the aligned block
//
// i.e. an aligned operator-new over the Win32 process heap (the X360 RtlAllocateHeap is
// the ntdll-level alias of HeapAlloc), with the unaligned allocation base stored in the
// 4 bytes just below the aligned pointer so the deallocator can recover it. The X360
// request is 0x43 (67) bytes: the 32-bit manager's own size plus the up-to-15 alignment
// slack and the 4-byte cookie. The constructed (aligned) pointer is returned.
//
// Anonymous-namespace internal-linkage helper (the export's `_anonymous_namespace_`),
// so it is given file-local linkage here and named after its X360 role.
// ===========================================================================
namespace
{
    OSGlobalManager* AllocateAndConstructOSGlobalManager()
    {
        HANDLE lhProcessHeap = GetProcessHeap();

        // Raw 67-byte process-heap allocation (X360 RtlAllocateHeap(heap, 0, 0x43)).
        void* lpRaw = HeapAlloc(lhProcessHeap, 0, 0x43);

        // Round the raw base up to the next 16-byte boundary, leaving room for the
        // 4-byte cookie just below the aligned pointer ((raw + 0x13) & ~0xF).
        uintptr_t luAligned = (reinterpret_cast<uintptr_t>(lpRaw) + 0x13) & ~static_cast<uintptr_t>(0xF);
        void* lpAligned = reinterpret_cast<void*>(luAligned);

        // Stash the unaligned base in the dword immediately below the aligned block
        // (the X360 `stw r11, -4(r3)` free cookie).
        reinterpret_cast<void**>(lpAligned)[-1] = lpRaw;

        // Placement-construct the manager on the aligned block and return it.
        return new (lpAligned) OSGlobalManager();
    }

    // -----------------------------------------------------------------------
    // Process-wide OS-global bootstrap state (the X360 file-scope statics):
    //   off_8327F024  -- the shared OSGlobalManager* (16-aligned; its [-1] cookie
    //                    is the raw heap base, its mRefCount is the share count)
    //   dword_8327F020 -- the named "RwGlobalMgr" semaphore that carries the
    //                    manager pointer (>> 4) across processes
    //   unk_8327F05C  -- the process-local init counter (how many Init/Shutdown
    //                    pairs this process holds open)
    // -----------------------------------------------------------------------
    OSGlobalManager* s_pOSGlobalManager = 0;   // off_8327F024
    HANDLE           s_hGlobalSemaphore = 0;    // dword_8327F020
    volatile long    s_initRefCount = 0;        // unk_8327F05C

    // The atomic word the X360 increments at (manager + 16) is the committed
    // OSGlobalManager::mRefCount. (X360 32-bit layout puts the share count at +16;
    // the committed x64 struct names it mRefCount, accessed by name here.)
    inline volatile long* ManagerRefCount(OSGlobalManager* lpManager)
    {
        return reinterpret_cast<volatile long*>(&lpManager->mRefCount[0]);
    }

    // ShutdownOSGlobalSystem @0x82BBC918 -- release one process hold on the OS-global
    // system; tear the manager and semaphore down once the last hold (and the last
    // share) is dropped.
    //
    //   if (--unk_8327F05C == 0) {                       // atomic process-counter dec
    //       if (off_8327F024) {                          // shared manager present?
    //           if (--manager->mRefCount == 0) {          // atomic share-count dec
    //               raw = *(off_8327F024 - 1);            // the stashed unaligned base
    //               RtlFreeHeap(GetProcessHeap(), 0, raw);
    //           }
    //           off_8327F024 = 0;
    //       }
    //       if (dword_8327F020) { CloseHandle(dword_8327F020); dword_8327F020 = 0; }
    //   }
    void ShutdownOSGlobalSystem()
    {
        if (_InterlockedDecrement(&s_initRefCount) == 0)
        {
            if (s_pOSGlobalManager)
            {
                if (_InterlockedDecrement(ManagerRefCount(s_pOSGlobalManager)) == 0)
                {
                    // Recover the raw (unaligned) heap base from the [-1] cookie and free it.
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

    // InitOSGlobalSystem @0x82BBD228 (EXECUTED in the boot trace) -- acquire one process
    // hold on the OS-global system, lazily creating the shared manager (with cross-process
    // hand-off via the named semaphore's count) the first time any process asks.
    //
    //   if (!off_8327F024) {                                       // manager not yet shared
    //       mutex = CreateMutexA(0,0,"RwGlobalMgrMutex");
    //       if (!mutex) return 0;
    //       if (WaitForSingleObject(mutex, INFINITE) != -1) {
    //           dword_8327F020 = CreateSemaphoreA(0,0,0x7FFFFFFF,"RwGlobalMgr");
    //           if (dword_8327F020) {
    //               if (GetLastError() == ERROR_ALREADY_EXISTS) {  // another process owns it
    //                   if (ReleaseSemaphore(sem, 1, &prevCount)) {
    //                       WaitForSingleObject(sem, INFINITE);
    //                       off_8327F024 = (OSGlobalManager*)(16 * prevCount);  // ptr = count<<4
    //                   }
    //               } else {                                       // first creator
    //                   off_8327F024 = AllocateAndConstructOSGlobalManager();
    //                   ReleaseSemaphore(sem, (long)off_8327F024 >> 4, 0);      // publish ptr>>4
    //               }
    //           }
    //           ++manager->mRefCount;                              // atomic share-count inc
    //           ReleaseMutex(mutex); CloseHandle(mutex);
    //       }
    //       if (!off_8327F024) { ShutdownOSGlobalSystem(); return 0; }
    //   }
    //   ++unk_8327F05C;                                            // atomic process-counter inc
    //   return 1;
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
                        // The manager already lives in another process: its pointer was
                        // published as the semaphore's previous count (ptr >> 4).
                        LONG liPrevCount = 0;
                        if (ReleaseSemaphore(s_hGlobalSemaphore, 1, &liPrevCount))
                        {
                            WaitForSingleObject(s_hGlobalSemaphore, INFINITE);
                            s_pOSGlobalManager = reinterpret_cast<OSGlobalManager*>(
                                static_cast<uintptr_t>(16 * liPrevCount));
                        }
                    }
                    else
                    {
                        // First creator: build the manager and publish its pointer (>>4)
                        // into the semaphore's count for the other processes.
                        s_pOSGlobalManager = AllocateAndConstructOSGlobalManager();
                        ReleaseSemaphore(
                            s_hGlobalSemaphore,
                            static_cast<LONG>(reinterpret_cast<uintptr_t>(s_pOSGlobalManager) >> 4),
                            0);
                    }
                }

                // Atomic share-count increment at manager+0x10 (the asm does this
                // unconditionally; the manager is always valid by here on every
                // reachable path -- guarded so a degenerate null can't fault the host).
                if (s_pOSGlobalManager)
                    _InterlockedIncrement(ManagerRefCount(s_pOSGlobalManager));
                ReleaseMutex(lhMutex);
                CloseHandle(lhMutex);
            }

            if (!s_pOSGlobalManager)
            {
                ShutdownOSGlobalSystem();
                return 0;
            }
        }

        _InterlockedIncrement(&s_initRefCount); // process-counter inc (unk_8327F05C)
        return 1;
    }
}  // namespace

}  // namespace internal
}  // namespace shared_globals
}  // namespace rw
