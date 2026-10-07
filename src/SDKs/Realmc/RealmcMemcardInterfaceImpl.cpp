// ===========================================================================
// RealmcIface::MemcardInterfaceImpl -- the concrete memory-card interface. Holds
// the MemcardInterface::CreateInstance factory, which the console image places
// inside this class's code. NOT MOUNTED: the impl ctor and its interface
// overrides are not bodied yet (see RealmcMemcardInterfaceImpl.h).
// ===========================================================================

#include "SDKs/Realmc/RealmcMemcardInterfaceImpl.h" // RealmcIface::MemcardInterfaceImpl (the concrete impl)
#include "SDKs/Realmc/RealmcCore.h"                 // RealmcCore::GetMemAllocator / AllocateMem / IRealmcAllocatorBackend
#include "SDKs/Realmc/RealmcObjectManager.h"        // RealmcCore::ObjectManager::Initialize
#include "SDKs/Realmc/RealmcLocale.h"               // RealmcCore::Locale::SetLocaleGetStrCallback
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"   // EA::Thread::ThreadParameters

#include <new>   // placement new -- the impl is constructed in the block CreateInstance allocates.

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// MemcardInterface::CreateInstance -- the memory-card interface factory /
// singleton accessor:
//   1. install the caller's allocator backend         (GetMemAllocator)
//   2. install the caller's locale template callback   (SetLocaleGetStrCallback)
//   3. pick the worker's EA::Thread::ThreadParameters: the caller's override
//      when non-null, else a module-static default that is default-constructed
//      once and field-configured once with the "realmemcard_xenon" settings.
//   4. bring up the Realmc ObjectManager                (ObjectManager::Initialize())
//   5. lazily construct the single MemcardInterfaceImpl into a 0x528-byte block and
//      cache it; return the cached instance.
// ObjectManager::Initialize() takes no arguments (see RealmcObjectManager.h), so
// the earlier calls' return values are discarded.
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
        // Module-static default, constructed once and configured once with the
        // memory-card worker's stack size / affinity / name.
        static EA::Thread::ThreadParameters s_defaultParams;
        static bool s_bDefaultConfigured = false;
        if (!s_bDefaultConfigured)
        {
            s_defaultParams.mpStack     = nullptr;              // +0x00
            s_defaultParams.mnStackSize = 0x10000;             // +0x04  64KB
            s_defaultParams.mnPriority  = 0;                   // +0x08
            s_defaultParams.mnProcessor = -1;                  // +0x0C
            s_defaultParams.mbSuspended = false;               // +0x10
            s_defaultParams.mpName      = "realmemcard_xenon"; // +0x14
            s_bDefaultConfigured = true;
        }
        pThreadParams = &s_defaultParams;
    }

    // Bring up the three persistent Realmc backend objects.
    RealmcCore::ObjectManager::Initialize();

    // Lazily construct the single MemcardInterfaceImpl (a module-static singleton).
    static MemcardInterface* s_pInstance = nullptr;
    if (!s_pInstance)
    {
        // The raw object storage (0x528 == 1320 bytes, the impl's fixed size) is
        // allocated with a null tag, then the impl is constructed in place.
        void* pMem = RealmcCore::AllocateMem(nullptr, 1320);
        s_pInstance = pMem
                          ? new (pMem) MemcardInterfaceImpl(pThreadParams, liParam2, liParam3)
                          : nullptr;
    }
    return s_pInstance;
}

} // namespace RealmcIface
