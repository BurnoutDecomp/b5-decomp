#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleManagerPerfMonHandles.h" // the 13 handles hoisted to external linkage (UpdateVehiclePhysics slice reads them)
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"     // CgsDev::PerfMonCpu::AddMonitor -- Construct's thirty monitors
#include "GameShared/GameClasses/Core/CgsAssert.h"                            // CGS_ASSERT -- the assert Construct fires in the eight-car loop
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                  // K_INVALID_ENTITY_ID (dword_82F2A3A4)
#include "GameShared/GameClasses/Physics/CgsRigidBody.h"                      // K_INVALID_RIGID_BODY_ID (qword_82F2A3A8)

// ==================================================================================================
// BrnPhysics::Vehicle::VehicleManager::Construct -- SPLIT OUT of BrnVehicleManager.cpp on
// 2026-08-03 (task #116). BUILD-MECHANICS SPLIT ONLY: the body below, its recipe comment and the
// twenty-nine file-scope perfmon handles it owns were MOVED verbatim, not retyped or re-derived.
// Same precedent, same reason, as RaceCarPhysics_Construct.cpp / BrnSimpleVehiclePhysics_Construct.cpp
// / TrafficPhysics_Construct.cpp.
//
// == WHY THE SPLIT ==
// BrnPhysics::PhysicsModule::Construct @0x825AE308 was a LIVE EMPTY STUB in WorldLinkStubs.cpp: a
// quiet no-op that ran every boot, so NOTHING in the physics module was ever constructed. Its
// callee VehicleManager::Construct @0x8263B7C8 is fully bodied -- but it lived in
// BrnVehicleManager.cpp, and mounting THAT whole TU is a different and much larger job (the file
// also holds HandleRaceCarRaceCarContact / ApplySlam / ApplyShunt / SetRaceCarCrashing, whose
// declare-only callees are ~14 unresolved externals, and whose faithful bodies add three more).
// /OPT:REF does not suppress LNK2019, so a mount's cost is the reference graph of the WHOLE TU.
//
// Construct's own closure is SIX functions (X360 xrefs_from of 0x8263B7C8), and after this split
// every one of them is mounted:
//     CgsDev::PerfMonCpu::AddMonitor            CgsPerfMonCpu.cpp
//     VehicleManagerDebugComponent::Construct   BrnVehicleManagerDebugComponent.cpp
//     VehicleDriver::Construct                  BrnVehicleDriver.cpp
//     VehiclePhysics::Construct                 VehiclePhysics.cpp  (via RaceCarPhysics_Construct.cpp)
//     PhysicalTrafficManager::Construct         BrnPhysicalTrafficManager.cpp
//     StuntOffencesManager::Construct           BrnStuntOffencesManager_Construct.cpp
//
// TO RE-MERGE: land the ~14 declare-only callees of the contact chain, mount BrnVehicleManager.cpp,
// then move this text back and delete the TU. Nothing here needs to change for that.
// ==================================================================================================

namespace BrnPhysics
{
namespace Vehicle
{
    // ==============================================================================================
    // ⭐ THE THIRTY PerfMonCpu MONITOR HANDLES. Twenty-nine of them are FILE-SCOPE globals on the
    // console (dword_82F2A14C..dword_82F2A290); the thirtieth is the member
    // miRaceCarWorldContactValidationPM. Every call passes page/min/budget/tag as (r4, r5, f1, r7)
    // with r6 never written, i.e. the FIVE-argument AddMonitor -- see the ⚠️⚠️ note in
    // CgsPerfMonCpu.h, which was settled from this very function.
    //
    // ⚠️ SEVEN OF THE TWENTY-NINE ARE GUARDED (`if (global < 0) global = AddMonitor(...)`), so they
    // MUST start NEGATIVE or they never register at all and the handle stays 0 -- a valid handle
    // belonging to somebody else's monitor. The guard is the console's register-once idiom for the
    // seven VehiclePhysics-LEVEL sub-monitors, which whichever object constructs first owns.
    // FLAG: only the SIGN of the console's initial value is observable (the guard is `blt`); -1 is
    // the committed sentinel this tree already uses for an unregistered handle, and it is also what
    // AddMonitor itself returns when the registry is full. The other twenty-two are unconditional,
    // so their initial value is never read; they are seeded identically for uniformity.
    // ⚠️ HOIST 2026-08-06 (UpdateVehiclePhysics wave): the THIRTEEN handles the per-frame
    // conductor brackets with (14C..19C) are now EXTERNAL (declared in
    // BrnVehicleManagerPerfMonHandles.h) because the UpdateVehiclePhysics slice TU reads them --
    // on the console both functions share one TU's file-scope statics. The rest stay static here.
    // ⚠️ These are file-scope HERE because nothing else in the tree registers them yet. On the
    // console the seven guarded slots are shared with the VehiclePhysics timing sites; when those
    // land they must bind to THESE symbols, not declare their own.
    // ==============================================================================================
    static const s32 KI_PERFMON_UNREGISTERED = -1;

    // flt_82004A20 -- the budget every one of the thirty passes in f1 (`lfs f22, flt_82004A20` once,
    // then `fmr f1, f22` at all thirty call sites).
    static const f32 KF_VMAN_PERFMON_BUDGET = 10.0f;

    static s32 gs_iUpdateStuntOffencesPM     = KI_PERFMON_UNREGISTERED;   // dword_82F2A1A0
    s32 gs_iUpdateVehicleImpactsPM    = KI_PERFMON_UNREGISTERED;   // dword_82F2A14C
    s32 gs_iProcessAboveGroundLTsPM   = KI_PERFMON_UNREGISTERED;   // dword_82F2A150
    s32 gs_iTractionLTsPM             = KI_PERFMON_UNREGISTERED;   // dword_82F2A154
    static s32 gs_iTractionGetLinesPM        = KI_PERFMON_UNREGISTERED;   // dword_82F2A158
    static s32 gs_iTractionLineTestsPM       = KI_PERFMON_UNREGISTERED;   // dword_82F2A15C
    // ⭐ HOISTED 2026-08-10 (ground wave): RunTractionLineTestJobs @0x825B5168 brackets its Begin /
    // RunStream stages with these two ids, so per the handles header's rule they move to external
    // linkage there rather than being re-declared locally.
    s32 gs_iLineTestsBeginPM          = KI_PERFMON_UNREGISTERED;   // dword_82F2A168
    s32 gs_iLineTestsRunStreamPM      = KI_PERFMON_UNREGISTERED;   // dword_82F2A16C
    static s32 gs_iLineTestsFinishPM         = KI_PERFMON_UNREGISTERED;   // dword_82F2A170
    static s32 gs_iLineTestsEndPM            = KI_PERFMON_UNREGISTERED;   // dword_82F2A174
    static s32 gs_iTractionProcessResultsPM  = KI_PERFMON_UNREGISTERED;   // dword_82F2A160
    static s32 gs_iTractionTrafficPM         = KI_PERFMON_UNREGISTERED;   // dword_82F2A164
    s32 gs_iCrashFatalPM              = KI_PERFMON_UNREGISTERED;   // dword_82F2A178
    s32 gs_iUpdateRaceCarsPM          = KI_PERFMON_UNREGISTERED;   // dword_82F2A17C
    s32 gs_iUpdateDriversPM           = KI_PERFMON_UNREGISTERED;   // dword_82F2A180
    s32 gs_iUpdateVehiclesPM          = KI_PERFMON_UNREGISTERED;   // dword_82F2A184
    // ⭐ HOISTED 2026-08-07 (orchestrator wave): the seven guarded VPhys sub-monitors gained a
    // second reader (VehiclePhysics::Update @0x826412C0 brackets its stages with them), so per
    // the handles header's rule they move to external linkage there. [GUARDED] registration
    // below is unchanged.
    s32 gs_iVPhysUpdatePM             = KI_PERFMON_UNREGISTERED;   // dword_82F2A278   [GUARDED]
    s32 gs_iVPhysSwitchAttribsPM      = KI_PERFMON_UNREGISTERED;   // dword_82F2A27C   [GUARDED]
    s32 gs_iVPhysUpdateCrashingPM     = KI_PERFMON_UNREGISTERED;   // dword_82F2A280   [GUARDED]
    s32 gs_iVPhysUpdateAirRamsPM      = KI_PERFMON_UNREGISTERED;   // dword_82F2A284   [GUARDED]
    s32 gs_iVPhysUpdateSpinPM         = KI_PERFMON_UNREGISTERED;   // dword_82F2A288   [GUARDED]
    s32 gs_iVPhysUpdateDrivingPM      = KI_PERFMON_UNREGISTERED;   // dword_82F2A28C   [GUARDED]
    s32 gs_iVPhysUpdateLVPM           = KI_PERFMON_UNREGISTERED;   // dword_82F2A290   [GUARDED]
    s32 gs_iRBChangePM                = KI_PERFMON_UNREGISTERED;   // dword_82F2A188
    s32 gs_iAfterTouchPM              = KI_PERFMON_UNREGISTERED;   // dword_82F2A18C
    s32 gs_iUpdateTrafficPM           = KI_PERFMON_UNREGISTERED;   // dword_82F2A190
    s32 gs_iUpdateAggressiveDrivingPM = KI_PERFMON_UNREGISTERED;   // dword_82F2A194
    s32 gs_iUpdateCrashesPM           = KI_PERFMON_UNREGISTERED;   // dword_82F2A198
    s32 gs_iUpdatePassBysPM           = KI_PERFMON_UNREGISTERED;   // dword_82F2A19C

    // ==============================================================================================
    // VehicleManager::Construct  @0x8263B7C8 -- 943 instructions.
    //
    // The shape, in issue order (the full instruction-level recipe, every rodata symbol and every
    // default value live in the ⭐ recipe block in BrnVehicleManager.h; this body is written off it
    // and adds no new decode):
    //     ~1..310    the thirty AddMonitor calls
    //     ~311..410  mePrepareStage / meReleaseStage, the debug component, then an INLINED
    //                CgsNumeric::Random::Construct
    //     ~411..510  the EIGHT-CAR LOOP
    //     ~511..600  the traffic manager, the discarded-contact queue, the camera matrix, the spare
    //                AI driver and the four RaceCarBitArray clears
    //     ~601..943  the TUNING BANK -- 91 seats
    //
    // ⛔ THE BANK IS NOT OPTIONAL. A body that ran the spine and skipped the last ~340 instructions
    // would leave every takedown/slam/shunt threshold at zero while LOOKING complete -- the
    // silent-drop-stub failure class this project keeps paying for. All 91 seats are here, and the
    // 85 scalar ones were emitted by a machine join of the asm seat table against this class's own
    // declarations rather than retyped, so a value cannot drift from the header it was proven in.
    //
    // ⚠️ WHAT THIS FUNCTION DOES *NOT* DO, stated so it is not "completed" later: it never touches
    // mbEasyCrashingEnabled, DEBUG_mbAlwaysCrashRaceCarToRaceCar, DEBUG_mbHornTakedownEnabled,
    // mbDebugModifyTrafficContacts, mbUpdatedPlayerDriver, mbForceNoSlowMo, miPlayerSpeed/Strength/
    // Control/Boost, mn8RoundRobinControlWord, mCurrentTime/mStartModeTime, or the whole
    // contact-generation block at +172465..+172580. Those are the console's own omissions.
    //
    // ⚠️ ORDERING. Two things are issued LATE and are kept late because the asm puts them there:
    // StuntOffencesManager::Construct fires BETWEEN the +172580 and +172584 counter stores
    // (0x8263C61C / 0x8263C620 / 0x8263C640), and the thirtieth monitor + its assert sit inside the
    // bank, not with the other twenty-nine. Neither is tidied up.
    // ==============================================================================================
    void VehicleManager::Construct()
    {
        gs_iUpdateStuntOffencesPM = CgsDev::PerfMonCpu::AddMonitor(
            "VMan: Update Stunt Offences", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iUpdateVehicleImpactsPM = CgsDev::PerfMonCpu::AddMonitor(
            "VMan: Update Vehicle Impacts", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iProcessAboveGroundLTsPM = CgsDev::PerfMonCpu::AddMonitor(
            "VMan: Process Above Ground LTs", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iTractionLTsPM = CgsDev::PerfMonCpu::AddMonitor(
            "VMan: Traction LTs", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iTractionGetLinesPM = CgsDev::PerfMonCpu::AddMonitor(
            "        GetLines", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iTractionLineTestsPM = CgsDev::PerfMonCpu::AddMonitor(
            "        LineTests", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iLineTestsBeginPM = CgsDev::PerfMonCpu::AddMonitor(
            "           Begin", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iLineTestsRunStreamPM = CgsDev::PerfMonCpu::AddMonitor(
            "           RunStream", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iLineTestsFinishPM = CgsDev::PerfMonCpu::AddMonitor(
            "           Finish", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iLineTestsEndPM = CgsDev::PerfMonCpu::AddMonitor(
            "           End", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iTractionProcessResultsPM = CgsDev::PerfMonCpu::AddMonitor(
            "        ProcessResults", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iTractionTrafficPM = CgsDev::PerfMonCpu::AddMonitor(
            "        Traffic", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iCrashFatalPM = CgsDev::PerfMonCpu::AddMonitor(
            "VMan: Crash Fatal", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iUpdateRaceCarsPM = CgsDev::PerfMonCpu::AddMonitor(
            "VMan: Update Race Cars", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iUpdateDriversPM = CgsDev::PerfMonCpu::AddMonitor(
            "        Drivers", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iUpdateVehiclesPM = CgsDev::PerfMonCpu::AddMonitor(
            "        Vehicles", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        if (gs_iVPhysUpdatePM < 0)
            gs_iVPhysUpdatePM = CgsDev::PerfMonCpu::AddMonitor(
                "          VPhys::Update", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        if (gs_iVPhysSwitchAttribsPM < 0)
            gs_iVPhysSwitchAttribsPM = CgsDev::PerfMonCpu::AddMonitor(
                "            Switch Attribs", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        if (gs_iVPhysUpdateCrashingPM < 0)
            gs_iVPhysUpdateCrashingPM = CgsDev::PerfMonCpu::AddMonitor(
                "            Update Crashing", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        if (gs_iVPhysUpdateAirRamsPM < 0)
            gs_iVPhysUpdateAirRamsPM = CgsDev::PerfMonCpu::AddMonitor(
                "            Update Air Rams", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        if (gs_iVPhysUpdateSpinPM < 0)
            gs_iVPhysUpdateSpinPM = CgsDev::PerfMonCpu::AddMonitor(
                "            Update Spin", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        if (gs_iVPhysUpdateDrivingPM < 0)
            gs_iVPhysUpdateDrivingPM = CgsDev::PerfMonCpu::AddMonitor(
                "            Update Driving", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        if (gs_iVPhysUpdateLVPM < 0)
            gs_iVPhysUpdateLVPM = CgsDev::PerfMonCpu::AddMonitor(
                "            Update LV", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iRBChangePM = CgsDev::PerfMonCpu::AddMonitor(
            "        RB Change", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iAfterTouchPM = CgsDev::PerfMonCpu::AddMonitor(
            "        AfterTouch", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iUpdateTrafficPM = CgsDev::PerfMonCpu::AddMonitor(
            "VMan: Update Traffic", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iUpdateAggressiveDrivingPM = CgsDev::PerfMonCpu::AddMonitor(
            "VMan: Update Aggressive Driving", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iUpdateCrashesPM = CgsDev::PerfMonCpu::AddMonitor(
            "VMan: Update Crashes", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);
        gs_iUpdatePassBysPM = CgsDev::PerfMonCpu::AddMonitor(
            "VMan: Update PassBys", CgsDev::E_PMP_12, false, KF_VMAN_PERFMON_BUDGET, true);

        mePrepareStage = 0;   // +0  (stw r30, 0(r31))
        meReleaseStage = 3;   // +4  (stw r24, 4(r31))

        // +44224 / +44232. Cleared here, ~200 instructions before the four RaceCarBitArrays below --
        // the asm issues them with the two `std r30` at 0x8263BCD0/0x8263BCD4, before the debug
        // component call, not with the other bitsets.
        mUsedRaceCars.UnSetAll();
        mUsedRaceCarCrashesList.UnSetAll();

        // +161968. TWO arguments on the console (r3 = the component, r4 = r31 = the manager);
        // Hex-Rays renders the call with none.
        mDebugComponent.Construct(this);

        // +16. The block at 0x8263BCDC..0x8263BEE8 is CgsNumeric::Random::Construct() inlined
        // VERBATIM -- muSeed = KU_RANDOM_DEFAULT_SEED, index 0, ring[0] = 1.0f, then seven
        // AddRandomFloatToBuffer draws and one final index step. Nothing to write.
        mRandom.Construct();

        // ==========================================================================================
        // THE EIGHT-CAR LOOP (0x8263BF04..0x8263BF90). r29 walks the record at the asm-literal
        // stride 0x1460; r25 walks the drivers at 0xE0; r26/r28/r27 walk the three sibling arrays at
        // 8/4/0x400. Every statement below is one of those cursors.
        // ==========================================================================================
        for (s32 liCar = 0; liCar < KI_MAX_ACTIVE_RACE_CARS; ++liCar)
        {
            maRaceCarDrivers[liCar].Construct();    // +64,   stride 224

            // ⭐ ONE CALL, not a base call plus six pokes. The X360 inlines RaceCarPhysics::Construct
            // here (`bl VehiclePhysics::Construct` then its own six writes); the PS3 build calls it
            // out of line at 0x6EB3D4 with exactly that body. See RaceCarPhysics::Construct.
            maRaceCarVehicles[liCar].Construct();   // +1856, stride 5216 on the console

            maRaceCarEntityIDs[liCar].muValue     = CgsSceneManager::K_INVALID_ENTITY_ID;
            maRaceCarHandlingBodyIDs[liCar]       = CgsPhysics::K_INVALID_RIGID_BODY_ID;
            maeRaceCarTypes[liCar]                = BrnWorld::E_RACE_CAR_TYPE_INACTIVE;   // == 3
            mauNetworkCarHiddenFramesRemaining[liCar] = 0;

            // The console's own assert, at the console's own file/line (`li r5, 0x8B4` ==
            // VehiclePhysics.h:2228). ⚠️ It is VACUOUS HERE and that is stated rather than dressed
            // up: on the X360 the argument is a walking cursor register (`addi r27,r27,0x400`) that
            // the compiler cannot prove non-null, while here it is the address of an array element,
            // which the standard guarantees is never null. Kept because it is the console's own
            // check at the console's own site, not because it can fire.
            CGS_ASSERT(&maRaceCarDebugComponent[liCar] != NULL, "lpDebugComponent != NULL");

            // ⚠️ FLAG (span cast, deliberate and inert). maRaceCarDebugComponent is an OPAQUE
            // 8x1024 byte span -- the console's DebugComponent is 1024 bytes and this tree
            // reconstructs 112 of them -- so the pointer this stores does not address a constructed
            // object. That is faithful to the store the console makes (it only records the address;
            // nothing constructs the components here either), and it is currently harmless: NOTHING
            // in the mounted tree dereferences VehiclePhysics::mpDebugComponent (grep: zero hits in
            // VehiclePhysics.cpp / RaceCarPhysics.cpp / Wheel.cpp). Size and alignment are at least
            // safe -- 112 <= 1024 and both the span base and the 1024 stride are 16-aligned.
            // ⛔ The day a debug-draw body reads through this pointer, the 1024-byte span has to
            // become a real DebugComponent[8] first. Do not "just" dereference it.
            maRaceCarVehicles[liCar].mpDebugComponent =
                reinterpret_cast<DebugComponent*>(&maRaceCarDebugComponent[liCar]);
        }

        mPhysicalTrafficManager.Construct();   // +44768

        // +160672. BaseEventQueue<T>::Construct sets {mpEvents = maEvents, miMaxLength = 20,
        // miLength = 0} and fires the same `lpEventBuffer != NULL` assert (CgsBaseEventQueue.h:160)
        // the console does -- the three `stw` at 0x8263C048/58/60 plus the guarded assert above them.
        mDiscardedContacts.Construct();

        // +172240. Four 16-byte lanes built on the stack from flt_82001C98 (1.0f) and flt_82001CC0
        // (0.0f): rows {1,0,0,0} {0,1,0,0} {0,0,1,0} and a ZERO fourth row -- which is exactly what
        // Matrix44Affine::SetIdentity() writes (its wAxis is {0,0,0,0}, not {0,0,0,1}).
        mCameraMatrix.SetIdentity();

        mPlayerAiDriver.Construct();   // +171968

        // +44704..+44736. Four `std r30` at 0x8263C0A8..0x8263C0C4 (the asm emits two of them twice;
        // there are four distinct addresses).
        mHiddenRaceCars.UnSetAll();
        mRaceCarsAddedForCollision.UnSetAll();
        mNetworkCarsAddedForCollisionThisFrame.UnSetAll();
        mNetworkCarsRecievedFirstUpdate.UnSetAll();

        // ==========================================================================================
        // THE TUNING BANK -- 91 seats, +171464..+172616. 85 emitted by machine join; the six below
        // that the join could not settle are hand-written and each says why.
        // ==========================================================================================
        mbSlamsAndShuntsOn                    = true;    // +171464
        mbAllowSlamsAndShuntsEffectsForRivals = true;    // +171465
        mfFrontRaySensorLength                = 4.0f;    // +171468
        mfFrontRayLength                      = 1.5f;    // +171472
        mfRearRayLength                       = 1.5f;    // +171476
        mfPlayerShuntScale                    = 0.325f;  // +171480
        mfAIShuntScale                        = 0.2f;    // +171484
        mfShuntDecay                          = 0.15f;   // +171488
        mfVulnerabilityFactorMax              = 4.0f;    // +171492
        mfPlayerVulnerabilityDurationSeconds  = 2.0f;    // +171496
        mfAIVulnerabilityDurationSeconds      = 4.0f;    // +171500
        mfMinSteeringOverrideTimeSlam         = 0.2f;    // +171504
        mfMinSteeringOverrideTimeShunt        = 0.2f;    // +171508
        mfPlayerMaxSteeringOverrideTimeSlam   = 0.7f;    // +171512
        mfAIMaxSteeringOverrideTimeSlam       = 0.9f;    // +171516
        mfPlayerMaxSteeringOverrideTimeShunt  = 0.4f;    // +171520
        mfAIMaxSteeringOverrideTimeShunt      = 0.7f;    // +171524
        mfPlayerSlamForceScale                = 0.25f;   // +171528
        mfAISlamForceScale                    = 0.25f;   // +171532
        mfMaxSlamClosingXSpeed                = 16.0f;   // +171536
        mfMinSecondsBetweenImpacts            = 0.3f;    // +171540
        mfMinAmountOfSlamForce                = 0.2f;    // +171544
        mfMinAmountOfShuntForce               = 0.25f;   // +171548

        // ⭐ The one value X360 Hex-Rays carried in a register (it renders as `v62`). The PS3 build
        // gives it literally at its +170880, and the symbol the X360 loads it from is flt_82001C98 --
        // the SAME slot this function uses for the mCameraMatrix identity diagonal, so this function
        // alone proves the value is 1.0f without leaving the X360 image.
        mfTailgatingVunerabilityTime = 1.0f;;  // +171552

        mfBaseSlamMagnitude                    = 3.0f;    // +171556
        mfBaseShuntMagnitude                   = 22.5f;   // +171560
        mfTBoneTakedownMaxAngle                = 35.0f;   // +171564
        mfTBoneTakedownSpeed                   = 30.0f;   // +171568
        mfMaxShuntAngle                        = 25.0f;   // +171572
        mfMinNudgeSpeed                        = 8.0f;    // +171576
        mfMinShuntSpeed                        = 12.0f;   // +171580
        mfFatalShuntSpeed                      = 140.0f;  // +171584
        mfSlamDecayRate                        = 0.13f;   // +171588
        mfSlamEffectMinMagnitude               = 0.4f;    // +171592
        mfSlamEffectMaxMagnitude               = 2.0f;    // +171596
        mfMinShuntMagnitude                    = 0.2f;    // +171600
        mfMaxShuntMagnitude                    = 0.4f;    // +171604
        mfMinShuntBackwardsMagnitude           = 0.3f;    // +171608
        mfMaxShuntBackwardsMagnitude           = 0.75f;   // +171612
        mfMinTradingPaintSpeed                 = 0.8f;    // +171616
        mfFatalSlamSpeed                       = 140.0f;  // +171620
        mfFatalHitCrashingCarSpeed             = 50.0f;   // +171624
        mfMaxHeadToHeadAngle                   = 45.0f;   // +171628
        mfMinHeadToHeadSpeed                   = 40.0f;   // +171632
        mfMinHeadToHeadIndividualSpeed         = 40.0f;   // +171636
        mfAngleForVerticleTakedown             = 60.0f;   // +171640
        mfCrashingAICollisionCrashThresholdMPH = 50.0f;   // +172208
        mfHeadOnWorldCrashThreshold            = 40.5f;   // +172212
        mfSideOnWorldCrashThreshold            = 50.0f;   // +172216
        mfTrafficCollisionCheckThresholdMPH    = 30.0f;   // +172220
        mfMinRCTrafficTranslateSpeedMPH        = 40.0f;   // +172224
        mfVerticalTakedownAngleDeg             = 65.0f;   // +172228
        mbImpactTime                           = false;   // +172304
        mbStopPlayerCrashing                   = false;   // +172306
        mbStopAICrashing                       = false;   // +172307
        mbCrashOnHandbrakeTurn                 = false;   // +172308
        mbCrashPlayerNextUpdate                = false;   // +172309
        mbTrafficCheckingAllowed               = true;    // +172313
        mbAftertouchIsForceAdditive            = false;   // +172314
        mbIsOnlineGameMode                     = false;   // +172315
        mfPlayerStatStrength                   = 0.0f;    // +172320
        mfPlayerStatDamageLimit                = 0.0f;    // +172324
        miCarSpeed                             = 0;       // +172328
        miCarStrength                          = 0;       // +172332
        miCarControl                           = 0;       // +172336
        miCarBoost                             = 0;       // +172340
        meCarType                              = 3;       // +172344
        meCurrentGameModeType                  = -1;      // +172380
        mfCarStatStrengthSlamMax               = 2.0f;    // +172384
        mfCarrStatStrengthSlamMin              = 0.5f;    // +172388
        mfCarStatStrengthShuntMax              = 2.0f;    // +172392
        mfCarrStatStrengthShuntMin             = 0.05f;   // +172396
        mfCarStatStrengthBeingSlammedMax       = 2.0f;    // +172400
        mfCarStatStrengthBeingSlammedMin       = 0.5f;    // +172404
        mfCarStatStrengthBeingShuntedMax       = 2.0f;    // +172408
        mfCarrStatStrengthBeingShuntedMin      = 0.05f;   // +172412

        // +172416 / +172420. The DWARF types these `const SimpleVehiclePhysics*`; they are modelled
        // as u32 slots so the 16-aligned normal below keeps its asm-proven +172432 seat. Construct
        // only NULLs them (`stwx r30`).
        muCachedCarASlot = 0;
        muCachedCarBSlot = 0;

        mbCachedCarCarPredictionResult = false;   // +172424

        // +172432. `stvx128 v0, r31, r9` of the 16 bytes at unk_82181520 -- the identity basis row
        // {0,0,1,0}, settled in-repo twice (ICECameraSpaceHandler.cpp:124, BrnShadowMap.cpp:955/998).
        // i.e. the cached car-vs-car prediction normal starts as the world +Z axis.
        mCachedCarCarPredictionNormal = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };

        meStationaryPlayerWheelAngle = 2;       // +172448
        mbCrashRaceCarWhenFatal      = true;    // +172452
        meShowtimeBehaviour          = 2;       // +172456

        // ⭐ THE THIRTIETH MONITOR -- page 6, not 12, and the only one stored INTO the object. The
        // console asserts its handle immediately (BrnVehicleManager.cpp:778, `li r5, 0x30A`), which
        // is where the member's DWARF name came from.
        miRaceCarWorldContactValidationPM = CgsDev::PerfMonCpu::AddMonitor(
            "PHYS ValidateRCWorldContact", CgsDev::E_PMP_6, false, KF_VMAN_PERFMON_BUDGET, true);
        CGS_ASSERT(miRaceCarWorldContactValidationPM >= 0,
                   "miRaceCarWorldContactValidationPM >= 0");

        miNumTrafficSphereWorldTests = 0;;  // +172580 (renamed at the 2026-08-06 carve; DWARF :1072)

        // ⚠️ ISSUED HERE, between the two counter stores -- 0x8263C61C stores +172580, 0x8263C620 is
        // this call, 0x8263C640 stores +172612. Not moved next to the other sub-constructors.
        mStuntOffencesManager.Construct();   // +44240

        muTakedownEventsThisFrame = 0;;  // +172612
        mpTractionLineStreamProducer = 0;;  // +172584 (renamed at the carve: the console `stwx 0` here null-stores the DWARF :1075 POINTER)

        mbInOnlineGameModeStartLine = false;;  // +172318
        mbPlayerCarInJunkYard       = false;;  // +172319

        // +172592 / +172608. One `stvx128 v127` (16 zero bytes) then one zero byte -- the DWARF's
        // adjacent `Sphere mStuckInCollisionTestCacheSphere` + `bool mbPlayerCarStuckInCollision`.
        // Raw bytes because Sphere has no committed home; the 16-byte size is what the stvx128 proves.
        for (s32 liByte = 0; liByte < 16; ++liByte)
            mStuckInCollisionTestCacheSphere[liByte] = 0;
        mbPlayerCarStuckInCollision = false;
    }
}
}
