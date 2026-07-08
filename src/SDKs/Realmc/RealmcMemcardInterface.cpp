// ===========================================================================
// RealmcIface::MemcardInterface -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODIES both come from the X360 asm. See
// RealmcMemcardInterface.h for the vtable/slot layout, and RealmcMemcardInterfaceImpl.h
// for the concrete implementation this factory constructs.
// ===========================================================================

#include "SDKs/Realmc/RealmcMemcardInterface.h"
#include "SDKs/Realmc/RealmcMemcardInterfaceImpl.h" // RealmcIface::MemcardInterfaceImpl (the concrete impl)
#include "SDKs/Realmc/RealmcCore.h"                 // RealmcCore::GetMemAllocator / AllocateMem / IRealmcAllocatorBackend
#include "SDKs/Realmc/RealmcObjectManager.h"        // RealmcCore::ObjectManager::Initialize
#include "SDKs/Realmc/RealmcLocale.h"               // RealmcCore::Locale::SetLocaleGetStrCallback
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"   // EA::Thread::ThreadParameters

#include <new>   // placement new -- the X360 constructs the impl in the block it allocated.

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// MemcardInterface::MemcardInterface @ 0x82B51C00
//
//   lis  r11, off_821484E0@ha ; addi r11, r11, off_821484E0@l
//   stw  r11, 0(r3) ; blr
//
// Install the class vtable and return. An empty ctor body emits exactly that
// store; the compiler produces off_821484E0 from the class definition.
// ---------------------------------------------------------------------------
MemcardInterface::MemcardInterface()
{
}

// ---------------------------------------------------------------------------
// MemcardInterface::~MemcardInterface -- backs the X360 `vector deleting
// destructor' @ 0x82B51BB8:
//
//   *a1 = off_821484E0 ; if ( (a2 & 1) != 0 ) operator delete(a1) ; return a1
//
// i.e. restore the vtable, then operator-delete when the delete flag bit0 is set.
// The compiler emits that deleting-destructor thunk from this (empty) virtual dtor.
// ---------------------------------------------------------------------------
MemcardInterface::~MemcardInterface()
{
}

// ---------------------------------------------------------------------------
// MemcardInterface::CreateInstance @ 0x82B52BF8
//
// The memory-card interface factory / singleton accessor. Faithful to the asm:
//   1. install the caller's allocator backend         (GetMemAllocator(a1[0]))
//   2. install the caller's locale template callback   (SetLocaleGetStrCallback(a1[1]))
//   3. pick the worker's EA::Thread::ThreadParameters: the caller's override
//      (a1[2]) when non-null, else a module-static default that is default-
//      constructed once (X360 guard dword_832500DC bit0) and field-configured once
//      (X360 guard byte_832500C0) with the "realmemcard_xenon" thread settings.
//   4. bring up the Realmc ObjectManager                (ObjectManager::Initialize())
//   5. lazily construct the single MemcardInterfaceImpl into a 0x528-byte block and
//      cache it (X360 singleton off_832500BC); return the cached instance.
//
// The Hex-Rays pseudocode threads the r3 return of the locale/ThreadParameters
// calls into ObjectManager::Initialize(); that is a decompiler artifact of tracking
// r3 -- ObjectManager::Initialize() takes no arguments (see RealmcObjectManager.h),
// so those returns are simply discarded here.
// ---------------------------------------------------------------------------
MemcardInterface* MemcardInterface::CreateInstance(const CreateParams* pParams,
                                                   int liParam2, int liParam3)
{
    // Install the caller's allocator backend and locale template getter.
    RealmcCore::GetMemAllocator(pParams->mpAllocator);
    RealmcCore::Locale::SetLocaleGetStrCallback(pParams->mpfnGetStr);

    // Select the ThreadParameters the memory-card worker thread runs under.
    EA::Thread::ThreadParameters* pThreadParams;
    if (pParams->mpThreadParams)
    {
        pThreadParams = pParams->mpThreadParams;
    }
    else
    {
        // Module-static default, constructed once (X360 dword_832500DC guard) and
        // configured once (X360 byte_832500C0 guard) with the memory-card worker's
        // stack size / affinity / name.
        static EA::Thread::ThreadParameters s_defaultParams;
        static bool s_bDefaultConfigured = false;
        if (!s_bDefaultConfigured)
        {
            s_defaultParams.mpStack     = nullptr;              // stw 0,    +0x00
            s_defaultParams.mnStackSize = 0x10000;             // stw 64KB, +0x04
            s_defaultParams.mnPriority  = 0;                   // stw 0,    +0x08
            s_defaultParams.mnProcessor = -1;                  // stw -1,   +0x0C
            s_defaultParams.mbSuspended = false;               // stb 0,    +0x10
            s_defaultParams.mpName      = "realmemcard_xenon"; // stw name, +0x14
            s_bDefaultConfigured = true;
        }
        pThreadParams = &s_defaultParams;
    }

    // Bring up the three persistent Realmc backend objects.
    RealmcCore::ObjectManager::Initialize();

    // Lazily construct the single MemcardInterfaceImpl (X360 singleton off_832500BC).
    static MemcardInterface* s_pInstance = nullptr;
    if (!s_pInstance)
    {
        // The X360 allocates the raw object storage (0x528 == 1320 bytes, the impl's
        // fixed size) with a null tag, then constructs the impl in place.
        void* pMem = RealmcCore::AllocateMem(nullptr, 1320);
        s_pInstance = pMem
                          ? new (pMem) MemcardInterfaceImpl(pThreadParams, liParam2, liParam3)
                          : nullptr;
    }
    return s_pInstance;
}

} // namespace RealmcIface
