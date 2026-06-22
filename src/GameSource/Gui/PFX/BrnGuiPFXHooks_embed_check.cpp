#include "GameSource/Gui/PFX/BrnGuiPFXHooks.h"
#include <cstddef>

// Compile-time exercise of BrnGui::PFXHook::FixDown and the proven field offsets.
// The FixDown asm @0x8250AFD8 reads mpaNodes at +0x34 and miNodeCount at +0x38,
// and treats each node's mpGroup at +4 -- those intra-struct offsets are pinned
// here. (No absolute / cross-pointer-member offset is asserted.)

using BrnGui::PFXHook;
using BrnGui::PFXHookNode;
using BrnGui::PFXHookBundle;

static_assert(offsetof(PFXHook, macName) == 0x00, "macName @0x00");
static_assert(offsetof(PFXHook, muId) == 0x20, "muId @0x20");
static_assert(offsetof(PFXHook, miPriority) == 0x24, "miPriority @0x24");
static_assert(offsetof(PFXHook, meTransitionMode) == 0x28, "meTransitionMode @0x28");
static_assert(offsetof(PFXHook, mfTransitionTime) == 0x2C, "mfTransitionTime @0x2C");
static_assert(offsetof(PFXHook, mbIsMenu) == 0x30, "mbIsMenu @0x30");
static_assert(offsetof(PFXHook, mpaNodes) == 0x34, "mpaNodes @0x34 (asm 0x34(r3))");
static_assert(offsetof(PFXHook, miNodeCount) == 0x38, "miNodeCount @0x38 (asm 0x38(r3))");

static_assert(offsetof(PFXHookNode, mfStartTime) == 0x00, "mfStartTime @0x00");
static_assert(offsetof(PFXHookNode, mpGroup) == 0x04, "mpGroup @0x04 (asm 4(node))");

static_assert(offsetof(PFXHookBundle, miHookCount) == 0x00, "miHookCount @0x00");
static_assert(offsetof(PFXHookBundle, miGroupCount) == 0x04, "miGroupCount @0x04");
static_assert(offsetof(PFXHookBundle, mpaHooks) == 0x08, "mpaHooks @0x08");
static_assert(offsetof(PFXHookBundle, mpaGroups) == 0x0C, "mpaGroups @0x0C");
static_assert(offsetof(PFXHookBundle, mSizeOfBundle) == 0x10, "mSizeOfBundle @0x10");

void BrnGuiPFXHooks_embed_check(PFXHook* lpHook, u32 luBase)
{
    lpHook->FixDown(luBase);
}
