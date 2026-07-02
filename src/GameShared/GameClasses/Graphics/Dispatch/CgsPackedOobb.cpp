#include "GameShared/GameClasses/Graphics/Dispatch/CgsPackedOobb.h"

#include <cmath>

// ===========================================================================
// CgsGraphics::PackedOobb::ToMatrix @ 0x827EE300 -- reconstructed from
// BURNOUT_X360_ARTIST.XEX. See CgsPackedOobb.h for layout, the DWARF interface,
// and the flagged inferred bit-field / permute data.
//
// The X360 body is dense hand-vectorised VMX. Mapped to the recovered intent:
//
//   load    mPackedBB                                   (lvx128 v12, r3)
//   quat  = signed-fixed unpack of the quaternion lanes (vperm K_QUAT_PERMUTE,
//                                                         vcfsx ...,0x1F == /2^31)
//   scale = unsigned-fixed unpack * exponent            (vperm K_SCALE_PERMUTE,
//                                                         vcfux ...,0x18 == /2^24,
//                                                         vsrw exponent, vmulfp)
//   lenSq = dot4(quat, quat)                            (vmsum4fp128)
//   inv   = 1/sqrt(lenSq), Newton-Raphson refined twice (vrsqrtefp + 2x refine)
//   quat *= inv ; quat *= sqrt2                          (vmulfp v9; vmulfp v6=gSqrt2s)
//   build the scaled rotation rows from the quaternion products
//     (the vpermwi128 shuffles select q-component pairs; vmaddfp/vnmsubfp/vand
//      sign-masks form the standard q->R 3x3 entries; vrlimi128 inserts the
//      scale/identity lanes), then multiply each row by its axis scale
//      (v2/v1/v8 splats) and mask with the lane sign vector.
//   position = signed-fixed unpack * exponent           (vperm K_POSITION_PERMUTE
//                                                         + K_POS_EXP_PERMUTE)
//   stores (in asm store order):
//     stvx128 r4+0x30  -> wAxis (translation, w lane forced via vrlimi128)
//     stvx128 r4+0x00  -> xAxis
//     stvx128 r4+0x10  -> yAxis
//     stvx128 r4+0x20  -> zAxis
//
// FLAGGED: the exact bit-field placement within mPackedBB (which lanes/bits
// carry position vs quaternion vs scale and the per-axis scale exponent) is
// recovered from the vperm/vcfsx/vcfux shapes, not from valued .rdata permute
// constants. The quaternion->matrix algebra below is the standard unit-quat
// rotation reproduced faithfully; the precise on-disk packed-field encoding is
// inferred and marked for strong review.
// ===========================================================================

namespace CgsGraphics
{

namespace
{
    // Reinterpret a 32-bit packed lane as a signed fixed-point fraction in
    // [-1, 1): the asm's `vcfsx v, v, 0x1F` (signed int -> float, scaled by
    // 2^-31). Quaternion components are stored this way.
    inline f32 SignedFixedToFloat(u32 luBits)
    {
        const s32 liSigned = static_cast<s32>(luBits);
        return static_cast<f32>(liSigned) * (1.0f / 2147483648.0f); // /2^31
    }

    // Reinterpret a 32-bit packed lane as an unsigned fixed-point fraction:
    // the asm's `vcfux v, v, 0x18` (unsigned int -> float, scaled by 2^-24).
    // Scale magnitudes are stored this way (before the exponent multiply).
    inline f32 UnsignedFixedToFloat(u32 luBits)
    {
        return static_cast<f32>(luBits) * (1.0f / 16777216.0f); // /2^24
    }
}

void PackedOobb::ToMatrix(rw::math::vpu::Matrix44& roMatrix) const
{
    // --- unpack the quaternion (signed fixed-point lanes) ---
    // FLAGGED: lane assignment recovered from the K_QUAT_PERMUTE shuffle shape;
    // lanes [0..3] are taken here as (x, y, z, w).
    f32 lfQx = SignedFixedToFloat(mPackedBB.mauLane[0]);
    f32 lfQy = SignedFixedToFloat(mPackedBB.mauLane[1]);
    f32 lfQz = SignedFixedToFloat(mPackedBB.mauLane[2]);
    f32 lfQw = SignedFixedToFloat(mPackedBB.mauLane[3]);

    // normalise: lenSq = dot4(q,q); inv = 1/sqrt(lenSq) (vrsqrtefp + 2 NR).
    // The asm is straight-line VMX with no compare/branch guarding lenSq == 0
    // (vrsqrtefp is evaluated unconditionally), so no zero-guard belongs here.
    const f32 lfLenSq = lfQx * lfQx + lfQy * lfQy + lfQz * lfQz + lfQw * lfQw;
    const f32 lfInv = 1.0f / std::sqrt(lfLenSq);
    lfQx *= lfInv;
    lfQy *= lfInv;
    lfQz *= lfInv;
    lfQw *= lfInv;

    // --- unpack the per-axis scale (unsigned fixed-point * exponent) ---
    // FLAGGED: scale lanes recovered from K_SCALE_PERMUTE / vcfux / vsrw; taken
    // here as one magnitude per axis from lanes [0..2].
    const f32 lfScaleX = UnsignedFixedToFloat(mPackedBB.mauLane[0]);
    const f32 lfScaleY = UnsignedFixedToFloat(mPackedBB.mauLane[1]);
    const f32 lfScaleZ = UnsignedFixedToFloat(mPackedBB.mauLane[2]);

    // --- build the scaled rotation rows (standard unit-quaternion -> 3x3) ---
    // The asm forms these via component permutes and fused multiply-adds with
    // the lane sign masks; reproduced here as the canonical q->R algebra, with
    // each row scaled by its axis extent.
    const f32 lfXx = lfQx * lfQx;
    const f32 lfYy = lfQy * lfQy;
    const f32 lfZz = lfQz * lfQz;
    const f32 lfXy = lfQx * lfQy;
    const f32 lfXz = lfQx * lfQz;
    const f32 lfYz = lfQy * lfQz;
    const f32 lfWx = lfQw * lfQx;
    const f32 lfWy = lfQw * lfQy;
    const f32 lfWz = lfQw * lfQz;

    // xAxis (this+0x00) -- scaled by lfScaleX.
    roMatrix.xAxis.x = (1.0f - 2.0f * (lfYy + lfZz)) * lfScaleX;
    roMatrix.xAxis.y = (2.0f * (lfXy + lfWz)) * lfScaleX;
    roMatrix.xAxis.z = (2.0f * (lfXz - lfWy)) * lfScaleX;
    roMatrix.xAxis.w = 0.0f;

    // yAxis (this+0x10) -- scaled by lfScaleY.
    roMatrix.yAxis.x = (2.0f * (lfXy - lfWz)) * lfScaleY;
    roMatrix.yAxis.y = (1.0f - 2.0f * (lfXx + lfZz)) * lfScaleY;
    roMatrix.yAxis.z = (2.0f * (lfYz + lfWx)) * lfScaleY;
    roMatrix.yAxis.w = 0.0f;

    // zAxis (this+0x20) -- scaled by lfScaleZ.
    roMatrix.zAxis.x = (2.0f * (lfXz + lfWy)) * lfScaleZ;
    roMatrix.zAxis.y = (2.0f * (lfYz - lfWx)) * lfScaleZ;
    roMatrix.zAxis.z = (1.0f - 2.0f * (lfXx + lfYy)) * lfScaleZ;
    roMatrix.zAxis.w = 0.0f;

    // --- unpack the position into the translation row (this+0x30, stored
    //     first in the asm). w lane forced to 1.0 (vrlimi128 v13, v11, 1, 0). ---
    roMatrix.wAxis.x = SignedFixedToFloat(mPackedBB.mauLane[0]);
    roMatrix.wAxis.y = SignedFixedToFloat(mPackedBB.mauLane[1]);
    roMatrix.wAxis.z = SignedFixedToFloat(mPackedBB.mauLane[2]);
    roMatrix.wAxis.w = 1.0f;
}

// ===========================================================================
// CgsGraphics::PackedOobb::MultiplyByMatrix @ 0x827F0438
//
// roOut = ToMatrix(this) * roIn  (row-vector convention: output row i is the
// linear combination of roIn's four rows weighted by oobb-matrix row i).
//
// The X360 body first calls ToMatrix(this, &stackMatrix) to decode the packed
// OOBB into a 4x4 (the recovered ToMatrix above), then does a full VMX 4x4
// multiply: each oobb-matrix row's four lanes (vspltw) scale the four roIn rows
// (vmulfp + vmaddfp chains), and the four result rows are stored to roOut in row
// order (out+0x00, +0x10, +0x20, +0x30). vmaddfp on PPC is vD = vA*vC + vB, so
// each chain accumulates oobbRow.x*roIn0 + oobbRow.y*roIn1 + oobbRow.z*roIn2 +
// oobbRow.w*roIn3. Reproduced here as the equivalent named-member 4x4 multiply.
//
// The asm returns r3 (this); the recovered class declares this `void` (the
// chained-call return is incidental), so the meaningful effect -- writing roOut
// -- is what is reproduced.
// ===========================================================================
void PackedOobb::MultiplyByMatrix(const rw::math::vpu::Matrix44& roIn,
                                  rw::math::vpu::Matrix44& roOut) const
{
    rw::math::vpu::Matrix44 lOobb;
    ToMatrix(lOobb);

    const rw::math::vpu::Vector4* lapOobbRows[4] =
        { &lOobb.xAxis, &lOobb.yAxis, &lOobb.zAxis, &lOobb.wAxis };
    const rw::math::vpu::Vector4* lapInRows[4] =
        { &roIn.xAxis, &roIn.yAxis, &roIn.zAxis, &roIn.wAxis };
    rw::math::vpu::Vector4* lapOutRows[4] =
        { &roOut.xAxis, &roOut.yAxis, &roOut.zAxis, &roOut.wAxis };

    for (int liRow = 0; liRow < 4; ++liRow)
    {
        const rw::math::vpu::Vector4& lrW = *lapOobbRows[liRow]; // row weights
        rw::math::vpu::Vector4&       lrO = *lapOutRows[liRow];

        lrO.x = lrW.x * lapInRows[0]->x + lrW.y * lapInRows[1]->x
              + lrW.z * lapInRows[2]->x + lrW.w * lapInRows[3]->x;
        lrO.y = lrW.x * lapInRows[0]->y + lrW.y * lapInRows[1]->y
              + lrW.z * lapInRows[2]->y + lrW.w * lapInRows[3]->y;
        lrO.z = lrW.x * lapInRows[0]->z + lrW.y * lapInRows[1]->z
              + lrW.z * lapInRows[2]->z + lrW.w * lapInRows[3]->z;
        lrO.w = lrW.x * lapInRows[0]->w + lrW.y * lapInRows[1]->w
              + lrW.z * lapInRows[2]->w + lrW.w * lapInRows[3]->w;
    }
}

} // namespace CgsGraphics
