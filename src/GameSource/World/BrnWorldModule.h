#pragma once

// ============================================================================
// b5-decomp/src/GameSource/World/BrnWorldModule.h
//
// Home of BrnWorld::WorldModule -- the World MODULE: a CgsModule::ModuleSingleBuffered
// subclass that owns + ticks the whole world subsystem (the race-car / traffic / prop /
// trigger / world entity modules, the physics + scene + AI + crash modules, the
// environment manager and the environment/shadow maps) through the per-frame
// scene-update interface.
//
// The ledger key's "GameSource/Unity/../World/BrnWorldModule.cpp" collapses to this
// GameSource/World/ home (the X360 build #included BrnWorldModule.cpp from a Unity TU).
//
// SOURCE-OF-TRUTH: member list + order + method signatures are the DecFIGS DWARF
// (BrnWorldModule.h:124..386 + the cpp-bodied method decls). The X360 ARTIST asm is the
// behaviour spine.
//
// ----------------------------------------------------------------------------
// SCOPE / FLAG (what is reconstructed here vs declaration-only):
//
// WorldModule embeds, BY VALUE, a fleet of large sub-modules each of which is a
// committed-but-only-MINIMALLY-homed aggregate (RaceCarEntityModule, TrafficEntityModule,
// WorldEntityModule, PropEntityModule, TriggerEntityModule, PhysicsModule, SceneManagerModule,
// AIModule, CrashModule, EnvironmentManager, ...). Their FULL byte layouts are NOT homed
// anywhere in the tree (each sibling header carries only the small slice its own ledger
// functions need -- see e.g. BrnRaceCarEntityModule.h). Because the real per-sub-module
// byte offsets are therefore NOT recoverable here, the X360 method bodies that index those
// sub-aggregates (Construct, Destruct, Release, the Update spine: EntityModulePost/PrePhysicsUpdate,
// ExternalSceneQueriesUpdate, UpdatePhysicsNetworkCatchup, HandleGameActions) cannot be written
// without raw-offset pokes into committed aggregates (FORBIDDEN by AGENTS.md) -- they are
// DECLARED here and DECLARATION-ONLY + FLAGGED in the .cpp.
//
// The three dispatch/visibility builders (GenerateDispatchLists, GenerateFrustumQueries,
// GenerateShadowMapDispatchLists) and the irradiance helper are multi-stage VMX/VPU pipelines
// -- DECLARATION-ONLY + FLAGGED (NEVER paraphrased to scalar).
//
// BODIED (faithful, through this TU's OWN members + named accessors + committed deps):
//   LoadDistrictMap(UpdateOutputBuffer*)  -- the District-map streaming state machine.
//
// The class is made instantiable by overriding the CgsModule base pure virtuals (the
// no-arg ModuleSingleBuffered overloads); the X360 WorldModule's own Construct/Prepare/
// Release/Destruct/Update take extra arguments and are SEPARATE (non-override) methods,
// exactly as the DWARF declares them.
// ----------------------------------------------------------------------------
#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"     // CgsModule::ModuleSingleBuffered base
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"   // CgsModule::EventReceiverQueue<N,16>
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"  // CgsResource::ResourceHandle
#include "GameSource/BurnoutConstants.h"                               // EActiveRaceCarIndex

namespace BrnWorld
{
    // ------------------------------------------------------------------------
    // FLAG (minimal slice): the World module's per-frame "Update" OUTPUT IO-buffer.
    // The real BrnWorldIO::UpdateOutputBuffer layout is grown by its own canonical TU
    // (BrnWorldModuleIO*; the GetResourceRequestResourceInterface accessor @0x... is a
    // separate [todo] ledger key). LoadDistrictMap only ever forwards through the named
    // accessor below, so the buffer's internal byte size is NOT load-bearing for this TU.
    // ------------------------------------------------------------------------
    struct UpdateOutputBuffer; // forward (defined in the IO TU); used by pointer only

    // ------------------------------------------------------------------------
    // BrnWorld::WorldModule  (DWARF BrnWorldModule.h:124)
    // ------------------------------------------------------------------------
    class WorldModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        // ---- prepare / release / resource / district-map stage enums (DWARF :127..185) ----
        enum EWorldPrepareStage : s32
        {
            eWorldPrepareStart                 = 0,
            eWorldPrepareModule                = 1,
            eWorldPrepareResources             = 2,
            eWorldPrepareSceneModule           = 3,
            eWorldPreparePhysicsModule         = 4,
            eWorldPrepareEnvironmentManager    = 5,
            eWorldPrepareRaceCarEntityModule   = 6,
            eWorldPrepareTrafficEntityModule   = 7,
            eWorldPrepareWorldEntityModule     = 8,
            eWorldPreparePropEntityModule      = 9,
            eWorldPrepareTriggerEntityModule   = 10,
            eWorldPrepareAI                    = 11,
            eWorldPrepareCrashModule           = 12,
            eWorldPrepareEnvmapCameras         = 13,
            eWorldPrepareDebug                 = 14,
            eWorldPrepareDone                  = 15,
        };

        enum EWorldReleaseStage : s32
        {
            eWorldReleaseStart                 = 0,
            eWorldReleaseCrashModule           = 1,
            eWorldReleaseAI                    = 2,
            eWorldReleaseTriggerEntityModule   = 3,
            eWorldReleasePropEntityModule      = 4,
            eWorldReleaseWorldEntityModule     = 5,
            eWorldReleaseTrafficEntityModule   = 6,
            eWorldReleaseRaceCarEntityModule   = 7,
            eWorldReleaseEnvironmentManager    = 8,
            eWorldReleasePhysicsModule         = 9,
            eWorldReleaseSceneModule           = 10,
            eWorldReleaseEnvmapCameras         = 11,
            eWorldReleaseModule                = 12,
            eWorldReleaseDone                  = 13,
        };

        enum EResourceAcquireState : s32
        {
            eResourceAcquireStateNotStarted = 0,
            eResourceAcquireStateRequested  = 1,
            eResourceAcquireStateAcquired   = 2,
        };

        enum EResourceAcquireStage : s32
        {
            E_RESOURCESTAGE_START             = 0,
            E_RESOURCESTAGE_LOADING_VAULT     = 1,
            E_RESOURCESTAGE_ACQUIRING_VAULT   = 2,
            E_RESOURCESTAGE_REGISTERING_VAULT = 3,
            E_RESOURCESTAGE_DONE              = 4,
        };

        enum DistrictMapLoadStage : s32
        {
            E_DISTRICT_MAP_LOAD_REQUEST     = 0,
            E_DISTRICT_MAP_LOAD_RESPONSE    = 1,
            E_DISTRICT_MAP_ACQUIRE_REQUEST  = 2,
            E_DISTRICT_MAP_ACQUIRE_RESPONSE = 3,
            E_DISTRICT_MAP_DONE             = 4,
        };

        // ====================================================================
        // CgsModule base pure-virtual overrides (make WorldModule instantiable).
        // The X360 WorldModule's real lifecycle entry points take extra IO arguments
        // (see the non-virtual Construct(const BrnCpuMonitors&)/Prepare(...)/Release(...)/
        // Update(...) below, per the DWARF). These no-arg overrides exist only to satisfy
        // the abstract CgsModule contract; they forward to the single-buffered base.
        // ====================================================================
        void Construct() override { CgsModule::ModuleSingleBuffered::Construct(); }
        bool Prepare()   override { return CgsModule::ModuleSingleBuffered::Prepare(); }
        bool Release()   override { return CgsModule::ModuleSingleBuffered::Release(); }
        void Destruct()  override { CgsModule::ModuleSingleBuffered::Destruct(); }
        void Update()    override { CgsModule::ModuleSingleBuffered::Update(); }

        // ====================================================================
        // X360 WorldModule methods (DWARF-declared signatures). Only LoadDistrictMap is
        // bodied (below); the rest are DECLARATION-ONLY + FLAG -- see the .cpp.
        // The arg/return TYPES that reach genuinely-un-homed IO/sub-module/VMX deps are
        // spelled here exactly as the DWARF gives them where the type is homed; where the
        // dep type is NOT homed in-tree the parameter is intentionally OMITTED from the
        // declaration-only stub and the real shape is recorded in the .cpp FLAG comment.
        // ====================================================================

        // Private District-map streaming state machine (DWARF cpp:2186). BODIED.
        // Returns true once the district map is fully loaded + acquired (stage DONE).
    private:
        bool LoadDistrictMap(UpdateOutputBuffer* lpOutput);

        // ---- MEMBERS THIS TU TOUCHES BY NAME ----------------------------------------
        // FLAG (minimal slice, NOT byte-exact): the full WorldModule object is ~6MB and is
        // dominated by the embedded sub-module fleet (RaceCarEntityModule, TrafficEntityModule,
        // PhysicsModule, SceneManagerModule, AIModule, CrashModule, the environment/shadow maps,
        // and ~50 i32 perf-monitor handles -- DWARF :282..386). Those sub-aggregates are NOT
        // homed at full byte width anywhere in the tree, so their real intra-object byte offsets
        // are unrecoverable here (and would, on the LLP64 PC target, differ from the X360's
        // 32-bit-pointer layout regardless). This TU therefore does NOT attempt to pin the X360
        // dword indices (a1[141], a1+1541554, ...) with offsetof -- doing so would be fabricated.
        //
        // Only the three members LoadDistrictMap actually reads/writes are modelled BY NAME, in
        // their DWARF logical order. They are accessed by name only; the store-for-store mutation
        // each performs is the parity contract. Everything else the object holds is intentionally
        // ABSENT from this slice (the bodies that need it are declaration-only -- see the .cpp).

        // [DWARF :280] the District-map streaming stage selector (X360 a1[141]).
        DistrictMapLoadStage                    meDistrictMapLoadStage;

        // [DWARF :301] the load-bundle response receiver queue (X360 +1541554).
        CgsModule::EventReceiverQueue<1024, 16> mReceiverQueue;

        // [DWARF :334] the loaded district-map resource handle (X360 stores 2 dwords:
        // mpResourceMemory @ a1[1541858], mpSourceEntry @ a1[1541859]).
        CgsResource::ResourceHandle             mDistrictMapResourceHandle;
    };
}
