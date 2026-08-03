// Layout check for BrnVehicleManager.h's TUNING BANK (this+171464 .. this+172616).
//
// ⚠️⚠️ WHY THIS TU EXISTS -- READ BEFORE DELETING IT.
// BrnVehicleManager.h advertises that its recovered offsets are "pinned by the offsetof asserts in
// _AssertLayout / _AssertLayoutPlayerStats", and that "the gate FAILS if any padding run is wrong,
// which is the signal". That was NOT TRUE as of 2026-08-03: both of those functions live in
// BrnVehicleManager.cpp / BrnVehicleManagerPlayerStats.cpp, and **neither TU is mounted in
// tools/build/build_game_exe.bat**. A static_assert in a TU that is never compiled is not a gate,
// it is a comment -- so every padding run in that ~172 KB class had been carried for many waves
// with nothing checking it. A green build proved exactly nothing about the layout.
//
// This TU closes that hole the cheap way: it is mounted, it includes the header, and it defines a
// third never-called static member (declared in the header alongside the other two) whose body is
// nothing but static_asserts. static_assert fires at COMPILE time, so /OPT:REF discarding the
// (uncalled) function afterwards is irrelevant -- unlike an LNK2019 witness, this one cannot be
// optimised away before it has done its job. Being a static member is what gives it access to the
// private members offsetof has to see.
//
// It pins the span this wave resolved. The pre-existing asserts in the two unmounted TUs are left
// where they are; mounting those TUs is a link-closure question, not a layout one.
//
// PROVENANCE of every offset below: an indexed store in VehicleManager::Construct @0x8263B7C8,
// symbolically resolved from the X360 asm, cross-checked against that function's Hex-Rays and
// against the PS3 DecFIGS build's same function @0x6EB6BC at a uniform shift of Δ = 672.
// Names are the DecFIGS DWARF's (references/DecFIGS/dwarfdump/.../BrnVehicleManager.h:865-1088).

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"

// ⭐ ADDED 2026-08-03 (the Construct-blocker wave) -- THIS INCLUDE IS ITSELF A GATE, not a
// convenience. BrnPhysicalTrafficManager.h used to define its own `struct VehicleDriver
// { u8 mOpaque[224]; }` at namespace scope in BrnPhysics::Vehicle, while BrnVehicleManager.h
// (above) pulls in the REAL VehicleDriver for maRaceCarDrivers[8] / mPlayerAiDriver. The two
// headers therefore could not be included in the same translation unit -- a hard C2011 -- and
// VehicleManager::Construct @0x8263B7C8 has to call both VehicleDriver::Construct and
// PhysicalTrafficManager::Construct. That fork is retired; this line is what keeps it retired,
// because BrnPhysicalTrafficManager.h's only other includer (BrnPhysicalTrafficManager.cpp) is
// NOT mounted, so nothing else in the build would ever notice a re-fork.
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
// The two sub-object classes whose real x64 size decides whether Construct can call their
// constructors at all. Included so the size verdicts below are compiled facts.
#include "GameSource/Physics/VehicleManager/BrnVehicleManagerDebugComponent.h"
// ⭐ ADDED 2026-08-03 (the un-pin wave) -- also a gate, not a convenience. sizeof(PotentialContact)
// is the one load-bearing INPUT to the backward derivation of the X360 sizeof(PhysicalTrafficManager)
// (see BrnPhysicalTrafficManager.h finding (4), derivation B). If that record ever gets a different
// committed layout, the derivation it feeds has to be redone -- so it is asserted here rather than
// left as a number in a comment.
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"

#include <cstddef>   // offsetof

namespace BrnPhysics
{
namespace Vehicle
{
    void VehicleManager::_AssertLayoutTuningBank()
    {

        // ==========================================================================================
        // ⛔ THE `VehicleManager::Construct` SUB-CONSTRUCTOR BLOCKER TABLE, as compiled asserts.
        //
        // These are not decoration. Construct @0x8263B7C8 calls six sub-constructors; three of them
        // take a `this` that VehicleManager cannot supply, because the member is an X360-sized
        // opaque span and the real x64 class is bigger. The header records the measured numbers;
        // these asserts make the build fail the day any of them changes, in EITHER direction --
        // including the good direction, which is the point: when a wave shrinks
        // VehicleManagerDebugComponent to 1296 or re-seats the traffic manager, one of these fires
        // and says "the blocker you were told about is gone".
        //
        // ⚠️ Deliberately written as `!=` / `==` against the MEASURED values rather than as
        // `<= span`, so they are tripwires, not permissions.
        // ==========================================================================================

        // ✅ READY -- fits exactly, so `VehicleDriver::Construct` is callable by name today.
        static_assert(sizeof(VehicleDriver) == 224,
                      "VehicleDriver fits maRaceCarDrivers[8] (stride 0xE0) and mPlayerAiDriver");
        static_assert(sizeof(VehicleManager::maRaceCarDrivers) == 8 * 224,
                      "the eight-car driver array is 1792 bytes: 64 + 1792 == 1856 == maRaceCarVehicles");

        // ✅ READY -- the ONE contained sub-object whose real class fits its X360 span on x64.
        static_assert(sizeof(BrnPhysics::StuntOffencesManager) == 464,
                      "StuntOffencesManager fits this+44240..44704 exactly (no pointer members; "
                      "last member ends at 0x1C4 == 452, padded to 464 at align 16 on both ISAs)");
        static_assert(alignof(BrnPhysics::StuntOffencesManager) == 16 &&
                      offsetof(VehicleManager, mStuntOffencesManager) % 16 == 0,
                      "and its 16-byte alignment is satisfied at 44240, so typing it moved nothing");

        // ✅ READY -- the discarded-contact queue fits because the pointer widening lands in the
        // padding the X360 header already carried: {T* , s32, s32} is 12->16 there and 8+4+4 == 16
        // here, then 16 + 20*64 == 1296 either way.
        static_assert(sizeof(VehicleManager::mDiscardedContacts) == 1296,
                      "EventQueue<DiscardedContact,20> fits this+160672..161968 exactly");
        static_assert(sizeof(BrnPhysics::ContactSpy::DiscardedContact) == 64,
                      "and the 64-byte entry is what makes 16 + 20*64 == 1296 close");
        static_assert(offsetof(VehicleManager, mDiscardedContacts) == 160672 + KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER,
                      "asm: addis r29,r31,2 ; addi r29,r29,0x73A0 ; stw r28,0(r29)");

        // ==========================================================================================
        // ✅ UN-BLOCKED 2026-08-03 -- the manager's own debug component, embedded BY NAME.
        //
        // This block read "⛔ BLOCKED -- grows 32 bytes on x64 ... CANNOT be called by name until
        // this class is 1296 or VehicleManager stops being byte-pinned". The second disjunct is what
        // happened. Note the difference from the traffic manager: THAT span was wrong, this one is
        // right -- 161968 and 163264 are both asm-literal -- so the +32 is a genuine host/console
        // width difference and is carried as the second drift term, not derived away.
        // ==========================================================================================
        static_assert(sizeof(VehicleManagerDebugComponent) == 1328,
                      "MEASURED host size: the X360's 1296 plus the base vptr and mpVehicleManager "
                      "each widening 4 -> 8");
        static_assert(sizeof(VehicleManager::mDebugComponent)
                          == 1296 + (KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT - KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER),
                      "and the second drift term is exactly that overrun -- 224 - 192 == 32");
        static_assert(offsetof(VehicleManager, mDebugComponent) == 161968 + KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER,
                      "asm: VehicleManagerDebugComponent::Construct(this + 161968, this) @0x8263BCD8 "
                      "-- it sits AFTER the traffic manager and BEFORE its own drift term applies");
        static_assert(alignof(VehicleManagerDebugComponent) == 16 &&
                      offsetof(VehicleManager, mDebugComponent) % 16 == 0,
                      "the component holds alignas(16) OutContactSpy/PotentialContact members, and "
                      "161968 + 192 == 162160 is 16-aligned, so embedding it inserted no leading pad");
        static_assert(offsetof(VehicleManager, mDiscardedContacts)
                          + sizeof(VehicleManager::mDiscardedContacts)
                          == offsetof(VehicleManager, mDebugComponent),
                      "and the discarded-contact queue abuts it with no gap, exactly as on the X360");

        // ==========================================================================================
        // ✅ UN-BLOCKED 2026-08-03 -- the contained traffic manager, embedded BY NAME.
        //
        // This block used to read "⛔ BLOCKED -- grows 2480 bytes on x64", measured against a
        // 103360-byte opaque span. The SPAN was wrong, not the class: the X360 size is 105648
        // (BrnPhysicalTrafficManager.h finding (4), derived twice) and the host overrun is +192,
        // which BrnVehicleManager.h now carries as KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER.
        //
        // The three numbers are tied together here so none of them can drift alone.
        // ==========================================================================================
        static_assert(offsetof(VehicleManager, mPhysicalTrafficManager) == 44768,
                      "asm: addis r3,r31,1 ; addi r3,r3,-0x5120 ; bl PhysicalTrafficManager::Construct "
                      "@0x8263BF9C -- the embedded manager keeps its X360 seat (nothing before it moved)");
        static_assert(sizeof(PhysicalTrafficManager)
                          == KU_X360_SIZEOF_PHYSICAL_TRAFFIC_MANAGER + KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER,
                      "MEASURED host size 105840 == DERIVED X360 size 105648 + the 192-byte drift this "
                      "class carries. If this fires, one of the three has moved: re-derive, do not "
                      "just bump the drift.");
        static_assert(KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER % 16 == 0,
                      "the drift MUST be a multiple of 16, or every 16-aligned member after the traffic "
                      "manager (mCameraMatrix, mCachedCarCarPredictionNormal, the Sphere) would need a "
                      "different correction than the scalars around it -- the alignment asserts below "
                      "are only valid because this holds");
        static_assert(alignof(PhysicalTrafficManager) == 16 &&
                      offsetof(VehicleManager, mPhysicalTrafficManager) % 16 == 0,
                      "44768 % 16 == 0, so embedding the real type inserted no leading pad");

        // ---- the traffic manager's own head, which IS pointer-free and so DOES reproduce ---------
        // These are the first compiled asserts this class has ever had. Everything up to (but not
        // including) maTrafficCarModelHandles is free of ResourceHandle/pointer widening, so the X360
        // offsets survive verbatim on the host -- and the one that matters is maTrafficEntityIDs,
        // because VehicleManager used to model it as a sibling of its own called
        // `maRaceCarEntityIdRemap[8]` at class +148128.
        static_assert(sizeof(TrafficPhysics) == 5168,
                      "asm: mulli r11, r29, 0x1430 -- the 20-element per-vehicle walk in "
                      "PhysicalTrafficManager::Construct @0x82636CA8");
        static_assert(offsetof(PhysicalTrafficManager, maTrafficEntityIDs) == 103360,
                      "asm: addi r8,r11,0x64F0 ; slwi r9,r8,2 ; stwx -1,r9,r31 for i<20 == 4*(i+25840)");
        static_assert(offsetof(VehicleManager, mPhysicalTrafficManager)
                          + offsetof(PhysicalTrafficManager, maTrafficEntityIDs) == 148128,
                      "⭐ THE IDENTIFICATION: 44768 + 103360 == 148128, the exact class offset the old "
                      "`EntityId maRaceCarEntityIdRemap[8]` sibling claimed. Same address, real name, "
                      "and the real bound is 20 (a TRAFFIC index), not 8.");
        static_assert(sizeof(PhysicalTrafficManager::maTrafficEntityIDs) == 20 * 4,
                      "EntityId[20] -- SetRaceCarCrashing's owner==2 branch indexes it with a traffic "
                      "index, so the old [8] model was an out-of-bounds read for slots 8..19");
        static_assert(sizeof(PhysicalTrafficManager::mArticulatedJointPool) == 832,
                      "asm: pool at this+103616, next written member at this+104448");
        static_assert(sizeof(PhysicalTrafficManager::mu8GlobalToPhysicalEntityIndexMap) == 600,
                      "asm: cmplwi 0x258 in ValidateAndFixUpTrafficTrafficContact (the DWARF says 601; "
                      "the asm is authoritative and the 16-alignment that follows absorbs the byte)");

        // ---- the BACKWARD derivation of the traffic manager's X360 size, as a compiled fact ------
        // 44768 + 105648 + 128*sizeof(PotentialContact) + 4, rounded up to mDiscardedContacts'
        // 16-alignment, must equal the asm-pinned 160672. sizeof(PotentialContact) == 80 is the one
        // input that comes from a committed record rather than from the asm, so it is pinned.
        static_assert(sizeof(CgsSceneManager::SceneManagerIO::PotentialContact) == 80,
                      "3 x Vector3 + 2 VolumeInstanceId + 2 uint32 + 2 uint16, 16-aligned (DWARF "
                      "CgsPotentialContact.h:60-68). This is derivation B's only non-asm input.");
        static_assert(sizeof(VehicleManager::mPadNonPhysicalContacts)
                          == 128 * sizeof(CgsSceneManager::SceneManagerIO::PotentialContact) + 4 + 12,
                      "maNonPhysicalContacts[128] + miNonPhysicalContactCount + 12 bytes of align pad "
                      "== 10256 == 160672 - 150416: the closure that forces X360 sizeof"
                      "(PhysicalTrafficManager) == 105648 and no other value");
        static_assert(44768 + KU_X360_SIZEOF_PHYSICAL_TRAFFIC_MANAGER
                          + sizeof(VehicleManager::mPadNonPhysicalContacts) == 160672,
                      "and it closes on the X360's own mDiscardedContacts seat");

        // ---- the two master gates (DWARF :865/:866) -------------------------------------------
        static_assert(offsetof(VehicleManager, mbSlamsAndShuntsOn) == 171464 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT,
                      "mbSlamsAndShuntsOn (asm stbx 1 @+171464)");
        static_assert(offsetof(VehicleManager, mbAllowSlamsAndShuntsEffectsForRivals) == 171465 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT,
                      "mbAllowSlamsAndShuntsEffectsForRivals (asm stbx 1 @+171465)");

        // ---- the 44-float tuning run (DWARF :868..:920) ----------------------------------------
        // Head, tail, the two retyped/renamed seats in the middle, and the closure. The closure is
        // the real test: 171468 + 44*4 == 171644 only holds if EVERY float in between is declared
        // and none of the old padding runs survived.
        static_assert(offsetof(VehicleManager, mfFrontRaySensorLength)     == 171468 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "run head (asm +171468 = 4.0f)");
        static_assert(offsetof(VehicleManager, mfMaxSlamClosingXSpeed)     == 171536 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +171536 = 16.0f");
        static_assert(offsetof(VehicleManager, mfMinSecondsBetweenImpacts) == 171540 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +171540 = 0.3f -- was mis-typed `s32 miAttackerToRecord`");
        static_assert(offsetof(VehicleManager, mfTailgatingVunerabilityTime) == 171552 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +171552 = 1.0f (value recovered from the PS3 build)");
        static_assert(offsetof(VehicleManager, mfTBoneTakedownMaxAngle)    == 171564 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +171564 = 35.0f");
        static_assert(offsetof(VehicleManager, mfTBoneTakedownSpeed)       == 171568 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +171568 = 30.0f -- was `mfTBoneSidePlaneHalfWidth`");
        static_assert(offsetof(VehicleManager, mfMinShuntSpeed)            == 171580 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +171580 = 12.0f -- was `mfNudgeMaxClosingSpeed`");
        static_assert(offsetof(VehicleManager, mfFatalShuntSpeed)          == 171584 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +171584 = 140.0f -- was `mfShuntMaxClosingSpeed`");
        static_assert(offsetof(VehicleManager, mfMinTradingPaintSpeed)     == 171616 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +171616 = 0.8f");
        static_assert(offsetof(VehicleManager, mfFatalSlamSpeed)           == 171620 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +171620 = 140.0f -- was `mfTradingPaintMaxSpeed`");
        static_assert(offsetof(VehicleManager, mfMaxHeadToHeadAngle)       == 171628 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +171628 = 45.0f");
        static_assert(offsetof(VehicleManager, mfMinHeadToHeadSpeed)       == 171632 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +171632 = 40.0f");
        static_assert(offsetof(VehicleManager, mfMinHeadToHeadIndividualSpeed) == 171636 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +171636 = 40.0f");
        static_assert(offsetof(VehicleManager, mfAngleForVerticleTakedown) == 171640 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "run tail (asm +171640 = 60.0f)");
        static_assert(offsetof(VehicleManager, maeImpactType)              == 171644 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT,
                      "THE CLOSURE: 171468 + 44*4 == 171644, the next independently asm-proven member");

        // ---- per-car impact bookkeeping (DWARF :923..:934) -------------------------------------
        static_assert(offsetof(VehicleManager, mauImpactScore)          == 171676 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +171676 (stbx 1 per victim)");
        static_assert(offsetof(VehicleManager, mafNoImpactTimeSeconds)  == 171684 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +171684 -- was mis-typed `s32[8] maRaceCarLastAttacker`");
        static_assert(sizeof(VehicleManager::mafNoImpactTimeSeconds) == 32,
                      "8 x f32: HandleRaceCarRaceCarContact seeds it with lfsx/stfsx, so it is a FLOAT array");
        static_assert(offsetof(VehicleManager, maiPhysicsSlamIndex)     == 171716 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "DWARF :926");
        static_assert(offsetof(VehicleManager, mPlayerWonImpact)        == 171736 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +171736 (DWARF :934) -- was `mTakenDownRaceCarsBitArray`");

        // ---- the per-car vulnerability / grinding arrays (DWARF :937..:951) --------------------
        // These two bases are what demote the committed scalars `mfGrindingThresholdA` (@171868)
        // and `mfGrindingThresholdB` (@171900) to ELEMENT 7 of two per-car arrays.
        static_assert(offsetof(VehicleManager, mafVulnerableTimeSeconds) == 171744 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "DWARF :937");
        static_assert(offsetof(VehicleManager, mafPlayerGrindingOtherDurationSeconds) == 171840 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT,
                      "base; 171840 + 7*4 == 171868 == the old scalar mfGrindingThresholdA seat");
        static_assert(offsetof(VehicleManager, mafOtherGrindingPlayerDurationSeconds) == 171872 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT,
                      "base; 171872 + 7*4 == 171900 == the old scalar mfGrindingThresholdB seat");
        static_assert(offsetof(VehicleManager, mabRubbingThisUpdate)    == 171952 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "DWARF :951");

        // ---- the spare AI driver and the run that closes onto mePlayerActiveRaceCarIndex -------
        static_assert(offsetof(VehicleManager, mPlayerAiDriver)          == 171968 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm VehicleDriver::Construct(this + 171968)");
        static_assert(offsetof(VehicleManager, mbPlayerAiDriverValid)    == 172192 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "DWARF :954");
        static_assert(offsetof(VehicleManager, mfSteeringUpdateRemainder) == 172200 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "DWARF :956");
        static_assert(offsetof(VehicleManager, mePlayerActiveRaceCarIndex) == 172204 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm/DWARF +172204");

        // ---- the six world/traffic crash thresholds (DWARF :962..:968) -------------------------
        static_assert(offsetof(VehicleManager, mfCrashingAICollisionCrashThresholdMPH) == 172208 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +172208 = 50.0f");
        static_assert(offsetof(VehicleManager, mfHeadOnWorldCrashThreshold)   == 172212 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +172212 = 40.5f");
        static_assert(offsetof(VehicleManager, mfSideOnWorldCrashThreshold)   == 172216 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +172216 = 50.0f");
        static_assert(offsetof(VehicleManager, mfTrafficCollisionCheckThresholdMPH) == 172220 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +172220 = 30.0f");
        static_assert(offsetof(VehicleManager, mfMinRCTrafficTranslateSpeedMPH) == 172224 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +172224 = 40.0f");
        static_assert(offsetof(VehicleManager, mfVerticalTakedownAngleDeg)    == 172228 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +172228 = 65.0f");

        static_assert(offsetof(VehicleManager, mCameraMatrix) == 172240 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm 4 x stvx128 from this+172240");
        static_assert(offsetof(VehicleManager, mCameraMatrix) % 16 == 0,
                      "stvx128 requires 16-alignment -- a compiler-inserted pad here would fault at runtime");

        // ---- the 16 gameplay/debug bools (DWARF :972..:988) ------------------------------------
        static_assert(offsetof(VehicleManager, mbImpactTime)             == 172304 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +172304");
        static_assert(offsetof(VehicleManager, mbStopPlayerCrashing)     == 172306 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +172306");
        static_assert(offsetof(VehicleManager, mbStopAICrashing)         == 172307 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +172307 -- was `mbSuppressIfAlreadyCrashState1`");
        static_assert(offsetof(VehicleManager, DEBUG_mbHornTakedownEnabled) == 172311 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "DWARF :979");
        static_assert(offsetof(VehicleManager, mbTrafficCheckingAllowed) == 172313 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +172313 -- the one bool Construct seeds TRUE");
        static_assert(offsetof(VehicleManager, mbIsOnlineGameMode)       == 172315 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +172315 -- was `mbStationaryTakedownsEnabled`");
        static_assert(offsetof(VehicleManager, mbPlayerCarInJunkYard)    == 172319 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +172319 (DWARF :988)");

        // ---- the player/car stat block (DWARF :993..:1007) -------------------------------------
        // The store opcodes are the type proof: stfsx for the two floats, stwx for the five words.
        static_assert(offsetof(VehicleManager, mfPlayerStatStrength)     == 172320 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm stfsx @+172320");
        static_assert(offsetof(VehicleManager, mfPlayerStatDamageLimit)  == 172324 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm stfsx @+172324");
        static_assert(offsetof(VehicleManager, miCarSpeed)               == 172328 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm stwx @+172328 -- was `f32 maPlayerCarStats[0]`");
        static_assert(offsetof(VehicleManager, miCarStrength)            == 172332 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm stwx @+172332");
        static_assert(offsetof(VehicleManager, miCarControl)             == 172336 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm stwx @+172336");
        static_assert(offsetof(VehicleManager, miCarBoost)               == 172340 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm stwx @+172340");
        static_assert(offsetof(VehicleManager, meCarType)                == 172344 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm stwx @+172344, seeded 3");
        static_assert(offsetof(VehicleManager, miPlayerBoost)            == 172360 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "DWARF :1007");
        static_assert(offsetof(VehicleManager, meCurrentGameModeType)    == 172380 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm stwx -1 @+172380");

        // ---- the eight car-stat strength scalars (DWARF :1015..:1023) --------------------------
        static_assert(offsetof(VehicleManager, mfCarStatStrengthSlamMax) == 172384 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +172384 = 2.0f");
        static_assert(offsetof(VehicleManager, mfCarrStatStrengthBeingShuntedMin) == 172412 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm +172412 = 0.05f (last of the eight)");

        // ---- the cached car-vs-car prediction (DWARF :1026..:1029) -----------------------------
        static_assert(offsetof(VehicleManager, muCachedCarASlot)         == 172416 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm stwx 0 @+172416");
        static_assert(offsetof(VehicleManager, muCachedCarBSlot)         == 172420 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm stwx 0 @+172420");
        static_assert(offsetof(VehicleManager, mbCachedCarCarPredictionResult) == 172424 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm stbx 0 @+172424");
        static_assert(offsetof(VehicleManager, mCachedCarCarPredictionNormal) == 172432 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT,
                      "asm stvx128 v0,r31,r9 with r9 == 172432 -- the 16 bytes of unk_82181520, i.e. {0,0,1,0}");
        static_assert(offsetof(VehicleManager, mCachedCarCarPredictionNormal) % 16 == 0,
                      "loaded/stored with lvx128/stvx128 -- must be 16-aligned");
        // ⚠️ The pointer-width decision this span depends on. If muCachedCarA/BSlot are ever widened
        // to real 8-byte pointers, the assert above is what will catch it -- do not "fix" it by
        // moving the normal.
        static_assert(sizeof(VehicleManager::muCachedCarASlot) == 4,
                      "mpCachedCarA/B are modelled as 32-bit slots so the 16-aligned prediction normal keeps +172432 on x64");

        // ---- the tail (DWARF :1032..:1088) ------------------------------------------------------
        static_assert(offsetof(VehicleManager, meStationaryPlayerWheelAngle) == 172448 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm stwx 2 @+172448");
        static_assert(offsetof(VehicleManager, mbCrashRaceCarWhenFatal)  == 172452 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm stbx 1 @+172452");
        static_assert(offsetof(VehicleManager, meShowtimeBehaviour)      == 172456 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm stwx 2 @+172456");
        static_assert(offsetof(VehicleManager, miRaceCarWorldContactValidationPM) == 172460 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT,
                      "asm stores the 30th AddMonitor handle here; named by the console's own assert text");
        static_assert(offsetof(VehicleManager, miContactStreamCounterA)  == 172580 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm stwx 0 @+172580");
        static_assert(offsetof(VehicleManager, miContactStreamCounterB)  == 172584 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm stwx 0 @+172584");
        static_assert(offsetof(VehicleManager, mStuckInCollisionTestCacheSphere) == 172592 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT,
                      "asm stvx128 v127,r31,r11 with r11 == 172592 (DWARF :1087 Sphere)");
        static_assert(offsetof(VehicleManager, mbPlayerCarStuckInCollision) == 172608 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT,
                      "172592 + 16 == 172608: the Sphere/bool pair closes to the byte (DWARF :1087/:1088)");
        static_assert(offsetof(VehicleManager, muTakedownEventsThisFrame) == 172612 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT, "asm stwx 0 @+172612");

        // ---- shape guards the offsets alone cannot catch ---------------------------------------
        // `sizeof` is permutation-blind and tail padding can absorb a grown array, so pin the END of
        // the data region as well as the seats inside it (the standing rule from the wave that lost
        // a 4->8 array growth to tail padding).
        static_assert(sizeof(VehicleManager) >= 172616 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT,
                      "the class must extend at least to the end of muTakedownEventsThisFrame");
        // ⭐ AND PIN THE END EXACTLY, not just `>=`. The standing rule from the wave that lost a
        // 4 -> 8 array growth to tail padding: a `>=` on sizeof cannot see a trailing member that
        // grew into slack. 172616 + 224 == 172840, rounded up to this class's 16-alignment == 172848
        // (MEASURED). This assert has already earned its keep once: it was written as 172816 (the
        // one-drift-term number) and it is the ONLY thing that fired when the debug component was
        // un-pinned in the same wave -- the correct response was the second named drift term above,
        // not a bigger literal here.
        static_assert(sizeof(VehicleManager) == 172848,
                      "MEASURED total. X360 end 172616 + 224 drift == 172840 -> 16-aligned 172848");
        static_assert(sizeof(VehicleManager::maeImpactType) == 32, "EImpactType[8]");
        static_assert(sizeof(VehicleManager::mauImpactScore) == 8, "uint8[8]");
        static_assert(sizeof(VehicleManager::mafPlayerGrindingOtherDurationSeconds) == 32, "f32[8] -- NOT a scalar threshold");
        static_assert(sizeof(VehicleManager::mafOtherGrindingPlayerDurationSeconds) == 32, "f32[8] -- NOT a scalar threshold");
        static_assert(sizeof(VehicleManager::mStuckInCollisionTestCacheSphere) == 16, "one stvx128 == 16 bytes");

        // =========================================================================================
        // ⭐⭐ THE RaceCarVehicleRecord IN-RECORD SEATS -- ADDED 2026-08-03 (RaceCarPhysics own-block
        // wave), AND THEY HAD NEVER BEEN CHECKED BY ANYTHING.
        //
        // BrnVehicleManager.h pins ten in-record field offsets on the 5216-byte stand-in for
        // RaceCarPhysics, and says so. The asserts that were supposed to enforce them live in
        // BrnVehicleManager.cpp (nine of them) and BrnVehicleManagerPlayerStats.cpp (one), and
        // NEITHER TU IS MOUNTED -- which is the whole reason this file exists for the outer class.
        // The record's seats had exactly the same hole and it had been missed because the header
        // reads as if the asserts were live. Duplicated here, in the mounted TU, so they run.
        //
        // ⭐⭐ ALL THREE SUSPECTS RESOLVED 2026-08-03 (VehiclePhysics own-block wave), and four more
        // fields renamed with them. This gate previously asserted mfProximityRadiusSq @1904,
        // mvWorldPosition @1920 and mCrashMatrix @3328 AS COMMITTED, with a note saying "when the
        // VehiclePhysics own-block pass moves them, these three lines are what will fail". That is
        // exactly what happened: all three were PHANTOMS (the first two are rows of the base's
        // mTransform, the third is the 16-byte mCrashNormal at in-record 5184), so the lines are
        // gone rather than re-typed, and the four survivors below are asserted under the names the
        // recovered VehiclePhysics / RaceCarPhysics blocks give them.
        // =========================================================================================
        static_assert(sizeof(RaceCarVehicleRecord) == 5216,
                      "per-car stride (asm: VehicleManager::Construct `addi r29, r29, 0x1460`)");
        static_assert(offsetof(RaceCarVehicleRecord, mTransform) == 16,
                      "ExternallySimulatedBody::mTransform -- the base sub-object starts at +0x10 "
                      "because the leaf vptr occupies +0x00 (SimpleVehiclePhysics::Construct "
                      "@0x82620400 `addi r3,r31,0x10 ; bl ExternalPhysicsBody::Construct`)");
        static_assert(offsetof(RaceCarVehicleRecord, mbCrashing) == 1808,
                      "SimpleVehiclePhysics::mbCrashing -- named by the console's own assert string "
                      "at RaceCarPhysics.h:328 (GetNormalCausingCrash @0x825B3944)");
        static_assert(offsetof(RaceCarVehicleRecord,
                               mvSpeedOnLastCrashMPH_TimeCrashing_CounterSteerSideMag_Spare) == 3824,
                      "VehiclePhysics::mvSpeedOnLastCrashMPH_... @+0xEF0 (asm: Construct "
                      "`li r28,0xEF0 ; stvx128 v127,r31,r28`, between mWeightTransfer and mEngine)");
        static_assert(offsetof(RaceCarVehicleRecord, meDriverType) == 4308,
                      "VehiclePhysics::mPreviousControls.meDriverType == 0x1090 + 0x44 (asm: "
                      "VehiclePhysics::Prepare `stw r30,0x10D4(r31)`; read with `lwz`, 4 bytes)");
        static_assert(offsetof(RaceCarVehicleRecord, mbDeformationModelIsActive) == 4953,
                      "VehiclePhysics::mbDeformationModelIsActive @+0x1359 -- SetRaceCarCrashing's "
                      "`stb r20,0x1359(r11)` is applied to the RECORD base (`addi r11,r11,0x740` "
                      "two instructions earlier), so no -1856 correction applies");
        static_assert(offsetof(RaceCarVehicleRecord, meCarType) == 5084,
                      "VehiclePhysics::meCarType @+0x13DC (asm: VehiclePhysics::Prepare "
                      "`stw r8,0x13DC(r31)` and ApplyPlayerStats `stw r10,0x1B1C(5216*idx + this)`)");
        static_assert(offsetof(RaceCarVehicleRecord, mCrashNormal) == 5184,
                      "RaceCarPhysics::mCrashNormal @+0x1440 -- the single 16-byte `stvx128 v127, "
                      "r30, 0x1440` the retired `mCrashMatrix` @3328 was a mis-based reading of");
        static_assert(offsetof(RaceCarVehicleRecord, mfTimeSinceTookDownPlayer) == 5120,
                      "RaceCarPhysics::mfTimeSinceTookDownPlayer @+0x1400 (DWARF :395)");
        static_assert(offsetof(RaceCarVehicleRecord, mEntityCausingCrash) == 5200,
                      "RaceCarPhysics::mEntityCausingCrash @+0x1450 (DWARF :415); "
                      "SetRaceCarCrashing @0x82635478 `stw r26, 0x1450(r30)`");
        static_assert(sizeof(RaceCarCrashData) == 12, "RaceCarCrashData stride (asm: 12)");

        // ⭐ THE CROSS-CHECK BETWEEN THE TWO MODELS OF THE SAME CLASS. The record is byte-pinned to
        // X360 and RaceCarPhysics is not, so they can only be compared by DELTA -- but a delta is
        // enough, because the two fields below bracket the whole RaceCarPhysics own block and were
        // derived from completely different evidence (this side from SetRaceCarCrashing's store
        // offsets; the other side from the DecFIGS DWARF member order closing on this class's own
        // stride). Both terms of the right-hand side are X360 literals.
        static_assert(offsetof(RaceCarVehicleRecord, mEntityCausingCrash)
                          - offsetof(RaceCarVehicleRecord, mfTimeSinceTookDownPlayer)
                          == 0x1450 - 0x1400,
                      "the record's two settled fields must be the same 0x50 apart as "
                      "RaceCarPhysics::mEntityCausingCrash and ::mfTimeSinceTookDownPlayer");
    }
}
}
