#include "GameShared/GameClasses/Graphics/Dispatch/CgsPackedOobb.h"

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
// The same recovered quaternion-to-matrix formula is now expressed through native
// four-lane operations. LESSON: "the export has no data section" is a statement
// about the EXPORT -- the same one CgsFrustum.cpp records for SetFromRwFrustum.
// ===========================================================================

namespace CgsGraphics
{

namespace
{
    inline rw::math::vpu::VectorIntrinsic VectorFromWords(u32 lu0, u32 lu1, u32 lu2, u32 lu3)
    {
        rw::math::vpu::detail::VectorIntrinsicUnion lValue;
        lValue.scalarUInt[0] = lu0;
        lValue.scalarUInt[1] = lu1;
        lValue.scalarUInt[2] = lu2;
        lValue.scalarUInt[3] = lu3;
        return lValue.vector;
    }

    inline rw::math::vpu::VectorIntrinsic SplatFloat(f32 lfValue)
    {
        return __vspltw(__lvlx(&lfValue, 0), 0);
    }
}

void PackedOobb::ToMatrix(rw::math::vpu::Matrix44& roMatrix) const
{
    using namespace rw::math::vpu;

    const VectorIntrinsic lvPacked = __loadunalignedvector(&mPackedBB);
    const VectorIntrinsic lvZero = __vspltisw(0);

    // The CRT vperm controls are materialised from their recovered words. Indices 0x10+
    // select the zero source; this preserves the exact byte and zero-fill behavior.
    const VectorIntrinsic lvQuaternionPermute = VectorFromWords(
        0x0C0C0C0Cu, 0x0D0D0D0Du, 0x0E0E0E0Eu, 0x0F0F0F0Fu);
    const VectorIntrinsic lvScalePermute = VectorFromWords(
        0x11090909u, 0x110A0A0Au, 0x110B0B0Bu, 0x11111111u);
    const VectorIntrinsic lvExponentPermute = VectorFromWords(
        0x08111111u, 0x08111111u, 0x08111111u, 0x08111111u);
    const VectorIntrinsic lvPositionPermute = VectorFromWords(
        0x02030203u, 0x04050405u, 0x06070607u, 0x11111111u);
    const VectorIntrinsic lvPositionScalePermute = VectorFromWords(
        0x00011111u, 0x00011111u, 0x00011111u, 0x00011111u);

    // Signed fixed quaternion bytes are gathered and converted in one four-lane VMX op.
    const VectorIntrinsic lvQuaternionFixed = __vcfsx(
        __vperm(lvPacked, lvZero, lvQuaternionPermute), 0x1F);
    const Vector4 lvQuaternionUnit = Normalize(Vector4(lvQuaternionFixed));
    const VectorIntrinsic lvQuaternion = __vmulfp(
        lvQuaternionUnit.GetVector(), SplatFloat(1.4142135623730951f));
    const VectorIntrinsic lvQx = __vspltw(lvQuaternion, 0);
    const VectorIntrinsic lvQy = __vspltw(lvQuaternion, 1);
    const VectorIntrinsic lvQz = __vspltw(lvQuaternion, 2);
    const VectorIntrinsic lvQw = __vspltw(lvQuaternion, 3);

    // Mantissas are unsigned fixed 0.24; exponent byte 8 is shifted into the IEEE exponent
    // position before the native float multiply. The three resulting lanes are axis scales.
    const VectorIntrinsic lvScaleMantissa = __vcuxwfp(
        __vperm(lvPacked, lvZero, lvScalePermute), 0x18);
    const VectorIntrinsic lvScaleExponent = __vsrw(
        __vperm(lvPacked, lvZero, lvExponentPermute), __vspltisw(1));
    const VectorIntrinsic lvScale = __vmulfp(lvScaleMantissa, lvScaleExponent);
    const VectorIntrinsic lvScaleX = __vspltw(lvScale, 0);
    const VectorIntrinsic lvScaleY = __vspltw(lvScale, 1);
    const VectorIntrinsic lvScaleZ = __vspltw(lvScale, 2);

    const VectorIntrinsic lvOne = __vcfsx(__vspltisw(1), 0);
    const VectorIntrinsic lvXX = __vmulfp(lvQx, lvQx);
    const VectorIntrinsic lvYY = __vmulfp(lvQy, lvQy);
    const VectorIntrinsic lvZZ = __vmulfp(lvQz, lvQz);
    const VectorIntrinsic lvXY = __vmulfp(lvQx, lvQy);
    const VectorIntrinsic lvXZ = __vmulfp(lvQx, lvQz);
    const VectorIntrinsic lvYZ = __vmulfp(lvQy, lvQz);
    const VectorIntrinsic lvWX = __vmulfp(lvQw, lvQx);
    const VectorIntrinsic lvWY = __vmulfp(lvQw, lvQy);
    const VectorIntrinsic lvWZ = __vmulfp(lvQw, lvQz);

    const VectorIntrinsic lvR00 = __vsubfp(lvOne, __vaddfp(lvYY, lvZZ));
    const VectorIntrinsic lvR01 = __vaddfp(lvXY, lvWZ);
    const VectorIntrinsic lvR02 = __vsubfp(lvXZ, lvWY);
    const VectorIntrinsic lvR10 = __vsubfp(lvXY, lvWZ);
    const VectorIntrinsic lvR11 = __vsubfp(lvOne, __vaddfp(lvXX, lvZZ));
    const VectorIntrinsic lvR12 = __vaddfp(lvYZ, lvWX);
    const VectorIntrinsic lvR20 = __vaddfp(lvXZ, lvWY);
    const VectorIntrinsic lvR21 = __vsubfp(lvYZ, lvWX);
    const VectorIntrinsic lvR22 = __vsubfp(lvOne, __vaddfp(lvXX, lvYY));

    const VectorIntrinsic lvX00 = __vmulfp(lvR00, lvScaleX);
    const VectorIntrinsic lvX01 = __vmulfp(lvR01, lvScaleX);
    const VectorIntrinsic lvX02 = __vmulfp(lvR02, lvScaleX);
    const VectorIntrinsic lvY10 = __vmulfp(lvR10, lvScaleY);
    const VectorIntrinsic lvY11 = __vmulfp(lvR11, lvScaleY);
    const VectorIntrinsic lvY12 = __vmulfp(lvR12, lvScaleY);
    const VectorIntrinsic lvZ20 = __vmulfp(lvR20, lvScaleZ);
    const VectorIntrinsic lvZ21 = __vmulfp(lvR21, lvScaleZ);
    const VectorIntrinsic lvZ22 = __vmulfp(lvR22, lvScaleZ);

    // Position words are signed fixed 16-bit values. The scale comes from the packed top
    // halfword as an IEEE float; the translation w lane is explicitly one.
    const VectorIntrinsic lvPositionFixed = __vcfsx(
        __vperm(lvPacked, lvZero, lvPositionPermute), 0x1F);
    const VectorIntrinsic lvPositionScale = __vperm(lvPacked, lvZero, lvPositionScalePermute);
    const VectorIntrinsic lvPosition = __vmulfp(lvPositionFixed, lvPositionScale);
    roMatrix.wAxis = Vector4(lvPosition);
    roMatrix.wAxis.SetW(1.0f);

    roMatrix.xAxis.SetZero();
    roMatrix.xAxis.SetX(VecFloat(lvX00, 0));
    roMatrix.xAxis.SetY(VecFloat(lvX01, 0));
    roMatrix.xAxis.SetZ(VecFloat(lvX02, 0));
    roMatrix.yAxis.SetZero();
    roMatrix.yAxis.SetX(VecFloat(lvY10, 0));
    roMatrix.yAxis.SetY(VecFloat(lvY11, 0));
    roMatrix.yAxis.SetZ(VecFloat(lvY12, 0));
    roMatrix.zAxis.SetZero();
    roMatrix.zAxis.SetX(VecFloat(lvZ20, 0));
    roMatrix.zAxis.SetY(VecFloat(lvZ21, 0));
    roMatrix.zAxis.SetZ(VecFloat(lvZ22, 0));
}

// ===========================================================================
// CgsGraphics::PackedOobb::MultiplyByMatrix @ 0x827F0438
//
// roOut = ToMatrix(this) * roIn  (row-vector convention: output row i is the
// linear combination of roIn's four rows weighted by oobb-matrix row i).
//
    // The X360 body first calls ToMatrix(this, &stackMatrix), then multiplies each
    // OOBB row against all four input rows with native VMX multiply/add chains.
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

    // ARTIST loads all four input rows before its first output store. Preserve
    // that snapshot on the native path too: roIn and roOut may be the same matrix.
    const rw::math::vpu::Matrix44 lInput = roIn;

    const rw::math::vpu::Vector4* lapOobbRows[4] =
        { &lOobb.xAxis, &lOobb.yAxis, &lOobb.zAxis, &lOobb.wAxis };
    const rw::math::vpu::Vector4* lapInRows[4] =
        { &lInput.xAxis, &lInput.yAxis, &lInput.zAxis, &lInput.wAxis };
    rw::math::vpu::Vector4* lapOutRows[4] =
        { &roOut.xAxis, &roOut.yAxis, &roOut.zAxis, &roOut.wAxis };

    for (int liRow = 0; liRow < 4; ++liRow)
    {
        const rw::math::vpu::VectorIntrinsic& lrWeights = lapOobbRows[liRow]->GetVector();
        rw::math::vpu::VectorIntrinsic lvResult = __vmulfp(
            __vspltw(lrWeights, 0), lapInRows[0]->GetVector());
        lvResult = __vmaddfp(__vspltw(lrWeights, 1), lapInRows[1]->GetVector(), lvResult);
        lvResult = __vmaddfp(__vspltw(lrWeights, 2), lapInRows[2]->GetVector(), lvResult);
        lvResult = __vmaddfp(__vspltw(lrWeights, 3), lapInRows[3]->GetVector(), lvResult);
        lapOutRows[liRow]->SetVector(lvResult);
    }
}

} // namespace CgsGraphics
