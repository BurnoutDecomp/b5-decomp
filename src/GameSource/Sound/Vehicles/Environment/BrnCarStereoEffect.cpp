#include "GameSource/Sound/Vehicles/Environment/BrnCarStereoEffect.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Vehicles/BrnVehicleState.h"
#include "GameSource/Sound/Vehicles/Environment/BrnEnvironmentSoundDiag.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"
#include "GameShared/GameClasses/Sound/Playback/Module/CgsSoundPlaybackModule.h"
#include "SharedClasses/DataLists/VehicleListEntry.h"
#include "SDKs/EATech/include/Nicotine/DMixIO.hpp"

// =============================================================================
// BrnSound::Vehicles::Environment::CarStereoEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Recon'd function set:
//   CarStereoEffect()              @ 0x826D1858  (MSVC inlined full-object ctor)
//   CreateObject(u32)              @ 0x826E6298  (the factory hook)
//   `scalar deleting destructor'   @ 0x826E62F8  (-> ~CarStereoEffect anchor)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

// ---------------------------------------------------------------------------
// CarStereoEffect::CarStereoEffect  @ 0x826D1858
//
// MSVC's INLINED full-object constructor (mirrors the committed ExplosionEffect ctor
// store-for-store apart from the class-specific leaf vptr constants): it inlines the
// BrnEffectObject base member zero-init and installs the two leaf vptrs directly, then
// constructs the embedded VoiceWrapper (mVoice) at +0x38. In reconstructed C++ the two
// vptr installs + base member zero-init are produced implicitly by the committed
// BrnEffectObject base sub-object's own default ctor (reused BY NAME); the only
// hand-written tail effect is the embedded mVoice construction. The leaf scalar
// zero-inits DWARF does not pin are covered by the member inits below / value-init.
// ---------------------------------------------------------------------------
CarStereoEffect::CarStereoEffect()
    : BrnEffectObject()   // installs the base dual vptrs + zero-inits base members (BY NAME)
    , mVoice()            // tail `bl CgsSound::Logic::VoiceWrapper::VoiceWrapper(this+0x38)`
    , mpLogicModule(nullptr)
    , mbHasStereo(false)
{
}

// ---------------------------------------------------------------------------
// CarStereoEffect::CreateObject(u32)  @ 0x826E6298
// Allocates a 144-byte (0x90) block via CgsSound::MemBase::operator new(size, tag,
// flavour) tagged "CarStereoEffect" and constructs a CarStereoEffect, handing it back
// through its EffectObject base sub-object (+4). The arg only selects the operator-new
// flavour (0/1).
// FLAG (allocator gate): CgsSound::MemBase::operator new is not homed here, so this uses
// the host `new`; observable result matches. The 0x90 size is documentation only.
// ---------------------------------------------------------------------------
CgsSound::Logic::EffectObject* CarStereoEffect::CreateObject( u32 /*luType*/ )
{
    return new CarStereoEffect();
}

// At most this many car stereos play at once.
static const s32 KI_MAX_CAR_STEREOS_PLAYING = 2;

s32 CarStereoEffect::siNumStereos = 0;

// ---------------------------------------------------------------------------
// CarStereoEffect::Attach
//
// A car gets a stereo when fewer than two are already playing, the player is in free
// roam, and its vehicle-list entry names a music loop. The loop is created on the car's
// positional voice (player slot, Send01 on the logic submix), started at once, and given
// its fixed simple-panning radius / centre / main / LFE levels.
// ---------------------------------------------------------------------------
bool CarStereoEffect::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;

    mpLogicModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(CgsSound::Logic::EffectBase::mpLogicModule);

    const BrnSound::Vehicles::VehicleState* lpState =
        static_cast<const BrnSound::Vehicles::VehicleState*>(GetStateBase());
    CGS_ASSERT(lpState->GetAttachInfo().mpVehicleAsset != nullptr,
               "lpState->GetAttachInfo().mpVehicleAsset");

    if (siNumStereos >= KI_MAX_CAR_STEREOS_PLAYING || !ShouldPlayMusic() ||
        static_cast<const BrnResource::VehicleListEntry*>(
            lpState->GetAttachInfo().mpVehicleAsset)->muiMusicLoopContentSpec == 0)
    {
        mbHasStereo = false;
        return true;
    }

    ++siNumStereos;
    mbHasStereo = true;

    CgsSound::Logic::VoiceWrapper::CreateParams lParams;
    lParams.mpLogicModule = CgsSound::Logic::EffectBase::mpLogicModule;
    lParams.mpOnPostInit = 0;
    lParams.mFactoryName = static_cast<u32>(
        CgsSound::Playback::GenericRwacFactorySkName().GetValue());
    lParams.mVoiceSpecName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("PositionalVoiceSpec"));
    lParams.mpContent = 0;
    lParams.mSlotName = static_cast<u32>(
        CgsSound::Playback::PlayerVoice::SK_PLAYER_SLOT_NAME.GetValue());
    lParams.mContentSpecName = static_cast<const BrnResource::VehicleListEntry*>(
        lpState->GetAttachInfo().mpVehicleAsset)->muiMusicLoopContentSpec;
    CGS_ASSERT(lParams.mContentSpecName != 0x7FFFFFFF,
               "lParams.mContentSpecName != 0x7FFFFFFF");
    lParams.miSendIndex = 0;
    lParams.mSubMixVoiceID = 1;
    lParams.mSendName = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01"));
    lParams.mReverbSendName = 0;
    lParams.mReverbSubMixVoiceID = 0;

    mVoice.Create(lParams);
    mVoice.Play(0);

    const u32 luRadius = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("SimplePanningRadius"));
    const u32 luCentreLevel = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("SimplePanningCentreLevel"));
    const u32 luMainLevel = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("SimplePanningMainLevel"));
    const u32 luLfeLevel = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("SimplePanningLfeLevel"));
    mVoice.SetParameter(2, 0.85f, &luRadius);
    mVoice.SetParameter(3, 1.0f, &luCentreLevel);
    mVoice.SetParameter(4, 1.0f, &luMainLevel);
    mVoice.SetParameter(5, 0.0f, &luLfeLevel);

    static s32 siDiagAttach = 0;
    if (SndEnvDiagBudget(siDiagAttach))
        *CgsDev::Log::gpDebugPrint << "[sndenv] car-stereo play content="
                                   << static_cast<u32>(lParams.mContentSpecName)
                                   << " stereos=" << siNumStereos << " [FLAG PC witness]\n";
    return true;
}

// ---------------------------------------------------------------------------
// CarStereoEffect::UpdateParams
//
// Drives the voice wrapper, then stops the stereo as soon as the player leaves free roam.
// ---------------------------------------------------------------------------
void CarStereoEffect::UpdateParams(f32 /*afTimeStep*/)
{
    mVoice.Update();
    if (mbHasStereo && !ShouldPlayMusic())
    {
        mVoice.Stop();
        --siNumStereos;
        mbHasStereo = false;

        static s32 siDiagStop = 0;
        if (SndEnvDiagBudget(siDiagStop))
            *CgsDev::Log::gpDebugPrint << "[sndenv] car-stereo stop stereos=" << siNumStereos
                                       << " [FLAG PC witness]\n";
    }
}

// ---------------------------------------------------------------------------
// CarStereoEffect::ProcessUpdate
//
// Pushes the dynamic-mixer outputs onto the positional voice: azimuth (output 1),
// pitch (output 2) and the Send01 gain (output 0, scaled by 4).
// ---------------------------------------------------------------------------
void CarStereoEffect::ProcessUpdate()
{
    const f32 lfGain    = GetRWACMixerOutputValue(0, Nicotine::DMixIO::DMX_VOL);
    const f32 lfPitch   = GetRWACMixerOutputValue(2, Nicotine::DMixIO::DMX_PITCH);
    const f32 lfAzimuth = GetRWACMixerOutputValue(1, Nicotine::DMixIO::DMX_AZIM);

    const u32 luAzimuth = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("SimplePanningAzimuth"));
    const u32 luPitch = static_cast<u32>(CgsSound::Playback::Name::MakeHash(
        "~GenericRwacPlayerVoice::SK_PLAYER_PARAMETER_PITCH~"));
    const u32 luSend = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01"));
    mVoice.SetParameter(1, lfAzimuth, &luAzimuth);
    mVoice.SetParameter(0, lfPitch, &luPitch);
    mVoice.SetGain(0, lfGain * 4.0f, &luSend);
}

// ---------------------------------------------------------------------------
// CarStereoEffect::Detach
//
// Resumable detach: give back the stereo slot, run the base detach, then release the
// voice wrapper. An unknown detach state reports "not finished".
// ---------------------------------------------------------------------------
bool CarStereoEffect::Detach()
{
    switch (meDetachState)
    {
    case E_DETACH_STATE_NONE:
    case E_DETACH_STATE_BEGIN:
        meDetachState = E_DETACH_STATE_BEGIN;
        if (mbHasStereo)
        {
            --siNumStereos;
            mbHasStereo = false;
        }
        // fall through
    case E_DETACH_STATE_UPDATING:
        meDetachState = E_DETACH_STATE_UPDATING;
        if (!BrnEffectObject::Detach())
            return false;
        // fall through
    case E_DETACH_STATE_FINISHED:
        meDetachState = E_DETACH_STATE_FINISHED;
        mVoice.Release();
        return true;
    default:
        return false;
    }
}

// ---------------------------------------------------------------------------
// CarStereoEffect::ShouldPlayMusic
// ---------------------------------------------------------------------------
bool CarStereoEffect::ShouldPlayMusic()
{
    const BrnSound::Module::Io::RootInputBuffer::GameModeOutputInterface* lpGameModeInterface =
        mpLogicModule->GetBrnInputStructure()->GetGameModeInterface();
    CGS_ASSERT(lpGameModeInterface != nullptr, "lpGameModeInterface");
    return lpGameModeInterface->miCurrentGameModeType ==
           BrnGameState::GameStateModuleIO::E_MODE_NONE;
}

// ---------------------------------------------------------------------------
// ~CarStereoEffect  @ 0x826E62F8  (anchor for the X360 `scalar deleting destructor').
// The embedded mVoice teardown + the dual-vptr settle + the attach/detach/resources-
// ready clears are the inherited BrnEffectObject teardown (plus the mVoice member dtor)
// the compiler emits; this leaf body adds nothing. The (a2 & 1) allocator-free tail is
// left to the host toolchain (off_82FFB954 not homed here).
// ---------------------------------------------------------------------------
CarStereoEffect::~CarStereoEffect()
{
}

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound
