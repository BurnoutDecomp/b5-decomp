#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficHorn.h"

// =============================================================================
// BrnSound::Logic::Traffic::TrafficHorn -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnTrafficHorn.h for the
// dual-base layout rationale, the embedded VoiceWrapper member (+0x38), the
// X360-32-bit-vs-host-64-bit offset note, and the un-homed-leaf-member FLAG.
//
// This TU's recon'd function set is exactly two entries:
//   TrafficHorn()                  @ 0x826CAEC0  (the leaf constructor)
//   `scalar deleting destructor'   @ 0x826E3108  (compiler-synthesised; forwards to
//        the ~TrafficHorn anchor below -- no separate hand-written body)
// It mirrors the committed sibling ExplosionEffect (same BrnEffectObject dual base +
// embedded-VoiceWrapper-at-+0x38 shape); the ONLY divergence is a TrafficHorn-
// specific leaf vptr/table store at +0x88 (off_820AC1AC), left un-attributed (FLAG)
// -- it is the vptr of an un-homed member sub-object (DWARF names mHornFunctionPointer,
// a VoiceWrapper::FunctorPointer<TrafficHorn>, whose home/vtable is DEFERRED).
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

// ---------------------------------------------------------------------------
// TrafficHorn::TrafficHorn  @ 0x826CAEC0
//
//   stfs 0.0f, 0x20 ; stfs 0.0f, 0x1C           ; leaf f32 = 0.0f (FLAG: un-homed)
//   stw off_820AE954, 4                          ; (transient) IResourceRequester base vptr
//   sth 0,0x10 ; stw 0,0xC ; stw 0,0x34 ; stw 0,8 ; stb 0,0x30 ; sth 0,0x12
//   stw 0,0x28 ; stw 0,0x24                      ; base meDetach/meAttach region = 0
//   stw off_820B3AD0,0 ; stw off_820B3A9C,4      ; final dual-base leaf vptrs
//   bl CgsSound::Logic::VoiceWrapper::VoiceWrapper(this+0x38)   ; embedded mHornVoice ctor
//   stw off_820AC1AC, 0x88                        ; FLAG: un-homed leaf member vptr @ +0x88
//   return this
//
// MSVC's INLINED full-object constructor: no `bl` to a base ctor -- it inlines the
// BrnEffectObject base member zero-init and installs the two leaf vptrs directly,
// then constructs the embedded VoiceWrapper at +0x38. In reconstructed C++ the two
// vptr installs + base member zero-init are produced implicitly by the committed
// BrnEffectObject base sub-object's own default ctor (reused BY NAME); the only
// hand-written tail effect is the embedded mHornVoice sub-object construction (its
// member default-construction in the init list, matching the ctor's tail `bl`).
//
// FLAG (un-homed leaf members): the additional inlined leaf scalar zero-inits and the
// vptr store @ +0x88 target TrafficHorn's OWN leaf members. DWARF attests the member
// SET but NOT a byte-offset layout map, so these stores are not attributed to named
// members with confidence. Per anti-fabrication they are DECLARATION-ONLY (deferred
// to the full layout/RTTI recon slice), NOT invented as named fields and NOT
// raw-offset-hacked here.
// ---------------------------------------------------------------------------
TrafficHorn::TrafficHorn()
    : BrnEffectObject()   // installs the base dual vptrs + zero-inits base members (BY NAME)
    , mHornVoice()        // tail `bl CgsSound::Logic::VoiceWrapper::VoiceWrapper(this+0x38)`
{
    // The remaining inlined leaf scalar zero-inits (+0x08/+0x0C/+0x10/+0x12/
    // +0x1C/+0x20/+0x30/+0x34) and the +0x88 vptr/table-ptr store target un-homed
    // leaf members (DECLARATION-ONLY; see header FLAG). NOT fabricated here.
}

// ---------------------------------------------------------------------------
// ~TrafficHorn  (the leaf destructor the scalar deleting destructor @ 0x826E3108
// forwards to). Its own member-teardown slice is DEFERRED; this out-of-line anchor
// forwards to the base destructor chain (which tears down the embedded mHornVoice
// member + the BrnEffectObject base members BY NAME). No fabricated teardown added.
// It exists so the class has a defined key function (the vtable emission point).
// ---------------------------------------------------------------------------
TrafficHorn::~TrafficHorn()
{
}

// ---------------------------------------------------------------------------
// `scalar deleting destructor'  @ 0x826E3108
//
//   mr   r31, r3 ; mr r30, r4
//   bl   ~TrafficHorn                    ; run the (deferred) leaf destructor
//   if (r30 & 1) {                       ; deleting flavour
//       memset(&v5[1], 0, 16) ; v5[0] = this
//       (*(*off_82FFB954 + 0x14))(off_82FFB954, v5)   ; global sound allocator Free(this)
//   }
//   return this
//
// The single observable source-level side effect is the ~TrafficHorn() call
// (forwarded to BY NAME above). The (r30 & 1) tail routes the object through the
// global sound allocator (off_82FFB954, vtable slot +0x14 == Free); that allocator
// is not homed here, so MSVC's deleting-destructor thunk is re-emitted by the host
// toolchain from this class's virtual destructor + operator delete. No fabricated
// allocator is added (same treatment as the committed ExplosionEffect sibling).
// ---------------------------------------------------------------------------

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound
