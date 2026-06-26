#include "GameSource/Sound/Module/LogicModule/BrnStateManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (GetResourceRegistrar vtable-filler)

// =============================================================================
// BrnSound::Logic::BrnStateManager — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnStateManager.h for the
// dual-base inheritance rationale.
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
// descriptor (ObjectID -1, name, canonical base, &CreateObject). createObject is now
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
// ---------------------------------------------------------------------------
void BrnStateManager::ResourcesAreReady()
{
}

// ---------------------------------------------------------------------------
// GetResourceRegistrar  @ PS3 DecFIGS 0x82AB64  -- the faithful body forwards to the owning
// module: `return GetBrnLogicModule()->GetResourceRegistrar();` (module vtable+0x64). FLAG:
// the module back-pointer plumbing (GetBrnLogicModule / mpLogicModule) is not wired in this
// slice, and every concrete leaf overrides this -- so the base is a vtable-filler never
// reached on the boot path (PrepareStateManagersOnBoot does not call it). Mirrors the leaf
// abort-stub: assert-if-reached + a TU-local registrar to satisfy the non-void signature.
// ---------------------------------------------------------------------------
ResourceRegistrar& BrnStateManager::GetResourceRegistrar()
{
    CGS_ASSERT(false,
               "BrnStateManager::GetResourceRegistrar reached (leaves override; base is a "
               "vtable-filler -- forward via GetBrnLogicModule once the module is wired)");
    static ResourceRegistrar sUnhomedRegistrar;
    return sUnhomedRegistrar;
}

} // namespace Logic
} // namespace BrnSound
