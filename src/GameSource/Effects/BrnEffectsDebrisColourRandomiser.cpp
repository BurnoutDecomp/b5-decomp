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
// DecFIGS BrnEffectsUtils.h:221-222 attests the two Vector4 members and their
// names. ARTIST @0x8227E698 confirms the X360 offsets (+0/+0x10) and arbitrates
// the X360 random-ring and colour arithmetic reconstructed below.
//
// THE DRAW (asm @ 0x8227E698): advance the Random's 64-bit LCG TWICE
//   (seed = seed * MULTIPLIER + 1), each step packing the high 32 bits of the
// PRE-step state into the mantissa of an IEEE-754 float in [1, 2) and writing
// those bits into the ring at a Vector-slot ((index + 3) & 4); each step loads the
// slot's PREVIOUS contents first and subtracts 1.0 (-> [0, 1)). The two prior
// draws are lfA (step 0) and lfB (step 1). Then per lane:
//   lvScaledA = min + diff * lfA (vmaddfp v13, v13, v12, v10)
//   lvScaledB = min + diff * lfB (vmaddfp v11, v13, v12, v11)
// IDA prints vmaddfp in RAW FIELD ORDER D,A,B,C; semantics are D = A*C + B:
// A == v13 == mDiffSaturationBrightness (lvx128 v13, r4, r30 with r30 == 16),
// B == v12 == mMinSaturationBrightness (lvx128 v12, r0, r4),
//    C == the [0,1) draw.  BASE + RANGE * r.  The first committed reading took the printed
//    order literally and multiplied the two BOUND vectors together, then added the raw
//    draw -- the same misread this wave corrected in Vector3Randomiser::RandomiseXYZ.
//   lvResult  = lvScaledA * lvScaledB.w         (splat w of B, vmulfp128)
//   lvResult.w = 1.0f                           (vrlimi128 of the 1.0 splat)
//
// ⛔ FX-CRASHVFX 2026-09-25 -- EACH DRAW IS ONE NUMBER, SPLATTED, AND EACH COMBINE IS FUSED.
//   * The quad loaded from the slot is SPLATTED FROM ITS WORD 0 before the 1.0 comes off:
//     `lvsl v13/v12, 0, r8` with r8 == 0 (0x8227E6B8 li r8, 0; 0x8227E6D8 / 0x8227E6DC) is the
//     identity permute, `vspltw v7, v13, 0` / `vspltw v6, v12, 0` (0x8227E6E4 / 0x8227E6E8) splat
//     its first word 0x00010203, and `vperm v13, v13, v13, v6` / `.., v7` (0x8227E714 / 0x8227E754)
//     turn the loaded quad into (q0, q0, q0, q0). So all four lanes of lvScaledA share ONE draw --
//     the colour moves from mMinSaturationBrightness to the upper bound, it does not pick a
//     different fraction per channel. The tree read the slot's four words as four draws (a
//     per-channel hue jitter the console never makes). It is the same vector-slot idiom as
//     CgsNumeric::Random::RandomVecFloat (CgsRandom.cpp): RandomFloat() on the vector slot.
//   * Both combines are ONE vmaddfp each (0x8227E788 / 0x8227E78C): one native fused rounding.
//     The tree's `mMinSaturationBrightness + mDiffSaturationBrightness * r` rounded the product first.
// =============================================================================

#include "GameSource/Effects/BrnEffectsDebrisColourRandomiser.h"

namespace BrnEffects
{
namespace Utils
{

// One VECTOR-SLOT draw (see the banner): the slot is ((index + 3) & 4), the value is WORD 0 of
// the quad already there minus 1.0 (-> [0, 1)), ONE LCG step writes the high word of the
// PRE-step seed into that one word as a [1, 2) float (inslwi r7,r9,23,9 ==
// ConvertUnsignedFixed32ToFloatRepresentation), and the index becomes slot + 1.
// @ 0x8227E698
Vector4 DebrisColourRandomiser::Randomise(CgsNumeric::Random& lrRandom)
{
    // First vector-slot draw: read word 0 of ((index + 3) & 4) before refilling
    // that slot. The LCG packs the PRE-step seed's high word into [1, 2).
    const u64 luSeedA = lrRandom.muSeed;
    const u32 luSlotA = (lrRandom.muOldestBufferIndex + 3) & 4;
    const f32 lfA = lrRandom.mafFloatBuffer[luSlotA] - 1.0f;
    lrRandom.muSeed = luSeedA * CgsNumeric::KU_RANDOM_MULTIPLIER + 1;
    lrRandom.mauIntegerBuffer[luSlotA] =
        CgsNumeric::KU_IEEE_754_REPRESENTATION_FLOAT_ONE
        | (static_cast<u32>(luSeedA >> 32) >> 9);
    lrRandom.muOldestBufferIndex = luSlotA + 1;

    // The second draw uses the just-stepped seed and the next slot, in the same
    // read/refill/index order as ARTIST 0x8227E6EC..77C.
    const u64 luSeedB = lrRandom.muSeed;
    const u32 luSlotB = (lrRandom.muOldestBufferIndex + 3) & 4;
    const f32 lfB = lrRandom.mafFloatBuffer[luSlotB] - 1.0f;
    lrRandom.muSeed = luSeedB * CgsNumeric::KU_RANDOM_MULTIPLIER + 1;
    lrRandom.mauIntegerBuffer[luSlotB] =
        CgsNumeric::KU_IEEE_754_REPRESENTATION_FLOAT_ONE
        | (static_cast<u32>(luSeedB >> 32) >> 9);
    lrRandom.muOldestBufferIndex = luSlotB + 1;

    // ARTIST @0x8227E788..794: both bounds use fused vector multiply-add; the
    // w lane of the second is then broadcast as the brightness scale.
    const __vector4 lvDrawA = __vspltw(__lvlx(&lfA, 0), 0);
    const __vector4 lvDrawB = __vspltw(__lvlx(&lfB, 0), 0);
    const __vector4 lvScaledB = __vmaddfp(
        mDiffSaturationBrightness.mV, lvDrawB, mMinSaturationBrightness.mV);
    __vector4 lvResult = __vmaddfp(
        mDiffSaturationBrightness.mV, lvDrawA, mMinSaturationBrightness.mV);
    lvResult = __vmulfp(lvResult, __vspltw(lvScaledB, 3));

    // ARTIST @0x8227E798 inserts the exact 1.0 splat into alpha.
    const __vector4 lvOne = __vcfsx(__vspltisw(1), 0);
    lvResult = __vrlimi(lvResult, lvOne, 1, 0);
    return Vector4(lvResult);
}

} // namespace Utils
} // namespace BrnEffects
