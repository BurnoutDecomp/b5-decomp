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
// draws are lfA (step 0) and lfB (step 1). Then per lane:
//   lvScaledA = mVecBase + mVecRange * lfA      (vmaddfp v13, v13, v12, v10)
//   lvScaledB = mVecBase + mVecRange * lfB      (vmaddfp v11, v13, v12, v11)
// ^^ IDA prints vmaddfp in RAW FIELD ORDER D,A,B,C, so D = A*C + B: A == v13 == mVecRange
//    (lvx128 v13, r4, r30 with r30 == 16), B == v12 == mVecBase (lvx128 v12, r0, r4),
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
//     the colour moves along the line from mVecBase to mVecBase + mVecRange, it does not pick a
//     different fraction per channel. The tree read the slot's four words as four draws (a
//     per-channel hue jitter the console never makes). It is the same vector-slot idiom as
//     CgsNumeric::Random::RandomVecFloat (CgsRandom.cpp): RandomFloat() on the vector slot.
//   * Both combines are ONE vmaddfp each (0x8227E788 / 0x8227E78C): rounded once, std::fma here.
//     The tree's `mVecBase + mVecRange * r` rounded the product first.
//   tests/run_fxcrashvfx_debris_colour.py pins both against 0x8227E698's own words on emu64.
// =============================================================================

#include "GameSource/Effects/BrnEffectsDebrisColourRandomiser.h"

#include <cmath>   // std::fma -- the two vmaddfp combines

namespace BrnEffects
{
namespace Utils
{

// One VECTOR-SLOT draw (see the banner): the slot is ((index + 3) & 4), the value is WORD 0 of
// the quad already there minus 1.0 (-> [0, 1)), ONE LCG step writes the high word of the
// PRE-step seed into that one word as a [1, 2) float (inslwi r7,r9,23,9 ==
// ConvertUnsignedFixed32ToFloatRepresentation), and the index becomes slot + 1.
f32 DebrisColourRandomiser::DrawNextRingSplat(CgsNumeric::Random& lrRandom)
{
    const u64 luSeed   = lrRandom.muSeed;
    const u32 luSeedHi = static_cast<u32>(luSeed >> 32);

    const u32 luSlot = (lrRandom.muOldestBufferIndex + 3) & 4;

    // The slot's PREVIOUS word 0 (lvx128 + the vperm splat), read before the refill.
    const f32 lfPrevious = lrRandom.mafFloatBuffer[luSlot] - 1.0f;

    lrRandom.muSeed = luSeed * CgsNumeric::KU_RANDOM_MULTIPLIER + 1;
    lrRandom.mauIntegerBuffer[luSlot] =
        CgsNumeric::KU_IEEE_754_REPRESENTATION_FLOAT_ONE | (luSeedHi >> 9);
    lrRandom.muOldestBufferIndex = luSlot + 1;

    return lfPrevious;
}

// @ 0x8227E698
void DebrisColourRandomiser::Randomise(Vector4& lrOut, CgsNumeric::Random& lrRandom)
{
    // Two draws from the ring (the asm runs the LCG step twice), each ONE number.
    const f32 lfA = DrawNextRingSplat(lrRandom);
    const f32 lfB = DrawNextRingSplat(lrRandom);

    // lvScaledB = mVecBase + mVecRange * lfB (vmaddfp v11 @0x8227E788) -- only its w is read.
    const f32 lfScale = std::fma(mVecRange.w, lfB, mVecBase.w);

    // lvScaledA = mVecBase + mVecRange * lfA (vmaddfp v13 @0x8227E78C), times lvScaledB.w
    // (vspltw v12, v11, 3; vmulfp128 @0x8227E794).
    lrOut.x = std::fma(mVecRange.x, lfA, mVecBase.x) * lfScale;
    lrOut.y = std::fma(mVecRange.y, lfA, mVecBase.y) * lfScale;
    lrOut.z = std::fma(mVecRange.z, lfA, mVecBase.z) * lfScale;

    // Force the alpha lane to 1.0f (vrlimi128 of the all-ones splat into lane w, @0x8227E798).
    lrOut.w = 1.0f;
}

} // namespace Utils
} // namespace BrnEffects
