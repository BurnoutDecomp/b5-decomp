#include "GameShared/GameClasses/Geometric/Primitives/CgsBox.h"

#include <cmath>                              // std::fabs -- the `vandc v,v,0x80000000` sign clear
#include "rw/math/vpu/vector3_operation.h"    // Dot / Cross / MagnitudeSquared / IsValid / GetVector3_?Axis
#include "rw/math/vpu/vector4_operation.h"    // And / CompLessThan over VecFloat + MaskScalar

// ==================================================================================================
// GameShared/GameClasses/Geometric/Primitives/CgsBox.cpp
//
// The three ORIENTED-BOX bodies the tree has never had, reconstructed from
// BURNOUT_X360_ARTIST.XEX (wave Q7, cluster `box`):
//
//   CgsGeometric::Box::IsValid          @0x825BEB80  (264 insns, 0x825BEB80..0x825BEF98)
//   CgsGeometric::BoxOverlappingTest    @0x825BEFA0  ( 60 insns) -- the (Box&,Box&,Vector3) overload
//   CgsGeometric::BoxOverlappingTest    @0x825BF090  ( 50 insns) -- the (Box&,Box&) overload,
//                                                    unnamed `sub_825BF090` in the export; named
//                                                    from the DWARF (see below)
//   CgsGeometric::GetBoxExtentsAlongAxis             -- NO standalone X360 body; the compiler
//                                                    inlined it into 0x825BEFA0 twice. Recovered by
//                                                    INLINING REVERSAL, see its own banner.
//
// ⭐ WHY A .cpp AT ALL. On the console all four live in CgsBox.h -- the DWARF puts
// GetBoxExtentsAlongAxis at CgsBox.h:143, BoxOverlappingTest(3) at :165, BoxOverlappingTest(2) at
// :190 and Box::IsValid at :53/:119, and Box::Set's own baked __FILE__ string is that same header.
// They are homed here on the PC side for one reason only: Box::Set is an inline in CgsBox.h that
// 55 translation units already compile, and giving it a 264-instruction IsValid inline would put
// that body in every one of them. Behaviour is identical; the ONLY consequence is that this TU
// MUST BE MOUNTED (see the report -- beside CgsSphere.cpp, build_game_exe.bat:2773), because
// Box::Set now really does call IsValid and an unmounted definition is an LNK2019.
// This mirrors the tree's own CgsSphere.cpp / CgsLine.cpp / CgsAxisAlignedBox.cpp precedent, all
// of which home console-header inlines in a sibling .cpp.
//
// ---- THE FOUR .rdata CONSTANTS, BYTE-DUMPED, NOT INFERRED ---------------------------------------
// Every one was read out of the image this wave with a targeted headless `idat` run on a PRIVATE
// copy of the .i64 (scratchpad/waveQ7/ida_box/dump_box.py -> out.json). Raw big-endian bytes:
//
//   w::math::vpu::detail::gIVector  @0x82181500  3F800000 00000000 00000000 00000000  = (1,0,0,0)
//   unk_82181510                    @0x82181510  00000000 3F800000 00000000 00000000  = (0,1,0,0)
//   unk_82181520                    @0x82181520  00000000 00000000 3F800000 00000000  = (0,0,1,0)
//   (unk_82181530                   @0x82181530  00000000 00000000 00000000 3F800000  = (0,0,0,1))
//   flt_82004014                    @0x82004014  3DCCCCCD                             = 0.1f
//   unk_82CDA350                    @0x82CDA350  00010203 14151617 00010203 00010203
//
// The first three are the SDK's gIVector / gJVector / gKVector basis registers -- the tree already
// records that identification independently at vendor rw/math/vpu/vector3_operation.h
// (GetVector3_XAxis/YAxis/ZAxis) and at PropManager_wQ2_05.cpp:194. `unk_82181530` is dumped only
// to show the family is the four unit registers laid out contiguously; IsValid never loads it.
//
// unk_82CDA350 is a `vperm` CONTROL vector, not data: byte i of the result takes source byte
// (control[i] & 0x1F), where 0x00..0x0F index vA and 0x10..0x1F index vB. Decoded --
//     bytes  0..3  = 00 01 02 03  -> vA word 0
//     bytes  4..7  = 14 15 16 17  -> vB word 1
//     bytes  8..11 = 00 01 02 03  -> vA word 0
//     bytes 12..15 = 00 01 02 03  -> vA word 0
// i.e. `vperm vD, vA, vB, unk_82CDA350` == { vA.w0, vB.w1, vA.w0, vA.w0 }. Both of its uses in
// IsValid feed it two `vmsum3fp128` results, and vmsum3fp128 BROADCASTS its scalar into all four
// lanes, so the permute is simply "gather two broadcast scalars into lanes 0 and 1"; the following
// `vrlimi128 vD, v3rd, 2, 0` drops the third scalar into lane 2. Net effect: build the 3-vector
// (error0, error1, error2). That is the whole of it -- there is no cross-lane semantics to guess.
// (The other stack-built control, four copies of 0x0004080C, gathers the top byte of each of the
// four words of a compare mask into one word: it is the mask -> bool packing the compiler uses
// before the `lwz` + `cntlzw` test, i.e. MaskScalar::GetBool. Also not data.)
//
// ---- HOW THE DWARF PINS EVERY SHAPE HERE --------------------------------------------------------
// references/DecFIGS/dwarfdump/GameShared/GameClasses/Geometric/Primitives/CgsBox.h:62/:65/:68 give
// the three free-function declarations verbatim, and the per-body dumps in
// references/DecFIGS/dwarfdump/_compile/BrnPhysicsUnity2.cpp name every local AND every callee:
//   :13101 Box::IsValid           -> local `bool lbIsValid` (CgsBox.h:119); callees
//                                   rw::math::vpu::IsValid x10, VecFloat<VectorAxisW>,
//                                   Matrix44Affine::Matrix44Affine x2, IsOrthogonal3x3, Transpose,
//                                   Matrix33::SetIdentity, CgsNumeric::IsRightHanded,
//                                   Vector3::Vector3, IsNormal3x3, MagnitudeSquared xN,
//                                   SelfSubtract, Subtract x2, VecFloat::VecFloat,
//                                   MaskScalar::GetBool, IsZero, Dot
//   :14283 GetBoxExtentsAlongAxis -> local `VecFloat lMaxExtent` (CgsBox.h:145)
//   :14335 BoxOverlappingTest(3)  -> locals lBox1Pos :167, lBox2Pos :168, lBox1Extents :169,
//                                   lBox2Extents :170, lDistance :172, lOverlapping :174; callee
//                                   TAIL, in order: ... Max x4, operator-, operator+, Abs,
//                                   operator-=, CompLessThan
//   :34873 BoxOverlappingTest(2)  -> local `MaskScalar lOverlapping` (CgsBox.h:192)
// Every statement below is one of those callees over those locals, in the emission order of the asm.
//
// ---- PC LOWERING: THE ONE PLACE THE ARITHMETIC IS NOT BIT-IDENTICAL -----------------------------
// It is inherited from the vendor lowering this file calls, not introduced here, and it is stated
// rather than hidden:
//
//   (1) `rw::math::vpu::Dot` / `MagnitudeSquared` return a plain `float` on this port (vendor
//       vector3_operation.h:117/:136 and the standing convention in its head banner), where the
//       console leaves a VecFloat broadcast from `vmsum3fp128`. vmsum3fp128 sums the three products
//       in ONE fused pass; the scalar form here is `x*x + y*y + z*z` evaluated left to right with
//       an independent rounding per operation. Same value to within one ulp on well-scaled input,
//       not bit-identical in general. Every consumer here is a tolerance comparison or a sign test,
//       so no branch in this file is decided by that last ulp except at exact ties.
//
//   NOT a second divergence, and previously mis-stated here: THE MAX LOWERING IS BIT-EXACT.
//   GetBoxExtentsAlongAxis's `vmaxfp` is spelled as the host ternary `a > b ? a : b`, and the
//   AltiVec PEM defines vmaxfp as exactly `if (vA) > (vB) then vD <- vA else vD <- vB` -- the same
//   operand order, therefore the same tie disposition and the same QNaN disposition (a QNaN
//   compares false, so both forms yield the second operand). There is nothing to state about it.
//   (This file does NOT call the vendor `Max(Vector4,Vector4)`; it inlines the ternary at the
//   reduction itself.)
//
// NOT a divergence, spelled literally on purpose (AGENTS.md gotcha 4 -- NaN polarity):
//   * the overlap verdict is written `!(lDistance >= 0)` (the console's `vcmpgefp` + `vnot`), NOT
//     `lDistance < 0`. On a NaN lane the console returns TRUE (overlapping); a host `<` would
//     return false. The vendor `CompLessThan(Vector4,Vector4)` is bodied as exactly that negation
//     (vector4_operation.h:136), which is why it is the call used.
//   * the orthogonality / normality verdicts are written `!(fabs(error) > tolerance)` (the
//     console's `vcmpgtfp` then the `cntlzw`-of-the-stored-mask inversion), so a NaN error reads
//     as "within tolerance". The PS3 SDK spells its IsZero the other way round
//     (`vcmpgefp_p(2, tolerance, Abs(v))`, matrix33/scalar_operation_platform_inline.h:105), which
//     is FALSE on NaN. ARTIST is rung 1, so the X360 form is what is reproduced.
// ==================================================================================================

namespace vpu = rw::math::vpu;

namespace CgsGeometric
{
    // ==============================================================================================
    // CgsGeometric::Box::IsValid @0x825BEB80  (264 instructions, 0x825BEB80..0x825BEF98)
    //
    // Nine independent predicates, ALL of them evaluated (the console has no short-circuit at this
    // level -- every one lands in its own GPR and the eight `and`s at 0x825BEF68..0x825BEF90 fold
    // them at the end), then one bit returned: `clrlwi r3, r11, 31 ; blr`.
    //
    // The DWARF's own spelling of the same nine, in source order:
    //     IsValid(dimensions) && IsValid(fatness)
    //  && IsValid(right) && IsValid(up) && IsValid(at) && IsValid(pos)
    //  && IsOrthogonal3x3(transform, tol) && IsNormal3x3(transform, tol)
    //  && IsRightHanded(transform).GetBool()
    //
    // ⚠️ THREE OF THOSE HELPERS ARE SPELLED OUT INLINE BELOW BECAUSE THEY HAVE NO HOME IN THIS TREE,
    //    and this cluster owns only CgsBox.{h,cpp}. Reported as header requests, exactly as
    //    PropManager_wQ2_05.cpp:811 already had to record for the first of them:
    //      rw::math::vpu::IsOrthogonal3x3(Matrix44Affine, VecFloat)  -- canonical home
    //          vendor rw/math/vpu/matrix44affine_operation.h; SDK body (Feb-2007
    //          detail/matrix44affine_operation_inline.h:874-891 + matrix33_operation_inline.h:61)
    //          is IsZero(GetOrthogonalError3x3(m), tol), and GetOrthogonalError is
    //          Mult(m, Transpose(m)) - identity -> per-row MagnitudeSquared -> MagnitudeSquared.
    //      rw::math::vpu::IsNormal3x3(Matrix44Affine, VecFloat)      -- same home; SDK body is
    //          IsZero(GetNormalError3x3(m), tol) with GetNormalError = the three
    //          MagnitudeSquared(row) - 1 assembled into a Vector3, then MagnitudeSquared.
    //      CgsNumeric::IsRightHanded(Matrix44Affine) -> MaskScalar   -- canonical home
    //          GameShared/GameClasses/Numeric/CgsVectorUtilities.h:109 (DWARF), a file that does
    //          not exist in this tree at all; its body's DWARF local is `Vector3 lRightHandedZ`
    //          at :111. BrnPhysicalBodyPart.cpp:218-228 already spells the same helper inline for
    //          the same reason.
    //    Nothing is invented by spelling them here: each block below is the instruction range that
    //    produced it, and the SDK source shape it matches is named.
    // ==============================================================================================
    bool Box::IsValid() const
    {
        // ---- (a) the per-lane NaN screen, 0x825BEB80..0x825BED94 ---------------------------------
        // Ten `vspltw` + `vcmpeqfp.` self-compare pairs: a lane equals itself iff it is not NaN, and
        // `mfocrf r11,2 ; extrwi r11,r11,1,24` reads CR6[0], the "ALL lanes compared true" bit
        // (`srwi r11,r11,7` at 0x825BEC00 / 0x825BEF44 reads the same bit). Because every operand
        // is a one-lane splat, "all lanes" and "this lane" are the same test.
        //
        // The five rows are screened in this order, and the WIDTHS are what type each test:
        //   +0x40 lanes 0/1/2 (dimensions, a Vector3)  0x825BEB88..0x825BEBE4 -> r9
        //   +0x40 lane  3     (fatness, a VecFloat)    0x825BEBE8..0x825BEC10 -> ANDed into r4
        //   +0x00 lanes 0/1/2 (transform right)        0x825BEBFC..0x825BEC64 -> r5
        //   +0x10 lanes 0/1/2 (transform up)           0x825BEC68..0x825BECC8 -> r6
        //   +0x20 lanes 0/1/2 (transform at)           0x825BECCC..0x825BED2C -> r7
        //   +0x30 lanes 0/1/2 (transform pos)          0x825BED30..0x825BED90 -> r8
        // The +0x30 row is screened and then DROPPED -- `vspltw v11, v11, 2` at 0x825BED70 clobbers
        // the register that held it, and nothing after this point reads the translation. Only the
        // three basis rows feed the orthonormality and handedness tests.
        const Vector3 lDimensions = GetDimensions();
        const bool lbDimensionsValid = vpu::IsValid(lDimensions);          // 3 splats + 3 vcmpeqfp.

        // The fatness lane. The DWARF names `VecFloat<VectorAxisW>` immediately beside an IsValid,
        // i.e. IsValid(VecFloat(mDimensionsAndFatness.W())); there is no IsValid(VecFloat) in this
        // tree, so the one-lane self-compare it reduces to is written out.
        const f32 lfFatness = GetFatness().w;
        const bool lbFatnessValid = (lfFatness == lfFatness);              // vspltw v0,v0,3 + vcmpeqfp.

        const Matrix44Affine lTransform = GetTransform();                  // DWARF Matrix44Affine ctor
        const bool lbRightValid = vpu::IsValid(lTransform.Right());        // +0x00
        const bool lbUpValid    = vpu::IsValid(lTransform.Up());           // +0x10
        const bool lbAtValid    = vpu::IsValid(lTransform.At());           // +0x20
        const bool lbPosValid   = vpu::IsValid(lTransform.Pos());          // +0x30

        // ---- (b) ORTHOGONALITY, 0x825BED94..0x825BEEE4 -------------------------------------------
        // rw::math::vpu::IsOrthogonal3x3(lTransform, tolerance), inlined.
        //
        // 0x825BED98..0x825BEDF8 are three `vmrghw`/`vmrglw` pairs that TRANSPOSE the 3x3 into its
        // columns -- (r.x,u.x,a.x), (r.y,u.y,a.y), (r.z,u.z,a.z) -- which is the SDK's
        // `Transpose(matrix)` (DWARF names it). 0x825BEE08..0x825BEE5C are three `vmulfp128` plus
        // six `vmaddfp`: each column triple is combined with the splatted components of ONE row, so
        // lane j of result i is Dot(row_i, row_j) -- that is `Mult(m, Transpose(m))`, the Gram
        // matrix. 0x825BEE64/6C/74 subtract the identity ROW BY ROW from the three dumped basis
        // registers (`SelfSubtract(result, identity)` + `Matrix33::SetIdentity`, both named by the
        // DWARF). 0x825BEE90/94/9C are three `vmsum3fp128` == MagnitudeSquared of each residual row;
        // 0x825BEEA0 + 0x825BEEB0 gather them into one Vector3 (the permute decoded in the head
        // banner); 0x825BEECC is a FOURTH vmsum3fp128 over that vector; 0x825BEED4 `vandc` clears
        // the sign bit (Abs) and 0x825BEEE4 `vcmpgtfp v11, v11, v4` compares it against the
        // broadcast tolerance -- i.e. IsZero(error, tolerance) inlined on a broadcast scalar.
        const Vector3& lrRight = lTransform.Right();
        const Vector3& lrUp    = lTransform.Up();
        const Vector3& lrAt    = lTransform.At();

        const Vector3 lGramErrorRight = Vector3{ vpu::Dot(lrRight, lrRight),
                                                 vpu::Dot(lrRight, lrUp),
                                                 vpu::Dot(lrRight, lrAt),
                                                 0.0f } - vpu::GetVector3_XAxis();   // - gIVector
        const Vector3 lGramErrorUp    = Vector3{ vpu::Dot(lrUp, lrRight),
                                                 vpu::Dot(lrUp, lrUp),
                                                 vpu::Dot(lrUp, lrAt),
                                                 0.0f } - vpu::GetVector3_YAxis();   // - 0x82181510
        const Vector3 lGramErrorAt    = Vector3{ vpu::Dot(lrAt, lrRight),
                                                 vpu::Dot(lrAt, lrUp),
                                                 vpu::Dot(lrAt, lrAt),
                                                 0.0f } - vpu::GetVector3_ZAxis();   // - 0x82181520

        const Vector3 lOrthogonalError = Vector3{ vpu::MagnitudeSquared(lGramErrorRight),
                                                  vpu::MagnitudeSquared(lGramErrorUp),
                                                  vpu::MagnitudeSquared(lGramErrorAt),
                                                  0.0f };
        const f32 lfOrthogonalError = vpu::MagnitudeSquared(lOrthogonalError);
        const bool lbOrthogonal = !(std::fabs(lfOrthogonalError) > KF_BOX_VALIDITY_TOLERANCE);

        // ---- (c) NORMALITY, 0x825BEE14/54/78 + 0x825BEEA4..0x825BEF00 ----------------------------
        // rw::math::vpu::IsNormal3x3(lTransform, tolerance), inlined: three `vmsum3fp128` of each
        // row with itself (0x825BEE14 right, 0x825BEE54 at, 0x825BEE78 up -- scheduled apart, the
        // same three MagnitudeSquared the DWARF names), each minus the exact 1.0f that
        // `vspltisw v10,1 ; vcfsx v10,v10,0` materialises (0x825BEE68/0x825BEE80), gathered by the
        // same permute pair, one more vmsum3fp128, `vandc` (Abs) and `vcmpgtfp v10, v10, v5`
        // against the SAME tolerance loaded a second time.
        const Vector3 lNormalError = Vector3{ vpu::MagnitudeSquared(lrRight) - 1.0f,
                                              vpu::MagnitudeSquared(lrUp)    - 1.0f,
                                              vpu::MagnitudeSquared(lrAt)    - 1.0f,
                                              0.0f };
        const f32 lfNormalError = vpu::MagnitudeSquared(lNormalError);
        const bool lbNormal = !(std::fabs(lfNormalError) > KF_BOX_VALIDITY_TOLERANCE);

        // ---- (d) HANDEDNESS, 0x825BEED8..0x825BEF44 ----------------------------------------------
        // CgsNumeric::IsRightHanded(lTransform).GetBool(), inlined. The two `vpermwi128 ..,0x63`
        // (a YZXW swizzle: the immediate's four 2-bit fields are 01|10|00|11) plus `vmulfp128` and
        // `vnmsubfp` at 0x825BEEDC..0x825BEF30 are the SDK's Cross(right, up); `vmsum3fp128 v0,v0,v12`
        // is Dot(that, at); `vcmpgefp v0, v0, 0` is the sign test, and the trailing
        // `vcmpeqfp. ; mfocrf ; srwi 7 ; not` at 0x825BEF3C..0x825BEF54 is MaskScalar::GetBool
        // (true iff at least one lane of the mask is non-zero).
        // DWARF local name for the cross product: `Vector3 lRightHandedZ` (CgsVectorUtilities.h:111).
        const Vector3 lRightHandedZ = vpu::Cross(lrRight, lrUp);
        const bool lbRightHanded = (vpu::Dot(lRightHandedZ, lrAt) >= 0.0f);

        // ---- (e) the fold, 0x825BEF48..0x825BEF98 ------------------------------------------------
        // The console ANDs in the reverse of the evaluation order (handedness first, dimensions
        // last) purely because that is the order its GPRs came free; the result is identical.
        // DWARF local: `bool lbIsValid` (CgsBox.h:119).
        const bool lbIsValid = lbRightHanded && lbNormal && lbOrthogonal
                            && lbPosValid && lbAtValid && lbUpValid && lbRightValid
                            && lbFatnessValid && lbDimensionsValid;
        return lbIsValid;
    }

    // ==============================================================================================
    // CgsGeometric::GetBoxExtentsAlongAxis -- DWARF CgsBox.h:143, local `VecFloat lMaxExtent` at :145.
    //
    // ⚠️ NO STANDALONE X360 SYMBOL, AND THAT IS NOT A GAP. It is absent from progress/status.json
    // and from progress/identity.json because the compiler INLINED it -- twice, into
    // BoxOverlappingTest @0x825BEFA0, once per box. This body is AGENTS.md's "inlining reversal"
    // over those two copies, which are instruction-for-instruction identical up to register
    // allocation; the DWARF supplies the signature and the local name. Nothing here is inferred:
    // every operation below is an instruction in 0x825BEFA0's 60, and the DWARF's callee tail for
    // that function counts exactly what two copies of this body need -- Dot x6, Abs<VecFloat> x6,
    // operator*<VectorAxisX/Y/Z> x2 each, Max<VecFloat> x4.
    //
    // ⚠️ IT IS A MAX, NOT A SUM. A textbook separating-axis test projects a box's radius as
    // SUM_i |axis . row_i| * halfDim_i. This one takes the MAX of the three terms
    // (`vmaxfp` at 0x825BF044 / 0x825BF050 for box 2 and 0x825BF06C / 0x825BF074 for box 1 -- four
    // vmaxfp, zero vaddfp inside the per-box reduction; the single `vaddfp` at 0x825BF07C is the
    // one that adds the two BOXES' extents together). Max <= Sum, so this is a DELIBERATELY
    // TIGHTER, non-conservative extent: it can report "separated" for boxes that actually touch.
    // That is what the console ships and it is not corrected here.
    //
    // The three terms are accumulated in the console's order -- x seeds, then y, then z -- with the
    // NEW term as vmaxfp's first operand each time (`vmaxfp vD, vNew, vAcc`).
    // ==============================================================================================
    VecFloat GetBoxExtentsAlongAxis(const Box& lrBox, Vector3 lAxis)
    {
        const Matrix44Affine lTransform  = lrBox.GetTransform();
        const Vector3        lDimensions = lrBox.GetDimensions();

        // `vmsum3fp128 v,axis,row` -> `vandc v,v,0x80000000` (Abs) -> `vmulfp128 v,v,splat(dim.?)`
        const f32 lfRightExtent = std::fabs(vpu::Dot(lAxis, lTransform.Right())) * lDimensions.x;
        const f32 lfUpExtent    = std::fabs(vpu::Dot(lAxis, lTransform.Up()))    * lDimensions.y;
        const f32 lfAtExtent    = std::fabs(vpu::Dot(lAxis, lTransform.At()))    * lDimensions.z;

        f32 lfMaxExtent = lfRightExtent;                                        // seed
        lfMaxExtent = (lfUpExtent > lfMaxExtent) ? lfUpExtent : lfMaxExtent;    // vmaxfp
        lfMaxExtent = (lfAtExtent > lfMaxExtent) ? lfAtExtent : lfMaxExtent;    // vmaxfp

        // The console leaves the result broadcast in a VMX register (every producer here is a
        // vmsum3fp128/vmaxfp, all four lanes equal); the DWARF types it VecFloat.
        return VecFloat{ lfMaxExtent, lfMaxExtent, lfMaxExtent, lfMaxExtent };
    }

