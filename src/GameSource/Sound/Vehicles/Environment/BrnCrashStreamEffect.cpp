#include "GameSource/Sound/Vehicles/Environment/BrnCrashStreamEffect.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SDKs/EATech/include/Nicotine/DMixIO.hpp"
#include "SDKs/EATech/include/Nicotine/IDynamicMixer.hpp"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameSource/Sound/BrnMixerData.h"
#include "GameSource/Sound/Global/BrnGlobalStateManager.h"
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Streaming/BrnStreamingStateManager.h"
#include "GameSource/Sound/Vehicles/Environment/BrnEnvironmentSoundDiag.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameShared/GameClasses/Sound/Logic/CgsEnvironment.h"
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"
#include "GameShared/GameClasses/Sound/Playback/Module/CgsSoundPlaybackModule.h"

// =============================================================================
// BrnSound::Vehicles::Environment::CrashStreamEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Recon'd function set:
//   CrashStreamEffect()            @ 0x826B9CB8  (MSVC inlined full-object ctor)
//   CreateObject(u32)              @ 0x826B9E50  (the RTTI factory hook)
//   GetContentSpecToPlay(bool)     @ 0x8269B908
//   `vector deleting destructor'   @ 0x826D0E98  (-> ~CrashStreamEffect anchor)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

// The two stream voice specs a take plays through: show time uses the music spec, a
// crash its own crash-stream spec.
static const CgsSound::Playback::Name K_MusicVoiceSpecName("MusicVoiceSpec");
static const CgsSound::Playback::Name K_CrashStreamVoiceSpecName("CrashStreamVoiceSpec");

// ---------------------------------------------------------------------------
// CrashStreamEffect::CrashStreamEffect()  @ 0x826B9CB8
//
// MSVC's INLINED full-object constructor: it inlines the base member zero-inits and
// installs THREE leaf vptrs directly (this+0 primary + this+4 IResourceRequester
// sub-object == the committed BrnEffectObject dual base; this+0x38 == the IStreamUser
// interface sub-object). In reconstructed C++ those three vptr installs + the base
// member zero-inits are produced structurally by the two base sub-objects' own default
// constructors (BrnEffectObject BY NAME + IStreamUser BY NAME). The only attested leaf
// effect modelled by name is mePrepareState = E_PREPARE_STATE_CONSTRUCT_VOICE.
//
// The leaf zero words and the -1 are mParams' default construction; the two data points
// start false and the submix voice starts empty.
// ---------------------------------------------------------------------------
CrashStreamEffect::CrashStreamEffect()
    : BrnEffectObject()                         // primary vptr @+0 + IResourceRequester vptr @+4, base zero-init (BY NAME)
    , BrnSound::Logic::Streaming::IStreamUser() // IStreamUser interface vptr @+0x38 (BY NAME)
    , mParams()
    , mbShowTime(false)
    , mbIsCrashing(false)
    , mpPhysicsControl(nullptr)
    , mSubmix()
    , mePrepareState(E_PREPARE_STATE_CONSTRUCT_VOICE)
{
}

s32 CrashStreamEffect::GetController(s32 aiIndex)
{
    return aiIndex == 0 ? 0 : -1;
}

void CrashStreamEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    CGS_ASSERT(apController != nullptr && apController->GetEffectID() == 0,
               "Unexpected control.");
    if (apController && apController->GetEffectID() == 0)
        mpPhysicsControl = static_cast<BrnSound::Vehicles::Engines::PhysicsControl*>(apController);
}

