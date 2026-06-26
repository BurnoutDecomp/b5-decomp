#include "GameSource/Sound/Vehicles/BrnAIVehicleStateManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// =============================================================================
// BrnSound::Vehicles::AIVehicleStateManager -- out-of-line bodies.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// This canonical home brings the AI-car engine sound-logic state manager up as a
// CONCRETE, registrable leaf the StateManager factory CreateStateMan @ 0x826A5B60
// can construct. The deep AI-engine audio domain (loading, per-frame voices) is
// deferred -- see the per-function FLAGs.
//
// Sources:
//   AIVehicleStateManager::CreateObject       @ 0x82702358  (real)
//   AIVehicleStateManager::Prepare            @ 0x826EFC18  (stub -- domain cascade)
//   AIVehicleStateManager::GetTypeName        @ 0x82684028  (real)
//   AIVehicleStateManager::ResourcesAreReady  @ 0x82684038  (real -- self-contained!)
//   ctor                                      @ 0x82700EB8  (JSON ABSENT -- minimal)
// GetTypeInfo / GetStaticTypeInfo / GetResourceRegistrar were NOT individually
// exported; reconstructed from the established in-tree RTTI pattern + the sibling
// BrnEffectObject::GetResourceRegistrar @ 0x82696850.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{

// ---------------------------------------------------------------------------
// AIVehicleStateManager::AIVehicleStateManager()  ctor @ 0x82700EB8  (JSON ABSENT)
//
// FLAG (ctor JSON absent -- minimal reconstruction): the individual ctor export
// for 0x82700EB8 is not present in the IDA dump (only its address is confirmed, as
// the xref target of CreateObject @ 0x82702358, which placement-news an 880-byte /
// 0x370 AIVehicleStateManager then `bl`s this ctor). The real ctor forwards to the
// BrnStateManager base ctor (-> CgsSound::Logic::StateManager ctor @ 0x826FAA18,
// the foundation), installs this class's vtables, then zero/seed-initialises the
// AI-engine members. Reproduced here as the implicit base construction + by-name
// init of the one modelled member (meAIEngineLoadingState = idle). The deferred
// AI-engine state (maDeferredAIEngineState) is left default-initialised; its real
// per-member seed is not recovered (see header FLAG).
// ---------------------------------------------------------------------------
AIVehicleStateManager::AIVehicleStateManager()
    : meAIEngineLoadingState( E_AI_ENGINE_LOADING_IDLE )
{
}

// ---------------------------------------------------------------------------
// AIVehicleStateManager::~AIVehicleStateManager()  (the X360 `vector deleting destructor`)
//
// FLAG (minimal): the matching dtor was not individually exported. The base
// BrnStateManager / CgsSound::Logic::StateManager virtual destructor tears down the
// base content pool, re-installs the MemBase vtable and routes storage back to the
// sound allocator (off_82FFB954) -- all re-synthesised by the host toolchain from
// this virtual destructor. No leaf member teardown is modelled (the deferred
// AI-engine state owns no recovered resources in this slice).
// ---------------------------------------------------------------------------
AIVehicleStateManager::~AIVehicleStateManager()
{
}

// ---------------------------------------------------------------------------
// AIVehicleStateManager::CreateObject(u32)  @ 0x82702358   (the factory hook)
//
//   if ( a1 ) { if ( MemBase::operator new(880, "AIVehicleStateManager", 1) ) return new'd ctor; }
//   else      { if ( MemBase::operator new(880, "AIVehicleStateManager", 0) ) return new'd ctor; }
//   return 0;
//
// The X360 allocates an 880-byte (0x370) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "AIVehicleStateManager" (off_82F2E890) and
// placement-constructs an AIVehicleStateManager into it. Both arms call the SAME
// size+ctor; the `a1` argument only selects the operator-new flavour (0/1). The
// factory CreateStateMan @ 0x826A5B60 calls this as createObject(0).
//
// FLAG (allocator gate): CgsSound::MemBase (CgsMemBase.h) does NOT model
// operator new(size, tag, flavour) -- the sound allocator (off_82FFB954) is not
// homed in this group -- so a faithful placement-new through that allocator is not
// yet expressible. This reconstruction uses the host `new` (global operator new, NOT
// the sound allocator); the observable result -- a constructed AIVehicleStateManager*
// (or null) handed to the factory -- matches. Replace with the sound-allocator
// placement-new once MemBase::operator new is homed. The 880-byte size is the X360
// 0x370; on the 64-bit host the real object is larger, so the literal size is
// documentation only and is NOT passed to the host new.
// ---------------------------------------------------------------------------
CgsSound::Logic::StateManager* AIVehicleStateManager::CreateObject( u32 /*luType*/ )
{
    return new AIVehicleStateManager();
}

// ---------------------------------------------------------------------------
// AIVehicleStateManager::GetStaticTypeInfo()  (RTTI descriptor)
//
// Mirrors the in-tree GetStaticTypeInfo convention (CgsStateManager.cpp:230,
// CgsEffectBase.cpp:130): a function-local static ClassTypeInfo<StateManager>
// seeded with (ObjectID, typeName, baseTypeInfo, createObject) so the factory
// CreateStateMan can match descriptor->ObjectID and call ->createObject.
//
// FLAG (ObjectID UNRESOLVED): the per-leaf registration static-init that calls
// StateManager::AddToClassTypeInfoArray(@0x8268DFE8) with the explicit ObjectID was
// NOT exported (CreateObject @ 0x82702358 has no xrefs_to) and no map-state enum
// names the id in-tree. Per the established in-tree placeholder convention (every
// committed GetStaticTypeInfo uses 0), the ObjectID is seeded 0 here and MUST be
// replaced with the real id at integration -- the id is this manager's slot in the
// CreateStateManagers 0..8 sequence (@ 0x826AFEF8).
//
// FLAG (registry hookup deferred): the minimal CgsSound::Logic::StateManager view
// pulled via BrnStateManager.h (this TU's base) does NOT declare
// AddToClassTypeInfoArray (that lives in the full CgsStateManager.h view, ODR-
// incompatible with BrnStateManager.h and not co-includable here). So this
// descriptor is produced here but its insertion into the static registry
// (dword_82FFBC58) must be done by a registration site using the full StateManager
// view (the conductor-owned CreateStateMan TU). &CreateObject is an ABI-compatible
// StateManager*(*)(u32) across both views.
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* AIVehicleStateManager::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager> sTypeInfo(
        2,                          // ObjectID         -- FLAG: arbitrary-unique (slot 2)
        "AIVehicleStateManager",    // typeName
        0,                          // baseTypeInfo     -- StateManager base descriptor (deferred)
        &AIVehicleStateManager::CreateObject // createObject
    );
    return &sTypeInfo;
}

// ---------------------------------------------------------------------------
// File-scope registration (Part D): land this leaf's descriptor in the shared
// StateManager RTTI registry (CgsStateManager.cpp gapClassTypeInfoArray, X360
// dword_82FFBC58) at load time, so StateManager::CreateStateMan (0x826A5B60) can
// find it by ObjectID. AddToClassTypeInfoArray is the canonical StateManager
// registration entry (@ 0x8268DFE8), reached through the BrnStateManager base.
//
// FLAG (ObjectID arbitrary-unique): the exact X360 ObjectID was not exported
// (CreateObject @ 0x82702358 has no xrefs_to). Assigned 2 here as a unique-among-the-9
// placeholder; the real id is this manager's slot in the CreateStateManagers 0..8 loop
// (@ 0x826AFEF8) -- pin at integration. This TU is OUT of the build, so dormant until
// the conductor adds it.
// ---------------------------------------------------------------------------
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* const
    gpAIVehicleStateManagerReg =
        CgsSound::Logic::StateManager::AddToClassTypeInfoArray(
            AIVehicleStateManager::GetStaticTypeInfo());

// ---------------------------------------------------------------------------
// AIVehicleStateManager::GetTypeInfo() const  (vtable RTTI hook)
//   Returns this leaf's static descriptor.
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* AIVehicleStateManager::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

// ---------------------------------------------------------------------------
// AIVehicleStateManager::GetTypeName() const  @ 0x82684028
//   X360: `lwz r3, off_82F2E890` -> returns the literal "AIVehicleStateManager".
// ---------------------------------------------------------------------------
const char* AIVehicleStateManager::GetTypeName() const
{
    return "AIVehicleStateManager";
}

// ---------------------------------------------------------------------------
// AIVehicleStateManager::Prepare()  @ 0x826EFC18   (vtable +0x0C)
//
// X360 body: a switch on the +0x24 prepare-state (cases 0/5 -> 0, 1, 2, 3, 4):
//   state 0: mCpuMonitor = PerfMonCpu::AddMonitor("AI Cars", 14, 0, 1.0, ...)
//   state 1: if (!PrepareAIEngineLoading(this)) return 0;
//   state 3: if (!StateManager::PrepareStates(this, 16920, 3, 0)) return 0;
//   state 4: return 1;
//
// FLAG (stub -- domain cascade): the real body cascades into
//   * CgsDev::PerfMonCpu::AddMonitor (the CPU perf monitor),
//   * AIVehicleStateManager::PrepareAIEngineLoading @ 0x826E2708 (loads the per-AI-car
//     engine components -- the deep AI engine audio domain), and
//   * CgsSound::Logic::StateManager::PrepareStates @ 0x826EAD30 (the State machine,
//     itself a declared-only stub in the foundation).
// None reconstructed in this slice. PrepareStateManagersOnBoot (0x826837F8) only
// needs Prepare() to return true to advance boot, so this stub returns true
// (boot-ready) WITHOUT running the AI-engine load / state bring-up. NOT an
// X360-faithful body -- the prepare state machine is deferred. X360 addr above.
// ---------------------------------------------------------------------------
bool AIVehicleStateManager::Prepare()
{
    return true;
}

