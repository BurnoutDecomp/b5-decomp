#include "GameSource/Sound/Vehicles/Engines/BrnDualGinsuEffect.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Sound/Vehicles/Engines/BrnHybridExhaustControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnEngineControl.h"
#include "GameShared/GameClasses/Sound/Logic/CgsSoundLogicModule.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

// =============================================================================
// BrnSound::Vehicles::Engines::DualGinsuEffect -- out-of-line bodies.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The graph evaluators reuse the committed
// serialized Graph / Point layout (BrnSoundLoopModelData.h) BY NAME. Principal bodies:
//   LoopOutputs::UpdateGainGraph   @ 0x82699F90  (preserved)
//   LoopOutputs::Update            @ 0x826B3DD8  (preserved)
//   AttachPartialToVoice           @ 0x826EC090
//   GetFreeLoopModelVoice          @ 0x826E0F78
//   `scalar deleting destructor'   @ 0x826E0EF0  (-> ~DualGinsuEffect anchor)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// X360 dword_830080A8 is initialized by sub_82C65270 from this literal.
static const s32 guLoopModelSlotName = static_cast<s32>(
    CgsSound::Playback::Name::MakeHash("~PlayerVoice::SK_PLAYER_SLOT_NAME~"));
static const u32 guGenericRwacFactoryName = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("~GenericRwacFactory::SK_NAME~"));
static const u32 guSend01Name = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("Send01"));
static const u32 guReverbSendName = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("ReverbSend"));
static const u32 guGinsuSlotName = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("GinsuSlot"));
static const u32 guLoopPitchParameterName = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash(
        "~GenericRwacPlayerVoice::SK_PLAYER_PARAMETER_PITCH~"));
static const u32 guGinsuFrequencyName = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("GinsuFrequency"));
static const u32 guGinsuSetPitchName = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("GinsuSetPitch"));
static const u32 guGinsuSetShuffleWidthName = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("GinsuSetShuffleWidth"));
static const u32 guGinsuPauseName = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("GinsuPause"));

static const u32 guPanningAngleName = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("PanningAngle"));
static const u32 guPanningDistanceName = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("PanningDistance"));
static const u32 guPanningSizeName = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("PanningSize"));
static const u32 guPanningTwistName = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("PanningTwist"));
static const u32 guPanningCentreLevelName = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("PanningCentreLevel"));
static const u32 guPanningMainLevelName = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("PanningMainLevel"));
static const u32 guPanningLfeLevelName = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("PanningLfeLevel"));
static const u32 guCutoffFreqName = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("CutoffFreq"));
static const u32 guLowShelfFreqName = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("LowShelfFreq"));
static const u32 guLowShelfGainName = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("LowShelfGain"));
static const u32 guHighShelfFreqName = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("HighShelfFreq"));
static const u32 guHighShelfGainName = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("HighShelfGain"));
static const u32 guPeakingFreqName = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("PeakingFreq"));
static const u32 guPeakingGainName = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("PeakingGain"));
static const u32 guPeakingQName = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("PeakingQ"));

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

