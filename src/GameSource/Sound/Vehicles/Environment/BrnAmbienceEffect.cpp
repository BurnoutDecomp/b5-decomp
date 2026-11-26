#include "GameSource/Sound/Vehicles/Environment/BrnAmbienceEffect.h"
#include "GameSource/Sound/Vehicles/Environment/BrnAmbienceControl.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Streaming/BrnStreamingStateManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"
#include "GameShared/GameClasses/Sound/Playback/Module/CgsSoundPlaybackModule.h"
#include "SDKs/EATech/include/Nicotine/DMixIO.hpp"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h" // GenericRwacFactory::SK_NAME (ARTIST reads the static, 0x83008650)

// ARTIST compiled this file through the GameSource unity file; its asserts carry this path.
#define BRN_AMBIENCEEFFECT_CPP_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Sound/Vehicles/Environment/BrnAmbienceEffect.cpp"

// =============================================================================
// BrnSound::Vehicles::Environment::AmbienceEffect -- out-of-line bodies.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

// A new ambience bed fades in, and a replaced one fades out, over this many seconds.
static const f32 KF_AMBIENCE_FADE_TIME = 2.0f;

// The ambience bed content specs, indexed by the ambience-control region: the map
// districts, then the light / heavy traffic and tunnel beds, then the invalid asset.
static const CgsSound::Playback::Name KA_AMBIENCE_NAMES[22] =
{
    CgsSound::Playback::Name("OceanView"),
    CgsSound::Playback::Name("WestAcres"),
    CgsSound::Playback::Name("TwinBridges"),
    CgsSound::Playback::Name("BigSurfBeach"),
    CgsSound::Playback::Name("EasternShore"),
    CgsSound::Playback::Name("HillsidePass"),
    CgsSound::Playback::Name("HeartbreakHills"),
    CgsSound::Playback::Name("RockridgeCliffs"),
    CgsSound::Playback::Name("SouthBay"),
    CgsSound::Playback::Name("ParkVale"),
    CgsSound::Playback::Name("ParadiseWharf"),
    CgsSound::Playback::Name("CrystalSummit"),
    CgsSound::Playback::Name("LonePeaks"),
    CgsSound::Playback::Name("SunsetValley"),
    CgsSound::Playback::Name("Downtown"),
    CgsSound::Playback::Name("RiverCity"),
    CgsSound::Playback::Name("MotorCity"),
    CgsSound::Playback::Name("Waterfront"),
    CgsSound::Playback::Name("DowntownTraffic"),
    CgsSound::Playback::Name("DowntownTrafficHv"),
    CgsSound::Playback::Name("tunnel_amb"),
    CgsSound::Playback::Name("INVALID_ASSET"),
};

// ---------------------------------------------------------------------------
// AmbienceEffect::AmbienceEffect
//
// The BrnEffectObject dual base and the IStreamUser interface sub-object are built by
// their own constructors; the leaf zero words and the -1 are mParams' default
// construction.
// ---------------------------------------------------------------------------
// ARTIST 0x82F2F7A8: {0x10080, "AmbienceEffect", BrnEffectObject, CreateObject}, registered by
// its startup thunk through ClassTypeInfo<EffectObject>::AddToClassTypeInfoArray.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> AmbienceEffect::sTypeInfo(
    0x10080, "AmbienceEffect", &BrnSound::Logic::BrnEffectObject::sTypeInfo, &AmbienceEffect::CreateObject);

CgsSound::Logic::EffectObject* AmbienceEffect::CreateObject(u32 luAllocator)
{
    if (luAllocator == 0)
        return new (sTypeInfo.typeName, false) AmbienceEffect();
    else
        return new (sTypeInfo.typeName, true) AmbienceEffect();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* AmbienceEffect::GetStaticTypeInfo()
{
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* AmbienceEffect::GetTypeInfo() const
{
    return &sTypeInfo;
}

const char* AmbienceEffect::GetTypeName() const
{
    return sTypeInfo.typeName;
}


// ---------------------------------------------------------------------------
// ~AmbienceEffect  (the leaf vtable emission point). The teardown is the inherited
// BrnEffectObject / IStreamUser chain; this leaf body adds nothing.
// ---------------------------------------------------------------------------
AmbienceEffect::~AmbienceEffect()
{
}

// Controller slot 0 is the ambience control.
s32 AmbienceEffect::GetController(s32 aiIndex)
{
    return aiIndex == 0 ? 16 : -1;
}

void AmbienceEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    // ARTIST 0x826861A0: no null test; a switch whose default asserts.
    switch (apController->GetEffectID())
    {
    case 16:
        mpAmbienceControl = static_cast<AmbienceControl*>(apController);
        break;
    default:
        CGS_FAIL_AT("Unexpected control.", BRN_AMBIENCEEFFECT_CPP_FILE, 448);
        break;
    }
}

// ---------------------------------------------------------------------------
// AmbienceEffect::UpdateParams
//
// When the ambience control's region changes, the bed for the new region is set up
// (MusicVoiceSpec on the player slot, Send01 on the logic submix), the playing bed is
// asked to fade out and the new one is requested (priority 0, lag tolerance 0.1 s).
// ---------------------------------------------------------------------------
void AmbienceEffect::UpdateParams(f32 /*afTimeStep*/)
{
    CGS_ASSERT(mpAmbienceControl != 0, "mpAmbienceControl");

    const CgsSound::Utils::DataPoint<u8>& lrRegion = mpAmbienceControl->GetRegion();
    if (!lrRegion.HasChanged())
        return;

    BrnSound::Logic::Streaming::StreamingStateManager* lpStreamingStateMan =
        static_cast<BrnSound::Logic::Streaming::StreamingStateManager*>(
            static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule)
                ->GetEnvironment().GetStateManager(6));
    CGS_ASSERT(lpStreamingStateMan != 0, "lpStreamingStateMan");

    mParams.mpLogicModule = mpLogicModule;
    mParams.mFactoryName = static_cast<u32>(
        CgsSound::Playback::GenericRwacFactory::SK_NAME.GetValue());
    mParams.mVoiceSpecName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("MusicVoiceSpec"));
    mParams.mSlotName = static_cast<u32>(
        CgsSound::Playback::PlayerVoice::SK_PLAYER_SLOT_NAME.GetValue());
    mParams.mContentSpecName = static_cast<u32>(
        KA_AMBIENCE_NAMES[lrRegion.GetCurrent()].GetValue());
    mParams.mSendName = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01"));
    mParams.mSubMixVoiceID = 1;
    mParams.miSendIndex = 0;

    lpStreamingStateMan->PostStreamRequest(
        BrnSound::Logic::Streaming::StreamStopRequest(this, KF_AMBIENCE_FADE_TIME));
    lpStreamingStateMan->PostStreamRequest(
        BrnSound::Logic::Streaming::StreamRequest(this, 0, 0.1f));

    static s32 siDiagPlay = 0;
}

// ---------------------------------------------------------------------------
// AmbienceEffect::Detach
//
// Resumable detach: fade the bed out, run the base detach, finish. An unknown detach
// state asserts and reports "not finished".
// ---------------------------------------------------------------------------
bool AmbienceEffect::Detach()
{
    switch (meDetachState)
    {
    case E_DETACH_STATE_NONE:
    case E_DETACH_STATE_BEGIN:
    {
        BrnSound::Logic::Streaming::StreamingStateManager* lpStreamingStateMan =
            static_cast<BrnSound::Logic::Streaming::StreamingStateManager*>(
                static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule)
                    ->GetEnvironment().GetStateManager(6));
        CGS_ASSERT(lpStreamingStateMan != 0, "lpStreamingStateMan");
        lpStreamingStateMan->PostStreamRequest(
            BrnSound::Logic::Streaming::StreamStopRequest(this, KF_AMBIENCE_FADE_TIME));
    }
        // fall through
    case E_DETACH_STATE_UPDATING:
        if (!BrnEffectObject::Detach())
            return false;
        // fall through
    case E_DETACH_STATE_FINISHED:
        meDetachState = E_DETACH_STATE_FINISHED;
        return true;
    default:
        CGS_ASSERT(meDetachState >= E_DETACH_STATE_NONE &&
                       meDetachState <= E_DETACH_STATE_FINISHED,
                   "Unexpected state");
        return false;
    }
}

const CgsSound::Logic::VoiceWrapper::CreateParams& AmbienceEffect::GetCreateParams() const
{
    return mParams;
}

// ---------------------------------------------------------------------------
// AmbienceEffect::UpdateVoiceParams
//
// The streamed bed plays unpaused; its Send01 gain is the stream gain times the
// dynamic-mixer volume, ramped linearly from silence over the first
// KF_AMBIENCE_FADE_TIME seconds of the stream.
// ---------------------------------------------------------------------------
void AmbienceEffect::UpdateVoiceParams(CgsSound::Logic::VoiceWrapper& arVoice,
                                       f32 afGain, f32 afElapsedTime)
{
    const f32 lfMixerGain = GetRWACMixerOutputValue(0, Nicotine::DMixIO::DMX_VOL);
    const f32 lfFadedGain = !(afElapsedTime < KF_AMBIENCE_FADE_TIME)
        ? lfMixerGain
        : (afElapsedTime / KF_AMBIENCE_FADE_TIME) * lfMixerGain;

    const u32 luPauseControl = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("PauseControl"));
    arVoice.SetParameter(1, 0.0f, &luPauseControl);
    const u32 luSend = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01"));
    arVoice.SetGain(0, lfFadedGain * afGain, &luSend);
}

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound
