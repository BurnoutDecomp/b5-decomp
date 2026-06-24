#include "GameSource/Sound/Vehicles/Engines/BrnDualGinsuEffect.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// =============================================================================
// BrnSound::Vehicles::Engines::DualGinsuEffect::LoopOutputs
//   Update          @ 0x826B3DD8
//   UpdateGainGraph @ 0x82699F90
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnDualGinsuEffect.h for the
// LoopOutputs layout. The graph evaluators reuse the committed serialized Graph /
// Point layout (BrnSoundLoopModelData.h) BY NAME.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{
namespace DualGinsuEffect
{

// FLT_EPSILON, the IsZero threshold the X360 build inlines for RwMathFPU::IsZero.
// flt_820AA114 = +1.192092896e-7f, flt_82002514 = -1.192092896e-7f (asm-attested as
// the upper/lower bounds of the |x| < epsilon test).
static const f32 KF_FLT_EPSILON = 1.192092896e-7f;

// Range-check bounds for the gain output (flt_82001CC0 = 0.0, flt_82001C98 = 1.0).
static const f32 KF_ZERO = 0.0f;
static const f32 KF_ONE  = 1.0f;

// RwMathFPU::IsZero — |afValue| < FLT_EPSILON (matches the inlined ±epsilon compare).
static inline bool IsZero( f32 afValue )
{
    return !(afValue > KF_FLT_EPSILON) && !(afValue < -KF_FLT_EPSILON);
}

// @ 0x82699F90.
bool LoopOutputs::UpdateGainGraph( f64 afInput,
                                   const BrnSound::Vehicles::Engines::Graph* apGraph )
{
    CGS_ASSERT(apGraph->mu8NumOfPoints > 1, "lGraph.mu8NumOfPoints > 1");
    CGS_ASSERT(apGraph->mi8XAxis != LoopInputs::E_UNKNOWN, "lGraph.mi8XAxis != LoopInputs::E_UNKNOWN");
    CGS_ASSERT(apGraph->mi8YAxis != E_UNKNOWN, "lGraph.mi8YAxis != E_UNKNOWN");

    const BrnSound::Vehicles::Engines::Point* lpaPoints = apGraph->mpaPoints;
    const s32 liNumPoints = apGraph->mu8NumOfPoints;
    const s32 liLastSeg = liNumPoints - 1;
    const f32 lfInput = static_cast<f32>(afInput);

    // Clamp the input into [points[0].x, points[last].x] (the two fsel idioms).
    //   f0 = max(points[0].x, input)         (fsel on points[0].x - input)
    //   x  = min(f0, points[last].x)          (fsel on points[last].x - f0)
    f32 lfLow = lpaPoints[0].mfXpos;
    f32 lfClamped = ((lfLow - lfInput) >= 0.0f) ? lfLow : lfInput;
    f32 lfHigh = lpaPoints[liNumPoints - 1].mfXpos;
    lfClamped = ((lfHigh - lfClamped) >= 0.0f) ? lfClamped : lfHigh;

    // Find the segment [points[seg].x, points[seg+1].x) the clamped value falls in.
    s32 liSeg = 0;
    if ( liLastSeg > 0 )
    {
        s32 liIndex = 0;
        do
        {
            const BrnSound::Vehicles::Engines::Point* lpSeg = &lpaPoints[liIndex];
            if ( lfClamped >= lpSeg[0].mfXpos && lfClamped < lpSeg[1].mfXpos )
                break;
            liSeg = liIndex + 1;
            liIndex = liSeg;
        }
        while ( liSeg < liLastSeg );
    }

    // Linear interpolation across the chosen segment.
    const f32 lfX0 = lpaPoints[liSeg].mfXpos;
    const f32 lfDeltaX = lpaPoints[liSeg + 1].mfXpos - lfX0;
    CGS_ASSERT(!IsZero(lfDeltaX), "!RwMathFPU::IsZero( lfDeltaX )");

    const f32 lfY0 = lpaPoints[liSeg].mfYpos;
    const f32 lfDeltaY = lpaPoints[liSeg + 1].mfYpos - lfY0;
    const f32 lfGain = (((lfClamped - lfX0) / lfDeltaX) * lfDeltaY) + lfY0;

    CGS_ASSERT(apGraph->mi8YAxis == E_GAIN, "lGraph.mi8YAxis == E_GAIN");

    // Multiply the GAIN output in place (mafOutputs[+4] == mafOutputs[E_GAIN]).
    mafOutputs[E_GAIN] *= lfGain;

    // Range-check the just-written output (indexed by the graph's output axis).
    const f32 lfOut = mafOutputs[apGraph->mi8YAxis];
    CGS_ASSERT(lfOut >= KF_ZERO && lfOut <= KF_ONE,
        "Volume out of range?! You got your data right?!");

    // Return whether the interpolated gain is non-zero.
    return !IsZero(lfGain);
}

// @ 0x826B3DD8.
s32 LoopOutputs::Update( f64 aRpm, f64 aAccelerator,
                         const BrnSound::Vehicles::Engines::Partial* apPartial )
{
    // Reset all three outputs to 1.0 (stfs flt_82001C98 at +0/+4/+8).
    mafOutputs[E_UNKNOWN] = KF_ONE;
    mafOutputs[E_GAIN]    = KF_ONE;
    mafOutputs[E_PITCH]   = KF_ONE;

    CGS_ASSERT(apPartial->mu8NumOfGraphs == 3, "lPartial.mu8NumOfGraphs == 3");

    const BrnSound::Vehicles::Engines::Graph* lpaGraphs = apPartial->mpaGraphs;

    // graph[1]: RPM -> GAIN.
    CGS_ASSERT(lpaGraphs[1].mi8XAxis == LoopInputs::E_RPM, "lPartial.mpaGraphs[1].mi8XAxis == LoopInputs::E_RPM");
    CGS_ASSERT(lpaGraphs[1].mi8YAxis == E_GAIN, "lPartial.mpaGraphs[1].mi8YAxis == LoopOutputs::E_GAIN");

    if ( UpdateGainGraph(aRpm, &lpaGraphs[1]) )
    {
        // graph[2]: ACCELERATOR -> GAIN.
        CGS_ASSERT(lpaGraphs[2].mi8XAxis == LoopInputs::E_ACCELERATOR, "lPartial.mpaGraphs[2].mi8XAxis == LoopInputs::E_ACCELERATOR");
        CGS_ASSERT(lpaGraphs[2].mi8YAxis == E_GAIN, "lPartial.mpaGraphs[2].mi8YAxis == LoopOutputs::E_GAIN");
        UpdateGainGraph(aAccelerator, &lpaGraphs[2]);

        // graph[0]: RPM -> PITCH.
        CGS_ASSERT(lpaGraphs[0].mi8XAxis == LoopInputs::E_RPM, "lPartial.mpaGraphs[0].mi8XAxis == LoopInputs::E_RPM");
        CGS_ASSERT(lpaGraphs[0].mi8YAxis == E_PITCH, "lPartial.mpaGraphs[0].mi8YAxis == LoopOutputs::E_PITCH");
        UpdatePitchGraph(aRpm, &lpaGraphs[0]);

        return 1;
    }

    return 0;
}

} // namespace DualGinsuEffect
} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