// ---------------------------------------------------------------------------
// CrashStreamEffect::Prepare
//
// First call: latch the state and logic module (the base Prepare, inlined on the
// console) and construct the submix voice. Every call until the voice reports ready
// returns false with the state parked on CONNECT_VOICE. The show-time data point is then
// pushed false. A call in any later state only asserts and reports ready.
// ---------------------------------------------------------------------------
bool CrashStreamEffect::Prepare(CgsSound::Logic::State* apState)
{
    switch (mePrepareState)
    {
    case E_PREPARE_STATE_CONSTRUCT_VOICE:
    {
        mePrepareState = E_PREPARE_STATE_CONSTRUCT_VOICE;
        CgsSound::Logic::EffectBase::Prepare(apState);
        const u32 luSpecName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("SubmixVoiceSpec"));
        CgsSound::Logic::Module* lpModule = mpLogicModule;
        const u32 luFactoryName = static_cast<u32>(
            CgsSound::Playback::GenericRwacFactorySkName().GetValue());
        mSubmix.Construct(lpModule, lpModule->GetUniqueId(), luFactoryName, luSpecName);
    }
        // fall through
    case E_PREPARE_STATE_CONNECT_VOICE:
        mePrepareState = E_PREPARE_STATE_CONNECT_VOICE;
        if (!mSubmix.IsReady())
            return false;
        break;
    default:
        CGS_ASSERT(mePrepareState == E_PREPARE_STATE_CONSTRUCT_VOICE ||
                       mePrepareState == E_PREPARE_STATE_CONNECT_VOICE,
                   "Unexpected state in Prepare.");
        break;
    }

    mbShowTime.Update(false);

    static s32 siDiagPrepared = 0;
    if (SndEnvDiagBudget(siDiagPrepared))
        *CgsDev::Log::gpDebugPrint << "[sndenv] crash-stream submix ready [FLAG PC witness]\n";
    return true;
}

// ---------------------------------------------------------------------------
// CrashStreamEffect::UpdateParams
//
// 1. Once the submix voice is ready, connect its Send01 to the global collision submix.
// 2. Show time is "the current game mode is offline or online show time"; its rising
//    and falling edges switch the show-time dynamic-mixer snapshot on and off.
// 3. The player's fatal-crash flag is sampled into mbIsCrashing.
// 4. On any edge of either flag the running stream is stopped at once and, while either
//    flag is set, a new take is requested: a show-time take through MusicVoiceSpec or a
//    crash take through CrashStreamVoiceSpec, sent into the effect's own submix.
// ---------------------------------------------------------------------------
void CrashStreamEffect::UpdateParams(f32 /*afTimeStep*/)
{
    using namespace BrnGameState::GameStateModuleIO;

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);

    if (mePrepareState < E_PREPARE_STATE_DONE)
    {
        CGS_ASSERT(E_PREPARE_STATE_CONNECT_VOICE == mePrepareState,
                   "E_PREPARE_STATE_CONNECT_VOICE == mePrepareState");
        BrnSound::Logic::GlobalStateManager* lpMgr =
            static_cast<BrnSound::Logic::GlobalStateManager*>(
                lpModule->GetEnvironment().GetStateManager(0));
        CGS_ASSERT(lpMgr != nullptr, "lpMgr");
        const u32 luSendName = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01"));
        mSubmix.Connect(luSendName,
                        static_cast<u32>(lpMgr->GetSubmixVoice(
                            BrnSound::Logic::GlobalStateManager::E_SUBMIX_VOICE_COLLISION)
                                             .GetIdent()));
        mePrepareState = E_PREPARE_STATE_DONE;
    }

    const BrnSound::Module::Io::RootInputBuffer::GameModeOutputInterface* lpGameMode =
        lpModule->GetBrnInputStructure()->GetGameModeInterface();
    const s32 liGameModeType = lpGameMode ? lpGameMode->miCurrentGameModeType : E_MODE_NONE;
    mbShowTime.Update(liGameModeType == E_MODE_ONLINE_SHOWTIME ||
                      liGameModeType == E_MODE_OFFLINE_SHOWTIME);

    if (mbShowTime.HasChangedTo(true))
        lpModule->GetEnvironment().GetDynamicMixer().SetSnapshot(
            BrnSound::E_SNAPSHOT_TYPE_SHOW_TIME, true);
    else if (mbShowTime.HasChangedTo(false))
        lpModule->GetEnvironment().GetDynamicMixer().SetSnapshot(
            BrnSound::E_SNAPSHOT_TYPE_SHOW_TIME, false);

    mbIsCrashing.Update(
        lpModule->GetBrnInputStructure()->GetVehicleInterface()->IsPlayerCarFatalyCrashing());

    static s32 siDiagUpdate = 0;
    if (SndEnvDiagBudget(siDiagUpdate))
        *CgsDev::Log::gpDebugPrint << "[sndenv] crash-stream update [FLAG PC witness]\n";

    if (!mbIsCrashing.HasChanged() && !mbShowTime.HasChanged())
        return;

    static s32 siDiagEdge = 0;
    if (SndEnvDiagBudget(siDiagEdge))
        *CgsDev::Log::gpDebugPrint << "[sndenv] crash-stream edge showtime="
                                   << static_cast<s32>(mbShowTime.GetCurrent())
                                   << " crashing=" << static_cast<s32>(mbIsCrashing.GetCurrent())
                                   << " [FLAG PC witness]\n";

    BrnSound::Logic::Streaming::StreamingStateManager* lpStreamingStateMgr =
        static_cast<BrnSound::Logic::Streaming::StreamingStateManager*>(
            lpModule->GetEnvironment().GetStateManager(6));
    CGS_ASSERT(lpStreamingStateMgr != nullptr, "lpStreamingStateMgr");
    lpStreamingStateMgr->PostStreamRequest(
        BrnSound::Logic::Streaming::StreamStopRequest(this, 0.0f));

    if (!mbIsCrashing.GetCurrent() && !mbShowTime.GetCurrent())
        return;

    const bool lbShowTime = mbShowTime.GetCurrent();
    mParams.mVoiceSpecName = static_cast<u32>(
        (lbShowTime ? K_MusicVoiceSpecName : K_CrashStreamVoiceSpecName).GetValue());
    mParams.miSendIndex = 0;
    mParams.mpLogicModule = mpLogicModule;
    mParams.mFactoryName = static_cast<u32>(
        CgsSound::Playback::GenericRwacFactorySkName().GetValue());
    mParams.mSlotName = static_cast<u32>(
        CgsSound::Playback::PlayerVoice::SK_PLAYER_SLOT_NAME.GetValue());
    mParams.mContentSpecName = static_cast<u32>(GetContentSpecToPlay(lbShowTime).GetValue());
    mParams.mSendName = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01"));
    mParams.mSubMixVoiceID = static_cast<u32>(mSubmix.GetIdent());
    lpStreamingStateMgr->PostStreamRequest(
        BrnSound::Logic::Streaming::StreamRequest(this, 1, 0.1f));

    static s32 siDiagPlay = 0;
    if (SndEnvDiagBudget(siDiagPlay))
        *CgsDev::Log::gpDebugPrint << "[sndenv] crash-stream play showtime="
                                   << static_cast<s32>(lbShowTime) << " content="
                                   << static_cast<u32>(mParams.mContentSpecName)
                                   << " [FLAG PC witness]\n";
}

// ---------------------------------------------------------------------------
// CrashStreamEffect::Detach
//
// After the base detach succeeds, a stop request with a 0.25 s fade-out is posted
// unconditionally (the null manager asserts, it does not skip).
// ---------------------------------------------------------------------------
bool CrashStreamEffect::Detach()
{
    if (!BrnEffectObject::Detach())
        return false;

    BrnSound::Logic::Streaming::StreamingStateManager* lpStreamingStateMan =
        static_cast<BrnSound::Logic::Streaming::StreamingStateManager*>(
            static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule)
                ->GetEnvironment().GetStateManager(6));
    CGS_ASSERT(lpStreamingStateMan != nullptr, "lpStreamingStateMan");
    lpStreamingStateMan->PostStreamRequest(
        BrnSound::Logic::Streaming::StreamStopRequest(this, 0.25f));
    return true;
}

const CgsSound::Logic::VoiceWrapper::CreateParams&
CrashStreamEffect::GetCreateParams() const
{
    return mParams;
}

// ---------------------------------------------------------------------------
// CrashStreamEffect::UpdateVoiceParams
//
// Branches on the voice spec the stream voice was created with. Both specs clear
// PauseControl (parameter 1). A show-time take (MusicVoiceSpec) sends the stream gain
// scaled by mixer output 3. A crash take (CrashStreamVoiceSpec) sends the stream gain
// unscaled and drives its gain array: Gain0 / Gain1 (parameters 2, 3) from mixer
// output 0 and Gain2 / Gain3 (parameters 4, 5) from mixer output 1.
// ---------------------------------------------------------------------------
void CrashStreamEffect::UpdateVoiceParams(CgsSound::Logic::VoiceWrapper& arVoice,
                                           f32 afGain, f32 /*afElapsedTime*/)
{
    const u32 luPauseControl = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("PauseControl"));
    const u32 luSend = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01"));

    if (arVoice.GetCreateParams().mVoiceSpecName ==
        static_cast<u32>(K_MusicVoiceSpecName.GetValue()))
    {
        arVoice.SetParameter(1, 0.0f, &luPauseControl);
        const f32 lfMixerGain = GetRWACMixerOutputValue(3, Nicotine::DMixIO::DMX_VOL);
        arVoice.SetGain(0, lfMixerGain * afGain, &luSend);
        return;
    }

    arVoice.SetParameter(1, 0.0f, &luPauseControl);
    arVoice.SetGain(0, afGain, &luSend);

    const f32 lfMixerGain1 = GetRWACMixerOutputValue(1, Nicotine::DMixIO::DMX_VOL);
    const f32 lfMixerGain0 = GetRWACMixerOutputValue(0, Nicotine::DMixIO::DMX_VOL);
    const u32 luGain0 = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Gain0"));
    const u32 luGain1 = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Gain1"));
    const u32 luGain2 = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Gain2"));
    const u32 luGain3 = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Gain3"));
    arVoice.SetParameter(2, lfMixerGain0, &luGain0);
    arVoice.SetParameter(3, lfMixerGain0, &luGain1);
    arVoice.SetParameter(4, lfMixerGain1, &luGain2);
    arVoice.SetParameter(5, lfMixerGain1, &luGain3);
}

// ---------------------------------------------------------------------------
// CrashStreamEffect::CreateObject(u32)  @ 0x826B9E50   (the RTTI factory hook)
// Allocates a 132-byte (0x84) block via CgsSound::MemBase::operator new(size, tag,
// flavour) tagged "CrashStreamEffect", placement-constructs a CrashStreamEffect, and
// returns the EffectObject base pointer (+4). The incoming u32 only selects the
// operator-new flavour (0/1). DWARF (BrnCrashStreamEffect.h:62): EffectObject*
// CreateObject(uint32_t).
// FLAG (allocator gate): CgsSound::MemBase::operator new is not homed here, so this uses
// the host `new`; observable result matches. The 0x84 size is documentation only.
// ---------------------------------------------------------------------------
CgsSound::Logic::EffectObject* CrashStreamEffect::CreateObject( u32 /*luType*/ )
{
    return new CrashStreamEffect();
}

// ---------------------------------------------------------------------------
// CrashStreamEffect::GetContentSpecToPlay(bool bShowTime) const  @ 0x8269B908
//
// Formats a rotating content-spec name ("ShowTime00".. modulo 4, or "Crash00".. modulo
// 15) from a per-variant static counter and returns the interned Name for it. The two
// counters are function-local statics, each a u16 that post-increments every call so
// successive calls cycle through the take indices.
//
// FLAG: the X360 else-branch's 0x88888889 constant is the compiler's signed-division-
// by-15 reciprocal used to compute counter % 15; the modulo is expressed directly here.
// ---------------------------------------------------------------------------
const CgsSound::Playback::Name CrashStreamEffect::GetContentSpecToPlay(bool bShowTime) const
{
    static u16 su16ShowTimeCounter; // word_8300C6C4
    static u16 su16CrashCounter;    // word_8300C6C8

    const char* lkpcFormat;
    u32         luIndex;

    if (bShowTime)
    {
        luIndex    = static_cast<u32>(su16ShowTimeCounter++) % 4u;  // ShowTime%02u, mod 4
        lkpcFormat = "ShowTime%02u";
    }
    else
    {
        luIndex    = static_cast<u32>(su16CrashCounter++) % 15u;    // Crash%02u, mod 15
        lkpcFormat = "Crash%02u";
    }

    char lacName[32]; // v24 [sp+0x50]
    CgsCore::SPrintf(lacName, 32, lkpcFormat, luIndex);

    return CgsSound::Playback::Name(CgsSound::Playback::Name::MakeHash(lacName));
}

// ---------------------------------------------------------------------------
// ~CrashStreamEffect  @ 0x826D0E98  (anchor for the X360 `vector deleting destructor').
// All observable member teardown lives in the committed BrnEffectObject / IStreamUser
// base chains + the mSubmix member destructor (the voice release), so this anchor body is
// empty; the host toolchain re-synthesises the vector deleting destructor + operator
// delete from this class's MI base list + virtual destructor. The (a2 & 1) allocator-
// free tail is left to the host toolchain (off_82FFB954 not homed here).
// ---------------------------------------------------------------------------
CrashStreamEffect::~CrashStreamEffect()
{
}

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound
