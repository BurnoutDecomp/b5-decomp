#include "rw/rwcore_structs.h"

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

}  // namespace internal
}  // namespace shared_globals
}  // namespace rw