// ---------------------------------------------------------------------------
// AIVehicleStateManager::ResourcesAreReady()  @ 0x82684038
//
// X360 body (SELF-CONTAINED -- decompiled faithfully): the AI-engine-loading
// completion callback advances meAIEngineLoadingState (the +0x8 member):
//   assert( meAIEngineLoadingState == E_AI_ENGINE_LOADING_WAITING_FOR_LOAD_ATTRIB ||
//           meAIEngineLoadingState == E_AI_ENGINE_LOADING_WAITING_LOADING_ENGINE_COMPONENTS )
//   if      ( meAIEngineLoadingState == 1 ) meAIEngineLoadingState = 2;
//   else if ( meAIEngineLoadingState == 3 ) meAIEngineLoadingState = 4;
//   else      return;                      // (unreachable after the assert)
//
// This one IS reconstructable in full -- it only touches the modelled
// meAIEngineLoadingState member (no domain cascade). Store-for-store with the asm
// (result[2] read twice; 1->2, 3->4; the !=1 && !=3 branch fires the assert at
// BrnAIVehicleStateManager.cpp:354 then re-reads). The assert is the non-gating
// tripwire. Reached BY NAME (member), no raw +0x8 cast.
// ---------------------------------------------------------------------------
void AIVehicleStateManager::ResourcesAreReady()
{
    CGS_ASSERT( meAIEngineLoadingState == E_AI_ENGINE_LOADING_WAITING_FOR_LOAD_ATTRIB
                || meAIEngineLoadingState == E_AI_ENGINE_LOADING_WAITING_LOADING_ENGINE_COMPONENTS,
                "E_AI_ENGINE_LOADING_WAITING_FOR_LOAD_ATTRIB == meAIEngineLoadingState || "
                "E_AI_ENGINE_LOADING_WAITING_LOADING_ENGINE_COMPONENTS == meAIEngineLoadingState" );

    if ( meAIEngineLoadingState == E_AI_ENGINE_LOADING_WAITING_FOR_LOAD_ATTRIB )
    {
        meAIEngineLoadingState = E_AI_ENGINE_LOADING_LOADING_ENGINE_COMPONENTS;        // 1 -> 2
    }
    else if ( meAIEngineLoadingState == E_AI_ENGINE_LOADING_WAITING_LOADING_ENGINE_COMPONENTS )
    {
        meAIEngineLoadingState = E_AI_ENGINE_LOADING_DONE;                             // 3 -> 4
    }
}

// ---------------------------------------------------------------------------
// AIVehicleStateManager::GetResourceRegistrar()  (IResourceRequester slot 1)
//
// Recovered semantically from the sibling BrnEffectObject::GetResourceRegistrar
// @ 0x82696850: load this->mpLogicModule (+0x2C), tail-call the IResourceRequester
// slot-1 of the module's embedded ResourceRegistrar (SoundLogicModule::
// mResourceRegistrar @ module+0x4C90). The state-manager leaves share the +0x2C
// module back-pointer (stamped by CreateStateMan).
//
// FLAG (module opaque): the minimal CgsSound::Logic::StateManager view in this TU
// (via BrnStateManager.h) does not expose mpLogicModule (+0x2C), and the
// SoundLogicModule home is not reconstructed in this slice -- so this cannot be
// bodied faithfully here. Provided as a non-cascading stub that abort-asserts if
// ever reached on boot (PrepareStateManagersOnBoot does NOT call it; it is only used
// on the per-frame attach/detach path this slice never exercises). Returns a
// TU-local empty registrar purely to satisfy the non-void signature; NOT a faithful
// body. Body via the module once the full StateManager view (mpLogicModule) +
// SoundLogicModule are available.
// ---------------------------------------------------------------------------
BrnSound::Logic::ResourceRegistrar& AIVehicleStateManager::GetResourceRegistrar()
{
    CGS_ASSERT( false,
                "AIVehicleStateManager::GetResourceRegistrar reached without a homed "
                "SoundLogicModule (boot path does not call this)" );
    static BrnSound::Logic::ResourceRegistrar sUnhomedRegistrar;
    return sUnhomedRegistrar;
}

} // namespace Vehicles
} // namespace BrnSound
