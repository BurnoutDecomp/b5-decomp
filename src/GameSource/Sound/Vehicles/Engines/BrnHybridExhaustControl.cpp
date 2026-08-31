#include "GameSource/Sound/Vehicles/Engines/BrnHybridExhaustControl.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnEngineControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnShiftControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnClutchControl.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"

#include <algorithm>
#include <cmath>

// =============================================================================
// BrnSound::Vehicles::Engines::HybridExhaustControl -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnHybridExhaustControl.h for the
// base rationale + opaque-crossfade-span FLAG. Recon'd function set:
//   Create(bool)                   @ 0x826B34E0  (the RTTI factory hook)
//   HybridExhaustControl()         @ 0x826AF938  (field-init ctor)
//   `vector deleting destructor'   @ 0x826AFA60  (-> ~HybridExhaustControl anchor)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

namespace
{
const f32 KF_DECEL_GINSU_FADE_IN = 0.05f; // flt_82F2CC64
const f32 KF_THROTTLE_THRESHOLD = 1.0f;    // flt_82F2CC68
const f32 KF_DELTA_RPM_OFFSET = 6.0f;      // flt_82F2CC6C
const f32 KF_AI_DELTA_RPM_OFFSET = 15.0f;  // flt_82F2CC70
const f32 KF_SMOOTHNESS_FACTOR = 0.2f;     // flt_82F2CC74
const f32 KF_LOW_PASS_SMOOTHING_FACTOR = 6000.0f; // flt_82F2CC78
const f32 KF_GINSU_CUTOFF = 25000.0f;      // flt_820B3D08

f32 Clamp01(f32 lfValue)
{
    return (std::max)(0.0f, (std::min)(1.0f, lfValue));
}

f32 Lerp(f32 lfStart, f32 lfFinish, f32 lfAmount)
{
    return lfStart + (lfFinish - lfStart) * lfAmount;
}

f32 SmoothTowards(f32 lfCurrent, f32 lfTarget, f32 lfStep)
{
    const f32 lfDelta = lfTarget - lfCurrent;
    if (std::fabs(lfDelta) <= lfStep)
        return lfTarget;
    return lfCurrent + (lfDelta < 0.0f ? -lfStep : lfStep);
}
}

// ---------------------------------------------------------------------------
// HybridExhaustControl::Create  @ 0x826B34E0   (the factory hook)
//
// The X360 allocates a 304-byte (0x130) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "HybridExhaustControl" and inline-constructs a
// HybridExhaustControl, returning it as the +4 IResourceRequester second-base view.
// `abFlavour` selects only the operator-new flavour (0/1); both arms allocate the same
// size and run the same ctor.
//
// FLAG (allocator gate): CgsSound::MemBase does NOT model operator new(size, tag,
// flavour) here, so this uses the host `new`; the +4 adjust is the static_cast to the
// IResourceRequester base. The 0x130 size is documentation only.
// ---------------------------------------------------------------------------
BrnSound::Logic::IResourceRequester* HybridExhaustControl::Create( bool abFlavour )
{
    (void)abFlavour; // selects only the MemBase operator-new flavour (0/1)
    return static_cast<BrnSound::Logic::IResourceRequester*>( new HybridExhaustControl() );
}

// ---------------------------------------------------------------------------
// HybridExhaustControl::HybridExhaustControl  @ 0x826AF938  (field-init ctor)
//
// The BrnEffectControl dual-base sub-object (both vptr stores + the base bookkeeping
// clears) is emitted by the base default ctor; this leaf then wires the declared
// members: the two vehicleengine attribute instances (constructed empty/unbound), the
// Average<3,f32> / three DataPoint<f32> (default-zeroed), the Graph over its embedded
// Vector2[6] table, the two EngineMix values, and the mfPercentOf* thresholds.
// ---------------------------------------------------------------------------
HybridExhaustControl::HybridExhaustControl()
    : mpPhysicsControl(nullptr)
    , mpEngineControl(nullptr)
    , mpShiftControl(nullptr)
    , mpClutchControl(nullptr)
    , mVehicleEngineAttributes(nullptr, nullptr)                 // vehicleengine(this+0x48,0,0)
    , mMasterVehicleEngineComponentAttributes(nullptr, nullptr)  // vehicleengine(this+0x58,0,0)
    // mAverageDeltaRPM: Average<3,f32> default ctor zeroes maPoints/muNextPoint/mfAverage
    // mPhysicsDeltaRpm / mAudioDeltaRpm / mGinsuRpm: DataPoint<f32> default -> {0,0}
    , mDecelCrossfadeMix(maCrossFadesPoints, 6)
    , mfPercentOfAccelThreshold(0.0f)   // X360 leaves uninitialised; safe default
    , mfPercentOfDecelThreshold(0.0f)   // X360 leaves uninitialised; safe default
    // mFinalEngineMix / mFinalEngineVolume: EngineMix default ctor -> all 0.0f
{
    for (u32 luPoint = 0; luPoint < 6; ++luPoint)
        maCrossFadesPoints[luPoint] = Vector2{0.0f, 0.0f, 0.0f, 0.0f};
}

// ---------------------------------------------------------------------------
// ~HybridExhaustControl  @ 0x826AFA60  (anchor for the X360 `vector deleting destructor').
// The observable member teardown is the inherited ~BrnEffectControl dual-base chain
// plus the two vehicleengine sub-object dtors (compiler-synthesised from the declared
// members), so this leaf body is empty. The (a2 & 1) allocator-free tail is
// re-synthesised by the host toolchain (off_82FFB954 not homed here).
// ---------------------------------------------------------------------------
HybridExhaustControl::~HybridExhaustControl()
{
}

s32 HybridExhaustControl::GetController(s32 aiSlot)
{
    static const s32 kaiControllers[] = { 0, 4, 2, 3, 12 };
    if (aiSlot < 0 || aiSlot >= static_cast<s32>(sizeof(kaiControllers) / sizeof(kaiControllers[0])))
        return -1;
    if (aiSlot == 4 && GetStateId() != 1)
        return -1;
    return kaiControllers[aiSlot];
}

void HybridExhaustControl::AttachController(CgsSound::Logic::EffectBase* apController)
{
    switch (apController->GetEffectID())
    {
    case 0: mpPhysicsControl = static_cast<PhysicsControl*>(apController); break;
    case 2: mpShiftControl = static_cast<ShiftControl*>(apController); break;
    case 3: mpClutchControl = static_cast<ClutchControl*>(apController); break;
    case 4: mpEngineControl = static_cast<EngineControl*>(apController); break;
    case 1:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
        break;
    default:
        CGS_ASSERT(false, "I don't know how to handle attaching this type of EffectBase.");
        break;
    }
}

bool HybridExhaustControl::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;

    CGS_ASSERT(mpPhysicsControl != nullptr, "mpPhysicsControl");
    if (!mpPhysicsControl)
        return false;

    const BrnSound::Vehicles::VehicleState::EEngineComponentType leComponent =
        (GetId() & 0x7F0) == 0x60
            ? BrnSound::Vehicles::VehicleState::E_ENGINE
            : BrnSound::Vehicles::VehicleState::E_EXHAUST;
    const u64 luCollectionKey = mpPhysicsControl->GetEngineComponentKey(leComponent);
    Attrib::Collection* lpCollection = Attrib::FindCollectionWithDefault(
        0x7F161D94482CB3BFull, luCollectionKey);
    mVehicleEngineAttributes.Change(lpCollection);
    mMasterVehicleEngineComponentAttributes.Change(lpCollection);

    CGS_ASSERT(mVehicleEngineAttributes.LoopModel() != nullptr,
               "mVehicleEngineAttributes.LoopModel()");
    CGS_ASSERT(mVehicleEngineAttributes.GinsuFileAccel() != nullptr,
               "mVehicleEngineAttributes.GinsuFileAccel()");
    CGS_ASSERT(mVehicleEngineAttributes.GinsuFileDecel() != nullptr,
               "mVehicleEngineAttributes.GinsuFileDecel()");

    // ARTIST @ 0x826998D4..0x826999E0. The decel Ginsu voice fades in over the
    // upper five percent of the authored decel RPM band, stays present down to
    // idle+200, and is silent outside that range.
    const f32 lfIdleRpm = mVehicleEngineAttributes.IdleRpm();
    const f32 lfDecelMinRpm = mVehicleEngineAttributes.DecelMinRpm();
    const f32 lfDecelMaxRpm = mVehicleEngineAttributes.DecelMaxRpm();
    const f32 lfFadeInRpm = lfDecelMaxRpm -
        (lfDecelMaxRpm - lfDecelMinRpm) * KF_DECEL_GINSU_FADE_IN;
    maCrossFadesPoints[0] = Vector2{0.0f, 0.0f, 0.0f, 0.0f};
    maCrossFadesPoints[1] = Vector2{lfIdleRpm, 0.0f, 0.0f, 0.0f};
    maCrossFadesPoints[2] = Vector2{lfIdleRpm + 200.0f, 1.0f, 0.0f, 0.0f};
    maCrossFadesPoints[3] = Vector2{lfFadeInRpm, 1.0f, 0.0f, 0.0f};
    maCrossFadesPoints[4] = Vector2{lfDecelMaxRpm, 0.0f, 0.0f, 0.0f};
    maCrossFadesPoints[5] = Vector2{mVehicleEngineAttributes.MaxRpm(),
                                    0.0f, 0.0f, 0.0f};

    mPhysicsDeltaRpm.Flush(0.0f);
    mAudioDeltaRpm.Flush(0.0f);
    mGinsuRpm.Flush(mpPhysicsControl->GetPhysicsData().mNormalizedRpm.GetCurrent());
    return true;
}

void HybridExhaustControl::UpdateParams(f32 afTimeStep)
{
    if (!mpPhysicsControl)
        return;

    CGS_ASSERT(mVehicleEngineAttributes.LoopModel() != nullptr,
               "mVehicleEngineAttributes.LoopModel()");
    CGS_ASSERT(mVehicleEngineAttributes.GinsuFileAccel() != nullptr,
               "mVehicleEngineAttributes.GinsuFileAccel()");
    CGS_ASSERT(mVehicleEngineAttributes.GinsuFileDecel() != nullptr,
               "mVehicleEngineAttributes.GinsuFileDecel()");

    // ARTIST @ 0x826E3DE0: delta tracking, the virtual Ginsu-RPM mapping, then
    // the data-driven source mix.  The previous interim body skipped the middle
    // controller and fed normalized 1k..10k RPM directly to the Ginsu voices.
    UpdateDeltaRPM();
    UpdateGinsuRPM();
    UpdateMix(afTimeStep);
}

void HybridExhaustControl::UpdateDeltaRPM()
{
    CGS_ASSERT(mpEngineControl != nullptr, "mpEngineControl");
    CGS_ASSERT(mpShiftControl != nullptr, "mpShiftControl");
    CGS_ASSERT(mpClutchControl != nullptr, "mpClutchControl");
    if (!mpEngineControl || !mpShiftControl || !mpClutchControl)
        return;

    const PhysicsControl::PhysicsData& lrPhysics = mpPhysicsControl->GetPhysicsData();
    mPhysicsDeltaRpm.Update(lrPhysics.mNormalizedRpm.GetCurrent() -
                            lrPhysics.mNormalizedRpm.GetPrevious());

    const f32 lfAudioDelta = mpEngineControl->GetAudioRPM().GetCurrent() -
                             mpEngineControl->GetAudioRPM().GetPrevious();
    mAudioDeltaRpm.Update(lfAudioDelta);

    const ClutchControl::EClutchState leClutchState =
        mpClutchControl->GetClutchState();
    if (leClutchState == ClutchControl::E_CLUTCH_STATE_ATTACK_UPDATE)
    {
        mAverageDeltaRPM.Flush(mPhysicsDeltaRpm.GetCurrent());
    }
    else if (leClutchState == ClutchControl::E_CLUTCH_STATE_IDLE_REVING)
    {
        mAverageDeltaRPM.Record(mAudioDeltaRpm.GetCurrent());
    }
    else if (mpShiftControl->IsActive())
    {
        const ShiftControl::EShiftStage leShiftState =
            mpShiftControl->GetShiftingState();
        if (leShiftState == ShiftControl::E_SHFT_DOWN_ENGAGING_RISE)
        {
            mAverageDeltaRPM.Flush(mAudioDeltaRpm.GetCurrent());
        }
        else if (mpShiftControl->GetShiftingStateChange() ==
                 ShiftControl::E_SHFT_UP_ENGAGING)
        {
            mAverageDeltaRPM.Flush(
                std::fabs(mPhysicsDeltaRpm.GetCurrent() * 0.7f));
        }
        else if (leShiftState == ShiftControl::E_SHFT_UP_ENGAGING ||
                 leShiftState == ShiftControl::E_SHFT_UP_LFO)
        {
            mAverageDeltaRPM.Record(std::fabs(mAudioDeltaRpm.GetCurrent()));
        }
        else
        {
            mAverageDeltaRPM.Record(mAudioDeltaRpm.GetCurrent());
        }
    }
    else if (mpEngineControl->GetClutchState())
    {
        mAverageDeltaRPM.Record(mAudioDeltaRpm.GetCurrent());
    }
    else
    {
        mAverageDeltaRPM.Record(mPhysicsDeltaRpm.GetCurrent());
    }
}