DualGinsuEffect::DualGinsuEffect()
    : BrnSound::Logic::BrnEffectObject()
    , mpHybridControl(nullptr)
    , mpPhysicsControl(nullptr)
    , mpEngineControl(nullptr)
    , meDualGinsuPrepareState(E_GINSU_PREPARE_STATE_NONE)
    , mLoopModelVoice()
    , maiVoiceToPartial()
    , maiPartialToVoice()
    , mLoopModelOutput()
    , miNumberOfLoops(0)
    , maLoopContentSpecs()
    , mapLoopContentSpecs()
    , mAccelGinsuVoice()
    , mDecelGinsuVoice()
    , mCarSubmix()
    , mAICarReverbSubmix()
    , mSubmixIdent(0)
    , miCarSubmixSend01Index(-1)
    , miCarSubmixReverbIndex(-1)
    , miCarSubmixPanningAngleIndex(-1)
    , miCarSubmixPanningDistanceIndex(-1)
    , miCarSubmixPanningSizeIndex(-1)
    , miCarSubmixPanningTwistIndex(-1)
    , miCarSubmixPanningCentreLevelIndex(-1)
    , miCarSubmixPanningMainLevelIndex(-1)
    , miCarSubmixPanningLfeLevelIndex(-1)
    , miCarSubmixCutoffFreq(-1)
    , miCarSubmixLowShelfFreq(-1)
    , miCarSubmixLowShelfGain(-1)
    , miCarSubmixHighShelfFreq(-1)
    , miCarSubmixHighShelfGain(-1)
    , miCarSubmixPeakingFreq(-1)
    , miCarSubmixPeakingGain(-1)
    , miCarSubmixPeakingQ(-1)
    , mLoopModelResource()
    , mAccGinsuResource()
    , mDecGinsuResource()
    , mAttribs()
    , mfAccelGinsuRPM(0.0f)
    , mfDecelGinsuRPM(0.0f)
    , mfLoopModelRPM(0.0f)
    , mfMinRpm(0.0f)
    , mfDecelMinRpm(0.0f)
    , mfLowShelfFreq(0.0f)
    , mfLowShelfGain(0.0f)
    , mfHighShelfFreq(0.0f)
    , mfHighShelfGain(0.0f)
    , mfPeakingFreq(0.0f)
    , mfPeakingGain(0.0f)
    , mfPeakingQ(0.0f)
    , miAIEngineIndex(0)
{
    // ARTIST @ 0x826C8980: the three voice slots and ten partial slots begin
    // unattached; every graph output starts at unity.
    for (s32 liVoice = 0; liVoice < KI_LOOP_VOICE_COUNT; ++liVoice)
        maiVoiceToPartial[liVoice] = -1;
    for (s32 liPartial = 0; liPartial < KI_MAX_LOOPS; ++liPartial)
    {
        maiPartialToVoice[liPartial] = -1;
        mapLoopContentSpecs[liPartial] = nullptr;
        for (s32 liAxis = 0; liAxis < LoopOutputs::E_COUNT; ++liAxis)
            mLoopModelOutput[liPartial].mafOutputs[liAxis] = 1.0f;
    }
    mLoopModelResource.Clear();
    mAccGinsuResource.Clear();
    mDecGinsuResource.Clear();
    mAttribs.Clear();
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

void DualGinsuEffect::LoopOutputs::UpdatePitchGraph(
    f64 afInput, const BrnSound::Vehicles::Engines::Graph* apGraph)
{
    CGS_ASSERT(apGraph->mu8NumOfPoints == 2, "lGraph.mu8NumOfPoints == 2");
    CGS_ASSERT(apGraph->mi8YAxis == E_PITCH,
               "lGraph.mi8YAxis == E_PITCH");

    const BrnSound::Vehicles::Engines::Point* lpaPoints = apGraph->mpaPoints;
    const f32 lfInput = static_cast<f32>(afInput);
    f32 lfValue = lfInput < lpaPoints[0].mfXpos ? lpaPoints[0].mfXpos : lfInput;
    if (lfValue > lpaPoints[1].mfXpos)
        lfValue = lpaPoints[1].mfXpos;

    const f32 lfX0 = lpaPoints[0].mfXpos;
    const f32 lfDX = lpaPoints[1].mfXpos - lfX0;
    CGS_ASSERT(!IsZero(lfDX), "!RwMathFPU::IsZero( lfDeltaX )");
    const f32 lfT = (lfValue - lfX0) / lfDX;
    mafOutputs[E_PITCH] = lpaPoints[0].mfYpos +
        lfT * (lpaPoints[1].mfYpos - lpaPoints[0].mfYpos);
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
    const CgsSound::Logic::Content* lpContent = mapLoopContentSpecs[liPartial];
    CGS_ASSERT(lpContent != nullptr, "mapLoopContentSpecs[liPartial]");
    if (!lpContent)
        return;
    lpVoice->Attach(guLoopModelSlotName, *lpContent);

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
// returning that slot, and only evicts once all KI_LOOP_VOICE_COUNT slots are examined.
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

    return liSlot;
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

s32 DualGinsuEffect::GetController(s32 aiSlot)
{
    if (aiSlot == 0) return 6;
    if (aiSlot == 1) return 0;
    if (aiSlot == 2) return 4;
    return -1;
}

void DualGinsuEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    switch (apController->GetEffectID())
    {
    case 0: mpPhysicsControl = static_cast<PhysicsControl*>(apController); break;
    case 4: mpEngineControl = static_cast<EngineControl*>(apController); break;
    case 6: mpHybridControl = static_cast<HybridExhaustControl*>(apController); break;
    default: CGS_ASSERT(false, "Cound't attach controller "); break;
    }
}

void DualGinsuEffect::SetupLoadData()
{
    if (GetStateId() != 1)
    {
        SetAttachState(CgsSound::Logic::EffectBase::E_ATTACH_STATE_PREPARING);
        return;
    }

    CGS_ASSERT(mpPhysicsControl != nullptr, "mpPhysicsControl");
    CGS_ASSERT(mpHybridControl != nullptr, "mpHybridControl");
    if (!mpPhysicsControl || !mpHybridControl)
        return;

    // ARTIST @ 0x826E41D8 reads the LoopModel through mpHybridControl (+0x34),
    // whose attribute instance is +0x4C. PhysicsControl owns the exhaust
    // collection and therefore cannot be used for the engine effect here.
    const Attrib::Gen::vehicleengine& lrAttribs =
        mpHybridControl->GetVehicleEngineAttributes();
    const char* lpcLoopModel = lrAttribs.LoopModel();
    CGS_ASSERT(lpcLoopModel != nullptr && *lpcLoopModel != '\0', "Bad Engine Data");
    if (!lpcLoopModel || !*lpcLoopModel)
        return;

    LoadAsset(lpcLoopModel, E_SOUND_DATA_POOL,
              BrnSound::Logic::ResourceRegistrar::E_DATA);
}

bool DualGinsuEffect::Attach()
{
    CgsSound::Logic::Module* lpModule = GetLogicModule();
    CGS_ASSERT(lpModule != nullptr, "mpLogicModule");
    if (!lpModule)
        return false;

    switch (meDualGinsuPrepareState)
    {
    case E_GINSU_PREPARE_STATE_NONE:
        if (!CgsSound::Logic::EffectBase::Attach())
            return false;
        meDualGinsuPrepareState = E_GINSU_PREPARE_STATE_INITIALIZE_SUBMIX;
        // ARTIST falls through and creates the submix in the same tick.
    case E_GINSU_PREPARE_STATE_INITIALIZE_SUBMIX:
        if (GetStateId() != 1)
        {
            // AI content pooling remains owned by AIVehicleStateManager.  The
            // player graph is independent and is the path mounted here.
            meDualGinsuPrepareState = E_GINSU_PREPARE_STATE_FINISHED;
            return true;
        }

        mCarSubmix.Construct(lpModule, lpModule->GetUniqueId(),
            guGenericRwacFactoryName,
            static_cast<u32>(CgsSound::Playback::Name::MakeHash(
                "PlayerCarSubmixVoiceSpec")));
        miCarSubmixSend01Index = 0;
        miCarSubmixReverbIndex = 1;
        miCarSubmixPanningAngleIndex = 8;
        miCarSubmixPanningDistanceIndex = 9;
        miCarSubmixPanningSizeIndex = 10;
        miCarSubmixPanningTwistIndex = 11;
        miCarSubmixPanningCentreLevelIndex = 12;
        miCarSubmixPanningMainLevelIndex = 13;
        miCarSubmixPanningLfeLevelIndex = 14;
        miCarSubmixCutoffFreq = 7;
        miCarSubmixLowShelfFreq = 3;
        miCarSubmixLowShelfGain = 4;
        miCarSubmixHighShelfFreq = 5;
        miCarSubmixHighShelfGain = 6;
        miCarSubmixPeakingFreq = 0;
        miCarSubmixPeakingGain = 1;
        miCarSubmixPeakingQ = 2;
        mSubmixIdent = static_cast<u32>(mCarSubmix.GetIdent());
        meDualGinsuPrepareState = E_GINSU_PREPARE_STATE_CREATE_VOICES;
        return false;

    case E_GINSU_PREPARE_STATE_WAIT_FOR_CREATE:
        meDualGinsuPrepareState = E_GINSU_PREPARE_STATE_CREATE_VOICES;
        return false;

    case E_GINSU_PREPARE_STATE_CREATE_VOICES:
    {
        mCarSubmix.Connect(guSend01Name, 1);
        mCarSubmix.Connect(guReverbSendName, 2);

        CGS_ASSERT(mpHybridControl != nullptr, "mpHybridControl");
        if (!mpHybridControl)
            return false;
        const Attrib::Gen::vehicleengine& lrAttribs =
            mpHybridControl->GetVehicleEngineAttributes();
        mfLowShelfFreq = lrAttribs.EQ_LowShelf_Freq();
        mfLowShelfGain = lrAttribs.EQ_LowShelf_Gain();
        mfHighShelfFreq = lrAttribs.EQ_HighShelf_Freq();
        mfHighShelfGain = lrAttribs.EQ_HighShelf_Gain();
        mfPeakingFreq = lrAttribs.EQ_Peaking_Freq();
        mfPeakingGain = lrAttribs.EQ_Peaking_Gain();
        mfPeakingQ = lrAttribs.EQ_Peaking_Q();
        mfMinRpm = lrAttribs.MinRpm();
        mfDecelMinRpm = lrAttribs.DecelMinRpm();

        CgsResource::ResourceHandle* lpHandle = GetResourceRegistrar().GetResource(
            nullptr, lrAttribs.LoopModel());
        if (!lpHandle)
            return false;
        mLoopModelResource = *lpHandle;

        CgsResource::ResourcePtr<LoopModelData> lLoopModel(mLoopModelResource);
        if (!lLoopModel.HasMemoryResource())
            return false;
        const LoopModelData* lpLoopModel = lLoopModel.GetMemoryResource();
        miNumberOfLoops = lpLoopModel->muNumOfPartials;
        CGS_ASSERT(miNumberOfLoops > 0 && miNumberOfLoops <= KI_MAX_LOOPS,
                   "miNumberOfLoops <= KI_MAX_LOOPS && miNumberOfLoops > 0");
        if (!miNumberOfLoops || miNumberOfLoops > KI_MAX_LOOPS)
            return false;

        const u32 luLoopVoiceSpec = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("LoopVoiceSpec"));
        for (s32 liVoice = 0; liVoice < KI_LOOP_VOICE_COUNT; ++liVoice)
        {
            mLoopModelVoice[liVoice].Construct(lpModule, lpModule->GetUniqueId(),
                                               guGenericRwacFactoryName,
                                               luLoopVoiceSpec);
            maiVoiceToPartial[liVoice] = -1;
        }
        for (s32 liPartial = 0; liPartial < KI_MAX_LOOPS; ++liPartial)
            maiPartialToVoice[liPartial] = -1;

        for (u32 liPartial = 0; liPartial < miNumberOfLoops; ++liPartial)
        {
            maLoopContentSpecs[liPartial].Construct(
                lpModule, guGenericRwacFactoryName,
                lpLoopModel->mpaPartials[liPartial].mWaveName.mHash);
            mapLoopContentSpecs[liPartial] = &maLoopContentSpecs[liPartial];
        }

        CgsSound::Logic::VoiceWrapper::CreateParams lParams;
        lParams.mpLogicModule = lpModule;
        lParams.mFactoryName = guGenericRwacFactoryName;
        lParams.mVoiceSpecName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("GinsuVoiceSpec"));
        lParams.mSlotName = guGinsuSlotName;
        lParams.mSendName = guSend01Name;
        lParams.mSubMixVoiceID = mSubmixIdent;
        lParams.miSendIndex = 0;
        lParams.mContentSpecName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash(lrAttribs.GinsuFileAccel()));
        mAccelGinsuVoice.Create(lParams);
        mAccelGinsuVoice.Play(0);

        lParams.mContentSpecName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash(lrAttribs.GinsuFileDecel()));
        mDecelGinsuVoice.Create(lParams);
        mDecelGinsuVoice.Play(0);

        meDualGinsuPrepareState = E_GINSU_PREPARE_STATE_VOICES_ARE_PLAYING;
        return false;
    }

    case E_GINSU_PREPARE_STATE_VOICES_ARE_PLAYING:
        for (u32 liPartial = 0; liPartial < miNumberOfLoops; ++liPartial)
        {
            if (!mapLoopContentSpecs[liPartial] ||
                !mapLoopContentSpecs[liPartial]->IsLoaded())
                return false;
        }
        mDecelGinsuVoice.Update();
        mAccelGinsuVoice.Update();
        if ((mDecelGinsuVoice.GetUpdateStage() != CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_PLAYING &&
             mDecelGinsuVoice.GetUpdateStage() != CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_FINISHED) ||
            (mAccelGinsuVoice.GetUpdateStage() != CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_PLAYING &&
             mAccelGinsuVoice.GetUpdateStage() != CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_FINISHED))
            return false;

        for (s32 liVoice = 0; liVoice < KI_LOOP_VOICE_COUNT; ++liVoice)
            mLoopModelVoice[liVoice].Connect(guSend01Name, mSubmixIdent);
        meDualGinsuPrepareState = E_GINSU_PREPARE_STATE_FINISHED;
        return true;

    case E_GINSU_PREPARE_STATE_FINISHED:
        return true;
    }
    return true;
}

void DualGinsuEffect::UpdateParams(f32 /*afTimeStep*/)
{
    CGS_ASSERT(mpPhysicsControl != nullptr, "mpPhysicsControl");
    if (!mpPhysicsControl || meDualGinsuPrepareState != E_GINSU_PREPARE_STATE_FINISHED ||
        !mCarSubmix.GetVoiceObject())
        return;

    // ARTIST @ 0x826B3770 refreshes the authored EQ and RPM thresholds every
    // frame, then writes the six fixed PlayerCarSubmix panning coefficients.
    CGS_ASSERT(mpHybridControl != nullptr, "mpHybridControl");
    if (!mpHybridControl)
        return;
    const Attrib::Gen::vehicleengine& lrAttribs =
        mpHybridControl->GetVehicleEngineAttributes();
    mfLowShelfFreq = lrAttribs.EQ_LowShelf_Freq();
    mfLowShelfGain = lrAttribs.EQ_LowShelf_Gain();
    mfHighShelfFreq = lrAttribs.EQ_HighShelf_Freq();
    mfHighShelfGain = lrAttribs.EQ_HighShelf_Gain();
    mfPeakingFreq = lrAttribs.EQ_Peaking_Freq();
    mfPeakingGain = lrAttribs.EQ_Peaking_Gain();
    mfPeakingQ = lrAttribs.EQ_Peaking_Q();
    mfMinRpm = lrAttribs.MinRpm();
    mfDecelMinRpm = lrAttribs.DecelMinRpm();

    mCarSubmix.SetParameter(miCarSubmixPanningDistanceIndex, 0.75f, 0,
                            &guPanningDistanceName);
    mCarSubmix.SetParameter(miCarSubmixPanningSizeIndex, 1.0f, 0,
                            &guPanningSizeName);
    mCarSubmix.SetParameter(miCarSubmixPanningTwistIndex, 0.0f, 0,
                            &guPanningTwistName);
    mCarSubmix.SetParameter(miCarSubmixPanningCentreLevelIndex, 1.0f, 0,
                            &guPanningCentreLevelName);
    mCarSubmix.SetParameter(miCarSubmixPanningMainLevelIndex, 1.0f, 0,
                            &guPanningMainLevelName);
    mCarSubmix.SetParameter(miCarSubmixPanningLfeLevelIndex, 1.0f, 0,
                            &guPanningLfeLevelName);
}

void DualGinsuEffect::ProcessUpdate()
{
    if (meDualGinsuPrepareState != E_GINSU_PREPARE_STATE_FINISHED ||
        !mCarSubmix.GetVoiceObject())
        return;

    const f32 lfCutoff = GetRWACMixerOutputValue(4, Nicotine::DMixIO::DMX_FREQ);
    const f32 lfPanningAngle =
        GetRWACMixerOutputValue(0, Nicotine::DMixIO::DMX_AZIM);
    const f32 lfSubmixGain =
        GetRWACMixerOutputValue(5, Nicotine::DMixIO::DMX_VOL);

    mCarSubmix.SetParameter(miCarSubmixPanningAngleIndex, lfPanningAngle, 0,
                            &guPanningAngleName);
    mCarSubmix.SetParameter(miCarSubmixLowShelfFreq, mfLowShelfFreq, 0,
                            &guLowShelfFreqName);
    mCarSubmix.SetParameter(miCarSubmixLowShelfGain, mfLowShelfGain, 0,
                            &guLowShelfGainName);
    mCarSubmix.SetParameter(miCarSubmixHighShelfFreq, mfHighShelfFreq, 0,
                            &guHighShelfFreqName);
    mCarSubmix.SetParameter(miCarSubmixHighShelfGain, mfHighShelfGain, 0,
                            &guHighShelfGainName);
    mCarSubmix.SetParameter(miCarSubmixPeakingFreq, mfPeakingFreq, 0,
                            &guPeakingFreqName);
    mCarSubmix.SetParameter(miCarSubmixPeakingGain, mfPeakingGain, 0,
                            &guPeakingGainName);
    mCarSubmix.SetParameter(miCarSubmixPeakingQ, mfPeakingQ, 0,
                            &guPeakingQName);
    mCarSubmix.SetGain(miCarSubmixReverbIndex, lfSubmixGain, 0,
                       &guReverbSendName);
    mCarSubmix.SetParameter(miCarSubmixCutoffFreq, lfCutoff, 0,
                            &guCutoffFreqName);

    UpdateAccelGinsu();
    UpdateDecelGinsu();
    UpdateLoopModelParams();
}

void DualGinsuEffect::DetachPartialFromVoice(s32 aiVoice)
{
    CGS_ASSERT(aiVoice >= 0 && aiVoice < KI_LOOP_VOICE_COUNT,
               "liVoice >= 0 && liVoice < KI_LOOP_VOICE_COUNT");
    if (aiVoice < 0 || aiVoice >= KI_LOOP_VOICE_COUNT)
        return;

    const s32 liPartial = maiVoiceToPartial[aiVoice];
    if (liPartial == -1)
        return;

    mLoopModelVoice[aiVoice].Stop();
    mLoopModelVoice[aiVoice].Detach(guLoopModelSlotName);
    maiPartialToVoice[liPartial] = -1;
    maiVoiceToPartial[aiVoice] = -1;
}

void DualGinsuEffect::UpdateLoopModelParams()
{
    CGS_ASSERT(mpHybridControl != nullptr, "mpHybridControl");
    CGS_ASSERT(mpPhysicsControl != nullptr, "mpPhysicsControl");
    if (!mpHybridControl || !mpPhysicsControl ||
        mLoopModelResource == CgsResource::NULLResourceHandle)
        return;

    CgsResource::ResourcePtr<LoopModelData> lLoopModel(mLoopModelResource);
    if (!lLoopModel.HasMemoryResource())
        return;
    const LoopModelData* lpLoopModel = lLoopModel.GetMemoryResource();

    const f32 lfRpm = mpHybridControl->GetGinsuRPM();
    CGS_ASSERT(mpEngineControl != nullptr, "mpEngineControl");
    if (!mpEngineControl)
        return;
    const f32 lfAccelerator =
        mpEngineControl->GetAudioThrottle().GetCurrent();
    mfLoopModelRPM.Update(lfRpm);

    const f32 lfPitchScale =
        GetRWACMixerOutputValue(3, Nicotine::DMixIO::DMX_PITCH) *
        mpEngineControl->GetAudioPitch();
    const f32 lfGainScale =
        GetRWACMixerOutputValue(2, Nicotine::DMixIO::DMX_VOL) *
        mpHybridControl->GetFinalEngineVolume().Loop;

    CGS_ASSERT(miNumberOfLoops == KI_MAX_LOOPS,
               "miNumberOfLoops == KI_MAX_LOOPS");
    const u32 luLoopCount = miNumberOfLoops < KI_MAX_LOOPS
        ? miNumberOfLoops : static_cast<u32>(KI_MAX_LOOPS);
    for (u32 liPartial = 0; liPartial < luLoopCount; ++liPartial)
    {
        LoopOutputs& lrOutput = mLoopModelOutput[liPartial];
        if (lrOutput.Update(lfRpm, lfAccelerator,
                            &lpLoopModel->mpaPartials[liPartial]))
        {
            if (maiPartialToVoice[liPartial] == -1)
            {
                const s32 liVoice = GetFreeLoopModelVoice(
                    lrOutput.mafOutputs[LoopOutputs::E_GAIN]);
                if (liVoice != -1)
                    AttachPartialToVoice(static_cast<s32>(liPartial), liVoice);
            }

            const s32 liVoice = maiPartialToVoice[liPartial];
            if (liVoice != -1)
            {
                const f32 lfGain =
                    lrOutput.mafOutputs[LoopOutputs::E_GAIN] * lfGainScale;
                const f32 lfPitch = (std::max)(0.0f, (std::min)(4.0f,
                    lrOutput.mafOutputs[LoopOutputs::E_PITCH] * lfPitchScale));
                mLoopModelVoice[liVoice].SetGain(0, lfGain, 0, &guSend01Name);
                mLoopModelVoice[liVoice].SetParameter(
                    0, lfPitch, 0, &guLoopPitchParameterName);
            }
        }
        else if (maiPartialToVoice[liPartial] != -1)
        {
            DetachPartialFromVoice(maiPartialToVoice[liPartial]);
        }
    }
}

void DualGinsuEffect::UpdateAccelGinsu()
{
    if (!mpHybridControl)
        return;

    f32 lfRpm = mpHybridControl->GetGinsuRPM();
    CGS_ASSERT(mpEngineControl != nullptr, "mpEngineControl");
    if (!mpEngineControl)
        return;
    f32 lfPitch = GetRWACMixerOutputValue(3, Nicotine::DMixIO::DMX_PITCH) *
        mpEngineControl->GetAudioPitch();
    if (lfRpm < mfMinRpm && mfMinRpm > 0.0f)
    {
        lfPitch = (lfPitch / mfMinRpm) * lfRpm;
        if (lfRpm < 0.0f)
            lfRpm = 0.0f;
    }
    mfAccelGinsuRPM.Update(lfRpm);

    const f32 lfGain =
        GetRWACMixerOutputValue(1, Nicotine::DMixIO::DMX_VOL) *
        mpHybridControl->GetFinalEngineVolume().AccelGinsu;
    mAccelGinsuVoice.SetParameter(3, 0.0f, &guGinsuPauseName);
    mAccelGinsuVoice.SetGain(0, lfGain, &guSend01Name);
    mAccelGinsuVoice.SetParameter(0, lfRpm, &guGinsuFrequencyName);
    mAccelGinsuVoice.SetParameter(1, lfPitch, &guGinsuSetPitchName);
    mAccelGinsuVoice.SetParameter(2, 1.0f, &guGinsuSetShuffleWidthName);
}

void DualGinsuEffect::UpdateDecelGinsu()
{
    if (!mpHybridControl)
        return;

    f32 lfRpm = mpHybridControl->GetGinsuRPM();
    CGS_ASSERT(mpEngineControl != nullptr, "mpEngineControl");
    if (!mpEngineControl)
        return;
    const f32 lfPitch =
        GetRWACMixerOutputValue(3, Nicotine::DMixIO::DMX_PITCH) *
        mpEngineControl->GetAudioPitch();
    mfDecelGinsuRPM.Update(lfRpm);

    const f32 lfGain =
        GetRWACMixerOutputValue(1, Nicotine::DMixIO::DMX_VOL) *
        mpHybridControl->GetFinalEngineVolume().DecelGinsu;
    mDecelGinsuVoice.SetParameter(3, 0.0f, &guGinsuPauseName);
    mDecelGinsuVoice.SetGain(0, lfGain, &guSend01Name);
    mDecelGinsuVoice.SetParameter(0, lfRpm, &guGinsuFrequencyName);
    mDecelGinsuVoice.SetParameter(1, lfPitch, &guGinsuSetPitchName);
    mDecelGinsuVoice.SetParameter(2, 1.0f, &guGinsuSetShuffleWidthName);
}

bool DualGinsuEffect::Detach()
{
    mAccelGinsuVoice.Release();
    mDecelGinsuVoice.Release();

    for (s32 liVoice = 0; liVoice < KI_LOOP_VOICE_COUNT; ++liVoice)
    {
        if (maiVoiceToPartial[liVoice] != -1)
            DetachPartialFromVoice(liVoice);
        if (mLoopModelVoice[liVoice].GetVoiceObject())
        {
            if (mLoopModelVoice[liVoice].IsPlaying())
                mLoopModelVoice[liVoice].Stop();
            mLoopModelVoice[liVoice].Destruct();
        }
    }
    for (s32 liPartial = 0; liPartial < KI_MAX_LOOPS; ++liPartial)
    {
        if (maLoopContentSpecs[liPartial].IsCreated())
            maLoopContentSpecs[liPartial].Destruct();
        mapLoopContentSpecs[liPartial] = nullptr;
        maiPartialToVoice[liPartial] = -1;
    }

    if (mCarSubmix.GetVoiceObject())
    {
        if (mCarSubmix.IsPlaying())
            mCarSubmix.Stop();
        mCarSubmix.Destruct();
    }
    if (mAICarReverbSubmix.GetVoiceObject())
    {
        if (mAICarReverbSubmix.IsPlaying())
            mAICarReverbSubmix.Stop();
        mAICarReverbSubmix.Destruct();
    }

    mLoopModelResource.Clear();
    mAccGinsuResource.Clear();
    mDecGinsuResource.Clear();
    mAttribs.Clear();
    miNumberOfLoops = 0;
    meDualGinsuPrepareState = E_GINSU_PREPARE_STATE_NONE;
    return BrnSound::Logic::BrnEffectObject::Detach();
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
