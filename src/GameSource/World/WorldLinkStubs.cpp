// ===========================================================================
// WorldLinkStubs.cpp -- FLAG (world-fleet link-mount stubs, 2026-07-26).
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
ShaderConstantTable::ShaderConstantTable()
{
}

// ---------------------------------------------------------------------------
// rw::math::vpu::Inverse @ X360 0x825B2628 -- 4x4 inverse + determinant out.
// Declared TU-locally by its callers (BrnShadowMap.cpp); no shared header, so
// the definition is namespace-wrapped to self-declare with the same mangling.
// ---------------------------------------------------------------------------
namespace rw { namespace math { namespace vpu {
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
Matrix44 Inverse(const Matrix44&, Vector4&)
{
    CGS_ASSERT(false, "rw::math::vpu::Inverse: link stub (world fleet mount) -- reconstruct from X360");
    return Matrix44();
}
} } }










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
// BOOT-GATE (world-module mount 2026-07-26): REACHED at boot by the wired
// WorldModule::Construct @0x827CF540 fleet cascade; quiet no-op -- the member
// stays inert/unprepared (zero-initialised static storage), which the boot
// path tolerates. Reconstruct from X360 before wiring the world Prepare.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
void BrnAI::AIModule::Construct()
{
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnAI::AIModule::Destruct()
{
    CGS_ASSERT(false, "AIModule::Destruct: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnAI::AIModule::Prepare(class BrnResource::GameDataIO::AllocatorList *,struct BrnAI::AIModuleIO::OutputBuffer *)
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
            *CgsDev::Log::gpDebugPrint << "AIModule::Prepare: inert [FLAG PC boot gate]\n";
    }
    return true;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnAI::AIModule::Release()
{
    CGS_ASSERT(false, "AIModule::Release: link stub (world fleet mount) -- reconstruct from X360");
    return false;
}

// -------------------------------------------------------------------------
// BrnAI::AIModuleIO::OutputBuffer
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
struct BrnAI::AIModuleIO::AICarOutputInterface const * BrnAI::AIModuleIO::OutputBuffer::GetAICarOutputInterfaceConst() const
{
    CGS_ASSERT(false, "GetAICarOutputInterfaceConst: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
struct BrnResource::GameDataIO::RequestInterface<4096> const * BrnAI::AIModuleIO::OutputBuffer::GetAIResourceRequestInterface() const
{
    CGS_ASSERT(false, "GetAIResourceRequestInterface: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
class CgsModule::VariableEventQueue<1536,16> const * BrnAI::AIModuleIO::OutputBuffer::GetGameEventQueueConst() const
{
    CGS_ASSERT(false, "GetGameEventQueueConst: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
class CgsModule::EventQueue<struct BrnAI::RouteMapModuleIO::RouteResponse,16> const * BrnAI::AIModuleIO::OutputBuffer::GetRouteResponseQueue() const
{
    CGS_ASSERT(false, "GetRouteResponseQueue: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

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
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnDirector::HookNameStringWrapper::Set(char const *)
{
    CGS_ASSERT(false, "HookNameStringWrapper::Set: link stub (world fleet mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// BrnGame::DispatchThreadInputBuffer
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnGame::DispatchThreadInputBuffer::SetCameraViewProjection(struct rw::math::vpu::Matrix44 const &)
{
    CGS_ASSERT(false, "DispatchThreadInputBuffer::SetCameraViewProjection: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnGame::DispatchThreadInputBuffer::SetEnvMapFaceRendered(int,bool)
{
    CGS_ASSERT(false, "DispatchThreadInputBuffer::SetEnvMapFaceRendered: link stub (world fleet mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// BrnGraphics::EnvironmentMap
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnGraphics::EnvironmentMap::Prepare()
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
            *CgsDev::Log::gpDebugPrint << "EnvironmentMap::Prepare: inert [FLAG PC boot gate]\n";
    }
    return true;
}

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

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
int BrnMassive::BrnMassive::Destruct()
{
    CGS_ASSERT(false, "BrnMassive::Destruct: link stub (world fleet mount) -- reconstruct from X360");
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
void MassiveAdClient3::CMassiveAdObjectSubscriber::operator delete(void *)
{
    CGS_ASSERT(false, "CMassiveAdObjectSubscriber::operator delete: link stub (world-module mount) -- MassiveAd heap not linked");
}

// -------------------------------------------------------------------------
// BrnPhysics::PhysicsModule
// -------------------------------------------------------------------------
// BOOT-GATE (world-module mount 2026-07-26): REACHED at boot by the wired
// WorldModule::Construct @0x827CF540 fleet cascade; quiet no-op (see
// AIModule::Construct above). Reconstruct from X360 before wiring Prepare.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
void BrnPhysics::PhysicsModule::Construct()
{
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnPhysics::PhysicsModule::Prepare(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,struct CgsSceneManager::SceneManagerIO::InputBuffer_Update *,class BrnResource::GameDataIO::AllocatorList *)
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
            *CgsDev::Log::gpDebugPrint << "PhysicsModule::Prepare: inert [FLAG PC boot gate]\n";
    }
    return true;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnPhysics::PhysicsModule::PropPrepareTypes(class BrnPhysics::PhysicsModuleIO::InputBuffer *)
{
    // BOOT-GATE (attribsys wave 2026-07-26): REACHED by the prop stage tail of
    // WorldModule::Prepare. The physics module is boot-gated inert; quiet no-op.
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "PhysicsModule::PropPrepareTypes: inert [FLAG PC boot gate]\n";
    }
}

// (PhysicsModule::UpdateNetworkCatchup(int,int) stub RETIRED 2026-07-27: the
// signature was a decompiler misread -- the world drive passes the physics INPUT
// buffer + the frame update set. The retyped gate lives in the world-drive block
// at the end of this file.)

// -------------------------------------------------------------------------
// BrnPhysics::Props::PropInputInterface
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnPhysics::Props::PropInputInterface::Append(struct BrnPhysics::Props::PropInputInterface const &)
{
    // BOOT-GATE (attribsys wave 2026-07-26): REACHED by the prop->physics prepare
    // bridge (WorldModule::Prepare prop stage). The physics module is boot-gated
    // inert, so the staged prop-type merge is dropped consistently. One-shot log.
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "PropInputInterface::Append: inert [FLAG PC boot gate]\n";
    }
}

// -------------------------------------------------------------------------
// BrnPhysics::Vehicle::VehicleManager
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnPhysics::Vehicle::VehicleManager::ReadSurfaceProperties()
{
    // BOOT-GATE (attribsys wave 2026-07-26): REACHED right after the WORLDENTITY
    // prepare stage (WorldModule::Prepare @0x827D53B0 tail). Reads the surface
    // attributes out of the LIVE Attrib database -- gated with the schema/DB
    // cluster (see PrepareSurfaceList's gate). One-shot log + no-op.
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "VehicleManager::ReadSurfaceProperties: inert [FLAG PC boot gate]\n";
    }
}

// -------------------------------------------------------------------------
// BrnReplays::ReplayIO::RequestInterface
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnReplays::ReplayIO::RequestInterface::Append(struct BrnReplays::ReplayIO::RequestInterface const *)
{
    CGS_ASSERT(false, "RequestInterface::Append: link stub (world fleet mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// BrnSound::Module::Io::SoundWorldLoadEvent
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnSound::Module::Io::SoundWorldLoadEvent::Construct(enum BrnSound::Module::Io::SoundWorldLoadEvent::eLoadEvent,unsigned short)
{
    CGS_ASSERT(false, "SoundWorldLoadEvent::Construct: link stub (world fleet mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// BrnTraffic::BrnTrafficIO::InputBuffer_Dispatch
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
class CgsModule::VariableEventQueue<32768,16> * BrnTraffic::BrnTrafficIO::InputBuffer_Dispatch::GetSceneResultQueue()
{
    CGS_ASSERT(false, "InputBuffer_Dispatch::GetSceneResultQueue: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

// -------------------------------------------------------------------------
// BrnTraffic::BrnTrafficIO::InputBuffer_PreDispatch
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnTraffic::BrnTrafficIO::InputBuffer_PreDispatch::Construct()
{
    CGS_ASSERT(false, "InputBuffer_PreDispatch::Construct: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnTraffic::BrnTrafficIO::InputBuffer_PreDispatch::SetCameraPosition(struct rw::math::vpu::Vector3)
{
    CGS_ASSERT(false, "InputBuffer_PreDispatch::SetCameraPosition: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnTraffic::BrnTrafficIO::InputBuffer_PreDispatch::SetVisibleEntities(class Array<class CgsSceneManager::EntityId,650> const &)
{
    CGS_ASSERT(false, "InputBuffer_PreDispatch::SetVisibleEntities: link stub (world fleet mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// BrnTraffic::BrnTrafficIO::OutputBuffer_PreDispatch
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnTraffic::BrnTrafficIO::OutputBuffer_PreDispatch::Construct()
{
    CGS_ASSERT(false, "OutputBuffer_PreDispatch::Construct: link stub (world fleet mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// BrnTraffic::BrnTrafficIO::OutputBuffer_Prepare
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
struct BrnResource::GameDataIO::RequestInterface<4096> const * BrnTraffic::BrnTrafficIO::OutputBuffer_Prepare::GetResourceRequestInterface() const
{
    CGS_ASSERT(false, "GetResourceRequestInterface: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
struct BrnTraffic::BrnTrafficIO::OutputBuffer_Prepare::SceneInputInterface const * BrnTraffic::BrnTrafficIO::OutputBuffer_Prepare::GetSceneInputInterface() const
{
    CGS_ASSERT(false, "GetSceneInputInterface: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

// -------------------------------------------------------------------------
// BrnTraffic::TrafficEntityModule
// -------------------------------------------------------------------------
// BOOT-GATE (world-module mount 2026-07-26): REACHED at boot by the wired
// WorldModule::Construct @0x827CF540 fleet cascade; quiet no-op (see
// AIModule::Construct above). Reconstruct from X360 before wiring Prepare.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
void BrnTraffic::TrafficEntityModule::Construct()
{
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnTraffic::TrafficEntityModule::Destruct()
{
    CGS_ASSERT(false, "TrafficEntityModule::Destruct: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnTraffic::TrafficEntityModule::EnterTearingDownState()
{
    CGS_ASSERT(false, "TrafficEntityModule::EnterTearingDownState: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnTraffic::TrafficEntityModule::GenerateDispatchLists(class BrnTraffic::BrnTrafficIO::InputBuffer_Dispatch *,class BrnTraffic::BrnTrafficIO::OutputBuffer_PreDispatch *,int,int,int,struct BrnDirector::Camera::Camera const *)
{
    CGS_ASSERT(false, "TrafficEntityModule::GenerateDispatchLists: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnTraffic::TrafficEntityModule::PostSceneUpdate(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,class BrnTraffic::BrnTrafficIO::InputBuffer_PostScene *,struct BrnTraffic::BrnTrafficIO::OutputBuffer_PostScene *,unsigned short)
{
    CGS_ASSERT(false, "TrafficEntityModule::PostSceneUpdate: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnTraffic::TrafficEntityModule::PreDispatchUpdate(class BrnTraffic::BrnTrafficIO::InputBuffer_PreDispatch *,class BrnTraffic::BrnTrafficIO::OutputBuffer_PreDispatch *)
{
    CGS_ASSERT(false, "TrafficEntityModule::PreDispatchUpdate: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnTraffic::TrafficEntityModule::PrePhysicsUpdate(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,class BrnTraffic::BrnTrafficIO::InputBuffer_PrePhysics *,class BrnTraffic::BrnTrafficIO::OutputBuffer_PrePhysics *,unsigned short)
{
    CGS_ASSERT(false, "TrafficEntityModule::PrePhysicsUpdate: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnTraffic::TrafficEntityModule::Prepare(class BrnTraffic::BrnTrafficIO::OutputBuffer_Prepare *)
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
            *CgsDev::Log::gpDebugPrint << "TrafficEntityModule::Prepare: inert [FLAG PC boot gate]\n";
    }
    return true;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnTraffic::TrafficEntityModule::Release()
{
    CGS_ASSERT(false, "TrafficEntityModule::Release: link stub (world fleet mount) -- reconstruct from X360");
    return false;
}

// -------------------------------------------------------------------------
// BrnWorld::EnvironmentSettings
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnWorld::EnvironmentSettings::ParseEnvironmentFile(float &,char (&)[4][256],float (&)[4],struct BrnEffects::BloomData &,struct BrnEffects::VignetteData &,char *,class BrnWorld::EnvironmentSettings::ScatteringData &,class BrnWorld::EnvironmentSettings::LightingData &,class BrnWorld::EnvironmentSettings::CloudsData &,char const *)
{
    CGS_ASSERT(false, "EnvironmentSettings::ParseEnvironmentFile: link stub (world fleet mount) -- reconstruct from X360");
    return false;
}

// -------------------------------------------------------------------------
// BrnWorld::EnvironmentSettings::CloudsData
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::EnvironmentSettings::CloudsData::SetToBlend(class BrnWorld::EnvironmentSettings::CloudsData const &,float,class BrnWorld::EnvironmentSettings::CloudsData const &,float,class BrnWorld::EnvironmentSettings::CloudsData const &,float,class BrnWorld::EnvironmentSettings::CloudsData const &,float)
{
    CGS_ASSERT(false, "CloudsData::SetToBlend: link stub (world fleet mount) -- reconstruct from X360");
}

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
void BrnWorld::EnvironmentSettings::DebugComponent::Update()
{
    CGS_ASSERT(false, "DebugComponent::Update: link stub (world-module mount) -- reconstruct from X360");
}

// LINK STUB (world-module mount 2026-07-26): body not reconstructed yet (X360 @0x827C79A0).
void BrnWorld::EnvironmentSettings::DebugComponent::RenderHUD(struct CgsDev::Debug2DImmediateRender *)
{
    CGS_ASSERT(false, "DebugComponent::RenderHUD: link stub (world-module mount) -- reconstruct from X360");
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
void BrnWorld::EnvironmentSettings::DebugComponent::OnActivate()
{
    CGS_ASSERT(false, "DebugComponent::OnActivate: link stub (world-module mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// BrnWorld::EnvironmentSettings::EnvironmentManager -- PARTIALLY DESTUBBED
// (2026-07-26 wave): BeginRelease (the WorldModule::Release stage-8 inline) now
// lives in BrnEnvironmentManager.cpp. The remaining six stay stubbed, each for
// a concrete dependency reason:
//   * Construct @0x827CA408 / Prepare @0x827D49A8: large staged resource
//     machines (colour-cube dictionary / keyframe bundle loads via the
//     GameData request queue, StrStream-formatted asserts, debug-variable
//     registration) over a member region (+0x700..+0x1240) the committed
//     class model has not homed yet.
//   * CalcKeyLightDirection @0x827B0638: forwards into BrnWorld::
//     EnvironmentSettings::ComputeKeyLightDirection @0x82678AB0, which is
//     built on XMMatrixRotationX/Y (xnamath helpers, not reconstructed) with
//     a decompiler-garbled register-lane matrix combine.
//   * Enable/DisableJunkyardLightingSetup @0x827B0F98/@0x827B10E8 and
//     GenerateEffects @0x827BE698: touch the un-homed +0x1220..+0x1C70 member
//     region (override-key-light vector, junkyard light table + count, the
//     time-of-day bounds) -- the committed class model ends at +0x11E8 and
//     needs a dedicated layout-growth pass first.
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
struct rw::math::vpu::Vector3 BrnWorld::EnvironmentSettings::EnvironmentManager::CalcKeyLightDirection() const
{
    CGS_ASSERT(false, "EnvironmentManager::CalcKeyLightDirection: link stub (world fleet mount) -- reconstruct from X360");
    return rw::math::vpu::Vector3();
}

// BOOT-GATE (world-module mount 2026-07-26): REACHED at boot by the wired
// WorldModule::Construct @0x827CF540 fleet cascade; quiet no-op (see
// AIModule::Construct above). Reconstruct from X360 before wiring Prepare.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
void BrnWorld::EnvironmentSettings::EnvironmentManager::Construct()
{
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::EnvironmentSettings::EnvironmentManager::DisableJunkyardLightingSetup()
{
    CGS_ASSERT(false, "EnvironmentManager::DisableJunkyardLightingSetup: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::EnvironmentSettings::EnvironmentManager::EnableJunkyardLightingSetup()
{
    CGS_ASSERT(false, "EnvironmentManager::EnableJunkyardLightingSetup: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::EnvironmentSettings::EnvironmentManager::GenerateEffects(class BrnEffectsFrame *,class BrnEffectsFrame *,class BrnEffectsFrame *,class BrnEffectsFrame *)
{
    CGS_ASSERT(false, "EnvironmentManager::GenerateEffects: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnWorld::EnvironmentSettings::EnvironmentManager::Prepare(struct BrnWorldIO::UpdateOutputBuffer *)
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
            *CgsDev::Log::gpDebugPrint << "EnvironmentManager::Prepare: inert [FLAG PC boot gate]\n";
    }
    return true;
}

// -------------------------------------------------------------------------
// BrnWorld::EnvironmentSettings::Keyframe
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::EnvironmentSettings::Keyframe::Construct()
{
    CGS_ASSERT(false, "Keyframe::Construct: link stub (world fleet mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// BrnWorld::EnvironmentSettings::LightingData
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::EnvironmentSettings::LightingData::SetToBlend(class BrnWorld::EnvironmentSettings::LightingData const &,float,class BrnWorld::EnvironmentSettings::LightingData const &,float,class BrnWorld::EnvironmentSettings::LightingData const &,float,class BrnWorld::EnvironmentSettings::LightingData const &,float)
{
    CGS_ASSERT(false, "LightingData::SetToBlend: link stub (world fleet mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// BrnWorld::EnvironmentSettings::ScatteringData
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::EnvironmentSettings::ScatteringData::SetToBlend(class BrnWorld::EnvironmentSettings::ScatteringData const &,float,class BrnWorld::EnvironmentSettings::ScatteringData const &,float,class BrnWorld::EnvironmentSettings::ScatteringData const &,float,class BrnWorld::EnvironmentSettings::ScatteringData const &,float)
{
    CGS_ASSERT(false, "ScatteringData::SetToBlend: link stub (world fleet mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// BrnWorld::InternalBaseStreamer
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnWorld::InternalBaseStreamer::AddEntry(unsigned __int64,bool,unsigned __int64)
{
    CGS_ASSERT(false, "InternalBaseStreamer::AddEntry: link stub (world fleet mount) -- reconstruct from X360");
    return false;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::InternalBaseStreamer::ClearTargetList()
{
    CGS_ASSERT(false, "InternalBaseStreamer::ClearTargetList: link stub (world fleet mount) -- reconstruct from X360");
}

// BOOT-GATE (world-module mount 2026-07-26): REACHED at boot via
// WorldEntityModule::Construct -> WorldGraphicsStreamer::Construct ->
// BaseStreamer<32>::Construct; quiet no-op -- the streamer stays raw, nothing
// streams until the world Prepare is wired. Reconstruct from X360 first.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
void BrnWorld::InternalBaseStreamer::Construct(class BrnWorld::StreamerTargetEntry *,class BrnWorld::StreamerTargetEntry *,class BrnWorld::StreamerCurrentEntry *,int,int,enum BrnResource::EAssetSet,bool)
{
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnWorld::InternalBaseStreamer::IsStreamComplete() const
{
    CGS_ASSERT(false, "IsStreamComplete: link stub (world fleet mount) -- reconstruct from X360");
    return false;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::InternalBaseStreamer::Update()
{
    CGS_ASSERT(false, "InternalBaseStreamer::Update: link stub (world fleet mount) -- reconstruct from X360");
}

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
void BrnWorld::PVSDebugComponent::RenderHUD(struct CgsDev::Debug2DImmediateRender *)
{
    CGS_ASSERT(false, "PVSDebugComponent::RenderHUD: link stub (world-module mount) -- link/reconstruct from X360");
}

// LINK STUB (world-module mount 2026-07-26): body not reconstructed yet (X360 @0x827B2178).
void BrnWorld::PVSDebugComponent::OnActivate()
{
    CGS_ASSERT(false, "PVSDebugComponent::OnActivate: link stub (world-module mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// BrnWorld::PropEntityModule
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::PropEntityModule::CachePropGraphicsLists()
{
    CGS_ASSERT(false, "PropEntityModule::CachePropGraphicsLists: link stub (world fleet mount) -- reconstruct from X360");
}

// BOOT-GATE (world-module mount 2026-07-26): REACHED at boot by the wired
// WorldModule::Construct @0x827CF540 fleet cascade; quiet no-op (see
// AIModule::Construct above). Reconstruct from X360 before wiring Prepare.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
void BrnWorld::PropEntityModule::Construct()
{
}

// BOOT-GATE (world-module mount 2026-07-26): REACHED at boot -- WorldModule::
// Construct registers the prop module's nested perf monitors mid-way through
// its own AddMonitor block. Quiet no-op: the handles stay 0/unregistered,
// consumed only by the un-wired Update path. Reconstruct from X360.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
void BrnWorld::PropEntityModule::ConstructPostPhysicsPerfMonitors()
{
}

// BOOT-GATE (world-module mount 2026-07-26): REACHED at boot (see
// ConstructPostPhysicsPerfMonitors above). Quiet no-op.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
void BrnWorld::PropEntityModule::ConstructPreScenePerfMonitors()
{
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::PropEntityModule::Destruct()
{
    CGS_ASSERT(false, "PropEntityModule::Destruct: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::PropEntityModule::GenerateDispatchLists(class BrnWorld::PropEntityIO::InputBuffer_Dispatch *,class Array<class CgsSceneManager::EntityId,5400> const &,struct rw::math::vpu::Matrix44 const &,struct rw::math::vpu::Vector3 const &,float,struct BrnWorld::ShaderLodInfo const *,int,int,int)
{
    CGS_ASSERT(false, "PropEntityModule::GenerateDispatchLists: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::PropEntityModule::PostSceneUpdate(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,struct BrnWorld::PropEntityIO::InputBuffer_PostScene *,struct BrnWorld::PropEntityIO::OutputBuffer_PostScene *,unsigned short)
{
    CGS_ASSERT(false, "PropEntityModule::PostSceneUpdate: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::PropEntityModule::PrePhysicsUpdate(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,class BrnWorld::PropEntityIO::InputBuffer_PrePhysics *,class BrnWorld::PropEntityIO::OutputBuffer_PrePhysics *,unsigned short)
{
    CGS_ASSERT(false, "PropEntityModule::PrePhysicsUpdate: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnWorld::PropEntityModule::Prepare(class BrnWorld::PropEntityIO::OutputBuffer_Prepare *,struct rw::IResourceAllocator *)
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
            *CgsDev::Log::gpDebugPrint << "PropEntityModule::Prepare: inert [FLAG PC boot gate]\n";
    }
    return true;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnWorld::PropEntityModule::Release()
{
    CGS_ASSERT(false, "PropEntityModule::Release: link stub (world fleet mount) -- reconstruct from X360");
    return false;
}

// -------------------------------------------------------------------------
// BrnWorld::RaceCarEntityModule
// -------------------------------------------------------------------------
// BOOT-GATE (world-module mount 2026-07-26): REACHED at boot by the wired
// WorldModule::Construct @0x827CF540 fleet cascade; quiet no-op (see
// AIModule::Construct above). Reconstruct from X360 before wiring Prepare.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
void BrnWorld::RaceCarEntityModule::Construct()
{
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::RaceCarEntityModule::Destruct()
{
    CGS_ASSERT(false, "RaceCarEntityModule::Destruct: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::RaceCarEntityModule::GenerateDispatchLists(struct BrnWorld::RaceCarEntityModuleIO::InputBuffer_GenerateDispatchLists *,class Array<class CgsSceneManager::EntityId,32> const &,struct rw::math::vpu::Vector4,struct rw::math::vpu::Vector4,struct rw::math::vpu::Vector3)
{
    CGS_ASSERT(false, "RaceCarEntityModule::GenerateDispatchLists: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnWorld::RaceCarEntityModule::IsPlayerCarTailgatingOtherRaceCars(enum EActiveRaceCarIndex,class BrnWorld::ActiveRaceCar const *)
{
    CGS_ASSERT(false, "RaceCarEntityModule::IsPlayerCarTailgatingOtherRaceCars: link stub (world fleet mount) -- reconstruct from X360");
    return false;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::RaceCarEntityModule::PostSceneUpdate(struct BrnWorld::RaceCarEntityModuleIO::InputBuffer_PostScene *,struct BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostScene *,unsigned short)
{
    CGS_ASSERT(false, "RaceCarEntityModule::PostSceneUpdate: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::RaceCarEntityModule::PrePhysicsUpdate(struct BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics *,struct BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PrePhysics *,unsigned short)
{
    CGS_ASSERT(false, "RaceCarEntityModule::PrePhysicsUpdate: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnWorld::RaceCarEntityModule::Prepare(struct CgsResource::ResourceHandle const &)
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
            *CgsDev::Log::gpDebugPrint << "RaceCarEntityModule::Prepare: inert [FLAG PC boot gate]\n";
    }
    return true;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnWorld::RaceCarEntityModule::Release()
{
    CGS_ASSERT(false, "RaceCarEntityModule::Release: link stub (world fleet mount) -- reconstruct from X360");
    return false;
}

// -------------------------------------------------------------------------
// BrnWorld::RaceCarEntityModuleIO::InputBuffer_GenerateDispatchLists
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
struct BrnWorld::RaceCarEntityModuleIO::SceneResultQueue * BrnWorld::RaceCarEntityModuleIO::InputBuffer_GenerateDispatchLists::GetSceneResultQueue()
{
    CGS_ASSERT(false, "InputBuffer_GenerateDispatchLists::GetSceneResultQueue: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

// -------------------------------------------------------------------------
// BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
struct BrnWorld::RaceCarEntityModuleIO::SceneResultQueue * BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics::GetSceneResultQueue()
{
    CGS_ASSERT(false, "InputBuffer_PrePhysics::GetSceneResultQueue: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

// -------------------------------------------------------------------------
// BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
struct BrnPhysics::Vehicle::RaceCarState const * BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface::GetRaceCarState(enum EActiveRaceCarIndex) const
{
    CGS_ASSERT(false, "GetRaceCarState: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface::IsPlayerCarActive() const
{
    CGS_ASSERT(false, "IsPlayerCarActive: link stub (world fleet mount) -- reconstruct from X360");
    return false;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface::IsRaceCarActive(enum EActiveRaceCarIndex) const
{
    CGS_ASSERT(false, "IsRaceCarActive: link stub (world fleet mount) -- reconstruct from X360");
    return false;
}

// -------------------------------------------------------------------------
// BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
enum EGlobalRaceCarIndex BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface::GetPlayerGlobalRaceCarIndex() const
{
    CGS_ASSERT(false, "GetPlayerGlobalRaceCarIndex: link stub (world fleet mount) -- reconstruct from X360");
    return (EGlobalRaceCarIndex)0;
}

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
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::TriggerEntityModule::PostSceneUpdate(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,class BrnWorld::TriggerEntityModuleIO::InputBuffer_PostScene *,class BrnWorld::TriggerEntityModuleIO::OutputBuffer_PostScene *,unsigned short)
{
    CGS_ASSERT(false, "TriggerEntityModule::PostSceneUpdate: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::TriggerEntityModule::PrePhysicsUpdate(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,class BrnWorld::TriggerEntityModuleIO::InputBuffer_PrePhysics *,class BrnWorld::TriggerEntityModuleIO::OutputBuffer_PrePhysics *,unsigned short)
{
    CGS_ASSERT(false, "TriggerEntityModule::PrePhysicsUpdate: link stub (world fleet mount) -- reconstruct from X360");
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
void BrnWorld::TriggerEntityModuleDebugComponent::RenderWorld(struct CgsDev::Debug3DImmediateRender *)
{
    CGS_ASSERT(false, "TriggerEntityModuleDebugComponent::RenderWorld: link stub (world-module mount) -- link/reconstruct from X360");
}

// LINK STUB (world-module mount 2026-07-26): committed body not linkable yet (X360 @0x822C4368).
void BrnWorld::TriggerEntityModuleDebugComponent::RenderHUD(struct CgsDev::Debug2DImmediateRender *)
{
    CGS_ASSERT(false, "TriggerEntityModuleDebugComponent::RenderHUD: link stub (world-module mount) -- link/reconstruct from X360");
}

// LINK STUB (world-module mount 2026-07-26): committed body not linkable yet (X360 @0x822A8FF8).  (inert getter)
const char * BrnWorld::TriggerEntityModuleDebugComponent::GetName() const
{
    return "TriggerEntityModuleDebugComponent::GetName link stub";
}

// LINK STUB (world-module mount 2026-07-26): committed body not linkable yet (X360 @0x822A9018).
void BrnWorld::TriggerEntityModuleDebugComponent::OnActivate()
{
    CGS_ASSERT(false, "TriggerEntityModuleDebugComponent::OnActivate: link stub (world-module mount) -- link/reconstruct from X360");
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
class CgsModule::VariableEventQueue<32768,16> * BrnWorld::WorldEntityIO::InputBuffer_GenerateDispatchLists::GetSceneResultQueue()
{
    CGS_ASSERT(false, "InputBuffer_GenerateDispatchLists::GetSceneResultQueue: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

// (BrnWorld::WorldEntityIO::OutputBuffer_Prepare::GetSceneInputInterface stub
// RETIRED 2026-07-26: the real accessor now lives in its owning TU,
// BrnWorldEntityModuleIO_OutputBuffer_Prepare.cpp.)

// -------------------------------------------------------------------------
// BrnWorld::WorldEntityIO::StatusInterface
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::WorldEntityIO::StatusInterface::SetAllStreamed(bool)
{
    CGS_ASSERT(false, "StatusInterface::SetAllStreamed: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::WorldEntityIO::StatusInterface::SetCollisionWorldInvalid(bool)
{
    CGS_ASSERT(false, "StatusInterface::SetCollisionWorldInvalid: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::WorldEntityIO::StatusInterface::SetCollisionWorldInvalidating(bool)
{
    CGS_ASSERT(false, "StatusInterface::SetCollisionWorldInvalidating: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::WorldEntityIO::StatusInterface::SetCollisionWorldValidating(bool)
{
    CGS_ASSERT(false, "StatusInterface::SetCollisionWorldValidating: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::WorldEntityIO::StatusInterface::SetImmediateStreamed(bool)
{
    CGS_ASSERT(false, "StatusInterface::SetImmediateStreamed: link stub (world fleet mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// BrnWorld::WorldEntityModule
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::WorldEntityModule::GenerateMassiveImpressionData(struct CgsGraphics::Instance *,struct rw::math::vpu::Vector3 const &)
{
    CGS_ASSERT(false, "WorldEntityModule::GenerateMassiveImpressionData: link stub (world fleet mount) -- reconstruct from X360");
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

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::WorldEntityModule::UpdateMassive(unsigned short)
{
    CGS_ASSERT(false, "WorldEntityModule::UpdateMassive: link stub (world fleet mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// BrnWorld::WorldModule
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::WorldModule::BridgeWorldModuleToEntityModules_Render(class BrnTraffic::BrnTrafficIO::InputBuffer_Dispatch *,struct BrnWorld::RaceCarEntityModuleIO::InputBuffer_GenerateDispatchLists *,struct BrnWorld::WorldEntityIO::InputBuffer_GenerateDispatchLists *,class BrnWorld::PropEntityIO::InputBuffer_Dispatch *,struct BrnWorldIO::DispatchInputBuffer const *)
{
    CGS_ASSERT(false, "WorldModule::BridgeWorldModuleToEntityModules_Render: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::WorldModule::CalculateVehicleLODs(struct rw::math::vpu::Vector3)
{
    CGS_ASSERT(false, "WorldModule::CalculateVehicleLODs: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::WorldModule::FilterFrustumTestResults(struct CgsModule::Event const *,class Array<class CgsSceneManager::EntityId,4500> *,class Array<class CgsSceneManager::EntityId,32> *,class Array<class CgsSceneManager::EntityId,650> *,class Array<class CgsSceneManager::EntityId,5400> *)
{
    CGS_ASSERT(false, "WorldModule::FilterFrustumTestResults: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::WorldModule::SetupShaderConstantsBeforeRendering(struct BrnShaderConstantsFrame *,float,float)
{
    CGS_ASSERT(false, "WorldModule::SetupShaderConstantsBeforeRendering: link stub (world fleet mount) -- reconstruct from X360");
}

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
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsDev::DebugRender::DrawCircle(struct rw::math::vpu::Vector3,struct rw::math::vpu::Vector3,float,unsigned int)
{
    CGS_ASSERT(false, "DebugRender::DrawCircle: link stub (world fleet mount) -- reconstruct from X360");
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
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsGeometric::Frustum::CalcVertices(struct rw::math::vpu::Vector4 *) const
{
    CGS_ASSERT(false, "Frustum::CalcVertices: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsGeometric::Frustum::SetFromRwFrustum(struct CgsGraphics::CameraRwFrustum const &)
{
    CGS_ASSERT(false, "Frustum::SetFromRwFrustum: link stub (world fleet mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// CgsGraphics::Camera -- DESTUBBED (2026-07-26 wave): Construct (both overloads,
// @0x827F0A08 / @0x827F94E8), Release, SetFovHorizontal @0x821F13B0,
// UpdatePerspectiveProjectionMatrix @0x827EC778,
// SetPerspectiveProjectionMatrixRightHanded @0x827EC698, LookAt @0x827F9510,
// Clone @0x827E7018, SetFarClip and the GetPosition/GetDirection additive
// accessors now live in GameShared/GameClasses/Graphics/CgsCamera.cpp.
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
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsGraphics::DispatchBin::HandleMemoryOverflow(unsigned int)
{
    CGS_ASSERT(false, "DispatchBin::HandleMemoryOverflow: link stub (world fleet mount) -- reconstruct from X360");
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
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
unsigned int CgsGraphics::Model::GetNumLods() const
{
    CGS_ASSERT(false, "GetNumLods: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
unsigned int CgsGraphics::Model::GetNumRenderables() const
{
    CGS_ASSERT(false, "GetNumRenderables: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

// -------------------------------------------------------------------------
// CgsSceneManager::CachedTriangleList
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool CgsSceneManager::CachedTriangleList::Prepare(struct rw::IResourceAllocator *,int)
{
    // BOOT-GATE (attribsys wave 2026-07-26): REACHED by the world Prepare chain
    // (SceneManagerModule::Prepare / the world stage machine). One-shot log +
    // report success so the scripted load advances; the sub-manager stays
    // inert (zero-initialised storage) and its consumers keep their traps.
    // Reconstruct from X360 (triangle-cache cluster).
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "CachedTriangleList::Prepare: inert [FLAG PC boot gate]\n";
    }
    return true;
}

// -------------------------------------------------------------------------
// CgsSceneManager::EntityManager
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
int CgsSceneManager::EntityManager::GetVolumeInstanceIndexByID(struct CgsSceneManager::VolumeInstanceId) const
{
    CGS_ASSERT(false, "GetVolumeInstanceIndexByID: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
struct CgsSceneManager::VolumeInstance * CgsSceneManager::EntityManager::GetVolumeInstance(int)
{
    CGS_ASSERT(false, "EntityManager::GetVolumeInstance: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

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
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsSceneManager::OverlapGenerationModule::GenerateOverlaps(void *,void const *)
{
    CGS_ASSERT(false, "OverlapGenerationModule::GenerateOverlaps: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool CgsSceneManager::OverlapGenerationModule::Prepare(void *,void *)
{
    // BOOT-GATE (attribsys wave 2026-07-26): REACHED by the world Prepare chain
    // (SceneManagerModule::Prepare / the world stage machine). One-shot log +
    // report success so the scripted load advances; the sub-manager stays
    // inert (zero-initialised storage) and its consumers keep their traps.
    // Reconstruct from X360 (overlap-generation cluster).
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "OverlapGenerationModule::Prepare: inert [FLAG PC boot gate]\n";
    }
    return true;
}

// The four module virtuals below became link-required when the game module
// mounted the REAL WorldModule (2026-07-26): SceneManagerModule embeds
// mOverlapGenerator by value inside the by-value fleet, so BrnGameModule.obj
// emits the vtable. Construct IS reached at boot (the real
// SceneManagerModule::Construct @0x828D09A0 constructs each sub-manager);
// its committed body (@0x828D0460, CgsOverlapGenerationModule TU) is not
// linked because it drags the rw::collision / loose-octree closure (see the
// banner) -- boot-fire is diagnosed + gated per the mount-wave log.

// BOOT-GATE (world-module mount 2026-07-26): REACHED at boot via the real
// SceneManagerModule::Construct @0x828D09A0 sub-manager cascade; quiet no-op --
// the committed body (@0x828D0460) stays unlinked until the rw::collision
// closure lands. Link/reconstruct before wiring Prepare.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
void CgsSceneManager::OverlapGenerationModule::Construct()
{
}

// LINK STUB (world-module mount 2026-07-26): committed body not linkable yet (X360 @0x828CB798).
bool CgsSceneManager::OverlapGenerationModule::Release()
{
    CGS_ASSERT(false, "OverlapGenerationModule::Release: link stub (world-module mount) -- reconstruct/link from X360");
    return true;
}

// LINK STUB (world-module mount 2026-07-26): body not reconstructed yet.
void CgsSceneManager::OverlapGenerationModule::Destruct()
{
    CGS_ASSERT(false, "OverlapGenerationModule::Destruct: link stub (world-module mount) -- reconstruct from X360");
}

// LINK STUB (world-module mount 2026-07-26): body not reconstructed yet.
void CgsSceneManager::OverlapGenerationModule::Update()
{
    CGS_ASSERT(false, "OverlapGenerationModule::Update: link stub (world-module mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// CgsSceneManager::SceneManagerIO::InSceneUpdateInterface
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::AddEntity(class CgsSceneManager::EntityId,unsigned int,struct rw::math::vpu::Vector3,float)
{
    CGS_ASSERT(false, "InSceneUpdateInterface::AddEntity: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::AddVolumeInstance(class CgsSceneManager::EntityId,struct rw::math::vpu::Matrix44Affine const &)
{
    CGS_ASSERT(false, "InSceneUpdateInterface::AddVolumeInstance: link stub (world fleet mount) -- reconstruct from X360");
}

// BOOT-GATE (attribsys wave 2026-07-26): REACHED by WorldModule::Prepare's
// WORLDENTITY fail path (merge the world-entity buffer's staged scene adds into
// the live scene input). Quiet one-shot log + drop -- the scene managers the
// merged events would feed are themselves gated inert, so the drop is the
// consistent observable. Reconstruct the 25-queue whole-interface Append from
// the X360 alongside SceneManagerModule::Update.
void CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::Append(struct CgsSceneManager::SceneManagerIO::InSceneUpdateInterface const &)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "InSceneUpdateInterface::Append: inert [FLAG PC boot gate]\n";
    }
}

// (InSceneUpdateInterface::SetCullingGroupPair stub RETIRED 2026-07-26: the real
// producer @0x822B1B60 now lives in CgsSceneManagerIO_SceneUpdate.cpp, alongside
// the new ClearCullingTable @0x827BAB78 + InSceneUpdateInterface::Construct.)

// -------------------------------------------------------------------------
// CgsSceneManager::SceneManagerIO::InputBuffer_Query
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsSceneManager::SceneManagerIO::InputBuffer_Query::Construct()
{
    CGS_ASSERT(false, "InputBuffer_Query::Construct: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsSceneManager::SceneManagerIO::InputBuffer_Query::Destruct()
{
    CGS_ASSERT(false, "InputBuffer_Query::Destruct: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
class CgsSceneManager::SceneManagerIO::InCoarseQueryQueue<16384> * CgsSceneManager::SceneManagerIO::InputBuffer_Query::GetInCoarseQueryQueue()
{
    CGS_ASSERT(false, "InputBuffer_Query::GetInCoarseQueryQueue: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

// -------------------------------------------------------------------------
// CgsSceneManager::SceneManagerIO::OutputBuffer
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
class CgsModule::VariableEventQueue<32768,16> const * CgsSceneManager::SceneManagerIO::OutputBuffer::GetSceneQueryResultsQueue() const
{
    CGS_ASSERT(false, "GetSceneQueryResultsQueue: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

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
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsSceneManager::SceneManagerModule::BridgeOverlapCullerToOutputBuffer(struct CgsSceneManager::SceneManagerIO::OutputBuffer *,struct CgsSceneManager::SceneManagerIO::OutputBuffer *)
{
    CGS_ASSERT(false, "SceneManagerModule::BridgeOverlapCullerToOutputBuffer: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsSceneManager::SceneManagerModule::BridgeOverlapGenerationToOutputBuffer(struct CgsSceneManager::SceneManagerIO::OutputBuffer *,struct CgsSceneManager::SceneManagerIO::OutputBuffer *)
{
    CGS_ASSERT(false, "SceneManagerModule::BridgeOverlapGenerationToOutputBuffer: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsSceneManager::SceneManagerModule::BridgeOverlapGenerationToOverlapCulling(struct CgsSceneManager::SceneManagerIO::OutputBuffer *,struct CgsSceneManager::SceneManagerIO::OutputBuffer *)
{
    CGS_ASSERT(false, "SceneManagerModule::BridgeOverlapGenerationToOverlapCulling: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsSceneManager::SceneManagerModule::ExternalSceneQueriesUpdate()
{
    CGS_ASSERT(false, "SceneManagerModule::ExternalSceneQueriesUpdate: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsSceneManager::SceneManagerModule::ProcessFrustumTestJobRequests(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,struct CgsSceneManager::SceneManagerIO::InputBuffer_Query *,struct CgsSceneManager::SceneManagerIO::OutputBuffer *)
{
    CGS_ASSERT(false, "SceneManagerModule::ProcessFrustumTestJobRequests: link stub (world fleet mount) -- reconstruct from X360");
}

// BOOT GATE -- SceneManagerModule::UpdateScene @0x828D4C28 (X360 vtbl+64).
// RENAMED 2026-07-27 (world-drive wave) from the placeholder "Update"; the X360
// symbol literally named SceneManagerModule::Update @0x827E1F28 is the assert
// "Don't use this function. Use UpdateScene(), UpdateSceneQueries() and
// UpdateContactGeneration() instead" (CgsSceneManagerModule.h:276).
//
// The X360 shell, step for step (CgsSceneManagerModule.cpp:728..):
//   1. StartMonitor(the module's UpdateScene CPU monitor, dword_82F33EC8);
//   2. four null tripwires (:728 lpInputBufferStack, :729 lpOutputBufferStack,
//      :730 lpSceneInputBuffer, :731 lpSceneOutputBuffer);
//   3. CreateIOBuffer<SpatialPartitionIO::InputBuffer_Update>  ("SpatialPartition")
//      and <OverlapGenerationIO::InputBuffer> ("OverlapGeneration") on the INPUT
//      stack; <SpatialPartitionIO::OutputBuffer> + <OverlapGenerationIO::OutputBuffer>
//      on the OUTPUT stack;
//   4. read-lock the scene input, write-lock both sub-module inputs, and fan the
//      scene input's update interface out through
//      SceneManagerModule::BridgeInputSceneUpdateInterfaceToSubModules(ogIn, spIn,
//      sceneIn, lbPrepare), then unlock in reverse;
//   5. SpatialPartitionManager::UpdateScene(&mSpatialPartitionManager, spIn) --
//      i.e. drain the add/remove/set-position/set-radius queue into the octree;
//   6. (*(vtbl(mOverlapGenerator) + 68))(&mOverlapGenerator, ogIn) -- the overlap
//      generator's own update;
//   7. write-lock the scene output and publish &mTriangleCacheManager on it
//      (the "lpTriangleCacheManager != NULL" tripwire, CgsSceneManagerModuleIO.h:1268);
//   8. destroy the four buffers; StopMonitor.
//
// WHY INERT IS FAITHFUL HERE: steps 5-7 are pure transfers into sub-modules that
// hold NO DATA on the PC build -- the spatial partition's octree and the overlap
// generator are both boot-gated (their Prepare never allocates), and the scene
// input's update interface is empty because every entity-module -> scene bridge
// feeding it is itself gated. With no entities registered, the X360 shell's
// observable effect reduces to the triangle-cache-manager pointer publish, which
// no committed consumer reads yet. Reconstruct steps 3-7 for real when the
// SpatialPartition/OverlapGeneration IO buffers + BridgeInputSceneUpdateInterface-
// ToSubModules land, and DELETE this gate.
bool CgsSceneManager::SceneManagerModule::UpdateScene(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,struct CgsSceneManager::SceneManagerIO::InputBuffer_Update *,struct CgsSceneManager::SceneManagerIO::OutputBuffer *,bool)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "SceneManagerModule::UpdateScene: inert [FLAG PC boot gate]\n";
    }
    return true;
}

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
void CgsSceneManager::SceneManagerModule::ProcessSceneQueries(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,struct CgsSceneManager::SceneManagerIO::InputBuffer_Query *,struct CgsSceneManager::SceneManagerIO::OutputBuffer *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "SceneManagerModule::ProcessSceneQueries: inert [FLAG PC boot gate]\n";
    }
}

// -------------------------------------------------------------------------
// CgsSceneManager::SpatialPartitionManager
// -------------------------------------------------------------------------
// BOOT-GATE (world-module mount 2026-07-26): REACHED at boot via the real
// SceneManagerModule::Construct @0x828D09A0 sub-manager cascade; quiet no-op
// (see AIModule::Construct). Reconstruct from X360 before wiring Prepare.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
void CgsSceneManager::SpatialPartitionManager::Construct()
{
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool CgsSceneManager::SpatialPartitionManager::Prepare(struct CgsSceneManager::SpatialPartitionConstructParams *,struct rw::IResourceAllocator *)
{
    // BOOT-GATE (attribsys wave 2026-07-26): REACHED by the world Prepare chain
    // (SceneManagerModule::Prepare / the world stage machine). One-shot log +
    // report success so the scripted load advances; the sub-manager stays
    // inert (zero-initialised storage) and its consumers keep their traps.
    // Reconstruct from X360 (SpatialPartition/LooseOctree cluster).
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "SpatialPartitionManager::Prepare: inert [FLAG PC boot gate]\n";
    }
    return true;
}

// -------------------------------------------------------------------------
// CgsSceneManager::TriangleCacheManager
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsSceneManager::TriangleCacheManager::EndUpdateTriangleCaches(void *,void *)
{
    CGS_ASSERT(false, "TriangleCacheManager::EndUpdateTriangleCaches: link stub (world fleet mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// CgsSceneManager::TriangleCollisionManager
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool CgsSceneManager::TriangleCollisionManager::Prepare(class CgsMemory::LinearMalloc *,int)
{
    // BOOT-GATE (attribsys wave 2026-07-26): REACHED by the world Prepare chain
    // (SceneManagerModule::Prepare / the world stage machine). One-shot log +
    // report success so the scripted load advances; the sub-manager stays
    // inert (zero-initialised storage) and its consumers keep their traps.
    // Reconstruct from X360 0x828D0C40 (TriangleCollisionManager::Prepare; see ledger).
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "TriangleCollisionManager::Prepare: inert [FLAG PC boot gate]\n";
    }
    return true;
}

// -------------------------------------------------------------------------
// CgsSceneManager::VolumeManager
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool CgsSceneManager::VolumeManager::Prepare()
{
    // BOOT-GATE (attribsys wave 2026-07-26): REACHED by the world Prepare chain
    // (SceneManagerModule::Prepare / the world stage machine). One-shot log +
    // report success so the scripted load advances; the sub-manager stays
    // inert (zero-initialised storage) and its consumers keep their traps.
    // Reconstruct from X360 0x828CFD38.
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "VolumeManager::Prepare: inert [FLAG PC boot gate]\n";
    }
    return true;
}

// -------------------------------------------------------------------------
// ShaderConstantTable
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void ShaderConstantTable::SetShaderConstantData(unsigned int,struct rw::math::vpu::Matrix44)
{
    CGS_ASSERT(false, "ShaderConstantTable::SetShaderConstantData: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void ShaderConstantTable::SetShaderConstantData(unsigned int,struct rw::math::vpu::Matrix44Affine)
{
    CGS_ASSERT(false, "ShaderConstantTable::SetShaderConstantData: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void ShaderConstantTable::SetShaderConstantData(unsigned int,struct rw::math::vpu::Vector3)
{
    CGS_ASSERT(false, "ShaderConstantTable::SetShaderConstantData: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (destub wave 2026-07-26, referenced by ShadowMap::SetConstantsForEnvmap
// @0x827C1AD0 -- the 16-byte overload with a live w lane): body not reconstructed yet
// (X360 @0x822B32E8; needs UpdateShaderChangeTableAndGetConstantDestination @0x822A0A20).
void ShaderConstantTable::SetShaderConstantData(unsigned int,struct rw::math::vpu::Vector4)
{
    CGS_ASSERT(false, "ShaderConstantTable::SetShaderConstantData: link stub (world fleet mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// WorldModule
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void WorldModule::BridgeCrashModuleToPropModule_PostScene(void *,struct BrnWorld::PropEntityIO::InputBuffer_PostScene *,class BrnWorld::CrashModuleIO::OutputBuffer_PostScene const *)
{
    CGS_ASSERT(false, "WorldModule::BridgeCrashModuleToPropModule_PostScene: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void WorldModule::BridgeCrashModuleToRaceCarModule_PostScene(void *,struct BrnWorld::RaceCarEntityModuleIO::InputBuffer_PostScene *,class BrnWorld::CrashModuleIO::OutputBuffer_PostScene const *)
{
    CGS_ASSERT(false, "WorldModule::BridgeCrashModuleToRaceCarModule_PostScene: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void WorldModule::BridgeCrashModuleToTrafficModule_PostScene(void *,class BrnTraffic::BrnTrafficIO::InputBuffer_PostScene *,class BrnWorld::CrashModuleIO::OutputBuffer_PostScene const *)
{
    CGS_ASSERT(false, "WorldModule::BridgeCrashModuleToTrafficModule_PostScene: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void WorldModule::BridgePropModuleToTrafficModule_PrePhysics(void *,class BrnTraffic::BrnTrafficIO::InputBuffer_PrePhysics *,class BrnWorld::PropEntityIO::OutputBuffer_PrePhysics const *)
{
    CGS_ASSERT(false, "WorldModule::BridgePropModuleToTrafficModule_PrePhysics: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void WorldModule::BridgeRaceCarEntityInfoToOutput_PrePhysics(void *,struct BrnWorldIO::UpdateOutputBuffer *,struct BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PrePhysics const *)
{
    CGS_ASSERT(false, "WorldModule::BridgeRaceCarEntityInfoToOutput_PrePhysics: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void WorldModule::BridgeRaceCarModuleToSceneModule_PostScene(void *,struct CgsSceneManager::SceneManagerIO::InputBuffer_Query *,struct BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostScene const *)
{
    CGS_ASSERT(false, "WorldModule::BridgeRaceCarModuleToSceneModule_PostScene: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void WorldModule::BridgeRaceCarModuleToTrafficModule_PostScene(void *,class BrnTraffic::BrnTrafficIO::InputBuffer_PostScene *,struct BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostScene const *)
{
    CGS_ASSERT(false, "WorldModule::BridgeRaceCarModuleToTrafficModule_PostScene: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void WorldModule::BridgeRaceCarModuleToTrafficModule_PrePhysics(void *,class BrnTraffic::BrnTrafficIO::InputBuffer_PrePhysics *,struct BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PrePhysics const *)
{
    CGS_ASSERT(false, "WorldModule::BridgeRaceCarModuleToTrafficModule_PrePhysics: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void WorldModule::BridgeSceneContactsToPropModule_PrePhysics(void *,class BrnWorld::PropEntityIO::InputBuffer_PrePhysics *,struct CgsSceneManager::SceneManagerIO::OutputBuffer const *)
{
    CGS_ASSERT(false, "WorldModule::BridgeSceneContactsToPropModule_PrePhysics: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void WorldModule::BridgeSceneContactsToRaceCarModule_PrePhysics(void *,struct BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics *,struct CgsSceneManager::SceneManagerIO::OutputBuffer const *)
{
    CGS_ASSERT(false, "WorldModule::BridgeSceneContactsToRaceCarModule_PrePhysics: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void WorldModule::BridgeSceneContactsToTrafficModule_PrePhysics(void *,class BrnTraffic::BrnTrafficIO::InputBuffer_PrePhysics *,struct CgsSceneManager::SceneManagerIO::OutputBuffer const *)
{
    CGS_ASSERT(false, "WorldModule::BridgeSceneContactsToTrafficModule_PrePhysics: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void WorldModule::BridgeSceneQueryResultsToTrafficModule_PrePhysics(void *,class BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics *,class BrnTraffic::BrnTrafficIO::InputBuffer_PrePhysics *,struct CgsSceneManager::SceneManagerIO::OutputBuffer const *)
{
    CGS_ASSERT(false, "WorldModule::BridgeSceneQueryResultsToTrafficModule_PrePhysics: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void WorldModule::BridgeSceneQueryResultsToTriggerModule_PrePhysics(void *,class BrnWorld::TriggerEntityModuleIO::InputBuffer_PrePhysics *,struct CgsSceneManager::SceneManagerIO::OutputBuffer const *)
{
    CGS_ASSERT(false, "WorldModule::BridgeSceneQueryResultsToTriggerModule_PrePhysics: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void WorldModule::BridgeTrafficCarEntityInfoToOutput_PrePhysics(void *,struct BrnWorldIO::UpdateOutputBuffer *,class BrnTraffic::BrnTrafficIO::OutputBuffer_PrePhysics const *)
{
    CGS_ASSERT(false, "WorldModule::BridgeTrafficCarEntityInfoToOutput_PrePhysics: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void WorldModule::BridgeTrafficModuleToSceneModule_PostScene(void *,struct CgsSceneManager::SceneManagerIO::InputBuffer_Query *,struct BrnTraffic::BrnTrafficIO::OutputBuffer_PostScene const *)
{
    CGS_ASSERT(false, "WorldModule::BridgeTrafficModuleToSceneModule_PostScene: link stub (world fleet mount) -- reconstruct from X360");
}

// (BridgeTrafficToRaceCar_PrePhysics stub RETIRED 2026-07-27: the REAL body
// @0x827A51F0 lives in its own home TU, Bridges/WorldBridgeEntityModulesToEntityModules.cpp,
// which the world-drive wave mounts on the build list.)

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void WorldModule::BridgeTriggerModuleToSceneModule_PostScene(void *,struct CgsSceneManager::SceneManagerIO::InputBuffer_Query *,class BrnWorld::TriggerEntityModuleIO::OutputBuffer_PostScene const *)
{
    CGS_ASSERT(false, "WorldModule::BridgeTriggerModuleToSceneModule_PostScene: link stub (world fleet mount) -- reconstruct from X360");
}

// (rw::BitTable::GetResourceDescriptor stub RETIRED 2026-07-26: the real body now
// lives in its owning TU, src/vendor/renderware/collision/BitTable.cpp.)

// -------------------------------------------------------------------------
// rw::collision::Volume
// -------------------------------------------------------------------------
// BOOT-GATE (world-module mount 2026-07-26): REACHED at boot -- the real
// SceneManagerModule::Construct @0x828D09A0 lazily fills the shared Volume
// processing vtable here. Quiet no-op returning 0: no rw::collision volume is
// ever processed until the world Prepare/query path is wired. The real body
// is owned by the rwcollision SDK TU (volume.cpp) -- link it with the
// rw::collision closure.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
int rw::collision::Volume::InitializeVTable()
{
    return 0;
}

// -------------------------------------------------------------------------
// rw::collision::VolumeVolumeQuery
// -------------------------------------------------------------------------
// BOOT-GATE (world-module mount 2026-07-26): REACHED at boot -- the real
// OverlapCullingModule::Construct @0x828C18E8 stores the result in
// mpVolVolQuery. Quiet null return: the handle is only consumed by the
// un-wired Prepare/CullOverlaps path. Real body = rwcollision SDK TU.
// FLAG PC-platform leaf: boot-gate no-op (world-module mount 2026-07-26) -- reached by the wired WorldModule::Construct cascade; real body pending X360 reconstruction (see note above).
void * rw::collision::VolumeVolumeQuery::Initialize(void * *,int,int)
{
    return 0;
}

// BOOT-GATE (world-module mount 2026-07-26): REACHED at boot -- the real
// OverlapCullingModule::Construct @0x828C18E8 calls this and then COPIES 10
// words out of the returned pointer (returning null here crashed the first
// mount boot). Fill the caller's scratch with the two words the asm actually
// consults -- size <= 0x62000 (the X360's exact scratch budget for the 100/100
// query) and alignment == 16 (GTALIGN) -- and hand it back. These are the
// asm-attested immediates of the consuming asserts, not invented behaviour;
// the real SDK body computes the same-or-smaller size for 100/100. The
// resulting mpVolVolQuery stays null (Initialize below) and is only consumed
// by the un-wired Prepare/CullOverlaps path.
void * rw::collision::VolumeVolumeQuery::GetResourceDescriptor(void * lpScratch, int, int)
{
    unsigned int* lpuWords = static_cast<unsigned int*>(lpScratch);
    for (int liWord = 0; liWord < 10; ++liWord)
        lpuWords[liWord] = 0;
    lpuWords[0] = 0x62000u;   // size word (caller asserts <= 0x62000)
    lpuWords[1] = 16u;        // alignment word (caller asserts == 16)
    return lpScratch;
}

// -------------------------------------------------------------------------
// rw::math::vpu
// -------------------------------------------------------------------------
// -------------------------------------------------------------------------
// struct BrnPhysics::Deformation::WheelPhysicalStates & __ptr64 BrnPhysics::Deformation::WheelPhysicalStates::operator=(struct BrnPhysics::Deformation
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
struct BrnPhysics::Deformation::WheelPhysicalStates & BrnPhysics::Deformation::WheelPhysicalStates::operator=(struct BrnPhysics::Deformation::WheelPhysicalStates const &)
{
    CGS_ASSERT(false, "WheelPhysicalStates::operator=: link stub (world fleet mount) -- reconstruct from X360");
    return *this;
}

// -------------------------------------------------------------------------
// void BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface::operator=(struct BrnWorld::RaceCarEntityModuleIO
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface::operator=(struct BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface const &)
{
    CGS_ASSERT(false, "RCEntityActiveRaceCarOutputInterface::operator=: link stub (world fleet mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// void BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface::operator=(struct BrnWorld::RaceCarEntityModuleIO
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface::operator=(struct BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface const &)
{
    CGS_ASSERT(false, "RCEntityGlobalRaceCarOutputInterface::operator=: link stub (world fleet mount) -- reconstruct from X360");
}

// ---------------------------------------------------------------------------
// Attrib mount closure stubs (2026-07-27): symbols the linked SDK TUs
// reference whose bodies are documented NEXT-WAVE gaps (attrib_sdk_wave_log
// G-list). Each traps loudly; none is on the schema/vault-register path.
// ---------------------------------------------------------------------------
// LINK STUB (attrib mount closure): generated-accessor keyed lookup; the X360
// no-arg form is real in attribinstance.cpp -- this keyed overload is gap G5
// (runs only when Gen:: accessors walk a materialized collection).
void * Attrib::Instance::GetAttributePointer(unsigned __int64, unsigned int) const
{
    CGS_ASSERT(false, "Attrib::Instance::GetAttributePointer(key,idx): attrib gap G5 -- reconstruct");
    return 0;
}

// LINK STUB (attrib mount closure): gap G5 sibling (Gen:: ChangeWithDefault edit path).
Attrib::Collection * Attrib::FindCollectionWithDefault(int)
{
    CGS_ASSERT(false, "Attrib::FindCollectionWithDefault: attrib gap G5 -- reconstruct");
    return 0;
}

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

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 0x827A50E0 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgeRaceCarModuleToTrafficModule_PreScene(void *,class BrnTraffic::BrnTrafficIO::InputBuffer_PreScene *,class BrnTraffic::BrnTrafficIO::InputBuffer_PostScene *,class BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics *,struct BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeRaceCarModuleToTrafficModule_PreScene: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 0x827A5510 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgeRaceCarModuleToPropModule_PreScene(void *,class BrnWorld::PropEntityIO::InputBuffer_PreScene *,struct BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene const *,unsigned short)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeRaceCarModuleToPropModule_PreScene: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 0x827AACF8 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgeWorldModuleToPropModule_PreScene(void *,class BrnWorld::PropEntityIO::InputBuffer_PreScene *,struct BrnWorld::WorldEntityIO::OutputBuffer_PreScene const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeWorldModuleToPropModule_PreScene: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 0x827AE9D0 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgePhysicsModuleToRaceCarModule_PostPhysics(void *,struct BrnWorld::RaceCarEntityModuleIO::InputBuffer_PostPhysics *,class BrnPhysics::PhysicsModuleIO::OutputBuffer const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgePhysicsModuleToRaceCarModule_PostPhysics: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 0x827AB910 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgePhysicsModuleToTrafficModule_PostPhysics(void *,class BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics *,class BrnPhysics::PhysicsModuleIO::OutputBuffer const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgePhysicsModuleToTrafficModule_PostPhysics: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 0x827AB998 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgePhysicsModuleToPropModule_PostPhysics(void *,class BrnWorld::PropEntityIO::InputBuffer_PostPhysics *,class BrnPhysics::PhysicsModuleIO::OutputBuffer const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgePhysicsModuleToPropModule_PostPhysics: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 0x827AB8B0 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgePhysicsModuleToCrashModule_PostPhysics(void *,struct BrnWorld::CrashIO::InputBuffer_PostPhysics *,class BrnPhysics::PhysicsModuleIO::OutputBuffer const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgePhysicsModuleToCrashModule_PostPhysics: inert [FLAG PC boot gate]\n";
    }
}

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

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 0x827AD708 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgeTrafficToCrashModule_PostPhysics(void *,struct BrnWorld::CrashIO::InputBuffer_PostPhysics *,struct BrnTraffic::BrnTrafficIO::OutputBuffer_PostPhysics const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeTrafficToCrashModule_PostPhysics: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 the Update input fan-out -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgeInputToPhysicsModule(void *,class BrnPhysics::PhysicsModuleIO::InputBuffer *,struct BrnWorldIO::UpdateInputBuffer const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeInputToPhysicsModule: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 0x827ADEE8 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgeInputToCrashModule(void *,struct BrnWorld::CrashIO::InputBuffer_PreScene *,struct BrnWorldIO::UpdateInputBuffer const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeInputToCrashModule: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 0x827ABF40 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgeActionsToRaceCarModule(void *,struct BrnWorld::RaceCarEntityModuleIO::InputBuffer_PreScene *,struct BrnWorldIO::UpdateInputBuffer const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeActionsToRaceCarModule: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 0x827AF258 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgePropToOutput_PreScene(void *,struct BrnWorldIO::UpdateOutputBuffer *,class BrnWorld::PropEntityIO::OutputBuffer_PreScene const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgePropToOutput_PreScene: inert [FLAG PC boot gate]\n";
    }
}

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

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 the post-physics output fan-in -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgeEntityModulesToOutput_PostPhysics(void *,struct BrnWorldIO::UpdateOutputBuffer *,struct BrnTraffic::BrnTrafficIO::OutputBuffer_PostPhysics const *,struct BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostPhysics const *,class BrnWorld::PropEntityIO::OutputBuffer_PostPhysics const *,struct BrnWorld::WorldEntityIO::OutputBuffer_PostPhysics const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeEntityModulesToOutput_PostPhysics: inert [FLAG PC boot gate]\n";
    }
}

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

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 the scene-output leg of the query round trip -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgeSceneModuleToOutput(void *,struct BrnWorldIO::UpdateOutputBuffer *,struct CgsSceneManager::SceneManagerIO::OutputBuffer const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeSceneModuleToOutput: inert [FLAG PC boot gate]\n";
    }
}

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

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 0x827AB490 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgeEntityModulesToSceneModule_PreScene(void *,struct CgsSceneManager::SceneManagerIO::InputBuffer_Update *,class BrnWorld::TriggerEntityModuleIO::OutputBuffer_PreScene const *,class BrnTraffic::BrnTrafficIO::OutputBuffer_PreScene const *,struct BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene const *,class BrnWorld::PropEntityIO::OutputBuffer_PreScene const *,struct BrnWorld::WorldEntityIO::OutputBuffer_PreScene const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeEntityModulesToSceneModule_PreScene: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 0x827AB608 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgeEntityModulesToScene_PostPhysics(void *,struct CgsSceneManager::SceneManagerIO::InputBuffer_Update *,struct BrnTraffic::BrnTrafficIO::OutputBuffer_PostPhysics const *,struct BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostPhysics const *,class BrnWorld::PropEntityIO::OutputBuffer_PostPhysics const *,struct BrnWorld::WorldEntityIO::OutputBuffer_PostPhysics const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeEntityModulesToScene_PostPhysics: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 0x827AADB8 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgeEntityModulesToPhysicsModule_PreScene(void *,class BrnPhysics::PhysicsModuleIO::InputBuffer *,struct BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene const *,class BrnWorld::PropEntityIO::OutputBuffer_PreScene const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeEntityModulesToPhysicsModule_PreScene: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 0x827AAEC0 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgeEntityModulesToPhysicsModule_PrePhysics(void *,class BrnPhysics::PhysicsModuleIO::InputBuffer *,struct BrnTraffic::BrnTrafficIO::OutputBuffer_PrePhysics const *,struct BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PrePhysics const *,class BrnWorld::PropEntityIO::OutputBuffer_PrePhysics const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeEntityModulesToPhysicsModule_PrePhysics: inert [FLAG PC boot gate]\n";
    }
}

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
// X360 0x827AD540 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgeAIToEntityModules_PrePhysics(void *,struct BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics *,class BrnWorld::PropEntityIO::InputBuffer_PrePhysics *,struct BrnAI::AIModuleIO::OutputBuffer const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeAIToEntityModules_PrePhysics: inert [FLAG PC boot gate]\n";
    }
}

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

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 0x827A8E88 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgeSceneQueryResultsToPhysics(void *,class BrnPhysics::PhysicsModuleIO::InputBuffer *,struct CgsSceneManager::SceneManagerIO::OutputBuffer const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeSceneQueryResultsToPhysics: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. Per-frame world bridge.
// X360 0x827ABD80 -- reconstruct and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void WorldModule::BridgeScenePotentialContactsToPhysics(void *,class BrnPhysics::PhysicsModuleIO::InputBuffer *,struct CgsSceneManager::SceneManagerIO::OutputBuffer const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeScenePotentialContactsToPhysics: inert [FLAG PC boot gate]\n";
    }
}


// ---- module entry points driven by the spines ----------------------------

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. the race-car pre-scene tick.
// Reconstruct from X360 and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void BrnWorld::RaceCarEntityModule::PreSceneUpdate(struct BrnWorld::RaceCarEntityModuleIO::InputBuffer_PreScene *,struct BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene *,unsigned short)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "RaceCarEntityModule::PreSceneUpdate: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. the race-car post-physics tick.
// Reconstruct from X360 and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void BrnWorld::RaceCarEntityModule::PostPhysicsUpdate(struct BrnWorld::RaceCarEntityModuleIO::InputBuffer_PostPhysics *,struct BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostPhysics *,unsigned short)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "RaceCarEntityModule::PostPhysicsUpdate: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. the traffic pre-scene tick.
// Reconstruct from X360 and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void BrnTraffic::TrafficEntityModule::PreSceneUpdate(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,class BrnTraffic::BrnTrafficIO::InputBuffer_PreScene *,class BrnTraffic::BrnTrafficIO::OutputBuffer_PreScene *,unsigned short)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "TrafficEntityModule::PreSceneUpdate: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. the traffic post-physics tick (also driven by UpdateForBootUpVideo @0x827CFDE0).
// Reconstruct from X360 and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void BrnTraffic::TrafficEntityModule::PostPhysicsUpdate(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,class BrnTraffic::BrnTrafficIO::InputBuffer_PostPhysics *,class BrnTraffic::BrnTrafficIO::OutputBuffer_PostPhysics *,unsigned short)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "TrafficEntityModule::PostPhysicsUpdate: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. the prop pre-scene tick (X360 vtbl+68).
// Reconstruct from X360 and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void BrnWorld::PropEntityModule::PreSceneUpdate(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,class BrnWorld::PropEntityIO::InputBuffer_PreScene *,class BrnWorld::PropEntityIO::OutputBuffer_PreScene *,unsigned short)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "PropEntityModule::PreSceneUpdate: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. the prop post-physics tick (X360 vtbl+80).
// Reconstruct from X360 and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void BrnWorld::PropEntityModule::PostPhysicsUpdate(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,class BrnWorld::PropEntityIO::InputBuffer_PostPhysics *,class BrnWorld::PropEntityIO::OutputBuffer_PostPhysics *,unsigned short)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "PropEntityModule::PostPhysicsUpdate: inert [FLAG PC boot gate]\n";
    }
}

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

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. the crash pre-scene tick.
// Reconstruct from X360 and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void BrnWorld::CrashModule::PreSceneUpdate(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,struct BrnWorld::CrashIO::InputBuffer_PreScene const *,struct BrnWorld::CrashIO::OutputBuffer_PreScene *,unsigned short)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "CrashModule::PreSceneUpdate: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. the crash post-physics tick.
// Reconstruct from X360 and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void BrnWorld::CrashModule::PostPhysicsUpdate(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,struct BrnWorld::CrashIO::InputBuffer_PostPhysics const *,struct BrnWorld::CrashIO::OutputBuffer_PostPhysics *,unsigned short)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "CrashModule::PostPhysicsUpdate: inert [FLAG PC boot gate]\n";
    }
}

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

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. the cached-position restage into the scene input (@0x8259C370).
// Reconstruct from X360 and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void BrnPhysics::PhysicsModule::UpdateCachedPositions(struct CgsSceneManager::SceneManagerIO::InputBuffer_Update *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "PhysicsModule::UpdateCachedPositions: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. the physics post-scene tick (@0x825ABC10).
// Reconstruct from X360 and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void BrnPhysics::PhysicsModule::PostSceneUpdate(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,class BrnPhysics::PhysicsModuleIO::InputBuffer const *,class BrnPhysics::PhysicsModuleIO::OutputBuffer *,unsigned short)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "PhysicsModule::PostSceneUpdate: inert [FLAG PC boot gate]\n";
    }
}

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

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. the physics step (@0x825B0640).
// Reconstruct from X360 and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void BrnPhysics::PhysicsModule::Update(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,class BrnPhysics::PhysicsModuleIO::InputBuffer const *,class BrnPhysics::PhysicsModuleIO::OutputBuffer *,unsigned short)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "PhysicsModule::Update: inert [FLAG PC boot gate]\n";
    }
}

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

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. the environment tick (time of day / fog / key light) -- DWARF BrnEnvironmentManager.h:386.
// Reconstruct from X360 and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void BrnWorld::EnvironmentSettings::EnvironmentManager::Update(float,struct BrnWorldIO::UpdateOutputBuffer *,struct rw::math::vpu::Vector3)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "EnvironmentSettings::EnvironmentManager::Update: inert [FLAG PC boot gate]\n";
    }
}

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

// BOOT GATE (world-drive wave 2026-07-27): REACHED every frame by
// WorldModule::Update @0x827D63E8 once the drive is wired. the per-frame triangle-cache kick (@0x828C73D8).
// Reconstruct from X360 and DELETE this gate.
// One-shot log + inert: the module/interface it would feed is itself gated
// inert, so dropping the transfer is the consistent observable.
void CgsSceneManager::SceneManagerModule::StartUpdateTriangleCache(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,struct CgsSceneManager::SceneManagerIO::InputBuffer_Update *,struct CgsSceneManager::CgsCollision::BaseCollisionGenerator *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "SceneManagerModule::StartUpdateTriangleCache: inert [FLAG PC boot gate]\n";
    }
}



// ===========================================================================
// WORLD-DRIVE BOOT GATES, part 2 (2026-07-27) -- the link closure of the real
// WorldModule::Update @0x827D63E8.
// ===========================================================================

// ---- module-IO buffer Construct() ------------------------------------------
// WorldModule::Update creates ~30 module IO buffers per frame on the update
// stacks. The X360 CreateIOBuffer<T> template instantiation runs T::Construct
// after the stack alloc; the generic PC template placement-news only, so the
// drive calls Construct explicitly. For the buffers below the owning IO TU has
// no Construct body yet -- these gates run ONLY the IOBuffer base bring-up
// (raising the status the Lock/Unlock tripwires assert on). The member queue /
// interface bring-up each real Construct also performs is deferred with that
// IO TU; every one of these buffers belongs to a module whose update is itself
// boot-gated, so nothing writes into the un-constructed members.
// Replace each with the real T::Construct in its own IO TU (and delete here).

// BOOT GATE: base bring-up only (see the block note above).
void BrnAI::AIModuleIO::InputBuffer_PostPhysics::Construct()
{
    CgsModule::IOBuffer::Construct();
}

// BOOT GATE: base bring-up only (see the block note above).
void BrnTraffic::BrnTrafficIO::OutputBuffer_PostScene::Construct()
{
    CgsModule::IOBuffer::Construct();
}

// BOOT GATE: base bring-up only (see the block note above).
void BrnWorld::PropEntityIO::InputBuffer_PostPhysics::Construct()
{
    CgsModule::IOBuffer::Construct();
}

// BOOT GATE: base bring-up only (see the block note above).
void BrnWorld::PropEntityIO::InputBuffer_PrePhysics::Construct()
{
    CgsModule::IOBuffer::Construct();
}

// BOOT GATE: base bring-up only (see the block note above).
void BrnWorld::RaceCarEntityModuleIO::InputBuffer_PostPhysics::Construct()
{
    CgsModule::IOBuffer::Construct();
}

// BOOT GATE: base bring-up only (see the block note above).
void BrnWorld::RaceCarEntityModuleIO::InputBuffer_PostScene::Construct()
{
    CgsModule::IOBuffer::Construct();
}

// BOOT GATE: base bring-up only (see the block note above).
void BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics::Construct()
{
    CgsModule::IOBuffer::Construct();
}

// BOOT GATE: base bring-up only (see the block note above).
void BrnWorld::RaceCarEntityModuleIO::InputBuffer_PreScene::Construct()
{
    CgsModule::IOBuffer::Construct();
}

// BOOT GATE: base bring-up only (see the block note above).
void BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostPhysics::Construct()
{
    CgsModule::IOBuffer::Construct();
}

// BOOT GATE: base bring-up only (see the block note above).
void BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PostScene::Construct()
{
    CgsModule::IOBuffer::Construct();
}

// BOOT GATE: base bring-up only (see the block note above).
void BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PrePhysics::Construct()
{
    CgsModule::IOBuffer::Construct();
}

// BOOT GATE: base bring-up only (see the block note above).
void BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene::Construct()
{
    CgsModule::IOBuffer::Construct();
}

// BOOT GATE: base bring-up only (see the block note above).
void BrnWorld::TriggerEntityModuleIO::OutputBuffer_PreScene::Construct()
{
    CgsModule::IOBuffer::Construct();
}


// ---- the collision generator the frame carves -------------------------------
// WorldModule::Update carves ONE 336896-byte BaseCollisionGenerator out of the
// world frame allocator (object + a 0x40000 result region) and hands it to
// SceneManagerModule::StartUpdateTriangleCache. The REAL bodies (Construct
// @0x828105F8 / Prepare @0x82810660) live in
// GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.cpp,
// which is NOT on the build list (it drags the EA::Jobs job-system + collision
// batch closure -- cost rule). Gated here: the generator's only consumer,
// StartUpdateTriangleCache, is itself gated, so an unconstructed generator is
// never dereferenced. Mount that TU (and delete these two) with the contact-
// generation wave.
void CgsSceneManager::CgsCollision::BaseCollisionGenerator::Construct()
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "BaseCollisionGenerator::Construct: inert [FLAG PC boot gate]\n";
    }
}

bool CgsSceneManager::CgsCollision::BaseCollisionGenerator::Prepare(void *, int)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "BaseCollisionGenerator::Prepare: inert [FLAG PC boot gate]\n";
    }
    return true;
}

// ---- two read accessors the drive reads through ------------------------------
// BrnWorldIO::UpdateInputBuffer::GetPlayerVehicleControls (X360 read-lock, the
// controls block Update copies straight into the update output) and the traffic
// pre-scene output's traffic->race-car interface (the 544-byte block Update
// snapshots for the post-scene spine). Both belong to their own IO TUs; gated
// here as read-lock-checked null returns -- the drive tolerates null on both
// paths (the copy and the snapshot are skipped) because the producing modules
// are boot-gated.
struct BrnWorldIO::PlayerVehicleControls const * BrnWorldIO::UpdateInputBuffer::GetPlayerVehicleControls(void) const
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "UpdateInputBuffer::GetPlayerVehicleControls: inert [FLAG PC boot gate]\n";
    }
    return 0;
}

struct BrnTraffic::BrnTrafficIO::OutputBuffer_PreScene::TrafficToRaceCarInterface_PreScene const * BrnTraffic::BrnTrafficIO::OutputBuffer_PreScene::GetTrafficToRaceCarInterface_PreScene(void) const
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "OutputBuffer_PreScene::GetTrafficToRaceCarInterface_PreScene: inert [FLAG PC boot gate]\n";
    }
    return 0;
}

// ---- the six unmounted sibling bridge TUs' entry points ----------------------
// Each of these has a REAL committed body in its own home TU; those TUs are not
// on the build list because each drags declaration-only module-IO accessors (see
// the bat note next to the world-fleet block). Gated here so the real drive links
// TODAY; delete each gate when its home TU is mounted.

// BOOT GATE -- real body @0x827ADF88 in its own home TU (not mounted: IO accessor closure).
void WorldModule::BridgeInputToEntityModules(void *,class BrnWorld::TriggerEntityModuleIO::InputBuffer_PreScene *,class BrnWorld::TriggerEntityModuleIO::InputBuffer_PostScene *,class BrnTraffic::BrnTrafficIO::InputBuffer_PreScene *,struct BrnWorld::RaceCarEntityModuleIO::InputBuffer_PreScene *,struct BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics *,struct BrnWorld::WorldEntityIO::InputBuffer_PreScene *,class BrnWorld::PropEntityIO::InputBuffer_PreScene *,struct BrnWorldIO::UpdateInputBuffer const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeInputToEntityModules: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE -- real body @0x827ABA40 in its own home TU (not mounted: IO accessor closure).
void WorldModule::BridgePhysicsSceneUpdateToScene(void *,struct CgsSceneManager::SceneManagerIO::InputBuffer_Update *,class BrnPhysics::PhysicsModuleIO::OutputBuffer const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgePhysicsSceneUpdateToScene: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE -- real body @0x827A52B0 in its own home TU (not mounted: IO accessor closure).
void WorldModule::BridgeRaceCarModuleToWorldModule_PreScene(void *,struct BrnWorld::WorldEntityIO::InputBuffer_PreScene *,struct BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeRaceCarModuleToWorldModule_PreScene: inert [FLAG PC boot gate]\n";
    }
}

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

// BOOT GATE -- real body @0x827A5060 in its own home TU (not mounted: IO accessor closure).
void WorldModule::BridgeEntityModulesToCrashModule_PreScene(void *,struct BrnWorld::CrashIO::InputBuffer_PreScene *,struct BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeEntityModulesToCrashModule_PreScene: inert [FLAG PC boot gate]\n";
    }
}

// BOOT GATE -- real body @0x827AAC70 in its own home TU (not mounted: IO accessor closure).
void WorldModule::BridgeCrashModuleToPhysicsModule(void *,class BrnPhysics::PhysicsModuleIO::InputBuffer *,struct BrnWorld::CrashIO::OutputBuffer_PreScene const *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldModule::BridgeCrashModuleToPhysicsModule: inert [FLAG PC boot gate]\n";
    }
}

// ---------------------------------------------------------------------------
// CgsResource::ShaderTechniqueResourceType -- the two members its own TU
// documents as DEFERRED (declared, deliberately not bodied there):
//   PostFixUp @0x827EEBF0 -- the ~150-line shader-profile classification pass.
//   GetShaderConstantExternalSerialisedResourceDescriptorSize -- its private
//   descriptor-size helper.
// The type is now REGISTERED (world-render resource types, 2026-07-27), so the
// vtable is emitted and the linker needs both symbols. Marked link stubs until
// that TU's deferral is lifted.
// ---------------------------------------------------------------------------
void CgsResource::ShaderTechniqueResourceType::PostFixUp(void* /*lpResource*/,
                                                         const rw::Resource& /*lrResource*/) const
{
    CGS_ASSERT(false, "ShaderTechniqueResourceType::PostFixUp @0x827EEBF0: documented deferral -- reconstruct");
}

uint32_t CgsResource::ShaderTechniqueResourceType::GetShaderConstantExternalSerialisedResourceDescriptorSize(
    const ShaderConstantsExternal* /*lpBlock*/) const
{
    CGS_ASSERT(false, "ShaderTechniqueResourceType::GetShaderConstantExternalSerialisedResourceDescriptorSize: documented deferral -- reconstruct");
    return 0;
}
