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

    // DWARF BrnEffectsUtils.h:121 / asm @ 0x82277EC8. Defined in BrnEffectsUtils.cpp.
    Vector3 RandomiseXYZ(CgsNumeric::Random &lrRandom);
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

    // DWARF BrnEffectsUtils.h:163 / asm @ 0x82277FB8. Defined in BrnEffectsUtils.cpp.
    Vector4 RandomiseXYZW(CgsNumeric::Random &lrRandom);
};

} // namespace Utils
} // namespace BrnEffects
