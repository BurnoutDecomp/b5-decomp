#include "GameSource/Sound/Vehicles/Environment/BrnSpeedStreamEffect.h"
#include "GameSource/Sound/Vehicles/Environment/BrnSpeedStreamControl.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SDKs/EATech/include/Nicotine/DMixIO.hpp"

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
// StreamingEffect.
//
// FLAG (deferred DWARF members): the leaf region +0x3C..+0x68 (all-zero words plus the
// `stw -1, +0x68`) is the default-construction of mParams -- DWARF-named
// CgsSound::Logic::VoiceWrapper::CreateParams (un-homed) -- and mpSpeedStreamControl
// (SpeedStreamControl*) is un-touched here. Both are DECLARATION-ONLY / DEFERRED per the
// anti-fabrication rule and the committed SpeechEffect / StreamingEffect minimal-home
// convention. NO standalone sentinel is invented for the `stw -1`.
// ---------------------------------------------------------------------------
SpeedStreamEffect::SpeedStreamEffect()
    : BrnEffectObject()                         // primary vptr @+0 + IResourceRequester vptr @+4, base zero-init (BY NAME)
    , BrnSound::Logic::Streaming::IStreamUser() // IStreamUser interface vptr @+0x38 (BY NAME)
    , mParams()
    , mpSpeedStreamControl(nullptr)
{
    // The leaf region +0x3C..+0x68 (including the `stw -1, +0x68`) is the deferred
    // mParams (VoiceWrapper::CreateParams) sub-object default-init, and mpSpeedStreamControl
    // is DECLARATION-ONLY / DEFERRED (see FLAGs) -- NOT fabricated here.
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
