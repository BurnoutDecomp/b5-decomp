#include "GameSource/Sound/Vehicles/Traffic/BrnTrafficEngine.h"

// =============================================================================
// BrnSound::Logic::Traffic::Traffic3DControl — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnTrafficEngine.h for the
// inheritance rationale and the X360-32-bit-vs-host-64-bit offset note.
//
// This TU's recon'd function set is exactly ONE entry:
//   `vector deleting destructor'  @ 0x826E3000
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

// ---------------------------------------------------------------------------
// ~Traffic3DControl  @ 0x826E3000  (the X360 `vector deleting destructor')
//
//   bl   Attrib::Instance::~Instance(this + 0xB0)  ; destroy mEngineDataAtrib
//   li   r9, 3 ; stw r9, 0x24(this)                ; meDetachState = FINISHED
//   stw  &off_820AA820, 0(this)                    ; primary vptr settle
//   stb  0, 0x2D(this)                             ; control bookkeeping flag = false
//   stw  0, 0x20(this)                             ; mfDeltaTime = 0
//   if (a2 & 1) { deallocate via off_82FFB954 (the global sound allocator) }
//   return this
//
// The Attrib::Instance member teardown (mEngineDataAtrib) is produced by the
// inherited ~Brn3DEffectControl destructor chain, which destroys the member via
// the committed Attrib::Instance::~Instance. The remaining stores settle members
// owned by the inherited bases, so this leaf destructor body adds nothing of its
// own (identical in shape to the committed Passby3DControl destructor).
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to
// free the object; that allocator is not homed here, so operator-delete dispatch
// is left to the host toolchain (the `delete` half of the X360 vector deleting
// destructor) rather than reproducing the raw allocator vtable call.
// ---------------------------------------------------------------------------
Traffic3DControl::~Traffic3DControl()
{
}

// ---------------------------------------------------------------------------
// TrafficEngine::TrafficEngine  @ 0x826C9160  (the leaf constructor)
//
//   ... base leaf zero-inits (+0x08..+0x34, un-homed BrnEffectObject members) ...
//   stw  off_820B37C4, 0(r31)   ; primary leaf vptr (this+0)      -- final
//   stw  off_820B3790, 4(r31)   ; IResourceRequester vptr (this+4) -- final
//   sth  0, 0x38(r31)           ; mu16AemsPatchTrafficIndex = 0   (DWARF h:94, FIRST own member)
//   stw  0, 0x3C(r31)           ; mpTrafficControl   = nullptr    (DWARF h:95)
//   stw  0, 0x40(r31)           ; mpTraffic3DControl = nullptr    (DWARF h:96)
//   bl   CgsSound::Logic::VoiceWrapper::VoiceWrapper(r31+0x44) ; mTrafficEngineVoice ctor (DWARF h:97)
//   return this
//
// This is MSVC's INLINED full-object constructor (same shape as the committed
// ExplosionEffect::ExplosionEffect @ 0x826D5480 sibling): it does NOT `bl` a base
// constructor -- it inlines the BrnEffectObject dual-base member zero-init and
// installs the two leaf vptrs directly (off_820B37C4 @ this+0, off_820B3790 @
// this+4), then constructs the embedded VoiceWrapper member at +0x44. DWARF
// (references/DecFIGS/dwarfdump/.../BrnTrafficEngine.h:42) attests
// TrafficEngine : public BrnEffectObject with the four own members reconstructed
// BY NAME below. The base's inlined leaf scalar zero-inits (+0x08..+0x34) are
// un-homed BrnEffectObject-base members produced by the base default ctor (reused
// BY NAME) -- NOT TrafficEngine members and NOT fabricated here.
// ---------------------------------------------------------------------------
TrafficEngine::TrafficEngine()
    : BrnEffectObject()              // installs the two base vptrs + zero-inits the base members (BY NAME)
    , mu16AemsPatchTrafficIndex(0)   // DWARF BrnTrafficEngine.h:94; X360 sth 0, +0x38(r31)
    , mpTrafficControl(nullptr)      // DWARF BrnTrafficEngine.h:95; X360 stw 0, +0x3C(r31)
    , mpTraffic3DControl(nullptr)    // DWARF BrnTrafficEngine.h:96; X360 stw 0, +0x40(r31)
    , mTrafficEngineVoice()          // DWARF BrnTrafficEngine.h:97; X360 bl VoiceWrapper::VoiceWrapper(this+0x44)
{
}

// ---------------------------------------------------------------------------
// ~TrafficEngine   (anchors the X360 `vector deleting destructor' @ 0x826E18B8)
//
//   bl   TrafficEngine::~TrafficEngine(this)             ; run the scalar dtor
//   if (a2 & 1) { <sound allocator>.Free(this) }         ; deleting flavour (off_82FFB954 slot +0x14)
//   return this
//
// The X360 vector-deleting-destructor @ 0x826E18B8 is MSVC's compiler-emitted thunk
// (call the scalar dtor, then conditionally free through the global sound allocator).
// Per this corpus's convention (identical to the sibling Traffic3DControl vdtor
// @ 0x826E3000 already in this file, and every *Effect/*State sibling), the thunk is
// NOT hand-bodied: declaring `virtual ~TrafficEngine();` and giving it this empty
// out-of-line body is sufficient -- the host toolchain synthesises the dtor-call +
// conditional `delete`. The (a2 & 1) free through off_82FFB954 is left to the host
// operator delete (that allocator is not homed in this group). The scalar dtor adds
// no teardown of its own here (the base settle + embedded VoiceWrapper destruction
// is compiler-synthesised from the virtual dtor + the base ~BrnEffectObject).
// ---------------------------------------------------------------------------
TrafficEngine::~TrafficEngine()
{
}

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound
