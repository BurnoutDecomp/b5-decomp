#include "GameSource/Sound/Module/LogicModule/BrnStateManager.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"  // the concrete module (GetResourceRegistrar dispatch)

// =============================================================================
// BrnSound::Logic::BrnStateManager — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnStateManager.h for the
// dual-base inheritance rationale.
//
// This TU's dossier attests GetResourceRegistrar (X360 0x82696510) and the vector
// deleting destructor (X360 0x826FAB58). GetTypeName (X360 0x82682AB8), CreateObject
// (X360 0x826FB5F0) and GetStaticTypeInfo/GetTypeInfo (PS3 DecFIGS 0x822BF8, the
// sTypeInfo descriptor) are ALSO bodied below -- the header declares them and the
// 8 concrete leaf managers' vtables reference the base versions, so they are
// link-required. (RESTORED 2026-07-01: a prior wave dropped these bodies while
// keeping the header declarations, leaving 9 TUs with unresolved externals.)
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// ---------------------------------------------------------------------------
// GetTypeName  @ 0x82682AB8
//
//   lis   r11, off_82F2E7F0@ha
//   addi  r11, r11, off_82F2E7F0@l
//   lwz   r3,  (off_82F2E7F0)(r11)   ; r3 = "BrnStateManager"
//   blr
//
// Returns the per-class RTTI type name. The X360 loads a pointer to the static
// string literal "BrnStateManager" (the rodata at off_82F2E7F0 holds the
// address of that C string).
// ---------------------------------------------------------------------------
const char* BrnStateManager::GetTypeName() const
{
    return "BrnStateManager";
}

// ---------------------------------------------------------------------------
// CreateObject  @ X360 0x826FB5F0  (PS3 sTypeInfo.createObject = &CreateObject)
//
// The descriptor's factory hook: X360 allocates a 152-byte StateManager via
// CgsSound::MemBase::operator new (the int arg is the operator-new flavour
// selector, NOT `this` -- the function never reads an instance), runs the
// StateManager ctor, then patches the BrnStateManager vptrs. The C++ `new`
// expression performs exactly that sequence (allocate + ctor + vptr setup), so
// the faithful PC equivalent is `new BrnStateManager`. STATIC class function so
// its address is storable as the descriptor's free-function createObject pointer.
// ---------------------------------------------------------------------------
CgsSound::Logic::StateManager* BrnStateManager::CreateObject( u32 /*luType*/ )
{
    return new BrnStateManager();
}

// ---------------------------------------------------------------------------
// GetStaticTypeInfo / GetTypeInfo  @ PS3 DecFIGS 0x822BF8  (`return &sTypeInfo;`)
//
// PS3 static-init (0x85FA1C): BrnStateManager::sTypeInfo = { ObjectID=-1,
// "BrnStateManager", baseTypeInfo=&StateManager::sTypeInfo, createObject=&CreateObject }.
// The 8 concrete leaves OVERRIDE this with their own descriptor; BrnStateManager's own
// version is a vtable-filler -- a bare BrnStateManager is never created (ObjectID -1, so
// CreateStateMan's 0..8 loop never matches it). Modelled as a function-local static
// descriptor (ObjectID -1, name, canonical base, &CreateObject). createObject is
// wired to the real factory (X360 0x826FB5F0 confirms the body), matching the PS3
// descriptor field.
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* BrnStateManager::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager> sTypeInfo(
        -1, "BrnStateManager", CgsSound::Logic::StateManager::GetStaticTypeInfo(), &BrnStateManager::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* BrnStateManager::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

// ---------------------------------------------------------------------------
// ResourcesAreReady  @ PS3 DecFIGS 0x8E8D24  -- empty (no-op). The leaves override with
// their real resource-ready callbacks; the base does nothing (faithful to PS3).
// (RESTORED with the RTTI bodies above -- same dropped-bodies regression.)
// ---------------------------------------------------------------------------
void BrnStateManager::ResourcesAreReady()
{
}

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
// HOST FORM (fixed 2026-08-25, audio-faithfulness wave 1): mpLogicModule is stored
// as an opaque void* pointing at the PRIMARY SoundLogicModule object. The previous
// revision `static_cast<IResourceRequester*>(mpLogicModule)` performed NO
// this-adjustment on the void* (the X360's `addi r3, r11, 0x4C90` IS that
// adjustment), so the virtual dispatched through the module's primary vptr -- the
// wrong vtable. Correct host equivalent: recover the concrete module type first;
// the derived->interface conversion in the virtual call then performs the same
// sub-object walk the console's addi did.
// ---------------------------------------------------------------------------
ResourceRegistrar& BrnStateManager::GetResourceRegistrar()
{
    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    return lpModule->GetResourceRegistrar();
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
