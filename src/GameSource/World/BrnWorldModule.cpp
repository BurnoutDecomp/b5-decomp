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
#include <ctime>   // [DIAG culling wave] clock() for the producer-fps readout
#include <cstdlib>                                                // getenv/atof (the BRN_WORLD_CAMDIST bring-up diagnostic)
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
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // the SCENE-stage allocator-hold one-shot log
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysSharedIO.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsSpatialPartitionManager.h"
#include "GameSource/World/Bridges/WorldBridgeEntityModulesToOutput.h"
#include "GameSource/World/Bridges/WorldBridgeToEntityModules.h"
#include "GameSource/World/Bridges/WorldBridgeSceneToEntityModules.h"
#include "GameSource/World/Bridges/WorldBridgeCrashToEntityModules.h"
#include "GameSource/World/Bridges/WorldBridgeEntityModulesToEntityModules.h"
#include "GameSource/World/Bridges/WorldBridgeEntityModulesToScene.h"
// ---- the world-drive (Update @0x827D63E8) bridge families -------------------
#include "GameSource/World/Bridges/WorldBridgeInputToEntityModules.h"
#include "GameSource/World/Bridges/WorldBridgeInputToAI.h"
#include "GameSource/World/Bridges/WorldBridgeEntityModulesToAI.h"
#include "GameSource/World/Bridges/WorldBridgeEntityModulesToCrash.h"
#include "GameSource/World/Bridges/WorldBridgeEntityModulesToPhysics.h"
#include "GameSource/World/Bridges/WorldBridgeAIToEntityModules.h"
#include "GameSource/World/Bridges/WorldBridgePhysicsToEntityModules.h"
#include "GameSource/World/Bridges/WorldBridgePhysicsToScene.h"
#include "GameSource/World/Bridges/WorldBridgeSceneToPhysics.h"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h" // the frame collision generator Update carves
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameSource/World/BrnWorldModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // VariableEventQueue<4096,16>::AddEvent
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"       // CgsResource::ID::HashString
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"       // BrnResource::GameDataIO::RequestInterface<4096>

#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"     // DispatchFrame / DispatchList
#include <cmath>   // sqrtf / tanf ([FLAG PC bring-up] the dispatch producer's camera)

// The global runtime shader-constant register (X360 symbol mShaderConstantTable;
// same extern as the world-entity TU -- the defining home lands with the shader TU).
namespace CgsGraphics { extern ShaderConstantTable mShaderConstantTable; }

// [FLAG PC bring-up] the PC back-buffer extent (pc/gcm/renderengine/device.h). Declared
// here rather than included: that header pulls <windows.h>/<d3d9.h>, which must not enter
// this TU. Only the aspect ratio of the bring-up dispatch camera reads them.
namespace renderengine { extern s32 gDisplayWidth; extern s32 gDisplayHeight; }

namespace BrnWorld
{

static CgsSceneManager::SceneQueryId KA_FRUSTUM_QUERY_IDS[11];
static CgsGraphics::Camera gFrustumQueryCamera;

// The dispatch-pass camera (X360 file static at 0x8300FB40).
static CgsGraphics::Camera gDispatchCamera;

// The world dispatch/sort list ids the X360 passes in registers to the world
// module's dispatch feed (dropped by the decompiler at the call site).
//
// CORRECTED (world-pixels wave 2026-07-28) against the renderer's list map and a
// live run. `liList` is the GDL OBJECT list RenderInstance submits the
// DRAWRENDERABLE packet into (ConvertObjectsToMeshes expands object lists 0..12);
// `liSortLayer` / `liSortKey` are the DESTINATION MESH list ids DrawRenderable::
// Interpret routes each expanded mesh to -- opaque and transparent respectively.
// The old 19/20 were the CAR opaque/transparent mesh lists, and a boot with the
// producer live proved it: the world's 187 expanded meshes landed in mesh list 19.
// The X360 world pass is list 11 opaque / 15 transparent / 21 pre-Z (renderer
// Render @0x8240BFA8: mbRenderWorldOpaque walks 11, mbRenderWorldTransparent 15,
// mbRenderPreZ 21, mbRenderCarsOpaque 19, mbRenderCarsTransparent 20).
static const s32 KI_WORLD_OPAQUE_LIST = 2;    // GDL object list
static const s32 KI_WORLD_SORT_LAYER  = 11;   // -> mesh list: WORLD OPAQUE
static const s32 KI_WORLD_SORT_KEY    = 15;   // -> mesh list: WORLD TRANSPARENT
static const s32 KI_WORLD_PREZ_LIST   = 21;   // -> mesh list: PRE-Z

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
                // as { &mReceiverQueue, 1, 5 (pool), HashString("Districts") } and pushes
                // it via VariableEventQueue<4096,16>::AddEvent(payload, type=4, size=24).
                mReceiverQueue.Clear();
                BrnResource::GameDataIO::RequestInterface<4096>* lpRequest =
                    lpOutput->GetResourceRequestResourceInterface();

                // CONSOLE payload order (asm 0x827D11D8): v9[0]=&queue, v9[1]=1, v9[2]=5 (pool),
                // then the u64 = the RAW HashString("Districts") return at sp+0x60 (the
                // `| 0x500000000` in the pseudocode is a fusion artifact of the `li r10,5 /
                // stw @+8` miPoolId store -- HashString zero-extends, high dword is 0).
                // PoolId lands at +8 and the u64 resourceId at +16 (NOT the reverse).
                struct AcquireEvent
                {
                    CgsModule::BaseEventReceiverQueue* mpReceiverQueue; // CONSOLE +0
                    s32                                miEventId;       // CONSOLE +4  (=1)
                    s32                                miPoolId;        // CONSOLE +8  (=5)
                    s32                                mi_pad;          // CONSOLE +12
                    u64                                muResourceId;    // CONSOLE +16 (untagged hash)
                } lEvent;
                lEvent.mpReceiverQueue = &mReceiverQueue;
                lEvent.miEventId       = 1;
                lEvent.miPoolId        = 5;
                lEvent.mi_pad          = 0;
                lEvent.muResourceId    =
                    static_cast<u64>(static_cast<u32>(CgsResource::ID::HashString(
                        reinterpret_cast<const u8*>("Districts"))));   // untagged (high dword 0)

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

    // [FLAG PC bring-up] the director-camera override starts empty (see the header).
    mbBringUpCameraOverrideValid = false;
    mfBringUpCameraOverrideFOV   = 0.0f;
    mBringUpCameraOverride.SetIdentity();

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
    CgsModule::IOBufferStack* lpInputBufferStack,
    CgsModule::IOBufferStack* lpOutputBufferStack,
    const BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
    const BrnPhysics::PhysicsModuleIO::OutputBuffer* lpPhysicsModuleOutputBuffer,
    BrnUpdateSet lUpdateSet )
{
    CGS_ASSERT( lpInputBufferStack != 0, "lpInputBufferStack != NULL" );   // X360 cpp:2305
    CGS_ASSERT( lpOutputBufferStack != 0, "lpOutputBufferStack != NULL" ); // X360 cpp:2306
    CGS_ASSERT( lpPhysicsModuleOutputBuffer != 0, "lpPhysicsModuleOutputBuffer != NULL" ); // :2307

    // X360: PhysicsModule::UpdateNetworkCatchup(this + 1561376, a4, a6) -- the
    // physics INPUT buffer and the frame update set (the output buffer is only
    // null-checked here).
    mPhysicsModule.UpdateNetworkCatchup( lpPhysicsModuleInputBuffer, lUpdateSet );
}


// ============================================================================
// LoadAttribSysVault  @ 0x827D3D08
//
// The AttribSys world-vault streaming machine (meVaultResourceStage):
//   START            -> LoadBundle "WorldVault.bin" (event id 1, pool 7)
//   LOADING_VAULT    -> on reply, acquire "WorldVault" (type-4 request; untagged
//                       hash id + pool 7 in the miPoolId field)
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
            // X360 id = the RAW zero-extended HashString return (asm @0x827D3DEC:
            // `bl HashString; mr r11,r3; std r11` -- HashString @0x828D84A8 ends
            // `clrldi r3,32`, so the high dword is ZERO). The earlier `| 0x700000000`
            // was a Hex-Rays fusion artifact of the separate `li r10,7 / stw @+8`
            // miPoolId store; the pool rides the miPoolId field, never the id.
            lRequest.mResourceId.SetHash(
                static_cast<u64>( static_cast<u32>( CgsResource::ID::HashString(
                    reinterpret_cast<const u8*>( "WorldVault" ) ) ) ) );
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

            // [FLAG PC boot gate] the world vault bundle is still X360 big-endian data on
            // PC (the loader refuses it), so the acquire legitimately resolves to a null
            // handle here -- a state the X360 can never reach (its vault always loads).
            // Registering a null vault would fire RegisterVault's handle assert; hold this
            // stage (one-shot log) until the vault bundle is ported to platform 4.
            if ( lpResponse == 0 || lpResponse->mpSourceEntry == 0 )
            {
                static bool s_bLoggedVaultHold = false;
                if ( !s_bLoggedVaultHold )
                {
                    s_bLoggedVaultHold = true;
                    if ( CgsDev::Message::gxMessageFilterFlags & 1 )
                        *CgsDev::Log::gpDebugPrint
                            << "WorldModule::LoadAttribSysVault: ACQUIRING_VAULT holding -- "
                               "'WorldVault' resource absent (vault bundle not PC-converted) "
                               "[FLAG PC boot gate]\n";
                }
                mReceiverQueue.Clear();
                break;
            }

            mAttribSysVaultResourceHandle.mpResourceMemory = lpResponse->mpResourceMemory;
            mAttribSysVaultResourceHandle.mpSourceEntry    = lpResponse->mpSourceEntry;

            // X360 @0x827D3F04-ish: RegisterVault(iface, &mReceiverQueue, handle,
            // r7 = 1 (miEventId), r8 = 0 (E_VAULT_TYPE_RESIDENT)) -- the push helper
            // @0x8229D6C8 stores r7 @+12 (miEventId) and r8 @+16 (meVaultType).
            lpOutput->GetAttribSysVaultRequestInterface()->RegisterVault(
                &mReceiverQueue, mAttribSysVaultResourceHandle,
                /*liEventId*/ 1, CgsAttribSys::AttribSysIO::E_VAULT_TYPE_RESIDENT );

            mReceiverQueue.Clear();
            meVaultResourceStage = E_RESOURCESTAGE_REGISTERING_VAULT;
            break;
        }

        case E_RESOURCESTAGE_REGISTERING_VAULT:
        {
            if ( mReceiverQueue.GetLength() <= 0 )
            {
                // Waiting for the AttribSysModule's type-3 RegisterVault reply (posted by
                // AttribSysModule::RegisterVault @0x8280EBF0 straight to this receiver
                // queue; the request rode the world output's attrib interface ->
                // LoadWorldModule's Append<2048> into the GameData input -> the GameData
                // pump's "Attrib" module input). Same wait shape as the X360.
                break;
            }

            if ( CgsDev::Message::gxMessageFilterFlags & 1 )
                *CgsDev::Log::gpDebugPrint
                    << "WorldModule::LoadAttribSysVault: vault registered (AttribSys "
                       "reply received)\n";

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
            lParams.meType     = static_cast<CgsSceneManager::ESpatialPartitionType>( 1 );
            lParams.muDepth    = 3;
            lParams.mCentrePos.SetZero();
            lParams.mfBaseSize = 11000.0f;
            lParams.mfLooseness = 0.30000001f;
            lParams.muAdaptiveNodeSplitThreshold = 32;
            lParams.muAdaptiveMaxDepth           = 10;

            // [FLAG PC boot gate] GameDataModule::CreateAllocators (0x8266DD00) is still an
            // inert stand-in, so the allocator registry cannot serve the scene (49) /
            // physics (23) / triangle-collision (61) allocators yet. On the X360 these are
            // always live by world-prepare time; a null here would fail the asserts below
            // and then fault inside SceneManagerModule::Prepare. Hold the stage (resumable
            // "not ready yet" -- the spine keeps re-driving) with a one-shot log instead of
            // inventing allocators. Remove this gate when CreateAllocators lands.
            if ( lpAllocatorList == 0 ||
                 lpAllocatorList->GetRWLinearResourceAllocator( 49 ) == 0 ||
                 lpAllocatorList->GetRWLinearResourceAllocator( 23 ) == 0 ||
                 lpAllocatorList->GetLinearAllocator( 61 ) == 0 )
            {
                static bool s_bLoggedAllocatorHold = false;
                if ( !s_bLoggedAllocatorHold )
                {
                    s_bLoggedAllocatorHold = true;
                    if ( CgsDev::Message::gxMessageFilterFlags & 1 )
                        *CgsDev::Log::gpDebugPrint
                            << "WorldModule::Prepare: SCENE stage holding -- allocator registry "
                               "empty (CreateAllocators deferred) [FLAG PC boot gate]\n";
                }
                return false;
            }

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
            // The X360 CreateIOBuffer<T> instantiations run the buffers' Construct after
            // the stack alloc; the generic PC template placement-news only (same pattern
            // as LoadWorldModule's world output buffer).
            lpSceneInput->Construct();
            lpSceneOutput->Construct();

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

            mSceneModule.UpdateScene( lpInputBufferStack, lpOutputBufferStack,
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
            // PC Construct restoration (see the SCENE stage above).
            lpSceneInput->Construct();
            lpSceneOutput->Construct();

            lpSceneInput->LockForWrite();
            const bool lbPhysicsPrepared = mPhysicsModule.Prepare(
                lpInputBufferStack, lpOutputBufferStack, lpSceneInput, lpAllocatorList );
            lpSceneInput->UnlockForWrite();

            mSceneModule.UpdateScene( lpInputBufferStack, lpOutputBufferStack,
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
            // PC Construct restoration (the X360 CreateIOBuffer<T> stack template runs
            // T::Construct after the alloc; the generic PC template placement-news only).
            lpRaceCarOutput->Construct();
            // PC Construct restoration (base IOBuffer status; the buffer interior is a
            // minimal slice and its module Prepare is boot-gated).
            lpRaceCarOutput->CgsModule::IOBuffer::Construct();

            if ( !mRaceCarEntityModule.Prepare( lpRaceCarOutput, mDistrictMapResourceHandle ) )
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
            // PC Construct restoration (the X360 CreateIOBuffer<T> stack template runs
            // T::Construct after the alloc; the generic PC template placement-news only).
            lpTrafficOutput->Construct();
            // PC Construct restoration (see the RaceCar stage above).
            lpTrafficOutput->CgsModule::IOBuffer::Construct();

            if ( !mTrafficEntityModule.Prepare( lpTrafficOutput ) )
            {
                CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInput = 0;
                CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneOutput = 0;
                lpInputBufferStack->CreateIOBuffer( &lpSceneInput, "Scene" );
                lpOutputBufferStack->CreateIOBuffer( &lpSceneOutput, "Scene" );
                // PC Construct restoration (see the SCENE stage above).
                lpSceneInput->Construct();
                lpSceneOutput->Construct();

                CgsModule::LockBuffersForIO( lpSceneInput, lpTrafficOutput );
                ::WorldModule::BridgeTrafficModuleToSceneModule_Prepare(
                    this, lpSceneInput, lpTrafficOutput );
                CgsModule::UnlockBuffersForIO( lpSceneInput, lpTrafficOutput );

                mSceneModule.UpdateScene( lpInputBufferStack, lpOutputBufferStack,
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
            // PC Construct restoration (see the SCENE stage above).
            lpWorldEntityOutput->Construct();
            lpSceneInput->Construct();
            lpSceneOutput->Construct();

            if ( !mWorldEntityModule.Prepare( lpWorldEntityOutput ) )
            {
                CgsModule::LockBuffersForIO( lpUpdateOutputBuffer, lpWorldEntityOutput );
                ::WorldModule::BridgeWorldResourceRequestsToOutput_Prepare(
                    this, lpUpdateOutputBuffer, lpWorldEntityOutput );
                CgsModule::UnlockBuffersForIO( lpUpdateOutputBuffer, lpWorldEntityOutput );

                CgsModule::LockBuffersForIO( lpSceneInput, lpWorldEntityOutput );
                // The world-entity output is the SOURCE of this bracket, i.e. READ-locked,
                // so the read has to go through the CONST twin (0x827BBBA8, bit 4) --
                // the non-const one (0x827BBC50, bit 3) asserts the WRITE lock.
                {
                    const WorldEntityIO::OutputBuffer_Prepare* lpWorldEntityRead = lpWorldEntityOutput;
                    lpSceneInput->GetInSceneUpdateInterface()->Append(
                        *lpWorldEntityRead->GetSceneInputInterface() );
                }
                CgsModule::UnlockBuffersForIO( lpSceneInput, lpWorldEntityOutput );

                mSceneModule.UpdateScene( lpInputBufferStack, lpOutputBufferStack,
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
            // PC Construct restoration (the X360 CreateIOBuffer<T> stack template runs
            // T::Construct after the alloc; the generic PC template placement-news only).
            lpPropOutput->Construct();
            // PC Construct restoration (see the RaceCar stage above).
            lpPropOutput->CgsModule::IOBuffer::Construct();

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
                // PC Construct restoration (see the SCENE stage above).
                lpSceneInput->Construct();
                lpSceneOutput->Construct();

                CgsModule::LockBuffersForIO( lpSceneInput, lpPropOutput );
                ::WorldModule::BridgePropModuleToSceneModule_Prepare(
                    this, lpSceneInput, lpPropOutput );
                CgsModule::UnlockBuffersForIO( lpSceneInput, lpPropOutput );

                mSceneModule.UpdateScene( lpInputBufferStack, lpOutputBufferStack,
                                     lpSceneInput, lpSceneOutput, true );

                lpOutputBufferStack->DestroyIOBuffer( &lpSceneOutput );
                lpInputBufferStack->DestroyIOBuffer( &lpSceneInput );
            }

            {
                BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsInput = 0;
                lpInputBufferStack->CreateIOBuffer( &lpPhysicsInput, "Physics" );
                // PC Construct restoration (base IOBuffer status; minimal-slice interior).
                lpPhysicsInput->CgsModule::IOBuffer::Construct();

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
            // PC Construct restoration (see the RaceCar stage above).
            lpAIOutput->CgsModule::IOBuffer::Construct();
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

            // [FLAG PC boot gate 2026-07-26] CrashModule's own staged Prepare override is
            // not committed (the behavioural slice defers the lifecycle set to its own TU),
            // so the call resolves to the BASE ModuleSingleBuffered::Prepare, which asserts
            // "new module type" on the data-structure path. Skip (one-shot log) until the
            // crash lifecycle TU lands; the module stays inert.
            {
                static bool s_bLoggedCrashGate = false;
                if ( !s_bLoggedCrashGate )
                {
                    s_bLoggedCrashGate = true;
                    if ( CgsDev::Message::gxMessageFilterFlags & 1 )
                        *CgsDev::Log::gpDebugPrint
                            << "WorldModule::Prepare: CrashModule::Prepare skipped "
                               "(lifecycle TU deferred) [FLAG PC boot gate]\n";
                }
            }
            if ( false && !mCrashModule.Prepare() )
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
// PrepareWorldCollision  @ 0x827C9478
//
// One frame of the WORLD COLLISION prepare -- the scripted load's stage 7. Same
// shape as Prepare's eWorldPrepareWorldEntityModule stage, but driving
// WorldEntityModule::PrepareWorldCollision instead of ::Prepare: create the
// world-entity prepare output + the scene IO pair, run one step of the collision
// prepare under the world-entity buffer's own write lock, and -- while it reports
// "still working" -- bridge the staged resource requests out to the caller's
// update output and push the staged scene requests through the scene manager.
//
// Returns the sub-module's own answer: TRUE == the whole world collision is
// prepared. (LoadingScriptedState::LoadWorldCollision inverts nothing -- it takes
// the true arm to fire EffectsModule::PostWorldPreparePrepare, and the false arm
// to forward the requests, so a "false" here means "call me again next frame".)
//
// ⚠️ Two lock brackets, three accessors, and the console picks a DIFFERENT overload
// in each: under its own LockForWrite it takes the NON-CONST scene-input accessor
// (0x827BBC50, bit 3) and the NON-CONST request accessor (0x822BA180); under the
// LockBuffersForIO read bracket it takes the CONST scene-input accessor
// (0x827BBBA8, bit 4). Getting that wrong is what cost the previous wave 927
// asserts on the sibling scene-manager pair.
// ============================================================================
bool
WorldModule::PrepareWorldCollision( CgsModule::IOBufferStack* lpInputBufferStack,
                                    CgsModule::IOBufferStack* lpOutputBufferStack,
                                    BrnWorldIO::UpdateOutputBuffer* lpOutput )
{
    WorldEntityIO::OutputBuffer_Prepare*                  lpWorldEntityOutput = 0;
    CgsSceneManager::SceneManagerIO::InputBuffer_Update*  lpSceneInput        = 0;
    CgsSceneManager::SceneManagerIO::OutputBuffer*        lpSceneOutput       = 0;

    // The X360 asserts each CreateIOBuffer through the CgsModuleIOHelper.h:52 wrapper
    // ("mpStack->CreateIOBuffer( &mpBuffer, lpcName )"); the PC stack template returns the
    // same bool, so the guard is kept verbatim.
    CGS_ASSERT( lpOutputBufferStack->CreateIOBuffer( &lpWorldEntityOutput, "WorldEntityPrepare" ),
                "mpStack->CreateIOBuffer( &mpBuffer, lpcName )" );
    CGS_ASSERT( lpInputBufferStack->CreateIOBuffer( &lpSceneInput, "Scene" ),
                "mpStack->CreateIOBuffer( &mpBuffer, lpcName )" );
    CGS_ASSERT( lpOutputBufferStack->CreateIOBuffer( &lpSceneOutput, "Scene" ),
                "mpStack->CreateIOBuffer( &mpBuffer, lpcName )" );

    // PC Construct restoration (the X360 CreateIOBuffer<T> stack template runs T::Construct
    // after the alloc; the generic PC template placement-news only) -- the same restoration
    // Prepare's WORLDENTITY stage does for this exact buffer trio.
    lpWorldEntityOutput->Construct();
    lpSceneInput->Construct();
    lpSceneOutput->Construct();

    lpWorldEntityOutput->LockForWrite();
    // Both reached through the NON-CONST overloads: this buffer is WRITE-locked here.
    WorldEntityIO::SceneInputInterface*    lpSceneInterface   = lpWorldEntityOutput->GetSceneInputInterface();
    WorldEntityIO::ResourceRequestInterface* lpRequestInterface = lpWorldEntityOutput->GetResourceRequestInterface();
    const bool lbPrepared =
        mWorldEntityModule.PrepareWorldCollision( lpRequestInterface, lpSceneInterface, true );
    lpWorldEntityOutput->UnlockForWrite();

    if ( !lbPrepared )
    {
        CgsModule::LockBuffersForIO( lpOutput, lpWorldEntityOutput );
        ::WorldModule::BridgeWorldResourceRequestsToOutput_Prepare(
            this, lpOutput, lpWorldEntityOutput );
        CgsModule::UnlockBuffersForIO( lpOutput, lpWorldEntityOutput );

        CgsModule::LockBuffersForIO( lpSceneInput, lpWorldEntityOutput );
        {
            // READ-locked here -> the CONST twin (0x827BBBA8).
            const WorldEntityIO::OutputBuffer_Prepare* lpWorldEntityRead = lpWorldEntityOutput;
            lpSceneInput->GetInSceneUpdateInterface()->Append(
                *lpWorldEntityRead->GetSceneInputInterface() );
        }
        CgsModule::UnlockBuffersForIO( lpSceneInput, lpWorldEntityOutput );

        // The X360 reaches this through the module's vtable +0x40 on this + 2002304
        // (== &mSceneModule); named here, same as Prepare's WORLDENTITY stage.
        mSceneModule.UpdateScene( lpInputBufferStack, lpOutputBufferStack,
                                  lpSceneInput, lpSceneOutput, true );
    }

    CGS_ASSERT( lpOutputBufferStack->DestroyIOBuffer( &lpSceneOutput ),
                "mpStack->DestroyIOBuffer( &mpBuffer )" );       // CgsModuleIOHelper.h:57
    CGS_ASSERT( lpInputBufferStack->DestroyIOBuffer( &lpSceneInput ),
                "mpStack->DestroyIOBuffer( &mpBuffer )" );
    CGS_ASSERT( lpOutputBufferStack->DestroyIOBuffer( &lpWorldEntityOutput ),
                "mpStack->DestroyIOBuffer( &mpBuffer )" );

    return lbPrepared;
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
                // ⛔ THE BAIL PREVENTS AN OUT-OF-BOUNDS WRITE. On a miss the index is -1 and the
                // console's own next instruction indexes the array with it -- `maeCarControls[-1]`
                // lands on mfLocalPlayerActiveRaceCarSpeed, the member immediately before the
                // array, and silently overwrites it with the payload word. The console has no
                // bounds check here either; its index simply is not -1 by the time an action 7
                // arrives.
                //
                // ⚠️ BANNER CORRECTED 2026-08-01 (physics wave 1) -- IT WAS STALE AND IT MISLED.
                // This block used to say the index is "ALWAYS -1" because
                // WorldModule::BridgeRaceCarModuleToWorldModule_PreScene "is not reconstructed".
                // ⛔ THAT BRIDGE HAS LANDED: it is real, it is MOUNTED
                // (WorldBridgeRaceCarToWorldModule.cpp, build_game_exe.bat line 137), and the
                // car-select wave measured the player active-race-car index published as 0.
                // KEEP THE GUARD ANYWAY: the assert still fires exactly once per boot, on the
                // ONE-FRAME TRANSIENT at the slot-0 -> slot-1 car swap, before the bridge has
                // republished the new slot. (Measured again this wave: exactly one
                // "[ASSERT 1] Unable to set the player car under AI control ..." per boot.)
                //
                // ⛔ AND THE INDEX EXPRESSION BELOW IS NOT A TRANSCRIPTION BUG -- do not "fix" it.
                // X360 HandleGameActions @0x827C44D8, jump-table case 0 (== action 7):
                //     lis r11,0x17 ; ori r22,r11,0x86BC     ; r22 = 0x1786BC
                //     lwz r11, 0(r31)                       ; meLocalPlayerActiveRaceCarIndex
                //     add r11, r11, r22 ; slwi r11, r11, 2  ; (idx + 0x1786BC) * 4
                //     stwx r10, r11, r26                    ; *(this + 4*idx + 0x5E1AF0) = payload[0]
                // and 0x1786BC * 4 == 0x5E1AF0 == &maeCarControls exactly.
                // DELETE-WHEN: nothing. The guard is now permanent PC-side hardening of a
                // console behaviour, not a placeholder.
                if ( static_cast<s32>( meLocalPlayerActiveRaceCarIndex ) < 0 ||
                     static_cast<s32>( meLocalPlayerActiveRaceCarIndex ) >= 8 )
                {
                    break;
                }
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
        // PC Construct restoration (the X360 CreateIOBuffer<T> stack template runs
        // T::Construct after the alloc; the generic PC template placement-news only).
        lpQueryInput->Construct();
        lpQueryOutput->Construct();

        PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_RaceCar_Bridge );
        CgsModule::LockBuffersForIO( lpQueryInput, lpRaceCarOutputBuffer_PostScene );
        ::WorldModule::BridgeRaceCarModuleToSceneModule_PostScene(
            this, lpQueryInput, lpRaceCarOutputBuffer_PostScene );
        CgsModule::UnlockBuffersForIO( lpQueryInput, lpRaceCarOutputBuffer_PostScene );
        PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_RaceCar_Bridge );

        PerfMonCpu::StartMonitor( miSceneManagerQueryPM );
        mSceneModule.ProcessSceneQueries( lpInputBufferStack, lpOutputBufferStack,
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
        // PC Construct restoration (the X360 CreateIOBuffer<T> stack template runs
        // T::Construct after the alloc; the generic PC template placement-news only).
        lpQueryInput->Construct();
        lpQueryOutput->Construct();

        PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Traffic_Bridge );
        CgsModule::LockBuffersForIO( lpQueryInput, lpTrafficOutputBuffer_PostScene );
        ::WorldModule::BridgeTrafficModuleToSceneModule_PostScene(
            this, lpQueryInput, lpTrafficOutputBuffer_PostScene );
        CgsModule::UnlockBuffersForIO( lpQueryInput, lpTrafficOutputBuffer_PostScene );
        PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Traffic_Bridge );

        PerfMonCpu::StartMonitor( miSceneManagerQueryPM );
        mSceneModule.ProcessSceneQueries( lpInputBufferStack, lpOutputBufferStack,
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
        // PC Construct restoration (the X360 CreateIOBuffer<T> stack template runs
        // T::Construct after the alloc; the generic PC template placement-news only).
        lpTriggerOutput->Construct();

        mTriggerEntityModule.PostSceneUpdate( lpInputBufferStack, lpOutputBufferStack,
                                              lpTriggerInputBuffer_PostScene,
                                              lpTriggerOutput, lUpdateSet );

        PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Triggers_SQ );
        {
            CgsSceneManager::SceneManagerIO::InputBuffer_Query* lpQueryInput = 0;
            CgsSceneManager::SceneManagerIO::OutputBuffer* lpQueryOutput = 0;
            lpInputBufferStack->CreateIOBuffer( &lpQueryInput, "Scene" );
            lpOutputBufferStack->CreateIOBuffer( &lpQueryOutput, "Scene" );
            // PC Construct restoration (the X360 CreateIOBuffer<T> stack template runs
            // T::Construct after the alloc; the generic PC template placement-news only).
            lpQueryInput->Construct();
            lpQueryOutput->Construct();

            CgsModule::LockBuffersForIO( lpQueryInput, lpTriggerOutput );
            ::WorldModule::BridgeTriggerModuleToSceneModule_PostScene(
                this, lpQueryInput, lpTriggerOutput );
            CgsModule::UnlockBuffersForIO( lpQueryInput, lpTriggerOutput );

            PerfMonCpu::StartMonitor( miSceneManagerQueryPM );
            mSceneModule.ProcessSceneQueries( lpInputBufferStack, lpOutputBufferStack,
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
// EntityModulePreSceneUpdate  @ 0x827BD1F0   (DWARF BrnWorldModule.h:398)
//
// The per-frame PRE-SCENE entity-module spine:
//   * race car pre-scene (bracketed by NetworkAIRaceCar + RaceCar UT monitors);
//   * the race-car -> traffic staging (pre-scene + post-scene + post-physics
//     traffic inputs primed from the race-car pre-scene output) and the
//     environment time-of-day copy into the traffic pre-scene input;
//   * traffic pre-scene;
//   * the race-car -> WORLD staging, then the WORLD ENTITY pre-scene -- the PVS
//     zone query + UpdateStream, i.e. the world streamer's per-frame drive. The
//     X360 loads the simulated camera position from this+6167792 (the last
//     director camera's position row) into v1 for the call;
//   * prop pre-scene (race-car + world staged in first), via the prop module's
//     X360 vtbl+68 slot (devirtualised per the PC convention);
//   * the traffic -> trigger staging, then trigger pre-scene (X360 vtbl+64).
// ============================================================================
void
WorldModule::EntityModulePreSceneUpdate(
    CgsModule::IOBufferStack* lpInputBufferStack,
    CgsModule::IOBufferStack* lpOutputBufferStack,
    TriggerEntityModuleIO::InputBuffer_PreScene* lpTriggerInputBuffer_PreScene,
    TriggerEntityModuleIO::OutputBuffer_PreScene* lpTriggerOutputBuffer_PreScene,
    BrnTraffic::BrnTrafficIO::InputBuffer_PreScene* lpTrafficInputBuffer_PreScene,
    BrnTraffic::BrnTrafficIO::OutputBuffer_PreScene* lpTrafficOutputBuffer_PreScene,
    BrnTraffic::BrnTrafficIO::InputBuffer_PostScene* lpTrafficInputBuffer_PostScene,
    BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics* lpTrafficInputBuffer_PostPhysics,
    RaceCarEntityModuleIO::InputBuffer_PreScene* lpRaceCarInputBuffer_PreScene,
    RaceCarEntityModuleIO::OutputBuffer_PreScene* lpRaceCarOutputBuffer_PreScene,
    PropEntityIO::InputBuffer_PreScene* lpPropInputBuffer_PreScene,
    PropEntityIO::OutputBuffer_PreScene* lpPropOutputBuffer_PreScene,
    WorldEntityIO::InputBuffer_PreScene* lpWorldInputBuffer_PreScene,
    WorldEntityIO::OutputBuffer_PreScene* lpWorldOutputBuffer_PreScene,
    BrnUpdateSet lUpdateSet )
{
    using namespace CgsDev;

    // ---- race car ----------------------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_NetworkAIRaceCar );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_RaceCar );

    mRaceCarEntityModule.PreSceneUpdate( lpRaceCarInputBuffer_PreScene,
                                         lpRaceCarOutputBuffer_PreScene, lUpdateSet );

    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_RaceCar );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_NetworkAIRaceCar );

    // ---- race car -> traffic staging + the env time-of-day copy ------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Traffic );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Traffic_Bridge );

    CgsModule::LockBuffersForIO( lpTrafficInputBuffer_PreScene, lpRaceCarOutputBuffer_PreScene );
    lpTrafficInputBuffer_PostScene->LockForWrite();
    lpTrafficInputBuffer_PostPhysics->LockForWrite();
    ::WorldModule::BridgeRaceCarModuleToTrafficModule_PreScene(
        this, lpTrafficInputBuffer_PreScene, lpTrafficInputBuffer_PostScene,
        lpTrafficInputBuffer_PostPhysics, lpRaceCarOutputBuffer_PreScene );
    lpTrafficInputBuffer_PostPhysics->UnlockForWrite();
    lpTrafficInputBuffer_PostScene->UnlockForWrite();
    CgsModule::UnlockBuffersForIO( lpTrafficInputBuffer_PreScene, lpRaceCarOutputBuffer_PreScene );

    // The environment manager's current time-of-day seconds into the traffic
    // input (X360 raw f32 store: trafficPreIn+13072 <- this+1995876).
    lpTrafficInputBuffer_PreScene->LockForWrite();
    lpTrafficInputBuffer_PreScene->SetTimeOfDaySeconds( mEnvironmentManager.GetCurrTimeOfDay() );
    lpTrafficInputBuffer_PreScene->UnlockForWrite();

    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Traffic_Bridge );

    mTrafficEntityModule.PreSceneUpdate( lpInputBufferStack, lpOutputBufferStack,
                                         lpTrafficInputBuffer_PreScene,
                                         lpTrafficOutputBuffer_PreScene, lUpdateSet );

    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Traffic );

    // ---- WORLD entity module (the PVS query + streamer drive) --------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_World );

    CgsModule::LockBuffersForIO( lpWorldInputBuffer_PreScene, lpRaceCarOutputBuffer_PreScene );
    ::WorldModule::BridgeRaceCarModuleToWorldModule_PreScene(
        this, lpWorldInputBuffer_PreScene, lpRaceCarOutputBuffer_PreScene );
    CgsModule::UnlockBuffersForIO( lpWorldInputBuffer_PreScene, lpRaceCarOutputBuffer_PreScene );

    // v1 = the last director camera's position row (X360 lvx128 this+6167792).
    mWorldEntityModule.PreSceneUpdate( lpWorldInputBuffer_PreScene,
                                       lpWorldOutputBuffer_PreScene,
                                       mLastCameraInput.GetPosition(), lUpdateSet );

    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_World );

    // ---- prop --------------------------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Physics );
    PerfMonCpu::StartMonitor( miPhysicsSummaryPM );
    PerfMonCpu::StartMonitor( miPhysicsPropSummaryPM );
    PerfMonCpu::StartMonitor( miPhysicsPropBridgePM );

    CgsModule::LockBuffersForIO( lpPropInputBuffer_PreScene, lpRaceCarOutputBuffer_PreScene );
    ::WorldModule::BridgeRaceCarModuleToPropModule_PreScene(
        this, lpPropInputBuffer_PreScene, lpRaceCarOutputBuffer_PreScene, lUpdateSet );
    CgsModule::UnlockBuffersForIO( lpPropInputBuffer_PreScene, lpRaceCarOutputBuffer_PreScene );

    CgsModule::LockBuffersForIO( lpPropInputBuffer_PreScene, lpWorldOutputBuffer_PreScene );
    ::WorldModule::BridgeWorldModuleToPropModule_PreScene(
        this, lpPropInputBuffer_PreScene, lpWorldOutputBuffer_PreScene );
    CgsModule::UnlockBuffersForIO( lpPropInputBuffer_PreScene, lpWorldOutputBuffer_PreScene );

    PerfMonCpu::StopMonitor( miPhysicsPropBridgePM );

    PerfMonCpu::StartMonitor( miPhysicsPropPreSceneUpdatePM );
    // X360 (*(vtbl(mPropEntityModule) + 68)) -- devirtualised.
    mPropEntityModule.PreSceneUpdate( lpInputBufferStack, lpOutputBufferStack,
                                      lpPropInputBuffer_PreScene,
                                      lpPropOutputBuffer_PreScene, lUpdateSet );
    PerfMonCpu::StopMonitor( miPhysicsPropPreSceneUpdatePM );

    PerfMonCpu::StopMonitor( miPhysicsPropSummaryPM );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Physics );
    PerfMonCpu::StopMonitor( miPhysicsSummaryPM );

    // ---- trigger -----------------------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Triggers );

    CgsModule::LockBuffersForIO( lpTriggerInputBuffer_PreScene, lpTrafficOutputBuffer_PreScene );
    ::WorldModule::BridgeTrafficToTrigger_PreScene(
        this, lpTriggerInputBuffer_PreScene, lpTrafficOutputBuffer_PreScene );
    CgsModule::UnlockBuffersForIO( lpTriggerInputBuffer_PreScene, lpTrafficOutputBuffer_PreScene );

    // X360 (*(vtbl(mTriggerEntityModule) + 64)) -- devirtualised.
    mTriggerEntityModule.PreSceneUpdate( lpInputBufferStack, lpOutputBufferStack,
                                         lpTriggerInputBuffer_PreScene,
                                         lpTriggerOutputBuffer_PreScene, lUpdateSet );

    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Triggers );
}


// ============================================================================
// EntityModulePostPhysicsUpdate  @ 0x827D3F10   (DWARF BrnWorldModule.h:407)
//
// The per-frame POST-PHYSICS entity-module spine: race car, traffic, prop
// (X360 vtbl+80), the WORLD ENTITY module (the collision-world validate
// protocol + the streamer's GameData request flush) and the crash module, each
// staged from the physics module's output buffer first.
// ============================================================================
void
WorldModule::EntityModulePostPhysicsUpdate(
    CgsModule::IOBufferStack* lpInputBufferStack,
    CgsModule::IOBufferStack* lpOutputBufferStack,
    const BrnPhysics::PhysicsModuleIO::OutputBuffer* lpPhysicsModuleOutputBuffer,
    BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics* lpTrafficInputBuffer_PostPhysics,
    BrnTraffic::BrnTrafficIO::OutputBuffer_PostPhysics* lpTrafficOutputBuffer_PostPhysics,
    RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpRaceCarInputBuffer_PostPhysics,
    RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpRaceCarOutputBuffer_PostPhysics,
    CrashIO::InputBuffer_PostPhysics* lpCrashInputBuffer_PostPhysics,
    CrashIO::OutputBuffer_PostPhysics* lpCrashOutputBuffer_PostPhysics,
    PropEntityIO::InputBuffer_PostPhysics* lpPropInputBuffer_PostPhysics,
    PropEntityIO::OutputBuffer_PostPhysics* lpPropOutputBuffer_PostPhysics,
    WorldEntityIO::InputBuffer_PostPhysics* lpWorldInputBuffer_PostPhysics,
    WorldEntityIO::OutputBuffer_PostPhysics* lpWorldOutputBuffer_PostPhysics,
    BrnUpdateSet lUpdateSet )
{
    CGS_ASSERT( lpPhysicsModuleOutputBuffer != 0, "lpPhysicsModuleOutputBuffer != NULL" );

    using namespace CgsDev;

    // ---- race car ----------------------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_NetworkAIRaceCar );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_RaceCar );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_RaceCar_Bridge );

    CgsModule::LockBuffersForIO( lpRaceCarInputBuffer_PostPhysics, lpPhysicsModuleOutputBuffer );
    ::WorldModule::BridgePhysicsModuleToRaceCarModule_PostPhysics(
        this, lpRaceCarInputBuffer_PostPhysics, lpPhysicsModuleOutputBuffer );
    CgsModule::UnlockBuffersForIO( lpRaceCarInputBuffer_PostPhysics, lpPhysicsModuleOutputBuffer );

    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_RaceCar_Bridge );

    mRaceCarEntityModule.PostPhysicsUpdate( lpRaceCarInputBuffer_PostPhysics,
                                            lpRaceCarOutputBuffer_PostPhysics, lUpdateSet );

    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_RaceCar );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_NetworkAIRaceCar );

    // ---- traffic -----------------------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Traffic );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Traffic_Bridge );

    CgsModule::LockBuffersForIO( lpTrafficInputBuffer_PostPhysics, lpPhysicsModuleOutputBuffer );
    ::WorldModule::BridgePhysicsModuleToTrafficModule_PostPhysics(
        this, lpTrafficInputBuffer_PostPhysics, lpPhysicsModuleOutputBuffer );
    CgsModule::UnlockBuffersForIO( lpTrafficInputBuffer_PostPhysics, lpPhysicsModuleOutputBuffer );

    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Traffic_Bridge );

    mTrafficEntityModule.PostPhysicsUpdate( lpInputBufferStack, lpOutputBufferStack,
                                            lpTrafficInputBuffer_PostPhysics,
                                            lpTrafficOutputBuffer_PostPhysics, lUpdateSet );

    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Traffic );

    // ---- prop --------------------------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Physics );
    PerfMonCpu::StartMonitor( miPhysicsSummaryPM );
    PerfMonCpu::StartMonitor( miPhysicsPropSummaryPM );
    PerfMonCpu::StartMonitor( miPhysicsPropBridgePM );

    CgsModule::LockBuffersForIO( lpPropInputBuffer_PostPhysics, lpPhysicsModuleOutputBuffer );
    ::WorldModule::BridgePhysicsModuleToPropModule_PostPhysics(
        this, lpPropInputBuffer_PostPhysics, lpPhysicsModuleOutputBuffer );
    CgsModule::UnlockBuffersForIO( lpPropInputBuffer_PostPhysics, lpPhysicsModuleOutputBuffer );

    PerfMonCpu::StopMonitor( miPhysicsPropBridgePM );

    PerfMonCpu::StartMonitor( miPhysicsPropPostPhysicsUpdatePM );
    // X360 (*(vtbl(mPropEntityModule) + 80)) -- devirtualised.
    mPropEntityModule.PostPhysicsUpdate( lpInputBufferStack, lpOutputBufferStack,
                                         lpPropInputBuffer_PostPhysics,
                                         lpPropOutputBuffer_PostPhysics, lUpdateSet );
    PerfMonCpu::StopMonitor( miPhysicsPropPostPhysicsUpdatePM );

    PerfMonCpu::StopMonitor( miPhysicsPropSummaryPM );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Physics );
    PerfMonCpu::StopMonitor( miPhysicsSummaryPM );

    // ---- WORLD entity module (validate protocol + streamer request flush) --
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_World );
    mWorldEntityModule.PostPhysicsUpdate( lpWorldInputBuffer_PostPhysics,
                                          lpWorldOutputBuffer_PostPhysics, lUpdateSet );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_World );

    // ---- crash -------------------------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_CrashManager );

    CgsModule::LockBuffersForIO( lpCrashInputBuffer_PostPhysics, lpPhysicsModuleOutputBuffer,
                                 lpTrafficOutputBuffer_PostPhysics );
    ::WorldModule::BridgePhysicsModuleToCrashModule_PostPhysics(
        this, lpCrashInputBuffer_PostPhysics, lpPhysicsModuleOutputBuffer );
    ::WorldModule::BridgeTrafficToCrashModule_PostPhysics(
        this, lpCrashInputBuffer_PostPhysics, lpTrafficOutputBuffer_PostPhysics );
    CgsModule::UnlockBuffersForIO( lpCrashInputBuffer_PostPhysics, lpPhysicsModuleOutputBuffer,
                                   lpTrafficOutputBuffer_PostPhysics );

    mCrashModule.PostPhysicsUpdate( lpInputBufferStack, lpOutputBufferStack,
                                    lpCrashInputBuffer_PostPhysics,
                                    lpCrashOutputBuffer_PostPhysics, lUpdateSet );

    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_CrashManager );
}


// ============================================================================
// UpdateForBootUpVideo  @ 0x827CFDE0   (DWARF BrnWorldModule.h:361)
//
// The trimmed world update the game module runs while the boot-up video plays
// (updateSet & 0x20): drain the world in-event queue into the traffic
// post-physics input, run the world-entity validation protocol, tick traffic
// post-physics, then flush the world-entity resource requests + status and the
// traffic GUI events into the update output.
// ============================================================================
void
WorldModule::UpdateForBootUpVideo( BrnUpdateSet lUpdateSet,
                                   CgsModule::IOBufferStack* lpInputBufferStack,
                                   CgsModule::IOBufferStack* lpOutputBufferStack,
                                   const BrnWorldIO::UpdateInputBuffer* lpUpdateInputBuffer,
                                   BrnWorldIO::UpdateOutputBuffer* lpUpdateOutputBuffer )
{
    WorldEntityIO::OutputBuffer_PostPhysics* lpWorldEntityOutput_PostPhysics = 0;
    BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics* lpTrafficInput_PostPhysics = 0;
    BrnTraffic::BrnTrafficIO::OutputBuffer_PostPhysics* lpTrafficOutput_PostPhysics = 0;

    lpOutputBufferStack->CreateIOBuffer( &lpWorldEntityOutput_PostPhysics, "WorldEntityPostPhysics" );
    lpInputBufferStack->CreateIOBuffer( &lpTrafficInput_PostPhysics, "TrafficPostPhysics" );
    lpOutputBufferStack->CreateIOBuffer( &lpTrafficOutput_PostPhysics, "TrafficPostPhysics" );
    // PC Construct restoration (see WorldModule::Prepare's SCENE stage).
    lpWorldEntityOutput_PostPhysics->Construct();
    lpTrafficInput_PostPhysics->Construct();
    lpTrafficOutput_PostPhysics->Construct();

    // Drain the world in-event queue into the traffic post-physics action queue.
    // FLAG cross-home cast (the committed bridge precedent): the traffic input's
    // game-action member is an opaque 13328-byte storage stand-in, but it IS the
    // same CgsModule::VariableEventQueue<13312,16> the world input publishes (the
    // X360 calls VariableEventQueue<13312,16>::Append on it directly).
    lpUpdateInputBuffer->LockForRead();
    lpTrafficInput_PostPhysics->LockForWrite();
    reinterpret_cast<BrnWorldIO::GameActionQueue*>(
        lpTrafficInput_PostPhysics->GetGameActionQueue() )->Append(
            *lpUpdateInputBuffer->GetGameActionQueue() );
    lpTrafficInput_PostPhysics->UnlockForWrite();
    lpUpdateInputBuffer->UnlockForRead();

    // The world-entity validation protocol.
    // FLAG cross-home cast: BrnWorldIO::WorldEntityRequestInterface and
    // WorldEntityIO::RequestInterface model the SAME X360 payload (the world
    // module's update input carries the world-entity module's own request
    // interface verbatim); the X360 passes the pointer straight through.
    lpUpdateInputBuffer->LockForRead();
    mWorldEntityModule.ProcessValidationRequests(
        reinterpret_cast<const WorldEntityIO::RequestInterface*>(
            lpUpdateInputBuffer->GetWorldEntityRequestInterface() ) );
    lpUpdateInputBuffer->UnlockForRead();

    lpWorldEntityOutput_PostPhysics->LockForWrite();
    mWorldEntityModule.UpdateCollisionValidation( lpWorldEntityOutput_PostPhysics );
    lpWorldEntityOutput_PostPhysics->UnlockForWrite();

    mTrafficEntityModule.PostPhysicsUpdate( lpInputBufferStack, lpOutputBufferStack,
                                            lpTrafficInput_PostPhysics,
                                            lpTrafficOutput_PostPhysics, lUpdateSet );

    // Flush the world-entity requests + status and the traffic GUI events out.
    lpWorldEntityOutput_PostPhysics->LockForWrite();
    lpTrafficOutput_PostPhysics->LockForWrite();
    lpUpdateOutputBuffer->LockForWrite();

    lpUpdateOutputBuffer->GetResourceRequestResourceInterface()->Append(
        *static_cast<const WorldEntityIO::OutputBuffer_PostPhysics*>(
            lpWorldEntityOutput_PostPhysics )->GetResourceRequestInterface() );
    lpUpdateOutputBuffer->SetWorldEntityStatusInterface(
        static_cast<const WorldEntityIO::OutputBuffer_PostPhysics*>(
            lpWorldEntityOutput_PostPhysics )->GetStatusInterface() );
    // [FLAG PC boot gate] the traffic GUI-event forward
    // (VariableEventQueue<32768,16>::Append of the traffic post-physics output's
    // GUI queue into the update output's GUI queue) is deferred with the traffic
    // module: the traffic OutputBuffer_PostPhysics slice has no GUI-event queue
    // accessor committed yet, and the gated traffic PostPhysicsUpdate stages
    // nothing into it. Restore with the traffic post-physics IO pass.

    lpUpdateOutputBuffer->UnlockForWrite();
    lpTrafficOutput_PostPhysics->UnlockForWrite();
    lpWorldEntityOutput_PostPhysics->UnlockForWrite();

    lpOutputBufferStack->DestroyIOBuffer( &lpTrafficOutput_PostPhysics );
    lpInputBufferStack->DestroyIOBuffer( &lpTrafficInput_PostPhysics );
    lpOutputBufferStack->DestroyIOBuffer( &lpWorldEntityOutput_PostPhysics );
}


// ============================================================================
// Update  @ 0x827D63E8   (DWARF BrnWorldModule.h:358 -- the X360 vtable+76 slot)
//
// The real per-frame world UPDATE spine. Frame shape (X360, reproduced in
// order):
//   1. create the frame IO buffers (physics / scene / trigger / AI / traffic /
//      crash) and carve the frame's triangle-cache collision generator from the
//      per-frame world linear allocator (one 336896-byte Malloc: the generator
//      object + its 0x40000 result buffer);
//   2. debug component + player-vehicle-controls copies;
//   3. input bridges (physics / crash / game actions / race car / entity
//      modules), then the PRE-SCENE spine and the pre-scene output bridges;
//   4. crash pre-scene; the pre-scene -> scene/physics staging;
//   5. physics cached positions; StartUpdateTriangleCache; the scene manager
//      UpdateScene pass; the POST-SCENE spine (in EntityModulePostSceneUpdate);
//      AI update; physics network catch-up; the PRE-PHYSICS spine;
//   6. physics post-scene + scene queries round-trip + physics update + the
//      second scene UpdateScene pass (post-physics);
//   7. the POST-PHYSICS spine (world streamer request flush rides it), the
//      output bridges, the environment manager/map tail and the player-car
//      position/speed latch.
// ============================================================================
void
WorldModule::Update( BrnUpdateSet lUpdateSet,
                     CgsModule::IOBufferStack* lpInputBufferStack,
                     CgsModule::IOBufferStack* lpOutputBufferStack,
                     const BrnWorldIO::UpdateInputBuffer* lpUpdateInputBuffer,
                     BrnWorldIO::UpdateOutputBuffer* lpUpdateOutputBuffer,
                     CgsMemory::LinearMalloc* lpFrameAllocator )
{
    using namespace CgsDev;

    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_World );

    CGS_ASSERT( lpInputBufferStack != 0, "lpInputBufferStack != NULL" );   // X360 cpp:1248
    CGS_ASSERT( lpOutputBufferStack != 0, "lpOutputBufferStack != NULL" ); // X360 cpp:1249

    // ---- the frame IO buffers ---------------------------------------------
    BrnPhysics::PhysicsModuleIO::InputBuffer*  lpPhysicsInput  = 0;
    BrnPhysics::PhysicsModuleIO::OutputBuffer* lpPhysicsOutput = 0;
    CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneOutput = 0;
    TriggerEntityModuleIO::InputBuffer_PreScene*   lpTriggerInput_PreScene  = 0;
    TriggerEntityModuleIO::OutputBuffer_PreScene*  lpTriggerOutput_PreScene = 0;
    TriggerEntityModuleIO::InputBuffer_PostScene*  lpTriggerInput_PostScene  = 0;
    TriggerEntityModuleIO::OutputBuffer_PostScene* lpTriggerOutput_PostScene = 0;
    TriggerEntityModuleIO::InputBuffer_PrePhysics*  lpTriggerInput_PrePhysics  = 0;
    TriggerEntityModuleIO::OutputBuffer_PrePhysics* lpTriggerOutput_PrePhysics = 0;
    BrnAI::AIModuleIO::OutputBuffer* lpAIOutput = 0;
    BrnTraffic::BrnTrafficIO::InputBuffer_PostScene*   lpTrafficInput_PostScene   = 0;
    BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics* lpTrafficInput_PostPhysics = 0;
    CrashIO::InputBuffer_PreScene*     lpCrashInput_PreScene     = 0;
    CrashIO::OutputBuffer_PreScene*    lpCrashOutput_PreScene    = 0;
    CrashIO::InputBuffer_PostPhysics*  lpCrashInput_PostPhysics  = 0;
    CrashIO::OutputBuffer_PostPhysics* lpCrashOutput_PostPhysics = 0;

    lpInputBufferStack->CreateIOBuffer( &lpPhysicsInput, "Physics" );
    lpOutputBufferStack->CreateIOBuffer( &lpPhysicsOutput, "Physics" );
    lpOutputBufferStack->CreateIOBuffer( &lpSceneOutput, "Scene" );
    lpInputBufferStack->CreateIOBuffer( &lpTriggerInput_PreScene, "TriggerPreScene" );
    lpOutputBufferStack->CreateIOBuffer( &lpTriggerOutput_PreScene, "TriggerPreScene" );
    lpInputBufferStack->CreateIOBuffer( &lpTriggerInput_PostScene, "TriggerPostScene" );
    lpOutputBufferStack->CreateIOBuffer( &lpTriggerOutput_PostScene, "TriggerPostScene" );
    lpInputBufferStack->CreateIOBuffer( &lpTriggerInput_PrePhysics, "TriggerPrePhysics" );
    lpOutputBufferStack->CreateIOBuffer( &lpTriggerOutput_PrePhysics, "TriggerPrePhysics" );
    lpOutputBufferStack->CreateIOBuffer( &lpAIOutput, "AI" );
    lpInputBufferStack->CreateIOBuffer( &lpTrafficInput_PostScene, "TrafficPostScene" );
    lpInputBufferStack->CreateIOBuffer( &lpTrafficInput_PostPhysics, "TrafficPostPhysics" );
    lpInputBufferStack->CreateIOBuffer( &lpCrashInput_PreScene, "CrashPreScene" );
    lpOutputBufferStack->CreateIOBuffer( &lpCrashOutput_PreScene, "CrashPreScene" );
    lpInputBufferStack->CreateIOBuffer( &lpCrashInput_PostPhysics, "CrashPostPhysics" );
    lpOutputBufferStack->CreateIOBuffer( &lpCrashOutput_PostPhysics, "CrashPostPhysics" );
    // PC Construct restoration (see WorldModule::Prepare's SCENE stage; the X360
    // CreateIOBuffer<T> stack template runs T::Construct after the alloc).
    lpPhysicsInput->Construct();
    // ⛔ 2026-08-10 (root-cause wave): the OUTPUT half was never Constructed. Its console
    // Construct is X360 0x825ABB10 and it is what leaves the vehicle-output REQUEST
    // interface's queues live; without it PhysicsModule::Update's BridgeVehicleManagerToOutput
    // appended into an unconstructed VariableEventQueue<13440,16> every frame.
    lpPhysicsOutput->Construct();
    lpPhysicsOutput->Construct();
    lpSceneOutput->Construct();
    lpTriggerInput_PreScene->Construct();
    lpTriggerOutput_PreScene->Construct();
    lpTriggerInput_PostScene->Construct();
    lpTriggerOutput_PostScene->Construct();
    lpTriggerInput_PrePhysics->Construct();
    lpTriggerOutput_PrePhysics->Construct();
    lpAIOutput->Construct();
    lpTrafficInput_PostScene->Construct();
    lpTrafficInput_PostPhysics->Construct();
    lpCrashInput_PreScene->Construct();
    lpCrashOutput_PreScene->Construct();
    lpCrashInput_PostPhysics->Construct();
    lpCrashOutput_PostPhysics->Construct();

    // ---- the frame's triangle-cache collision generator --------------------
    // ONE carve from the per-frame world allocator: the generator object at the
    // base + its 0x40000-byte collision result region. The console literal is
    // 336896 == 74752 + 0x40000, and 74752 == 0x12400 is precisely the X360
    // sizeof(BaseCollisionGenerator).
    // ⚠️⚠️ FIXED 2026-08-10 (cache-fill wave): the two byte literals are now a
    // `sizeof`. THIS OBJECT IS CARVED AT RUNTIME, NOT DESERIALISED, so on x64 every
    // one of its pointers widens (64 embedded CollisionBatch, each holding an
    // EA::Jobs::Job, plus a 200-entry pointer array) and it is materially LARGER than
    // 74752 bytes. Until this wave the generator was never Construct()ed or Prepare()d
    // (both were WorldLinkStubs gates), so nothing had ever written past +74752 and the
    // console offset was harmless; mounting the real Prepare -- which placement-
    // constructs all 64 batches -- would have walked straight off the end of the object
    // and into the result region it is about to hand the bump allocator.
    // (Standing rule: console size literals become `sizeof`, and a runtime-carved
    // struct's console byte offsets must never be pinned on the host.)
    const size_t lnCollisionGeneratorBytes =
        sizeof( CgsSceneManager::CgsCollision::BaseCollisionGenerator );
    void* lpCollisionGeneratorMemory =
        lpFrameAllocator->Malloc( lnCollisionGeneratorBytes + 0x40000 );
    CgsSceneManager::CgsCollision::BaseCollisionGenerator* lpCollisionGenerator =
        static_cast<CgsSceneManager::CgsCollision::BaseCollisionGenerator*>(
            lpCollisionGeneratorMemory );
    lpCollisionGenerator->Construct();

    // ---- pre-scene output + pre-physics input buffers ----------------------
    RaceCarEntityModuleIO::OutputBuffer_PreScene*   lpRaceCarOutput_PreScene = 0;
    BrnTraffic::BrnTrafficIO::OutputBuffer_PreScene* lpTrafficOutput_PreScene = 0;
    PropEntityIO::OutputBuffer_PreScene*            lpPropOutput_PreScene    = 0;
    WorldEntityIO::OutputBuffer_PreScene*           lpWorldEntityOutput_PreScene = 0;
    RaceCarEntityModuleIO::InputBuffer_PrePhysics*  lpRaceCarInput_PrePhysics = 0;
    BrnTraffic::BrnTrafficIO::InputBuffer_PrePhysics* lpTrafficInput_PrePhysics = 0;
    PropEntityIO::InputBuffer_PrePhysics*           lpPropInput_PrePhysics   = 0;
    CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInput_Update = 0;

    lpOutputBufferStack->CreateIOBuffer( &lpRaceCarOutput_PreScene, "RaceCarPreScene" );
    lpOutputBufferStack->CreateIOBuffer( &lpTrafficOutput_PreScene, "TrafficPreScene" );
    lpOutputBufferStack->CreateIOBuffer( &lpPropOutput_PreScene, "PropPreScene" );
    lpOutputBufferStack->CreateIOBuffer( &lpWorldEntityOutput_PreScene, "WorldEntityPreScene" );
    lpInputBufferStack->CreateIOBuffer( &lpRaceCarInput_PrePhysics, "RaceCarPrePhysics" );
    lpInputBufferStack->CreateIOBuffer( &lpTrafficInput_PrePhysics, "TrafficPrePhysics" );
    lpInputBufferStack->CreateIOBuffer( &lpPropInput_PrePhysics, "PropPrePhysics" );
    lpInputBufferStack->CreateIOBuffer( &lpSceneInput_Update, "SceneInput_Update" );
    lpRaceCarOutput_PreScene->Construct();
    lpTrafficOutput_PreScene->Construct();
    lpPropOutput_PreScene->Construct();
    lpWorldEntityOutput_PreScene->Construct();
    lpRaceCarInput_PrePhysics->Construct();
    lpTrafficInput_PrePhysics->Construct();
    lpPropInput_PrePhysics->Construct();
    lpSceneInput_Update->Construct();

    lpUpdateInputBuffer->LockForRead();

    // ---- debug component (excluded from the frame totals) -------------------
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_World );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_TotalUpdate );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_EachUpdate );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_DebugManager );

    mDebugComponent.Update( lpUpdateInputBuffer->GetDebugController() );
    lpUpdateOutputBuffer->LockForWrite();
    lpUpdateOutputBuffer->SetWorldWantsDebugControllerFocus(
        mDebugComponent.GetWantsDebugControllerFocus() );
    lpUpdateOutputBuffer->UnlockForWrite();

    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_DebugManager );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_EachUpdate );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_TotalUpdate );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_World );

    // ---- player vehicle controls copy ---------------------------------------
    // [FLAG PC boot gate] UpdateInputBuffer::GetPlayerVehicleControls is still the
    // WorldLinkStubs read-lock stub (its producing module is boot-gated) and returns
    // NULL; the faithful setter is an unguarded 60-byte memcpy, so skip the copy
    // while the source is absent. Delete the guard when the real accessor lands.
    {
        const BrnWorldIO::PlayerVehicleControls* lpControls =
            lpUpdateInputBuffer->GetPlayerVehicleControls();
        if ( lpControls != 0 )
        {
            lpUpdateOutputBuffer->LockForWrite();
            lpUpdateOutputBuffer->SetPlayerVehicleControls( lpControls );
            lpUpdateOutputBuffer->UnlockForWrite();
        }
    }

    // ---- input bridges ------------------------------------------------------
    CGS_ASSERT( lpPhysicsInput != 0, "lpInputBuffer" );   // X360 CgsModuleUtils.h:238
    lpPhysicsInput->LockForWrite();
    ::WorldModule::BridgeInputToPhysicsModule( this, lpPhysicsInput, lpUpdateInputBuffer );
    lpPhysicsInput->UnlockForWrite();

    CGS_ASSERT( lpCrashInput_PreScene != 0, "lpInputBuffer" );
    lpCrashInput_PreScene->LockForWrite();
    ::WorldModule::BridgeInputToCrashModule( this, lpCrashInput_PreScene, lpUpdateInputBuffer );
    lpCrashInput_PreScene->UnlockForWrite();

    HandleGameActions( lpPhysicsInput, lpTrafficInput_PostPhysics, 0, 0, lpUpdateInputBuffer );

    // ---- pre-scene inputs + the input fan-out -------------------------------
    RaceCarEntityModuleIO::InputBuffer_PreScene*   lpRaceCarInput_PreScene = 0;
    BrnTraffic::BrnTrafficIO::InputBuffer_PreScene* lpTrafficInput_PreScene = 0;
    PropEntityIO::InputBuffer_PreScene*            lpPropInput_PreScene    = 0;
    WorldEntityIO::InputBuffer_PreScene*           lpWorldEntityInput_PreScene = 0;
    lpInputBufferStack->CreateIOBuffer( &lpRaceCarInput_PreScene, "RaceCarPreScene" );
    lpInputBufferStack->CreateIOBuffer( &lpTrafficInput_PreScene, "TrafficPreScene" );
    lpInputBufferStack->CreateIOBuffer( &lpPropInput_PreScene, "PropPreScene" );
    lpInputBufferStack->CreateIOBuffer( &lpWorldEntityInput_PreScene, "WorldEntityPreScene" );
    lpRaceCarInput_PreScene->Construct();
    lpTrafficInput_PreScene->Construct();
    lpPropInput_PreScene->Construct();
    lpWorldEntityInput_PreScene->Construct();

    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_RaceCar_Bridge );
    CGS_ASSERT( lpRaceCarInput_PreScene != 0, "lpInputBuffer" );
    lpRaceCarInput_PreScene->LockForWrite();
    ::WorldModule::BridgeActionsToRaceCarModule( this, lpRaceCarInput_PreScene,
                                                 lpUpdateInputBuffer );
    lpRaceCarInput_PreScene->UnlockForWrite();
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_RaceCar_Bridge );

    lpRaceCarInput_PreScene->LockForWrite();
    lpRaceCarInput_PrePhysics->LockForWrite();
    lpTrafficInput_PreScene->LockForWrite();
    lpTriggerInput_PreScene->LockForWrite();
    lpTriggerInput_PostScene->LockForWrite();
    lpWorldEntityInput_PreScene->LockForWrite();
    lpPropInput_PreScene->LockForWrite();
    ::WorldModule::BridgeInputToEntityModules(
        this, lpTriggerInput_PreScene, lpTriggerInput_PostScene, lpTrafficInput_PreScene,
        lpRaceCarInput_PreScene, lpRaceCarInput_PrePhysics, lpWorldEntityInput_PreScene,
        lpPropInput_PreScene, lpUpdateInputBuffer );
    lpPropInput_PreScene->UnlockForWrite();
    lpWorldEntityInput_PreScene->UnlockForWrite();
    lpTriggerInput_PostScene->UnlockForWrite();
    lpTriggerInput_PreScene->UnlockForWrite();
    lpTrafficInput_PreScene->UnlockForWrite();
    lpRaceCarInput_PrePhysics->UnlockForWrite();
    lpRaceCarInput_PreScene->UnlockForWrite();

    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_World );

    // ---- PRE-SCENE spine ----------------------------------------------------
    EntityModulePreSceneUpdate(
        lpInputBufferStack, lpOutputBufferStack,
        lpTriggerInput_PreScene, lpTriggerOutput_PreScene,
        lpTrafficInput_PreScene, lpTrafficOutput_PreScene,
        lpTrafficInput_PostScene, lpTrafficInput_PostPhysics,
        lpRaceCarInput_PreScene, lpRaceCarOutput_PreScene,
        lpPropInput_PreScene, lpPropOutput_PreScene,
        lpWorldEntityInput_PreScene, lpWorldEntityOutput_PreScene,
        lUpdateSet );

    // ---- pre-scene output bridges -------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_World );

    CgsModule::LockBuffersForIO( lpUpdateOutputBuffer, lpPropOutput_PreScene );
    ::WorldModule::BridgePropToOutput_PreScene( this, lpUpdateOutputBuffer,
                                                lpPropOutput_PreScene );
    CgsModule::UnlockBuffersForIO( lpUpdateOutputBuffer, lpPropOutput_PreScene );

    CgsModule::LockBuffersForIO( lpUpdateOutputBuffer, lpWorldEntityOutput_PreScene,
                                 lpRaceCarOutput_PreScene );
    ::WorldModule::BridgeWorldEntityInfoToOutput( this, lpUpdateOutputBuffer,
                                                  lpWorldEntityOutput_PreScene );
    ::WorldModule::BridgeRaceCarEntityInfoToOutput_PreScene( this, lpUpdateOutputBuffer,
                                                             lpRaceCarOutput_PreScene );
    ::WorldModule::BridgeTrafficEntityInfoToOutput_PreScene( this, lpUpdateOutputBuffer,
                                                             lpTrafficOutput_PreScene );
    CgsModule::UnlockBuffersForIO( lpUpdateOutputBuffer, lpWorldEntityOutput_PreScene,
                                   lpRaceCarOutput_PreScene );

    // Snapshot the traffic -> race-car pre-scene interface before its buffer dies
    // (the X360 memcpy's the 544-byte block to the stack for the post-scene spine).
    // FLAG cross-home: the buffer-nested and class-level spellings of
    // TrafficToRaceCarInterface_PreScene model the SAME 544-byte payload; the
    // snapshot is taken in the nested form and handed to the spine (which names
    // the class-level one) through the documented adapter cast.
    BrnTraffic::BrnTrafficIO::OutputBuffer_PreScene::TrafficToRaceCarInterface_PreScene
        lTrafficToRaceCar_PreScene;
    lpTrafficOutput_PreScene->LockForRead();
    {
        // [FLAG PC boot gate] same seam as the controls copy above: the traffic
        // pre-scene accessor is still the WorldLinkStubs null return, and the
        // snapshot is a 544-byte structure copy. Skip it while the producer is gated.
        const BrnTraffic::BrnTrafficIO::OutputBuffer_PreScene::TrafficToRaceCarInterface_PreScene*
            lpTrafficToRaceCar = lpTrafficOutput_PreScene->GetTrafficToRaceCarInterface_PreScene();
        if ( lpTrafficToRaceCar != 0 )
            lTrafficToRaceCar_PreScene = *lpTrafficToRaceCar;
    }
    lpTrafficOutput_PreScene->UnlockForRead();

    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_World );

    lpInputBufferStack->DestroyIOBuffer( &lpWorldEntityInput_PreScene );
    lpInputBufferStack->DestroyIOBuffer( &lpPropInput_PreScene );
    lpInputBufferStack->DestroyIOBuffer( &lpTrafficInput_PreScene );
    lpInputBufferStack->DestroyIOBuffer( &lpRaceCarInput_PreScene );

    // ---- crash pre-scene ----------------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_World );
    PerfMonCpu::StartMonitor( miCrashModuleUpdatePM );

    CgsModule::LockBuffersForIO( lpCrashInput_PreScene, lpRaceCarOutput_PreScene );
    ::WorldModule::BridgeEntityModulesToCrashModule_PreScene(
        this, lpCrashInput_PreScene, lpRaceCarOutput_PreScene );
    CgsModule::UnlockBuffersForIO( lpCrashInput_PreScene, lpRaceCarOutput_PreScene );

    mCrashModule.PreSceneUpdate( lpInputBufferStack, lpOutputBufferStack,
                                 lpCrashInput_PreScene, lpCrashOutput_PreScene, lUpdateSet );

    PerfMonCpu::StopMonitor( miCrashModuleUpdatePM );

    // ---- pre-scene -> scene / physics staging -------------------------------
    CgsModule::LockBuffersForIO( lpSceneInput_Update, lpTriggerOutput_PreScene,
                                 lpTrafficOutput_PreScene, lpRaceCarOutput_PreScene,
                                 lpPropOutput_PreScene, lpWorldEntityOutput_PreScene );
    ::WorldModule::BridgeEntityModulesToSceneModule_PreScene(
        this, lpSceneInput_Update, lpTriggerOutput_PreScene, lpTrafficOutput_PreScene,
        lpRaceCarOutput_PreScene, lpPropOutput_PreScene, lpWorldEntityOutput_PreScene );
    CgsModule::UnlockBuffersForIO( lpSceneInput_Update, lpTriggerOutput_PreScene,
                                   lpTrafficOutput_PreScene, lpRaceCarOutput_PreScene,
                                   lpPropOutput_PreScene, lpWorldEntityOutput_PreScene );

    CgsModule::LockBuffersForIO( lpPhysicsInput, lpRaceCarOutput_PreScene,
                                 lpPropOutput_PreScene );
    ::WorldModule::BridgeEntityModulesToPhysicsModule_PreScene(
        this, lpPhysicsInput, lpRaceCarOutput_PreScene, lpPropOutput_PreScene );
    CgsModule::UnlockBuffersForIO( lpPhysicsInput, lpRaceCarOutput_PreScene,
                                   lpPropOutput_PreScene );

    lpOutputBufferStack->DestroyIOBuffer( &lpWorldEntityOutput_PreScene );
    lpOutputBufferStack->DestroyIOBuffer( &lpPropOutput_PreScene );
    lpOutputBufferStack->DestroyIOBuffer( &lpTrafficOutput_PreScene );

    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_World );

    // ---- physics cached positions -------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Physics );
    lpSceneInput_Update->LockForWrite();
    mPhysicsModule.UpdateCachedPositions( lpSceneInput_Update );
    lpSceneInput_Update->UnlockForWrite();
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Physics );

    // ---- triangle cache + the first scene UpdateScene pass ------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_World );
    PerfMonCpu::StartMonitor( miSceneModuleUpdateContactsPM );

    lpCollisionGenerator->Prepare(
        static_cast<u8*>( lpCollisionGeneratorMemory ) + lnCollisionGeneratorBytes, 0x40000 );
    mSceneModule.StartUpdateTriangleCache( lpInputBufferStack, lpOutputBufferStack,
                                           lpSceneInput_Update, lpCollisionGenerator );

    PerfMonCpu::StopMonitor( miSceneModuleUpdateContactsPM );

    PerfMonCpu::StartMonitor( miSceneManagerUpdatePM );
    // X360 (*(vtbl(mSceneModule) + 64)) == UpdateScene; devirtualised.
    mSceneModule.UpdateScene( lpInputBufferStack, lpOutputBufferStack,
                              lpSceneInput_Update, lpSceneOutput, false );
    lpSceneInput_Update->LockForWrite();
    lpSceneInput_Update->GetInSceneUpdateInterface()->Clear();
    lpSceneInput_Update->UnlockForWrite();
    PerfMonCpu::StopMonitor( miSceneManagerUpdatePM );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_World );

    // ---- contact generation --------------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Physics );
    PerfMonCpu::StartMonitor( miPhysicsSummaryPM );
    PerfMonCpu::StartMonitor( miSceneModuleUpdateContactsPM );
    if ( ( lUpdateSet & 1 ) == 0 )
    {
        // X360 (*(vtbl(mSceneModule) + 72)) == UpdateContactGeneration; devirtualised.
        mSceneModule.UpdateContactGeneration( lpInputBufferStack, lpOutputBufferStack,
                                              lpSceneInput_Update, lpSceneOutput );
    }
    mSceneModule.EndUpdateTriangleCache( lpInputBufferStack, lpOutputBufferStack );
    PerfMonCpu::StopMonitor( miSceneModuleUpdateContactsPM );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Physics );
    PerfMonCpu::StopMonitor( miPhysicsSummaryPM );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_World );

    lpInputBufferStack->DestroyIOBuffer( &lpSceneInput_Update );

    // ---- post-scene buffers --------------------------------------------------
    // [FLAG PC boot gate] the WORLD-ENTITY post-scene IO pair: the X360 creates
    // WorldEntityIO::InputBuffer_PostScene / OutputBuffer_PostScene around the
    // post-scene spine and destroys them straight after -- the spine itself never
    // reads or writes them (they are not in its parameter list). Neither type is
    // committed in BrnWorldEntityModuleIO.h yet, so the pair is omitted here; the
    // observable frame is unchanged. Restore both with the world-entity post-scene
    // IO pass.
    BrnTraffic::BrnTrafficIO::OutputBuffer_PostScene* lpTrafficOutput_PostScene = 0;
    RaceCarEntityModuleIO::OutputBuffer_PostScene*    lpRaceCarOutput_PostScene = 0;
    PropEntityIO::OutputBuffer_PostScene*             lpPropOutput_PostScene    = 0;
    lpOutputBufferStack->CreateIOBuffer( &lpTrafficOutput_PostScene, "TrafficPostScene" );
    lpOutputBufferStack->CreateIOBuffer( &lpRaceCarOutput_PostScene, "RaceCarPostScene" );
    lpOutputBufferStack->CreateIOBuffer( &lpPropOutput_PostScene, "PropPostScene" );
    lpTrafficOutput_PostScene->Construct();
    lpRaceCarOutput_PostScene->Construct();
    lpPropOutput_PostScene->Construct();
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_World );

    RaceCarEntityModuleIO::InputBuffer_PostScene* lpRaceCarInput_PostScene = 0;
    PropEntityIO::InputBuffer_PostScene*          lpPropInput_PostScene    = 0;
    lpInputBufferStack->CreateIOBuffer( &lpRaceCarInput_PostScene, "RaceCarPostScene" );
    lpInputBufferStack->CreateIOBuffer( &lpPropInput_PostScene, "PropPostScene" );
    lpRaceCarInput_PostScene->Construct();
    lpPropInput_PostScene->Construct();

    // ---- POST-SCENE spine ----------------------------------------------------
    // (The world-entity post-scene pair is created/destroyed around the call per
    // the X360 frame; the reviewed spine signature omits the pair -- the X360
    // callee never touches it. The crash argument is the crash PRE-SCENE output;
    // the committed spine still carries the minimal-slice CrashModuleIO::
    // OutputBuffer_PostScene type [FLAG: type reconcile with the crash IO TU --
    // the DWARF spells OutputBuffer_PreScene], hence the cast.)
    EntityModulePostSceneUpdate(
        lpInputBufferStack, lpOutputBufferStack,
        lpTriggerInput_PrePhysics, lpTriggerInput_PostScene,
        lpTrafficInput_PostScene,
        reinterpret_cast<const BrnTraffic::BrnTrafficIO::TrafficToRaceCarInterface_PreScene*>(
            &lTrafficToRaceCar_PreScene ),
        lpTrafficOutput_PostScene, lpTrafficInput_PostPhysics, lpTrafficInput_PrePhysics,
        lpRaceCarInput_PostScene, lpRaceCarOutput_PostScene, lpRaceCarInput_PrePhysics,
        reinterpret_cast<const CrashModuleIO::OutputBuffer_PostScene*>( lpCrashOutput_PreScene ),
        lpPropInput_PostScene, lpPropOutput_PostScene,
        lUpdateSet );
    lpInputBufferStack->DestroyIOBuffer( &lpPropInput_PostScene );
    lpInputBufferStack->DestroyIOBuffer( &lpRaceCarInput_PostScene );

    // ---- AI update -----------------------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_NetworkAIRaceCar );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_AI );

    BrnAI::AIModuleIO::InputBuffer* lpAIInput = 0;
    lpInputBufferStack->CreateIOBuffer( &lpAIInput, "AIInput" );
    lpAIInput->Construct();

    CgsModule::LockBuffersForIO( lpAIInput, lpTrafficOutput_PostScene,
                                 lpRaceCarOutput_PostScene, lpSceneOutput,
                                 lpRaceCarOutput_PreScene );
    ::WorldModule::BridgeInputToAIModule( this, lpAIInput, lpUpdateInputBuffer );
    ::WorldModule::BridgeTrafficModuleToAIModule_Update( this, lpAIInput,
                                                         lpTrafficOutput_PostScene );
    ::WorldModule::BridgeRaceCarModuleToAIModule_PostScene( this, lpAIInput,
                                                            lpRaceCarOutput_PostScene );
    ::WorldModule::BridgeRaceCarModuleToAIModule_PreScene( this, lpAIInput,
                                                           lpRaceCarOutput_PreScene );
    CgsModule::UnlockBuffersForIO( lpAIInput, lpTrafficOutput_PostScene,
                                   lpRaceCarOutput_PostScene, lpSceneOutput,
                                   lpRaceCarOutput_PreScene );

    // The player-car-under-AI-control latch + the AI camera copy: the X360 pokes
    // both straight into mAIModule's interior (this+0x5DFD00 camera <-
    // mLastCameraInput; this+0x5DFE82 byte <- maeCarControls[player] == 2).
    // [FLAG PC boot gate] the AI module's committed slice models that interior as
    // opaque padding, so the two stores have no named home yet; the AI update
    // below is gated inert, so the observable is unchanged. Restore both with
    // the AI module TU (add SetPlayerUnderAIControl + the camera member).

    PerfMonCpu::StartMonitor( miAIModuleUpdatePM );
    // X360 (*(vtbl(mAIModule) + 68)) == Update; devirtualised.
    mAIModule.Update( lpInputBufferStack, lpOutputBufferStack, lpAIInput, lpAIOutput,
                      lUpdateSet );
    PerfMonCpu::StopMonitor( miAIModuleUpdatePM );

    lpInputBufferStack->DestroyIOBuffer( &lpAIInput );

    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_AI );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_NetworkAIRaceCar );

    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_World );
    lpOutputBufferStack->DestroyIOBuffer( &lpPropOutput_PostScene );
    lpOutputBufferStack->DestroyIOBuffer( &lpRaceCarOutput_PostScene );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_World );

    // ---- physics network catch-up -------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Physics );
    PerfMonCpu::StartMonitor( miPhysicsSummaryPM );
    PerfMonCpu::StartMonitor( miPhysicsNetworkCatchupPM );
    UpdatePhysicsNetworkCatchup( lpInputBufferStack, lpOutputBufferStack,
                                 lpPhysicsInput, lpPhysicsOutput, lUpdateSet );
    PerfMonCpu::StopMonitor( miPhysicsNetworkCatchupPM );
    PerfMonCpu::StopMonitor( miPhysicsSummaryPM );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Physics );

    // ---- pre-physics buffers + AI staging -----------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_World );
    RaceCarEntityModuleIO::OutputBuffer_PrePhysics*   lpRaceCarOutput_PrePhysics = 0;
    BrnTraffic::BrnTrafficIO::OutputBuffer_PrePhysics* lpTrafficOutput_PrePhysics = 0;
    PropEntityIO::OutputBuffer_PrePhysics*            lpPropOutput_PrePhysics    = 0;
    WorldEntityIO::InputBuffer_PostPhysics*           lpWorldEntityInput_PrePhysics = 0;
    WorldEntityIO::OutputBuffer_PostPhysics*          lpWorldEntityOutput_PrePhysics = 0;
    lpOutputBufferStack->CreateIOBuffer( &lpRaceCarOutput_PrePhysics, "RaceCarPrePhysics" );
    lpOutputBufferStack->CreateIOBuffer( &lpTrafficOutput_PrePhysics, "TrafficPrePhysics" );
    lpOutputBufferStack->CreateIOBuffer( &lpPropOutput_PrePhysics, "PropPrePhysics" );
    lpInputBufferStack->CreateIOBuffer( &lpWorldEntityInput_PrePhysics, "WorldEntityPrePhysics" );
    lpOutputBufferStack->CreateIOBuffer( &lpWorldEntityOutput_PrePhysics, "WorldEntityPrePhysics" );
    lpRaceCarOutput_PrePhysics->Construct();
    lpTrafficOutput_PrePhysics->Construct();
    lpPropOutput_PrePhysics->Construct();
    lpWorldEntityInput_PrePhysics->Construct();
    lpWorldEntityOutput_PrePhysics->Construct();

    lpRaceCarInput_PrePhysics->LockForWrite();
    lpPropInput_PrePhysics->LockForWrite();
    lpAIOutput->LockForRead();
    ::WorldModule::BridgeAIToEntityModules_PrePhysics(
        this, lpRaceCarInput_PrePhysics, lpPropInput_PrePhysics, lpAIOutput );
    lpAIOutput->UnlockForRead();
    lpPropInput_PrePhysics->UnlockForWrite();
    lpRaceCarInput_PrePhysics->UnlockForWrite();
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_World );

    // ---- PRE-PHYSICS spine ---------------------------------------------------
    EntityModulePrePhysicsUpdate(
        lpInputBufferStack, lpOutputBufferStack,
        lpTriggerInput_PrePhysics, lpTriggerOutput_PrePhysics,
        lpSceneOutput,
        lpTrafficInput_PrePhysics, lpTrafficOutput_PrePhysics,
        lpTrafficOutput_PostScene,
        lpRaceCarInput_PrePhysics, lpRaceCarOutput_PrePhysics,
        lpPropInput_PrePhysics, lpPropOutput_PrePhysics,
        lpWorldEntityInput_PrePhysics, lpWorldEntityOutput_PrePhysics,
        lUpdateSet );

    // ---- pre-physics -> physics / output staging -----------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_World );
    CgsModule::LockBuffersForIO( lpPhysicsInput, lpTrafficOutput_PrePhysics,
                                 lpRaceCarOutput_PrePhysics, lpPropOutput_PrePhysics );
    ::WorldModule::BridgeEntityModulesToPhysicsModule_PrePhysics(
        this, lpPhysicsInput, lpTrafficOutput_PrePhysics, lpRaceCarOutput_PrePhysics,
        lpPropOutput_PrePhysics );
    CgsModule::UnlockBuffersForIO( lpPhysicsInput, lpTrafficOutput_PrePhysics,
                                   lpRaceCarOutput_PrePhysics, lpPropOutput_PrePhysics );

    CgsModule::LockBuffersForIO( lpUpdateOutputBuffer, lpRaceCarOutput_PrePhysics,
                                 lpTrafficOutput_PrePhysics, lpTriggerOutput_PrePhysics );
    ::WorldModule::BridgeEntityModulesToOutput_PrePhysics(
        this, lpUpdateOutputBuffer, lpRaceCarOutput_PrePhysics, lpTrafficOutput_PrePhysics,
        lpTriggerOutput_PrePhysics );
    CgsModule::UnlockBuffersForIO( lpUpdateOutputBuffer, lpRaceCarOutput_PrePhysics,
                                   lpTrafficOutput_PrePhysics, lpTriggerOutput_PrePhysics );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_World );

    lpOutputBufferStack->DestroyIOBuffer( &lpWorldEntityOutput_PrePhysics );
    lpInputBufferStack->DestroyIOBuffer( &lpWorldEntityInput_PrePhysics );
    lpOutputBufferStack->DestroyIOBuffer( &lpPropOutput_PrePhysics );
    lpOutputBufferStack->DestroyIOBuffer( &lpTrafficOutput_PrePhysics );
    lpOutputBufferStack->DestroyIOBuffer( &lpRaceCarOutput_PrePhysics );

    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_World );
    lpOutputBufferStack->DestroyIOBuffer( &lpTrafficOutput_PostScene );
    lpOutputBufferStack->DestroyIOBuffer( &lpRaceCarOutput_PreScene );
    lpInputBufferStack->DestroyIOBuffer( &lpPropInput_PrePhysics );
    lpInputBufferStack->DestroyIOBuffer( &lpTrafficInput_PrePhysics );
    lpInputBufferStack->DestroyIOBuffer( &lpRaceCarInput_PrePhysics );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_World );

    // ---- physics post-scene + scene queries + physics update -----------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Physics );
    PerfMonCpu::StartMonitor( miPhysicsSummaryPM );
    PerfMonCpu::StartMonitor( miPhysicsBridgesPM );
    CgsModule::LockBuffersForIO( lpPhysicsInput, lpAIOutput, lpCrashOutput_PreScene );
    ::WorldModule::BridgeAIModuleToPhysicsModule( this, lpPhysicsInput, lpAIOutput );
    ::WorldModule::BridgeCrashModuleToPhysicsModule( this, lpPhysicsInput,
                                                     lpCrashOutput_PreScene );
    CgsModule::UnlockBuffersForIO( lpPhysicsInput, lpAIOutput, lpCrashOutput_PreScene );
    PerfMonCpu::StopMonitor( miPhysicsBridgesPM );

    PerfMonCpu::StartMonitor( miPhysicsModulePreSceneUpdatePM );
    mPhysicsModule.PostSceneUpdate( lpInputBufferStack, lpOutputBufferStack,
                                    lpPhysicsInput, lpPhysicsOutput, lUpdateSet );
    PerfMonCpu::StopMonitor( miPhysicsModulePreSceneUpdatePM );

    CgsSceneManager::SceneManagerIO::InputBuffer_Query* lpSceneInput_PhysicsQueries = 0;
    lpInputBufferStack->CreateIOBuffer( &lpSceneInput_PhysicsQueries, "SceneInput_PhysicsQueries" );
    lpSceneInput_PhysicsQueries->Construct();

    PerfMonCpu::StartMonitor( miPhysicsModuleGenerateSceneQueriesPM );
    mPhysicsModule.GenerateSceneQueries( lpPhysicsOutput, lUpdateSet );
    PerfMonCpu::StopMonitor( miPhysicsModuleGenerateSceneQueriesPM );

    PerfMonCpu::StartMonitor( miPhysicsBridgesPM );
    CgsModule::LockBuffersForIO( lpSceneInput_PhysicsQueries, lpPhysicsOutput );
    ::WorldModule::BridgePhysicsSceneQueriesToScene( this, lpSceneInput_PhysicsQueries,
                                                     lpPhysicsOutput );
    CgsModule::UnlockBuffersForIO( lpSceneInput_PhysicsQueries, lpPhysicsOutput );
    PerfMonCpu::StopMonitor( miPhysicsBridgesPM );

    PerfMonCpu::StartMonitor( miSceneManagerQueryPM );
    // X360 (*(vtbl(mSceneModule) + 68)) == ProcessSceneQueries; devirtualised.
    mSceneModule.ProcessSceneQueries( lpInputBufferStack, lpOutputBufferStack,
                                      lpSceneInput_PhysicsQueries, lpSceneOutput );
    PerfMonCpu::StopMonitor( miSceneManagerQueryPM );

    PerfMonCpu::StartMonitor( miPhysicsBridgesPM );
    CgsModule::LockBuffersForIO( lpPhysicsInput, lpSceneOutput );
    ::WorldModule::BridgeSceneQueryResultsToPhysics( this, lpPhysicsInput, lpSceneOutput );
    ::WorldModule::BridgeSceneModuleToOutput( this, lpUpdateOutputBuffer, lpSceneOutput );
    ::WorldModule::BridgeScenePotentialContactsToPhysics( this, lpPhysicsInput,
                                                          lpSceneOutput );
    CgsModule::UnlockBuffersForIO( lpPhysicsInput, lpSceneOutput );
    PerfMonCpu::StopMonitor( miPhysicsBridgesPM );

    lpInputBufferStack->DestroyIOBuffer( &lpSceneInput_PhysicsQueries );

    PerfMonCpu::StartMonitor( miPhysicsModuleUpdatePM );
    mPhysicsModule.Update( lpInputBufferStack, lpOutputBufferStack, lpPhysicsInput,
                           lpPhysicsOutput, lUpdateSet );
    PerfMonCpu::StopMonitor( miPhysicsModuleUpdatePM );

    PerfMonCpu::StartMonitor( miPhysicsBridgesPM );
    lpInputBufferStack->CreateIOBuffer( &lpSceneInput_Update, "SceneInput_Update" );
    lpSceneInput_Update->Construct();
    CgsModule::LockBuffersForIO( lpSceneInput_Update, lpPhysicsOutput );
    ::WorldModule::BridgePhysicsSceneUpdateToScene( this, lpSceneInput_Update,
                                                    lpPhysicsOutput );
    CgsModule::UnlockBuffersForIO( lpSceneInput_Update, lpPhysicsOutput );
    PerfMonCpu::StopMonitor( miPhysicsBridgesPM );
    PerfMonCpu::StopMonitor( miPhysicsSummaryPM );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Physics );

    // ---- environment tail ----------------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_World );

    // X360: (*vtbl(mSkyDebugComponent))[0] -- the sky debug component's per-frame
    // virtual Update (the environment-settings tuning page).
    mSkyDebugComponent.Update();

    // [FLAG PC boot gate] the environment time-of-day override + frame-delta
    // staging (X360: when the director camera's override byte [camera+289] is
    // set, mEnvironmentManager's time-of-day <- camera float [camera+256] * 3600;
    // then env frame delta [env+4548] <- timer scale * timer delta from the
    // world input's TimerStatusInterface). The committed Director camera slice
    // has no named home for the two override fields and the committed
    // EnvironmentManager slice none for the frame delta; the env Update below is
    // gated inert, so the staging is unobservable. Restore with those TUs.

    mEnvironmentManager.Update( mfLocalPlayerActiveRaceCarSpeed, lpUpdateOutputBuffer,
                                mLastCameraInput.GetPosition() );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_World );

    // ---- AI post-physics -----------------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_NetworkAIRaceCar );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_AI );

    BrnAI::AIModuleIO::InputBuffer_PostPhysics* lpAIInput_PostPhysics = 0;
    lpInputBufferStack->CreateIOBuffer( &lpAIInput_PostPhysics, "AIInputPostPhysics" );
    lpAIInput_PostPhysics->Construct();

    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_AI_Bridge );
    CgsModule::LockBuffersForIO( lpAIInput_PostPhysics, lpPhysicsOutput );
    ::WorldModule::BridgePhysicsModuleToAIModule_PostPhysics(
        this, lpAIInput_PostPhysics, lpPhysicsOutput );
    CgsModule::UnlockBuffersForIO( lpAIInput_PostPhysics, lpPhysicsOutput );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_AI_Bridge );

    PerfMonCpu::StartMonitor( miAIModuleUpdatePM );
    mAIModule.PostPhysicsUpdate( lpAIInput_PostPhysics );
    PerfMonCpu::StopMonitor( miAIModuleUpdatePM );

    lpInputBufferStack->DestroyIOBuffer( &lpAIInput_PostPhysics );

    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_AI );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_NetworkAIRaceCar );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_World );

    // ---- post-physics buffers ------------------------------------------------
    RaceCarEntityModuleIO::OutputBuffer_PostPhysics*    lpRaceCarOutput_PostPhysics = 0;
    BrnTraffic::BrnTrafficIO::OutputBuffer_PostPhysics* lpTrafficOutput_PostPhysics = 0;
    PropEntityIO::OutputBuffer_PostPhysics*             lpPropOutput_PostPhysics    = 0;
    WorldEntityIO::OutputBuffer_PostPhysics*            lpWorldEntityOutput_PostPhysics = 0;
    RaceCarEntityModuleIO::InputBuffer_PostPhysics*     lpRaceCarInput_PostPhysics  = 0;
    PropEntityIO::InputBuffer_PostPhysics*              lpPropInput_PostPhysics     = 0;
    WorldEntityIO::InputBuffer_PostPhysics*             lpWorldEntityInput_PostPhysics = 0;
    lpOutputBufferStack->CreateIOBuffer( &lpRaceCarOutput_PostPhysics, "RaceCarPostPhysics" );
    lpOutputBufferStack->CreateIOBuffer( &lpTrafficOutput_PostPhysics, "TrafficPostPhysics" );
    lpOutputBufferStack->CreateIOBuffer( &lpPropOutput_PostPhysics, "PropPostPhysics" );
    lpOutputBufferStack->CreateIOBuffer( &lpWorldEntityOutput_PostPhysics, "WorldEntityPostPhysics" );
    lpInputBufferStack->CreateIOBuffer( &lpRaceCarInput_PostPhysics, "RaceCarPostPhysics" );
    lpInputBufferStack->CreateIOBuffer( &lpPropInput_PostPhysics, "PropPostPhysics" );
    lpInputBufferStack->CreateIOBuffer( &lpWorldEntityInput_PostPhysics, "WorldEntityPostPhysics" );
    lpRaceCarOutput_PostPhysics->Construct();
    lpTrafficOutput_PostPhysics->Construct();
    lpPropOutput_PostPhysics->Construct();
    lpWorldEntityOutput_PostPhysics->Construct();
    lpRaceCarInput_PostPhysics->Construct();
    lpPropInput_PostPhysics->Construct();
    lpWorldEntityInput_PostPhysics->Construct();
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_World );

    // ---- AI -> entity modules post-physics ----------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_NetworkAIRaceCar );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_AI );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_AI_Bridge );
    CgsModule::LockBuffersForIO( lpRaceCarInput_PostPhysics, lpAIOutput );
    ::WorldModule::BridgeAIToEntityModules_PostPhysics(
        this, lpRaceCarInput_PostPhysics, lpAIOutput );
    CgsModule::UnlockBuffersForIO( lpRaceCarInput_PostPhysics, lpAIOutput );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_AI_Bridge );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_AI );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_NetworkAIRaceCar );

    // ---- world-entity action staging ----------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_World );
    CGS_ASSERT( lpWorldEntityInput_PostPhysics != 0, "lpInputBuffer" );
    lpWorldEntityInput_PostPhysics->LockForWrite();
    ::WorldModule::BridgeActionsToWorldModule( this, lpWorldEntityInput_PostPhysics,
                                               lpUpdateInputBuffer );
    lpWorldEntityInput_PostPhysics->UnlockForWrite();
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_World );

    // ---- POST-PHYSICS spine (streamer request flush rides it) ---------------
    EntityModulePostPhysicsUpdate(
        lpInputBufferStack, lpOutputBufferStack,
        lpPhysicsOutput,
        lpTrafficInput_PostPhysics, lpTrafficOutput_PostPhysics,
        lpRaceCarInput_PostPhysics, lpRaceCarOutput_PostPhysics,
        lpCrashInput_PostPhysics, lpCrashOutput_PostPhysics,
        lpPropInput_PostPhysics, lpPropOutput_PostPhysics,
        lpWorldEntityInput_PostPhysics, lpWorldEntityOutput_PostPhysics,
        lUpdateSet );

    // ---- post-physics -> scene / output staging ------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_World );
    CgsModule::LockBuffersForIO( lpSceneInput_Update, lpTrafficOutput_PostPhysics,
                                 lpRaceCarOutput_PostPhysics, lpPropOutput_PostPhysics,
                                 lpWorldEntityOutput_PostPhysics );
    ::WorldModule::BridgeEntityModulesToScene_PostPhysics(
        this, lpSceneInput_Update, lpTrafficOutput_PostPhysics, lpRaceCarOutput_PostPhysics,
        lpPropOutput_PostPhysics, lpWorldEntityOutput_PostPhysics );
    CgsModule::UnlockBuffersForIO( lpSceneInput_Update, lpTrafficOutput_PostPhysics,
                                   lpRaceCarOutput_PostPhysics, lpPropOutput_PostPhysics,
                                   lpWorldEntityOutput_PostPhysics );

    CgsModule::LockBuffersForIO( lpUpdateOutputBuffer, lpTrafficOutput_PostPhysics,
                                 lpRaceCarOutput_PostPhysics, lpPropOutput_PostPhysics,
                                 lpWorldEntityOutput_PostPhysics );
    ::WorldModule::BridgeEntityModulesToOutput_PostPhysics(
        this, lpUpdateOutputBuffer, lpTrafficOutput_PostPhysics, lpRaceCarOutput_PostPhysics,
        lpPropOutput_PostPhysics, lpWorldEntityOutput_PostPhysics );
    CgsModule::UnlockBuffersForIO( lpUpdateOutputBuffer, lpTrafficOutput_PostPhysics,
                                   lpRaceCarOutput_PostPhysics, lpPropOutput_PostPhysics,
                                   lpWorldEntityOutput_PostPhysics );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_World );

    lpInputBufferStack->DestroyIOBuffer( &lpWorldEntityInput_PostPhysics );
    lpInputBufferStack->DestroyIOBuffer( &lpPropInput_PostPhysics );
    lpInputBufferStack->DestroyIOBuffer( &lpRaceCarInput_PostPhysics );
    lpOutputBufferStack->DestroyIOBuffer( &lpWorldEntityOutput_PostPhysics );
    lpOutputBufferStack->DestroyIOBuffer( &lpPropOutput_PostPhysics );
    lpOutputBufferStack->DestroyIOBuffer( &lpTrafficOutput_PostPhysics );
    lpOutputBufferStack->DestroyIOBuffer( &lpRaceCarOutput_PostPhysics );

    // ---- the second scene UpdateScene pass (post-physics) -------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_World );
    PerfMonCpu::StartMonitor( miSceneManagerUpdatePM );
    mSceneModule.UpdateScene( lpInputBufferStack, lpOutputBufferStack,
                              lpSceneInput_Update, lpSceneOutput, true );
    PerfMonCpu::StopMonitor( miSceneManagerUpdatePM );
    lpInputBufferStack->DestroyIOBuffer( &lpSceneInput_Update );
    lpUpdateInputBuffer->UnlockForRead();
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_World );

    // ---- output bridges ------------------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_NetworkAIRaceCar );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_AI );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_AI_Bridge );
    CgsModule::LockBuffersForIO( lpUpdateOutputBuffer, lpAIOutput );
    ::WorldModule::BridgeAIModuleToOutput( this, lpUpdateOutputBuffer, lpAIOutput );
    CgsModule::UnlockBuffersForIO( lpUpdateOutputBuffer, lpAIOutput );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_AI_Bridge );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_AI );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_NetworkAIRaceCar );

    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_Physics );
    PerfMonCpu::StartMonitor( miPhysicsSummaryPM );
    PerfMonCpu::StartMonitor( miPhysicsBridgesPM );
    CgsModule::LockBuffersForIO( lpUpdateOutputBuffer, lpPhysicsOutput );
    ::WorldModule::BridgePhysicsToOutput( this, lpUpdateOutputBuffer, lpPhysicsOutput );
    CgsModule::UnlockBuffersForIO( lpUpdateOutputBuffer, lpPhysicsOutput );
    PerfMonCpu::StopMonitor( miPhysicsBridgesPM );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_Physics );
    PerfMonCpu::StopMonitor( miPhysicsSummaryPM );

    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_World );
    CgsModule::LockBuffersForIO( lpUpdateOutputBuffer, lpCrashOutput_PostPhysics );
    ::WorldModule::BridgeCrashModuleToOutput( this, lpUpdateOutputBuffer,
                                              lpCrashOutput_PostPhysics );
    CgsModule::UnlockBuffersForIO( lpUpdateOutputBuffer, lpCrashOutput_PostPhysics );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_World );

    // ---- player-car position/speed latch + environment map -------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_World );
    lpUpdateOutputBuffer->LockForRead();
    {
        // The replay interface is selected when the update set carries 0x100.
        const BrnWorldIO::UpdateOutputBuffer::RCEntityActiveRaceCarOutputInterface*
            lpActiveRaceCars = ( lUpdateSet & 0x100 )
                ? lpUpdateOutputBuffer->GetReplayActiveRaceCarOutputInterface()
                : lpUpdateOutputBuffer->GetActiveRaceCarOutputInterface();

        if ( lpActiveRaceCars->IsPlayerCarActive() )
        {
            const EActiveRaceCarIndex lePlayerIndex =
                lpActiveRaceCars->GetPlayerActiveRaceCarIndex();
            const BrnPhysics::Vehicle::RaceCarState* lpPlayerState =
                lpActiveRaceCars->GetRaceCarState( lePlayerIndex );

            // Env-map refresh around the player car; latch its position.
            mEnvironmentMap.Update( lpPlayerState->mTransform.Pos() );
            const Vector3 lPlayerPosition = lpPlayerState->mTransform.Pos();
            mPlayerCarPosition =
                Vector4{ lPlayerPosition.x, lPlayerPosition.y, lPlayerPosition.z, 0.0f };

            // |linear velocity| -> the player speed member (X360 vmsum3fp + vrsqrte
            // + Newton refinement == rw::math::vpu::Magnitude).
            mfLocalPlayerActiveRaceCarSpeed =
                rw::math::vpu::Magnitude( lpPlayerState->mLinearVelocity );
        }
    }
    lpUpdateOutputBuffer->UnlockForRead();

    // The data-dump monitor pair (X360 start/stop back-to-back -- the dump body
    // itself is compiled out of the ARTIST build).
    PerfMonCpu::StartMonitor( miWorldModuleDataDumpPM );
    PerfMonCpu::StopMonitor( miWorldModuleDataDumpPM );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_World );

    // ---- teardown ------------------------------------------------------------
    lpOutputBufferStack->DestroyIOBuffer( &lpCrashOutput_PostPhysics );
    lpInputBufferStack->DestroyIOBuffer( &lpCrashInput_PostPhysics );
    lpOutputBufferStack->DestroyIOBuffer( &lpCrashOutput_PreScene );
    lpInputBufferStack->DestroyIOBuffer( &lpCrashInput_PreScene );
    lpInputBufferStack->DestroyIOBuffer( &lpTrafficInput_PostPhysics );
    lpInputBufferStack->DestroyIOBuffer( &lpTrafficInput_PostScene );
    lpOutputBufferStack->DestroyIOBuffer( &lpAIOutput );
    lpOutputBufferStack->DestroyIOBuffer( &lpTriggerOutput_PrePhysics );
    lpInputBufferStack->DestroyIOBuffer( &lpTriggerInput_PrePhysics );
    lpOutputBufferStack->DestroyIOBuffer( &lpTriggerOutput_PostScene );
    lpInputBufferStack->DestroyIOBuffer( &lpTriggerInput_PostScene );
    lpOutputBufferStack->DestroyIOBuffer( &lpTriggerOutput_PreScene );
    lpInputBufferStack->DestroyIOBuffer( &lpTriggerInput_PreScene );
    lpOutputBufferStack->DestroyIOBuffer( &lpSceneOutput );
    lpOutputBufferStack->DestroyIOBuffer( &lpPhysicsOutput );
    lpInputBufferStack->DestroyIOBuffer( &lpPhysicsInput );
}

// ============================================================================
// FilterFrustumTestResults  @ 0x827BDA60
//
// Split one coarse-query RESULT record (SceneManagerIO::OutCoarseQueryResult:
// { SceneQueryId, numResults, numResultsAttempted, EntityId ids[] }) into the four
// per-owner id arrays the modules' GenerateDispatchLists consume. All four arrays
// are cleared first (the X360 stores 0 straight into each array's count word), then
// every id is dispatched on its OWNER byte:
//     1, 0x21 -> race car (32)      2 -> traffic (650)
//     3, 0x22 -> prop (5400)        5 -> WORLD (4500)
// Any other owner is dropped. Each append is guarded by the array's own capacity --
// the X360 compares the live count against the capacity and SKIPS the append when
// full rather than growing (a full array silently stops collecting).
// (The X360 reads the owner as the first byte of the big-endian 4-byte id; on the
// host that is EntityId::GetOwner(), the id's top 8 bits -- same value.)
// ============================================================================
void
WorldModule::FilterFrustumTestResults(
    const CgsModule::Event* lpFrustumTestResult,
    Array<CgsSceneManager::EntityId, 4500u>* lpWorldIds,
    Array<CgsSceneManager::EntityId, 32u>* lpRaceCarIds,
    Array<CgsSceneManager::EntityId, 650u>* lpTrafficIds,
    Array<CgsSceneManager::EntityId, 5400u>* lpPropIds )
{
    const CgsSceneManager::SceneManagerIO::OutCoarseQueryResult* lpResult =
        static_cast< const CgsSceneManager::SceneManagerIO::OutCoarseQueryResult* >(
            lpFrustumTestResult );

    const s32 liNumResults = lpResult->miNumResults;

    lpWorldIds->Clear();
    lpRaceCarIds->Clear();
    lpTrafficIds->Clear();
    lpPropIds->Clear();

    if ( liNumResults <= 0 )
    {
        return;
    }

    const CgsSceneManager::EntityId* lpIds = lpResult->GetEntityIds();

    for ( s32 liResult = 0; liResult < liNumResults; liResult++ )
    {
        const CgsSceneManager::EntityId lEntityId = lpIds[ liResult ];

        switch ( lEntityId.GetOwner() )
        {
            case 1:
            case 0x21:
                if ( lpRaceCarIds->GetLength() < 32u )
                {
                    lpRaceCarIds->Append( lEntityId );
                }
                break;

            case 2:
                if ( lpTrafficIds->GetLength() < 650u )
                {
                    lpTrafficIds->Append( lEntityId );
                }
                break;

            case 3:
            case 0x22:
                if ( lpPropIds->GetLength() < 5400u )
                {
                    lpPropIds->Append( lEntityId );
                }
                break;

            case 5:
                if ( lpWorldIds->GetLength() < 4500u )
                {
                    lpWorldIds->Append( lEntityId );
                }
                break;

            default:
                break;
        }
    }
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
    BrnWorldIO::DispatchOutputBuffer* /*lpDispatchOutputBuffer*/,
    const BrnUpdateSet* lpUpdateSet )
{
    // Frustum testing is selected by update-set bit 7. (SIGNATURE RECONCILED
    // 2026-07-28: the X360 passes six args -- the dispatch OUTPUT buffer and the update
    // set BY POINTER, `&updateSet`, because BrnGameModule::DoDispatch @0x823DC458 clears
    // bit 7 in place when the streamer reports live streaming and both producers read the
    // same word. The old 4-arg by-value form could not see that clear.)
    const BrnUpdateSet lUpdateSet = *lpUpdateSet;
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
    // PC Construct restoration (the X360 CreateIOBuffer<T> stack template runs
    // T::Construct after the alloc; the generic PC template placement-news only).
    lpQueryInput->Construct();
    lpQueryOutput->Construct();

    lpDispatchInputBuffer->LockForRead();

    const BrnDirector::Camera::Camera* lpCameraInput = lpDispatchInputBuffer->GetCameraInput();

    // The frame's graphics camera (file-static; the X360 rebuilds it in place).
    gFrustumQueryCamera.Release();
    lpCameraInput->CopyToCgsCamera( &gFrustumQueryCamera );

    // ---- shadow-map cascade cameras ---------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_RenderShadowMap );
    if ( mShadowMap.IsEnabled() )
    {
        // RECONCILED with the landed ShadowMap camera math: the X360 r4 IS the
        // DIRECTOR camera input (asm-proven in the CalculateShadowMapCameras
        // reconstruction; the real overload takes BrnDirector::Camera::Camera*).
        mShadowMap.CalculateShadowMapCameras( mEnvironmentManager.CalcKeyLightDirection(),
                                              lpCameraInput );
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
    // PC Construct restoration (the X360 CreateIOBuffer<T> stack template runs
    // T::Construct after the alloc; the generic PC template placement-news only).
    lpQueryInput->Construct();
    lpQueryOutput->Construct();
    lpInputBufferStack->CreateIOBuffer( &lpRaceCarDispatchInput, "RaceCar" );
    lpInputBufferStack->CreateIOBuffer( &lpFilteredEntityData, "Filtered Entity Data" );
    // PC Construct restoration (the X360 CreateIOBuffer<T> stack template runs
    // T::Construct after the alloc; the generic PC template placement-news only).
    lpWorldDispatchInput->Construct();
    lpTrafficDispatchInput->Construct();
    lpPropDispatchInput->Construct();
    lpRaceCarDispatchInput->Construct();
    lpFilteredEntityData->Construct();
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
    // (RECONCILED 2026-07-27: the scene module's stack parameters are the plain
    //  CgsModule::IOBufferStack the world drive threads everywhere; the old
    //  scene-local alias casts are retired.)
    mSceneModule.ProcessFrustumTestJobResults( lpInputBufferStack, lpOutputBufferStack,
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
                KI_RACE_CAR_OBJECT_LIST, KI_RACE_CAR_OPAQUE_MESH_LIST,
                KI_RACE_CAR_TRANSPARENT_MESH_LIST, false,
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
                liCascadeList, liCascadeList, liCascadeList, false,
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


// =============================================================================
// [FLAG PC bring-up] PublishWorldShadingConstantsBringUp -- NOT an X360 function.
//
// The world's real vertex/pixel programs (SHADERS.BNDL) read a fixed set of engine
// constants that the console publishes from the lighting / environment / shadow-map
// modules: BrnEnvironmentManager feeds the key light + the two irradiance quadrics,
// BrnSkyDomeManager the scattering + fog, BrnShadowMap the cascade matrices and their
// two constant vectors. None of those producers is live on this build, so every one of
// those constants would arrive NULL and the technique would draw with whatever was left
// in the register file.
//
// The values below are NOT invented: they are the SHIPPED environment keyframe
// ENV_KF_Paradise_ingame_junk_city_1200 (build/game/ENVIRONMENTSETTINGS/
// PARADISE_INGAME_JUNK.BUNDLE, one of the nine city_HHMM keyframes; 0x240 bytes each,
// decoded against BrnEnvironmentKeyframe.h's asm-attested layout) pushed through the real
// X360 producer maths:
//
//   WorldModule::SetupShaderConstantsBeforeRendering  @0x827D1410  (out-param -> slot)
//   EnvironmentSettings::EnvironmentManager::
//                            GenerateShaderConstants  @0x827D0098
//   GlobalIrradianceManager::ComputeFrameCoeffs       @0x827C5188  (basis + 6 projections)
//   GlobalIrradianceManager::UpdateCoefficients       @0x827BEF70  (order-2 SH projection)
//   GlobalIrradianceManager::ComputeIrradianceMatrix  @0x827B1160  (irradiance matrix)
//   sub_827B0790                                                   (matrices -> quadrics)
//
// Two inputs of that chain cannot be attested until ENVIRONMENTSETTINGS is converted from
// its stock X360 platform-2 form and actually streamed, so they stay bring-up choices and
// everything else is made consistent with them:
//   * KeyLightDirection. ComputeKeyLightDirection @0x82678AB0 derives it from the manager's
//     time-of-day + three tuning angles (float504/510/514/518), none of which exist yet.
//     The pre-existing bring-up direction is kept, and the irradiance rig below is solved
//     for THAT direction, so the fill lights and the key light agree.
//   * whiteLevel (EnvironmentManager::float11C0). Every colour the console publishes is
//     scaled by it and the post-FX tonemap divides it back out; with post-FX off, 1.0 is
//     the only non-blowing value -- which is what HDRConstants already assumed.
//
//   IrradianceQuadricA/B  ComputeIrradianceFast = saturate(A[0].xyz + linear(N) +
//                         quadratic(N)). Solved offline by the four functions above from
//                         the keyframe's six fill colours x AmbientIrradianceScale (0.4);
//                         the repack was checked against n^T M n on 2000 random unit
//                         normals (max error 1.1e-16). ComputeIrradianceRigFromSky
//                         (@0x8267C948, gated on the manager's gap6F5) is treated as off,
//                         i.e. the authored fill colours are used rather than sky-derived
//                         ones.
//   KeyLight*             the sun. Direction is the direction the light TRAVELS (the
//                         shaders use -KeyLightDirection as the vector towards it).
//                         Colour/Specular are the keyframe's; Specular additionally
//                         carries gfSpecularScale, which is 1.0 in the shipped image
//                         (@0x82F307E8). Clamped = min(max(colour,0), whiteLevel).
//   ScattCoeffs           CalculateScattering = pow(saturate(d*x - y), z) * w with
//                         x = 1/(far-near), y = near/(far-near), z = ScattPow, w = ScattCap
//                         -- i.e. .w is the fog CAP, and publishing 0 (as this function
//                         used to) made fog identically zero on every world mesh.
//   ShadowMap_Constants/2 CalcShadowFactor*CSM ends in
//                         ApplyFade(factor, fade) = factor*fade + 1 - fade with
//                         fade = saturate(Constants.w - eyeZ * Constants2.w). Zeroing both
//                         .w gives fade = 0 and therefore a shadow factor of EXACTLY 1.0
//                         whatever the (unbound) shadow-map sampler returns -- the
//                         data-driven "shadows off" configuration, not a shader edit.
//   ShadowMap_WorldToLight the three cascade matrices; identity while there is no shadow
//                         pass (their result is multiplied out by the fade above).
//
// DELETE together with GenerateDispatchListsBringUp when the environment / sky / shadow
// modules publish these for real.
// =============================================================================
void
WorldModule::PublishWorldShadingConstantsBringUp()
{
    ::ShaderConstantTable& lrTable = CgsGraphics::mShaderConstantTable;

    // --- ambient irradiance: the real six-fill rig solved into the shader's quadric form
    // (rows 1..3 of A are the per-channel (x, y, z, x*x) coefficients, rows 0..2 of B the
    // per-channel (x*y, y*z, z*x, y*y) coefficients; A[0].xyz is the constant term).
    Matrix44 lQuadricA;
    lQuadricA.xAxis = Vector4{  0.365244f,  0.427732f,  0.430775f,  0.000000f };
    lQuadricA.yAxis = Vector4{ -0.061486f,  0.087392f, -0.067495f,  0.000131f };
    lQuadricA.zAxis = Vector4{ -0.058614f,  0.118284f, -0.064128f, -0.000443f };
    lQuadricA.wAxis = Vector4{ -0.056434f,  0.132722f, -0.057723f, -0.000384f };
    Matrix44 lQuadricB;
    lQuadricB.xAxis = Vector4{  0.000000f,  0.000000f, -0.004146f, -0.057522f };
    lQuadricB.yAxis = Vector4{  0.000000f,  0.000000f,  0.014042f, -0.073080f };
    lQuadricB.zAxis = Vector4{  0.000000f,  0.000000f,  0.012188f, -0.066545f };
    lQuadricB.wAxis = Vector4{  0.000000f,  0.000000f,  0.000000f,  0.000000f };
    lrTable.SetShaderConstantData( 18, lQuadricA );
    lrTable.SetShaderConstantData( 19, lQuadricB );

    // --- the key light (direction of travel: high sun, slightly behind the camera) -----
    const Vector4 lKeyLightDirection{ 0.406f, -0.812f, 0.419f, 0.0f };
    lrTable.SetShaderConstantData( 10, lKeyLightDirection );
    lrTable.SetShaderConstantData(  9, Vector4{ 1.700000f, 1.700000f, 1.054000f, 1.0f } );   // KeyLightColour
    lrTable.SetShaderConstantData( 11, Vector4{ 1.591840f, 1.286597f, 0.954163f, 1.0f } );   // KeyLightSpecularColour
    lrTable.SetShaderConstantData( 12, Vector4{ 1.000000f, 1.000000f, 1.000000f, 1.0f } );   // KeyLightClampedColour

    // --- atmosphere: ScattDist (25, 1500), ScattPow 1, ScattCap 0.87; the fog colour is
    // the keyframe's ScattHorColour and the sky reflection its (SkyHorColour, SkyHorPow).
    lrTable.SetShaderConstantData( 27, Vector4{ 0.000678f, 0.016949f, 1.000000f, 0.870000f } );   // ScattCoeffs
    lrTable.SetShaderConstantData( 28, Vector4{ 0.383928f, 0.437485f, 0.527000f, 1.000000f } );   // FogColourPlusWhiteLevel
    lrTable.SetShaderConstantData( 33, Vector4{ 1.015278f, 0.882773f, 0.807155f, 0.500000f } );   // SkyReflectionColour
    lrTable.SetShaderConstantData( 29, Vector4{ 1.000000f, 1.000000f, 0.000000f, 0.000000f } );   // HDRConstants

    // --- shadow cascades: fade = 0 -> shadow factor exactly 1 (see the banner) ---------
    Matrix44 laWorldToLight[3];
    for ( int liCascade = 0; liCascade < 3; ++liCascade )
    {
        laWorldToLight[liCascade].xAxis = Vector4{ 1.0f, 0.0f, 0.0f, 0.0f };
        laWorldToLight[liCascade].yAxis = Vector4{ 0.0f, 1.0f, 0.0f, 0.0f };
        laWorldToLight[liCascade].zAxis = Vector4{ 0.0f, 0.0f, 1.0f, 0.0f };
        laWorldToLight[liCascade].wAxis = Vector4{ 0.0f, 0.0f, 0.0f, 1.0f };
    }
    lrTable.SetShaderConstantArrayData( 14, laWorldToLight );
    lrTable.SetShaderConstantData( 15, Vector4{ 0.0f, 0.0f, 0.0f, 0.0f } );      // ShadowMap_Constants
    lrTable.SetShaderConstantData( 16, Vector4{ 0.0f, 0.0f, 0.0f, 0.0f } );      // ShadowMap_Constants2
    lrTable.SetShaderConstantData( 17, Vector4{ 0.0f, 1.0f, 0.0f, 0.0f } );      // ShadowMap_ObjectCsmSelect

    // --- misc ---------------------------------------------------------------------------
    lrTable.SetShaderConstantData( 13, Vector4{ 0.0f, 0.0f, 0.0f, 0.0f } );      // Time
}

// =============================================================================
// [FLAG PC bring-up] GenerateDispatchListsBringUp -- NOT an X360 function.
//
// The console producer chain is BrnGameModule::DoDispatch @0x823DC458 ->
// (BrnRendererModule::Update publishes the GDL frame; BridgeRendererToWorld hands
// it to the world) -> WorldModule::GenerateFrustumQueries -> ::GenerateDispatchLists.
// On this build that chain has three dead links: the director module produces no
// camera, the renderer/world dispatch IO buffer set is not created anywhere, and the
// scene manager's entity registration + frustum-test jobs are documented inert gates
// (so ::GenerateDispatchLists' frustum result would be null). This is the smallest
// honest stand-in: frame a camera on the geometry the streamer has actually
// delivered, publish the three camera shader constants the dispatch interpreter
// reads back, and run the world entity module's streamer-driven feed.
//
// Everything downstream of here is the real reconstructed path: RenderInstance ->
// DrawRenderable::AddToBin -> DispatchList::Submit -> ConvertObjectsToMeshes ->
// DrawRenderable::Interpret -> DispatchAllMeshes -> the D3D9 draw leaf.
//
// DELETE the whole function (and its DoDispatch call) once DoDispatch and the scene
// manager's frustum query are real.
// =============================================================================
// (The three plane-building vector helpers that used to live here are gone: the frustum
// is now produced by the real CgsGraphics::Camera::GetFrustumPerspective @0x827F0AD8.)

// [FLAG PC bring-up] see the header. Staged by BrnGameModule::DoDispatch from the director
// module's published camera; consumed (and cleared) by the next GenerateDispatchListsBringUp.
void
WorldModule::SetBringUpCameraOverride( const rw::math::vpu::Matrix44Affine& lrTransform,
                                       f32 lfFOVDegrees )
{
    mBringUpCameraOverride       = lrTransform;
    mfBringUpCameraOverrideFOV   = lfFOVDegrees;
    mbBringUpCameraOverrideValid = true;
}

void
WorldModule::GenerateDispatchListsBringUp( CgsGraphics::DispatchFrame* lpDispatchFrame )
{
    if ( lpDispatchFrame == 0 )
    {
        return;
    }

    // [DIAG] BRN_WORLD_CAMFREE=1 makes this stand-in IGNORE the director override, so a
    // capture gets the tour/establishing camera below (which frames the spawned race car)
    // instead of whatever the director's fly-by is pointing at. Sibling of the two
    // BRN_WORLD_CAM* switches below and equally capture-only: it changes nothing unless the
    // variable is set. Needed because on this build the director parks the view inside the
    // junkyard's scrap geometry, so no frame of a default run ever shows the car.
    static s32 siCamFree = -1;
    if ( siCamFree < 0 )
    {
        const char* lpcFreeEnv = std::getenv( "BRN_WORLD_CAMFREE" );
        siCamFree = ( lpcFreeEnv != 0 && lpcFreeEnv[0] != '0' ) ? 1 : 0;
    }

    // Consume this frame's director-camera override (one frame only -- see the header).
    const bool                          lbUseDirectorCamera =
        mbBringUpCameraOverrideValid && ( siCamFree == 0 );
    const rw::math::vpu::Matrix44Affine lDirectorTransform  = mBringUpCameraOverride;
    const f32                           lfDirectorFOVDegs   = mfBringUpCameraOverrideFOV;
    mbBringUpCameraOverrideValid = false;

    // ---- frame an establishing camera on the loaded world -------------------
    Vector3 lCentre;
    f32     lfRadius = 0.0f;
    if ( !mWorldEntityModule.GetLoadedWorldBounds( &lCentre, &lfRadius ) )
    {
        return;   // the streamer has delivered nothing yet
    }
    if ( lfRadius < 1.0f )
    {
        lfRadius = 1.0f;
    }

    // Eye: pulled back and up from the centroid along -Z, looking at the centroid.
    // (Burnout world space is Y-up -- the PVS query is a ground-plane XZ lookup.)
    //
    // [DIAG] BRN_WORLD_CAMDIST scales the pull-back so a capture can inspect the
    // surfaces close up instead of the whole-city establishing shot (every world
    // texel is minified to its smallest mip from the default distance, which hides
    // exactly the texture defects this camera exists to expose). Read once.
    static f32 sfCamDist = -1.0f;
    if ( sfCamDist < 0.0f )
    {
        sfCamDist = 1.0f;
        const char* lpcEnv = std::getenv( "BRN_WORLD_CAMDIST" );
        if ( lpcEnv != 0 )
        {
            const f32 lfValue = static_cast<f32>( atof( lpcEnv ) );
            if ( lfValue > 0.0001f )
            {
                sfCamDist = lfValue;
            }
        }
    }

    // [FLAG PC bring-up] BRN_WORLD_CAMSPEED (default 1.0, 0 == the static establishing
    // shot) drives the FLY-THROUGH stand-in: the console's camera comes from the director
    // module, and until that subsystem lands nothing moves -- which also means the world
    // streamer's PVS query point never moves, so its working set can never roll. The path
    // is a circle in the world XZ plane around the FIRST framing the streamer delivered
    // (latched once: the live bounds move as the working set rolls, and feeding those back
    // into the camera would make it chase its own tail). DELETE with the rest of this
    // function when the director camera lands.
    static f32 sfCamSpeed = -1.0f;
    if ( sfCamSpeed < 0.0f )
    {
        sfCamSpeed = 1.0f;
        const char* lpcSpeedEnv = std::getenv( "BRN_WORLD_CAMSPEED" );
        if ( lpcSpeedEnv != 0 )
        {
            const f32 lfValue = static_cast<f32>( atof( lpcSpeedEnv ) );
            if ( lfValue >= 0.0f )
            {
                sfCamSpeed = lfValue;
            }
        }
    }

    static bool    sbPathLatched = false;
    static bool    sbFramingCar  = false;
    static Vector3 sPathOrigin;
    static f32     sfPathRadius = 1.0f;
    static f32     sfPathAngle  = 0.0f;
    if ( !sbPathLatched )
    {
        // ⚠️ THE LATCH NOW WAITS FOR THE CAR (2026-08-01, reset-player-car wave). It used to
        // fire on its first frame, which worked only because the car was spawned by
        // SpawnFirstUnlockedCarBringUp from inside RaceCarEntityModule::UpdateStreaming --
        // i.e. before this producer ever ran. The car is placed by the console's own
        // ResetPlayerCarAction chain now, and that chain starts at IN-GAME entry, ~100 log
        // lines AFTER this function's first frame. MEASURED: without this the tour latched on
        // the resident-world centre and orbited 3.5 km from the only car in the world.
        sPathOrigin   = lCentre;

        // [FLAG PC bring-up] ...unless a race car has actually been spawned, in which case
        // frame IT. The car is parked at the junkyard spawn location the trigger data
        // authored, which is nowhere near the resident-world centre this tour normally
        // orbits, so without this the only thing on this build that is genuinely posed in
        // the world would never be on screen. Publishing the car's position as the PVS query
        // centre is also what makes the streamer deliver the city AROUND it.
        // DELETE with the rest of this bring-up producer.
        {
            Vector3 lCarPosition;
            if ( mRaceCarEntityModule.GetSpawnedCarPositionBringUp( lCarPosition ) )
            {
                sPathOrigin  = lCarPosition;
                sbFramingCar = true;
                if ( CgsDev::Log::gpDebugPrint != 0 )
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[FLAG PC bring-up] tour camera framing the spawned race car at ("
                        << lCarPosition.x << ", " << lCarPosition.y << ", "
                        << lCarPosition.z << ") instead of the resident-world centre\n";
                }
            }
        }

        // [FLAG PC bring-up] The tour has to stay INSIDE the resident footprint, because
        // the fly-through publishes its EYE as the PVS query position (see lPvsPosition
        // below) -- an eye outside the city drops the ground-plane zone lookup off the map
        // and the streamer unloads the world the camera is supposed to be touring.
        // The orbit below breathes between 1.1x and 2.6x this value, so it must be a
        // FRACTION of the resident-world radius, not the radius itself.
        //
        // This only became visible once the producer stopped running outside the in-game
        // state: it used to latch on the boot loading screen with two track units resident
        // (a small radius, so 1.1-2.6x of it still landed inside the city), and now latches
        // at in-game entry with the whole PVS working set resident. Scaling it makes the
        // framing independent of how much the streamer happens to have delivered when the
        // producer's first frame runs.
        const f32 KF_TOUR_RADIUS_FRACTION = 0.25f;
        sfPathRadius  = lfRadius * KF_TOUR_RADIUS_FRACTION;
        if ( sfPathRadius < 1.0f )
        {
            sfPathRadius = 1.0f;
        }

        // [FLAG PC bring-up] framing the CAR is a close-up, not a city tour: a 350 m
        // orbit puts a 4 m car below one pixel, and a 350 m PVS neighbourhood centred
        // 3.5 km from where the loading screen left the working set floods
        // BaseStreamer's target list ("Stream target list is full", measured: 425 dev
        // asserts in one run). 9 m keeps both the framing and the streamer sane.
        if ( sbFramingCar )
        {
            sfPathRadius = 9.0f;
        }

        // Only LATCH once there is a car to frame; until then re-evaluate every frame (the
        // world-centre framing above is what it falls back to, exactly as before).
        sbPathLatched = sbFramingCar;

        if ( sbPathLatched && CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint
                << "[FLAG PC bring-up] establishing camera latched: centre=("
                << lCentre.x << "," << lCentre.y << "," << lCentre.z
                << ") worldRadius=" << lfRadius << " tourRadius=" << sfPathRadius << "\n";
        }
    }

    Vector3 lEye;
    Vector3 lLookAt = sPathOrigin;
    // The point published as the PVS query position. For the fly-through that IS the eye
    // (it travels through the city, which is what the console's camera position does); for
    // the FROZEN establishing shot the eye is deliberately pulled far back and up OUT of the
    // city footprint, and publishing it would drop the ground-plane zone lookup off the map
    // and unload everything -- so the frozen shot publishes the point it is framing.
    Vector3 lPvsPosition;

    if ( lbUseDirectorCamera )
    {
        // ⭐ THE DIRECTOR CAMERA. Its transform rows are the fly-by's own basis: wAxis is the
        // eye and zAxis is the unit forward the road runner's look-at built, so the framing is
        // eye -> eye + forward and the PVS query point is the eye itself -- exactly what the
        // console's WorldModule::Update feeds WorldEntityModule::PreSceneUpdate
        // (mLastCameraInput.GetPosition()).
        lEye        = lDirectorTransform.wAxis;
        lEye.w      = 0.0f;
        lLookAt.x   = lEye.x + lDirectorTransform.zAxis.x;
        lLookAt.y   = lEye.y + lDirectorTransform.zAxis.y;
        lLookAt.z   = lEye.z + lDirectorTransform.zAxis.z;
        lLookAt.w   = 0.0f;
        lPvsPosition = lEye;
    }
    else if ( sfCamSpeed > 0.0f )
    {
        // One lap per ~40 s of dispatch frames at speed 1 (the dispatch spine runs once
        // per rendered frame; a fixed per-frame step keeps a capture reproducible).
        sfPathAngle += 0.0052f * sfCamSpeed;

        // The orbit radius itself breathes between 1.1x and 2.6x the latched framing over
        // four laps, so the tour sweeps a whole annulus of the city instead of retracing
        // one ring (a fixed ring converges after one lap: every point on it shares the same
        // PVS neighbourhood, so the working set stops rolling).
        const f32 lfOrbit = sfPathRadius
                          * ( 1.10f + 1.50f * ( 0.5f - 0.5f * cosf( sfPathAngle * 0.25f ) ) );
        lEye.x = sPathOrigin.x + lfOrbit * cosf( sfPathAngle );
        lEye.y = sPathOrigin.y + sfPathRadius * 0.30f * sfCamDist;
        lEye.z = sPathOrigin.z + lfOrbit * sinf( sfPathAngle );
        lEye.w = 0.0f;

        // Look along the tangent and slightly down -- a drive-through framing, so the
        // geometry the streamer is bringing in ahead of the camera is what fills the view.
        lLookAt.x = lEye.x - lfOrbit * 0.75f * sinf( sfPathAngle );
        lLookAt.y = sPathOrigin.y - sfPathRadius * 0.10f;
        lLookAt.z = lEye.z + lfOrbit * 0.75f * cosf( sfPathAngle );
        lLookAt.w = 0.0f;

        // [FLAG PC bring-up] ...except when the orbit is framing the spawned race car, in
        // which case look AT it. The tangential framing above exists to sweep new city
        // geometry into view; a 9 m orbit around a parked car needs the opposite.
        if ( sbFramingCar )
        {
            lLookAt   = sPathOrigin;
            lLookAt.w = 0.0f;
        }

        lPvsPosition = lEye;
    }
    else
    {
        // The static establishing shot, framed on the LATCHED first framing (using the live
        // bounds here would close a feedback loop: the bounds follow the working set, the
        // working set follows the camera, and the camera would chase its own tail).
        lEye.x = sPathOrigin.x;
        lEye.y = sPathOrigin.y + sfPathRadius * 0.45f * sfCamDist;
        lEye.z = sPathOrigin.z - sfPathRadius * 1.15f * sfCamDist;
        lEye.w = 0.0f;

        lPvsPosition = sPathOrigin;
    }

    // [FLAG PC bring-up] The console latches the frame camera into mLastCameraInput inside
    // the real WorldModule::GenerateDispatchLists (`mLastCameraInput = *lpCameraInput`);
    // WorldModule::Update then feeds mLastCameraInput.GetPosition() to
    // WorldEntityModule::PreSceneUpdate as the PVS query point. Reproduce exactly that
    // latch for the stand-in camera so the streamer's working set follows the view.
    mLastCameraInput.mTransform.Pos() = lPvsPosition;

    Vector3 lForward;
    lForward.x = lLookAt.x - lEye.x;
    lForward.y = lLookAt.y - lEye.y;
    lForward.z = lLookAt.z - lEye.z;
    lForward.w = 0.0f;
    {
        const f32 lfLen = sqrtf( lForward.x * lForward.x + lForward.y * lForward.y
                                 + lForward.z * lForward.z );
        const f32 lfInv = ( lfLen > 0.0001f ) ? ( 1.0f / lfLen ) : 1.0f;
        lForward.x *= lfInv;
        lForward.y *= lfInv;
        lForward.z *= lfInv;
    }
    mLastCameraInput.mTransform.At() = lForward;   // the camera's view-direction row (+0x20)

    // ---- the projection scalars (needed before the camera is framed) --------
    // 60 degrees VERTICAL; the horizontal fov the camera caches is derived from it
    // and the display aspect (tanHalfH == aspect * tanHalfV).
    const f32 lfAspect = ( renderengine::gDisplayHeight > 0 )
        ? ( static_cast< f32 >( renderengine::gDisplayWidth )
            / static_cast< f32 >( renderengine::gDisplayHeight ) )
        : ( 16.0f / 9.0f );
    const f32 lfNear = 0.5f;
    // ⚠ The floor has to clear the SKY DOME. BrnSkyDomeManager pushes the dome's vertices
    // out to KF_SKY_SCALE = 9500 world units from the eye (flt_820473B0, the sky scale in
    // ViewPositionAndSkyScale.w), and this stand-in camera's old 4000-unit floor -- and the
    // radius*4 term, which measured 5665 for the resident world -- put the whole dome
    // BEYOND the far plane, where it is geometrically clipped away whatever depth state the
    // sky pass binds. 12000 clears it with margin. The console's own far plane comes from
    // the director camera, which is not live here.
    const f32 lfFar  = ( lfRadius * 4.0f > 12000.0f ) ? ( lfRadius * 4.0f ) : 12000.0f;
    // 60 degrees vertical for the stand-in tour; when the director drives, its own FOV
    // (BehaviourRoadRunner::Update's SetFOV(95)) is the horizontal one, so the vertical it
    // implies is 2*atan( tan(fovH/2) / aspect ).
    const f32 KF_DEGS_TO_RADS = 0.017453292f;
    f32 lfVerticalFov = 1.0471976f;
    if ( lbUseDirectorCamera && lfDirectorFOVDegs > 1.0f && lfDirectorFOVDegs < 179.0f )
    {
        const f32 lfHalfH = 0.5f * lfDirectorFOVDegs * KF_DEGS_TO_RADS;
        lfVerticalFov = 2.0f * atanf( tanf( lfHalfH ) / lfAspect );
    }
    const f32 lfCotHalfFov = 1.0f / tanf( 0.5f * lfVerticalFov );

    // ---- the view basis: built by the ENGINE'S OWN LookAt -------------------
    // CgsGraphics::Camera::LookAt @0x827F9510 is the AUTHORITY on this engine's camera
    // handedness, and it is not the textbook D3DXMatrixLookAtLH recipe. Transcribed from
    // the asm (fmsub stream @0x827F95A8..0x827F95BC and @0x827F95F0..0x827F9608) its basis
    // columns are
    //     column0 = normalize( cross( dir, up ) )        <-- NOT cross( up, dir )
    //     column1 = normalize( cross( column0, dir ) )
    //     column2 = dir
    // i.e. the screen-x axis is the NEGATIVE of a D3DXMatrixLookAtLH "right" vector, so the
    // engine's view matrix has a NEGATIVE 3x3 determinant by construction and the whole
    // world -- geometry winding, material cull modes, the frustum-plane construction in
    // Camera::GetFrustumPerspective -- is authored against that convention.
    //
    // This stand-in used to build the basis by hand as right = cross( worldUp, forward ),
    // the opposite handedness. That is a MIRROR: the rendered city came out flipped
    // left-for-right, which in turn inverted every triangle's screen-space winding (so the
    // material CullMode had to be read backwards to keep the city solid) and reversed four
    // of the six frustum planes (so they needed an orientation fix-up pass). Both of those
    // compensations are removed with this change.
    //
    // Route the stand-in through the real function so the convention cannot drift again.
    static CgsGraphics::Camera sBringUpCamera;
    sBringUpCamera.Release();                                   // the @0x827F94E8 defaults reset
    sBringUpCamera.maProjectionScalars[ 6 ] = lfAspect;         // m_aspectRatio
    sBringUpCamera.SetFovHorizontal( 2.0f * atanf( lfAspect / lfCotHalfFov ) );
    sBringUpCamera.maProjectionScalars[ 7 ] = lfNear;           // m_nearClipPlane
    sBringUpCamera.maProjectionScalars[ 8 ] = lfFar;            // m_farClipPlane
    {
        const Vector3 lWorldUp = { 0.0f, 1.0f, 0.0f, 0.0f };
        sBringUpCamera.LookAt( lEye, lWorldUp, lLookAt );        // fills mView (+ the f64 basis)
    }

    // Perspective projection (left-handed, D3D depth 0..1), 60 degrees vertical.
    // [FLAG PC-platform leaf] The console's own UpdatePerspectiveProjectionMatrix
    // @0x827EC778 emits the RenderWare/OpenGL depth mapping (z' in [-1,1]:
    // [2][2] = (n+f)/(f-n), [3][2] = -2nf/(f-n)) -- the Xenos can be put in OGL clip
    // space, D3D9 on PC cannot. Only the DEPTH row deviates; x/y are the console's own
    // cot-half-fov scalars, so the frustum built from this camera and the matrix drawn
    // with here describe the same view volume.
    //
    // It is installed ON the camera so the REAL combiner
    // CgsGraphics::Camera::UpdateViewProjectionMatrix @0x827E7030 produces the
    // view-projection. That matters: it is an AFFINE Mult -- the view's fourth COLUMN is
    // the implicit (0,0,0,1), which is why LookAt leaves the w LANE of every view row
    // zero -- so the projection's translation row is added to row 3 unconditionally. A
    // plain 4x4 multiply over these rows would instead scale that row by the stored w
    // lane (0), which loses the -n*f/(f-n) depth bias from the view-projection.
    sBringUpCamera.mProjection.xAxis = Vector4{ lfCotHalfFov / lfAspect, 0.0f, 0.0f, 0.0f };
    sBringUpCamera.mProjection.yAxis = Vector4{ 0.0f, lfCotHalfFov, 0.0f, 0.0f };
    sBringUpCamera.mProjection.zAxis = Vector4{ 0.0f, 0.0f, lfFar / ( lfFar - lfNear ), 1.0f };
    sBringUpCamera.mProjection.wAxis = Vector4{ 0.0f, 0.0f,
                                                -lfNear * lfFar / ( lfFar - lfNear ), 0.0f };
    sBringUpCamera.UpdateViewProjectionMatrix();

    // ROW-VECTOR convention throughout the dispatch path (DrawRenderable::Interpret
    // computes WVP = world * viewProjection and the draw leaf evaluates
    // hpos = pos.x*row0 + pos.y*row1 + pos.z*row2 + row3).
    const Matrix44& lViewProjection = sBringUpCamera.GetViewProjectionMatrix();

    // ---- the camera shader constants the dispatch interpreter reads back -----
    // (the same three ids WorldModule::GenerateDispatchLists publishes: 8 = view
    //  position, 3 = view-projection, 34 = the modified view-projection.)
    mShaderLodInfo.Update();
    CgsGraphics::mShaderConstantTable.SetShaderConstantData( 8, lEye );
    CgsGraphics::mShaderConstantTable.SetShaderConstantData( 3, lViewProjection );

    // [FLAG PC bring-up] Hand the same framing to the renderer's sky pass. The console
    // routes it through WorldModule::SetupShaderConstantsBeforeRendering @0x827D1410;
    // see BrnSkyCameraBringUp in BrnShaderConstantsFrame.h. DELETE with this function.
    gBrnSkyCameraBringUp.mViewProjection = lViewProjection;
    gBrnSkyCameraBringUp.mViewPosition   = lEye;
    gBrnSkyCameraBringUp.mbValid         = true;

    // ViewProjectionModified (34) is NOT the view-projection: the shaders consume it as
    //     hpos.x = dot(worldPos4, VPM[0]);  hpos.y = dot(worldPos4, VPM[1]);
    //     z2     = dot(worldPos4, VPM[2]);
    //     hpos.z = z2 * VPM[3].x + VPM[3].y;   hpos.w = z2 * VPM[3].z + VPM[3].w;
    // (Include/Transform.fxh TransformWorldToProjection; GetViewSpaceDepthFromWorldPosition
    // returns that same z2, i.e. VPM row 2 is the VIEW-DEPTH row, which is what makes fog and
    // the depth encode cheap). So rows 0/1/2 are COLUMNS 0/1/3 of the row-vector
    // view-projection and row 3 is the {zScale, zBias, wScale, wBias} remap that turns the
    // view depth back into clip z/w.
    //
    // [FLAG PC bring-up] The console builds this in CgsGraphics::Camera::
    // GetViewProjectionMatrixModified @0x827EC858 (VMX, not reconstructed). Derived here from
    // the shader contract above for the establishing camera; DELETE with the rest of
    // GenerateDispatchListsBringUp when the real camera + Camera::GetViewProjectionMatrixModified
    // land.
    {
        const f32 lfZScale = lfFar / ( lfFar - lfNear );
        const f32 lfZBias  = -lfNear * lfFar / ( lfFar - lfNear );

        Matrix44 lViewProjectionModified;
        lViewProjectionModified.xAxis = Vector4{ lViewProjection.xAxis.x, lViewProjection.yAxis.x,
                                                 lViewProjection.zAxis.x, lViewProjection.wAxis.x };
        lViewProjectionModified.yAxis = Vector4{ lViewProjection.xAxis.y, lViewProjection.yAxis.y,
                                                 lViewProjection.zAxis.y, lViewProjection.wAxis.y };
        lViewProjectionModified.zAxis = Vector4{ lViewProjection.xAxis.w, lViewProjection.yAxis.w,
                                                 lViewProjection.zAxis.w, lViewProjection.wAxis.w };
        lViewProjectionModified.wAxis = Vector4{ lfZScale, lfZBias, 1.0f, 0.0f };
        CgsGraphics::mShaderConstantTable.SetShaderConstantData( 34, lViewProjectionModified );
    }

    PublishWorldShadingConstantsBringUp();

    // ======================================================================
    // THE REAL DISPATCH FEED. Everything below here is the console path:
    //   InCoarseQueryQueue::FrustumTestVp   (stage the camera query)
    //     -> SceneManagerModule::ProcessFrustumTestJobRequests   @0x828C7628
    //     -> LooseOctree::AddJobFrustumTest / StartFrustumTestJobs
    //     -> SceneManagerModule::ProcessFrustumTestJobResults    @0x828C7838
    //     -> WorldModule::FilterFrustumTestResults               @0x827BDA60
    //     -> WorldEntityModule::GenerateDispatchLists            @0x822D5AB0
    // (the streamer-walking stand-in GenerateDispatchListsFromStreamer is DELETED).
    //
    // [FLAG PC bring-up] What is still the stand-in is only the CAMERA and the IO
    // BUFFER SET: on the console BrnGameModule::DoDispatch @0x823DC458 creates the
    // world-dispatch / renderer / effects buffer pairs on the dispatch stacks, takes
    // the camera from the director module's output and runs
    // WorldModule::GenerateFrustumQueries + ::GenerateDispatchLists over the whole
    // module set. The director publishes no camera on this build and DoDispatch has no
    // buffer stacks, so the four buffers this leg needs are file statics and the camera
    // is the tour camera above. DELETE this block with the rest of the function once
    // DoDispatch is real.
    // ======================================================================
    {
        // The frame's frustum, produced by the REAL console writer
        // CgsGraphics::Camera::GetFrustumPerspective @0x827F0AD8 over the camera framed
        // above (it reads only mView and the cached tan-half-fov / near / far scalars,
        // never mProjection, so the PC depth deviation in the drawn projection cannot
        // reach it). Its six planes come out [N, dot3(N, pointOnPlane)] with N pointing
        // INTO the view volume -- the convention CgsGeometric::Frustum::SetFromRwFrustum
        // and every culling test downstream consume.
        //
        // This used to be a hand-rolled copy of the same construction followed by an
        // orientation fix-up pass that flipped whichever planes came out reversed. That
        // pass existed only because the stand-in built its camera basis with the OPPOSITE
        // handedness to CgsGraphics::Camera::LookAt (see the banner above): the mirrored
        // basis reverses the left/right/top/bottom cross products. With the basis now
        // coming from the engine, the console writer's planes are inward by construction
        // and both the copy and the fix-up are gone.
        CgsGraphics::CameraRwFrustum lRwFrustum;
        sBringUpCamera.GetFrustumPerspective( lRwFrustum, false );

        CgsGeometric::Frustum lFrustum;
        lFrustum.SetFromRwFrustum( lRwFrustum );

        // The four IO buffers the query round trip needs (see the FLAG above).
        static CgsSceneManager::SceneManagerIO::InputBuffer_Query  sQueryInput;
        static CgsSceneManager::SceneManagerIO::OutputBuffer       sQueryOutput;
        static WorldEntityIO::InputBuffer_GenerateDispatchLists    sWorldDispatchInput;
        static RaceCarEntityModuleIO::InputBuffer_GenerateDispatchLists sRaceCarDispatchInput;
        static FilteredEntityData                                  sFilteredEntityData;
        static bool sbBuffersConstructed = false;
        if ( !sbBuffersConstructed )
        {
            sbBuffersConstructed = true;
            sWorldDispatchInput.Construct();
            sRaceCarDispatchInput.Construct();
            sFilteredEntityData.Construct();
        }
        sQueryInput.Construct();
        sQueryOutput.Construct();

        // ---- stage the main camera query (the same event WorldModule::
        //      GenerateFrustumQueries @0x827DADF8 emits for FrustumQuery_MainView) ----
        sQueryInput.LockForWrite();
        {
            u32 luEntityTypeFlags = mbForceOnlyBackdrops ? 0u : 1024u;
            if ( mbRenderBackdrops )
            {
                luEntityTypeFlags |= 0x1000u;
            }
            sQueryInput.GetInCoarseQueryQueue()->FrustumTestVp(
                KA_FRUSTUM_QUERY_IDS[ 0 ], luEntityTypeFlags,
                lFrustum.maSwizzledPlanes, lViewProjection, 0u );
        }
        sQueryInput.UnlockForWrite();

        mSceneModule.ProcessFrustumTestJobRequests( 0, 0, &sQueryInput, &sQueryOutput );
        mSceneModule.ProcessFrustumTestJobResults( 0, 0, &sQueryInput, &sQueryOutput );

        // ---- filter the result into the per-owner id lists ----
        sQueryOutput.LockForRead();
        const CgsSceneManager::SceneManagerIO::OutputBuffer::SceneQueryResultsQueue* lpResultsQueue =
            sQueryOutput.GetSceneQueryResultsQueue();

        const CgsModule::Event* lpFrustumTestResult = 0;
        s32 liResultSize = 0;
        const s32 liResultType = lpResultsQueue->GetFirstEvent( &lpFrustumTestResult, &liResultSize );

        if ( liResultType >= 0 && lpFrustumTestResult != 0 )
        {
            FilterFrustumTestResults( lpFrustumTestResult,
                                      &sFilteredEntityData.maWorldEntityIds,
                                      &sFilteredEntityData.maRaceCarEntityIds,
                                      &sFilteredEntityData.maTrafficEntityIds,
                                      &sFilteredEntityData.maPropEntityIds );

            sWorldDispatchInput.LockForWrite();
            sWorldDispatchInput.GetSceneResultQueue()->Clear();
            sWorldDispatchInput.GetSceneResultQueue()->AddEvent(
                lpFrustumTestResult, liResultType, liResultSize );
            sWorldDispatchInput.SetDispatchFrame( lpDispatchFrame );
            // The consumer dereferences the shadow map unconditionally (it decides the
            // z-only shadow pass from it), so it has to be published even when the
            // shadow pass is off -- the console stages it through
            // BridgeWorldModuleToEntityModules_Render.
            sWorldDispatchInput.SetShadowMap( &mShadowMap );
            sWorldDispatchInput.UnlockForWrite();

            mWorldEntityModule.GenerateDispatchLists(
                &sWorldDispatchInput, sFilteredEntityData.maWorldEntityIds,
                lViewProjection, lEye, lForward, 1.0f, &mShaderLodInfo,
                KI_WORLD_OPAQUE_LIST, KI_WORLD_SORT_LAYER, KI_WORLD_SORT_KEY,
                KI_WORLD_PREZ_LIST, false );
        }

        // THE RACE CAR (pose wave 2026-07-31). This is now the REAL console leg:
        // RaceCarEntityModule::GenerateDispatchLists @0x822E79F8 sweeps its own eight
        // active-race-car slots, and every car whose muState is E_STATE_ACTIVE and whose
        // mbRenderThisFrame is armed goes through RenderRaceCar @0x822CF6A0 with the
        // module's OWN RenderParams -- no locally-owned render snapshot and no
        // camera-relative pose any more.
        //
        // [FLAG PC bring-up] what is still local to this bring-up producer:
        //   * the IO buffer. The console stages this one through
        //     BridgeWorldModuleToEntityModules_Render, which does not exist here; the
        //     static above is the same stand-in the world pass already uses.
        //   * the visible-entity array. GenerateDispatchLists' `else` arm ignores it (it
        //     sweeps maActiveRaceCars instead), so sFilteredEntityData's race-car ids are
        //     passed through unused, exactly as the console's own else arm does.
        //   * fog. The environment settings are not converted on this build, so the
        //     scattering vector is zero -- which makes RenderRaceCar's fog blend exactly
        //     zero, the same state the world pass is already in.
        {
            sRaceCarDispatchInput.LockForWrite();
            sRaceCarDispatchInput.SetDispatchFrame( lpDispatchFrame );
            sRaceCarDispatchInput.SetShadowMap( &mShadowMap );
            sRaceCarDispatchInput.UnlockForWrite();

            mRaceCarEntityModule.GenerateDispatchLists(
                &sRaceCarDispatchInput, sFilteredEntityData.maRaceCarEntityIds,
                KI_RACE_CAR_OBJECT_LIST, KI_RACE_CAR_OPAQUE_MESH_LIST,
                KI_RACE_CAR_TRANSPARENT_MESH_LIST, false,
                Vector4{ 0.0f, 0.0f, 0.0f, 0.0f }, Vector4{ 0.0f, 0.0f, 0.0f, 0.0f },
                lEye );
        }
        sQueryOutput.UnlockForRead();

        {
            // [DIAG culling wave] per-N-frame visibility tally.
            static s32 siDiagFrame = 0;
            static clock_t slDiagStart = 0;
            if ( siDiagFrame == 0 ) { slDiagStart = clock(); }
            if ( ( siDiagFrame++ % 120 ) == 0 && CgsDev::Log::gpDebugPrint != 0 )
            {
                const f32 lfElapsed =
                    static_cast< f32 >( clock() - slDiagStart ) / static_cast< f32 >( CLOCKS_PER_SEC );
                *CgsDev::Log::gpDebugPrint
                    << "[culling-diag] frame " << siDiagFrame
                    << " t=" << lfElapsed
                    << " producerFps=" << ( lfElapsed > 0.01f ? ( siDiagFrame / lfElapsed ) : 0.0f )
                    << " eye=(" << lEye.x << "," << lEye.y << "," << lEye.z
                    << ") visibleWorld="
                    << static_cast< s32 >( sFilteredEntityData.maWorldEntityIds.GetLength() )
                    << " list11=" << static_cast< s32 >(
                           lpDispatchFrame->GetList( KI_WORLD_OPAQUE_LIST )->GetCount() )
                    << "\n";
            }

            static bool sbLogged = false;
            if ( !sbLogged && CgsDev::Log::gpDebugPrint != 0 )
            {
                sbLogged = true;
                *CgsDev::Log::gpDebugPrint
                    << "[culling] real frustum producer live: centre ("
                    << lCentre.x << ", " << lCentre.y << ", " << lCentre.z
                    << ") radius " << lfRadius << " -- visible world entities "
                    << static_cast< s32 >( sFilteredEntityData.maWorldEntityIds.GetLength() )
                    << ", object list " << static_cast< s32 >( KI_WORLD_OPAQUE_LIST )
                    << " count "
                    << static_cast< s32 >( lpDispatchFrame->GetList( KI_WORLD_OPAQUE_LIST )->GetCount() )
                    << "\n";
            }
        }
    }
}

}
