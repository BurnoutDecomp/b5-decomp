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
// GROWN 2026-07-24 (world-render campaign): the sub-module fleet is now embedded BY
// VALUE with the real committed types (the earlier minimal-slice rationale -- "fleet
// layouts un-homed" -- became stale when BrnWorld::WorldEntityModule landed in full and
// the sibling module homes were confirmed instantiable). Per the x64 gate the embed is
// semantic-by-NAME: the X360 32-bit intra-object byte offsets (noted per member below,
// from WorldModule::Construct @0x827CF540 / Destruct @0x827BD0F0) are cross-reference
// comments, never layout pins.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"     // CgsModule::ModuleSingleBuffered base
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"
#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h"   // CgsModule::EventReceiverQueue<N,16>
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"  // CgsResource::ResourceHandle
#include "GameSource/World/DebugComponents/BrnSkyDebugComponent.h"     // EnvironmentSettings::DebugComponent (mSkyDebugComponent)
#include "GameShared/GameClasses/Graphics/CgsCamera.h"                 // Camera (mLastCameraInput)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModule.h" // CgsSceneManager::SceneManagerModule
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"     // SceneManagerIO::OutputBuffer (complete type: the
                                                                       // EntityModulePrePhysicsUpdate / dispatch decls below
                                                                       // name its NESTED SceneQueryResultsQueue, which a
                                                                       // forward decl cannot supply -- self-containment fix
                                                                       // surfaced by the world-module mount into BrnGameModule)
#include "GameSource/BurnoutConstants.h"                               // EActiveRaceCarIndex
#include "GameSource/Game/BrnGlobalCpuMonitors.h"                      // BrnCpuMonitors
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModule.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule.h"
#include "GameSource/World/Trigger/BrnTriggerEntityModule.h"
#include "GameSource/Physics/BrnPhysicsModule.h"
#include "GameSource/World/EnvironmentManager/BrnEnvironmentManager.h"
#include "GameSource/World/AI/BrnAIModule.h"
#include "GameSource/World/CrashModule/BrnCrashModule.h"
#include "GameSource/World/EnvironmentMap/BrnEnvironmentMap.h"
#include "GameSource/World/ShadowMap/BrnShadowMap.h"
#include "GameSource/World/DebugComponents/BrnWorldDebugComponent.h"
#include "GameSource/World/BrnWorldModuleIO.h"
#include "GameSource/World/BrnWorldModuleIO_DispatchInputBuffer.h"   // BrnWorldIO::DispatchInputBuffer
#include "GameSource/World/BrnWorldModuleIO_DispatchOutputBuffer.h"  // BrnWorldIO::DispatchOutputBuffer                         // BrnWorldIO::UpdateOutputBuffer (real home)

namespace BrnWorldIO { struct DispatchInputBuffer; }
struct BrnShaderConstantsFrame;
namespace BrnTraffic { namespace BrnTrafficIO { struct TrafficToRaceCarInterface_PreScene; class InputBuffer_PostPhysics; class InputBuffer_PostScene; class OutputBuffer_PostScene; } }
namespace BrnWorld { namespace CrashModuleIO { class OutputBuffer_PostScene; } }

namespace BrnWorld
{

    // ------------------------------------------------------------------------
    // FilteredEntityData  (DWARF BrnWorldModule.h:86) -- the per-dispatch
    // frustum-filter result buffer: the visible entity ids split by owner
    // category, filled by WorldModule::FilterFrustumTestResults and consumed by
    // each module's GenerateDispatchLists.
    // ------------------------------------------------------------------------
    struct FilteredEntityData : public CgsModule::IOBuffer
    {
        void Construct()   // :89
        {
            maWorldEntityIds.Construct();
            maRaceCarEntityIds.Construct();
            maTrafficEntityIds.Construct();
            maPropEntityIds.Construct();
        }

        void Clear()       // :98
        {
            maWorldEntityIds.Clear();
            maRaceCarEntityIds.Clear();
            maTrafficEntityIds.Clear();
            maPropEntityIds.Clear();
        }

        void Destruct() {} // :107

        Array<CgsSceneManager::EntityId, 4500u> maWorldEntityIds;    // :109
        Array<CgsSceneManager::EntityId, 32u>   maRaceCarEntityIds;  // :110
        Array<CgsSceneManager::EntityId, 650u>  maTrafficEntityIds;  // :111
        Array<CgsSceneManager::EntityId, 5400u> maPropEntityIds;     // :112
    };

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

        // The environment-settings tuning page (BrnSkyDebugComponent.h:55 --
        // the real EnvironmentSettings::DebugComponent subclass; Prepare
        // Constructs it against the environment manager).
        typedef BrnWorld::EnvironmentSettings::DebugComponent SkyDebugComponent;

        // ====================================================================
        // CgsModule base pure-virtual overrides (make WorldModule instantiable).
        // The X360 WorldModule's real lifecycle entry points take extra IO arguments
        // (Construct(const BrnCpuMonitors&) etc., per the DWARF); the no-arg overrides
        // exist only to satisfy the abstract CgsModule contract.
        // ====================================================================
        void Construct() override { CgsModule::ModuleSingleBuffered::Construct(); }
        bool Prepare()   override { return CgsModule::ModuleSingleBuffered::Prepare(); }
        // Release/Destruct: on the X360 the this-only virtual slots ARE the real
        // per-stage teardown bodies (@0x827BCE58 / @0x827BD0F0) -- declared below
        // with the lifecycle group, bodied in the cpp.
        bool Release()   override;
        void Destruct()  override;
        void Update()    override { CgsModule::ModuleSingleBuffered::Update(); }

        // ====================================================================
        // X360 WorldModule methods (DWARF-declared). BODIED so far: Construct,
        // Destruct, ExternalSceneQueriesUpdate, UpdatePhysicsNetworkCatchup,
        // LoadDistrictMap. The remaining spine/builders are added as their
        // pseudocode is transcribed (see scratch/wem_recon_checkpoint.md).
        // ====================================================================

        // DWARF :343 (virtual). @0x827CF540 -- construct the fleet, register the
        // world/physics perf monitors, copy the global CPU-monitor handle block.
        void Construct( const BrnGame::BrnCpuMonitors& lrCpuMonitors );

        // @0x827BD0F0 -- the virtual Destruct() body (declared with the base
        // overrides above): destruct eight sub-modules (AI, race car, traffic,
        // world entity, prop, trigger, physics, scene -- the X360 does NOT
        // destruct the crash module), clear the receiver queue, destruct the
        // environment map.

        // @0x827B06C8 -- forward the scene module's external-queries update.
        void ExternalSceneQueriesUpdate();

        // @0x827B06E0 -- forward the physics network catch-up (buffer-stack args
        // asserted non-null, then mPhysicsModule.UpdateNetworkCatchup).
        // @0x827D1CE8 -- the DISPATCH-thread render feed: junkyard-lighting latch,
        // shader-constant setup, effects generation, the frustum-result filter, and
        // the per-module GenerateDispatchLists calls (race car / traffic / WORLD /
        // prop, then the six env-map faces and the shadow cascades).
        void GenerateDispatchLists(
            CgsModule::IOBufferStack* lpInputBufferStack,
            CgsModule::IOBufferStack* lpOutputBufferStack,
            BrnWorldIO::DispatchInputBuffer* lpDispatchInputBuffer,
            BrnWorldIO::DispatchOutputBuffer* lpDispatchOutputBuffer,
            const BrnUpdateSet* lpUpdateSet );

        // @0x827DADF8 -- stage this frame's frustum-test queries for the scene
        // manager: the main camera frustum, the six environment-map face frusta
        // (alternating halves under the 30Hz env-map policy) and the three shadow
        // cascades, then hand them to the frustum-test job system.
        void GenerateFrustumQueries(
            CgsModule::IOBufferStack* lpInputBufferStack,
            CgsModule::IOBufferStack* lpOutputBufferStack,
            const BrnWorldIO::DispatchInputBuffer* lpDispatchInputBuffer,
            BrnUpdateSet lUpdateSet );

        // @0x827C3C58 -- the per-frame POST-SCENE entity-module spine: for each
        // entity module, bridge the crash/cross-module state in, run its post-scene
        // update, then round-trip its scene queries through the scene manager and
        // feed the results back into the module's pre-physics input.
        void EntityModulePostSceneUpdate(
            CgsModule::IOBufferStack* lpInputBufferStack,
            CgsModule::IOBufferStack* lpOutputBufferStack,
            TriggerEntityModuleIO::InputBuffer_PrePhysics* lpTriggerInputBuffer_PrePhysics,
            TriggerEntityModuleIO::InputBuffer_PostScene* lpTriggerInputBuffer_PostScene,
            BrnTraffic::BrnTrafficIO::InputBuffer_PostScene* lpTrafficInputBuffer_PostScene,
            const BrnTraffic::BrnTrafficIO::TrafficToRaceCarInterface_PreScene* lpTrafficToRaceCarInterface_PreScene,
            BrnTraffic::BrnTrafficIO::OutputBuffer_PostScene* lpTrafficOutputBuffer_PostScene,
            BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics* lpTrafficInputBuffer_PostPhysics,
            BrnTraffic::BrnTrafficIO::InputBuffer_PrePhysics* lpTrafficInputBuffer_PrePhysics,
            RaceCarEntityModuleIO::InputBuffer_PostScene* lpRaceCarInputBuffer_PostScene,
            RaceCarEntityModuleIO::OutputBuffer_PostScene* lpRaceCarOutputBuffer_PostScene,
            RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpRaceCarInputBuffer_PrePhysics,
            const CrashModuleIO::OutputBuffer_PostScene* lpCrashOutputBuffer_PostScene,
            PropEntityIO::InputBuffer_PostScene* lpPropInputBuffer_PostScene,
            PropEntityIO::OutputBuffer_PostScene* lpPropOutputBuffer_PostScene,
            BrnUpdateSet lUpdateSet );

        // @0x827BD5B8 -- the per-frame PRE-PHYSICS entity-module spine: bridge
        // scene contacts + cross-module state into each entity module, then tick
        // race car -> prop -> traffic -> trigger (each bracketed by its CPU
        // monitors). The X360 takes the full 40-buffer frame set; the buffers this
        // body actually touches are named here (the rest are the caller's other
        // phase buffers -- see WorldModule::Update).
        void EntityModulePrePhysicsUpdate(
            CgsModule::IOBufferStack* lpInputBufferStack,
            CgsModule::IOBufferStack* lpOutputBufferStack,
            TriggerEntityModuleIO::InputBuffer_PrePhysics* lpTriggerModuleInputBuffer_PrePhysics,
            TriggerEntityModuleIO::OutputBuffer_PrePhysics* lpTriggerModuleOutputBuffer_PrePhysics,
            const CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneContactsFromWorld,
            BrnTraffic::BrnTrafficIO::InputBuffer_PrePhysics* lpTrafficInputBuffer_PrePhysics,
            BrnTraffic::BrnTrafficIO::OutputBuffer_PrePhysics* lpTrafficOutputBuffer_PrePhysics,
            const BrnTraffic::BrnTrafficIO::OutputBuffer_PostScene* lpTrafficOutputBuffer_PostScene,
            RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpRaceCarInputBuffer_PrePhysics,
            RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpRaceCarOutputBuffer_PrePhysics,
            PropEntityIO::InputBuffer_PrePhysics* lpPropInputBuffer_PrePhysics,
            PropEntityIO::OutputBuffer_PrePhysics* lpPropOutputBuffer_PrePhysics,
            WorldEntityIO::InputBuffer_PostPhysics* lpWorldInputBuffer_PrePhysics,
            WorldEntityIO::OutputBuffer_PostPhysics* lpWorldOutputBuffer_PrePhysics,
            BrnUpdateSet lUpdateSet );

        // @0x827BCE58 -- the virtual Release() body (declared with the base
        // overrides above): the 13-stage world release chain (reverse of Prepare:
        // crash -> AI -> trigger -> prop -> world entity -> traffic -> race car ->
        // environment manager -> physics -> scene -> env map -> base module).
        // Resumable: false = call again.

        // @0x827C44D8 -- drain the world GameAction queue (player-car AI control,
        // debug toggles, game-mode car-control policy) and bridge the actions into
        // the physics + traffic modules.
        void HandleGameActions(
            BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
            BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics* lpTrafficModuleInputBuffer,
            void* lpUnusedA, void* lpUnusedB,
            const BrnWorldIO::UpdateInputBuffer* lpWorldInput );

        void UpdatePhysicsNetworkCatchup(
            void* lpInputBufferStack, void* lpOutputBufferStack,
            s32 liCatchupSteps, void* lpPhysicsModuleOutputBuffer, s32 liFlags );

        // @0x827D53B0 -- the 15-stage world prepare chain: base module -> vault +
        // district map -> scene manager (allocators + culling table) -> physics ->
        // environment manager -> race car -> traffic -> WORLD ENTITY (starts the
        // world streaming) -> prop -> trigger -> AI -> crash -> env-map cameras ->
        // debug -> done. Resumable: one stage (or more) per call; false = call again.
        bool Prepare( CgsModule::IOBufferStack* lpInputBufferStack,
                      CgsModule::IOBufferStack* lpOutputBufferStack,
                      BrnWorldIO::UpdateOutputBuffer* lpUpdateOutputBuffer,
                      BrnResource::GameDataIO::AllocatorList* lpAllocatorList );

    private:
        // @0x827C96D8 -- the shadow-map dispatch feed (three cascades over the
        // filtered world/racecar/traffic/prop sets). DECODE PENDING: declared with
        // the buffer set the dispatch pass hands it; body next.
        void GenerateShadowMapDispatchLists(
            const BrnDirector::Camera::Camera* lpCameraInput,
            f32 lfLodZoomFactor,
            BrnWorldIO::DispatchInputBuffer* lpDispatchInputBuffer,
            BrnWorldIO::DispatchOutputBuffer* lpDispatchOutputBuffer,
            WorldEntityIO::InputBuffer_GenerateDispatchLists* lpWorldDispatchInput,
            RaceCarEntityModuleIO::InputBuffer_GenerateDispatchLists* lpRaceCarDispatchInput,
            BrnTraffic::BrnTrafficIO::InputBuffer_Dispatch* lpTrafficDispatchInput,
            PropEntityIO::InputBuffer_Dispatch* lpPropDispatchInput,
            FilteredEntityData* lpFilteredEntityData,
            const CgsSceneManager::SceneManagerIO::OutputBuffer::SceneQueryResultsQueue* lpResultsQueue,
            const CgsModule::Event* lpFrustumTestResult,
            s32 liResultSize,
            BrnTraffic::BrnTrafficIO::OutputBuffer_PreDispatch* lpTrafficRenderInfos );

        // ---- dispatch helpers (ledger-'reviewed' PHANTOMS -- no bodies were ever
        //      committed; declared for the dispatch feed, reconstruct from the
        //      addresses noted). ----
        // @0x827BDA60 -- split a frustum-test result event's entity ids by owner
        // category into the filtered-entity buffer.
        void FilterFrustumTestResults(
            const CgsModule::Event* lpFrustumTestResult,
            Array<CgsSceneManager::EntityId, 4500u>* lpWorldIds,
            Array<CgsSceneManager::EntityId, 32u>* lpRaceCarIds,
            Array<CgsSceneManager::EntityId, 650u>* lpTrafficIds,
            Array<CgsSceneManager::EntityId, 5400u>* lpPropIds );

        // @0x827C3778 -- derive this frame's per-vehicle LOD set from the render
        // budget (the KF_*_VEHICLE_LOD_DISTANCES policy tables). The X360 hands
        // it the camera position in v1.
        void CalculateVehicleLODs( Vector3 lvCameraPosition );

        // @0x827D1410 -- stage the frame's global shader constants before any
        // dispatch-list generation.
        void SetupShaderConstantsBeforeRendering(
            BrnShaderConstantsFrame* lpShaderConstantsFrame,
            f32 lfSimTime, f32 lfGameTime );

        // (called between the module dispatch feeds) -- forward the world module's
        // render-side state into the entity modules' dispatch inputs.
        void BridgeWorldModuleToEntityModules_Render(
            BrnTraffic::BrnTrafficIO::InputBuffer_Dispatch* lpTrafficDispatchInput,
            RaceCarEntityModuleIO::InputBuffer_GenerateDispatchLists* lpRaceCarDispatchInput,
            WorldEntityIO::InputBuffer_GenerateDispatchLists* lpWorldDispatchInput,
            PropEntityIO::InputBuffer_Dispatch* lpPropDispatchInput,
            const BrnWorldIO::DispatchInputBuffer* lpDispatchInputBuffer );

        // @0x827D3D08 -- the AttribSys world-vault streaming state machine
        // (load WorldVault.bin -> acquire "WorldVault" -> register with AttribSys).
        bool LoadAttribSysVault( BrnWorldIO::UpdateOutputBuffer* lpOutput );

        // Private District-map streaming state machine (DWARF cpp:2186). BODIED.
        bool LoadDistrictMap( BrnWorldIO::UpdateOutputBuffer* lpOutput );

        // ---- members (DWARF :277..:339 order; X360 offsets as comments) ------------
        EWorldPrepareStage    mePrepareStage;          // :277 (X360 +552)
        EWorldReleaseStage    meReleaseStage;          // :278 (+556)
        EResourceAcquireStage meVaultResourceStage;    // :279 (+560)
        DistrictMapLoadStage  meDistrictMapLoadStage;  // :280 (+564)

        // ---- the by-value sub-module fleet (DWARF :282..:299) ----------------------
        RaceCarEntityModule                        mRaceCarEntityModule;  // :282 (+640)
        BrnTraffic::TrafficEntityModule            mTrafficEntityModule;  // :283 (+100992)
        WorldEntityModule                          mWorldEntityModule;    // :284 (+598656)
        PropEntityModule                           mPropEntityModule;     // :285 (+629888)
        TriggerEntityModule                        mTriggerEntityModule;  // :286 (+1495168)
        BrnPhysics::PhysicsModule                  mPhysicsModule;        // :288 (+1561376)
        EnvironmentSettings::EnvironmentManager    mEnvironmentManager;   // :290 (+1994592)
        SkyDebugComponent                          mSkyDebugComponent;    // :292
        CgsSceneManager::SceneManagerModule        mSceneModule;          // :295 (+2002304)
        BrnAI::AIModule                            mAIModule;             // :297 (+5837568)
        CrashModule                                mCrashModule;          // :299 (+6160416)

        CgsModule::EventReceiverQueue<1024, 16>    mReceiverQueue;        // :301 (+6166216)

        bool                  mbResourcesLoaded;                 // :304 (+6167264)
        EResourceAcquireState meResourceState;                   // :305 (+6167268)
        EActiveRaceCarIndex   meLocalPlayerActiveRaceCarIndex;   // :307 (+6167272)
        f32                   mfLocalPlayerActiveRaceCarSpeed;   // :308 (+6167276)
        // BrnWorld::CarControl[8] (DWARF :309); the control-mode selector per active
        // car (Construct primes every slot to 1).
        s32                   maeCarControls[8];                 // :309 (+6167280)
        bool                  mbDEBUGPlayerCarAlwaysUnderAIControl; // :310
        s32                   mnDEBUGKBToStoreEachFrame;         // :313 (+6167312)
        bool                  mbStoreKBEachFrame;                // :314
        bool                  mabEnvMapFaceRender[6];            // :317
        bool                  mbRenderFirstEnvMapFaces;          // :318
        bool                  mb30hzEnvironmentMap;              // :319
        bool                  mbFirstRenderFrame;                // :320
        bool                  mbForceOnlyBackdrops;              // :322
        bool                  mbRenderBackdrops;                 // :323
        f32                   mfCarKeyLightMultiplier;           // :324 (+6167332, 1.175)
        f32                   mfCarAmbientLightMultiplier;       // :325 (+6167336, 1.175)

        WorldDebugComponent   mDebugComponent;                   // :330 (+6167344)

        CgsResource::ResourceHandle mAttribSysVaultResourceHandle; // :333
        CgsResource::ResourceHandle mDistrictMapResourceHandle;    // :334

        // ---- CPU perf-monitor handles (DWARF :222..:321; registered by Construct) --
        s32 miSceneManagerUpdatePM;                    // (+6167440)
        s32 miSceneManagerQueryPM;                     // (+6167444)
        s32 miSceneManagerFrustumPM;                   // (+6167448, primed -1)
        s32 miCrashModuleUpdatePM;                     // (+6167452)
        s32 miSceneModuleUpdateContactsPM;             // (+6167456)
        s32 miAIModuleUpdatePM;                        // (+6167460)
        s32 miPhysicsSummaryPM;                        // (+6167464)
        s32 miPhysicsBridgesPM;                        // (+6167468)
        s32 miPhysicsNetworkCatchupPM;                 // (+6167472)
        s32 miPhysicsModulePreSceneUpdatePM;           // (+6167476)
        s32 miPhysicsModuleGenerateSceneQueriesPM;     // (+6167480)
        s32 miPhysicsModuleUpdatePM;                   // (+6167484)
        s32 miPhysicsPropSummaryPM;                    // (+6167488)
        s32 miPhysicsPropBridgePM;                     // (+6167492)
        s32 miPhysicsPropPreSceneUpdatePM;             // (+6167496)
        s32 miPhysicsPropPrePhysicsUpdatePM;           // (+6167500)
        s32 miPhysicsPropPostPhysicsUpdatePM;          // (+6167504)
        s32 miPhysicsPropPostScenePM;                  // (+6167508)
        s32 miWorldModuleDataDumpPM;                   // (+6167512)
        s32 miRaceCarSceneModuleQueriesTrace;          // (+6167516, 0)
        s32 miTrafficSceneModuleQueriesTrace;          // (+6167520, 0)
        s32 miWorldSceneModuleQueriesTrace;            // (+6167524, 0)
        s32 miPropSceneModuleQueriesTrace;             // (+6167528, 0)
        s32 miTriggerSceneModuleQueriesTrace;          // (+6167532, 0)
        s32 miSceneContactsQueriesTrace;               // (+6167536, 0)
        s32 miSceneUpdateTrace;                        // (+6167540, 0)
        s32 miSceneManagerFrustumTestPM;               // (+6167544)
        s32 miSceneManagerFrustumTestStartJobsPM;      // (+6167548)
        s32 miSceneManagerFrustumTestWaitOnJobsPM;     // (+6167552)
        s32 miGenerateDispatchListsPM;                 // (+6167556)
        s32 miFrustumTestFilterPM;                     // (+6167560)
        s32 miPropGenerateDispListClearPM;             // (+6167564)
        s32 miTrafficGenerateDispListClearPM;          // (+6167568)
        s32 miRaceCarGenerateDispListClearPM;          // (+6167572)

        BrnGame::BrnCpuMonitors    mGlobalCpuMonitors; // (+6167576, 160-byte ctor copy)
        BrnGraphics::EnvironmentMap mEnvironmentMap;   // (+6168096)
        ShadowMap                  mShadowMap;         // (+6170336)
        // The last director camera the dispatch pass consumed (X360 +6167744;
        // GenerateDispatchLists copies the frame camera into it each dispatch).
        BrnDirector::Camera::Camera mLastCameraInput;

        // DWARF :336 (X360 +6175760): the world shader-LOD policy block --
        // GenerateDispatchLists refreshes it (the broadcast-splat of
        // mfShaderLod1NearDistance) and hands it to the world/prop dispatch feeds.
        ShaderLodInfo mShaderLodInfo;

        bool mbIsInJunkyard;                           // :339 (X360 +6175808)
    };
}
