#include "GameSource/Sound/Vehicles/Environment/BrnSpeedStreamEffect.h"
#include "GameSource/Sound/Vehicles/Environment/BrnSpeedStreamControl.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SDKs/EATech/include/Nicotine/DMixIO.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Streaming/BrnStreamingStateManager.h"
#include "GameSource/Sound/Vehicles/Environment/BrnEnvironmentSoundDiag.h"
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"
#include "GameShared/GameClasses/Sound/Playback/Module/CgsSoundPlaybackModule.h"

// =============================================================================
// BrnSound::Vehicles::Environment::SpeedStreamEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Recon'd function set:
//   Create(int)                    @ 0x826D13D8  (static allocate+construct factory)
//   SpeedStreamEffect()            @ 0x826BA148  (MSVC inlined full-object ctor)
//   `scalar deleting destructor'   @ 0x826BA1F8  (-> ~SpeedStreamEffect anchor)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

// ---------------------------------------------------------------------------
// SpeedStreamEffect::Create  @ 0x826D13D8   (static allocate+construct factory)
// Allocates a 112-byte (0x70) block via CgsSound::MemBase::operator new(size, tag,
// flavour) tagged "SpeedStreamEffect", placement-constructs a SpeedStreamEffect, and
// returns the IResourceRequester sub-object pointer (`this + 4`). `aiFlavour` only
// selects the operator-new flavour (0/1).
// FLAG (allocator gate): CgsSound::MemBase::operator new is not homed here, so this uses
// the host `new`; the +4 adjust is the static_cast to the IResourceRequester base. The
// 0x70 size is documentation only.
// ---------------------------------------------------------------------------
BrnSound::Logic::IResourceRequester* SpeedStreamEffect::Create( int aiFlavour )
{
    (void)aiFlavour; // only selected the operator-new flavour on the X360.
    SpeedStreamEffect* lpEffect = new SpeedStreamEffect();
    if ( lpEffect == nullptr )
    {
        return nullptr;
    }
    // X360 `addi r3,r3,4`: return the IResourceRequester sub-object view (this+4).
    return static_cast<BrnSound::Logic::IResourceRequester*>( lpEffect );
}

// ---------------------------------------------------------------------------
// SpeedStreamEffect::SpeedStreamEffect()  @ 0x826BA148   (MINIMAL -- see FLAGs)
//
// MSVC's INLINED full-object constructor: it does NOT `bl` a base ctor -- it inlines the
// base member zero-inits and installs THREE leaf vptrs directly (this+0 primary +
// this+4 IResourceRequester sub-object == the committed BrnEffectObject dual base;
// this+0x38 == the IStreamUser interface sub-object). In reconstructed C++ those three
// vptr installs + the base member zero-inits are produced STRUCTURALLY by the two base
// sub-objects' own default constructors (BrnEffectObject BY NAME + IStreamUser BY NAME),
// identical store-for-store to the committed siblings SpeechEffect / PresentationEffect /
// StreamingEffect. The leaf zero words and the -1 are mParams' default construction.
// ---------------------------------------------------------------------------
SpeedStreamEffect::SpeedStreamEffect()
    : BrnEffectObject()                         // primary vptr @+0 + IResourceRequester vptr @+4, base zero-init (BY NAME)
    , BrnSound::Logic::Streaming::IStreamUser() // IStreamUser interface vptr @+0x38 (BY NAME)
    , mParams()
    , mpSpeedStreamControl(nullptr)
{
}

s32 SpeedStreamEffect::GetController(s32 aiIndex)
{
    return aiIndex == 0 ? 15 : -1;
}

void SpeedStreamEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    CGS_ASSERT(apController != nullptr && apController->GetEffectID() == 15,
               "Unexpected control.");
    if (apController && apController->GetEffectID() == 15)
        mpSpeedStreamControl = static_cast<SpeedStreamControl*>(apController);
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
    CGS_ASSERT(mpSpeedStreamControl != nullptr, "mpSpeedStreamControl");

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
            CgsSound::Playback::GenericRwacFactorySkName().GetValue());
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
        CGS_ASSERT(lpStreamingStateMan != nullptr, "lpStreamingStateMan");
        lpStreamingStateMan->PostStreamRequest(
            BrnSound::Logic::Streaming::StreamRequest(this, 1, 0.1f));

        static s32 siDiagPlay = 0;
        if (SndEnvDiagBudget(siDiagPlay))
            *CgsDev::Log::gpDebugPrint << "[sndenv] speed-stream play [FLAG PC witness]\n";
    }
    else
    {
        BrnSound::Logic::Streaming::StreamingStateManager* lpStreamingStateMan =
            static_cast<BrnSound::Logic::Streaming::StreamingStateManager*>(
                lpModule->GetEnvironment().GetStateManager(6));
        CGS_ASSERT(lpStreamingStateMan != nullptr, "lpStreamingStateMan");
        lpStreamingStateMan->PostStreamRequest(
            BrnSound::Logic::Streaming::StreamStopRequest(this, 0.0f));

        static s32 siDiagStop = 0;
        if (SndEnvDiagBudget(siDiagStop))
            *CgsDev::Log::gpDebugPrint << "[sndenv] speed-stream stop [FLAG PC witness]\n";
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
    CGS_ASSERT(lpStreamingStateMan != nullptr, "lpStreamingStateMan");
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
