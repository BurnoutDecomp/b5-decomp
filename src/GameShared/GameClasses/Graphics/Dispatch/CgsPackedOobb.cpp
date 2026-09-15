#include "GameShared/GameClasses/Graphics/Dispatch/CgsPackedOobb.h"

#include <cmath>
#include <cstring>   // memcpy (the bit-pattern reads below)

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
// UN-FLAGGED 2026-09-15 (b5-decomp issue #26). The banner here used to say the
// bit-field placement was "recovered from the vperm/vcfsx/vcfux shapes, not from
// valued .rdata permute constants ... inferred and marked for strong review", and
// the body it justified read mauLane[0..3] for the quaternion, mauLane[0..2] for
// the scale AND mauLane[0..2] for the position -- THE SAME BYTES THREE TIMES. Every
// mesh therefore decoded to the same ~1-unit box beside the model origin whatever
// the model was, and DrawRenderable::Interpret culled meshes against it.
//
// The five permute vectors ARE recoverable: they are dyn-init .bss (PackedOobb's
// five `static` members), so x360rd reads them as zero BY DEFINITION and only the
// CRT thunk has the values. findinit.py names the writers and ppcdis disassembles
// them at 0x82C6C598..0x82C6C6E4 (+ the vand w-mask at 0x82C740B0):
//   stru_83011130 = 0C0C0C0C 0D0D0D0D 0E0E0E0E 0F0F0F0F   quaternion
//   stru_83011370 = 11090909 110A0A0A 110B0B0B 11111111   scale mantissa
//   stru_83011220 = 08111111 x4                           scale exponent
//   stru_830113A0 = 02030203 04050405 06070607 11111111   position
//   unk_830113C0  = 00011111 x4                           position scale
//   unk_8327F130  = FFFFFFFF FFFFFFFF FFFFFFFF 00000000   the axis-row w mask
// which resolve mPackedBB to (console byte order):
//   [0..1] position scale (top 16 bits of a float)   [2..7] s16 position x/y/z
//   [8] shared scale exponent (2^(b-127))            [9..11] u8 scale mantissa x/y/z
//   [12..15] u8 quaternion x/y/z/w
// The quaternion->matrix algebra below was already correct and is unchanged; only
// the three unpacks are. LESSON: "the export has no data section" is a statement
// about the EXPORT -- the same one CgsFrustum.cpp records for SetFromRwFrustum.
// ===========================================================================

namespace CgsGraphics
{

namespace
{
    // One byte of the packed register, in the CONSOLE's byte order.
    //
    // mPackedBB is ONE 16-byte big-endian VMX value on the X360 and every vperm mask
    // below indexes it BYTE BY BYTE (0..15). The world porter transcodes it as four u32
    // LANES -- tools/assets/bundles/renderable_transcode.py reads `>4I` (:229) and writes
    // `<4I` (:386) -- so each lane's NUMERIC value is the console's word, and console
    // byte k is the (k & 3)-th byte of lane (k >> 2) counted from the TOP.
    inline u32 PackedByte(const u32* lpauLane, u32 luIndex)
    {
        return (lpauLane[luIndex >> 2] >> (8u * (3u - (luIndex & 3u)))) & 0xFFu;
    }

    // Reinterpret a word as a float (the console simply keeps it in a VMX lane and
    // multiplies; the bit pattern IS the float).
    inline f32 BitsToFloat(u32 luBits)
    {
        f32 lfValue;
        std::memcpy(&lfValue, &luBits, sizeof(lfValue));
        return lfValue;
    }

    // `vcfsx vD, vS, 0x1F` -- signed int -> float, scaled by 2^-31.
    inline f32 SignedFixed31(u32 luWord)
    {
        return static_cast<f32>(static_cast<s32>(luWord)) * (1.0f / 2147483648.0f);
    }

