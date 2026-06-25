#ifndef BRN_SOUND_VEHICLES_AI_VEHICLE_STATE_MANAGER_H
#define BRN_SOUND_VEHICLES_AI_VEHICLE_STATE_MANAGER_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnStateManager.h"   // BrnSound::Logic::BrnStateManager (committed base)

// =============================================================================
// BrnSound::Vehicles::AIVehicleStateManager
//   GameSource/Sound/Vehicles/BrnAIVehicleStateManager.{h,cpp}
//   (canonical home -- derived from the FireAssert source-path strings in the
//    bodies: ".../Sound/Vehicles/BrnAIVehicleStateManager.cpp")
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// AIVehicleStateManager is the sound-logic state manager that owns the AI-car
// engine audio: it loads the per-AI-car engine components and drives the AI
// engine voices each frame. It is one of the 9 managers the SoundLogicModule
// factory CreateStateManagers (0x826AFEF8) creates via CreateStateMan.
//
// NOTE: this is a DISTINCT class from the existing namespace-only home
// BrnSound::Vehicles::VehicleStateManager (BrnVehicleStateManager.{h,cpp}, which
// only hosts the free function GetAIEngineAssignment). The X360 mangles this one
// as BrnSound::Vehicles::AIVehicleStateManager.
//
// BASE CHAIN: AIVehicleStateManager : public BrnSound::Logic::BrnStateManager
//   (-> CgsSound::Logic::StateManager primary base + BrnSound::Logic::
//    IResourceRequester sub-object). Evidence: the leaf has its own GetTypeName
//   @ 0x82684028 ("AIVehicleStateManager") AND an IResourceRequester completion
//   callback ResourcesAreReady @ 0x82684038, i.e. it carries the IResourceRequester
//   sub-object -> it is a BrnStateManager descendant (same shape as the sibling
//   PassbyStateManager). The ctor @ 0x82700EB8 (JSON ABSENT) is confirmed only as
//   the xref target of CreateObject @ 0x82702358 (which placement-news an 880-byte
//   / 0x370 AIVehicleStateManager then `bl`s the ctor).
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 object is 880 bytes (0x370)
// behind 4-byte pointers/vptrs; on the 64-bit host the layout differs, so members
// are pinned BY NAME only and the 0x370 size / absolute offsets are NOT asserted.
// Only the ONE member a recovered body names (meAIEngineLoadingState, read at the
// object's +0x8 by ResourcesAreReady @ 0x82684038) is modelled; the remaining
// ~860 bytes of AI-engine state are deferred (see FLAG).
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{

class AIVehicleStateManager : public BrnSound::Logic::BrnStateManager
{
public:
    // BrnAIVehicleStateManager.cpp:354 (assert text). The AI-engine-loading sub-state
    // machine ResourcesAreReady (0x82684038) advances. The asserted-valid states are
    // E_AI_ENGINE_LOADING_WAITING_FOR_LOAD_ATTRIB (==1) and
    // E_AI_ENGINE_LOADING_WAITING_LOADING_ENGINE_COMPONENTS (==3); ResourcesAreReady
    // steps 1->2 and 3->4. The remaining members (0=idle/start, etc.) are not named by
    // a recovered body; only the two asserted values + the two targets are facts.
    // FLAG: enumerator names recovered from the assert string; the numeric values
    // (1,2,3,4) are recovered from the ResourcesAreReady transition (result[2]: 1->2,
    // 3->4). The 0 and 5 sentinels follow the Prepare switch convention (cases 0/5
    // reset) but their enumerator names are NOT recovered -- placeholders.
    enum EAIEngineLoadingState
    {
        E_AI_ENGINE_LOADING_IDLE                          = 0, // FLAG: name placeholder
        E_AI_ENGINE_LOADING_WAITING_FOR_LOAD_ATTRIB       = 1, // assert-named
        E_AI_ENGINE_LOADING_LOADING_ENGINE_COMPONENTS     = 2, // 1 -> 2 target
        E_AI_ENGINE_LOADING_WAITING_LOADING_ENGINE_COMPONENTS = 3, // assert-named
        E_AI_ENGINE_LOADING_DONE                          = 4, // 3 -> 4 target
    };

    // ctor @ 0x82700EB8 (JSON ABSENT -- minimal). Forwards to the BrnStateManager base.
    AIVehicleStateManager();
    virtual ~AIVehicleStateManager();

    // ---- RTTI hooks (the per-class descriptor + factory). STATIC GetStaticTypeInfo /
    // CreateObject so &CreateObject is storable in ClassTypeInfo<StateManager>::
    // mpfnCreateObject (the X360 CreateObject @ 0x82702358 never touches an instance --
    // its int arg is the operator-new flavour selector, not `this`). ----
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;                                              // @ 0x82684028
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetStaticTypeInfo();
    static CgsSound::Logic::StateManager* CreateObject( u32 luType );                     // @ 0x82702358

    // ---- boot + lifecycle virtuals ----
    virtual bool Prepare();                       // @ 0x826EFC18  (vtable +0x0C; stub -- see .cpp FLAG)

    // ---- IResourceRequester overrides (pure in IResourceRequester; BrnStateManager
    // declares but does not body them, so the concrete leaf must). ----
    virtual void                            ResourcesAreReady();    // @ 0x82684038 (real -- self-contained)
    virtual BrnSound::Logic::ResourceRegistrar& GetResourceRegistrar(); // (stub -- module not homed; see .cpp FLAG)

private:
    // +0x08: the AI-engine-loading sub-state (read at result[2] by ResourcesAreReady
    // @ 0x82684038). The only member named by a recovered body; modelled by name.
    EAIEngineLoadingState meAIEngineLoadingState;

    // FLAG (deferred body -- ~860 bytes): the X360 object is 880 bytes (0x370). The
    // remaining AI-engine state (per-AI-car engine component handles, the engine-voice
    // assignment table consumed via VehicleStateManager::GetAIEngineAssignment, the AI
    // engine loading bookkeeping driven by PrepareAIEngineLoading @ 0x826E2708, the
    // per-frame voice state touched by UpdateParams @ 0x826CA578) is NOT modelled in
    // this minimal shell -- those domains (AI engine audio) are not reconstructed. The
    // shell exists only to be a CONCRETE, registrable leaf whose Prepare() returns true
    // for PrepareStateManagersOnBoot. A single opaque pad keeps the deferred state
    // honestly named without fabricating field meanings. Size is UNVERIFIED on host
    // (the X360 0x370 is a 32-bit fact); NOT static_asserted.
    u8 maDeferredAIEngineState[1]; // placeholder for the un-reconstructed AI-engine members
};

} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_AI_VEHICLE_STATE_MANAGER_H
