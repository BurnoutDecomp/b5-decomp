#pragma once

// ============================================================================
// SharedClasses/Physics/Deformation/BrnBodyPartBBoxSpec.h
//
// BrnPhysics::Deformation::BodyPartBBoxSpec -- the per-body-part oriented
// bounding-box spec used by the streamed deformation system. It carries the box
// ORIENTATION (a full Matrix44Affine) plus the ten skinned BBox corner/control
// points (BBoxPointSkinData), and is fixed-up at stream-in time by
// StreamedDeformationSpec::FixUp.
//
// Layout (DWARF BrnIKBodyPartSpec.h:82-85; X360 offsets from the two bodies that walk it):
//   +0x000  Matrix44Affine    mOrientation        4 rows, +0x00/+0x10/+0x20/+0x30
//   +0x040  BBoxPointSkinData maCornerSkinData[8] (stride 0x20, ends at +0x140)
//   +0x140  BBoxPointSkinData mCentreSkinData
//   +0x160  BBoxPointSkinData mJointSkinData
//
// ⭐⭐ 2026-09-05 (crash wave 2): THE LEADING 64 BYTES ARE A MATRIX, NOT "3 basis rows +
// a pad". They were modelled `f32 mav4Basis[3][4]; f32 maPad30[4];` from
// HackCheckHandedness alone, which loads only the three basis rows for its triple product.
// PhysicalBodyPart::CalculateBoundingBoxExtents @0x825E2B80 loads ALL FOUR:
//     0x825E2BD8  addi r29, r11, 0x40      ; r29 = &mOrientation   (r11 = the IKBodyPartSpec)
//     0x825E2BE8  addi r24, r29, 0x10      ; row 1
//     0x825E2BEC  addi r23, r29, 0x20      ; row 2
//     0x825E2BE4  addi r25, r29, 0x30      ; row 3  <-- the "pad"
//     0x825E2C28  vmaddfp v13, v12, v13, v8   ; v13 = row0 * px + row3
//     0x825E2C38  vmaddfp v13, v11, v13, v7   ; v13 = row1 * py + v13
//     0x825E2C3C  vmaddfp v0,  v10, v13, v0   ; v0  = row2 * pz + v13
// (classic vmaddfp prints raw field order D,A,B,C == D = A*C + B). Row 3 is the
// TRANSLATION of an ordinary affine point transform -- it was never padding, and calling it
// padding is what let the extents builder skip the transform entirely.
//   +0x040  maCornerSkinData[0..7]  (the 8-iteration loop, `addi r26, r26, 0x20`)
//   +0x140  mCentreSkinData         (`addi r5, r29, 0x140`)
//   +0x160  mJointSkinData          (`addi r5, r29, 0x160`)
//
// X360 pointers are 32-bit; members are pinned BY NAME (no raw-offset access).
// GROW additively.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // Matrix44Affine

#include "SharedClasses/Physics/Deformation/BrnBBoxPointSkinData.h"

namespace BrnPhysics
{
namespace Deformation
{
    struct BodyPartBBoxSpec
    {
        // The ten skinned control points of the body-part bounding box: eight box
        // corners plus the centre and the joint point. The DWARF splits them into an
        // [8] array and two singles; they are contiguous, which is what lets
        // CalculateBoundingBoxExtents walk the first eight with one strided loop and
        // then address the last two by their own offsets.
        static const s32 KI_NUM_BBOX_CORNER_POINTS = 8;
        static const s32 KI_NUM_BBOX_POINTS        = KI_NUM_BBOX_CORNER_POINTS + 2;

        Matrix44Affine    mOrientation;                                  // +0x000 (DWARF :82)
        BBoxPointSkinData maCornerSkinData[KI_NUM_BBOX_CORNER_POINTS];   // +0x040 (DWARF :83)
        BBoxPointSkinData mCentreSkinData;                               // +0x140 (DWARF :84)
        BBoxPointSkinData mJointSkinData;                                // +0x160 (DWARF :85)

        // The ten control points as one contiguous run, in the order
        // CalculateBoundingBoxExtents visits them (corners 0..7, then centre, then joint).
        // Spelled as an accessor rather than a fork of the member set so both the DWARF's
        // names and the console's single strided walk are expressible.
        const BBoxPointSkinData& GetSkinPoint(s32 liIndex) const
        {
            if ( liIndex < KI_NUM_BBOX_CORNER_POINTS )
            {
                return maCornerSkinData[liIndex];
            }
            return (liIndex == KI_NUM_BBOX_CORNER_POINTS) ? mCentreSkinData : mJointSkinData;
        }

        // BodyPartBBoxSpec::HackCheckHandedness @ 0x825E6EA0. Tests the signed
        // triple product (winding) of the orientation's basis rows and, when it is
        // left-handed, mirrors every skinned point through
        // BBoxPointSkinData::HackSwapHandedness and flips a sign bit lane in the basis.
        // KEYSTONE: VMX pipeline -- see the banner in BrnBodyPartBBoxSpec.cpp.
        // Caller (X360 xref): StreamedDeformationSpec::FixUp.
        void HackCheckHandedness();
    };

    // The record is a VIEW over streamed console bytes inside IKBodyPartSpec (which is
    // itself stride-480), so these are contracts against the DATA, not merely against the
    // asm: 0x40 orientation + 10 * 0x20 control points == 0x180.
    static_assert(sizeof(BodyPartBBoxSpec) == 0x180, "BodyPartBBoxSpec is 384 bytes");
}
}
