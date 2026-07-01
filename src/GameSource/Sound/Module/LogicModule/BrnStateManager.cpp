#include "GameSource/Sound/Module/LogicModule/BrnStateManager.h"

// =============================================================================
// BrnSound::Logic::BrnStateManager — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnStateManager.h for the
// dual-base inheritance rationale.
//
// This TU's dossier attests exactly 2 functions: GetResourceRegistrar
// (X360 0x82696510) and the vector deleting destructor (X360 0x826FAB58).
// GetTypeName/CreateObject/GetStaticTypeInfo/GetTypeInfo are NOT in the X360
// ledger for this class (PS3-DecFIGS-only -- per the X360-attestation gate,
// left undeclared/undefined here; the 8 concrete leaf managers each carry
// their own real, attested versions).
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// ---------------------------------------------------------------------------
// GetResourceRegistrar  @ X360 0x82696510  (dossier name "GetResourceRegistr", truncated)
//
//   int __fastcall BrnStateManager::GetResourceRegistr(int a1)
//   {
//     return (*(*(*(a1 - 100) + 19600) + 4))(*(a1 - 100) + 19600);
//   }
//   asm:
//     lwz   r11, -0x64(r3)       ; r11 = *(this - 0x64)  == this->mpLogicModule
//     addi  r3,  r11, 0x4C90     ; r3  = module + 0x4C90 (== module->mResourceRegistrar's
//                                ;       IResourceRequester sub-object)  -- pointer ARITHMETIC,
//                                ;       not a further dereference (Hex-Rays' nested `*(...)+N`
//                                ;       notation folds "deref field, then add constant")
//     lwz   r11, 0(r3)           ; r11 = vptr
//     lwz   r11, 4(r11)          ; slot 1 (IResourceRequester: {dtor, ResourcesAreReady,
//                                ;         GetResourceRegistrar})
//     mtctr r11 ; bctr           ; tail-call GetResourceRegistrar() on it
//
// Identical shape to the sibling BrnEffectObject::GetResourceRegistrar @ 0x82696850
// (BrnEffectObject.cpp: `lwz r11,0x2C(r3); addi r3,r11,0x4C90; lwz r11,0(r3); lwz
// r11,4(r11); mtctr r11; bctr` -- the SAME +0x4C90 module-embedded-registrar
// forward and the SAME vtable-slot-1 tail-call). The only difference is the field
// offset used to load mpLogicModule: BrnEffectObject reads it directly at +0x2C
// (its `this` IS the primary object); this function's `this` (a1) is the
// IResourceRequester sub-object pointer -- the X360 this-adjustor thunk walks
// back -0x64 bytes to reach the primary BrnStateManager/StateManager object where
// mpLogicModule (CgsStateManager.h, protected, stamped by CreateStateMan) lives.
// Both name the exact same logical member; the negative X360 32-bit byte offset is
// documentation only (NOT reproduced as a host pointer-cast -- the C++ multiple-
// inheritance this-adjustment already performs the equivalent walk when this
// override is reached through the IResourceRequester base).
//
// FLAG: mpLogicModule is reconstructed as an opaque void* (the SoundLogicModule
// home's exact byte layout at +0x4C90 is not reconstructed in this view). Here we
// reinterpret the module pointer as the IResourceRequester* sub-object and
// dispatch the same virtual, mirroring BrnEffectObject::GetResourceRegistrar
// exactly.
// ---------------------------------------------------------------------------
ResourceRegistrar& BrnStateManager::GetResourceRegistrar()
{
    IResourceRequester* pModuleRequester =
        static_cast<IResourceRequester*>(mpLogicModule);
    return pModuleRequester->GetResourceRegistrar();
}

// ---------------------------------------------------------------------------
// `vector deleting destructor'  @ X360 0x826FAB58
//
//   *this = off_820B66E4;                                            ; (transient) own vtable
//   CgsSound::Logic::StateManager::RegisteredContent<4,int>::~ObjectPool(this + 0x30);
//   *this = &off_820AA820;                                           ; re-install MemBase vtable
//   if ( (flags & 1) != 0 )
//       <sound allocator>.Free(this);                                ; deleting flavour
//
// Structurally IDENTICAL to the base CgsSound::Logic::StateManager's own
// vector-deleting destructor (X360 0x826FAAB8, homed in CgsStateManagerDtor.cpp /
// CgsStateManager.cpp): same off_820B66E4 vtable store, same `+0x30 ~ObjectPool`
// call tearing down the inherited mContentPool (the already-homed
// ObjectPool<StateManager::RegisteredContent,4,int>::~ObjectPool @ 0x826EAC90,
// ObjectPool_StateManagerRegisteredContent_4.cpp), same MemBase vtable re-install,
// same conditional allocator-routed free. BrnStateManager adds no member requiring
// explicit teardown beyond the scalar miCpuMonitor, so the compiler re-derives
// this same thunk shape for BrnStateManager's own vtable slot. Per the established
// base-class precedent (StateManager::~StateManager()'s body is empty; the pool
// teardown + vtable re-installs + conditional free are the compiler's
// deleting-destructor thunk, re-synthesised from the virtual destructor + operator
// delete), this destructor's OBSERVABLE work -- destroying the inherited
// mContentPool -- happens via the implicit base-destructor chain; nothing
// additional is bodied here.
// ---------------------------------------------------------------------------
BrnStateManager::~BrnStateManager()
{
}

} // namespace Logic
} // namespace BrnSound
