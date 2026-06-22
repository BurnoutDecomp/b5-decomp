#include "GameSource/Gui/PFX/BrnGuiPFXHooks.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//   BrnGui::PFXHook::FixDown @ 0x8250AFD8  (called by
//   CgsResource::PFXHookBundleResourceType::FixDown when a hook bundle is
//   serialised back out)
//
// FixDown un-relocates the hook's node table: it walks the mpaNodes[] pointer
// array (each entry is a relocated 32-bit PFXHookNode address) and subtracts the
// base value from each node's mpGroup link and from each table entry, then from
// the mpaNodes table pointer itself -- turning runtime addresses back into
// file-relative offsets. The asm reads miNodeCount (@0x38) / mpaNodes (@0x34),
// subtracts a2 from *(node+4)=mpGroup and from each table slot, then mpaNodes.

namespace BrnGui
{
    template <typename T>
    static T* PointerFromU32(u32 luAddress)
    {
        return reinterpret_cast<T*>(static_cast<uintptr_t>(luAddress));
    }

    // @ 0x8250AFD8
    void PFXHook::FixDown(u32 luBaseValue)
    {
        u32* lpaNodeAddresses = PointerFromU32<u32>(mpaNodes);
        for (s32 liIndex = 0; liIndex < miNodeCount; ++liIndex)
        {
            PointerFromU32<PFXHookNode>(lpaNodeAddresses[liIndex])->mpGroup -= luBaseValue;
            lpaNodeAddresses[liIndex] -= luBaseValue;
        }
        mpaNodes -= luBaseValue;
    }
}
