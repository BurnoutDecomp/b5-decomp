#include "GameSource/Sound/Vehicles/Environment/BrnCarStereoEffect.h"

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
