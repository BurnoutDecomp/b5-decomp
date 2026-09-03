// cLionBindings -- implementation.
//
// Vendor code (eauk_lion), reconstructed from the X360 asm/pseudocode at
//   0x82908240 cLionBindings::DeInit
// against the DWARF layout in LionBindings.h. Store-for-store faithful; members by name.
//
// Only DeInit is in this TU's recon ledger; it is the single function bodied here.

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionBindings.h"
#include "SDKs/Packages/Lion/Final/Allocator/include/CoreAllocator/ITaggedAllocator.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionEffectManager.h"

// ⭐ THE LOCAL FORK IS GONE (2026-09-03). This TU used to declare its own one-method
// `struct cLionEffectManager` plus `extern cLionEffectManager* gpLionEffectManager` -- a
// re-declaration of a type that now has a real home, and an invented POINTER global that no
// TU in the tree defined, so this file could never have linked. The console has no such
// pointer: off_83121DA4 is `cLionEffectManager::mSingleton.mpAllocator`, i.e. this + 0x10 of
// the singleton object at 0x83121D94, and the real accessor is GetMe()->GetpAllocator().

void cLionBindings::DeInit()
{
    // The X360 build reaches the allocator via the effect manager (off_83121DA4) and frees
    // the owned locator array, then clears the array pointer and the count.
    if (mppLocators)
    {
        cLionEffectManager::GetMe()->GetpAllocator()->Free(mppLocators, 0);
        mppLocators   = nullptr;
        mLocatorCount = 0;
    }
}
