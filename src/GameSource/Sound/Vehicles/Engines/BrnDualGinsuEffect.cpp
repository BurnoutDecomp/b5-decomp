#include "GameSource/Sound/Vehicles/Engines/BrnDualGinsuEffect.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// =============================================================================
// BrnSound::Vehicles::Engines::DualGinsuEffect -- out-of-line bodies.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The graph evaluators reuse the committed
// serialized Graph / Point layout (BrnSoundLoopModelData.h) BY NAME. Function set:
//   LoopOutputs::UpdateGainGraph   @ 0x82699F90  (preserved)
//   LoopOutputs::Update            @ 0x826B3DD8  (preserved)
//   AttachPartialToVoice           @ 0x826EC090
//   GetFreeLoopModelVoice          @ 0x826E0F78
//   `scalar deleting destructor'   @ 0x826E0EF0  (-> ~DualGinsuEffect anchor)
//
// BLOCKED (NOT bodied): DualGinsuEffect::DualGinsuEffect @ 0x826C8980. Its inlined
// full-object ctor zeroes 15 contiguous f32 words at +0x2AC..+0x2E4, of which only
// mfMinRpm..mfPeakingQ + miAIEngineIndex are DWARF-named; the rest fall in an un-homed
// DataPoint<float>/ResourceHandle/Command::QueueElement tail whose layout is not
// recoverable in scope, plus embedded submix Voices/EQ blocks. Per the anti-fabrication
// HARD RULE this one function is blocked pending those types' homes; its declaration
// remains for the class shape (DualGinsuExhaustEffect chains it BY NAME under cl /c) but
// no body is emitted.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// The interned loop-model slot name + the un-homed voice-content attach helper the X360
// AttachPartialToVoice calls (dword_830080A8 / sub_826EAE38). Both are un-homed externs
// modelled as declared shims -- their real homes are separate recon slices.
extern const s32 guLoopModelSlotName;
void DualGinsuEffect_AttachVoiceContent( CgsSound::Logic::Voice* lpVoice,
                                         s32 liSlotName,
                                         const CgsSound::Logic::Content* lpContentSpec );

// ---------------------------------------------------------------------------
// FLT_EPSILON, the IsZero threshold the X360 build inlines for RwMathFPU::IsZero.
static const f32 KF_FLT_EPSILON = 1.192092896e-7f;
static const f32 KF_ZERO = 0.0f;
static const f32 KF_ONE  = 1.0f;

// RwMathFPU::IsZero -- |afValue| < FLT_EPSILON (matches the inlined +/-epsilon compare).
static inline bool IsZero( f32 afValue )
{
    return !(afValue > KF_FLT_EPSILON) && !(afValue < -KF_FLT_EPSILON);
}

// ---------------------------------------------------------------------------
// DualGinsuEffect::LoopOutputs::UpdateGainGraph  @ 0x82699F90
// ---------------------------------------------------------------------------
bool DualGinsuEffect::LoopOutputs::UpdateGainGraph( f64 afInput,
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

    return !IsZero(lfGain);
}

// ---------------------------------------------------------------------------
// DualGinsuEffect::LoopOutputs::Update  @ 0x826B3DD8
// ---------------------------------------------------------------------------
s32 DualGinsuEffect::LoopOutputs::Update( f64 aRpm, f64 aAccelerator,
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

// ---------------------------------------------------------------------------
// DualGinsuEffect::AttachPartialToVoice  @ 0x826EC090
//   maiVoiceToPartial[liVoice]   = liPartial;
//   maiPartialToVoice[liPartial] = liVoice;
//   DualGinsuEffect_AttachVoiceContent(&mLoopModelVoice[liVoice], guLoopModelSlotName,
//                                      mapLoopContentSpecs[liPartial]);
//   mLoopModelVoice[liVoice].Play(0);
//
// Bind the partial<->voice mapping both ways, hand the loop-model content spec to the
// (un-homed) voice-content attach helper, then start the voice.
// FLAG: sub_826EAE38 is an external/un-homed voice-content attach helper; called through
// the un-homed shim declared above. dword_830080A8 (the interned loop-model slot-name) is
// likewise un-homed and modelled as the extern ident guLoopModelSlotName.
// ---------------------------------------------------------------------------
void DualGinsuEffect::AttachPartialToVoice(s32 liPartial, s32 liVoice)
{
    maiVoiceToPartial[liVoice]   = liPartial;
    maiPartialToVoice[liPartial] = liVoice;

    CgsSound::Logic::Voice* lpVoice = &mLoopModelVoice[liVoice];
    DualGinsuEffect_AttachVoiceContent(lpVoice, guLoopModelSlotName,
                                       mapLoopContentSpecs[liPartial]);

    lpVoice->Play(0);
}

// ---------------------------------------------------------------------------
// DualGinsuEffect::GetFreeLoopModelVoice  @ 0x826E0F78
//
// Scan the KI_LOOP_VOICE_COUNT active loop-voice slots for the one whose assigned
// partial has the quietest GAIN output. If that minimum gain is below the caller's
// threshold (afThreshold), stop+detach that voice, clear its partial<->voice mapping
// both ways, and return the freed slot index. Otherwise return -1 (nothing evicted).
// The scan stops early at the first maiVoiceToPartial[i] == -1 (an unused slot),
// returning 0, and only evicts once all KI_LOOP_VOICE_COUNT slots are examined.
// ---------------------------------------------------------------------------
s32 DualGinsuEffect::GetFreeLoopModelVoice(f32 afThreshold)
{
    s32 liBestSlot = -1;
    f32 lfMinGain  = 1.0f;
    s32 liSlot     = 0;

    const s32* lpiVoiceToPartial = &maiVoiceToPartial[0];
    while (*lpiVoiceToPartial != -1)
    {
        const f32 lfGain = mLoopModelOutput[*lpiVoiceToPartial].mafOutputs[LoopOutputs::E_GAIN];
        if (lfGain < lfMinGain)
        {
            lfMinGain  = lfGain;
            liBestSlot = liSlot;
        }

        ++liSlot;
        ++lpiVoiceToPartial;
        if (liSlot >= KI_LOOP_VOICE_COUNT)
        {
            if (afThreshold <= lfMinGain)
                return -1;

            CgsSound::Logic::Voice* lpVoice = &mLoopModelVoice[liBestSlot];
            lpVoice->Stop();
            lpVoice->Detach(guLoopModelSlotName);

            const s32 liPartial = maiVoiceToPartial[liBestSlot];
            maiPartialToVoice[liPartial]  = -1;
            maiVoiceToPartial[liBestSlot] = -1;
            return liBestSlot;
        }
    }

    return 0;
}

// ---------------------------------------------------------------------------
// ~DualGinsuEffect  @ 0x826E0EF0  (anchor for the X360 `scalar deleting destructor').
// Forwards to the inherited BrnEffectObject teardown (dual-base settle) + the embedded
// Voice/VoiceWrapper member dtors (compiler-synthesised); this leaf adds nothing. The
// (a2 & 1) allocator-free tail is left to the host toolchain (off_82FFB954 not homed).
// ---------------------------------------------------------------------------
DualGinsuEffect::~DualGinsuEffect()
{
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
