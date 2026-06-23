#pragma once

// ============================================================================
// SharedClasses/Physics/Deformation/BrnBodyPartBBoxSpec.h
//
// BrnPhysics::Deformation::BodyPartBBoxSpec -- the per-body-part oriented
// bounding-box spec used by the streamed deformation system. It carries the box
// basis (three 16-byte vector rows) plus the ten skinned BBox corner/control
// points (BBoxPointSkinData), and is fixed-up at stream-in time by
// StreamedDeformationSpec::FixUp.
//
// Layout authority is the X360 ARTIST.XEX (HackCheckHandedness @ 0x825E6EA0):
//   +0x000  basis row 0   (lvx128 r0,r31)                  -- mav4Basis[0]
//   +0x010  basis row 1   (lvx128 r31,0x10)                -- mav4Basis[1]
//   +0x020  basis row 2   (lvx128 r31,0x20; vmsum3 operand)-- mav4Basis[2]
//   +0x040  maPoints[0]   (8-point loop base, stride 0x20) -- 8 x BBoxPointSkinData
//    ...      ... maPoints[7] ends at +0x140
//   +0x140  maPoints[8]   (r31+0x140 HackSwapHandedness)
//   +0x160  maPoints[9]   (r31+0x160 HackSwapHandedness)
// BBoxPointSkinData is 0x20 bytes (the 0x20 loop stride), matching the committed
// BrnBBoxPointSkinData home (a 16-byte point + the frame the transform reads).
//
// X360 pointers are 32-bit; members are pinned BY NAME (no raw-offset access).
// GROW additively.
// ============================================================================

#include "types.hpp"

#include "SharedClasses/Physics/Deformation/BrnBBoxPointSkinData.h"

namespace BrnPhysics
{
namespace Deformation
{
    struct BodyPartBBoxSpec
    {
        // The ten skinned points of the body-part bounding box. The first eight
        // form the box corners walked by the 8-iteration handedness-swap loop;
        // the trailing two (maPoints[8] @ +0x140, maPoints[9] @ +0x160) are the
        // extra control points the X360 swaps after the loop.
        static const s32 KI_NUM_BBOX_POINTS = 10;

        // The 4-lane box basis rows the handedness check loads (lvx128 at +0x00 /
        // +0x10 / +0x20). The first three lanes carry the basis vectors; the 4th
        // lane is the SIMD pad. maPoints starts at +0x40, i.e. one 16-byte row of
        // padding follows the three basis rows in the X360 layout.
        f32 mav4Basis[3][4];   // +0x00 .. +0x2F
        f32 maPad30[4];        // +0x30 (SIMD alignment; maPoints lands at +0x40)

        BBoxPointSkinData maPoints[KI_NUM_BBOX_POINTS];   // +0x40 (stride 0x20)

        // BodyPartBBoxSpec::HackCheckHandedness @ 0x825E6EA0. Tests the signed
        // triple product (winding) of the box basis and, when it is left-handed,
        // mirrors every skinned point through BBoxPointSkinData::HackSwapHandedness
        // and flips a sign bit lane in the basis. KEYSTONE: VMX pipeline -- see
        // the banner in BrnBodyPartBBoxSpec.cpp.
        // Caller (X360 xref): StreamedDeformationSpec::FixUp.
        void HackCheckHandedness();
    };
}
}
