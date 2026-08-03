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

#include <cstddef>   // offsetof

namespace BrnPhysics
{
namespace Vehicle
{
    void VehicleManager::_AssertLayoutTuningBank()
    {
        // ---- the two master gates (DWARF :865/:866) -------------------------------------------
        static_assert(offsetof(VehicleManager, mbSlamsAndShuntsOn) == 171464,
                      "mbSlamsAndShuntsOn (asm stbx 1 @+171464)");
        static_assert(offsetof(VehicleManager, mbAllowSlamsAndShuntsEffectsForRivals) == 171465,
                      "mbAllowSlamsAndShuntsEffectsForRivals (asm stbx 1 @+171465)");

        // ---- the 44-float tuning run (DWARF :868..:920) ----------------------------------------
        // Head, tail, the two retyped/renamed seats in the middle, and the closure. The closure is
        // the real test: 171468 + 44*4 == 171644 only holds if EVERY float in between is declared
        // and none of the old padding runs survived.
        static_assert(offsetof(VehicleManager, mfFrontRaySensorLength)     == 171468, "run head (asm +171468 = 4.0f)");
        static_assert(offsetof(VehicleManager, mfMaxSlamClosingXSpeed)     == 171536, "asm +171536 = 16.0f");
        static_assert(offsetof(VehicleManager, mfMinSecondsBetweenImpacts) == 171540, "asm +171540 = 0.3f -- was mis-typed `s32 miAttackerToRecord`");
        static_assert(offsetof(VehicleManager, mfTailgatingVunerabilityTime) == 171552, "asm +171552 = 1.0f (value recovered from the PS3 build)");
        static_assert(offsetof(VehicleManager, mfTBoneTakedownMaxAngle)    == 171564, "asm +171564 = 35.0f");
        static_assert(offsetof(VehicleManager, mfTBoneTakedownSpeed)       == 171568, "asm +171568 = 30.0f -- was `mfTBoneSidePlaneHalfWidth`");
        static_assert(offsetof(VehicleManager, mfMinShuntSpeed)            == 171580, "asm +171580 = 12.0f -- was `mfNudgeMaxClosingSpeed`");
        static_assert(offsetof(VehicleManager, mfFatalShuntSpeed)          == 171584, "asm +171584 = 140.0f -- was `mfShuntMaxClosingSpeed`");
        static_assert(offsetof(VehicleManager, mfMinTradingPaintSpeed)     == 171616, "asm +171616 = 0.8f");
        static_assert(offsetof(VehicleManager, mfFatalSlamSpeed)           == 171620, "asm +171620 = 140.0f -- was `mfTradingPaintMaxSpeed`");
        static_assert(offsetof(VehicleManager, mfMaxHeadToHeadAngle)       == 171628, "asm +171628 = 45.0f");
        static_assert(offsetof(VehicleManager, mfMinHeadToHeadSpeed)       == 171632, "asm +171632 = 40.0f");
        static_assert(offsetof(VehicleManager, mfMinHeadToHeadIndividualSpeed) == 171636, "asm +171636 = 40.0f");
        static_assert(offsetof(VehicleManager, mfAngleForVerticleTakedown) == 171640, "run tail (asm +171640 = 60.0f)");
        static_assert(offsetof(VehicleManager, maeImpactType)              == 171644,
                      "THE CLOSURE: 171468 + 44*4 == 171644, the next independently asm-proven member");

        // ---- per-car impact bookkeeping (DWARF :923..:934) -------------------------------------
        static_assert(offsetof(VehicleManager, mauImpactScore)          == 171676, "asm +171676 (stbx 1 per victim)");
        static_assert(offsetof(VehicleManager, mafNoImpactTimeSeconds)  == 171684, "asm +171684 -- was mis-typed `s32[8] maRaceCarLastAttacker`");
        static_assert(sizeof(VehicleManager::mafNoImpactTimeSeconds) == 32,
                      "8 x f32: HandleRaceCarRaceCarContact seeds it with lfsx/stfsx, so it is a FLOAT array");
        static_assert(offsetof(VehicleManager, maiPhysicsSlamIndex)     == 171716, "DWARF :926");
        static_assert(offsetof(VehicleManager, mPlayerWonImpact)        == 171736, "asm +171736 (DWARF :934) -- was `mTakenDownRaceCarsBitArray`");

        // ---- the per-car vulnerability / grinding arrays (DWARF :937..:951) --------------------
        // These two bases are what demote the committed scalars `mfGrindingThresholdA` (@171868)
        // and `mfGrindingThresholdB` (@171900) to ELEMENT 7 of two per-car arrays.
        static_assert(offsetof(VehicleManager, mafVulnerableTimeSeconds) == 171744, "DWARF :937");
        static_assert(offsetof(VehicleManager, mafPlayerGrindingOtherDurationSeconds) == 171840,
                      "base; 171840 + 7*4 == 171868 == the old scalar mfGrindingThresholdA seat");
        static_assert(offsetof(VehicleManager, mafOtherGrindingPlayerDurationSeconds) == 171872,
                      "base; 171872 + 7*4 == 171900 == the old scalar mfGrindingThresholdB seat");
        static_assert(offsetof(VehicleManager, mabRubbingThisUpdate)    == 171952, "DWARF :951");

        // ---- the spare AI driver and the run that closes onto mePlayerActiveRaceCarIndex -------
        static_assert(offsetof(VehicleManager, mPlayerAiDriver)          == 171968, "asm VehicleDriver::Construct(this + 171968)");
        static_assert(offsetof(VehicleManager, mbPlayerAiDriverValid)    == 172192, "DWARF :954");
        static_assert(offsetof(VehicleManager, mfSteeringUpdateRemainder) == 172200, "DWARF :956");
        static_assert(offsetof(VehicleManager, mePlayerActiveRaceCarIndex) == 172204, "asm/DWARF +172204");

        // ---- the six world/traffic crash thresholds (DWARF :962..:968) -------------------------
        static_assert(offsetof(VehicleManager, mfCrashingAICollisionCrashThresholdMPH) == 172208, "asm +172208 = 50.0f");
        static_assert(offsetof(VehicleManager, mfHeadOnWorldCrashThreshold)   == 172212, "asm +172212 = 40.5f");
        static_assert(offsetof(VehicleManager, mfSideOnWorldCrashThreshold)   == 172216, "asm +172216 = 50.0f");
        static_assert(offsetof(VehicleManager, mfTrafficCollisionCheckThresholdMPH) == 172220, "asm +172220 = 30.0f");
        static_assert(offsetof(VehicleManager, mfMinRCTrafficTranslateSpeedMPH) == 172224, "asm +172224 = 40.0f");
        static_assert(offsetof(VehicleManager, mfVerticalTakedownAngleDeg)    == 172228, "asm +172228 = 65.0f");

        static_assert(offsetof(VehicleManager, mCameraMatrix) == 172240, "asm 4 x stvx128 from this+172240");
        static_assert(offsetof(VehicleManager, mCameraMatrix) % 16 == 0,
                      "stvx128 requires 16-alignment -- a compiler-inserted pad here would fault at runtime");

        // ---- the 16 gameplay/debug bools (DWARF :972..:988) ------------------------------------
        static_assert(offsetof(VehicleManager, mbImpactTime)             == 172304, "asm +172304");
        static_assert(offsetof(VehicleManager, mbStopPlayerCrashing)     == 172306, "asm +172306");
        static_assert(offsetof(VehicleManager, mbStopAICrashing)         == 172307, "asm +172307 -- was `mbSuppressIfAlreadyCrashState1`");
        static_assert(offsetof(VehicleManager, DEBUG_mbHornTakedownEnabled) == 172311, "DWARF :979");
        static_assert(offsetof(VehicleManager, mbTrafficCheckingAllowed) == 172313, "asm +172313 -- the one bool Construct seeds TRUE");
        static_assert(offsetof(VehicleManager, mbIsOnlineGameMode)       == 172315, "asm +172315 -- was `mbStationaryTakedownsEnabled`");
        static_assert(offsetof(VehicleManager, mbPlayerCarInJunkYard)    == 172319, "asm +172319 (DWARF :988)");

        // ---- the player/car stat block (DWARF :993..:1007) -------------------------------------
        // The store opcodes are the type proof: stfsx for the two floats, stwx for the five words.
        static_assert(offsetof(VehicleManager, mfPlayerStatStrength)     == 172320, "asm stfsx @+172320");
        static_assert(offsetof(VehicleManager, mfPlayerStatDamageLimit)  == 172324, "asm stfsx @+172324");
        static_assert(offsetof(VehicleManager, miCarSpeed)               == 172328, "asm stwx @+172328 -- was `f32 maPlayerCarStats[0]`");
        static_assert(offsetof(VehicleManager, miCarStrength)            == 172332, "asm stwx @+172332");
        static_assert(offsetof(VehicleManager, miCarControl)             == 172336, "asm stwx @+172336");
        static_assert(offsetof(VehicleManager, miCarBoost)               == 172340, "asm stwx @+172340");
        static_assert(offsetof(VehicleManager, meCarType)                == 172344, "asm stwx @+172344, seeded 3");
        static_assert(offsetof(VehicleManager, miPlayerBoost)            == 172360, "DWARF :1007");
        static_assert(offsetof(VehicleManager, meCurrentGameModeType)    == 172380, "asm stwx -1 @+172380");

        // ---- the eight car-stat strength scalars (DWARF :1015..:1023) --------------------------
        static_assert(offsetof(VehicleManager, mfCarStatStrengthSlamMax) == 172384, "asm +172384 = 2.0f");
        static_assert(offsetof(VehicleManager, mfCarrStatStrengthBeingShuntedMin) == 172412, "asm +172412 = 0.05f (last of the eight)");

        // ---- the cached car-vs-car prediction (DWARF :1026..:1029) -----------------------------
        static_assert(offsetof(VehicleManager, muCachedCarASlot)         == 172416, "asm stwx 0 @+172416");
        static_assert(offsetof(VehicleManager, muCachedCarBSlot)         == 172420, "asm stwx 0 @+172420");
        static_assert(offsetof(VehicleManager, mbCachedCarCarPredictionResult) == 172424, "asm stbx 0 @+172424");
        static_assert(offsetof(VehicleManager, mCachedCarCarPredictionNormal) == 172432,
                      "asm stvx128 v0,r31,r9 with r9 == 172432 -- the 16 bytes of unk_82181520, i.e. {0,0,1,0}");
        static_assert(offsetof(VehicleManager, mCachedCarCarPredictionNormal) % 16 == 0,
                      "loaded/stored with lvx128/stvx128 -- must be 16-aligned");
        // ⚠️ The pointer-width decision this span depends on. If muCachedCarA/BSlot are ever widened
        // to real 8-byte pointers, the assert above is what will catch it -- do not "fix" it by
        // moving the normal.
        static_assert(sizeof(VehicleManager::muCachedCarASlot) == 4,
                      "mpCachedCarA/B are modelled as 32-bit slots so the 16-aligned prediction normal keeps +172432 on x64");

        // ---- the tail (DWARF :1032..:1088) ------------------------------------------------------
        static_assert(offsetof(VehicleManager, meStationaryPlayerWheelAngle) == 172448, "asm stwx 2 @+172448");
        static_assert(offsetof(VehicleManager, mbCrashRaceCarWhenFatal)  == 172452, "asm stbx 1 @+172452");
        static_assert(offsetof(VehicleManager, meShowtimeBehaviour)      == 172456, "asm stwx 2 @+172456");
        static_assert(offsetof(VehicleManager, miRaceCarWorldContactValidationPM) == 172460,
                      "asm stores the 30th AddMonitor handle here; named by the console's own assert text");
        static_assert(offsetof(VehicleManager, miContactStreamCounterA)  == 172580, "asm stwx 0 @+172580");
        static_assert(offsetof(VehicleManager, miContactStreamCounterB)  == 172584, "asm stwx 0 @+172584");
        static_assert(offsetof(VehicleManager, mStuckInCollisionTestCacheSphere) == 172592,
                      "asm stvx128 v127,r31,r11 with r11 == 172592 (DWARF :1087 Sphere)");
        static_assert(offsetof(VehicleManager, mbPlayerCarStuckInCollision) == 172608,
                      "172592 + 16 == 172608: the Sphere/bool pair closes to the byte (DWARF :1087/:1088)");
        static_assert(offsetof(VehicleManager, muTakedownEventsThisFrame) == 172612, "asm stwx 0 @+172612");

        // ---- shape guards the offsets alone cannot catch ---------------------------------------
        // `sizeof` is permutation-blind and tail padding can absorb a grown array, so pin the END of
        // the data region as well as the seats inside it (the standing rule from the wave that lost
        // a 4->8 array growth to tail padding).
        static_assert(sizeof(VehicleManager) >= 172616,
                      "the class must extend at least to the end of muTakedownEventsThisFrame");
        static_assert(sizeof(VehicleManager::maeImpactType) == 32, "EImpactType[8]");
        static_assert(sizeof(VehicleManager::mauImpactScore) == 8, "uint8[8]");
        static_assert(sizeof(VehicleManager::mafPlayerGrindingOtherDurationSeconds) == 32, "f32[8] -- NOT a scalar threshold");
        static_assert(sizeof(VehicleManager::mafOtherGrindingPlayerDurationSeconds) == 32, "f32[8] -- NOT a scalar threshold");
        static_assert(sizeof(VehicleManager::mStuckInCollisionTestCacheSphere) == 16, "one stvx128 == 16 bytes");
    }
}
}
