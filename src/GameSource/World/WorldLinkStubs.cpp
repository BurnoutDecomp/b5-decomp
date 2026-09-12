// ===========================================================================
// WorldLinkStubs.cpp -- FLAG: world-fleet link-mount stubs.
//
// Minimal out-of-line definitions so the game exe LINKS with the world-module fleet
// mounted. The real WorldModule drives every stub below at boot or per frame, so each
// is a quiet one-shot-log no-op or an inert-return getter (never a trap on a per-frame
// path); the few CGS_ASSERT traps are on paths the PC boot does not reach.
//
// NEVER add behaviour here -- reconstruct the real body and then delete the stub (the
// two definitions must not coexist in one build). Each gate carries one note: what
// blocks it (missing declarations, or the home TU that is not on the build list).
//
// Families still stubbed here: the Massive SDK seam (uncommitted third party);
// Attribulator LiveLink (GameTalk); the FineIntersection / OverlapGeneration
// sub-manager seams (their TUs drag the rw::collision query closure); and module-fleet
// Destruct/Release leaves.
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
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropCellManager.h"        // PropCellManager contact-gen gates

// ---------------------------------------------------------------------------
// rw::collision::Volume -- same platform/SDK forward-declaration exception as
// CgsSceneManagerModule.cpp (there is no shared rw::collision::Volume header).
// Nothing in this file defines a member of it any more.
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
// ---------------------------------------------------------------------------
namespace CgsGraphics
{
    ::ShaderConstantTable mShaderConstantTable;
}

// Attrib::DecodeLiveLinkMessage -- declared here (its home TU attriblivelink.cpp is not
// reconstructed; CgsAttribSysModule.cpp forward-declares the same signature); body below.
namespace Attrib { void DecodeLiveLinkMessage(const char*); }

// -------------------------------------------------------------------------
// BrnAI::AIModule
// -------------------------------------------------------------------------
// Boot gate: reached every frame by WorldModule::Update; body not reconstructed. Quiet one-shot log, never a trap.
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

// Boot gate: reached every frame by WorldModule::Update; body not reconstructed. Quiet one-shot log, never a trap.
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
// BrnMassive::BrnMassive
// -------------------------------------------------------------------------
// FLAG PC-platform leaf: boot-gate no-op, reached at boot by WorldEntityModule::Construct
// (mMassive.Construct()). The real body is in GameSource/Massive/BrnMassive.cpp, which is
// not on the build list (it pulls the uncommitted Massive SDK closure); no subscriber is
// created and the MassiveAd client is never touched.
int BrnMassive::BrnMassive::Construct()
{
    return 0;
}

// Boot gate: reached every frame by WorldModule::Update; body not reconstructed. Quiet one-shot log, never a trap.
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
// MassiveAd client package, which is not linked). Link-required because the
// by-value pool BrnMassive::maSubscribers[15] inside the mounted WorldModule
// needs BrnMassiveSubscriber's default ctor/dtor.
// -------------------------------------------------------------------------
// FLAG PC-platform leaf: MUST be a quiet no-op, NOT a trap -- it runs at process exit
// through the static gGameModule destructor chain (~WorldModule -> ~WorldEntityModule ->
// ~BrnMassive -> 15x ~BrnMassiveSubscriber -> this base dtor), and an assert during CRT
// static destruction is not survivable. No subscriber is ever placement-constructed on
// the boot path, so there is nothing to tear down.
MassiveAdClient3::CMassiveAdObjectSubscriber::~CMassiveAdObjectSubscriber()
{
}

// Pulled by the scalar deleting dtor in the emitted vtable; never invoked (pool slots are
// by-value members, never heap-deleted). Quiet one-shot log, never a trap.
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
// BrnSound::Module::Io::SoundWorldLoadEvent
// -------------------------------------------------------------------------
// The two member stores; no owning BrnSound IO TU is in the link.
void BrnSound::Module::Io::SoundWorldLoadEvent::Construct(
    enum BrnSound::Module::Io::SoundWorldLoadEvent::eLoadEvent leEvent,
    unsigned short lu16Zone)
{
    meEvent = leEvent;
    mu16Zone = lu16Zone;
}

// -------------------------------------------------------------------------
// BrnTraffic::BrnTrafficIO::InputBuffer_Dispatch
// -------------------------------------------------------------------------
// Boot gate: reached every frame by WorldModule::Update; body not reconstructed. Quiet one-shot log, never a trap.
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
// BrnTraffic::TrafficEntityModule
// -------------------------------------------------------------------------
// Boot gate: reached every frame by WorldModule::Update; body not reconstructed. Quiet one-shot log, never a trap.
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

// Boot gate: reached every frame by WorldModule::Update; body not reconstructed. Quiet one-shot log, never a trap.
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
// BrnWorld::PropEntityModule
// -------------------------------------------------------------------------
// Boot gate: reached every frame by WorldModule::Update; body not reconstructed. Quiet one-shot log, never a trap.
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

// -------------------------------------------------------------------------
// BrnWorld::RaceCarEntityModule
// -------------------------------------------------------------------------
// Boot gate: reached every frame by WorldModule::Update; body not reconstructed. Quiet one-shot log, never a trap.
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

// Boot gate: reached every frame by WorldModule::Update; body not reconstructed. Quiet one-shot log, never a trap.
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
// BrnWorld::WorldEntityModule
// -------------------------------------------------------------------------
// Boot gate: the dispatch-list producer's Massive impression feed (uncommitted third-party
// SDK, as UpdateMassive below); one-shot log + inert so the dispatch pass keeps running.
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

// Boot gate: reached by the world Prepare stage chain; reports success so the scripted load
// advances (the Massive SDK is uncommitted, so there is nothing to prepare).
bool BrnWorld::WorldEntityModule::PrepareMassive(struct BrnWorld::WorldEntityIO::OutputBuffer_Prepare *)
{
    static bool s_bLogged = false;
    if (!s_bLogged)
    {
        s_bLogged = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "WorldEntityModule::PrepareMassive: inert [FLAG PC boot gate]\n";
    }
    return true;
}

// Boot gate: reached every frame by WorldEntityModule::PreSceneUpdate. The Massive
// in-game-advertising SDK is uncommitted, so the per-frame impression update has nothing
// to drive; one-shot log + inert, never a trap.
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
// CgsDev::PerfMonCpu
// -------------------------------------------------------------------------
// Forwarder: the 6-arg hierarchical console form is reached at boot by the
// SceneManagerModule::Construct perf-mon block. Forwards into the live 5-arg registry
// (colour == the page id at every call site; liFlags is the libperf tag). The
// parent-handle nesting is NOT modelled by the PC registry yet -- reconstruct the
// hierarchical registry before relying on the perfmon tree view.
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
// CgsSceneManager::FineIntersectionTestModule
// -------------------------------------------------------------------------
// Both real bodies are in GameShared/.../FineIntersectionTestModule/CgsFineIntersectionTestModule.cpp,
// which is not on the build list (it drags the rw::collision query closure). Construct is
// reached at boot by SceneManagerModule::Construct's sub-manager cascade: quiet no-op.
// FLAG PC-platform leaf: boot-gate no-op.
void CgsSceneManager::FineIntersectionTestModule::Construct()
{
}

// Prepare is reached by the world Prepare chain; reports success so the scripted load
// advances, and the sub-manager stays inert (zero-initialised storage).
bool CgsSceneManager::FineIntersectionTestModule::Prepare(class CgsSceneManager::EntityManager *,class CgsSceneManager::VolumeManager *)
{
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
// Destruct is the one member of this module with no body anywhere (the rest are real in
// ContactGen/CgsOverlapGenerationModule.cpp). Quiet one-shot log, never a trap.
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
// The (EntityId, Matrix44Affine) overload; the VolumeInstanceId overload is real in
// CgsSceneManagerIO_SceneUpdate.cpp. Body not reconstructed. Quiet one-shot log, never a trap.
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

// ---------------------------------------------------------------------------
// Attrib mount closure: GameTalk live-edit decode (attrib gap G6). Traps loudly; it is
// not on the schema/vault-register path.
// ---------------------------------------------------------------------------
void Attrib::DecodeLiveLinkMessage(char const *)
{
    CGS_ASSERT(false, "Attrib::DecodeLiveLinkMessage: attrib gap G6 -- reconstruct");
}

// ---------------------------------------------------------------------------
// RealmcIface::MemcardInterface base ctor/dtor -- trivial real bodies (the console
// ctor is a single vtable store == an empty C++ ctor; the virtual dtor backs the
// vector-deleting slot). Their home RealmcMemcardInterface.cpp is NOT linked because
// its CreateInstance drags the uncommitted RealmcCore closure (ObjectManager /
// AllocateMem / Locale callbacks); the SaveLoad TU needs only this base pair for
// its NoOpMemcardInterface.
// ---------------------------------------------------------------------------
RealmcIface::MemcardInterface::MemcardInterface()
{
}

RealmcIface::MemcardInterface::~MemcardInterface()
{
}

// ===========================================================================
// WORLD-DRIVE BOOT GATES -- per-frame bridges and module entry points that
// WorldModule::Update and its entity-module phase spines call and that are not
// reconstructed. Each is a QUIET ONE-SHOT-LOG NO-OP (never a CGS_ASSERT trap: a
// trap here blocks the sim on the first frame); delete the gate when the real
// body lands (the two definitions must not coexist).
// ===========================================================================


// ---- module entry points driven by the spines ----------------------------

// Boot gate: the network catch-up step WorldModule::UpdatePhysicsNetworkCatchup forwards to;
// body not reconstructed. Quiet one-shot log, never a trap.
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

// ---- module-IO buffer Construct() ------------------------------------------
// CgsModule::IOBufferStack::CreateIOBuffer<T> is DEFAULT-init + T::Construct, as on the
// console. FLAG PC: a buffer whose real Construct is not recovered zeroes ITS OWN storage
// here (the per-T escape hatch, never the template) and runs the IOBuffer base bring-up
// (raising the status the Lock/Unlock tripwires assert on). DELETE the memset together
// with the gate when the real Construct lands in the owning IO TU.

// BOOT GATE: base bring-up only (see the block note above).
void BrnTraffic::BrnTrafficIO::OutputBuffer_PostScene::Construct()
{
    memset(this, 0, sizeof(*this));   // FLAG PC: stands in for the unrecovered body
    CgsModule::IOBuffer::Construct();
    // The console body inlines TrafficAIInterface::Construct (BrnTrafficAIInterfaces.cpp);
    // without it the mUpdateRivalQueue the AI input buffer copies via SetTrafficAIInterface
    // has mpEvents == NULL. The other post-scene legs are still the memset stand-in above.
    mTrafficAIInterface.Construct();
    // Console +4: the coarse-query staging queue WorldModule::BridgeTrafficModuleToSceneModule_
    // PostScene merges into the scene query input buffer. Append asserts it is Constructed.
    mSceneCoarseQueryQueue.Construct();
}

// ---------------------------------------------------------------------------
// CgsResource::ShaderTechniqueResourceType -- the one member its own TU still
// documents as DEFERRED:
//   GetShaderConstantExternalSerialisedResourceDescriptorSize -- its private
//   descriptor-size helper (serialiser path only; no runtime caller).
// The type is REGISTERED, so the vtable is emitted and the linker needs the symbol.
// ---------------------------------------------------------------------------
uint32_t CgsResource::ShaderTechniqueResourceType::GetShaderConstantExternalSerialisedResourceDescriptorSize(
    const ShaderConstantsExternal* /*lpBlock*/) const
{
    CGS_ASSERT(false, "ShaderTechniqueResourceType::GetShaderConstantExternalSerialisedResourceDescriptorSize: documented deferral -- reconstruct");
    return 0;
}
