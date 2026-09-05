// =============================================================================
// BrnEffectsUtils.cpp  (definitions for BrnEffects::Utils::Vector3Randomiser
//                       and BrnEffects::Utils::Vector4Randomiser)
//
// Out-of-line home for the two asm-attested per-call vector randomisers:
//   Vector3Randomiser::RandomiseXYZ  @ 0x82277EC8
//   Vector4Randomiser::RandomiseXYZW @ 0x82277FB8
// Reconstructed store-for-store. The Prepare / RandomInterpolate surface is owned
// by other TUs and is declared-only in the header.
//
// THE DRAW (both): advance the Random's 64-bit LCG (seed = seed * MULTIPLIER + 1)
// once per output component, pack the high 32 bits of each step into the mantissa
// of an IEEE-754 float in [1, 2) and write those float-bits into the Random's ring
// at a Vector-slot chosen by ((index + 3) & 4). It RETURNS the slot primed on the
// PREVIOUS call (a 1-deep pipeline): load the slot BEFORE writing new bits,
// subtract 1.0 (components -> [0, 1)), result = mVecA * mVecB + (prev - 1.0).
// =============================================================================

#include "GameSource/Effects/BrnEffectsUtils.h"

#include <cmath>   // floorf -- the vrfim in the sin/cos fold

