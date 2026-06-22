#include "GameSource/Gui/PFX/BrnGuiPFXHookNodeBlender.h"

namespace BrnGui
{
// 0x824ECFC0  BrnGui::PFXHookNodeBlender::IsActive
// Semantic parity with the X360 body:
//   if (!mpHook) return false;
//   liCount = mpHook->miNodeCount;       // 0x38(r11)
//   if (liCount <= 0) return false;
//   walk the node-blend slots; stop at the first slot whose state != FINISHED
//   (return true). If every slot up to liCount is FINISHED, return false.
// The X360 loop steps a pointer by 0x130 from this+0x14 (the maNodeBlends
// array) and bumps an index toward liCount; reproduced here by name.
bool PFXHookNodeBlender::IsActive() const
{
    if (mpHook == nullptr)
    {
        return false;
    }

    const s32 liNodeCount = mpHook->miNodeCount;
    if (liNodeCount <= 0)
    {
        return false;
    }

    for (s32 liIndex = 0; liIndex < liNodeCount; ++liIndex)
    {
        if (maNodeBlends[liIndex].meState != E_NODE_BLEND_FINISHED)
        {
            return true;
        }
    }

    return false;
}
}
