#ifndef BRN_SOUND_LOGIC_WORLD_BRN_EMITTER_STATE_MANAGER_H
#define BRN_SOUND_LOGIC_WORLD_BRN_EMITTER_STATE_MANAGER_H

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"           // CgsContainers::ObjectPool
#include "GameShared/GameClasses/Sound/Logic/CgsStateManagerRegisteredContent.h" // CgsSound::Logic::StateManager(::RegisteredContent)

// =============================================================================
// BrnSound::Logic::World::EmitterStateManager
//   GameSource/Sound/Module/LogicModule/BrnEmitterStateManager.{h,cpp}
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The factory CreateObject @ 0x82701518
// allocates the object through `CgsSound::MemBase::operator new(1088, ...)`, so
// EmitterStateManager is a 1088-byte (0x440) CgsSound::Logic::StateManager descendant.
//
// The ctor @ 0x826FE290 forwards to the base CgsSound::Logic::StateManager ctor
// (CgsSound::Logic::StateManager::StateManager @ 0x826FAA18, homed in its own TU) and
// then installs this class's vtables (primary @ +0, two secondary sub-object vtables
// @ +0x90 and +0x98). The `vector deleting destructor` @ 0x826FE2E0 destructs a member
// ObjectPool<StateManager::RegisteredContent, 4, int> at +0x30 (calling its
// ~ObjectPool @ 0x826EAC90, homed in ObjectPool_StateManagerRegisteredContent_4.cpp)
// and then re-installs the shared MemBase sub-object vtable (off_820AA820) before
// routing the storage back to the sound allocator.
//
// MODELLING (ODR coexistence — conductor please note): this TU includes the
// CgsStateManagerRegisteredContent.h partial view of CgsSound::Logic::StateManager
// because it needs the nested RegisteredContent element type for the pool member. Per
// the documented one-view-per-TU rule (see CgsStateManagerRegisteredContent.h FLAG),
// the alternate scalar+IsStateAlias view (CgsStateManager.h) is NOT included here. The
// base ctor's real body lives in its own done TU; this view supplies the type so the
// derivation and the pool teardown compile by name.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 stores the pool at +0x30 and the
// secondary vtables at +0x90/+0x98 behind 4-byte pointers; on a 64-bit host those
// offsets differ, so the member is pinned BY NAME only and the 0x440 size / absolute
// offsets are NOT static_asserted. The two secondary vtables are reproduced
// structurally (the class declares virtuals), not hand-stored. FLAG: only the
// base-forwarding + pool-teardown semantics are load-bearing.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace World
{

// Sound-logic state manager specialised for world emitters. Owns a small fixed pool
// of registered content slots.
class EmitterStateManager : public CgsSound::Logic::StateManager
{
public:
    // EmitterStateManager @ 0x826FE290 — forwards to the base StateManager ctor and
    // installs this class's vtables (host-synthesised).
    EmitterStateManager();

    // The X360 `vector deleting destructor' @ 0x826FE2E0 destructs the member content
    // pool and re-installs the MemBase sub-object vtable before freeing. Virtual so
    // the teardown is reached through the vtable exactly as the X360 dispatches it.
    virtual ~EmitterStateManager();

private:
    // +0x30: the registered-content pool destructed by ~EmitterStateManager (its
    // ~ObjectPool @ 0x826EAC90 runs the per-slot RegisteredContent reference drop).
    CgsContainers::ObjectPool<CgsSound::Logic::StateManager::RegisteredContent, 4, int> maContentPool;
};

} // namespace World
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_WORLD_BRN_EMITTER_STATE_MANAGER_H
