#ifndef BRN_SOUND_LOGIC_WORLD_BRN_EMITTER_STATE_MANAGER_H
#define BRN_SOUND_LOGIC_WORLD_BRN_EMITTER_STATE_MANAGER_H

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/Sound/Module/LogicModule/BrnStateManager.h"   // BrnSound::Logic::BrnStateManager (canonical base) -> CgsSound::Logic::StateManager + IResourceRequester
#include "SharedClasses/Sound/World/BrnSoundWorldScene.h"

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
// @ +0x90 and +0x98). The `vector deleting destructor` @ 0x826FE2E0 destructs the base
// ObjectPool<StateManager::RegisteredContent, 4, int> at +0x30 (calling its
// ~ObjectPool @ 0x826EAC90) and then re-installs the shared MemBase sub-object vtable
// (off_820AA820) before routing the storage back to the sound allocator.
//
// ODR FOLD (2026-06-25): Emitter previously derived CgsSound::Logic::StateManager
// DIRECTLY via the CgsStateManagerRegisteredContent.h partial view (which declares a
// SECOND, RegisteredContent-only CgsSound::Logic::StateManager) and carried its OWN
// ObjectPool<RegisteredContent,4> member + its OWN minimal ClassTypeInfo template.
// To collapse the StateManager ODR split, Emitter is reconciled to the SAME canonical
// base the other eight managers use: it now derives BrnSound::Logic::BrnStateManager
// (-> the FULL CgsSound::Logic::StateManager from CgsStateManager.h, which already
// owns the +0x30 RegisteredContent content pool, AND BrnSound::Logic::
// IResourceRequester). Consequences of the fold:
//   * the local ClassTypeInfo template is DELETED (the canonical
//     CgsSound::Logic::ClassTypeInfo from CgsStateManager.h is used);
//   * the local maContentPool member is DROPPED (the canonical base already owns the
//     content pool -- a second pool here would have been a non-faithful duplicate);
//   * EmitterStateManager now ALSO inherits the IResourceRequester sub-object the
//     X360 ctor installs at +0x90/+0x98 (the third sub-object vtable @ +0x98 is still
//     not separately modelled -- see FLAG), so it must override the two
//     IResourceRequester pure virtuals to stay CONCRETE (its CreateObject `new`s one).
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 stores the pool at +0x30 and the
// secondary vtables at +0x90/+0x98 behind 4-byte pointers; on a 64-bit host those
// offsets differ, so all members are pinned BY NAME only and the 0x440 size / absolute
// offsets are NOT static_asserted. The secondary vtables are reproduced structurally
// (the class declares virtuals / inherits the second base), not hand-stored. FLAG:
// only the base-forwarding + pool-teardown semantics are load-bearing.
//
// FLAG (third sub-object vtable @ +0x98 not separately modelled): the X360 ctor
// installs a third vtable at +0x98 beyond the primary (+0) and IResourceRequester
// (+0x90). That extra secondary base sub-object is not identified in this slice; the
// shell derives only BrnStateManager (primary StateManager + IResourceRequester), so
// the +0x98 base is absent. Boot (Prepare) does not exercise it.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace World
{

// Sound-logic state manager specialised for world emitters. Derives the canonical
// BrnStateManager (CgsSound::Logic::StateManager primary base -- which owns the
// RegisteredContent content pool -- + BrnSound::Logic::IResourceRequester sub-object),
// the same base the eight sibling managers use.
class EmitterStateManager : public BrnSound::Logic::BrnStateManager
{
public:
    // EmitterStateManager @ 0x826FE290 — forwards to the base ctor chain and
    // installs this class's vtables (host-synthesised).
    EmitterStateManager();

    // The X360 `vector deleting destructor' @ 0x826FE2E0 destructs the base content
    // pool and re-installs the MemBase sub-object vtable before freeing. Virtual so
    // the teardown is reached through the vtable exactly as the X360 dispatches it.
    virtual ~EmitterStateManager();

    // ---- RTTI hooks + factory (grown so EmitterStateManager is a registrable leaf the
    // StateManager factory CreateStateMan @ 0x826A5B60 can construct). STATIC
    // GetStaticTypeInfo / CreateObject so &CreateObject is storable in
    // ClassTypeInfo<StateManager>::createObject (the X360 CreateObject @ 0x82701518
    // never touches an instance -- its int arg is the operator-new flavour selector,
    // not `this`). ----
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;                                            // @ 0x826867B8
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetStaticTypeInfo();
    static CgsSound::Logic::StateManager* CreateObject( u32 luType );                   // @ 0x82701518

    // @ 0x826F5AC0 (vtable +0x0C). The per-manager boot bring-up state machine.
    virtual bool Prepare();
    virtual void UpdateParams(f32 lfDeltaTime) override;
    virtual CgsSound::Logic::State* GetFreeState(void* lpvAttachment) override;

    // ---- IResourceRequester overrides (pure in IResourceRequester; BrnStateManager
    // declares but does not body them, so the concrete leaf must override+body them to
    // be a CONCRETE (constructible) type -- required because CreateObject `new`s an
    // EmitterStateManager). ----
    virtual void                            ResourcesAreReady();    // @ 0x826867C8 (stub -- world/Content cascade)
    virtual BrnSound::Logic::ResourceRegistrar& GetResourceRegistrar(); // (stub -- module not homed)

private:
    BrnSound::Logic::World::SoundWorldScene mSoundScene;
    Vector3 mCameraPos;
};

} // namespace World
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_WORLD_BRN_EMITTER_STATE_MANAGER_H
