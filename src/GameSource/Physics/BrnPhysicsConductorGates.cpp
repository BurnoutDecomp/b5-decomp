// =================================================================================================
// GameSource/Physics/BrnPhysicsConductorGates.cpp -- ⚠⚠ THE CONDUCTOR'S NAMED DEFERRALS.
//
// 2026-08-09 (conductor wave): PhysicsModule::Update @0x825B0640 is REAL (its 1,999-insn body
// is landed in BrnPhysicsModuleUpdateFunctions.cpp, its call sequence complete and faithful) --
// but a measured ~8,200 instructions of its direct callee closure are not reconstructed yet.
// Every one of those callees is REACHED EVERY FRAME the moment the conductor runs, so a
// CGS_ASSERT(false) trap would block the sim on frame one. Each body below is therefore the
// repo's sanctioned BOOT-GATE shape instead: ONE log line per boot naming the symbol, its X360
// address and its insn count, then inert.
//
// ⛔ NEVER silently no-op these: the log-once IS the loudness. Reconstruct each body in its own
// TU and DELETE the gate here (duplicate-definition LNK2005 is the intended tripwire).
//
// What inert means per leg, honestly:
//   * contact generation end / validation / ordering -- the scene's potential contacts reach
//     the interface (SetConstQueue + the landed StartVehicleContactGeneration), but the
//     vehicle-vs-world harvest legs produce nothing: the car has NO GROUND CONTACTS from this
//     family. With the create path (Phase 2) still inert there are no bodies in the sim, so
//     nothing falls yet -- land the generation family BEFORE the create path, or the first
//     body added will drop through the world.
//   * DoCrashPrediction -- NOT dead-gated on the console: it runs unconditionally in the
//     non-catchup path. Deferral list = the L1/L2 web banked in BrnVehicleManager.h's banner.
//   * deformation Update/post/sensors/verify -- no deformation this wave.
//   * CheckState -- pure validation sweep; skipping it validates nothing (170 insns).
//
// ⭐⭐ THE TRACTION-LINE CHAIN, RESOLVED BY NAME 2026-08-10 (create-path wave) -- READ THIS
// BEFORE PLANNING THE CREATE PATH. The line above about contact generation ("land the generation
// family BEFORE the create path, or the first body added will drop through the world") is right
// about the ORDER and wrong about WHICH family. A Burnout car does not rest on contacts: contacts
// are the body-shell/crash path. The wheels rest on TRACTION LINE TESTS. The chain that ends in
// a wheel knowing it is on the ground is, per BrnSimpleVehiclePhysics.cpp:336 and the asm:
//     StartVehicleTractionLineTests -> {alloc, add tests, run jobs}
//       -> EndVehicleTractionLineTests -> ReadRaceCarTractionLineTestResults
//       -> RaceCarPhysics::AddTractionPoint -> SimpleVehiclePhysics::AddTractionPoint
//       -> Wheel::SetRoadContact   (which is what sets mRoadContact.mbIsOnGround)
// and only then does the landed UpdateSuspensionSprings have anything to push against.
//
// All thirteen members are in the export set (a name index over all 30,084 JSONs, 2026-08-10 --
// two of these were previously written off as holes, see BrnVehicleManagerLinkStubs.cpp):
//     0x82629CE0   78  VehicleManager::StartVehicleTractionLineTests            [gated below]
//     0x825B5098   52  VehicleManager::DoVehicleTractionLineAllocations
//     0x825E9640  313  VehicleManager::AddRaceCarTractionLineTests
//     0x8261D580  418  PhysicalTrafficManager::AddTrafficTractionLineTests
//     0x825E9B28  171  VehicleManager::AddPlayerStuckInCollisionLineTests
//     0x825E9DD8   87  VehicleManager::UpdatePlayerStuckInCollisionTest
//     0x825B5168   64  VehicleManager::RunTractionLineTestJobs
//     0x82633CD8   68  VehicleManager::EndVehicleTractionLineTests   [link stub, NOT a hole]
//     0x82618058  231  VehicleManager::ReadRaceCarTractionLineTestResults
//     0x8262D2B8  291  PhysicalTrafficManager::ReadTrafficTractionLineTestResults
//     0x825C3898  118  VehicleManager::ReadPlayerStuckTractionLineTestResults
//     0x825B5268   37  VehicleManager::DoVehicleTractionLineDecallocations
//     0x826185A0  548  VehicleManager::DoPlayerTractionLineTestsPostSimulation
// ⚠️ StartVehicleTractionLineTests calls its six callees UNCONDITIONALLY (pseudocode read, not
// inferred), so there is no cheap partial: bodying it alone would trade one gate for six. Treat
// the chain as one wave. Its two out-of-family dependencies are EA::Jobs (WaitOn is already
// mounted) and CgsMemory::DataStreamCommandPoster/SimpleDataStreamProducer.
//
// ⭐ ReadUpdatedBodies is NO LONGER in this file: it is real (BrnVehicleManager_ReadUpdatedBodies
// .cpp) and it is the gravity + integration step, not a read-back. Note what that means for
// sequencing -- with it landed and the traction chain still gated, a body in the sim would
// accelerate downward with nothing to stop it. THE GROUND IS THE TRACTION CHAIN; land it before
// the create path.
//   * ReadUpdatedBodies -- ⭐ DELETED 2026-08-10, it is real now (see
//     BrnVehicleManager_ReadUpdatedBodies.cpp). ⛔ AND THE LINE THAT WAS HERE WAS WRONG: it
//     called it "THE transform read-back". It reads no transform back from anywhere. It is the
//     per-frame gravity + ExternalPhysicsBody::IntegrateTransform step -- which makes the
//     conclusion ("even a stepping body would not move the game-side car") accidentally right
//     for the wrong reason: gated, a car had no gravity and no integration at all.
// =================================================================================================

#include "GameSource/Physics/BrnPhysicsModule.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"
#include "GameSource/Physics/PropManager/BrnPropManager.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint / gxMessageFilterFlags

namespace
{
    inline void GateLogOnce(bool& lrbLogged, const char* lpcMessage)
    {
        if (!lrbLogged)
        {
            lrbLogged = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << lpcMessage;
        }
    }
}

#define BRN_CONDUCTOR_GATE(TAG)                                                            \
    do { static bool s_bLogged = false;                                                    \
         GateLogOnce(s_bLogged, "conductor gate: " TAG " inert [FLAG PC boot gate]\n"); } while (0)

namespace BrnPhysics
{
    // @0x825A72F0 (185 insns; PS3 0x69CD60). The game-action dispatch -- drags ~10
    // VehicleManager mode/showtime/impact methods.
    void PhysicsModule::HandleGameActions(
        const BrnGameState::GameStateModuleIO::GameActionQueue*, PhysicsModuleIO::OutputBuffer*)
    {
        BRN_CONDUCTOR_GATE("PhysicsModule::HandleGameActions @0x825A72F0 (185)");
    }

namespace Vehicle
{
    void VehicleManager::CheckState()
    {
        BRN_CONDUCTOR_GATE("VehicleManager::CheckState @0x825EADA8 (170)");
    }

    void VehicleManager::StartVehicleTractionLineTests(CgsModule::IOBufferStack*,
                                                       const VehicleInputInterface*,
                                                       Deformation::DeformationManager*, f32)
    {
        BRN_CONDUCTOR_GATE("VehicleManager::StartVehicleTractionLineTests @0x82629CE0 (78)");
    }

    void VehicleManager::EndVehicleContactGeneration(
        const CgsSceneManager::SceneManagerIO::TriangleCacheInterface*,
        const CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::OutOverlapPair, 128>*,
        f32, BrnPhysics::Deformation::DeformationManager*, CgsModule::IOBufferStack*,
        CgsMemory::LinearMalloc*, BrnPhysics::PhysicsModuleIO::PotentialContactInterface*)
    {
        BRN_CONDUCTOR_GATE("VehicleManager::EndVehicleContactGeneration @0x8261AC38 (661)");
    }

    void VehicleManager::StartPartContactGeneration(
        const CgsSceneManager::SceneManagerIO::TriangleCacheInterface*, f32,
        BrnPhysics::Deformation::DeformationManager*, CgsModule::IOBufferStack*,
        BrnPhysics::PhysicsModuleIO::PotentialContactInterface*, CgsMemory::LinearMalloc*)
    {
        BRN_CONDUCTOR_GATE("VehicleManager::StartPartContactGeneration @0x8262C220 (114)");
    }

    void VehicleManager::EndPartContactGeneration(f32, BrnPhysics::Deformation::DeformationManager*,
                                                  CgsModule::IOBufferStack*,
                                                  BrnPhysics::PhysicsModuleIO::PotentialContactInterface*)
    {
        BRN_CONDUCTOR_GATE("VehicleManager::EndPartContactGeneration @0x8261B690 (276)");
    }

