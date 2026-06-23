#include "SharedClasses/Physics/Deformation/BrnBodyPartBBoxSpec.h"

// ============================================================================
// BrnPhysics::Deformation::BodyPartBBoxSpec::HackCheckHandedness @ 0x825E6EA0
//
// KEYSTONE -- NOT reconstructed. The X360 body is a hand-vectorised AltiVec/
// VMX128 pipeline that computes the SIGN of the box basis's scalar triple
// product (its winding / handedness) and, only when it is left-handed, mirrors
// every skinned corner point:
//
//   lvx128 v0  <- mav4Basis[0]            (basis row 0)
//   lvx128 v12 <- mav4Basis[1]            (basis row 1)
//   lvx128 v11 <- mav4Basis[2]            (basis row 2)
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
//           maPoints[i].HackSwapHandedness( *this as frame );   // r3 += 0x20
//       maPoints[8].HackSwapHandedness( ... );                  // r31 + 0x140
//       maPoints[9].HackSwapHandedness( ... );                  // r31 + 0x160
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
        (void)mav4Basis;
        (void)maPoints;
    }
}
}
