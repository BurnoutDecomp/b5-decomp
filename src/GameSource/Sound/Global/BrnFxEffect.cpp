#include "GameSource/Sound/Global/BrnFxEffect.h"

// =============================================================================
// BrnSound::Logic::FxEffect - out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnFxEffect.h for the dual-base
// layout rationale, the embedded VoiceWrapper[4] array member, and the X360-32-bit
// -vs-host-64-bit offset note + the un-homed-leaf-member FLAG.
//
// This TU's recon'd function set is exactly two entries:
//   FxEffect()                                     @ 0x826C9288 (leaf constructor)
//   `vector deleting destructor'`adjustor{4}'      @ 0x826C9328 (a thin `this-4`
//        forwarder to FxEffect::`scalar deleting destructor', a SEPARATE un-homed
//        slice; compiler-synthesised, no source body -- see below).
//
// NOTE: class is BrnSound::Logic::FxEffect (X360 mangling
// `BrnSound::Logic::FxEffect::FxEffect`) -- there is NO `Fx` sub-namespace. The
// `Global/` directory is only a filesystem location, not a namespace level.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// ---------------------------------------------------------------------------
// FxEffect::FxEffect  @ 0x826C9288
//
//   stfs 0.0f, 0x20 ; stfs 0.0f, 0x1C           ; leaf f32 = 0.0f (FLAG: un-homed)
//   sth 0,0x10 ; stw 0,0xC ; stw 0,0x34 ; stw 0,8 ; stb 0,0x30 ; sth 0,0x12
//   stw 0,0x28 ; stw 0,0x24                      ; base meDetach/meAttach region = 0
//   stw off_820AE954,4 ; stw off_820B3800,0 ; stw off_820B37CC,4   ; dual-base vptrs
//   do { CgsSound::Logic::VoiceWrapper::VoiceWrapper(this + 0x38 + 0x50*i); } while(--i>=0)
//     ; r29 = 3 -> 4 iterations; 4 sub-objects constructed at +0x38/+0x88/+0xD8/+0x128.
//   stw 0,0x18C ; stb 0,0x190 ; stb 0,0x191      ; leaf words/bytes (FLAG: un-homed)
//   return this
//
// Same shape as the committed sibling ExplosionEffect::ExplosionEffect @ 0x826D5480:
// MSVC's INLINED full-object constructor installs the BrnEffectObject dual-vptr pair
// directly (primary @ this+0, IResourceRequester sub-object @ this+4, the transient
// off_820AE954 @ +4 overwritten by the final off_820B37CC) and inlines the base
// member zero-inits (meAttachState @ +0x24, meDetachState @ +0x28). In reconstructed
// C++ the vptr installs + base zero-inits are produced implicitly by the
// BrnEffectObject base sub-object's own default constructor (reused BY NAME). The raw
// vptr addresses NATURALLY DIFFER from ExplosionEffect's because each leaf class owns
// its own vtable -- what is shared is the dual-base STRUCTURE, not the addresses.
//
// FxEffect's own leaf tail differs from ExplosionEffect's: instead of one embedded
// VoiceWrapper it embeds an ARRAY of 4, constructed in the do/while loop (stride
// 0x50 = 80 bytes, base +0x38), reused BY NAME from the minimal VoiceWrapper home.
//
// FLAG (un-homed leaf members): the additional inlined leaf scalar zero-inits target
// FxEffect's OWN leaf members. Names/types are un-homed (no DWARF, no Feb-2007
// source). They are DECLARATION-ONLY (see header FLAG) and are NOT fabricated as
// named fields and NOT raw-offset-hacked here.
// ---------------------------------------------------------------------------
FxEffect::FxEffect()
    : BrnEffectObject()   // installs the base vptrs + zero-inits the base members (BY NAME)
    , mVoiceWrappers()    // tail do/while `bl CgsSound::Logic::VoiceWrapper::VoiceWrapper`
                          // x4 at this+0x38, +0x88, +0xD8, +0x128 (stride 0x50)
{
    // The remaining inlined leaf scalar zero-inits target un-homed leaf members
    // (DECLARATION-ONLY; see header FLAG). NOT fabricated here.
}

// ---------------------------------------------------------------------------
// ~FxEffect  (the leaf destructor the deleting-destructor thunks forward to).
// Its full member-teardown slice is DEFERRED; this out-of-line anchor forwards to
// the inherited BrnEffectObject base destructor chain (which tears down the embedded
// mVoiceWrappers[] array + the base members BY NAME). No fabricated teardown added.
// It exists so the class has a defined key function (the vtable emission point).
// ---------------------------------------------------------------------------
FxEffect::~FxEffect()
{
}

// ---------------------------------------------------------------------------
// `vector deleting destructor'`adjustor{4}'  @ 0x826C9328
//
//   0x826C9328  addi  r3, r3, -4
//   0x826C932C  b     BrnSound::Logic::FxEffect::`scalar deleting destructor'
//
// The compiler-synthesised multiple-inheritance adjustor thunk. A delete through an
// IResourceRequester* enters here on the IResourceRequester sub-object (which lives
// at this+4), recovers the primary FxEffect `this` (this - 4), then tail-calls the
// real (scalar) deleting destructor. The MI layout comes from the DWARF-confirmed
// base graph (FxEffect : BrnEffectObject; BrnEffectObject : EffectObject,
// IResourceRequester -> IResourceRequester sub-object vptr @ this+4). Per project
// convention this pure `this-4` MI-adjustor-then-tailcall is a compiler-synthesised
// artifact -- the host toolchain regenerates the equivalent thunk automatically from
// `virtual ~FxEffect()` plus the inheritance graph. No source body is emitted.
// (Same precedent as the committed BrnEffectObject.cpp adjustor{4} @ 0x826967E0.)
// ---------------------------------------------------------------------------

} // namespace Logic
} // namespace BrnSound
