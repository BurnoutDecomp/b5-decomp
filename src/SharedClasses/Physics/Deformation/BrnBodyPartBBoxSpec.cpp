#include "SharedClasses/Physics/Deformation/BrnBodyPartBBoxSpec.h"

// ============================================================================
// BrnPhysics::Deformation::BodyPartBBoxSpec::HackCheckHandedness @ 0x825E6EA0
//
// KEYSTONE -- NOT reconstructed. The X360 body is a hand-vectorised AltiVec/
// VMX128 pipeline that computes the SIGN of the box basis's scalar triple
// product (its winding / handedness) and, only when it is left-handed, mirrors
// every skinned corner point:
//
//   lvx128 v0  <- mOrientation.xAxis      (basis row 0)
//   lvx128 v12 <- mOrientation.yAxis      (basis row 1)
//   lvx128 v11 <- mOrientation.zAxis      (basis row 2)
//   vpermwi128 v10, v0,  0x63             (lane reshuffle of row 0)
//   vpermwi128 v12, v12, 0x63             (lane reshuffle of row 1)
//   vmulfp128  v0,  v0,  v12              (row0 * row1')
//   vnmsubfp   v0,  v10, v0,  v9          (cross-product accumulation)
//   vpermwi128 v0,  v0,  0x63
//   vmsum3fp128 v0, v0,  v11              (dot with row 2 -> scalar triple prod)
//   vcmpgefp   v0, v0, 0                  (sign tests against the zero splat)
//   vcmpeqfp.  v0, v0, 0                  (CR record -> the branch predicate)
//   if ( left-handed ) {
//       for ( i = 0; i < 8; ++i )
//           maCornerSkinData[i].HackSwapHandedness( mOrientation );   // r3 += 0x20
//       mCentreSkinData.HackSwapHandedness( ... );                    // r31 + 0x140
//       mJointSkinData.HackSwapHandedness( ... );                     // r31 + 0x160
//       // vspltisw -1; vslw; vxor; stvx128 -> flip a sign-bit lane in row 0
//   }
//
// Per the project rule on hand-vectorised VMX: a multi-stage pipeline built from
// vpermwi128 lane-permute immediates (0x63), vnmsubfp, vmsum3fp128, and a vsel-
// style vcmpgefp/vcmpeqfp predicate does NOT reliably lower to scalar C++, and
// inventing a per-lane cross-product/permutation formula would be fabrication.
// The dependent BBoxPointSkinData::HackSwapHandedness is itself an already-
// documented VMX keystone stub, so the inner mirror is likewise unrecoverable
// here. The body is therefore left as an HONEST no-op stub: it neither corrupts
// the basis nor the points; it simply does not yet evaluate the handedness test
// or apply the mirror, pending a VMX-aware reconstruction pass that decodes the
// 0x63 permute lanes and the sign-bit flip. The named operands are modelled so
// the declaration and the StreamedDeformationSpec::FixUp call site compile and
// link cleanly.
//
// ⭐⭐ 2026-09-05 (crash wave 2): THE COST OF THIS STUB IS MEASURED, AND IT IS ZERO ON
// SHIPPED DATA. The mirror arm only runs when the streamed box basis is LEFT-handed. Scanning
// every ported VEH_*_AT.BIN -- 429 cars, 11,258 BodyPartBBoxSpec records -- by the same signed
// triple product this function computes (dot(cross(row0, row1), row2)):
//     right-handed  10829
//     LEFT-handed       0
//     degenerate      429   (exactly one all-zero placeholder IK part per car; its whole
//                            orientation is zero, so there is no handedness to test)
// So HackCheckHandedness takes its early-out on every real record the game loads, the mirror
// never fires, and the dependent BBoxPointSkinData::HackSwapHandedness keystone is unreachable
// in practice. That does NOT make either body optional -- it makes the stub non-blocking, and
// it means a bbox orientation bug can never be blamed on this file without first finding a
// left-handed record. Re-measure if new vehicle content is ever authored.
// ⚠️ The same scan pins mOrientation's row 3 (the row this header used to call SIMD padding):
// it is (0, 0, 0, 1) in 10,829 of 10,829 real records -- a genuine affine translation row whose
// value happens to be zero. PhysicalBodyPart::CalculateBoundingBoxExtents loads and uses it.
// ============================================================================

namespace BrnPhysics
{
namespace Deformation
{
    void BodyPartBBoxSpec::HackCheckHandedness()
    {
        // KEYSTONE STUB: VMX triple-product handedness test + point mirror not
        // reconstructed. Reference the operands so the signature is honest about
        // what it consumes/produces without fabricating the per-lane math.
        (void)mOrientation;
        (void)maCornerSkinData;
        (void)mCentreSkinData;
        (void)mJointSkinData;
    }
}
}
