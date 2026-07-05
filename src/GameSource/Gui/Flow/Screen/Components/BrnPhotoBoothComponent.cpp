// ===================================================================================
// BrnGui::PhotoBoothComponent  -- resource-plumbing implementation
//   class:BrnGui::PhotoBoothComponent
//
// SetCachePointer            @ 0x824B3490
// EnsureResourcesAreLoaded   @ 0x824B34F0
// EnsureResourcesAreUnloaded @ 0x824B3578
//   Reconstructed store-for-store from the X360 pseudocode/asm.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/Components/BrnPhotoBoothComponent.h"
#include "GameSource/Gui/BrnGuiCache.h"                  // BrnGui::GuiCache
#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT

namespace BrnGui
{
    // E_GUI_RESOURCEID_NUM -- the terminating count of the GUI resource-id enum
    // (BrnGuiShared.h). The X360 asserts the component's photo resource id is a real
    // id (not the sentinel count) before touching the cache. That enum is not committed
    // to this tree, so the count is spelled by its X360-attested guest value (0xED).
    static const u32 KU_GUI_RESOURCEID_NUM = 237;   // 0xED, attested by cmplwi

    // @ 0x824B3490 -- latch the GUI cache pointer the component drives its resource
    // load/unload through (asserts non-NULL). stw r31,0x404(this) => mpGuiCache.
    void PhotoBoothComponent::SetCachePointer(GuiCache* lpGuiCache)
    {
        CGS_ASSERT(lpGuiCache != 0, "NULL != lpGuiCache");
        mpGuiCache = lpGuiCache;
    }

    // @ 0x824B34F0 -- ask the GUI cache to load the component's single photo resource,
    // latch the result in mbResourceLoaded, and return it.
    bool PhotoBoothComponent::EnsureResourcesAreLoaded()
    {
        CGS_ASSERT(mPhotoResourceToLoad.muId != KU_GUI_RESOURCEID_NUM,
                   "E_GUI_RESOURCEID_NUM != mPhotoResourceToLoad.muId");
        CGS_ASSERT(mpGuiCache, "mpGuiCache");

        mbResourceLoaded = mpGuiCache->EnsureResourceIsLoaded(mPhotoResourceToLoad);
        return mbResourceLoaded;
    }

    // @ 0x824B3578 -- clear the loaded flag, then ask the GUI cache to unload the
    // component's photo resource and return the unload result. The asm snapshots
    // mpGuiCache, zeroes mbResourceLoaded, then makes the (tail) call in that order.
    bool PhotoBoothComponent::EnsureResourcesAreUnloaded()
    {
        CGS_ASSERT(mPhotoResourceToLoad.muId != KU_GUI_RESOURCEID_NUM,
                   "E_GUI_RESOURCEID_NUM != mPhotoResourceToLoad.muId");
        CGS_ASSERT(mpGuiCache, "mpGuiCache");

        GuiCache* lpGuiCache = mpGuiCache;
        mbResourceLoaded = false;
        return lpGuiCache->EnsureResourceIsUnloaded(mPhotoResourceToLoad);
    }
}
