#include "rw/rwcore_structs.h"

#include <cstdint>
#include <new>  // placement new (the X360 ctor runs on the aligned heap block)

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
}  // namespace

}  // namespace internal
}  // namespace shared_globals
}  // namespace rw
