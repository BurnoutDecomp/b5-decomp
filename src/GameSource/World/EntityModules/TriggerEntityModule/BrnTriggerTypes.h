#ifndef BRN_TRIGGER_TYPES_H
#define BRN_TRIGGER_TYPES_H

#include "types.hpp"
#include "rw/math/vpu/types.h"   // rw::math::vpu::Vector3 / Matrix44Affine

// Trigger entity types. ETriggerTypeID is recovered verbatim from the DecFIGS DWARF
// (BrnTriggerTypes.h). The Trigger record layout is pinned from the X360 ARTIST
// TriggerEntityModuleDebugComponent::RenderWorld (0x822DA1F0): the trigger array stride is 128
// bytes and the debug renderer reads meType @ +12, the transform @ +32 (a 64-byte affine matrix,
// W/position column @ +80), the dimensions @ +96, and the radius @ +112. The trigger's gameplay
// HANDLE word lives at +8 (X360 TriggerEntityModule::ProcessAddTriggerEvents writes the add-event
// handle there; ProcessLineTestFineResult reads it back to publish the per-overlap handle list).
// The intervening spans are not touched by these slices and are preserved with explicit padding so
// every accessed field is named. FLAG: the leading entity-handle/collision-tag word at +0..+7 and
// the trailing fields after +116 are not yet reconstructed.

namespace BrnWorld
{
    enum ETriggerTypeID
    {
        E_TRIGGERTYPE_INVALID       = -1,
        E_TRIGGERTYPE_PLANE_SEGMENT = 0,
        E_TRIGGERTYPE_SPHERE        = 1,
        E_TRIGGERTYPE_BOX           = 2,
        E_TRIGGERTYPE_COUNT         = 3,
    };

    // Plane-segment triggers are built as a very thin box. ProcessAddTriggerEvents feeds this
    // value DIRECTLY as the box's Z half-extent (the asm literal 0.0099999998 -- no x0.5 in the
    // plane branch; only the BOX case halves its dimensions), so the plane volume is 0.01 thick
    // on each side of the plane.
    static const f32 KF_PLANE_SEGMENT_TRIGGER_DEPTH = 0.01f;

    struct Trigger
    {
        u8                          mPad0[8];    // +0   entity handle / collision tag (unrecovered)
        u32                         mHandle;     // +8   gameplay trigger handle (the add-event handle,
                                                 //      republished per-overlap by ProcessLineTestFineResult)
        ETriggerTypeID              meType;      // +12  E_TRIGGERTYPE_*
        u8                          mPad1[16];   // +16  (pad to the 16-byte aligned transform)
        rw::math::vpu::Matrix44Affine mTransform; // +32  world transform (W column == position)
        rw::math::vpu::Vector3      mDimensions; // +96  box half-extents source (full dimensions)
        f32                         mfRadius;    // +112 sphere radius
        u8                          mPad2[12];   // +116 (pad to the 128-byte record stride)

        // X360 ProcessAddTriggerEvents writes the add-event gameplay handle into the +8 word;
        // ProcessLineTestFineResult reads it back to publish the trigger-overlap list.
        u32  GetHandle() const            { return mHandle; }
        void SetHandle(u32 luHandle)      { mHandle = luHandle; }
    };
}

#endif // BRN_TRIGGER_TYPES_H
