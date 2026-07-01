#include "GameSource/Sound/World/BrnEmitterEffect.h"

// =============================================================================
// BrnSound::Logic::World::EmitterEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnEmitterEffect.h for the
// dual-base layout rationale, the embedded VoiceWrapper member, the
// X360-32-bit-vs-host-64-bit offset note + the un-homed-leaf-member FLAG.
//
// This is the leaf-shape sibling of BrnSound::Logic::Explosion::ExplosionEffect
// (BrnExplosionEffect.cpp @ 0x826D5480): the two constructors are store-for-store
// identical apart from the class-specific vtable constants (primary/IResourceRequester
// leaf vptrs off_820B4010/off_820B3FDC here).
//
// This TU's recon'd function set (dossier) has three entries; the constructor and the
// scalar deleting destructor are bodied here:
//   EmitterEffect()                @ 0x826D1C00  (the leaf constructor)
//   GetSoundEntity()               @ 0x82686708  (DEFERRED -- owned-state surface not homed)
//   `scalar deleting destructor'   @ 0x826E6B40  (compiler-synthesised)
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace World
{

// ---------------------------------------------------------------------------
// EmitterEffect::EmitterEffect  @ 0x826D1C00
//
//   stfs 0.0f, 0x20 ; stfs 0.0f, 0x1C           ; leaf f32 = 0.0f (FLAG: un-homed)
//   stw off_820AE954, 4                          ; (transient) IResourceRequester base vptr
//   sth 0,0x10 ; stw 0,0xC ; stw 0,0x34 ; stw 0,8 ; stb 0,0x30 ; sth 0,0x12
//   stw 0,0x28 ; stw 0,0x24                      ; base meDetach/meAttach region = 0
//   stw off_820B4010,0 ; stw off_820B3FDC,4      ; final dual-base leaf vptrs
//   bl CgsSound::Logic::VoiceWrapper::VoiceWrapper(this+0x38)   ; embedded member ctor
//   return this
//
// MSVC's INLINED full-object constructor: no `bl` to a base ctor -- it inlines the
// BrnEffectObject base member zero-init and installs the two leaf vptrs directly, then
// constructs the embedded VoiceWrapper at +0x38. In reconstructed C++ the two vptr
// installs + base member zero-init are produced implicitly by the BrnEffectObject base
// sub-object's own default ctor (reused BY NAME); the only hand-written tail effect is
// the embedded VoiceWrapper sub-object construction (matching the ExplosionEffect
// precedent).
//
// FLAG (un-homed leaf members): the additional inlined leaf scalar zero-inits target
// EmitterEffect's OWN leaf members, whose names/types are un-homed. They are
// DECLARATION-ONLY (see header FLAG) and are NOT fabricated as named fields and NOT
// raw-offset-hacked here.
// ---------------------------------------------------------------------------
EmitterEffect::EmitterEffect()
    : BrnEffectObject()   // installs the base vptrs + zero-inits the base members (BY NAME)
    , mVoiceWrapper()     // tail `bl CgsSound::Logic::VoiceWrapper::VoiceWrapper(this+0x38)`
{
    // The remaining inlined leaf scalar zero-inits target un-homed leaf members
    // (DECLARATION-ONLY; see header FLAG). NOT fabricated here.
}

// ---------------------------------------------------------------------------
// ~EmitterEffect  (the out-of-line anchor the scalar deleting destructor @ 0x826E6B40
// forwards to). Its member teardown lives in the inherited BrnEffectObject base chain
// + the embedded VoiceWrapper member (reused BY NAME), so this leaf body is empty. It
// exists so the class has a defined key function (the vtable emission point), mirroring
// the committed CollisionEffect / ExplosionEffect precedent.
//
// The X360 scalar deleting destructor @ 0x826E6B40 chains this dtor and, when bit0 of
// its flags arg is set, frees the object through the global sound MemBase allocator
// (off_82FFB954, vtable slot +0x14); that allocator is not homed here, so the `delete`
// half is left to the host toolchain's operator delete -- the raw allocator vtable
// dispatch is NOT reproduced and no allocator is fabricated.
// ---------------------------------------------------------------------------
EmitterEffect::~EmitterEffect()
{
}

} // namespace World
} // namespace Logic
} // namespace BrnSound
