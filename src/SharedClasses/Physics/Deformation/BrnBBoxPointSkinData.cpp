#include "SharedClasses/Physics/Deformation/BrnBBoxPointSkinData.h"

// BrnPhysics::Deformation::BBoxPointSkinData::HackSwapHandedness @ 0x825E6DB8.
//
// KEYSTONE -- NOT reconstructed. The X360 body is a hand-vectorised AltiVec/VMX128
// pipeline:
//   - lvx128 of the four 16-byte source rows (r4 +0x00/+0x10/+0x20/+0x30) and the
//     destination point (r3);
//   - vsubfp v0,v0 negates the +0x30 row;
//   - a cascade of vmrglw / vmrghw lane interleaves builds working vectors;
//   - vspltw broadcasts + vmulfp128 + a vmaddfp accumulation chain apply a matrix*point
//     transform (three accumulation stages);
//   - vrlimi128 v12,v0,8,0 performs a handedness flip, scaled by an undecoded rodata
//     constant flt_820037C8;
//   - three stvx128 stores write the result back to the destination point at r3.
//
// Per the project rule on hand-vectorised VMX over undecoded rodata: this does not
// reliably lower to scalar C++, and inventing a per-lane formula would be fabrication.
// The body is left as an honest no-op stub (it does not corrupt the destination point;
// it simply does not yet apply the transform) pending a VMX-aware reconstruction pass
// that decodes flt_820037C8 and the exact lane permutation. The named operands above are
// modelled so the declaration and call site compile and link cleanly.

namespace BrnPhysics
{
namespace Deformation
{
	void BBoxPointSkinData::HackSwapHandedness( const BBoxSkinFrame& arFrame )
	{
		// KEYSTONE STUB: VMX matrix*point transform with handedness flip not reconstructed.
		// Reference the operands so the signature is honest about what it consumes/produces
		// without fabricating the per-lane math.
		(void)arFrame;
		(void)maPoint;
	}
}
}
