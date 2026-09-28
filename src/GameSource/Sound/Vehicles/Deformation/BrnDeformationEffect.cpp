// ============================================================================
// b5-decomp/src/GameSource/Sound/Vehicles/Deformation/BrnDeformationEffect.cpp
//
// BrnSound::Vehicles::Deformation::DeformationEffect -- the car-body crumple/deformation
// sound effect object. Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX.
//
// Lifecycle and live deformation controls are reconstructed below. UpdateParams
// restores the ARTIST 826B53B0 sensor/envelope path. SoundLogicModule replay
// serialiser plumbing remains an explicit separate dependency.
// ============================================================================

#include "GameSource/Sound/Vehicles/Deformation/BrnDeformationEffect.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h" // complete PhysicsControl for the AttachController downcast (BY NAME)
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationState.h"
#include "GameShared/GameClasses/Sound/Playback/CgsSoundPcmTrace.h"
#include <cmath>
#include <limits>

namespace BrnSound
{
namespace Vehicles
{
namespace Deformation
{

// ---------------------------------------------------------------------------
// DeformationEffect::DeformationEffect()  @ 0x826CF6C0
//
// MSVC's INLINED full-object constructor. It does NOT `bl` a base ctor -- it inlines
// the BrnEffectObject dual-base member zero-inits + installs both leaf vptrs (this+0
// off_820B3E44 + this+4 IResourceRequester off_820B3E10), then default-constructs
// every leaf member. In reconstructed C++ the dual-base settle is produced structurally
// by the BrnEffectObject base default ctor (BY NAME); the leaf members' zero-inits are
// their own default ctors (DataPoint / Average / InterpolateLine all zero-init;
// InterpolateLine seeds mbComplete=true == the X360 stb 1 @+0x80), and the tail
// `bl VoiceWrapper::VoiceWrapper(this+0x8C)` is mPatchVoice's default ctor.
// The X360 leaves mfAemsIntensity / mfTimeDeforming / mpPhysicsControl UNINITIALIZED
// (populated on Attach / AttachController) -- same convention as the sibling
// ReverbEffect ctor -- so they are not seeded here.
// ---------------------------------------------------------------------------
DeformationEffect::DeformationEffect()
    : BrnSound::Logic::BrnEffectObject()  // dual-base vptr settle + base zero-init (BY NAME)
    // mbDeforming/mDeformAmount/mDeformDeltaAverage/mDeformIntensityLagged/mFadeOut/
    // mPatchVoice: default-constructed (each embedded default ctor zeroes; InterpolateLine
    // mbComplete=true -> stb 1 @+0x80; VoiceWrapper ctor is the tail bl @ +0x8C).
    // mfAemsIntensity(+0x64)/mfTimeDeforming(+0x84)/mpPhysicsControl(+0x88): intentionally
    // UNINITIALIZED -- the X360 ctor writes nothing there (seeded on Attach/AttachController).
{
}

DeformationEffect::~DeformationEffect()
{
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>*
DeformationEffect::GetTypeInfo() const
{
    // The original returns DeformationEffect::sTypeInfo.  The native build keeps
    // the same descriptor in EffectObject's registration slot 15.
    return CgsSound::Logic::EffectObject::GetRegisteredTypeInfo(15);
}

// ---------------------------------------------------------------------------
// DeformationEffect::GetTypeName() const  @ 0x82685730  -> "DeformationEffect"
//   The X360 leaf loads the interned type-name string (off_82F2F684, the tag
//   CreateObject's operator new / RTTI uses) and returns it. Mirrors the
//   committed PlayerVehicleStateManager / TrafficStateManager GetTypeName leaves.
// ---------------------------------------------------------------------------
const char* DeformationEffect::GetTypeName() const
{
    return "DeformationEffect";
}

// ---------------------------------------------------------------------------
// DeformationEffect::SetupLoadData()  @ 0x826E4C88
//   Tail-forwards to the IResourceRequester base's LoadAsset: request the crumple
//   patch bank bundle. The X360 `addi r3,r3,-4` is the multiple-inheritance base
//   adjustment recovering the IResourceRequester sub-object from the BrnEffectObject
//   `this`; reproduced here as a static_cast to the IResourceRequester base plus a
//   plain member call. r5=0 -> the 2nd (const char*) arg is null; r6=0 -> EType == E_DATA.
// ---------------------------------------------------------------------------
void DeformationEffect::SetupLoadData()
{
    static_cast<BrnSound::Logic::IResourceRequester*>(this)->LoadAsset(
        "sound\\aems\\CRUMPLEPATCHBANK.BUNDLE", nullptr,
        BrnSound::Logic::ResourceRegistrar::E_DATA);
}

// The controller-class band the AttachController gate tests. The X360 masks the
// supplied controller's object-id (`*(controller+0x14) & 0x7F0`) -- the class-tag band
// shared with the sibling SingleGinsuEffect::AttachController. DeformationEffect accepts
// ONLY a controller whose class band is 0 (the PhysicsControl class); any set bit in the
// band is a mis-attached controller and trips the assert.
static const s32 KI_CONTROLLER_CLASS_MASK = 0x7F0;

s32 DeformationEffect::GetController(s32 aiSlot)
{
    // DecFIGS @ 0x823E44: slot zero requests PhysicsControl; every other slot
    // terminates the controller walk.
    return aiSlot == 0 ? 0 : -1;
}

// ---------------------------------------------------------------------------
// DeformationEffect::AttachController  @ 0x82685740   (override of EffectBase::AttachController)
//
//   r11 = *(controller+0x14);            ; controller->miObjectId  (GetObjectId)
//   r11 &= 0x7F0;                         ; class-tag band
//   if (r11 != 0)                         ; wrong controller class?
//       << assert "Cound't attach controller " >>   ; (Begin/Fire/EndAssert chain)
//   else
//       this->mpPhysicsControl = controller - 4;     ; latch the physics controller
//
// The controller is handed in via its CgsSound::Logic::EffectBase sub-object pointer; the
// X360 `controller - 4` recovers the primary object, modelled here as a by-name downcast
// to PhysicsControl (its EffectBase primary base performs the equivalent adjustment). The
// de-inlined BeginAssert/Clear/FireAssert/EndAssert chain (which streams the failing
// object-id after the message) collapses to one CGS_ASSERT with the verbatim rodata
// string; the file-path/line args are dropped per convention.
// ---------------------------------------------------------------------------
void DeformationEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    if ((apController->GetId() & KI_CONTROLLER_CLASS_MASK) != 0)
    {
        CGS_ASSERT(false, "Cound't attach controller ");
    }
    else
    {
        mpPhysicsControl = static_cast<BrnSound::Vehicles::Engines::PhysicsControl*>(apController);
    }
}

bool DeformationEffect::Attach()
{
    // ARTIST @ 0x826F37E8.  The base Attach sequence is inlined in the original.
    CgsSound::Logic::EffectBase::Attach();

    mFadeOut.mfElapsedTime = 0.0f;
    mFadeOut.mfLength = 0.01f;
    mFadeOut.mfStart = 1.0f;
    mFadeOut.mfFinish = 1.0f;
    mFadeOut.meCurveTypes = CgsSound::Utils::Curve::E_LINEAR;
    mFadeOut.mfCurrentValue = 1.0f;
    mFadeOut.mbComplete = false;
    mfAemsIntensity = 0.0f;

    const CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE leStage =
        mPatchVoice.GetUpdateStage();
    if (leStage == CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_IDLE ||
        leStage == CgsSound::Logic::VoiceWrapper::E_UPDATE_STAGE_FINISHED)
    {
        CgsSound::Logic::VoiceWrapper::CreateParams lParams;
        lParams.mpLogicModule = GetLogicModule();
        lParams.mFactoryName = static_cast<u32>(
            CgsSound::Playback::AemsFactorySkName().GetValue());
        lParams.mVoiceSpecName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("AEMS_crumple"));
        lParams.mContentSpecName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("CrumplePatchBank.abi"));
        lParams.mSlotName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("AEMS_Slot"));
        lParams.mSendName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("Send01"));
        lParams.mSubMixVoiceID = 1;
        lParams.mReverbSendName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("ReverbSend"));
        lParams.mReverbSubMixVoiceID = 2;
        lParams.miSendIndex = 0;
        mPatchVoice.Create(lParams);
        mPatchVoice.Play(0);
    }

    mfTimeDeforming = 0.0f;
    mDeformAmount.Flush(0.0f);
    mDeformDeltaAverage.Flush(0.0f);
    mDeformIntensityLagged.Flush(0.0f);
    mbDeforming.Flush(false);
    return true;
}

// ARTIST 826B55F8. FLAG (model): vmsum = one rounding of the f64 sum
// (xenia DOT_PRODUCT_3), with VMX denormal flush and finite-overflow QNaN.
static f32 DeformationDisplacementSquared(const Vector3& lDisplacement)
{
    const auto flush = [](f32 v) {
        return std::fpclassify(v) == FP_SUBNORMAL ? std::copysign(0.0f, v) : v;
    };
    const double x = flush(lDisplacement.x);
    const double y = flush(lDisplacement.y);
    const double z = flush(lDisplacement.z);
    const double sum = (x * x + y * y) + z * z;
    const f32 result = static_cast<f32>(sum);
    if (std::isfinite(sum) && std::isinf(result))
        return std::numeric_limits<f32>::quiet_NaN();
    return flush(result);
}

void DeformationEffect::UpdateParams(f32 afTimeStep)
{
    auto* lpModule = static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    const auto* lpInput = lpModule->GetBrnInputStructure();
    const auto& lFrame = lpModule->GetFrameInformation();
    const f32 lfVolume = GetMixerOutputValue(0, 0);
    const f32 lfPitch = GetMixerOutputValue(2, 1);
    const f32 lfAzimuth = GetMixerOutputValue(1, 3);

    // ARTIST 826B5508..5854, live simulation branch. SoundLogicModule's replay
    // serialiser binding (826B54D0..5504 / 5858..587C) remains unhomed.
    mbDeforming.Update(lFrame.meFatality.GetCurrent() != BrnSound::Logic::E_FATAL_OFF &&
        lFrame.meImpactTime.GetCurrent() == AttribSys::Enums::eImpactTime::VSlow);
    if (mbDeforming.GetCurrent())
    {
        const auto* lpDeformation = lpInput->GetDeformationInterface().mpDeformationState;
        if (lpDeformation != nullptr)
        {
            const auto* lpCar = lpDeformation->GetCarStateF(
                mpPhysicsControl->GetRawPhysicsData()->mEntityId.muValue);
            if (lpCar != nullptr)
            {
                const bool lbRising = !mbDeforming.GetPrevious();
                if (lbRising)
                {
                    mFadeOut.Initialize(1.0f, 1.0f, 10.0f, CgsSound::Utils::Curve::E_LINEAR);
                    mDeformIntensityLagged.Update(0.0f);
                }
                f32 lfMaximum = 0.0f;
                for (u32 i = 0; i < lpCar->mu8NumSensors; ++i)
                {
                    // GetSensor's attested accessor is non-const but only reads.
                    const auto& lSensor = const_cast<BrnPhysics::Deformation::CarState*>(lpCar)
                        ->GetSensor(static_cast<u8>(i));
                    const f32 lfSquared = DeformationDisplacementSquared(lSensor.mDisplacement);
                    lfMaximum = (lfMaximum - lfSquared >= 0.0f) ? lfMaximum : lfSquared;
                }
                mDeformAmount.Update(lfMaximum);
                mDeformDeltaAverage.Record(lbRising ? 0.0f :
                    lfMaximum - mDeformAmount.GetPrevious());
                const f32 lfAverage = mDeformDeltaAverage.GetAverage();
                // 826B5680 ble -> 56E0, including unordered; peak arm 5684.
                if (lfAverage > mFadeOut.GetValueFloat() * mDeformIntensityLagged.GetCurrent())
                {
                    mDeformIntensityLagged.Update(lfAverage);
                    mFadeOut.Initialize(1.0f, 0.0f, 1000.0f, CgsSound::Utils::Curve::E_LINEAR);
                }
                else
                    mFadeOut.Update(afTimeStep);
                mfTimeDeforming += afTimeStep;
            }
        }
    }
    else
    {
        if (mbDeforming.GetPrevious())
            mFadeOut.Initialize(mFadeOut.GetValueFloat(), 0.0f, 1000.0f,
                CgsSound::Utils::Curve::E_LINEAR);
        mFadeOut.Update(afTimeStep);
        mfTimeDeforming = 0.0f;
        mDeformAmount.Update(0.0f);
        mDeformDeltaAverage.Flush(0.0f);
    }

    // Constants: 82F2FD0C=655340, 82F2CD58=1000ms, 82F2CD5C=700.
    // 826B57C4/E0 are separate fmuls; E8/F4 are fsel (NaN selects 32767).
    const f32 lfFaded = mFadeOut.GetValueFloat() * mDeformIntensityLagged.GetCurrent();
    f32 lfTarget = lfFaded * 655340.0f;
    lfTarget = (-lfTarget >= 0.0f) ? 0.0f : lfTarget;
    lfTarget = (32767.0f - lfTarget >= 0.0f) ? lfTarget : 32767.0f;
    // 57F8/5800 bge -> 5854, including unordered; smoothing arm 5804.
    if (!(mfAemsIntensity < 700.0f) || !(lfTarget < 700.0f))
        mfAemsIntensity = lfTarget;
    else if (lfTarget - mfAemsIntensity > 32000.0f)
        mfAemsIntensity += 32000.0f;
    else if (mfAemsIntensity - lfTarget > afTimeStep * 500.0f)
        mfAemsIntensity -= afTimeStep * 500.0f;
    else
        mfAemsIntensity = lfTarget;

    static const u32 luIntensity = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("AEMS_intensity"));
    static const u32 luVolume = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("AEMS_volume"));
    static const u32 luPitch = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("AEMS_pitch"));
    static const u32 luAzimuth = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("AEMS_azimuth"));
    static const u32 luSend01 = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("Send01"));
    static const u32 luReverbSend = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("ReverbSend"));

    // FLAG PC-platform witness: observe live controls without changing them.
    if (CgsSound::PcmTrace::File())
    {
        static s32 liLastFatal = -1, liLastImpact = -1;
        static u32 luRows = 0;
        const s32 liFatal = static_cast<s32>(lFrame.meFatality.GetCurrent());
        const s32 liImpact = static_cast<s32>(lFrame.meImpactTime.GetCurrent());
        if (luRows < 96 && (liFatal != liLastFatal || liImpact != liLastImpact || lfTarget > 0.0f))
        {
            ++luRows;
            CgsSound::PcmTrace::Log("crumple-control fatal=%d impact=%d active=%d amount=%.9g delta=%.9g lag=%.9g fade=%.9g target=%.9g volume=%.9g dt=%.9g\n",
                liFatal, liImpact, mbDeforming.GetCurrent() ? 1 : 0,
                mDeformAmount.GetCurrent(), mDeformDeltaAverage.GetAverage(),
                mDeformIntensityLagged.GetCurrent(), mFadeOut.GetValueFloat(),
                lfTarget, lfVolume, afTimeStep);
        }
        liLastFatal = liFatal;
        liLastImpact = liImpact;
    }

    // 826B5BAC passes f31 (target), not the smoothed cache at this+0x60.
    mPatchVoice.SetParameter(2, lfTarget, &luIntensity);
    mPatchVoice.SetParameter(0, lfVolume, &luVolume);
    mPatchVoice.SetParameter(3, lfAzimuth, &luAzimuth);
    mPatchVoice.SetParameter(1, lfPitch, &luPitch);
    mPatchVoice.SetGain(0, 1.0f, &luSend01);
    mPatchVoice.SetGain(1, 0.0f, &luReverbSend);
}


void DeformationEffect::ProcessUpdate()
{
    // DecFIGS @ 0x8A206C is this exact tail-forwarder.
    mPatchVoice.Update();
}

// ---------------------------------------------------------------------------
// DeformationEffect::Detach  @ 0x826F39C8   (override of EffectBase::Detach)
//
//   if (!BrnEffectObject::Detach()) return false;   ; base teardown must succeed
//   VoiceWrapper::Release(this+0x88);                 ; release mPatchVoice
//   return true;
//
// Forwards to the committed BrnEffectObject::Detach (resource-request teardown +
// attach-state finalise). If that succeeds, releases the crumple patch voice.
// ---------------------------------------------------------------------------
bool DeformationEffect::Detach()
{
    if (!BrnSound::Logic::BrnEffectObject::Detach())
    {
        return false;
    }
    mPatchVoice.Release();
    return true;
}

} // namespace Deformation
} // namespace Vehicles
} // namespace BrnSound
