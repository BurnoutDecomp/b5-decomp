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
//     ⛔⛔ CORRECTED 2026-08-10 (create-path wave), and the correction matters for sequencing:
//     "no bodies in the sim, so nothing falls yet" understates it in one direction and
//     overstates it in another. The SIM is not the mechanism -- BrnVehicleManager_ReadUpdatedBodies
//     .cpp's gravity + IntegrateTransform loop is, it is MOUNTED, it is called every frame from
//     PhysicsModule::Update, and its only gate is a set bit in mUsedRaceCars. So the first body to
//     FALL will not need rw::physics at all; it will need only VehicleManager::ProcessCreateEvents
//     to set that bit. The traction chain is still the prerequisite; the reason is one function
//     earlier than this note said.
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
// inferred), so there is no cheap partial: bodying it alone would trade one gate for six.
//
// ⭐⭐ WHAT THE GROUND ACTUALLY COSTS -- measured 2026-08-10 (ground wave), so that the sentence
// above ("treat the chain as one wave") is not read as an estimate. The thirteen are ~2,500 insns;
// they are NOT the job. Four of them bottom out in machinery this tree does not have:
//   * THE COMMANDS CARRY TRIANGLES. AddRaceCarTractionLineTests @0x825E9640 builds a 176-byte
//     command whose payload is {4x line start, 4x line end, Triangle4* lpTriangles, s32
//     miNumTriangles, s32 numLines=4}, taking the triangle list from a per-object TRIANGLE CACHE
//     -- it asserts "lpCacheInterface != NULL" / "mpTriangleCacheManager != NULL" /
//     "mpaTriangleCache != NULL" and then dereferences it.
//     ⚠️ CORRECTED 2026-08-10 (triangle-cache wave): the previous text here said
//     "CgsSceneManager::TriangleCacheManager is ABSENT" and listed all six methods as missing.
//     That was WRONG IN BOTH DIRECTIONS and re-measured against the name index over all 30,084
//     X360 exports. The manager's TU has been mounted since before this note was written, and
//     the whole READ side -- the API AddRaceCarTractionLineTests actually calls -- is bodied:
//         Prepare 88 [LANDED, and REACHED via SceneManagerModule::Prepare's CACHE_MANAGER stage]
//         GetTrianglesForCachedObject 32 [LANDED] · TriangleCacheInterface::GetCache 27 [LANDED]
//         GetNumCachedTriangleBatches 31 [LANDED] · Append [LANDED, inlined]
//     and as of this wave the SLOT BOOKKEEPING is bodied too (CgsTriangleCacheManager_Events.cpp):
//         ProcessAddToCacheEvents 222 · ProcessRemoveFromCacheEvents 346 ·
//         ProcessUpdateCachedPositionEvents 279 · CacheSlot::UpdateCachedObject 54
//     What is ACTUALLY missing is the FILL half -- the part that puts triangles in the cache:
//         StartUpdateTriangleCaches 278 · EndUpdateTriangleCaches **0x828BF150** 475
//         (both still WorldLinkStubs gates, and both REACHED EVERY FRAME by WorldModule::Update
//          through SceneManagerModule::Start/EndUpdateTriangleCache -- so they are inert stubs on
//          a live path, not unreachable code), plus SceneManagerModule::StartUpdateTriangleCache
//         73, VehicleManager::UpdateTriangleCache 240 / PrepareTriangleCache 37, and the
//         PolygonSoupTesterJob fill path (FillTriangleCache 219, ExecuteFillTriangleCache 170,
//         ExecuteFillTriangleCacheStream 145).
//   * SimpleVehiclePhysics::GetTractionLine @0x825D85C0 -- a GENUINE export hole, re-verified this
//     wave the way the rule demands rather than repeated: the export dir goes
//     GetIndexOfOtherHalf @0x825D8490 (76 insns, so it ENDS exactly at 0x825D85C0) -> the next
//     export at 0x825D8878, and a name-index sweep over all 30,084 exports returns no
//     GetTractionLine at all. Span 0x825D85C0..0x825D8874 == **174 instructions**, image-only.
//   * THE TEST ITSELF is a job: RunTractionLineTestJobs -> BaseCollisionGenerator::RunLineWith-
//     TriangleListStream @0x82810E80 (90; export hole, address proved by decoding the bl word at
//     0x825B5248) -> ContactGeneratorJob::ExecuteLineWithTriangleListStream @0x82921968 (589) over
//     CgsGeometric::IntersectLinePolygonSoupNearestSingleSided 575 /
//     PolygonSoupListSpatialMap::RunQuery 261. All absent.
// ⇒ the ground is ~6,000 insns across ~40 functions in four subsystems: several waves, not one.
//
// ⭐ LANDED 2026-08-10 (ground wave), real bodies, mounted for link closure but NOT wired in --
// BrnVehicleManager_TractionLineTests.cpp has DoVehicleTractionLineAllocations,
// RunTractionLineTestJobs, DoVehicleTractionLineDecallocations and ReadRaceCarTractionLineTest-
// Results; CgsCollisionGenerator_LineStream.cpp has CreateLineWithTriangleListStream.
// ⛔ AND THE TWO HALVES ARE LIFETIME-COUPLED, which is why neither can go live alone:
// mpTractionLineStreamProducer is allocated inside Start and nulled inside End, and End
// dereferences it with no null guard while the mounted UpdateVehiclePhysics reaches End every
// frame. Land Start and End in the SAME wave, after the triangle cache.
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

    // =============================================================================================
    // ⭐ ADDED 2026-08-10 (create-path wave): the three PostSceneUpdate callees whose own closures
    // are not reconstructed, plus THE SIM FIREWALL. PhysicsModule::PostSceneUpdate @0x825ABC10 is
    // REAL as of this wave (its WorldLinkStubs boot gate is deleted), so every one of these is now
    // REACHED EVERY FRAME -- which is exactly why each gets a named, logged deferral rather than a
    // silent no-op or a trap.
    // =============================================================================================

    // @0x825A70C0 (63 insns). The POST-scene game-action dispatch. Distinct from HandleGameActions
    // above and much smaller: DeformationManager::ProcessDebugResetDeformationModels, then a switch
    // over the same VariableEventQueue<13312,16> with four arms -- 23 OnPrepareGameMode,
    // 34 OnStartGameMode, 97 ProcessResetDeformationModelEvent (bracketed by two VerifyPartIndices
    // sweeps), 99 OnJunkYardDriveThru. None of those five callees is reconstructed.
    void PhysicsModule::HandleGameActionsPostScene(
        const BrnGameState::GameStateModuleIO::GameActionQueue*,
        CgsPhysics::PhysicsSimulationIO::InputBuffer*,
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface*)
    {
        BRN_CONDUCTOR_GATE("PhysicsModule::HandleGameActionsPostScene @0x825A70C0 (63)");
    }

    // ⭐⭐ GATE DELETED 2026-08-11 (prepare-chain wave): PhysicsModule::
    // BridgeVehicleManagerToSimulation_PostScene @0x825AB408 -- THE SIM FIREWALL -- is REAL, in
    // BrnPhysicsModuleBridgeFunctions.cpp next to its PostPhysics sibling. The ⛔ precondition
    // that held it here ("do not land until the traction-line chain is closed") was CHECKED, not
    // assumed: StartVehicleTractionLineTests @0x82629CE0 and EndVehicleTractionLineTests
    // @0x82633CD8 both carry real bodies in BrnVehicleManager_TractionLineTests.cpp as of the
    // 2026-08-11 lifetime wave. If a gate for it ever reappears here the link will say so
    // (LNK2005).

namespace Deformation
{
    // ⭐⭐ GATE DELETED 2026-08-14 (deformation-mount wave): DeformationManager::PostSceneUpdate
    // @0x82644F40's REAL body (BrnDeformationManager.cpp -- the perfmon pair bracketing
    // ProcessEvents, whose four model-event drains are all real) is MOUNTED as of this wave. The
    // registration consumer now RUNS: the add-model events the producer has been queuing on every
    // create are drained, the model table goes live (table != -1), and the
    // StartVehicleContactGeneration skip line goes with it. If a gate for it reappears here the
    // link will say so (LNK2005).
}

namespace Props
{
    // @0x8263AF30 (209 insns). Drains the prop input interface's add/remove prop-and-part instance
    // queues and re-runs the jointed-prop update. All six of its callees
    // (ProcessRemove/AddPropInstanceEvents, ProcessRemove/AddPartInstanceEvents,
    // RemoveAllPropsAndParts, UpdateJointedProps) are unreconstructed.
    void PropManager::ProcessInputsPreScene(
        const BrnPhysics::Props::PropInputInterface*,
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface*,
        bool,
        CgsPhysics::PhysicsSimulationIO::InputBuffer*)
    {
        BRN_CONDUCTOR_GATE("PropManager::ProcessInputsPreScene @0x8263AF30 (209)");
    }
}

namespace Vehicle
{
    void VehicleManager::CheckState()
    {
        BRN_CONDUCTOR_GATE("VehicleManager::CheckState @0x825EADA8 (170)");
    }

    // ⭐⭐ 2026-08-11 (lifetime wave): the StartVehicleTractionLineTests @0x82629CE0 gate that
    // stood here is DELETED. The real 78-instruction body is in
    // BrnVehicleManager_TractionLineTests.cpp, alongside the matching EndVehicleTractionLineTests
    // (whose link stub in BrnVehicleManagerLinkStubs.cpp is deleted with it -- they are one
    // lifetime and can never be split; End dereferences mpTractionLineStreamProducer with no null
    // guard and UpdateVehiclePhysics reaches it unconditionally every frame).
    //
    // What replaces it is SMALLER and PAIRED: six named gates for the two SIDE legs the race-car
    // ground path does not need (1,319 console instructions measured this wave). ⛔ Each Add is
    // gated WITH its own Read, because EndVehicleTractionLineTests hands ONE result cursor to the
    // three harvests in turn -- an Add without its Read leaks records into the next harvest, and a
    // Read without its Add consumes the race-car leg's answers.
    //
    // ⚠️ These six are reached EVERY FRAME as of this wave (the lifetime above them is live), so
    // each is the repo's log-once boot-gate shape, not a silent no-op. The three that return a
    // command count return 0, which is the truthful "posted nothing".

    s32 VehicleManager::AddPlayerStuckInCollisionLineTests(
            CgsSceneManager::CgsCollision::CollisionGenerator*,
            const CgsSceneManager::SceneManagerIO::TriangleCacheInterface*,
            BrnPhysics::Deformation::DeformationManager*)
    {
        BRN_CONDUCTOR_GATE("VehicleManager::AddPlayerStuckInCollisionLineTests @0x825E9B28 (171) "
                           "-- PAIRED with ReadPlayerStuckTractionLineTestResults @0x825C3898; "
                           "reconstruct and un-gate BOTH together");
        return 0;
    }

    void VehicleManager::UpdatePlayerStuckInCollisionTest(
            const CgsSceneManager::SceneManagerIO::TriangleCacheInterface*, f32)
    {
        BRN_CONDUCTOR_GATE("VehicleManager::UpdatePlayerStuckInCollisionTest @0x825E9DD8 (87) "
                           "-- player-stuck leg (drives UpdatePlayerStuckInCollisionSpheres)");
    }

    void VehicleManager::UpdatePlayerStuckInCollisionSpheres(f32)
    {
        BRN_CONDUCTOR_GATE("VehicleManager::UpdatePlayerStuckInCollisionSpheres @0x825C4AB8 (147) "
                           "-- player-stuck leg");
    }

