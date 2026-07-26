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
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsSpatialPartitionManager.h"
#include "GameSource/World/Bridges/WorldBridgeEntityModulesToOutput.h"
#include "GameSource/World/Bridges/WorldBridgeToEntityModules.h"
#include "GameSource/World/Bridges/WorldBridgeSceneToEntityModules.h"
#include "GameSource/World/Bridges/WorldBridgeCrashToEntityModules.h"
#include "GameSource/World/Bridges/WorldBridgeEntityModulesToEntityModules.h"
#include "GameSource/World/Bridges/WorldBridgeEntityModulesToScene.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameSource/World/BrnWorldModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
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

// -------------------------------------------------------------------------
// Attrib
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void Attrib::AssertOnClassCheck(int,int,void *)
{
    CGS_ASSERT(false, "Attrib::AssertOnClassCheck: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
struct Attrib::Collection * Attrib::FindCollectionWithDefault(int)
{
    CGS_ASSERT(false, "Attrib::FindCollectionWithDefault: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

// -------------------------------------------------------------------------
// Attrib::Instance
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void * Attrib::Instance::GetAttributePointer(unsigned __int64,unsigned int) const
{
    CGS_ASSERT(false, "GetAttributePointer: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

// -------------------------------------------------------------------------
// Attrib::RefSpec
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
struct Attrib::Collection const * Attrib::RefSpec::GetCollection()
{
    CGS_ASSERT(false, "RefSpec::GetCollection: link stub (world fleet mount) -- reconstruct from X360");
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
    CGS_ASSERT(false, "AIModule::Prepare: link stub (world fleet mount) -- reconstruct from X360");
    return false;
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
    CGS_ASSERT(false, "EnvironmentMap::Prepare: link stub (world fleet mount) -- reconstruct from X360");
    return false;
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
    CGS_ASSERT(false, "PhysicsModule::Prepare: link stub (world fleet mount) -- reconstruct from X360");
    return false;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnPhysics::PhysicsModule::PropPrepareTypes(class BrnPhysics::PhysicsModuleIO::InputBuffer *)
{
    CGS_ASSERT(false, "PhysicsModule::PropPrepareTypes: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnPhysics::PhysicsModule::UpdateNetworkCatchup(int,int)
{
    CGS_ASSERT(false, "PhysicsModule::UpdateNetworkCatchup: link stub (world fleet mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// BrnPhysics::Props::PropInputInterface
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnPhysics::Props::PropInputInterface::Append(struct BrnPhysics::Props::PropInputInterface const &)
{
    CGS_ASSERT(false, "PropInputInterface::Append: link stub (world fleet mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// BrnPhysics::Vehicle::VehicleManager
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnPhysics::Vehicle::VehicleManager::ReadSurfaceProperties()
{
    CGS_ASSERT(false, "VehicleManager::ReadSurfaceProperties: link stub (world fleet mount) -- reconstruct from X360");
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
    CGS_ASSERT(false, "TrafficEntityModule::Prepare: link stub (world fleet mount) -- reconstruct from X360");
    return false;
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
    CGS_ASSERT(false, "DebugComponent::Construct: link stub (world fleet mount) -- reconstruct from X360");
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
    CGS_ASSERT(false, "EnvironmentManager::Prepare: link stub (world fleet mount) -- reconstruct from X360");
    return false;
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
    CGS_ASSERT(false, "PropEntityModule::Prepare: link stub (world fleet mount) -- reconstruct from X360");
    return false;
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
    CGS_ASSERT(false, "RaceCarEntityModule::Prepare: link stub (world fleet mount) -- reconstruct from X360");
    return false;
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
// SetConstantsForEnvmap @0x827C1AD0 and the inlined accessor set now live in
// GameSource/World/ShadowMap/BrnShadowMap.cpp. ONLY CalculateShadowMapCameras
// remains: its X360 body @0x827DA820 drives ComputeBoundingBoxMatrix
// @0x827D91B0 (~2000 lines of VMX pseudocode) + ComputeOptimalViewVolume
// @0x827D8980 (~800 lines) -- the dependency set explodes; reconstruct as its
// own dedicated VMX pass (same treatment as the landed ComputeTSMMatrix).
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::ShadowMap::CalculateShadowMapCameras(struct rw::math::vpu::Vector3,class CgsGraphics::Camera const *)
{
    CGS_ASSERT(false, "ShadowMap::CalculateShadowMapCameras: link stub (world fleet mount) -- reconstruct from X360");
}

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

// -------------------------------------------------------------------------
// BrnWorld::WorldEntityIO::OutputBuffer_Prepare
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
struct CgsSceneManager::SceneManagerIO::InSceneUpdateInterface * BrnWorld::WorldEntityIO::OutputBuffer_Prepare::GetSceneInputInterface()
{
    CGS_ASSERT(false, "OutputBuffer_Prepare::GetSceneInputInterface: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

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
    CGS_ASSERT(false, "WorldEntityModule::PrepareMassive: link stub (world fleet mount) -- reconstruct from X360");
    return false;
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
// STILL STUBBED (each with a reason):
//   * the frustum family (GetFrustum(CameraRwFrustum&), GetFrustumParallel,
//     GetFrustumPerspective, GetCgsFrustumParallel): the real X360 writers
//     (@0x827F0AD8 / @0x827F11A8 / @0x827F97B8) are large VMX plane-derivation
//     pipelines with un-dumped vperm lane controls, AND the committed accessor
//     shapes (const-ref returns) diverge from the DWARF out-param signatures
//     (GetFrustumPerspective(Frustum&, bool) etc.) -- needs a reconciliation
//     pass of its own.
//   * GetViewProjectionMatrixModified @0x827EC858: the tail is a vperm/vmrghw
//     row-assembly puzzle over un-dumped lane controls (unk_82CDA3C0/400);
//     decoding it needs the same symbolic-evaluation treatment as
//     ComputeTSMMatrix.
// (Camera::Clear() remains declaration-only in the header -- never referenced
// by a linked TU, so it carries no stub here.)
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsGraphics::Camera::GetCgsFrustumParallel(struct CgsGeometric::Frustum *)
{
    CGS_ASSERT(false, "Camera::GetCgsFrustumParallel: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsGraphics::Camera::GetFrustum(struct CgsGraphics::CameraRwFrustum &)
{
    CGS_ASSERT(false, "Camera::GetFrustum: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.  (inert getter)
struct CgsGeometric::Frustum const & CgsGraphics::Camera::GetFrustumParallel() const
{
    static CgsGeometric::Frustum* sNull = 0; return *sNull;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.  (inert getter)
struct CgsGeometric::Frustum const & CgsGraphics::Camera::GetFrustumPerspective() const
{
    static CgsGeometric::Frustum* sNull = 0; return *sNull;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
struct rw::math::vpu::Matrix44 CgsGraphics::Camera::GetViewProjectionMatrixModified() const
{
    CGS_ASSERT(false, "GetViewProjectionMatrixModified: link stub (world fleet mount) -- reconstruct from X360");
    return rw::math::vpu::Matrix44();
}


// -------------------------------------------------------------------------
// CgsGraphics::DispatchBin
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsGraphics::DispatchBin::HandleMemoryOverflow(unsigned int)
{
    CGS_ASSERT(false, "DispatchBin::HandleMemoryOverflow: link stub (world fleet mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// CgsGraphics::DispatchList
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
class CgsGraphics::DispatchList * CgsGraphics::DispatchList::AllocateKeyBlock()
{
    CGS_ASSERT(false, "DispatchList::AllocateKeyBlock: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

// -------------------------------------------------------------------------
// CgsGraphics::DrawRenderable
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool CgsGraphics::DrawRenderable::AddToBin(struct Renderable const *,class CgsGraphics::DispatchFrame *,bool,signed char,signed char,unsigned char,unsigned char,bool,unsigned char,unsigned char,int,unsigned char)
{
    CGS_ASSERT(false, "DrawRenderable::AddToBin: link stub (world fleet mount) -- reconstruct from X360");
    return false;
}

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
    CGS_ASSERT(false, "CachedTriangleList::Prepare: link stub (world fleet mount) -- reconstruct from X360");
    return false;
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
    CGS_ASSERT(false, "FineIntersectionTestModule::Prepare: link stub (world fleet mount) -- reconstruct from X360");
    return false;
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
    CGS_ASSERT(false, "OverlapGenerationModule::Prepare: link stub (world fleet mount) -- reconstruct from X360");
    return false;
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

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::Append(struct CgsSceneManager::SceneManagerIO::InSceneUpdateInterface const &)
{
    CGS_ASSERT(false, "InSceneUpdateInterface::Append: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::SetCullingGroupPair(unsigned char,unsigned char,unsigned char)
{
    CGS_ASSERT(false, "InSceneUpdateInterface::SetCullingGroupPair: link stub (world fleet mount) -- reconstruct from X360");
}

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

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool CgsSceneManager::SceneManagerModule::Update(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,struct CgsSceneManager::SceneManagerIO::InputBuffer_Update *,struct CgsSceneManager::SceneManagerIO::OutputBuffer *,bool)
{
    CGS_ASSERT(false, "SceneManagerModule::Update: link stub (world fleet mount) -- reconstruct from X360");
    return false;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsSceneManager::SceneManagerModule::UpdateQueries(struct CgsModule::IOBufferStack *,struct CgsModule::IOBufferStack *,struct CgsSceneManager::SceneManagerIO::InputBuffer_Query *,struct CgsSceneManager::SceneManagerIO::OutputBuffer *)
{
    CGS_ASSERT(false, "SceneManagerModule::UpdateQueries: link stub (world fleet mount) -- reconstruct from X360");
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
    CGS_ASSERT(false, "SpatialPartitionManager::Prepare: link stub (world fleet mount) -- reconstruct from X360");
    return false;
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
    CGS_ASSERT(false, "TriangleCollisionManager::Prepare: link stub (world fleet mount) -- reconstruct from X360");
    return false;
}

// -------------------------------------------------------------------------
// CgsSceneManager::VolumeManager
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool CgsSceneManager::VolumeManager::Prepare()
{
    CGS_ASSERT(false, "VolumeManager::Prepare: link stub (world fleet mount) -- reconstruct from X360");
    return false;
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

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void WorldModule::BridgeTrafficToRaceCar_PrePhysics(void *,struct BrnWorld::RaceCarEntityModuleIO::InputBuffer_PrePhysics *,struct BrnTraffic::BrnTrafficIO::OutputBuffer_PostScene const *)
{
    CGS_ASSERT(false, "WorldModule::BridgeTrafficToRaceCar_PrePhysics: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void WorldModule::BridgeTriggerModuleToSceneModule_PostScene(void *,struct CgsSceneManager::SceneManagerIO::InputBuffer_Query *,class BrnWorld::TriggerEntityModuleIO::OutputBuffer_PostScene const *)
{
    CGS_ASSERT(false, "WorldModule::BridgeTriggerModuleToSceneModule_PostScene: link stub (world fleet mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// rw::BitTable
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void rw::BitTable::GetResourceDescriptor(struct rw::BaseResourceDescriptor *,int,int)
{
    CGS_ASSERT(false, "BitTable::GetResourceDescriptor: link stub (world fleet mount) -- reconstruct from X360");
}

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

