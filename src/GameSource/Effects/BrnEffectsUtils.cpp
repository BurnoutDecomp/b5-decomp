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
// BrnEffects::Utils::BuildUVs @ 0x822781E0  -- KEYSTONE, NOT reconstructed.
//
// The X360 body is a multi-stage hand-vectorised VMX128 pipeline over two undecoded rodata
// permute tables (unk_82CDA400 / unk_82CDA3C0) and two rodata constant vectors
// (unk_82FABA60 / unk_82FAB880). It branches on a wrap bit (quad +0x40 & 1):
//   - wrapped path: lvx128 the three input vectors, vrfim (floor) + vcmpgefp + vsel to take
//     fractional parts, a vmulfp128 / vrfim / vsubfp tiling cascade, then four
//     vperm + vsldoi lane-weaves through the rodata permute tables, stvx128 x4 to the out;
//   - unwrapped path: vcfsx a 1.0, stvx128 a rodata constant + two splats + a second rodata
//     constant as the four UV corners.
// The vperm/vsldoi weaves are driven by rodata permute selectors that are not decoded here;
// inventing the per-lane UV math would be fabrication. Honest non-corrupting stub: the four
// out vectors are zero-initialised (defined output) pending a VMX-aware lowering pass that
// decodes the permute tables. Operands referenced so the signature stays honest.
//
// BrnEffects::Utils::FastMatrix33FromEulerXYZ @ 0x8227E7A8  -- KEYSTONE, NOT reconstructed.
//
// The X360 body is a hand-vectorised VMX128 polynomial sin/cos matrix builder over six
// undecoded rodata constant vectors (unk_8307A590/680/3C0/560/3B0/670/5F0): vmrghw/vspltw
// splat the Euler lanes, two vmaddfp range-reduce them, vrfim/vsubfp wrap to a base period,
// vminfp folds the quadrant, two vmaddfp stages evaluate the sin/cos polynomial, then a long
// vspltw + vmulfp128 + vaddfp/vsubfp + vslw/vxor cascade assembles the nine matrix entries and
// stvx128 writes the three rows. The polynomial coefficients live in the undecoded rodata, so a
// scalar reconstruction cannot be written without fabricating them. Honest stub: write an
// identity 3x3 (a defined, non-corrupting rotation) pending the rodata decode + VMX lowering.
// =============================================================================

void BuildUVs(const Vector4* lpQuad, Vector4* lpaUVsOut)
{
    // KEYSTONE STUB: VMX UV-weave pipeline over undecoded rodata permute tables not
    // reconstructed (see header above). Zero the four out corners (defined output).
    (void)lpQuad;
    if (lpaUVsOut != nullptr)
    {
        lpaUVsOut[0].SetZero();
        lpaUVsOut[1].SetZero();
        lpaUVsOut[2].SetZero();
        lpaUVsOut[3].SetZero();
    }
}

void FastMatrix33FromEulerXYZ(Matrix33* lpMatrixOut, Vector3 lv3EulerAngles)
{
    // KEYSTONE STUB: VMX polynomial sin/cos matrix builder over undecoded rodata coefficients
    // not reconstructed (see header above). Write an identity rotation (defined, non-corrupting).
    (void)lv3EulerAngles;
    if (lpMatrixOut != nullptr)
    {
        lpMatrixOut->SetIdentity();
    }
}

} // namespace Utils
} // namespace BrnEffects
