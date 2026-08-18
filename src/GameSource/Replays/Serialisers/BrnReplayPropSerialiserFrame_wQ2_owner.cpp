// GameSource/Replays/Serialisers/BrnReplayPropSerialiserFrame_wQ2_owner.cpp
//
// Wave Q round 2 -- the six PropSerialiserFrame record/playback accessors that the frame
// interior decode unblocked. Partfile of the BrnReplayPropSerialiserFrame TU (the sibling
// BrnReplayPropSerialiserFrame.cpp owns the loaded-zone half).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (per-address JSON `assembly`, read instruction
// by instruction -- never the pseudocode, which renders every VMX op as inline __asm):
//   BrnReplays::PropSerialiserFrame::WriteProp          @ 0x822BB528  (124 insns)
//   BrnReplays::PropSerialiserFrame::WritePart          @ 0x822BB718  (129 insns)
//   BrnReplays::PropSerialiserFrame::GetPropTransform   @ 0x822BB920  ( 62 insns)
//   BrnReplays::PropSerialiserFrame::GetPartTransform   @ 0x822BBA18  ( 62 insns)
//   BrnReplays::PropSerialiserFrame::IsCellActive       @ 0x822BBDA0  ( 26 insns)
//   BrnReplays::PropSerialiserFrame::IsPropAddedToScene @ 0x822CD818  ( 66 insns)
// (Instruction counts MEASURED: IDA function extent / 4 for each of the six.)
//
// NO CONSOLE OFFSET IS TRANSCRIBED BELOW. Every frame array is reached by member name; the
// header's _AssertLayout pins the nineteen offsets the console's absolute forms use.
//
// =================================================================================================
// THE FOUR TRANSFORM BODIES ARE ONE ALGORITHM AND ITS INVERSE
// =================================================================================================
// The frame stores a transform SPLIT: the position row verbatim in one array, and the rotation as
// a unit quaternion in another. So Write{Prop,Part} runs matrix->quaternion and Get{Prop,Part}
// Transform runs quaternion->matrix. The console inlines each direction identically into its two
// callers.
//
// BOTH DIRECTIONS ALREADY HAVE A REAL HOME IN THE TREE. [2026-08-18 round-3 correction: an
// earlier revision of this banner claimed "no symbol names either helper at these two sites, so
// they stay TU-local rather than inventing an rwmath entry point". That was FALSE in both
// directions and it is the reason the sign/lane table below was presented as derived when a
// third, decisive pin -- the original rwmath source -- was sitting in the tree. It is corrected
// here, and the matrix->quaternion re-implementation it justified has been deleted.]
//   * matrix -> quaternion  ==  rw::math::vpu::QuaternionFromMatrix33
//     (b5-decomp/src/vendor/renderware/physics/JointFrames.hpp:64, a header inline). That body is
//     recovered from the Feb-2007 rwmath 1.02.00 ORIGINAL SOURCE
//     (rw/math/vpu/detail/quaternion_operation_inline.h:282-382 for the VMX form,
//     fpu/quaternion_operation.h:802-853 for the scalar branch twin) and its own banner already
//     names the very constants this site uses -- the three gQuatFromMat_{x,y,z}Signs vxor masks
//     unk_8327F120/F100/F0F0 and the unk_82CDA360..430 lane controls. THAT SOURCE IS THE
//     STRONGEST PIN for the sign/lane table, stronger than either of the two derivations the old
//     banner offered. It is CALLED here (see the adapter below); nothing is re-derived.
//     The console's compare operand is lane 0 of unk_8201444C, dumped == 0x00000000 == 0.0f,
//     which is exactly QuaternionFromMatrix33's DEFAULTED epsilon -- so it is passed explicitly
//     as 0.0f rather than relying on the default. Existing users of the same entry point:
//     rw::physics::RigidBody::SetTransform (RigidBody.cpp:259) and the three JointFrames setters.
//   * quaternion -> matrix  ==  rw::physics::Quaternion::UnitQuaternionToMatrix
//     (b5-decomp/vendor/renderware/include/rw/physics/quaternion.h:36). ⚠️ NOT CALLED HERE, and
//     the reason is behavioural, not "no symbol exists": that entry point takes
//     `Quaternion*` (non-const) and NORMALISES IT IN PLACE, writing all four components back
//     through the caller's storage. Get{Prop,Part}Transform has no vrsqrtefp at all and never
//     writes back into the frame (see the next block), so calling it would invent a store into
//     the replay record. The nine terms are still taken verbatim from that committed body; the
//     file-local ExpandUnitQuaternionRows below is the same expansion MINUS the normalise.
//
// ---- quaternion -> rotation rows (Get{Prop,Part}Transform @0x822BB944..0x822BB9F8) --------------
// MEASURED, and it is the SAME CODE as the committed vendor body
// b5-decomp/vendor/renderware/src/rw/physics/Quaternion.cpp (UnitQuaternionToMatrix). I compared
// the two listings register by register against DynamicUpdate's inlined copy at
// 0x82BC2C94..0x82BC2D14:
//     * same scale constant  `lvx128 <c>, rw::math::vpu::detail::gSqrt2s` then `vmulfp128 s, c, q`
//     * same diagonal term   `vnmsubfp d, s, half, s`  ==  d = 0.5 - s*s
//       (IDA prints the A-form operands in RAW FIELD ORDER vD,vA,vB,vC and the semantic is
//        vD = -(vA*vC - vB); pinned independently by the two-step Newton-Raphson in WriteProp,
//        which only reduces to the standard refinement under that reading. Do NOT "fix" it to
//        the ISA's vD,vA,vC,vB assembly spelling -- that would corrupt correct math.)
//     * same off-diagonal pair  `vpermwi128 .yzxx (0x60)` / `.zxyx (0x84)` + `vspltw s.w`
//     * same three permute constants unk_82CDA3D0 / unk_82CDA450 / unk_82CDA410 and the same
//       `vrlimi128 mask=2` rotates 0 / 3 / 2 that route them into the three rows.
// The ONE difference: DynamicUpdate normalises first (vmsum4fp128 + vrsqrtefp + 2 NR + a
// write-back of the normalised quaternion); Get{Prop,Part}Transform has NO vrsqrtefp at all, so
// it assumes the stored quaternion is unit and never writes back into the frame. Reproduced
// exactly -- adding a normalise here would be inventing a store into the replay record.
// The nine terms are therefore taken verbatim from the committed vendor body.
//
// ---- rotation rows -> quaternion (Write{Prop,Part} @0x822BB54C..0x822BB6F8) ---------------------
// THIS SITE NO LONGER CARRIES AN IMPLEMENTATION. It calls rw::math::vpu::QuaternionFromMatrix33
// (JointFrames.hpp:64), whose body is the ORIGINAL rwmath source for exactly this routine -- see
// the block above. What is recorded here is only the IDENTIFICATION: why that committed helper is
// the function the console inlined at 0x822BB54C, read out of the asm instruction by instruction.
//   * the console computes ALL FOUR cases of the classic largest-component extraction in parallel
//     lanes and picks with `vsel`; the committed helper is the SDK's own scalar branch twin of
//     that network (its banner cites fpu/quaternion_operation.h:802-853), so the de-optimisation
//     is the SDK's, not a project invention.
//   * the four trace variants ride the four lanes of one vector: `vxor` against three sign-mask
//     constants (unk_8327F120 on m00, unk_8327F100 on m11, unk_8327F0F0 on m22) then
//     `vaddfp ... + 1.0`. Those three masks live in a RUNTIME-INITIALISED data segment, so the
//     image bytes read all-zero and they cannot be dumped -- they are the rwmath source's
//     gQuatFromMat_{x,y,z}Signs, named as such by JointFrames.hpp's banner. THE SOURCE IS THE PIN.
//   * `vrsqrtefp` + two Newton-Raphson steps, then `vmulfp128 t*rsqrt` = sqrt(t) and
//     `vmulfp128 0.5*rsqrt` = 0.5/sqrt(t), per lane -- the helper's `s` / `s2`, de-optimised to
//     std::sqrt there per the standing precedent.
//   * the two operand vectors, decoded from the permute constants dumped out of the image
//     (unk_82CDA3D0 / 3F0 / 370 / 3A0 plus the `vsldoi ...,8` pair):
//         v9  = ( m12, m20, m01, 0.5 )      v10 = ( m21, m02, m10, 0.0 )
//         diff = v9 - v10 = ( m12-m21, m20-m02, m01-m10, 0.5 )
//         sum  = v9 + v10 = ( m12+m21, m20+m02, m01+m10, 0.5 )
//     which is term-for-term the helper's four branches (Q0 uses diff0/1/2 with w = 0.5s;
//     Q1 uses 0.5s / sum2 / sum1 / diff0; Q2 sum2 / 0.5s / sum0 / diff1; Q3 sum1 / sum0 / 0.5s /
//     diff2). The lane->component routing came from the four remaining permute constants
//     (unk_82CDA390 / 3E0 / 3B0 / 380 selecting from diff|sum, and unk_82CDA360 / 420 / 430 / 390
//     selecting sqrt(t)|0.5/sqrt(t)), and lane 0/1/2/3 is the trace/m00/m11/m22 branch.
//   * the selection cascade and its priority, from the `vcmpgtfp`/`vand`/`vsel` chain:
//         vsel(m22case, m11case, m11 > m22) -> vsel(that, m00case, m00 > m11 && m00 > m22)
//                                           -> vsel(that, tracecase, trace > 0)
//     so trace wins, then m00, then m11, else m22 -- exactly the helper's if / if / if / else.
//     `vcmpgtfp` is a strict, unordered-false ">" so the helper's C++ `>` is exact (gotcha 4).
//     The trace test is against 0.0f: it splats lane 0 of (+-m00 +- m11 + m22), and the compare
//     operand is lane 0 of unk_8201444C, dumped == 0x00000000 == 0.0f == the epsilon passed below.
// The console evaluates sqrt() on all four lanes including the negative ones (NaN, discarded by
// the vsel); the helper's branch form simply does not compute them. Same selected value.
// =================================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Replays/Serialisers/BrnReplayPropSerialiserFrame.h"
#include "vendor/renderware/physics/JointFrames.hpp"   // rw::math::vpu::QuaternionFromMatrix33

namespace
{
    typedef rw::math::vpu::Quaternion Quaternion;

    // Expand a UNIT quaternion into the three rotation rows of lrTransform. Terms taken verbatim
    // from rw::physics::Quaternion::UnitQuaternionToMatrix, whose instruction sequence this block
    // reproduces exactly minus the normalise -- see the banner (which also says why that
    // committed entry point is NOT called here: it normalises its argument in place).
    // Writes only .x/.y/.z of the three basis rows; the Pos row and the three basis w lanes are
    // left to the caller, which SetZero()s the whole matrix first -- see the ROW-W NOTE below.
    void ExpandUnitQuaternionRows(const Quaternion& lrQuat, Matrix44Affine& lrTransform)
    {
        const f32 lfX = lrQuat.x, lfY = lrQuat.y, lfZ = lrQuat.z, lfW = lrQuat.w;

        const f32 lfXY = 2.0f * lfX * lfY, lfYZ = 2.0f * lfY * lfZ, lfZX = 2.0f * lfZ * lfX;
        const f32 lfWX = 2.0f * lfW * lfX, lfWY = 2.0f * lfW * lfY, lfWZ = 2.0f * lfW * lfZ;
        const f32 lfXX = 2.0f * lfX * lfX, lfYY = 2.0f * lfY * lfY, lfZZ = 2.0f * lfZ * lfZ;

        lrTransform.xAxis.x = 1.0f - lfYY - lfZZ;
        lrTransform.xAxis.y = lfXY + lfWZ;
        lrTransform.xAxis.z = lfZX - lfWY;

        lrTransform.yAxis.x = lfXY - lfWZ;
        lrTransform.yAxis.y = 1.0f - lfXX - lfZZ;
        lrTransform.yAxis.z = lfYZ + lfWX;

        lrTransform.zAxis.x = lfZX + lfWY;
        lrTransform.zAxis.y = lfYZ - lfWX;
        lrTransform.zAxis.z = 1.0f - lfXX - lfYY;
    }

    // [2026-08-18 round 3] The matrix->quaternion helper that used to live here has been DELETED.
    // It was a second implementation of rw::math::vpu::QuaternionFromMatrix33 (JointFrames.hpp:64),
    // which is recovered from the original rwmath source; Write{Prop,Part} now call it directly,
    // building the rw::math::vpu::Matrix33 it takes from the transform's three basis rows exactly
    // as the committed rw::physics::RigidBody::SetTransform does (RigidBody.cpp:255-260).
}

namespace BrnReplays
{
    // ---- WriteProp @0x822BB528 -----------------------------------------------------------------
    // asm, register-exact: r3 = this (r30), r4 = the transform (r31), r5 = the type id (r29).
    //     addi r3, r30, 0x610 ; addi r4, r31, 0x30 ; bl <PushBack>   ; maPropPositions += Pos()
    //     <the 41-instruction matrix->quaternion block, result staged in var_30>
    //     addi r3, r30, 0x1600 ; addi r4, sp+var_30 ; bl <PushBack>  ; maPropOrientations += quat
    //     sth r29, var_40 ; addi r3, r30, 0x25F0 ; addi r4, sp+var_40 ; bl <PushBack> ; maTypes
    // The three PushBacks carry the console's own capacity tripwire ("muLength < MaxLength",
    // BrnReplayArray.h:98) -- non-gating there and non-gating here.
    void PropSerialiserFrame::WriteProp(const Matrix44Affine& lrTransform, u16 lu16TypeId)
    {
        maPropPositions.PushBack(lrTransform.Pos());

        // The matrix->quaternion block the console inlined at 0x822BB54C IS
        // rw::math::vpu::QuaternionFromMatrix33; epsilon 0.0f == lane 0 of unk_8201444C.
        rw::math::vpu::Matrix33 lBasis;
        lBasis.xAxis = lrTransform.xAxis;
        lBasis.yAxis = lrTransform.yAxis;
        lBasis.zAxis = lrTransform.zAxis;
        maPropOrientations.PushBack(rw::math::vpu::QuaternionFromMatrix33(lBasis, 0.0f));

        maTypes.PushBack(lu16TypeId);
    }

    // ---- WritePart @0x822BB718 -----------------------------------------------------------------
    // Same shape with the part arrays and one extra id: r3 this (r31), r4 transform (r30),
    // r5 type id (r29), r6 part id (r28). Bases 0x27F0 / 0x3000 / 0x3810 / 0x3912, and the last
    // two PushBacks are literally the same instantiation (sub_822AAB98), which is how the two
    // u16 part arrays are known to share T and N.
    void PropSerialiserFrame::WritePart(const Matrix44Affine& lrTransform,
                                        u16                   lu16TypeId,
                                        u16                   lu16PartId)
    {
        maPartPositions.PushBack(lrTransform.Pos());

        // Same helper, same epsilon -- WritePart inlines the identical block WriteProp does.
        rw::math::vpu::Matrix33 lBasis;
        lBasis.xAxis = lrTransform.xAxis;
        lBasis.yAxis = lrTransform.yAxis;
        lBasis.zAxis = lrTransform.zAxis;
        maPartOrientations.PushBack(rw::math::vpu::QuaternionFromMatrix33(lBasis, 0.0f));

        maPartTypes.PushBack(lu16TypeId);
        maPartIds.PushBack(lu16PartId);
    }

    // ---- GetPropTransform @0x822BB920 ----------------------------------------------------------
    // asm: r3 = the hidden return-buffer pointer (r31), r4 = this (r30), r5 = the index
    // (`clrlwi r29, r5, 24` -> u8); the epilogue returns that buffer (`mr r3, r31`). So the
    // console really does return the Matrix44Affine by value.
    //     addi r3, r30, 0x1600 ; bl <operator[]>   ; maPropOrientations[i]   (bounds-asserted)
    //     stvx128 <zero>, r31+0x30                 ; the Pos row is cleared first...
    //     <quaternion -> three rows, stored to r31+0/0x10/0x20>
    //     addi r3, r30, 0x610 ; bl <operator[]>    ; maPropPositions[i]      (bounds-asserted)
    //     stvx128 <element>, r31+0x30              ; ...and then overwritten in full
    // The clear is kept because the console performs it; it is dead only because the position
    // store that follows covers all 16 bytes.
    //
    // ROW-W NOTE (round-3, a real console-vs-host divergence, stated rather than hidden). The
    // console's three basis stores are FULL 16-byte `stvx128` to r31+0/+0x10/+0x20, so each row's
    // w lane receives a permute LEFTOVER rather than a computed value (the round-2 verifier
    // decoded the vperm+vrlimi chain and read xAxis.w as a stale copy of the 1-2x^2-2y^2 term;
    // that decode is not re-derived here because nothing depends on the value).
    // ExpandUnitQuaternionRows writes only .x/.y/.z of each row, which would leave those three
    // lanes INDETERMINATE and then copy them out through the by-value return. Reproducing the
    // console's junk has no consumer (no Matrix44Affine user reads a basis row's w lane), and
    // reading indeterminate storage is a genuine defect under /O2 -- so the whole matrix is
    // SetZero()'d first. That subsumes the console's Pos-row clear above and makes the three
    // basis w lanes a determinate 0. NOT byte-identical to the console in those three lanes,
    // deliberately; every lane any consumer reads is identical.
    Matrix44Affine PropSerialiserFrame::GetPropTransform(u8 lu8Index) const
    {
        const Quaternion& lrOrientation = maPropOrientations[lu8Index];

        Matrix44Affine lTransform;
        lTransform.SetZero();   // subsumes the console's Pos-row clear -- see the ROW-W note above
        ExpandUnitQuaternionRows(lrOrientation, lTransform);
        lTransform.Pos() = maPropPositions[lu8Index];

        return lTransform;
    }

