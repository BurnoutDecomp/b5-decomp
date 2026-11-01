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
    // Store the minimum colour and high-minus-low span consumed by ARTIST's
    // `minimum + span * draw` fused vector operation.
    void Prepare(Vector4 lvLow, Vector4 lvHigh)
    {
        mMinSaturationBrightness = lvLow;
        mDiffSaturationBrightness.mV = __vsubfp(lvHigh.mV, lvLow.mV);
    }

    // Returns the native Vector4 result; X360 passes the hidden result pointer in r3.
    Vector4 Randomise(CgsNumeric::Random& lrRandom);

private:
    // DecFIGS BrnEffectsUtils.h:221; ARTIST reads this vector at this+0x00.
    Vector4 mMinSaturationBrightness;
    // DecFIGS BrnEffectsUtils.h:222; ARTIST reads this vector at this+0x10.
    Vector4 mDiffSaturationBrightness;

};

} // namespace Utils
} // namespace BrnEffects