    // ==============================================================================================
    // CgsGeometric::BoxOverlappingTest(const Box&, const Box&, Vector3) @0x825BEFA0  (60 insns)
    //
    // ONE separating-axis test along ONE caller-supplied axis. Register map read off the body:
    // r3 = lBox1, r4 = lBox2, v1 = lAxis (the axis is the only VMX argument and is consumed by
    // every `vmsum3fp128`), result in v1 (`vnot v1, v0 ; blr`) -- a MaskScalar, per DWARF :165.
    //
    // The console computes box 2's terms first and box 1's second; the source order (which is what
    // the DWARF's local declaration lines :167-:174 record) is box 1 first. Only scheduling.
    //
    // The verdict, 0x825BF07C..0x825BF08C, literally:
    //     vaddfp   v13, v13, v12   ; extents1 + extents2
    //     vsubfp   v0,  v0,  v13   ; lDistance -= (extents1 + extents2)      <- DWARF operator-=
    //     vcmpgefp v0,  v0,  v4    ; v4 == vspltisw 0 -> (lDistance >= 0) == SEPARATED
    //     vnot     v1,  v0         ; overlapping = NOT separated
    // `vcmpgefp` followed by `vnot` IS the SDK's CompLessThan (vendor vector4_operation.h:136,
    // bodied as the literal `!(a >= b)`), so that is the call used rather than a bare `<`.
    // ==============================================================================================
    rw::math::vpu::MaskScalar BoxOverlappingTest(const Box& lrBox1, const Box& lrBox2, Vector3 lAxis)
    {
        // `lvx128 v3, r3+0x30 ; vmsum3fp128 v3, v3, v1` -- note the POSITION is the first Dot
        // operand here, where GetBoxExtentsAlongAxis puts the axis first. Kept as the asm has it.
        const f32 lfBox1Pos = vpu::Dot(lrBox1.GetTransform().Pos(), lAxis);   // DWARF lBox1Pos :167
        const f32 lfBox2Pos = vpu::Dot(lrBox2.GetTransform().Pos(), lAxis);   // DWARF lBox2Pos :168

        const VecFloat lBox1Extents = GetBoxExtentsAlongAxis(lrBox1, lAxis);  // DWARF :169
        const VecFloat lBox2Extents = GetBoxExtentsAlongAxis(lrBox2, lAxis);  // DWARF :170

        // `vsubfp` then `vandc` (Abs) at 0x825BF070/0x825BF078.
        f32 lfDistance = std::fabs(lfBox1Pos - lfBox2Pos);                    // DWARF lDistance :172
        lfDistance -= (lBox1Extents.x + lBox2Extents.x);                      // DWARF operator-=

        // DWARF lOverlapping :174.
        const rw::math::vpu::MaskScalar lOverlapping =
            vpu::CompLessThan(VecFloat{ lfDistance, lfDistance, lfDistance, lfDistance },
                              VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f });
        return lOverlapping;
    }

    // ==============================================================================================
    // CgsGeometric::BoxOverlappingTest(const Box&, const Box&) @0x825BF090  (50 insns)
    //
    // ⭐ THE NAME. The export calls this `sub_825BF090` and progress/identity.json has no row for
    // it at all, so it is named from the DWARF, which declares exactly one two-argument overload:
    // `MaskScalar BoxOverlappingTest(const Box &, const Box &)` at CgsBox.h:190, with local
    // `MaskScalar lOverlapping` at :192 (dwarfdump _compile/BrnPhysicsUnity2.cpp:34873). Three
    // independent facts nail the match: the body's ONLY callee is the three-argument overload
    // (`bl CgsGeometric__BoxOverlappingTest` x6), it takes exactly the two box pointers
    // (r3 -> r31, r4 -> r30) and no VMX argument, and its ONLY REAL CALLER in the whole image is
    // BrnPhysics::Vehicle::VehicleManager::EndVehicleContactGeneration @0x8261AC38 -- whose DWARF
    // (BrnVehicleManagerContactGeneration.cpp:356) names `CgsGeometric::BoxOverlappingTest`.
    // ⚠️ "only REAL caller", stated exactly as measured: the dump lists TWO xrefs to 0x825BF090 --
    // 0x8261AFF4, inside EndVehicleContactGeneration, and 0x821CE540, which is a LINKER BRANCH
    // ISLAND, not a caller. Every function in that same dump carries one companion xref in the
    // 0x821B..0x821D range (Box::Set -> 0x821CEBA0, EndVehicleContactGeneration -> 0x821CF2D8,
    // sub_821F0FC0 -> 0x821BB070) and none of those addresses has a per-address IDA export of its
    // own. The identification does not rest on the caller count in any case: the DWARF declaration,
    // the six-`bl` callee set and the r3/r4-no-VMX register shape carry it on their own.
    //
    // SIX FACE AXES, NO EDGE-CROSS AXES. The console tests box 1's three basis rows and then box 2's
    // three, in that order, and ANDs all six results:
    //     0x825BF0AC lvx128 v1, r31+0x00   -> BoxOverlappingTest(b1, b2, b1.Right())  -> v127
    //     0x825BF0C4 lvx128 v0, r31+0x10   ->                        b1.Up()          -> v126
    //     0x825BF0E0 lvx128 v0, r31+0x20   ->                        b1.At()          -> v125
    //     0x825BF0EC lvx128 v0, r30+0x00   ->                        b2.Right()       -> v124
    //     0x825BF104 lvx128 v0, r30+0x10   ->                        b2.Up()          -> v123
    //     0x825BF11C lvx128 v0, r30+0x20   ->                        b2.At()          -> v1
    //     0x825BF134..0x825BF144  vand128 x5 + vand, left-associated in that same order.
    // The +0x30 (centre) rows are never used as axes, and the nine edge-cross axes a real 3D SAT
    // would add are simply not tested -- another place this is deliberately cheaper than the
    // textbook algorithm. All six calls are made unconditionally: there is no early-out, so the
    // named locals below are computed before the fold rather than short-circuited inside it.
    // ==============================================================================================
    rw::math::vpu::MaskScalar BoxOverlappingTest(const Box& lrBox1, const Box& lrBox2)
    {
        const Matrix44Affine lBox1Transform = lrBox1.GetTransform();
        const Matrix44Affine lBox2Transform = lrBox2.GetTransform();

        const rw::math::vpu::MaskScalar lBox1RightTest =
            BoxOverlappingTest(lrBox1, lrBox2, lBox1Transform.Right());
        const rw::math::vpu::MaskScalar lBox1UpTest =
            BoxOverlappingTest(lrBox1, lrBox2, lBox1Transform.Up());
        const rw::math::vpu::MaskScalar lBox1AtTest =
            BoxOverlappingTest(lrBox1, lrBox2, lBox1Transform.At());
        const rw::math::vpu::MaskScalar lBox2RightTest =
            BoxOverlappingTest(lrBox1, lrBox2, lBox2Transform.Right());
        const rw::math::vpu::MaskScalar lBox2UpTest =
            BoxOverlappingTest(lrBox1, lrBox2, lBox2Transform.Up());
        const rw::math::vpu::MaskScalar lBox2AtTest =
            BoxOverlappingTest(lrBox1, lrBox2, lBox2Transform.At());

        // DWARF lOverlapping :192 -- the five vand128 plus the closing vand, same association.
        const rw::math::vpu::MaskScalar lOverlapping =
            vpu::And(vpu::And(vpu::And(vpu::And(vpu::And(lBox1RightTest, lBox1UpTest),
                                                lBox1AtTest),
                                       lBox2RightTest),
                              lBox2UpTest),
                     lBox2AtTest);
        return lOverlapping;
    }
}