    void VehicleManager::DoRaceCarWorldContactValidation(
        BrnPhysics::PhysicsModuleIO::PotentialContactInterface*,
        const CgsSceneManager::SceneManagerIO::TriangleCacheInterface*, f32,
        BrnPhysics::Deformation::DeformationManager*)
    {
        BRN_CONDUCTOR_GATE("VehicleManager::DoRaceCarWorldContactValidation @0x825EB6C8 (416)");
    }

    void VehicleManager::DoTrafficWorldContactOrdering(
        BrnPhysics::PhysicsModuleIO::PotentialContactInterface*)
    {
        BRN_CONDUCTOR_GATE("VehicleManager::DoTrafficWorldContactOrdering @0x825C8F18 (143)");
    }

    // ⚠ NOT dead-gated on the console -- unconditional in the non-catchup path. The L1/L2
    // web is banked in BrnVehicleManager.h's census banner.
    void VehicleManager::DoCrashPrediction(CgsModule::IOBufferStack*, CgsModule::IOBufferStack*,
                                           f32, const VehicleInputInterface*, VehicleOutputInterface*,
                                           BrnPhysics::Vehicle::VehicleOutputRequestInterface*,
                                           VehicleManagerOutputInterface*,
                                           BrnPhysics::Deformation::DeformationInputInterface*,
                                           BrnPhysics::PhysicsModuleIO::PotentialContactInterface*)
    {
        BRN_CONDUCTOR_GATE("VehicleManager::DoCrashPrediction @0x82645FE0 (814 + the L1/L2 web)");
    }

    void VehicleManager::UpdateDrivers(f32, const VehicleDriverInputInterface*,
                                       BrnPhysics::Vehicle::VehicleOutputRequestInterface*,
                                       VehicleManagerOutputInterface*,
                                       BrnPhysics::Deformation::DeformationInputInterface*,
                                       VehicleOutputInterface*)
    {
        BRN_CONDUCTOR_GATE("VehicleManager::UpdateDrivers @0x82642C68 (120)");
    }

    void VehicleManager::ClearSnappedNetworkCarContacts(Deformation::DeformationManager*)
    {
        BRN_CONDUCTOR_GATE("VehicleManager::ClearSnappedNetworkCarContacts @0x8261A8D0 (217)");
    }

    // ⭐⭐ GATE DELETED 2026-08-10 (create-path wave): VehicleManager::ReadUpdatedBodies
    // @0x82619A10 is REAL, in BrnVehicleManager_ReadUpdatedBodies.cpp, together with the
    // PhysicalTrafficManager::ReadUpdatedBodies @0x825EF608 it tail-calls. It is the per-frame
    // gravity + IntegrateTransform step, not a read-back -- see that TU's banner. If a gate for
    // it ever reappears here the link will say so (LNK2005).

    void VehicleManager::GetUpdatedVehicleBodies(
        CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InUpdateExternalBody, 60>*)
    {
        BRN_CONDUCTOR_GATE("VehicleManager::GetUpdatedVehicleBodies @0x82619340 (export hole)");
    }

    void VehicleManager::UpdateVehiclePhysicsPostSimulation(
        const VehicleInputInterface*, const CgsPhysics::PhysicsSimulationIO::OutputBuffer*, f32,
        CgsModule::VariableEventQueue<1536, 16>*)
    {
        BRN_CONDUCTOR_GATE("VehicleManager::UpdateVehiclePhysicsPostSimulation @0x826426E0 (354)");
    }

    void VehicleManager::ProcessCrashingNetworkCars(
        const VehicleDriverInputInterface*, BrnPhysics::Vehicle::VehicleOutputRequestInterface*,
        VehicleManagerOutputInterface*, BrnPhysics::Deformation::DeformationInputInterface*,
        VehicleOutputInterface*)
    {
        BRN_CONDUCTOR_GATE("VehicleManager::ProcessCrashingNetworkCars @0x8263C7C0 (export hole)");
    }

    void VehicleManager::WriteOutVehicleStats(VehicleOutputInterface*)
    {
        BRN_CONDUCTOR_GATE("VehicleManager::WriteOutVehicleStats @0x8263F460 (380)");
    }

    void VehicleManager::ProcessResetEvents(const VehicleInputInterface*,
                                            BrnPhysics::Vehicle::VehicleOutputRequestInterface*,
                                            VehicleManagerOutputInterface*,
                                            BrnPhysics::Deformation::DeformationInputInterface*)
    {
        BRN_CONDUCTOR_GATE("VehicleManager::ProcessResetEvents @0x82617820 (526)");
    }

    void VehicleManager::ProcessContactSpies(const ContactSpy::ContactSpyData*,
                                             BrnPhysics::Vehicle::VehicleOutputRequestInterface*,
                                             VehicleOutputInterface*, VehicleManagerOutputInterface*,
                                             BrnPhysics::Deformation::DeformationInputInterface*,
                                             Deformation::DeformationManager*, f32)
    {
        BRN_CONDUCTOR_GATE("VehicleManager::ProcessContactSpies @0x82646C98 (118)");
    }

    void VehicleManager::UpdateFatalCrashFlags(VehicleOutputInterface*)
    {
        BRN_CONDUCTOR_GATE("VehicleManager::UpdateFatalCrashFlags @0x825EA970 (173)");
    }
}

namespace Deformation
{
    void DeformationManager::VerifyPartIndices()
    {
        BRN_CONDUCTOR_GATE("DeformationManager::VerifyPartIndices @0x826042F8 (165)");
    }

    void DeformationManager::UpdateSensorDisplacements(VecFloat)
    {
        BRN_CONDUCTOR_GATE("DeformationManager::UpdateSensorDisplacements @0x82604000 (189)");
    }

    void DeformationManager::Update(CgsPhysics::PhysicsSimulationIO::InputBuffer*,
                                    CgsPhysics::PhysicsSimulationIO::OutputBuffer*,
                                    const PhysicsModuleIO::InputBuffer*,
                                    PhysicsModuleIO::OutputBuffer*,
                                    PhysicsModuleIO::PotentialContactInterface*, VecFloat, s32)
    {
        BRN_CONDUCTOR_GATE("DeformationManager::Update @0x82649B40 (1021)");
    }

    void DeformationManager::UpdatePostPhysics(const CgsPhysics::PhysicsSimulationIO::OutputBuffer*,
                                               PhysicsModuleIO::OutputBuffer*, ContactSpy::ContactSpyData*,
                                               IOBufferStack*,
                                               const PhysicsModuleIO::PotentialContactInterface*)
    {
        BRN_CONDUCTOR_GATE("DeformationManager::UpdatePostPhysics @0x82630420 (236)");
    }

    // OutputData @0x826225D8 (339) has a REAL body in the unmounted BrnDeformationManager.cpp;
    // mounting that TU is its own closure job. This gate carries the seam until then -- the
    // mount DELETES it (LNK2005 says so loudly).
    void DeformationManager::OutputData(DeformationOutputInterfaceForEntityModules*,
                                        DeformationOutputInterface*)
    {
        BRN_CONDUCTOR_GATE("DeformationManager::OutputData @0x826225D8 (339; real body in the "
                           "unmounted BrnDeformationManager.cpp)");
    }
}

namespace Props
{
    void PropManager::BeginPropWorldContactGeneration(
        const CgsSceneManager::SceneManagerIO::TriangleCacheInterface*,
        CgsSceneManager::CgsCollision::CollisionGenerator*, CgsMemory::LinearMalloc*, VecFloat)
    {
        BRN_CONDUCTOR_GATE("PropManager::BeginPropWorldContactGeneration @0x82628CB0 (89)");
    }

    void PropManager::EndPropWorldContactGeneration(
        BrnPhysics::PhysicsModuleIO::PotentialContactInterface*,
        CgsSceneManager::CgsCollision::CollisionGenerator*, CgsSceneManager::EntityId)
    {
        BRN_CONDUCTOR_GATE("PropManager::EndPropWorldContactGeneration @0x82628E18 (37)");
    }

    void PropManager::ReadUpdatedBodies(
        const CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::OutUpdateRigidBody, 200>*,
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface*,
        CgsPhysics::PhysicsSimulationIO::InputBuffer*, VecFloat)
    {
        BRN_CONDUCTOR_GATE("PropManager::ReadUpdatedBodies @0x82632918 (752)");
    }

    void PropManager::OutputUpdatedProps(BrnPhysics::PhysicsModuleIO::OutputBuffer*)
    {
        BRN_CONDUCTOR_GATE("PropManager::OutputUpdatedProps @0x82627EC8 (14)");
    }
}
}
