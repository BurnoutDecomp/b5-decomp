// =============================================================================
// BrnEffectsDebrisColourRandomiser.cpp
//   (bodies for BrnEffects::Utils::DebrisColourRandomiser; the struct itself is
//    declared in BrnEffectsDebrisColourRandomiser.h so SpawnDebris can reach it)
//
// The per-spawn debris colour randomiser (callers BrnParticle::ParticleModule::
// SpawnDebris / HandleFireDebrisBurstEvent). A sibling of the Vector3/Vector4
// randomisers in BrnEffectsUtils.cpp: it draws straight from a CgsNumeric::Random
// LCG ring (reused BY NAME via the existing friend grant in CgsRandom.h) and
// interpolates a colour from a base/range pair, forcing the alpha (w) lane to 1.
//
// No prior source and no DecFIGS DWARF exist for this TU, so the randomiser holds
// the asm-attested base/range Vector4 pair and the body is reconstructed
// store-for-store from
//   BrnEffects::Utils::DebrisColourRandomiser::Randomise @ 0x8227E698
//
// THE DRAW (asm @ 0x8227E698): advance the Random's 64-bit LCG TWICE
//   (seed = seed * MULTIPLIER + 1), each step packing the high 32 bits of the
// PRE-step state into the mantissa of an IEEE-754 float in [1, 2) and writing
// those bits into the ring at a Vector-slot ((index + 3) & 4); each step loads the
// slot's PREVIOUS contents first and subtracts 1.0 (-> [0, 1)). The two prior
// draws are lvA (step 0) and lvB (step 1). Then per lane:
//   lvScaledA = mVecBase + mVecRange * lvA      (vmaddfp v13, v13, v12, v10)
//   lvScaledB = mVecBase + mVecRange * lvB      (vmaddfp v11, v13, v12, v11)
// ^^ IDA prints vmaddfp in RAW FIELD ORDER D,A,B,C, so D = A*C + B: A == v13 == mVecRange
//    (lvx128 v13, r4, r30 with r30 == 16), B == v12 == mVecBase (lvx128 v12, r0, r4),
//    C == the [0,1) draw.  BASE + RANGE * r.  The first committed reading took the printed
//    order literally and multiplied the two BOUND vectors together, then added the raw
//    draw -- the same misread this wave corrected in Vector3Randomiser::RandomiseXYZ.
//   lvResult  = lvScaledA * lvScaledB.w         (splat w of B, vmulfp128)
//   lvResult.w = 1.0f                           (vrlimi128 of the 1.0 splat)
// =============================================================================

#include "GameSource/Effects/BrnEffectsDebrisColourRandomiser.h"

namespace BrnEffects
{
namespace Utils
{

// One LCG draw: pack the high word of the PRE-step seed into a [1, 2) float,
// write it into the ring at slot ((index + 3) & 4), advance index and seed, and
// return the slot's PREVIOUS contents mapped into [0, 1) (component - 1.0f).
Vector4 DebrisColourRandomiser::DrawNextRingVector(CgsNumeric::Random& lrRandom)
{
    const u64 luSeed   = lrRandom.muSeed;
    const u32 luSeedHi = static_cast<u32>(luSeed >> 32);

    const u32 luSlot = (lrRandom.muOldestBufferIndex + 3) & 4;

    // Load the slot's PREVIOUS contents (primed earlier) before overwriting it.
    Vector4 lvPrev;
    lvPrev.x = lrRandom.mafFloatBuffer[luSlot + 0] - 1.0f;
    lvPrev.y = lrRandom.mafFloatBuffer[luSlot + 1] - 1.0f;
    lvPrev.z = lrRandom.mafFloatBuffer[luSlot + 2] - 1.0f;
    lvPrev.w = lrRandom.mafFloatBuffer[luSlot + 3] - 1.0f;

    // Advance the LCG and write the new float-bits into the ring (inslwi r7,r9,23,9
    // == ConvertUnsignedFixed32ToFloatRepresentation).
    lrRandom.muSeed = luSeed * CgsNumeric::KU_RANDOM_MULTIPLIER + 1;
    lrRandom.mauIntegerBuffer[luSlot] =
        CgsNumeric::KU_IEEE_754_REPRESENTATION_FLOAT_ONE | (luSeedHi >> 9);
    lrRandom.muOldestBufferIndex = luSlot + 1;

    return lvPrev;
}

// @ 0x8227E698
void DebrisColourRandomiser::Randomise(Vector4& lrOut, CgsNumeric::Random& lrRandom)
{
    // Two draws from the ring (the asm runs the LCG step twice).
    const Vector4 lvA = DrawNextRingVector(lrRandom);
    const Vector4 lvB = DrawNextRingVector(lrRandom);

    // lvScaledA/B = mVecBase + mVecRange * draw, per lane (vmaddfp; see the banner).
    Vector4 lvScaledA;
    lvScaledA.x = mVecBase.x + mVecRange.x * lvA.x;
    lvScaledA.y = mVecBase.y + mVecRange.y * lvA.y;
    lvScaledA.z = mVecBase.z + mVecRange.z * lvA.z;
    lvScaledA.w = mVecBase.w + mVecRange.w * lvA.w;

    Vector4 lvScaledB;
    lvScaledB.x = mVecBase.x + mVecRange.x * lvB.x;
    lvScaledB.y = mVecBase.y + mVecRange.y * lvB.y;
    lvScaledB.z = mVecBase.z + mVecRange.z * lvB.z;
    lvScaledB.w = mVecBase.w + mVecRange.w * lvB.w;

    // Scale lvScaledA by the w lane of lvScaledB (vspltw v12, v11, 3; vmulfp128).
    const f32 lfScale = lvScaledB.w;
    lrOut.x = lvScaledA.x * lfScale;
    lrOut.y = lvScaledA.y * lfScale;
    lrOut.z = lvScaledA.z * lfScale;

    // Force the alpha lane to 1.0f (vrlimi128 of the all-ones splat into lane w).
    lrOut.w = 1.0f;
}

} // namespace Utils
} // namespace BrnEffects
