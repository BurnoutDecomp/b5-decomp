#ifndef BRN_GUI_PFX_HOOK_NODE_BLENDER_H
#define BRN_GUI_PFX_HOOK_NODE_BLENDER_H

#include "types.hpp"                              // s32, u32, f32
#include "GameSource/Gui/PFX/BrnGuiPFXHooks.h"    // BrnGui::PFXHook

// BrnGui::PFXHookNodeBlender - the per-hook runtime blender that drives one
// PFXHook's node list (the hook describes the static node/group graph; the
// blender holds the live per-node blend state while that hook is playing).
//
// Layout proven from BURNOUT_X360_ARTIST.XEX @ 0x824ECFC0 (IsActive):
//   +0x00  mpHook        PFXHook*   (`lwz r11,0(r3)`; node count read at
//                                     mpHook->miNodeCount == 0x38(r11))
//   +0x14  maNodeBlends  NodeBlend[] (iterate from `addi r10,r3,0x14`,
//                                     stride 0x130, state field at entry+0,
//                                     compared `cmpwi r8,4`)
// The 16 bytes between mpHook and the blend array hold the blender's other
// runtime fields (timing / cursor); IsActive does not touch them, so they are
// reserved here by name without inventing semantics. Member access is by name;
// no absolute or cross-pointer-member offset is asserted.
namespace BrnGui
{
    // Per-node blend state. The X360 compares the leading state word against 4
    // to decide "this node has finished blending" - any node still != that
    // finished value means the blender is still doing work.
    enum NodeBlendState
    {
        E_NODE_BLEND_FINISHED = 4,   // 0x824ECFE4 `cmpwi cr6, r8, 4`
    };

    // One live node-blend slot. Stride 0x130 (304B) proven by the loop's
    // `addi r10, r10, 0x130`; only the leading state word is referenced by the
    // recovered method, so the remainder is reserved by name.
    struct PFXHookNodeBlend
    {
        u32 meState;            // entry+0x00 - NodeBlendState
        u8  maReserved[0x130 - sizeof(u32)];
    };

    class PFXHookNodeBlender
    {
    public:
        // 0x824ECFC0 - true while the blender is still working: false if it has
        // no hook, the hook has no nodes, or every live node slot has reached
        // the finished state; otherwise true (some node is still blending).
        bool IsActive() const;

    private:
        PFXHook* mpHook;                 // +0x00
        u32      maReservedToBlends[4];  // +0x04 .. +0x13 (runtime cursor/timing)
        PFXHookNodeBlend maNodeBlends[1];// +0x14 - up to mpHook->miNodeCount slots
    };
}

#endif