    // ---- GetPartTransform @0x822BBA18 ----------------------------------------------------------
    // Instruction-for-instruction GetPropTransform with the part arrays (0x3000 then 0x27F0).
    Matrix44Affine PropSerialiserFrame::GetPartTransform(u8 lu8Index) const
    {
        const Quaternion& lrOrientation = maPartOrientations[lu8Index];

        Matrix44Affine lTransform;
        lTransform.SetZero();   // subsumes the console's Pos-row clear -- see the ROW-W note above
        ExpandUnitQuaternionRows(lrOrientation, lTransform);
        lTransform.Pos() = maPartPositions[lu8Index];

        return lTransform;
    }

    // ---- IsCellActive @0x822BBDA0 --------------------------------------------------------------
    // asm: r3 = this (r28), r4 = the packed cell word (r29).
    //     lbz r11, 0x600(r28) ; beq -> return 0        ; empty array
    //   loop:
    //     clrlwi r4, index, 24 ; bl <operator[]>       ; maRecordedCells[index]
    //     lwz r11, 0(r3) ; cmplw r11, r29 ; beq -> return 1
    //     lbz r11, 0x600(r28) ; addi index,1 ; cmpw ; blt loop
    //     -> return 0
    // The count is re-read every iteration on the console; it cannot change inside the loop, so
    // it is hoisted into the loop condition here (same treatment as the sibling GetZone).
    bool PropSerialiserFrame::IsCellActive(u32 luPackedCellId) const
    {
        for (u8 lu8Index = 0; lu8Index < maRecordedCells.muLength; ++lu8Index)
        {
            if (maRecordedCells[lu8Index] == luPackedCellId)
            {
                return true;
            }
        }

        return false;
    }

    // ---- IsPropAddedToScene @0x822CD818 --------------------------------------------------------
    // The sibling of WasPropPreviouslyHit @0x822CD920 (see BrnReplayPropSerialiserFrame.cpp),
    // identical instruction for instruction except the bit-run offset:
    //     bl GetZone ; cmplwi r27, 0 ; beq -> return 0
    //     cmplwi r28, 0x258 ; blt ok                   ; assert index < 600, streamed as
    //                                                  ;   "invalid index : " << i << " < " << 600
    //                                                  ;   (CgsBitArray.h:203, the inlined
    //                                                  ;    BitArray::IsBitSet guard)
    //   ok:
    //     srwi r11, r28, 6 ; addi r11, r11, 1 ; slwi r11, r11, 3 ; ldx ; sld ; and
    // `addi r11, r11, 1` == +8 bytes == maPropsAddedToScene, where WasPropPreviouslyHit has
    // `addi r11, r11, 0xB` == +0x58 == maPropsPreviouslyHit. Nothing else differs.
    // No console offset is transcribed: the field/bit split lives inside BitArray<600>::IsBitSet
    // and the record is indexed by member name.
    bool PropSerialiserFrame::IsPropAddedToScene(s32 liZoneId, u32 luPropIndex) const
    {
        const PropLoadedZoneRecord* lpZone = GetZone(liZoneId);
        if (lpZone == 0)
        {
            return false;
        }

        CGS_ASSERT(luPropIndex < KU_PROPS_PER_ZONE, "invalid index : ");

        return lpZone->maPropsAddedToScene.IsBitSet(luPropIndex);
    }
}