    void VehicleManager::ReadPlayerStuckTractionLineTestResults(
            CgsMemory::SimpleDataStreamResultIterator*)
    {
        BRN_CONDUCTOR_GATE("VehicleManager::ReadPlayerStuckTractionLineTestResults @0x825C3898 "
                           "(118) -- PAIRED with AddPlayerStuckInCollisionLineTests @0x825E9B28; "
                           "leaving it inert is CORRECT while the Add posts nothing");
    }

    // ⭐⭐ GATE DELETED 2026-08-14 (walls leg 3): VehicleManager::EndVehicleContactGeneration
    // @0x8261AC38 (661) is REAL in BrnVehicleManagerContactGeneration.cpp — the harvest runs.

    // ⭐ GATE MOVED 2026-08-14 (walls leg 3): VehicleManager::StartPartContactGeneration
    // @0x8262C220 is a PARTIAL body in BrnVehicleManagerContactGeneration.cpp now — its FIRST
    // store (the miFirstPartContactGenEntry boundary stamp the harvest reads) is real; the
    // part-contact-generation tail stays a named log-once gate there.

    void VehicleManager::EndPartContactGeneration(f32, BrnPhysics::Deformation::DeformationManager*,
                                                  CgsModule::IOBufferStack*,
                                                  BrnPhysics::PhysicsModuleIO::PotentialContactInterface*)
    {
        BRN_CONDUCTOR_GATE("VehicleManager::EndPartContactGeneration @0x8261B690 (276)");
    }

    // ⭐⭐ GATE DELETED 2026-08-14 (walls leg 3): VehicleManager::DoRaceCarWorldContactValidation
    // @0x825EB6C8 (416) is REAL in BrnVehicleManagerContactGeneration.cpp, and its callee
    // ValidateRaceCarWorldContact @0x825C6088 (988) is REAL in
    // BrnVehicleManager_ValidateRaceCarWorldContact.cpp.

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

    // ⭐⭐ 2026-08-11 (prepare-chain wave): the VehicleManager::UpdateDrivers @0x82642C68 gate that
    // stood here is DELETED. The real 120-instruction body is in
    // BrnVehicleManager_UpdateDrivers.cpp -- it drains the driver-control queue, runs the
    // fixed-step steering decay and publishes the target-assist list.
    //
    // ⭐⭐ ...AND AS OF 2026-08-11 (driver-arms wave) ALL FIVE OF ITS ARMS ARE GONE FROM HERE TOO.
    // The five gates that stood at this seat for one wave are DELETED; every one has a real body:
    //     UpdatePlayerDriver   @0x825E9F38  401 -> BrnVehicleManager_DriverArms.cpp
    //     UpdateNetworkDriver  @0x825C4D08  258 -> BrnVehicleManager_DriverArms.cpp
    //     UpdateAIDriver       @0x825C5110  185 -> BrnVehicleManager_DriverArms.cpp
    //     DoHornTakedowns      @0x8263CC68  333 -> BrnVehicleManager_DriverArms.cpp
    //     UpdateTrafficDriver  @0x825CA8A0  169 -> BrnPhysicalTrafficManager_UpdateTrafficDriver.cpp
    //                          ------------ 1,346 console instructions, all landed, none parked.
    // The one absent callee across the five (BrnTraffic::GetVehicleSpecies @0x821F4648, 32 insns)
    // was landed with them, in its own console home BrnTrafficVehicle.h. If a gate for any of the
    // five ever reappears here the link will say so (LNK2005).
    //
    // ⭐ THE HONEST CONSEQUENCE, RE-UPDATED (conductor, same day). The record LANDS in
    // VehicleManager::maRaceCarDrivers[car].mControls -- and the "one more seam"
    // (VehicleDriver::UpdateVehicle as a missing controls-installer) was a MIS-DIAGNOSIS:
    // that function is landed (BrnVehicleDriver.cpp, @0x825D7290) and is the slerp-transform
    // applier, never touching mControls. No installer exists or is needed -- the mounted
    // per-car loop passes maRaceCarDrivers[liCar].GetControls() straight into
    // VehiclePhysics::Update (BrnVehicleManager_UpdateVehiclePhysics.cpp:504), which memcpy's
    // it into mPreviousControls itself. The controls chain is CLOSED end to end as of this
    // wave. See the corrected closing banner in BrnVehicleManager_DriverArms.cpp.

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

    // ⭐⭐ GATE DELETED 2026-08-11 (physics->output publish wave): VehicleManager::WriteOutVehicleStats
    // @0x8263F460 is REAL, in BrnVehicleManager_WriteOutVehicleStats.cpp, together with the
    // VehicleManager::IsRaceCarHidden @0x825C2EA0 it calls (which was a trap stub, not a hole) and
    // the VehicleOutputInterface::UpdateRaceCarState @0x825EC808 that does the actual publishing
    // (which was ABSENT FROM THE TREE ALTOGETHER -- not on any gate list). That trio is the whole
    // physics->output publish; with it landed, VehicleOutputInterface::mUsedRaceCars and
    // maRaceCarStates[] are written every frame and the world-side readback stops being gated off.
    // If a gate for it ever reappears here the link will say so (LNK2005).
    //
    // ⛔ ...but the TRAFFIC half of the same publish is NOT landed, so it keeps a gate of its own.
    // The console's own call sequence is preserved: WriteOutVehicleStats tail-calls this.
    void PhysicalTrafficManager::WriteOutVehicleStats(VehicleOutputInterface*)
    {
        BRN_CONDUCTOR_GATE("PhysicalTrafficManager::WriteOutVehicleStats @0x825F0308 (481) -- "
                           "the TRAFFIC half of the per-frame publish; race-car half is LANDED");
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

    // ---- 2026-08-11 (lifetime wave): the TRAFFIC traction-line pair. Same pairing rule as the
    // player-stuck four above -- Add and Read are one leg and must be un-gated together.
    s32 PhysicalTrafficManager::AddTrafficTractionLineTests(
            CgsSceneManager::CgsCollision::CollisionGenerator*,
            CgsMemory::SimpleDataStreamProducer*,
            const CgsSceneManager::SceneManagerIO::TriangleCacheInterface*)
    {
        BRN_CONDUCTOR_GATE("PhysicalTrafficManager::AddTrafficTractionLineTests @0x8261D580 (418) "
                           "-- PAIRED with ReadTrafficTractionLineTestResults @0x8262D2B8; "
                           "reconstruct and un-gate BOTH together");
        return 0;
    }

    void PhysicalTrafficManager::ReadTrafficTractionLineTestResults(
            CgsMemory::SimpleDataStreamResultIterator*)
    {
        BRN_CONDUCTOR_GATE("PhysicalTrafficManager::ReadTrafficTractionLineTestResults @0x8262D2B8 "
                           "(291) -- PAIRED with AddTrafficTractionLineTests @0x8261D580; leaving "
                           "it inert is CORRECT while the Add posts nothing");
    }

    // ---- 2026-08-11 (driver-arms wave): the TRAFFIC arm gate that stood here for one wave is
    // DELETED. PhysicalTrafficManager::UpdateTrafficDriver @0x825CA8A0 (169) is REAL, in
    // BrnPhysicalTrafficManager_UpdateTrafficDriver.cpp, together with the four VehicleManager arms
    // noted in the Vehicle namespace above. LNK2005 if it ever comes back.

    // ---- 2026-08-11 (lifetime wave): the two NON-VEHICLE arms of
    // PhysicsModule::UpdateCachedPositions @0x8259C370 (bodied this wave in BrnPhysicsModule.cpp).
    // ⭐ WHY GATING THESE TWO IS NOT THE FORBIDDEN "PARTIAL": the five per-manager
    // UpdateTriangleCache producers write INDEPENDENT per-slot InEventUpdateCachedPosition
    // events into ONE queue whose consumer (CacheSlot::UpdateCachedObject, landed and draining
    // every frame) is per-slot. There is no lifetime pair and no count invariant between
    // managers. And props/deformation own ZERO triangle-cache slots today -- the runtime witness
    // is `usedSlots=28`, which is exactly KI_MAX_ACTIVE_RACE_CARS(8) +
    // KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC(20) -- so no consumer can read a stale prop sphere. A
    // manager that does not push leaves its own unclaimed slots where they already are.
    // Their closures are 573 (props: GetTriangleCacheSlotAndRadius 247 + UpdateTriangleCache 116
    // + UpdatePro 41 + Has{Part,Prop}JustBeenRemoved 2x99 ...) and 585 (deformation:
    // DetachedWheel 264 + DetachedPart 113 + PhysicalBodyPartPool::GetPart 110 + the conductor 35
    // + IsPartIndexUsed 63) console instructions.
}

namespace Deformation
{
    void DeformationManager::VerifyPartIndices()
    {
        BRN_CONDUCTOR_GATE("DeformationManager::VerifyPartIndices @0x826042F8 (165)");
    }

    // ⭐⭐⭐ 2026-08-16 (walls leg 10): the UpdateSensorDisplacements @0x82604000 gate is DELETED --
    // the real body (a perf-mon-bracketed walk of mModelsAdded calling the per-model
    // DeformableObject::UpdateSensorDisplacements) is in BrnDeformationManager.cpp. LNK2005 if it
    // ever comes back. It was NOT a harmless seam: PhysicsModule::Update calls it every frame and
    // the vector it writes is the denominator of ValidateAndAddContact's impact-time latch, so
    // while it was inert no contact of any kind could become a deformation impulse.

    // ⭐ 2026-08-14 (walls leg 4): the Update @0x82649B40 + UpdatePostPhysics @0x82630420 gates
    // are DELETED -- both are REAL in BrnDeformationManager.cpp (the per-step conductor + the
    // post-physics solve/read-back pass). LNK2005 if they ever come back.

    // OutputData @0x826225D8 (339). ⭐ 2026-08-14 (deformation-mount wave): the walls-wave mount
    // plan EXECUTED -- the home TU's lifecycle+drain half is MOUNTED and OutputData's real body
    // was split into the (still unmounted) BrnDeformationManager_Output.cpp. This gate carries
    // the runtime seam until that slice's closure lands (OutputSensorState @0x82605618
    // X360-export-HOLE/PS3 0x6F3E10, UpdateAndOutputJointStates @0x82609AE8, OutputWheelData
    // @0x82608E28, DetachedPartManager::OutputEvents -> pool OutputEvents @0x8260DBE8); mounting
    // the slice DELETES this gate (LNK2005 says so loudly).
    void DeformationManager::OutputData(DeformationOutputInterfaceForEntityModules*,
                                        DeformationOutputInterface*)
    {
        BRN_CONDUCTOR_GATE("DeformationManager::OutputData @0x826225D8 (339; real body in the "
                           "unmounted BrnDeformationManager_Output.cpp)");
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

    // 2026-08-11 (lifetime wave) -- arm 2 of PhysicsModule::UpdateCachedPositions. See the note
    // at the foot of the Vehicle namespace above for why this one is gated while arm 1 is real.
    void PropManager::UpdateTriangleCache(
        CgsSceneManager::SceneManagerIO::InputBuffer_Update*)
    {
        BRN_CONDUCTOR_GATE("PropManager::UpdateTriangleCache @0x826119A0 (116; 573-insn closure) "
                           "-- props own ZERO triangle-cache slots today (usedSlots==28==8+20)");
    }
}

namespace Deformation
{
    // ⭐ 2026-08-14 (walls leg 4): the UpdateTriangleCache @0x826230E8 gate is DELETED -- the
    // real body mounts with BrnDeformationManager_Contacts.cpp (its detached-manager callees
    // land with the part/wheel manager TUs this wave). LNK2005 if it ever comes back.
}
}
