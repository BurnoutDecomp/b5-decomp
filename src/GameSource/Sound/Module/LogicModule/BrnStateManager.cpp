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
// GetTypeInfo  @ PS3 DecFIGS 0x822BF8  (`return &BrnStateManager::sTypeInfo;`)
//
// PS3 static-init (0x85FA1C): BrnStateManager::sTypeInfo = { ObjectID=-1,
// "BrnStateManager", baseTypeInfo=&StateManager::sTypeInfo, createObject=&CreateObject }.
// The 8 concrete leaves OVERRIDE this with their own descriptor; BrnStateManager's own
// version is a vtable-filler -- a bare BrnStateManager is never created (ObjectID -1, so
// CreateStateMan's 0..8 loop never matches it). Modelled as a function-local static
// descriptor (ObjectID -1, name, canonical base). FLAG: createObject elided (null) -- the
// base is never factory-instantiated, so its factory hook is unused here.
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* BrnStateManager::GetTypeInfo() const
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager> sTypeInfo(
        -1, "BrnStateManager", CgsSound::Logic::StateManager::GetStaticTypeInfo(), 0);
    return &sTypeInfo;
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
