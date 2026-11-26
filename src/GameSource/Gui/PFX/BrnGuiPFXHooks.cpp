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
    // @ 0x8250B038 (inlined into PFXHookBundleResourceType::FixUp through the BrnGuiUnity
    // build; no ARTIST copy): relocate the node table, then each entry, then each node's
    // group link, by the load base. DWARF local liIndex (cpp:176).
    void PFXHook::FixUp(uint32_t luBaseValue)
    {
        mpaNodes = reinterpret_cast<PFXHookNode**>(reinterpret_cast<uint32_t>(mpaNodes) + luBaseValue);
        for (int32_t liIndex = 0; liIndex < miNodeCount; ++liIndex)
        {
            mpaNodes[liIndex] = reinterpret_cast<PFXHookNode*>(reinterpret_cast<uint32_t>(mpaNodes[liIndex]) + luBaseValue);
            mpaNodes[liIndex]->mpGroup =
                reinterpret_cast<PFXGroup*>(reinterpret_cast<uint32_t>(mpaNodes[liIndex]->mpGroup) + luBaseValue);
        }
    }

    // @ 0x8250AFD8: the reverse, the group link before its node and the table last.
    // DWARF local liIndex (cpp:122).
    void PFXHook::FixDown(uint32_t luBaseValue)
    {
        for (int32_t liIndex = 0; liIndex < miNodeCount; ++liIndex)
        {
            mpaNodes[liIndex]->mpGroup =
                reinterpret_cast<PFXGroup*>(reinterpret_cast<uint32_t>(mpaNodes[liIndex]->mpGroup) - luBaseValue);
            mpaNodes[liIndex] = reinterpret_cast<PFXHookNode*>(reinterpret_cast<uint32_t>(mpaNodes[liIndex]) - luBaseValue);
        }
        mpaNodes = reinterpret_cast<PFXHookNode**>(reinterpret_cast<uint32_t>(mpaNodes) - luBaseValue);
    }
}
