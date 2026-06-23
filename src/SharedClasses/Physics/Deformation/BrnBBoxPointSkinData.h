#pragma once

// ============================================================================
// SharedClasses/Physics/Deformation/BrnBBoxPointSkinData.h
//
// BrnPhysics::Deformation::BBoxPointSkinData -- the per-bounding-box-point skin data used
// by the body-part bounding-box spec to transform a skinned BBox corner point through a
// bone frame. Only the HackSwapHandedness operation is reached in this pass.
//
// KEYSTONE (hand-vectorised VMX -- NOT reconstructed): HackSwapHandedness @ 0x825E6DB8 is
// a multi-stage AltiVec/VMX128 pipeline (lvx128 of four 16-byte rows at the source frame
// r4+0x00/0x10/0x20/0x30; vsubfp negation; a cascade of vmrglw/vmrghw lane interleaves;
// vmulfp128 + a vmaddfp accumulation chain forming a matrix*point transform; a
// vrlimi128 handedness flip gated by an undecoded rodata constant flt_820037C8; three
// stvx128 stores back to the destination point at r3). Per the project rule, a multi-stage
// VMX pipeline over undecoded rodata does NOT reliably lower to scalar C++ and must not be
// paraphrased to an invented per-lane formula. The body is therefore an HONEST documented
// stub; the named operands are modelled from the asm (the r3 destination point and the r4
// source frame) so the declaration and call site are real, but the transform itself is
// left for a VMX-aware reconstruction pass. See BrnBBoxPointSkinData.cpp.
//
// X360 pointers are 32-bit; members here are pinned BY NAME. GROW additively.
// ============================================================================

#include "types.hpp"

namespace BrnPhysics
{
namespace Deformation
{
	// The source bone/transform frame HackSwapHandedness reads: four 16-byte rows
	// (lvx128 at +0x00/+0x10/+0x20/+0x30). Modelled as four 4-lane rows so the loads land
	// on named members. The +0x30 row is negated (vsubfp v0,v0) before use.
	struct BBoxSkinFrame
	{
		f32 maRow0[4];   // r4 +0x00
		f32 maRow1[4];   // r4 +0x10
		f32 maRow2[4];   // r4 +0x20
		f32 maRow3[4];   // r4 +0x30
	};

	struct BBoxPointSkinData
	{
		// The destination point (r3) the transform reads and writes (lvx128/stvx128 at r3).
		f32 maPoint[4];

		// BBoxPointSkinData::HackSwapHandedness @ 0x825E6DB8. Transform maPoint through the
		// source frame with a handedness flip. KEYSTONE: VMX pipeline, see banner / .cpp.
		// Caller (X360 xref): BrnPhysics::Deformation::BodyPartBBoxSpec::HackCheckHandedness.
		void HackSwapHandedness( const BBoxSkinFrame& arFrame );
	};
}
}
