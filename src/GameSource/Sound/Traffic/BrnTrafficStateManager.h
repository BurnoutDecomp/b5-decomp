#ifndef BRN_SOUND_LOGIC_TRAFFIC_TRAFFIC_STATE_MANAGER_H
#define BRN_SOUND_LOGIC_TRAFFIC_TRAFFIC_STATE_MANAGER_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnStateManager.h"   // BrnSound::Logic::BrnStateManager (committed base)

// =============================================================================
// BrnSound::Logic::Traffic::TrafficStateManager
//   GameSource/Sound/Traffic/BrnTrafficStateManager.{h,cpp}
//   (canonical home -- derived from the X360 mangled name
//    BrnSound::Logic::Traffic::TrafficStateManager; the Sound/Traffic/ dir already
//    exists in-tree, this is its first occupant.)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// TrafficStateManager is the sound-logic state manager that owns the ambient
// traffic-car / horn audio: it loads the Traffic_Bank + patch_bank_horns splicer
// banks (TrafficCsis / HornsCsis content) and drives the traffic voices each
// frame. It is one of the 9 managers the SoundLogicModule factory
// CreateStateManagers (0x826AFEF8) creates via CreateStateMan.
//
// BASE CHAIN: TrafficStateManager : public BrnSound::Logic::BrnStateManager
//   (-> CgsSound::Logic::StateManager primary base + BrnSound::Logic::
//    IResourceRequester sub-object). Evidence: the ctor @ 0x826FC250 installs a
//   primary vtable @ +0 (off_820B7FB4) AND a secondary sub-object vtable @ +0x90
//   (result[36] = off_820B7FAC, after a transient off_820AB608) -- the
//   IResourceRequester sub-object vptr -- and Prepare @ 0x826F0E78 calls
//   IResourceRequester::LoadAsset(a1+36 == this+0x90, ...). Same shape as the
//   committed siblings AIVehicleStateManager / PassbyStateManager.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 object is 3280 bytes (0xCD0)
// behind 4-byte pointers/vptrs (CreateObject @ 0x82701208 allocates 3280); on the
// 64-bit host the layout differs, so members are pinned BY NAME only and the
// 0xCD0 size / absolute offsets are NOT static_asserted. No recovered body in this
// slice names an individual data member by a recovered field name (the ctor zero-
// inits a 31-entry table at +61.. and four Content sub-objects at result[808..819];
// Prepare reads only the prepare-state at a1[9] and the CPU-monitor handle at
// a1[37], both base/inherited bookkeeping), so the ~3.2KB of traffic-voice state is
// deferred (see FLAG) and a single opaque pad honestly names it.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

class TrafficStateManager : public BrnSound::Logic::BrnStateManager
{
public:
    // TrafficStateManager @ 0x826FC250. Forwards to the BrnStateManager base ctor
    // and zero-inits the traffic-voice table + the four Content sub-objects.
    TrafficStateManager();
    virtual ~TrafficStateManager();

    // ---- RTTI hooks (the per-class descriptor + factory). STATIC GetStaticTypeInfo
    // / CreateObject so &CreateObject is storable in ClassTypeInfo<StateManager>::
    // mpfnCreateObject (the X360 CreateObject @ 0x82701208 never touches an instance
    // -- its int arg is the operator-new flavour selector, not `this`). ----
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetStaticTypeInfo();
    static CgsSound::Logic::StateManager* CreateObject( u32 luType );                     // @ 0x82701208

    // ---- boot + lifecycle virtuals ----
    virtual bool Prepare();                       // @ 0x826F0E78  (vtable +0x0C; stub -- see .cpp FLAG)

    // ---- IResourceRequester overrides (pure in IResourceRequester; BrnStateManager
    // declares but does not body them, so the concrete leaf must override+body them to
    // be a CONCRETE (constructible) type -- required because CreateObject below `new`s
    // a TrafficStateManager). ----
    virtual void                            ResourcesAreReady();    // (stub -- domain cascade; see .cpp FLAG)
    virtual BrnSound::Logic::ResourceRegistrar& GetResourceRegistrar(); // (stub -- module not homed; see .cpp FLAG)

private:
    // FLAG (deferred body -- ~3.2KB): the X360 object is 3280 bytes (0xCD0). The
    // traffic-voice state (the 31-entry voice table the ctor zero-inits at +61.., the
    // four splicer-bank Content sub-objects at result[808..819] -- TrafficCsis /
    // HornsCsis / two more -- driven by Prepare's Content::Construct/IsLoaded, the
    // per-frame voice state) is NOT modelled in this minimal shell -- the traffic
    // audio domain is not reconstructed. The shell exists only to be a CONCRETE,
    // registrable leaf whose Prepare() returns true for PrepareStateManagersOnBoot. A
    // single opaque pad keeps the deferred state honestly named without fabricating
    // field meanings. Size is UNVERIFIED on host (the X360 0xCD0 is a 32-bit fact);
    // NOT static_asserted.
    u8 maDeferredTrafficState[1]; // placeholder for the un-reconstructed traffic-voice members
};

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_TRAFFIC_TRAFFIC_STATE_MANAGER_H
