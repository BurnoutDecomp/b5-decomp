#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"  // the concrete module (GetResourceRegistrar dispatch)

// =============================================================================
// BrnSound::Logic::BrnEffectObject — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnEffectObject.h for the
// dual-base layout rationale and the X360-32-bit-vs-host-64-bit offset note.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

BrnEffectObject::BrnEffectObject()
    : CgsSound::Logic::EffectObject()
    , IResourceRequester()
    , mbResourceRequestActive(false)
    , mbResourcesReady(false)
{
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* BrnEffectObject::GetTypeInfo() const
{
    return CgsSound::Logic::EffectObject::GetStaticTypeInfo();
}

// ---------------------------------------------------------------------------
// GetTypeName  @ 0x82682BF8
//
//   lis   r11, off_82F2E800@ha
//   addi  r11, r11, off_82F2E800@l
//   lwz   r3,  (off_82F2E800)(r11)   ; r3 = "BrnEffectObject"
//   blr
//
// Returns the per-class RTTI type name. The X360 loads a pointer to the static
// string literal "BrnEffectObject" (the rodata at off_82F2E800 holds the
// address of that C string).
// ---------------------------------------------------------------------------
const char* BrnEffectObject::GetTypeName() const
{
    return "BrnEffectObject";
}

// ---------------------------------------------------------------------------
// GetResourceRegistrar  @ 0x82696850
//
//   lwz   r11, 0x2C(r3)        ; r11 = this->mpLogicModule
//   addi  r3,  r11, 0x4C90     ; r3  = &module->mResourceRegistrar's requester
//   lwz   r11, 0(r3)           ; r11 = vptr
//   lwz   r11, 4(r11)          ; slot 1
//   mtctr r11 ; bctr           ; tail-call GetResourceRegistrar() on it
//
// The owning SoundLogicModule embeds the BrnSound::Logic::ResourceRegistrar at
// byte +0x4C90 (DWARF: SoundLogicModule::mResourceRegistrar). The X360 tail-calls
// that sub-object's IResourceRequester virtual slot 1 (GetResourceRegistrar),
// which returns the module's registrar. We reproduce that semantically by routing
// through the module's IResourceRequester interface.
//
// HOST FORM (fixed 2026-08-25, audio-faithfulness wave 1): mpLogicModule is an
// opaque void* pointing at the PRIMARY SoundLogicModule object. The previous
// `static_cast<IResourceRequester*>(mpLogicModule)` did NO this-adjustment (the
// X360's `addi r3, r11, 0x4C90` IS that adjustment) and so dispatched through the
// module's primary vptr -- the wrong vtable. Correct host equivalent: recover the
// concrete module type; the virtual call's derived->interface conversion then
// performs the same sub-object walk the console's addi did.
// ---------------------------------------------------------------------------
ResourceRegistrar& BrnEffectObject::GetResourceRegistrar()
{
    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    return lpModule->GetResourceRegistrar();
}

// ---------------------------------------------------------------------------
// ~BrnEffectObject  @ 0x826AF4C8  (the X360 `vector deleting destructor`)
//
//   stw  off_820AE9BC, 0(r31)     ; primary vptr (EffectObject path)
//   stw  off_820AE988, 4(r31)     ; (transient) base-class IResourceRequester vptr
//   li   r7, 3 ; stw r7, 0x28(r31) ; meDetachState = E_DETACH_STATE_FINISHED
//   stw  off_820AA820, 4(r31)     ; final IResourceRequester sub-object vptr
//   stb  0, 0x31(r31)             ; mbResourcesReady = false
//   stw  0, 0x24(r31)             ; meAttachState = E_ATTACH_STATE_NONE
//   if (a2 & 1) { ... deallocate via off_82FFB954 (the MemBase allocator) }
//   return this
//
// The leading vptr stores are the compiler-emitted devirtualization of the
// destructor base sub-objects; in reconstructed C++ they are produced implicitly
// by the destructor chain, so the BODY here is the observable member teardown.
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to
// free the object; that allocator is not homed here, so operator-delete dispatch
// is left to the host toolchain (the `delete` half of the X360 vector deleting
// destructor) rather than reproducing the raw allocator vtable call.
// ---------------------------------------------------------------------------
BrnEffectObject::~BrnEffectObject()
{
    meDetachState    = CgsSound::Logic::EffectBase::E_DETACH_STATE_FINISHED;
    mbResourcesReady = false;
    meAttachState    = CgsSound::Logic::EffectBase::E_ATTACH_STATE_NONE;
}

// ---------------------------------------------------------------------------
// `vector deleting destructor' adjustor{4}  @ 0x826967E0
//
//   addi r3, r3, -4
//   b    ~BrnEffectObject (vector deleting destructor)
//
// The IResourceRequester sub-object lives at this+4; a delete through an
// IResourceRequester* enters here, recovers the primary BrnEffectObject `this`
// (this - 4), and forwards to the real destructor. In reconstructed C++ this
// thunk is generated by the compiler from the multiple-inheritance vtable
// layout (the destructor declared in the header is virtual and reached through
// the IResourceRequester base), so no hand-written body is emitted.
// FLAG: thunk reproduced structurally by the dual-base declaration in the
// header; not a hand-bodied function.
// ---------------------------------------------------------------------------

} // namespace Logic
} // namespace BrnSound
