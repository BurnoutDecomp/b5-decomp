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
#include "pc/gcm/renderengine/GraphicsDiagnostics.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "pc/gcm/renderengine/reflections/EnvironmentMap.h"
#include "pc/gcm/renderengine/reflections/SceneCapture.h"
#include "pc/gcm/renderengine/reflections/SceneSettings.h"
#include "pc/gcm/renderengine/reflections/SceneRender.h"
#include <chrono>  // [DIAG shadow-perf wave] steady_clock for the per-phase producer timers
#include <cstdlib>                                                // getenv/atof (the BRN_WORLD_CAMDIST bring-up diagnostic)
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"   // CgsGraphics::ShaderConstantTable
#include "GameSource/Graphics/BrnShaderConstantsFrame.h"             // BrnShaderConstantsFrame
#include "GameShared/GameClasses/Module/CgsModuleUtils.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/Jobs/Traffic/BrnTrafficSwerveWatch.h"   // [DIAG] BRN_WORLD_CAMTRAFFIC
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO_OutputBuffer.h"
#include "GameSource/Physics/BrnPhysicsModuleIO.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
// ADDED 2026-08-27 (showtime S3 wave): HandleGameActions' case-23 arm reads the PrepareForModeAction
// record BY NAME now instead of by X360 word index -- see the banner at that arm.
#include "GameSource/GameState/BrnGameActions.h"             // PrepareForModeAction + GameModeParams
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // the SCENE-stage allocator-hold one-shot log
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysSharedIO.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h" // Attrib::StringToKey
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
#include "pc/debug/RaceCarControls.h"

#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"   // CgsSystem::TimerStatus{,Interface} -- WorldModule::Update stages the environment frame delta

#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // VariableEventQueue<4096,16>::AddEvent
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"       // CgsResource::ID::HashString
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"       // BrnResource::GameDataIO::RequestInterface<4096>

#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"     // DispatchFrame / DispatchList
#include "rw/math/vpu/vector3_operation.h"   // rw::math::vpu::operator- / Magnitude (CalculateVehicleLODs)
#include "GameSource/Director/Camera/Utils/CameraUtils.h" // Utils::GetZoomFromFOVDegs (the bring-up LOD zoom)
#include "GameSource/Director/Camera/Camera.h"      // BrnDirector::Camera::KF_DEFAULT_NEAR_CLIP_DISTANCE
#include "GameShared/GameClasses/System/Timer/CgsFrameInterpolation.h" // ⚠️ FLAG PC QoL: GetFrameSeconds (the tour camera's advance)
#include <cmath>    // sqrtf / tanf ([FLAG PC bring-up] the dispatch producer's camera) + std::sqrt (vehicle LODs)
#include <cstddef>  // offsetof (the VehicleRenderInfo layout pins)

// includes folded in from the BrnWorldModule_w*.cpp partfiles (2026-09-15)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTest.h"    // InEventLineTestFine
#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerEntityModuleIO.h"
#include "GameSource/World/BrnWorldModuleIO.h"                                      // BrnWorldIO::UpdateOutputBuffer
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO.h"                       // BrnWorld::CrashIO::OutputBuffer_PostPhysics
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficGuiInterface.h" // ScoringVehicleArray
#include "GameShared/GameClasses/Containers/CgsArray.h"                                   // Array<short,25> (the GUI record's byte image)
#include "GameSource/World/AI/BrnAIHarnessPad.h"                                          // [PC HARNESS] BRN_AI_PAD_PLAYER (gHarnessAIPad)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.h"     // [PC HARNESS] the AI record queue (HarnessStashAIPadControls)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"           // [PC HARNESS] BrnAIDriverControls
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnPlayerVehicleControls.h" // [PC HARNESS] BrnWorld::PlayerVehicleControls
#include <cstring>                                                                        // [PC HARNESS] std::strcmp (BRN_AI_PAD_PLAYER)

// The global runtime shader-constant register (X360 symbol mShaderConstantTable;
// same extern as the world-entity TU -- the defining home lands with the shader TU).
namespace CgsGraphics { extern ShaderConstantTable mShaderConstantTable; }

// [FLAG PC bring-up] the PC back-buffer extent (pc/gcm/renderengine/device.h). Declared
// here rather than included: that header pulls <windows.h>/<d3d9.h>, which must not enter
// this TU. Only the aspect ratio of the bring-up dispatch camera reads them.
namespace renderengine { extern s32 gDisplayWidth; extern s32 gDisplayHeight; }
// [FLAG PC bring-up] the config.ini [Settings] EnvironmentMap knob (pc/gcm/renderengine/
// device.h, reflections step 1). Declared for the same <windows.h> reason. It is the PC seed
// of the console's RendererIO::RenderSwitches::mbRenderEnvironmentMap, which gates BOTH the
// renderer's face pass and this producer's env-map arm (::GenerateDispatchLists :4003) --
// so the producer reads the same seed the renderer does; verify finding F5 (envproducer):
// with the knob off, the six queries and dispatch legs must not run either.
namespace renderengine { extern s32 gEnvironmentMap; }

namespace renderengine { extern u32 guPresentCount; extern bool gbDiagLastPresentBlack; }   // [DIAG] issue #30 (device.cpp)

namespace BrnWorld
{

static void RegisterVehicleLodDebugVariablesPC();

// qword_8300E9B8: X360 static initializer @0x82C6A9D8 hashes this exact text;
// WorldModule::Prepare loads the resulting 64-bit key at @0x827D5B6C.
static const u64 gs_uSurfaceListKey = Attrib::StringToKey("340654");

// The eleven scene-query ids this module stamps its coarse frustum queries with, read out of
// the retail data image. Slot 0 is the main view, slots 2..7 the six environment-map faces and
// slots 8..10 the three shadow cascades; slot 1 is present in the table but has no emitter.
// The ids are what the scene manager copies onto each result batch, so they are the ONLY thing
// that keeps this module's results apart from every other module's -- they must stay distinct
// from the traffic module's query ids (a different, much lower range) and from each other.
static CgsSceneManager::SceneQueryId KA_FRUSTUM_QUERY_IDS[11] =
{
    { 0xFF000000u },    // main view
    { 0xFF000001u },    // (no emitter)
    { 0xFF000002u },    // env-map face 0
    { 0xFF000003u },    // env-map face 1
    { 0xFF000004u },    // env-map face 2
    { 0xFF000005u },    // env-map face 3
    { 0xFF000006u },    // env-map face 4
    { 0xFF000007u },    // env-map face 5
    { 0xFF000008u },    // shadow cascade 0
    { 0xFF000009u },    // shadow cascade 1
    { 0xFF00000Au },    // shadow cascade 2
};
static CgsGraphics::Camera gFrustumQueryCamera;

// ShadowMap::GetFrustum -- the per-cascade cull volume this file's shadow queries submit --
// now lives in its canonical home next to GetCascadeCamera (BrnShadowMap.cpp), relocated
// there by the conductor once the concurrent ShadowMap wave released that file.

// The dispatch-pass camera (X360 file static at 0x8300FB40).
static CgsGraphics::Camera gDispatchCamera;



// ARTIST GenerateDispatchLists @827D28C8..28F4: r9=11 (object list),
// r10=11 (opaque mesh list), stack=15 (transparent), stack=21 (pre-Z).
// Object list 2 belongs to near traffic casters. Sharing it with the main
// world pass skips the traffic list's initial full constant snapshot, leaving
// its body meshes with the main camera until the next 128-object refresh.
static const s32 KI_WORLD_OBJECT_LIST = 11;  // GDL object list
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
                // Clear the queue and push an AcquireResourceRequest (event type 4) acquiring the
                // loaded "Districts" resource from pool 5. The X360 builds the record on the
                // stack as { &mReceiverQueue, 1, 5 (pool), HashString("Districts") } and pushes
                // it via VariableEventQueue<4096,16>::AddEvent(record, type=4, size=24).
                mReceiverQueue.Clear();
                BrnResource::GameDataIO::RequestInterface<4096>* lpRequest =
                    lpOutput->GetResourceRequestResourceInterface();

                // ⭐ BY-MEMBER REQUEST BIND (2026-08-11, district-map wave). This block used to
                // build a LOCAL struct laid out at the CONSOLE's 32-bit offsets
                // ({queue@+0, id@+4, pool@+8, pad@+12, u64 resourceId@+16}) and post it with the
                // console's literal 24-byte size. On the x64 host that record is a DIFFERENT
                // shape (the leading pointer is 8 bytes, so miEventId slides to +8, miPoolId to
                // +12 and the u64 to +24) while the pool that consumes it --
                // PoolModule::DoAcquireResourceRequest -- reads a real
                // CgsResource::Events::AcquireResourceRequest BY NAME (mResourceId at +16 on
                // x64). Net effect on PC: the pool read the padding word as the resource id and
                // the 24-byte post truncated the id away entirely, so FindResource could never
                // match "Districts" and the acquire always resolved to a NULL handle.
                //
                // CONSOLE attestation (asm 0x827D12DC..0x827D1334, the case-2 block):
                //   stw r30, var_40 (+0)   = &mReceiverQueue
                //   stw r10(1), var_3C (+4)  = miEventId
                //   stw r10(5), var_38 (+8)  = miPoolId
                //   std r11,  var_30 (+16)   = the RAW HashString("Districts") return
                //   li r5,4 / li r6,0x18 -> AddEvent(type 4, size 24 == the 32-bit sizeof)
                // The id is UNTAGGED: HashString @0x828D84A8 ends `clrldi r3,32`, so the high
                // dword is zero -- the `| 0x500000000` Hex-Rays shows is a fusion artifact of the
                // separate `li r10,5 / stw @+8` miPoolId store (same artifact as the
                // `| 0x700000000` already corrected in LoadAttribSysVault below).
                CgsResource::Events::AcquireResourceRequest lRequest;
                lRequest.mpUser    = &mReceiverQueue;
                lRequest.miEventId = 1;
                lRequest.miPoolId  = 5;
                lRequest.mResourceId.SetHash(
                    static_cast<u64>(static_cast<u32>(CgsResource::ID::HashString(
                        reinterpret_cast<const u8*>("Districts")))));   // untagged (high dword 0)
                lRequest.mbCheckRefCount = false;

                // The request interface's queue IS a VariableEventQueue<4096,16> (RequestQueue
                // <4096> -> ResourceRequestQueue<4096> -> VariableEventQueue<4096,16>); the X360
                // pushes the acquire event straight onto it via the 3-arg AddEvent. The X360
                // literal size 24 is its 32-bit sizeof(AcquireResourceRequest); the host record
                // is wider and the consumer reads it back by NAME, so post sizeof() (same
                // convention as RaceCarEntityModule::LoadGlobalResources and every committed
                // GameDataModule request).
                lpRequest->mRequestQueue.AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lRequest), /*liType*/ 4,
                    static_cast<s32>(sizeof(lRequest)));

                meDistrictMapLoadStage = E_DISTRICT_MAP_ACQUIRE_RESPONSE;
                lpOutput->UnlockForWrite();
                return false;
            }

            case E_DISTRICT_MAP_ACQUIRE_RESPONSE:
            {
                // Wait for the acquire response, then capture the resolved resource handle from
                // the first response event.
                if (mReceiverQueue.GetCount() <= 0)
                {
                    lpOutput->UnlockForWrite();
                    return false;
                }
                meDistrictMapLoadStage = E_DISTRICT_MAP_DONE;

                // ⭐ BY-MEMBER HANDLE BIND (2026-08-11, district-map wave). This used to read
                // TWO u32s at a raw payload +24 and widen them into pointers -- the CONSOLE's
                // 32-bit record layout applied to the x64 host, which produces a truncated /
                // garbage handle (the host's response is wider and its handle members are 8-byte
                // pointers, so nothing lives at +24/+28 any more).
                //
                // CONSOLE attestation (pseudocode/asm 0x827D11D8, case 3):
                //   v7 = (count>0) ? mpBuffer + miStartOffset + 8 : 0   // == GetFirstEvent's payload
                //   v8 = v7 + 24;  a1[1541858] = *v8;  a1[1541859] = v8[1];
                // 1541858*4 / 1541859*4 are mDistrictMapResourceHandle.mpResourceMemory /
                // .mpSourceEntry. The record at payload +0x18 IS the AcquireResourceResponse's
                // {mpResourceMemory, mpSourceEntry} pair: PoolModule::DoAcquireResourceRequest
                // @0x828FCD48 builds a 32-byte reply {mpUser@0, miEventId@4, miPoolId@8,
                // mResourceId@16, handle pair@24} and posts it with AddEvent(tag 6, 32). So it is
                // read BY MEMBER off the real response type -- never at the console's literal
                // +0x18/+0x1C, because the host handle pair starts past a wider PoolEvent base.
                // Same idiom as LoadAttribSysVault below, StreetManager::LoadDistrictMap and
                // RaceCarEntityModule::LoadGlobalResources.
                const CgsModule::Event* lpEventData = 0;
                s32 liSize = 0;
                mReceiverQueue.GetFirstEvent(&lpEventData, &liSize);

                if (lpEventData != 0)
                {
                    // reinterpret_cast, not static_cast: CgsResource::Events::Event and
                    // CgsModule::Event are unrelated roots and the receiver queue hands out the
                    // module one.
                    const CgsResource::Events::AcquireResourceResponse* lpResponse =
                        reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>(lpEventData);

                    mDistrictMapResourceHandle.mpResourceMemory = lpResponse->mpResourceMemory;
                    mDistrictMapResourceHandle.mpSourceEntry    = lpResponse->mpSourceEntry;
                }

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
    // ⚠ READ THIS BEFORE COSTING THE ENV-MAP PASS. mb30hzEnvironmentMap ships FALSE and
    // NOTHING else in the image writes it (grep below), so GenerateFrustumQueries'
    // schedule at :3482 takes the `!mb30hzEnvironmentMap` arm EVERY frame and all SIX
    // faces refresh every frame -- the "three faces per frame at 30 Hz" half-schedule is
    // the OFF-by-default debug path, not the shipped one.
    //   $ grep -n "mb30hzEnvironmentMap" b5-decomp/src/GameSource/World/BrnWorldModule.cpp
    //   520:    mb30hzEnvironmentMap = false;                   // X360 +6167328
    //   3482:    if ( !mb30hzEnvironmentMap || mbFirstRenderFrame )
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

    RegisterVehicleLodDebugVariablesPC();

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
// A pointer-adjusting tail-jump onto the scene module's virtual slot 17, so the
// target receives the caller's first four arguments verbatim. The fifth (the frame
// update set) is not consumed by the target and stops here.
void
WorldModule::ExternalSceneQueriesUpdate(
    CgsModule::IOBufferStack* lpInputBufferStack,
    CgsModule::IOBufferStack* lpOutputBufferStack,
    CgsSceneManager::SceneManagerIO::InputBuffer_Query* lpQueryInput,
    CgsSceneManager::SceneManagerIO::OutputBuffer* lpQueryOutput,
    BrnUpdateSet /*lUpdateSet*/ )
{
    mSceneModule.ExternalSceneQueriesUpdate( lpInputBufferStack, lpOutputBufferStack,
                                             lpQueryInput, lpQueryOutput );
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
            // (CreateIOBuffer<T> runs each buffer's own Construct after the stack alloc,
            //  exactly as the X360 instantiations do -- no hand Construct needed here.)

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
            // (CreateIOBuffer<T> ran OutputBuffer_Prepare::Construct -- X360 instantiation
            //  @0x827B5BA0. FLAG: the PC body is still a minimal slice, and its module
            //  Prepare is boot-gated.)

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

            // Breaker @0x827D5B60..0x827D5B70: r3 = this->mPhysicsModule.mVehicleManager,
            // r4 = qword_8300E9B8 (StringToKey("340654")). The old PC call targeted an
            // invented no-argument stub, leaving gbReadSurfaceProperties false.
            mPhysicsModule.mVehicleManager.ReadSurfaceProperties(gs_uSurfaceListKey);

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
            // (CreateIOBuffer<T> itself runs PropEntityIO::OutputBuffer_Prepare::Construct
            //  @0x822EFC58 -- X360 instantiation @0x827B5D48. That Construct sets the base
            //  status byte itself (`stb 1, 0(this)` is its first instruction), so no hand
            //  IOBuffer::Construct belongs here either.)

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

                mSceneModule.UpdateScene( lpInputBufferStack, lpOutputBufferStack,
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

            // ⭐ GATE RETIRED 2026-08-25 (crash exit). The gate's own text named the defect and it
            // was read as a reason to skip: "the call resolves to the BASE
            // ModuleSingleBuffered::Prepare". It did -- because BrnCrashModule.h declared NO
            // lifecycle at all, so `mCrashModule.Construct()` at line 505 also bound to the base
            // and the module's tunables never left zero (mbClearUpEnabled == 0 alone made the
            // crash countdown unreachable). BrnCrashModule_Lifecycle.cpp lands the real
            // Construct/Prepare/Release/Reset; Construct sets Module::mbIsNewModule, which is what
            // makes every data-structure arm of the base Prepare skip itself. Skipping Prepare was
            // never the fix -- landing Construct is.
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

    // (CreateIOBuffer<T> ran each buffer's own Construct; the hand restoration this trio
    //  used to carry is gone -- see CgsIOBufferStack.h.)

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
                // ⚠️ BANNER CORRECTED TWICE. Read the history before touching this.
                // (2026-08-01) It first said the index is "ALWAYS -1" because
                //   WorldModule::BridgeRaceCarModuleToWorldModule_PreScene "is not reconstructed".
                //   That bridge then landed and was MOUNTED (WorldBridgeRaceCarToWorldModule.cpp,
                //   build_game_exe.bat), so the banner was rewritten to blame a harmless
                //   "ONE-FRAME TRANSIENT at the slot-0 -> slot-1 car swap".
                // ⛔ (2026-08-11) THAT SECOND STORY WAS ALSO WRONG, and it cost a wave. The
                //   mounted bridge was writing the index through the X360 BYTE OFFSET +6167272
                //   applied to the x64 PC object; the real member is at PC offset 6234776
                //   (compile-time offsetof probe, this build), so meLocalPlayerActiveRaceCarIndex
                //   was NEVER written and stayed at Prepare's -1 for the whole session -- exactly
                //   the original "always -1", just re-hidden behind a mount. The bridge's own
                //   one-shot "player active race-car index published = 0" diag printed the SOURCE
                //   interface value, not this member, which is why it read as proof. The bridge is
                //   now a WorldModule METHOD and writes both outputs BY NAME; see its banner.
                // KEEP THE GUARD: it is PC hardening for the console's unbounded index, not a
                //   placeholder. If this assert fires again it is a REAL producer failure -- check
                //   that the pre-scene bridge ran and that IsPlayerCarActive() was true; do NOT
                //   write another "harmless transient" banner.
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
                // ⛔⛔ FIXED 2026-08-27 (showtime S3 wave) -- THIS ARM WAS READING OFF THE END OF
                // THE RECORD, AND HAD BEEN SINCE IT WAS WRITTEN.
                // It used to be:
                //     if ( lpiPayload[0] == 0 || lpiPayload[0] == 1 ) {
                //         liControl = ( lpiPayload[549] & 0x10000 ) ? 2
                //                   : ( lpiPayload[94] == 15 ) ? 1 : 0;
                // -- word indices transcribed straight off the X360 (549*4 == 2196 == the flag
                // word, 94*4 == 376 == the mode-type word). Those displacements are correct FOR THE
                // CONSOLE RECORD, which is 0x8E0 == 2272 bytes. The host `PrepareForModeAction` is a
                // fully typed struct and measures **1792** bytes on this build (printed by the
                // [s3-action] witness in BrnPhysicsModuleGameActions.cpp, which is how this was
                // found): +2196 is 404 bytes PAST THE END of the object -- an out-of-bounds read
                // whose result was reliably 0, i.e. "no AI control", the most plausible-looking
                // wrong answer available. +376 landed mid-member and could never equal 15 except by
                // accident. So EVERY car's control policy was being derived from garbage, silently,
                // on every mode prepare. No gate could see it: the read is in-bounds as far as the
                // compiler is concerned and the answer is a legal enum value.
                // [[serialized-slots-stay-32-bit]] -- host layout is NOT console layout.
                //
                // Both fields now come through the same accessors
                // RaceCarEntityModule::HandlePrepareForModeAction uses, and the two magic numbers
                // are named: 0x10000 is KU_FLAG_SET_ALL_CARS_TO_STARTING_AI_CONTROL
                // (BrnGameModeParams.h:154) and 15 is E_MODE_ONLINE_FREEBURN_LOBBY. The `lpiPayload[0]`
                // guard was the one read that WAS right (mePrepareForModeStage is the first member);
                // it is spelled as IsFirstPrepareForMode(), whose own banner derives it from the
                // same `cmpwi 0 / cmpwi 1` pair.
                const BrnGameState::GameStateModuleIO::PrepareForModeAction* const lpPFMAction =
                    reinterpret_cast<
                        const BrnGameState::GameStateModuleIO::PrepareForModeAction*>( lpEventData );

                if ( lpPFMAction->IsFirstPrepareForMode() )
                {
                    const BrnGameState::GameModeParams* const lpModeParams =
                        lpPFMAction->GetGameModeParams();
                    CGS_ASSERT( lpModeParams, "lpModeParams" );

                    s32 liControl;
                    if ( lpModeParams->GetFlag(
                             BrnGameState::GameModeParams::KU_FLAG_SET_ALL_CARS_TO_STARTING_AI_CONTROL ) )
                    {
                        liControl = 2;
                    }
                    else
                    {
                        liControl =
                            ( static_cast<s32>( lpModeParams->GetGameModeType() ) == 15 ) ? 1 : 0;
                    }

                    for ( s32 liI = 0; liI < 8; liI++ )
                    {
                        maeCarControls[ liI ] = liControl;
                    }

                    // NOT X360. One line, once: this arm produced a wrong answer for its whole life
                    // and the only reason anyone noticed was a witness that printed a VALUE.
                    // DELETE-WHEN: the mode-prepare chain has been exercised in anger and the
                    // control policy is confirmed against a real online/offline mode.
                    {
                        static bool sbLogged = false;
                        if ( !sbLogged && CgsDev::Log::gpDebugPrint != 0 )
                        {
                            sbLogged = true;
                            *CgsDev::Log::gpDebugPrint
                                << "[s3-action] WorldModule case 23: modeType "
                                << static_cast<s32>( lpModeParams->GetGameModeType() )
                                << " startAI "
                                << ( lpModeParams->GetFlag(
                                         BrnGameState::GameModeParams::
                                             KU_FLAG_SET_ALL_CARS_TO_STARTING_AI_CONTROL ) ? 1 : 0 )
                                << " -> maeCarControls[0..7] = " << liControl << "\n";
                        }
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
    // [crash exit 2026-08-25] was `const CrashModuleIO::OutputBuffer_PostScene*` -- a phantom
    // type. This is the crash module's ONE output buffer; the caller below used to
    // reinterpret_cast the real thing into the phantom to satisfy this signature.
    const CrashIO::OutputBuffer_PreScene* lpCrashOutputBuffer_PostScene,
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
    // A WorldModule METHOD (DWARF BrnWorldModule.h:473), not a `void* lpWorldModule`
    // namespace bridge -- see its banner in Bridges/WorldBridgeRaceCarToWorldModule.cpp.
    BridgeRaceCarModuleToWorldModule_PreScene(
        lpWorldInputBuffer_PreScene, lpRaceCarOutputBuffer_PreScene );
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
    // ⭐ 2026-08-15 (IO-buffer zero-fill removal audit): the sixteen hand-written
    // `lp*->Construct();` calls that used to stand here are DELETED. They were the PC
    // work-around for a CreateIOBuffer<T> that only placement-new'd; the template is now the
    // console's own (CgsIOBufferStack.h -- `new (mem) T` then `T::Construct()`), so each buffer
    // is already Constructed by the call above it and the hand calls were a second Construct on
    // an already-constructed buffer. (One of them was even doubled --
    // `lpPhysicsOutput->Construct();` appeared twice -- which is what a manual list of sixteen
    // gets you. lpPhysicsInput never had one at all, and now does not need one.) Nothing between
    // the creates and here depends on them: the buffers are only read/written further down,
    // after their Lock*/bridge calls.

    // ---- the frame's triangle-cache collision generator --------------------
    // ONE carve from the per-frame world allocator: the generator object at the
    // base + its 0x40000-byte collision result region. The console literal is
    // 336896 == 74752 + 0x40000, and 74752 == 0x12400 is precisely the X360
    // sizeof(BaseCollisionGenerator).
    // ⚠️⚠️ The generator's size is a `sizeof`, not the console byte literal. THIS
    // OBJECT IS CARVED AT RUNTIME, NOT DESERIALISED, so on x64 every one of its
    // pointers widens (64 embedded CollisionBatch, each holding an EA::Jobs::Job, plus
    // a 200-entry pointer array) and it is materially LARGER than 74752 bytes. Its
    // Prepare placement-constructs all 64 batches, so a console-sized carve would walk
    // straight off the end of the object and into the result region it is about to
    // hand the bump allocator.
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
    // [FLAG PC] The null guard is host-only and now dead: UpdateInputBuffer::
    // GetPlayerVehicleControls (BrnWorldModuleIO.cpp) returns the buffer's own member,
    // never NULL. The console's setter is an unguarded 60-byte memcpy.
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
    // [PC HARNESS, NOT X360] BRN_AI_PAD_PLAYER -- the AI's player-seat decision of the previous frame
    // replaces the real pad's driving channels in the race-car pre-scene input the bridge above just
    // filled (still write-locked). Inert without the variable; see HarnessApplyAIPad (end of file).
    HarnessApplyAIPad( lpRaceCarInput_PreScene, lpUpdateInputBuffer );
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
        // The CONST read-lock accessor (X360 0x8279FD58-family, baked :185) -- the buffer is
        // read-locked here, and the non-const twin asserts 'Not locked for writing' (70 asserts
        // per boot the moment the accessor became real, 2026-08-19 wave Q6). The null test is
        // kept: the accessor is real now but the snapshot copy is unchanged.
        const BrnTraffic::BrnTrafficIO::OutputBuffer_PreScene* lpTrafficOutput_PreSceneRead =
            lpTrafficOutput_PreScene;
        const BrnTraffic::BrnTrafficIO::OutputBuffer_PreScene::TrafficToRaceCarInterface_PreScene*
            lpTrafficToRaceCar = lpTrafficOutput_PreSceneRead->GetTrafficToRaceCarInterface_PreScene();
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
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_World );

    RaceCarEntityModuleIO::InputBuffer_PostScene* lpRaceCarInput_PostScene = 0;
    PropEntityIO::InputBuffer_PostScene*          lpPropInput_PostScene    = 0;
    lpInputBufferStack->CreateIOBuffer( &lpRaceCarInput_PostScene, "RaceCarPostScene" );
    lpInputBufferStack->CreateIOBuffer( &lpPropInput_PostScene, "PropPostScene" );

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
        // ⭐ [crash exit 2026-08-25] THE CAST IS GONE. It used to launder the real
        // CrashIO::OutputBuffer_PreScene into the phantom CrashModuleIO::OutputBuffer_PostScene
        // -- i.e. this build had ALREADY worked out that the post-scene crash bridges read the
        // pre-scene output buffer (the console passes this same local in argument slot 38), and
        // then hid that fact behind a reinterpret_cast. The cast is exactly what made the
        // buffer's RaceCarCrashCompleteEvent ring "unreachable by name".
        lpCrashOutput_PreScene,
        lpPropInput_PostScene, lpPropOutput_PostScene,
        lUpdateSet );
    lpInputBufferStack->DestroyIOBuffer( &lpPropInput_PostScene );
    lpInputBufferStack->DestroyIOBuffer( &lpRaceCarInput_PostScene );

    // ---- AI update -----------------------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_NetworkAIRaceCar );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_AI );

    BrnAI::AIModuleIO::InputBuffer* lpAIInput = 0;
    lpInputBufferStack->CreateIOBuffer( &lpAIInput, "AIInput" );

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

    // [PC HARNESS, NOT X360] BRN_AI_DRIVES_PLAYER -- see HarnessArmAIDrivesPlayer (end of file).
    // Placed BEFORE the refresh below so the frame that arms it already runs the AI-owned path.
    HarnessArmAIDrivesPlayer();
    // [PC HARNESS, NOT X360] BRN_AI_PAD_PLAYER -- the AI PAD seat's standing policy for THIS frame's AI
    // update (gHarnessAIPad); the control word is never touched. See HarnessArmAIPadPlayer (end of file).
    HarnessArmAIPadPlayer();

    // ARTIST 0x827D74FC..0x827D753C refreshes ownership before StoreDrivenCarData.
    // The management event resets the route/PID once; this latch keeps subsequent
    // frames from overwriting the handoff with "human driving" again.
    mAIModule.SetAIDrivesPlayer(meLocalPlayerActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID &&
        GetCarControl(meLocalPlayerActiveRaceCarIndex) == E_CAR_CONTROL_AI_MODULE);

    // ARTIST 0x827D750C..0x827D7540: AIModule::SetCamera inlined as Camera::operator=(this +
    // 0x5DFD00 == mAIModule.mCamera, this + 0x5E1CC0 == mLastCameraInput), before the AIModule
    // Update vtable call. ResetOnTrackManager::PlayerIsLookingBackwards reads it (crash parity
    // G05-D3, 2026-09-22).
    mAIModule.SetCamera(mLastCameraInput);

    PerfMonCpu::StartMonitor( miAIModuleUpdatePM );
    // X360 (*(vtbl(mAIModule) + 68)) == Update; devirtualised.
    mAIModule.Update( lpInputBufferStack, lpOutputBufferStack, lpAIInput, lpAIOutput,
                      lUpdateSet );
    PerfMonCpu::StopMonitor( miAIModuleUpdatePM );

    // [PC HARNESS, NOT X360] BRN_AI_PAD_PLAYER -- copy the AI's player-slot record out of this frame's
    // AI output for next frame's pad (HarnessApplyAIPad). Inert without the variable.
    HarnessStashAIPadControls( lpAIOutput );

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

    // [FLAG PC boot gate -- FRAME DELTA + TIME-OF-DAY OVERRIDE RESTORED; post-fx step 9
    //  (group envblend) then DMV look-dev wave 2026-08-20 (group timeofday)]
    // X360 WorldModule::Update @0x827D63E8, 0x827D7CEC-0x827D7D78.
    //
    // RESTORED -- the environment frame delta. `stfsx f0, r31, 0x1E8124` is
    // mEnvironmentManager + 0x11C4 == EnvironmentManager::mrTimeStep (DWARF
    // BrnEnvironmentManager.h:425; the console inlines the setter DWARF :107 names
    // SetCurrentTimeStep). Source: the world input's SIM timer status --
    //     if (status[+0x24]) mrTimeStep = status[+0x20] * status[+0x1C]; else mrTimeStep = 0
    // and +0x18 is where the 48-byte block's SECOND CgsSystem::TimerStatus starts, so
    // +0x1C/+0x20/+0x24 are its mfBaseTimeStep / mfTimeStepMultiplier / mbRunning --
    // i.e. `IsRunning() ? GetCurrentTimeStep() : 0`, reached BY NAME below.
    //
    // WHY IT IS LOAD-BEARING: mrTimeStep is the ONLY frame delta the environment manager
    // has. EnvironmentManager::SetupTimeOfDayBlend @0x827D35C0 advances the time of day by
    // `mfTimeOfDayDelta * mrTimeStep` and Update @0x827D6060 scrolls the cloud UVs by it.
    // Left at its zero-initialised value the whole environment chain is arithmetically
    // inert -- the sky would sit at Construct's 13:00 for ever, which reads on screen as
    // "the time of day never moves" rather than as a crash.
    //
    // ⭐ RESTORED 2026-08-20 (DMV look-dev wave, group timeofday) -- THE DIRECTOR-CAMERA
    // TIME-OF-DAY OVERRIDE. The old park here said "the committed Director camera slice
    // still has no named home for those two fields". That was STALE: BrnCameraEffects.h
    // carved mfTimeOfDay (+0x98) on 2026-07-31 and mbSetTimeOfDay (+0xB9) beside it, both
    // DWARF-named (BrnCameraEffects.h:311 / :329). The arm is the console's, verbatim:
    //
    //   0x827D7CEC  lbzx  r11, r31, 0x5E1DE1        ; mLastCameraInput.mEffects.mbSetTimeOfDay
    //   0x827D7CFC  beq   -> skip
    //   0x827D7D10  lfsx  f13, r31, 0x5E1DC0        ; ...mEffects.mfTimeOfDay  (HOURS)
    //   0x827D7D18  lfs   f0, flt_82004C6C          ; 60.0  (image bytes 42 70 00 00, dumped
    //                                               ;        headless from ARTIST, .rdata)
    //   0x827D7D1C  fmuls f13, f13, f0              ; hours * 60          -> minutes
    //   0x827D7D20  fmuls f0,  f13, f0              ; minutes * 60        -> SECONDS
    //   0x827D7D24  stfsx f0,  r31, 0x1E7464        ; mEnvironmentManager.mfTimeOfDay
    //
    // NOTE THE SHAPE: two separate `fmuls` by the SAME 60.0 constant, i.e. the source wrote
    // `* 60.0f * 60.0f`, not `* 3600.0f` -- reproduced literally below so the rounding is
    // identical. mLastCameraInput sits at WorldModule +0x5E1CC0 (pinned by the
    // `lvx128 v1, r31, 0x5E1CF0` at 0x827D7D94 = camera +0x30 = GetPosition(), the third
    // argument of the EnvironmentManager::Update call just below), so +0x5E1DC0/+0x5E1DE1
    // are camera +0x100/+0x121 == mEffects +0x98/+0xB9. Reached BY NAME here, because those
    // console offsets do not survive the x64 pointer widening in Camera (three 4->8 byte
    // pointer members precede mEffects).
    //
    // THERE IS NO CLAMP. The store is a bare `stfsx` -- no fsel, no compare against
    // mfTimeOfDayLowerBound/UpperBound (28800/61200, flt_820CC768/flt_820CAB98). The bounds
    // are applied one step later, by SetupTimeOfDayBlend inside EnvironmentManager::Update.
    //
    // ORDER IS LOAD-BEARING and is the console's: AFTER mSkyDebugComponent.Update() (the
    // `bctrl` at 0x827D7CE8), BEFORE the SetCurrentTimeStep block below (0x827D7D28) and
    // before EnvironmentManager::Update (0x827D7DA0) -- so the override lands in the same
    // frame it is requested, and SetupTimeOfDayBlend advances from the overridden value.
    //
    // WHY IT MATTERS FOR THE DMV BUG. Construct seeds KF_DEF_TIME_OF_DAY = 46800 s
    // (flt_820CA580, image bytes 47 36 D0 00 == 46800.0 -- the default is NOT misdecoded)
    // and only two console mechanisms ever move mfTimeOfDay off it: THIS override, and the
    // junkyard-lighting latch (EnableJunkyardLightingSetup pins 18:00 = flt_82F307F0 and
    // EnvironmentManager::Update re-pins it every frame while latched, asm 0x827D6358 --
    // and that re-pin runs AFTER this override, so the latch wins when both are live).
    // The one console producer of THIS request, BrnDirector::ArbStateCarSelect::Update
    // @0x8226F5D0, raises it only in E_STATE_GAME_INTRO_PART_ONE: `stb r28(=1), 0x131(r31)`
    // + `stfs flt_8200CA28, 0x110(r31)` at 0x8226FC94/0x8226FC9C -- camera(r31+0x10)
    // +0x121/+0x100 -- with flt_8200CA28 = 0x41840000 = 16.5 h, and 12.5 h (flt_8200CA00 =
    // 0x41480000) for the outro arms at 0x82270C10 and 0x82270C38. 16.5 * 60 * 60 = 59400 s
    // = 16:30, late-afternoon golden hour. The ordinary DMV BROWSE state raises nothing --
    // its console look is the 18:00 junkyard latch, which never fires on this build yet
    // (no `[env] junkyard` line in the boot log; see the wave findings). The producer's
    // three writes are ALREADY committed (BrnArbStateCarSelect.cpp:827/905/1255,
    // KF_JUNKYARD_TIME_OF_DAY / KF_OUTRO_TIME_OF_DAY); with no consumer they went nowhere
    // and the backdrop sat at 13:00 (`[env] tod=46800.9s (13:00)` in build/game/BrnGame.log).
    if ( mLastCameraInput.GetEffects().IsTimeOfDaySet() )
    {
        // flt_82004C6C == 60.0f, loaded ONCE and used for both multiplies (see above).
        const f32 KF_MINUTES_PER_HOUR   = 60.0f;
        const f32 KF_SECONDS_PER_MINUTE = 60.0f;
        mEnvironmentManager.SetTimeOfDay_Seconds(
            mLastCameraInput.GetEffects().GetTimeOfDay()
                * KF_MINUTES_PER_HOUR * KF_SECONDS_PER_MINUTE );

        // [FLAG PC bring-up diagnostic] one-shot proof line, NOT console code. Delete when
        // the override is signed off. Pairs with the existing `[env] tod=...` line.
        static bool sbLoggedTimeOfDayOverride = false;
        if ( !sbLoggedTimeOfDayOverride && CgsDev::Log::gpDebugPrint != 0 )
        {
            sbLoggedTimeOfDayOverride = true;
            *CgsDev::Log::gpDebugPrint
                << "[env-tod] director camera override ACTIVE: "
                << mLastCameraInput.GetEffects().GetTimeOfDay() << " h -> "
                << ( mLastCameraInput.GetEffects().GetTimeOfDay() * 60.0f * 60.0f )
                << " s\n";
        }
    }

    // FLAG cross-home cast: BrnWorldIO models the world buffer's timer block as its own
    // 48-byte pointer-free POD while the canonical type is CgsSystem::TimerStatusInterface.
    // Both model the SAME X360 member, the sizes are pinned equal here, and this is the
    // identical cast the two committed bridges already make (Bridges/
    // WorldBridgeInputToPhysicsModule.cpp and Bridges/WorldBridgeInputToEntityModules.cpp).
    // Retire all three together when BrnWorldIO adopts the canonical type.
    {
        static_assert( sizeof( BrnWorldIO::TimerStatusInterface )
                           == sizeof( CgsSystem::TimerStatusInterface ),
                       "world timer-status block must be the canonical 48-byte X360 member" );

        const CgsSystem::TimerStatusInterface* const lpTimerStatus =
            reinterpret_cast< const CgsSystem::TimerStatusInterface* >(
                lpUpdateInputBuffer->GetTimerStatusInterface() );
        const CgsSystem::TimerStatus* const lpSimTimerStatus =
            lpTimerStatus->GetSimTimerStatus();

        mEnvironmentManager.SetCurrentTimeStep(
            lpSimTimerStatus->IsRunning() ? lpSimTimerStatus->GetCurrentTimeStep() : 0.0f );
    }

    mEnvironmentManager.Update( mfLocalPlayerActiveRaceCarSpeed, lpUpdateOutputBuffer,
                                mLastCameraInput.GetPosition() );
    PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_World );

    // ---- AI post-physics -----------------------------------------------------
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_NetworkAIRaceCar );
    PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_AI );

    BrnAI::AIModuleIO::InputBuffer_PostPhysics* lpAIInput_PostPhysics = 0;
    lpInputBufferStack->CreateIOBuffer( &lpAIInput_PostPhysics, "AIInputPostPhysics" );

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
            // [FLAG PC bring-up] ...and record that the six face cameras now carry a real
            // LookAt basis, which is the gate GenerateDispatchListsBringUp's env-map arm
            // uses in place of the dispatch input buffer's RenderSwitches (see
            // BrnWorldModule.h). Not a console store. DELETE with that producer.
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
// The vehicle LOD policy tables + debug toggles  (X360 .data / .bss)
//
// Registered as debug-tunables by BrnWorld::RaceCarEntityModuleDebugComponent::
// OnActivate @0x822C2538 under "Graphics/Vehicles.../LODs..." -- the five quality
// bands as "Quality LOD 0".."Quality LOD 4" and the five aggressive bands as
// "Aggressive LOD 0".."Aggressive LOD 4", all with SetRange(0, 300) + SetStep(1);
// the three bools through the bool-registration helper and the fixed-LOD index
// through the int one with range [0, 4]. Tunable => NOT const, exactly as the
// console keeps them in writable data.
//
// NOTE: the shipped Quality and Aggressive tables are BYTE-IDENTICAL, so the blend
// below is a no-op in the retail build. They are kept as two tables because the
// binary keeps two, they sit at two distinct addresses, and both are separately
// tunable at runtime.
//
// (A previous RE note claiming 20/30/40/50/60 for these bands was never verified
// against the image and is WRONG -- the values below are the recovered bytes.)
// ============================================================================
static const u32 KU_NUM_VEHICLE_LODS = 5;

f32 KA_VEHICLE_QUALITY_LOD_DISTANCE[ KU_NUM_VEHICLE_LODS ] =     // X360 0x82F307B4
    { 10.0f, 22.0f, 35.0f, 50.0f, 70.0f };
f32 KA_VEHICLE_AGGRESSIVE_LOD_DISTANCE[ KU_NUM_VEHICLE_LODS ] =  // X360 0x82F307C8
    { 10.0f, 22.0f, 35.0f, 50.0f, 70.0f };

bool sbUseDynamicLods    = true;   // X360 byte_82F307DC  (initialised data, shipped 0x01)
bool sbUseFixedLods      = false;  // X360 byte_8300E114  (zero-init segment)
bool sbUseAggressiveLods = false;  // X360 byte_8300E115  (zero-init segment)
s32  siFixedVehicleLod   = 0;      // X360 dword_8300E118 (zero-init segment, range [0,4])

// FLAG PC-platform leaf: expose the original LOD globals before component
// activation, so early INI and menu edits share the ARTIST registrations. The
// recovered component uses the same registration guard to avoid duplicate rows.
static void RegisterVehicleLodDebugVariablesPC()
{
    if (!CgsPC::Debug::BeginVehicleLodRegistration()) return;
    CgsDev::DebugInterface lDebugInterface;
    const char* lpcGroup = "Graphics/Vehicles.../LODs...";
    lDebugInterface.RegisterVariable(&sbUseDynamicLods, lpcGroup, "Use Dynamic LODs");
    lDebugInterface.RegisterVariable(&sbUseFixedLods, lpcGroup, "Use Fixed LODs");
    lDebugInterface.RegisterVariable(&sbUseAggressiveLods, lpcGroup, "Use Aggressive LODs");
    lDebugInterface.RegisterVariable(&siFixedVehicleLod, lpcGroup, "Vehicle LOD");
    lDebugInterface.SetRange(&siFixedVehicleLod, 0, 4);
    static const char* const kapcQuality[5] =
        { "Quality LOD 0", "Quality LOD 1", "Quality LOD 2", "Quality LOD 3", "Quality LOD 4" };
    static const char* const kapcAggressive[5] =
        { "Aggressive LOD 0", "Aggressive LOD 1", "Aggressive LOD 2", "Aggressive LOD 3", "Aggressive LOD 4" };
    for (u32 luLod = 0; luLod < KU_NUM_VEHICLE_LODS; ++luLod)
    {
        lDebugInterface.RegisterVariable(&KA_VEHICLE_QUALITY_LOD_DISTANCE[luLod], lpcGroup, kapcQuality[luLod]);
        lDebugInterface.RegisterVariable(&KA_VEHICLE_AGGRESSIVE_LOD_DISTANCE[luLod], lpcGroup, kapcAggressive[luLod]);
        lDebugInterface.SetRange(&KA_VEHICLE_QUALITY_LOD_DISTANCE[luLod], 0.0f, 300.0f);
        lDebugInterface.SetRange(&KA_VEHICLE_AGGRESSIVE_LOD_DISTANCE[luLod], 0.0f, 300.0f);
        lDebugInterface.SetStep(&KA_VEHICLE_QUALITY_LOD_DISTANCE[luLod], 1.0f);
        lDebugInterface.SetStep(&KA_VEHICLE_AGGRESSIVE_LOD_DISTANCE[luLod], 1.0f);
    }
}

// The crowding-metric blend constants (X360 flt_82004744 / flt_820CC1CC, loaded by
// the `fsubs f13,f31,f0` + `fmuls f0,f13,f0` pair @0x827C3930..0x827C3938).
// 0.76923078f == 1/1.3.
static const f32 KF_LOD_BLEND_BIAS  = 0.2f;
static const f32 KF_LOD_BLEND_SCALE = 0.76923078f;

// The one console byte offset this body depends on that is not already pinned in its
// owning TU (ActiveRaceCar::mRenderParams @+2016 and RenderParams::mLOD @+5120 --
// together the `stw r9, 0x1BE0(r3)` == +7136 store -- are pinned by the
// static_asserts in BrnActiveRaceCarRenderParams.cpp). VehicleRenderInfo is a public
// standard-layout record, so its two touched fields can be pinned right here:
// `lfs f0, 4(r3)` reads mfDistanceSq and `stw r9, 8(r3)` writes mLOD.
static_assert( offsetof( BrnTraffic::VehicleRenderInfo, mfDistanceSq ) == 4,
               "VehicleRenderInfo::mfDistanceSq @ +0x04 (X360 lfs f0, 4(r3) @0x827C3BF8)" );
static_assert( offsetof( BrnTraffic::VehicleRenderInfo, mLOD ) == 8,
               "VehicleRenderInfo::mLOD @ +0x08 (X360 stw r9, 8(r3) @0x827C3C34)" );

// Pick the first band the distance falls short of; LOD 4 (the coarsest) when it is
// past every band. The X360 compiler emits this loop TWICE inside
// CalculateVehicleLODs -- once for race cars @0x827C3B78..0x827C3BAC and once for
// traffic @0x827C3BF8..0x827C3C34 -- instruction for instruction identical:
//   li r11,0 / mr r9,r26(==4) ; loop: lfs f13,0(r10) ; fcmpu f0,f13 ;
//   blt -> (mr r9,r11 ; done) ; addi r11,1 ; addi r10,4 ; cmplwi r11,5 ; blt loop
// i.e. a STRICT `<` against each band in ascending order, with the 4 preloaded as
// the fall-through. Outlined here so the two sites cannot drift apart.
static CgsGraphics::Model::State
ClassifyVehicleLOD( f32 lfDistance, const f32* lpafLODDistances )
{
    for ( u32 luIndex = 0; luIndex < KU_NUM_VEHICLE_LODS; luIndex++ )
    {
        if ( lfDistance < lpafLODDistances[ luIndex ] )
        {
            return static_cast< CgsGraphics::Model::State >( luIndex );
        }
    }
    return CgsGraphics::Model::E_STATE_LOD_4;
}

// ============================================================================
// CalculateVehicleLODs  @ 0x827C3778
//
// ⭐ THIS FUNCTION IS THE ONLY PER-FRAME WRITER OF ActiveRaceCar::RenderParams::mLOD.
// Without it every race car renders at the LOD that RenderParams::Reset seeds --
// E_STATE_LOD_4, the coarsest of the five -- permanently, and every body part whose
// model carries only 2 or 3 states fails DoesStateExist(4) and does not render at all.
// Reset's `4` is console-faithful (the console Reset stores 4 into +5120): it is the
// deliberate "past every threshold" fallback that THIS function is expected to lift
// off every frame.
//
// Shape (X360, read instruction for instruction):
//   * both input arrays are length-checked through Array<>::GetLength (the two
//     "Array used before Construct/Clear was called" assert sites @0x827C37C4 /
//     @0x827C37EC over the count words at +0x80 and +0x300);
//   * per visible race car: decode the entity id's 14-bit entity index
//     (`extrwi r4, r11, 14, 8` == (id >> 10) & 0x3FFF == EntityId::GetEntityIndex),
//     fetch the ActiveRaceCar, and take the distance from the camera to its body
//     transform's translation row (`lvx128 v0, r0, r11` with r11 = car + 0x810 ==
//     2016 (mRenderParams) + 48 (mBodyTransform.wAxis)). The console computes it as
//     vsubfp + vmsum3fp128 + vrsqrtefp with two Newton refinements and a
//     vcmpeqfp/vsel guard that returns 0 for a zero-length delta -- exactly what
//     rw::math::vpu::Magnitude reduces to here;
//   * lfRenderingCostEstimate = SUM of 1/distance over the visible race cars AND
//     the traffic render infos (traffic distance = sqrt(mfDistanceSq)). It is a
//     CROWDING metric: the closer/more numerous the vehicles, the larger it gets;
//   * the blend factor: 0 normally, (cost - 0.2) * (1/1.3) when Use Dynamic LODs is
//     on, 1 when Use Aggressive LODs is on. NOT clamped to [0,1] on console;
//   * lafLODDistances[i] = Lerp(quality[i], aggressive[i], alpha) * lfZoomFactor.
//     The zoom multiply is a per-band `fmuls f0, f0, f28` (five of them,
//     @0x827C3A18/3A34/3A4C/3AB4/3AC4) -- a zoomed-in camera (zoom > 1) pushes every
//     band further out so distant cars keep their detail;
//   * classify: liLod = 4; for (i = 0; i < 5; ++i) if (dist < band[i]) { liLod = i;
//     break; }. The console spells it as the `fcmpu / blt` + `cmplwi r11, 5` loop
//     @0x827C3B84..0x827C3BA8 -- STRICT `<`, and the 4 fallback is the register
//     preload `mr r9, r26` with r26 == 4;
//   * store: race car -> mRenderParams.mLOD (`stw r9, 0x1BE0(r3)` == +7136 ==
//     2016 + 5120), traffic -> VehicleRenderInfo::mLOD (`stw r9, 8(r3)`);
//   * the PLAYER's own car is then forced to LOD 0 unconditionally (`lwzx r4` from
//     raceCarModule + 0x182F8, `blt` skip when negative, then `li r11,0` +
//     `stw r11, 0x1BE0(r3)` -- an INTEGER zero store, not a float one). This leg
//     runs only in the dynamic branch; the fixed-LOD branch overrides it.
// With the shipped tables and zoom 1 the resulting bands are
//   LOD0 < 10 m | LOD1 10-22 | LOD2 22-35 | LOD3 35-50 | LOD4 >= 50.
//
// De-optimisations applied (per AGENTS.md): the two Newton rsqrt refinements reduce
// to Magnitude/std::sqrt, the five unrolled VMX band lanes are re-rolled into one
// loop, and the `goto LABEL_27/LABEL_36` classifier tails become a `break`.
// ============================================================================
void
WorldModule::CalculateVehicleLODs(
    Vector3 lCameraPos,
    f32 lfZoomFactor,
    Array<CgsSceneManager::EntityId, 32u>& laRaceCarEntityIDs,
    Array<BrnTraffic::VehicleRenderInfo, 64u>& laTrafficRenderInfos )
{
    const s32 liRaceCarCount = static_cast< s32 >( laRaceCarEntityIDs.GetLength() );
    const s32 liTrafficCount = static_cast< s32 >( laTrafficRenderInfos.GetLength() );

    f32 lafRaceCarDistances[ 32 ];
    f32 lfRenderingCostEstimate = 0.0f;

    // ---- pass 1: per-race-car distance + the crowding metric ----------------
    for ( s32 liRaceCarIndex = 0; liRaceCarIndex < liRaceCarCount; liRaceCarIndex++ )
    {
        const CgsSceneManager::EntityId lEntityID =
            laRaceCarEntityIDs[ static_cast< u32 >( liRaceCarIndex ) ];
        ActiveRaceCar* lpRaceCar = mRaceCarEntityModule.GetActiveRaceCar(
            static_cast< EActiveRaceCarIndex >( lEntityID.GetEntityIndex() ) );

        const Vector3& lVehiclePosition =
            lpRaceCar->GetRenderParams()->GetBodyTransform().Pos();

        const f32 lfDistance =
            rw::math::vpu::Magnitude( lCameraPos - lVehiclePosition );

        lafRaceCarDistances[ liRaceCarIndex ] = lfDistance;
        lfRenderingCostEstimate += 1.0f / lfDistance;
    }

    // ---- pass 2: the traffic half of the crowding metric --------------------
    // (the DWARF spells the root rw::math::fpu::Sqrt<float>; the console emits a
    //  bare `fsqrts f0, f0` over the record's cached squared distance.)
    for ( s32 liTrafficIndex = 0; liTrafficIndex < liTrafficCount; liTrafficIndex++ )
    {
        lfRenderingCostEstimate +=
            1.0f / std::sqrt( laTrafficRenderInfos[ static_cast< u32 >( liTrafficIndex ) ].mfDistanceSq );
    }

    // ---- this frame's band set ----------------------------------------------
    f32 lfAlpha = 0.0f;
    if ( sbUseDynamicLods )
    {
        lfAlpha = ( lfRenderingCostEstimate - KF_LOD_BLEND_BIAS ) * KF_LOD_BLEND_SCALE;
    }
    else if ( sbUseAggressiveLods )
    {
        lfAlpha = 1.0f;
    }

    f32 lafLODDistances[ KU_NUM_VEHICLE_LODS ];
    for ( u32 luIndex = 0; luIndex < KU_NUM_VEHICLE_LODS; luIndex++ )
    {
        const f32 lfQuality    = KA_VEHICLE_QUALITY_LOD_DISTANCE[ luIndex ];
        const f32 lfAggressive = KA_VEHICLE_AGGRESSIVE_LOD_DISTANCE[ luIndex ];
        lafLODDistances[ luIndex ] =
            ( lfQuality + ( lfAggressive - lfQuality ) * lfAlpha ) * lfZoomFactor;
    }

    CgsPC::Reflections::SetVehicleLodDistances(lafLODDistances);

    // ---- publish ------------------------------------------------------------
    if ( sbUseFixedLods )
    {
        const CgsGraphics::Model::State leFixedLod =
            static_cast< CgsGraphics::Model::State >( siFixedVehicleLod );

        for ( s32 liRaceCarIndex = 0; liRaceCarIndex < liRaceCarCount; liRaceCarIndex++ )
        {
            const CgsSceneManager::EntityId lEntityID =
                laRaceCarEntityIDs[ static_cast< u32 >( liRaceCarIndex ) ];
            ActiveRaceCar* lpRaceCar = mRaceCarEntityModule.GetActiveRaceCar(
                static_cast< EActiveRaceCarIndex >( lEntityID.GetEntityIndex() ) );
            lpRaceCar->GetRenderParams()->SetLOD( leFixedLod );
        }

        for ( s32 liTrafficIndex = 0; liTrafficIndex < liTrafficCount; liTrafficIndex++ )
        {
            laTrafficRenderInfos[ static_cast< u32 >( liTrafficIndex ) ].mLOD = leFixedLod;
        }
    }
    else
    {
        for ( s32 liRaceCarIndex = 0; liRaceCarIndex < liRaceCarCount; liRaceCarIndex++ )
        {
            const CgsSceneManager::EntityId lEntityID =
                laRaceCarEntityIDs[ static_cast< u32 >( liRaceCarIndex ) ];
            ActiveRaceCar* lpRaceCar = mRaceCarEntityModule.GetActiveRaceCar(
                static_cast< EActiveRaceCarIndex >( lEntityID.GetEntityIndex() ) );

            lpRaceCar->GetRenderParams()->SetLOD(
                ClassifyVehicleLOD( lafRaceCarDistances[ liRaceCarIndex ], lafLODDistances ) );
        }

        // The player's own car always renders at the finest LOD.
        //
        // ⚠ FLAG (holder substitution, 2026-08-12): the console reads the RACE CAR
        // MODULE's own mePlayerActiveRaceCarIndex -- `lwzx r4, r3, r11` with
        // r3 == this + 0x280 (mRaceCarEntityModule) and r11 == 0x182F8, i.e.
        // BrnRaceCarEntityModule.h:483. That member is PRIVATE and the class exposes
        // no accessor for it, so this body reads the WorldModule's own mirror
        // instead. It is the same value: BridgeRaceCarModuleToWorldModule_PreScene
        // @0x827A52B0 publishes GetPlayerActiveRaceCarIndex() into it every PreScene,
        // set to E_ACTIVE_RACE_CAR_INDEX_INVALID otherwise -- which is the same `< 0`
        // early-out the console branches on (polarity verified: INVALID == -1,
        // E_ACTIVE_RACE_CAR_INDEX_0 == 0, matching `blt cr6` @0x827C3BD0).
        //
        // ⚠ NOT FULLY EQUIVALENT (verifier, 2026-08-12): the publish chain
        // (UpdateOutputInterfaces -> SetPlayerActiveRaceCarData -> bridge) is gated on
        // lpPlayerSlot->IsAttached() at BrnRaceCarEntityModule.cpp:2171, not merely on
        // IsPlayerCarActive(), and there is a PreScene->Dispatch phase gap. So a
        // VALID-BUT-UNATTACHED player slot leaves this mirror INVALID where the
        // console would still force LOD 0.
        // ✅ SWAPPED (car+lights step 1b, 2026-08-17): the accessor exists now and this
        // reads the RACE CAR MODULE's own mePlayerActiveRaceCarIndex, exactly the word the
        // console reads. It was NOT low impact: on the Car Select / junkyard screen the
        // player car is created but not attached, the mirror sat INVALID, RenderParams::mLOD
        // stayed at Reset's 4, and the wheels drew their authored LOD-4 BOX proxy (the
        // "rectangle wheels" -- wheels2 REPORT.md: renderable 04F87E17 mesh 1 is a 24-vertex
        // cube in the shipped data; the leaf's decode was correct throughout).
        const EActiveRaceCarIndex lePlayerIndex = mRaceCarEntityModule.GetPlayerActiveRaceCarIndex();
        if ( lePlayerIndex >= E_ACTIVE_RACE_CAR_INDEX_0 )
        {
            mRaceCarEntityModule.GetActiveRaceCar( lePlayerIndex )
                ->GetRenderParams()->SetLOD( CgsGraphics::Model::E_STATE_LOD_0 );
        }

        for ( s32 liTrafficIndex = 0; liTrafficIndex < liTrafficCount; liTrafficIndex++ )
        {
            BrnTraffic::VehicleRenderInfo& lrRenderInfo =
                laTrafficRenderInfos[ static_cast< u32 >( liTrafficIndex ) ];
            lrRenderInfo.mLOD =
                ClassifyVehicleLOD( std::sqrt( lrRenderInfo.mfDistanceSq ), lafLODDistances );
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

    lpDispatchInputBuffer->LockForRead();

    const BrnDirector::Camera::Camera* lpCameraInput = lpDispatchInputBuffer->GetCameraInput();

    // The frame's graphics camera (file-static; the X360 rebuilds it in place).
    // Construct(), NOT Release(): GenerateFrustumQueries @0x827DADF8 is one of the nine
    // xrefs of sub_827F94E8, the KF_DEFAULT_* reset == Camera::Construct(). Release() is
    // the EMPTY body @0x8284CB38 (corrected 2026-08-17 -- see the CgsCamera.cpp banner).
    gFrustumQueryCamera.Construct();
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
            renderengine::SetEnvironmentMapProjectionPC(lProjectedCamera);

            // ⭐ CORRECTED 2026-08-17 (reflections step 1): lbNegateNearFar is TRUE for the
            // env-map faces. This leg used to call the no-arg PC bridge
            // Camera::GetFrustumPerspective(), which hard-codes `false` -- but the X360
            // sets r5 = 1 here and r5 = 0 for the main view, thirty instructions apart in
            // the same function:
            //     0x827DB010  li  r5, 0        <- the MAIN-VIEW query, a few lines above
            //     0x827DB014  addi r4, r1, var_620
            //     0x827DB018  mr  r3, r31
            //     0x827DB01C  bl  CgsGraphics__Camera__GetFrustumPerspective
            //     ...
            //     0x827DB100  li  r5, 1        <- THIS leg, once per rendered face
            //     0x827DB104  mr  r4, r29
            //     0x827DB108  mr  r3, r28
            //     0x827DB10C  bl  CgsGraphics__Camera__GetFrustumPerspective
            // It is not a decompiler artifact and it is not cosmetic: the negate path fnegs
            // both clip planes on entry and negates all six result planes on exit
            // (CgsCamera.cpp:281-285 + the exit loop), which is what makes the volume the
            // one the face camera can actually SEE. The env-map face cameras are the only
            // RIGHT-HANDED cameras in the frame -- EnvironmentMap::Update ends each face on
            // SetPerspectiveProjectionMatrixRightHanded @0x827EC698, whose z row carries
            // w = -1 (CgsCamera.cpp:658-663), i.e. clip.w = -view.z, i.e. the visible half
            // space is at NEGATIVE distance along the LookAt direction. Without the negate
            // the query culls against the volume BEHIND each face and every face list comes
            // back with the wrong half of the world in it.
            // ⚠ CORRECTED 2026-09-06 (b5-decomp#5): the paragraph above describes the state
            // this build WAS in, and it is exactly the bug. The face cameras are no longer
            // right-handed -- EnvironmentMap::Update publishes the ordinary D3D projection now,
            // because LookAt's view is LEFT-handed and the pair drew the antipodal hemisphere
            // into every cube face (+Y held the ground, -Y the sky; measured with
            // BRN_ENVMAP_STATS). So the negate flag goes with it, here and at the live
            // producer's copy of this block. See the FLAG on EnvironmentMap::Update.
            CgsGraphics::CameraRwFrustum lFaceRwFrustum;
            mEnvironmentMap.maEnvMapCameras[ liFace ].GetFrustumPerspective( lFaceRwFrustum, false );
            CgsGeometric::Frustum lFaceFrustum;
            lFaceFrustum.SetFromRwFrustum( lFaceRwFrustum );

            CgsSceneManager::SceneManagerIO::InEventFrustumTestVp lEvent;
            lEvent.mViewProjection = lProjectedCamera.GetViewProjectionMatrix();
            for ( s32 liPlane = 0; liPlane < 8; liPlane++ )
            {
                lEvent.maFrustumPlanes[ liPlane ] = lFaceFrustum.maSwizzledPlanes[ liPlane ];
            }
            lEvent.mQueryId             = KA_FRUSTUM_QUERY_IDS[ 2 + liFace ];
            lEvent.mx32EntityTypeFlags  = 1024u;    // asm: event+0xC4 = 1024
            lEvent.mxQueryFlags         = 0u;       // asm: event+0xC8 = 0

            lpQueryInput->GetInCoarseQueryQueue()->AddEvent( &lEvent, 4, sizeof( lEvent ) );
        }
    }

    // ---- the three shadow cascades ----------------------------------------
    //
    // ✅ ASM RE-READ 2026-08-12 (@0x827DB250..0x827DB300). This leg calls NEITHER
    // Camera::GetFrustumPerspective NOR Frustum::SetFromRwFrustum -- unlike the main-view
    // and env-map legs above, which both do. It copies two ALREADY-COMPUTED per-cascade
    // blocks straight out of the ShadowMap that CalculateShadowMapCameras filled a few
    // hundred instructions earlier (@0x827DAFC4):
    //
    //   r31 = this + 0x5E2BB0            (== mShadowMap + 0x4D0), += 0x170 per cascade
    //   r28 = this + 0x5E3A20            (== mShadowMap + 0x1340), += 0x80  per cascade
    //
    //   the VIEW-PROJECTION: four lvx128 at r31-0x20 / r31-0x10 / r31+0 / r31+0x10 ->
    //     event rows 0..3. Base = mShadowMap + 0x4B0, stride 0x170 == sizeof(
    //     CgsGraphics::Camera). maCgsShadowMapCamera lives at mShadowMap+0x430 and a
    //     Camera's mViewProjection is at +0x80 (pinned independently by the env-map leg
    //     above, which reads its CLONE at clone+0x80) -- 0x430 + 0x80 == 0x4B0. So this
    //     is maCgsShadowMapCamera[i].mViewProjection == GetCascadeCamera(i)->
    //     GetViewProjectionMatrix(). UNCHANGED; the mirror already had this right.
    //
    //   the FRUSTUM: `mtctr 16; ld/std` -- a flat 128-byte copy from r28, i.e. from
    //     mShadowMap + 0x1340 + i*0x80. BrnShadowMap.cpp:407 attests maFrustum at
    //     this+0x1340 with stride 0x80 == sizeof(CgsGeometric::Frustum), and
    //     CgsGeometric::Frustum is exactly Vector4 maSwizzledPlanes[8] (128 bytes), so
    //     the copy IS `maFrustum[i]` verbatim -> ShadowMap::GetFrustum(i).
    //
    // That is the per-cascade narrowing. maFrustum[i] is the light-space optimal view
    // volume ComputeBoundingBoxMatrix -> ComputeOptimalViewVolume fits around cascade i's
    // sub-frustum slab (KAF_SHADOWMAP_SUBSET_FRUSTUM_NEAR/FAR_CLIP = 0/10.5, 10.5/34,
    // 34/120 m), so the three cascades cull against three DIFFERENT volumes.
    // GetFrustumPerspective() -- what this mirror used to call -- reads only mView plus
    // the cached fov/near/far scalars, which CalculateShadowMapCameras leaves identical on
    // all three cascades, and would have handed all three the same ~650 m cone. The
    // console never asks for it here. (SUSPECT flagged at the bring-up mirror below on
    // 2026-08-12: CONFIRMED REAL, and it was OUR mis-read, not the console's.)
    if ( mShadowMap.IsEnabled() &&
         lpDispatchInputBuffer->GetRenderSwitches()->mbRenderShadowMap )
    {
        for ( s32 liCascade = 0; liCascade < 3; liCascade++ )
        {
            const CgsGraphics::Camera* lpCascadeCamera = mShadowMap.GetCascadeCamera( liCascade );

            CgsSceneManager::SceneManagerIO::InEventFrustumTestVp lEvent;
            const CgsGeometric::Frustum& lrCascadeFrustum =
                mShadowMap.GetFrustum( static_cast< u32 >( liCascade ) );
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
    renderengine::BeginGraphicsDiagnosticsPC();

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

    CgsPC::Reflections::BeginSceneFrame();
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
    // Construct(), NOT Release(): GenerateDispatchLists @0x827D1CE8 is one of the nine
    // xrefs of sub_827F94E8 == Camera::Construct() (2026-08-17; Release() is the empty
    // @0x8284CB38 -- see the CgsCamera.cpp banner).
    gDispatchCamera.Construct();
    lpCameraInput->CopyToCgsCamera( &gDispatchCamera );
    CgsPC::Reflections::sfParticleNormalDistance = gDispatchCamera.maProjectionScalars[8];
    lpDispatchThreadInputBuffer->SetCameraViewProjection(
        gDispatchCamera.GetViewProjectionMatrix() );

    // ---- global shader constants for the frame -----------------------------
    // X360 @0x827D2074-0x827D20B8. The argument order is the asm's, not the pseudocode's:
    // (viewProjection, cameraTransform, GAME time, SIM time, frame, output buffer). The
    // camera transform argument is the director Camera* itself on the console -- its
    // mTransform is the object's first member -- which is what GetTransform() returns here.
    //
    // The renderer's external shader bank remains writable until SwapBuffers
    // publishes it; this producer writes the exact bank lent through RendererIO.
    lpDispatchOutputBuffer->LockForWrite();
    SetupShaderConstantsBeforeRendering(
        gDispatchCamera.GetViewProjectionMatrix(),
        lpCameraInput->GetTransform(),
        lpDispatchInputBuffer->GetGameTime(),
        lpDispatchInputBuffer->GetSimTime(),
        lpDispatchInputBuffer->GetShaderConstantsFrame(),
        lpDispatchOutputBuffer );
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
    // `addi r22, r19, 4` @0x827D24B0 -- the array INSIDE the output buffer, not the buffer.
    CalculateVehicleLODs( lpCameraInput->GetPosition(), lfLodZoomFactor,
                          lpFilteredEntityData->maRaceCarEntityIds,
                          lpTrafficRenderInfos->maTrafficRenderInfos );
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
            // Arg 2 is the array INSIDE the buffer (`addi r22, r19, 4` @0x827D24B0). The four
            // vectors the call site loads into v1..v4 are fog scattering / fog colour + white
            // level / camera POSITION / camera FORWARD (@0x827D27F8..0x827D2810). The three
            // ints are liModelOnlyDisplayList / liOpaqueList / liTransparentList, in order.
            mTrafficEntityModule.GenerateDispatchLists(
                lpTrafficDispatchInput, lpTrafficRenderInfos->maTrafficRenderInfos,
                lvFogScattering, lvFogColourPlusWhiteLevel,
                gDispatchCamera.GetPosition(), gDispatchCamera.GetDirection(),
                12, 19, 20, mLastCameraInput );
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
            KI_WORLD_OBJECT_LIST, KI_WORLD_SORT_LAYER, KI_WORLD_SORT_KEY,
            KI_WORLD_PREZ_LIST, false );
        PerfMonCpu::StopMonitor( miGenerateDispatchListsPM );
    }

    if ( lpDispatchInputBuffer->GetRenderSwitches()->mbRenderProps )
    {
        PerfMonCpu::StartMonitor( miPropGenerateDispListClearPM );
        mPropEntityModule.GenerateDispatchLists(
            lpPropDispatchInput, lpFilteredEntityData->maPropEntityIds,
            gDispatchCamera.GetViewProjectionMatrix(), gDispatchCamera.GetPosition(),
            // The trailing pair is (lbRenderingEnvironmentMap, lbRenderCoronas). X360
            // @0x827D294C..58 writes @0x6F = 0 / @0x77 = 1 for the main view.
            lfLodZoomFactor, &mShaderLodInfo, 11, 11, 15, false, true );
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
            lpDispatchThreadInputBuffer->SetEnvMapFaceRender(
                static_cast< u32 >( liFace ), mabEnvMapFaceRender[ liFace ] );

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
            CgsPC::Reflections::LightCapture::BeginFace(static_cast<u32>(liFace), lFaceCamera);

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

            // X360 id-8 payload: `lvx128 v1, r30, r20`, r20 = 6170304 == WorldModule +
            //  6168096 (maEnvMapCameras[6] end) == mEnvironmentMap.mCameraPosition -- the
            //  cube centre EnvironmentMap::Update was handed, NOT the dispatch camera.
            CgsGraphics::mShaderConstantTable.SetShaderConstantData( 8, mEnvironmentMap.mCameraPosition );
            CgsGraphics::mShaderConstantTable.SetShaderConstantData( 3, lFaceCamera.GetViewProjectionMatrix() );
            CgsGraphics::mShaderConstantTable.SetShaderConstantData( 34, lFaceCamera.GetViewProjectionMatrixModified() );

            PerfMonCpu::StartMonitor( miGenerateDispatchListsPM );
            mWorldEntityModule.GenerateDispatchListsForEnvironmentMap(
                lpWorldDispatchInput, lpFilteredEntityData->maWorldEntityIds,
                lFaceCamera.GetViewProjectionMatrix(), gDispatchCamera.GetPosition(),
                &mShaderLodInfo, 5 + liFace, 5 + liFace, 5 + liFace );
            CgsPC::Reflections::WorldCapture::SubmitBackdrops(mWorldEntityModule,
                lpDispatchInputBuffer->GetDispatchFrame(), lFaceCamera, mShaderLodInfo, 5 + liFace);
            PerfMonCpu::StopMonitor( miGenerateDispatchListsPM );

            PerfMonCpu::StartMonitor( miPropGenerateDispListClearPM );
            mPropEntityModule.GenerateDispatchLists(
                lpPropDispatchInput, lpFilteredEntityData->maPropEntityIds,
                lFaceCamera.GetViewProjectionMatrix(), gDispatchCamera.GetPosition(),
                // Env-map face: X360 @0x827D2C58..70 writes @0x6F = 1 / @0x77 = 0 --
                // rendering the environment map, and no coronas on it.
                1.0f, &mShaderLodInfo, 5 + liFace, 5 + liFace, 5 + liFace, true, false );
            PerfMonCpu::StopMonitor( miPropGenerateDispListClearPM );

            // FLAG PC-platform leaf: optional vehicle feeds use this face's view
            // and query results; they never mutate the main-camera LOD/history.
            if (CgsPC::Reflections::Rivals().mbEnabled && lpDispatchInputBuffer->GetRenderSwitches()->mbRenderRaceCars)
            {
                CgsPC::Reflections::VehicleScope lScope(CgsPC::Reflections::E_CAPTURE_RIVALS, 5 + liFace);
                mRaceCarEntityModule.GenerateDispatchLists(lpRaceCarDispatchInput,
                    lpFilteredEntityData->maRaceCarEntityIds, 5 + liFace, 5 + liFace, 5 + liFace,
                    true, lvFogScattering, lvFogColourPlusWhiteLevel, lFaceCamera.GetPosition());
            }
            if (lpDispatchInputBuffer->GetRenderSwitches()->mbRenderTraffic)
                CgsPC::Reflections::TrafficCapture::Submit(mTrafficEntityModule,
                    lpFilteredEntityData->maTrafficEntityIds, lpTrafficDispatchInput,
                    lFaceCamera, lvFogScattering, lvFogColourPlusWhiteLevel, 5 + liFace);
            CgsPC::Reflections::LightCapture::SubmitRaceCars(static_cast<u32>(liFace), mRaceCarEntityModule);
            CgsPC::Reflections::LightCapture::SubmitTrafficSignals(static_cast<u32>(liFace), mTrafficEntityModule);

            // Refresh the face camera's projection for the renderer (far 10000)
            // and record its view-projection for the env-map resolve -- ON THE
            // LOCAL COPY (X360 v219): the member maEnvMapCameras[face] is never
            // mutated by the dispatch pass.
            // FLAG PC-platform leaf: far-clip setters rebuild the ordinary
            // projection. Reapply the native cube orientation before publishing
            // the sky matrix, matching the world/prop face projection above.
            lFaceCamera.SetFarClipPlane( 10000.0f );   // the store + the D3D rebuild, one member
            renderengine::SetEnvironmentMapProjectionPC(lFaceCamera);
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
    renderengine::EndGraphicsDiagnosticsPC();
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
        CgsPC::Reflections::SetShadowCamera(luCascade, *lpCascadeCamera);

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
            // The shadow-cascade pass: all three list ids are the cascade's own list, and the
            // fog vectors are zero (the same shape the race-car cascade call above uses -- a
            // shadow pass has no fog blend to publish). Arg 2 is the array inside the buffer.
            mTrafficEntityModule.GenerateDispatchLists(
                lpTrafficDispatchInput, lpTrafficRenderInfos->maTrafficRenderInfos,
                Vector4{ 0.0f, 0.0f, 0.0f, 0.0f }, Vector4{ 0.0f, 0.0f, 0.0f, 0.0f },
                lpCameraInput->GetPosition(), lpCameraInput->GetDirection(),
                static_cast<s32>( luCascade ) + 2, static_cast<s32>( luCascade ) + 2,
                static_cast<s32>( luCascade ) + 2, *lpCameraInput );
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
                // Shadow cascade: a depth-only pass, so neither the env-map flag nor
                // coronas apply (GenerateShadowMapDispatchLists @0x827C96D8 runs the same
                // prop leg per cascade).
                lfLodZoomFactor, &mShaderLodInfo, liCascadeList, liCascadeList, liCascadeList,
                false, false );
        }
        PerfMonCpu::StopMonitor( miPropGenerateDispListClearPM );

        mShadowMap.SetRenderingShadowMap( false );
    }
}


// -----------------------------------------------------------------------------
// Lane accessors for the two rw::math::vpu idioms this file's new bodies need and the PC
// type vocabulary (plain named lanes, no SDK operations) does not carry:
// Matrix44::SetRow / GetRow (`stvx v, rMatrix, rRow*16`) and Vector4::SetElem (the
// store-vector / patch-one-lane / reload sequence at var_1C0 in sub_827B0790). Same shape
// as the existing ShadowPerfLaneGet below.
// -----------------------------------------------------------------------------
static Vector4& Matrix44Row( Matrix44& lrMatrix, u32 luRow )
{
    switch ( luRow )
    {
        case 0:  return lrMatrix.xAxis;
        case 1:  return lrMatrix.yAxis;
        case 2:  return lrMatrix.zAxis;
        default: return lrMatrix.wAxis;
    }
}

static void Vector4SetElem( Vector4& lrVector, u32 luElement, f32 lfValue )
{
    switch ( luElement )
    {
        case 0:  lrVector.x = lfValue; break;
        case 1:  lrVector.y = lfValue; break;
        case 2:  lrVector.z = lfValue; break;
        default: lrVector.w = lfValue; break;
    }
}

// =============================================================================
// GenerateShaderConstantsForQuadricIrradiance  @ X360 sub_827B0790   [BODIED]
//
// NAME: the export leaves it unnamed (sub_827B0790, no symbol), but the DecFIGS DWARF
// names it and its whole local set --
// references/DecFIGS/dwarfdump/GameSource/World/BrnWorldModule.cpp:36 (source line 2321):
//     void GenerateShaderConstantsForQuadricIrradiance(
//              const rw::math::vpu::Matrix44& lIrradianceMatrixR,
//              const rw::math::vpu::Matrix44& lIrradianceMatrixG,
//              const rw::math::vpu::Matrix44& lIrradianceMatrixB,
//              Matrix44& lOutQuadricMatrix0, Matrix44& lOutQuadricMatrix1 )
// (the _compile rendering spells the two out-params `const Matrix44&`; they are WRITTEN
// -- 0x827B0B08..0x827B0B48 store eight rows through r6/r7 -- so the const is the known
// lossy half of that rendering, exactly as it is for CalculateVehicleLODs' arrays.)
// Its locals, in DWARF declaration order (cpp:2323..2401 / 2331..2349):
//     Vector4 lIrradiance_1;  Matrix44 lIrradiance_x_y_z_xx;  Matrix44 lIrradiance_xy_yz_zx_yy;
//     const Matrix44* laIrradianceMatrices[3];  Matrix44 lIrradianceQuadricForShaderA/B;
//     uint32_t luChannel; const Matrix44& lIrradianceMatrix;
//     float Cxx, Cyy, Czz, C1, Cx, Cy, Cz, Cxy, Cyz, Czx;
// Its only caller is WorldModule::SetupShaderConstantsBeforeRendering @0x827D1410
// (`bl sub_827B0790` @0x827D1780, the single xref).
//
// WHAT THE ASM DOES. Three inputs = the per-channel (R,G,B) order-2 SH IRRADIANCE
// MATRICES GlobalIrradianceManager::ComputeIrradianceMatrix builds; two outputs = the
// pair of Matrix44 "quadrics" shader constants 18/19 carry, in the form the pixel shaders
// evaluate as
//     E_c(n) = A[0][c]
//            + dot( A[c+1], (n.x, n.y, n.z, n.x*n.x) )
//            + dot( B[c]  , (n.x*n.y, n.y*n.z, n.z*n.x, n.y*n.y) )
//
// r3/r4/r5 are stashed as the three-entry pointer array laIrradianceMatrices
// (@0x827B079C/0x827B07A4/0x827B07B4 -> var_1F8/var_1F4/var_1F0) and the body is a
// three-iteration loop (`addi r8,r8,4` / `cmpwi r8,0xC` / `blt` @0x827B0A18..0x827B0AE0)
// that indexes it with `lwzx r10, r8, r10` @0x827B0878.
//
// Per channel the loop reads exactly TEN elements of the matrix (rows are 16-byte lanes;
// the reads are lvx + a vperm splat, or an lfs off the row copy):
//     row0.x  @var_110[0]   (0x827B08C0)   -> Cxx base    == m00
//     row1.y  @var_D0 +4    (0x827B09D4)   -> Cyy base    == m11
//     row2.z  @var_170+8    (0x827B093C)   -> Czz         == m22
//     row3.w  @var_C0 +0xC  (0x827B08DC)   -> C1  base    == m33
//     row0.w  (vperm w-splat @0x827B089C)  -> Cx  = 2*m03
//     row1.w  (vperm w-splat @0x827B08F4)  -> Cy  = 2*m13
//     row2.w  (vperm w-splat @0x827B08D0)  -> Cz  = 2*m23
//     row0.y  (vperm y-splat @0x827B0954)  -> Cxy = 2*m01
//     row1.z  (vperm z-splat @0x827B0A0C)  -> Cyz = 2*m12
//     row2.x  (vperm x-splat @0x827B0A28)  -> Czx = 2*m20
// the six 2.0f multipliers are six separate (2,0,0,0) stack VecFloats built from
// flt_82001D9C (== 2.0f, DATA_DUMP: `40000000 3F000000 ...`), one per product -- i.e. six
// occurrences of a literal 2.0f in the source that the compiler did not CSE.
//
// and folds Czz into the constant + the two square terms:
//     Cxx -= Czz  (`fsubs f11, f11, f13` @0x827B0940)
//     Cyy -= Czz  (`fsubs f13, f11, f13` @0x827B09EC)
//     C1  += Czz  (`fadds f12, f12, f13` @0x827B094C)
// which is the n.x^2 + n.y^2 + n.z^2 == 1 identity used to drop the z^2 term:
//     m00 x^2 + m11 y^2 + m22 z^2 == (m00-m22) x^2 + (m11-m22) y^2 + m22
// So the packed form is EXACTLY n^T M n for a unit normal (round-trip verified to 4.4e-15 (double)
// over 20000 random unit normals x 3 channels -- work/quadric_roundtrip.py).
//
// The three per-channel results land in
//     lIrradiance_1[luChannel]              (the accumulate-into-a-stack-vector idiom at
//                                            var_1C0: store the running vector, overwrite
//                                            lane luChannel with `stfsx f12, r8, r28`
//                                            @0x827B09D8, reload -- rw's Vector4::SetElem)
//     lIrradiance_x_y_z_xx   row luChannel+1  (var_B0 + luChannel*16, @0x827B09BC)
//     lIrradiance_xy_yz_zx_yy row luChannel   (var_70 + luChannel*16, @0x827B0A70)
// and the tail (@0x827B0AE4..0x827B0B48) assembles the two outputs:
//     A = lIrradiance_x_y_z_xx with row 0 replaced by lIrradiance_1
//     B = lIrradiance_xy_yz_zx_yy with row 3 = (0,0,0,1)  (unk_82181530, the identity's
//         fourth row -- DATA_DUMP pins it at 00000000 00000000 00000000 3F800000)
// Rows 0 of x_y_z_xx and 3 of xy_yz_zx_yy are never written in the loop, so the compiler
// dead-stored them; they are written here as SetZero for definedness (they are both
// overwritten below and cannot reach the outputs).
//
// ⚠ DELTA vs THE BRING-UP: PublishWorldShadingConstantsBringUp's hard-coded lQuadricB
// carries wAxis = (0,0,0,0); the console writes (0,0,0,1). Nothing consumes B row 3 (the
// shader dots B rows 0..2 only), so this is a cosmetic difference, but it IS a difference
// and the regression oracle must not flag it.
// =============================================================================
void
GenerateShaderConstantsForQuadricIrradiance( const rw::math::vpu::Matrix44& lIrradianceMatrixR,
                                             const rw::math::vpu::Matrix44& lIrradianceMatrixG,
                                             const rw::math::vpu::Matrix44& lIrradianceMatrixB,
                                             Matrix44& lOutQuadricMatrix0,
                                             Matrix44& lOutQuadricMatrix1 )
{
    Vector4  lIrradiance_1;
    lIrradiance_1.SetZero();                    // `vspltisw v10, 0` @0x827B07AC

    Matrix44 lIrradiance_x_y_z_xx;
    Matrix44 lIrradiance_xy_yz_zx_yy;
    lIrradiance_x_y_z_xx.SetZero();
    lIrradiance_xy_yz_zx_yy.SetZero();

    const rw::math::vpu::Matrix44* laIrradianceMatrices[ 3 ] =
        { &lIrradianceMatrixR, &lIrradianceMatrixG, &lIrradianceMatrixB };

    for ( u32 luChannel = 0; luChannel < 3; luChannel++ )
    {
        const rw::math::vpu::Matrix44& lIrradianceMatrix = *laIrradianceMatrices[ luChannel ];

        // The three squared-term coefficients + the constant, with z^2 folded away
        // (x^2 + y^2 + z^2 == 1 for a unit normal).
        const f32 Czz = lIrradianceMatrix.zAxis.z;          // m22
        const f32 Cxx = lIrradianceMatrix.xAxis.x - Czz;    // m00 - m22
        const f32 Cyy = lIrradianceMatrix.yAxis.y - Czz;    // m11 - m22
        const f32 C1  = lIrradianceMatrix.wAxis.w + Czz;    // m33 + m22

        // The linear terms (the matrix is symmetric, so 2*m0..3 is the whole contribution
        // of both the row and the column entry).
        const f32 Cx = 2.0f * lIrradianceMatrix.xAxis.w;    // 2 * m03
        const f32 Cy = 2.0f * lIrradianceMatrix.yAxis.w;    // 2 * m13
        const f32 Cz = 2.0f * lIrradianceMatrix.zAxis.w;    // 2 * m23

        // The cross terms. Czx reads m20, not m02 -- the console reads row 2 lane 0
        // (`vperm v12, v11, v11, v0` with v0 reloaded to the x-splat control @0x827B0A20);
        // for the symmetric irradiance matrix the two are equal.
        const f32 Cxy = 2.0f * lIrradianceMatrix.xAxis.y;   // 2 * m01
        const f32 Cyz = 2.0f * lIrradianceMatrix.yAxis.z;   // 2 * m12
        const f32 Czx = 2.0f * lIrradianceMatrix.zAxis.x;   // 2 * m20

        Vector4SetElem( lIrradiance_1, luChannel, C1 );
        Matrix44Row( lIrradiance_x_y_z_xx,    luChannel + 1 ) = Vector4{ Cx,  Cy,  Cz,  Cxx };
        Matrix44Row( lIrradiance_xy_yz_zx_yy, luChannel     ) = Vector4{ Cxy, Cyz, Czx, Cyy };
    }

    Matrix44 lIrradianceQuadricForShaderA = lIrradiance_x_y_z_xx;
    lIrradianceQuadricForShaderA.xAxis    = lIrradiance_1;

    Matrix44 lIrradianceQuadricForShaderB = lIrradiance_xy_yz_zx_yy;
    lIrradianceQuadricForShaderB.wAxis    = Vector4{ 0.0f, 0.0f, 0.0f, 1.0f };

    lOutQuadricMatrix0 = lIrradianceQuadricForShaderA;
    lOutQuadricMatrix1 = lIrradianceQuadricForShaderB;
}

// =============================================================================
// WorldModule::SetupShaderConstantsBeforeRendering  @ X360 0x827D1410   [BODIED]
//
// The frame's GLOBAL shading publish: ask the environment manager for the blended
// sky / scattering / key-light / cloud / irradiance set, then write it into (a) the
// global runtime shader-constant register CgsGraphics::mShaderConstantTable, (b) the
// renderer's per-frame BrnShaderConstantsFrame, and (c) the world module's dispatch
// OUTPUT buffer (the copy the per-pass car/world legs of GenerateDispatchLists read
// back). Its only console caller is WorldModule::GenerateDispatchLists @0x827D1CE8
// (`bl` @0x827D20B0, the single xref).
//
// ---- SIGNATURE (asm prologue + DWARF, NOT the pseudocode) --------------------------
// DecFIGS references/DecFIGS/dwarfdump/GameSource/World/BrnWorldModule.h:677 and
// _compile/BrnWorldUnity.cpp:7634 (source BrnWorldModule.cpp:2436):
//     void WorldModule::SetupShaderConstantsBeforeRendering(
//              const rw::math::vpu::Matrix44&       lCameraViewProjection,
//              const rw::math::vpu::Matrix44Affine& lCameraTransform,
//              const float32_t                      lfGameTime,
//              const float32_t                      lfSimTime,
//              BrnShaderConstantsFrame*             lpOutputShaderConstants,
//              DispatchOutputBuffer*                lpOutputBuffer )
// The prologue agrees and pins the PPC slot assignment: r3=this, r4->r24, r5->r25,
// f1 (slot 4, its GPR r6 SKIPPED), f2 (slot 5, r7 SKIPPED), r8->r26, r9->r22. The call
// site closes it: @0x827D2098-0x827D20AC loads r4 = the local Matrix44 copied out of
// gDispatchCamera+0x80..0xB0 (GetViewProjectionMatrix), r5 = the director Camera* (whose
// mTransform is at +0x00, so the Camera* IS the Matrix44Affine&), f1 = GetGameTime(),
// f2 = GetSimTime() (`fmr f2, f31`), r8 = GetShaderConstantsFrame(), r9 = the dispatch
// OUTPUT buffer that is LockForWrite'd around the call. Hex-Rays renders the whole thing
// as `int SetupShaderConstantsBeforeRendering()` -- ZERO parameters -- which is why the
// pre-existing PC declaration (frame, simTime, gameTime) was three arguments short AND
// had the two floats the wrong way round.
//
// ⚠ The two floats are (GAME, SIM) in that order. The old PC decl said (sim, game).
//
// ---- THE 26 OUT-PARAMS OF EnvironmentManager::GenerateShaderConstants @0x827D0098 ----
// DWARF BrnEnvironmentManager.h:398 gives the 26 reference types; this caller's STORE
// SITES give what each one is. The X360 parameter area is 8-BYTE slots (PPC64), base
// r1+0x14, so the stack arguments are slot n at 0x14+(n-1)*8 -- 0x54, 0x5C, ... 0xE4,
// exactly the 19 `stw` targets @0x827D1488..0x827D1544. Hex-Rays assumes 4-byte slots and
// therefore invents 62 arguments (v122..v140 are its uninitialised phantom args 9..27).
//
//  #  reg/slot  local(sp)  DWARF type + name                where THIS function stores it
//  -- --------- ---------- -------------------------------- ---------------------------------
//   1 r4        var_2B0    Vector4& lSky_TopColourDrk       frame +0x2B0 mTopColourDrk
//   2 r5        var_260    Vector4& lSky_HorColourPow       table 33 (SkyReflectionColour);
//                                                           frame +0x2C0 mHorColourPow
//   3 r6        var_250    Vector4& lSky_SunColourPow       frame +0x2D0 mSunColourPow
//   4 r7        var_2F0    Vector3& lSky_HorBleedSclPow     frame +0x2E0 mHorBleedSclPow
//   5 r8        var_1B0    Vector4& lScatt_HorColourPow     (NOT CONSUMED HERE)
//   6 r9        var_3E0    Vector4& lScatt_TopColourDrk     .xyz -> lFogColourPlusWhiteLevel
//                                                           (table 28 + output SetFogColour...)
//   7 r10       var_1C0    Vector4& lScatt_SunColourPow     (NOT CONSUMED HERE)
//   8 sp+0x54   var_1A0    Vector3& lScatt_HorBleedSclPow   (NOT CONSUMED HERE)
//   9 sp+0x5C   var_2A0    Vector4& lScatt_Coeffs           table 27 (ScattCoeffs);
//                                                           frame +0x2F0; output SetFogScattering
//  10 sp+0x64   var_270    Vector3& lKeyLightDirection      table 10; frame +0x220;
//                                                           output SetKeyLightDirection
//  11 sp+0x6C   var_290    Vector3& lKeyLightColour         table 9; table 12 (clamped);
//                                                           frame +0x230; output SetKeyLightColour
//  12 sp+0x74   var_2D0    Vector3& lKeyLightSpecularColour table 11
//  13 sp+0x7C   var_1E0    Vector3& laCloudLiteColours[0]   frame +0x260 mCloudLiteColour0 (w=1)
//  14 sp+0x84   var_1D0    Vector3& laCloudLiteColours[1]   (NOT CONSUMED HERE)
//  15 sp+0x8C   var_200    Vector3& laCloudDarkColours[0]   frame +0x250 mCloudDarkColour0 (w=1)
//  16 sp+0x94   var_1F0    Vector3& laCloudDarkColours[1]   (NOT CONSUMED HERE)
//  17 sp+0x9C   var_240    Vector4& laCloudScaleAndOffsets[0] frame +0x270
//  18 sp+0xA4   var_360    Vector2& lCloudOpacity           frame +0x2A0 = (x, y, 0, 0)
//  19 sp+0xAC   var_350    Vector2& lCloudNegativeDensity   frame +0x280 = (1-x, 1-y, 0, 0)
//  20 sp+0xB4   var_340    Vector2& lCloudFeathering        frame +0x290 = (1/x, 1/y, 0, 0)
//  21 sp+0xBC   var_3B0    float32_t& lfWhiteLevel          f31 everywhere; frame +0x318;
//                                                           table 28.w / 29; output SetWhiteLevel
//  22 sp+0xC4   var_110    Matrix44& lIrradianceMatrixR     -> the quadric packer (r3)
//  23 sp+0xCC   var_150    Matrix44& lIrradianceMatrixG     -> the quadric packer (r4)
//  24 sp+0xD4   var_190    Matrix44& lIrradianceMatrixB     -> the quadric packer (r5)
//  25 sp+0xDC   var_2C0    Vector3& lAverageIrradianceColour output SetAverageIrradianceColour
//  26 sp+0xE4   var_280    Vector3& lUnbiasedKeyLightDirection frame +0x300
//
// THREE INDEPENDENT NAME CONFIRMATIONS that the ordering above is right, not guessed:
//   * #19 is named lCloudNegativeDensity and the store is 1 - it, into mCloudLayerDensity;
//   * #20 is named lCloudFeathering and the store is 1 / it, into mCloudLayerInvFeather;
//   * #15/#13 land in mCloudDarkColour0 / mCloudLiteColour0 respectively, which is what
//     tells lite from dark (they are otherwise the same type in adjacent slots).
//
// ---- WHAT IS PUBLISHED --------------------------------------------------------------
//  mShaderConstantTable slots (in the console's write order):
//     8  ViewPosition            = lCameraTransform.Pos()  (`lvx128 v125, r25, 0x30`)
//     9  KeyLightColour          = lKeyLightColour
//    10  KeyLightDirection       = lKeyLightDirection
//    11  KeyLightSpecularColour  = lKeyLightSpecularColour
//    12  KeyLightClampedColour   = min(max(colour,0), whiteLevel) (`vmaxfp128`+`vminfp`,
//                                  the clamp vector being (wl,wl,wl,0) so .w comes out 0)
//    13  Time                    = (lSimTime, lGameTime, 0, 0)     <-- x is SIM, y is GAME
//     0..4  the five ENGINE matrix slots, reset to the IDENTITY (five inlined
//           GetMatrix44_Identity temporaries built from gIVector + unk_82181510/20/30,
//           whose four rows DATA_DUMP pins as (1,0,0,0)/(0,1,0,0)/(0,0,1,0)/(0,0,0,1)).
//           Slot 3 is ViewProjection: GenerateDispatchLists re-publishes 3/34 from the
//           dispatch camera IMMEDIATELY after this call, which is why resetting it here
//           is harmless -- and why this call must stay AHEAD of that publish.
//    18/19 IrradianceQuadricA/B  = GenerateShaderConstantsForQuadricIrradiance(R,G,B)
//    27  ScattCoeffs             = lScatt_Coeffs
//    28  FogColourPlusWhiteLevel = (lScatt_TopColourDrk.xyz, whiteLevel)
//    29  HDRConstants            = (whiteLevel, 1/whiteLevel, 0, 0)
//    33  SkyReflectionColour     = lSky_HorColourPow
//  the BrnShaderConstantsFrame, in the console's store order:
//    +0x000 ViewProjection, +0x050 CameraTransform, +0x040 ViewPosition,
//    +0x220 KeyLightDirection, +0x230 KeyLightColour, +0x2B0 TopColourDrk,
//    +0x2C0 HorColourPow, +0x2D0 SunColourPow, +0x2E0 HorBleedSclPow,
//    +0x250 CloudDarkColour0, +0x260 CloudLiteColour0, +0x270 CloudTexScaleAndOffsets0,
//    +0x2A0 CloudLayerOpacity, +0x280 CloudLayerDensity, +0x290 CloudLayerInvFeather,
//    +0x310 CloudDistanceCurve, +0x314 GameTime, +0x318 WhiteLevel,
//    +0x2F0 FogScattering, +0x300 UnbiasedKeyLightDirection.
//    mCloudLayerRadii (+0x240) is the ONE frame member this function does not write.
//  the DispatchOutputBuffer: FogColourPlusWhiteLevel, FogScattering, KeyLightDirection,
//    KeyLightColour, QuadricIrradianceA, QuadricIrradianceB, AverageIrradianceColour,
//    WhiteLevel -- all eight members, in that order.
//
// LOCK DISCIPLINE. The console does NOT open either lock here: every frame setter it
// emitted out-of-line carries `Assert(true == mbLockedForWriting)` (twelve surviving
// checks of this+0x31C, the redundant ones CSE'd away), i.e. the CALLER must have the
// frame write-locked, and GenerateDispatchLists brackets the call with the output
// buffer's IOBuffer::LockForWrite / UnlockForWrite (@0x827D2074 / @0x827D20B8).
//
// PERF MONITORS: none. The asm contains no PerfMonCpu call; the UT_RenderMainScreen /
// UT_RenderFX brackets around it live in the caller.
//
// +0x310 mfCloudDistanceCurve comes from `lfsx f30, r23, 0x1E7650` -- WorldModule +
// 0x1E7650 == mEnvironmentManager (WorldModule+0x1E6F60) + 0x6F0, which the committed
// BrnEnvironmentManager.h:211 already names mfCloudDistanceCurve (Construct seeds 1.0f).
// The console reads the member directly; the PC reads it through the accessor added for
// this wave (the member is private in the recon header).
// =============================================================================
void
WorldModule::SetupShaderConstantsBeforeRendering( const rw::math::vpu::Matrix44& lCameraViewProjection,
                                                  const rw::math::vpu::Matrix44Affine& lCameraTransform,
                                                  const f32 lfGameTime,
                                                  const f32 lfSimTime,
                                                  BrnShaderConstantsFrame* lpOutputShaderConstants,
                                                  BrnWorldIO::DispatchOutputBuffer* lpOutputBuffer )
{
    // lTime.x = the SIM time, lTime.y = the GAME time, zw zero (the two vrlimi128 inserts
    // @0x827D14E8 mask 8 = lane x from f2, @0x827D1558 mask 4 = lane y from f1).
    const VecFloat lSimTime  = VecFloat{ lfSimTime,  lfSimTime,  lfSimTime,  lfSimTime  };
    const VecFloat lGameTime = VecFloat{ lfGameTime, lfGameTime, lfGameTime, lfGameTime };
    Vector4 lTime;
    lTime.SetZero();
    lTime.x = lSimTime.x;
    lTime.y = lGameTime.x;

    // `lvx128 v125, r25, 0x30` -- the camera transform's translation row.
    const Vector3 lCameraPosition = lCameraTransform.Pos();

    // ---- the environment manager's blended set ---------------------------------------
    Vector4  lSky_TopColourDrk;
    Vector4  lSky_HorColourPow;
    Vector4  lSky_SunColourPow;
    Vector3  lSky_HorBleedSclPow;
    Vector4  lScatt_HorColourPow;
    Vector4  lScatt_TopColourDrk;
    Vector4  lScatt_SunColourPow;
    Vector3  lScatt_HorBleedSclPow;
    Vector4  lScatt_Coeffs;
    Vector3  lKeyLightDirection;
    Vector3  lKeyLightColour;
    Vector3  lKeyLightSpecularColour;
    Vector3  laCloudLiteColours[ 2 ];
    Vector3  laCloudDarkColours[ 2 ];
    Vector4  laCloudScaleAndOffsets[ 2 ];
    Vector2  lCloudOpacity;
    Vector2  lCloudNegativeDensity;
    Vector2  lCloudFeathering;
    Matrix44 lIrradianceMatrixR;
    Matrix44 lIrradianceMatrixG;
    Matrix44 lIrradianceMatrixB;
    Vector3  lAverageIrradianceColour;
    Vector3  lUnbiasedKeyLightDirection;
    f32      lfWhiteLevel = 0.0f;

    // The 26 out-params, in the asm's argument order (see the table in the banner). The
    // console leaves every one of them UNINITIALISED on entry -- GenerateShaderConstants
    // writes all 26 unconditionally.
    mEnvironmentManager.GenerateShaderConstants(
        lSky_TopColourDrk, lSky_HorColourPow, lSky_SunColourPow, lSky_HorBleedSclPow,
        lScatt_HorColourPow, lScatt_TopColourDrk, lScatt_SunColourPow, lScatt_HorBleedSclPow,
        lScatt_Coeffs,
        lKeyLightDirection, lKeyLightColour, lKeyLightSpecularColour,
        laCloudLiteColours[ 0 ], laCloudLiteColours[ 1 ],
        laCloudDarkColours[ 0 ], laCloudDarkColours[ 1 ],
        laCloudScaleAndOffsets[ 0 ],
        lCloudOpacity, lCloudNegativeDensity, lCloudFeathering,
        lfWhiteLevel,
        lIrradianceMatrixR, lIrradianceMatrixG, lIrradianceMatrixB,
        lAverageIrradianceColour, lUnbiasedKeyLightDirection );

    ::ShaderConstantTable& lrTable = CgsGraphics::mShaderConstantTable;

    // ---- the global shader-constant register -----------------------------------------
    lrTable.SetShaderConstantData(  8, lCameraPosition );
    lrTable.SetShaderConstantData(  9, lKeyLightColour );
    lrTable.SetShaderConstantData( 10, lKeyLightDirection );
    lrTable.SetShaderConstantData( 11, lKeyLightSpecularColour );

    // `vmaxfp128 v13, v126, v127` (v127 == 0) then `vminfp v1, v13, v0` with
    // v0 = (whiteLevel, whiteLevel, whiteLevel, 0.0f) -- so the w lane comes out 0
    // whatever the manager left in it.
    Vector3 lKeyLightClampedColour = lKeyLightColour;
    ClampColourToWhiteLevel( lKeyLightClampedColour, lfWhiteLevel );
    lKeyLightClampedColour.w = 0.0f;
    lrTable.SetShaderConstantData( 12, lKeyLightClampedColour );

    lrTable.SetShaderConstantData( 13, lTime );

    // The five ENGINE matrix slots, reset to the identity for the frame.
    for ( u32 luMatrixConstant = 0; luMatrixConstant < 5; luMatrixConstant++ )
    {
        Matrix44 lIdentity;
        lIdentity.SetIdentity();
        lrTable.SetShaderConstantData( luMatrixConstant, lIdentity );
    }

    Matrix44 lQuadricIrradianceA;
    Matrix44 lQuadricIrradianceB;
    GenerateShaderConstantsForQuadricIrradiance( lIrradianceMatrixR, lIrradianceMatrixG,
                                                 lIrradianceMatrixB,
                                                 lQuadricIrradianceA, lQuadricIrradianceB );
    lrTable.SetShaderConstantData( 18, lQuadricIrradianceA );
    lrTable.SetShaderConstantData( 19, lQuadricIrradianceB );

    lrTable.SetShaderConstantData( 27, lScatt_Coeffs );

    Vector4 lFogColourPlusWhiteLevel;
    lFogColourPlusWhiteLevel.x = lScatt_TopColourDrk.x;
    lFogColourPlusWhiteLevel.y = lScatt_TopColourDrk.y;
    lFogColourPlusWhiteLevel.z = lScatt_TopColourDrk.z;
    lFogColourPlusWhiteLevel.w = lfWhiteLevel;
    lrTable.SetShaderConstantData( 28, lFogColourPlusWhiteLevel );

    // flt_82001C98 == 1.0f and flt_82001CC0 == 0.0f (DATA_DUMP).
    Vector4 lHDRConstants;
    lHDRConstants.x = lfWhiteLevel;
    lHDRConstants.y = 1.0f / lfWhiteLevel;
    lHDRConstants.z = 0.0f;
    lHDRConstants.w = 0.0f;
    lrTable.SetShaderConstantData( 29, lHDRConstants );

    const Vector4 lSkyReflectionColour = lSky_HorColourPow;
    lrTable.SetShaderConstantData( 33, lSkyReflectionColour );

    // ---- the renderer's per-frame constants frame (caller holds the write lock) -------
    lpOutputShaderConstants->SetViewProjectionMatrix( lCameraViewProjection );
    lpOutputShaderConstants->SetCameraTransform( lCameraTransform );
    lpOutputShaderConstants->SetViewPosition( lCameraPosition );
    lpOutputShaderConstants->SetKeyLightDirection( lKeyLightDirection );
    lpOutputShaderConstants->SetKeyLightColour( lKeyLightColour );
    lpOutputShaderConstants->SetTopColourDrk( lSky_TopColourDrk );
    lpOutputShaderConstants->SetHorColourPow( lSkyReflectionColour );
    lpOutputShaderConstants->SetSunColourPow( lSky_SunColourPow );
    lpOutputShaderConstants->SetHorBleedSclPow( lSky_HorBleedSclPow );

    // The two cloud colours go in as Vector4 with w == 1.0f: the console stores a 16-byte
    // vector built at var_3C0 by copying the Vector3 and then `stfs f29, var_3B4` with
    // f29 == flt_82001C98 == 1.0f (@0x827D19C8/0x827D19CC and @0x827D1A10/0x827D1A14).
    lpOutputShaderConstants->SetCloudDarkColour0( Vector4{ laCloudDarkColours[ 0 ].x,
                                                          laCloudDarkColours[ 0 ].y,
                                                          laCloudDarkColours[ 0 ].z, 1.0f } );
    lpOutputShaderConstants->SetCloudLiteColour0( Vector4{ laCloudLiteColours[ 0 ].x,
                                                          laCloudLiteColours[ 0 ].y,
                                                          laCloudLiteColours[ 0 ].z, 1.0f } );
    lpOutputShaderConstants->SetCloudTextureScaleAndOffsets0( laCloudScaleAndOffsets[ 0 ] );

    lpOutputShaderConstants->SetCloudLayerOpacity( Vector4{ lCloudOpacity.x,
                                                           lCloudOpacity.y, 0.0f, 0.0f } );
    lpOutputShaderConstants->SetCloudLayerDensity( Vector4{ 1.0f - lCloudNegativeDensity.x,
                                                           1.0f - lCloudNegativeDensity.y,
                                                           0.0f, 0.0f } );
    lpOutputShaderConstants->SetCloudLayerInvFeather( Vector4{ 1.0f / lCloudFeathering.x,
                                                              1.0f / lCloudFeathering.y,
                                                              0.0f, 0.0f } );
    lpOutputShaderConstants->SetCloudDistanceCurve( mEnvironmentManager.GetCloudDistanceCurve() );

    lpOutputShaderConstants->SetGameTime( lfGameTime );
    lpOutputShaderConstants->SetWhiteLevel( lfWhiteLevel );
    lpOutputShaderConstants->SetFogScattering( lScatt_Coeffs );
    lpOutputShaderConstants->SetUnbiasedKeyLightDirection( lUnbiasedKeyLightDirection );

    // ---- the dispatch output buffer (caller holds the IOBuffer write lock) -----------
    lpOutputBuffer->SetFogColourPlusWhiteLevel( lFogColourPlusWhiteLevel );
    lpOutputBuffer->SetFogScattering( lScatt_Coeffs );
    lpOutputBuffer->SetKeyLightDirection( lKeyLightDirection );
    lpOutputBuffer->SetKeyLightColour( lKeyLightColour );
    lpOutputBuffer->SetQuadricIrradianceA( lQuadricIrradianceA );
    lpOutputBuffer->SetQuadricIrradianceB( lQuadricIrradianceB );
    lpOutputBuffer->SetAverageIrradianceColour( lAverageIrradianceColour );
    lpOutputBuffer->SetWhiteLevel( lfWhiteLevel );
}

// (PublishWorldShadingConstantsBringUp RETIRED 2026-08-16, env-manager go-live wave.
//  Its ten hard-coded shader-constant slots -- 9/10/11/12/13/18/19/27/28/29/33, derived
//  offline from the shipped noon keyframe ENV_KF_Paradise_ingame_junk_city_1200 -- are a
//  STRICT SUBSET of what the real console producer publishes, and that producer is now
//  reconstructed and live: WorldModule::SetupShaderConstantsBeforeRendering @0x827D1410,
//  called from GenerateDispatchListsBringUp immediately before the 8/3/34 camera-constant
//  publish. Coverage, slot by slot:
//     9 KeyLightColour, 10 KeyLightDirection, 11 KeyLightSpecularColour,
//    12 KeyLightClampedColour, 13 Time, 18/19 IrradianceQuadricA/B, 27 ScattCoeffs,
//    28 FogColourPlusWhiteLevel, 29 HDRConstants, 33 SkyReflectionColour -- all published,
//    plus 8 ViewPosition and the engine matrix slots 0..4 the bring-up never wrote.
//  The bring-up's shadow block was already retired 2026-08-12; the real producer does not
//  write c14..c17 at all, so that retirement now holds by construction.
//
//  EXPECTED VISUAL DELTA, so it is not misread as a regression: the KEY LIGHT DIRECTION
//  moves. The bring-up chose (0.406, -0.812, 0.419); the real value is
//  EnvironmentSettings::ComputeKeyLightDirection @0x82678AB0 at the manager's time of day
//  with the keyframe rig angles (rig 45 deg, tilt 20 deg at horizon / 50 deg at midday) --
//  (-0.60917, -0.66981, -0.42458) at 12:00 and (-0.424578, -0.669810, -0.609170) at the
//  Construct default 13:00 exactly (46800.0 s; the shipped log's tod=46800.9 reads
//  (-0.42452, -0.66982, -0.60920)). (CORRECTED 2026-08-20, DMV look-dev wave: the Z lane was
//  POSITIVE here until ComputeKeyLightDirection's two row-vector products were un-transposed
//  -- see the XMMatrixRotationX/Y vpermwi128 decode in BrnEnvironmentUtil.cpp. The sun
//  therefore now travels the mirrored-in-Z arc, which is the console's.)
//  The world's shading AND the shadow direction change accordingly, and the sun in
//  the sky dome only agrees once BrnRendererModule::PublishSkyConstantsBringUp is retired
//  in favour of gBrnWorldShaderConstantsFrameBringUp.
//  The other documented delta is IrradianceQuadricB row 3: the bring-up carried (0,0,0,0),
//  the console writes (0,0,0,1). Nothing consumes that row.)




// =================================================================================================
// [PC HARNESS, NOT X360] BRN_AI_DRIVES_PLAYER=1 -- hand the player car to the game's OWN AI.
//
// The console has this seat. WorldDebugComponent registers the debug variable "AI drives player"
// and its callback AIDrivesPlayerChanged @0x827B1FC0 stores maeCarControls[player] = 2 and
// mAIModule.mbAIPlayerInvulnerable = false. From there every link is the console's:
//   * Update (above) refreshes AIModule::mbAIDrivesPlayer from the control word each frame
//     (ARTIST 0x827D74FC..0x827D753C);
//   * AIModule::StoreDrivenCarData derives AICar::mbIsDrivenByPlayer = isPlayer && !mbAIDrivesPlayer
//     (asm 0x82795CB4..0x82795CD4), so the player's AIDriver drives its car like any rival's;
//   * WorldBridgeAIModule forwards the AI's BrnAIDriverControls for the player slot to physics ONLY
//     when the control word is 2 (0x827AAC1C..0x827AAC2C), and WorldBridgeEntityModulesToPhysics
//     forwards the pad record ONLY when it is 1 (0x827AAFEC..0x827AB008);
//   * VehicleManager::UpdateAIDriver @0x825C5110 installs the AI record in the per-car slot and
//     mirrors it into mPlayerAiDriver.
// The same seat is what ModeManager posts at event finish (SET_PLAYER_CAR_DRIVER selector 2) and
// what the AI aggression code tests (FindTarget refuses an AI-driven player car as a target).
//
// A harness run has no debug menu, so this arm flips the same member through the same callback
// once the player's slot is attached AND its AI driver slot is active (before that the callback's
// index assert would fire and there is no driver to take the wheel). It is a STANDING policy, like
// the console's sibling mbDEBUGPlayerCarAlwaysUnderAIControl: several console flows hand the car
// back to pad control (SET_PLAYER_CAR_DRIVER selector 1 at a drive-thru exit, the case-34 "reset
// every car to player control" arm), so whenever the word is found back at 1 while the variable is
// set, the toggle is applied again -- the first time with the full banner, afterwards one short
// line, so a log shows exactly when the game reclaimed the car and the harness re-took it.
// Opt-in; inert without the variable. The pad channels the harness holds (-Drive/-Steer/-*Script)
// are dropped by the bridge while the word is 2 -- by the console's own gate, not by anything here.
// DELETE-WHEN the debug menu is drivable from the harness.
// =================================================================================================
void WorldModule::HarnessArmAIDrivesPlayer()
{
    static const bool skbWanted = ( std::getenv( "BRN_AI_DRIVES_PLAYER" ) != 0 );
    static s32        siArmCount = 0;
    if ( !skbWanted )
    {
        return;
    }
    if ( meLocalPlayerActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID )
    {
        return;
    }
    if ( maeCarControls[ meLocalPlayerActiveRaceCarIndex ] == E_CAR_CONTROL_AI_MODULE )
    {
        return;   // already the AI's -- nothing to re-apply
    }
    const BrnAI::AIDriver* lpPlayerDriver = mAIModule.GetAIDriver( meLocalPlayerActiveRaceCarIndex );
    if ( lpPlayerDriver == 0 || !lpPlayerDriver->IsActive() )
    {
        return;
    }

    const s32 liPreviousControl = maeCarControls[ meLocalPlayerActiveRaceCarIndex ];
    ++siArmCount;
    mDebugComponent.HarnessSetAIDrivesPlayer( true );

    if ( CgsDev::Log::gpDebugPrint != 0 )
    {
        if ( siArmCount == 1 )
        {
            *CgsDev::Log::gpDebugPrint
                << "[ai-drive] ***** HARNESS-ONLY (BRN_AI_DRIVES_PLAYER=1): the game's own AI now drives the "
                << "player car -- WorldDebugComponent::AIDrivesPlayerChanged @0x827B1FC0 stored maeCarControls["
                << static_cast<s32>( meLocalPlayerActiveRaceCarIndex ) << "]=2 and mbAIPlayerInvulnerable=0. "
                << "The pad record is no longer forwarded to physics; the AI record for this slot is. "
                << "Standing: re-applied whenever the game hands the car back. *****\n";
        }
        else
        {
            *CgsDev::Log::gpDebugPrint
                << "[ai-drive] re-armed #" << siArmCount << ": the game had set maeCarControls["
                << static_cast<s32>( meLocalPlayerActiveRaceCarIndex ) << "]=" << liPreviousControl
                << "; back to 2 (AI) via the same @0x827B1FC0 callback\n";
        }
    }
}

// =================================================================================================
// [PC HARNESS, NOT X360] BRN_AI_PAD_PLAYER=cruise|race|pursuit -- the "AI PAD" seat, world half.
//
// The console seat above (BRN_AI_DRIVES_PLAYER) hands the car to the AI by flipping the control word
// to 2, and with it every reader of AICar::mbIsDrivenByPlayer -- including AIAggression::FindTarget
// @0x82793C60, which then refuses the player as a rival target (0x82793CA0..0x82793CB4). This seat keeps
// the control word at 1 (E_CAR_CONTROL_ENTITY_MODULE): to the rest of the game the car is PAD-driven,
// rivals target it, takedowns are player-credited, race logic treats it as the player. Only the SOURCE
// of the pad changes:
//   * AI side (GameSource/World/AI/BrnAIHarnessPad.h): the player's own AICar::Update / AIDriver::Update
//     run with the console seat's mbIsDrivenByPlayer (0) -- with it set, AIDriver::GetTargetPosition
//     @0x8277CBF8 aims one metre straight ahead (0x8277CC20..0x8277CC28) and the record the AI already
//     emits for the player's slot every frame (ProcessAIVehicleInputs @0x82795E10) carries no decision.
//   * Stash (after AIModule::Update): that record -- the one WorldModule::BridgeAIModuleToPhysicsModule
//     @0x827AAAA8 would forward in the console seat (0x827AAC08..0x827AAC2C) -- is copied out of the AI
//     output. Here the bridge drops it, as the console does for control word 1.
//   * Apply (next frame, right after WorldModule::BridgeInputToEntityModules copied the real pad into
//     the race-car pre-scene input): the record's gas / brake / handbrake / steering / boost overwrite
//     the pad's mfAcceleration / mfBraking / mfHandBrake / mfSteering / mbBoost, and mbIsWheel is set --
//     the AI record's +0x41 mbIsSteeringWheel is 1 (0x82796020), and ProcessPlayerVehicleInput
//     @0x822FFE30 copies mbIsWheel into exactly that byte (0x82300264..0x82300270). From there it is
//     the console's own chain: PreSceneUpdate's 60-byte latch, ProcessPlayerVehicleInput's PLAYER
//     record (boost through the boost strategy and the bar, the start-line / wreck / invulnerability
//     gates), BridgeEntityModulesToPhysicsModule_PrePhysics' `== 1` forward (0x827AAFE8..0x827AB008),
//     VehicleManager::UpdatePlayerDriver @0x825E9F38. One frame of latency: the pad is latched at
//     pre-scene, before the AI update.
//   * The real pad's BRAKE is a manual override (harness glue for the organic runner's logged UNSTICK
//     fallback): while it is held, the real pad passes through untouched.
//   * pursuit: inside the console's slam window (BrnAIModule_Drive.cpp) the pad also holds the
//     throttle and the boost button -- a pad's ram; the AI's own Slam fan steers.
// Standing policy, the same shape as HarnessArmAIDrivesPlayer: armed whenever the player's slot is
// attached, its AI driver slot is active and the control word is 1 (the console's own seat keeps the
// car while it holds it: junkyard exit, drive-thrus, rolling starts); `race` also needs the player's
// AICar in a game mode. The first arm prints a banner, every later one `[ai-pad] re-armed #n`.
// Opt-in; a run without the variable never writes gHarnessAIPad and never touches the pad.
// DELETE-WHEN a real pad can drive the harness. Trace: scratch/CRASHPARITY_0922/fixes/FX-AIPAD.md.
// =================================================================================================
namespace
{
    // One frame's player-seat decision: copied out of the AI output after AIModule::Update, written
    // into the pad on the next frame.
    struct HarnessAIPadStash
    {
        bool mbValid;
        f32  mfGas;
        f32  mfBrake;
        f32  mfHandBrake;
        f32  mfSteering;
        bool mbBoost;
        bool mbRamming;
    };
    HarnessAIPadStash sHarnessAIPadStash = { false, 0.0f, 0.0f, 0.0f, 0.0f, false, false };

    // [FLAG PC witness] the pad trace: one line per KI_HARNESS_AI_PAD_TRACE_PERIOD applied frames (60 ==
    // one second of the fixed 1/60 sim step), at most KI_HARNESS_AI_PAD_TRACE_LINES lines.
    const s32 KI_HARNESS_AI_PAD_TRACE_PERIOD = 60;
    const s32 KI_HARNESS_AI_PAD_TRACE_LINES  = 1200;
    s32       siHarnessAIPadApplied          = 0;
    s32       siHarnessAIPadTraceLines       = 0;
    bool      sbHarnessAIPadManual           = false;

    const char* HarnessAIPadModeName( BrnAI::EHarnessAIPadMode leMode )
    {
        switch ( leMode )
        {
            case BrnAI::E_HARNESS_AI_PAD_CRUISE:  return "cruise";
            case BrnAI::E_HARNESS_AI_PAD_RACE:    return "race";
            case BrnAI::E_HARNESS_AI_PAD_PURSUIT: return "pursuit";
            case BrnAI::E_HARNESS_AI_PAD_COMBAT: return "combat";
            default:                              return "off";
        }
    }

    // BRN_AI_PAD_PLAYER's value: cruise | race | pursuit ("1", what -DiagEnv passes for a bare name, is
    // cruise). Anything else is REFUSED loudly and the seat stays off.
    BrnAI::EHarnessAIPadMode HarnessAIPadModeFromEnvironment()
    {
        const char* lpcSpec = std::getenv( "BRN_AI_PAD_PLAYER" );
        if ( lpcSpec == 0 || lpcSpec[ 0 ] == '\0' )
        {
            return BrnAI::E_HARNESS_AI_PAD_OFF;
        }
        if ( std::strcmp( lpcSpec, "cruise" ) == 0 || std::strcmp( lpcSpec, "1" ) == 0 )
        {
            return BrnAI::E_HARNESS_AI_PAD_CRUISE;
        }
        if ( std::strcmp( lpcSpec, "race" ) == 0 )
        {
            return BrnAI::E_HARNESS_AI_PAD_RACE;
        }
        if ( std::strcmp( lpcSpec, "pursuit" ) == 0 )
        {
            return BrnAI::E_HARNESS_AI_PAD_PURSUIT;
        }
        if ( std::strcmp( lpcSpec, "combat" ) == 0 )
            return BrnAI::E_HARNESS_AI_PAD_COMBAT;
        if ( CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint << "[ai-pad] REFUSED: BRN_AI_PAD_PLAYER='" << lpcSpec
                                       << "' is not cruise | race | pursuit | combat -- the seat stays OFF [PC HARNESS]\n";
        }
        return BrnAI::E_HARNESS_AI_PAD_OFF;
    }
}

void WorldModule::HarnessArmAIPadPlayer()
{
    static const BrnAI::EHarnessAIPadMode seMode = HarnessAIPadModeFromEnvironment();
    static s32         siArmCount    = 0;
    static s32         siNoteLines   = 0;
    static const char* spcLastReason = 0;
    if ( seMode == BrnAI::E_HARNESS_AI_PAD_OFF )
    {
        return;
    }

    BrnAI::HarnessAIPad& lrPad = BrnAI::gHarnessAIPad;
    lrPad.meMode = seMode;

    const char* lpcBlocked = 0;
    s32         liControl  = -1;
    if ( meLocalPlayerActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID )
    {
        lpcBlocked = "no local player race car yet";
    }
    else
    {
        liControl = maeCarControls[ meLocalPlayerActiveRaceCarIndex ];
        const BrnAI::AIDriver* lpDriver = mAIModule.GetAIDriver( meLocalPlayerActiveRaceCarIndex );
        if ( liControl != E_CAR_CONTROL_ENTITY_MODULE )
        {
            lpcBlocked = "the console's own seat has the car (control word is not 1)";
        }
        else if ( lpDriver == 0 || !lpDriver->IsActive() || lpDriver->GetCar() == 0 )
        {
            lpcBlocked = "the player's AI driver slot is not active";
        }
        else if ( (seMode == BrnAI::E_HARNESS_AI_PAD_RACE || seMode == BrnAI::E_HARNESS_AI_PAD_COMBAT)
                  && !lpDriver->GetCar()->mbIsInGameMode )
        {
            lpcBlocked = "race: no event is running";
        }
    }

    const bool lbArm = ( lpcBlocked == 0 );
    if ( CgsDev::Log::gpDebugPrint != 0 )
    {
        if ( lbArm && !lrPad.mbArmed )
        {
            ++siArmCount;
            if ( siArmCount == 1 )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[ai-pad] ***** HARNESS-ONLY (BRN_AI_PAD_PLAYER=" << HarnessAIPadModeName( seMode )
                    << "): armed -- the game's own AI computes the player car's controls and they reach "
                    << "physics through the PAD path; maeCarControls["
                    << static_cast<s32>( meLocalPlayerActiveRaceCarIndex ) << "] stays 1, so rivals target "
                    << "the player and takedowns are player-credited. Standing: re-armed whenever the game "
                    << "hands the car back. *****\n";
            }
            else
            {
                *CgsDev::Log::gpDebugPrint << "[ai-pad] re-armed #" << siArmCount << " ("
                                           << HarnessAIPadModeName( seMode ) << ")\n";
            }
            spcLastReason = 0;
        }
        else if ( !lbArm && ( lrPad.mbArmed || lpcBlocked != spcLastReason ) && siNoteLines < 200 )
        {
            ++siNoteLines;
            *CgsDev::Log::gpDebugPrint << "[ai-pad] " << ( lrPad.mbArmed ? "stood down" : "waiting" ) << ": "
                                       << lpcBlocked << " (control word " << liControl << ")\n";
            spcLastReason = lpcBlocked;
        }
    }
    lrPad.mbArmed = lbArm;
}

void WorldModule::HarnessStashAIPadControls( BrnAI::AIModuleIO::OutputBuffer* lpAIOutput )
{
    HarnessAIPadStash& lrStash = sHarnessAIPadStash;
    lrStash.mbValid = false;
    const BrnAI::HarnessAIPad& lrPad = BrnAI::gHarnessAIPad;
    if ( !lrPad.mbArmed || lpAIOutput == 0 )
    {
        return;
    }

    lpAIOutput->LockForRead();
    // Through a const view: the non-const GetVehicleDriverInterface is the WRITE seat (0x8276D9C8,
    // asserts "Not locked for writing"); the read twin is 0x8279CA00, the one the AI bridge uses.
    const BrnAI::AIModuleIO::OutputBuffer* lpAIOutputRead = lpAIOutput;
    const BrnPhysics::Vehicle::VehicleDriverInputInterface::UpdateDriverEventQueue* lpQueue =
        lpAIOutputRead->GetVehicleDriverInterface()->GetUpdateDriverQueue();
    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    for ( s32 liType = lpQueue->GetFirstEvent( &lpEvent, &liSize );
          liType >= 0;
          liType = lpQueue->GetNextEvent( lpEvent, &lpEvent, &liSize ) )
    {
        if ( liType != BrnPhysics::Vehicle::E_DRIVER_TYPE_AI )
        {
            continue;
        }
        const BrnPhysics::Vehicle::BrnAIDriverControls* lpControls =
            static_cast<const BrnPhysics::Vehicle::BrnAIDriverControls*>( lpEvent );
        if ( lpControls->miVehicleID != static_cast<s32>( meLocalPlayerActiveRaceCarIndex ) )
        {
            continue;
        }
        lrStash.mbValid     = true;
        lrStash.mfGas       = lpControls->mfGas;
        lrStash.mfBrake     = lpControls->mfBrake;
        lrStash.mfHandBrake = lpControls->mfHandBrake;
        lrStash.mfSteering  = lpControls->mfSteering;
        lrStash.mbBoost     = lpControls->mbBoost;
        lrStash.mbRamming   = lrPad.mbRamming;
        if (lrPad.meMode == BrnAI::E_HARNESS_AI_PAD_COMBAT && lrPad.mbCombatControl)
        {
            lrStash.mfSteering = lrPad.mfCombatSteering;
            lrStash.mfGas = 1.0f;
            lrStash.mfBrake = 0.0f;
            lrStash.mfHandBrake = 0.0f;
            lrStash.mbBoost = true;
        }
        break;
    }
    lpAIOutput->UnlockForRead();
}

void WorldModule::HarnessApplyAIPad( RaceCarEntityModuleIO::InputBuffer_PreScene* lpRaceCarInput_PreScene,
                                     const BrnWorldIO::UpdateInputBuffer* lpUpdateInputBuffer )
{
    const HarnessAIPadStash& lrStash = sHarnessAIPadStash;
    if ( !lrStash.mbValid )
    {
        return;
    }
    const BrnWorldIO::PlayerVehicleControls* lpRealPad = lpUpdateInputBuffer->GetPlayerVehicleControls();
    if ( lpRealPad == 0 )
    {
        return;
    }

    // FLAG cross-home cast -- the one WorldBridgeInputToEntityModules makes for the same 60-byte copy.
    BrnWorld::PlayerVehicleControls lPad = *reinterpret_cast<const BrnWorld::PlayerVehicleControls*>( lpRealPad );

    const bool lbManual = ( lPad.mfBraking > 0.0f );
    if ( lbManual != sbHarnessAIPadManual && CgsDev::Log::gpDebugPrint != 0
         && siHarnessAIPadTraceLines < KI_HARNESS_AI_PAD_TRACE_LINES )
    {
        ++siHarnessAIPadTraceLines;
        *CgsDev::Log::gpDebugPrint << ( lbManual ? "[ai-pad] manual override: the real pad holds brake -- it drives"
                                                 : "[ai-pad] manual override released: the AI drives again" )
                                   << " [PC HARNESS]\n";
    }
    sbHarnessAIPadManual = lbManual;
    if ( lbManual )
    {
        return;   // the real pad already sits in the buffer (the bridge just copied it)
    }

    lPad.mfAcceleration = lrStash.mfGas;
    lPad.mfBraking      = lrStash.mfBrake;
    lPad.mfHandBrake    = lrStash.mfHandBrake;
    lPad.mfSteering     = lrStash.mfSteering;
    lPad.mbBoost        = lrStash.mbBoost;
    lPad.mbIsWheel      = true;
    if ( lrStash.mbRamming )
    {
        lPad.mfAcceleration = 1.0f;
        lPad.mfBraking      = 0.0f;
        lPad.mbBoost        = true;
    }
    lpRaceCarInput_PreScene->SetPlayerVehicleControls( &lPad );

    ++siHarnessAIPadApplied;
    if ( ( siHarnessAIPadApplied % KI_HARNESS_AI_PAD_TRACE_PERIOD ) == 1 && CgsDev::Log::gpDebugPrint != 0
         && siHarnessAIPadTraceLines < KI_HARNESS_AI_PAD_TRACE_LINES )
    {
        ++siHarnessAIPadTraceLines;
        const BrnAI::HarnessAIPad& lrPad = BrnAI::gHarnessAIPad;
        *CgsDev::Log::gpDebugPrint << "[ai-pad] pad #" << siHarnessAIPadApplied << " "
                                   << HarnessAIPadModeName( lrPad.meMode ) << " gas " << lPad.mfAcceleration
                                   << " brake " << lPad.mfBraking << " hb " << lPad.mfHandBrake << " steer "
                                   << lPad.mfSteering << " boost " << ( lPad.mbBoost ? 1 : 0 ) << " ram "
                                   << ( lrStash.mbRamming ? 1 : 0 ) << " target " << lrPad.miTarget << " sep "
                                   << lrPad.mfTargetSeparation << " ahead " << lrPad.mfTargetAheadness
                                   << " routes " << lrPad.miPlans << " rams " << lrPad.miRamEntries << "\n";
    }
}
}

// ============================================================================
// FOLDED FROM BrnWorldModule_wG_Bridges_01.cpp (wave G) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===========================================================================
// BrnWorldModule_wG_Bridges_01.cpp -- four WorldModule per-frame bridge seams:
// each takes one module's output-buffer queue or interface and merges or latches
// it into the next module's input buffer.
//
// Source getter first, then destination getter, then the merge -- both getters are
// lock tripwires, so the order is observable. Null tripwires appear only where the
// console body has one; two of the four have no compare at all.
// FLAG: the console brackets two of these in a CPU perf monitor taken out of the
// world-module context, which arrives here as an untyped pointer; the monitors are
// not modelled, the standing disposition of every landed sibling in Bridges/.
// ===========================================================================


namespace WorldModule
{

// ---------------------------------------------------------------------------
// WorldModule::BridgeRaceCarModuleToTrafficModule_PrePhysics
//
// One latch: read the race car's pre-physics player-reset interface out of its own
// output buffer (read-lock getter) and hand it to the traffic module's pre-physics
// input buffer (write-lock setter, which copies it into its member seat).
// No null tripwires: the console body has no compare and no assert, unlike its
// three siblings.
// ---------------------------------------------------------------------------
void BridgeRaceCarModuleToTrafficModule_PrePhysics(
    void* lpWorldModule,
    BrnTraffic::BrnTrafficIO::InputBuffer_PrePhysics* lpTrafficInputBuffer_PrePhysics,
    const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpRaceCarOutputBuffer_PrePhysics)
{
    (void)lpWorldModule;   // the world-module context: copied out of its register and never read

    // Source getter (read-lock), then destination setter (write-lock) -- the console's order.
    lpTrafficInputBuffer_PrePhysics->SetPlayerResetInterface(
        lpRaceCarOutputBuffer_PrePhysics->GetPlayerResetInterface());
}

// ---------------------------------------------------------------------------
// WorldModule::BridgeSceneQueryResultsToTriggerModule_PrePhysics
//
// The trigger module's half of the scene-query results fan-out: append the scene
// manager's results ring into the trigger module's pre-physics scene-result queue.
// Both are the same variable-event-queue instantiation, so the merge is the queue's
// own Append. No null tripwires -- this body has no compare at all.
// It is the inbound half of the round trip BridgeTriggerModuleToSceneModule_PostScene
// below opens.
// ---------------------------------------------------------------------------
void BridgeSceneQueryResultsToTriggerModule_PrePhysics(
    void* lpWorldModule,
    BrnWorld::TriggerEntityModuleIO::InputBuffer_PrePhysics* lpTriggerInputBuffer_PrePhysics,
    const CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneQueryOutput)
{
    (void)lpWorldModule;

    const CgsSceneManager::SceneManagerIO::OutputBuffer::SceneQueryResultsQueue* lpResults =
        lpSceneQueryOutput->GetSceneQueryResultsQueue();

    lpTriggerInputBuffer_PrePhysics->GetSceneResultQueue()->Append(*lpResults);
}

// ---------------------------------------------------------------------------
// WorldModule::BridgeRaceCarEntityInfoToOutput_PrePhysics
//
// The race car's pre-physics game-event flush: append the race-car module's own
// game-event queue into the world update-output buffer's game-event queue, which
// the game-state module drains.
// Both asserts are the console's and both are NON-gating -- it fires and falls
// through into the transfer. The strings are verbatim ("lpOutputBuffer" carries no
// "!= NULL" tail; not a typo).
// The source queue DERIVES from the shared variable-event-queue instantiation while
// the destination is a direct alias of it, so the source is bound to the base
// reference explicitly rather than deduced through the derived type.
// ---------------------------------------------------------------------------
void BridgeRaceCarEntityInfoToOutput_PrePhysics(
    void* lpWorldModule,
    BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
    const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpRaceCarOutput_PrePhysics)
{
    (void)lpWorldModule;   // read only for the perf-monitor handle, which is not modelled

    CGS_ASSERT(lpOutputBuffer != 0, "lpOutputBuffer");
    CGS_ASSERT(lpRaceCarOutput_PrePhysics != 0, "lpRaceCarOutputBuffer_PrePhysics");

    // Source getter (read-lock) first, destination getter (write-lock) second -- the
    // console's order, and both are lock tripwires.
    const CgsModule::VariableEventQueue<1536, 16>& lrSourceEvents =
        *lpRaceCarOutput_PrePhysics->GetGameEventQueue();

    lpOutputBuffer->GetGameEventQueue()->Append(lrSourceEvents);
}

// ---------------------------------------------------------------------------
// WorldModule::BridgeTriggerModuleToSceneModule_PostScene
//
// The trigger module's post-scene query staging. The trigger module posts its
// line queries into a VARIABLE event queue (heterogeneous, records tagged with a
// type id); the scene manager wants them in its TYPED fine-line-test queue. So
// this bridge cannot be a queue Append -- it walks the source record by record
// and re-adds each one.
//
// The loop guard is the record POINTER, not the type id: a type id of 0 still
// iterates, a null record ends the walk. Both asserts, and the "Invalid event type."
// arm, are non-gating -- the walk continues either way.
//
// The record is reinterpreted rather than cast through a hierarchy: the variable
// queue hands back a pointer into its own packed byte buffer and the scene-manager
// query element is an unrelated type -- the sanctioned external-byte-stream case.
// The literal 5 is the console's compare immediate, left a literal on purpose: it is
// the record id the trigger producer stamps, and the scene manager's query-result
// enumeration's 5 is a COUNT, so naming it after that would be invention.
// ---------------------------------------------------------------------------
void BridgeTriggerModuleToSceneModule_PostScene(
    void* lpWorldModule,
    CgsSceneManager::SceneManagerIO::InputBuffer_Query* lpSceneQueryInput,
    const BrnWorld::TriggerEntityModuleIO::OutputBuffer_PostScene* lpTriggerOutputBuffer_PostScene)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpSceneQueryInput != 0, "lpSceneModuleInputBuffer != NULL");
    CGS_ASSERT(lpTriggerOutputBuffer_PostScene != 0, "lpTriggerOutput_PostScene != NULL");

    const BrnWorld::TriggerEntityModuleIO::SceneFineQueryQueue* lpTriggerQueries =
        lpTriggerOutputBuffer_PostScene->GetSceneFineQueryQueue();

    const CgsModule::Event* lpEvent = 0;
    s32                     liSize  = 0;

    s32 liType = lpTriggerQueries->GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent != 0)
    {
        if (liType == 5)   // the trigger module's fine line-test record id
        {
            const CgsSceneManager::SceneManagerIO::InEventLineTestFine* lpQuery =
                reinterpret_cast<const CgsSceneManager::SceneManagerIO::InEventLineTestFine*>(lpEvent);

            lpSceneQueryInput->GetFineLineTestQueue()->AddEvent(*lpQuery);
        }
        else
        {
            // Non-gating on the console: it fires and then falls into the next step.
            CGS_ASSERT(false, "Invalid event type.");
        }

        liType = lpTriggerQueries->GetNextEvent(lpEvent, &lpEvent, &liSize);
    }
}

}

// ============================================================================
// FOLDED FROM BrnWorldModule_wG_Bridges_02.cpp (wave G) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===========================================================================
// BrnWorldModule_wG_Bridges_02.cpp -- two WorldModule per-frame output bridges:
// the traffic module's pre-scene score-target list posted as a GUI event, and the
// crash module's post-physics network interface + game events merged into the
// update output buffer.
//
// Source getter first, then destination getter, then the transfer -- the getters
// are lock tripwires, so the order is observable. Neither console body carries a
// null compare or an assert.
// FLAG: the console brackets the traffic bridge in a CPU perf monitor taken out of
// the world-module context, which arrives here as an untyped pointer; the monitor is
// not modelled, the standing disposition of every landed sibling in Bridges/.
// ===========================================================================


namespace WorldModule
{

// ---------------------------------------------------------------------------
// WorldModule::BridgeTrafficEntityInfoToOutput_PreScene
//
// Copy the traffic module's score-target array out of its pre-scene output buffer
// (a bare-displacement read on the console, no lock check) into a local, then post
// the local as one GUI traffic-car-info event on the update output buffer's GUI event
// queue (write-lock getter). The event carries no header: the payload IS the array,
// and the console's byte count is the array's size (656).
// The record id 208 is the console's immediate; the same event type in the
// bare-array form the GUI cache reads its score targets from.
// ---------------------------------------------------------------------------
void BridgeTrafficEntityInfoToOutput_PreScene(
    void* lpWorldModule,
    BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
    const BrnTraffic::BrnTrafficIO::OutputBuffer_PreScene* lpTrafficOutput_PreScene)
{
    (void)lpWorldModule;   // read only for the perf-monitor handle, which is not modelled

    BrnTraffic::BrnTrafficIO::ScoringVehicleArray lScoreTargets =
        *lpTrafficOutput_PreScene->GetPotentialScorees();

    lpOutputBuffer->GetGuiEventQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lScoreTargets),
        208,
        static_cast<s32>(sizeof(lScoreTargets)));
}

// ---------------------------------------------------------------------------
// WorldModule::BridgeCrashModuleToOutput
//
// Two transfers from the crash module's post-physics output buffer into the update
// output buffer: latch the crash network output interface (the setter clears the
// destination queue and appends the source's owned crashing-traffic updates), then
// append the crash module's game-event queue into the world's.
// ---------------------------------------------------------------------------
void BridgeCrashModuleToOutput(
    void* lpWorldModule,
    BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
    const BrnWorld::CrashIO::OutputBuffer_PostPhysics* lpCrashOutput_PostPhysics)
{
    (void)lpWorldModule;   // copied out of its register and never read

    // Source getter (read-lock) first, destination setter (write-lock) second.
    const BrnWorld::CrashIO::NetworkOutputInterface* lpNetworkOutput =
        lpCrashOutput_PostPhysics->GetNetworkOutputInterface();
    lpOutputBuffer->SetCrashNetworkOutputInterface(lpNetworkOutput);

    // Same order for the queue leg: source getter, then destination getter, then Append.
    const BrnWorld::CrashIO::OutputBuffer_PostPhysics::GameEventQueue* lpSourceEvents =
        lpCrashOutput_PostPhysics->GetGameEventQueue();
    lpOutputBuffer->GetGameEventQueue()->Append(*lpSourceEvents);
}

}

// ============================================================================
// FOLDED FROM BrnWorldModule_wG_Bridges_03.cpp (wave G) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===========================================================================
// BrnWorldModule_wG_Bridges_03.cpp -- four WorldModule per-frame bridge seams: two
// that stage a module's own query queues into the scene manager's query input buffer,
// one that fans the scene manager's results back out to the traffic module's two
// pre-physics input buffers, and one that posts the traffic module's removed-car list
// to the GUI as a single update-output event.
//
// Source getter first, then destination getter, then the merge -- both getters are
// lock tripwires, so the order is observable. Null tripwires appear only where the
// console body has one.
// FLAG: the console brackets these in CPU perf monitors taken out of the world-module
// context, which arrives here as an untyped pointer; the monitors are not modelled,
// the standing disposition of every landed sibling in Bridges/.
// ===========================================================================


namespace WorldModule
{

// ---------------------------------------------------------------------------
// WorldModule::BridgeRaceCarModuleToSceneModule_PostScene
//
// Two merges, in the console's order: the race car's coarse query queue into the
// scene query input buffer's coarse queue, then its fine line-test queue into the
// matching typed fine queue. Each leg is source getter (read-lock) then destination
// getter (write-lock) then the queue's own Append -- the coarse pair are the same
// variable-event-queue instantiation, the fine pair the same typed event queue.
//
// Both null tripwires are the console's and both are NON-gating: it fires and falls
// through into the transfer.
//
// The coarse source DERIVES from the shared variable-event-queue instantiation
// (InCoarseQueryQueue<16384> adds only enqueue helpers), so it is bound to the base
// reference explicitly rather than deduced through the derived type -- the same
// binding the landed BridgeRaceCarEntityInfoToOutput_PrePhysics needs.
// ---------------------------------------------------------------------------
void BridgeRaceCarModuleToSceneModule_PostScene(
    void* lpWorldModule,
    CgsSceneManager::SceneManagerIO::InputBuffer_Query* lpSceneQueryInput,
    const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostScene* lpRaceCarOutputBuffer_PostScene)
{
    (void)lpWorldModule;   // the world-module context: copied out of its register and never read

    CGS_ASSERT(lpSceneQueryInput != 0, "lpSceneModuleInputBuffer != NULL");                    // :123
    CGS_ASSERT(lpRaceCarOutputBuffer_PostScene != 0, "lpRaceCarOutputBuffer_PostScene != NULL"); // :124

    const CgsModule::VariableEventQueue<16384, 16>& lrCoarseSource =
        *lpRaceCarOutputBuffer_PostScene->GetSceneCoarseQueryQueue();
    lpSceneQueryInput->GetCoarseQueryQueue()->Append(lrCoarseSource);

    lpSceneQueryInput->GetFineLineTestQueue()->Append(
        *lpRaceCarOutputBuffer_PostScene->GetSceneFineLineTestQueue());
}

// ---------------------------------------------------------------------------
// WorldModule::BridgeTrafficModuleToSceneModule_PostScene
//
// The traffic module's half of the same staging, coarse queue only: its post-scene
// output buffer carries no fine line-test queue, so there is one merge and one
// tripwire. The console asserts the DESTINATION only -- there is no compare on the
// traffic output buffer at all, and the assert that is there is non-gating.
// ---------------------------------------------------------------------------
void BridgeTrafficModuleToSceneModule_PostScene(
    void* lpWorldModule,
    CgsSceneManager::SceneManagerIO::InputBuffer_Query* lpSceneQueryInput,
    const BrnTraffic::BrnTrafficIO::OutputBuffer_PostScene* lpTrafficOutputBuffer_PostScene)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpSceneQueryInput != 0, "lpSceneModuleInputBuffer != NULL");   // :144

    const CgsModule::VariableEventQueue<16384, 16>& lrCoarseSource =
        *lpTrafficOutputBuffer_PostScene->GetSceneCoarseQueryQueue();

    lpSceneQueryInput->GetCoarseQueryQueue()->Append(lrCoarseSource);
}

// ---------------------------------------------------------------------------
// WorldModule::BridgeSceneQueryResultsToTrafficModule_PrePhysics
//
// The traffic module's half of the scene-query results fan-out, and the only one of
// the family with TWO destinations: the SAME results ring is appended onto the
// traffic module's pre-physics scene-result queue and onto its post-physics one.
// Both are the same variable-event-queue instantiation as the source, so each merge
// is the queue's own Append. The source getter runs again for the second leg -- it is
// a read-lock tripwire, so the repeat is observable and kept.
//
// Only the SOURCE has a null tripwire on the console (and it is non-gating); neither
// destination is compared.
//
// ⚠️ MEASURED, and left as it stands: the console reaches its second argument's
// scene-result queue at +166960 and its third argument's through the other buffer's
// accessor, i.e. the two traffic input buffers arrive in the opposite order to the
// spelling this declaration has carried since it was first declared. Because BOTH
// destinations receive an Append of the SAME source ring, the two orders are
// byte-identical in effect, so the parameter names are left alone rather than
// churning the call site in BrnWorldModule.cpp for a no-op.
// ---------------------------------------------------------------------------
void BridgeSceneQueryResultsToTrafficModule_PrePhysics(
    void* lpWorldModule,
    BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics* lpTrafficInputBuffer_PostPhysics,
    BrnTraffic::BrnTrafficIO::InputBuffer_PrePhysics* lpTrafficInputBuffer_PrePhysics,
    const CgsSceneManager::SceneManagerIO::OutputBuffer* lpSceneQueryOutput)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpSceneQueryOutput != 0, "lpSceneModuleOutputBuffer != NULL");   // :119

    lpTrafficInputBuffer_PostPhysics->GetSceneResultQueue()->Append(
        *lpSceneQueryOutput->GetSceneQueryResultsQueue());

    lpTrafficInputBuffer_PrePhysics->GetSceneResultQueue()->Append(
        *lpSceneQueryOutput->GetSceneQueryResultsQueue());
}

// ---------------------------------------------------------------------------
// WorldModule::BridgeTrafficCarEntityInfoToOutput_PrePhysics
//
// The only bridge in this TU that BUILDS a record instead of merging a queue: it
// walks the traffic module's remove-crashed-traffic request queue, pulls the 14-bit
// entity index out of each request's volume-instance handle, and posts the whole
// list to the GUI as one event on the world update-output buffer's GUI event queue.
//
// The whole body is gated on the pre-physics output buffer's showtime flag -- when it
// is clear nothing is read and nothing is posted (this IS a gate, unlike the asserts).
//
// Both null tripwires are the console's and both are NON-gating; the strings are
// verbatim ("lpOutputBuffer" carries no "!= NULL" tail, as on its pre-physics sibling).
//
// The record is an Array<short,25> by its own byte image: the console Clears the count
// word, Appends one short per request through Array<short,25>::Append, and passes 56 --
// sizeof(Array<short,25>) -- as the event size. The record id 209 is the console's
// immediate, kept a literal exactly as its 208 sibling
// (BridgeTrafficEntityInfoToOutput_PreScene) keeps its own.
//
// The loop re-reads the queue length each iteration, as the console does; the guard is
// a separate ">0" test before the first pass (a do/while walk under an if).
// ---------------------------------------------------------------------------
void BridgeTrafficCarEntityInfoToOutput_PrePhysics(
    void* lpWorldModule,
    BrnWorldIO::UpdateOutputBuffer* lpOutputBuffer,
    const BrnTraffic::BrnTrafficIO::OutputBuffer_PrePhysics* lpTrafficOutput_PrePhysics)
{
    (void)lpWorldModule;   // read only for the perf-monitor handle, which is not modelled

    CGS_ASSERT(lpOutputBuffer != 0, "lpOutputBuffer");                              // :355
    CGS_ASSERT(lpTrafficOutput_PrePhysics != 0, "lpTrafficOutput_PrePhysics");      // :356

    if (!lpTrafficOutput_PrePhysics->GetPlayingShowtime())
        return;

    const BrnPhysics::Vehicle::VehicleInputInterface::RemoveTrafficEventQueue* lpRemoveRequests =
        lpTrafficOutput_PrePhysics->GetVehicleInputInterface()->GetRemoveTrafficEvents();

    Array<short, 25> lCrashedCars;
    static_assert(sizeof(lCrashedCars) == 56,
                  "the GUI record's byte image must match the console's baked event size");
    lCrashedCars.Clear();

    if (lpRemoveRequests->GetLength() > 0)
    {
        s32 liIndex = 0;
        do
        {
            const short lsEntityIndex = static_cast<short>(
                lpRemoveRequests->GetEvent(liIndex).mVolumeInstanceID.GetEntityIDEntityIndex());
            lCrashedCars.Append(lsEntityIndex);
            ++liIndex;
        }
        while (liIndex < lpRemoveRequests->GetLength());
    }

    lpOutputBuffer->GetGuiEventQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lCrashedCars),
        209,
        static_cast<s32>(sizeof(lCrashedCars)));
}

// ---------------------------------------------------------------------------
// WorldModule::BridgeRaceCarModuleToTrafficModule_PostScene
//
// One publish: read the race car's post-scene race-car-to-traffic interface out of its
// own output buffer (read-lock getter) and hand it to the traffic module's post-scene
// input buffer (write-lock setter, which Clear+Appends the two rival queues onto its
// member seat and copies the flag word and the showtime density scale).
//
// Both null tripwires are the console's and both are NON-gating; the strings are
// verbatim (neither carries a "!= NULL" tail).
// ---------------------------------------------------------------------------
void BridgeRaceCarModuleToTrafficModule_PostScene(
    void* lpWorldModule,
    BrnTraffic::BrnTrafficIO::InputBuffer_PostScene* lpTrafficInputBuffer_PostScene,
    const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostScene* lpRaceCarOutputBuffer_PostScene)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpTrafficInputBuffer_PostScene != 0, "lpTrafficInputBuffer_PostScene");   // :61
    CGS_ASSERT(lpRaceCarOutputBuffer_PostScene != 0, "lpRaceCarOutputBuffer_PostScene"); // :62

    // Source getter (read-lock) first, destination setter (write-lock) second -- the
    // console's order, and both are lock tripwires.
    lpTrafficInputBuffer_PostScene->SetRaceCarToTrafficInterface(
        lpRaceCarOutputBuffer_PostScene->GetRaceCarToTrafficInterface());
}

}   // namespace WorldModule
