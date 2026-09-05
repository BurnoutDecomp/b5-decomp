#include "SharedClasses/Physics/Deformation/BrnBBoxPointSkinData.h"

// BrnPhysics::Deformation::BBoxPointSkinData::HackSwapHandedness @ 0x825E6DB8.
//
// KEYSTONE -- NOT reconstructed. The X360 body is a hand-vectorised AltiVec/VMX128
// pipeline:
//   - lvx128 of the four 16-byte source rows (r4 +0x00/+0x10/+0x20/+0x30) and the
//     destination vertex (r3);
//   - vsubfp v0,v0 negates the +0x30 row;
//   - a cascade of vmrglw / vmrghw lane interleaves builds working vectors;
//   - vspltw broadcasts + vmulfp128 + a vmaddfp accumulation chain apply a matrix*point
//     transform (three accumulation stages);
//   - vrlimi128 v12,v0,8,0 performs a handedness flip, scaled by an undecoded rodata
//     constant flt_820037C8;
//   - three stvx128 stores write the result back to the destination vertex at r3.
//
// Per the project rule on hand-vectorised VMX over undecoded rodata: this does not
// reliably lower to scalar C++, and inventing a per-lane formula would be fabrication.
// The body is left as an honest no-op stub (it does not corrupt the destination vertex;
// it simply does not yet apply the transform) pending a VMX-aware reconstruction pass
// that decodes flt_820037C8 and the exact lane permutation.
//
// ⭐ 2026-09-05: the invented `BBoxSkinFrame` argument type is GONE. It modelled "four
// 16-byte rows at r4" because that is all the asm showed; the DWARF (BrnIKBodyPartSpec.h:60)
// names the parameter Matrix44Affine, which is exactly those four rows and is a type this
// tree already owns. The skin members this struct grew in the same commit (mafWeights /
// mauBoneIndices) are NOT touched by the handedness swap -- it moves the vertex only, which
// is why reading only the vertex was enough to model it and not enough to model the struct.
//
// STILL A STUB, AND IT MATTERS ELSEWHERE, NOT HERE: HackCheckHandedness only mirrors the
// control points when the streamed box basis is LEFT-handed, so a no-op leaves right-handed
// specs exactly right and mirrors nothing on the others. It does not zero anything, so it
// cannot re-degenerate the box CalculateSkinnedPoint now builds.

namespace BrnPhysics
{
namespace Deformation
{
	void BBoxPointSkinData::HackSwapHandedness( const Matrix44Affine& arFrame )
	{
		// KEYSTONE STUB: VMX matrix*point transform with handedness flip not reconstructed.
		// Reference the operands so the signature is honest about what it consumes/produces
		// without fabricating the per-lane math.
		(void)arFrame;
		(void)mVertex;
	}
}
}
