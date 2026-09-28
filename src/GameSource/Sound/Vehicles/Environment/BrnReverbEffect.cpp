#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameSource/AttribSys/Generated/classes/reverbparams.h"
#include "GameSource/Sound/Vehicles/Environment/BrnReverbEffect.h"
#include "GameSource/Sound/Vehicles/Environment/BrnEnclosureControl.h"
#include "GameSource/Sound/Vehicles/Environment/BrnEnvironmentSoundDiag.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/AttribSys/Enums/eImpactTime.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SharedClasses/Trigger/BrnGenericRegion.h"

// =============================================================================
// BrnSound::Vehicles::Environment::ReverbEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Recon'd function set:
//   CreateObject(u32)              @ 0x826D1438  (the factory hook)
//   ReverbEffect()                 @ 0x826BA458  (MSVC inlined full-object ctor)
//   `vector deleting destructor'   @ 0x826BA4E8  (-> ~ReverbEffect anchor)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

// ---------------------------------------------------------------------------
// ReverbEffect::CreateObject(u32)  @ 0x826D1438   (the factory hook)
// Allocates a 120-byte (0x78) block via CgsSound::MemBase::operator new(size, tag,
// flavour) tagged "ReverbEffect" and constructs a ReverbEffect, upcast to the primary
// EffectObject* (+4). `a1` only selects the operator-new flavour (0/1).
// FLAG (allocator gate): CgsSound::MemBase::operator new is not homed here, so this uses
// the host `new`; observable result matches. The 0x78 size is documentation only.
// ---------------------------------------------------------------------------
CgsSound::Logic::EffectObject* ReverbEffect::CreateObject( u32 /*luType*/ )
{
    return new ReverbEffect();
}

// ---------------------------------------------------------------------------
// ReverbEffect::ReverbEffect  @ 0x826BA458   (the leaf constructor)
//
// MSVC's INLINED full-object constructor: it does NOT `bl` a base ctor -- it inlines the
// BrnEffectObject dual-base zero-init + installs the two leaf vptrs directly, then
// inlines the default construction of the two embedded sub-objects.
//
// NOTE (refutes the naive read): the four scalar leaf floats mfTime/mfSpaceSize/
// mfBrightness/mfGain, meReverbState, mpEnclosureControl and mpPhysicsControl are NOT
// written by this ctor -- they are left uninitialized by the X360 code. Do NOT add
// initializers for them; doing so would fabricate stores the binary does not emit. The
// only leaf effects are the two embedded-member default constructions (InterpolateLine
// @+0x48 with mbComplete=true -> stb 1, and DataPoint @+0x68 zeroed).
// ---------------------------------------------------------------------------
ReverbEffect::ReverbEffect()
    : BrnSound::Logic::BrnEffectObject()  // installs both vptrs + zero-inits the base region (BY NAME)
    // mInterpolateReverb: default-constructed -> stores @+0x48..+0x60 (mbComplete=true -> stb 1)
    // mReverbType:        default-constructed -> zeros @+0x68/+0x6C
    // mfTime/mfSpaceSize/mfBrightness/mfGain/meReverbState/mpEnclosureControl/
    // mpPhysicsControl: intentionally UNINITIALIZED (the X360 ctor writes nothing here).
{
}

// ---------------------------------------------------------------------------
// ReverbEffect::Attach
//
// Starts from the neutral preset (no time, space 15, full brightness and gain), a
// settled level-1 interpolator, no transition, and a "no preset yet" type so the first
// update always selects one.
// ---------------------------------------------------------------------------
bool ReverbEffect::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;

    CGS_ASSERT(mpEnclosureControl != nullptr, "mpEnclosureControl");
    mfTime       = 0.0f;
    mfSpaceSize  = 15.0f;
    mfBrightness = 1.0f;
    mfGain       = 1.0f;
    mInterpolateReverb.Initialize(1.0f, 1.0f, 0.0f, CgsSound::Utils::Curve::E_LINEAR);
    meReverbState = E_REVERB_STATE_NONE;
    mReverbType.Flush(AttribSys::Enums::eReverbTypes::ReverbTypeCount);

    static s32 siDiagAttach = 0;
    if (SndEnvDiagBudget(siDiagAttach))
        *CgsDev::Log::gpDebugPrint << "[sndenv] reverb attach [FLAG PC witness]\n";
    return true;
}

// Controller slot 0 is the enclosure control, slot 1 the physics control.
s32 ReverbEffect::GetController(s32 aiIndex)
{
    if (aiIndex == 0)
        return 10;
    if (aiIndex == 1)
        return 0;
    return -1;
}

void ReverbEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    const s32 liEffectId = apController->GetEffectID();
    if (liEffectId == 0)
    {
        mpPhysicsControl =
            static_cast<const BrnSound::Vehicles::Engines::PhysicsControl*>(apController);
        return;
    }
    CGS_ASSERT(liEffectId == 10, "Unexpected control.");
    if (liEffectId == 10)
        mpEnclosureControl = static_cast<const EnclosureControl*>(apController);
}

