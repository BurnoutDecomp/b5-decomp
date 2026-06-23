// ============================================================================
// BrnEngines.cpp -- BrnSound::Vehicles::Engines namespace-scope helpers.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BoostEf  @ 0x826852E8
//   Physic   @ 0x82684510
//   PhysicsC @ 0x826C8708  (+ its file-local range helper @ 0x826BF998)
//
// Cited by X360 address only -- no leaked-source provenance.
// ============================================================================

#include "GameSource/Sound/Vehicles/Engines/BrnEngines.h"

#include <cmath> // std::fabs

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// FLAG (UNRECOVERABLE RODATA -> honest placeholder): see BrnEngines.h. The real
// 6 x 16-byte table at unk_82F2F508 is not in the function export; these zeros are
// a placeholder so Physic's control flow compiles and runs faithfully -- they are
// NOT the X360 table values.
const PhysicValue gakPhysicSampleTable[KU_NUM_PHYSIC_SAMPLES] =
{
    { { 0u, 0u, 0u, 0u } },
    { { 0u, 0u, 0u, 0u } },
    { { 0u, 0u, 0u, 0u } },
    { { 0u, 0u, 0u, 0u } },
    { { 0u, 0u, 0u, 0u } },
    { { 0u, 0u, 0u, 0u } },
};

// 0x826852E8. The X360 returns base + 0x70 -- the address of the boost-effect
// sub-object embedded in the owner. Return that sub-object's address by name.
void* BoostEf(EnginesBoostOwner& arOwner)
{
    return arOwner.mau8BoostEffect;
}

// 0x82684510. 64-bit linear-congruential advance, then pick one table record using
// the *previous* seed's high 32 bits reduced modulo KU_NUM_PHYSIC_SAMPLES.
//   newSeed = oldSeed * 0x5851F42D4C957F2D + 1   (the MMIX/PCG multiplier the X360
//                                                 assembles from 0x4C957F2D | 0x5851F42D<<32)
//   index   = (oldSeed >> 32) % 6                (the X360 computes this via the
//                                                 0xAAAAAAAB magic divide-by-6)
PhysicValue* Physic(PhysicValue& arResult, PhysicSeedHolder& arSeed)
{
    const u64 lu64Multiplier = 0x5851F42D4C957F2DuLL;

    const u64 lu64Old = arSeed.mu64LcgSeed;
    arSeed.mu64LcgSeed = lu64Old * lu64Multiplier + 1u;

    const u32 lu32High = static_cast<u32>(lu64Old >> 32);
    const u32 luIndex  = lu32High % KU_NUM_PHYSIC_SAMPLES;

    arResult = gakPhysicSampleTable[luIndex];
    return &arResult;
}

// 0x826BF998. File-local range helper: build a {lo, hi, aux0, aux1} record from
// four floats, nudging hi away from lo by 1e-6 when they are (near-)equal so the
// later division by (hi - lo) cannot blow up. Pure scalar; lowers store-for-store.
namespace
{
    struct EngineRange
    {
        f32 mfLo;   // result[0] = afLo
        f32 mfHi;   // result[1] = afHi (nudged when |hi-lo| < 1e-6)
        f32 mfAux0; // result[2] = afAux0
        f32 mfAux1; // result[3] = afAux1
    };

    EngineRange* BuildRange(EngineRange& arResult,
                            f32 afLo, f32 afHi, f32 afAux0, f32 afAux1)
    {
        arResult.mfLo   = 0.0f;
        arResult.mfHi   = 0.0f;
        arResult.mfAux0 = 0.0f;
        arResult.mfAux1 = 0.0f;

        arResult.mfHi   = afHi;
        const f32 lfHi  = arResult.mfHi;
        arResult.mfLo   = afLo;
        const f32 lfDiff = lfHi - arResult.mfLo;
        arResult.mfAux0 = afAux0;
        arResult.mfAux1 = afAux1;
        if (std::fabs(lfDiff) < 0.000001f)
            arResult.mfHi = lfHi + 0.000001f;
        return &arResult;
    }

    // Two-stage PowerPC fsel clamp of afValue into [0, 1] (the X360 uses
    // fneg + fsel against 0.0 and 1.0). fsel(a, b, c) = (a >= 0) ? b : c.
    f32 ClampUnit(f32 afValue)
    {
        // Stage 1: clamp below 0 -> 0. fsel(-v, 0, v) = (v <= 0) ? 0 : v.
        const f32 lfLow = (afValue <= 0.0f) ? 0.0f : afValue;
        // Stage 2: clamp above 1 -> 1. fsel(1-low, low, 1) = (low <= 1) ? low : 1.
        const f32 lfHigh = (lfLow <= 1.0f) ? lfLow : 1.0f;
        return lfHigh;
    }
}

// 0x826C8708. Evaluate the engine envelope at the dt-advanced playhead time and
// write {time, lerp(ch2), lerp(ch1)}.
EngineEnvelopeValue* PhysicsC(EngineEnvelopeValue& arResult,
                              EngineEnvelope&      arEnvelope,
                              f32                  afDeltaTime)
{
    EngineEnvelopeValue* const lpResult = &arResult;

    // Snap-to-boundary condition: no points, or the current segment index is at /
    // past the point count. (X360: r9 = mpPoints; if 0 -> snap; else compare index
    // vs count, snap when index >= count.)
    bool lbSnap;
    if (arEnvelope.mpPoints == 0)
    {
        lbSnap = true;
    }
    else
    {
        lbSnap = (arEnvelope.miIndex >= arEnvelope.miNumPoints);
    }

    if (lbSnap)
    {
        // Copy the boundary keyframe's three channels straight out.
        const EngineEnvelopePoint& lrPoint = arEnvelope.mpPoints[arEnvelope.miIndex];
        lpResult->mfTime = lrPoint.mfCh0;
        lpResult->mfOut1 = lrPoint.mfCh1;
        lpResult->mfOut2 = lrPoint.mfCh2;
        return lpResult;
    }

    const s32                   liIndex = arEnvelope.miIndex;
    const EngineEnvelopePoint*  lpSeg   = &arEnvelope.mpPoints[liIndex];
    const EngineEnvelopePoint&  lrA     = lpSeg[0]; // point[index]
    const EngineEnvelopePoint&  lrB     = lpSeg[1]; // point[index+1]

    // rangeA = {time axis (ch0), aux = output ch1}; built first (X360 var_30).
    // rangeB = {time axis (ch0), aux = output ch2}; built second (X360 var_20).
    EngineRange lRangeA;
    EngineRange lRangeB;
    BuildRange(lRangeA, lrA.mfCh0, lrB.mfCh0, lrA.mfCh1, lrB.mfCh1);
    BuildRange(lRangeB, lrA.mfCh0, lrB.mfCh0, lrA.mfCh2, lrB.mfCh2);

    // Advance the playhead and store it back.
    const f32 lfTime = arEnvelope.mfTime + afDeltaTime;
    arEnvelope.mfTime = lfTime;

    // Advance the segment when the playhead passes the next keyframe's time.
    if (lfTime > lrB.mfCh0)
        arEnvelope.miIndex = liIndex + 1;

    lpResult->mfTime = lfTime;

    // Fraction along each range's time axis, clamped to [0, 1]. (Both axes are ch0,
    // so the fractions are equal; the X360 computes them separately, as do we.)
    const f32 lfFracB = ClampUnit((lfTime - lRangeB.mfLo) / (lRangeB.mfHi - lRangeB.mfLo));
    const f32 lfFracA = ClampUnit((lfTime - lRangeA.mfLo) / (lRangeA.mfHi - lRangeA.mfLo));

    // result[2] (+8) = lerp(ch2) over rangeB's aux pair (X360 fmadds f12,f6,f9).
    lpResult->mfOut2 = lfFracB * (lRangeB.mfAux1 - lRangeB.mfAux0) + lRangeB.mfAux0;
    // result[1] (+4) = lerp(ch1) over rangeA's aux pair (X360 fmadds f0,f7,f10).
    lpResult->mfOut1 = lfFracA * (lRangeA.mfAux1 - lRangeA.mfAux0) + lRangeA.mfAux0;

    return lpResult;
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