namespace BrnEffects
{
namespace Utils
{

// @ 0x82277EC8
Vector3 Vector3Randomiser::RandomiseXYZ(CgsNumeric::Random &lrRandom)
{
    // -- advance the LCG twice; capture the high words of each state used --
    const u64 luSeed0 = lrRandom.muSeed;            // pre-draw state
    const u64 luSeed1 = luSeed0 * CgsNumeric::KU_RANDOM_MULTIPLIER + 1;
    const u64 luSeed2 = luSeed1 * CgsNumeric::KU_RANDOM_MULTIPLIER + 1;

    const u32 luS0Hi  = static_cast<u32>(luSeed0 >> 32);
    const u32 luS1Hi  = static_cast<u32>(luSeed1 >> 32);

    // Vector-slot for this draw: ((index + 3) & 4) selects ring half {0..3} or {4..7}.
    const u32 luSlot  = (lrRandom.muOldestBufferIndex + 3) & 4;
    lrRandom.muOldestBufferIndex = luSlot;

    // Load the slot's PREVIOUS contents (the draw primed last call) before we
    // overwrite it. Subtract 1.0 to map each [1, 2) component into [0, 1).
    Vector3 lvPrev;
    lvPrev.x = lrRandom.mafFloatBuffer[luSlot + 0] - 1.0f;
    lvPrev.y = lrRandom.mafFloatBuffer[luSlot + 1] - 1.0f;
    lvPrev.z = lrRandom.mafFloatBuffer[luSlot + 2] - 1.0f;
    lvPrev.w = lrRandom.mafFloatBuffer[luSlot + 3] - 1.0f;

    // Commit the advanced seed.
    lrRandom.muSeed = luSeed2;

    // Pack the new draw's float-bits into the ring (consumed next call).
    const u32 luOne = CgsNumeric::KU_IEEE_754_REPRESENTATION_FLOAT_ONE;
    lrRandom.mauIntegerBuffer[luSlot + 0] = luOne | ((luS1Hi << 2) & 0x7FFFFC);
    lrRandom.mauIntegerBuffer[luSlot + 1] = luOne | ((luS0Hi << 13) & 0x7FE000) | (luS1Hi >> 19);
    lrRandom.mauIntegerBuffer[luSlot + 2] = luOne | (luS0Hi >> 9);
    lrRandom.muOldestBufferIndex = luSlot + 3;

    // result = mVecA * mVecB + (previousDraw - 1.0), per lane.
    Vector3 lvResult;
    lvResult.x = mVecA.x * mVecB.x + lvPrev.x;
    lvResult.y = mVecA.y * mVecB.y + lvPrev.y;
    lvResult.z = mVecA.z * mVecB.z + lvPrev.z;
    lvResult.w = mVecA.w * mVecB.w + lvPrev.w;
    return lvResult;
}

// @ 0x82277FB8
Vector4 Vector4Randomiser::RandomiseXYZW(CgsNumeric::Random &lrRandom)
{
    // -- advance the LCG three times (four components packed from three steps) --
    const u64 luSeed0 = lrRandom.muSeed;
    const u64 luSeed1 = luSeed0 * CgsNumeric::KU_RANDOM_MULTIPLIER + 1;
    const u64 luSeed2 = luSeed1 * CgsNumeric::KU_RANDOM_MULTIPLIER + 1;
    const u64 luSeed3 = luSeed2 * CgsNumeric::KU_RANDOM_MULTIPLIER + 1;

    const u32 luS0Hi  = static_cast<u32>(luSeed0 >> 32);
    const u32 luS1Hi  = static_cast<u32>(luSeed1 >> 32);
    const u32 luS2Hi  = static_cast<u32>(luSeed2 >> 32);

    const u32 luSlot  = (lrRandom.muOldestBufferIndex + 3) & 4;
    lrRandom.muOldestBufferIndex = luSlot;

    Vector4 lvPrev;
    lvPrev.x = lrRandom.mafFloatBuffer[luSlot + 0] - 1.0f;
    lvPrev.y = lrRandom.mafFloatBuffer[luSlot + 1] - 1.0f;
    lvPrev.z = lrRandom.mafFloatBuffer[luSlot + 2] - 1.0f;
    lvPrev.w = lrRandom.mafFloatBuffer[luSlot + 3] - 1.0f;

    lrRandom.muSeed = luSeed3;

    const u32 luOne = CgsNumeric::KU_IEEE_754_REPRESENTATION_FLOAT_ONE;
    lrRandom.mauIntegerBuffer[luSlot + 0] = luOne | (luS0Hi >> 9);
    lrRandom.mauIntegerBuffer[luSlot + 1] = luOne | ((luS0Hi << 14) & 0x7FC000) | (luS1Hi >> 18);
    lrRandom.mauIntegerBuffer[luSlot + 2] = luOne | ((luS1Hi << 5) & 0x7FFFE0) | (luS2Hi >> 27);
    lrRandom.mauIntegerBuffer[luSlot + 3] = luOne | ((luS2Hi >> 4) & 0x7FFFFF);

    // Next call swaps to the other ring half.
    lrRandom.muOldestBufferIndex = luSlot ^ 4u;

    Vector4 lvResult;
    lvResult.x = mVecA.x * mVecB.x + lvPrev.x;
    lvResult.y = mVecA.y * mVecB.y + lvPrev.y;
    lvResult.z = mVecA.z * mVecB.z + lvPrev.z;
    lvResult.w = mVecA.w * mVecB.w + lvPrev.w;
    return lvResult;
}

// =============================================================================
// The three named colour constants (DWARF BrnEffectsUtils.h:43/:44/:45). Each is a
// dynamically-initialised .bss splat on the console, recovered through its CRT init thunk --
// see the header for the address chain. Written as the image's exact floats.
// =============================================================================
const VecFloat K_VECFLOAT_255            = { 255.0f, 255.0f, 255.0f, 255.0f };
const Vector4  K_VECTOR4_511_511_511_255 = { 511.0f, 511.0f, 511.0f, 255.0f };
const VecFloat K_VECFLOAT_ONEOVER255     = { 0.003921568859368563f, 0.003921568859368563f,
                                             0.003921568859368563f, 0.003921568859368563f };

// =================================================================================================
// BrnEffects::Utils::BuildUVs  @ 0x822781E0    (DWARF BrnEffectsUtils.h:453)
//
// Pick the frame-atlas cell for one particle and hand the caller its four corner UVs -- each
// carrying BOTH the current frame's pair and the next frame's, so the shader can cross-fade.
//
// ⭐ THIS WAS THE LAST QUIET STUB ON THE LION DRAW PATH. It wrote four zero UV corners, which
// maps every particle to texel (0,0) and reads on screen as a shader bug rather than as missing
// code. Everything it needed was readable:
//
//   unk_82CDA3C0 (.rdata) = { 00010203, 00010203, 00010203, 14151617 } => vperm(A,B) = (A.x, ., ., B.y)
//   unk_82CDA400 (.rdata) = { 08090A0B, 1C1D1E1F, 00010203, 00010203 } => vperm(A,B) = (A.z, B.w, ., .)
//   unk_82FABA60 <- CRT thunk 0x82C4A188 : (flt_82001CC0, flt_82001C98) x2 == (0, 1, 0, 1)
//   unk_82FAB880 <- CRT thunk 0x82C4A1C0 : (flt_82001C98, flt_82001CC0) x2 == (1, 0, 1, 0)
//
// and the `vsldoi ..., 8` that follows each vperm pair is a real immediate, not a register --
// the raw word at 0x82278288 is 0x10C5322C, whose SH field (bits 22-25) is 8, so it takes
// bytes 8..23 of (vA || vB) == (vA.z, vA.w, vB.x, vB.y). IDA renders that immediate as "v8",
// which is what made the tail look like an undecodable lane weave.
//
// ⭐⭐ THE TWO PATHS CONFIRM EACH OTHER, which is why the corner order below is safe. Work the
// vperm/vsldoi weave through and the four outputs come out as
//   [0] (uLeft, vBottom)  [1] (uLeft, vTop)  [2] (uRight, vBottom)  [3] (uRight, vTop)
// -- and the non-atlas path, decoded completely independently from two CRT thunks and two
// splats, writes exactly (0,1) / (0,0) / (1,1) / (1,0) into those same four slots. Two
// derivations, one corner order. It also matches the corner order the three draw halves build
// their geometry in ([0] = the -pivot corner in both axes).
//
// ⚠ THE FRAME WRAP IS A SNAP, NOT A MODULO: `vcmpgefp` + `vsel` (0x822781FC/0x82278234)
// replaces the whole floored frame index with 0 as soon as it reaches mvfMaterialFrameCount.
//
// The lane arithmetic is done on scalars here because every input is a broadcast: the two
// frame arguments arrive splatted (QuadDraw's `vspltw v1, ..., 3`), and every BuildUVData
// member is splatted by SetupFromMaterial, so all four lanes of every intermediate hold the
// same value.
// =================================================================================================

void BuildUVs(const BuildUVData& arUVData, VecFloat lvfFrame, VecFloat lvfNextFrame,
              Vector4* lpaUVsOut)
{
    // asm 0x822781E0..0x822781F0 -- FLAG_MULTIFRAME off means the quad takes the whole texture.
    if ((arUVData.muMaterialFlags & BuildUVData::KU_MATERIAL_FLAG_MULTIFRAME) == 0u)
    {
        // asm 0x822782DC..0x82278318. Both frame slots hold the same pair, so the shader's
        // cross-fade is a no-op whatever weight it is given.
        lpaUVsOut[0] = Vector4{ 0.0f, 1.0f, 0.0f, 1.0f };   // unk_82FABA60
        lpaUVsOut[1] = Vector4{ 0.0f, 0.0f, 0.0f, 0.0f };   // vspltisw v12, 0
        lpaUVsOut[2] = Vector4{ 1.0f, 1.0f, 1.0f, 1.0f };   // vcfsx(vspltisw 1, 0)
        lpaUVsOut[3] = Vector4{ 1.0f, 0.0f, 1.0f, 0.0f };   // unk_82FAB880
        return;
    }

    // asm 0x822781F4..0x8227822C -- the atlas description.
    const f32 lfFrameCount = arUVData.mvfMaterialFrameCount.x;
    const f32 lfNumXFrames = arUVData.mvfMaterialNumXFrames.x;
    const f32 lfInvNumX    = arUVData.mvfMaterialOneOverNumXFrames.x;
    const f32 lfInvNumY    = arUVData.mvfMaterialOneOverNumYFrames.x;

    // asm 0x822781F8..0x8227823C -- floor, then snap a past-the-end index back to frame 0.
    f32 lfFrame0 = floorf(lvfFrame.x);
    if (lvfFrame.x >= lfFrameCount)
    {
        lfFrame0 = 0.0f;
    }
    f32 lfFrame1 = floorf(lvfNextFrame.x);
    if (lvfNextFrame.x >= lfFrameCount)
    {
        lfFrame1 = 0.0f;
    }

    // asm 0x82278240..0x8227827C -- index -> (column, row) -> the cell's four edges.
    const f32 lfRow0 = floorf(lfFrame0 * lfInvNumX);
    const f32 lfRow1 = floorf(lfFrame1 * lfInvNumX);

    const f32 lfV0Top = lfRow0 * lfInvNumY;
    const f32 lfV1Top = lfRow1 * lfInvNumY;

    const f32 lfColumn0 = lfFrame0 - lfRow0 * lfNumXFrames;
    const f32 lfColumn1 = lfFrame1 - lfRow1 * lfNumXFrames;

    const f32 lfU0Left = lfColumn0 * lfInvNumX;
    const f32 lfU1Left = lfColumn1 * lfInvNumX;

    const f32 lfV0Bottom = lfV0Top + lfInvNumY;
    const f32 lfV1Bottom = lfV1Top + lfInvNumY;
    const f32 lfU0Right  = lfU0Left + lfInvNumX;
    const f32 lfU1Right  = lfU1Left + lfInvNumX;

    // asm 0x82278280..0x822782D4 -- the four (u, v, u', v') corners.
    lpaUVsOut[0] = Vector4{ lfU0Left,  lfV0Bottom, lfU1Left,  lfV1Bottom };
    lpaUVsOut[1] = Vector4{ lfU0Left,  lfV0Top,    lfU1Left,  lfV1Top    };
    lpaUVsOut[2] = Vector4{ lfU0Right, lfV0Bottom, lfU1Right, lfV1Bottom };
    lpaUVsOut[3] = Vector4{ lfU0Right, lfV0Top,    lfU1Right, lfV1Top    };
}

// =================================================================================================
// BrnEffects::Utils::FastMatrix33FromEulerXYZ  @ 0x8227E7A8   (DWARF BrnEffectsUtils.h:259)
//
// Build the 3x3 rotation Rx * Ry * Rz (row-major) from packed Euler XYZ angles in radians,
// using a minimax odd-quintic sin/cos over a folded phase. Called by RenderSprites @0x82282608
// and RenderQuads @0x82282B28 for every particle that carries an x or y rotation.
//
// ⭐⭐ THE SEVEN "UNDECODED RODATA CONSTANTS" ARE NOW READ, NOT ARGUED. They are dynamically-
// initialised .bss splats, so a literal read of unk_8307A5F0 & co returns 0x00000000 BY
// DEFINITION and every literal scan finds only readers. tools/re/findinit.py locates each CRT
// thunk in the 0x82C6Exxx/0x82C6Fxxx init bank, tools/re/ppcdis.py shows the `lfs <rodata> ;
// stfs ; lvlx ; vspltw ; stvx128` copy, and tools/re/x360rd.py reads the rodata float:
//
//   unk_8307A680 <- 0x82C6EB18 : splat4(flt_82001C90) == 0x3E22F983 ==   0.15915494  (1/2pi)
//   unk_8307A590 <- 0x82C6EBC8 : (flt_8200D56C, flt_82001CC0) x2       == (-0.25, 0, -0.25, 0)
//   unk_8307A3C0 <- 0x82C6EB90 : splat4(flt_82001C98) == 0x3F800000 ==   1.0
//   unk_8307A560 <- 0x82C6EBB8 : splat4(flt_8200D56C) == 0xBE800000 ==  -0.25
//   unk_8307A5F0 <- 0x82C6F118 : splat4(flt_820F1014) == 0xC0C8E5BA ==  -6.2780428
//   unk_8307A3B0 <- 0x82C6F140 : splat4(flt_820F1018) == 0x422396E8 ==  40.897369
//   unk_8307A670 <- 0x82C6F168 : splat4(flt_820F101C) == 0xC28E5BA2 == -71.178970
//
// ⭐ AND THE MEANING FALLS OUT OF THEM EXACTLY -- no coefficient is fitted or guessed:
//   phase   p = angle * (1/2pi) + offset, offset = -0.25 on lanes 0/2 and 0 on lanes 1/3
//   fold    u = p - floor(p) in [0,1);   m = min(u, 1 - u);   t = m - 0.25 in [-0.25, 0.25]
//   value   f(t) = t * (-6.2780428 + 40.897369*t^2 - 71.178970*t^4)
// f(-0.25) evaluates to EXACTLY -0.25 * -4.0 == 1.0 and f(0) == 0, so the lanes carrying the
// -0.25 phase offset are sin and the lanes carrying 0 are cos. Spot-checked at 0, pi/2, pi and
// 3pi/2 (all four exact) and at pi/4 (0.70705 vs 0.70711 -- a normal quintic minimax error).
// The DWARF names the shared helper this came from: CgsNumeric::TrigBaseFunctions5 ::
// Cos4_UnitCycles, driven by TrigFunctions<>::SinCos. That class has NO row in the X360 ledger
// (it is folded into every caller), so it is reproduced here as a file-local helper rather than
// minted as a class the target build does not contain.
//
// ⭐ THE ROW ORDER IS ALSO MEASURED. The nine products are gathered lane by lane through the
// stack red zone (0x8227E8F0..0x8227E98C) into three blocks that are stvx128'd to r3+0x00,
// r3+0x10 and r3+0x20; following each `lfs`/`stfs` pair gives
//   row0 = ( cy*cz,             cy*sz,             -sy,    0 )
//   row1 = ( sx*sy*cz - cx*sz,  cx*cz + sx*sy*sz,  sx*cy,  0 )
//   row2 = ( sx*sz + cx*sy*cz,  cx*sy*sz - sx*cz,  cx*cy,  0 )
// which is Rx*Ry*Rz for row vectors, term for term. ⭐ INDEPENDENT CONFIRMATION: setting
// sx = sy = 0, cx = cy = 1 in those rows gives (cz, sz, 0) / (-sz, cz, 0) / (0, 0, 1), i.e.
// exactly the hand-written 2D rotation the two draw halves use on their z-only fast path
// (RenderSprites 0x82282888..0x822828C4). Two independently decoded blocks of VMX agree, so
// neither the sin/cos lane assignment nor the row order rests on a single reading.
//
// The w lane of every row is written as a hard 0 (`stw r11, 0(r9)` / `stw r11, 0(r10)` with
// r11 == 0 at 0x8227E934 and 0x8227E954, plus the third at 0x8227E988).
// =================================================================================================

namespace
{
    // The sin/cos evaluation above, per lane. `lfPhase` is the angle already scaled into turns
    // and phase-shifted; the caller decides which lanes are sin and which are cos purely by
    // that shift. Reproduces vrfim / vsubfp / vsubfp / vminfp / vaddfp / two vmaddfp / vmulfp128
    // step for step.
    const f32 KF_ONE_OVER_TWO_PI = 0.15915493667125702f;   // flt_82001C90
    const f32 KF_SIN_PHASE       = -0.25f;                 // flt_8200D56C (unk_8307A590 lanes 0/2)
    const f32 KF_COS_PHASE       = 0.0f;                   // flt_82001CC0 (unk_8307A590 lanes 1/3)
    const f32 KF_FOLD_PERIOD     = 1.0f;                   // unk_8307A3C0
    const f32 KF_FOLD_BIAS       = -0.25f;                 // unk_8307A560
    const f32 KF_POLY_C1         = -6.278042793273926f;    // unk_8307A5F0
    const f32 KF_POLY_C3         = 40.897369384765625f;    // unk_8307A3B0
    const f32 KF_POLY_C5         = -71.17897033691406f;    // unk_8307A670

    // CgsNumeric::TrigBaseFunctions5::Cos4_UnitCycles, one lane. lfCycles is in TURNS.
    inline f32 Cos4_UnitCycles(f32 lfCycles)
    {
        const f32 lfFraction = lfCycles - floorf(lfCycles);                       // vrfim/vsubfp
        f32 lfFolded = lfFraction;                                                // vsubfp/vminfp
        const f32 lfComplement = KF_FOLD_PERIOD - lfFraction;
        if (lfComplement < lfFolded)
        {
            lfFolded = lfComplement;
        }
        const f32 lfT  = lfFolded + KF_FOLD_BIAS;                                 // vaddfp
        const f32 lfT2 = lfT * lfT;                                               // vmulfp128
        return lfT * (lfT2 * (lfT2 * KF_POLY_C5 + KF_POLY_C3) + KF_POLY_C1);      // 2x vmaddfp
    }

}

// CgsNumeric::TrigFunctions<CgsNumeric::TrigBaseFunctions5>::SinCos for one angle: the two lanes
// of the same polynomial, a quarter turn apart. Exposed (rather than kept file-local) because the
// two Lion draw halves inline the identical polynomial for their z-only rotation path and the
// recovered coefficients must have exactly one home.
void SinCosCycles(f32 lfRadians, f32& arSin, f32& arCos)
{
    const f32 lfCycles = lfRadians * KF_ONE_OVER_TWO_PI;                      // vmaddfp
    arSin = Cos4_UnitCycles(lfCycles + KF_SIN_PHASE);
    arCos = Cos4_UnitCycles(lfCycles + KF_COS_PHASE);
}

Matrix33 FastMatrix33FromEulerXYZ(Vector3 lv3EulerAngles)
{
    // vmrghw(v1,v1) puts (x, x, y, y) in one register and vspltw(v1,2) puts (z, z, z, z) in
    // another, so the console evaluates all six sines and cosines in two polynomial passes.
    f32 lfSinX, lfCosX, lfSinY, lfCosY, lfSinZ, lfCosZ;
    SinCosCycles(lv3EulerAngles.x, lfSinX, lfCosX);
    SinCosCycles(lv3EulerAngles.y, lfSinY, lfCosY);
    SinCosCycles(lv3EulerAngles.z, lfSinZ, lfCosZ);

    Matrix33 lResult;

    // row 0 -- 0x8227E8F0..0x8227E8FC gather (cy*cz, cy*sz) and 0x8227E944 the negated sy.
    lResult.xAxis.x = lfCosY * lfCosZ;
    lResult.xAxis.y = lfCosY * lfSinZ;
    lResult.xAxis.z = -lfSinY;
    lResult.xAxis.w = 0.0f;

    // row 1 -- the three vsubfp/vaddfp at 0x8227E8DC..0x8227E8E4 plus sx*cy from 0x8227E8B0.
    lResult.yAxis.x = lfSinY * lfSinX * lfCosZ - lfCosX * lfSinZ;
    lResult.yAxis.y = lfSinY * lfSinX * lfSinZ + lfCosX * lfCosZ;
    lResult.yAxis.z = lfSinX * lfCosY;
    lResult.yAxis.w = 0.0f;

    // row 2 -- the remaining pair at 0x8227E8E0/0x8227E8E8 plus cx*cy from 0x8227E8B4.
    lResult.zAxis.x = lfSinY * lfCosX * lfCosZ + lfSinX * lfSinZ;
    lResult.zAxis.y = lfSinY * lfCosX * lfSinZ - lfSinX * lfCosZ;
    lResult.zAxis.z = lfCosX * lfCosY;
    lResult.zAxis.w = 0.0f;

    return lResult;
}

} // namespace Utils
} // namespace BrnEffects