// ---------------------------------------------------------------------------
// ReverbEffect::GetActiveReverb
//
// Very slow impact time selects the super-slow-motion preset and any other impact time
// the impact preset. In normal time the sound-enclosure regions at the car decide;
// the later region in the list wins when several are active.
// ---------------------------------------------------------------------------
AttribSys::Enums::eReverbTypes::eReverbTypes ReverbEffect::GetActiveReverb() const
{
    using namespace AttribSys::Enums::eReverbTypes;
    using BrnTrigger::GenericRegion;

    const AttribSys::Enums::eImpactTime::eImpactTime leImpactTime =
        static_cast<const BrnSound::Module::SoundLogicModule*>(mpLogicModule)
            ->GetFrameInformation().meImpactTime.GetCurrent();
    if (leImpactTime == AttribSys::Enums::eImpactTime::VSlow)
        return ReverbTypeSuperSloMo;
    if (leImpactTime != AttribSys::Enums::eImpactTime::False)
        return ReverbTypeImpactTime;

    const EntityTriggerInfo& lrTriggers =
        mpEnclosureControl->GetTriggerInfo(E_TRIGGER_POSITION_AT_ENTITY);
    eReverbTypes leReverb = ReverbTypeNone;
    if (lrTriggers.IsTypeActive(GenericRegion::E_TYPE_TUNNEL))
        leReverb = ReverbTypeTunnel;
    if (lrTriggers.IsTypeActive(GenericRegion::E_TYPE_OVERPASS))
        leReverb = ReverbTypeOverpass;
    if (lrTriggers.IsTypeActive(GenericRegion::E_TYPE_BRIDGE))
        leReverb = ReverbTypeBridge;
    if (lrTriggers.IsTypeActive(GenericRegion::E_TYPE_WAREHOUSE))
        leReverb = ReverbTypeWarehouse;
    if (lrTriggers.IsTypeActive(GenericRegion::E_TYPE_LARGE_OVERHEAD_OBJECT))
        leReverb = ReverbTypeLargeOverheadObject;
    if (lrTriggers.IsTypeActive(GenericRegion::E_TYPE_NARROW_ALLEY))
        leReverb = ReverbTypeNarrowAlley;
    return leReverb;
}

// Initial values at 82F2CE20/82F2CE1C and 82FFB8C9; original debug variables.
f32 KF_REVERB_INTERP_TIME_MAX = 500.0f;
f32 KF_REVERB_INTERP_TIME_MIN = 100.0f;
bool KB_DEBUG_REVERB_ZONE = false;

// 82C63E00 hashes the three parameter names; 82C636E8 hashes Send01.
static const u32 K_ParamReverbTime = static_cast<u32>(CgsSound::Playback::Name::MakeHash("ParamReverbTime"));
static const u32 K_ParamReverbSpaceSize = static_cast<u32>(CgsSound::Playback::Name::MakeHash("ParamReverbSpaceSize"));
static const u32 K_ParamReverbBrightness = static_cast<u32>(CgsSound::Playback::Name::MakeHash("ParamReverbBrightness"));
static const u32 K_DefaultSendName = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01"));

// ARTIST 826D1498. The physics pointer is the full PhysicsControl; +0x128
// is mSpeedMPH.current (its UpdateParams stores it at adjusted-this+0x124).
void ReverbEffect::UpdateParams(f32 afTimeStep)
{
    using namespace CgsSound::Utils;
    const auto leReverb = GetActiveReverb();
    mReverbType.Update(leReverb);
    if (mReverbType.HasChanged())
    {
        meReverbState = E_REVERB_STATE_INTERPOLATING;
        const Slope lSlope(SlopeParams(0.0f, 60.0f,
            KF_REVERB_INTERP_TIME_MAX, KF_REVERB_INTERP_TIME_MIN));
        const f32 lfMillis = lSlope.GetValue(
            mpPhysicsControl->GetPhysicsData().mSpeedMPH.GetCurrent(), Curve::E_LINEAR);
        const f32 lfCurrent = mInterpolateReverb.GetValueFloat();
        const f32 lfScaledMillis = lfMillis * lfCurrent;
        f32 lfDuration = lfScaledMillis * 0.001f;
        // 826D1590 bgt ->1598 keeps it; <=0 OR unordered ->1594 stores .01.
        if (!(lfDuration > 0.0f))
            lfDuration = 0.01f;

        // This copy of Initialize is inlined in ARTIST. Keep the stores here:
        // the duration is already in seconds, and NaN takes the floor above.
        mInterpolateReverb.mfElapsedTime = 0.0f;
        mInterpolateReverb.mfLength = lfDuration;
        mInterpolateReverb.mfStart = lfCurrent;
        mInterpolateReverb.mfFinish = 0.0f;
        mInterpolateReverb.meCurveTypes = Curve::E_POWER;
        mInterpolateReverb.mfCurrentValue = lfCurrent;
        mInterpolateReverb.mbComplete = false;
    }

    mInterpolateReverb.Update(afTimeStep);
    if (meReverbState == E_REVERB_STATE_INTERPOLATING && mInterpolateReverb.IsFinished())
    {
        auto* lpLogicModule = static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
        CGS_ASSERT(lpLogicModule != nullptr, "lpLogicModule");
        const Attrib::Gen::reverbparams lParams(lpLogicModule->GetGlobalData().ReverbSettings(leReverb));
        mfTime = lParams.Time();
        mfSpaceSize = lParams.SpaceSize();
        mfBrightness = lParams.Brightness();
        mfGain = lParams.Gain();
        mInterpolateReverb.Initialize(1.0f, 1.0f, 0.0f, Curve::E_LINEAR);
        meReverbState = E_REVERB_STATE_NONE;

        static s32 siDiagPreset = 0;
        if (SndEnvDiagBudget(siDiagPreset))
            *CgsDev::Log::gpDebugPrint << "[sndenv] reverb preset=" << static_cast<s32>(leReverb)
                << " time=" << mfTime << " space=" << mfSpaceSize
                << " brightness=" << mfBrightness << " gain=" << mfGain << " [FLAG PC witness]\n";
    }

    if (KB_DEBUG_REVERB_ZONE)
    {
        auto* lpLogicModule = static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
        CGS_ASSERT(lpLogicModule != nullptr, "lpLogicModule");
        const Attrib::Gen::reverbparams lParams(lpLogicModule->GetGlobalData().ReverbSettings(mReverbType.GetCurrent()));
        mfTime = lParams.Time();
        mfSpaceSize = lParams.SpaceSize();
        mfBrightness = lParams.Brightness();
        mfGain = lParams.Gain();
        *CgsDev::Log::gpDebugPrint << "[Time]: " << mfTime << "[Space]: " << mfSpaceSize
            << "[Bright]: " << mfBrightness << "[Gain]: " << mfGain << "\n";
        static const char* const kaNames[] = {
            "ReverbTypeNone", "ReverbTypeTunnel", "ReverbTypeOverpass", "ReverbTypeBridge",
            "ReverbTypeWarehouse", "ReverbTypeLargeOverheadObject", "ReverbTypeNarrowAlley",
            "ReverbTypeUrban", "ReverbTypeRural", "ReverbTypeImpactTime", "ReverbTypeSuperSloMo", "ReverbTypeCount"
        };
        CgsDev::DebugInterface lDebug;
        lDebug.Get2dRender().Draw2DText(kaNames[mReverbType.GetCurrent()], 900.0f, 50.0f, 40.0f, 0xFF00FFA0u);
    }
}

// ARTIST 826BA590: every frame sends the preset's three parameters and the
// interpolated wet gain to the live global reverb voice (id 2).
void ReverbEffect::ProcessUpdate()
{
    auto& lrVoice = static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule)->GetGlobalReverbVoice();
    lrVoice.SetParameter(0, mfTime, &K_ParamReverbTime);
    lrVoice.SetParameter(1, mfSpaceSize, &K_ParamReverbSpaceSize);
    lrVoice.SetParameter(2, mfBrightness, &K_ParamReverbBrightness);
    const f32 lfMixGain = GetRWACMixerOutputValue(0, Nicotine::DMixIO::DMX_VOL);
    const f32 lfPresetGain = mInterpolateReverb.GetValueFloat() * mfGain;
    const f32 lfGain = lfPresetGain * lfMixGain;
    lrVoice.SetGain(0, lfGain, &K_DefaultSendName);

    static s32 siDiagProcess = 0;
    if (meReverbState == E_REVERB_STATE_NONE && SndEnvDiagBudget(siDiagProcess))
        *CgsDev::Log::gpDebugPrint << "[sndenv] reverb applied ready=" << static_cast<s32>(lrVoice.IsReady())
            << " preset=" << static_cast<s32>(mReverbType.GetCurrent()) << " gain=" << lfGain
            << " [FLAG PC witness]\n";
}

// ---------------------------------------------------------------------------
// ~ReverbEffect  @ 0x826BA4E8  (anchor for the X360 `vector deleting destructor').
// This X360 body is store-for-store identical to the committed BrnEffectObject dtor
// @ 0x826AF4C8 -- the same dual-vptr settle + the same attach/detach/resources-ready
// clears. In reconstructed C++ that dual-base settle and the deleting-destructor thunk
// are compiler-synthesised from the virtual dtor declared in the header, so the leaf
// body is empty. The (a2 & 1) allocator-free tail is left to the host toolchain.
// ---------------------------------------------------------------------------
ReverbEffect::~ReverbEffect()
{
}

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound
