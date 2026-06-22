// cLionBindings -- implementation.
//
// Vendor code (eauk_lion), reconstructed from the X360 asm/pseudocode at
//   0x82908240 cLionBindings::DeInit
// against the DWARF layout in LionBindings.h. Store-for-store faithful; members by name.
//
// Only DeInit is in this TU's recon ledger; it is the single function bodied here.

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionBindings.h"
#include "SDKs/Packages/Lion/Final/Allocator/include/CoreAllocator/ITaggedAllocator.h"

// The owning tagged allocator is reached through the Lion effect-manager singleton
// (cLionEffectManager::GetpAllocator, LionEffectManager.h). That TU is owned by another
// group; this is the minimal cross-TU accessor DeInit needs. FLAGGED cross-TU dep.
struct cLionEffectManager
{
    EA::Allocator::ITaggedAllocator* GetpAllocator();
};
extern cLionEffectManager* gpLionEffectManager;

void cLionBindings::DeInit()
{
    // The X360 build reaches the allocator via the effect manager (off_83121DA4) and frees
    // the owned locator array, then clears the array pointer and the count.
    if (mppLocators)
    {
        gpLionEffectManager->GetpAllocator()->Free(mppLocators, 0);
        mppLocators   = nullptr;
        mLocatorCount = 0;
    }
}
