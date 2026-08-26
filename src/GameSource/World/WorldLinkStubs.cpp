// ===========================================================================
// WorldLinkStubs.cpp -- FLAG (world-fleet link-mount stubs, 2026-07-26).
// (<cstring> is included below for the per-gate memsets that stand in for the
//  unrecovered IO-buffer Construct bodies -- see the WORLD-DRIVE BOOT GATES note.)
//
// Minimal out-of-line definitions so the game exe LINKS with the world-module
// fleet TUs mounted (BrnWorldModule + entity modules + scene manager + shadow/
// environment + physics/AI IO surface). Since the world-module MOUNT (2026-07-26)
// BrnGameModule embeds the REAL BrnWorld::WorldModule and wires the real
// Construct(BrnCpuMonitors&) -- so the Construct-path stubs ARE reached at boot
// (before the first rendered frame, where CGS_ASSERT logs + continues); the
// Prepare/Update/Release/dispatch surface is still never driven. Every stub is
// either a CGS_ASSERT(false) trap (side-effectful call) or an inert-return
// getter.
//
// NEVER add behaviour here -- reconstruct the real body from X360 and then
// delete the stub (the two definitions must not coexist in one build).
//
// Notable stub families and why their real TUs are not linked:
//   - WorldModule::Bridge* (traffic/racecar/prop cross-bridges): the owning TU
//     Bridges/WorldBridgeEntityModulesToEntityModules.cpp does not compile
//     standalone (its body references WorldEntityIO::InputBuffer_PreScene::
//     ActiveRaceCarInterfaceStorage, which the committed BrnWorldEntityModuleIO.h
//     does not declare yet). Crash bridges: declared in
//     WorldBridgeCrashToEntityModules.h with no committed body anywhere.
//   - BrnMassive / Attrib runtime: linking the committed TUs pulls the Massive
//     SDK / AttribSys allocator closure (net-negative: more new unresolved than
//     fixed) -- stubbed at the game-facing seam instead.
//   - SceneManager submodules (SpatialPartition / OverlapGeneration /
//     FineIntersection / TriangleCollision): their committed TUs drag the
//     rw::collision query + loose-octree closure (incl. unreconstructed data
//     tables, e.g. rw::collision::gapFindBestSeparatingDirection) -- stubbed at
//     the module Construct/Prepare/Update seam instead.
//   - Module-fleet Construct/Update/Release seams (PropEntityModule,
//     TrafficEntityModule, RaceCarEntityModule, TriggerEntityModule, AIModule,
//     PhysicsModule, ShadowMap, EnvironmentManager, CgsGraphics::Camera,
//     BrnDirector::Camera sub-objects): bodies not reconstructed yet.
//
// The include preamble mirrors BrnWorldModule.cpp (IO headers before the module
// headers) so the nested IO buffer types resolve exactly as they do for the
// referencing TUs -- this also keeps the class/struct keys (and therefore the
// MSVC manglings) identical to the references being satisfied.
// ===========================================================================

#include <cstring>                                                   // memset (unrecovered-Construct boot gates)
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"      // ShaderConstantTable (mShaderConstantTable definition below)
#include "GameSource/Graphics/BrnShaderConstantsFrame.h"
#include "GameShared/GameClasses/Module/CgsModuleUtils.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO_OutputBuffer.h"
#include "GameSource/Physics/BrnPhysicsModuleIO.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysSharedIO.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultArray.h"    // VaultArray Register/UnregisterVault stubs
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultSlot.h"     // VaultSlot::DoLoad stub
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attribsys.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/vechashmap.h" // CollectionHashMap (RemoveIndex gap stub)
#include "GameSource/World/EnvironmentManager/BrnEnvironmentManager.h" // the three env sub-object Constructs below
#include "SDKs/Realmc/RealmcMemcardInterface.h" // MemcardInterface base ctor/dtor (trivial real bodies)
#include "GameShared/GameClasses/Graphics/Resources/CgsShaderTechniqueResourceType.h" // the two documented deferrals below       // Attrib::Database stubs
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribloadandgo.h" // Attrib::Vault / IGarbageCollector stubs
#include "SDKs/EA/GameTalk/GameTalk.h"                                          // GameTalkMessage accessor stubs
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsSpatialPartitionManager.h"
#include "GameSource/World/Bridges/WorldBridgeEntityModulesToOutput.h"
#include "GameSource/World/Bridges/WorldBridgeToEntityModules.h"
#include "GameSource/World/Bridges/WorldBridgeSceneToEntityModules.h"
#include "GameSource/World/Bridges/WorldBridgeCrashToEntityModules.h"
#include "GameSource/World/Bridges/WorldBridgeEntityModulesToEntityModules.h"
#include "GameSource/World/Bridges/WorldBridgeEntityModulesToScene.h"
#include "GameSource/World/Bridges/WorldBridgeInputToEntityModules.h"
#include "GameSource/World/Bridges/WorldBridgeInputToAI.h"
#include "GameSource/World/Bridges/WorldBridgeEntityModulesToAI.h"
#include "GameSource/World/Bridges/WorldBridgeEntityModulesToCrash.h"
#include "GameSource/World/Bridges/WorldBridgeEntityModulesToPhysics.h"
#include "GameSource/World/Bridges/WorldBridgeAIToEntityModules.h"
#include "GameSource/World/Bridges/WorldBridgePhysicsToEntityModules.h"
#include "GameSource/World/Bridges/WorldBridgePhysicsToScene.h"
#include "GameSource/World/Bridges/WorldBridgeSceneToPhysics.h"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameSource/World/BrnWorldModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint / gxMessageFilterFlags (the boot-gate one-shot logs)
#include "GameSource/World/BrnWorldModuleIO_DispatchInputBuffer.h"
#include "GameSource/World/BrnWorldModuleIO_DispatchOutputBuffer.h"
#include "GameSource/World/BrnBaseStreamer.h"
#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModule.h"
#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModuleIO.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule.h"

#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityDebugComponent.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/Trigger/BrnTriggerEntityModule.h"
#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerEntityModuleIO.h"
#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerEntityModuleDebugComponent.h"
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO.h"
#include "GameSource/World/DebugComponents/BrnWorldDebugComponent.h"
#include "GameSource/World/DebugComponents/BrnPVSDebugComponent.h"
#include "GameSource/World/AI/BrnAIModule.h"
#include "GameSource/Physics/BrnPhysicsModule.h"
#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h"
#include "GameShared/GameClasses/Physics/Deformation/BrnWheelPhysicalStates.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModule.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityManager.h"
#include "GameShared/GameClasses/SceneManager/FineIntersectionTestModule/CgsFineIntersectionTestModule.h"
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManager.h"
#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionManager.h"
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapGenerationModule.h"
#include "GameShared/GameClasses/Graphics/CgsCamera.h"
#include "GameShared/GameClasses/Graphics/CgsModel.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsFrustum.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"
#include "GameSource/Director/Camera/Camera.h"
#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"
#include "GameSource/Game/BrnDispatchThreadInputBuffer.h"
#include "GameSource/World/ShadowMap/BrnShadowMap.h"
#include "GameSource/World/EnvironmentManager/BrnEnvironmentManager.h"
#include "GameSource/World/EnvironmentMap/BrnEnvironmentMap.h"
#include "GameSource/World/EnvironmentSettings/BrnEnvironmentSettings.h"
#include "GameSource/World/EnvironmentSettings/BrnEnvironmentKeyframe.h"
#include "GameSource/World/EnvironmentSettings/BrnEnvCloudsData.h"
#include "GameSource/World/EnvironmentSettings/BrnEnvLightingData.h"
#include "GameSource/World/EnvironmentSettings/BrnEnvScatteringData.h"
#include "GameSource/Sound/Module/SharedIO/BrnSoundRootSharedIO.h"
#include "GameSource/Massive/BrnMassive.h"
#include "GameSource/Replays/BrnReplayModuleIO.h"
#include "vendor/renderware/collision/BitTable.hpp"
#include "vendor/renderware/collision/VolumeQuery.hpp"
// Prop-spawn wave (2026-08-12) link-closure gates -- see the block at the FOOT of this file.
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropCellManager.h"        // PropCellManager contact-gen gates
#include "GameSource/Replays/Serialisers/BrnReplayPropSerialiserFrame.h"                // PropSerialiserFrame delta-serialisation gates

// ---------------------------------------------------------------------------
// rw::collision::Volume -- same documented platform/SDK forward-declaration
// exception as CgsSceneManagerModule.cpp: there is no shared rw::collision::
// Volume header; the one entry point referenced (static InitializeVTable) is
// declared minimally so the link stub below can carry the exact mangling.
// ---------------------------------------------------------------------------
namespace rw { namespace collision {
    class Volume
    {
    public:
        static int InitializeVTable();
    };
} }

// ---------------------------------------------------------------------------
// CgsGraphics::mShaderConstantTable -- the global shader-constant table the
// world dispatch path writes through (referenced by BrnWorldModule /
// BrnWorldEntityModule / BrnShadowMap). Zero-initialised storage only.
// LINK STUB (world-fleet mount 2026-07-26): value not reconstructed yet.
// ---------------------------------------------------------------------------
namespace CgsGraphics
{
    ::ShaderConstantTable mShaderConstantTable;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.  (inert:
// runs in the static-initializer of mShaderConstantTable above, BEFORE main --
// must NOT assert; members stay default/garbage until the real ctor lands.)
// (ShaderConstantTable::ShaderConstantTable is REAL now, in its home TU
//  GameShared/GameClasses/Graphics/CgsShaderConstantTable.cpp -- the empty stub here
//  left mu8NumUsedConstants at 0, so no shader constant could ever be set.)

// ---------------------------------------------------------------------------
// ⭐⭐ STUB RETIRED 2026-08-12 (prop-render wave): rw::math::vpu::Inverse
// @ X360 0x825B2628 IS BODIED, in its canonical RenderWare vendor home
//   vendor/renderware/src/rw/math/vpu/Matrix44Operation.cpp
//   (declared in vendor/renderware/include/rw/math/vpu/matrix44_operation.h --
//    the hand-maintained sibling of types.h / matrix44affine_operation.h; the
//    header generator only writes rwcore_*.h, so rw/math/vpu/ is not generated).
// It is the GENERAL 4x4 cofactor inverse (no affine fast path) plus the
// broadcast determinant out-param, exactly as the X360 VMX body computes it.
//
// The stub here returned a value-initialised (all-zero) Matrix44, so every
// caller got a zero matrix: ShadowMap::ComputeBoundingBoxMatrix then produced a
// NaN view-projection, which is what drove the 57 Inverse-stub asserts plus the
// 228 RwMath::IsSimilar(m_projectionMatrix ...) asserts per boot.
// ---------------------------------------------------------------------------










// LINK STUB (AttribSysModule mount 2026-07-26): body not reconstructed yet.
// (Declared inline here -- its home TU attriblivelink.cpp is not reconstructed;
// CgsAttribSysModule.cpp forward-declares the same signature.)
namespace Attrib { void DecodeLiveLinkMessage(const char*); }





// LINK STUB (AttribSysModule mount 2026-07-26): bodies not reconstructed yet --
// the GameTalk message accessors the (unregistered on PC) Attribulator LiveLink
// handler reads.
const char* EA::GameTalk::GameTalkMessage::GetChannel() const
{
    CGS_ASSERT(false, "GameTalkMessage::GetChannel: link stub (attribsys module mount) -- reconstruct from X360");
    return 0;
}

// LINK STUB (AttribSysModule mount 2026-07-26): body not reconstructed yet.
const char* EA::GameTalk::GameTalkMessage::GetKeyContent(const char*) const
{
    CGS_ASSERT(false, "GameTalkMessage::GetKeyContent: link stub (attribsys module mount) -- reconstruct from X360");
    return 0;
}



// -------------------------------------------------------------------------
// BrnAI::AIModule
// -------------------------------------------------------------------------
// GATE RETIRED 2026-08-25 (aimodule wave). BrnAI::AIModule::Construct @0x82794D08 now lives in
// GameSource/World/AI/BrnAIModule.cpp. The gate's own text ("the member stays inert/unprepared
// (zero-initialised static storage), which the boot path tolerates") was true, and that was the
// whole problem: AIModule did not even DERIVE from CgsModule::ModuleSingleBuffered, so there was
// no base lifecycle for the call to reach, Module::mbIsNewModule was never set, and the module's
// prepare/release stage machines, its resource receiver queue and the RouteMapModule base were
// never initialised. See the banner in BrnAIModule.cpp for what the real body does and, just as
// importantly, what it still parks.

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
void BrnAI::AIModule::Destruct()
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "BrnAI::AIModule::Destruct: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// ⭐⭐⭐ GATE RETIRED 2026-08-25 (aimodule wave) -- THIS WAS THE REASON A CRASHED CAR COULD NOT
// BE PUT BACK ON THE ROAD. The real body @0x82798070 now lives in
// GameSource/World/AI/BrnAIModule.cpp together with its stage-2 helper LoadMapData @0x82795340.
// With the gate in place AI.dat was never loaded, "WorldMapData" was never acquired, the AI road
// network did not exist in this build, and BrnAI::ResetOnTrackManager -- an embedded member of
// AIModule whose ONLY constructor call site in the whole image is that Prepare's stage 3 -- was
// never Constructed. Read BrnAIModule.cpp's banner for the parked remainder (the AI cars, the AI
// drivers and stage 4): the module is NOT whole yet.

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
bool BrnAI::AIModule::Release()
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "BrnAI::AIModule::Release: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
    return false;
}

// -------------------------------------------------------------------------
// BrnAI::AIModuleIO::OutputBuffer
// -------------------------------------------------------------------------
// GATES RETIRED 2026-08-25 (aimodule wave). The four const getters
// (GetAICarOutputInterfaceConst / GetAIResourceRequestInterface / GetGameEventQueueConst /
// GetRouteResponseQueue) returned NULL here, which is why WorldModule::BridgeAIModuleToOutput
// carries four null guards around its four transfers. They now have real bodies in
// GameSource/World/AI/SharedIO/BrnAIModuleIO_OutputBuffer.cpp, returning the addresses of REAL
// TYPED MEMBERS -- read that file's header banner: until this wave the buffer declared no members
// at all, so every one of its eleven accessors handed out `this + <X360 offset>` (up to
// this+110448) out of a ONE-BYTE host allocation.

// -------------------------------------------------------------------------
// BrnDirector::Camera -- DESTUBBED (2026-07-26 wave): Camera::Clear @0x8223CE70,
// CopyToCgsCamera @0x8220AC48 and the GetPosition/GetDirection/IsInJunkyard/
// GetLodZoomFactor frame reads now live in GameSource/Director/Camera/Camera.cpp;
// CameraEffects::Construct / DepthOfField::Construct (the inlined default-init
// store runs those bring-up paths share) live in BrnCameraEffects.cpp /
// BrnDepthOfField.cpp; CameraState::Clear @0x82220950 lives in BrnCameraState.cpp.
//
// CameraState::Construct @0x82252348 DESTUBBED (DRIVE wave 2026-07-26): the real
// body (zero the three flag sets -> ValidityAccount::SetupFailFlagMask -> Clear)
// now lives in BrnCameraState.cpp, with SetupFailFlagMask @0x82221118 in
// BrnCameraValidityAccount.cpp -- the world-prepare path reaches CameraState::
// Clear through WorldModule::Prepare's mLastCameraInput.Clear(), which asserts
// the mask was set up.

// -------------------------------------------------------------------------
// BrnDirector::HookNameStringWrapper
// -------------------------------------------------------------------------
// DESTUBBED (camera-family wave 2026-08-01). The real body is now a header inline
// in BrnDirectorEffectTrigger.h -- HookNameStringWrapper::Set @0x821F15B8, whose own
// asserts cite BrnDirectorEffectTrigger.h lines 0x36/0x37, so the console defines it
// in the header too.
//
// Worth recording why this mattered: the gate below was REACHED EVERY FRAME once
// WorldModule::Update @0x827D63E8 went live, and it silently did nothing -- so every
// effect-hook name the camera set was dropped on the floor, with a one-shot log line
// as the only trace. Camera::EnsureEffectIsPlaying (@0x821F2720, landed this wave)
// re-requests through exactly this call, so the stub had to go for it to work at all.

// -------------------------------------------------------------------------
// BrnGame::DispatchThreadInputBuffer
// -------------------------------------------------------------------------
// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
void BrnGame::DispatchThreadInputBuffer::SetCameraViewProjection(struct rw::math::vpu::Matrix44 const &)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "BrnGame::DispatchThreadInputBuffer::SetCameraViewProjection: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// TOMBSTONE (reflections step 1, 2026-08-17): the
// BrnGame::DispatchThreadInputBuffer::SetEnvMapFaceRender boot gate is DELETED (it was
// spelt ...FaceRendered until the step-2 DWARF rename). The
// real body now lives in its own TU, GameSource/Game/BrnDispatchThreadInputBuffer.cpp,
// which is on tools/build/build_game_exe.bat (line 2257) -- leaving this gate beside it
// is LNK2005. Do NOT re-add it.

// -------------------------------------------------------------------------
// BrnGraphics::EnvironmentMap
// -------------------------------------------------------------------------
// TOMBSTONE (reflections step 1, 2026-08-17): the BrnGraphics::EnvironmentMap::Prepare
// boot gate is DELETED. The real X360 body (@0x827B4188) now lives in its own TU,
// GameSource/World/EnvironmentMap/BrnEnvironmentMap.cpp, which is on
// tools/build/build_game_exe.bat (line 351) -- leaving this gate beside it is LNK2005.
// Do NOT re-add it.

// -------------------------------------------------------------------------
// BrnMassive::BrnMassive
// -------------------------------------------------------------------------
// BOOT-GATE (world-module mount 2026-07-26): REACHED at boot via the real
// WorldEntityModule::Construct @0x82302398 (mMassive.Construct()); quiet
// no-op returning 0 -- no subscriber is created, the MassiveAd client is
// never touched. Reconstruct from X360 before wiring the ad pipeline.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
int BrnMassive::BrnMassive::Construct()
{
    return 0;
}

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
int BrnMassive::BrnMassive::Destruct()
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "BrnMassive::BrnMassive::Destruct: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
    return 0;
}

// -------------------------------------------------------------------------
// MassiveAdClient3::CMassiveAdObjectSubscriber (SDK base; body lives in the
// MassiveAd client package, which is not linked). Link-required since the
// world-module mount: BrnMassiveSubscriber's default ctor/dtor exist so the
// by-value pool (BrnMassive::maSubscribers[15], inside WorldEntityModule,
// inside the mounted WorldModule) is instantiable.
// -------------------------------------------------------------------------
// LINK STUB (world-module mount 2026-07-26): MUST be a quiet no-op, NOT a trap --
// it runs at normal process exit through the static gGameModule destructor chain
// (~WorldModule -> ~WorldEntityModule -> ~BrnMassive -> 15x ~BrnMassiveSubscriber
// -> this base dtor); an assert during CRT static destruction is not survivable.
// No subscriber is ever placement-constructed on the boot path, so there is
// nothing to tear down.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
MassiveAdClient3::CMassiveAdObjectSubscriber::~CMassiveAdObjectSubscriber()
{
}

// LINK STUB (world-module mount 2026-07-26): pulled by the scalar deleting dtor in
// the emitted vtable; never invoked (pool slots are by-value members, never
// heap-deleted).
// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log -- reached on the per-frame world drive, where a trap stops the
// simulation. The body is still NOT reconstructed; the fix is the real X360 body
// in its own TU, not this gate.
void MassiveAdClient3::CMassiveAdObjectSubscriber::operator delete(void *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "MassiveAdClient3::CMassiveAdObjectSubscriber::operator delete: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// -------------------------------------------------------------------------
// BrnPhysics::PhysicsModule
// -------------------------------------------------------------------------
// ⭐⭐ PhysicsModule::Construct STUB RETIRED 2026-08-03 (task #123). THE REAL BODY IS LIVE in
// GameSource/Physics/BrnPhysicsModule.cpp (X360 @0x825AE308), which is mounted.
//
// It had been a no-op here since the 2026-07-26 world-module mount. Task #116 proved the blocker
// was never the link -- the closure of all ten callees has been green since 54e1868d -- but the
// class layout: BrnPhysicsModule.h ended at +0x636A0 (407,200) while the body writes as far as
// +433208, and its fabricated 112-byte `ContainedListInterface mContainedList` sat exactly on
// mPropManager's seat (+0x63630), so bodying it then would have overrun the object by ~26 KB AND
// sliced a real PropManager into 112 bytes.
//
// Task #123 re-seated the header instead: the five formerly-opaque sub-objects are real typed
// members, the trailing state/perf-monitor block is modelled from the DWARF, and the console
// arithmetic behind it is gated by the MOUNTED BrnPhysicsModule_layout_check.cpp. Two numbers in
// the old note here were also wrong and are corrected in that header's banner: there are TWENTY-
// SEVEN perf-monitor members (21 is only how many Construct registers), and +433208 is a `stbx`,
// so the console object is 433,209 bytes raw, not "at least 433,212".

// ⭐⭐ RETIRED 2026-08-04 (task #135): BrnPhysics::PhysicsModule::Prepare IS BODIED, in
// GameSource/Physics/BrnPhysicsModule.cpp. It was the stub that kept the entire rw::physics
// solver unreachable -- its stage 3 is the only path to PhysicsSimulationModule::Prepare, which
// is the only assignment to mpSimulation in the tree. The three stubs below are what is LEFT of
// it: one named symbol per sibling subsystem whose own prepare pass is still unreconstructed,
// instead of one silent `return true` for the whole module.
// GATE RETIRED 2026-08-18 (wave Q4 PropManager mount): BrnPhysics::Props::PropManager::Prepare @0x8260EE18 is REAL in PropManager_wQ2_06.cpp.

// ⭐⭐ RETIRED 2026-08-10 (producer wave): BrnPhysics::Vehicle::VehicleManager::Prepare
// @0x8263C688 IS BODIED, in GameSource/Physics/VehicleManager/BrnVehicleManager_Prepare.cpp.
// It was the stub that kept every car out of the triangle cache: its stage-2 arm,
// PrepareTriangleCache @0x82615BA0, is the ONLY filler of the scene input's mAddToCacheQueue on
// the race-car path, and that queue is the only setter of TriangleCacheManager::mUsedCacheSlots.
// The stub below is what is LEFT of it -- its stage-1 arm.
//
// LINK STUB (producer wave, 2026-08-10): X360 @0x82633568 (161 insns). Called from
// VehicleManager::Prepare's case 0/1 arm.
//
// ⛔ WHAT IS DROPPED, stated plainly rather than hidden behind `return true`: the per-car DATA
// build -- 8x VehiclePhysics::Construct @0x8262DBD0 (the only one of its four callees that
// exists in this tree), then 8x { VehicleDriver::Prepare @0x825B8680, VehiclePhysics::Construct,
// Vehicle::DebugComponent::Construct @0x82602F68 }, PhysicalTrafficManager::Prepare @0x8262CA48,
// VehicleDriver::Prepare on the traffic driver, and ~30 scalar seeds.
// ⛔ WHY IT IS NOT RECONSTRUCTED HERE: (a) ~470 further instructions across four functions, three
// of them absent, i.e. a wave of its own; and (b) Hex-Rays degenerates the body into `_R28`/`_R31`
// inline asm with every store at a raw console byte offset PAST mPhysicalTrafficManager -- past
// the +224 host drift BrnVehicleManager.h documents -- so writing it from the pseudocode would be
// the offset hack the project forbids. It must be re-derived from the raw asm with every member
// reached by name.
// ⭐ WHY THE FSM ABOVE IS STILL LANDABLE WITHOUT IT: the console body has NO failure path -- it
// returns the constant 1 -- so `return true` here is the console's own control flow, not a
// convenient one. What is genuinely absent is vehicle DATA, not the stage transition.
// ⚠️ CONSEQUENCE TO EXPECT: the 8 race-car cache slots are claimed with no VehiclePhysics behind
// them. That is harmless today (ProcessAddToCacheEvents stamps a radius and a used bit and does
// not touch mbIsDirty, and StartUpdateTriangleCaches skips every non-dirty slot), and it becomes
// load-bearing the moment UpdateCachedPositions lands.
// GATE RETIRED 2026-08-22 (traffic wave T3): VehicleManager::PrepareData @0x82633568 is REAL in
// GameSource/Physics/VehicleManager/BrnVehicleManager_PrepareData.cpp; it is the only caller of
// PhysicalTrafficManager::Prepare @0x8262CA48, the sole seater of the three physics traffic arrays.

// ⭐⭐ LINK STUB DELETED 2026-08-14 (deformation-mount wave): DeformationManager::Prepare
// @0x82630230's REAL body (BrnDeformationManager.cpp) is MOUNTED as of this wave -- the stub that
// stood here ("DeformationManager::Prepare: inert [FLAG PC boot gate]") would have collided
// (LNK2005) or, worse, been picked silently. PhysicsModule::Prepare stage 4
// (E_PREPARESTAGE_DEFORMATIONMANAGER) now reaches the real pool carve + per-model ClearVariables,
// and its fourteen deferred deformation-IO clears go live with it. If a stub for it reappears
// here the link will say so (LNK2005).

// GATE RETIRED 2026-08-19 (wave Q5 round-3 integration): PhysicsModule::PropPrepareTypes @0x825A14A8
// is REAL in BrnPhysicsModule.cpp -- it hands the prop-physics data handle to the physics PropManager
// (ProcessInputs_Prepare @0x825E3400, PropManager_wQ5_01.cpp).

// (PhysicsModule::UpdateNetworkCatchup(int,int) stub RETIRED 2026-07-27: the
// signature was a decompiler misread -- the world drive passes the physics INPUT
// buffer + the frame update set. The retyped gate lives in the world-drive block
// at the end of this file.)

// -------------------------------------------------------------------------
// BrnPhysics::Props::PropInputInterface
// -------------------------------------------------------------------------
// (PropInputInterface::Append gate RETIRED 2026-08-10, root-cause wave: the real
//  body @0x827A9CA8 now lives in its own home TU
//  GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.cpp, alongside the
//  Construct/Clear pair the console inlines into PhysicsModuleIO::InputBuffer::Construct.
//  ⚠️ The stub's parameter was a REFERENCE; the DWARF and the PS3 mangle both say
//  pointer -- corrected with the body.)

// GATE RETIRED 2026-08-18 (breakable-props wave Q mount): BrnReplays::ReplayIO::RequestInterface::Append @0x823A6868 is REAL in GameSource/Replays/BrnReplayRequestInterface.cpp (mounted with RegisterSerialiser, which PropEntityModule::PostPhysicsUpdate now calls).

// -------------------------------------------------------------------------
// BrnSound::Module::Io::SoundWorldLoadEvent
// -------------------------------------------------------------------------
// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
void BrnSound::Module::Io::SoundWorldLoadEvent::Construct(enum BrnSound::Module::Io::SoundWorldLoadEvent::eLoadEvent,unsigned short)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "BrnSound::Module::Io::SoundWorldLoadEvent::Construct: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// -------------------------------------------------------------------------
// BrnTraffic::BrnTrafficIO::InputBuffer_Dispatch
// -------------------------------------------------------------------------
// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
class CgsModule::VariableEventQueue<32768,16> * BrnTraffic::BrnTrafficIO::InputBuffer_Dispatch::GetSceneResultQueue()
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "BrnTraffic::BrnTrafficIO::InputBuffer_Dispatch::GetSceneResultQueue: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
    return 0;
}

// -------------------------------------------------------------------------
// BrnTraffic::BrnTrafficIO::InputBuffer_PreDispatch / OutputBuffer_PreDispatch
// -------------------------------------------------------------------------
// ⛔⛔ FOUR GATES RETIRED: `InputBuffer_PreDispatch::Construct` (@0x8275CEE8),
//     `::SetCameraPosition`, `::SetVisibleEntities` and `OutputBuffer_PreDispatch::
//     Construct` (@0x8275CF28) are REAL in BrnTrafficEntityModuleIO.cpp.
//     ⚠️ Behaviour change on a live path, toward the console: the gate Constructs
//     memset the buffer and never called CgsModule::IOBuffer::Construct, leaving
//     eStatusConstructed CLEAR; the real ones open with `stb 1,0(r3)`. The two setters
//     silently dropped WorldModule::GenerateDispatchLists' stores and now store.

// -------------------------------------------------------------------------
// BrnTraffic::BrnTrafficIO::OutputBuffer_Prepare
// -------------------------------------------------------------------------
// ⛔⛔ TWO GATES RETIRED 2026-08-10 (pre-physics bridge wave), AND THEY WERE
//     SILENT-DROP STUBS, NOT PLACEHOLDERS.
//     `OutputBuffer_Prepare::GetResourceRequestInterface() const` @0x8279FA30 and
//     `::GetSceneInputInterface() const` @0x8279F988 have had REAL committed bodies in
//     GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.cpp all
//     along -- that TU had simply never been on the build list, so the copies that LINKED
//     were these two, each `return 0`. Their own banner said "REACHED every frame", so every
//     caller has been handed a NULL interface and has been silently doing nothing with it.
//     Found by MOUNTING the real TU (for the pre-physics OutputBuffer this wave): the link
//     immediately produced LNK2005 on both, which is the whole point of mounting a
//     re-parented TU even when the wave does not call it.
//     ⚠️ This is a real behaviour change on a live path -- both accessors now return a valid
//     pointer where they returned NULL. Gates re-run and clean; recorded here so a later
//     regression is attributed correctly.

// -------------------------------------------------------------------------
// BrnTraffic::TrafficEntityModule
// -------------------------------------------------------------------------
// ⛔⛔ GATE RETIRED: `TrafficEntityModule::Construct` @0x82740220 is REAL (PARTIAL,
//     named in-body gates) in BrnTrafficEntityModule_wT1_01.cpp. It Constructs
//     mReceiverQueue and mStreamer, closing the never-Constructed-queue park on the
//     streamer's three base queues.

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
void BrnTraffic::TrafficEntityModule::Destruct()
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "BrnTraffic::TrafficEntityModule::Destruct: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// ⛔⛔ GATE RETIRED: `TrafficEntityModule::EnterTearingDownState` @0x82708168 is REAL
//     (complete) in BrnTrafficEntityModule_wT1_01.cpp.

// ⛔⛔ GATE RETIRED: BrnTraffic::TrafficEntityModule::GenerateDispatchLists @0x8273B280 is
//     REAL in EntityModules/TrafficEntityModule/BrnTrafficEntityModule_Render.cpp, with the
//     DWARF's 10-argument signature (arg 2 is the Array<VehicleRenderInfo,64> inside the
//     output buffer, plus four vector arguments) -- see the call site in BrnWorldModule.cpp.
//     Documented PARTIAL: the corona pass (SubmitCoronasForVehicle /
//     RenderTrafficLightCoronas) is a NAMED in-body gate.

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
void BrnTraffic::TrafficEntityModule::PostSceneUpdate(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,class BrnTraffic::BrnTrafficIO::InputBuffer_PostScene *,struct BrnTraffic::BrnTrafficIO::OutputBuffer_PostScene *,unsigned short)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "BrnTraffic::TrafficEntityModule::PostSceneUpdate: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// ⛔⛔ GATE RETIRED: BrnTraffic::TrafficEntityModule::PreDispatchUpdate @0x8274D900 is REAL
//     in EntityModules/TrafficEntityModule/BrnTrafficEntityModule_Render.cpp, documented
//     PARTIAL (one named gate, G7: the TRAILER liveness arm, which needs
//     Vehicle::GetCabIndex, undeclared anywhere in the tree). @0x8274D900 is an EXPORT HOLE
//     (nearest exported neighbours 0x8274C870 and 0x8274E508), so there is no pseudocode or
//     asm and DecFIGS DWARF :19315 is the highest rung available; the body is written
//     against its scope/callee tree. Its input parameter is const, per the DWARF.

// GATE RETIRED 2026-08-19 (wave Q7, cluster traffic): BrnTraffic::TrafficEntityModule::PrePhysicsUpdate @0x8274C690
// is REAL (documented PARTIAL: all real legs + 8 named one-shot gates inside the body) in
// EntityModules/TrafficEntityModule/BrnTrafficEntityModule_wQ7_01.cpp (mounted). HandlePropModuleRequests
// @0x82720A90 -- the traffic-light knockdown/restore consumer -- is REAL in the same partfile.

// GATE RETIRED 2026-08-19 (wave Q7, cluster traffic): BrnTraffic::TrafficEntityModule::Prepare @0x8274A578 is
// REAL (documented PARTIAL: stages 0/2/5 real incl. LoadData @0x82746A88 resource stages 0/1 that SEAT mpData;
// stages 1/3/4 named gates) in EntityModules/TrafficEntityModule/BrnTrafficEntityModule_wQ7_02.cpp (mounted).

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
bool BrnTraffic::TrafficEntityModule::Release()
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "BrnTraffic::TrafficEntityModule::Release: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
    return false;
}

// -------------------------------------------------------------------------
// BrnWorld::EnvironmentSettings
// -------------------------------------------------------------------------
// (ParseEnvironmentFile and CloudsData::SetToBlend RETIRED 2026-08-16, post-fx
//  step 9 "environment live" / envfix round. Both are real bodies now and their
//  TUs are on tools/build/build_game_exe.bat:
//    ParseEnvironmentFile @0x8267CD70   -> GameSource/World/EnvironmentSettings/
//                                          BrnEnvironmentSettings.cpp
//    CloudsData::SetToBlend @0x82675FC0 -> GameSource/World/EnvironmentSettings/
//                                          BrnEnvCloudsData.cpp
//  Leaving either gate here alongside the mounted TU is LNK2005.)

// -------------------------------------------------------------------------
// BrnWorld::EnvironmentSettings::DebugComponent
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::EnvironmentSettings::DebugComponent::Construct(class BrnWorld::EnvironmentSettings::EnvironmentManager *)
{
    // BOOT-GATE (attribsys wave 2026-07-26): REACHED by WorldModule::Prepare's
    // env-manager stage (mSkyDebugComponent.Construct @0x827C7668 -- no ida export;
    // body when the EnvironmentSettings debug TU lands). Quiet one-shot log; the
    // component stays unregistered-inert.
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "EnvironmentSettings::DebugComponent::Construct: inert [FLAG PC boot gate]\n";
    }
}

// The five vtable-pulled virtuals below became link-required when the game module
// mounted the REAL WorldModule (2026-07-26): mSkyDebugComponent is embedded by
// value, so BrnGameModule.obj emits the vtable. None are reached at boot (the
// component is never Construct()ed/registered -- see the Construct trap above).

// LINK STUB (world-module mount 2026-07-26): body not reconstructed yet (X360 @0x827C7760).
// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log -- reached on the per-frame world drive, where a trap stops the
// simulation. The body is still NOT reconstructed; the fix is the real X360 body
// in its own TU, not this gate.
void BrnWorld::EnvironmentSettings::DebugComponent::Update()
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "BrnWorld::EnvironmentSettings::DebugComponent::Update: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// LINK STUB (world-module mount 2026-07-26): body not reconstructed yet (X360 @0x827C79A0).
// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log -- reached on the per-frame world drive, where a trap stops the
// simulation. The body is still NOT reconstructed; the fix is the real X360 body
// in its own TU, not this gate.
void BrnWorld::EnvironmentSettings::DebugComponent::RenderHUD(struct CgsDev::Debug2DImmediateRender *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "BrnWorld::EnvironmentSettings::DebugComponent::RenderHUD: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// LINK STUB (world-module mount 2026-07-26): body not reconstructed yet (X360 @0x827B23E8).  (inert getter)
const char * BrnWorld::EnvironmentSettings::DebugComponent::GetName() const
{
    return "DebugComponent::GetName link stub";
}

// LINK STUB (world-module mount 2026-07-26): body not reconstructed yet.  (inert getter)
const char * BrnWorld::EnvironmentSettings::DebugComponent::GetPath() const
{
    return "DebugComponent::GetPath link stub";
}

// LINK STUB (world-module mount 2026-07-26): body not reconstructed yet (X360 @0x827B2408).
// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log -- reached on the per-frame world drive, where a trap stops the
// simulation. The body is still NOT reconstructed; the fix is the real X360 body
// in its own TU, not this gate.
void BrnWorld::EnvironmentSettings::DebugComponent::OnActivate()
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "BrnWorld::EnvironmentSettings::DebugComponent::OnActivate: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// TOMBSTONE (bloom wave 2026-08-15): the EnvironmentManager::GenerateEffects boot gate that
// stood here is DELETED. The real X360 body (@0x827BE698) is homed in
// GameSource/World/EnvironmentManager/BrnEnvironmentManager.cpp, which is mounted at
// tools/build/build_game_exe.bat:219 -- keeping this gate as well would be LNK2005.

// TOMBSTONE (envstream wave 2026-08-16): the EnvironmentManager::Prepare boot gate that stood
// here is DELETED. The real X360 body (@0x827D49A8) is homed in
// GameSource/World/EnvironmentManager/BrnEnvironmentManager.cpp, which is mounted at
// tools/build/build_game_exe.bat:224 -- keeping this gate as well would be LNK2005.

// -------------------------------------------------------------------------
// BrnWorld::EnvironmentSettings::Keyframe
// -------------------------------------------------------------------------
// (Keyframe::Construct, LightingData::SetToBlend and ScatteringData::SetToBlend
//  RETIRED 2026-08-16, post-fx step 9 "environment live" / envfix round. All three
//  are real bodies now, in mounted TUs:
//    Keyframe::Construct @0x82676298        -> GameSource/World/EnvironmentSettings/
//                                              BrnEnvironmentKeyframe.cpp   (NEW TU)
//    LightingData::SetToBlend @0x827AFAA8   -> .../BrnEnvLightingData.cpp
//    ScatteringData::SetToBlend @0x827AF468 -> .../BrnEnvScatteringData.cpp
//  These three were the reason PerformBlend was two thirds inert: with them gated,
//  mScattering / mLighting / mClouds were NEVER written, so a live
//  GenerateShaderConstants would have read an all-zero lighting block and the
//  world would have gone dark. Leaving a gate beside a mounted TU is LNK2005.)

// -------------------------------------------------------------------------
// BrnWorld::InternalBaseStreamer
// -------------------------------------------------------------------------
// (ALL FIVE stubs -- AddEntry / ClearTargetList / Construct / IsStreamComplete /
//  Update -- RETIRED 2026-07-27. The whole streamer engine is now reconstructed
//  in its DWARF home GameSource/World/BrnBaseStreamer.cpp from the X360 bodies
//  (Construct 0x827C4A60, AddEntry 0x827C4B58, ClearTargetList 0x827B0B50,
//  IsStreamComplete 0x827B0BE8, Update 0x827D5F50 + the Idle/Loading/Unloading
//  legs and the potential-list/attempt helpers).)

// -------------------------------------------------------------------------
// BrnWorld::PVSDebugComponent
// (world-module mount 2026-07-26: linking the committed BrnPVSDebugComponent.cpp
// was tried and REVERTED -- it is PARTIAL (RenderPVS / RenderPvsCentrePosition
// declared, bodies unrecovered) and drags Debug2DImmediateRender::DrawCircle;
// net-negative closure, so the vtable seam stays stubbed here. RenderHUD /
// OnActivate below joined Construct when the mounted by-value fleet made
// BrnGameModule.obj emit the vtable.)
// -------------------------------------------------------------------------
// BOOT-GATE (world-module mount 2026-07-26): REACHED at boot via the real
// WorldEntityModule::Construct @0x82302398; quiet no-op -- the component is
// never registered, so the stubbed RenderHUD/OnActivate below never run.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
void BrnWorld::PVSDebugComponent::Construct(class BrnWorld::WorldEntityModule *)
{
}

// LINK STUB (world-module mount 2026-07-26): committed body not linkable yet (X360 @0x827CEAD8).
// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log -- reached on the per-frame world drive, where a trap stops the
// simulation. The body is still NOT reconstructed; the fix is the real X360 body
// in its own TU, not this gate.
void BrnWorld::PVSDebugComponent::RenderHUD(struct CgsDev::Debug2DImmediateRender *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "BrnWorld::PVSDebugComponent::RenderHUD: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// LINK STUB (world-module mount 2026-07-26): body not reconstructed yet (X360 @0x827B2178).
// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log -- reached on the per-frame world drive, where a trap stops the
// simulation. The body is still NOT reconstructed; the fix is the real X360 body
// in its own TU, not this gate.
void BrnWorld::PVSDebugComponent::OnActivate()
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "BrnWorld::PVSDebugComponent::OnActivate: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// GATE RETIRED 2026-08-12 (prop-spawn wave): BrnWorld::PropEntityModule::CachePropGraphicsLists @0x822DBF28 is now REAL.

// The body lives in this module's own TU under PropEntityModule/ (see the

// 'PROP SPAWN WAVE' block in tools/build/build_game_exe.bat). Leaving this inert

// definition here would be a duplicate symbol at link.

// GATE RETIRED 2026-08-12 (prop-spawn wave): BrnWorld::PropEntityModule::Construct @0x822FA068 is now REAL.

// The body lives in this module's own TU under PropEntityModule/ (see the

// 'PROP SPAWN WAVE' block in tools/build/build_game_exe.bat). Leaving this inert

// definition here would be a duplicate symbol at link.

// GATE RETIRED 2026-08-12 (prop-spawn wave): BrnWorld::PropEntityModule::ConstructPostPhysicsPerfMonitors @0x822A9218 is now REAL.

// The body lives in this module's own TU under PropEntityModule/ (see the

// 'PROP SPAWN WAVE' block in tools/build/build_game_exe.bat). Leaving this inert

// definition here would be a duplicate symbol at link.

// GATE RETIRED 2026-08-12 (prop-spawn wave): BrnWorld::PropEntityModule::ConstructPreScenePerfMonitors @0x822A90A0 is now REAL.

// The body lives in this module's own TU under PropEntityModule/ (see the

// 'PROP SPAWN WAVE' block in tools/build/build_game_exe.bat). Leaving this inert

// definition here would be a duplicate symbol at link.

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
void BrnWorld::PropEntityModule::Destruct()
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "BrnWorld::PropEntityModule::Destruct: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// GATE RETIRED 2026-08-12 (prop-spawn wave): BrnWorld::PropEntityModule::GenerateDispatchLists @0x822FB4F0 is now REAL.

// The body lives in this module's own TU under PropEntityModule/ (see the

// 'PROP SPAWN WAVE' block in tools/build/build_game_exe.bat). Leaving this inert

// definition here would be a duplicate symbol at link.
// GATE RETIRED 2026-08-18 (breakable-props wave Q mount): BrnWorld::PropEntityModule::PostSceneUpdate @0x822C4718 (PropEntityModule_wQ2_03.cpp) is now REAL.
// GATE RETIRED 2026-08-18 (breakable-props wave Q mount): BrnWorld::PropEntityModule::PrePhysicsUpdate @0x82303048 (PropEntityModule_wQ_07.cpp) is now REAL.

// GATE RETIRED 2026-08-12 (prop-spawn wave): BrnWorld::PropEntityModule::Prepare @0x82306DB8 is now REAL.

// The body lives in this module's own TU under PropEntityModule/ (see the

// 'PROP SPAWN WAVE' block in tools/build/build_game_exe.bat). Leaving this inert

// definition here would be a duplicate symbol at link.

// GATE RETIRED 2026-08-12 (prop-spawn wave): BrnWorld::PropEntityModule::Release @0x822A92F8 is now REAL.

// The body lives in this module's own TU under PropEntityModule/ (see the

// 'PROP SPAWN WAVE' block in tools/build/build_game_exe.bat). Leaving this inert

// definition here would be a duplicate symbol at link.

// -------------------------------------------------------------------------
// BrnWorld::RaceCarEntityModule
// -------------------------------------------------------------------------
// BOOT-GATE (world-module mount 2026-07-26): REACHED at boot by the wired
// WorldModule::Construct @0x827CF540 fleet cascade; quiet no-op (see
// AIModule::Construct above). Reconstruct from X360 before wiring Prepare.
// RETIRED (global-resource wave 2026-07-31): RaceCarEntityModule::Construct is now bodied
// in BrnRaceCarEntityModule.cpp -- it brings up mReceiverQueue (without which every
// GameData reply addressed to the module is dropped on the floor) and seeds the two stage
// machines. The rest of the module interior is still opaque and stays unconstructed there.

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
void BrnWorld::RaceCarEntityModule::Destruct()
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "BrnWorld::RaceCarEntityModule::Destruct: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// (RaceCarEntityModule::GenerateDispatchLists gate RETIRED 2026-07-31: the real X360
//  body @0x822E79F8 lands in BrnRaceCarEntityModule_Render.cpp.)


// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
bool BrnWorld::RaceCarEntityModule::IsPlayerCarTailgatingOtherRaceCars(enum EActiveRaceCarIndex,class BrnWorld::ActiveRaceCar const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "BrnWorld::RaceCarEntityModule::IsPlayerCarTailgatingOtherRaceCars: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
    return false;
}

// GATE RETIRED 2026-08-25 (crash exit): BrnWorld::RaceCarEntityModule::PostSceneUpdate @0x822FE3F0
// is REAL in GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule_CrashExit.cpp
// -- as a MINIMAL-COMPLETE SLICE. THIS WAS THE NINTH GATE, and it was on the CONSUMER side: six of
// its eight console helpers have no body anywhere in this tree, so no wave had tried it, and while
// it stayed a stub the crash module's RaceCarCrashCompleteEvent ring was read by nobody.
// The slice runs the lock pair + ProcessRaceCarCrashCompleteEvents and logs the six absentees once.

// RETIRED (drivable wave 2026-08-01): RaceCarEntityModule::PrePhysicsUpdate is the real
// X360 0x82307160 slice in BrnRaceCarEntityModule.cpp now. It was a SILENT-DROP stub of
// exactly the class the brief calls the top defect class: it swallowed both buffers, and
// with them mPlaceOnTrackManager::PrePhysicsUpdate -- the only caller of
// ResetActiveRaceCar, the only writer of E_STATE_ACTIVE in the XEX. Nothing could ever
// have become an active race car while this body existed.
//
// ⚠️ ITS SIBLING AT :1358 (PostSceneUpdate @0x822FE3F0) IS STILL A STUB and still drops
// the line-test REQUEST half of the place-on-track round trip. Left deliberately: the
// four other links in that chain are stubs too (see
// PlaceOnTrackManager::ApplyPendingRequestsWithoutSceneQueryBringUp for the five-item
// list), so bodying this one alone would move nothing.

// RETIRED (global-resource wave 2026-07-31): RaceCarEntityModule::Prepare is now the real
// X360 0x82303E78 body in BrnRaceCarEntityModule.cpp -- it takes the OutputBuffer_Prepare
// the console signature always had, and its stage 2 runs the real LoadGlobalResources
// (CarColours acquire + "Vehicles/VEHICLETEX.BIN" into pool 25 + the vehicle/wheel list
// GETs). The old one-argument stub that reported success is gone.

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
bool BrnWorld::RaceCarEntityModule::Release()
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "BrnWorld::RaceCarEntityModule::Release: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
    return false;
}

// -------------------------------------------------------------------------
// BrnWorld::RaceCarEntityModuleIO::InputBuffer_GenerateDispatchLists
// -------------------------------------------------------------------------

// -------------------------------------------------------------------------
// BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics
// -------------------------------------------------------------------------

// -------------------------------------------------------------------------
// BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface
// -------------------------------------------------------------------------
// (RCEntityActiveRaceCarOutputInterface::GetRaceCarState / IsPlayerCarActive /
//  IsRaceCarActive stubs RETIRED 2026-07-27 -- real bodies now in the interface's
//  own TU BrnRCEntityActiveRaceCarOutputInterface.cpp, recovered from X360
//  0x82277B90 / 0x82277B10 (+ the ICF-folded const GetRaceCarState). The world
//  loading drive calls IsPlayerCarActive every frame from
//  WorldEntityModule::PreSceneUpdate, so the trap blocked the PVS query.)



// -------------------------------------------------------------------------
// BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface
// -------------------------------------------------------------------------
// GetPlayerGlobalRaceCarIndex DESTUBBED (intro wave 2026-07-29): the real body lives
// in the interface's own TU BrnRCEntityGlobalRaceCarOutputInterface.cpp. The gate here
// returned (EGlobalRaceCarIndex)0 -- i.e. "the player is global car 0" -- where the
// X360 returns mePlayerGlobalRaceCarIndex, which Clear() @0x822B4088 seeds to
// E_GLOBAL_RACE_CAR_INDEX_INVALID(-1). (The gate's "REACHED every frame" note was not
// borne out: the one-shot log never appears in build/game/BrnGame.log, so nothing on
// the current PC boot path calls it -- BridgeWorldVehicleDataToGui, its only X360
// caller, is not reconstructed.)

// -------------------------------------------------------------------------
// BrnWorld::ShadowMap -- DESTUBBED (2026-07-26 wave): Construct @0x827B43E8,
// SetConstantsForEnvmap @0x827C1AD0 and the inlined accessor set live in
// GameSource/World/ShadowMap/BrnShadowMap.cpp.
// DESTUBBED (shadow-camera wave 2026-07-27): CalculateShadowMapCameras
// @0x827DA820 is now REAL in BrnShadowMap.cpp (director-camera overload; the
// CGS-camera shape the committed WorldModule call site uses is a documented
// trap bridge there until that call site passes the director camera input),
// together with SetConstants @0x827C16E0 + ObjectCSMSelect @0x827C1630.
// ComputeBoundingBoxMatrix/ComputeOptimalViewVolume/DebugRender carry FLAG
// assert-trap bodies in the same TU (off the default ORTHO path) -- see
// BrnShadowMap.h for the per-function notes.
// -------------------------------------------------------------------------
// BrnWorld::TriggerEntityModule
// -------------------------------------------------------------------------
// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
void BrnWorld::TriggerEntityModule::PostSceneUpdate(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,class BrnWorld::TriggerEntityModuleIO::InputBuffer_PostScene *,class BrnWorld::TriggerEntityModuleIO::OutputBuffer_PostScene *,unsigned short)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "BrnWorld::TriggerEntityModule::PostSceneUpdate: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
void BrnWorld::TriggerEntityModule::PrePhysicsUpdate(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,class BrnWorld::TriggerEntityModuleIO::InputBuffer_PrePhysics *,class BrnWorld::TriggerEntityModuleIO::OutputBuffer_PrePhysics *,unsigned short)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "BrnWorld::TriggerEntityModule::PrePhysicsUpdate: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// -------------------------------------------------------------------------
// BrnWorld::TriggerEntityModuleDebugComponent
// (world-module mount 2026-07-26: linking the committed
// BrnTriggerEntityModuleDebugComponent.cpp was tried and REVERTED -- its
// RenderWorld drags Debug3DImmediateRender::DrawBox/DrawSphere + the
// rw::RGBA ctor, none linked; net-negative closure, so the vtable seam stays
// stubbed here. RenderWorld/RenderHUD/GetName/OnActivate below joined
// Construct when the mounted by-value fleet made BrnGameModule.obj emit the
// vtable.)
// -------------------------------------------------------------------------
// BOOT-GATE (world-module mount 2026-07-26): REACHED at boot via the real
// TriggerEntityModule::Construct; quiet no-op -- the component is never
// registered, so the stubbed render surface below never runs.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
void BrnWorld::TriggerEntityModuleDebugComponent::Construct(class BrnWorld::TriggerEntityModule *)
{
}

// LINK STUB (world-module mount 2026-07-26): committed body not linkable yet (X360 @0x822DA1F0).
// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log -- reached on the per-frame world drive, where a trap stops the
// simulation. The body is still NOT reconstructed; the fix is the real X360 body
// in its own TU, not this gate.
void BrnWorld::TriggerEntityModuleDebugComponent::RenderWorld(struct CgsDev::Debug3DImmediateRender *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "BrnWorld::TriggerEntityModuleDebugComponent::RenderWorld: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// LINK STUB (world-module mount 2026-07-26): committed body not linkable yet (X360 @0x822C4368).
// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log -- reached on the per-frame world drive, where a trap stops the
// simulation. The body is still NOT reconstructed; the fix is the real X360 body
// in its own TU, not this gate.
void BrnWorld::TriggerEntityModuleDebugComponent::RenderHUD(struct CgsDev::Debug2DImmediateRender *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "BrnWorld::TriggerEntityModuleDebugComponent::RenderHUD: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// LINK STUB (world-module mount 2026-07-26): committed body not linkable yet (X360 @0x822A8FF8).  (inert getter)
const char * BrnWorld::TriggerEntityModuleDebugComponent::GetName() const
{
    return "TriggerEntityModuleDebugComponent::GetName link stub";
}

// LINK STUB (world-module mount 2026-07-26): committed body not linkable yet (X360 @0x822A9018).
// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log -- reached on the per-frame world drive, where a trap stops the
// simulation. The body is still NOT reconstructed; the fix is the real X360 body
// in its own TU, not this gate.
void BrnWorld::TriggerEntityModuleDebugComponent::OnActivate()
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "BrnWorld::TriggerEntityModuleDebugComponent::OnActivate: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// -------------------------------------------------------------------------
// BrnWorld::WorldDebugComponent
// -------------------------------------------------------------------------
// BOOT-GATE (world-module mount 2026-07-26): REACHED at boot -- the tail of the
// wired WorldModule::Construct is mDebugComponent.Construct(this). Quiet no-op:
// the component is never registered with the debug manager, so its (real,
// linked) GetName and the stubbed debug surface are never invoked. Reconstruct
// from X360 (registration + member wiring) before wiring the world debug UI.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
void BrnWorld::WorldDebugComponent::Construct(class BrnWorld::WorldModule *)
{
}

// -------------------------------------------------------------------------
// BrnWorld::WorldEntityIO::InputBuffer_GenerateDispatchLists
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
// (BrnWorld::WorldEntityIO::InputBuffer_GenerateDispatchLists::GetSceneResultQueue
// stub RETIRED 2026-07-27: the queue member is pinned by Construct @0x822D8BC8
// (VariableEventQueue<32768,16>::Construct(this+8), shadow-map pointer at
// this+0x8018 == 8 + 32784), so the accessor is a real inline in its own header.)

// (BrnWorld::WorldEntityIO::OutputBuffer_Prepare::GetSceneInputInterface stub
// RETIRED 2026-07-26: the real accessor now lives in its owning TU,
// BrnWorldEntityModuleIO_OutputBuffer_Prepare.cpp.)

// -------------------------------------------------------------------------
// BrnWorld::WorldEntityIO::StatusInterface
// -------------------------------------------------------------------------
// (The five StatusInterface setter stubs RETIRED 2026-07-27: all twelve
// StatusInterface methods are X360 HEADER-INLINES -- none has an out-of-line
// symbol in the ARTIST export set -- so their real bodies now live in
// BrnWorldEntityStatusInterface.h next to the flags they store. Keeping them as
// asserting stubs would have trapped the world streamer's per-frame status
// publish.)

// -------------------------------------------------------------------------
// BrnWorld::WorldEntityModule
// -------------------------------------------------------------------------
// BOOT GATE (world-IO wave 2026-07-27): the dispatch-list producer's Massive
// impression feed. Same uncommitted third-party SDK as UpdateMassive above;
// one-shot log + inert rather than a trap so the dispatch pass keeps running.
void BrnWorld::WorldEntityModule::GenerateMassiveImpressionData(struct CgsGraphics::Instance *,struct rw::math::vpu::Vector3 const &)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldEntityModule::GenerateMassiveImpressionData: inert (Massive SDK uncommitted) [FLAG PC boot gate]\n";
    }
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnWorld::WorldEntityModule::PrepareMassive(struct BrnWorld::WorldEntityIO::OutputBuffer_Prepare *)
{
    // BOOT-GATE (attribsys wave 2026-07-26): REACHED by the world Prepare stage
    // chain. One-shot log + report success so the scripted load advances toward
    // WORLDENTITY; the module stays inert (zero-initialised storage) and its
    // deeper consumers keep their traps. Reconstruct from X360.
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldEntityModule::PrepareMassive: inert [FLAG PC boot gate]\n";
    }
    return true;
}

// BOOT GATE (world-IO wave 2026-07-27): REACHED every frame by
// WorldEntityModule::PreSceneUpdate @0x82302A08 now that the world drive runs.
// The Massive in-game-advertising SDK (BrnMassive / CMassiveAdObjectSubscriber)
// is an UNCOMMITTED third-party subsystem -- its Prepare is already the inert
// gate above -- so the per-frame impression update has nothing to drive.
// One-shot log + inert (a trap here would block the sim on frame 1).
void BrnWorld::WorldEntityModule::UpdateMassive(unsigned short)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldEntityModule::UpdateMassive: inert (Massive SDK uncommitted) [FLAG PC boot gate]\n";
    }
}

// -------------------------------------------------------------------------
// BrnWorld::WorldModule
// -------------------------------------------------------------------------
// GATE RETIRED 2026-08-19 (wave Q6 / rdisp): WorldModule::BridgeWorldModuleToEntityModules_Render is REAL in Bridges/WorldBridgeToEntityModules.cpp -- the entity modules (traffic / race car / world entity / prop) now receive their Dispatch input every frame; prop coronas and the prop dispatch input are seeded.

// (WorldModule::CalculateVehicleLODs stub RETIRED 2026-08-12, vehicle-LOD wave:
//  the real body @0x827C3778 now lives in BrnWorldModule.cpp. It was the ONLY
//  per-frame writer of ActiveRaceCar::RenderParams::mLOD, so while it was inert
//  every race car rendered at the E_STATE_LOD_4 that RenderParams::Reset seeds and
//  2-/3-state body parts failed DoesStateExist(4) and did not render at all. The
//  stub also carried the wrong signature -- one argument instead of four.)

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
// (WorldModule::FilterFrustumTestResults stub RETIRED 2026-07-28, culling wave:
//  the real body @0x827BDA60 now lives in BrnWorldModule.cpp beside its two
//  callers. It is the split of one coarse-query result record into the four
//  per-owner id arrays every module's GenerateDispatchLists consumes.)

// (WorldModule::SetupShaderConstantsBeforeRendering boot gate RETIRED 2026-08-16,
//  env-manager go-live wave: the real X360 body @0x827D1410 now lives in
//  BrnWorldModule.cpp beside its caller, and the signature it was stubbing was wrong --
//  three arguments short, with the two float parameters transposed. See the SIGNATURE
//  CORRECTED note on the declaration in BrnWorldModule.h.)

// -------------------------------------------------------------------------
// BrnWorldIO::DispatchInputBuffer / DispatchOutputBuffer -- DESTUBBED
// (2026-07-26 wave): GetRenderSwitches now lives in
// BrnWorldModuleIO_DispatchInputBuffer.cpp and GetFogColourPlusWhiteLevel in
// BrnWorldModuleIO_DispatchOutputBuffer.cpp (both the read-lock accessor shape
// their attested siblings share).
// -------------------------------------------------------------------------

// -------------------------------------------------------------------------
// CgsAttribSys::AttribSysIO::AttribSysRequestInterface<2048>
// -------------------------------------------------------------------------
// DESTUBBED (DRIVE wave 2026-07-26): the real <2048>::RegisterVault (generic body
// in CgsAttribSysSharedIOImpl.h, instantiated by CgsAttribSysModuleIO.cpp) now
// links -- CgsAttribSysModuleIO.cpp joined the exe build list.

// -------------------------------------------------------------------------
// CgsDev::DebugInterface
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
class CgsDev::DebugRender & CgsDev::DebugInterface::GetRender()
{
    CGS_ASSERT(false, "DebugInterface::GetRender: link stub (world fleet mount) -- reconstruct from X360");
    static CgsDev::DebugRender* sNull = 0; return *sNull;
}

// BOOT-GATE (world-module mount 2026-07-26): REACHED at boot by the real
// WorldEntityModule::Construct / ShadowMap::Construct dev-menu variable
// registrations (the DebugInterface automatic-handle idiom). Quiet no-op: the
// tuning rows simply do not exist until the DebugInterface TU is reconstructed.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
void CgsDev::DebugInterface::RegisterVariable(int *,char const *,char const *)
{
}

// BOOT-GATE (world-module mount 2026-07-26): see RegisterVariable(int*) above.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
void CgsDev::DebugInterface::RegisterVariable(float *,char const *,char const *)
{
}

// BOOT-GATE (world-module mount 2026-07-26): see RegisterVariable(int*) above.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
void CgsDev::DebugInterface::RegisterVariable(bool *,char const *,char const *)
{
}

// BOOT-GATE (world-module mount 2026-07-26): see RegisterVariable(int*) above.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
void CgsDev::DebugInterface::SetRange(int *,int,int)
{
}

// BOOT-GATE (world-module mount 2026-07-26): see RegisterVariable(int*) above.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
void CgsDev::DebugInterface::SetStep(int *,int)
{
}

// BOOT-GATE (world-module mount 2026-07-26, referenced by ShadowMap::Construct
// @0x827B43E8): see RegisterVariable(int*) above.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
void CgsDev::DebugInterface::SetRange(float *,float,float)
{
}

// BOOT-GATE (world-module mount 2026-07-26, referenced by ShadowMap::Construct
// @0x827B43E8): see RegisterVariable(int*) above.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
void CgsDev::DebugInterface::SetStep(float *,float)
{
}

// BOOT-GATE (world-module mount 2026-07-26, referenced by ShadowMap::Construct
// @0x827B43E8): see RegisterVariable(int*) above.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
void CgsDev::DebugInterface::SetOptions(int *,struct CgsDev::DebugUI::StringList const *)
{
}

// -------------------------------------------------------------------------
// CgsDev::DebugRender
// -------------------------------------------------------------------------
// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
void CgsDev::DebugRender::DrawCircle(struct rw::math::vpu::Vector3,struct rw::math::vpu::Vector3,float,unsigned int)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "CgsDev::DebugRender::DrawCircle: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// -------------------------------------------------------------------------
// LINK STUBS (update-transcribe wave 2026-08-02): the two 3D debug primitives
// BehaviourGameplayExternal::Update's debug-render arm draws (X360 @0x822414C0 /
// @0x82241524). NEITHER HAS A RECONSTRUCTED BODY ANYWHERE -- the buffered
// DebugRender in this tree models only the 2D event queue, and these queue into
// the 3D one.
// ⭐ THEY ARE NOT ON THE SHIPPING PATH: the arm that calls them is gated on
// BehaviourGameplayExternal::mbEnableDebugRender, which nothing on this build
// ever raises, so these must not fire. They log ONCE if they ever do -- an
// unannounced no-op is exactly the failure this tree keeps paying for.
void CgsDev::DebugRender::DrawBox(const f32*, RGBA, Vector4, Vector4)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "DebugRender::DrawBox: inert (3D queue not reconstructed) [FLAG PC boot gate]\n";
    }
}

void CgsDev::DebugRender::DrawLine(RGBA, Vector3, Vector3)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "DebugRender::DrawLine: inert (3D queue not reconstructed) [FLAG PC boot gate]\n";
    }
}

// -------------------------------------------------------------------------
// CgsDev::PerfMonCpu
// -------------------------------------------------------------------------
// BOOT-GATE FORWARDER (world-module mount 2026-07-26): the 6-arg hierarchical
// X360 ARTIST form is REACHED at boot ~27x by the real SceneManagerModule::
// Construct @0x828D09A0 perf-mon block. Forward into the live 5-arg registry
// (colour == the page id at every call site -- SceneManagerModule's own
// comments name colour 16/17 as pages; liFlags is the libperf tag). The
// parent-handle nesting is NOT modelled by the PC registry yet -- reconstruct
// the hierarchical registry before relying on the perfmon tree view.
int CgsDev::PerfMonCpu::AddMonitor(char const * lpcName, int liColour, int liMinimum, double lfCpuBudget, int liParentHandle, int liFlags)
{
    (void)liParentHandle;
    return CgsDev::PerfMonCpu::AddMonitor(lpcName,
                                          static_cast<CgsDev::PerfMonCpuPage>(liColour),
                                          liMinimum != 0,
                                          static_cast<float>(lfCpuBudget),
                                          liFlags != 0);
}

// -------------------------------------------------------------------------
// CgsGeometric::Frustum
// -------------------------------------------------------------------------
// (CgsGeometric::Frustum::CalcVertices stub RETIRED 2026-08-12, shadow-cascade
//  wave. The inert boot gate that stood here -- an empty body + a one-shot log,
//  installed 2026-07-27 when the trap form stopped the sim on frame 1 -- was the
//  SOLE definition in the tree, so BrnWorld::ShadowMap::ComputeBoundingBoxMatrix
//  fitted its best-fit box to an UNINITIALISED stack array and 39 of 41 sampled
//  frames reported a non-finite cascade view-projection. The real X360 body
//  @0x82840DF8 now lives in its canonical home
//  GameShared/GameClasses/Geometric/Primitives/CgsFrustum.cpp (already mounted in
//  tools/build/build_game_exe.bat) -- eight plane-triple Cramer solves over the
//  de-swizzled six planes. That file's banner carries the recovered rodata
//  permute masks and the raw-word vsldoi decode that pin the transpose.
//  The declaration this file needs comes from the CgsFrustum.h include at :114,
//  which stays.)

// (CgsGeometric::Frustum::SetFromRwFrustum stub RETIRED 2026-07-28, culling wave:
//  the real body @0x82839FA8 lives in its X360 home
//  GameShared/GameClasses/Geometric/Primitives/CgsFrustum.cpp.
//  ⚠ The note that stood here -- "the rodata vperm masks the X360 transposes with
//  are unrecoverable from the exports, but the lane MEANING is pinned by the two
//  readers of the stored form" -- was WRONG on both counts and is RETRACTED
//  (2026-08-12). All eight masks were read out of the .i64; they encode a
//  PERMUTATION (RW near/far/left/right/top/bottom -> stored slots 5/4/0/2/1/3)
//  that the body had flattened to identity, and the pad lanes duplicate far/near
//  rather than being zero. The permutation-invariant readers cited as the pin
//  were exactly why the bug stayed invisible. See the body's banner.)

// -------------------------------------------------------------------------
// CgsGraphics::Camera -- DESTUBBED (2026-07-26 wave): Construct (both overloads,
// @0x827F0A08 / @0x827F94E8), Release @0x8284CB38, SetFovHorizontal @0x821F13B0,
// UpdatePerspectiveProjectionMatrix @0x827EC778,
// SetPerspectiveProjectionMatrixRightHanded @0x827EC698, LookAt @0x827F9510,
// Clone @0x827E7018 and the GetPosition/GetDirection additive accessors now live in
// GameShared/GameClasses/Graphics/CgsCamera.cpp. (2026-08-17: the PC-additive
// SetFarClip is RETIRED in favour of the DWARF Camera::SetFarClipPlane, which -- with
// SetAspectRatio and SetNearClipPlane -- is now an inline in CgsCamera.h; and the
// no-arg Construct()/Release() pair is NOT one ICF-folded body, Release() is the empty
// @0x8284CB38. See the CgsCamera.cpp banner.)
//
// DESTUBBED (camera-frustum wave 2026-07-27): the whole frustum-writer family
// -- GetFrustum(CameraRwFrustum&)/GetFrustumParallel/GetFrustumPerspective
// (@0x82277298 / @0x827F11A8 / @0x827F0AD8, out-param DWARF shapes, numeric-
// emulation-verified), the GetCgsFrustum/GetCgsFrustumParallel wrappers
// (@0x827F9778 / @0x827F97B8), UpdateOrthogonalProjectionMatrix @0x827E72E0,
// GetViewProjectionMatrixModified @0x827EC858 (the un-dumped unk_82CDA3C0/400
// vperm controls resolved by derivation -- see CgsCamera.cpp), and the no-arg
// PC-bridge accessors -- all now live in CgsCamera.cpp.
// (Camera::Clear() remains declaration-only in the header -- never referenced
// by a linked TU, so it carries no stub here.)
// -------------------------------------------------------------------------
// CgsGraphics::DispatchBin
// -------------------------------------------------------------------------
// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
void CgsGraphics::DispatchBin::HandleMemoryOverflow(unsigned int)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "CgsGraphics::DispatchBin::HandleMemoryOverflow: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// -------------------------------------------------------------------------
// CgsGraphics::DispatchList / CgsGraphics::DrawRenderable
// -------------------------------------------------------------------------
// (renderer world-pass wave 2026-07-27): DispatchList::AllocateKeyBlock
// @0x827FA730 and DrawRenderable::AddToBin @0x827FA0D0 are now REAL in their
// home TUs (Dispatch/CgsGraphicsDispatchList.cpp and
// Dispatch/CgsDispatcherCommands.cpp, both on the exe source list) -- their
// link stubs were DELETED here to clear the duplicate-symbol errors.

// -------------------------------------------------------------------------
// CgsGraphics::Model
// -------------------------------------------------------------------------
// GetNumLods / GetNumRenderables are REAL now, in their home TU
// (GameShared/GameClasses/Graphics/CgsModel.cpp, beside GetRenderable /
// DoesStateExist / GetLodDistance): both are single named-field reads
// (mu8NumStates @+0x12, mu8NumRenderables @+0x10). Their "return 0" gates here made
// every streamed world instance fail RenderInstance's "Model in unit has no lods!"
// assert, so they were the first hard stop on the world-draw path. Stubs DELETED.

// -------------------------------------------------------------------------
// CgsSceneManager::CachedTriangleList
// -------------------------------------------------------------------------
// ⭐⭐ GATE DELETED 2026-08-10 (fill-worker wave). CachedTriangleList::Prepare
// @0x828BE520 (79 insns) is now a real body in
// GameShared/GameClasses/SceneManager/CacheManager/CgsCachedTriangleList.cpp.
// This gate was NOT harmless: it returned true without allocating, so the shared
// triangle-cache arena was a NULL pointer that all 298 cache-slot windows indexed
// into. The shipped tripwire that says so (CgsTriangleCacheManager.h:172,
// "mpaTriangleCache != NULL") had never once executed, because no slot had ever
// been marked dirty -- it fired the first time this wave forced the console's own
// mbDEBUGForceAllDirty switch for one instrumented boot.
// ⚠️ 0x828BE520 is an X360 export-set HOLE; the name came from the xrefs_from of
// its only caller, TriangleCacheManager::Prepare @0x828BE738, and the PS3 DWARF
// mangle @0xC7B30C types the signature.

// -------------------------------------------------------------------------
// CgsSceneManager::EntityManager
// -------------------------------------------------------------------------
// GATE RETIRED 2026-08-18 (wave Q5): CgsSceneManager::EntityManager::GetVolumeInstanceIndexByID @0x828CD4B8 is REAL in CgsEntityManager.cpp (this stub RETURNED 0 for every id, every frame).
// GATE RETIRED 2026-08-18 (wave Q5): CgsSceneManager::EntityManager::GetVolumeInstance @0x828B9F28 is REAL in CgsEntityManager.cpp.

// -------------------------------------------------------------------------
// CgsSceneManager::FineIntersectionTestModule
// -------------------------------------------------------------------------
// BOOT-GATE (world-module mount 2026-07-26): REACHED at boot via the real
// SceneManagerModule::Construct @0x828D09A0 sub-manager cascade; quiet no-op
// (see AIModule::Construct). Reconstruct from X360 before wiring Prepare.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
void CgsSceneManager::FineIntersectionTestModule::Construct()
{
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool CgsSceneManager::FineIntersectionTestModule::Prepare(class CgsSceneManager::EntityManager *,class CgsSceneManager::VolumeManager *)
{
    // BOOT-GATE (attribsys wave 2026-07-26): REACHED by the world Prepare chain
    // (SceneManagerModule::Prepare / the world stage machine). One-shot log +
    // report success so the scripted load advances; the sub-manager stays
    // inert (zero-initialised storage) and its consumers keep their traps.
    // Reconstruct from X360 (FineIntersectionTest + rw::collision query cluster).
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "FineIntersectionTestModule::Prepare: inert [FLAG PC boot gate]\n";
    }
    return true;
}

// -------------------------------------------------------------------------
// CgsSceneManager::OverlapGenerationModule
// -------------------------------------------------------------------------
// GATE RETIRED 2026-08-19 (wave Q5 round 3 / E2): OverlapGenerationModule::GenerateOverlaps @0x828D5C08, Prepare, Construct, Release and Update(const InputBuffer*) are REAL in ContactGen/CgsOverlapGenerationModule.cpp (mounted). Destruct below is the one survivor (no address, no body anywhere).


// The four module virtuals below became link-required when the game module
// mounted the REAL WorldModule (2026-07-26): SceneManagerModule embeds
// mOverlapGenerator by value inside the by-value fleet, so BrnGameModule.obj
// emits the vtable. Construct IS reached at boot (the real
// SceneManagerModule::Construct @0x828D09A0 constructs each sub-manager);
// its committed body (@0x828D0460, CgsOverlapGenerationModule TU) is not
// linked because it drags the rw::collision / loose-octree closure (see the
// banner) -- boot-fire is diagnosed + gated per the mount-wave log.

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
void CgsSceneManager::OverlapGenerationModule::Destruct()
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "CgsSceneManager::OverlapGenerationModule::Destruct: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// -------------------------------------------------------------------------
// CgsSceneManager::SceneManagerIO::InSceneUpdateInterface
// -------------------------------------------------------------------------
// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
// (InSceneUpdateInterface::AddEntity(EntityId,u32,Vector3,f32) stub RETIRED
//  2026-07-28, culling wave: the real 4-arg producer @0x822B11F8 -- the one that
//  carries the bounding-sphere CENTRE in the vmx lane -- now lives beside its
//  3-arg sibling in CgsSceneManagerIO_SceneUpdate.cpp. It is the entry point of
//  the whole broad-phase registration chain, so it can no longer be inert.)

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
void CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::AddVolumeInstance(class CgsSceneManager::EntityId,struct rw::math::vpu::Matrix44Affine const &)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::AddVolumeInstance: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// (InSceneUpdateInterface::Append stub RETIRED 2026-07-28, culling wave: the real
//  25-queue whole-interface merge @0x827A9340 now lives in
//  CgsSceneManagerIO_SceneUpdate.cpp. It is the hop that carries the entity
//  modules' staged scene adds into the scene manager's update input, so the drop
//  is no longer the consistent observable -- the broad-phase now holds data.)

// (InSceneUpdateInterface::SetCullingGroupPair stub RETIRED 2026-07-26: the real
// producer @0x822B1B60 now lives in CgsSceneManagerIO_SceneUpdate.cpp, alongside
// the new ClearCullingTable @0x827BAB78 + InSceneUpdateInterface::Construct.)

// -------------------------------------------------------------------------
// CgsSceneManager::SceneManagerIO::InputBuffer_Query
// -------------------------------------------------------------------------
// (CgsSceneManager::SceneManagerIO::InputBuffer_Query::Construct stub RETIRED
//  2026-07-27: the real (partial-slice) body now lives in CgsSceneManagerIO.h,
//  matching the X360 0x828C7BC0 status + coarse-query-queue bring-up.)

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
void CgsSceneManager::SceneManagerIO::InputBuffer_Query::Destruct()
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "CgsSceneManager::SceneManagerIO::InputBuffer_Query::Destruct: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// (InputBuffer_Query::GetInCoarseQueryQueue gate RETIRED 2026-07-29, culling wave:
//  it returned NULL, so the first real frustum query null-dereferenced it. The member
//  mInCoarseQueryQueue IS committed (CgsSceneManagerIO.h) and Construct already brings
//  it up, so the real accessor is now inline beside it -- X360 0x828AF270 returns
//  this+0x28.)

// -------------------------------------------------------------------------
// CgsSceneManager::SceneManagerIO::OutputBuffer
// -------------------------------------------------------------------------
// (CgsSceneManager::SceneManagerIO::OutputBuffer::GetSceneQueryResultsQueue stub
//  RETIRED 2026-07-27: real read-locked accessor now inline in CgsSceneManagerIO.h.)

// -------------------------------------------------------------------------
// CgsSceneManager::SceneManagerModule -- ASSESSED, all seven left stubbed
// (2026-07-26 wave), each for a concrete reason:
//   * Update: the committed 5-arg signature is the X360 vtbl+64 VIRTUAL the
//     WorldModule stages dispatch through; the concrete target (UpdateScene
//     @0x828D4C28) drives the spatial-partition / overlap sub-modules that are
//     deliberately seam-stubbed (rw::collision closure) -- reconstructing the
//     shell would still trap inside the seams. (The X360 symbol literally
//     named SceneManagerModule::Update @0x827E1F28 is a "Don't use this
//     function" assert trap, not this entry.)
//   * UpdateQueries / ExternalSceneQueriesUpdate: vtbl+68 dispatch; the
//     concrete targets (ProcessSceneQueries @0x828D57D0 / ProcessFineQueries
//     @0x828D5608 family) cannot be pinned without the un-dumped vtable and
//     also run the seam-stubbed fine-query sub-modules.
//   * ProcessFrustumTestJobRequests @0x828C7628: starts the loose-octree
//     frustum-test jobs (rw::collision + job-system closure, seam-stubbed).
//   * The three Bridge* bodies (@0x828BA8C8 / @0x828BA538 / the truncated-
//     symbol third): 2-4KB event-queue merge pipelines over the overlap
//     sub-module IO formats that are not homed yet.
// -------------------------------------------------------------------------
// GATES RETIRED 2026-08-19 (wave Q5 round 3 / F1): SceneManagerModule::BridgeOverlapCullerToOutputBuffer @0x828BA8C8 (THE PotentialContact producer), BridgeOverlapGenerationToOutputBuffer @0x828BA6A0 and BridgeOverlapGenerationToOverlapCulling @0x828BA538 are REAL in CgsSceneManagerBridgeFunctions.cpp (mounted), with the DWARF parameter types.


// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
void CgsSceneManager::SceneManagerModule::ExternalSceneQueriesUpdate()
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "CgsSceneManager::SceneManagerModule::ExternalSceneQueriesUpdate: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// (CgsSceneManager::SceneManagerModule::ProcessFrustumTestJobRequests gate RETIRED
//  2026-07-28, culling wave: the real body @0x828C7628 now lives in
//  CgsSceneManagerModule.cpp -- it stages every frame's coarse frustum queries into
//  the octree's job data blocks and kicks them.)

// (CgsSceneManager::SceneManagerModule::UpdateScene gate RETIRED 2026-07-28,
//  culling wave: the real body now lives in CgsSceneManagerModule.cpp -- it fans
//  the scene input's InSceneUpdateInterface out through
//  BridgeInputSceneUpdateInterfaceToSubModules @0x828D1F88 (entity legs) and runs
//  the partition's own per-frame bounds pass.)

// BOOT GATE -- SceneManagerModule::ProcessSceneQueries @0x828D57D0 (X360 vtbl+68).
// RENAMED 2026-07-27 from the placeholder "UpdateQueries", and SOFTENED from a
// CGS_ASSERT trap: WorldModule::Update @0x827D63E8 now drives it every frame (the
// physics query round trip) and the entity-module post-scene spine drives it once
// per module, so a trap here would block the sim on frame 1.
//
// The X360 shell (CgsSceneManagerModule.cpp:806..):
//   1. StartMonitor(dword_82F33ECC);
//   2. four null tripwires (:806..:809);
//   3. StartMonitor(dword_82F33ED0); ProcessCoarseQueries(inStack, outStack,
//      sceneIn, sceneOut); StopMonitor;
//   4. StartMonitor(dword_82F33ED4); ProcessFineQueries(inStack, outStack,
//      sceneIn, sceneOut); StopMonitor;
//   5. write-lock the scene output, publish &mTriangleCacheManager on it
//      (same :1268 tripwire as UpdateScene), unlock; StopMonitor.
// Both query passes walk the coarse/fine query queues; on the PC build those
// queues are empty (no entity is registered with the scene manager while the
// partition managers are gated), so the pass is a no-op with or without this gate.
//
// ⛔ GATE RETIRED 2026-08-11 (triangle-cache wiring wave) -- AND IT WAS NOT A NO-OP AFTER
// ALL. The banner above was right about steps 3/4 and WRONG about step 5: that publish is
// the ONLY write of a TriangleCacheManager* into a TriangleCacheInterface anywhere in the
// program, so gating this function left every downstream consumer's mpTriangleCacheManager
// NULL -- which is exactly what killed the traction-line leg the first time a race car
// existed ("mpTriangleCacheManager != NULL" + an AV in GetTrianglesForCachedObject).
// The real body now lives in CgsSceneManagerModule.cpp; steps 3/4 stay FLAGGED there.

// -------------------------------------------------------------------------
// CgsSceneManager::SpatialPartitionManager
// -------------------------------------------------------------------------
// (CgsSceneManager::SpatialPartitionManager::Construct gate RETIRED 2026-07-28,
//  culling wave: the real body now lives in CgsSpatialPartitionManager.cpp with
//  the rest of the manager, and that TU is on the exe source list.)

// (CgsSceneManager::SpatialPartitionManager::Prepare gate RETIRED 2026-07-28,
//  culling wave: the real staged handshake @0x828CFFA8 now carves + Constructs +
//  Prepares the LooseOctree out of the scene resource allocator, so the broad
//  phase really holds the world.)

// -------------------------------------------------------------------------
// CgsSceneManager::TriangleCacheManager
// -------------------------------------------------------------------------
// (GATE RETIRED 2026-08-10, cache-fill wave: TriangleCacheManager::EndUpdateTriangleCaches
//  @0x828BF150 (475) now has its REAL body in
//  GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManager_Update.cpp,
//  together with its Start partner @0x828BECF8 (278). This one is REACHED EVERY FRAME --
//  SceneManagerModule::EndUpdateTriangleCache @0x828C7500 is real and WorldModule::Update
//  calls it at BrnWorldModule.cpp:2471 -- so the real body runs from the frame it lands.
//  It opens with a null guard on mpUpdateTriangleCacheStream and takes it while the Start
//  side is still gated, which is exactly what the console does on a frame that posted no
//  fill. LNK2005 is the tripwire if this stub is ever restored.)

// -------------------------------------------------------------------------
// CgsSceneManager::TriangleCollisionManager
// -------------------------------------------------------------------------
// (GATE RETIRED 2026-08-10, spatial-partition wave: TriangleCollisionManager::Prepare now has
//  its REAL body in GameShared/GameClasses/SceneManager/TriangleCollision/
//  CgsTriangleCollisionManager.cpp, together with ProcessAddPolySoupListEvents @0x828B3160 and
//  ProcessClearPolySoupListEvents. BOTH were already fully reconstructed and had simply never
//  been MOUNTED -- [[mount-gap-is-the-bottleneck]]; this wave supplied the one body they were
//  missing, BuildSpacialPartition @0x82841740, and put the TU on the link.
//  ⚠️ THE ADDRESS IN THIS GATE'S OWN COMMENT WAS WRONG: it read "Reconstruct from X360
//  0x828D0C40", and NO EXPORT LIVES AT 0x828D0C40 -- checked against all 30,084 X360 export
//  JSONs. The real address is 0x828B2FF0 (91 insns), which the class header had right all
//  along. A committed address is a claim. LNK2005 is the tripwire if this stub is restored.)

// -------------------------------------------------------------------------
// CgsSceneManager::VolumeManager
// -------------------------------------------------------------------------
// GATE RETIRED 2026-08-18 (wave Q5): CgsSceneManager::VolumeManager::Prepare @0x828CFD38 is REAL in CgsVolumeManager.cpp (mounted).

// -------------------------------------------------------------------------
// ShaderConstantTable
// -------------------------------------------------------------------------
// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.

// LINK STUB (destub wave 2026-07-26, referenced by ShadowMap::SetConstantsForEnvmap
// @0x827C1AD0 -- the 16-byte overload with a live w lane): body not reconstructed yet
// (X360 @0x822B32E8; needs UpdateShaderChangeTableAndGetConstantDestination @0x822A0A20).
// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log -- reached on the per-frame world drive, where a trap stops the
// simulation. The body is still NOT reconstructed; the fix is the real X360 body
// in its own TU, not this gate.

// -------------------------------------------------------------------------
// WorldModule
// -------------------------------------------------------------------------
// GATE RETIRED 2026-08-18 (wave Q4 bridges): WorldModule::BridgeCrashModuleToPropModule_PostScene @0x827AAD78 is REAL in GameSource/World/Bridges/WorldBridgePropModule.cpp.

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
// GATE RETIRED 2026-08-25 (crash exit): WorldModule::BridgeCrashModuleToRaceCarModule_PostScene
// @0x827AD5A0 is REAL in GameSource/World/Bridges/WorldBridgeCrashPostScene.cpp. The phantom
// CrashModuleIO::OutputBuffer_PostScene it was declared against is deleted; the real parameter
// type is BrnWorld::CrashIO::OutputBuffer_PreScene (the crash module's ONE output buffer).

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
// GATE RETIRED 2026-08-25 (crash exit): WorldModule::BridgeCrashModuleToTrafficModule_PostScene
// @0x827AD5E0 is REAL in GameSource/World/Bridges/WorldBridgeCrashPostScene.cpp (same TU, same
// phantom-type retirement as its race-car sibling above).

// GATE RETIRED 2026-08-18 (wave Q4 bridges): WorldModule::BridgePropModuleToTrafficModule_PrePhysics @0x827AEA70 is REAL in GameSource/World/Bridges/WorldBridgePropModule.cpp.

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
void WorldModule::BridgeRaceCarEntityInfoToOutput_PrePhysics(void *,struct BrnWorldIO::UpdateOutputBuffer *,struct BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PrePhysics const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeRaceCarEntityInfoToOutput_PrePhysics: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
void WorldModule::BridgeRaceCarModuleToSceneModule_PostScene(void *,struct CgsSceneManager::SceneManagerIO::InputBuffer_Query *,struct BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostScene const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeRaceCarModuleToSceneModule_PostScene: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
void WorldModule::BridgeRaceCarModuleToTrafficModule_PostScene(void *,class BrnTraffic::BrnTrafficIO::InputBuffer_PostScene *,struct BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostScene const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeRaceCarModuleToTrafficModule_PostScene: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
void WorldModule::BridgeRaceCarModuleToTrafficModule_PrePhysics(void *,class BrnTraffic::BrnTrafficIO::InputBuffer_PrePhysics *,struct BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PrePhysics const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeRaceCarModuleToTrafficModule_PrePhysics: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// GATE RETIRED 2026-08-18 (wave Q4 bridges): WorldModule::BridgeSceneContactsToPropModule_PrePhysics @0x827ABCB0 is REAL in GameSource/World/Bridges/WorldBridgePropModule.cpp.

// GATE RETIRED 2026-08-19 (wave Q6 / rcfork): BridgeSceneContactsToRaceCarModule_PrePhysics @0x827ABBD0 is REAL in WorldBridgeSceneToEntityModules.cpp (the RaceCarEntityModuleIO::PotentialContactQueue derived-struct fork collapsed to the DWARF typedef).

// GATE RETIRED 2026-08-19 (wave Q5 round 3 / F2): BridgeSceneContactsToTrafficModule_PrePhysics @0x827ABC50 is REAL in WorldBridgeSceneToEntityModules.cpp.


// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
void WorldModule::BridgeSceneQueryResultsToTrafficModule_PrePhysics(void *,class BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics *,class BrnTraffic::BrnTrafficIO::InputBuffer_PrePhysics *,struct CgsSceneManager::SceneManagerIO::OutputBuffer const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeSceneQueryResultsToTrafficModule_PrePhysics: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
void WorldModule::BridgeSceneQueryResultsToTriggerModule_PrePhysics(void *,class BrnWorld::TriggerEntityModuleIO::InputBuffer_PrePhysics *,struct CgsSceneManager::SceneManagerIO::OutputBuffer const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeSceneQueryResultsToTriggerModule_PrePhysics: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
void WorldModule::BridgeTrafficCarEntityInfoToOutput_PrePhysics(void *,struct BrnWorldIO::UpdateOutputBuffer *,class BrnTraffic::BrnTrafficIO::OutputBuffer_PrePhysics const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeTrafficCarEntityInfoToOutput_PrePhysics: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
void WorldModule::BridgeTrafficModuleToSceneModule_PostScene(void *,struct CgsSceneManager::SceneManagerIO::InputBuffer_Query *,struct BrnTraffic::BrnTrafficIO::OutputBuffer_PostScene const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeTrafficModuleToSceneModule_PostScene: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// (BridgeTrafficToRaceCar_PrePhysics stub RETIRED 2026-07-27: the REAL body
// @0x827A51F0 lives in its own home TU, Bridges/WorldBridgeEntityModulesToEntityModules.cpp,
// which the world-drive wave mounts on the build list.)

// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
void WorldModule::BridgeTriggerModuleToSceneModule_PostScene(void *,struct CgsSceneManager::SceneManagerIO::InputBuffer_Query *,class BrnWorld::TriggerEntityModuleIO::OutputBuffer_PostScene const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeTriggerModuleToSceneModule_PostScene: inert (body not reconstructed) [FLAG PC boot gate]\n";
    }
}

// (rw::BitTable::GetResourceDescriptor stub RETIRED 2026-07-26: the real body now
// lives in its owning TU, src/vendor/renderware/collision/BitTable.cpp.)

// -------------------------------------------------------------------------
// rw::collision::Volume / rw::collision::VolumeVolumeQuery
// -------------------------------------------------------------------------
// GATES RETIRED 2026-08-18 (wave Q5, rw::collision narrow-phase mount): Volume::InitializeVTable
// (static, REAL in SDKs/EATech/rwcollision/volume.cpp), VolumeVolumeQuery::Initialize (was a NULL
// return that left OverlapCullingModule::mpVolVolQuery null) and VolumeVolumeQuery::
// GetResourceDescriptor (was a FABRICATED 0x62000/16 descriptor) -- REAL in
// vendor/renderware/collision/VolumeQuery.cpp, mounted with the collision block.

// -------------------------------------------------------------------------
// rw::math::vpu
// -------------------------------------------------------------------------
// -------------------------------------------------------------------------
// struct BrnPhysics::Deformation::WheelPhysicalStates & __ptr64 BrnPhysics::Deformation::WheelPhysicalStates::operator=(struct BrnPhysics::Deformation
// -------------------------------------------------------------------------
// RETIRED 2026-08-24 (physics mount wave B1): the real store-order-faithful body
// @0x825C0A00 now mounts in GameShared/GameClasses/Physics/Deformation/
// BrnWheelPhysicalStates.cpp (the link flagged the duplicate, exactly as this gate's
// banner predicted: "the fix is the real X360 body in its own TU, not this gate").
// NB the gate was a SILENT-DROP stub reached every frame -- wheel physical states
// now actually copy for the first time; boot-measured after the mount.

// RETIRED 2026-08-01 (camera wave): RCEntityActiveRaceCarOutputInterface::operator= now has
// its real member-wise body in
// SharedIO/BrnRCEntityActiveRaceCarOutputInterface.cpp. While this gate was here,
// UpdateOutputBuffer::SetActiveRaceCarOutputInterface -- the world's only per-frame
// race-car publish -- ran every frame and copied NOTHING.

// -------------------------------------------------------------------------
// void BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface::operator=(struct BrnWorld::RaceCarEntityModuleIO
// -------------------------------------------------------------------------
// BOOT GATE (world-IO wave 2026-07-27): converted from an assert TRAP to a quiet
// one-shot log. This symbol is REACHED every frame now that WorldModule::Update
// @0x827D63E8 drives the world, and a trap stops the simulation on frame 1. The
// body is still NOT reconstructed -- the fix is the real X360 body in its own TU,
// not this gate.
// RETIRED 2026-08-01 (camera wave): the real member-wise body now lives in
// SharedIO/BrnRCEntityGlobalRaceCarOutputInterface.cpp. Same silent-drop story as its
// active-race-car sibling above.

// ---------------------------------------------------------------------------
// Attrib mount closure stubs (2026-07-27): symbols the linked SDK TUs
// reference whose bodies are documented NEXT-WAVE gaps (attrib_sdk_wave_log
// G-list). Each traps loudly; none is on the schema/vault-register path.
// ---------------------------------------------------------------------------
// (Attrib::Instance::GetAttributePointer(key, index) was a CGS_ASSERT(false) stub here
// until 2026-07-31, on the theory that the X360 "no-arg form" was the real one and this
// keyed overload was a separate un-landed function. There is only ONE such symbol
// @0x82805880 and this IS its signature -- the body tail-calls Collection::GetData with
// r4/r5 untouched. Real in attribinstance.cpp now; the no-arg spelling is retired.)

// LINK STUB (attrib mount closure): GameTalk live-edit decode (gap G6).
void Attrib::DecodeLiveLinkMessage(char const *)
{
    CGS_ASSERT(false, "Attrib::DecodeLiveLinkMessage: attrib gap G6 -- reconstruct");
}

// LINK STUB (attrib mount closure): hashmap removal (gap G2, edit/GC path).
Attrib::Collection * Attrib::CollectionHashMap::RemoveIndex(unsigned int)
{
    CGS_ASSERT(false, "Attrib::CollectionHashMap::RemoveIndex: attrib gap G2 -- reconstruct");
    return 0;
}

// LINK STUB (attrib mount closure): node schema lookup (gap G2, GC/Clear path).
Attrib::TypeDesc const * Attrib::Node::GetTypeDesc(void) const
{
    CGS_ASSERT(false, "Attrib::Node::GetTypeDesc: attrib gap G2 -- reconstruct");
    return 0;
}

// ---------------------------------------------------------------------------
// RealmcIface::MemcardInterface base ctor/dtor -- TRIVIAL REAL BODIES (the
// header's own notes: ctor @0x82B51C00 is a single vtable store == an empty
// C++ ctor; the virtual dtor backs the vector-deleting slot @0x82B51BB8).
// Their DWARF home RealmcMemcardInterface.cpp is NOT linked because its
// CreateInstance drags the uncommitted RealmcCore closure (ObjectManager /
// AllocateMem / Locale callbacks) -- cost rule; the wave-B SaveLoad PS3 TU
// needs only this base pair for its NoOpMemcardInterface.
// ---------------------------------------------------------------------------
RealmcIface::MemcardInterface::MemcardInterface()
{
}

RealmcIface::MemcardInterface::~MemcardInterface()
{
}

// ===========================================================================
// WORLD-DRIVE BOOT GATES (2026-07-27)
//
// WorldModule::Update @0x827D63E8 + its four entity-module phase spines are now
// REAL, so every per-frame bridge and module entry point they call must exist.
// The ones below are not reconstructed yet; each is a QUIET ONE-SHOT-LOG NO-OP
// (never a CGS_ASSERT trap -- a trap here blocks the sim on the first frame).
// Every gate names its X360 address so the reconstruction replaces it in place;
// delete the gate when the real body lands (the two definitions must not coexist).
//
// Why inert is the consistent observable: the destination modules (race car /
// traffic / prop / trigger / crash / AI / physics) are themselves gated inert,
// so a bridged payload would have no consumer. The world-entity legs -- the ones
// the STREAMER rides -- are deliberately NOT in this list: BridgeWorldEntityInfoToOutput
// @0x827ADD78, BridgeActionsToWorldModule @0x827AC488 and
// BridgeRaceCarModuleToWorldModule_PreScene @0x827A52B0 are real bodies in their
// own TUs.
// ===========================================================================

// ⛔⛔ GATE RETIRED: WorldModule::BridgeRaceCarModuleToTrafficModule_PreScene @0x827A50E0
//     is REAL in GameSource/World/Bridges/WorldBridgeRaceCarToTrafficModule.cpp. It is the
//     only producer that primes the traffic input buffers' ActiveRaceCarOutputInterface, so
//     while gated, PreSceneUpdate's IsPlayerCarActive() and UpdateRaceCarHulls read a buffer
//     nothing filled (activeHulls stuck at 0). ⚠️ Behaviour change on a live path: the
//     traffic module now sees the real race-car state each frame.

// GATE RETIRED 2026-08-12 (prop-spawn wave): WorldModule::BridgeRaceCarModuleToPropModule_PreScene @0x827A5510
// is now REAL -- body in GameSource/World/Bridges/ (see the 'PROP SPAWN WAVE'
// block in tools/build/build_game_exe.bat). It publishes the player position/index/crashing/wrecked flags and the 8-slot race-car
// velocity array (w lane = speed in MPH) into the prop module's pre-scene input.

// GATE RETIRED 2026-08-12 (prop-spawn wave): WorldModule::BridgeWorldModuleToPropModule_PreScene @0x827AACF8
// is now REAL -- body in GameSource/World/Bridges/ (see the 'PROP SPAWN WAVE'
// block in tools/build/build_game_exe.bat). It carries the PropInstancesNeededForZone / PropGraphicsLoaded / PropGraphicsUnloaded
// queues and the player zone number. While it was inert every queue read length 0, so the
// streaming machine had nothing to ask for and NO ZONE COULD EVER LOAD.

// ⭐⭐ GATE DELETED 2026-08-11 (physics->output publish wave):
// WorldModule::BridgePhysicsModuleToRaceCarModule_PostPhysics @0x827AE9D0 is REAL, in
// GameSource/World/Bridges/WorldBridgePhysicsToEntityModules.cpp (its declared home). It is the
// ONLY thing in the XEX that copies the physics module's output buffer into the race-car module's
// post-physics input buffer, so while it was inert the landed readback
// (RaceCarEntityModule::ReadUpdatedActiveRaceCarDataFromPhysics) was gated off no matter what the
// physics side published. Its two live legs carry the VehicleOutputInterface and the
// VehicleManagerOutputInterface; the four deformation/scene/contact-spy legs are parked LOUDLY in
// that file (their blockers are named there). If a gate for it reappears here the link will say
// so (LNK2005).

// GATE RETIRED 2026-08-22 (traffic wave T3): WorldModule::BridgePhysicsModuleToTrafficModule_PostPhysics
// @0x827AB910 is REAL in GameSource/World/Bridges/WorldBridgePhysicsToEntityModules.cpp (the
// physics->traffic readback leg; its destination queues are Constructed in InputBuffer_PostPhysics).

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// GATE RETIRED 2026-08-18 (wave Q4 bridges): WorldModule::BridgePhysicsModuleToPropModule_PostPhysics @0x827AB998 is REAL in GameSource/World/Bridges/WorldBridgePropModule.cpp.

// GATE RETIRED 2026-08-25 (crash exit): WorldModule::BridgePhysicsModuleToCrashModule_PostPhysics
// @0x827AB8B0 is REAL in GameSource/World/Bridges/WorldBridgeCrashInputs.cpp. 17 instructions --
// and they are the ONLY route by which a RaceCarCrashEvent reaches the crash module.

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 0x827A5680 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgePhysicsModuleToAIModule_PostPhysics(void *,struct BrnAI::AIModuleIO::InputBuffer_PostPhysics *,class BrnPhysics::PhysicsModuleIO::OutputBuffer const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgePhysicsModuleToAIModule_PostPhysics: inert [FLAG PC boot gate]\n";
    }
}

// GATE RETIRED 2026-08-25 (crash exit): WorldModule::BridgeTrafficToCrashModule_PostPhysics
// @0x827AD708 is REAL in GameSource/World/Bridges/WorldBridgeCrashInputs.cpp.

// RETIRED 2026-08-09 (feed wave): WorldModule::BridgeInputToPhysicsModule is REAL in
// GameSource/World/Bridges/WorldBridgeInputToPhysicsModule.cpp. Its X360 address --
// 0x827AB830 -- is a HOLE in the IDA export set and was recovered by decoding the `bl`
// at the WorldModule::Update call site out of the image; see that TU's banner. The gate
// that used to sit here said "X360 the Update input fan-out" precisely because no wave
// had been able to name an address for it.

// GATE RETIRED 2026-08-25 (crash exit): WorldModule::BridgeInputToCrashModule @0x827ADEE8 is
// REAL in GameSource/World/Bridges/WorldBridgeCrashInputs.cpp. Its TIMER leg is what makes the
// crash countdown advance at all; three blob legs stay parked INSIDE the real body.

// RETIRED 2026-08-01 (reset-player-car wave): WorldModule::BridgeActionsToRaceCarModule
// @0x827ABF40 is REAL in
// GameSource/World/Bridges/WorldBridgeToEntityModules.cpp beside its physics/traffic/
// world-entity siblings. It is the only producer of the race-car module's game-action
// queue, so this gate discarded every game action the race-car module was ever sent --
// including action 0, ResetPlayerCarAction.

// GATE RETIRED 2026-08-12 (prop-spawn wave): WorldModule::BridgePropToOutput_PreScene @0x827AF258
// is now REAL -- body in GameSource/World/Bridges/ (see the 'PROP SPAWN WAVE'
// block in tools/build/build_game_exe.bat). It carries the prop pre-scene resource-request ring out to the world output buffer.

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 0x827AF318 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgeRaceCarEntityInfoToOutput_PreScene(void *,struct BrnWorldIO::UpdateOutputBuffer *,struct BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeRaceCarEntityInfoToOutput_PreScene: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 the _PrePhysics sibling's pre-scene twin -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgeTrafficEntityInfoToOutput_PreScene(void *,struct BrnWorldIO::UpdateOutputBuffer *,class BrnTraffic::BrnTrafficIO::OutputBuffer_PreScene const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeTrafficEntityInfoToOutput_PreScene: inert [FLAG PC boot gate]\n";
    }
}

// (WorldModule::BridgeEntityModulesToOutput_PostPhysics gate RETIRED 2026-07-27:
//  the real streamer leg (X360 0x827AEEB0) now lives in its home TU
//  Bridges/WorldBridgeEntityModulesToOutput.cpp.)

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 0x827AEB18 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgePhysicsToOutput(void *,struct BrnWorldIO::UpdateOutputBuffer *,class BrnPhysics::PhysicsModuleIO::OutputBuffer const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgePhysicsToOutput: inert [FLAG PC boot gate]\n";
    }
}

// ⛔ GATE RETIRED 2026-08-11 (triangle-cache wiring wave): WorldModule::
// BridgeSceneModuleToOutput @0x827A5700 now has its REAL body in
// GameSource/World/Bridges/WorldBridgeSceneToOutput.cpp (the console's own file name, from
// the assert rodata). It is the ONLY caller of the already-landed
// BrnWorldIO::UpdateOutputBuffer::AppendTriangleCacheInterface @0x8279BAF8.

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 the crash post-physics output leg -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgeCrashModuleToOutput(void *,struct BrnWorldIO::UpdateOutputBuffer *,struct BrnWorld::CrashIO::OutputBuffer_PostPhysics const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeCrashModuleToOutput: inert [FLAG PC boot gate]\n";
    }
}

// (WorldModule::BridgeEntityModulesToSceneModule_PreScene gate RETIRED 2026-07-28,
//  culling wave: the real body @0x827AB490 now lives in its X360 home TU
//  GameSource/World/Bridges/WorldBridgeEntityModulesToScene.cpp. It is the hop
//  that carries every entity module's staged scene adds into the scene manager's
//  update input, so the broad-phase now holds the streamed world.)

// (WorldModule::BridgeEntityModulesToScene_PostPhysics gate RETIRED 2026-07-28,
//  culling wave: the real body @0x827AB608 now lives in its X360 home TU
//  GameSource/World/Bridges/WorldBridgeEntityModulesToScene.cpp.)

// (WorldModule::BridgeEntityModulesToPhysicsModule_PreScene gate RETIRED 2026-08-10,
//  root-cause wave: the real body @0x827AADB8 now lives in its X360 home TU
//  GameSource/World/Bridges/WorldBridgeEntityModulesToPhysics.cpp. It is the ONLY
//  caller in the image of PhysicsModuleIO::InputBuffer::SetSolverMaxIterations
//  @0x8279F240, so while it was inert the solver iteration cap stayed at 0 and the
//  whole MaxIterations chain inside PhysicsModule::Update asserted.)

// (WorldModule::BridgeEntityModulesToPhysicsModule_PrePhysics gate RETIRED 2026-08-10,
//  pre-physics bridge wave: the real body @0x827AAEC0 now lives in its X360 home TU
//  GameSource/World/Bridges/WorldBridgeEntityModulesToPhysics.cpp. It is the ONLY thing in
//  the image that carries a staged CreateRaceCarEvent from the race-car entity module into
//  the physics module's input buffer, so while it was inert VehicleManager::ProcessCreateEvents
//  had an empty queue at every drain of a 275 s run -- MEASURED by the previous wave's census.
//  Landing it also forced two latent memory bugs out of hiding: BrnTrafficIO::
//  OutputBuffer_PrePhysics modelled its vehicle-driver interface as `unsigned char[1]`, and
//  PhysicsModuleIO::InputBuffer did the same for its vehicle-effects interface -- this bridge
//  Appends 5,284 and 1,792 bytes into them respectively.)

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 0x827AB738 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgeInputToAIModule(void *,struct BrnAI::AIModuleIO::InputBuffer *,struct BrnWorldIO::UpdateInputBuffer const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeInputToAIModule: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 0x827A5020 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgeTrafficModuleToAIModule_Update(void *,struct BrnAI::AIModuleIO::InputBuffer *,struct BrnTraffic::BrnTrafficIO::OutputBuffer_PostScene const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeTrafficModuleToAIModule_Update: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 0x827AD688 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgeRaceCarModuleToAIModule_PostScene(void *,struct BrnAI::AIModuleIO::InputBuffer *,class BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostScene const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeRaceCarModuleToAIModule_PostScene: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// GATE RETIRED 2026-08-18 (wave Q4 bridges): WorldModule::BridgeAIToEntityModules_PrePhysics @0x827AD540 is REAL in GameSource/World/Bridges/WorldBridgePropModule.cpp.

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 0x827A4F58 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgeAIToEntityModules_PostPhysics(void *,struct BrnWorld::RaceCarEntityModuleIO::InputBuffer_PostPhysics *,struct BrnAI::AIModuleIO::OutputBuffer const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeAIToEntityModules_PostPhysics: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 the AI -> physics staging -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgeAIModuleToPhysicsModule(void *,class BrnPhysics::PhysicsModuleIO::InputBuffer *,struct BrnAI::AIModuleIO::OutputBuffer const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeAIModuleToPhysicsModule: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 0x827A8D20 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgePhysicsSceneQueriesToScene(void *,struct CgsSceneManager::SceneManagerIO::InputBuffer_Query *,class BrnPhysics::PhysicsModuleIO::OutputBuffer const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgePhysicsSceneQueriesToScene: inert [FLAG PC boot gate]\n";
    }
}

// ⛔ GATE RETIRED 2026-08-11 (triangle-cache wiring wave): WorldModule::
// BridgeSceneQueryResultsToPhysics @0x827A8E88 now has its REAL body in
// GameSource/World/Bridges/WorldBridgeSceneToPhysics.cpp. Its tail
// (VehicleInputInterface::AppendTriangleCacheInterface @0x8279B978) is the ONLY code that
// ever hands the physics vehicle input its triangle-cache manager -- the note that used to
// stand here ("the module/interface it would feed is itself gated inert, so dropping the
// transfer is the consistent observable") was FALSE: the consumer was live and crashing.

// GATE RETIRED 2026-08-19 (wave Q5 round 3 / F2): BridgeScenePotentialContactsToPhysics @0x827ABD80 is REAL in WorldBridgeSceneToPhysics.cpp.



// ---- module entry points driven by the spines ----------------------------

// RETIRED (race-car streamer wave 2026-07-31): RaceCarEntityModule::PreSceneUpdate and
// ::PostPhysicsUpdate are now REAL partial slices in BrnRaceCarEntityModule.cpp -- they
// latch the sim time step, pump the five component streamers (UpdateStreaming @0x822FEFE0)
// and drain their GameData request rings onto the output buffer (SendStreamerEvents
// @0x82304F70). The rest of both console spines is still un-homed and FLAGGED there.


// ⛔⛔ GATE RETIRED: BrnTraffic::TrafficEntityModule::PreSceneUpdate @0x8274A968 is REAL in
//     EntityModules/TrafficEntityModule/BrnTrafficEntityModule_wT1_02.cpp, documented
//     PARTIAL: the IO lock bracket, the whole meState switch, and the
//     E_STARTINGUPSTATE_WAITING_FOR_PLAYER -> _POPULATING transition including its
//     GetActiveRaceCarOutputInterface()->IsPlayerCarActive() test are real; the RUNNING and
//     TEARING_DOWN arms and the five output producers are NAMED in-body gates.
//     ⚠️ BrnTrafficEntityModule_wT1_02.cpp must stay mounted in
//     tools/build/build_game_exe.bat beside _wT1_01.cpp. Retiring this gate without that
//     mount is LNK2019 at exe link; mounting without retiring is LNK2005, because the gate
//     spelled the symbol with a token-identical signature (BrnUpdateSet is a bare
//     `typedef u16`) that the per-TU `cl /c` gate is blind to.

// ⛔⛔ GATE RETIRED: `TrafficEntityModule::PostPhysicsUpdate` @0x8274E6D0 is REAL in
//     BrnTrafficEntityModule_wT1_01.cpp, documented PARTIAL: lock bracket, streaming latch,
//     UpdateDensity, tear-down trigger and the whole E_STATE_STARTING_UP arm are real;
//     head, RUNNING, TEARING_DOWN and tail are NAMED in-body gates.

// GATE RETIRED 2026-08-12 (prop-spawn wave): BrnWorld::PropEntityModule::PreSceneUpdate @0x82309A40 is now REAL.

// The body lives in this module's own TU under PropEntityModule/ (see the

// 'PROP SPAWN WAVE' block in tools/build/build_game_exe.bat). Leaving this inert

// definition here would be a duplicate symbol at link.
// GATE RETIRED 2026-08-18 (breakable-props wave Q mount): BrnWorld::PropEntityModule::PostPhysicsUpdate @0x823031D8 (PropEntityModule_wQ2_02.cpp) is now REAL.

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. the trigger pre-scene tick (X360 vtbl+64).
// Reconstruct from X360 and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void BrnWorld::TriggerEntityModule::PreSceneUpdate(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,class BrnWorld::TriggerEntityModuleIO::InputBuffer_PreScene *,class BrnWorld::TriggerEntityModuleIO::OutputBuffer_PreScene *,unsigned short)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "TriggerEntityModule::PreSceneUpdate: inert [FLAG PC boot gate]\n";
    }
}

// GATE RETIRED 2026-08-25 (crash exit): BrnWorld::CrashModule::PreSceneUpdate @0x827D3A60 is REAL
// in GameSource/World/CrashModule/BrnCrashModule_RaceCarCrashes.cpp -- it runs TickCrashes and
// ClearupCrashes, i.e. the crash countdown and the crash EXIT.

// GATE RETIRED 2026-08-25 (crash exit): BrnWorld::CrashModule::PostPhysicsUpdate @0x827D3BB8 is
// REAL in GameSource/World/CrashModule/BrnCrashModule_RaceCarCrashes.cpp -- it runs
// ProcessCrashedRaceCarEvents, i.e. the crash ENTRY into the module.

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. the AI update (X360 vtbl+68).
// Reconstruct from X360 and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void BrnAI::AIModule::Update(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,struct BrnAI::AIModuleIO::InputBuffer const *,struct BrnAI::AIModuleIO::OutputBuffer *,unsigned short)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "AIModule::Update: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. the AI post-physics tick.
// Reconstruct from X360 and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void BrnAI::AIModule::PostPhysicsUpdate(struct BrnAI::AIModuleIO::InputBuffer_PostPhysics const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "AIModule::PostPhysicsUpdate: inert [FLAG PC boot gate]\n";
    }
}

// ⭐⭐ 2026-08-11 (lifetime wave): the PhysicsModule::UpdateCachedPositions @0x8259C370 boot gate
// that stood here since 2026-07-27 is DELETED. The real 34-instruction body is in
// BrnPhysicsModule.cpp, next to the module's other own-TU bodies.
// This one mattered far beyond its size: it is the ONLY writer of a triangle-cache slot's sphere
// CENTRE in the whole XEX, so while it was inert every claimed slot sat at the WORLD ORIGIN and
// the fill worker cached geometry from there. It lands in the same commit as the traction-line
// producer lifetime, because a car tested against triangles cached three kilometres away would be
// valid, green and wrong -- this project's signature failure.
// Arms 2 and 3 (PropManager:: / DeformationManager::UpdateTriangleCache) are named gates in
// BrnPhysicsConductorGates.cpp; arm 1 (VehicleManager, plus the traffic pool behind it) is real.

// ⭐⭐ 2026-08-10 (create-path wave): the PhysicsModule::PostSceneUpdate boot gate that stood here
// since 2026-07-27 is DELETED -- the real 278-insn body @0x825ABC10 is landed in
// BrnPhysicsModuleUpdateFunctions.cpp. It was the last link in the chain that made
// VehicleManager::ProcessVehicleMaintenanceEvents (and behind it the whole create path)
// unreachable: `xrefs_to` on that function is a one-element set naming only this stub.
// The four callees of PostSceneUpdate whose own closures are absent are each their own NAMED
// one-shot gate in BrnPhysicsConductorGates.cpp -- including
// BridgeVehicleManagerToSimulation_PostScene, which is HELD INERT DELIBERATELY because it is the
// only path from the vehicle manager's rigid-body request queues into the simulation.

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. the physics scene-query producer (@0x825A1428).
// Reconstruct from X360 and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void BrnPhysics::PhysicsModule::GenerateSceneQueries(class BrnPhysics::PhysicsModuleIO::OutputBuffer *,unsigned short)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "PhysicsModule::GenerateSceneQueries: inert [FLAG PC boot gate]\n";
    }
}

// ⭐⭐ 2026-08-09 (conductor wave): the PhysicsModule::Update boot gate that stood here for
// thirteen days is DELETED -- the real 1,999-insn body @0x825B0640 is landed in
// BrnPhysicsModuleUpdateFunctions.cpp, and the deferrals it still carries are each their own
// NAMED one-shot gate in BrnPhysicsConductorGates.cpp.

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. the network catch-up step WorldModule::UpdatePhysicsNetworkCatchup @0x827B06E0 forwards to.
// Reconstruct from X360 and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void BrnPhysics::PhysicsModule::UpdateNetworkCatchup(class BrnPhysics::PhysicsModuleIO::InputBuffer const *,unsigned short)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "PhysicsModule::UpdateNetworkCatchup: inert [FLAG PC boot gate]\n";
    }
}

// TOMBSTONE (post-fx step 9, 2026-08-16, group envblend): the EnvironmentManager::Update boot
// gate that stood here since the world-drive wave is DELETED -- the real body @0x827D6060 is
// landed in GameSource/World/EnvironmentManager/BrnEnvironmentManager.cpp (which is on
// build_game_exe.bat), together with SetupBlend @0x827D4FE8, SetupTimeOfDayBlend @0x827D35C0
// and SetupSeasonsBlend @0x827D37A0. Leaving both would be LNK2005.
// ORDERING: this deletion is only correct once group `envstream` has landed StreamOut /
// StreamIn / Prepare and group `envdata` the TimeLine/Dictionary relayout -- the real Update
// dereferences maSeasonPtrs[miCurrSeason], which only Prepare/StreamIn populate.

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. the world debug-menu pump (@0x827BF818).
// Reconstruct from X360 and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void BrnWorld::WorldDebugComponent::Update(struct BrnWorldIO::DebugController const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldDebugComponent::Update: inert [FLAG PC boot gate]\n";
    }
}

// (BrnGraphics::EnvironmentMap::Update gate NOT needed: the real body already lives
//  in its own TU, GameSource/World/EnvironmentMap/BrnEnvironmentMap.cpp, which is
//  on the build list -- WorldModule::Update's env-map refresh is REAL.)

// (GATE RETIRED 2026-08-10, spatial-partition wave: SceneManagerModule::StartUpdateTriangleCache
//  @0x828C73D8 (73) now has its REAL body in GameShared/GameClasses/SceneManager/
//  CgsSceneManagerModule.cpp, next to its End partner @0x828C7500. It is REACHED EVERY FRAME
//  from WorldModule::Update (BrnWorldModule.cpp:2446), so the real body runs from the frame it
//  lands. Six of its seven callees were already bodied; the one that was not --
//  PolygonSoupListSpatialMap::BuildSpacialPartition @0x82841740 (2,255) -- landed this wave,
//  which is the whole reason this gate could go. LNK2005 is the tripwire if it is restored.)



// ===========================================================================
// WORLD-DRIVE BOOT GATES, part 2 (2026-07-27) -- the link closure of the real
// WorldModule::Update @0x827D63E8.
// ===========================================================================

// ---- module-IO buffer Construct() ------------------------------------------
// WorldModule::Update creates ~30 module IO buffers per frame on the update
// stacks; CgsModule::IOBufferStack::CreateIOBuffer<T> runs T::Construct right
// after the stack alloc, exactly as the console does. For the buffers below the
// owning IO TU has no Construct body yet -- these gates run ONLY the IOBuffer
// base bring-up (raising the status the Lock/Unlock tripwires assert on). The
// member queue / interface bring-up each real Construct also performs is
// deferred with that IO TU; every one of these buffers belongs to a module whose
// update is itself boot-gated, so nothing writes into the un-constructed members.
// Replace each with the real T::Construct in its own IO TU (and delete here).
//
// FLAG PC (2026-08-15, IO-buffer zero-fill removal): CreateIOBuffer<T> used to
// VALUE-initialise (`new (p) T()`), i.e. zero-fill every buffer, which is what put
// ~18-20 MB of memset per frame into WorldModule::Update / PhysicsModule::Update.
// The template is now DEFAULT-init + Construct, matching every X360 instantiation.
// For the gates below that is a behaviour change these partial buffers were leaning
// on: their un-reconstructed members used to arrive zeroed and now arrive holding the
// previous stack tenant's bytes. Since their real Construct bodies are NOT recovered
// (the X360 instantiation only shows the out-of-line `T::Construct()` call, address
// noted per gate), each gate zeroes ITS OWN buffer -- the per-T escape hatch, never
// the template. Total ~296 KB/frame vs the ~18-20 MB the blanket zeroing cost.
// DELETE the memset together with the gate when the real Construct lands.

// (BrnAI::AIModuleIO::InputBuffer_PostPhysics::Construct used to be a memset gate here. Its REAL
//  body -- and the Destruct the console DestroyIOBuffer<T> reaches -- were already reconstructed in
//  BrnAIModuleIO_InputBuffer_PostPhysics.cpp; that TU is on the bat since 2026-08-15.)

// BOOT GATE: base bring-up only (see the block note above).
// X360 CreateIOBuffer<OutputBuffer_PostScene> @0x827B69F0 (Alloc 63440).
void BrnTraffic::BrnTrafficIO::OutputBuffer_PostScene::Construct()
{
    memset(this, 0, sizeof(*this));   // FLAG PC: stands in for the unrecovered body
    CgsModule::IOBuffer::Construct();
}
// GATE RETIRED 2026-08-18 (breakable-props wave Q mount): BrnWorld::PropEntityIO::InputBuffer_PostPhysics::Construct @0x822EFDC8 (BrnPropEntityModuleIO_InputBuffer_PostPhysics.cpp; the memset gate left its queues un-Constructed) is now REAL.
// GATE RETIRED 2026-08-18 (breakable-props wave Q mount): BrnWorld::PropEntityIO::InputBuffer_PrePhysics::Construct @0x822EFD68 (BrnPropEntityModuleIO_InputBuffer_PrePhysics.cpp) is now REAL.

// (BrnWorld::RaceCarEntityModuleIO::InputBuffer_PostPhysics::Construct gate RETIRED
//  2026-08-11, physics->output publish wave: the real partial slice now lives in
//  BrnRaceCarEntityModuleIO.cpp from X360 0x822EA838. The base-only gate left
//  mVehicleOutputInterface and mVehicleManagerOutputInterface un-Constructed, and the moment
//  BridgePhysicsModuleToRaceCarModule_PostPhysics went live their operator= Appended into
//  never-Constructed queues -- a "Base event queue overflow" assert followed by an AV writing
//  through a null mpEvents inside memcpy, on the first boot run. Same family as the
//  PhysicsModuleIO::OutputBuffer::Construct root cause. Leaving both = LNK2005.)

// GATE RETIRED 2026-08-25 (crash exit): BrnWorld::RaceCarEntityModuleIO::InputBuffer_PostScene::
// Construct @0x822EA5E8 is REAL in BrnRaceCarEntityModuleIO.cpp.
// ⭐⭐ THE STUB IT REPLACES WAS `memset(this, 0, sizeof(*this))`, and the block note directly
// above already described this exact failure for the PostPhysics sibling -- "the base-only gate
// left ... un-Constructed, and the moment [the bridge] went live their operator= Appended into
// never-Constructed queues -- a 'Base event queue overflow' assert followed by an AV writing
// through a null mpEvents inside memcpy". It happened again here, verbatim, the first frame the
// crash module posted a real RaceCarCrashCompleteEvent, because a memset leaves an EventQueue's
// mpEvents NULL: zeroing a queue is not constructing it.
// ⚠️ A memset stub is WORSE than no stub for this: it looks like initialisation and it silences
// nothing, so the defect waits for the day a producer lands.

// (BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics::Construct gate RETIRED
//  2026-07-27: the real partial slice now lives in BrnRaceCarEntityModuleIO.h from
//  X360 0x822EA6F0 -- the base-only gate left mSceneResultQueue un-Constructed and
//  the scene->race-car pre-physics bridge Appends into it every frame.)

// (BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics::GetSceneResultQueue and
//  InputBuffer_GenerateDispatchLists::GetSceneResultQueue stubs RETIRED 2026-07-27:
//  both classes carry the committed mSceneResultQueue member, so the accessors are
//  real inlines in BrnRaceCarEntityModuleIO.h. The stubs returned NULL, which the
//  scene->race-car bridge then dereferenced.)

// (BrnWorld::RaceCarEntityModuleIO::InputBuffer_PreScene::Construct gate RETIRED
//  2026-07-31: real partial slice in BrnRaceCarEntityModuleIO.h from X360 0x822EA3C0 --
//  the base-only gate left mTimerStatusInterface / mGameActionQueue /
//  mAudioCarLoadedDataQueue un-Constructed; the race-car streamer pump reads two of them
//  every frame.)

// (BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostPhysics::Construct gate RETIRED
//  2026-07-31: the real partial slice now lives in BrnRaceCarEntityModuleIO.h from
//  X360 0x822EA8F8 -- the base-only gate left mResourceRequestInterface.mRequestQueue
//  un-Constructed, and RaceCarEntityModule::SendStreamerEvents Appends into it every
//  frame, which fired a "Not Constructed" assert per frame.)

// BOOT GATE: base bring-up only (see the block note above).
// X360 CreateIOBuffer<OutputBuffer_PostScene> @0x827B6AC8 (Alloc 36592).
void BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostScene::Construct()
{
    memset(this, 0, sizeof(*this));   // FLAG PC: stands in for the unrecovered body
    CgsModule::IOBuffer::Construct();
}

// (BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PrePhysics::Construct gate RETIRED
//  2026-08-01, drivable wave: the real partial slice lives in BrnRaceCarEntityModuleIO.h
//  from X360 0x822EA7B0. The base-only gate left mVehicleInputInterface's FIFTEEN embedded
//  EventQueues with mpEvents == NULL, and the first car to reach ResetActiveRaceCar ->
//  AddHandlingModel -> VehicleInputInterface::CreateRaceCar fired
//  "mpEvents != NULL" + "EventQueue::AddEvent - Reached Max length" and killed the process.
//  MEASURED, DRV_RUN1.)

// (BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene::Construct gate RETIRED
//  2026-07-31: real partial slice in BrnRaceCarEntityModuleIO.h from X360 0x822EA4E0 --
//  mAudioCarLoadedDataQueue is written by RaceCarAudioStreamer::Update every frame.)

// (BrnWorld::TriggerEntityModuleIO::OutputBuffer_PreScene::Construct gate RETIRED
//  2026-07-27: the REAL body (X360 0x822EED90) already lived in the owning TU
//  BrnTriggerEntityModuleIO.cpp, which is now on the build list -- together with the
//  five sibling trigger-buffer Constructs recovered in the same pass.)


// ---- the collision generator the frame carves -------------------------------
// (BOTH GATES RETIRED 2026-08-10, cache-fill wave.) WorldModule::Update carves ONE
// BaseCollisionGenerator (object + a 0x40000 result region) out of the world frame
// allocator each frame and hands it to SceneManagerModule::StartUpdateTriangleCache.
// Its REAL bodies -- Construct @0x828105F8 and Prepare @0x82810660 -- had been fully
// reconstructed since 2026-08-06 in
// GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.cpp;
// that TU is simply not mounted, so the generator was carved and left UNINITIALISED
// behind these two one-shot logs. The TU is now on the build list and both stubs are
// deleted: the frame's generator is really constructed (the shared "StartJobs" perfmon
// latch) and really prepared (bump allocator over the result region + all 64 collision
// batches placement-constructed). LNK2005 is the tripwire if either stub is restored.

// ---- two read accessors the drive reads through ------------------------------
// BrnWorldIO::UpdateInputBuffer::GetPlayerVehicleControls (X360 read-lock, the
// controls block Update copies straight into the update output) and the traffic
// pre-scene output's traffic->race-car interface (the 544-byte block Update
// snapshots for the post-scene spine). Both belong to their own IO TUs; gated
// here as read-lock-checked null returns -- the drive tolerates null on both
// paths (the copy and the snapshot are skipped) because the producing modules
// are boot-gated.
// GetPlayerVehicleControls RETIRED (driving-input wave 2026-08-11): the real
// read-lock accessor is homed beside its writer in BrnWorldModuleIO.cpp. The
// null-return gate here is what kept the per-frame controls copy skipped and
// the player car deaf to input once the world went live.

// ⛔ THIRD SILENT-DROP STUB RETIRED 2026-08-10 (same mount, same mechanism as the two
//    OutputBuffer_Prepare accessors above): `OutputBuffer_PreScene::
//    GetTrafficToRaceCarInterface_PreScene() const` has a real body in
//    BrnTrafficEntityModuleIO.cpp; the `return 0` copy here is what linked while that TU was
//    off the build list. LNK2005 on mounting.

// ---- the six unmounted sibling bridge TUs' entry points ----------------------
// Each of these has a REAL committed body in its own home TU; those TUs are not
// on the build list because each drags declaration-only module-IO accessors (see
// the bat note next to the world-fleet block). Gated here so the real drive links
// TODAY; delete each gate when its home TU is mounted.

// ⛔⛔ STUB RETIRED 2026-08-11 (driving-input wave). The real body @0x827ADF88 is now
// MOUNTED, out of GameSource/World/Bridges/WorldBridgeInputToEntityModules.cpp.
// This inert copy was the ONLY definition in the link, and it sat on the LAST hop of the
// player-input path: BridgeInputToEntityModules is the only caller in the image of
// RaceCarEntityModuleIO::InputBuffer_PreScene::SetPlayerVehicleControls, so with this gate
// linked the freshly-wired keyboard/pad controls reached the world's UpdateInputBuffer and
// stopped dead there -- the race-car module's own pre-scene buffer never saw a control
// frame. Nothing else in the image runs these hand-offs either, so the whole
// per-active-race-car latch set (colour, paint finish, lost/regained contact,
// car-select) never reached the race-car buffer, nor did the camera hand-off, the
// takedown / scoring / online-scoring feeds, the trigger add+remove and query queues,
// the world-entity request interface, or the prop game-action fan-out.
// What it took to retire: the 21 declaration-only IO accessors it referenced (4 world
// UpdateInputBuffer const getters, 15 RaceCarEntityModuleIO PreScene/PrePhysics setters,
// the prop replay-status setter, and the trigger pre-scene GetInputInterface, whose home
// TU was simply never on the build list) -- see the wave notes in BrnWorldModuleIO.cpp,
// BrnRaceCarEntityModuleIO.h/.cpp and BrnPropEntityModuleIO_InputBuffer_PreScene.cpp.

// GATE RETIRED 2026-08-19 (wave Q5 round 3 / F2): BridgePhysicsSceneUpdateToScene @0x827ABA40 is REAL in WorldBridgePhysicsToScene.cpp (mounted this commit).


// ⛔⛔ STUB RETIRED 2026-08-01 (car-select hand-off wave). The real body @0x827A52B0 is now
// MOUNTED, out of GameSource/World/Bridges/WorldBridgeRaceCarToWorldModule.cpp.
// This inert copy was the ONLY definition in the link and it was the ONLY producer of
// WorldModule::meLocalPlayerActiveRaceCarIndex -- so that index stayed at Construct's -1 all
// session and HandleGameActions case 7 could never put the player car under the control mode
// the junkyard asked for. Its own comment ("real body in its own home TU (not mounted: IO
// accessor closure)") was accurate AND is exactly why nobody looked: it said the body existed,
// not that nothing was running it.

// BOOT GATE -- real body @0x827AD788 in its own home TU (not mounted: IO accessor closure).
void WorldModule::BridgeTrafficToTrigger_PreScene(void *,class BrnWorld::TriggerEntityModuleIO::InputBuffer_PreScene *,class BrnTraffic::BrnTrafficIO::OutputBuffer_PreScene const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeTrafficToTrigger_PreScene: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE -- real body @0x827A51F0 in its own home TU (not mounted: IO accessor closure).
void WorldModule::BridgeTrafficToRaceCar_PrePhysics(void *,struct BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics *,struct BrnTraffic::BrnTrafficIO::OutputBuffer_PostScene const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeTrafficToRaceCar_PrePhysics: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE -- real body @0x827A4FA0 in its own home TU (not mounted: IO accessor closure).
void WorldModule::BridgeRaceCarModuleToAIModule_PreScene(void *,struct BrnAI::AIModuleIO::InputBuffer *,struct BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeRaceCarModuleToAIModule_PreScene: inert [FLAG PC boot gate]\n";
    }
}

// GATE RETIRED 2026-08-25 (crash exit): WorldModule::BridgeEntityModulesToCrashModule_PreScene
// @0x827A5060 is REAL in GameSource/World/Bridges/WorldBridgeCrashInputs.cpp.

// GATE RETIRED 2026-08-25 (crash exit): WorldModule::BridgeCrashModuleToPhysicsModule @0x827AAC70
// is REAL in GameSource/World/Bridges/WorldBridgeCrashInputs.cpp (a documented one-shot park
// inside a real body -- its source member is still a 1-byte placeholder).

// ---------------------------------------------------------------------------
// CgsResource::ShaderTechniqueResourceType -- the one member its own TU still
// documents as DEFERRED:
//   GetShaderConstantExternalSerialisedResourceDescriptorSize -- its private
//   descriptor-size helper (serialiser path only; no runtime caller).
// The type is REGISTERED (world-render resource types, 2026-07-27), so the
// vtable is emitted and the linker needs the symbol.
//
// PostFixUp @0x827EEBF0's gate is GONE (shading wave 2026-07-28): the function is
// reconstructed in its home TU CgsShaderTechniqueResourceType.cpp, on top of the
// now-real ShaderConstantsExternal::FixUp(const ProgramBuffer*) (X360 sub_827ED8D0)
// in CgsShaderConstants.cpp.
// ---------------------------------------------------------------------------
uint32_t CgsResource::ShaderTechniqueResourceType::GetShaderConstantExternalSerialisedResourceDescriptorSize(
    const ShaderConstantsExternal* /*lpBlock*/) const
{
    CGS_ASSERT(false, "ShaderTechniqueResourceType::GetShaderConstantExternalSerialisedResourceDescriptorSize: documented deferral -- reconstruct");
    return 0;
}

// ---------------------------------------------------------------------------
// BrnWorld::EnvironmentSettings -- the three environment sub-object Constructs
// that the REAL EnvironmentManager::Construct @0x827CA408 (sky wave) calls.
// Their own bodies belong to the environment-data TUs, which are not yet
// reconstructed; the manager's Construct zero-seeds the aggregates it owns, so
// a quiet no-op here leaves them in the zeroed state the manager already put
// them in rather than faulting the boot.
// DELETE-WHEN: the environment-data TUs (Scattering/Lighting/Clouds) land.
// ---------------------------------------------------------------------------
// (ScatteringData / LightingData / CloudsData ::Construct RETIRED 2026-08-16,
//  post-fx step 9 envfix round -- the DELETE-WHEN above is met: the three
//  environment-data TUs are mounted on tools/build/build_game_exe.bat, with the
//  static default template vectors they read (KAF_*) now defined in them.
//    ScatteringData::Construct @0x82674FD0 -> .../BrnEnvScatteringData.cpp
//    LightingData::Construct   @0x826750E0 -> .../BrnEnvLightingData.cpp
//    CloudsData::Construct     @0x82675190 -> .../BrnEnvCloudsData.cpp)

// ============================================================================
// PropEntityDebugComponent -- the FOUR out-of-line virtuals + Construct.
// (prop-spawn wave, 2026-08-12)
//
// PropEntityDebugComponent is embedded BY VALUE in PropEntityModule (console +0xCD900),
// so PropEntityModule::Construct emits the component's vtable and the linker demands
// every out-of-line virtual -- whether or not the debug menu is ever opened.
//
// Its real TU EXISTS and is fully reconstructed, but mounting it MEASURABLY made things
// worse: RenderWorld/RenderHUD pull in the whole un-reconstructed CgsDev debug-render
// stack (Debug3DImmediateRender::DrawBox / DrawSphere / DrawHollowSphere / DrawSolidBox /
// DrawText, Debug2DImmediateRender::DrawCircle, rw::RGBA's ctor, Volume::GetRelativeTransform,
// plus the component's own ToCellGridScreenCoords). Mounting it took the unresolved count
// UP, not down.
//
// So the vtable is served here instead, exactly as the 2026-08-11 baseline wave did for the
// same class of embedded debug component: GetName is REAL (it is a one-line string return
// and the debug menu keys on it), the rest are inert. This is a DEV-MENU-ONLY surface and
// cannot affect whether props spawn or render.
//
// RETIRE ALL FIVE and mount BrnPropEntityDebugComponent.cpp once the debug-render stack
// lands -- the bodies are already written and waiting.
// ============================================================================
const char* BrnWorld::PropEntityDebugComponent::GetName() const
{
    // REAL -- @0x822A9740 returns this literal.
    return "Prop Entity Module";
}

void BrnWorld::PropEntityDebugComponent::Construct(class BrnWorld::PropEntityModule*)
{
    // @0x822A96A8. Inert: the component's own state is only read by the render passes
    // below, which are themselves inert here. PropEntityModule::Construct calls this at
    // the console's call position, so the call itself is faithful.
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "PropEntityDebugComponent::Construct: inert [FLAG PC boot gate]\n";
    }
}

void BrnWorld::PropEntityDebugComponent::OnActivate()
{
    // @0x822C52C8 -- registers the render toggles / cell-grid zoom / override tuning and
    // the "Reset props" action with the debug menu. Inert: CgsDebugManager exposes no
    // RegisterVariable/SetLimits on this build.
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "PropEntityDebugComponent::OnActivate: inert [FLAG PC boot gate]\n";
    }
}

void BrnWorld::PropEntityDebugComponent::RenderWorld(CgsDev::Debug3DImmediateRender*)
{
    // @0x822EFEB8 -- 3D debug pass (volumes, prop stats, inertia boxes). Inert.
}

void BrnWorld::PropEntityDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender*)
{
    // @0x822FC108 -- 2D debug pass (module stats + cell grid). Inert.
}

// ============================================================================
// PROP-SPAWN WAVE, LINK-CLOSURE GATES (2026-08-12, agent B7)
// ----------------------------------------------------------------------------
// Everything else in this wave's unresolved set was BODIED in its real home. These nine
// are the deliberate exceptions: two families that are out of scope for "make props spawn
// and render", each parked for a REAL reason rather than papered over.
//
// Neither family can affect whether a prop appears in the world:
//   * contact generation is what makes a prop SMASHABLE, and it is reached only from
//     PropCellManager::ActivateCell / the state-change path -- a prop is added to the
//     SCENE (which is what renders it) by AddPropToScene / AddPropPartsToScene, both of
//     which are real bodies and neither of which calls these.
//   * the PropSerialiserFrame delta half is replay record/playback only; the load path
//     asks the serialiser IsPlaying() and takes the not-playing branch in normal play.
//
// ⚠️ NONE of these is silently wrong: each logs once through the boot gate, states what
// the real body would do, carries its X360 address, and names exactly what unparks it.
// ============================================================================

// ---- (a) PROP CONTACT GENERATION x4 -- GATES RETIRED 2026-08-18 (wave Q4): PropCellManager::
//      AddPropToContactGeneration @0x822DF6C8 / AddPropPartsToContactGeneration @0x822DF9D8 /
//      RemovePropFromContactGeneration @0x822C6318 / RemovePropPartsFromContactGeneration @0x822C6430
//      are REAL in BrnPropCellManager_wQ4.cpp; the 64-bit VolumeInstanceId overloads of
//      InSceneUpdateInterface::AddForCollision/AddVolumeInstance they need are landed.

// ---- (b) PROP REPLAY DELTA SERIALISATION x4 ---------------------------------
// The record/playback half of BrnReplays::PropSerialiserFrame. Referenced by
// PropEntitySerialiser::Read/Write, which the prop load path reaches only through
// GetStaticLayout() -- and every gameplay caller takes the IsPlaying()==false branch, so
// none of these runs while props are simply spawning and rendering.
//
// These are parked as OUT OF SCOPE, not blocked: the frame interior is a ~15 KB record
// whose per-cell / per-prop / per-part arrays are still modelled as padding runs
// (BrnReplayPropSerialiserFrame.h is one long maPadNNNN[] ladder with a handful of named
// flags). Writing a delta codec against padding would be fabrication; the bodies are
// substantial (Read 135 insns @0x82653120, Write 156 @0x82657FE0, KeyFrameRead 295
// @0x826586B0, KeyFrameWrite unnamed in IDA) and each walks members that do not exist by
// name yet.
//
// UNPARKED BY: reconstructing the PropSerialiserFrame interior (the padding ladder ->
// named per-cell/per-prop/per-part arrays). Then all four decode straightforwardly, since
// the sibling WriteProp @0x822BB528 / WritePart @0x822BB718 already show the idiom.
void BrnReplays::PropSerialiserFrame::Read(BrnReplays::BaseSerialiser*)
{
    // @0x82653120 (135 insns). WOULD: delta-read one prop frame out of the replay stream.
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "PropSerialiserFrame::Read: inert -- frame interior still "
                                          "padding-modelled; replay only [FLAG PC boot gate]\n";
    }
}

void BrnReplays::PropSerialiserFrame::KeyFrameRead(BrnReplays::BaseSerialiser*)
{
    // @0x826586B0 (295 insns). WOULD: read a full (non-delta) prop key frame.
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "PropSerialiserFrame::KeyFrameRead: inert -- frame interior still "
                                          "padding-modelled; replay only [FLAG PC boot gate]\n";
    }
}

void BrnReplays::PropSerialiserFrame::Write(BrnReplays::BaseSerialiser*,
                                            BrnReplays::PropSerialiserFrame*)
{
    // @0x82657FE0 (156 insns). WOULD: delta-write this frame against the static layout.
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "PropSerialiserFrame::Write: inert -- frame interior still "
                                          "padding-modelled; replay only [FLAG PC boot gate]\n";
    }
}

void BrnReplays::PropSerialiserFrame::KeyFrameWrite(BrnReplays::BaseSerialiser*,
                                                    BrnReplays::PropSerialiserFrame*)
{
    // X360 body exists but is UNNAMED in the IDA export (its siblings Read/Write/KeyFrameRead
    // are named; this one is only reachable as the fourth call out of
    // PropEntitySerialiser::Write). WOULD: write a full prop key frame.
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "PropSerialiserFrame::KeyFrameWrite: inert -- frame interior still "
                                          "padding-modelled; replay only [FLAG PC boot gate]\n";
    }
}
