#pragma once

// ============================================================================
// SharedClasses/Physics/Deformation/BrnBBoxPointSkinData.h
//
// BrnPhysics::Deformation::BBoxPointSkinData -- the per-bounding-box-point skin data the
// body-part bounding box is built from: a rest VERTEX plus a three-influence skin binding
// (three weights and three bone indices) into the owning IKBodyPart's tag-point / driven-
// point arrays.
//
// ⭐⭐ 2026-09-05 (crash wave 2): THE MEMBER SET IS THE DWARF'S, AND IT IS NOW COMPLETE.
// Until now this struct was a single `f32 maPoint[4]` -- 16 bytes modelled from the only
// body that had been read (HackSwapHandedness, which touches nothing but the vertex). The
// DWARF (BrnIKBodyPartSpec.h:63-65) names three members, and the X360
// PhysicalBodyPart::CalculateSkinnedPoint @0x825E2560 reads all three at the offsets the
// DWARF implies:
//     +0x00  Vector3 mVertex             lvx128 v123, r0, r20   (the blend's seed)
//     +0x10  f32     mafWeights[3]       lfs 0(r27), r27 += 4, three times; and the
//                                        unrolled path's `lvlx v0, r0, r21` + vspltw 0/1/2
//     +0x1C  u8      mauBoneIndices[3]   lbz 0(r29), r29 += 1;  lbz 0x1D(r20), lbz 0x1E(r20)
// The 0x20 stride the BodyPartBBoxSpec loop already used is therefore the real sizeof, and
// the record's 16-byte alignment is asserted by the console itself
// ("Expected lPoint.mafWeights to be 16 byte aligned", BrnPhysicalBodyPart.cpp:218) --
// which only means anything because mafWeights sits at +0x10 of a 16-aligned record.
//
// ⛔ WHY THE SHORT MODEL WAS A LIVE DEFECT, not a harmless partial: with no weights and no
// bone indices there was nothing for CalculateSkinnedPoint to read, so it stayed declare-only
// and every skinned bbox corner resolved to the ORIGIN. Ten identical corners make a
// degenerate box, the box floors at the 0.05 half-extent, and the world-contact generator
// then pads that "thin" box by the console's full 0.5 m anti-tunnelling ceiling -- a ~0.55 m
// near-cube standing in for a flat panel. That is the "detached doors stand on edge like
// headstones" report.
//
// X360 pointers are 32-bit; members here are pinned BY NAME. GROW additively.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3, Matrix44Affine

namespace BrnPhysics
{
namespace Deformation
{
	struct BBoxPointSkinData
	{
		// How many skin influences one bbox control point carries. The X360 loop counter is
		// a literal `li r28, 3` @0x825E2580, and the unrolled path splats exactly lanes
		// 0/1/2 of the weight vector -- three, not four.
		static const s32 KI_NUM_SKIN_INFLUENCES = 3;

		// DWARF BrnIKBodyPartSpec.h:63/64/65.
		Vector3 mVertex;                                 // +0x00
		f32     mafWeights[KI_NUM_SKIN_INFLUENCES];      // +0x10
		u8      mauBoneIndices[KI_NUM_SKIN_INFLUENCES];  // +0x1C
		u8      mu8Pad1F;                                // +0x1F (the record's 0x20 stride)

		// ---- read accessors (the CalculateSkinnedPoint blend consumes exactly these) ----
		const Vector3& GetVertex() const { return mVertex; }
		f32 GetWeight(s32 liInfluence) const { return mafWeights[liInfluence]; }
		u8  GetBoneIndex(s32 liInfluence) const { return mauBoneIndices[liInfluence]; }

		// BBoxPointSkinData::HackSwapHandedness @ 0x825E6DB8. Transform mVertex through the
		// passed frame with a handedness flip. KEYSTONE: VMX pipeline, see banner / .cpp.
		// The DWARF (BrnIKBodyPartSpec.h:60) types the argument Matrix44Affine -- the four
		// 16-byte rows the asm lvx128's at r4 +0x00/+0x10/+0x20/+0x30.
		// Caller (X360 xref): BrnPhysics::Deformation::BodyPartBBoxSpec::HackCheckHandedness.
		void HackSwapHandedness( const Matrix44Affine& arFrame );
	};

	// The 0x20 stride is load-bearing twice over: BodyPartBBoxSpec's ten control points are
	// walked at that stride by CalculateBoundingBoxExtents (`addi r26, r26, 0x20`), and the
	// whole IKBodyPartSpec record is a VIEW over streamed console bytes.
	static_assert(sizeof(BBoxPointSkinData) == 32, "BBoxPointSkinData stride 0x20");
	static_assert(alignof(BBoxPointSkinData) == 16, "BBoxPointSkinData is 16-byte aligned (the console asserts it)");
}
}
