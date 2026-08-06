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
// The per-car physics DebugComponent VehicleManager::Construct casts the opaque 8x1024 span to.
// Needed as a COMPLETE type here so the fit/alignment assert at the bottom is real and not a
// promise about a forward declaration.
#include "GameSource/Physics/VehicleManager/VehiclePhysics/B5PhysicsHandlingDebugComponent.h"

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
                      "the drift is a multiple of 16 (asserted above), so embedding it inserted no "
                      "leading pad");
        static_assert(offsetof(VehicleManager, mDiscardedContacts)
                          + sizeof(VehicleManager::mDiscardedContacts)
                          == offsetof(VehicleManager, mDebugComponent),
                      "and the discarded-contact queue abuts it with no gap, exactly as on the X360");

        // ==========================================================================================
        // ✅ UN-BLOCKED 2026-08-03 -- the contained traffic manager, embedded BY NAME.
        //
        // This block used to read "⛔ BLOCKED -- grows 2480 bytes on x64", measured against a
        // 103360-byte opaque span. The SPAN was wrong, not the class: the X360 size is 105648
        // (BrnPhysicalTrafficManager.h finding (4), derived twice) and the host DRIFT is -3968
        // (2026-08-03, once maFullTrafficPhysics became the real TrafficPhysics[20]; it was +192
        // while that array was a byte-pinned stand-in), which BrnVehicleManager.h carries as
        // KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER.
        //
        // The three numbers are tied together here so none of them can drift alone.
        // ==========================================================================================
        static_assert(offsetof(VehicleManager, mPhysicalTrafficManager) == 44768 + KU_HOST_DRIFT_AFTER_RACECAR_ARRAY,
                      "asm: addis r3,r31,1 ; addi r3,r3,-0x5120 ; bl PhysicalTrafficManager::Construct "
                      "@0x8263BF9C -- the embedded manager keeps its X360 seat (nothing before it moved)");
        // ⭐ 2026-08-03 (the record-fold wave): this used to read
        // `KU_X360_SIZEOF_... + KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER`, which was right only while
        // that term was the FIRST drift zone. It is now the SECOND (the race-car array's -1664 sits
        // ahead of it), so what belongs here is the ZONE STEP -- the difference between the term
        // after this sub-object and the term before it. That is a strictly stronger assert: it fires
        // if either term moves independently, which the absolute form could not see.
        // ⛔ THIS FIRED FOR REAL when the third term landed, and the wrong fix would have been to
        // bump the literal. The right one is here.
        static_assert(sizeof(PhysicalTrafficManager)
                          == KU_X360_SIZEOF_PHYSICAL_TRAFFIC_MANAGER
                             + (KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER - KU_HOST_DRIFT_AFTER_RACECAR_ARRAY),
                      "MEASURED host size 101680 == DERIVED X360 size 105648 + the -3968 step this "
                      "sub-object contributes. If this fires, one of the three has moved: re-derive, "
                      "do not just bump the drift.");
        static_assert((KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER - KU_HOST_DRIFT_AFTER_RACECAR_ARRAY) % 16 == 0
                      && KU_HOST_DRIFT_AFTER_TRAFFIC_MANAGER % 16 == 0,
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
        // ⚠️⚠️ REWRITTEN 2026-08-03 (the TrafficPhysics de-fork). What stood here was
        //     static_assert(sizeof(TrafficPhysics) == 5168, "asm: mulli r11, r29, 0x1430 ...");
        // -- a HOST sizeof gate wearing a console literal. It held only because TrafficPhysics was a
        // byte-pinned `u8[5168]` stand-in, i.e. it asserted that the stand-in was still a stand-in.
        // Now that the member is the real class, the console 0x1430 is asserted where it belongs:
        // as CONSOLE ARITHMETIC over the recovered member seats, in TrafficPhysics_layout_check.cpp,
        // whose chain closes on this very 5168. Here what is checked is the thing this TU can see --
        // that the array is exactly 20 elements with no pad behind it, and that the console
        // identification still holds as console arithmetic.
        static_assert(X360Layout::KU_TP_SIZEOF == 5168u,
                      "asm: mulli r11, r29, 0x1430 -- the 20-element per-vehicle walk in "
                      "PhysicalTrafficManager::Construct @0x82636CA8");
        static_assert(20u * X360Layout::KU_TP_SIZEOF + 44768u == 148128u,
                      "⭐ THE IDENTIFICATION, as console arithmetic: 44768 + 20*0x1430 == 148128, the "
                      "exact class offset the old `EntityId maRaceCarEntityIdRemap[8]` sibling "
                      "claimed. Same address, real name, and the real bound is 20 (a TRAFFIC index), "
                      "not 8.");
        static_assert(offsetof(PhysicalTrafficManager, maTrafficEntityIDs)
                          == 20u * sizeof(TrafficPhysics),
                      "the entity-id array must abut maFullTrafficPhysics[20] with no pad -- this is "
                      "what fires if the array bound changes or TrafficPhysics grows an alignment "
                      "that inserts one");
        static_assert(offsetof(PhysicalTrafficManager, maTrafficEntityIDs) == 99200,
                      "MEASURED host seat (X360 103360 + the array's 20 * (4960 - 5168) == -4160). A "
                      "literal, so it is a tripwire in both directions rather than a restatement of "
                      "the line above.");
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
        // ⭐ ADDED 2026-08-06 (PhysicsModule::Update leaves wave): the two contact-generation
        // pointers carved from the head of the old +172465..+172580 opaque span (DWARF :1045/:1046;
        // asm seats FreeAllocations @0x8261BAE0 / StartVehicleTractionLineTests @0x82629CE0). The
        // extra +4/+8 terms are the HOST-ONLY widening inside the carve: 4 alignment bytes so the
        // 8-byte pointers seat on an 8 boundary, then each pointer is 4 bytes wider than the
        // console's. The growth is absorbed by the REMAINING opaque run of the same span, which is
        // why miContactStreamCounterA below keeps its seat -- that pair of asserts is the carve's
        // whole tripwire.
        static_assert(offsetof(VehicleManager, mpContactGenList)    == 172468 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT + 4,
                      "console +172468; +4 = host 8-alignment of the pointer pair");
        static_assert(offsetof(VehicleManager, mpContactGenerator)  == 172472 + KU_HOST_DRIFT_AFTER_DEBUG_COMPONENT + 8,
                      "console +172472; +8 = the alignment pad + mpContactGenList's own 4->8 widening");
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
        // grew into slack. 172616 + (-1440) == 171176, rounded up to this class's 16-alignment
        // == 171184 (MEASURED).
        // ⭐ THIS ASSERT HAS NOW EARNED ITS KEEP TWICE. It was written as 172816 (the one-drift-term
        // number) and was the ONLY thing that fired when the debug component was un-pinned; it was
        // 172848 (the two-term number) and was one of two things that fired when the race-car array
        // was folded to the real RaceCarPhysics. Both times the correct response was the named drift
        // term above, not a bigger literal here -- and both times the literal moved only after the
        // term did.
        // ⭐ THREE TIMES NOW: 172816 -> 172848 -> 171184 -> 167024, and every move followed a named
        // drift term, never the other way round. This one is the TrafficPhysics de-fork.
        static_assert(sizeof(VehicleManager) == 167024,
                      "MEASURED total. X360 end 172616 + (-5600) drift == 167016 -> 16-aligned 167024");
        static_assert(sizeof(VehicleManager::maeImpactType) == 32, "EImpactType[8]");
        static_assert(sizeof(VehicleManager::mauImpactScore) == 8, "uint8[8]");
        static_assert(sizeof(VehicleManager::mafPlayerGrindingOtherDurationSeconds) == 32, "f32[8] -- NOT a scalar threshold");
        static_assert(sizeof(VehicleManager::mafOtherGrindingPlayerDurationSeconds) == 32, "f32[8] -- NOT a scalar threshold");
        static_assert(sizeof(VehicleManager::mStuckInCollisionTestCacheSphere) == 16, "one stvx128 == 16 bytes");

        // =========================================================================================
        // ⭐⭐⭐ THE RECORD IS GONE -- FOLDED 2026-08-03. `maRaceCarVehicles` is
        // `BrnPhysics::Vehicle::RaceCarPhysics[8]`, the real class.
        //
        // WHAT STOOD HERE, and why it could not be kept. This block held ten
        // `offsetof(RaceCarVehicleRecord, ...) == <X360 in-record seat>` asserts -- the byte-pinning
        // gate for the stand-in. A host class does not reproduce console offsets, so after the fold
        // every one of those ten would be FALSE. Re-basing them to whatever the host happens to
        // produce would be the worst of both: a green gate that measures nothing. They are deleted,
        // and the seats they guarded are guarded elsewhere:
        //     RaceCarPhysics_layout_check.cpp   the +0x13F0..+0x1460 own block, closing on 5216
        //     VehiclePhysics_layout_check.cpp   +0x130..+0x720 and +0x720..+0x13F0
        // Both are MOUNTED, both assert CONSOLE ARITHMETIC over X360Layout literals, and between
        // them every one of the ten former record fields is additionally named in a
        // `(void)offsetof(...)` existence check -- so a rename or a deletion still fails the build.
        //
        // WHAT IS STILL THIS FILE'S JOB is the claim the fold actually makes about THIS class: that
        // the element is the real type, that its host size is the number the drift term is derived
        // from, and that the array still starts on the asm-literal +1856 at 16 alignment. Those are
        // checkable and they are checked. Tamper-tested: changing either constant fires.
        // =========================================================================================
        static_assert(sizeof(RaceCarPhysics) == 5008,
                      "host sizeof(RaceCarPhysics). NOT an X360 number -- the console class is 5216 "
                      "(`mulli r11,r22,0x1460`). This line exists so the class cannot change size "
                      "without the drift term below being revisited");
        static_assert(8 * (5216 - static_cast<std::ptrdiff_t>(sizeof(RaceCarPhysics)))
                          == -KU_HOST_DRIFT_AFTER_RACECAR_ARRAY,
                      "⭐ KU_HOST_DRIFT_AFTER_RACECAR_ARRAY must BE 8 * (console stride - host "
                      "sizeof). It is written as a literal in the header, deliberately, so this is "
                      "a tripwire in both directions and not a definition");
        static_assert(alignof(RaceCarPhysics) == 16,
                      "the array element must stay 16-aligned so element 0 keeps the asm-literal "
                      "+1856 base (1856 % 16 == 0)");
        static_assert(offsetof(VehicleManager, maRaceCarVehicles) == 1856,
                      "⭐ the fold must NOT move element 0: 1856 is the asm literal "
                      "VehicleManager::Construct hands VehiclePhysics::Construct "
                      "(`addi r3, r29, -0x140D`)");
        static_assert(offsetof(VehicleManager, maRaceCarEntityIDs)
                          == offsetof(VehicleManager, maRaceCarVehicles) + 8 * sizeof(RaceCarPhysics),
                      "the eight-car array must abut maRaceCarEntityIDs with no slack -- the same "
                      "closure the X360's 1856 + 8*5216 == 43584 has");
        static_assert(sizeof(RaceCarCrashData) == 12, "RaceCarCrashData stride (asm: 12)");

        // ⭐ The eight-car loop bound VehicleManager::Construct uses must BE the width of every
        // per-car array it walks. The 8 is asm-literal (`li r23, 8`); this pins that the constant
        // and the arrays cannot drift apart, which is the only way that loop could run off an end.
        static_assert(sizeof(VehicleManager::maRaceCarVehicles) / sizeof(RaceCarPhysics)
                          == VehicleManager::KI_MAX_ACTIVE_RACE_CARS, "maRaceCarVehicles[8]");
        static_assert(sizeof(VehicleManager::maRaceCarDrivers) / sizeof(VehicleDriver)
                          == VehicleManager::KI_MAX_ACTIVE_RACE_CARS, "maRaceCarDrivers[8]");
        static_assert(sizeof(VehicleManager::maRaceCarEntityIDs) / sizeof(EntityId)
                          == VehicleManager::KI_MAX_ACTIVE_RACE_CARS, "maRaceCarEntityIDs[8]");
        static_assert(sizeof(VehicleManager::maRaceCarHandlingBodyIDs) / sizeof(u64)
                          == VehicleManager::KI_MAX_ACTIVE_RACE_CARS, "maRaceCarHandlingBodyIDs[8]");
        static_assert(sizeof(VehicleManager::maeRaceCarTypes) / sizeof(BrnWorld::ERaceCarType)
                          == VehicleManager::KI_MAX_ACTIVE_RACE_CARS, "maeRaceCarTypes[8]");
        static_assert(sizeof(VehicleManager::mauNetworkCarHiddenFramesRemaining) / sizeof(u32)
                          == VehicleManager::KI_MAX_ACTIVE_RACE_CARS, "mauNetworkCarHiddenFrames[8]");
        // ⭐ AND the debug-component span the loop casts into: 8 slots of 1024, big enough and
        // aligned enough for the reconstructed DebugComponent (112 bytes, align 16). If the class
        // ever outgrows the slot, the reinterpret_cast in the loop stops being merely inert.
        static_assert(sizeof(VehicleManager::maRaceCarDebugComponent)
                          == VehicleManager::KI_MAX_ACTIVE_RACE_CARS * 1024, "8 x 1024 (asm stride 0x400)");
        static_assert(sizeof(DebugComponent) <= 1024 && (1024 % alignof(DebugComponent)) == 0,
                      "the reconstructed per-car DebugComponent must still fit its opaque slot");
    }
}
}
