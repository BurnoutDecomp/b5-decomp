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

#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"   // CgsSystem::TimerStatus{,Interface} -- WorldModule::Update stages the environment frame delta

#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // VariableEventQueue<4096,16>::AddEvent
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"       // CgsResource::ID::HashString
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"       // BrnResource::GameDataIO::RequestInterface<4096>

#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"     // DispatchFrame / DispatchList
#include "rw/math/vpu/vector3_operation.h"   // rw::math::vpu::operator- / Magnitude (CalculateVehicleLODs)
#include "GameSource/Director/Camera/Utils/CameraUtils.h" // Utils::GetZoomFromFOVDegs (the bring-up LOD zoom)
#include "GameShared/GameClasses/System/Timer/CgsFrameInterpolation.h" // ⚠️ FLAG PC QoL: GetFrameSeconds (the tour camera's advance)
#include <cmath>    // sqrtf / tanf ([FLAG PC bring-up] the dispatch producer's camera) + std::sqrt (vehicle LODs)
#include <cstddef>  // offsetof (the VehicleRenderInfo layout pins)

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
namespace renderengine { extern s32 gEnvironmentMap; extern s32 gEnvironmentMap30Hz; }

namespace BrnWorld
{

// qword_8300E9B8: X360 static initializer @0x82C6A9D8 hashes this exact text;
// WorldModule::Prepare loads the resulting 64-bit key at @0x827D5B6C.
static const u64 gs_uSurfaceListKey = Attrib::StringToKey("340654");

static CgsSceneManager::SceneQueryId KA_FRUSTUM_QUERY_IDS[11];
static CgsGraphics::Camera gFrustumQueryCamera;

// ShadowMap::GetFrustum -- the per-cascade cull volume this file's shadow queries submit --
// now lives in its canonical home next to GetCascadeCamera (BrnShadowMap.cpp), relocated
// there by the conductor once the concurrent ShadowMap wave released that file.

// The dispatch-pass camera (X360 file static at 0x8300FB40).
static CgsGraphics::Camera gDispatchCamera;

// [FLAG PC bring-up] The per-frame shader-constants frame + dispatch output buffer that
// WorldModule::GenerateDispatchListsBringUp hands to the REAL
// WorldModule::SetupShaderConstantsBeforeRendering @0x827D1410.
//
// STANDS IN FOR lpDispatchInputBuffer->GetShaderConstantsFrame() and the
// BrnWorldIO::DispatchOutputBuffer the console's GenerateDispatchLists CreateIOBuffer's --
// neither the renderer/world dispatch IO buffer set nor the renderer's own
// maShaderConstantsFrames[] is reachable from this producer (the renderer owns that array
// privately, and it is the same seam gBrnSkyCameraBringUp already crosses).
//
// The frame is EXPORTED (BrnShaderConstantsFrame.h) because it now carries the real sky /
// cloud / key-light set that BrnRendererModule::PublishSkyConstantsBringUp currently
// hard-codes from the noon keyframe: that publisher becomes a copy of this frame in the
// follow-up change, and only then can it be retired. The output buffer stays private --
// SetupShaderConstantsBeforeRendering publishes the same eight values into the global
// shader-constant table itself, which is what the bring-up producer's passes read.
//
// DELETE both with GenerateDispatchListsBringUp.
// ⚠ Defined at GLOBAL scope, closing BrnWorld for two lines: BrnShaderConstantsFrame.h declares
// them `extern` outside any namespace (like gBrnSkyCameraBringUp, the precedent this seam copies),
// and a definition inside BrnWorld would be a DIFFERENT entity -- the consumer's UNDEF would never
// resolve (step-9 worldconst verify, finding 1).
}   // namespace BrnWorld (reopened below)
BrnShaderConstantsFrame gBrnWorldShaderConstantsFrameBringUp;
bool                    gbBrnWorldShaderConstantsFrameBringUpValid = false;
namespace BrnWorld
{
static BrnWorldIO::DispatchOutputBuffer gWorldDispatchOutputBringUp;

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

    // [FLAG PC bring-up] the director-camera override starts empty (see the header).
    mbBringUpCameraOverrideValid = false;
    mfBringUpCameraOverrideFOV   = 0.0f;
    mBringUpCameraOverride.SetIdentity();

    // [FLAG PC bring-up] ...and so do the four staged world effects frames. Until
    // BrnGameModule::DoDispatch stages them (which it cannot do before the renderer's
    // arbitrator is Constructed) the producer below must not call GenerateEffects.
    for ( s32 liSlot = 0; liSlot < 4; liSlot++ )
    {
        mapBringUpEffectsFrames[ liSlot ] = 0;
    }

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
    // [FLAG PC bring-up] the env-map producer seams (see BrnWorldModule.h).
    mpBringUpDispatchThreadInputBuffer = 0;
    mpBringUpCoronaSubmissionInterface = 0;
    mbEnvMapCamerasPositionedBringUp   = false;
    mbBringUpCameraInJunkyardBringUp   = false;
    // [FLAG PC bring-up] the time-of-day request staging (see BrnWorldModule.h). Cleared,
    // which is what Camera::Clear()/CameraEffects::Construct() leaves on the console's own
    // mLastCameraInput before the director ever publishes -- so "no request" is the state.
    mbBringUpCameraSetTimeOfDayBringUp   = false;
    mfBringUpCameraTimeOfDayHoursBringUp = 0.0f;
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
            mbEnvMapCamerasPositionedBringUp = true;
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
// While it was an inert stub in WorldLinkStubs.cpp every race car rendered at the
// LOD that RenderParams::Reset seeds -- E_STATE_LOD_4, the coarsest of the five --
// permanently, and every body part whose model carries only 2 or 3 states failed
// DoesStateExist(4) and did not render at all. Reset's `4` is console-faithful (X360
// Reset @0x822E6818 stores 4 into +5120): it is the deliberate "past every threshold"
// fallback that THIS function is expected to lift off every frame.
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
            lProjectedCamera.UpdatePerspectiveProjectionMatrix();

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
            CgsGraphics::CameraRwFrustum lFaceRwFrustum;
            mEnvironmentMap.maEnvMapCameras[ liFace ].GetFrustumPerspective( lFaceRwFrustum, true );
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
    // Construct(), NOT Release(): GenerateDispatchLists @0x827D1CE8 is one of the nine
    // xrefs of sub_827F94E8 == Camera::Construct() (2026-08-17; Release() is the empty
    // @0x8284CB38 -- see the CgsCamera.cpp banner).
    gDispatchCamera.Construct();
    lpCameraInput->CopyToCgsCamera( &gDispatchCamera );
    lpDispatchThreadInputBuffer->SetCameraViewProjection(
        gDispatchCamera.GetViewProjectionMatrix() );

    // ---- global shader constants for the frame -----------------------------
    // X360 @0x827D2074-0x827D20B8. The argument order is the asm's, not the pseudocode's:
    // (viewProjection, cameraTransform, GAME time, SIM time, frame, output buffer). The
    // camera transform argument is the director Camera* itself on the console -- its
    // mTransform is the object's first member -- which is what GetTransform() returns here.
    //
    // Only the OUTPUT BUFFER is locked, exactly as the console does it: the frame's write
    // lock belongs to BrnRendererModule::Update, which is not reconstructed, so on this
    // build the real path (which has no callers -- GenerateDispatchListsBringUp is the live
    // producer) would trip the frame setters' lock assert. That is the console's own
    // discipline reproduced, not an omission; the bring-up seam brackets its own frame.
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
            PerfMonCpu::StopMonitor( miGenerateDispatchListsPM );

            PerfMonCpu::StartMonitor( miPropGenerateDispListClearPM );
            mPropEntityModule.GenerateDispatchLists(
                lpPropDispatchInput, lpFilteredEntityData->maPropEntityIds,
                lFaceCamera.GetViewProjectionMatrix(), gDispatchCamera.GetPosition(),
                // Env-map face: X360 @0x827D2C58..70 writes @0x6F = 1 / @0x77 = 0 --
                // rendering the environment map, and no coronas on it.
                1.0f, &mShaderLodInfo, 5 + liFace, 5 + liFace, 5 + liFace, true, false );
            PerfMonCpu::StopMonitor( miPropGenerateDispListClearPM );

            // Refresh the face camera's projection for the renderer (far 10000)
            // and record its view-projection for the env-map resolve -- ON THE
            // LOCAL COPY (X360 v219): the member maEnvMapCameras[face] is never
            // mutated by the dispatch pass.
            lFaceCamera.SetFarClipPlane( 10000.0f );   // the store + the rebuild, one member
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
                                       f32 lfFOVDegrees,
                                       bool lbIsInJunkyard,
                                       bool lbSetTimeOfDay,
                                       f32 lfTimeOfDayHours )
{
    mBringUpCameraOverride       = lrTransform;
    mfBringUpCameraOverrideFOV   = lfFOVDegrees;
    mbBringUpCameraOverrideValid = true;
    // LEVEL, not one-shot (see the header): the junkyard state is a property of the game,
    // not of this frame's camera publish, and the console's camera input is never absent.
    mbBringUpCameraInJunkyardBringUp = lbIsInJunkyard;
    // LEVEL for the same reason: on the console these two ride the camera record itself, so
    // they persist exactly as long as the director keeps publishing a camera that carries
    // them. See the header entry.
    mbBringUpCameraSetTimeOfDayBringUp   = lbSetTimeOfDay;
    mfBringUpCameraTimeOfDayHoursBringUp = lfTimeOfDayHours;
}

// [FLAG PC bring-up] see the header. STANDS IN FOR the four
// BrnWorldIO::DispatchInputBuffer::SetEffectsFrame @0x823B6BD8 calls that
// BrnGameModule::BridgeRendererToWorld @0x823CDD20 makes (GameBridgeRendererToX.cpp:50),
// whose source is BrnRendererModule::Update @0x82405E28 line 110
// (mEffectsArbitrator.GetExternalEffectsFrame(KU_EFFECTS_LAYER_WORLD, luSlot)).
// Unlike the camera override this is NOT one-shot: the console's dispatch input buffer holds
// the frames for the whole pass and DoDispatch re-stages them every frame, so they are simply
// overwritten rather than consumed-and-cleared.
// DELETE-WHEN the RendererIO/BrnWorldIO dispatch buffer set is created on PC.
void
WorldModule::SetBringUpEffectsFrames( BrnEffectsFrame* lpFrame0, BrnEffectsFrame* lpFrame1,
                                      BrnEffectsFrame* lpFrame2, BrnEffectsFrame* lpFrame3 )
{
    mapBringUpEffectsFrames[ 0 ] = lpFrame0;
    mapBringUpEffectsFrames[ 1 ] = lpFrame1;
    mapBringUpEffectsFrames[ 2 ] = lpFrame2;
    mapBringUpEffectsFrames[ 3 ] = lpFrame3;
}

// [FLAG PC bring-up] see the header. STANDS IN FOR
// BrnWorldIO::DispatchInputBuffer::SetDispatchThreadInputBuffer @0x823B5408, which
// BrnGameModule::DoDispatch @0x823DC458 calls on the world dispatch input buffer; the real
// GenerateDispatchLists reads it back at :3725 and writes the six env-map face-rendered
// flags through it at :4018. Overwritten every frame, never consumed.
// DELETE-WHEN DoDispatch's IO buffer set is real.
void
WorldModule::SetBringUpDispatchThreadInputBuffer( BrnGame::DispatchThreadInputBuffer* lpBuffer )
{
    mpBringUpDispatchThreadInputBuffer = lpBuffer;
}

// [FLAG PC bring-up] see the header. STANDS IN FOR the console's RendererIO -> BrnWorldIO ->
// RaceCarEntityModuleIO copy chain of the corona submission interface (the last hop is
// InputBuffer_GenerateDispatchLists::SetCoronaSubmissionInterface @0x8279EBC8, applied by
// GenerateDispatchListsBringUp beside SetShadowMap every frame). Overwritten every frame.
// DELETE-WHEN DoDispatch's IO buffer set is real.
void
WorldModule::SetBringUpCoronaSubmissionInterface( BrnCoronaManager::BrnSubmissionInterface* lpInterface )
{
    mpBringUpCoronaSubmissionInterface = lpInterface;
}

// =============================================================================
// ⭐ [DIAG shadow-perf wave 2026-08-12] THE PRODUCER PHASE TIMERS.
//
// WHY IT WAS BUILT: adding the shadow-producer arm took the dispatch loop from ~47-49
// producerFps to ~16.6, and the record counts did not explain it (713 main-view records
// + 277/104/75 cascade == 1.6x the records for 3x the frame). These timers split the
// producer into its phases so the cost could be attributed instead of guessed at.
//
// ⭐ THE ANSWER, MEASURED 2026-08-12 -- READ THIS BEFORE RE-INVESTIGATING.
// THE PRODUCER IS NOT THE BOTTLENECK. One boot at 120-frame averages:
//
//   frame=60360.6  total=522.8            <-- the whole producer is 0.9% of the frame
//   cam=21.3  stage=1.2  query=136.8
//   mainFilter=6.6 mainWorld=164.2 mainCar=17.6 mainProp=61.6 propCache=13.0
//   casc=62.8 (cascFilter=2.4 cascCar=27.5 cascWorld=31.6)
//   mainEnt=978 mainRec=713 cascEnt=206/30/0 cascWorldRec=202/29/0
//   cascCarRec=75/75/75 cascPadPlanes=0/2/1 | jobPool=2000/8192 batches=4
//
// The ENTIRE cascade arm is 63 us. `frame - total` ~= 59.8 ms is DOWNSTREAM of this
// function -- the renderer, whose shadow pass carries the extra draw calls the cascade
// lists produce. Do not look for the regression in this file; it is not here.
//
// Three hypotheses this run KILLED, recorded so they are not re-run:
//   * "a cascade volume is all clear-plane sentinels and culls nothing" -- NO.
//     cascPadPlanes=0/2/1, never 8. The fits are real.
//   * "the cascades all share one volume" -- NO, not on this build.
//     cascEnt=206/30/0 genuinely narrows per slab. (It WAS true earlier the same day,
//     when CalcVertices was still an inert stub writing nothing into
//     ComputeBoundingBoxMatrix's uninitialised corner array; the CalcVertices +
//     SetFromRwFrustum-permutation + far-clip fixes changed this path materially.)
//   * "the shared job-result pool is overflowing and truncating cascades" -- NO.
//     jobPool=2000/8192 across all four batches. Comfortable headroom.
//
// WHAT THE FIELDS ARE FOR NOW: the cascade volume fit is actively being worked in
// BrnShadowMap.cpp and has already changed twice, so cascPadPlanes / cascEnt / jobPool
// are kept as REGRESSION TRIPWIRES -- they are the cheapest way to notice a fit that
// silently starts culling nothing (padPlanes -> 8), a volume that balloons (cascEnt >>
// mainEnt), or a pool that starts truncating. `frame` vs `total` stays the
// producer-versus-downstream discriminator. cascCarRec is constant by construction
// (ShadowMap::Construct leaves mbRenderRaceCarsNearOnly false, so the car is
// re-dispatched into every cascade) and must never be read as caster work.
//
// ⚠️ LATCHED ON A VALUE (the accumulated frame count), never on a `static bool`
// one-shot: this function runs on the loading screen before the world exists, and a
// one-shot probe there fires once into an empty world and then never again --
// indistinguishable in the log from "this code was never reached" (the project has
// learned this three times). The accumulator is only advanced on frames that get
// past GetLoadedWorldBounds, i.e. real producer frames, and it reprints forever.
//
// Every number is MICROSECONDS PER FRAME, averaged over the reporting window.
// DELETE with GenerateDispatchListsBringUp.
// =============================================================================
namespace
{
    typedef std::chrono::steady_clock ShadowPerfClock;

    inline ShadowPerfClock::time_point ShadowPerfNow()
    {
        return ShadowPerfClock::now();
    }

    inline f64 ShadowPerfUsSince( const ShadowPerfClock::time_point& lrFrom )
    {
        return std::chrono::duration< f64, std::micro >( ShadowPerfClock::now() - lrFrom ).count();
    }

    struct ShadowPerfAccumulator
    {
        // ---- accumulated microseconds over the window ----
        f64 mfTotalUs;          // the whole producer body
        f64 mfCamerasUs;        // PART 1: CalculateShadowMapCameras + the finiteness tripwire
        f64 mfStageUs;          // frustum build + IO Construct + the four query events
        f64 mfQueryUs;          // ProcessFrustumTestJobRequests + ...Results (the octree walks)
        f64 mfMainFilterUs;     // main view: FilterFrustumTestResults + buffer seeding
        f64 mfMainWorldUs;      // main view: WorldEntityModule::GenerateDispatchLists
        f64 mfMainCarUs;        // main view: CalculateVehicleLODs + the race-car leg
        f64 mfMainPropUs;       // main view: the prop leg
        f64 mfMainTrafficUs;    // main view: the traffic dispatch leg
        f64 mfPropCacheUs;      // PropEntityModule::CachePropGraphicsLists (same-day sibling
                                // change -- timed so the arm is not blamed for its cost)
        f64 mfCascadeUs;        // PART 2 in total (loop overhead included)
        f64 mfCascadeFilterUs;  // ...of which: the three per-cascade filters + seeding
        f64 mfCascadeCarUs;     // ...of which: the three per-cascade race-car legs
        f64 mfCascadeWorldUs;   // ...of which: the three per-cascade world legs
        // [reflections step 1] PART 3, the environment-map faces. Query STAGING for the
        // faces rides mfStageUs and their octree walks ride mfQueryUs (they are submitted
        // in the same batch as the main view and the cascades); this bucket is the arm
        // itself -- the per-face filter, the world leg and the prop leg.
        f64 mfEnvMapUs;

        f64 mfFramePeriodUs;    // producer entry -> next producer entry (the WHOLE frame), so
                                // the producer's share of it is readable off one line

        // ---- last-frame counts (not averaged -- a snapshot of the reporting frame) ----
        s32 miMainWorldEnts;
        s32 miMainWorldRecords;
        s32 maiCascadeWorldEnts[ 3 ];
        s32 maiCascadeCarRecords[ 3 ];    // split out: the race car is a FIXED per-cascade cost
        s32 maiCascadeWorldRecords[ 3 ];  // ...so it must not be confused with caster work
        s32 maiCascadePadPlanes[ 3 ];     // ComputeOptimalViewVolume clear-plane sentinels
        s32 miEnvMapFaces;      // env-map faces STAGED this frame (0 / 3 / 6)
        s32 miEnvMapWorldEnts;  // world entities the staged face filters produced, summed
        s32 miEnvMapRecords;    // records the face legs added to lists 5..10, summed
        s32 miPoolResults;      // sum of every result batch's miNumResults this frame
        s32 maiPoolPerJob[ 4 ]; // ...and per JOB (batch i -> job i/4), reflections step 1:
                                // with 10 queries the batches no longer all land in job 0,
                                // and the 8192 cap is PER JOB (verify F4, envproducer)
        s32 miPoolMaxJob;       // the fullest job's count this frame -- the value to compare
        s32 miPoolBatches;
        s32 miArmed;

        s32 miFrames;

        void ResetWindow()
        {
            mfTotalUs = 0.0; mfCamerasUs = 0.0; mfStageUs = 0.0; mfQueryUs = 0.0;
            mfMainFilterUs = 0.0; mfMainWorldUs = 0.0; mfMainCarUs = 0.0; mfMainPropUs = 0.0;
            mfMainTrafficUs = 0.0;
            mfPropCacheUs = 0.0;
            mfCascadeUs = 0.0; mfCascadeFilterUs = 0.0; mfCascadeCarUs = 0.0;
            mfCascadeWorldUs = 0.0; mfFramePeriodUs = 0.0;
            mfEnvMapUs = 0.0;
            miFrames = 0;
        }
    };

    ShadowPerfAccumulator gShadowPerf = {};

    // The job-result pools the queries SHARE. LooseOctree::KU_JOB_RESULT_BUFFER_SIZE is
    // 8192 u16 entries and muCurrentWriteOffset is per-JOB cumulative; SceneManagerModule's
    // jobIndex map is q*4/16 == q/4, so with the FOUR pre-reflections queries everything
    // landed in job 0 and the headroom question was "main + three cascades in 8192". Since
    // reflections step 1 stages up to TEN (main, six env-map faces, three cascades):
    // main+face0..2 -> job 0, face3..5+cascade0 -> job 1, cascade1..2 -> job 2, so the
    // cap is compared PER JOB (maiPoolPerJob / miPoolMaxJob), keyed by batch index / 4
    // (batch order == submission order). PushCoarseResult drops SILENTLY past the cap, so a
    // per-job count pinned at 8192 == truncation of that job's queries.
    const s32 KI_SHADOW_PERF_POOL_CAPACITY = 8192;

    // ---- the CLEAR-PLANE SENTINEL CENSUS ------------------------------------------
    // ComputeOptimalViewVolume pads any candidate-plane slot it could not fill with its
    // own saClearPlanes defaults {N, D = -1000000}, and CgsGeometric::Frustum::
    // SetPlaneByIndex stores the NEGATION of the plane handed to it (VectorToPlane's
    // whole-vector vxor against the 0x80000000 splat -- re-confirmed 2026-08-12 by the
    // recovered CalcVertices de-swizzle, which reproduces the same negate independently).
    // So a padded slot reads back with an offset lane of +1e6, and LooseOctree's
    // `Nx*cx + Ny*cy + Nz*cz - D > R` evaluates to `-N.C - 1e6`, which is INSIDE for
    // every finite point: a padded slot cannot reject anything. Counting them is a DIRECT
    // read of "the fit ran out of real planes", instead of inferring it from entity counts.
    //
    // ⚠ NOT to be confused with SetFromRwFrustum's pad lanes. That writer -- the
    // PERSPECTIVE path, used by the main view -- fills slots 6/7 by DUPLICATING far and
    // near (CgsFrustum.cpp:331), which reject exactly when slots 4/5 already do. That is a
    // different, benign padding scheme on a different frustum; this census only ever looks
    // at the cascade volumes ShadowMap::GetFrustum(i) hands back.
    const f32 KF_SHADOW_PERF_CLEAR_PLANE_THRESHOLD = 500000.0f;

    inline f32 ShadowPerfLaneGet( const Vector4& lrLane, u32 luLane )
    {
        switch ( luLane )
        {
            case 0:  return lrLane.x;
            case 1:  return lrLane.y;
            case 2:  return lrLane.z;
            default: return lrLane.w;
        }
    }

    // How many of the eight stored planes are clear-plane sentinels. The lanes are
    // struct-of-arrays -- two batches of four planes, each batch {Nx, Ny, Nz, offset} --
    // so plane p's offset is float lane (p & 3) of maSwizzledPlanes[(p >= 4 ? 4 : 0) + 3].
    s32 ShadowPerfCountClearPlanes( const CgsGeometric::Frustum& lrFrustum )
    {
        s32 liCount = 0;
        for ( u32 luPlane = 0; luPlane < 8; luPlane++ )
        {
            const u32 luBatch  = ( luPlane >= 4 ) ? 4u : 0u;
            const f32 lfOffset =
                ShadowPerfLaneGet( lrFrustum.maSwizzledPlanes[ luBatch + 3 ], luPlane & 3u );
            if ( lfOffset > KF_SHADOW_PERF_CLEAR_PLANE_THRESHOLD
                 || lfOffset < -KF_SHADOW_PERF_CLEAR_PLANE_THRESHOLD )
            {
                ++liCount;
            }
        }
        return liCount;
    }

    // (A BRN_SHADOW_ARM=0..3 A/B ladder lived here during the investigation, gating the
    // cascade cameras / queries / dispatch legs so the arm's cost could be attributed by
    // subtraction across boots. REMOVED once `total` answered the question directly:
    // at 523 us of a 60,361 us frame there is nothing left to attribute, and it was the
    // only diagnostic in this file that sat in LIVE CONTROL FLOW -- a stray environment
    // variable could silently disable the shadow arm and turn "why are there no cascades"
    // into a debugging trap. Every probe that remains is pure observation.)
}

// [FLAG PC bring-up] Finiteness tripwire for the shadow-producer arm below. See the
// HONEST GATE banner there: while ShadowMap::Construct's per-slot ortho scale is
// un-recovered rodata carried as zero, UpdateOrthogonalProjectionMatrix divides 1.0f
// by it and every cascade view-projection comes out +inf. Handing inf/NaN to
// LooseOctree::AddJobFrustumTest is not a faithful query, so the arm parks instead.
// DELETE with GenerateDispatchListsBringUp.
static bool IsFiniteMatrix44BringUp( const Matrix44& lrMatrix )
{
    const f32* lpfLanes = &lrMatrix.xAxis.x;
    for ( s32 liLane = 0; liLane < 16; liLane++ )
    {
        if ( !std::isfinite( lpfLanes[ liLane ] ) )
        {
            return false;
        }
    }
    return true;
}

void
WorldModule::GenerateDispatchListsBringUp( CgsGraphics::DispatchFrame* lpDispatchFrame )
{
    if ( lpDispatchFrame == 0 )
    {
        return;
    }

    // [DIAG shadow-perf] the whole-producer stopwatch. Started here and BANKED only at the
    // bottom, so the early-out frames (no world delivered yet) never enter the average.
    const ShadowPerfClock::time_point lProducerStart = ShadowPerfNow();

    // [DIAG shadow-perf] the WHOLE-FRAME period, measured producer-entry to
    // producer-entry. Without it `total` cannot be read on its own: the arm's cost may
    // sit DOWNSTREAM of this function (the renderer expands GDL object lists 0/1/4 into
    // mesh lists and sorts them, both no-ops on the empty lists of the pre-arm build), and
    // `frame - total` is exactly that remainder. The very first frame has no predecessor,
    // so it contributes nothing rather than a bogus epoch-sized delta.
    {
        static ShadowPerfClock::time_point sPrevProducerEntry;
        static bool                        sbHavePrevProducerEntry = false;
        if ( sbHavePrevProducerEntry )
        {
            gShadowPerf.mfFramePeriodUs +=
                std::chrono::duration< f64, std::micro >( lProducerStart - sPrevProducerEntry ).count();
        }
        sPrevProducerEntry      = lProducerStart;
        sbHavePrevProducerEntry = true;
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

    // ---- [DIAG] BRN_WORLD_CAMTRAFFIC -- FRAME THE SWERVING TRAFFIC CAR ---------------------
    // NOT IN THE X360 BINARY. Capture-only, inert unless the variable is set, and DELETE-WHEN-
    // STABLE. Declared here (rather than beside the tour path below) because the director
    // override is consumed two lines down and this mode has to be able to keep it.
    //
    // WHY. The avoidance chain (UpdateParams_TryAvoidCrashing -> CalcSwerveAmount ->
    // CalcTargetPos -> MoveToTarget) is complete and its DECISION is measured by [T5-avoid],
    // but no frame had ever SHOWN a traffic car altering course -- every camera on this build
    // follows the player, who is by construction either crashed or driving away from the cars
    // reacting to him.
    //
    // ⛔⛔ IT DOES **NOT** IMPLY BRN_WORLD_CAMFREE, AND THAT IS THE WHOLE DESIGN. It did, for
    // one build, and the result was MEASURED: with the free camera armed from frame one, the
    // tour camera latches at the SPAWN point and never moves, so mCameraLastFrame -- which the
    // traffic module uses as its BEHAVIOUR CENTRE -- freezes there too. The [T-anchor] rung
    // showed the player 570 m away from a `cam` that had not moved since the junkyard, and the
    // run produced ZERO swerves and ZERO junction-FUP action where the SAME BINARY, unarmed,
    // produced seven swerves and nine RemoveVehicle removals. ⭐ BRN_WORLD_CAMFREE does not
    // merely change what is photographed; it changes the SIMULATION, because traffic behaviour
    // is keyed off the camera. So this mode leaves the director camera exactly alone until the
    // swerve happens, and only then takes the view.
    //
    // ⭐ AND IT THEN HOLDS STILL, also deliberately. A camera that TRACKS the car keeps it
    // centred, and a centred car cannot be seen to deviate -- the camera would cancel exactly
    // the evidence being gathered. A fixed wide shot at the place the swerve began lets the car
    // drive THROUGH the frame, which is the only framing in which a bent path is a visible fact
    // rather than an assertion.
    //
    // =1 latches on the first swerve of any kind (including the ordinary
    // get-out-of-the-player's-way one); =2 latches only on a miBehaviour==2 publish, i.e. the
    // CRASH-AVOIDANCE swerve, which is the arm the junction-FUP gate controls.
    static bool    sbPathLatched      = false;
    static bool    sbFramingCar       = false;
    static Vector3 sPathOrigin;
    static f32     sfPathRadius       = 1.0f;
    static f32     sfPathAngle        = 0.0f;
    static bool    sbSwerveCamLatched = false;
    static s32     siSwerveCam        = -1;
    if ( siSwerveCam < 0 )
    {
        const char* lpcSwerveEnv = std::getenv( "BRN_WORLD_CAMTRAFFIC" );
        siSwerveCam = ( lpcSwerveEnv != 0 && lpcSwerveEnv[0] != '0' )
                    ? ( lpcSwerveEnv[0] == '2' ? 2 : 1 )
                    : 0;
    }
    if ( siSwerveCam != 0 && !sbSwerveCamLatched )
    {
        const u32 luTrigger = ( siSwerveCam == 2 ) ? BrnTraffic::gSwerveWatch.muBehaviour2
                                                   : BrnTraffic::gSwerveWatch.muPublishes;
        if ( luTrigger != 0u )
        {
            sPathOrigin.x = BrnTraffic::gSwerveWatch.mfPosX;
            sPathOrigin.y = BrnTraffic::gSwerveWatch.mfPosY;
            sPathOrigin.z = BrnTraffic::gSwerveWatch.mfPosZ;
            sPathOrigin.w = 0.0f;
            sfPathRadius       = 9.0f;
            sbFramingCar       = true;
            sbPathLatched      = true;
            sbSwerveCamLatched = true;
            // Arms the back-buffer writer under BRN_FRAME_DUMP_ARM -- see BrnTrafficSwerveWatch.h.
            BrnTraffic::gSwerveWatch.muCameraLatched = 1u;
            if ( CgsDev::Log::gpDebugPrint != 0 )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[T5-swervecam] LATCHED on traffic vehicle "
                    << BrnTraffic::gSwerveWatch.miVehicle
                    << " behaviour " << BrnTraffic::gSwerveWatch.miBehaviour << " at ("
                    << sPathOrigin.x << ", " << sPathOrigin.y << ", " << sPathOrigin.z
                    << ") -- the camera is now STATIC; the car drives through the frame\n";
            }
        }
    }

    // Consume this frame's director-camera override (one frame only -- see the header).
    const bool                          lbUseDirectorCamera =
        mbBringUpCameraOverrideValid && ( siCamFree == 0 ) && !sbSwerveCamLatched;
    const rw::math::vpu::Matrix44Affine lDirectorTransform  = mBringUpCameraOverride;
    const f32                           lfDirectorFOVDegs   = mfBringUpCameraOverrideFOV;
    mbBringUpCameraOverrideValid = false;

    // ---- junkyard lighting latch (camera flag bit 0x400000) ----------------
    // ⭐ WIRED 2026-08-17 (reflections step 2, envproducer findings F4 / B3). THIS IS THE
    // CONSOLE'S OWN BLOCK, at the console's own position in the pass: the real
    // GenerateDispatchLists @0x827D1CE8 runs it at :3757 -- immediately after the camera
    // input is read, before the dispatch-thread buffer is locked and before the
    // update-set bit-7 early-out. X360 pseudocode lines 201-218:
    //     v15 = *(_R19 + 320);                      // camera->mState_uFlags
    //     v17 = (v15 & 0x400000) == 0;
    //     v18 = *(this + 6175808);                  // mbIsInJunkyard
    //     if ( v17 ) { if ( v18 ) DisableJunkyardLightingSetup(...); *(this+6175808) = 0; }
    //     else       { if ( !v18 ) EnableJunkyardLightingSetup(...);  *(this+6175808) = 1; }
    // (this+1994592 is mEnvironmentManager; @0x827B0F98 / @0x827B10E8 are the two setups.)
    //
    // [FLAG PC bring-up] ONE THING IS STAND-IN: where the flag comes from. The console
    // reads `lpDispatchInputBuffer->GetCameraInput()->IsInJunkyard()`, i.e. the camera
    // BridgeRendererToWorld @0x823CDD20 put in the world dispatch INPUT buffer
    // (`SetCameraInput(a2, RendererIO::OutputBuffer::GetBrnCamera(a3))`) -- a buffer that
    // does not exist on this build. Here it is the value BrnGameModule::DoDispatch staged
    // off the
    // SAME director camera it already stages the transform and FOV from
    // (SetBringUpCameraOverride's third argument, itself just
    // BrnDirector::Camera::Camera::IsInJunkyard() -- Camera.cpp:564, mState_uFlags &
    // 0x400000, the bit BrnArbStateCarSelect.cpp sets as KI_CAMERA_STATE_JUNKYARD).
    // The latch logic, the edge triggering and the member are all the console's.
    // DELETE-WHEN the dispatch INPUT buffer is real (this goes with the whole producer).
    //
    // WHAT IT TURNS ON. Two things, both console:
    //   * `if ( mbIsInJunkyard ) mShadowMap.SetConstantsForEnvmap()` in the env-map arm
    //     below -- the ONLY gate on it, and until now it could never fire (envproducer
    //     B3: "mbIsInJunkyard is never set on PC"). Shader constants 15/16 now carry the
    //     env-map shadow tint/bias while a junkyard face is drawn instead of whatever the
    //     main pass last staged (BrnShadowMap.cpp:370).
    //   * the junkyard lighting setup itself: time of day pinned to 18:00 and the key
    //     light forced to the nearest loaded junkyard rig on entry, restored on exit.
    //     THAT IS A VISIBLE CHANGE the first time the game enters the junkyard, and it is
    //     the change the console makes. The two log lines below are the proof.
    if ( mbBringUpCameraInJunkyardBringUp )
    {
        if ( !mbIsInJunkyard )
        {
            mEnvironmentManager.EnableJunkyardLightingSetup();
            if ( CgsDev::Log::gpDebugPrint != 0 )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[junkyard] ENTER -- junkyard lighting setup enabled"
                       " (camera state flag 0x400000); env-map faces now take"
                       " ShadowMap::SetConstantsForEnvmap\n";
            }
        }
        mbIsInJunkyard = true;
    }
    else
    {
        if ( mbIsInJunkyard )
        {
            mEnvironmentManager.DisableJunkyardLightingSetup();
            if ( CgsDev::Log::gpDebugPrint != 0 )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[junkyard] LEAVE -- junkyard lighting setup disabled,"
                       " time of day restored\n";
            }
        }
        mbIsInJunkyard = false;
    }

    // [DIAG frame-pacing, host only -- BRN_PACE_DIAG=1] one line per rendered frame:
    // the interpolation alpha this frame is drawing at, the director-camera eye it is
    // drawing from, and the spawned car's render pose. Together they say which of the two
    // is stepping at 60 Hz and which is continuous. Costs nothing unless the variable is
    // set. Remove when the pacing work is signed off.
    static s32 siPaceDiag = -1;
    if ( siPaceDiag < 0 )
    {
        const char* lpcPaceEnv = std::getenv( "BRN_PACE_DIAG" );
        siPaceDiag = ( lpcPaceEnv != 0 && lpcPaceEnv[0] != '0' ) ? 1 : 0;
    }
    // ⚠️ FLAG PC quality-of-life: THIS FRAME'S CAR DISPLAY POSE.
    //
    // Before anything reads a race car's RenderParams -- the tour camera's framing below,
    // the vehicle LOD banding, and the dispatch legs themselves -- publish the blend of the
    // last two simulation ticks into it. The camera is already continuous (the game module
    // interpolates the director camera); without this the CAR is the only thing in the frame
    // still stepping at 60 Hz, which reads as the car shuddering against a smooth world.
    // Idempotent, and a no-op in console-locked pacing (alpha pins at 1.0).
    mRaceCarEntityModule.ApplyRenderPoseInterpolationBringUp(
        CgsSystem::FrameInterpolation::GetAlpha() );

    // ⚠️ MUST BE READ AFTER THE APPLY ABOVE. Sampled before it, this reports whatever the
    // sub-step's latch left behind on a stepping frame and the previous frame's blend on a
    // zero-step frame -- an alternating pair that looks exactly like broken interpolation.
    // That cost one diagnosis round on 2026-08-17.
    if ( siPaceDiag != 0 && CgsDev::Log::gpDebugPrint != 0 )
    {
        Vector3 lDiagCar;
        lDiagCar.SetZero();
        const bool lbHaveCar = mRaceCarEntityModule.GetSpawnedCarPositionBringUp( lDiagCar );
        *CgsDev::Log::gpDebugPrint
            << "[pace] alpha " << CgsSystem::FrameInterpolation::GetAlpha()
            << " frameMs " << ( CgsSystem::FrameInterpolation::GetFrameSeconds() * 1000.0f )
            << " camValid " << ( lbUseDirectorCamera ? 1 : 0 )
            << " eye " << lDirectorTransform.wAxis.x << " " << lDirectorTransform.wAxis.y
            << " " << lDirectorTransform.wAxis.z
            << " car " << ( lbHaveCar ? 1 : 0 ) << " "
            << lDiagCar.x << " " << lDiagCar.y << " " << lDiagCar.z;
        // Wheel 0: its world TRANSLATION (does the hub track the body?) and one BASIS
        // component (does the SPIN advance smoothly?). The two answer different questions --
        // a hub that steps and a spin that steps are different producers.
        {
            const BrnWorld::ActiveRaceCar* lpDiagCar0 = mRaceCarEntityModule.GetActiveRaceCarConstBringUp( 0 );
            if ( lpDiagCar0 != 0 )
            {
                const Matrix44Affine& lrW0 = lpDiagCar0->GetRenderParams()->GetWheelTransformConst( 0u );
                *CgsDev::Log::gpDebugPrint
                    << " w0pos " << lrW0.wAxis.x << " " << lrW0.wAxis.y << " " << lrW0.wAxis.z
                    << " w0basis " << lrW0.xAxis.x << " " << lrW0.xAxis.y;
            }
        }
        *CgsDev::Log::gpDebugPrint << "\n";
    }

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

        // [DIAG] ...and BRN_WORLD_CAMTRAFFIC forces the STATIC shot unless the capture asked
        // for an orbit explicitly. See the swerve-camera block below for why a moving camera
        // destroys the very evidence this mode exists to gather. DELETE-WHEN-STABLE.
        const char* lpcSwerveCamEnv2 = std::getenv( "BRN_WORLD_CAMTRAFFIC" );
        if ( lpcSwerveCamEnv2 != 0 && lpcSwerveCamEnv2[0] != '0' && lpcSpeedEnv == 0 )
        {
            sfCamSpeed = 0.0f;
        }
    }

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
        // One lap per ~40 s at speed 1.
        //
        // ⚠️ FLAG PC quality-of-life, 2026-08-17: ADVANCED ON REAL TIME, NOT PER FRAME.
        // This used to be a flat `+= 0.0052f * sfCamSpeed` per call, and this producer runs
        // once per RENDERED frame -- so the tour ran at whatever rate the renderer happened
        // to hit. That was invisible while the renderer was pinned to the simulation at
        // 60 Hz; with the two decoupled it means the boot/loading camera sweeps the city at
        // 2.4x on a 144 Hz panel. Scaling by the real frame length, normalised to the 60 Hz
        // step the constant was tuned against, keeps the sweep at its authored speed at any
        // render rate -- and, unlike everything the simulation owns, this stand-in needs no
        // interpolation because it is now genuinely continuous.
        //
        // A capture stays reproducible: BRN_WORLD_CAMSPEED still scales it, and the frame
        // length is clamped (CgsFrameInterpolation::SetFrameSeconds) so a stall cannot jump
        // the tour.
        sfPathAngle += 0.0052f * sfCamSpeed
                     * ( CgsSystem::FrameInterpolation::GetFrameSeconds() * 60.0f );

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

    // ⭐ ...and the SAME latch for the two fields of that record the ENVIRONMENT consumer
    // reads. The console's latch is a whole-camera copy, so WorldModule::Update @0x827D63E8
    // finds the director's time-of-day request already sitting in
    // mLastCameraInput.mEffects (`lbzx r11, r31, 0x5E1DE1` at 0x827D7CEC == camera +0x121).
    // Here mLastCameraInput is synthesised, never copied, so without this the restored
    // override in Update reads Camera::Clear()'s zeroed mbSetTimeOfDay for ever and the DMV
    // backdrop stays at Construct's 13:00. The values come across from the very camera the
    // console would have copied -- BrnGameModule::DoDispatch stages them beside the
    // transform, the FOV and the junkyard bit (SetBringUpCameraOverride).
    // DELETE with the rest of this producer.
    mLastCameraInput.GetEffects().mbSetTimeOfDay = mbBringUpCameraSetTimeOfDayBringUp;
    mLastCameraInput.GetEffects().mfTimeOfDay    = mfBringUpCameraTimeOfDayHoursBringUp;

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

    // The HORIZONTAL field of view this stand-in camera is actually built with
    // (SetFovHorizontal below consumes it in radians). Hoisted out of that call so
    // the LOD zoom factor below is derived from the same number.
    const f32 lfHorizontalFov = 2.0f * atanf( lfAspect / lfCotHalfFov );

    // ⭐ THE LOD ZOOM FACTOR (2026-08-12, vehicle-LOD wave) -- this used to be a
    // hard-coded 1.0f at the world dispatch call below.
    //
    // The console computes it in GenerateDispatchLists @0x827D1CE8 as
    //     f32 lfLodZoomFactor = lpCameraInput->GetLodZoomFactor();
    //     if ( lfLodZoomFactor < 1.0f ) lfLodZoomFactor = 1.0f;     // X360 fsel
    // where BrnDirector::Camera::Camera::GetLodZoomFactor @0x827BAC40 forwards the
    // camera's FOV (DEGREES) into Utils::GetZoomFromFOVDegs == 1/tan(fov/2). It is
    // consumed by WorldEntityModule::GenerateDispatchLists @0x822D5AB0, which
    // divides dist^2 by zoom^2 (so a zoomed-IN camera lengthens every world LOD band
    // and the cull radius), and by CalculateVehicleLODs, which multiplies every
    // vehicle band by it.
    //
    // [FLAG PC bring-up] the director publishes no camera here, so the FOV comes from
    // the stand-in camera framed above -- the same value SetFovHorizontal is given,
    // in the degrees domain GetZoomFromFOVDegs works in. DELETE with the rest of this
    // producer; the console line above is the replacement.
    //
    // ⚠ MEASURED ON THE LIVE PATH (2026-08-12, verifier re-measure -- an earlier note
    // here claimed the clamp binds and the change was inert; THAT WAS WRONG, it was
    // measured against the tour fallback rather than what the director publishes).
    // Whenever the director is driving, BrnGameModule.cpp:1362 stages
    // lpCamera->GetFOV() and this producer round-trips it back out, and the boot log
    // shows that FOV blending 61.13 deg -> 42.94 deg (BrnBehaviourInterpolate cuts at
    // t == 1, so 42.94 deg is the steady state). GetZoomFromFOVDegs of those is
    // 1.693 .. 2.543 -- the clamp does NOT bind. Even the no-director tour fallback
    // only lands under 1 at exactly 16:9 (0.974); at 16:10 it is 1.083, at 4:3 1.299.
    //
    // SO THIS IS A BEHAVIOURAL CHANGE, in two places, both console-faithful given the
    // FOV but both real:
    //   * the vehicle bands stretch from 10/22/35/50/70 m to ~25/56/89/127/178 m;
    //   * the world pass at the GenerateDispatchLists call below now divides every
    //     dist^2 by zoom^2 (BrnWorldEntityModule.cpp:1776), which makes world LOD
    //     selection finer AND grows the max-draw-distance cull radius ~2.5x.
    // The second one is a per-frame draw-call cost increase -- MEASURE IT before
    // treating this producer's framing as settled.
    f32 lfLodZoomFactor = BrnDirector::Camera::Utils::GetZoomFromFOVDegs(
        lfHorizontalFov / KF_DEGS_TO_RADS );
    if ( lfLodZoomFactor < 1.0f )
    {
        lfLodZoomFactor = 1.0f;
    }

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
    sBringUpCamera.Construct();                                 // the @0x827F94E8 defaults reset
                                                                // (it said Release() until
                                                                // 2026-08-17 -- Release() is the
                                                                // EMPTY @0x8284CB38 body)
    sBringUpCamera.maProjectionScalars[ 6 ] = lfAspect;         // m_aspectRatio
    sBringUpCamera.SetFovHorizontal( lfHorizontalFov );
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

    // =========================================================================
    // THE REAL FRAME SHADING PUBLISH (env-manager go-live wave, 2026-08-16).
    //
    // This is the CONSOLE call at the CONSOLE's relative position: in
    // WorldModule::GenerateDispatchLists (this file, :3707 -- X360 @0x827D20B0)
    // SetupShaderConstantsBeforeRendering sits immediately after the dispatch camera is
    // rebuilt and immediately before the 8/3/34 camera-constant publish, because the
    // function resets engine matrix slots 0..4 (slot 3 == ViewProjection) to the identity.
    // It REPLACES PublishWorldShadingConstantsBringUp, whose ten hard-coded slots
    // (9/10/11/12/13/18/19/27/28/29/33) are a strict subset of what this publishes.
    //
    // [FLAG PC bring-up] THREE of the six arguments are stand-ins, and only three:
    //   * the camera TRANSFORM. The console passes the director Camera* (its mTransform is
    //     the object's first member). This producer never receives that object -- only the
    //     transform + FOV SetBringUpCameraOverride staged -- so the transform is rebuilt
    //     from the basis the stand-in graphics camera above was framed with, exactly as the
    //     shadow arm below rebuilds it (sBringUpCamera.mView's basis COLUMNS are the
    //     transform's basis ROWS; CgsGraphics::Camera::LookAt @0x827F9510 fills those
    //     columns, so the two cannot drift).
    //   * the GAME / SIM times. The console reads them off the dispatch INPUT buffer
    //     (GetGameTime / GetSimTime), which does not exist here. Passed as 0.0f/0.0f --
    //     which is EXACTLY what PublishWorldShadingConstantsBringUp published for shader
    //     slot 13 ("Time" = (0,0,0,0)), so this changes nothing on screen. It is not
    //     replaced by a host clock: inventing a time base is not a reconstruction.
    //     DELETE-WHEN BrnGameModule::DoDispatch stages the frame times the way it already
    //     stages the camera (SetBringUpCameraOverride) -- see the report's follow-ups.
    //   * the frame + output buffer. See the banner on gBrnWorldShaderConstantsFrameBringUp
    //     (this file, next to gDispatchCamera).
    // The KEY LIGHT, the irradiance quadrics, the fog/scattering, the sky gradient, the
    // cloud set and the white level are now the REAL environment manager's.
    // =========================================================================
    {
        // One-shot lifecycle for the two stand-in buffers. CgsModule::IOBuffer::LockForWrite
        // asserts eStatusConstructed and BrnShaderConstantsFrame::Construct seeds
        // mfWhiteLevel = 1.0f / clears the write lock, so both have to be Constructed exactly
        // once -- on the console that happens in GenerateDispatchLists' CreateIOBuffer and in
        // BrnRendererModule::Construct respectively.
        static bool sbBringUpShadingBuffersConstructed = false;
        if ( !sbBringUpShadingBuffersConstructed )
        {
            sbBringUpShadingBuffersConstructed = true;
            gBrnWorldShaderConstantsFrameBringUp.Construct();
            gWorldDispatchOutputBringUp.Construct();
        }

        rw::math::vpu::Matrix44Affine lCameraTransform;
        const Matrix44&               lrView = sBringUpCamera.mView;
        lCameraTransform.xAxis = Vector3{ lrView.xAxis.x, lrView.yAxis.x, lrView.zAxis.x, 0.0f };
        lCameraTransform.yAxis = Vector3{ lrView.xAxis.y, lrView.yAxis.y, lrView.zAxis.y, 0.0f };
        lCameraTransform.zAxis = Vector3{ lrView.xAxis.z, lrView.yAxis.z, lrView.zAxis.z, 0.0f };
        lCameraTransform.wAxis = Vector3{ lEye.x, lEye.y, lEye.z, 0.0f };

        // The console does not open either lock inside the producer -- the frame's belongs
        // to BrnRendererModule::Update and the buffer's to GenerateDispatchLists -- so the
        // seam opens both around the call, in the console's own order.
        gBrnWorldShaderConstantsFrameBringUp.LockForWriting();
        gWorldDispatchOutputBringUp.LockForWrite();
        SetupShaderConstantsBeforeRendering( lViewProjection, lCameraTransform,
                                             0.0f, 0.0f,
                                             &gBrnWorldShaderConstantsFrameBringUp,
                                             &gWorldDispatchOutputBringUp );
        gWorldDispatchOutputBringUp.UnlockForWrite();
        gBrnWorldShaderConstantsFrameBringUp.UnlockForWriting();
        gbBrnWorldShaderConstantsFrameBringUpValid = true;
    }

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
    // (Hoisted out of the block below so the shadow arm can re-publish it -- see the
    // main-view constant restore at the bottom of SHADOW PRODUCER, PART 2.)
    Matrix44 lViewProjectionModified;
    {
        const f32 lfZScale = lfFar / ( lfFar - lfNear );
        const f32 lfZBias  = -lfNear * lfFar / ( lfFar - lfNear );

        lViewProjectionModified.xAxis = Vector4{ lViewProjection.xAxis.x, lViewProjection.yAxis.x,
                                                 lViewProjection.zAxis.x, lViewProjection.wAxis.x };
        lViewProjectionModified.yAxis = Vector4{ lViewProjection.xAxis.y, lViewProjection.yAxis.y,
                                                 lViewProjection.zAxis.y, lViewProjection.wAxis.y };
        lViewProjectionModified.zAxis = Vector4{ lViewProjection.xAxis.w, lViewProjection.yAxis.w,
                                                 lViewProjection.zAxis.w, lViewProjection.wAxis.w };
        lViewProjectionModified.wAxis = Vector4{ lfZScale, lfZBias, 1.0f, 0.0f };
        CgsGraphics::mShaderConstantTable.SetShaderConstantData( 34, lViewProjectionModified );
    }

    // =========================================================================
    // ⭐ [FLAG PC bring-up] SHADOW PRODUCER, PART 1 -- the three cascade cameras.
    //
    // MIRRORS the console leg in WorldModule::GenerateFrustumQueries @0x827DADF8
    // (this file, :3541-3548). That function is bodied, compiled AND linked -- and has
    // ZERO callers on this build: BrnGameModule::DoDispatch runs this bring-up producer
    // instead, and until now it had no shadow arm at all, which is why the renderer's
    // "MESH lists:" probe has never once shown the shadow-caster lists 0/1/2/3/4.
    //
    // ---- THE RENDER CAMERA -------------------------------------------------------
    // The console hands ShadowMap::CalculateShadowMapCameras the DIRECTOR camera
    // (BrnDirector::Camera::Camera*, asm-proven -- see the RECONCILE note on the decl).
    // DoDispatch's director camera object is NOT reachable from inside this producer:
    // all this function ever receives of it is the transform + FOV that
    // SetBringUpCameraOverride stages, and the module's own mLastCameraInput is NOT a
    // stand-in for it -- that member is deliberately the PVS QUERY LATCH (its Pos row is
    // lPvsPosition, which for the frozen establishing shot is the point being framed,
    // not the eye, and repurposing it would move the streamer's query point).
    //
    // So the leg builds its own director camera from EXACTLY the basis the stand-in
    // graphics camera above was framed with -- read straight out of sBringUpCamera.mView,
    // whose basis COLUMNS the engine's own CgsGraphics::Camera::LookAt @0x827F9510 filled
    // (column0 = right, column1 = up, column2 = dir), so the two cannot drift. When the
    // director IS driving, that basis is the director's own framing.
    //
    // ---- THE KEY LIGHT -----------------------------------------------------------
    // mEnvironmentManager.CalcKeyLightDirection() @0x827B0638 -- the REAL console
    // producer. Verified live-safe: it is a complete reconstruction, and every input it
    // reads (mfTimeOfDay, the three sun-rig tuning angles, the 09:00..16:00 elevation
    // clamp) is seeded by EnvironmentManager::Construct, which WorldModule::Construct
    // calls at :451. At the Construct default time of day (46800 s == 13:00) it evaluates
    // to (-0.4246, -0.6698, -0.6092) -- a real, normalised, downward direction. (The Z lane
    // read +0.6092 until 2026-08-20; ComputeKeyLightDirection was applying XMMatrixRotationX
    // and XMMatrixRotationY transposed, which negates exactly that lane.)
    //
    // ⚠ IT IS NOT THE DIRECTION THE WORLD IS CURRENTLY LIT BY. Both bring-up publishes
    // (PublishWorldShadingConstantsBringUp below, shader slot 10, and
    // BrnRendererModule::PublishSkyConstantsBringUp) push the hard-coded bring-up value
    // (0.406, -0.812, 0.419). The two are in the SAME HEMISPHERE (dot == +0.63; both
    // travel downward) but about 51 degrees apart in azimuth/elevation. The real function
    // is used here deliberately: it is the console call, it is what the world WILL be lit
    // by the moment the two bring-up publishes are retired, and nothing renders out of
    // these cascades yet. Unify all three in the change that retires the publishes.
    // =========================================================================
    bool lbShadowArmLive = false;
    // [DIAG shadow-perf] PART 1's stopwatch (`cam`). Measured 21.3 us/frame -- the
    // three-cascade camera solve is not a cost worth thinking about.
    const ShadowPerfClock::time_point lCamerasStart = ShadowPerfNow();
    if ( mShadowMap.IsEnabled() )
    {
        // The director camera this leg owns. Construct() once for the FOV / aspect /
        // camera-state defaults; only the transform + FOV (+ the junkyard flag, step 2) move per frame.
        static BrnDirector::Camera::Camera sShadowRenderCamera;
        static bool                        sbShadowRenderCameraConstructed = false;
        if ( !sbShadowRenderCameraConstructed )
        {
            sbShadowRenderCameraConstructed = true;
            sShadowRenderCamera.Construct();
        }

        // mView's basis COLUMNS -> the camera transform's basis ROWS.
        const Matrix44& lrView = sBringUpCamera.mView;
        sShadowRenderCamera.mTransform.xAxis =
            Vector3{ lrView.xAxis.x, lrView.yAxis.x, lrView.zAxis.x, 0.0f };   // right
        sShadowRenderCamera.mTransform.yAxis =
            Vector3{ lrView.xAxis.y, lrView.yAxis.y, lrView.zAxis.y, 0.0f };   // up
        sShadowRenderCamera.mTransform.zAxis =
            Vector3{ lrView.xAxis.z, lrView.yAxis.z, lrView.zAxis.z, 0.0f };   // at
        sShadowRenderCamera.mTransform.wAxis =
            Vector3{ lEye.x, lEye.y, lEye.z, 0.0f };                           // pos
        sShadowRenderCamera.mfFOV         = lfHorizontalFov / KF_DEGS_TO_RADS; // mfFOV is DEGREES
        sShadowRenderCamera.mfAspectRatio = lfAspect;

        const Vector3 lKeyLightDirection = mEnvironmentManager.CalcKeyLightDirection();

        // ⚠ SUSPECT, NOT MINE TO FIX (flagged 2026-08-12, shadow-producer wave).
        // CalculateShadowMapCameras step 3 makes a CGS copy of THIS camera
        // (CopyToCgsCamera) and steps 4/5 hand it to ComputeBoundingBoxMatrix -- which is
        // now a LIVE path, because the dumped KA_SHADOWMAPTYPE rodata says all three
        // cascades ship as E_SHADOWMAP_TYPE_BOUNDINGBOX, not ortho. CopyToCgsCamera sets
        // that copy's far clip from BrnDirector::Camera::Camera::KF_DEFAULT_FAR_CLIP_DISTANCE,
        // which is still a FLAGGED 0.0f PLACEHOLDER (Camera.cpp:64 -- "no pinned address").
        // A zero far clip behind a 0.15 near clip is a degenerate view volume, so the
        // per-cascade best-fit matrix it feeds can come out degenerate or non-finite. That
        // shows up in the [shadow-prod] line below as the view-projection WARN; it cannot
        // reach the screen (see the tripwire note) and it cannot be fixed from this file.
        mShadowMap.CalculateShadowMapCameras( lKeyLightDirection, &sShadowRenderCamera );

        // ---- FINITENESS TRIPWIRE -------------------------------------------------
        // This should never fire now: the 0x820CA81C rodata block was dumped on
        // 2026-08-12, so ShadowMap::Construct seeds real ortho scales { 7, 30, 120 } and
        // at-offsets { 7.5, 30, 120 } instead of the zeros it used to carry (a zero ortho
        // scale made CgsGraphics::Camera::UpdateOrthogonalProjectionMatrix compute 1.0f/0
        // and every cascade view-projection came out +inf). The tripwire stays because
        // that rodata is exactly the kind of value a later wave re-derives, and the two
        // failure modes are NOT equivalent:
        //
        //   * a non-finite cascade FRUSTUM would poison the query, so it PARKS the arm --
        //     the octree's frustum test culls against nothing but those planes
        //     (LooseOctree::FrustumTestVpRecursive -> FrustumTestEntities).
        //   * a non-finite cascade VIEW-PROJECTION does not poison the query DIRECTLY:
        //     AddJobFrustumTest copies it but no test path reads it, and inside the
        //     cascade legs it only reaches shader constants 3/34, which only the
        //     shadow-list records snapshot (AddToBin bakes the dirty block per record)
        //     and the renderer dispatches no shadow list yet. So it is WARNED about,
        //     latched, and the leg proceeds.
        //
        // ⚠️ CORRECTED 2026-08-12 (cull-volume wave): those two are NOT independent, and
        // the note above used to imply they were. Now that the cull volume is maFrustum[i]
        // (the asm-correct source -- see ::GenerateFrustumQueries), the FRUSTUM is
        // DOWNSTREAM OF THE VIEW-PROJECTION: ComputeBoundingBoxMatrix takes
        // lWorldToLight = maCgsShadowMapCamera[i].mViewProjection and transforms the
        // sub-frustum corners through it before ComputeOptimalViewVolume fits the planes
        // (BrnShadowMap.cpp:1697/1740). So while the VP is NaN, maFrustum[i] is garbage in
        // one of two ways, neither of which can produce a correct per-cascade split:
        //   (a) NaN planes            -> this tripwire fires and the arm parks (correct);
        //   (b) zero surviving        -> ComputeOptimalViewVolume's sbClearPlanes padding
        //       candidate planes         installs the eight never-culling +/-1e6 defaults,
        //                                which are FINITE, so the arm stays live and every
        //                                cascade accepts everything -- i.e. the counts stay
        //                                identical, for a completely different reason.
        // Either way THE CASCADE COUNTS CANNOT DIVERGE UNTIL THE NaN VIEW-PROJECTION IS
        // FIXED (owned separately, in BrnShadowMap.cpp). This wave fixes WHICH volume is
        // submitted; that wave fixes WHAT IS IN IT. Do not read a still-identical
        // [shadow-pass] line as evidence that the cull-volume source is wrong again --
        // check [shadow-prod]'s view-projection WARN first.
        lbShadowArmLive = true;
        bool lbCascadeVpFinite = true;
        for ( s32 liCascade = 0; liCascade < 3; liCascade++ )
        {
            const CgsGraphics::Camera* lpCascadeCamera = mShadowMap.GetCascadeCamera( liCascade );

            // ASM RE-READ 2026-08-12: the volume the console actually submits is
            // maFrustum[i], NOT the cascade camera's perspective frustum (see the long
            // note at ::GenerateFrustumQueries above). The tripwire has to test THE PLANES
            // THAT GET SUBMITTED, so it reads the same source the arm below does -- and
            // that source is the one genuinely at risk here, because maFrustum[i] is
            // produced by ComputeBoundingBoxMatrix from the cascade CAMERA, which the log
            // says is non-finite in 39 of 41 samples.
            const CgsGeometric::Frustum& lrCascadeFrustum =
                mShadowMap.GetFrustum( static_cast< u32 >( liCascade ) );

            // [DIAG shadow-perf] how many of this cascade's eight planes are
            // ComputeOptimalViewVolume clear-plane sentinels (see the census banner).
            gShadowPerf.maiCascadePadPlanes[ liCascade ] =
                ShadowPerfCountClearPlanes( lrCascadeFrustum );

            for ( s32 liPlane = 0; liPlane < 8; liPlane++ )
            {
                const Vector4& lrPlane = lrCascadeFrustum.maSwizzledPlanes[ liPlane ];
                if ( !std::isfinite( lrPlane.x ) || !std::isfinite( lrPlane.y )
                     || !std::isfinite( lrPlane.z ) || !std::isfinite( lrPlane.w ) )
                {
                    lbShadowArmLive = false;
                }
            }

            if ( !IsFiniteMatrix44BringUp( lpCascadeCamera->GetViewProjectionMatrix() ) )
            {
                lbCascadeVpFinite = false;
            }
        }

        // ---- the value-latched liveness probe ------------------------------------
        // ⚠️ LATCHED ON THE VALUE, never on a `static bool` one-shot. Project lesson,
        // learned three times: a one-shot probe here fires on the loading screen before
        // the world exists and then never again, which is indistinguishable in the log
        // from "this code was never reached". This reprints whenever cascade 0's camera
        // origin moves more than a metre -- i.e. whenever the producer is genuinely
        // running over a moving camera. A NON-FINITE origin also prints (once per
        // transition, and the latch is reset to the sentinel so the recovery prints too):
        // a NaN would make every `moved > 1` test false and silently reproduce exactly
        // the "looks like it was never reached" failure this probe exists to avoid.
        {
            static Vector3 sLastCascadeOrigin  = { 1.0e30f, 1.0e30f, 1.0e30f, 0.0f };
            static bool    sbLastOriginFinite  = true;

            const Vector3 lCascadeOrigin = mShadowMap.GetCascadeCamera( 0 )->GetPosition();
            const bool    lbOriginFinite = std::isfinite( lCascadeOrigin.x )
                                        && std::isfinite( lCascadeOrigin.y )
                                        && std::isfinite( lCascadeOrigin.z );
            const f32     lfDeltaX = lCascadeOrigin.x - sLastCascadeOrigin.x;
            const f32     lfDeltaY = lCascadeOrigin.y - sLastCascadeOrigin.y;
            const f32     lfDeltaZ = lCascadeOrigin.z - sLastCascadeOrigin.z;
            const f32     lfMovedSq =
                lfDeltaX * lfDeltaX + lfDeltaY * lfDeltaY + lfDeltaZ * lfDeltaZ;

            // Rate limit ON TOP of the value latch, never instead of it: when the
            // director is driving, the camera covers more than a metre in a single
            // dispatch frame, and an unlimited value latch would then print on every
            // one of the ~7000 frames of a boot-drive session. The latch still decides
            // WHETHER there is anything to say; this only decides how often it is said.
            static s32 siFramesSincePrint = 1000;
            ++siFramesSincePrint;

            const bool lbPrint = ( siFramesSincePrint >= 60 )
                              && ( lbOriginFinite ? ( lfMovedSq > 1.0f ) : sbLastOriginFinite );

            if ( lbPrint && CgsDev::Log::gpDebugPrint != 0 )
            {
                siFramesSincePrint = 0;
                sbLastOriginFinite = lbOriginFinite;
                sLastCascadeOrigin = lbOriginFinite
                    ? lCascadeOrigin
                    : Vector3{ 1.0e30f, 1.0e30f, 1.0e30f, 0.0f };
                *CgsDev::Log::gpDebugPrint
                    << "[shadow-prod] cascade0 origin=(" << lCascadeOrigin.x << ","
                    << lCascadeOrigin.y << "," << lCascadeOrigin.z
                    << ") renderEye=(" << lEye.x << "," << lEye.y << "," << lEye.z
                    << ") keyLight=(" << lKeyLightDirection.x << ","
                    << lKeyLightDirection.y << "," << lKeyLightDirection.z
                    << ") cascades=" << ( mShadowMap.GetRenderMultipleShadowMaps() ? 3 : 1 )
                    << " armed=" << ( lbShadowArmLive ? 1 : 0 )
                    << ( lbCascadeVpFinite
                             ? ""
                             : "  [WARN: a cascade view-projection is not finite -- check"
                               " ShadowMap KAF_ORTHO_SCALE; the frustum planes are"
                               " unaffected so the query still runs]" )
                    << ( lbShadowArmLive
                             ? ""
                             : "  [PARKED: a cascade frustum plane is not finite]" )
                    << "\n";
            }
        }
    }
    gShadowPerf.mfCamerasUs += ShadowPerfUsSince( lCamerasStart );
    gShadowPerf.miArmed      = lbShadowArmLive ? 1 : 0;

    // ⚠️ THE SINGLE MOST DANGEROUS BIT OF THE SHADOW ARM, MADE HARMLESS BY CONSTRUCTION.
    // WorldEntityModule::GenerateDispatchLists (BrnWorldEntityModule.cpp:1774) and
    // RenderRaceCar (BrnRaceCarEntityModule_Render.cpp:164) both select their Z-only
    // shadow path from ShadowMap::IsRenderingShadowMap(), so a latch left raised would
    // silently corrupt the MAIN pass. The console only ever raises it inside
    // GenerateShadowMapDispatchLists and lowers it at the bottom of the same loop body;
    // this producer additionally FORCES it down here, before any main-view leg runs, so
    // the main pass is provably unaffected even if a future edit leaks the latch.
    mShadowMap.SetRenderingShadowMap( false );

    // (The bring-up lighting/atmosphere publish used to sit here. It is now the REAL
    //  WorldModule::SetupShaderConstantsBeforeRendering @0x827D1410, called ABOVE, at the
    //  console's own position immediately before the 8/3/34 camera-constant publish --
    //  it has to be there, because it resets engine matrix slot 3 (ViewProjection) to the
    //  identity. Nothing is published here any more. The shadow constants c14..c17 that
    //  ShadowMap::SetConstants writes from CalculateShadowMapCameras above are untouched by
    //  the real producer, so the 2026-08-12 retirement of the bring-up's shadow block still
    //  holds -- it now holds by construction rather than by ordering.)

    // ======================================================================
    // THE WORLD-LAYER EFFECTS PRODUCER (bloom wave 2026-08-15).
    //
    // This is the CONSOLE call, at the console's relative position. In the real producer
    // WorldModule::GenerateDispatchLists (this file, :3836 -- X360 @0x827D1CE8 line 382)
    // mEnvironmentManager.GenerateEffects sits immediately after the frame's shader-constant
    // publish and immediately before PropEntityModule::CachePropGraphicsLists, bracketed by
    // the UT_RenderFX CPU monitor. PublishWorldShadingConstantsBringUp() above is this
    // producer's shader-constant publish and the prop cache is right below, so the ordering
    // is preserved exactly -- including the monitor bracket.
    //
    // [FLAG PC bring-up] only the four FRAME POINTERS are the stand-in (staged by
    // BrnGameModule::DoDispatch via SetBringUpEffectsFrames, because none of the dispatch IO
    // buffers exists here); GenerateEffects itself is the real X360 body. They are null until
    // BrnRendererModule's effects arbitrator is Constructed, and the console never calls
    // GenerateEffects with a null frame -- the dispatch input buffer always holds four -- so
    // skip the call entirely rather than passing nulls in.
    // DELETE the guard (not the call) when the IO buffer set is real.
    // ======================================================================
    if ( mapBringUpEffectsFrames[ 0 ] != 0 && mapBringUpEffectsFrames[ 1 ] != 0
      && mapBringUpEffectsFrames[ 2 ] != 0 && mapBringUpEffectsFrames[ 3 ] != 0 )
    {
        // TOMBSTONE (post-fx step 9, group envblend): the noon-keyframe staging that stood
        // here is DELETED. EnvironmentManager::Update @0x827D6060 -> SetupBlend @0x827D4FE8 ->
        // SetupTimeOfDayBlend @0x827D35C0 -> PerformBlend now fill mBlendFrame from the
        // STREAMED timeline every frame, at the live time of day, which is what this call
        // stood in for. GenerateEffects below is unchanged -- it was always the real body.

        // (explicitly qualified: unlike the real GenerateDispatchLists this function has no
        //  `using namespace CgsDev;` in scope)
        CgsDev::PerfMonCpu::StartMonitor( mGlobalCpuMonitors.miUT_RenderFX );
        mEnvironmentManager.GenerateEffects( mapBringUpEffectsFrames[ 0 ],
                                             mapBringUpEffectsFrames[ 1 ],
                                             mapBringUpEffectsFrames[ 2 ],
                                             mapBringUpEffectsFrames[ 3 ] );
        CgsDev::PerfMonCpu::StopMonitor( mGlobalCpuMonitors.miUT_RenderFX );
    }

    // ======================================================================
    // ⭐ [FLAG PC bring-up] THE PROP-GRAPHICS REGISTRATION TABLE (prop-render wave,
    // 2026-08-12).
    //
    // MIRRORS the console call in WorldModule::GenerateDispatchLists (this file, :3806
    // -- X360 @0x827D1CE8), which is the ONLY caller of PropEntityModule::
    // CachePropGraphicsLists @0x822DBF28 anywhere in the image (its xrefs_to list has
    // exactly one entry). That console producer has NO callers in this build --
    // BrnGameModule::DoDispatch drives GenerateDispatchListsBringUp instead -- so the
    // 500-slot "prop type id -> PropGraphics" table was never rebuilt after boot and
    // PropGraphicsManager::GetPropGraphics() returned 0 for every type.
    //
    // MEASURED, this build, before this line existed: the frustum handed the prop module
    // 193 visible ids, ALL of them owner 3 (whole props) with sane type ids (0/6/21/22/
    // 53/108/115/123/124), and all 193 were dropped at the
    // `mPropGraphicsManager.GetPropGraphics( luPropTypeId ) == 0` continue in
    // PropEntityModule::GenerateDispatchLists, with the registration census reporting
    // registered=0 of 500 slots. Nothing else on the path was wrong.
    //
    // Console ORDER is preserved: the cache is rebuilt HERE, before the frustum query and
    // the module dispatch legs below, exactly as :3806 sits ahead of the LOD-zoom factor
    // and the dispatch calls in the real producer. It is safely after the bind, too --
    // mapGraphicsLists[zone] is written a whole phase earlier, in the prop module's
    // PreScene leg (BrnPropEntityModule_PreScene.cpp:659).
    //
    // DELETE this call (not the cache) when the real WorldModule::GenerateDispatchLists
    // becomes the live producer -- it already carries the console call at :3806.
    // ======================================================================
    {
        // [DIAG shadow-perf] timed separately: this call landed the SAME DAY as the shadow
        // arm, so the fps attribution has to be able to tell the two apart.
        const ShadowPerfClock::time_point lPropCacheStart = ShadowPerfNow();
        mPropEntityModule.CachePropGraphicsLists();
        gShadowPerf.mfPropCacheUs += ShadowPerfUsSince( lPropCacheStart );
    }

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
        // [DIAG shadow-perf] the staging phase stopwatch (frustum build + the IO buffer
        // recycle + the four query events).
        const ShadowPerfClock::time_point lStageStart = ShadowPerfNow();

        CgsGraphics::CameraRwFrustum lRwFrustum;
        sBringUpCamera.GetFrustumPerspective( lRwFrustum, false );

        CgsGeometric::Frustum lFrustum;
        lFrustum.SetFromRwFrustum( lRwFrustum );

        // The four IO buffers the query round trip needs (see the FLAG above).
        static CgsSceneManager::SceneManagerIO::InputBuffer_Query  sQueryInput;
        static CgsSceneManager::SceneManagerIO::OutputBuffer       sQueryOutput;
        static WorldEntityIO::InputBuffer_GenerateDispatchLists    sWorldDispatchInput;
        static RaceCarEntityModuleIO::InputBuffer_GenerateDispatchLists sRaceCarDispatchInput;
        // [FLAG PC bring-up] the prop leg's stand-in IO buffer -- same stand-in pattern as
        // the two above, for the same reason (BridgeWorldModuleToEntityModules_Render is
        // still gated). Retire all three together.
        static PropEntityIO::InputBuffer_Dispatch                  sPropDispatchInput;
        // [FLAG PC bring-up] the traffic leg's three stand-in IO buffers. Same pattern and
        // the same retirement condition as the three above: the console gets all three off
        // the dispatch IO stacks inside WorldModule::GenerateDispatchLists @0x827D1CE8 and
        // stages the dispatch one through BridgeWorldModuleToEntityModules_Render, still an
        // inert gate here. DELETE-WHEN that gate is retired; retire all six together.
        static BrnTraffic::BrnTrafficIO::InputBuffer_Dispatch      sTrafficDispatchInput;
        static BrnTraffic::BrnTrafficIO::InputBuffer_PreDispatch   sTrafficPreDispatchInput;
        static BrnTraffic::BrnTrafficIO::OutputBuffer_PreDispatch  sTrafficRenderInfos;
        static FilteredEntityData                                  sFilteredEntityData;
        static bool sbBuffersConstructed = false;
        if ( !sbBuffersConstructed )
        {
            sbBuffersConstructed = true;
            sWorldDispatchInput.Construct();
            sRaceCarDispatchInput.Construct();
            sPropDispatchInput.Construct();
            // InputBuffer_Dispatch::Construct @0x8275CF40 seeds the four handle words the
            // traffic dispatch leg reads. Constructed once here alongside the siblings so
            // the buffer never carries the previous stack tenant's bytes.
            sTrafficDispatchInput.Construct();
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

        // ==================================================================
        // ⭐ [FLAG PC bring-up] ENVIRONMENT-MAP PRODUCER, PART 1 -- the six face queries.
        //
        // MIRRORS the console leg in WorldModule::GenerateFrustumQueries @0x827DADF8
        // (this file, :3481-3502 for the face schedule and :3560-3590 for the events).
        // That function is bodied, compiled AND linked, and has ZERO callers on this
        // build -- DoDispatch runs this producer instead -- which is why mesh lists
        // 5..10 have never once been non-empty.
        //
        // ORDER IS THE CONTRACT. SceneManagerModule::ProcessFrustumTestJobRequests
        // stamps each result batch with its query's id and the octree drains jobs in
        // index order and each job's queries in submission order, so the result stream
        // comes back in SUBMISSION order (CgsSceneManagerModule.cpp:952-965). These
        // events therefore go in between the main view and the cascades, exactly where
        // the console puts them, and PART 2 below walks the cursor past them before the
        // cascade arm reads. KA_FRUSTUM_QUERY_IDS is still all-zero on PC (it is
        // declared at :93 with no writer -- the X360 table lives at data 0x82F30DC4 and
        // has not been dumped), so the ids do NOT discriminate the batches today; order
        // does, on both arms.
        //
        // The four-jobs map is `(queryIndex * 4) / 16 == queryIndex / 4`
        // (CgsSceneManagerModule.cpp:1016) with KU_MAX_FRUSTUM_TEST_JOB_QUERIES == 16,
        // so growing from 4 to at most 10 staged queries stays inside the cap and stays
        // non-decreasing: main + 3 faces in job 0, the other 3 faces + cascade 0 in job
        // 1, cascades 1/2 in job 2. Each job owns its own 8192-entry result pool, so
        // spreading them RAISES headroom rather than lowering it -- watch the existing
        // jobPool= field in the [shadow-perf] line anyway, because that is the only
        // place a silent truncation shows up.
        // ==================================================================
        // The face schedule (::GenerateFrustumQueries :3481-3502, reproduced exactly).
        // ⚠ The shipped console mb30hzEnvironmentMap == false selects all six every frame
        // (Construct seeds it false; only the debug menu "Render environment map at 30hz"
        // flips it). ON PC THE SEED IS config.ini [Settings] EnvironmentMap30Hz
        // (renderengine::gEnvironmentMap30Hz, DEFAULT 1 -- a documented perf deviation,
        // reflections step 2: the pass is draw-bound at the D3D9 per-draw floor, ~1.7 ms
        // for six faces; see device.h). It seeds the CONSOLE'S OWN member once, here, the
        // same way gEnvironmentMap seeds mbRenderEnvmap. The env vars keep their override
        // for measuring: BRN_ENVMAP_30HZ=1 forces the half schedule, BRN_ENVMAP_ALLFACES=1
        // forces all six.
        static s32 siEnvMapAllFaces = -1;
        static s32 siEnvMap30Hz     = -1;
        if ( siEnvMapAllFaces < 0 )
        {
            const char* lpcAll = std::getenv( "BRN_ENVMAP_ALLFACES" );
            siEnvMapAllFaces = ( lpcAll != 0 && lpcAll[0] != '0' ) ? 1 : 0;
            const char* lpc30 = std::getenv( "BRN_ENVMAP_30HZ" );
            siEnvMap30Hz = ( lpc30 != 0 && lpc30[0] != '0' ) ? 1 : 0;
            mb30hzEnvironmentMap = ( renderengine::gEnvironmentMap30Hz != 0 );
            if ( siEnvMap30Hz != 0 )
            {
                mb30hzEnvironmentMap = true;
            }
            if ( siEnvMapAllFaces != 0 )
            {
                mb30hzEnvironmentMap = false;
            }
        }

        // [FLAG PC bring-up] the gate. The console reads
        // lpDispatchInputBuffer->GetRenderSwitches()->mbRenderEnvironmentMap; this
        // producer owns no dispatch input buffer, so the honest gate is "the six face
        // cameras have been positioned at least once" -- see the member's banner in
        // BrnWorldModule.h. (CROSS-GROUP REQUEST in the report: once the renderer
        // exposes its own RendererIO::RenderSwitches, that switch becomes the outer
        // gate and this latch stays only as the not-yet-positioned guard.)
        // OUTER GATE (verify F5): renderengine::gEnvironmentMap -- the config.ini seed of
        // the console's mbRenderEnvironmentMap switch, the same word the renderer seeds
        // mRenderSwitches.mbRenderEnvmap from. Off => no queries, no legs, no flags.
        const bool lbEnvMapArmLive =
            ( renderengine::gEnvironmentMap != 0 ) && mbEnvMapCamerasPositionedBringUp;

        bool labEnvMapFaceStaged[ 6 ] = { false, false, false, false, false, false };
        s32  liEnvMapFacesStaged      = 0;
        if ( lbEnvMapArmLive )
        {
            if ( !mb30hzEnvironmentMap || mbFirstRenderFrame || siEnvMapAllFaces != 0 )
            {
                for ( s32 liFace = 0; liFace < 6; liFace++ )
                {
                    mabEnvMapFaceRender[ liFace ] = true;
                }
            }
            else
            {
                const bool lbFirstHalf = mbRenderFirstEnvMapFaces;

                mabEnvMapFaceRender[ 0 ] = lbFirstHalf;
                mabEnvMapFaceRender[ 1 ] = lbFirstHalf;
                mabEnvMapFaceRender[ 2 ] = lbFirstHalf;
                mabEnvMapFaceRender[ 3 ] = !lbFirstHalf;
                mabEnvMapFaceRender[ 4 ] = !lbFirstHalf;
                mabEnvMapFaceRender[ 5 ] = !lbFirstHalf;

                mbRenderFirstEnvMapFaces = !lbFirstHalf;
            }
            mbFirstRenderFrame = false;

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

                // negate = TRUE: the face cameras are right-handed. See the full asm
                // derivation on the same call in ::GenerateFrustumQueries above.
                CgsGraphics::CameraRwFrustum lFaceRwFrustum;
                mEnvironmentMap.maEnvMapCameras[ liFace ].GetFrustumPerspective( lFaceRwFrustum, true );
                CgsGeometric::Frustum lFaceFrustum;
                lFaceFrustum.SetFromRwFrustum( lFaceRwFrustum );

                CgsSceneManager::SceneManagerIO::InEventFrustumTestVp lEvent;
                lEvent.mViewProjection = lProjectedCamera.GetViewProjectionMatrix();
                for ( s32 liPlane = 0; liPlane < 8; liPlane++ )
                {
                    lEvent.maFrustumPlanes[ liPlane ] = lFaceFrustum.maSwizzledPlanes[ liPlane ];
                }
                lEvent.mQueryId            = KA_FRUSTUM_QUERY_IDS[ 2 + liFace ];
                lEvent.mx32EntityTypeFlags = 1024u;   // asm @0x827DB1{..}: `v66 = 1024` (event+0xC4)
                lEvent.mxQueryFlags        = 0u;      // asm: `v67 = 0` (event+0xC8)

                sQueryInput.GetInCoarseQueryQueue()->AddEvent( &lEvent, 4, sizeof( lEvent ) );
                labEnvMapFaceStaged[ liFace ] = true;
                ++liEnvMapFacesStaged;
            }
        }
        gShadowPerf.miEnvMapFaces = liEnvMapFacesStaged;

        // ---- stage the three shadow cascades (the same three events
        //      WorldModule::GenerateFrustumQueries @0x827DADF8 emits for
        //      FrustumQuery_Shadowmap0..2 -- this file, :3615-3636) ----
        // [FLAG PC bring-up] The console additionally gates this on the dispatch input
        // buffer's RenderSwitches.mbRenderShadowMap; there is no dispatch input buffer
        // in this producer (see the FLAG above), so the ShadowMap's own enable is the
        // only gate, plus the finiteness park from part 1.
        //
        // ✅ THE SUSPECT FLAGGED HERE ON 2026-08-12 WAS REAL, AND IT WAS OURS. Re-read of
        // @0x827DADF8's shadow leg (@0x827DB250..0x827DB300) settles it: the console does
        // NOT call Camera::GetFrustumPerspective for the cascades. It flat-copies 128 bytes
        // from mShadowMap + 0x1340 + i*0x80 == maFrustum[i] == ShadowMap::GetFrustum(i),
        // which ComputeOptimalViewVolume fits to cascade i's 0..10.5 / 10.5..34 / 34..120 m
        // sub-frustum slab. The identical-caster-sets symptom (c0 == c1 == c2 in every
        // [shadow-pass] line) was this mirror handing all three the same ~650 m cone, not a
        // console defect. The full asm derivation lives at ::GenerateFrustumQueries above,
        // which is fixed in the same change; this stays a faithful mirror OF THAT.
        //
        // The VIEW-PROJECTION was already right (maCgsShadowMapCamera[i].mViewProjection),
        // so it is unchanged.
        if ( lbShadowArmLive )
        {
            for ( s32 liCascade = 0; liCascade < 3; liCascade++ )
            {
                const CgsGraphics::Camera* lpCascadeCamera =
                    mShadowMap.GetCascadeCamera( liCascade );

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

                sQueryInput.GetInCoarseQueryQueue()->AddEvent( &lEvent, 4, sizeof( lEvent ) );
            }
        }
        sQueryInput.UnlockForWrite();
        gShadowPerf.mfStageUs += ShadowPerfUsSince( lStageStart );

        // ---- THE KNOWN STRUCTURAL DIVERGENCE. DO NOT RE-INVESTIGATE. ----------------
        // This is the query round trip -- the four octree walks (main view + three
        // cascades). It is the ONE place in this producer where the PC build is
        // architecturally slower than the console, and the gap is understood:
        //
        //   * the console runs these as FOUR PARALLEL JOBS. LooseOctree::
        //     StartFrustumTestJobs @0x828B23E0 hands each job to JobScheduler::AddJobs,
        //     and SceneManagerModule's KA_FRUSTUM_QUERY_JOB_INDEX map spreads the staged
        //     queries across them. The PC leaf has no job scheduler, so it runs every
        //     query INLINE on this thread (see the FLAG in StartFrustumTestJobs).
        //   * the console also overlaps this whole producer with the render thread; the
        //     bring-up spine runs producer and render serially on one thread.
        //
        // MEASURED 2026-08-12: query=136.8 us/frame of a 60,361 us frame. Even taken as
        // 100% avoidable it is 0.2% of the frame, so closing this gap is worth nothing
        // today -- it is recorded here only so a future reader does not re-derive it.
        // It becomes relevant when (and only when) the job scheduler lands.
        const ShadowPerfClock::time_point lQueryStart = ShadowPerfNow();
        mSceneModule.ProcessFrustumTestJobRequests( 0, 0, &sQueryInput, &sQueryOutput );
        mSceneModule.ProcessFrustumTestJobResults( 0, 0, &sQueryInput, &sQueryOutput );
        gShadowPerf.mfQueryUs += ShadowPerfUsSince( lQueryStart );

        // ---- filter the result into the per-owner id lists ----
        sQueryOutput.LockForRead();
        const CgsSceneManager::SceneManagerIO::OutputBuffer::SceneQueryResultsQueue* lpResultsQueue =
            sQueryOutput.GetSceneQueryResultsQueue();

        // [DIAG shadow-perf] THE JOB-RESULT-POOL HEADROOM WALK. All four queries share job
        // 0's single 8192-entry u16 run pool (muCurrentWriteOffset is per-job cumulative and
        // is reset only in StartFrustumTestJobs), and LooseOctree::PushCoarseResult drops
        // SILENTLY once it is full -- so a cascade can lose its whole caster set with no
        // symptom other than an empty list. Summing the batches' miNumResults is exactly the
        // number of u16 entries the pool took this frame. Read-only, at most a handful of
        // events, and deliberately outside every stopwatch above.
        {
            const CgsModule::Event* lpPoolProbe = 0;
            s32 liPoolProbeSize = 0;
            s32 liPoolProbeType = lpResultsQueue->GetFirstEvent( &lpPoolProbe, &liPoolProbeSize );
            s32 liPoolTotal   = 0;
            s32 liPoolBatches = 0;
            s32 laiPoolPerJob[ 4 ] = { 0, 0, 0, 0 };
            while ( liPoolProbeType >= 0 && lpPoolProbe != 0 && liPoolBatches < 16 )
            {
                const s32 liBatchResults =
                    static_cast< const CgsSceneManager::SceneManagerIO::OutCoarseQueryResult* >(
                        lpPoolProbe )->miNumResults;
                liPoolTotal += liBatchResults;
                laiPoolPerJob[ liPoolBatches / 4 ] += liBatchResults;   // batch i -> job i/4
                ++liPoolBatches;
                liPoolProbeType =
                    lpResultsQueue->GetNextEvent( lpPoolProbe, &lpPoolProbe, &liPoolProbeSize );
            }
            gShadowPerf.miPoolResults = liPoolTotal;
            gShadowPerf.miPoolBatches = liPoolBatches;
            gShadowPerf.miPoolMaxJob  = 0;
            for ( s32 liJob = 0; liJob < 4; ++liJob )
            {
                gShadowPerf.maiPoolPerJob[ liJob ] = laiPoolPerJob[ liJob ];
                if ( laiPoolPerJob[ liJob ] > gShadowPerf.miPoolMaxJob )
                {
                    gShadowPerf.miPoolMaxJob = laiPoolPerJob[ liJob ];
                }
            }
        }

        const CgsModule::Event* lpFrustumTestResult = 0;
        s32 liResultSize = 0;
        const s32 liResultType = lpResultsQueue->GetFirstEvent( &lpFrustumTestResult, &liResultSize );

        if ( liResultType >= 0 && lpFrustumTestResult != 0 )
        {
            // [DIAG shadow-perf] the main-view filter + buffer seeding.
            const ShadowPerfClock::time_point lMainFilterStart = ShadowPerfNow();
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
            gShadowPerf.mfMainFilterUs += ShadowPerfUsSince( lMainFilterStart );

            // [DIAG shadow-perf] the main-view world leg -- the per-record BASELINE the
            // three cascade world legs below are compared against.
            const ShadowPerfClock::time_point lMainWorldStart = ShadowPerfNow();
            mWorldEntityModule.GenerateDispatchLists(
                &sWorldDispatchInput, sFilteredEntityData.maWorldEntityIds,
                lViewProjection, lEye, lForward, lfLodZoomFactor, &mShaderLodInfo,
                KI_WORLD_OPAQUE_LIST, KI_WORLD_SORT_LAYER, KI_WORLD_SORT_KEY,
                KI_WORLD_PREZ_LIST, false );
            gShadowPerf.mfMainWorldUs += ShadowPerfUsSince( lMainWorldStart );
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
        // ⭐ [FLAG PC bring-up] THE PER-VEHICLE LOD POLICY (2026-08-12).
        //
        // This MIRRORS the console call in WorldModule::GenerateDispatchLists at the
        // `CalculateVehicleLODs(...)` line above (X360 @0x827D24C8) and sits at the
        // console-equivalent position: after the traffic pre-dispatch leg, before the
        // race-car dispatch leg. It exists here only because the frame is driven by
        // this bring-up stand-in rather than by the console producer -- FOLD IT BACK
        // (delete this block) the moment GenerateDispatchListsBringUp is retired.
        //
        // Without it nothing lifts ActiveRaceCar::RenderParams::mLOD off the
        // E_STATE_LOD_4 that RenderParams::Reset seeds, so every car renders at the
        // coarsest LOD forever and 2-/3-state body parts vanish entirely.
        //
        // HONEST SCOPE on this build: maRaceCarEntityIds is whatever the frustum
        // filter produced, so the per-car distance banding only reaches cars the
        // scene manager actually registered as race-car entities (owner 1 / 0x21).
        // The PLAYER-car force-to-LOD-0 leg inside the function does NOT depend on
        // that array -- it keys off the race-car module's own player index -- so the
        // player car comes off LOD 4 regardless.
        //
        // ⭐ [FLAG PC bring-up] THE OTHER CARS (car+lights step 1b, 2026-08-17). On this
        // build race cars are NOT registered as scene entities, so the frustum filter
        // hands CalculateVehicleLODs an EMPTY race-car id list and every non-player car
        // keeps Reset's E_STATE_LOD_4 for ever. That was the "rectangle wheels": Car
        // Select shows a SECOND active race car (the previewed slot, not the player-index
        // one) whose wheels drew their authored LOD-4 box proxy from four metres away
        // ([racecar-lod] alternated "mLOD 0" / "mLOD 4" per RenderRaceCar walk).
        // The console's frustum query would return every active race car in view; the
        // honest stand-in for that result is the set of ACTIVE slots -- banding a car that
        // is out of view is harmless (nothing draws it), and the distance classifier is
        // the real one. Encoded exactly as FilterFrustumTestResults expects (owner 1,
        // entity index = the active-race-car index, part 0 -- :3156-3160).
        // DELETE-WHEN race cars are registered with the scene manager (then the frustum
        // result is non-empty and this stand-in never engages).
        // [DIAG shadow-perf] the main-view race-car leg (LOD policy + dispatch). The SAME
        // race-car dispatch runs again in every cascade below with no near-only gate
        // (ShadowMap::Construct leaves mbRenderRaceCarsNearOnly false), so this number is
        // the per-cascade FIXED cost of the car -- the one that does not move with the
        // world caster count.
        const ShadowPerfClock::time_point lMainCarStart = ShadowPerfNow();
        Array< CgsSceneManager::EntityId, 32u >* lpRaceCarLodIds = &sFilteredEntityData.maRaceCarEntityIds;
        static Array< CgsSceneManager::EntityId, 32u > sRaceCarLodIdsBringUp;
        if ( sFilteredEntityData.maRaceCarEntityIds.GetLength() == 0u )
        {
            sRaceCarLodIdsBringUp.Clear();
            for ( u32 luSlot = 0; luSlot < static_cast< u32 >( E_ACTIVE_RACE_CAR_INDEX_COUNT ); ++luSlot )
            {
                const ActiveRaceCar* lpSlot = mRaceCarEntityModule.GetActiveRaceCar(
                    static_cast< EActiveRaceCarIndex >( luSlot ) );
                if ( lpSlot != 0 && lpSlot->IsActive() )
                {
                    CgsSceneManager::EntityId lId;
                    lId.Set( 1u, luSlot, 0u );          // owner 1 == race car (:3116)
                    sRaceCarLodIdsBringUp.Append( lId );
                }
            }
            lpRaceCarLodIds = &sRaceCarLodIdsBringUp;
        }

        // ==================================================================
        // [FLAG PC bring-up] THE TRAFFIC PRE-DISPATCH LEG.
        //
        // Mirrors WorldModule::GenerateDispatchLists @0x827D1CE8 (this file, :4022-4047) at
        // the console's own position: the traffic dispatch input is seeded with the frustum
        // result alongside the world/race-car/prop ones, then the pre-dispatch pair is
        // Construct()ed fresh, fed the filtered traffic ids and the dispatch camera position,
        // and PreDispatchUpdate runs INSIDE the (input read / output write) lock bracket with
        // CalculateVehicleLODs. That bracket is why the LOD call sits between the two locks.
        //
        // The console producer WorldModule::GenerateDispatchLists has no callers on PC, so
        // this stand-in is the live per-frame producer of the traffic render infos.
        // DELETE-WHEN the console producer is driven. The [T1-dispatch] probe below reports
        // the leg's own liveness.
        // ==================================================================
        {
            sTrafficDispatchInput.LockForWrite();
            // The console clears + re-adds the raw frustum result event on the traffic
            // dispatch input exactly as it does for the other three (:4022-4025). The PC
            // accessor is still a gate that returns NULL (WorldLinkStubs.cpp:611), so this
            // seeding is guarded and does nothing today. It costs nothing either: the traffic
            // GenerateDispatchLists body @0x8273B280 walks the render-info array, not this
            // queue.
            //
            // ⛔ DO NOT RETIRE WorldLinkStubs.cpp:611 (InputBuffer_Dispatch::
            // GetSceneResultQueue) until the queue member is homed AND Constructed. The PC
            // InputBuffer_Dispatch::Construct deliberately omits the console's
            // `VariableEventQueue<32768,16>::Construct(this+4)` leg (@0x8275CF40) because the
            // queue lives inside the opaque maPayloadAndPad span with no named member to
            // reach. sTrafficDispatchInput is a zero-initialised function-local static, so its
            // mbIsConstructed stays false for ever, and both VariableEventQueue::Clear and
            // ::AddEvent FireAssert on `!mbIsConstructed` (CgsVariableEventQueue.h). Retiring
            // the gate first buys an assert every frame, not a working seed. Home the member
            // and restore that Construct leg first.
            if ( sTrafficDispatchInput.GetSceneResultQueue() != 0
                 && liResultType >= 0 && lpFrustumTestResult != 0 )
            {
                sTrafficDispatchInput.GetSceneResultQueue()->Clear();
                sTrafficDispatchInput.GetSceneResultQueue()->AddEvent(
                    lpFrustumTestResult, liResultType, liResultSize );
            }
            sTrafficDispatchInput.SetDispatchFrame( lpDispatchFrame );
            sTrafficDispatchInput.SetShadowMap( &mShadowMap );
            // The corona sink, same source and same per-frame reason as the race-car leg's
            // (BrnCoronaManager::Swap moves the write slot). The traffic corona producers
            // are still gated, so nothing consumes this yet. It is staged because the console
            // asserts it non-null on entry to GenerateDispatchLists (@0x8273B3CC).
            sTrafficDispatchInput.SetCoronaSubmissionInterface( mpBringUpCoronaSubmissionInterface );
            // ⛔ PARK -- NOT staged: the blobby-shadow buffer (console assert
            // "lpBlobbyShadowRenderer" @0x8273B39C). BrnBlobbyShadowManager has no owner on
            // this build: WorldModule holds no BrnBlobbyShadowBuffer and nothing calls
            // SetBlobbyShadowBuffer anywhere in the tree, so there is no pointer to publish.
            // RenderTrafficCar's AddShadow leg is blocked on the same gap.
            sTrafficDispatchInput.UnlockForWrite();

            // :4035-4036 -- the console Construct()s both pre-dispatch buffers every frame
            // (fresh IO-stack allocations there; the statics above here). Both bodies
            // (BrnTrafficEntityModuleIO.cpp, X360 @0x8275CEE8 / @0x8275CF28) open with
            // CgsModule::IOBuffer::Construct(), which arms eStatusConstructed for the
            // LockForRead/LockForWrite below.
            sTrafficPreDispatchInput.Construct();
            sTrafficRenderInfos.Construct();

            sTrafficPreDispatchInput.SetVisibleEntities( sFilteredEntityData.maTrafficEntityIds );
            sTrafficPreDispatchInput.SetCameraPosition( lEye );

            sTrafficPreDispatchInput.LockForRead();
            sTrafficRenderInfos.LockForWrite();
            mTrafficEntityModule.PreDispatchUpdate( &sTrafficPreDispatchInput, &sTrafficRenderInfos );
        }
        CalculateVehicleLODs( lEye, lfLodZoomFactor,
                              *lpRaceCarLodIds,
                              // The console passes `lpTrafficRenderInfos + 4`:
                              // `addi r22, r19, 4` @0x827D24B0 with r19 == the
                              // OutputBuffer_PreDispatch, handed to CalculateVehicleLODs in
                              // r6 @0x827D24BC. That +4 object is
                              // OutputBuffer_PreDispatch::maTrafficRenderInfos (public per
                              // DWARF :457; sizes attested from CreateIOBuffer @0x827B7320
                              // `Alloc 0x308` and Construct @0x8275CF28 `stw r10, 0x304(r3)`).
                              sTrafficRenderInfos.maTrafficRenderInfos );
        sTrafficRenderInfos.UnlockForWrite();
        sTrafficPreDispatchInput.UnlockForRead();
        // [DIAG racecar-lod] value-latched: which slots were banded, from where, to what.
        {
            static u32 suLastKey = 0xFFFFFFFFu;
            u32 luKey = static_cast< u32 >( lpRaceCarLodIds->GetLength() ) << 24;
            for ( u32 luI = 0; luI < lpRaceCarLodIds->GetLength() && luI < 8u; ++luI )
            {
                const ActiveRaceCar* lpCar = mRaceCarEntityModule.GetActiveRaceCar(
                    static_cast< EActiveRaceCarIndex >( ( *lpRaceCarLodIds )[ luI ].GetEntityIndex() ) );
                luKey ^= static_cast< u32 >( lpCar->GetRenderParams()->GetLOD() ) << ( 3u * luI );
            }
            if ( luKey != suLastKey && CgsDev::Log::gpDebugPrint != 0 )
            {
                suLastKey = luKey;
                *CgsDev::Log::gpDebugPrint << "[racecar-lod] banded " << static_cast< s32 >( lpRaceCarLodIds->GetLength() )
                                            << " cars from eye (" << lEye.x << "," << lEye.y << "," << lEye.z << ") zoom "
                                            << lfLodZoomFactor << ":";
                for ( u32 luI = 0; luI < lpRaceCarLodIds->GetLength() && luI < 8u; ++luI )
                {
                    const u32 luSlot = ( *lpRaceCarLodIds )[ luI ].GetEntityIndex();
                    const ActiveRaceCar* lpCar = mRaceCarEntityModule.GetActiveRaceCar(
                        static_cast< EActiveRaceCarIndex >( luSlot ) );
                    const Vector3 lPos = lpCar->GetRenderParams()->GetBodyTransform().Pos();
                    *CgsDev::Log::gpDebugPrint << " slot" << static_cast< s32 >( luSlot )
                                                << " at (" << lPos.x << "," << lPos.y << "," << lPos.z << ") lod "
                                                << static_cast< s32 >( lpCar->GetRenderParams()->GetLOD() );
                }
                *CgsDev::Log::gpDebugPrint << " player " << static_cast< s32 >( mRaceCarEntityModule.GetPlayerActiveRaceCarIndex() ) << "\n";
            }
        }

        {
            sRaceCarDispatchInput.LockForWrite();
            sRaceCarDispatchInput.SetDispatchFrame( lpDispatchFrame );
            sRaceCarDispatchInput.SetShadowMap( &mShadowMap );
            // [FLAG PC bring-up] the corona submission interface (SubmitCoronasForRaceCar's
            // sink), staged by BrnGameModule::DoDispatch this frame. On the console
            // BrnWorldIO::DispatchInputBuffer carries it in from the renderer's Update; here it
            // is applied per frame because BrnCoronaManager::Swap moves the write slot.
            // DELETE-WHEN BridgeRendererToWorld + the world->entity-module IO copy are real.
            sRaceCarDispatchInput.SetCoronaSubmissionInterface( mpBringUpCoronaSubmissionInterface );
            sRaceCarDispatchInput.UnlockForWrite();

            mRaceCarEntityModule.GenerateDispatchLists(
                &sRaceCarDispatchInput, sFilteredEntityData.maRaceCarEntityIds,
                KI_RACE_CAR_OBJECT_LIST, KI_RACE_CAR_OPAQUE_MESH_LIST,
                KI_RACE_CAR_TRANSPARENT_MESH_LIST, false,
                Vector4{ 0.0f, 0.0f, 0.0f, 0.0f }, Vector4{ 0.0f, 0.0f, 0.0f, 0.0f },
                lEye );
        }
        gShadowPerf.mfMainCarUs += ShadowPerfUsSince( lMainCarStart );

        // ==================================================================
        // [FLAG PC bring-up] THE TRAFFIC DISPATCH LEG.
        //
        // Mirrors WorldModule::GenerateDispatchLists @0x827D1CE8's own traffic call (this
        // file, :4103-4110) at the console's own position: immediately after the race-car leg
        // (X360 @0x827D27D4 tests render switch byte 5 the instruction after the race-car
        // leg's StopMonitor) and before the world pass republishes the unscaled key light.
        //
        // ⚠ On the console that order is load-bearing, because both vehicle legs run while
        // shader constants 9/12/18/19 still carry the CAR multipliers (this file, :4071-4090).
        // On the bring-up path they do not: GenerateDispatchListsBringUp publishes only
        // 8 / 3 / 34, and 9/12/18/19 arrive UNSCALED from GenerateShaderConstants
        // (:4842-4860). The position here is kept for fidelity. When the car-multiplier
        // bracket is restored it must wrap BOTH vehicle legs, and this leg already sits
        // inside where it belongs.
        //
        // The three list ids are the console's own literals, read off the call site
        // (`li r8,0x14` @0x827D27FC, `li r7,0x13` @0x827D2804, `li r6,0xC` @0x827D280C):
        // 12 is the dispatch OBJECT list, 19/20 the OPAQUE and TRANSPARENT mesh bins. Same
        // three the race car uses, hence the race-car constants rather than fresh literals.
        //
        // Argument order comes from the DWARF, verbatim: lpInput, laTrafficRenderInfos,
        // lFogScattering, lFogColourPlusWhiteLevel, lCameraPosition, lCameraDirection,
        // liModelOnlyDisplayList, liOpaqueList, liTransparentList, lBrnCamera. Arg 2 is the
        // Array<VehicleRenderInfo,64> INSIDE the buffer, not the buffer (`addi r22, r19, 4`
        // @0x827D24B0 then `mr r5, r22` @0x827D2814). On the host the two addresses differ:
        // IOBuffer leads with a 1-byte FlagSet8 and the Array is 4-aligned, so passing the
        // buffer would read miCount out of the status byte. Contrast PreDispatchUpdate above,
        // which does take the buffer (`mr r5, r19` @0x827D249C).
        // ==================================================================
        {
            const ShadowPerfClock::time_point lMainTrafficStart = ShadowPerfNow();

            // [DIAG T1-dispatch] the OBJECT-list occupancy across the traffic leg. Read at
            // the consuming end, the list the submission leaf appends to, so a module that
            // runs but submits nothing reads differently from one that never runs.
            const s32 liTrafficRecordsBefore =
                lpDispatchFrame->GetList( KI_RACE_CAR_OBJECT_LIST )->GetCount();

            // ⛔ FLAG (bring-up): the two FOG vectors are ZERO here, as the race-car leg
            // twelve lines above also passes zeros. GenerateDispatchListsBringUp has no
            // DispatchOutputBuffer to read lvFogScattering / lvFogColourPlusWhiteLevel from
            // (GenerateShaderConstants produces them into the buffer the dead console producer
            // reads at :3918). Shader constant 26's per-car fog blend is therefore {0,0,0,1}
            // for traffic, i.e. no distance fog on traffic cars, matching the race car.
            // ⛔ DELETE-WHEN BridgeRendererToWorld carries the dispatch output buffer.
            mTrafficEntityModule.GenerateDispatchLists(
                &sTrafficDispatchInput, sTrafficRenderInfos.maTrafficRenderInfos,
                Vector4{ 0.0f, 0.0f, 0.0f, 0.0f }, Vector4{ 0.0f, 0.0f, 0.0f, 0.0f },
                lEye, lForward,
                KI_RACE_CAR_OBJECT_LIST, KI_RACE_CAR_OPAQUE_MESH_LIST,
                KI_RACE_CAR_TRANSPARENT_MESH_LIST,
                *GetLastCameraInput() );

            gShadowPerf.mfMainTrafficUs += ShadowPerfUsSince( lMainTrafficStart );

            // ---- [DIAG T1-dispatch] ----------------------------------------------
            // ⛔ DELETE-WHEN parked traffic is confirmed on a booted run.
            // Gated on BRN_TRAFFIC_DIAG. Two probes, neither a printed-once bool: the first
            // frame a traffic renderable reaches the object list (the one line that says the
            // whole chain closed), and the per-frame submitted count latched on the VALUE, so
            // 0 -> 7 -> 12 prints three lines and a steady world stays quiet.
            {
                static const bool skbTrafficDiag = ( std::getenv( "BRN_TRAFFIC_DIAG" ) != 0 );
                if ( skbTrafficDiag && CgsDev::Log::gpDebugPrint != 0 )
                {
                    const s32 liTrafficRecords =
                        lpDispatchFrame->GetList( KI_RACE_CAR_OBJECT_LIST )->GetCount()
                        - liTrafficRecordsBefore;

                    static bool sbLoggedFirstTrafficRecord = false;
                    if ( !sbLoggedFirstTrafficRecord && liTrafficRecords > 0 )
                    {
                        sbLoggedFirstTrafficRecord = true;
                        *CgsDev::Log::gpDebugPrint
                            << "[T1-dispatch] FIRST traffic renderable submitted: "
                            << liTrafficRecords << " record(s) into object list "
                            << KI_RACE_CAR_OBJECT_LIST << " (mesh bins "
                            << KI_RACE_CAR_OPAQUE_MESH_LIST << "/"
                            << KI_RACE_CAR_TRANSPARENT_MESH_LIST << ") from eye ("
                            << lEye.x << ", " << lEye.y << ", " << lEye.z << ")\n";
                    }

                    static s32 siLastTrafficRecords = -1;
                    static u32 suLastVisibleTraffic = 0xFFFFFFFFu;
                    const u32  luVisibleTraffic =
                        sFilteredEntityData.maTrafficEntityIds.GetLength();
                    if ( liTrafficRecords != siLastTrafficRecords
                         || luVisibleTraffic != suLastVisibleTraffic )
                    {
                        siLastTrafficRecords = liTrafficRecords;
                        suLastVisibleTraffic = luVisibleTraffic;
                        *CgsDev::Log::gpDebugPrint
                            << "[T1-dispatch] visibleTrafficIds " << static_cast< s32 >( luVisibleTraffic )
                            // GetCount(), not GetLength(): GetLength() asserts the array has
                            // left the KI_UNCONSTRUCTED(-1) sentinel, and a probe must never
                            // be the thing that fires an assert.
                            << " renderInfos " << static_cast< s32 >(
                                   sTrafficRenderInfos.maTrafficRenderInfos.GetCount() )
                            << " submitted " << liTrafficRecords << "\n";
                    }
                }
            }
        }

        // ==================================================================
        // ⭐ [FLAG PC bring-up] THE PROPS (prop-spawn wave, 2026-08-12).
        //
        // MIRRORS the console leg in WorldModule::GenerateDispatchLists (this file,
        // X360 @0x827D1CE8 -> the mPropEntityModule.GenerateDispatchLists call) and sits
        // at the console-equivalent position: after the race-car dispatch leg.
        //
        // WHY IT IS HERE AT ALL: FilterFrustumTestResults above has ALWAYS been filling
        // sFilteredEntityData.maPropEntityIds (its `case 3:` / `case 0x22:` arm -- owner 3
        // is a whole prop, 0x22 a part of a smashed one), and this bring-up producer then
        // dropped the array on the floor every single frame. That is the last link in the
        // "no props anywhere" chain: even once the module spawns them and the culler sees
        // them, nothing asked the prop module to draw them.
        //
        // The console stages this buffer through BridgeWorldModuleToEntityModules_Render;
        // that bridge is still an inert gate, so -- exactly as the world and race-car legs
        // above already do -- the frame and shadow map are seeded directly here. DELETE
        // this seeding (not the call) when that bridge lands.
        // No render-switch guard and no perf monitor here, matching the race-car leg above:
        // this bring-up producer has no dispatch input buffer to read GetRenderSwitches()
        // from (the real GenerateDispatchLists gets one), and PerfMonCpu is not in scope at
        // this point in the file. Both come back with the real producer.
        {
            const ShadowPerfClock::time_point lMainPropStart = ShadowPerfNow();
            sPropDispatchInput.LockForWrite();
            sPropDispatchInput.SetDispatchFrame( lpDispatchFrame );
            sPropDispatchInput.SetShadowMap( &mShadowMap );
            sPropDispatchInput.UnlockForWrite();

            mPropEntityModule.GenerateDispatchLists(
                &sPropDispatchInput, sFilteredEntityData.maPropEntityIds,
                lViewProjection, lEye, lfLodZoomFactor, &mShaderLodInfo,
                // object list 11 / opaque 11 / transparent 15 -- the console's own triple.
                // Trailing pair: not the environment map, and coronas on (main view).
                11, 11, 15, false, true );
            gShadowPerf.mfMainPropUs += ShadowPerfUsSince( lMainPropStart );
        }

        // The MAIN-VIEW visible-world count, snapshotted here because the shadow arm
        // below re-uses sFilteredEntityData for each cascade -- without this the two
        // culling diagnostics at the bottom of this function would silently start
        // reporting the LAST CASCADE's count under the name `visibleWorld`.
        const s32 liMainViewVisibleWorld =
            static_cast< s32 >( sFilteredEntityData.maWorldEntityIds.GetLength() );
        gShadowPerf.miMainWorldEnts    = liMainViewVisibleWorld;
        gShadowPerf.miMainWorldRecords = static_cast< s32 >(
            lpDispatchFrame->GetList( KI_WORLD_OPAQUE_LIST )->GetCount() );
        for ( s32 liSlot = 0; liSlot < 3; liSlot++ )
        {
            gShadowPerf.maiCascadeWorldEnts[ liSlot ]    = 0;
            gShadowPerf.maiCascadeCarRecords[ liSlot ]   = 0;
            gShadowPerf.maiCascadeWorldRecords[ liSlot ] = 0;
        }

        // ==================================================================
        // ⭐ [FLAG PC bring-up] ENVIRONMENT-MAP PRODUCER, PART 2 -- the six face
        //    dispatch legs (mesh lists 5..10).
        //
        // MIRRORS WorldModule::GenerateDispatchLists @0x827D1CE8's env-map block (this
        // file, :4002-4092) at the CONSOLE'S OWN POSITION: after the main-view world /
        // race-car / prop legs, before the shadow cascades. That ordering is load-bearing
        // for the shader-constant table, which is a DELTA channel -- SetShaderConstantData
        // allocates a FRESH copy of the value in the dispatch bin and records the pointer
        // (CgsShaderConstantTable.cpp:148-165), DrawRenderable::AddToBin drains the dirty
        // list of pointers into each record as it is emitted
        // (CgsDispatcherCommands.cpp:517-518), and DrawRenderable::Interpret restores them
        // into the object context at replay (CgsDispatcherCommands.cpp:891-899). So the
        // main-view records already carry the main-view camera before this leg republishes
        // slots 8/3/34, and each face's FIRST record carries that face's view-projection.
        //
        // WHAT IS STAND-IN HERE, and only this:
        //   * the shader-constants FRAME. The console writes the env-map view position and
        //     the six face view-projections into lpDispatchInputBuffer->
        //     GetShaderConstantsFrame(); this producer writes them into
        //     gBrnWorldShaderConstantsFrameBringUp, the same frame the main publish above
        //     already fills, which is the frame the renderer reads on PC.
        //   * the dispatch-thread input buffer, staged by
        //     SetBringUpDispatchThreadInputBuffer (see the header). The console gets it
        //     off the dispatch input buffer at :3725 and holds ONE write lock across the
        //     whole of GenerateDispatchLists; here the arm brackets its own, exactly as
        //     BrnParticle::PCBringUpProduceParticleRenderData does
        //     (ParticleModuleBringUp.cpp:445-447).
        //   * TRAFFIC and RACE CARS are not in this block on the console either -- the
        //     env-map face renders the WORLD and the PROPS only (:4061 and :4068).
        // ==================================================================
        // The walking result cursor. The main-view legs above consumed
        // lpFrustumTestResult; every staged env-map face consumes exactly one more, and
        // the cascade arm below then walks on from wherever this leaves it.
        const CgsModule::Event* lpEnvMapResultCursor = lpFrustumTestResult;
        s32                     liEnvMapCursorSize   = liResultSize;
        s32                     liEnvMapCursorType   = liResultType;

        const ShadowPerfClock::time_point lEnvMapStart = ShadowPerfNow();
        gShadowPerf.miEnvMapWorldEnts = 0;
        gShadowPerf.miEnvMapRecords   = 0;
        if ( liEnvMapFacesStaged > 0 && liResultType >= 0 && lpFrustumTestResult != 0 )
        {
            // :4007-4010. ⭐ LIVE as of 2026-08-17: mbIsInJunkyard is latched at the top of
            // THIS producer now, by the console's own camera-flag block (see the
            // "junkyard lighting latch" banner above), off the director camera's
            // IsInJunkyard() staged through SetBringUpCameraOverride. Until then nothing on
            // PC ever set it and this line could not fire (envproducer B3).
            if ( mbIsInJunkyard )
            {
                mShadowMap.SetConstantsForEnvmap();
            }

            // :4014 -- the env-map view POSITION, published once for all six faces.
            gBrnWorldShaderConstantsFrameBringUp.LockForWriting();
            gBrnWorldShaderConstantsFrameBringUp.SetEnvMapViewPosition( lEye );
            gBrnWorldShaderConstantsFrameBringUp.UnlockForWriting();

            BrnGame::DispatchThreadInputBuffer* lpDispatchThreadInputBuffer =
                mpBringUpDispatchThreadInputBuffer;
            if ( lpDispatchThreadInputBuffer != 0 )
            {
                lpDispatchThreadInputBuffer->LockForWrite();
            }

            for ( s32 liFace = 0; liFace < 6; liFace++ )
            {
                // :4018 -- ALL SIX flags are written every dispatch, false included.
                if ( lpDispatchThreadInputBuffer != 0 )
                {
                    lpDispatchThreadInputBuffer->SetEnvMapFaceRender(
                        static_cast< u32 >( liFace ), mabEnvMapFaceRender[ liFace ] );
                }

                if ( !labEnvMapFaceStaged[ liFace ] )
                {
                    continue;
                }

                // :3883 + :4087 -- the console consumes the MAIN-VIEW batch before this
                // block (GetNextEvent at :3883) and one batch per face inside it. This
                // producer's main-view legs do NOT advance (lpFrustumTestResult is
                // const-seeded at the GetFirstEvent above), so the step to the next batch
                // happens HERE, BEFORE the face is served: face f consumes batch[1+f]
                // (batch[0] is the main view; the faces are staged between the main view
                // and the cascades, and batch order is submission order --
                // CgsSceneManagerModule.cpp's i/4 job map is non-decreasing). Verify
                // finding F1 (reflections step 1): the first cut advanced at the BOTTOM
                // of the loop, so face 0 was served the whole MAIN-VIEW entity set and
                // every other face its neighbour's -- the cube rotated by one face and
                // list 5 carried thousands of extra records, with no log line.
                liEnvMapCursorType = lpResultsQueue->GetNextEvent(
                    lpEnvMapResultCursor, &lpEnvMapResultCursor, &liEnvMapCursorSize );
                if ( liEnvMapCursorType < 0 || lpEnvMapResultCursor == 0 )
                {
                    // Verify F3: a face flagged RENDERED with no records would make the
                    // renderer run a full clear/dispatch-nothing/sky/resolve on it -- the
                    // console cannot reach this state (it asserts). Un-flag this face and
                    // every later one before leaving.
                    if ( lpDispatchThreadInputBuffer != 0 )
                    {
                        for ( u32 luClear = static_cast< u32 >( liFace ); luClear < 6u; ++luClear )
                        {
                            lpDispatchThreadInputBuffer->SetEnvMapFaceRender( luClear, false );
                        }
                    }
                    break;
                }

                // The console asserts the result id here (:4025-4028). A hard assert
                // would storm the log on a bring-up mis-order, and the id table is
                // all-zero on PC anyway, so this is the cascade arm's soft, value-latched
                // park instead.
                const u32 luResultId =
                    reinterpret_cast< const CgsSceneManager::SceneQueryId* >( lpEnvMapResultCursor )->mId;
                if ( luResultId != KA_FRUSTUM_QUERY_IDS[ 2 + liFace ].mId )
                {
                    static u32 suLastEnvMismatchId = 0xFFFFFFFFu;
                    if ( luResultId != suLastEnvMismatchId && CgsDev::Log::gpDebugPrint != 0 )
                    {
                        suLastEnvMismatchId = luResultId;
                        *CgsDev::Log::gpDebugPrint
                            << "[envmap-prod] PARKED: face " << liFace
                            << " result carries query id " << static_cast< s32 >( luResultId )
                            << ", expected " << static_cast< s32 >( KA_FRUSTUM_QUERY_IDS[ 2 + liFace ].mId )
                            << " -- the env-map queries did not come back in submission order\n";
                    }
                    if ( lpDispatchThreadInputBuffer != 0 )
                    {
                        for ( u32 luClear = static_cast< u32 >( liFace ); luClear < 6u; ++luClear )
                        {
                            lpDispatchThreadInputBuffer->SetEnvMapFaceRender( luClear, false );
                        }
                    }
                    break;
                }

                sFilteredEntityData.Clear();

                // :4032 -- the LOCAL COPY. The member camera is never mutated by the
                // dispatch pass (the far-clip push at the bottom lands on this copy).
                CgsGraphics::Camera lFaceCamera = mEnvironmentMap.maEnvMapCameras[ liFace ];

                FilterFrustumTestResults( lpEnvMapResultCursor,
                                          &sFilteredEntityData.maWorldEntityIds,
                                          &sFilteredEntityData.maRaceCarEntityIds,
                                          &sFilteredEntityData.maTrafficEntityIds,
                                          &sFilteredEntityData.maPropEntityIds );

                sWorldDispatchInput.LockForWrite();
                sWorldDispatchInput.GetSceneResultQueue()->Clear();
                sWorldDispatchInput.GetSceneResultQueue()->AddEvent(
                    lpEnvMapResultCursor, liEnvMapCursorType, liEnvMapCursorSize );
                sWorldDispatchInput.SetDispatchFrame( lpDispatchFrame );
                sWorldDispatchInput.SetShadowMap( &mShadowMap );
                sWorldDispatchInput.UnlockForWrite();

                // :4056-4058 -- the per-face camera constants. Slot 8: X360 0x827D2BC4
                // `lvx128 v1, r30, r20` with r20 = 6170304 == WorldModule + 6168096
                // (maEnvMapCameras[6] end) == mEnvironmentMap.mCameraPosition -- the CUBE
                // CENTRE, i.e. the player-car position EnvironmentMap::Update was handed
                // (:3046), NOT the director camera. The other three eye arguments in this
                // block are v127 == the director camera position (0x827D2048, r19+0x30)
                // and stay `lEye`. (Verify finding F2: the first cut passed lEye here --
                // wrong by the chase-camera offset, swinging as the camera orbits.)
                // 3 is the face view-projection, 34 its modified form.
                CgsGraphics::mShaderConstantTable.SetShaderConstantData( 8, mEnvironmentMap.mCameraPosition );
                CgsGraphics::mShaderConstantTable.SetShaderConstantData(
                    3, lFaceCamera.GetViewProjectionMatrix() );
                CgsGraphics::mShaderConstantTable.SetShaderConstantData(
                    34, lFaceCamera.GetViewProjectionMatrixModified() );

                const s32 liFaceList        = 5 + liFace;
                const s32 liRecordsBefore   = static_cast< s32 >(
                    lpDispatchFrame->GetList( liFaceList )->GetCount() );

                // :4061 -- the world leg. NOT ::GenerateDispatchLists: the env-map feed is
                // its own console entry point (fixed LOD miEnvironmentMapLOD, the shader
                // LOD info's env-map technique), BrnWorldEntityModule.cpp:1959.
                mWorldEntityModule.GenerateDispatchListsForEnvironmentMap(
                    &sWorldDispatchInput, sFilteredEntityData.maWorldEntityIds,
                    lFaceCamera.GetViewProjectionMatrix(), lEye,
                    &mShaderLodInfo, liFaceList, liFaceList, liFaceList );

                // :4068 -- the prop leg, with the console's own trailing pair
                // (lbRenderingEnvironmentMap = true, lbRenderCoronas = false) and its
                // hard-coded 1.0f zoom factor.
                sPropDispatchInput.LockForWrite();
                sPropDispatchInput.SetDispatchFrame( lpDispatchFrame );
                sPropDispatchInput.SetShadowMap( &mShadowMap );
                sPropDispatchInput.UnlockForWrite();
                mPropEntityModule.GenerateDispatchLists(
                    &sPropDispatchInput, sFilteredEntityData.maPropEntityIds,
                    lFaceCamera.GetViewProjectionMatrix(), lEye,
                    1.0f, &mShaderLodInfo, liFaceList, liFaceList, liFaceList, true, false );

                // :4080-4085 -- push the far clip out for the RENDERER's copy of the face
                // view-projection and publish it into the frame. SetPerspectiveProjection-
                // MatrixRightHanded is last, so the published matrix is the RIGHT-HANDED
                // one (which is why the query above negates).
                lFaceCamera.SetFarClipPlane( 10000.0f );   // the store + the rebuild, one member
                lFaceCamera.SetPerspectiveProjectionMatrixRightHanded();
                gBrnWorldShaderConstantsFrameBringUp.LockForWriting();
                gBrnWorldShaderConstantsFrameBringUp.SetEnvMapViewProjectionMatrix(
                    static_cast< BrnGraphics::EEnvironmentMapFace >( liFace ),
                    lFaceCamera.GetViewProjectionMatrix() );
                gBrnWorldShaderConstantsFrameBringUp.UnlockForWriting();

                gShadowPerf.miEnvMapWorldEnts +=
                    static_cast< s32 >( sFilteredEntityData.maWorldEntityIds.GetLength() );
                gShadowPerf.miEnvMapRecords +=
                    static_cast< s32 >( lpDispatchFrame->GetList( liFaceList )->GetCount() )
                    - liRecordsBefore;
                // (the cursor already advanced at the top of this iteration -- see the
                //  F1 note there; the post-loop cursor is the last CONSUMED face batch,
                //  which is what the cascade arm's GetNextEvent seed below relies on.)
            }

            if ( lpDispatchThreadInputBuffer != 0 )
            {
                lpDispatchThreadInputBuffer->UnlockForWrite();
            }

            // Re-publish the MAIN-VIEW camera constants, the same belt-and-braces guard
            // the cascade arm below uses: not console behaviour (the console leaves the
            // last face's constants staged because its renderer republishes per pass), but
            // it makes this arm provably invisible to every consumer downstream.
            CgsGraphics::mShaderConstantTable.SetShaderConstantData( 8, lEye );
            CgsGraphics::mShaderConstantTable.SetShaderConstantData( 3, lViewProjection );
            CgsGraphics::mShaderConstantTable.SetShaderConstantData( 34, lViewProjectionModified );

            // ONE line, the first time a face is actually produced, so the conductor can
            // see the arm go live without a per-frame log. Value-latched on the record
            // count, never a bare `static bool` (this producer runs on the loading screen
            // long before the world exists -- the project has learned that three times).
            static s32 siEnvMapLoggedRecords = -1;
            if ( gShadowPerf.miEnvMapRecords > 0 && siEnvMapLoggedRecords < 0
                 && CgsDev::Log::gpDebugPrint != 0 )
            {
                siEnvMapLoggedRecords = gShadowPerf.miEnvMapRecords;
                *CgsDev::Log::gpDebugPrint
                    << "[envmap-prod] first faces produced: staged=" << liEnvMapFacesStaged
                    << " lists=5.." << ( 5 + 5 )
                    << " worldEnts=" << gShadowPerf.miEnvMapWorldEnts
                    << " records=" << gShadowPerf.miEnvMapRecords
                    << " eye=(" << lEye.x << "," << lEye.y << "," << lEye.z << ")\n";
            }
        }
        gShadowPerf.mfEnvMapUs += ShadowPerfUsSince( lEnvMapStart );

        // ==================================================================
        // ⭐ [FLAG PC bring-up] SHADOW PRODUCER, PART 2 -- the cascade dispatch legs.
        //
        // MIRRORS WorldModule::GenerateShadowMapDispatchLists @0x827C96D8 (this file,
        // :4151-4297), the console's real 3-cascade producer, which is bodied and linked
        // and unreachable (its only caller, ::GenerateDispatchLists, has no callers).
        //
        // Console order is preserved: the main-view legs above run FIRST, then the
        // cascades, exactly as @0x827D1CE8 does. That matters for the shader-constant
        // table, which is a DELTA channel -- DrawRenderable::AddToBin drains the dirty
        // list into each command as it is emitted (CgsDispatcherCommands.cpp:516), so the
        // main-view records already carry the main-view camera constants before this leg
        // touches slots 8/3/34. (Belt and braces: the main-view values are re-published
        // at the bottom of this block anyway, so the table's end-of-frame state is byte
        // for byte what it was before this arm existed.)
        //
        // SCOPE, honestly: TRAFFIC and PROPS are gated OFF. The console seeds their
        // dispatch inputs here too, but this producer owns no BrnTrafficIO /
        // PropEntityIO buffers (the console gets them from DoDispatch's IO stacks, which
        // do not exist), and inventing them would be fabrication. World + race cars --
        // the two feeds this producer already drives for the main view -- are the ones
        // that matter, and they are wired for real.
        //
        // CASCADE -> LIST MAP, read off :4251 (`liCascadeList = (c >= 2) ? c + 2 : c`;
        // the traffic feed uses c + 2 throughout, which is why the far cascade skips the
        // env-map ids): cascade 0 -> list 0, cascade 1 -> list 1, cascade 2 -> list 4.
        // Those are GDL OBJECT lists AND the destination mesh lists; the renderer sorts
        // {0,2,1,3,4} and its cascade passes are still gated off
        // (BrnRendererModule::RenderWorldPasses), so nothing is drawn from them yet --
        // they are what the "MESH lists:" probe will finally show as non-empty.
        // ==================================================================
        const ShadowPerfClock::time_point lCascadeStart = ShadowPerfNow();
        if ( lbShadowArmLive && liResultType >= 0 && lpFrustumTestResult != 0 )
        {
            const u32 luNumCascades = mShadowMap.GetRenderMultipleShadowMaps() ? 3u : 1u;

            // Walk on from the LAST RESULT THE LEGS ABOVE CONSUMED. That is the
            // main-view result when no env-map face was staged, and the last staged
            // face's otherwise -- lpEnvMapResultCursor is seeded from
            // lpFrustumTestResult and advanced once per produced face, so this one
            // expression covers both. (Before the env-map arm landed this read
            // lpFrustumTestResult directly, which is the same thing when the arm is
            // parked and WRONG the moment it is not.)
            const CgsModule::Event* lpCascadeResult = 0;
            s32 liCascadeSize = 0;
            s32 liCascadeType =
                lpResultsQueue->GetNextEvent( lpEnvMapResultCursor, &lpCascadeResult, &liCascadeSize );

            for ( u32 luCascade = 0; luCascade < luNumCascades; luCascade++ )
            {
                if ( liCascadeType < 0 || lpCascadeResult == 0 )
                {
                    break;
                }

                // The console asserts this (:4176). A hard assert here would storm the
                // log on a bring-up mis-order, so it is a soft, value-latched park.
                const u32 luResultId =
                    reinterpret_cast< const CgsSceneManager::SceneQueryId* >( lpCascadeResult )->mId;
                if ( luResultId != KA_FRUSTUM_QUERY_IDS[ 8 + luCascade ].mId )
                {
                    static u32 suLastMismatchId = 0xFFFFFFFFu;
                    if ( luResultId != suLastMismatchId && CgsDev::Log::gpDebugPrint != 0 )
                    {
                        suLastMismatchId = luResultId;
                        *CgsDev::Log::gpDebugPrint
                            << "[shadow-prod] PARKED: result " << static_cast< s32 >( luCascade )
                            << " carries query id " << static_cast< s32 >( luResultId )
                            << ", expected " << static_cast< s32 >( KA_FRUSTUM_QUERY_IDS[ 8 + luCascade ].mId )
                            << " -- the cascade queries did not come back in submission order\n";
                    }
                    break;
                }

                mShadowMap.SetCurrentCascadeIndex( luCascade );
                sFilteredEntityData.Clear();

                const CgsGraphics::Camera* lpCascadeCamera =
                    mShadowMap.GetCascadeCamera( static_cast< s32 >( luCascade ) );

                // The per-module near-only gates (:4163-4173) minus the RenderSwitches
                // half, which needs the dispatch input buffer this producer has not got.
                const bool lbRaceCars = mShadowMap.GetRenderRaceCarsIntoShadowMap()
                                     && ( luCascade == 0 || !mShadowMap.GetRenderRaceCarsNearOnly() );
                const bool lbWorld    = mShadowMap.GetRenderWorldIntoShadowMap();

                // [DIAG shadow-perf] this cascade's filter + seeding.
                const ShadowPerfClock::time_point lCascFilterStart = ShadowPerfNow();
                FilterFrustumTestResults( lpCascadeResult,
                                          &sFilteredEntityData.maWorldEntityIds,
                                          &sFilteredEntityData.maRaceCarEntityIds,
                                          &sFilteredEntityData.maTrafficEntityIds,
                                          &sFilteredEntityData.maPropEntityIds );

                // Seed the enabled modules' dispatch inputs with this cascade's result.
                sWorldDispatchInput.LockForWrite();
                sWorldDispatchInput.GetSceneResultQueue()->Clear();
                if ( lbWorld )
                {
                    sWorldDispatchInput.GetSceneResultQueue()->AddEvent(
                        lpCascadeResult, liCascadeType, liCascadeSize );
                }
                sWorldDispatchInput.SetDispatchFrame( lpDispatchFrame );
                sWorldDispatchInput.SetShadowMap( &mShadowMap );
                sWorldDispatchInput.UnlockForWrite();

                sRaceCarDispatchInput.LockForWrite();
                sRaceCarDispatchInput.GetSceneResultQueue()->Clear();
                if ( lbRaceCars )
                {
                    sRaceCarDispatchInput.GetSceneResultQueue()->AddEvent(
                        lpCascadeResult, liCascadeType, liCascadeSize );
                }
                sRaceCarDispatchInput.SetDispatchFrame( lpDispatchFrame );
                sRaceCarDispatchInput.SetShadowMap( &mShadowMap );
                sRaceCarDispatchInput.UnlockForWrite();
                gShadowPerf.mfCascadeFilterUs += ShadowPerfUsSince( lCascFilterStart );
                if ( luCascade < 3u )
                {
                    gShadowPerf.maiCascadeWorldEnts[ luCascade ] =
                        static_cast< s32 >( sFilteredEntityData.maWorldEntityIds.GetLength() );
                }

                // Advance to the next cascade's result BEFORE the dispatch legs run --
                // the console does exactly this at :4225 (the event pointer the legs
                // consumed is already copied into the module queues above).
                liCascadeType =
                    lpResultsQueue->GetNextEvent( lpCascadeResult, &lpCascadeResult, &liCascadeSize );

                // ---- THE LATCH. Raised and lowered inside this ONE loop body, with no
                // `continue`, no `break` and no early return between the two, exactly as
                // the console does at :4242 / :4296. Everything that reads it
                // (WorldEntityModule::GenerateDispatchLists, RenderRaceCar) is called
                // between them and nowhere else in this frame; the whole arm runs AFTER
                // the main-view legs; and the producer force-clears it at the top of
                // every frame. There is no path on which the main pass sees it raised.
                mShadowMap.SetRenderingShadowMap( true );

                CgsGraphics::mShaderConstantTable.SetShaderConstantData( 8, lEye );
                CgsGraphics::mShaderConstantTable.SetShaderConstantData(
                    3, lpCascadeCamera->GetViewProjectionMatrix() );

                // ---- [DIAG prop-render wave 2026-08-12] THE ISSIMILAR-STORM WITNESS ----
                // ⛔ DELETE-WHEN the cascade projection satisfies the console tripwire.
                // THIS call site is the source of every
                // `RwMath::IsSimilar( m_projectionMatrix.GetElem(..), 0.0f )` assert in the
                // log (CgsCamera.cpp:548-551, four per call). It is NOT the rw::math::vpu::
                // Inverse stub: those asserts carried this exact callstack BEFORE Inverse was
                // bodied, and they still fire now that Inverse is real and the log contains
                // ZERO `nan(ind)`. Every writer of Camera::mProjection in CgsCamera.cpp
                // stores literal 0.0f into the three asserted slots, so the non-zero can only
                // come from ShadowMap::CalculateShadowMapCameras' TSM post-multiply
                // (BrnShadowMap.cpp:1241/1249, `mProjection = mProjection * mBestFitMatrix`).
                // This prints the actual magnitudes ONCE so the next wave knows whether it is
                // float noise under the console's (un-dumped) IsSimilar epsilon or a real
                // best-fit-matrix defect.
                if ( CgsDev::Log::gpDebugPrint != 0 )
                {
                    static bool sbLoggedCascadeProj = false;
                    if ( !sbLoggedCascadeProj )
                    {
                        sbLoggedCascadeProj = true;
                        const Matrix44& lrProj = lpCascadeCamera->mProjection;
                        *CgsDev::Log::gpDebugPrint
                            << "[shadow-proj] cascade " << static_cast< s32 >( luCascade )
                            << " projection off-axis terms: [0][2]=" << lrProj.xAxis.z
                            << " [1][2]=" << lrProj.yAxis.z
                            << " [0][3]=" << lrProj.xAxis.w
                            << " | diag [0][0]=" << lrProj.xAxis.x
                            << " [1][1]=" << lrProj.yAxis.y
                            << " [2][2]=" << lrProj.zAxis.z
                            << " [3][3]=" << lrProj.wAxis.w << "\n";
                    }
                }

                CgsGraphics::mShaderConstantTable.SetShaderConstantData(
                    34, lpCascadeCamera->GetViewProjectionMatrixModified() );

                const s32 liCascadeList = ( luCascade >= 2 ) ? static_cast< s32 >( luCascade ) + 2
                                                             : static_cast< s32 >( luCascade );

                // [DIAG shadow-perf] the car and world legs share one destination list, so
                // the split is taken as two deltas around them. This is the measurement
                // that tells a car-only cascade (the fixed cost) apart from a cascade that
                // is actually carrying world casters.
                const s32 liRecordsBefore = static_cast< s32 >(
                    lpDispatchFrame->GetList( liCascadeList )->GetCount() );

                // [DIAG shadow-perf] this cascade's race-car leg. FIXED cost: the module
                // sweeps its own eight active-car slots, so it does not shrink when the
                // cascade's caster set does.
                const ShadowPerfClock::time_point lCascCarStart = ShadowPerfNow();
                if ( lbRaceCars )
                {
                    mRaceCarEntityModule.GenerateDispatchLists(
                        &sRaceCarDispatchInput, sFilteredEntityData.maRaceCarEntityIds,
                        liCascadeList, liCascadeList, liCascadeList, false,
                        Vector4{ 0.0f, 0.0f, 0.0f, 0.0f }, Vector4{ 0.0f, 0.0f, 0.0f, 0.0f },
                        lEye );
                }
                gShadowPerf.mfCascadeCarUs += ShadowPerfUsSince( lCascCarStart );

                const s32 liRecordsAfterCar = static_cast< s32 >(
                    lpDispatchFrame->GetList( liCascadeList )->GetCount() );

                // [DIAG shadow-perf] this cascade's world leg.
                const ShadowPerfClock::time_point lCascWorldStart = ShadowPerfNow();
                if ( lbWorld )
                {
                    mWorldEntityModule.GenerateDispatchLists(
                        &sWorldDispatchInput, sFilteredEntityData.maWorldEntityIds,
                        lpCascadeCamera->GetViewProjectionMatrix(), lEye, lForward,
                        lfLodZoomFactor, &mShaderLodInfo,
                        liCascadeList, liCascadeList, liCascadeList, liCascadeList, true );
                }
                gShadowPerf.mfCascadeWorldUs += ShadowPerfUsSince( lCascWorldStart );

                if ( luCascade < 3u )
                {
                    gShadowPerf.maiCascadeCarRecords[ luCascade ] =
                        liRecordsAfterCar - liRecordsBefore;
                    gShadowPerf.maiCascadeWorldRecords[ luCascade ] =
                        static_cast< s32 >( lpDispatchFrame->GetList( liCascadeList )->GetCount() )
                        - liRecordsAfterCar;
                }

                mShadowMap.SetRenderingShadowMap( false );
            }

            // Unconditional lower, whatever the loop did above (see the latch note).
            mShadowMap.SetRenderingShadowMap( false );

            // Re-publish the MAIN-VIEW camera constants so the table's end-of-frame state
            // is exactly what it was before this arm existed. Not console behaviour (the
            // console leaves the last cascade's constants staged, because its renderer
            // re-publishes per pass); it is a bring-up guard that makes this change
            // provably invisible to every consumer downstream of the producer.
            CgsGraphics::mShaderConstantTable.SetShaderConstantData( 8, lEye );
            CgsGraphics::mShaderConstantTable.SetShaderConstantData( 3, lViewProjection );
            CgsGraphics::mShaderConstantTable.SetShaderConstantData( 34, lViewProjectionModified );
        }
        gShadowPerf.mfCascadeUs += ShadowPerfUsSince( lCascadeStart );
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
                    << ") visibleWorld=" << liMainViewVisibleWorld
                    << " list11=" << static_cast< s32 >(
                           lpDispatchFrame->GetList( KI_WORLD_OPAQUE_LIST )->GetCount() )
                    << " shadowLists=" << static_cast< s32 >( lpDispatchFrame->GetList( 0 )->GetCount() )
                    << "/" << static_cast< s32 >( lpDispatchFrame->GetList( 1 )->GetCount() )
                    << "/" << static_cast< s32 >( lpDispatchFrame->GetList( 4 )->GetCount() )
                    << "\n";
            }

            // ==============================================================
            // ⭐ [DIAG shadow-perf wave 2026-08-12] THE PHASE REPORT.
            //
            // Bank this frame's total and, once the window is full, print the
            // per-frame average of every phase. Value-latched on the accumulated
            // FRAME COUNT (never a `static bool`): the counter only advances on
            // frames the producer actually completed, and the line reprints for
            // the whole session, so it can never read as "never reached".
            //
            // HOW TO READ IT. `frame - total` is everything downstream of this
            // producer; `total` is everything inside it. The 2026-08-12 run put
            // total at 0.9% of the frame, so a frame-time regression that does not
            // move `total` is NOT a producer regression -- check the renderer.
            //
            // The count fields are correctness tripwires, not perf fields:
            //   * cascPadPlanes -> 8 on any cascade: ComputeOptimalViewVolume found no
            //     real supporting plane, so that cascade culls NOTHING and its query
            //     degenerates into a full-octree trivial-accept walk. Measured 0/2/1
            //     (healthy). A partly-padded volume is normal; a fully padded one is a
            //     defect in the fit.
            //   * cascEnt >> mainEnt: the fitted volume ballooned. Measured 206/30/0
            //     against mainEnt 978 -- the three slabs genuinely narrow.
            //   * cascEnt identical across all three: the cascades are sharing one
            //     volume. Note this CANNOT be diagnosed from record counts alone --
            //     both shadow-LOD toggles ship FALSE (BrnShadowMap.cpp:65/66,
            //     sbOptimiseShadowLods / sbOptimiseShadowLodDistances), so
            //     CalcLodDistanceModifier returns 0 for every cascade and the
            //     {50,50,75} table is never read. Identical sets therefore FORCE
            //     identical counts, and identical counts prove identical sets. That
            //     state was real earlier the same day, when CalcVertices was an inert
            //     stub and ComputeBoundingBoxMatrix solved all three fits from the same
            //     uninitialised stack corners; it is gone on this build.
            //   * jobPool at 8192: silent truncation (see the pool banner).
            // Every one of those is owned by BrnShadowMap.cpp, not by this file.
            // ==============================================================
            {
                gShadowPerf.mfTotalUs += ShadowPerfUsSince( lProducerStart );
                ++gShadowPerf.miFrames;

                if ( gShadowPerf.miFrames >= 120 && CgsDev::Log::gpDebugPrint != 0 )
                {
                    const f64 lfInvFrames = 1.0 / static_cast< f64 >( gShadowPerf.miFrames );
                    *CgsDev::Log::gpDebugPrint
                        << "[shadow-perf] armed=" << gShadowPerf.miArmed
                        << " frames=" << gShadowPerf.miFrames
                        << " us/frame: frame=" << static_cast< f32 >( gShadowPerf.mfFramePeriodUs * lfInvFrames )
                        << " total=" << static_cast< f32 >( gShadowPerf.mfTotalUs * lfInvFrames )
                        << " cam=" << static_cast< f32 >( gShadowPerf.mfCamerasUs * lfInvFrames )
                        << " stage=" << static_cast< f32 >( gShadowPerf.mfStageUs * lfInvFrames )
                        << " query=" << static_cast< f32 >( gShadowPerf.mfQueryUs * lfInvFrames )
                        << " mainFilter=" << static_cast< f32 >( gShadowPerf.mfMainFilterUs * lfInvFrames )
                        << " mainWorld=" << static_cast< f32 >( gShadowPerf.mfMainWorldUs * lfInvFrames )
                        << " mainCar=" << static_cast< f32 >( gShadowPerf.mfMainCarUs * lfInvFrames )
                        << " mainProp=" << static_cast< f32 >( gShadowPerf.mfMainPropUs * lfInvFrames )
                        << " mainTraffic=" << static_cast< f32 >( gShadowPerf.mfMainTrafficUs * lfInvFrames )
                        << " propCache=" << static_cast< f32 >( gShadowPerf.mfPropCacheUs * lfInvFrames )
                        << " casc=" << static_cast< f32 >( gShadowPerf.mfCascadeUs * lfInvFrames )
                        << " cascFilter=" << static_cast< f32 >( gShadowPerf.mfCascadeFilterUs * lfInvFrames )
                        << " cascCar=" << static_cast< f32 >( gShadowPerf.mfCascadeCarUs * lfInvFrames )
                        << " cascWorld=" << static_cast< f32 >( gShadowPerf.mfCascadeWorldUs * lfInvFrames )
                        << " envmap=" << static_cast< f32 >( gShadowPerf.mfEnvMapUs * lfInvFrames )
                        << " | envFaces=" << gShadowPerf.miEnvMapFaces
                        << " envEnt=" << gShadowPerf.miEnvMapWorldEnts
                        << " envRec=" << gShadowPerf.miEnvMapRecords
                        << " | mainEnt=" << gShadowPerf.miMainWorldEnts
                        << " mainRec=" << gShadowPerf.miMainWorldRecords
                        << " cascEnt=" << gShadowPerf.maiCascadeWorldEnts[ 0 ]
                        << "/" << gShadowPerf.maiCascadeWorldEnts[ 1 ]
                        << "/" << gShadowPerf.maiCascadeWorldEnts[ 2 ]
                        << " cascWorldRec=" << gShadowPerf.maiCascadeWorldRecords[ 0 ]
                        << "/" << gShadowPerf.maiCascadeWorldRecords[ 1 ]
                        << "/" << gShadowPerf.maiCascadeWorldRecords[ 2 ]
                        << " cascCarRec=" << gShadowPerf.maiCascadeCarRecords[ 0 ]
                        << "/" << gShadowPerf.maiCascadeCarRecords[ 1 ]
                        << "/" << gShadowPerf.maiCascadeCarRecords[ 2 ]
                        << " cascPadPlanes=" << gShadowPerf.maiCascadePadPlanes[ 0 ]
                        << "/" << gShadowPerf.maiCascadePadPlanes[ 1 ]
                        << "/" << gShadowPerf.maiCascadePadPlanes[ 2 ]
                        << " | jobPool=" << gShadowPerf.miPoolResults
                        << " perJob=" << gShadowPerf.maiPoolPerJob[ 0 ]
                        << "/" << gShadowPerf.maiPoolPerJob[ 1 ]
                        << "/" << gShadowPerf.maiPoolPerJob[ 2 ]
                        << "/" << gShadowPerf.maiPoolPerJob[ 3 ]
                        << " cap=" << KI_SHADOW_PERF_POOL_CAPACITY
                        << " batches=" << gShadowPerf.miPoolBatches
                        << ( gShadowPerf.miPoolMaxJob >= KI_SHADOW_PERF_POOL_CAPACITY
                                 ? "  [WARN: a job's result pool is FULL --"
                                   " LooseOctree::PushCoarseResult is dropping results silently,"
                                   " so that job's query results (main/face/cascade) are truncated]"
                                 : "" )
                        // ⚠ THE NEVER-CULLING CASCADE. ComputeOptimalViewVolume pads any
                        // unused plane slot with its sbClearPlanes defaults {N, D=-1e6},
                        // and CgsGeometric::Frustum::SetPlaneByIndex stores the NEGATION of
                        // the plane it is given -- so a padded lane reads back as
                        // (-N, +1e6) and LooseOctree's test `N.C - D > R` evaluates to
                        // -N.C - 1e6, which is inside for every finite point. A cascade
                        // whose fit yields no surviving candidate planes therefore CULLS
                        // NOTHING: its query degenerates into a full-octree
                        // TrivialAcceptRecursive walk that pushes the entire mask-128
                        // population into the shared pool (starving the later cascades) and
                        // its world dispatch leg then iterates that whole population. The
                        // main view is the reference for "how much of the world is even
                        // near the camera", so a cascade seeing far more than it does is
                        // the signature. The fix for it lives in BrnShadowMap.cpp
                        // (ComputeBoundingBoxMatrix / ComputeOptimalViewVolume), not here.
                        << ( ( gShadowPerf.maiCascadePadPlanes[ 0 ] >= 8
                            || gShadowPerf.maiCascadePadPlanes[ 1 ] >= 8
                            || gShadowPerf.maiCascadePadPlanes[ 2 ] >= 8 )
                                 ? "  [WARN: a cascade volume is ALL clear-plane sentinels --"
                                   " ComputeOptimalViewVolume found no real supporting plane,"
                                   " so that cascade culls NOTHING and its query degenerates"
                                   " into a full-octree trivial-accept walk]"
                                 : "" )
                        << ( ( gShadowPerf.maiCascadeWorldEnts[ 0 ] > 2 * gShadowPerf.miMainWorldEnts
                            || gShadowPerf.maiCascadeWorldEnts[ 1 ] > 2 * gShadowPerf.miMainWorldEnts
                            || gShadowPerf.maiCascadeWorldEnts[ 2 ] > 2 * gShadowPerf.miMainWorldEnts )
                                 ? "  [WARN: a cascade returned far more world entities than the"
                                   " main view -- its fitted volume is far too large even if it"
                                   " is not fully padded]"
                                 : "" )
                        << "\n";

                    gShadowPerf.ResetWindow();
                }
            }

            static bool sbLogged = false;
            if ( !sbLogged && CgsDev::Log::gpDebugPrint != 0 )
            {
                sbLogged = true;
                *CgsDev::Log::gpDebugPrint
                    << "[culling] real frustum producer live: centre ("
                    << lCentre.x << ", " << lCentre.y << ", " << lCentre.z
                    << ") radius " << lfRadius << " -- visible world entities "
                    << liMainViewVisibleWorld
                    << ", object list " << static_cast< s32 >( KI_WORLD_OPAQUE_LIST )
                    << " count "
                    << static_cast< s32 >( lpDispatchFrame->GetList( KI_WORLD_OPAQUE_LIST )->GetCount() )
                    << "\n";
            }
        }
    }
}

}
