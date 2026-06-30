#include "GameSource/Sound/Explosions/BrnExplosionEffect.h"

// =============================================================================
// BrnSound::Logic::Explosion::ExplosionEffect — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnExplosionEffect.h for the
// dual-base layout rationale, the embedded VoiceWrapper member, and the
// X360-32-bit-vs-host-64-bit offset note + the un-homed-leaf-member FLAG.
//
// This TU's recon'd function set is exactly two entries:
//   ExplosionEffect()              @ 0x826D5480  (the leaf constructor)
//   `scalar deleting destructor'   @ 0x826E8F78
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Explosion
{

// ---------------------------------------------------------------------------
// ExplosionEffect::ExplosionEffect  @ 0x826D5480
//
//   stfs f0(=flt_82001CC0, 0.0f), 0x20(r31)   ; leaf f32 = 0.0f  (FLAG: un-homed)
//   stfs f0(0.0f),               0x1C(r31)     ; leaf f32 = 0.0f  (FLAG: un-homed)
//   stw  off_820AE954,           4(r31)        ; (transient) IResourceRequester base vptr
//   sth  0,                      0x10(r31)     ; leaf s16 = 0     (FLAG: un-homed)
//   stw  0,                      0xC(r31)      ; leaf word = 0    (FLAG: un-homed)
//   stw  0,                      0x34(r31)     ; leaf word = 0    (FLAG: un-homed)
//   stw  0,                      8(r31)        ; leaf word = 0    (FLAG: un-homed)
//   stb  0,                      0x30(r31)     ; leaf byte = 0    (FLAG: un-homed)
//   stw  0,                      0x28(r31)     ; meDetachState region = 0
//   stw  0,                      0x24(r31)     ; meAttachState region = 0
//   sth  0,                      0x12(r31)     ; leaf s16 = 0     (FLAG: un-homed)
//   stw  off_820B4618,           0(r31)        ; primary leaf vptr (this+0)
//   stw  off_820B45E4,           4(r31)        ; IResourceRequester sub-object vptr (this+4)
//   bl   CgsSound::Logic::VoiceWrapper::VoiceWrapper(r31+0x38)   ; embedded member ctor
//   return this
//
// This is MSVC's INLINED full-object constructor: it does NOT `bl` a base
// constructor -- it inlines the BrnEffectObject base member zero-init and installs
// the two leaf vptrs directly, then constructs the embedded VoiceWrapper at +0x38.
// In reconstructed C++ the two vptr installs and the base member zero-init are
// produced implicitly by the BrnEffectObject base sub-object's own default
// constructor (reused BY NAME); the only hand-written tail effect is the embedded
// VoiceWrapper sub-object construction, here expressed as the member's default
// construction in the initialiser list (matching the ctor's tail `bl`).
//
// FLAG (un-homed leaf members): the additional inlined leaf scalar zero-inits
// (the two f32 at +0x1C/+0x20, the two s16 at +0x10/+0x12, and the word/byte
// fields at +0x08/+0x0C/+0x30/+0x34) target ExplosionEffect's OWN leaf members,
// whose names/types are un-homed (no DWARF, no Feb-2007 source). They are
// DECLARATION-ONLY (see header FLAG) and are NOT fabricated as named fields and
// NOT raw-offset-hacked here; only the base + VoiceWrapper construction is bodied.
// ---------------------------------------------------------------------------
ExplosionEffect::ExplosionEffect()
    : BrnEffectObject()   // installs the base vptrs + zero-inits the base members (BY NAME)
    , mVoiceWrapper()     // tail `bl CgsSound::Logic::VoiceWrapper::VoiceWrapper(this+0x38)`
{
    // The remaining inlined leaf scalar zero-inits target un-homed leaf members
    // (DECLARATION-ONLY; see header FLAG). NOT fabricated here.
}

// ---------------------------------------------------------------------------
// ~ExplosionEffect  (the leaf non-virtual destructor the scalar deleting
// destructor @ 0x826E8F78 forwards to)
//
// The X360 scalar deleting destructor's first effect is `bl ~ExplosionEffect`.
// That leaf destructor is a SEPARATE un-homed recon slice (its own body is DEFERRED);
// the observable teardown lives in it (and in the BrnEffectObject base, reused BY
// NAME). This anchor body is therefore empty -- it forwards to the base destructor
// chain (which tears down the embedded VoiceWrapper member + the BrnEffectObject
// base members BY NAME). No fabricated teardown is added.
// ---------------------------------------------------------------------------
ExplosionEffect::~ExplosionEffect()
{
}

// ---------------------------------------------------------------------------
// `scalar deleting destructor'  @ 0x826E8F78
//
//   mr   r31, r3 ; mr r30, r4
//   bl   ~ExplosionEffect                ; run the (deferred) leaf destructor
//   if (r30 & 1) {                       ; deleting flavour
//       memset(&v5[1], 0, 16) ; v5[0] = this
//       (*(*off_82FFB954 + 0x14))(off_82FFB954, v5)   ; sound allocator Free(this)
//   }
//   return this
//
// The single observable source-level side effect is the ~ExplosionEffect() call
// (forwarded to BY NAME above). The (r30 & 1) tail routes the object through the
// global sound allocator (off_82FFB954, vtable slot +0x14 == Free); that allocator
// is not homed in this group, so MSVC's deleting-destructor thunk -- the vtable
// installs and the allocator-routed free -- is re-emitted by the host toolchain
// from this class's virtual destructor + operator delete (the `delete` half of the
// X360 scalar deleting destructor). off_82FFB954's raw allocator vtable call is NOT
// reproduced; no fabricated allocator is added (same treatment as the committed
// BrnEffectObject / BrnEffectControl / GlobalState sibling homes).
// ---------------------------------------------------------------------------

} // namespace Explosion
} // namespace Logic
} // namespace BrnSound
