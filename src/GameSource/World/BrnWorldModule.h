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
namespace CgsMemory { class LinearMalloc; }
struct BrnShaderConstantsFrame;
namespace BrnTraffic { namespace BrnTrafficIO { struct TrafficToRaceCarInterface_PreScene; class InputBuffer_PostPhysics; class InputBuffer_PostScene; class OutputBuffer_PostScene; class InputBuffer_PreScene; class OutputBuffer_PreScene; class OutputBuffer_PostPhysics; } }
namespace BrnWorld { namespace CrashModuleIO { class OutputBuffer_PostScene; } }
namespace BrnWorld { namespace CrashIO { struct InputBuffer_PostPhysics; struct OutputBuffer_PostPhysics; struct InputBuffer_PreScene; struct OutputBuffer_PreScene; } }
namespace BrnPhysics { namespace PhysicsModuleIO { class OutputBuffer; } }

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

        // DWARF :358 (virtual). @0x827D63E8 -- the real per-frame world UPDATE
        // spine (the X360 vtable+76 slot the game module's DoUpdate_World and the
        // loading spine dispatch through): input bridging, the four entity-module
        // phase spines (pre-scene -> post-scene -> pre-physics -> post-physics)
        // interleaved with the scene manager / physics / AI / crash module
        // updates, then the output bridges and the environment-manager /
        // environment-map tail. lpFrameAllocator is the per-frame world linear
        // allocator (FreeAll'd by the caller each frame); the one Malloc carves
        // the frame's triangle-cache collision generator (336896 = generator
        // object 74752 + 0x40000 result buffer).
        virtual void Update( BrnUpdateSet lUpdateSet,
                             CgsModule::IOBufferStack* lpInputBufferStack,
                             CgsModule::IOBufferStack* lpOutputBufferStack,
                             const BrnWorldIO::UpdateInputBuffer* lpUpdateInputBuffer,
                             BrnWorldIO::UpdateOutputBuffer* lpUpdateOutputBuffer,
                             CgsMemory::LinearMalloc* lpFrameAllocator );

        // DWARF :361. @0x827CFDE0 -- the trimmed update the game module runs
        // while the boot-up video plays (updateSet & 0x20).
        void UpdateForBootUpVideo( BrnUpdateSet lUpdateSet,
                                   CgsModule::IOBufferStack* lpInputBufferStack,
                                   CgsModule::IOBufferStack* lpOutputBufferStack,
                                   const BrnWorldIO::UpdateInputBuffer* lpUpdateInputBuffer,
                                   BrnWorldIO::UpdateOutputBuffer* lpUpdateOutputBuffer );

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

        // ---- [FLAG PC bring-up] the world dispatch producer -----------------
        // NOT an X360 function. The console producer is BrnGameModule::DoDispatch
        // @0x823DC458 -> WorldModule::GenerateDispatchLists (above), which needs the
        // director's camera output, the renderer/world dispatch IO buffer set and the
        // scene manager's frustum-test result -- none of which is live on this build
        // (the director module is inert, and SceneManagerModule::UpdateScene /
        // InSceneUpdateInterface::Append are documented gates). This produces the
        // world-opaque dispatch stream directly: it frames a camera on the loaded
        // world, publishes the camera shader constants the dispatch interpreter reads
        // (0 = per-object world transform, 3 = view-projection, 8 = view position) and
        // runs WorldEntityModule::GenerateDispatchListsFromStreamer.
        // DELETE the whole entry when DoDispatch + the frustum query are real.
        void GenerateDispatchListsBringUp( CgsGraphics::DispatchFrame* lpDispatchFrame );

        // [FLAG PC bring-up] Hand the bring-up producer above the REAL director camera for
        // this frame, so it frames the world from the fly-by's viewpoint instead of its own
        // orbiting stand-in and publishes the fly-by's EYE as the PVS query point. The
        // console has no such entry: there the director camera reaches the world through
        // DoDispatch's renderer/world IO buffer set (RendererIO::InputBuffer's camera ->
        // BridgeRendererToWorld -> the real GenerateDispatchLists' `mLastCameraInput =
        // *lpCameraInput`), and none of those four buffers exists on this build.
        // The override lasts ONE dispatch frame: the producer consumes and clears it, so a
        // frame in which the director publishes nothing falls back to the tour camera
        // rather than freezing on a stale viewpoint.
        //
        // lbIsInJunkyard is the SAME camera's BrnDirector::Camera::Camera::IsInJunkyard()
        // (Camera.cpp:564 -- `mState_uFlags & 0x400000`, the bit BrnArbStateCarSelect.cpp:172
        // names KI_CAMERA_STATE_JUNKYARD and sets at :587/:592/:620/:867/:885/:922/:959).
        // ADDED 2026-08-17: it is exactly what the console's GenerateDispatchLists reads at
        // its :3757 latch -- there off `lpDispatchInputBuffer->GetCameraInput()`, i.e. the
        // camera BridgeGameToWorld put in the world dispatch input buffer, which is the same
        // director camera this override carries. It is a SEPARATE value from the transform,
        // and it does NOT expire with the one-frame override: a frame on which the director
        // publishes nothing must not be read as "left the junkyard".
        // DELETE with GenerateDispatchListsBringUp itself.
        void SetBringUpCameraOverride( const rw::math::vpu::Matrix44Affine& lrTransform,
                                       f32 lfFOVDegrees,
                                       bool lbIsInJunkyard );

        // [FLAG PC bring-up] Hand the bring-up producer the renderer's four WORLD-layer
        // effects frames for this frame, so EnvironmentManager::GenerateEffects @0x827BE698
        // has somewhere real to write.
        // STANDS IN FOR: BrnWorldIO::DispatchInputBuffer::SetEffectsFrame @0x823B6BD8, called
        // four times by BrnGameModule::BridgeRendererToWorld @0x823CDD20 (this tree,
        // GameBridgeRendererToX.cpp:50 -- `lpWorldDispatchInput->SetEffectsFrame(luSlot,
        // lpRendererOutput->GetWorldEffectsFrame(luSlot))`), whose values the real
        // GenerateDispatchLists then reads back as GetEffectsFrame(0..3). None of the
        // renderer/world dispatch IO buffers is created on this build, so the four pointers
        // come straight across from BrnRendererModule instead.
        // FOUR slots because kau8SlotsPerEffectsLayer[KU_EFFECTS_LAYER_WORLD] == 4
        // (byte_8203E110 = 01 04 02). Any of them may be null (the renderer's accessor
        // returns null until its arbitrator is Constructed) -- the producer checks.
        // DELETE-WHEN the RendererIO/BrnWorldIO dispatch buffer set is real on PC.
        void SetBringUpEffectsFrames( BrnEffectsFrame* lpFrame0, BrnEffectsFrame* lpFrame1,
                                      BrnEffectsFrame* lpFrame2, BrnEffectsFrame* lpFrame3 );

        // [FLAG PC bring-up] Hand the bring-up producer the DISPATCH-THREAD input buffer
        // so its env-map arm can publish the six per-face rendered flags the renderer's
        // six-face loop reads back (seam S2 of the reflections wave).
        // STANDS IN FOR: BrnWorldIO::DispatchInputBuffer::SetDispatchThreadInputBuffer
        // @0x823B5408, which BrnGameModule::DoDispatch @0x823DC458 calls on the world
        // dispatch INPUT buffer; the real WorldModule::GenerateDispatchLists then reads it
        // back with GetDispatchThreadInputBuffer() (this file's :244 entry, cpp :3725) and
        // writes the flags at cpp:4018. None of that IO buffer set is created on PC, so
        // the pointer comes straight across from BrnGameModule instead -- the same
        // stand-in shape as SetBringUpEffectsFrames above, and staged from the same place
        // (BrnGameModule.cpp, immediately before GenerateDispatchListsBringUp).
        // NOT one-shot: DoDispatch re-stages it every frame and the producer only reads it.
        // DELETE-WHEN the RendererIO/BrnWorldIO dispatch buffer set is real on PC.
        void SetBringUpDispatchThreadInputBuffer( BrnGame::DispatchThreadInputBuffer* lpBuffer );

        // [FLAG PC bring-up] Hand the race-car producer THE CORONA SUBMISSION INTERFACE
        // (SubmitCoronasForRaceCar @0x822D1600 posts its lamp flares through it).
        // STANDS IN FOR: the console's copy chain BrnRendererModule::Update -> RendererIO
        // output -> GameBridgeRendererToX -> BrnWorldIO::DispatchInputBuffer ->
        // RaceCarEntityModuleIO::InputBuffer_GenerateDispatchLists::SetCoronaSubmissionInterface
        // @0x8279EBC8. That IO buffer set does not exist on PC, so the pointer comes straight
        // across from BrnRendererModule (GetCoronaSubmissionInterfaceBringUp) via
        // BrnGameModule::DoDispatch, and GenerateDispatchListsBringUp applies it to the
        // race-car dispatch input EVERY FRAME beside SetShadowMap (the manager's Swap moves
        // which slot is the write slot). DELETE-WHEN the dispatch buffer set is real on PC.
        void SetBringUpCoronaSubmissionInterface( BrnCoronaManager::BrnSubmissionInterface* lpInterface );

        // (PublishWorldShadingConstantsBringUp RETIRED 2026-08-16 -- the real producer
        // SetupShaderConstantsBeforeRendering @0x827D1410 below publishes a superset of its
        // slots. See the retirement note in BrnWorldModule.cpp.)

        // @0x827DADF8 -- stage this frame's frustum-test queries for the scene
        // manager: the main camera frustum, the six environment-map face frusta
        // (alternating halves under the 30Hz env-map policy) and the three shadow
        // cascades, then hand them to the frustum-test job system.
        // SIGNATURE RECONCILED 2026-07-28 (culling wave): six args, with the dispatch
        // OUTPUT buffer and the update set BY POINTER -- BrnGameModule::DoDispatch
        // @0x823DC458 clears bit 7 in place (`updateSet &= ~0x80`) when the game-data
        // output reports live streaming, and both this producer and GenerateDispatchLists
        // read that same word.
        void GenerateFrustumQueries(
            CgsModule::IOBufferStack* lpInputBufferStack,
            CgsModule::IOBufferStack* lpOutputBufferStack,
            const BrnWorldIO::DispatchInputBuffer* lpDispatchInputBuffer,
            BrnWorldIO::DispatchOutputBuffer* lpDispatchOutputBuffer,
            const BrnUpdateSet* lpUpdateSet );

        // @0x827BD1F0 (DWARF :398) -- the per-frame PRE-SCENE entity-module
        // spine: race car pre-scene, the race-car -> traffic staging (+ the
        // environment time-of-day copy into the traffic input), traffic
        // pre-scene, the race-car -> WORLD staging, the WORLD ENTITY pre-scene
        // (the PVS zone query + UpdateStream -- the world streamer's per-frame
        // drive), prop pre-scene, then the traffic -> trigger staging and the
        // trigger pre-scene.
        void EntityModulePreSceneUpdate(
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
            BrnUpdateSet lUpdateSet );

        // @0x827D3F10 (DWARF :407) -- the per-frame POST-PHYSICS entity-module
        // spine: race car, traffic, prop, WORLD ENTITY (the collision-world
        // validate protocol + the streamer's GameData request flush) and crash,
        // each staged from the physics module's output.
        void EntityModulePostPhysicsUpdate(
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

        // RETYPED 2026-07-27 (world-drive wave) against the X360 body @0x827B06E0
        // and its Update call site: the two middle arguments are the physics
        // module's INPUT and OUTPUT buffers (not step/flag counters), and the last
        // is the frame update set -- the body forwards (input, updateSet) to
        // PhysicsModule::UpdateNetworkCatchup.
        void UpdatePhysicsNetworkCatchup(
            CgsModule::IOBufferStack* lpInputBufferStack,
            CgsModule::IOBufferStack* lpOutputBufferStack,
            const BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
            const BrnPhysics::PhysicsModuleIO::OutputBuffer* lpPhysicsModuleOutputBuffer,
            BrnUpdateSet lUpdateSet );

        // @0x827D53B0 -- the 15-stage world prepare chain: base module -> vault +
        // district map -> scene manager (allocators + culling table) -> physics ->
        // environment manager -> race car -> traffic -> WORLD ENTITY (starts the
        // world streaming) -> prop -> trigger -> AI -> crash -> env-map cameras ->
        // debug -> done. Resumable: one stage (or more) per call; false = call again.
        bool Prepare( CgsModule::IOBufferStack* lpInputBufferStack,
                      CgsModule::IOBufferStack* lpOutputBufferStack,
                      BrnWorldIO::UpdateOutputBuffer* lpUpdateOutputBuffer,
                      BrnResource::GameDataIO::AllocatorList* lpAllocatorList );

        // @0x827C9478 -- one frame of the WORLD COLLISION prepare (the scripted load's
        // stage 7, LoadingScriptedState::LoadWorldCollision @0x823E73E0). Drives
        // WorldEntityModule::PrepareWorldCollision through the same buffer trio Prepare's
        // WORLDENTITY stage uses. Resumable: false = call again next frame.
        // Arity + parameter NAMES confirmed against the DecFIGS DWARF
        // (BrnWorldModule.h:349 -- `bool PrepareWorldCollision(IOBufferStack*,
        // IOBufferStack*, UpdateOutputBuffer*)`, body BrnWorldUnity.cpp:11577 naming them
        // lpInputBufferStack / lpOutputBufferStack / lpOutput).
        bool PrepareWorldCollision( CgsModule::IOBufferStack* lpInputBufferStack,
                                    CgsModule::IOBufferStack* lpOutputBufferStack,
                                    BrnWorldIO::UpdateOutputBuffer* lpOutput );

        // ⭐ ADDITIVE 2026-08-09 (feed wave; header-only inline, no out-of-line symbol).
        // WorldModule::BridgeInputToPhysicsModule @0x827AB830 hands the module's last director
        // camera to the physics input buffer, reaching it as the raw `this + 0x5E1CC0`
        // (`addis r4,r29,0x5E / addi r4,r4,0x1CC0` -- 6167744, the console seat this header
        // already documents for mLastCameraInput). Same pattern as
        // Vehicle::VehicleInputInterface::GetLineTestResults: expose it BY NAME so host
        // addressing stays layout-correct instead of poking a console byte offset through a
        // void*, which on the x64 layout would not land on this member at all.
        const BrnDirector::Camera::Camera* GetLastCameraInput() const { return &mLastCameraInput; }

        // ⭐ ADDITIVE 2026-08-10 (pre-physics bridge wave; header-only inlines, no out-of-line
        // symbols) -- EXACTLY the GetLastCameraInput pattern one line above, for exactly the
        // same reason. The DecFIGS DWARF makes BridgeEntityModulesToPhysicsModule_PrePhysics a
        // MEMBER of this class (BrnWorldModule.h:578, four parameters -- which is why the X360
        // passes the WorldModule in r3: it is the implicit `this`), and its driver-controls
        // filter reads these two members directly:
        //     0x827AAFEC  add r11, r11, 1541820 ; slwi 2 ; lwzx r11, r11, r20   -> maeCarControls[id]
        //     0x827AB000  lbzx r11, r20, 0x5E1B10                               -> mbDEBUG... (+6167312)
        // The PC tree models the bridge layer as free functions taking the r3 seat explicitly,
        // so it needs a named door. A friend declaration was tried first and is NOT usable here:
        // naming the global `WorldModule` namespace from this header makes
        // `using BrnWorld::WorldModule;` (BrnGameModule.hpp:63) ambiguous.
        // ⛔ THE ALTERNATIVE IS THE REAL HAZARD: the Bridges layer's existing habit is to poke
        // these members at their CONSOLE byte offsets through the void* (see
        // WorldBridgeRaceCarToWorldModule.cpp's KU_WORLD_MODULE_* constants) -- which cannot be
        // right on an x64 WorldModule layout. Named access is the point of these two lines.
        // AS SHIPPED: the console indexes maeCarControls with the event's raw miVehicleID and
        // range-guards nothing here, so neither does this accessor.
        s32  GetCarControl(s32 liActiveRaceCarIndex) const { return maeCarControls[liActiveRaceCarIndex]; }
        bool IsDEBUGPlayerCarAlwaysUnderAIControl() const  { return mbDEBUGPlayerCarAlwaysUnderAIControl; }

    private:
        // @0x827A52B0 (DWARF BrnWorldModule.h:473 -- `void BridgeRaceCarModuleToWorldModule_
        // PreScene(InputBuffer_PreScene*, const OutputBuffer_PreScene*)`, i.e. a WorldModule
        // METHOD with `this` implicit). Latch the race-car module's pre-scene active-race-car
        // output interface into the world-entity pre-scene input buffer, then publish the
        // player active-race-car index + the per-car rival control markers into THIS object.
        //
        // ⛔ IT IS DECLARED AS A MEMBER ON PURPOSE (2026-08-11). The rest of the WorldBridge*
        // family is modelled as namespace functions taking the X360 r3 as a leading `void*
        // lpWorldModule`; this one is the ONLY bridge that dereferences that pointer, and under
        // the void* model it reached meLocalPlayerActiveRaceCarIndex / maeCarControls through
        // the X360 BYTE OFFSETS -- which are 67,504 bytes wrong on the x64 PC layout, so the
        // publish silently missed and corrupted the embedded sub-module fleet instead. Body +
        // measurements: Bridges/WorldBridgeRaceCarToWorldModule.cpp. Keep it a member so the
        // members are reached by NAME. (A global `namespace WorldModule` cannot be introduced
        // into this header: BrnGameModule.hpp does `using BrnWorld::WorldModule;`.)
        void BridgeRaceCarModuleToWorldModule_PreScene(
            WorldEntityIO::InputBuffer_PreScene* lpWorldInputBuffer_PreScene,
            const RaceCarEntityModuleIO::OutputBuffer_PreScene* lpRaceCarOutputBuffer_PreScene );

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
        // budget (the KA_VEHICLE_*_LOD_DISTANCE policy tables). BODIED in
        // BrnWorldModule.cpp; see the banner there for the asm attestation.
        //
        // ⭐ SIGNATURE CORRECTED 2026-08-12 (vehicle-LOD wave). This used to read
        // `void CalculateVehicleLODs( Vector3 )` -- THREE arguments short, and the
        // body it gated could therefore never be written. The X360 prologue
        // @0x827C3778 settles it: `mr r25,r3` (this), `vmr128 v124,v1` (the camera
        // position -- a VMX register argument), `fmr f28,f1` (the LOD zoom factor),
        // `mr r28,r5` / `mr r24,r6` (the two arrays). r4 is UNTOUCHED: on PPC the
        // float argument RESERVES its GPR slot without using it, which is exactly
        // why the arrays land in r5/r6 and why a register-counting read of this
        // prologue loses lfZoomFactor. The DecFIGS DWARF names all four
        // (BrnWorldModule.h:428 / _compile/BrnWorldUnity.cpp:7973).
        //
        // CONSTNESS: BOTH arrays are non-const references. The DWARF _compile
        // rendering spells them `const&`, but that rendering is lossy -- the DWARF
        // HEADER dump (BrnWorldModule.h:428) spells both non-const, and the asm
        // agrees on both counts:
        //   * the body WRITES the traffic array (`stw r9, 8(r3)` into each
        //     VehicleRenderInfo::mLOD, @0x827C3C34 / @0x827C3B38);
        //   * it reaches the race-car id array through the NON-CONST Array<>::GetItem
        //     overload -- the three `bl` sites @0x827C3848 / @0x827C3AE8 / @0x827C3B5C
        //     all target 0x827BA878, whose assert line is CgsArray.h:556 (non-const).
        //     The const overload is a separate symbol (0x822AFAB8, CgsArray.h:538)
        //     whose only caller is RaceCarEntityModule::GenerateDispatchLists. The
        //     split is already documented in Array_EntityId_32.cpp:11-13.
        void CalculateVehicleLODs(
            Vector3 lCameraPos,
            f32 lfZoomFactor,
            Array<CgsSceneManager::EntityId, 32u>& laRaceCarEntityIDs,
            Array<BrnTraffic::VehicleRenderInfo, 64u>& laTrafficRenderInfos );

        // @0x827D1410 -- stage the frame's global shader constants before any
        // dispatch-list generation. BODIED (post-fx step 9).
        //
        // ⭐ SIGNATURE CORRECTED 2026-08-16 (env-manager go-live wave). This used to read
        // `void SetupShaderConstantsBeforeRendering( BrnShaderConstantsFrame*, f32 lfSimTime,
        // f32 lfGameTime )` -- THREE arguments short, and the two floats in the WRONG ORDER,
        // which is why the body it gated could never be written. Hex-Rays renders the X360
        // function as `int SetupShaderConstantsBeforeRendering()` (zero parameters); the
        // prologue and the single call site settle it.
        //   * @0x827D1410-0x827D151C: `mr r24,r4` (the view-projection), `mr r25,r5` (the
        //     camera transform -- `lvx128 v125, r25, 0x30` later reads its Pos row),
        //     `stfs f1, var_3D0` / `stfs f2, var_3C0`, `mr r26,r8` (the frame),
        //     `mr r22,r9` (the dispatch output buffer). r6 and r7 are NEVER touched: on PPC
        //     a float argument RESERVES its GPR slot without using it, so the two floats are
        //     parameters 4 and 5 and the two pointers land in r8/r9, not r6/r7.
        //   * the caller, WorldModule::GenerateDispatchLists @0x827D1CE8, loads them at
        //     @0x827D2098-0x827D20AC: r4 = a local Matrix44 copied out of gDispatchCamera
        //     +0x80..0xB0 (== GetViewProjectionMatrix), r5 = the director Camera* (whose
        //     mTransform is at +0x00, i.e. the Camera* IS the Matrix44Affine&),
        //     f1 = DispatchInputBuffer::GetGameTime(), f2 = GetSimTime() (`fmr f2,f31`),
        //     r8 = GetShaderConstantsFrame(), r9 = the output buffer it LockForWrite's.
        //   * DecFIGS DWARF names all six: GameSource/World/BrnWorldModule.h:677 and
        //     _compile/BrnWorldUnity.cpp:7634 (source BrnWorldModule.cpp:2436).
        //
        // The CALLER owns both locks: every frame setter the console emitted out-of-line
        // asserts `true == mbLockedForWriting`, and GenerateDispatchLists brackets the call
        // with the output buffer's IOBuffer::LockForWrite / UnlockForWrite.
        void SetupShaderConstantsBeforeRendering(
            const rw::math::vpu::Matrix44& lCameraViewProjection,
            const rw::math::vpu::Matrix44Affine& lCameraTransform,
            f32 lfGameTime, f32 lfSimTime,
            BrnShaderConstantsFrame* lpOutputShaderConstants,
            BrnWorldIO::DispatchOutputBuffer* lpOutputBuffer );

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
        // The active player car's world position, latched by Update's tail for
        // the environment-map / dispatch consumers (X360 stvx128 @ +6175680,
        // right after the shadow map -- WorldModule::Update @0x827D63E8).
        Vector4                    mPlayerCarPosition; // (+6175680)
        // The last director camera the dispatch pass consumed (X360 +6167744;
        // GenerateDispatchLists copies the frame camera into it each dispatch).
        // [FLAG PC bring-up] the one-frame director-camera override (see
        // SetBringUpCameraOverride). DELETE with GenerateDispatchListsBringUp.
        rw::math::vpu::Matrix44Affine mBringUpCameraOverride;
        f32                           mfBringUpCameraOverrideFOV;
        bool                          mbBringUpCameraOverrideValid;
        // [FLAG PC bring-up] the director camera's junkyard state bit, staged by the same
        // setter. STANDS IN FOR the console's `lpDispatchInputBuffer->GetCameraInput()->
        // IsInJunkyard()` at GenerateDispatchLists @0x827D1CE8 (cpp :3757; asm reads
        // camera+320 == mState_uFlags and tests & 0x400000). Unlike the transform it is
        // LEVEL, not one-shot -- it keeps its last staged value across frames on which the
        // director publishes nothing, because the console's camera input is never absent.
        // DELETE with GenerateDispatchListsBringUp.
        bool                          mbBringUpCameraInJunkyardBringUp;
        // [FLAG PC bring-up] the four world-layer effects frames staged by
        // SetBringUpEffectsFrames (see the header entry). DELETE with it.
        BrnEffectsFrame*              mapBringUpEffectsFrames[ 4 ];
        // [FLAG PC bring-up] the dispatch-thread input buffer staged by
        // SetBringUpDispatchThreadInputBuffer (see the header entry). DELETE with it.
        BrnGame::DispatchThreadInputBuffer* mpBringUpDispatchThreadInputBuffer;
        // [FLAG PC bring-up] the corona submission interface staged by
        // SetBringUpCoronaSubmissionInterface (see the header entry). DELETE with it.
        BrnCoronaManager::BrnSubmissionInterface* mpBringUpCoronaSubmissionInterface;
        // [FLAG PC bring-up] THE ENV-MAP ARM'S HONEST GATE. Raised by WorldModule::Update
        // @0x827D63E8 at the exact line that drives the console's own env-map camera
        // refresh (`mEnvironmentMap.Update( lpPlayerState->mTransform.Pos() )`, cpp :3046),
        // i.e. the first frame on which a PLAYER CAR is active. Until then
        // EnvironmentMap::Construct/Prepare have left all six face cameras on the identity
        // view -- all six would render the same view from world origin -- so producing
        // face lists before this point is six pointless world dispatches, not a
        // reconstruction of anything. The console needs no such flag because its gate is
        // RenderSwitches.mbRenderEnvironmentMap off the dispatch INPUT buffer, which this
        // producer has not got (see SetBringUpDispatchThreadInputBuffer). DELETE with
        // GenerateDispatchListsBringUp.
        bool                          mbEnvMapCamerasPositionedBringUp;

        BrnDirector::Camera::Camera mLastCameraInput;

        // DWARF :336 (X360 +6175760): the world shader-LOD policy block --
        // GenerateDispatchLists refreshes it (the broadcast-splat of
        // mfShaderLod1NearDistance) and hands it to the world/prop dispatch feeds.
        ShaderLodInfo mShaderLodInfo;

        bool mbIsInJunkyard;                           // :339 (X360 +6175808)
    };
}
