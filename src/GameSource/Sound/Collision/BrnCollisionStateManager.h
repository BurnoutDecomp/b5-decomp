#ifndef BRN_SOUND_LOGIC_COLLISION_COLLISION_STATE_MANAGER_H
#define BRN_SOUND_LOGIC_COLLISION_COLLISION_STATE_MANAGER_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnStateManager.h"   // BrnSound::Logic::BrnStateManager (committed base)

// =============================================================================
// BrnSound::Logic::Collision::CollisionStateManager
//   GameSource/Sound/Collision/BrnCollisionStateManager.{h,cpp}
//   (canonical home -- derived from the X360 mangled name
//    BrnSound::Logic::Collision::CollisionStateManager; the Sound/Collision/ dir
//    already exists in-tree and hosts the sibling collision-audio classes --
//    BrnCollisionDataStructures, BrnCollisionFrameInformation, BrnHingeStateCache.)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// CollisionStateManager is the sound-logic state manager that owns the collision /
// crash audio -- BY FAR the largest of the 9 managers (33408 bytes / 0x8280). It
// builds the per-material collision-generator lists, the crash-bin attribute tables,
// and the collision-event splicer banks, then drives the crash voices each frame. It
// is one of the 9 managers the SoundLogicModule factory CreateStateManagers
// (0x826AFEF8) creates via CreateStateMan.
//
// BASE CHAIN: CollisionStateManager : public BrnSound::Logic::BrnStateManager
//   (-> CgsSound::Logic::StateManager primary base + BrnSound::Logic::
//    IResourceRequester sub-object). Evidence: the ctor @ 0x826FFAC0 installs a
//   primary vtable @ +0 (off_820B844C) AND a secondary sub-object vtable @ +0x90
//   (*(a1+144) = off_820B8444, after a transient off_820AB608) -- the
//   IResourceRequester sub-object vptr -- and the dtor @ 0x826FFD48 tears down the
//   base CgsSound::Logic::StateManager::RegisteredContent ObjectPool at +0xC. Same
//   shape as the committed siblings AIVehicleStateManager / PassbyStateManager.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 object is 33408 bytes (0x8280)
// behind 4-byte pointers/vptrs (CreateObject @ 0x82701FA8 allocates 33408); on the
// 64-bit host the layout differs, so members are pinned BY NAME only and the 0x8280
// size / absolute offsets are NOT static_asserted. No recovered body in this slice
// names an individual data member by a recovered field name; the ~33KB of collision-
// audio state is deferred (see FLAG) and a single opaque pad honestly names it.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

class CollisionStateManager : public BrnSound::Logic::BrnStateManager
{
public:
    // CollisionStateManager @ 0x826FFAC0 (HEAVY -- builds collision generator lists +
    // crash-bin attribute tables; this shell does a MINIMAL ctor, see .cpp FLAG).
    CollisionStateManager();

    // ~CollisionStateManager @ 0x826FFD48 (the X360 `vector deleting destructor`).
    virtual ~CollisionStateManager();

    // ---- RTTI hooks (the per-class descriptor + factory). STATIC GetStaticTypeInfo
    // / CreateObject so &CreateObject is storable in ClassTypeInfo<StateManager>::
    // mpfnCreateObject (the X360 CreateObject @ 0x82701FA8 never touches an instance
    // -- its int arg is the operator-new flavour selector, not `this`). ----
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetStaticTypeInfo();
    static CgsSound::Logic::StateManager* CreateObject( u32 luType );                     // @ 0x82701FA8

    // ---- boot + lifecycle virtuals ----
    virtual bool Prepare();                       // @ 0x826F8B78  (vtable +0x0C; stub -- see .cpp FLAG)

    // ---- IResourceRequester overrides (pure in IResourceRequester; BrnStateManager
    // declares but does not body them, so the concrete leaf must override+body them). ----
    virtual void                            ResourcesAreReady();    // (stub -- domain cascade; see .cpp FLAG)
    virtual BrnSound::Logic::ResourceRegistrar& GetResourceRegistrar(); // (stub -- module not homed; see .cpp FLAG)

private:
    // FLAG (deferred body -- ~33KB; the WHOLE collision domain): the X360 object is
    // 33408 bytes (0x8280). The collision-audio state -- a SelectionHistory<512>
    // (+0x8B0), a 32-entry table (+0x1300, stride 28), a 500-entry table (+0x1670), a
    // 16-entry table (+0x1E69, stride 48), TWO 64-entry arrays of vector-constructed
    // CgsSceneManager::CgsCollision::BaseCollisionGenerator (+0x2170/+0x4990, the
    // per-material collision generators), three Attrib::Gen tables
    // (crashbinlist / propscrashbinlist / proptomaterialmappings @ +0x8234..), and the
    // crash splicer-bank Content sub-objects (+0x8210..) -- is NOT modelled in this
    // minimal shell. Per the task constraint, this shell does NOT pull in the collision
    // domain (CgsSceneManager::CgsCollision, the Attrib::Gen crash tables); the heavy
    // construction is DEFERRED (see ctor FLAG). The shell exists only to be a CONCRETE,
    // registrable leaf whose Prepare() returns true for PrepareStateManagersOnBoot. A
    // single opaque pad keeps the deferred state honestly named without fabricating
    // field meanings. Size is UNVERIFIED on host (the X360 0x8280 is a 32-bit fact);
    // NOT static_asserted.
    u8 maDeferredCollisionState[1]; // placeholder for the un-reconstructed collision-audio members
};

} // namespace Collision
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_COLLISION_COLLISION_STATE_MANAGER_H
