// ===========================================================================
// WorldLinkStubs.cpp -- FLAG (world-fleet link-mount stubs, 2026-07-26).
//
// Minimal out-of-line definitions so the game exe LINKS with the world-module
// fleet TUs mounted (BrnWorldModule + entity modules + scene manager + shadow/
// environment + physics/AI IO surface). The world module is NOT driven yet:
// BrnGameModule.hpp still instantiates its own placeholder WorldModule, so none
// of these bodies execute on the boot -> title path. Every stub is either a
// CGS_ASSERT(false) trap (side-effectful call) or an inert-return getter.
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
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnAI::AIModule::Construct()
{
    CGS_ASSERT(false, "AIModule::Construct: link stub (world fleet mount) -- reconstruct from X360");
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
// BrnDirector::Camera::Camera
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnDirector::Camera::Camera::Clear()
{
    CGS_ASSERT(false, "Camera::Clear: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnDirector::Camera::Camera::CopyToCgsCamera(class CgsGraphics::Camera *) const
{
    CGS_ASSERT(false, "Camera::CopyToCgsCamera: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
struct rw::math::vpu::Vector3 BrnDirector::Camera::Camera::GetDirection() const
{
    CGS_ASSERT(false, "GetDirection: link stub (world fleet mount) -- reconstruct from X360");
    return rw::math::vpu::Vector3();
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
float BrnDirector::Camera::Camera::GetLodZoomFactor() const
{
    CGS_ASSERT(false, "GetLodZoomFactor: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
struct rw::math::vpu::Vector3 BrnDirector::Camera::Camera::GetPosition() const
{
    CGS_ASSERT(false, "GetPosition: link stub (world fleet mount) -- reconstruct from X360");
    return rw::math::vpu::Vector3();
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnDirector::Camera::Camera::IsInJunkyard() const
{
    CGS_ASSERT(false, "IsInJunkyard: link stub (world fleet mount) -- reconstruct from X360");
    return false;
}

// -------------------------------------------------------------------------
// BrnDirector::Camera::CameraEffects
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnDirector::Camera::CameraEffects::Construct()
{
    CGS_ASSERT(false, "CameraEffects::Construct: link stub (world fleet mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// BrnDirector::Camera::CameraState
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnDirector::Camera::CameraState::Construct()
{
    CGS_ASSERT(false, "CameraState::Construct: link stub (world fleet mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// BrnDirector::Camera::DepthOfField
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnDirector::Camera::DepthOfField::Construct()
{
    CGS_ASSERT(false, "DepthOfField::Construct: link stub (world fleet mount) -- reconstruct from X360");
}

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
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
int BrnMassive::BrnMassive::Construct()
{
    CGS_ASSERT(false, "BrnMassive::Construct: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
int BrnMassive::BrnMassive::Destruct()
{
    CGS_ASSERT(false, "BrnMassive::Destruct: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

// -------------------------------------------------------------------------
// BrnPhysics::PhysicsModule
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnPhysics::PhysicsModule::Construct()
{
    CGS_ASSERT(false, "PhysicsModule::Construct: link stub (world fleet mount) -- reconstruct from X360");
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
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnTraffic::TrafficEntityModule::Construct()
{
    CGS_ASSERT(false, "TrafficEntityModule::Construct: link stub (world fleet mount) -- reconstruct from X360");
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

// -------------------------------------------------------------------------
// BrnWorld::EnvironmentSettings::EnvironmentManager
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::EnvironmentSettings::EnvironmentManager::BeginRelease()
{
    CGS_ASSERT(false, "EnvironmentManager::BeginRelease: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
struct rw::math::vpu::Vector3 BrnWorld::EnvironmentSettings::EnvironmentManager::CalcKeyLightDirection() const
{
    CGS_ASSERT(false, "EnvironmentManager::CalcKeyLightDirection: link stub (world fleet mount) -- reconstruct from X360");
    return rw::math::vpu::Vector3();
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::EnvironmentSettings::EnvironmentManager::Construct()
{
    CGS_ASSERT(false, "EnvironmentManager::Construct: link stub (world fleet mount) -- reconstruct from X360");
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

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::InternalBaseStreamer::Construct(class BrnWorld::StreamerTargetEntry *,class BrnWorld::StreamerTargetEntry *,class BrnWorld::StreamerCurrentEntry *,int,int,enum BrnResource::EAssetSet,bool)
{
    CGS_ASSERT(false, "InternalBaseStreamer::Construct: link stub (world fleet mount) -- reconstruct from X360");
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
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::PVSDebugComponent::Construct(class BrnWorld::WorldEntityModule *)
{
    CGS_ASSERT(false, "PVSDebugComponent::Construct: link stub (world fleet mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// BrnWorld::PropEntityModule
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::PropEntityModule::CachePropGraphicsLists()
{
    CGS_ASSERT(false, "PropEntityModule::CachePropGraphicsLists: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::PropEntityModule::Construct()
{
    CGS_ASSERT(false, "PropEntityModule::Construct: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::PropEntityModule::ConstructPostPhysicsPerfMonitors()
{
    CGS_ASSERT(false, "PropEntityModule::ConstructPostPhysicsPerfMonitors: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::PropEntityModule::ConstructPreScenePerfMonitors()
{
    CGS_ASSERT(false, "PropEntityModule::ConstructPreScenePerfMonitors: link stub (world fleet mount) -- reconstruct from X360");
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
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::RaceCarEntityModule::Construct()
{
    CGS_ASSERT(false, "RaceCarEntityModule::Construct: link stub (world fleet mount) -- reconstruct from X360");
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
// BrnWorld::ShadowMap
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::ShadowMap::CalculateShadowMapCameras(struct rw::math::vpu::Vector3,class CgsGraphics::Camera const *)
{
    CGS_ASSERT(false, "ShadowMap::CalculateShadowMapCameras: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::ShadowMap::Construct()
{
    CGS_ASSERT(false, "ShadowMap::Construct: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
class CgsGraphics::Camera const * BrnWorld::ShadowMap::GetCascadeCamera(int) const
{
    CGS_ASSERT(false, "GetCascadeCamera: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnWorld::ShadowMap::GetRenderMultipleShadowMaps() const
{
    CGS_ASSERT(false, "GetRenderMultipleShadowMaps: link stub (world fleet mount) -- reconstruct from X360");
    return false;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnWorld::ShadowMap::GetRenderPropsIntoShadowMap() const
{
    CGS_ASSERT(false, "GetRenderPropsIntoShadowMap: link stub (world fleet mount) -- reconstruct from X360");
    return false;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnWorld::ShadowMap::GetRenderPropsNearOnly() const
{
    CGS_ASSERT(false, "GetRenderPropsNearOnly: link stub (world fleet mount) -- reconstruct from X360");
    return false;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnWorld::ShadowMap::GetRenderRaceCarsIntoShadowMap() const
{
    CGS_ASSERT(false, "GetRenderRaceCarsIntoShadowMap: link stub (world fleet mount) -- reconstruct from X360");
    return false;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnWorld::ShadowMap::GetRenderRaceCarsNearOnly() const
{
    CGS_ASSERT(false, "GetRenderRaceCarsNearOnly: link stub (world fleet mount) -- reconstruct from X360");
    return false;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnWorld::ShadowMap::GetRenderTrafficIntoShadowMap() const
{
    CGS_ASSERT(false, "GetRenderTrafficIntoShadowMap: link stub (world fleet mount) -- reconstruct from X360");
    return false;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnWorld::ShadowMap::GetRenderTrafficNearOnly() const
{
    CGS_ASSERT(false, "GetRenderTrafficNearOnly: link stub (world fleet mount) -- reconstruct from X360");
    return false;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnWorld::ShadowMap::GetRenderWorldIntoShadowMap() const
{
    CGS_ASSERT(false, "GetRenderWorldIntoShadowMap: link stub (world fleet mount) -- reconstruct from X360");
    return false;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool BrnWorld::ShadowMap::IsEnabled() const
{
    CGS_ASSERT(false, "IsEnabled: link stub (world fleet mount) -- reconstruct from X360");
    return false;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::ShadowMap::SetConstantsForEnvmap()
{
    CGS_ASSERT(false, "ShadowMap::SetConstantsForEnvmap: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::ShadowMap::SetCurrentCascadeIndex(unsigned int)
{
    CGS_ASSERT(false, "ShadowMap::SetCurrentCascadeIndex: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::ShadowMap::SetRenderingShadowMap(bool)
{
    CGS_ASSERT(false, "ShadowMap::SetRenderingShadowMap: link stub (world fleet mount) -- reconstruct from X360");
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
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::TriggerEntityModuleDebugComponent::Construct(class BrnWorld::TriggerEntityModule *)
{
    CGS_ASSERT(false, "TriggerEntityModuleDebugComponent::Construct: link stub (world fleet mount) -- reconstruct from X360");
}

// -------------------------------------------------------------------------
// BrnWorld::WorldDebugComponent
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void BrnWorld::WorldDebugComponent::Construct(class BrnWorld::WorldModule *)
{
    CGS_ASSERT(false, "WorldDebugComponent::Construct: link stub (world fleet mount) -- reconstruct from X360");
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
// BrnWorldIO::DispatchInputBuffer
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
struct BrnWorldIO::DispatchInputBuffer::RenderSwitches const * BrnWorldIO::DispatchInputBuffer::GetRenderSwitches() const
{
    CGS_ASSERT(false, "GetRenderSwitches: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

// -------------------------------------------------------------------------
// BrnWorldIO::DispatchOutputBuffer
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
struct rw::math::vpu::Vector4 BrnWorldIO::DispatchOutputBuffer::GetFogColourPlusWhiteLevel() const
{
    CGS_ASSERT(false, "GetFogColourPlusWhiteLevel: link stub (world fleet mount) -- reconstruct from X360");
    return rw::math::vpu::Vector4();
}

// -------------------------------------------------------------------------
// CgsAttribSys::AttribSysIO::AttribSysRequestInterface<2048>
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
bool CgsAttribSys::AttribSysIO::AttribSysRequestInterface<2048>::RegisterVault(class CgsModule::BaseEventReceiverQueue *,struct CgsResource::ResourceHandle,enum CgsAttribSys::AttribSysIO::EAttribSysVaultType)
{
    CGS_ASSERT(false, "AttribSysRequestInterface<2048>::RegisterVault: link stub (world fleet mount) -- reconstruct from X360");
    return false;
}

// -------------------------------------------------------------------------
// CgsDev::DebugInterface
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
class CgsDev::DebugRender & CgsDev::DebugInterface::GetRender()
{
    CGS_ASSERT(false, "DebugInterface::GetRender: link stub (world fleet mount) -- reconstruct from X360");
    static CgsDev::DebugRender* sNull = 0; return *sNull;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsDev::DebugInterface::RegisterVariable(int *,char const *,char const *)
{
    CGS_ASSERT(false, "DebugInterface::RegisterVariable: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsDev::DebugInterface::RegisterVariable(float *,char const *,char const *)
{
    CGS_ASSERT(false, "DebugInterface::RegisterVariable: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsDev::DebugInterface::RegisterVariable(bool *,char const *,char const *)
{
    CGS_ASSERT(false, "DebugInterface::RegisterVariable: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsDev::DebugInterface::SetRange(int *,int,int)
{
    CGS_ASSERT(false, "DebugInterface::SetRange: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsDev::DebugInterface::SetStep(int *,int)
{
    CGS_ASSERT(false, "DebugInterface::SetStep: link stub (world fleet mount) -- reconstruct from X360");
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
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
int CgsDev::PerfMonCpu::AddMonitor(char const *,int,int,double,int,int)
{
    CGS_ASSERT(false, "PerfMonCpu::AddMonitor: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
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
// CgsGraphics::Camera
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsGraphics::Camera::Clone(class CgsGraphics::Camera *)
{
    CGS_ASSERT(false, "Camera::Clone: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsGraphics::Camera::Construct()
{
    CGS_ASSERT(false, "Camera::Construct: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsGraphics::Camera::GetCgsFrustumParallel(struct CgsGeometric::Frustum *)
{
    CGS_ASSERT(false, "Camera::GetCgsFrustumParallel: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
struct rw::math::vpu::Vector3 CgsGraphics::Camera::GetDirection() const
{
    CGS_ASSERT(false, "GetDirection: link stub (world fleet mount) -- reconstruct from X360");
    return rw::math::vpu::Vector3();
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
struct rw::math::vpu::Vector3 CgsGraphics::Camera::GetPosition() const
{
    CGS_ASSERT(false, "GetPosition: link stub (world fleet mount) -- reconstruct from X360");
    return rw::math::vpu::Vector3();
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
struct rw::math::vpu::Matrix44 CgsGraphics::Camera::GetViewProjectionMatrixModified() const
{
    CGS_ASSERT(false, "GetViewProjectionMatrixModified: link stub (world fleet mount) -- reconstruct from X360");
    return rw::math::vpu::Matrix44();
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsGraphics::Camera::LookAt(struct rw::math::vpu::Vector3,struct rw::math::vpu::Vector3,struct rw::math::vpu::Vector3)
{
    CGS_ASSERT(false, "Camera::LookAt: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsGraphics::Camera::Release()
{
    CGS_ASSERT(false, "Camera::Release: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsGraphics::Camera::SetFarClip(float)
{
    CGS_ASSERT(false, "Camera::SetFarClip: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsGraphics::Camera::SetPerspectiveProjectionMatrixRightHanded()
{
    CGS_ASSERT(false, "Camera::SetPerspectiveProjectionMatrixRightHanded: link stub (world fleet mount) -- reconstruct from X360");
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsGraphics::Camera::UpdatePerspectiveProjectionMatrix()
{
    CGS_ASSERT(false, "Camera::UpdatePerspectiveProjectionMatrix: link stub (world fleet mount) -- reconstruct from X360");
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
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsSceneManager::FineIntersectionTestModule::Construct()
{
    CGS_ASSERT(false, "FineIntersectionTestModule::Construct: link stub (world fleet mount) -- reconstruct from X360");
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
// CgsSceneManager::SceneManagerModule
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
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void CgsSceneManager::SpatialPartitionManager::Construct()
{
    CGS_ASSERT(false, "SpatialPartitionManager::Construct: link stub (world fleet mount) -- reconstruct from X360");
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
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
int rw::collision::Volume::InitializeVTable()
{
    CGS_ASSERT(false, "Volume::InitializeVTable: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

// -------------------------------------------------------------------------
// rw::collision::VolumeVolumeQuery
// -------------------------------------------------------------------------
// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void * rw::collision::VolumeVolumeQuery::Initialize(void * *,int,int)
{
    CGS_ASSERT(false, "VolumeVolumeQuery::Initialize: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
}

// LINK STUB (world-fleet mount 2026-07-26): body not reconstructed yet.
void * rw::collision::VolumeVolumeQuery::GetResourceDescriptor(void *,int,int)
{
    CGS_ASSERT(false, "VolumeVolumeQuery::GetResourceDescriptor: link stub (world fleet mount) -- reconstruct from X360");
    return 0;
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

