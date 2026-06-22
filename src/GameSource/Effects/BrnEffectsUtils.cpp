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

} // namespace Utils
} // namespace BrnEffects
