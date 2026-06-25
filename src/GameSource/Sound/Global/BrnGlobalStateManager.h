#ifndef BRN_SOUND_LOGIC_GLOBAL_STATE_MANAGER_H
#define BRN_SOUND_LOGIC_GLOBAL_STATE_MANAGER_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnStateManager.h"   // BrnSound::Logic::BrnStateManager (committed base)

// =============================================================================
// BrnSound::Logic::GlobalStateManager
//   GameSource/Sound/Global/BrnGlobalStateManager.{h,cpp}
//   (canonical home -- derived from the X360 mangled name
//    BrnSound::Logic::GlobalStateManager; the Sound/Global/ dir already exists
//    in-tree and hosts the sibling global-domain classes -- BrnGlobalState,
//    BrnMixerControl, BrnMusicEffect, etc.)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// GlobalStateManager is the sound-logic state manager that owns the game-global
// audio: the global Voice/Content domain (the global voices it Connects + the
// content banks it loads). It is one of the 9 managers the SoundLogicModule factory
// CreateStateManagers (0x826AFEF8) creates via CreateStateMan.
//
// NOTE: this is a DISTINCT class from the existing sibling BrnGlobalState
// (BrnGlobalState.{h,cpp}); the X360 mangles this one as
// BrnSound::Logic::GlobalStateManager.
//
// BASE CHAIN: GlobalStateManager : public BrnSound::Logic::BrnStateManager
//   (-> CgsSound::Logic::StateManager primary base + BrnSound::Logic::
//    IResourceRequester sub-object). Evidence: the ctor @ 0x826FBB50 installs a
//   primary vtable @ +0 (off_820B7DA8) AND a secondary sub-object vtable @ +0x90
//   (*(result+144) = off_820B7DA0, after a transient off_820AB608) -- the
//   IResourceRequester sub-object vptr -- and Prepare @ 0x826F5BF8 routes through it.
//   Same shape as the committed siblings AIVehicleStateManager / PassbyStateManager.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 object is 208 bytes (0xD0)
// behind 4-byte pointers/vptrs (CreateObject @ 0x826FE380 allocates 208); on the
// 64-bit host the layout differs, so members are pinned BY NAME only and the 0xD0
// size / absolute offsets are NOT static_asserted. The ctor builds two refcounted
// global-Voice sub-objects (+0x98/+0xA4, each {&off_820B0E20 vtable, 0, 0}) and two
// splicer-bank Content sub-objects (+0xB0/+0xBC, each {&off_820B3250 vtable, 0, 0}),
// plus a tail word at +0xC8; no recovered body in this slice names an individual
// data member by a recovered field name, so the global Voice/Content state is
// deferred (see FLAG) and a single opaque pad honestly names it.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

class GlobalStateManager : public BrnSound::Logic::BrnStateManager
{
public:
    // GlobalStateManager @ 0x826FBB50. Forwards to the BrnStateManager base ctor and
    // builds the two global-Voice + two Content sub-objects.
    GlobalStateManager();

    // ~GlobalStateManager @ 0x826FBBE8 (the X360 `vector deleting destructor`).
    virtual ~GlobalStateManager();

    // ---- RTTI hooks (the per-class descriptor + factory). STATIC GetStaticTypeInfo
    // / CreateObject so &CreateObject is storable in ClassTypeInfo<StateManager>::
    // mpfnCreateObject (the X360 CreateObject @ 0x826FE380 never touches an instance
    // -- its int arg is the operator-new flavour selector, not `this`). ----
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetStaticTypeInfo();
    static CgsSound::Logic::StateManager* CreateObject( u32 luType );                     // @ 0x826FE380

    // ---- boot + lifecycle virtuals ----
    virtual bool Prepare();                       // @ 0x826F5BF8  (vtable +0x0C; stub -- see .cpp FLAG)

    // ---- IResourceRequester overrides (pure in IResourceRequester; BrnStateManager
    // declares but does not body them, so the concrete leaf must override+body them). ----
    virtual void                            ResourcesAreReady();    // (stub -- domain cascade; see .cpp FLAG)
    virtual BrnSound::Logic::ResourceRegistrar& GetResourceRegistrar(); // (stub -- module not homed; see .cpp FLAG)

private:
    // FLAG (deferred body -- ~110 bytes): the X360 object is 208 bytes (0xD0). The
    // global Voice/Content state (the two refcounted global-Voice sub-objects the ctor
    // builds at +0x98/+0xA4 -- Voice::Construct/Connect/SetGain in Prepare -- the two
    // splicer-bank Content sub-objects at +0xB0/+0xBC, and the +0xC8 tail word) is NOT
    // modelled in this minimal shell -- the global audio domain is not reconstructed.
    // The shell exists only to be a CONCRETE, registrable leaf whose Prepare() returns
    // true for PrepareStateManagersOnBoot. A single opaque pad keeps the deferred state
    // honestly named without fabricating field meanings. Size is UNVERIFIED on host
    // (the X360 0xD0 is a 32-bit fact); NOT static_asserted.
    u8 maDeferredGlobalState[1]; // placeholder for the un-reconstructed global Voice/Content members
};

} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_GLOBAL_STATE_MANAGER_H
