// ============================================================================
// b5-decomp/src/GameSource/World/BrnWorldModule.cpp
//
// BrnWorld::WorldModule -- the World MODULE spine. See BrnWorldModule.h for the
// full scope/FLAG rationale.
//
// The X360 TU "GameSource/Unity/../World/BrnWorldModule.cpp" has 13 functions:
//   Construct, Destruct, EntityModulePostSceneUpdate, EntityModulePrePhysicsUpdate,
//   ExternalSceneQueriesUpdate, GenerateDispatchLists, GenerateFrustumQueries,
//   GenerateShadowMapDispatchLists, HandleGameActions, LoadDistrictMap, Prepare,
//   Release, UpdatePhysicsNetworkCatchup.
//
// BODIED (1):  LoadDistrictMap  -- faithful, through this TU's own named members +
//              committed deps (RequestInterface<4096>::LoadBundle, VariableEventQueue
//              <4096,16>::AddEvent, CgsResource::ID::HashString, the receiver-queue
//              accessors). All branches/stores/early-outs mirror X360 0x827D11D8.
//
// DECLARATION-ONLY + FLAG (12):  every other dossier function reaches a genuinely
//              un-homed dependency -- either it indexes the embedded sub-module fleet by
//              raw offset (Construct @0x827CF540, Destruct @0x827BD0F0, Release @0x827BCE58,
//              ExternalSceneQueriesUpdate @0x827B06C8, UpdatePhysicsNetworkCatchup @0x827B06E0,
//              EntityModulePostSceneUpdate @0x827C3C58, EntityModulePrePhysicsUpdate @0x827BD5B8,
//              HandleGameActions @0x827C44D8 -- the latter two also call the [todo] Bridge*
//              helpers that live in their own TUs), or it is a multi-stage VMX/VPU pipeline
//              (GenerateDispatchLists @0x827D1CE8, GenerateFrustumQueries @0x827DADF8,
//              GenerateShadowMapDispatchLists @0x827C96D8). Per AGENTS.md these are NOT
//              paraphrased to scalar and NOT poked by raw offset into committed aggregates;
//              they are recorded here with their X360 address + the exact reason they are
//              blocked, to be bodied once their sub-module/IO deps are homed.
// ============================================================================
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"   // CgsGraphics::ShaderConstantTable
#include "GameSource/Graphics/BrnShaderConstantsFrame.h"             // BrnShaderConstantsFrame
#include "GameShared/GameClasses/Module/CgsModuleUtils.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO_OutputBuffer.h"
#include "GameSource/Physics/BrnPhysicsModuleIO.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysSharedIO.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsSpatialPartitionManager.h"
#include "GameSource/World/Bridges/WorldBridgeEntityModulesToOutput.h"
#include "GameSource/World/Bridges/WorldBridgeToEntityModules.h"
#include "GameSource/World/Bridges/WorldBridgeSceneToEntityModules.h"
#include "GameSource/World/Bridges/WorldBridgeCrashToEntityModules.h"
#include "GameSource/World/Bridges/WorldBridgeEntityModulesToEntityModules.h"
#include "GameSource/World/Bridges/WorldBridgeEntityModulesToScene.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameSource/World/BrnWorldModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // VariableEventQueue<4096,16>::AddEvent
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"       // CgsResource::ID::HashString
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"       // BrnResource::GameDataIO::RequestInterface<4096>

// The global runtime shader-constant register (X360 symbol mShaderConstantTable;
// same extern as the world-entity TU -- the defining home lands with the shader TU).
namespace CgsGraphics { extern ShaderConstantTable mShaderConstantTable; }

namespace BrnWorld
{

static CgsSceneManager::SceneQueryId KA_FRUSTUM_QUERY_IDS[11];
static CgsGraphics::Camera gFrustumQueryCamera;

// The dispatch-pass camera (X360 file static at 0x8300FB40).
static CgsGraphics::Camera gDispatchCamera;

// The world dispatch/sort list ids the X360 passes in registers to the world
// module's dispatch feed (dropped by the decompiler at the call site -- FLAG:
// role-correct values pinned at postmortem from the 0x827D1CE8 asm).
static const s32 KI_WORLD_OPAQUE_LIST = 2;
static const s32 KI_WORLD_SORT_LAYER  = 19;
static const s32 KI_WORLD_SORT_KEY    = 20;
static const s32 KI_WORLD_PREZ_LIST   = 15;

// Clamp a colour to [0, white level] per channel (the X360 vmaxfp/vminfp pair).
static void ClampColourToWhiteLevel( Vector3& lrColour, f32 lfWhiteLevel )
{
    lrColour.x = ( lrColour.x < 0.0f ) ? 0.0f : ( lrColour.x > lfWhiteLevel ? lfWhiteLevel : lrColour.x );
    lrColour.y = ( lrColour.y < 0.0f ) ? 0.0f : ( lrColour.y > lfWhiteLevel ? lfWhiteLevel : lrColour.y );
    lrColour.z = ( lrColour.z < 0.0f ) ? 0.0f : ( lrColour.z > lfWhiteLevel ? lfWhiteLevel : lrColour.z );
}

// Scale every irradiance row by the ambient multiplier (the X360 vspltw+vmulfp
// row pipeline).
static void ScaleIrradiance( Matrix44& lrIrradiance, f32 lfScale )
{
    f32* lpfRows = &lrIrradiance.xAxis.x;
    for ( s32 liLane = 0; liLane < 16; liLane++ )
    {
        lpfRows[ liLane ] *= lfScale;
    }
}


    // ------------------------------------------------------------------------
    // (The interim cpp-local UpdateOutputBuffer accessor slice was retired 2026-07-24:
    // the real BrnWorldIO::UpdateOutputBuffer home (BrnWorldModuleIO.h, done TU) now
    // provides GetResourceRequestResourceInterface / GetAttribSysVaultRequestInterface.)


    // ========================================================================
    // WorldModule::LoadDistrictMap  @ X360 0x827D11D8   [BODIED]
    //
    // The Districts.dat streaming state machine. Drives meDistrictMapLoadStage through
    //   REQUEST -> RESPONSE -> ACQUIRE_REQUEST -> ACQUIRE_RESPONSE -> DONE,
    // returning false until DONE (stage 4). Each stage write-locks the output buffer,
    // does its one step, unlocks, and returns. (X360 wraps the whole switch in a single
    // LockForWrite/UnlockForWrite per stage -- mirrored exactly below.)
    // ========================================================================
    bool WorldModule::LoadDistrictMap( BrnWorldIO::UpdateOutputBuffer* lpOutput )
    {
        CGS_ASSERT(lpOutput, "lpOutput");

        lpOutput->LockForWrite();

        switch (meDistrictMapLoadStage)
        {
            case E_DISTRICT_MAP_LOAD_REQUEST:
            {
                // Clear the response receiver queue, then push a LoadBundle("Districts.dat",
                // pool 5) request onto the output buffer's request interface.
                mReceiverQueue.Clear();
                BrnResource::GameDataIO::RequestInterface<4096>* lpRequest =
                    lpOutput->GetResourceRequestResourceInterface();
                lpRequest->LoadBundle(&mReceiverQueue, /*liEventId*/ 1, /*liPoolId*/ 5,
                                      "Districts.dat", /*lbUseHDCache*/ false);
                meDistrictMapLoadStage = E_DISTRICT_MAP_LOAD_RESPONSE;
                lpOutput->UnlockForWrite();
                return false;
            }

            case E_DISTRICT_MAP_LOAD_RESPONSE:
            {
                // Wait for the load to report at least one response event, then advance.
                if (mReceiverQueue.GetCount() <= 0)
                {
                    lpOutput->UnlockForWrite();
                    return false;
                }
                meDistrictMapLoadStage = E_DISTRICT_MAP_ACQUIRE_REQUEST;
                lpOutput->UnlockForWrite();
                return false;
            }

            case E_DISTRICT_MAP_ACQUIRE_REQUEST:
            {
                // Clear the queue and push a GetGameDataEvent (type 24) acquiring the loaded
                // "Districts" data from pool 5. The X360 builds the event payload on the stack
                // as { &mReceiverQueue, 1, 5 (pool), (HashString("Districts") | (5 << 32)) } and pushes
                // it via VariableEventQueue<4096,16>::AddEvent(payload, type=4, size=24).
                mReceiverQueue.Clear();
                BrnResource::GameDataIO::RequestInterface<4096>* lpRequest =
                    lpOutput->GetResourceRequestResourceInterface();

                // CONSOLE payload order (asm 0x827D11D8): v9[0]=&queue, v9[1]=1, v9[2]=5 (pool),
                // then the u64 v10 = HashString("Districts")|0x500000000 at sp+0x60. So poolId
                // lands at +8 and the u64 resourceId at +16 (NOT the reverse).
                struct AcquireEvent
                {
                    CgsModule::BaseEventReceiverQueue* mpReceiverQueue; // CONSOLE +0
                    s32                                miEventId;       // CONSOLE +4  (=1)
                    s32                                miPoolId;        // CONSOLE +8  (=5)
                    s32                                mi_pad;          // CONSOLE +12
                    u64                                muResourceId;    // CONSOLE +16 (id | pool<<32)
                } lEvent;
                lEvent.mpReceiverQueue = &mReceiverQueue;
                lEvent.miEventId       = 1;
                lEvent.miPoolId        = 5;
                lEvent.mi_pad          = 0;
                lEvent.muResourceId    =
                    static_cast<u64>(static_cast<u32>(CgsResource::ID::HashString(
                        reinterpret_cast<const u8*>("Districts"))))
                    | 0x500000000ULL;   // pool 5 in the high dword

                // The request interface's queue IS a VariableEventQueue<4096,16> (RequestQueue
                // <4096> -> ResourceRequestQueue<4096> -> VariableEventQueue<4096,16>); the X360
                // pushes the acquire event straight onto it via the 3-arg AddEvent.
                lpRequest->mRequestQueue.AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lEvent), /*liType*/ 4, /*liSize*/ 24);

                meDistrictMapLoadStage = E_DISTRICT_MAP_ACQUIRE_RESPONSE;
                lpOutput->UnlockForWrite();
                return false;
            }

            case E_DISTRICT_MAP_ACQUIRE_RESPONSE:
            {
                // Wait for the acquire response, then capture the resolved resource handle from
                // the first response event's payload (X360 reads two dwords at payload + 24 into
                // mDistrictMapResourceHandle).
                if (mReceiverQueue.GetCount() <= 0)
                {
                    lpOutput->UnlockForWrite();
                    return false;
                }
                meDistrictMapLoadStage = E_DISTRICT_MAP_DONE;

                const CgsModule::Event* lpEventData = nullptr;
                s32 liSize = 0;
                // X360: v7 = (count>0) ? mpBuffer + miStartOffset + 8 : 0  (== the first event's
                // payload pointer) -- exactly what GetFirstEvent returns. The handle is at +24.
                mReceiverQueue.GetFirstEvent(&lpEventData, &liSize);

                const u32* lpPayload =
                    lpEventData ? reinterpret_cast<const u32*>(lpEventData) : nullptr;
                // FLAG: the +24 handle dwords live in the opaque LoadGameDataResponse payload
                // (external, forward-declared) -- read by attested offset (allowed per AGENTS.md).
                const u32* lpHandleWords =
                    reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(lpPayload) + 24);
                mDistrictMapResourceHandle.mpResourceMemory =
                    reinterpret_cast<void*>(static_cast<uintptr_t>(lpHandleWords[0]));
                mDistrictMapResourceHandle.mpSourceEntry =
                    reinterpret_cast<CgsResource::Entry*>(static_cast<uintptr_t>(lpHandleWords[1]));

                lpOutput->UnlockForWrite();
                return false;
            }

            case E_DISTRICT_MAP_DONE:
            {
                lpOutput->UnlockForWrite();
                return true;
            }

            default:
            {
                CGS_ASSERT(false, "Unknown meDistrictMapLoadStage");
                lpOutput->UnlockForWrite();
                return false;
            }
        }
    }

// ============================================================================
// BrnWorld::ShaderLodInfo -- Construct/Update (DWARF BrnShaderLodInfo.h:43/:46;
// the header notes both are the owning world-module TU's work). The X360
// inlines both: Construct's default block inside WorldModule::Construct
// (@0x827CF540, +6175760..+6175804) and Update's broadcast splat at the top of
// WorldModule::GenerateDispatchLists (@0x827D1CE8: v174 = the scalar near
// distance, vspltw lane 0, store to +6175760).
// ============================================================================
void
ShaderLodInfo::Construct()
{
    mShaderLod1NearDistance  = Vector4{ 0.0f, 0.0f, 0.0f, 0.0f };
    mMaxBelievableRadius     = Vector4{ 0.0f, 0.0f, 0.0f, 0.0f };
    mfShaderLod1NearDistance = 20.0f;
    miEnvMapTechnique        = 0;
    miOverrideTechnique      = -1;
    mbUseShaderLod           = false;
}

void
ShaderLodInfo::Update()
{
    // The broadcast splat: the tuned scalar into every lane of the SIMD member
    // the dispatch feeds read.
    mShaderLod1NearDistance = Vector4{ mfShaderLod1NearDistance,
                                       mfShaderLod1NearDistance,
                                       mfShaderLod1NearDistance,
                                       mfShaderLod1NearDistance };
}

// ============================================================================
// Construct  @ 0x827CF540  (DWARF :343 -- Construct(const BrnCpuMonitors&))
//
// Registers the world/physics CPU perf monitors, copies the global CPU-monitor
// handle block, constructs the whole sub-module fleet (X360 virtual-dispatch
// order preserved), then primes the module state. Page ids: 4 == the world
// perf-mon page (KE_WORLD_PERFMON_PAGE), 6 == the physics sub-page.
// ============================================================================
void
WorldModule::Construct( const BrnGame::BrnCpuMonitors& lrCpuMonitors )
{
    CgsModule::ModuleSingleBuffered::Construct();

    mePrepareStage = eWorldPrepareStart;
    meReleaseStage = eWorldReleaseDone;

    {
        using namespace CgsDev;

        miSceneManagerUpdatePM = PerfMonCpu::AddMonitor(
            "Scene manager update", static_cast<PerfMonCpuPage>( 4 ), false, 10.0f, true );
        CGS_ASSERT( miSceneManagerUpdatePM >= 0, "miSceneManagerUpdatePM >= 0" );

        miSceneManagerQueryPM = PerfMonCpu::AddMonitor(
            "Scene manager queries", static_cast<PerfMonCpuPage>( 4 ), false, 10.0f, true );
        CGS_ASSERT( miSceneManagerQueryPM >= 0, "miSceneManagerQueryPM >= 0" );

        miSceneManagerFrustumPM = -1;

        miCrashModuleUpdatePM = PerfMonCpu::AddMonitor(
            "Crash module update", static_cast<PerfMonCpuPage>( 4 ), false, 10.0f, true );
        CGS_ASSERT( miCrashModuleUpdatePM >= 0, "miCrashModuleUpdatePM >= 0" );

        miAIModuleUpdatePM = PerfMonCpu::AddMonitor(
            "AI module update", static_cast<PerfMonCpuPage>( 4 ), false, 10.0f, true );
        CGS_ASSERT( miAIModuleUpdatePM >= 0, "miAIModuleUpdatePM >= 0" );

        miPhysicsSummaryPM = PerfMonCpu::AddMonitor(
            "Total Physics", static_cast<PerfMonCpuPage>( 6 ), false, 20.0f, true );
        miPhysicsBridgesPM = PerfMonCpu::AddMonitor(
            "  Physics Bridges", static_cast<PerfMonCpuPage>( 6 ), false, 0.1f, true );
        miSceneModuleUpdateContactsPM = PerfMonCpu::AddMonitor(
            "  Scene upd contacts & tricache", static_cast<PerfMonCpuPage>( 6 ), false, 10.0f, true );
        miPhysicsModuleGenerateSceneQueriesPM = PerfMonCpu::AddMonitor(
            "  Physics gen scene queries", static_cast<PerfMonCpuPage>( 6 ), false, 10.0f, true );
        miPhysicsModulePreSceneUpdatePM = PerfMonCpu::AddMonitor(
            "  Physics pre scene update", static_cast<PerfMonCpuPage>( 6 ), false, 10.0f, true );
        miPhysicsNetworkCatchupPM = PerfMonCpu::AddMonitor(
            "  Network Catchup", static_cast<PerfMonCpuPage>( 6 ), false, 10.0f, true );

        miPhysicsPropSummaryPM = PerfMonCpu::AddMonitor(
            "  Total Prop Entity Module", static_cast<PerfMonCpuPage>( 6 ), false, 20.0f, true );
        miPhysicsPropBridgePM = PerfMonCpu::AddMonitor(
            "    Prop Bridges", static_cast<PerfMonCpuPage>( 6 ), false, 20.0f, true );
        miPhysicsPropPreSceneUpdatePM = PerfMonCpu::AddMonitor(
            "    Prop PreScene Update", static_cast<PerfMonCpuPage>( 6 ), false, 20.0f, true );
        mPropEntityModule.ConstructPreScenePerfMonitors();
        miPhysicsPropPrePhysicsUpdatePM = PerfMonCpu::AddMonitor(
            "    Prop PrePhysics Update", static_cast<PerfMonCpuPage>( 6 ), false, 20.0f, true );
        miPhysicsPropPostPhysicsUpdatePM = PerfMonCpu::AddMonitor(
            "    Prop PostPhysics Update", static_cast<PerfMonCpuPage>( 6 ), false, 20.0f, true );
        mPropEntityModule.ConstructPostPhysicsPerfMonitors();
        miPhysicsPropPostScenePM = PerfMonCpu::AddMonitor(
            "    Prop PostScene Update", static_cast<PerfMonCpuPage>( 6 ), false, 20.0f, true );
        miPhysicsModuleUpdatePM = PerfMonCpu::AddMonitor(
            "  Total Physics update", static_cast<PerfMonCpuPage>( 6 ), false, 10.0f, true );

        miWorldModuleDataDumpPM = PerfMonCpu::AddMonitor(
            "File systemd dump update", static_cast<PerfMonCpuPage>( 4 ), false, 10.0f, true );
        CGS_ASSERT( miWorldModuleDataDumpPM >= 0, "miWorldModuleDataDumpPM >= 0" );

        miRaceCarSceneModuleQueriesTrace = 0;
        miTrafficSceneModuleQueriesTrace = 0;
        miWorldSceneModuleQueriesTrace = 0;
        miPropSceneModuleQueriesTrace = 0;
        miTriggerSceneModuleQueriesTrace = 0;
        miSceneContactsQueriesTrace = 0;
        miSceneUpdateTrace = 0;

        miSceneManagerFrustumTestPM = PerfMonCpu::AddMonitor(
            "Scene manager frustum tests", static_cast<PerfMonCpuPage>( 4 ), false, 10.0f, false );
        miSceneManagerFrustumTestStartJobsPM = PerfMonCpu::AddMonitor(
            "  Start Frustum Test Jobs", static_cast<PerfMonCpuPage>( 4 ), false, 10.0f, false );
        miSceneManagerFrustumTestWaitOnJobsPM = PerfMonCpu::AddMonitor(
            "  Wait on Frustum Test Jobs", static_cast<PerfMonCpuPage>( 4 ), false, 10.0f, false );
        miGenerateDispatchListsPM = PerfMonCpu::AddMonitor(
            "WorldGenerateDispatchLists", static_cast<PerfMonCpuPage>( 4 ), false, 10.0f, false );
        miFrustumTestFilterPM = PerfMonCpu::AddMonitor(
            "FrustumTestFilter", static_cast<PerfMonCpuPage>( 4 ), false, 10.0f, false );
        miPropGenerateDispListClearPM = PerfMonCpu::AddMonitor(
            "Generate Prop Dist List", static_cast<PerfMonCpuPage>( 4 ), false, 10.0f, false );
        miTrafficGenerateDispListClearPM = PerfMonCpu::AddMonitor(
            "Generate Traffic Dist List", static_cast<PerfMonCpuPage>( 4 ), false, 10.0f, false );
        miRaceCarGenerateDispListClearPM = PerfMonCpu::AddMonitor(
            "Generate RaceCar Dist List", static_cast<PerfMonCpuPage>( 4 ), false, 10.0f, false );
    }

    mGlobalCpuMonitors = lrCpuMonitors;   // X360: 160-byte copy @ +6167576

    // The fleet, in the X360 virtual-dispatch order.
    mSceneModule.Construct();
    mPhysicsModule.Construct();
    mRaceCarEntityModule.Construct();
    mTrafficEntityModule.Construct();
    mWorldEntityModule.Construct();
    mPropEntityModule.Construct();
    mTriggerEntityModule.Construct();
    mAIModule.Construct();
    mCrashModule.Construct();

    mEnvironmentMap.Construct();
    mEnvironmentManager.Construct();
    mShadowMap.Construct();
    mDebugComponent.Construct( this );

    mbResourcesLoaded = false;
    meResourceState = eResourceAcquireStateNotStarted;

    mReceiverQueue.Construct();

    meVaultResourceStage = E_RESOURCESTAGE_START;
    meDistrictMapLoadStage = E_DISTRICT_MAP_LOAD_REQUEST;

    mfLocalPlayerActiveRaceCarSpeed = 0.0f;
    meLocalPlayerActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>( -1 );

    for ( s32 liI = 0; liI < 8; liI++ )
    {
        // (the X360 walks the slots with the BurnoutConstants.h:39 enum-bound assert)
        maeCarControls[ liI ] = 1;
    }

    mbDEBUGPlayerCarAlwaysUnderAIControl = false;   // X360 +6167312 (int store 0)
    mnDEBUGKBToStoreEachFrame = 32;                 // X360 +6167316
    mbStoreKBEachFrame = false;                     // X360 +6167320
    mbRenderFirstEnvMapFaces = true;                // X360 +6167327
    mb30hzEnvironmentMap = false;                   // X360 +6167328
    mbFirstRenderFrame = true;                      // X360 +6167329
    mbForceOnlyBackdrops = false;                   // X360 +6167330
    mbRenderBackdrops = true;                       // X360 +6167331
    mfCarKeyLightMultiplier = 1.175f;               // X360 +6167332
    mfCarAmbientLightMultiplier = 1.175f;           // X360 +6167336

    // The shader-LOD policy defaults (X360 +6175760..+6175804: the inlined
    // ShaderLodInfo::Construct default block -- two zero splats, near distance
    // 20.0, env-map technique 0, override -1, shader LOD off). The span is
    // mShaderLodInfo (DWARF pins it at +6175760); mLastCameraInput (+6167744)
    // is NOT touched by the X360 Construct.
    mShaderLodInfo.Construct();

    mbIsInJunkyard = false;                         // X360 +6175808

    { mbIsNewModule = true; }
}

// ============================================================================
// Destruct  @ 0x827BD0F0  (the this-only virtual slot)
//
// The X360 destructs EIGHT sub-modules (no crash-module destruct), clears the
// receiver queue and tears down the environment map.
// ============================================================================
void
WorldModule::Destruct()
{
    CgsModule::ModuleSingleBuffered::Destruct();

    mAIModule.Destruct();
    mRaceCarEntityModule.Destruct();
    mTrafficEntityModule.Destruct();
    mWorldEntityModule.Destruct();
    mPropEntityModule.Destruct();
    mTriggerEntityModule.Destruct();
    mPhysicsModule.Destruct();
    mSceneModule.Destruct();

    mReceiverQueue.Clear();

    mEnvironmentMap.Destruct();
}

// ============================================================================
// ExternalSceneQueriesUpdate  @ 0x827B06C8
// ============================================================================
void
WorldModule::ExternalSceneQueriesUpdate()
{
    mSceneModule.ExternalSceneQueriesUpdate();
}

// ============================================================================
// UpdatePhysicsNetworkCatchup  @ 0x827B06E0
// ============================================================================
void
WorldModule::UpdatePhysicsNetworkCatchup(
    void* lpInputBufferStack, void* lpOutputBufferStack,
    s32 liCatchupSteps, void* lpPhysicsModuleOutputBuffer, s32 liFlags )
{
    CGS_ASSERT( lpInputBufferStack != 0, "lpInputBufferStack != NULL" );
    CGS_ASSERT( lpOutputBufferStack != 0, "lpOutputBufferStack != NULL" );
    CGS_ASSERT( lpPhysicsModuleOutputBuffer != 0, "lpPhysicsModuleOutputBuffer != NULL" );

    mPhysicsModule.UpdateNetworkCatchup( liCatchupSteps, liFlags );
}


// ============================================================================
// LoadAttribSysVault  @ 0x827D3D08
//
// The AttribSys world-vault streaming machine (meVaultResourceStage):
//   START            -> LoadBundle "WorldVault.bin" (event id 1, pool 7)
//   LOADING_VAULT    -> on reply, acquire "WorldVault" (type-4 request; the id
//                       carries the pool in its upper word, X360 | 0x7'00000000)
//   ACQUIRING_VAULT  -> on reply, capture the resource handle + register the
//                       vault with the AttribSys request pipe
//   REGISTERING_VAULT-> on reply, done
// ============================================================================
bool
WorldModule::LoadAttribSysVault( BrnWorldIO::UpdateOutputBuffer* lpOutput )
{
    lpOutput->LockForWrite();
    CGS_ASSERT( lpOutput, "lpOutput" );

    switch ( meVaultResourceStage )
    {
        case E_RESOURCESTAGE_START:
        {
            lpOutput->GetResourceRequestResourceInterface()->LoadBundle(
                &mReceiverQueue, 1, 7, "WorldVault.bin", false );
            meVaultResourceStage = E_RESOURCESTAGE_LOADING_VAULT;
        }
        // fall through

        case E_RESOURCESTAGE_LOADING_VAULT:
        {
            if ( mReceiverQueue.GetLength() <= 0 )
            {
                break;
            }

            CgsResource::Events::AcquireResourceRequest lRequest;
            lRequest.mpUser     = &mReceiverQueue;
            lRequest.miEventId  = 1;
            lRequest.miPoolId   = 7;
            // X360 id word: HashString("WorldVault") | (7ull << 32).
            lRequest.mResourceId.SetHash(
                static_cast<u64>( static_cast<u32>( CgsResource::ID::HashString(
                    reinterpret_cast<const u8*>( "WorldVault" ) ) ) )
                | 0x700000000ull );
            lRequest.mbCheckRefCount = false;

            lpOutput->GetResourceRequestResourceInterface()->mRequestQueue.AddEvent(
                &lRequest, 4 );

            mReceiverQueue.Clear();
            meVaultResourceStage = E_RESOURCESTAGE_ACQUIRING_VAULT;
            break;
        }

        case E_RESOURCESTAGE_ACQUIRING_VAULT:
        {
            if ( mReceiverQueue.GetLength() <= 0 )
            {
                break;
            }

            const CgsModule::Event* lpEventData = 0;
            s32 liSize = 0;
            mReceiverQueue.GetFirstEvent( &lpEventData, &liSize );

            const CgsResource::Events::AcquireResourceResponse* lpResponse =
                reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>( lpEventData );

            mAttribSysVaultResourceHandle.mpResourceMemory = lpResponse->mpResourceMemory;
            mAttribSysVaultResourceHandle.mpSourceEntry    = lpResponse->mpSourceEntry;

            lpOutput->GetAttribSysVaultRequestInterface()->RegisterVault(
                &mReceiverQueue, mAttribSysVaultResourceHandle,
                static_cast<CgsAttribSys::AttribSysIO::EAttribSysVaultType>( 1 ) );

            mReceiverQueue.Clear();
            meVaultResourceStage = E_RESOURCESTAGE_REGISTERING_VAULT;
            break;
        }

        case E_RESOURCESTAGE_REGISTERING_VAULT:
        {
            if ( mReceiverQueue.GetLength() <= 0 )
            {
                break;
            }

            mReceiverQueue.Clear();
            meVaultResourceStage = E_RESOURCESTAGE_DONE;
            break;
        }

        case E_RESOURCESTAGE_DONE:
        {
            lpOutput->UnlockForWrite();
            return true;
        }

        default:
            break;
    }

    lpOutput->UnlockForWrite();
    return false;
}


// ============================================================================
// Prepare  @ 0x827D53B0
//
// The 15-stage world prepare chain (resumable; false = call again next frame):
//   MODULE -> RESOURCES (vault + district map) -> SCENE (allocators + culling
//   table + one scene tick) -> PHYSICS -> ENVIRONMENT MANAGER -> RACE CAR ->
//   TRAFFIC -> WORLD ENTITY (starts the world streaming) -> PROP -> TRIGGER ->
//   AI -> CRASH -> ENV-MAP CAMERAS -> DEBUG -> DONE.
// Every fail path bridges the sub-module's staged resource requests into the
// world output buffer before returning false, exactly as the X360 does.
// ============================================================================
bool
WorldModule::Prepare( CgsModule::IOBufferStack* lpInputBufferStack,
                      CgsModule::IOBufferStack* lpOutputBufferStack,
                      BrnWorldIO::UpdateOutputBuffer* lpUpdateOutputBuffer,
                      BrnResource::GameDataIO::AllocatorList* lpAllocatorList )
{
    switch ( mePrepareStage )
    {
        case eWorldPrepareStart:
        case eWorldPrepareModule:
        {
            mePrepareStage = eWorldPrepareModule;

            if ( !CgsModule::ModuleSingleBuffered::Prepare() )
            {
                return false;
            }

            mbResourcesLoaded = false;
            meResourceState = eResourceAcquireStateNotStarted;
            mReceiverQueue.Clear();
            mLastCameraInput.Clear();
        }
        // fall through

        case eWorldPrepareResources:
        {
            mePrepareStage = eWorldPrepareResources;

            if ( !LoadAttribSysVault( lpUpdateOutputBuffer ) ||
                 !LoadDistrictMap( lpUpdateOutputBuffer ) )
            {
                return false;
            }
        }
        // fall through

        case eWorldPrepareSceneModule:
        {
            mePrepareStage = eWorldPrepareSceneModule;

            // The world spatial partition (X360 literal block).
            CgsSceneManager::SpatialPartitionConstructParams lParams;
            lParams.meType        = static_cast<CgsSceneManager::ESpatialPartitionType>( 1 );
            lParams.miNumLevels   = 3;
            lParams.mOrigin.SetZero();
            lParams.mfWorldExtent = 11000.0f;
            lParams.mfLooseness   = 0.30000001f;
            lParams.miMaxEntries  = 32;
            lParams.miMaxDepth    = 10;
            lParams.miPad         = 0;

            rw::IResourceAllocator* lpSceneAllocator =
                lpAllocatorList->GetRWLinearResourceAllocator( 49 );
            CGS_ASSERT( lpSceneAllocator, "lpSceneAllocator" );

            rw::IResourceAllocator* lpPhysicsAllocator =
                lpAllocatorList->GetRWLinearResourceAllocator( 23 );
            CGS_ASSERT( lpPhysicsAllocator, "lpPhysicsAllocator" );

            CgsMemory::LinearMalloc* lpTriangleCollisionAllocator =
                lpAllocatorList->GetLinearAllocator( 61 );
            CGS_ASSERT( lpTriangleCollisionAllocator, "lpTriangleCollisionAllocator" );

            if ( !mSceneModule.Prepare( &lParams, lpSceneAllocator, lpPhysicsAllocator,
                                        lpTriangleCollisionAllocator ) )
            {
                return false;
            }

            // Prime the culling-group table with one scene tick.
            CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInput = 0;
            CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneOutput = 0;
            lpInputBufferStack->CreateIOBuffer( &lpSceneInput, "Scene" );
            lpOutputBufferStack->CreateIOBuffer( &lpSceneOutput, "Scene" );

            lpSceneInput->LockForWrite();
            {
                CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpScene =
                    lpSceneInput->GetInSceneUpdateInterface();

                lpScene->ClearCullingTable( 1 );
                lpScene->SetCullingGroupPair( 7, 1, 0 );
                lpScene->SetCullingGroupPair( 7, 6, 0 );
                lpScene->SetCullingGroupPair( 7, 5, 0 );
                lpScene->SetCullingGroupPair( 7, 3, 0 );
                lpScene->SetCullingGroupPair( 8, 1, 0 );
                lpScene->SetCullingGroupPair( 8, 6, 0 );
                lpScene->SetCullingGroupPair( 8, 5, 0 );
                lpScene->SetCullingGroupPair( 8, 3, 0 );
                lpScene->SetCullingGroupPair( 7, 9, 0 );
                lpScene->SetCullingGroupPair( 8, 9, 0 );
            }
            lpSceneInput->UnlockForWrite();

            mSceneModule.Update( lpInputBufferStack, lpOutputBufferStack,
                                 lpSceneInput, lpSceneOutput, true );

            lpOutputBufferStack->DestroyIOBuffer( &lpSceneOutput );
            lpInputBufferStack->DestroyIOBuffer( &lpSceneInput );
        }
        // fall through

        case eWorldPreparePhysicsModule:
        {
            mePrepareStage = eWorldPreparePhysicsModule;

            CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInput = 0;
            CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneOutput = 0;
            lpInputBufferStack->CreateIOBuffer( &lpSceneInput, "Scene" );
            lpOutputBufferStack->CreateIOBuffer( &lpSceneOutput, "Scene" );

            lpSceneInput->LockForWrite();
            const bool lbPhysicsPrepared = mPhysicsModule.Prepare(
                lpInputBufferStack, lpOutputBufferStack, lpSceneInput, lpAllocatorList );
            lpSceneInput->UnlockForWrite();

            mSceneModule.Update( lpInputBufferStack, lpOutputBufferStack,
                                 lpSceneInput, lpSceneOutput, true );

            lpOutputBufferStack->DestroyIOBuffer( &lpSceneOutput );
            lpInputBufferStack->DestroyIOBuffer( &lpSceneInput );

            if ( !lbPhysicsPrepared )
            {
                return false;
            }
        }
        // fall through

        case eWorldPrepareEnvironmentManager:
        {
            meVaultResourceStage = E_RESOURCESTAGE_LOADING_VAULT;   // X360 +560 = 1
            mePrepareStage = eWorldPrepareEnvironmentManager;

            if ( !mEnvironmentManager.Prepare( lpUpdateOutputBuffer ) )
            {
                return false;
            }

            // The environment-settings debug component rides the sky slot
            // (X360 @0x827D5880: Construct against the environment manager).
            mSkyDebugComponent.Construct( &mEnvironmentManager );
            mSkyDebugComponent.Register();
        }
        // fall through

        case eWorldPrepareRaceCarEntityModule:
        {
            mePrepareStage = eWorldPrepareRaceCarEntityModule;

            RaceCarEntityModuleIO::OutputBuffer_Prepare* lpRaceCarOutput = 0;
            lpOutputBufferStack->CreateIOBuffer( &lpRaceCarOutput, "RaceCar" );

            if ( !mRaceCarEntityModule.Prepare( mDistrictMapResourceHandle ) )
            {
                CgsModule::LockBuffersForIO( lpUpdateOutputBuffer, lpRaceCarOutput );
                ::WorldModule::BridgeRaceCarResourceRequestsToOutput_Prepare(
                    this, lpUpdateOutputBuffer, lpRaceCarOutput );
                CgsModule::UnlockBuffersForIO( lpUpdateOutputBuffer, lpRaceCarOutput );
                lpOutputBufferStack->DestroyIOBuffer( &lpRaceCarOutput );
                return false;
            }

            lpOutputBufferStack->DestroyIOBuffer( &lpRaceCarOutput );
        }
        // fall through

        case eWorldPrepareTrafficEntityModule:
        {
            mePrepareStage = eWorldPrepareTrafficEntityModule;

            BrnTraffic::BrnTrafficIO::OutputBuffer_Prepare* lpTrafficOutput = 0;
            lpOutputBufferStack->CreateIOBuffer( &lpTrafficOutput, "Traffic" );

            if ( !mTrafficEntityModule.Prepare( lpTrafficOutput ) )
            {
                CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInput = 0;
                CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneOutput = 0;
                lpInputBufferStack->CreateIOBuffer( &lpSceneInput, "Scene" );
                lpOutputBufferStack->CreateIOBuffer( &lpSceneOutput, "Scene" );

                CgsModule::LockBuffersForIO( lpSceneInput, lpTrafficOutput );
                ::WorldModule::BridgeTrafficModuleToSceneModule_Prepare(
                    this, lpSceneInput, lpTrafficOutput );
                CgsModule::UnlockBuffersForIO( lpSceneInput, lpTrafficOutput );

                mSceneModule.Update( lpInputBufferStack, lpOutputBufferStack,
                                     lpSceneInput, lpSceneOutput, true );

                lpOutputBufferStack->DestroyIOBuffer( &lpSceneOutput );
                lpInputBufferStack->DestroyIOBuffer( &lpSceneInput );

                CgsModule::LockBuffersForIO( lpUpdateOutputBuffer, lpTrafficOutput );
                ::WorldModule::BridgeTrafficResourceRequestsToOutput(
                    this, lpUpdateOutputBuffer, lpTrafficOutput );
                CgsModule::UnlockBuffersForIO( lpUpdateOutputBuffer, lpTrafficOutput );

                lpOutputBufferStack->DestroyIOBuffer( &lpTrafficOutput );
                return false;
            }

            lpOutputBufferStack->DestroyIOBuffer( &lpTrafficOutput );
        }
        // fall through

        case eWorldPrepareWorldEntityModule:
        {
            mePrepareStage = eWorldPrepareWorldEntityModule;

            WorldEntityIO::OutputBuffer_Prepare* lpWorldEntityOutput = 0;
            CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInput = 0;
            CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneOutput = 0;
            lpOutputBufferStack->CreateIOBuffer( &lpWorldEntityOutput, "WorldEntityPrepare" );
            lpInputBufferStack->CreateIOBuffer( &lpSceneInput, "Scene" );
            lpOutputBufferStack->CreateIOBuffer( &lpSceneOutput, "Scene" );

            if ( !mWorldEntityModule.Prepare( lpWorldEntityOutput ) )
            {
                CgsModule::LockBuffersForIO( lpUpdateOutputBuffer, lpWorldEntityOutput );
                ::WorldModule::BridgeWorldResourceRequestsToOutput_Prepare(
                    this, lpUpdateOutputBuffer, lpWorldEntityOutput );
                CgsModule::UnlockBuffersForIO( lpUpdateOutputBuffer, lpWorldEntityOutput );

                CgsModule::LockBuffersForIO( lpSceneInput, lpWorldEntityOutput );
                lpSceneInput->GetInSceneUpdateInterface()->Append(
                    *lpWorldEntityOutput->GetSceneInputInterface() );
                CgsModule::UnlockBuffersForIO( lpSceneInput, lpWorldEntityOutput );

                mSceneModule.Update( lpInputBufferStack, lpOutputBufferStack,
                                     lpSceneInput, lpSceneOutput, true );

                lpOutputBufferStack->DestroyIOBuffer( &lpSceneOutput );
                lpInputBufferStack->DestroyIOBuffer( &lpSceneInput );
                lpOutputBufferStack->DestroyIOBuffer( &lpWorldEntityOutput );
                return false;
            }

            BrnPhysics::Vehicle::VehicleManager::ReadSurfaceProperties();

            lpOutputBufferStack->DestroyIOBuffer( &lpSceneOutput );
            lpInputBufferStack->DestroyIOBuffer( &lpSceneInput );
            lpOutputBufferStack->DestroyIOBuffer( &lpWorldEntityOutput );
        }
        // fall through

        case eWorldPreparePropEntityModule:
        {
            mePrepareStage = eWorldPreparePropEntityModule;

            rw::IResourceAllocator* lpPhysicsAllocator =
                lpAllocatorList->GetRWLinearResourceAllocator( 23 );
            CGS_ASSERT( lpPhysicsAllocator, "lpPhysicsAllocator" );

            PropEntityIO::OutputBuffer_Prepare* lpPropOutput = 0;
            lpOutputBufferStack->CreateIOBuffer( &lpPropOutput, "Prop" );

            CgsModule::LockBuffersForIO( lpPropOutput );
            const bool lbPropPrepared =
                mPropEntityModule.Prepare( lpPropOutput, lpPhysicsAllocator );
            CgsModule::UnlockBuffersForIO( lpPropOutput );

            if ( !lbPropPrepared )
            {
                CgsModule::LockBuffersForIO( lpUpdateOutputBuffer, lpPropOutput );
                ::WorldModule::BridgePropResourceRequestsToOutput_Prepare(
                    this, lpUpdateOutputBuffer, lpPropOutput );
                CgsModule::UnlockBuffersForIO( lpUpdateOutputBuffer, lpPropOutput );
                lpOutputBufferStack->DestroyIOBuffer( &lpPropOutput );
                return false;
            }

            {
                CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInput = 0;
                CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneOutput = 0;
                lpInputBufferStack->CreateIOBuffer( &lpSceneInput, "Scene" );
                lpOutputBufferStack->CreateIOBuffer( &lpSceneOutput, "Scene" );

                CgsModule::LockBuffersForIO( lpSceneInput, lpPropOutput );
                ::WorldModule::BridgePropModuleToSceneModule_Prepare(
                    this, lpSceneInput, lpPropOutput );
                CgsModule::UnlockBuffersForIO( lpSceneInput, lpPropOutput );

                mSceneModule.Update( lpInputBufferStack, lpOutputBufferStack,
                                     lpSceneInput, lpSceneOutput, true );

                lpOutputBufferStack->DestroyIOBuffer( &lpSceneOutput );
                lpInputBufferStack->DestroyIOBuffer( &lpSceneInput );
            }

            {
                BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsInput = 0;
                lpInputBufferStack->CreateIOBuffer( &lpPhysicsInput, "Physics" );

                CgsModule::LockBuffersForIO( lpPhysicsInput, lpPropOutput );
                ::WorldModule::BridgePropModuleToPhysicsModule_Prepare(
                    this, lpPhysicsInput, lpPropOutput );
                CgsModule::UnlockBuffersForIO( lpPhysicsInput, lpPropOutput );

                mPhysicsModule.PropPrepareTypes( lpPhysicsInput );

                lpInputBufferStack->DestroyIOBuffer( &lpPhysicsInput );
            }

            lpOutputBufferStack->DestroyIOBuffer( &lpPropOutput );
        }
        // fall through

        case eWorldPrepareTriggerEntityModule:
        {
            mePrepareStage = eWorldPrepareTriggerEntityModule;

            if ( !mTriggerEntityModule.Prepare() )
            {
                return false;
            }
        }
        // fall through

        case eWorldPrepareAI:
        {
            mePrepareStage = eWorldPrepareAI;

            BrnAI::AIModuleIO::OutputBuffer* lpAIOutput = 0;
            lpOutputBufferStack->CreateIOBuffer( &lpAIOutput, "AI" );
            CGS_ASSERT( lpAIOutput, "lpAIOutputBuffer" );

            if ( !mAIModule.Prepare( lpAllocatorList, lpAIOutput ) )
            {
                lpUpdateOutputBuffer->LockForWrite();
                lpAIOutput->LockForRead();
                ::WorldModule::BridgeAIModuleToOutput( this, lpUpdateOutputBuffer, lpAIOutput );
                lpAIOutput->UnlockForRead();
                lpUpdateOutputBuffer->UnlockForWrite();
                lpOutputBufferStack->DestroyIOBuffer( &lpAIOutput );
                return false;
            }

            lpOutputBufferStack->DestroyIOBuffer( &lpAIOutput );
        }
        // fall through

        case eWorldPrepareCrashModule:
        {
            mePrepareStage = eWorldPrepareCrashModule;

            if ( !mCrashModule.Prepare() )
            {
                return false;
            }
        }
        // fall through

        case eWorldPrepareEnvmapCameras:
        {
            mePrepareStage = eWorldPrepareEnvmapCameras;

            mEnvironmentMap.Prepare();
        }
        // fall through

        case eWorldPrepareDebug:
        {
            mDebugComponent.Register();
        }
        // fall through

        case eWorldPrepareDone:
        {
            mePrepareStage = eWorldPrepareDone;

            meLocalPlayerActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>( -1 );
            mfLocalPlayerActiveRaceCarSpeed = 0.0f;

            for ( s32 liI = 0; liI < 8; liI++ )
            {
                // (the X360 walks the slots with the BurnoutConstants.h:39 enum-bound assert)
                maeCarControls[ liI ] = 1;
            }

            meReleaseStage = eWorldReleaseStart;
            mbIsInJunkyard = false;   // X360 +6175808

            return true;
        }

        default:
        {
            CGS_ASSERT( false, "0" );
        }
        break;
    }

    return false;
}


// ============================================================================
// Release  @ 0x827BCE58  (the this-only virtual slot)
//
// The 13-stage release chain (reverse of Prepare). Each sub-module releases
// through its own resumable Release; the environment manager is nudged by
// stamping its leading stage pair (the X360 inline), and the DONE stage rewinds
// mePrepareStage for the next prepare cycle.
// ============================================================================
bool
WorldModule::Release()
{
    switch ( meReleaseStage )
    {
        case eWorldReleaseStart:
        case eWorldReleaseCrashModule:
        {
            meReleaseStage = eWorldReleaseCrashModule;
            if ( !mCrashModule.Release() )
            {
                return false;
            }
        }
        // fall through

        case eWorldReleaseAI:
        {
            meReleaseStage = eWorldReleaseAI;
            if ( !mAIModule.Release() )
            {
                return false;
            }
        }
        // fall through

        case eWorldReleaseTriggerEntityModule:
        {
            meReleaseStage = eWorldReleaseTriggerEntityModule;
            if ( !mTriggerEntityModule.Release() )
            {
                return false;
            }
        }
        // fall through

        case eWorldReleasePropEntityModule:
        {
            meReleaseStage = eWorldReleasePropEntityModule;
            if ( !mPropEntityModule.Release() )
            {
                return false;
            }
        }
        // fall through

        case eWorldReleaseWorldEntityModule:
        {
            meReleaseStage = eWorldReleaseWorldEntityModule;
            if ( !mWorldEntityModule.Release() )
            {
                return false;
            }
        }
        // fall through

        case eWorldReleaseTrafficEntityModule:
        {
            meReleaseStage = eWorldReleaseTrafficEntityModule;
            if ( !mTrafficEntityModule.Release() )
            {
                return false;
            }
        }
        // fall through

        case eWorldReleaseRaceCarEntityModule:
        {
            meReleaseStage = eWorldReleaseRaceCarEntityModule;
            if ( !mRaceCarEntityModule.Release() )
            {
                return false;
            }
        }
        // fall through

        case eWorldReleaseEnvironmentManager:
        {
            meReleaseStage = eWorldReleaseEnvironmentManager;
            // X360 inline: stamp the manager's leading stage pair to {0, 1}.
            mEnvironmentManager.BeginRelease();
        }
        // fall through

        case eWorldReleasePhysicsModule:
        {
            meReleaseStage = eWorldReleasePhysicsModule;
            if ( !mPhysicsModule.Release() )
            {
                return false;
            }
        }
        // fall through

        case eWorldReleaseSceneModule:
        {
            meReleaseStage = eWorldReleaseSceneModule;
            if ( !mSceneModule.Release() )
            {
                return false;
            }
        }
        // fall through

        case eWorldReleaseEnvmapCameras:
        {
            meReleaseStage = eWorldReleaseEnvmapCameras;
            mEnvironmentMap.Release();
        }
        // fall through

        case eWorldReleaseModule:
        {
            meReleaseStage = eWorldReleaseModule;
            if ( !CgsModule::ModuleSingleBuffered::Release() )
            {
                return false;
            }
            mReceiverQueue.Clear();
        }
        // fall through

        case eWorldReleaseDone:
        {
            mePrepareStage = eWorldPrepareStart;
            meReleaseStage = eWorldReleaseDone;
            return true;
        }

        default:
        {
            CGS_ASSERT( false, "0" );
        }
        break;
    }

    return false;
}

// ============================================================================
// HandleGameActions  @ 0x827C44D8
//
// Drains the world GameAction queue, then bridges the frame's actions into the
// physics and traffic modules (bracketed by the physics / traffic-bridge global
// CPU monitors).
// ============================================================================
void
WorldModule::HandleGameActions(
    BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
    BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics* lpTrafficModuleInputBuffer,
    void* lpUnusedA, void* lpUnusedB,
    const BrnWorldIO::UpdateInputBuffer* lpWorldInput )
{
    (void)lpUnusedA;
    (void)lpUnusedB;

    const BrnWorldIO::GameActionQueue* lpInQueue = lpWorldInput->GetGameActionQueue();
    CGS_ASSERT( lpInQueue, "lpInQueue" );

    const CgsModule::Event* lpEventData = 0;
    s32 liEventSize = 0;
    s32 liAction = lpInQueue->GetFirstEvent( &lpEventData, &liEventSize );

    while ( lpEventData )
    {
        // The payload is the raw GameAction record (its own event structs are not
        // yet homed; dword-indexed reads mirror the X360 exactly -- POSTMORTEM).
        const s32* lpiPayload = reinterpret_cast<const s32*>( lpEventData );

        switch ( liAction )
        {
            case 7:   // set the player car's control mode
            {
                CGS_ASSERT( meLocalPlayerActiveRaceCarIndex != -1,
                    "Unable to set the player car under AI control, as we don't know who they are yet" );
                maeCarControls[ meLocalPlayerActiveRaceCarIndex ] = lpiPayload[ 0 ];
                break;
            }

            case 8:   // toggle the always-under-AI debug policy
            {
                mbDEBUGPlayerCarAlwaysUnderAIControl = !mbDEBUGPlayerCarAlwaysUnderAIControl;
                break;
            }

            case 23:  // game-mode change: derive every car's control policy
            {
                if ( lpiPayload[ 0 ] == 0 || lpiPayload[ 0 ] == 1 )
                {
                    s32 liControl;
                    if ( ( lpiPayload[ 549 ] & 0x10000 ) != 0 )
                    {
                        liControl = 2;
                    }
                    else
                    {
                        liControl = ( lpiPayload[ 94 ] == 15 ) ? 1 : 0;
                    }

                    for ( s32 liI = 0; liI < 8; liI++ )
                    {
                        maeCarControls[ liI ] = liControl;
                    }
                }
                break;
            }

            case 34:  // reset every car to player control
            {
                for ( s32 liI = 0; liI < 8; liI++ )
                {
                    maeCarControls[ liI ] = 1;
                }
                break;
            }

            default:
                break;
        }

        liAction = lpInQueue->GetNextEvent( lpEventData, &lpEventData, &liEventSize );
    }

    {
        using namespace CgsDev;

        PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Physics );
        PerfMonCpu::StartMonitor( miPhysicsSummaryPM );
        PerfMonCpu::StartMonitor( miPhysicsBridgesPM );

        CGS_ASSERT( lpPhysicsModuleInputBuffer, "lpInputBuffer" );
        lpPhysicsModuleInputBuffer->LockForWrite();
        ::WorldModule::BridgeActionsToPhysicsModule(
            this, lpPhysicsModuleInputBuffer, lpWorldInput );
        lpPhysicsModuleInputBuffer->UnlockForWrite();

        PerfMonCpu::StopMonitor( miPhysicsBridgesPM );
        PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Physics );
        PerfMonCpu::StopMonitor( miPhysicsSummaryPM );

        PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Traffic_Bridge );
        lpTrafficModuleInputBuffer->LockForWrite();
        ::WorldModule::BridgeActionsToTrafficModule(
            this, lpTrafficModuleInputBuffer, lpWorldInput );
        lpTrafficModuleInputBuffer->UnlockForWrite();
        PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Traffic_Bridge );
    }
}


// ============================================================================
// EntityModulePrePhysicsUpdate  @ 0x827BD5B8
//
// The per-frame PRE-PHYSICS entity-module spine. For each entity module: bridge
// the scene contacts + the other modules' state into its input buffer, then run
// its pre-physics update. Order (X360): race car -> prop -> traffic -> trigger,
// each bracketed by the global + per-module CPU monitors.
// ============================================================================
void
WorldModule::EntityModulePrePhysicsUpdate(
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
    BrnUpdateSet lUpdateSet )
{
    (void)lpWorldInputBuffer_PrePhysics;
    (void)lpWorldOutputBuffer_PrePhysics;

    CGS_ASSERT( lpInputBufferStack != 0, "lpInputBufferStack != NULL" );
    CGS_ASSERT( lpOutputBufferStack != 0, "lpOutputBufferStack != NULL" );
    CGS_ASSERT( lpTriggerModuleInputBuffer_PrePhysics != 0, "lpTriggerModuleInputBuffer_PrePhysics" );
    CGS_ASSERT( lpTriggerModuleOutputBuffer_PrePhysics != 0, "lpTriggerModuleOutputBuffer_PrePhysics" );
    CGS_ASSERT( lpSceneContactsFromWorld != 0, "lpSceneContactsFromWorld" );
    CGS_ASSERT( lpTrafficInputBuffer_PrePhysics != 0, "lpTrafficInputBuffer_PrePhysics" );
    CGS_ASSERT( lpTrafficOutputBuffer_PrePhysics != 0, "lpTrafficOutputBuffer_PrePhysics" );
    CGS_ASSERT( lpTrafficOutputBuffer_PostScene != 0, "lpTrafficOutputBuffer_PostScene" );
    CGS_ASSERT( lpRaceCarInputBuffer_PrePhysics != 0, "lpRaceCarInputBuffer_PrePhysics" );
    CGS_ASSERT( lpRaceCarOutputBuffer_PrePhysics != 0, "lpRaceCarOutputBuffer_PrePhysics" );
    CGS_ASSERT( lpPropInputBuffer_PrePhysics != 0, "lpPropInputBuffer_PrePhysics" );
    CGS_ASSERT( lpPropOutputBuffer_PrePhysics != 0, "lpPropOutputBuffer_PrePhysics" );
    CGS_ASSERT( lpWorldInputBuffer_PrePhysics != 0, "lpWorldInputBuffer_PrePhysics" );
    CGS_ASSERT( lpWorldOutputBuffer_PrePhysics != 0, "lpWorldOutputBuffer_PrePhysics" );

    using namespace CgsDev;

    // ---- race car ----------------------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_NetworkAIRaceCar );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_RaceCar );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_RaceCar_Bridge );

    CgsModule::LockBuffersForIO( lpRaceCarInputBuffer_PrePhysics,
                                 lpSceneContactsFromWorld, lpTrafficOutputBuffer_PostScene );
    ::WorldModule::BridgeSceneContactsToRaceCarModule_PrePhysics(
        this, lpRaceCarInputBuffer_PrePhysics, lpSceneContactsFromWorld );
    ::WorldModule::BridgeTrafficToRaceCar_PrePhysics(
        this, lpRaceCarInputBuffer_PrePhysics, lpTrafficOutputBuffer_PostScene );
    CgsModule::UnlockBuffersForIO( lpRaceCarInputBuffer_PrePhysics,
                                   lpSceneContactsFromWorld, lpTrafficOutputBuffer_PostScene );

    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_RaceCar_Bridge );

    mRaceCarEntityModule.PrePhysicsUpdate(
        lpRaceCarInputBuffer_PrePhysics, lpRaceCarOutputBuffer_PrePhysics, lUpdateSet );

    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_RaceCar );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_NetworkAIRaceCar );

    // ---- prop --------------------------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Physics );
    PerfMonCpu::StartMonitor( miPhysicsSummaryPM );
    PerfMonCpu::StartMonitor( miPhysicsPropSummaryPM );
    PerfMonCpu::StartMonitor( miPhysicsPropBridgePM );

    CgsModule::LockBuffersForIO( lpPropInputBuffer_PrePhysics, lpSceneContactsFromWorld );
    ::WorldModule::BridgeSceneContactsToPropModule_PrePhysics(
        this, lpPropInputBuffer_PrePhysics, lpSceneContactsFromWorld );
    CgsModule::UnlockBuffersForIO( lpPropInputBuffer_PrePhysics, lpSceneContactsFromWorld );

    PerfMonCpu::StopMonitor( miPhysicsPropBridgePM );

    PerfMonCpu::StartMonitor( miPhysicsPropPrePhysicsUpdatePM );
    mPropEntityModule.PrePhysicsUpdate( lpInputBufferStack, lpOutputBufferStack,
                                        lpPropInputBuffer_PrePhysics,
                                        lpPropOutputBuffer_PrePhysics, lUpdateSet );
    PerfMonCpu::StopMonitor( miPhysicsPropPrePhysicsUpdatePM );

    PerfMonCpu::StopMonitor( miPhysicsPropSummaryPM );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Physics );
    PerfMonCpu::StopMonitor( miPhysicsSummaryPM );

    // ---- traffic -----------------------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Traffic );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Traffic_Bridge );

    CgsModule::LockBuffersForIO( lpTrafficInputBuffer_PrePhysics, lpSceneContactsFromWorld,
                                 lpRaceCarOutputBuffer_PrePhysics, lpPropOutputBuffer_PrePhysics );
    ::WorldModule::BridgeSceneContactsToTrafficModule_PrePhysics(
        this, lpTrafficInputBuffer_PrePhysics, lpSceneContactsFromWorld );
    ::WorldModule::BridgeRaceCarModuleToTrafficModule_PrePhysics(
        this, lpTrafficInputBuffer_PrePhysics, lpRaceCarOutputBuffer_PrePhysics );
    ::WorldModule::BridgePropModuleToTrafficModule_PrePhysics(
        this, lpTrafficInputBuffer_PrePhysics, lpPropOutputBuffer_PrePhysics );
    CgsModule::UnlockBuffersForIO( lpTrafficInputBuffer_PrePhysics, lpSceneContactsFromWorld,
                                   lpRaceCarOutputBuffer_PrePhysics, lpPropOutputBuffer_PrePhysics );

    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Traffic_Bridge );

    mTrafficEntityModule.PrePhysicsUpdate( lpInputBufferStack, lpOutputBufferStack,
                                           lpTrafficInputBuffer_PrePhysics,
                                           lpTrafficOutputBuffer_PrePhysics, lUpdateSet );

    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Traffic );

    // ---- trigger -----------------------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Triggers );
    mTriggerEntityModule.PrePhysicsUpdate( lpInputBufferStack, lpOutputBufferStack,
                                           lpTriggerModuleInputBuffer_PrePhysics,
                                           lpTriggerModuleOutputBuffer_PrePhysics, lUpdateSet );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Triggers );
}


// ============================================================================
// EntityModulePostSceneUpdate  @ 0x827C3C58
//
// The per-frame POST-SCENE entity-module spine. For race car, traffic, prop and
// trigger in turn: bridge the crash + cross-module state into the module's
// post-scene input, run its post-scene update, then (race car / traffic /
// trigger) round-trip its scene queries through the scene manager and feed the
// results into that module's pre-physics input. Monitor bracketing follows the
// X360 exactly (global UT monitors + the per-module scene-query monitors).
// ============================================================================
void
WorldModule::EntityModulePostSceneUpdate(
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
    BrnUpdateSet lUpdateSet )
{
    CGS_ASSERT( lpInputBufferStack != 0, "lpInputBufferStack != NULL" );
    CGS_ASSERT( lpOutputBufferStack != 0, "lpOutputBufferStack != NULL" );

    using namespace CgsDev;

    // ---- race car ----------------------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_NetworkAIRaceCar );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_RaceCar );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_RaceCar_Bridge );

    CgsModule::LockBuffersForIO( lpRaceCarInputBuffer_PostScene, lpCrashOutputBuffer_PostScene );
    ::WorldModule::BridgeCrashModuleToRaceCarModule_PostScene(
        this, lpRaceCarInputBuffer_PostScene, lpCrashOutputBuffer_PostScene );
    lpRaceCarInputBuffer_PostScene->SetTrafficToRaceCarInterface_PreScene(
        lpTrafficToRaceCarInterface_PreScene );
    CgsModule::UnlockBuffersForIO( lpRaceCarInputBuffer_PostScene, lpCrashOutputBuffer_PostScene );

    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_RaceCar_Bridge );

    mRaceCarEntityModule.PostSceneUpdate(
        lpRaceCarInputBuffer_PostScene, lpRaceCarOutputBuffer_PostScene, lUpdateSet );

    // race-car scene queries
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_RaceCar_SQ );
    {
        CgsSceneManager::SceneManagerIO::InputBuffer_Query* lpQueryInput = 0;
        CgsSceneManager::SceneManagerIO::OutputBuffer* lpQueryOutput = 0;
        lpInputBufferStack->CreateIOBuffer( &lpQueryInput, "Scene" );
        lpOutputBufferStack->CreateIOBuffer( &lpQueryOutput, "Scene" );

        PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_RaceCar_Bridge );
        CgsModule::LockBuffersForIO( lpQueryInput, lpRaceCarOutputBuffer_PostScene );
        ::WorldModule::BridgeRaceCarModuleToSceneModule_PostScene(
            this, lpQueryInput, lpRaceCarOutputBuffer_PostScene );
        CgsModule::UnlockBuffersForIO( lpQueryInput, lpRaceCarOutputBuffer_PostScene );
        PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_RaceCar_Bridge );

        PerfMonCpu::StartMonitor( miSceneManagerQueryPM );
        mSceneModule.UpdateQueries( lpInputBufferStack, lpOutputBufferStack,
                                    lpQueryInput, lpQueryOutput );
        PerfMonCpu::StopMonitor( miSceneManagerQueryPM );

        PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_RaceCar_Bridge );
        CgsModule::LockBuffersForIO( lpRaceCarInputBuffer_PrePhysics, lpQueryOutput );
        ::WorldModule::BridgeSceneQueryResultsToRaceCarModule_PrePhysics(
            this, lpRaceCarInputBuffer_PrePhysics, lpQueryOutput );
        CgsModule::UnlockBuffersForIO( lpRaceCarInputBuffer_PrePhysics, lpQueryOutput );
        PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_RaceCar_Bridge );

        lpOutputBufferStack->DestroyIOBuffer( &lpQueryOutput );
        lpInputBufferStack->DestroyIOBuffer( &lpQueryInput );
    }
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_RaceCar_SQ );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_RaceCar );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_NetworkAIRaceCar );

    // ---- traffic -----------------------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Traffic );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Traffic_Bridge );

    CgsModule::LockBuffersForIO( lpTrafficInputBuffer_PostScene, lpCrashOutputBuffer_PostScene,
                                 lpRaceCarOutputBuffer_PostScene );
    ::WorldModule::BridgeCrashModuleToTrafficModule_PostScene(
        this, lpTrafficInputBuffer_PostScene, lpCrashOutputBuffer_PostScene );
    ::WorldModule::BridgeRaceCarModuleToTrafficModule_PostScene(
        this, lpTrafficInputBuffer_PostScene, lpRaceCarOutputBuffer_PostScene );
    CgsModule::UnlockBuffersForIO( lpTrafficInputBuffer_PostScene, lpCrashOutputBuffer_PostScene,
                                   lpRaceCarOutputBuffer_PostScene );

    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Traffic_Bridge );

    mTrafficEntityModule.PostSceneUpdate( lpInputBufferStack, lpOutputBufferStack,
                                          lpTrafficInputBuffer_PostScene,
                                          lpTrafficOutputBuffer_PostScene, lUpdateSet );

    // traffic scene queries
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Traffic_SQ );
    {
        CgsSceneManager::SceneManagerIO::InputBuffer_Query* lpQueryInput = 0;
        CgsSceneManager::SceneManagerIO::OutputBuffer* lpQueryOutput = 0;
        lpInputBufferStack->CreateIOBuffer( &lpQueryInput, "Scene" );
        lpOutputBufferStack->CreateIOBuffer( &lpQueryOutput, "Scene" );

        PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Traffic_Bridge );
        CgsModule::LockBuffersForIO( lpQueryInput, lpTrafficOutputBuffer_PostScene );
        ::WorldModule::BridgeTrafficModuleToSceneModule_PostScene(
            this, lpQueryInput, lpTrafficOutputBuffer_PostScene );
        CgsModule::UnlockBuffersForIO( lpQueryInput, lpTrafficOutputBuffer_PostScene );
        PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Traffic_Bridge );

        PerfMonCpu::StartMonitor( miSceneManagerQueryPM );
        mSceneModule.UpdateQueries( lpInputBufferStack, lpOutputBufferStack,
                                    lpQueryInput, lpQueryOutput );
        PerfMonCpu::StopMonitor( miSceneManagerQueryPM );

        PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Traffic_Bridge );
        CgsModule::LockBuffersForIO( lpTrafficInputBuffer_PrePhysics, lpQueryOutput );
        // (the traffic variant also writes the post-physics input buffer)
        CGS_ASSERT( lpTrafficInputBuffer_PostPhysics, "lpInputBuffer" );
        lpTrafficInputBuffer_PostPhysics->LockForWrite();
        ::WorldModule::BridgeSceneQueryResultsToTrafficModule_PrePhysics(
            this, lpTrafficInputBuffer_PostPhysics, lpTrafficInputBuffer_PrePhysics, lpQueryOutput );
        lpTrafficInputBuffer_PostPhysics->UnlockForWrite();
        CgsModule::UnlockBuffersForIO( lpTrafficInputBuffer_PrePhysics, lpQueryOutput );
        PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Traffic_Bridge );

        lpOutputBufferStack->DestroyIOBuffer( &lpQueryOutput );
        lpInputBufferStack->DestroyIOBuffer( &lpQueryInput );
    }
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Traffic_SQ );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Traffic );

    // ---- prop --------------------------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Physics );
    PerfMonCpu::StartMonitor( miPhysicsSummaryPM );
    PerfMonCpu::StartMonitor( miPhysicsPropSummaryPM );
    PerfMonCpu::StartMonitor( miPhysicsPropPostScenePM );

    CgsModule::LockBuffersForIO( lpPropInputBuffer_PostScene, lpCrashOutputBuffer_PostScene );
    ::WorldModule::BridgeCrashModuleToPropModule_PostScene(
        this, lpPropInputBuffer_PostScene, lpCrashOutputBuffer_PostScene );
    CgsModule::UnlockBuffersForIO( lpPropInputBuffer_PostScene, lpCrashOutputBuffer_PostScene );

    mPropEntityModule.PostSceneUpdate( lpInputBufferStack, lpOutputBufferStack,
                                       lpPropInputBuffer_PostScene,
                                       lpPropOutputBuffer_PostScene, lUpdateSet );

    PerfMonCpu::StopMonitor( miPhysicsPropPostScenePM );
    PerfMonCpu::StopMonitor( miPhysicsPropSummaryPM );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Physics );
    PerfMonCpu::StopMonitor( miPhysicsSummaryPM );

    // ---- trigger -----------------------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Triggers );
    {
        TriggerEntityModuleIO::OutputBuffer_PostScene* lpTriggerOutput = 0;
        lpOutputBufferStack->CreateIOBuffer( &lpTriggerOutput, "TriggerPostScene" );

        mTriggerEntityModule.PostSceneUpdate( lpInputBufferStack, lpOutputBufferStack,
                                              lpTriggerInputBuffer_PostScene,
                                              lpTriggerOutput, lUpdateSet );

        PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Triggers_SQ );
        {
            CgsSceneManager::SceneManagerIO::InputBuffer_Query* lpQueryInput = 0;
            CgsSceneManager::SceneManagerIO::OutputBuffer* lpQueryOutput = 0;
            lpInputBufferStack->CreateIOBuffer( &lpQueryInput, "Scene" );
            lpOutputBufferStack->CreateIOBuffer( &lpQueryOutput, "Scene" );

            CgsModule::LockBuffersForIO( lpQueryInput, lpTriggerOutput );
            ::WorldModule::BridgeTriggerModuleToSceneModule_PostScene(
                this, lpQueryInput, lpTriggerOutput );
            CgsModule::UnlockBuffersForIO( lpQueryInput, lpTriggerOutput );

            PerfMonCpu::StartMonitor( miSceneManagerQueryPM );
            mSceneModule.UpdateQueries( lpInputBufferStack, lpOutputBufferStack,
                                        lpQueryInput, lpQueryOutput );
            PerfMonCpu::StopMonitor( miSceneManagerQueryPM );

            CgsModule::LockBuffersForIO( lpTriggerInputBuffer_PrePhysics, lpQueryOutput );
            ::WorldModule::BridgeSceneQueryResultsToTriggerModule_PrePhysics(
                this, lpTriggerInputBuffer_PrePhysics, lpQueryOutput );
            CgsModule::UnlockBuffersForIO( lpTriggerInputBuffer_PrePhysics, lpQueryOutput );

            lpOutputBufferStack->DestroyIOBuffer( &lpQueryOutput );
            lpInputBufferStack->DestroyIOBuffer( &lpQueryInput );
        }
        PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Triggers_SQ );

        lpOutputBufferStack->DestroyIOBuffer( &lpTriggerOutput );
    }
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Triggers );
}


// ============================================================================
// GenerateFrustumQueries  @ 0x827DADF8
//
// Runs only when the update set selects frustum testing (bit 7). Stages this
// frame's coarse frustum queries into the scene manager's query input buffer:
//   * the main camera frustum (the world/entity visibility query),
//   * the six environment-map cube faces -- under the 30Hz env-map policy only
//     one half is refreshed per frame (faces 0-2 then 3-5, toggling through
//     mbRenderFirstEnvMapFaces),
//   * the three shadow-map cascades (when the shadow map is enabled),
// then hands the staged queries to the frustum-test job system.
// ============================================================================
void
WorldModule::GenerateFrustumQueries(
    CgsModule::IOBufferStack* lpInputBufferStack,
    CgsModule::IOBufferStack* lpOutputBufferStack,
    const BrnWorldIO::DispatchInputBuffer* lpDispatchInputBuffer,
    BrnUpdateSet lUpdateSet )
{
    // Frustum testing is selected by update-set bit 7.
    if ( ( lUpdateSet & 0x80 ) == 0 )
    {
        return;
    }

    using namespace CgsDev;

    // ---- which environment-map faces refresh this frame -------------------
    if ( !mb30hzEnvironmentMap || mbFirstRenderFrame )
    {
        for ( s32 liFace = 0; liFace < 6; liFace++ )
        {
            mabEnvMapFaceRender[ liFace ] = true;
        }
    }
    else
    {
        // Alternate halves: faces 0-2 one frame, 3-5 the next.
        const bool lbFirstHalf = mbRenderFirstEnvMapFaces;

        mabEnvMapFaceRender[ 0 ] = lbFirstHalf;
        mabEnvMapFaceRender[ 1 ] = lbFirstHalf;
        mabEnvMapFaceRender[ 2 ] = lbFirstHalf;
        mabEnvMapFaceRender[ 3 ] = !lbFirstHalf;
        mabEnvMapFaceRender[ 4 ] = !lbFirstHalf;
        mabEnvMapFaceRender[ 5 ] = !lbFirstHalf;

        mbRenderFirstEnvMapFaces = !lbFirstHalf;
    }

    CgsSceneManager::SceneManagerIO::InputBuffer_Query* lpQueryInput = 0;
    CgsSceneManager::SceneManagerIO::OutputBuffer* lpQueryOutput = 0;
    lpInputBufferStack->CreateIOBuffer( &lpQueryInput, "Scene" );
    lpOutputBufferStack->CreateIOBuffer( &lpQueryOutput, "Scene" );

    lpDispatchInputBuffer->LockForRead();

    const BrnDirector::Camera::Camera* lpCameraInput = lpDispatchInputBuffer->GetCameraInput();

    // The frame's graphics camera (file-static; the X360 rebuilds it in place).
    gFrustumQueryCamera.Release();
    lpCameraInput->CopyToCgsCamera( &gFrustumQueryCamera );

    // ---- shadow-map cascade cameras ---------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_RenderShadowMap );
    if ( mShadowMap.IsEnabled() )
    {
        // FLAG: the decompiler's r4 reads as the DIRECTOR camera input, but the
        // committed CalculateShadowMapCameras decl takes CgsGraphics::Camera*;
        // the frustum-query camera is the same frame camera re-homed --
        // reconcile when the ShadowMap TU's camera math lands.
        mShadowMap.CalculateShadowMapCameras( mEnvironmentManager.CalcKeyLightDirection(),
                                              &gFrustumQueryCamera );
    }
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_RenderShadowMap );

    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_FrustumTesting );

    // Recycle both query buffers for this frame.
    lpQueryInput->Destruct();
    lpQueryInput->Construct();
    lpQueryOutput->Destruct();
    lpQueryOutput->Construct();

    lpQueryInput->LockForWrite();

    // ---- the main camera query --------------------------------------------
    {
        const CgsGeometric::Frustum& lrFrustum = gFrustumQueryCamera.GetFrustumPerspective();

        // The computed backdrops word rides the ENTITY-TYPE mask (the emitter's
        // asm-attested field order: r5 -> +0xC4 mx32EntityTypeFlags); the query
        // flags are zero.
        u32 luEntityTypeFlags = mbForceOnlyBackdrops ? 0u : 1024u;
        if ( mbRenderBackdrops )
        {
            luEntityTypeFlags |= 0x1000u;
        }

        lpQueryInput->GetInCoarseQueryQueue()->FrustumTestVp(
            KA_FRUSTUM_QUERY_IDS[ 0 ],
            luEntityTypeFlags,
            lrFrustum.maSwizzledPlanes,
            gFrustumQueryCamera.GetViewProjectionMatrix(),
            0u );
    }

    // ---- the six environment-map face queries ------------------------------
    if ( lpDispatchInputBuffer->GetRenderSwitches()->mbRenderEnvironmentMap )
    {
        for ( s32 liFace = 0; liFace < 6; liFace++ )
        {
            if ( !mabEnvMapFaceRender[ liFace ] )
            {
                continue;
            }

            CgsGraphics::Camera lFaceCamera = mEnvironmentMap.maEnvMapCameras[ liFace ];
            CgsGraphics::Camera lProjectedCamera;
            lFaceCamera.Clone( &lProjectedCamera );
            lProjectedCamera.UpdatePerspectiveProjectionMatrix();

            const CgsGeometric::Frustum& lrFaceFrustum =
                mEnvironmentMap.maEnvMapCameras[ liFace ].GetFrustumPerspective();

            CgsSceneManager::SceneManagerIO::InEventFrustumTestVp lEvent;
            lEvent.mViewProjection = lProjectedCamera.GetViewProjectionMatrix();
            for ( s32 liPlane = 0; liPlane < 8; liPlane++ )
            {
                lEvent.maFrustumPlanes[ liPlane ] = lrFaceFrustum.maSwizzledPlanes[ liPlane ];
            }
            lEvent.mQueryId             = KA_FRUSTUM_QUERY_IDS[ 2 + liFace ];
            lEvent.mx32EntityTypeFlags  = 1024u;    // asm: event+0xC4 = 1024
            lEvent.mxQueryFlags         = 0u;       // asm: event+0xC8 = 0

            lpQueryInput->GetInCoarseQueryQueue()->AddEvent( &lEvent, 4, sizeof( lEvent ) );
        }
    }

    // ---- the three shadow cascades ----------------------------------------
    if ( mShadowMap.IsEnabled() &&
         lpDispatchInputBuffer->GetRenderSwitches()->mbRenderShadowMap )
    {
        for ( s32 liCascade = 0; liCascade < 3; liCascade++ )
        {
            const CgsGraphics::Camera* lpCascadeCamera = mShadowMap.GetCascadeCamera( liCascade );

            CgsSceneManager::SceneManagerIO::InEventFrustumTestVp lEvent;
            const CgsGeometric::Frustum& lrCascadeFrustum =
                lpCascadeCamera->GetFrustumPerspective();
            lEvent.mViewProjection = lpCascadeCamera->GetViewProjectionMatrix();
            for ( s32 liPlane = 0; liPlane < 8; liPlane++ )
            {
                lEvent.maFrustumPlanes[ liPlane ] = lrCascadeFrustum.maSwizzledPlanes[ liPlane ];
            }
            lEvent.mQueryId            = KA_FRUSTUM_QUERY_IDS[ 8 + liCascade ];
            lEvent.mx32EntityTypeFlags = 128u;      // asm: the shadow entity mask
            lEvent.mxQueryFlags        = 0u;

            lpQueryInput->GetInCoarseQueryQueue()->AddEvent( &lEvent, 4, sizeof( lEvent ) );
        }
    }

    lpQueryInput->UnlockForWrite();

    // ---- hand the staged queries to the frustum-test jobs ------------------
    PerfMonCpu::StartMonitor( miSceneManagerFrustumTestPM );
    PerfMonCpu::StartMonitor( miSceneManagerFrustumTestStartJobsPM );
    mSceneModule.ProcessFrustumTestJobRequests( lpInputBufferStack, lpOutputBufferStack,
                                                lpQueryInput, lpQueryOutput );
    PerfMonCpu::StopMonitor( miSceneManagerFrustumTestStartJobsPM );
    PerfMonCpu::StopMonitor( miSceneManagerFrustumTestPM );

    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_FrustumTesting );

    lpDispatchInputBuffer->UnlockForRead();

    lpInputBufferStack->DestroyIOBuffer( &lpQueryInput );
    lpOutputBufferStack->DestroyIOBuffer( &lpQueryOutput );

    mbFirstRenderFrame = false;
}


// ============================================================================
// GenerateDispatchLists  @ 0x827D1CE8
//
// The DISPATCH-thread render feed (called from BrnGameModule::DoDispatch).
// Frame shape (X360, reproduced in order):
//   * refresh the shader-LOD policy block and latch the junkyard lighting from
//     the camera's junkyard flag;
//   * copy the frame camera; when the update set selects rendering (bit 7):
//   * stage the frame's global shader constants + fog/key-light/irradiance;
//   * generate the environment effects and cache the prop graphics lists;
//   * collect the frustum-test job results, filter the main-view result into
//     the per-category entity lists, and seed every module's dispatch input;
//   * traffic pre-dispatch + the per-vehicle LOD policy + the render bridge;
//   * run each enabled module's GenerateDispatchLists (race car, traffic,
//     WORLD, props), then the six environment-map faces, then the shadow map.
// ============================================================================
void
WorldModule::GenerateDispatchLists(
    CgsModule::IOBufferStack* lpInputBufferStack,
    CgsModule::IOBufferStack* lpOutputBufferStack,
    BrnWorldIO::DispatchInputBuffer* lpDispatchInputBuffer,
    BrnWorldIO::DispatchOutputBuffer* lpDispatchOutputBuffer,
    const BrnUpdateSet* lpUpdateSet )
{
    using namespace CgsDev;

    CGS_ASSERT( lpInputBufferStack != 0, "lpInputBufferStack != NULL" );
    CGS_ASSERT( lpOutputBufferStack != 0, "lpOutputBufferStack != NULL" );

    // Refresh the shader-LOD policy block (the broadcast splat of the near
    // distance -- ShaderLodInfo::Update()).
    mShaderLodInfo.Update();

    lpDispatchInputBuffer->LockForRead();

    const BrnDirector::Camera::Camera* lpCameraInput = lpDispatchInputBuffer->GetCameraInput();

    // ---- junkyard lighting latch (camera flag bit 0x400000) ----------------
    if ( lpCameraInput->IsInJunkyard() )
    {
        if ( !mbIsInJunkyard )
        {
            mEnvironmentManager.EnableJunkyardLightingSetup();
        }
        mbIsInJunkyard = true;
    }
    else
    {
        if ( mbIsInJunkyard )
        {
            mEnvironmentManager.DisableJunkyardLightingSetup();
        }
        mbIsInJunkyard = false;
    }

    BrnGame::DispatchThreadInputBuffer* lpDispatchThreadInputBuffer =
        lpDispatchInputBuffer->GetDispatchThreadInputBuffer();
    CGS_ASSERT( lpDispatchThreadInputBuffer->IsWriteBuffer(),
                "lpDispatchThreadInputBuffer->IsWriteBuffer()" );
    lpDispatchThreadInputBuffer->LockForWrite();

    mLastCameraInput = *lpCameraInput;

    if ( ( *lpUpdateSet & 0x80 ) == 0 )
    {
        lpDispatchInputBuffer->UnlockForRead();
        lpDispatchThreadInputBuffer->SetRendererFlags( 0 );
        lpDispatchThreadInputBuffer->UnlockForWrite();
        return;
    }

    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_RenderMainScreen );

    // ---- the dispatch-pass IO buffer set -----------------------------------
    WorldEntityIO::InputBuffer_GenerateDispatchLists* lpWorldDispatchInput = 0;
    BrnTraffic::BrnTrafficIO::InputBuffer_Dispatch* lpTrafficDispatchInput = 0;
    PropEntityIO::InputBuffer_Dispatch* lpPropDispatchInput = 0;
    CgsSceneManager::SceneManagerIO::InputBuffer_Query* lpQueryInput = 0;
    CgsSceneManager::SceneManagerIO::OutputBuffer* lpQueryOutput = 0;
    RaceCarEntityModuleIO::InputBuffer_GenerateDispatchLists* lpRaceCarDispatchInput = 0;
    FilteredEntityData* lpFilteredEntityData = 0;
    BrnTraffic::BrnTrafficIO::InputBuffer_PreDispatch* lpTrafficPreDispatchInput = 0;
    BrnTraffic::BrnTrafficIO::OutputBuffer_PreDispatch* lpTrafficRenderInfos = 0;

    lpInputBufferStack->CreateIOBuffer( &lpWorldDispatchInput, "WorldEntity" );
    lpInputBufferStack->CreateIOBuffer( &lpTrafficDispatchInput, "TrafficDispatch" );
    lpInputBufferStack->CreateIOBuffer( &lpPropDispatchInput, "PropDispatch" );
    lpInputBufferStack->CreateIOBuffer( &lpQueryInput, "Scene" );
    lpOutputBufferStack->CreateIOBuffer( &lpQueryOutput, "Scene" );
    lpInputBufferStack->CreateIOBuffer( &lpRaceCarDispatchInput, "RaceCar" );
    lpInputBufferStack->CreateIOBuffer( &lpFilteredEntityData, "Filtered Entity Data" );
    lpInputBufferStack->CreateIOBuffer( &lpTrafficPreDispatchInput, "TrafficVisibleEntities" );
    lpOutputBufferStack->CreateIOBuffer( &lpTrafficRenderInfos, "TrafficRenderInfos" );

    // The dispatch camera (X360 file static @0x8300FB40) rebuilt from the frame
    // camera; its view-projection rows also seed the dispatch thread's camera
    // block.
    gDispatchCamera.Release();
    lpCameraInput->CopyToCgsCamera( &gDispatchCamera );
    lpDispatchThreadInputBuffer->SetCameraViewProjection(
        gDispatchCamera.GetViewProjectionMatrix() );

    // ---- global shader constants for the frame -----------------------------
    lpDispatchOutputBuffer->LockForWrite();
    SetupShaderConstantsBeforeRendering(
        lpDispatchInputBuffer->GetShaderConstantsFrame(),
        lpDispatchInputBuffer->GetSimTime(),
        lpDispatchInputBuffer->GetGameTime() );
    lpDispatchOutputBuffer->UnlockForWrite();

    lpDispatchOutputBuffer->LockForRead();
    const Vector4 lvFogScattering            = lpDispatchOutputBuffer->GetFogScattering();
    const Vector4 lvFogColourPlusWhiteLevel  = lpDispatchOutputBuffer->GetFogColourPlusWhiteLevel();
    const Vector3 lvKeyLightColour           = lpDispatchOutputBuffer->GetKeyLightColour();
    const Matrix44 lQuadricIrradianceA       = lpDispatchOutputBuffer->GetQuadricIrradianceA();
    const Matrix44 lQuadricIrradianceB       = lpDispatchOutputBuffer->GetQuadricIrradianceB();
    const f32 lfWhiteLevel                   = lpDispatchOutputBuffer->GetWhiteLevel();
    lpDispatchOutputBuffer->UnlockForRead();

    // ---- environment effects + prop graphics cache -------------------------
    // (the main-screen span pauses across the FX pass, then brackets the cache)
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_RenderMainScreen );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_RenderFX );
    mEnvironmentManager.GenerateEffects(
        lpDispatchInputBuffer->GetEffectsFrame( 0 ),
        lpDispatchInputBuffer->GetEffectsFrame( 1 ),
        lpDispatchInputBuffer->GetEffectsFrame( 2 ),
        lpDispatchInputBuffer->GetEffectsFrame( 3 ) );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_RenderFX );

    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_RenderMainScreen );
    mPropEntityModule.CachePropGraphicsLists();
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_RenderMainScreen );

    // The LOD zoom scale never drops below 1 (X360 fsel).
    f32 lfLodZoomFactor = lpCameraInput->GetLodZoomFactor();
    if ( lfLodZoomFactor < 1.0f )
    {
        lfLodZoomFactor = 1.0f;
    }

    // ---- collect the frustum-test results ----------------------------------
    // (X360 brackets the collect with the global UT_FrustumTesting monitor,
    //  +6167656 -- not the module-local scene-manager one)
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_FrustumTesting );
    PerfMonCpu::StartMonitor( miSceneManagerFrustumTestPM );
    PerfMonCpu::StartMonitor( miSceneManagerFrustumTestWaitOnJobsPM );
    // (the committed :189 signature takes the scene-local IOBufferStack alias --
    //  the same stack object; the alias is reconciled when that TU's stack lands)
    mSceneModule.ProcessFrustumTestJobResults(
        reinterpret_cast<CgsSceneManager::SceneManagerIO::IOBufferStack*>( lpInputBufferStack ),
        reinterpret_cast<CgsSceneManager::SceneManagerIO::IOBufferStack*>( lpOutputBufferStack ),
        lpQueryInput, lpQueryOutput );
    PerfMonCpu::StopMonitor( miSceneManagerFrustumTestWaitOnJobsPM );
    PerfMonCpu::StopMonitor( miSceneManagerFrustumTestPM );

    lpQueryOutput->LockForRead();
    const CgsSceneManager::SceneManagerIO::OutputBuffer::SceneQueryResultsQueue* lpResultsQueue =
        lpQueryOutput->GetSceneQueryResultsQueue();

    const CgsModule::Event* lpFrustumTestResult = 0;
    s32 liResultSize = 0;
    s32 liResultType = lpResultsQueue->GetFirstEvent( &lpFrustumTestResult, &liResultSize );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_FrustumTesting );

    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_RenderMainScreen );
    CGS_ASSERT( reinterpret_cast<const CgsSceneManager::SceneQueryId*>( lpFrustumTestResult )->mId
                    == KA_FRUSTUM_QUERY_IDS[ 0 ].mId,
                "lpFrustumTestResult->mQueryId == KA_FRUSTUM_QUERY_IDS[ FrustumQuery_MainView ]" );

    // ---- filter the main-view result by owner category ---------------------
    PerfMonCpu::StartMonitor( miFrustumTestFilterPM );
    FilterFrustumTestResults( lpFrustumTestResult,
                              &lpFilteredEntityData->maWorldEntityIds,
                              &lpFilteredEntityData->maRaceCarEntityIds,
                              &lpFilteredEntityData->maTrafficEntityIds,
                              &lpFilteredEntityData->maPropEntityIds );
    PerfMonCpu::StopMonitor( miFrustumTestFilterPM );

    // Seed each module's dispatch input with the raw result event (world,
    // race car, traffic, prop -- clear then add, exactly as the X360 does).
    lpWorldDispatchInput->LockForWrite();
    lpWorldDispatchInput->GetSceneResultQueue()->Clear();
    lpWorldDispatchInput->GetSceneResultQueue()->AddEvent( lpFrustumTestResult, liResultType, liResultSize );
    lpWorldDispatchInput->UnlockForWrite();

    lpRaceCarDispatchInput->LockForWrite();
    lpRaceCarDispatchInput->GetSceneResultQueue()->Clear();
    lpRaceCarDispatchInput->GetSceneResultQueue()->AddEvent( lpFrustumTestResult, liResultType, liResultSize );
    lpRaceCarDispatchInput->UnlockForWrite();

    lpTrafficDispatchInput->LockForWrite();
    lpTrafficDispatchInput->GetSceneResultQueue()->Clear();
    lpTrafficDispatchInput->GetSceneResultQueue()->AddEvent( lpFrustumTestResult, liResultType, liResultSize );
    lpTrafficDispatchInput->UnlockForWrite();

    lpPropDispatchInput->LockForWrite();
    lpPropDispatchInput->GetSceneResultQueue()->Clear();
    lpPropDispatchInput->GetSceneResultQueue()->AddEvent( lpFrustumTestResult, liResultType, liResultSize );
    lpPropDispatchInput->UnlockForWrite();

    liResultType = lpResultsQueue->GetNextEvent( lpFrustumTestResult, &lpFrustumTestResult, &liResultSize );

    // ---- traffic pre-dispatch + the vehicle LOD policy ---------------------
    lpTrafficPreDispatchInput->Construct();
    lpTrafficRenderInfos->Construct();
    lpTrafficPreDispatchInput->SetVisibleEntities( lpFilteredEntityData->maTrafficEntityIds );
    lpTrafficPreDispatchInput->SetCameraPosition( gDispatchCamera.GetPosition() );

    lpTrafficPreDispatchInput->LockForRead();
    lpTrafficRenderInfos->LockForWrite();
    mTrafficEntityModule.PreDispatchUpdate( lpTrafficPreDispatchInput, lpTrafficRenderInfos );
    CalculateVehicleLODs( lpCameraInput->GetPosition() );
    lpTrafficRenderInfos->UnlockForWrite();
    lpTrafficPreDispatchInput->UnlockForRead();

    // ---- the render bridge into every module's dispatch input --------------
    lpWorldDispatchInput->LockForWrite();
    lpPropDispatchInput->LockForWrite();
    lpRaceCarDispatchInput->LockForWrite();
    lpTrafficDispatchInput->LockForWrite();
    BridgeWorldModuleToEntityModules_Render( lpTrafficDispatchInput, lpRaceCarDispatchInput,
                                             lpWorldDispatchInput, lpPropDispatchInput,
                                             lpDispatchInputBuffer );
    lpTrafficDispatchInput->UnlockForWrite();
    lpRaceCarDispatchInput->UnlockForWrite();
    lpPropDispatchInput->UnlockForWrite();
    lpWorldDispatchInput->UnlockForWrite();

    // ---- camera + fog/key-light shader constants ---------------------------
    // (X360 @0x827BACA8 = the vector setter (id + the vector in v1), @0x827BAD78
    //  = the matrix setter. Ids: 8 = view position, 3 = view-projection, 34 =
    //  modified view-projection, 9 = the UNCLAMPED key light, 12 = the clamped
    //  key light, 18/19 = quadric irradiance.)
    CgsGraphics::mShaderConstantTable.SetShaderConstantData( 8, gDispatchCamera.GetPosition() );
    CgsGraphics::mShaderConstantTable.SetShaderConstantData( 3, gDispatchCamera.GetViewProjectionMatrix() );
    CgsGraphics::mShaderConstantTable.SetShaderConstantData( 34, gDispatchCamera.GetViewProjectionMatrixModified() );

    {
        // Car passes: the key light + irradiance scaled by the car multipliers;
        // id 9 takes the unclamped colour, id 12 the [0, white level] clamp.
        Vector3 lvCarKeyLight = lvKeyLightColour;
        lvCarKeyLight.x *= mfCarKeyLightMultiplier;
        lvCarKeyLight.y *= mfCarKeyLightMultiplier;
        lvCarKeyLight.z *= mfCarKeyLightMultiplier;

        Matrix44 lCarIrradianceA = lQuadricIrradianceA;
        Matrix44 lCarIrradianceB = lQuadricIrradianceB;
        ScaleIrradiance( lCarIrradianceA, mfCarAmbientLightMultiplier );
        ScaleIrradiance( lCarIrradianceB, mfCarAmbientLightMultiplier );

        CgsGraphics::mShaderConstantTable.SetShaderConstantData( 9, lvCarKeyLight );

        Vector3 lvCarKeyLightClamped = lvCarKeyLight;
        ClampColourToWhiteLevel( lvCarKeyLightClamped, lfWhiteLevel );
        CgsGraphics::mShaderConstantTable.SetShaderConstantData( 12, lvCarKeyLightClamped );
        CgsGraphics::mShaderConstantTable.SetShaderConstantData( 18, lCarIrradianceA );
        CgsGraphics::mShaderConstantTable.SetShaderConstantData( 19, lCarIrradianceB );

        if ( lpDispatchInputBuffer->GetRenderSwitches()->mbRenderRaceCars )
        {
            PerfMonCpu::StartMonitor( miRaceCarGenerateDispListClearPM );
            mRaceCarEntityModule.GenerateDispatchLists(
                lpRaceCarDispatchInput, lpFilteredEntityData->maRaceCarEntityIds,
                lvFogScattering, lvFogColourPlusWhiteLevel, gDispatchCamera.GetPosition() );
            PerfMonCpu::StopMonitor( miRaceCarGenerateDispListClearPM );
        }

        if ( lpDispatchInputBuffer->GetRenderSwitches()->mbRenderTraffic )
        {
            PerfMonCpu::StartMonitor( miTrafficGenerateDispListClearPM );
            mTrafficEntityModule.GenerateDispatchLists(
                lpTrafficDispatchInput, lpTrafficRenderInfos,
                12, 19, 20, &mLastCameraInput );
            PerfMonCpu::StopMonitor( miTrafficGenerateDispListClearPM );
        }
    }

    // World passes: the unscaled key light (id 9 unclamped, id 12 clamped) +
    // the unscaled irradiance.
    {
        CgsGraphics::mShaderConstantTable.SetShaderConstantData( 9, lvKeyLightColour );

        Vector3 lvWorldKeyLight = lvKeyLightColour;
        ClampColourToWhiteLevel( lvWorldKeyLight, lfWhiteLevel );
        CgsGraphics::mShaderConstantTable.SetShaderConstantData( 12, lvWorldKeyLight );
        CgsGraphics::mShaderConstantTable.SetShaderConstantData( 18, lQuadricIrradianceA );
        CgsGraphics::mShaderConstantTable.SetShaderConstantData( 19, lQuadricIrradianceB );
    }

    if ( lpDispatchInputBuffer->GetRenderSwitches()->mbRenderWorld )
    {
        PerfMonCpu::StartMonitor( miGenerateDispatchListsPM );
        mWorldEntityModule.GenerateDispatchLists(
            lpWorldDispatchInput, lpFilteredEntityData->maWorldEntityIds,
            gDispatchCamera.GetViewProjectionMatrix(), gDispatchCamera.GetPosition(),
            gDispatchCamera.GetDirection(), lfLodZoomFactor, &mShaderLodInfo,
            KI_WORLD_OPAQUE_LIST, KI_WORLD_SORT_LAYER, KI_WORLD_SORT_KEY,
            KI_WORLD_PREZ_LIST, false );
        PerfMonCpu::StopMonitor( miGenerateDispatchListsPM );
    }

    if ( lpDispatchInputBuffer->GetRenderSwitches()->mbRenderProps )
    {
        PerfMonCpu::StartMonitor( miPropGenerateDispListClearPM );
        mPropEntityModule.GenerateDispatchLists(
            lpPropDispatchInput, lpFilteredEntityData->maPropEntityIds,
            gDispatchCamera.GetViewProjectionMatrix(), gDispatchCamera.GetPosition(),
            lfLodZoomFactor, &mShaderLodInfo, 11, 11, 15 );
        PerfMonCpu::StopMonitor( miPropGenerateDispListClearPM );
    }

    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_RenderMainScreen );

    // ---- the six environment-map faces -------------------------------------
    if ( lpDispatchInputBuffer->GetRenderSwitches()->mbRenderEnvironmentMap )
    {
        PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_RenderEnvMap );

        if ( mbIsInJunkyard )
        {
            mShadowMap.SetConstantsForEnvmap();
        }

        BrnShaderConstantsFrame* lpShaderConstantsFrame =
            lpDispatchInputBuffer->GetShaderConstantsFrame();
        lpShaderConstantsFrame->SetEnvMapViewPosition( gDispatchCamera.GetPosition() );

        for ( s32 liFace = 0; liFace < 6; liFace++ )
        {
            lpDispatchThreadInputBuffer->SetEnvMapFaceRendered( liFace, mabEnvMapFaceRender[ liFace ] );

            if ( !mabEnvMapFaceRender[ liFace ] )
            {
                continue;
            }

            CGS_ASSERT( lpFrustumTestResult, "lpFrustumResultEvent" );
            CGS_ASSERT( reinterpret_cast<const CgsSceneManager::SceneQueryId*>( lpFrustumTestResult )->mId
                            == KA_FRUSTUM_QUERY_IDS[ 2 + liFace ].mId,
                "lpFrustumTestResult->mQueryId == KA_FRUSTUM_QUERY_IDS[ FrustumQuery_EnvMap0 + luFace ]" );

            lpFilteredEntityData->Clear();

            CgsGraphics::Camera lFaceCamera = mEnvironmentMap.maEnvMapCameras[ liFace ];

            PerfMonCpu::StartMonitor( miFrustumTestFilterPM );
            FilterFrustumTestResults( lpFrustumTestResult,
                                      &lpFilteredEntityData->maWorldEntityIds,
                                      &lpFilteredEntityData->maRaceCarEntityIds,
                                      &lpFilteredEntityData->maTrafficEntityIds,
                                      &lpFilteredEntityData->maPropEntityIds );
            PerfMonCpu::StopMonitor( miFrustumTestFilterPM );

            lpWorldDispatchInput->LockForWrite();
            lpWorldDispatchInput->GetSceneResultQueue()->Clear();
            lpWorldDispatchInput->GetSceneResultQueue()->AddEvent(
                lpFrustumTestResult, liResultType, liResultSize );
            lpWorldDispatchInput->UnlockForWrite();

            lpWorldDispatchInput->LockForWrite();
            lpWorldDispatchInput->SetDispatchFrame( lpDispatchInputBuffer->GetDispatchFrame() );
            lpWorldDispatchInput->UnlockForWrite();

            // (X360 id-8 payload: the vector member at module+6170304 -- the
            //  env-map view position the frame constants were stamped with a
            //  moment ago, i.e. this camera position. FLAG: read through the
            //  camera here until that EnvironmentMap member is named.)
            CgsGraphics::mShaderConstantTable.SetShaderConstantData( 8, gDispatchCamera.GetPosition() );
            CgsGraphics::mShaderConstantTable.SetShaderConstantData( 3, lFaceCamera.GetViewProjectionMatrix() );
            CgsGraphics::mShaderConstantTable.SetShaderConstantData( 34, lFaceCamera.GetViewProjectionMatrixModified() );

            PerfMonCpu::StartMonitor( miGenerateDispatchListsPM );
            mWorldEntityModule.GenerateDispatchListsForEnvironmentMap(
                lpWorldDispatchInput, lpFilteredEntityData->maWorldEntityIds,
                lFaceCamera.GetViewProjectionMatrix(), gDispatchCamera.GetPosition(),
                &mShaderLodInfo, 5 + liFace, 5 + liFace, 5 + liFace );
            PerfMonCpu::StopMonitor( miGenerateDispatchListsPM );

            PerfMonCpu::StartMonitor( miPropGenerateDispListClearPM );
            mPropEntityModule.GenerateDispatchLists(
                lpPropDispatchInput, lpFilteredEntityData->maPropEntityIds,
                lFaceCamera.GetViewProjectionMatrix(), gDispatchCamera.GetPosition(),
                1.0f, &mShaderLodInfo, 5 + liFace, 5 + liFace, 5 + liFace );
            PerfMonCpu::StopMonitor( miPropGenerateDispListClearPM );

            // Refresh the face camera's projection for the renderer (far 10000)
            // and record its view-projection for the env-map resolve -- ON THE
            // LOCAL COPY (X360 v219): the member maEnvMapCameras[face] is never
            // mutated by the dispatch pass.
            lFaceCamera.SetFarClip( 10000.0f );
            lFaceCamera.UpdatePerspectiveProjectionMatrix();
            lFaceCamera.SetPerspectiveProjectionMatrixRightHanded();
            lpShaderConstantsFrame->SetEnvMapViewProjectionMatrix(
                static_cast<BrnGraphics::EEnvironmentMapFace>( liFace ),
                lFaceCamera.GetViewProjectionMatrix() );

            liResultType = lpResultsQueue->GetNextEvent(
                lpFrustumTestResult, &lpFrustumTestResult, &liResultSize );
        }

        PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_RenderEnvMap );
    }

    // ---- shadow map ---------------------------------------------------------
    if ( mShadowMap.IsEnabled() &&
         lpDispatchInputBuffer->GetRenderSwitches()->mbRenderShadowMap )
    {
        PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_RenderShadowMap );
        GenerateShadowMapDispatchLists(
            lpCameraInput, lfLodZoomFactor, lpDispatchInputBuffer,
            lpDispatchOutputBuffer, lpWorldDispatchInput, lpRaceCarDispatchInput,
            lpTrafficDispatchInput, lpPropDispatchInput, lpFilteredEntityData,
            lpResultsQueue, lpFrustumTestResult, liResultSize, lpTrafficRenderInfos );
        PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_RenderShadowMap );
    }

    // ---- teardown -----------------------------------------------------------
    lpQueryOutput->UnlockForRead();
    lpDispatchInputBuffer->UnlockForRead();

    lpInputBufferStack->DestroyIOBuffer( &lpTrafficPreDispatchInput );
    lpOutputBufferStack->DestroyIOBuffer( &lpTrafficRenderInfos );
    lpInputBufferStack->DestroyIOBuffer( &lpFilteredEntityData );
    lpInputBufferStack->DestroyIOBuffer( &lpRaceCarDispatchInput );
    lpInputBufferStack->DestroyIOBuffer( &lpQueryInput );
    lpOutputBufferStack->DestroyIOBuffer( &lpQueryOutput );
    lpInputBufferStack->DestroyIOBuffer( &lpPropDispatchInput );
    lpInputBufferStack->DestroyIOBuffer( &lpTrafficDispatchInput );

    lpDispatchThreadInputBuffer->SetRendererFlags( 0 );
    CGS_ASSERT( lpDispatchThreadInputBuffer->IsWriteBuffer(),
                "lpDispatchThreadInputBuffer->IsWriteBuffer()" );
    lpDispatchThreadInputBuffer->UnlockForWrite();

    lpInputBufferStack->DestroyIOBuffer( &lpWorldDispatchInput );
}


// ============================================================================
// GenerateShadowMapDispatchLists  @ 0x827C96D8
//
// The shadow-map dispatch feed: one pass per cascade (three when the shadow map
// renders multiple maps, else one). Each cascade filters its own frustum-test
// result, seeds the enabled modules' dispatch inputs, re-runs the render
// bridge, binds the cascade camera's constants, and drives the race-car /
// traffic / world / prop dispatch feeds with the shadow technique. The
// "near-only" module gates restrict race cars / traffic / props to cascade 0.
// The shadow map's rendering latch is raised around each cascade -- the world
// entity module's own dispatch feed reads it to select the shadow path.
// ============================================================================
void
WorldModule::GenerateShadowMapDispatchLists(
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
    BrnTraffic::BrnTrafficIO::OutputBuffer_PreDispatch* lpTrafficRenderInfos )
{
    (void)lpDispatchOutputBuffer;

    using namespace CgsDev;

    const u32 luNumCascades = mShadowMap.GetRenderMultipleShadowMaps() ? 3u : 1u;

    // The X360 seeds the first cascade's events with the LIVE result size the
    // caller hands over (a39) and a literal -1 type (the local NextEvent);
    // GetNextEvent refreshes both for the later cascades.
    s32 liEventType = -1;

    for ( u32 luCascade = 0; luCascade < luNumCascades; luCascade++ )
    {
        mShadowMap.SetCurrentCascadeIndex( luCascade );
        lpFilteredEntityData->Clear();

        const CgsGraphics::Camera* lpCascadeCamera = mShadowMap.GetCascadeCamera( luCascade );

        // Per-module gates for this cascade (the near-only policies restrict
        // race cars / traffic / props to cascade 0).
        const BrnWorldIO::DispatchInputBuffer::RenderSwitches* lpSwitches =
            lpDispatchInputBuffer->GetRenderSwitches();

        const bool lbRaceCars = mShadowMap.GetRenderRaceCarsIntoShadowMap() &&
                                lpSwitches->mbRenderRaceCars &&
                                ( luCascade == 0 || !mShadowMap.GetRenderRaceCarsNearOnly() );
        const bool lbTraffic  = mShadowMap.GetRenderTrafficIntoShadowMap() &&
                                lpSwitches->mbRenderTraffic &&
                                ( luCascade == 0 || !mShadowMap.GetRenderTrafficNearOnly() );
        const bool lbProps    = mShadowMap.GetRenderPropsIntoShadowMap() &&
                                lpSwitches->mbRenderProps &&
                                ( luCascade == 0 || !mShadowMap.GetRenderPropsNearOnly() );
        const bool lbWorld    = mShadowMap.GetRenderWorldIntoShadowMap() &&
                                lpSwitches->mbRenderWorld;

        CGS_ASSERT( lpFrustumTestResult, "lpEvent" );
        CGS_ASSERT( reinterpret_cast<const CgsSceneManager::SceneQueryId*>( lpFrustumTestResult )->mId
                        == KA_FRUSTUM_QUERY_IDS[ 8 + luCascade ].mId,
            "lpFrustumTestResult->mQueryId == KA_FRUSTUM_QUERY_IDS[ FrustumQuery_Shadowmap0 + luShadowMapIndex ]" );

        PerfMonCpu::StartMonitor( miFrustumTestFilterPM );
        FilterFrustumTestResults( lpFrustumTestResult,
                                  &lpFilteredEntityData->maWorldEntityIds,
                                  &lpFilteredEntityData->maRaceCarEntityIds,
                                  &lpFilteredEntityData->maTrafficEntityIds,
                                  &lpFilteredEntityData->maPropEntityIds );
        PerfMonCpu::StopMonitor( miFrustumTestFilterPM );

        // Seed the enabled modules' dispatch inputs with the cascade's result.
        lpWorldDispatchInput->LockForWrite();
        lpRaceCarDispatchInput->LockForWrite();
        lpTrafficDispatchInput->LockForWrite();
        lpPropDispatchInput->LockForWrite();

        lpWorldDispatchInput->GetSceneResultQueue()->Clear();
        lpRaceCarDispatchInput->GetSceneResultQueue()->Clear();
        lpTrafficDispatchInput->GetSceneResultQueue()->Clear();
        lpPropDispatchInput->GetSceneResultQueue()->Clear();

        if ( lbWorld )
        {
            lpWorldDispatchInput->GetSceneResultQueue()->AddEvent(
                lpFrustumTestResult, liEventType, liResultSize );
        }
        if ( lbRaceCars )
        {
            lpRaceCarDispatchInput->GetSceneResultQueue()->AddEvent(
                lpFrustumTestResult, liEventType, liResultSize );
        }
        if ( lbTraffic )
        {
            lpTrafficDispatchInput->GetSceneResultQueue()->AddEvent(
                lpFrustumTestResult, liEventType, liResultSize );
        }
        if ( lbProps )
        {
            lpPropDispatchInput->GetSceneResultQueue()->AddEvent(
                lpFrustumTestResult, liEventType, liResultSize );
        }

        lpPropDispatchInput->UnlockForWrite();
        lpTrafficDispatchInput->UnlockForWrite();
        lpRaceCarDispatchInput->UnlockForWrite();
        lpWorldDispatchInput->UnlockForWrite();

        liEventType = lpResultsQueue->GetNextEvent(
            lpFrustumTestResult, &lpFrustumTestResult, &liResultSize );

        // The render bridge for this cascade.
        lpWorldDispatchInput->LockForWrite();
        lpTrafficDispatchInput->LockForWrite();
        lpRaceCarDispatchInput->LockForWrite();
        lpPropDispatchInput->LockForWrite();
        BridgeWorldModuleToEntityModules_Render( lpTrafficDispatchInput, lpRaceCarDispatchInput,
                                                 lpWorldDispatchInput, lpPropDispatchInput,
                                                 lpDispatchInputBuffer );
        lpPropDispatchInput->UnlockForWrite();
        lpRaceCarDispatchInput->UnlockForWrite();
        lpTrafficDispatchInput->UnlockForWrite();
        lpWorldDispatchInput->UnlockForWrite();

        // The rendering latch the world entity module's dispatch feed reads.
        mShadowMap.SetRenderingShadowMap( true );

        // The cascade camera's constants (id 8 = the view position -- the X360
        // hands the camera-input position vector in v1).
        CgsGraphics::mShaderConstantTable.SetShaderConstantData( 8, lpCameraInput->GetPosition() );
        CgsGraphics::mShaderConstantTable.SetShaderConstantData( 3, lpCascadeCamera->GetViewProjectionMatrix() );
        CgsGraphics::mShaderConstantTable.SetShaderConstantData( 34, lpCascadeCamera->GetViewProjectionMatrixModified() );

        // The cascade list ids skip the env-map ids on the far cascades.
        const s32 liCascadeList = ( luCascade >= 2 ) ? static_cast<s32>( luCascade ) + 2
                                                     : static_cast<s32>( luCascade );

        PerfMonCpu::StartMonitor( miRaceCarGenerateDispListClearPM );
        if ( lbRaceCars )
        {
            mRaceCarEntityModule.GenerateDispatchLists(
                lpRaceCarDispatchInput, lpFilteredEntityData->maRaceCarEntityIds,
                Vector4{ 0.0f, 0.0f, 0.0f, 0.0f }, Vector4{ 0.0f, 0.0f, 0.0f, 0.0f },
                lpCameraInput->GetPosition() );
        }
        PerfMonCpu::StopMonitor( miRaceCarGenerateDispListClearPM );

        PerfMonCpu::StartMonitor( miTrafficGenerateDispListClearPM );
        if ( lbTraffic )
        {
            mTrafficEntityModule.GenerateDispatchLists(
                lpTrafficDispatchInput, lpTrafficRenderInfos,
                static_cast<s32>( luCascade ) + 2, static_cast<s32>( luCascade ) + 2,
                static_cast<s32>( luCascade ) + 2, lpCameraInput );
        }
        PerfMonCpu::StopMonitor( miTrafficGenerateDispListClearPM );

        PerfMonCpu::StartMonitor( miGenerateDispatchListsPM );
        if ( lbWorld )
        {
            mWorldEntityModule.GenerateDispatchLists(
                lpWorldDispatchInput, lpFilteredEntityData->maWorldEntityIds,
                lpCascadeCamera->GetViewProjectionMatrix(), lpCameraInput->GetPosition(),
                lpCameraInput->GetDirection(), lfLodZoomFactor, &mShaderLodInfo,
                liCascadeList, liCascadeList, liCascadeList, liCascadeList, true );
        }
        PerfMonCpu::StopMonitor( miGenerateDispatchListsPM );

        PerfMonCpu::StartMonitor( miPropGenerateDispListClearPM );
        if ( lbProps )
        {
            mPropEntityModule.GenerateDispatchLists(
                lpPropDispatchInput, lpFilteredEntityData->maPropEntityIds,
                lpCascadeCamera->GetViewProjectionMatrix(), lpCameraInput->GetPosition(),
                lfLodZoomFactor, &mShaderLodInfo, liCascadeList, liCascadeList, liCascadeList );
        }
        PerfMonCpu::StopMonitor( miPropGenerateDispListClearPM );

        mShadowMap.SetRenderingShadowMap( false );
    }
}

}
