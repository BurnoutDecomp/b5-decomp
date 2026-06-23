// =============================================================================
// BrnEffectsDebrisColourRandomiser.cpp
//   (OWNING HOME for BrnEffects::Utils::DebrisColourRandomiser)
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
//   lvScaledA = mVecRange * mVecBase + lvA      (vmaddfp v13)
//   lvScaledB = mVecRange * mVecBase + lvB      (vmaddfp v11)
//   lvResult  = lvScaledA * lvScaledB.w         (splat w of B, vmulfp128)
//   lvResult.w = 1.0f                           (vrlimi128 of the 1.0 splat)
// =============================================================================

#include "BrnCommonTypes.h"   // Vector4 (rw::math::vpu float lanes)
#include "GameShared/GameClasses/Numeric/CgsRandom.h"  // CgsNumeric::Random (+ LCG constants)

namespace BrnEffects
{
namespace Utils
{

struct DebrisColourRandomiser
{
private:
    // asm-attested base/range pair the body reads (lvx128 @ +0x00 / +0x10 off _R4).
    Vector4 mVecBase;   // +0x00
    Vector4 mVecRange;  // +0x10

    // One LCG draw from the Random ring (reused BY NAME via the friend grant in
    // CgsRandom.h). A member so the friendship reaches muSeed / the ring / index.
    static Vector4 DrawNextRingVector(CgsNumeric::Random& lrRandom);

public:
    // @ 0x8227E698. Draws a randomised colour into lrOut.
    void Randomise(Vector4& lrOut, CgsNumeric::Random& lrRandom);
};

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

    // lvScaledA/B = mVecRange * mVecBase + draw, per lane (vmaddfp).
    Vector4 lvScaledA;
    lvScaledA.x = mVecRange.x * mVecBase.x + lvA.x;
    lvScaledA.y = mVecRange.y * mVecBase.y + lvA.y;
    lvScaledA.z = mVecRange.z * mVecBase.z + lvA.z;
    lvScaledA.w = mVecRange.w * mVecBase.w + lvA.w;

    Vector4 lvScaledB;
    lvScaledB.x = mVecRange.x * mVecBase.x + lvB.x;
    lvScaledB.y = mVecRange.y * mVecBase.y + lvB.y;
    lvScaledB.z = mVecRange.z * mVecBase.z + lvB.z;
    lvScaledB.w = mVecRange.w * mVecBase.w + lvB.w;

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
