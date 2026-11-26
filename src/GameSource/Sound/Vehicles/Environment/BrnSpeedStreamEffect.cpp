#include "GameSource/Sound/Vehicles/Environment/BrnSpeedStreamEffect.h"
#include "GameSource/Sound/Vehicles/Environment/BrnSpeedStreamControl.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SDKs/EATech/include/Nicotine/DMixIO.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Streaming/BrnStreamingStateManager.h"
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"
#include "GameShared/GameClasses/Sound/Playback/Module/CgsSoundPlaybackModule.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h" // GenericRwacFactory::SK_NAME (ARTIST reads the static, 0x83008650)

// ARTIST compiled this file through the GameSource unity file; its asserts carry this path.
#define BRN_SPEEDSTREAMEFFECT_CPP_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Sound/Vehicles/Environment/BrnSpeedStreamEffect.cpp"

// =============================================================================
// BrnSound::Vehicles::Environment::SpeedStreamEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Recon'd function set:
//   CreateObject(uint32_t)         @ 0x826D13D8  (IDA name truncated to "Create")
//   SpeedStreamEffect()            @ 0x826BA148  (implicit: DWARF gives it the class line h:152;
//                                                  mpSpeedStreamControl is left unset)
//   `scalar deleting destructor'   @ 0x826BA1F8  (-> ~SpeedStreamEffect anchor)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{
// DecFIGS BrnSpeedStreamEffect.cpp:34..35 / ARTIST82F2CE14/18.
f32 KF_SPEED_STREAM_THRESHOLD_MPH = 105.0f;
f32 KF_SPEED_STREAM_CROSSFADE_WIDTH = 40.0f;

// ---------------------------------------------------------------------------
// ARTIST 0x82F2F7E8: {0x10070, "SpeedStreamEffect", BrnEffectObject, CreateObject}, registered by
// its startup thunk through ClassTypeInfo<EffectObject>::AddToClassTypeInfoArray.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> SpeedStreamEffect::sTypeInfo(
    0x10070, "SpeedStreamEffect", &BrnSound::Logic::BrnEffectObject::sTypeInfo, &SpeedStreamEffect::CreateObject);

CgsSound::Logic::EffectObject* SpeedStreamEffect::CreateObject(u32 luAllocator)
{
    if (luAllocator == 0)
        return new (sTypeInfo.typeName, false) SpeedStreamEffect();
    else
        return new (sTypeInfo.typeName, true) SpeedStreamEffect();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* SpeedStreamEffect::GetStaticTypeInfo()
{
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* SpeedStreamEffect::GetTypeInfo() const
{
    return &sTypeInfo;
}

const char* SpeedStreamEffect::GetTypeName() const
{
    return sTypeInfo.typeName;
}

s32 SpeedStreamEffect::GetController(s32 aiIndex)
{
    return aiIndex == 0 ? 15 : -1;
}

void SpeedStreamEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    // ARTIST 0x826863D8: no null test; a switch whose default asserts.
    switch (apController->GetEffectID())
    {
    case 15:
        mpSpeedStreamControl = static_cast<SpeedStreamControl*>(apController);
        break;
    default:
        CGS_ASSERT_AT(false, "Unexpected control.", BRN_SPEEDSTREAMEFFECT_CPP_FILE, 324);
        break;
    }
}

// ---------------------------------------------------------------------------
// SpeedStreamEffect::UpdateParams
//
// Follows the speed-stream control's on/off edge. On the rising edge the stream voice
// parameters are rebuilt (the "SpeedUrban" content through MusicVoiceSpec on the player
// slot, send 0 on the logic submix) and a play request goes to the streaming state
// manager (priority 1, lag tolerance 0.1 s); on the falling edge a stop request with no
// fade-out is posted. Both asserts are non-gating on the console.
// ---------------------------------------------------------------------------
void SpeedStreamEffect::UpdateParams(f32 /*afTimeStep*/)
{
    CGS_ASSERT(mpSpeedStreamControl != 0, "mpSpeedStreamControl");

    const CgsSound::Utils::DataPoint<bool>& lrSpeedStream =
        mpSpeedStreamControl->GetSpeedStreamStatus();
    if (!lrSpeedStream.HasChanged())
        return;

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);

    if (lrSpeedStream.GetCurrent())
    {
        mParams.mpLogicModule = lpModule;
        mParams.mFactoryName = static_cast<u32>(
            CgsSound::Playback::GenericRwacFactory::SK_NAME.GetValue());
        mParams.mVoiceSpecName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("MusicVoiceSpec"));
        mParams.mSlotName = static_cast<u32>(
            CgsSound::Playback::PlayerVoice::SK_PLAYER_SLOT_NAME.GetValue());
        mParams.mContentSpecName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("SpeedUrban"));
        mParams.mSendName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("Send01"));
        mParams.miSendIndex = 0;
        mParams.mSubMixVoiceID = 1;

        BrnSound::Logic::Streaming::StreamingStateManager* lpStreamingStateMan =
            static_cast<BrnSound::Logic::Streaming::StreamingStateManager*>(
                lpModule->GetEnvironment().GetStateManager(6));
        CGS_ASSERT(lpStreamingStateMan != 0, "lpStreamingStateMan");
        lpStreamingStateMan->PostStreamRequest(
            BrnSound::Logic::Streaming::StreamRequest(this, 1, 0.1f));

        static s32 siDiagPlay = 0;
    }
    else
    {
        BrnSound::Logic::Streaming::StreamingStateManager* lpStreamingStateMan =
            static_cast<BrnSound::Logic::Streaming::StreamingStateManager*>(
                lpModule->GetEnvironment().GetStateManager(6));
        CGS_ASSERT(lpStreamingStateMan != 0, "lpStreamingStateMan");
        lpStreamingStateMan->PostStreamRequest(
            BrnSound::Logic::Streaming::StreamStopRequest(this, 0.0f));

        static s32 siDiagStop = 0;
    }
}

// ---------------------------------------------------------------------------
// SpeedStreamEffect::Detach
//
// After the base detach succeeds, a stop request with a 0.25 s fade-out is posted
// unconditionally (the null manager asserts, it does not skip).
// ---------------------------------------------------------------------------
bool SpeedStreamEffect::Detach()
{
    if (!BrnEffectObject::Detach())
        return false;

    BrnSound::Logic::Streaming::StreamingStateManager* lpStreamingStateMan =
        static_cast<BrnSound::Logic::Streaming::StreamingStateManager*>(
            static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule)
                ->GetEnvironment().GetStateManager(6));
    CGS_ASSERT_AT(lpStreamingStateMan != 0, "lpStreamingStateMan", BRN_SPEEDSTREAMEFFECT_CPP_FILE, 415);
    lpStreamingStateMan->PostStreamRequest(
        BrnSound::Logic::Streaming::StreamStopRequest(this, 0.25f));
    return true;
}

const CgsSound::Logic::VoiceWrapper::CreateParams&
SpeedStreamEffect::GetCreateParams() const
{
    return mParams;
}

void SpeedStreamEffect::UpdateVoiceParams(CgsSound::Logic::VoiceWrapper& arVoice,
                                           f32 afGain, f32 /*afElapsedTime*/)
{
    // ARTIST @ 0x826BA370: MusicVoiceSpec parameter 1 is PauseControl; send 0
    // receives the streamed gain multiplied by DMX_VOL output 0.
    const u32 luPauseControl = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("PauseControl"));
    arVoice.SetParameter(1, 0.0f, &luPauseControl);
    const u32 luSend = mParams.mSendName;
    arVoice.SetGain(0,
                    afGain * GetRWACMixerOutputValue(0, Nicotine::DMixIO::DMX_VOL),
                    &luSend);
}

// ---------------------------------------------------------------------------
// ~SpeedStreamEffect  @ 0x826BA1F8   (anchor for the X360 `scalar deleting destructor').
// The dual-vptr settle (+0/+4) and the attach/detach/resources-ready member clears are
// the inherited BrnEffectObject teardown the compiler emits (the IStreamUser third-base
// teardown is likewise compiler-synthesised from the MI base list); this leaf body adds
// nothing. The (a2 & 1) allocator-free tail is left to the host toolchain (off_82FFB954
// not homed here).
// ---------------------------------------------------------------------------
SpeedStreamEffect::~SpeedStreamEffect()
{
}

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound
