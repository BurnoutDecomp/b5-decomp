#ifndef BRN_TRIGGER_TYPES_H
#define BRN_TRIGGER_TYPES_H

#include "types.hpp"
#include "rw/math/vpu/types.h"   // rw::math::vpu::Vector3 / Matrix44Affine

// Trigger entity types. ETriggerTypeID is recovered verbatim from the DecFIGS DWARF
// (BrnTriggerTypes.h). The Trigger record layout is pinned from the X360 ARTIST
// TriggerEntityModuleDebugComponent::RenderWorld (0x822DA1F0): the trigger array stride is 128
// bytes and the debug renderer reads meType @ +12, the transform @ +32 (a 64-byte affine matrix,
// W/position column @ +80), the dimensions @ +96, and the radius @ +112. The intervening spans are
// not touched by this slice and are preserved with explicit padding so every accessed field is
// named. FLAG: the full Trigger record (entity handle / collision tag at +0, trailing fields after
// +116) is not yet reconstructed.

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

    // Plane-segment triggers are drawn as a thin box; the X360 RenderWorld halves this depth
    // (0.005 == KF_PLANE_SEGMENT_TRIGGER_DEPTH * 0.5) into the box's Z half-extent.
    static const f32 KF_PLANE_SEGMENT_TRIGGER_DEPTH = 0.01f;

    struct Trigger
    {
        u8                          mPad0[12];   // +0   entity handle / collision tag (unrecovered)
        ETriggerTypeID              meType;      // +12  E_TRIGGERTYPE_*
        u8                          mPad1[16];   // +16  (pad to the 16-byte aligned transform)
        rw::math::vpu::Matrix44Affine mTransform; // +32  world transform (W column == position)
        rw::math::vpu::Vector3      mDimensions; // +96  box half-extents source (full dimensions)
        f32                         mfRadius;    // +112 sphere radius
        u8                          mPad2[12];   // +116 (pad to the 128-byte record stride)
    };
}

#endif // BRN_TRIGGER_TYPES_H