void HybridExhaustControl::UpdateGinsuRPM()
{
    const f32 lfAudioRpm = mpEngineControl
        ? mpEngineControl->GetAudioRPM().GetCurrent()
        : mpPhysicsControl->GetPhysicsData().mNormalizedRpm.GetCurrent();
    const f32 lfUnity = ((std::max)(1000.0f, (std::min)(10000.0f, lfAudioRpm)) -
                         1000.0f) * (1.0f / 9000.0f);
    const f32 lfIdleRpm = mVehicleEngineAttributes.IdleRpm();
    const f32 lfMaxRpm = mVehicleEngineAttributes.MaxRpm();
    mGinsuRpm.Update(lfIdleRpm + (lfMaxRpm - lfIdleRpm) * lfUnity);
}

void HybridExhaustControl::UpdateIdleVolume(f32& arfVolume)
{
    if (mpPhysicsControl->GetPhysicsData().mGear.GetCurrent() != 1)
        return;

    const f32 lfRpm = mpEngineControl->GetAudioRPM().GetCurrent();
    const f32 lfIdleBlend = Clamp01((lfRpm - 1000.0f) / 1000.0f);
    arfVolume *= Lerp(mVehicleEngineAttributes.IdleGain(), 1.0f, lfIdleBlend);
}

void HybridExhaustControl::UpdateRpmVolume(f32& arfVolume)
{
    const f32 lfRpm = mpEngineControl->GetAudioRPM().GetCurrent();
    const f32 lfUnityRpm = (lfRpm - 1000.0f) * (1.0f / 9000.0f);
    const f32 lfUnityRpm2 = lfUnityRpm * lfUnityRpm;
    const Matrix44 lVolumeCurve = mVehicleEngineAttributes.VolumeOverRPM();
    const f32 lfRpmVolume =
        lVolumeCurve.xAxis.y * (lfUnityRpm2 * lfUnityRpm) +
        lVolumeCurve.yAxis.y * lfUnityRpm2 +
        lVolumeCurve.zAxis.y * lfUnityRpm +
        lVolumeCurve.wAxis.y;
    arfVolume *= lfRpmVolume;
}

void HybridExhaustControl::UpdateRotationVolume(f32& arfVolume)
{
    if ((GetId() & 0xFF0000) != 0x10000)
        return;

    const f32 lfDrifting = mpPhysicsControl->GetPhysicsData().mDrifting.GetCurrent();
    f32 lfRearMix = mMasterVehicleEngineComponentAttributes.RotationMixRear();
    f32 lfFrontMix = mMasterVehicleEngineComponentAttributes.RotationMixFront();
    if (GetEffectID() == 6)
    {
        lfRearMix = 1.0f - lfRearMix;
        lfFrontMix = 1.0f - lfFrontMix;
    }
    const f32 lfMix = Lerp(lfRearMix, lfFrontMix, lfDrifting);
    const f32 lfVolume = Lerp(
        mMasterVehicleEngineComponentAttributes.RotationVolRear(),
        mMasterVehicleEngineComponentAttributes.RotationVolFront(),
        lfDrifting);
    arfVolume *= lfMix * lfVolume;
}

void HybridExhaustControl::UpdateMix(f32 /*afTimeStep*/)
{
    CGS_ASSERT(mpEngineControl != nullptr, "mpEngineControl");
    CGS_ASSERT(mpShiftControl != nullptr, "mpShiftControl");
    CGS_ASSERT(mpClutchControl != nullptr, "mpClutchControl");
    if (!mpEngineControl || !mpShiftControl || !mpClutchControl)
        return;

    const f32 lfThrottleMix = Clamp01(
        mpEngineControl->GetAudioThrottle().GetCurrent() / KF_THROTTLE_THRESHOLD);
    const f32 lfDeltaOffset = GetStateId() == 1
        ? KF_DELTA_RPM_OFFSET : KF_AI_DELTA_RPM_OFFSET;
    const f32 lfDeltaRpm = std::fabs(
        lfDeltaOffset + mAverageDeltaRPM.GetAverage());

    const f32 lfAccelThreshold =
        mVehicleEngineAttributes.AccelDeltaRpmThreshold();
    const f32 lfDecelThreshold =
        mVehicleEngineAttributes.DecelDeltaRpmThreshold();
    mfPercentOfAccelThreshold = lfAccelThreshold != 0.0f
        ? Clamp01(lfDeltaRpm / lfAccelThreshold) : 0.0f;
    mfPercentOfDecelThreshold = lfDecelThreshold != 0.0f
        ? Clamp01(lfDeltaRpm / lfDecelThreshold) : 0.0f;

    EngineMix lAccelMix;
    lAccelMix.Loop = Lerp(
        mVehicleEngineAttributes.LoopModelAccelSmallRpmGain(),
        mVehicleEngineAttributes.LoopModelAccelLargeRpmGain(),
        mfPercentOfAccelThreshold);
    lAccelMix.AccelGinsu = Lerp(
        mVehicleEngineAttributes.GinsuAccelSmallRpmGain(),
        mVehicleEngineAttributes.GinsuAccelLargeRpmGain(),
        mfPercentOfAccelThreshold);
    lAccelMix.DecelGinsu = 0.0f;
    lAccelMix.Cutoff = KF_GINSU_CUTOFF;

    EngineMix lDecelMix;
    lDecelMix.Loop = Lerp(
        mVehicleEngineAttributes.LoopModelDecelSmallRpmGain(),
        mVehicleEngineAttributes.LoopModelDecelLargeRpmGain(),
        mfPercentOfDecelThreshold);
    lDecelMix.AccelGinsu = Lerp(
        mVehicleEngineAttributes.GinsuAccelNegSmallRpmGain(),
        mVehicleEngineAttributes.GinsuAccelNegLargeRpmGain(),
        mfPercentOfDecelThreshold);
    lDecelMix.DecelGinsu = Lerp(
        mVehicleEngineAttributes.GinsuDecelSmallRpmGain(),
        mVehicleEngineAttributes.GinsuDecelLargeRpmGain(),
        mfPercentOfDecelThreshold);
    lDecelMix.Cutoff = KF_GINSU_CUTOFF;

    const f32 lfDecelCrossfade = mDecelCrossfadeMix.GetYValue(
        mGinsuRpm.GetCurrent());
    lDecelMix.Loop = lfDecelCrossfade * lDecelMix.Loop +
                     (1.0f - lfDecelCrossfade);
    lDecelMix.DecelGinsu *= lfDecelCrossfade;

    EngineMix lNewMix;
    lNewMix.Loop = Lerp(lDecelMix.Loop, lAccelMix.Loop, lfThrottleMix);
    lNewMix.AccelGinsu = Lerp(
        lDecelMix.AccelGinsu, lAccelMix.AccelGinsu, lfThrottleMix);
    lNewMix.DecelGinsu = Lerp(
        lDecelMix.DecelGinsu, lAccelMix.DecelGinsu, lfThrottleMix);
    lNewMix.Cutoff = Lerp(lDecelMix.Cutoff, lAccelMix.Cutoff, lfThrottleMix);

    const bool lbUseSmoothing =
        mpClutchControl->GetClutchState() ==
            ClutchControl::E_CLUTCH_STATE_ATTACK_UPDATE &&
        mpShiftControl->GetShiftingState() != ShiftControl::E_SHFT_UP_ENGAGING;
    if (lbUseSmoothing)
    {
        mFinalEngineMix.Loop = SmoothTowards(
            mFinalEngineMix.Loop, lNewMix.Loop, KF_SMOOTHNESS_FACTOR);
        mFinalEngineMix.AccelGinsu = SmoothTowards(
            mFinalEngineMix.AccelGinsu, lNewMix.AccelGinsu,
            KF_SMOOTHNESS_FACTOR);
        mFinalEngineMix.DecelGinsu = SmoothTowards(
            mFinalEngineMix.DecelGinsu, lNewMix.DecelGinsu,
            KF_SMOOTHNESS_FACTOR);
        mFinalEngineMix.Cutoff = SmoothTowards(
            mFinalEngineMix.Cutoff, lNewMix.Cutoff,
            KF_LOW_PASS_SMOOTHING_FACTOR);
    }
    else
    {
        mFinalEngineMix = lNewMix;
    }

    const f32 lfAudioVolume = mpEngineControl->GetAudioEngVolume().GetCurrent();
    const f32 lfLoopGain = Lerp(
        mVehicleEngineAttributes.LoopModelDecelGain(),
        mVehicleEngineAttributes.LoopModelAccelGain(),
        lfThrottleMix);

    EngineMix lNewVolume;
    lNewVolume.Loop = mFinalEngineMix.Loop * lfLoopGain * lfAudioVolume;
    lNewVolume.AccelGinsu = mFinalEngineMix.AccelGinsu *
        mVehicleEngineAttributes.GinsuAccelGain() * lfAudioVolume;
    lNewVolume.DecelGinsu = mFinalEngineMix.DecelGinsu *
        mVehicleEngineAttributes.GinsuDecelGain() * lfAudioVolume;
    lNewVolume.Cutoff = KF_GINSU_CUTOFF;

    f32 lfMasterGain = mVehicleEngineAttributes.MasterGain() *
        mMasterVehicleEngineComponentAttributes.MasterCarVolume();
    UpdateIdleVolume(lfMasterGain);
    UpdateRpmVolume(lfMasterGain);
    UpdateRotationVolume(lfMasterGain);
    lNewVolume.Loop *= lfMasterGain;
    lNewVolume.AccelGinsu *= lfMasterGain;
    lNewVolume.DecelGinsu *= lfMasterGain;

    if (lbUseSmoothing)
    {
        mFinalEngineVolume.Loop = SmoothTowards(
            mFinalEngineVolume.Loop, lNewVolume.Loop, KF_SMOOTHNESS_FACTOR);
        mFinalEngineVolume.AccelGinsu = SmoothTowards(
            mFinalEngineVolume.AccelGinsu, lNewVolume.AccelGinsu,
            KF_SMOOTHNESS_FACTOR);
        mFinalEngineVolume.DecelGinsu = SmoothTowards(
            mFinalEngineVolume.DecelGinsu, lNewVolume.DecelGinsu,
            KF_SMOOTHNESS_FACTOR);
        mFinalEngineVolume.Cutoff = SmoothTowards(
            mFinalEngineVolume.Cutoff, lNewVolume.Cutoff,
            KF_LOW_PASS_SMOOTHING_FACTOR);
    }
    else
    {
        mFinalEngineVolume = lNewVolume;
    }

    const f32 lfAccelDecelMix = Lerp(
        mfPercentOfDecelThreshold, mfPercentOfAccelThreshold, lfThrottleMix);
    SetMixerInputValue(0, static_cast<s32>(lfAccelDecelMix * 32767.0f));
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
