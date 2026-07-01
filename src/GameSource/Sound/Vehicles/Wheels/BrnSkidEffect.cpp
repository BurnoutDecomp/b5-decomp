#include "GameSource/Sound/Vehicles/Wheels/BrnSkidEffect.h"

// =============================================================================
// BrnSound::Vehicles::Wheels::SkidEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnSkidEffect.h for the dual-base
// layout rationale (mirrors the committed BrnEffectObject sibling), the embedded
// VoiceWrapper member (mSkidsVoice @ +0x54), and the un-homed-leaf-member FLAG.
// This slice's recon'd function is the leaf constructor SkidEffect() @ 0x826C8C78.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Wheels
{

// ---------------------------------------------------------------------------
// SkidEffect::SkidEffect  @ 0x826C8C78  (the leaf constructor)
//
// MSVC's INLINED full-object constructor: no `bl` to a base ctor -- it inlines the
// BrnEffectObject base member zero-init and installs the two leaf vptrs directly, then
// constructs the embedded VoiceWrapper (mSkidsVoice @ +0x54) and finally binds the
// embedded FunctorPointer (mSkidFunctorPointer @ +0xA8) to &SkidEffect::OnPostInit +
// `this`. In reconstructed C++ the two vptr installs + base member zero-init are
// produced implicitly by the BrnEffectObject base sub-object's own default ctor (reused
// BY NAME, same pattern as the committed ExplosionEffect / TrafficSkid / TrafficHorn
// siblings); the only bodied tail effect is the embedded VoiceWrapper construction.
//
// FLAG (un-homed leaf members): DWARF (BrnSkidEffect.h) NAMES SkidEffect's leaf members
// (mDataHandle / mpWheelControl / mpPhysicsControl / mfOverallMax / mfSkidAzimuth /
// mbSkidsLatched / mSkidFunctorPointer @ +0xA8), but their TYPES are UN-HOMED. Per the
// anti-fabrication rule those members are DECLARATION-ONLY / DEFERRED (see header FLAG)
// and are NOT emitted as real members. The +0xA8/+0xB0/+0xB8 FunctorPointer binding and
// the +0x08..+0x51 leaf zero-inits therefore have no bodied counterpart beyond the base
// + mSkidsVoice construction.
// ---------------------------------------------------------------------------
SkidEffect::SkidEffect()
    : BrnEffectObject()   // installs the base vptrs + zero-inits the base members (BY NAME)
    , mSkidsVoice()       // `bl CgsSound::Logic::VoiceWrapper::VoiceWrapper(this+0x54)`
{
    // The remaining inlined leaf zero-inits (+0x08..+0x51) and the mSkidFunctorPointer
    // (@ +0xA8) binding of &SkidEffect::OnPostInit to `this` target un-homed-type leaf
    // members (DECLARATION-ONLY; see header FLAG). NOT fabricated here.
}

// ---------------------------------------------------------------------------
// ~SkidEffect  (DWARF BrnSkidEffect.cpp:60). Out-of-line anchor / vtable emission
// point. The observable teardown is the inherited BrnEffectObject settle + the
// embedded mSkidsVoice dtor (compiler-synthesised); this leaf body adds nothing.
// ---------------------------------------------------------------------------
SkidEffect::~SkidEffect()
{
}

} // namespace Wheels
} // namespace Vehicles
} // namespace BrnSound
