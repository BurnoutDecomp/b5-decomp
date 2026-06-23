// ===================================================================================
// BrnGui::SatNavComponent  -- implementation
//   class:BrnGui::SatNavComponent
//
// SetCachePointer @ 0x82473638
//   Asserts the incoming cache pointer is non-null ("lpCache", BrnSatNav.h:342), then
//   stores it into the component at +0x25C (stw). Reconstructed store-for-store from the
//   X360 asm. The X360 returns r3 (the original `this`/result register) unused by the
//   HUD-state callers, so the reconstructed accessor is void.
// ===================================================================================
#include "GameSource/Gui/SatNav/BrnSatNavComponent.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnGui
{
    // @ 0x82473638
    void SatNavComponent::SetCachePointer(void* lpCache)
    {
        CGS_ASSERT(lpCache != 0, "lpCache");   // @0x8247365C

        mpCache = lpCache;                      // @0x8247367C (stw 0x25C)
    }
}