    // `vcfux vD, vS, 0x18` -- unsigned int -> float, scaled by 2^-24.
    inline f32 UnsignedFixed24(u32 luWord)
    {
        return static_cast<f32>(luWord) * (1.0f / 16777216.0f);
    }
}

void PackedOobb::ToMatrix(rw::math::vpu::Matrix44& roMatrix) const
{
    const u32* const lpauLane = mPackedBB.mauLane;

    // --- unpack the quaternion: vperm stru_83011130 -> vcfsx 0x1F ---
    // stru_83011130 = { 0C0C0C0C, 0D0D0D0D, 0E0E0E0E, 0F0F0F0F } -- byte 12/13/14/15
    // broadcast across its own word, so each component is ONE byte read as a signed
    // fraction. (The absolute scale is irrelevant: the quaternion is normalised below.)
    f32 lfQx = SignedFixed31(PackedByte(lpauLane, 12) * 0x01010101u);
    f32 lfQy = SignedFixed31(PackedByte(lpauLane, 13) * 0x01010101u);
    f32 lfQz = SignedFixed31(PackedByte(lpauLane, 14) * 0x01010101u);
    f32 lfQw = SignedFixed31(PackedByte(lpauLane, 15) * 0x01010101u);

    // normalise: lenSq = dot4(q,q); inv = 1/sqrt(lenSq) (vrsqrtefp + 2 NR).
    // The asm is straight-line VMX with no compare/branch guarding lenSq == 0
    // (vrsqrtefp is evaluated unconditionally), so no zero-guard belongs here.
    const f32 lfLenSq = lfQx * lfQx + lfQy * lfQy + lfQz * lfQz + lfQw * lfQw;
    const f32 lfInv = 1.0f / std::sqrt(lfLenSq);
    lfQx *= lfInv;
    lfQy *= lfInv;
    lfQz *= lfInv;
    lfQw *= lfInv;

    // --- unpack the per-axis scale: mantissa vperm stru_83011370 -> vcfux 0x18,
    //     times the shared exponent vperm stru_83011220 -> vsrw 1 ---
    // stru_83011370 = { 11090909, 110A0A0A, 110B0B0B, 11111111 } -- byte 9/10/11 into the
    // low THREE bytes of its word (mask index 0x11 >= 16 selects the vperm's ZERO source),
    // i.e. a u8 mantissa that vcfux 0x18 turns into [0, 1).
    // stru_83011220 = { 08111111 x4 } -- byte 8 into the TOP byte of every word; `vsrw v0,
    // v7, 1` then lands it at bit 23, which IS the IEEE single exponent field, so the three
    // axes share one exponent and the scale is mantissa * 2^(byte8 - 127).
    const f32 lfScaleExponent = BitsToFloat(PackedByte(lpauLane, 8) << 23);
    const f32 lfScaleX = UnsignedFixed24(PackedByte(lpauLane,  9) * 0x00010101u) * lfScaleExponent;
    const f32 lfScaleY = UnsignedFixed24(PackedByte(lpauLane, 10) * 0x00010101u) * lfScaleExponent;
    const f32 lfScaleZ = UnsignedFixed24(PackedByte(lpauLane, 11) * 0x00010101u) * lfScaleExponent;

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

    // --- unpack the position into the translation row (this+0x30, stored first in the
    //     asm): vperm stru_830113A0 -> vcfsx 0x1F, times vperm unk_830113C0.
    //     w lane forced to 1.0 (vrlimi128 v13, v11, 1, 0). ---
    // stru_830113A0 = { 02030203, 04050405, 06070607, 11111111 } -- a 16-BIT signed fixed
    // component per axis, from byte pairs (2,3) / (4,5) / (6,7), the halfword duplicated
    // into both halves of the word (which is what makes vcfsx 0x1F read it as ~h/2^15).
    // unk_830113C0 = { 00011111 x4 } -- bytes 0 and 1 become the TOP 16 bits of a float
    // (the rest zero), i.e. the position's own scale, and `vmulfp128 v13, v3, v13` applies
    // it. So the box centre is NOT a unit-range number: it is a real model-space position.
    const f32 lfPositionScale = BitsToFloat(  ( PackedByte(lpauLane, 0) << 24 )
                                            | ( PackedByte(lpauLane, 1) << 16 ) );
    const u32 luPosX = ( PackedByte(lpauLane, 2) << 24 ) | ( PackedByte(lpauLane, 3) << 16 )
                     | ( PackedByte(lpauLane, 2) <<  8 ) |   PackedByte(lpauLane, 3);
    const u32 luPosY = ( PackedByte(lpauLane, 4) << 24 ) | ( PackedByte(lpauLane, 5) << 16 )
                     | ( PackedByte(lpauLane, 4) <<  8 ) |   PackedByte(lpauLane, 5);
    const u32 luPosZ = ( PackedByte(lpauLane, 6) << 24 ) | ( PackedByte(lpauLane, 7) << 16 )
                     | ( PackedByte(lpauLane, 6) <<  8 ) |   PackedByte(lpauLane, 7);

    roMatrix.wAxis.x = SignedFixed31(luPosX) * lfPositionScale;
    roMatrix.wAxis.y = SignedFixed31(luPosY) * lfPositionScale;
    roMatrix.wAxis.z = SignedFixed31(luPosZ) * lfPositionScale;
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
