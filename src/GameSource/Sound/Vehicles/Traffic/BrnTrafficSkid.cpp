#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficSkid.h"

// =============================================================================
// BrnSound::Logic::Traffic::TrafficSkid -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnTrafficSkid.h for the dual-base
// layout rationale (mirrors the committed BrnEffectObject sibling), the embedded
// VoiceWrapper member, and the X360-32-bit-vs-host-64-bit offset note + the
// un-homed-leaf-member FLAG.
//
// This TU's recon'd function set is exactly two entries:
//   TrafficSkid()                   @ 0x826CAFE0  (the leaf constructor)
//   `scalar deleting destructor'    @ 0x826E31F0  (compiler-synthesised; forwards to
//        the ~TrafficSkid anchor -- no separate hand-written body)
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

// ---------------------------------------------------------------------------
// TrafficSkid::TrafficSkid  @ 0x826CAFE0
//
//   stfs 0.0f, 0x20 ; stfs 0.0f, 0x1C           ; leaf f32 = 0.0f (FLAG: un-homed)
//   stw off_820AE954, 4                          ; (transient) IResourceRequester base vptr
//   sth 0,0x10 ; stw 0,0xC ; stw 0,0x34 ; stw 0,8 ; stb 0,0x30 ; sth 0,0x12
//   stw 0,0x28 ; stw 0,0x24                      ; base meDetach/meAttach region = 0
//   stw off_820B3B0C,0 ; stw off_820B3AD8,4      ; final dual-base leaf vptrs
//   bl CgsSound::Logic::VoiceWrapper::VoiceWrapper(this+0x38)   ; embedded member ctor
//   stw off_820AC1B0, 0x88                        ; RTTI/ClassTypeInfo-style ptr (FLAG: un-homed)
//   return this
//
// MSVC's INLINED full-object constructor: no `bl` to a base ctor -- it inlines the
// BrnEffectObject base member zero-init and installs the two leaf vptrs directly,
// then constructs the embedded VoiceWrapper at +0x38, then stores an additional
// pointer at +0x88. In reconstructed C++ the two vptr installs + base member zero-init
// are produced implicitly by the BrnEffectObject base sub-object's own default ctor
// (reused BY NAME, same pattern as the committed ExplosionEffect sibling); the only
// hand-written tail effect is the embedded VoiceWrapper sub-object construction.
//
// FLAG (un-homed leaf members): the additional inlined leaf scalar zero-inits and the
// +0x88 pointer store target TrafficSkid's OWN leaf members, whose names/types are
// un-homed. They are DECLARATION-ONLY (see header FLAG) and are NOT fabricated as
// named fields and NOT raw-offset-hacked here.
// ---------------------------------------------------------------------------
TrafficSkid::TrafficSkid()
    : BrnEffectObject()   // installs the base vptrs + zero-inits the base members (BY NAME)
    , mVoiceWrapper()     // tail `bl CgsSound::Logic::VoiceWrapper::VoiceWrapper(this+0x38)`
{
    // The remaining inlined leaf scalar zero-inits and the +0x88 pointer store target
    // un-homed leaf members (DECLARATION-ONLY; see header FLAG). NOT fabricated here.
}

// ---------------------------------------------------------------------------
// ~TrafficSkid  (the out-of-line anchor the scalar deleting destructor @ 0x826E31F0
// forwards to). Its observable member teardown -- the embedded VoiceWrapper dtor
// (this+0x38) plus the BrnEffectObject base settle (meDetachState=FINISHED @ +0x28,
// meAttachState=0 @ +0x24, mbResourcesReady=false @ +0x31, the two vptr re-installs)
// -- is produced by the ~BrnEffectObject base destructor chain + the embedded
// VoiceWrapper member dtor (reused BY NAME), so this leaf body adds nothing of its
// own. It exists so the class has a defined key function (the vtable emission point),
// mirroring the committed ExplosionEffect sibling's empty ~ExplosionEffect() anchor.
//
// The X360 scalar deleting destructor @ 0x826E31F0 is FULLY INLINED (its first `bl`
// is the embedded VoiceWrapper::~VoiceWrapper on this+0x38, then the base settle) and
// its (a2 & 1) tail routes the object through the global sound allocator (off_82FFB954,
// vtable slot +0x14 == Free); that allocator is not homed here, so the `delete` half
// is left to the host toolchain's operator delete -- the raw allocator vtable call is
// NOT reproduced and no allocator is fabricated.
// ---------------------------------------------------------------------------
TrafficSkid::~TrafficSkid()
{
}

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound
