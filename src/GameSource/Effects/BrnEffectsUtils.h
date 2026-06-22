#pragma once

// =============================================================================
// BrnEffectsUtils.h  (OWNING HEADER for BrnEffects::Utils::Vector3Randomiser
//                     and BrnEffects::Utils::Vector4Randomiser)
//
// The two per-call vector randomisers. Each holds a pair of vectors
// (mVecA / mVecB, set up by the other-TU Prepare) and draws random components
// straight from a CgsNumeric::Random's internal LCG ring (reused BY NAME via
// the friend grant in CgsRandom.h).
//
// Layout (DWARF references/DecFIGS/dwarfdump/GameSource/Effects/BrnEffectsUtils.h):
//   :93  Vector3Randomiser { Vector3 mVecA(:130); Vector3 mVecB(:131); }
//   :135 Vector4Randomiser { Vector4 mVecA(:172); Vector4 mVecB(:173); }
//
// Bodied here (asm-attested, reconstructed store-for-store):
//   Vector3Randomiser::RandomiseXYZ  @ 0x82277EC8
//   Vector4Randomiser::RandomiseXYZW @ 0x82277FB8
// Prepare / RandomInterpolate are other-TU surface -> declared-only.
//
// THE DRAW (both): the randomiser advances the Random's 64-bit LCG
//   seed = seed * KU_RANDOM_MULTIPLIER + 1
// once per output component, packs the high 32 bits of each step into the
// mantissa of an IEEE-754 float in [1, 2) and writes those float-bits into the
// Random's ring at a Vector-slot chosen by ((index + 3) & 4) (a 2-slot Vector4
// double-buffer over the 8-entry f32 ring). It then RETURNS the slot that was
// primed on the PREVIOUS call (a 1-deep pipeline): the slot is loaded BEFORE the
// new bits are written, 1.0 is subtracted (giving components in [0, 1)), and the
// result is  mVecA * mVecB + (previousDraw - 1.0)  computed per lane.
// =============================================================================

#include "BrnCommonTypes.h"   // Vector3, Vector4 (rw::math::vpu float lanes)
#include "GameShared/GameClasses/Numeric/CgsRandom.h"  // CgsNumeric::Random (+ LCG constants)

namespace BrnEffects
{
namespace Utils
{

struct Vector3Randomiser
{
private:
    // DWARF BrnEffectsUtils.h:130 / :131.
    Vector3 mVecA;
    Vector3 mVecB;

public:
    // DWARF BrnEffectsUtils.h:100 / :109. Other-TU bodies.
    void    Prepare(Vector3 lvA, Vector3 lvB);
    Vector3 RandomInterpolate(CgsNumeric::Random &lrRandom);

    // DWARF BrnEffectsUtils.h:121 / asm @ 0x82277EC8.
    Vector3 RandomiseXYZ(CgsNumeric::Random &lrRandom)
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
};

struct Vector4Randomiser
{
private:
    // DWARF BrnEffectsUtils.h:172 / :173.
    Vector4 mVecA;
    Vector4 mVecB;

public:
    // DWARF BrnEffectsUtils.h:142 / :151. Other-TU bodies.
    void    Prepare(Vector4 lvA, Vector4 lvB);
    Vector4 RandomInterpolate(CgsNumeric::Random &lrRandom);

    // DWARF BrnEffectsUtils.h:163 / asm @ 0x82277FB8.
    Vector4 RandomiseXYZW(CgsNumeric::Random &lrRandom)
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
};

} // namespace Utils
} // namespace BrnEffects
