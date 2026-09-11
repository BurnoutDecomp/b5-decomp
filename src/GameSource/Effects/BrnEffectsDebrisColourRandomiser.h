#pragma once

// =============================================================================
// BrnEffectsDebrisColourRandomiser.h  (OWNING HEADER for
//                                      BrnEffects::Utils::DebrisColourRandomiser)
//
// The per-spawn debris colour randomiser. A sibling of the Vector3/Vector4
// randomisers in BrnEffectsUtils.h: it holds a base/range Vector4 pair, draws
// straight from a CgsNumeric::Random LCG ring (reused BY NAME via the existing
// friend grant in CgsRandom.h) and interpolates a colour out of that pair.
//
// The struct used to live inside BrnEffectsDebrisColourRandomiser.cpp, which made
// it unreachable from the caller the banner there names --
// BrnParticle::ParticleModule::SpawnDebris. It is declared here instead; the
// bodies stay in that .cpp.
// =============================================================================

#include "BrnCommonTypes.h"                            // Vector4
#include "GameShared/GameClasses/Numeric/CgsRandom.h"  // CgsNumeric::Random

namespace BrnEffects
{
namespace Utils
{

struct DebrisColourRandomiser
{
public:
    // Store the LOW bound and the SPAN, which is what Randomise's `base + range * r`
    // wants and what the caller's own stores build (SpawnDebris writes the low bound
    // into +0x00 and `high - low` into +0x10 before the call). The same base/range
    // contract Vector3Randomiser::Prepare carries.
    void Prepare(Vector4 lvLow, Vector4 lvHigh)
    {
        mVecBase  = lvLow;
        mVecRange.x = lvHigh.x - lvLow.x;
        mVecRange.y = lvHigh.y - lvLow.y;
        mVecRange.z = lvHigh.z - lvLow.z;
        mVecRange.w = lvHigh.w - lvLow.w;
    }

    // Draws a randomised colour into lrOut.
    void Randomise(Vector4& lrOut, CgsNumeric::Random& lrRandom);

private:
    // The base/range pair the body reads (+0x00 / +0x10).
    Vector4 mVecBase;   // +0x00
    Vector4 mVecRange;  // +0x10

    // One LCG draw from the Random ring (reused BY NAME via the friend grant in
    // CgsRandom.h). A member so the friendship reaches muSeed / the ring / index.
    static Vector4 DrawNextRingVector(CgsNumeric::Random& lrRandom);
};

} // namespace Utils
} // namespace BrnEffects
