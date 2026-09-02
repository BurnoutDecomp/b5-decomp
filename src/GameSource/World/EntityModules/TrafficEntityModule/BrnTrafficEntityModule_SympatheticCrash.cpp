// =================================================================================================
// GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule_SympatheticCrash.cpp
//
// THE CHAIN CRASH. A traffic car that sees another car crashing inside its cone stops driving,
// aims itself at the wreck, and then crashes ITSELF -- which is what turns one player impact
// into the pile-up the game is named for.
//
//   TrafficEntityModule::UpdateSympatheticCrashing            @0x8273D378 (594)  DWARF :1394
//   TrafficEntityModule::CrashVehicleForSympatheticCrashState @0x8272BA08 ( 95)  DWARF :1587
//
// No Feb-2007 source. ARTIST asm (the Hex-Rays output for @0x8273D378 is the degenerate
// "__asm { lvx128 ... }" form -- every branch below is read off the assembly);
// DecFIGS DWARF for declaration shape.
//
// -------------------------------------------------------------------------------------------
// WHY THIS ARM HAD NEVER RUN -- FOUR BREAKS IN ONE CHAIN, and this file closes the last two.
//
//   UpdateParams_TryStartSympatheticCrashing @0x827165D8   LIVE (_wT2_06.cpp)
//        Param::miBehaviour = KI_BEHAVIOUR_SLOWING_FOR_CRASH (== 0), mSympCrashTarget latched
//   UpdateVehiclesJob                                       LIVE (BrnUpdateVehiclesJob.cpp)
//        RequestNewPhysicalVehicle(..., E_PHYSICALREASON_SYMPATHETIC_CRASHING, target)
//   SafeRequestMakeVehiclePhysical @0x8274AFD0               (1) the seed was GATED
//        the console's 0x8274B2F4..0x8274B36C tail rolls mEffectRand and writes
//        meSympCrashState = ACCELERATE / HEADON. Without it IsSympatheticallyCrashing() is
//        permanently FALSE.
//   GenerateDriverInputs @0x82748E78                         (2) the arm was GATED
//   UpdateSympatheticCrashing @0x8273D378                    (3) NO BODY  <- this file
//   CrashVehicleForSympatheticCrashState @0x8272BA08         (4) NO BODY  <- this file
//        -> VehicleInputInterface::SetTrafficCrashing (live) -> mSetTrafficCrashingEventQueue
//        -> PhysicalTrafficManager::ProcessTrafficEvents (un-gated 2026-08-29)
//        -> SetTrafficVehicleCrashing (live)
//
// ⭐ NOT A REGRESSION RISK, and that is structural rather than hopeful: a car promoted with
// reason SYMPATHETIC_CRASHING(3) fails Vehicle::IsNormalPhysical() (which demands reason 5) and
// today falls through GenerateDriverInputs' whole ladder into its final `else { lbSend = 0; }`.
// It therefore sends NO driver record at all and coasts. Anything this arm does is strictly
// more than that.
//
// -------------------------------------------------------------------------------------------
// RECOVERED CONSTANTS (every one read out of the image at the address the asm names):
//   ⭐⭐ unk_8300CB90 = 0.44704f * 2.0f == 0.89408f -- RECOVERED 2026-08-30 (wave 2). It is
//       TWO MILES PER HOUR expressed in metres per second: the give-up arm fires once the car
//       is doing less than 2 mph, i.e. once it has effectively stopped.
//       HOW (no visual oracle, no tuning -- the DYN-INIT THUNK read out of the image):
//         The .data splat block 0x8300CB60..0x8300CBAC is all zero in the image because a CRT
//         dyn-init thunk fills it. Those thunks are NOT in the export set, which is why every
//         xref scan came up empty. Scanning the raw text for the operand form instead
//         (`addi rX, rX, 0xCB90` after `lis rX, 0x8301`) finds exactly two sites: our read at
//         0x8273D5B8 (lvx128) and ONE WRITER at 0x82C66A54 (stvx128). Its thunk, in full:
//           0x82C66A30  lis   r11, 0x82F3
//           0x82C66A38  lfs   f0,  0x1928(r11)   -> 0x82F31928 = 0.44704f  (MPH -> m/s)
//           0x82C66A3C  lis   r11, 0x820C
//           0x82C66A40  lfs   f13, 0xA86C(r11)   -> 0x820BA86C = 2.0f
//           0x82C66A48  fmuls f0, f0, f13                        (op 59, XO 25)
//           0x82C66A4C  stfs  f0, -16(r1) ; lvx v0,r0,r10 ; vspltw v0,v0,0
//           0x82C66A54  addi  r11, r11, 0xCB90  -> 0x8300CB90 ; stvx128 v0, r0, r11 ; blr
//       CALIBRATION (do not trust a decoder you have not seen agree with a known answer): the
//       same scan+decoder run on the NEIGHBOUR 0x8300CB80 finds its writer at 0x82C66128 taking
//       flt_820BA590, and this file's own banner already lists flt_820BA590 = 40.0f -- which is
//       exactly the "40.0f at runtime" an earlier wave attested for 0x8300CB80 from a different
//       direction. (It also fixes a detail of that note: 0x82C662D0 does not WRITE 0x8300CB80,
//       it READS it, squares it, and stores 1600.0f at 0x8300CC90 -- the swerve player-distance
//       cutoff.)
//       ⭐⭐ CRT INIT ORDER -- checked BEFORE trusting the value, because retail really does
//       ship a static-init-order bug elsewhere in this tree whose genuinely-0.0 result is
//       correct to reproduce. It does not apply here: BOTH operands are ordinary image
//       constants, not dyn-init outputs. 0x820BA86C is .rdata holding 2.0f, and 0x82F31928
//       holds 0.44704f in the image, sitting in a constants table between 1.5707964 (pi/2) and
//       6.2831855 (2*pi); the only text reference to 0x82F31928 besides this thunk is another
//       thunk READING it to splat 0x83017FE0 (the tree already knows that one as
//       KF_MPH_TO_METRES_PER_SECOND, BrnBehaviourGameplayExternal.cpp:1546). Nothing writes
//       either operand, so this thunk's result is 0.89408f no matter where it lands in the
//       init order.
//       ⭐ FAMILY: the tree has already recovered two siblings of this exact idiom --
//       BrnVehicleManager.cpp:99 (0.44704 * 50.0, "the old 0.0 made the gate a pass-through")
//       and BrnUpdateVehiclesJob.cpp:115 (1 / (0.44704 * 10.0)).
//       ⛔ THE 629 FRAMES WERE NOT EVIDENCE ABOUT THIS CONSTANT. The note that used to stand
//       here said "the car never leaves the arm because nothing clears meSympCrashState: the
//       console's demotion path (ReturnPhysicalVehicleToTraffic and friends) is not
//       reconstructed in this tree." BOTH halves are false. (a) ReturnPhysicalVehicleToTraffic
//       @0x8273DCD0, StopVehicleBeingPhysical @0x8271FED0 and CleanUpCrashedVehiclePhysics
//       @0x82720960 have all had bodies in BrnTrafficEntityModule_wT3_02.cpp since before that
//       line was written. (b) NOTHING clears meSympCrashState on the console either -- and it
//       does not need to: Vehicle::IsSympatheticallyCrashing @0x82704B18 reads miPhysicalReason
//       == 3, NOT meSympCrashState, so this arm's own SetPhysicalReason(E_PHYSICALREASON_NORMAL)
//       below IS the exit. The 629 consecutive frames were the wrong predicate latching.
//       ⚠️ KEEP THE ONE-XREF WARNING, it is still true and still load-bearing: a scan of all
//       27,549 exported functions finds exactly ONE reference to 0x8300CB90 and ZERO to
//       0x8300CB80, and both have writers. "One xref" NEVER means "nothing writes it" for a
//       .data splat -- the writers are dyn-init thunks and the export set does not contain them.
//   flt_820BA2A8 = 15.0f   approach distance that ends the ACCELERATE run (normal play)
//   flt_820BA590 = 40.0f   ... and in showtime, where the crash is meant to start further out
//   flt_820BA8F8 =  6.0f   ACCELERATE also ends on a timeout
//   flt_820047C8 =  0.05f  HEADON gives up steering once the front stuck-timer trips
//   flt_82004014 =  0.1f   HANDBRAKE holds for this long before committing the crash
//   flt_820BA884 =  1.1f   LOCKUP holds for this long before committing the crash
//   flt_82001C98 =  1.0f   flt_820037C8 = -1.0f   flt_82001CC0 = 0.0f
// =================================================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"   // MakeTrafficEntityId
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficParam.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameSource/Jobs/Traffic/BrnTrafficSwerveWatch.h"   // [DIAG] the chain-crash film arm

#include "rw/math/vpu/vector3_operation.h"   // Dot, Normalize, IsValid
#include "rw/math/vpu/vector4_operation.h"   // Splat

#include <cstdlib>   // getenv (BRN_TRAFFIC_DIAG)

namespace BrnTraffic
{
namespace
{
    // ---- the function's own RODATA, dumped from the image at the addresses the asm names ----
    // unk_8300CB90 <- dyn-init thunk 0x82C66A30: fmuls(flt_82F31928, flt_820BA86C) splatted.
    // 0.44704 is the MPH->m/s factor, so this is 2 MPH == 0.89408 m/s. Spelled as the product
    // the thunk computes, matching BrnVehicleManager.cpp:99 and BrnUpdateVehiclesJob.cpp:115.
    // See the banner for the decode, the calibration and the CRT-init-order check.
    const f32 KF_SYMP_GIVE_UP_SPEED        = 0.44704f * 2.0f;   // unk_8300CB90 == 0.89408f
    const f32 KF_SYMP_APPROACH_DIST        = 15.0f;   // flt_820BA2A8
    const f32 KF_SYMP_APPROACH_DIST_SHOWTIME = 40.0f; // flt_820BA590
    const f32 KF_SYMP_ACCELERATE_TIMEOUT   = 6.0f;    // flt_820BA8F8
    const f32 KF_SYMP_STUCK_FRONT_GIVE_UP  = 0.05f;   // flt_820047C8
    const f32 KF_SYMP_HANDBRAKE_HOLD       = 0.1f;    // flt_82004014
    const f32 KF_SYMP_LOCKUP_HOLD          = 1.1f;    // flt_820BA884
    const f32 KF_ONE                       = 1.0f;    // flt_82001C98
    const f32 KF_MINUS_ONE                 = -1.0f;   // flt_820037C8
    const f32 KF_ZERO                      = 0.0f;    // flt_82001CC0

    // ---- [DIAG] THE CHAIN-CRASH FILM ARM. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. ------
    // Publish where the sympathetic crasher IS, so (a) BRN_WORLD_CAMTRAFFIC=3/4 can stage a
    // static wide shot on it, (b) BRN_FRAME_DUMP_ARM can start filming at that instant, and
    // (c) every dumped frame carries the position in frames.csv, which is what lets a marker
    // be projected from the car's OWN logged coordinates instead of a crash being asserted
    // from a log line. Three float stores and two counters; no branch in the sim's control
    // flow, and it is gated on the same BRN_TRAFFIC_DIAG the [T6-symp] rungs already use.
    void PublishSympCrasher(u32 luVehicle, s32 liState, const Vector3& lvPos, bool lbCommitBound)
    {
        BrnTraffic::gSwerveWatch.mfSympPosX    = lvPos.x;
        BrnTraffic::gSwerveWatch.mfSympPosY    = lvPos.y;
        BrnTraffic::gSwerveWatch.mfSympPosZ    = lvPos.z;
        BrnTraffic::gSwerveWatch.miSympVehicle = static_cast<s32>(luVehicle);
        BrnTraffic::gSwerveWatch.miSympState   = liState;
        ++BrnTraffic::gSwerveWatch.muSympPublishes;
        if (lbCommitBound)
        {
            ++BrnTraffic::gSwerveWatch.muSympCommits;
        }
    }

    // The `% 101` reduction the console does with the 0x446F8657 magic-multiply reciprocal at
    // 0x8273D910..0x8273D92C, and the `< 50` split at 0x8273D930. Same pair
    // SafeRequestMakeVehiclePhysical's seed uses.
    const u32 KU_SYMP_PERCENT_MODULUS      = 101u;
    const s32 KI_SYMP_HANDBRAKE_PERCENT    = 50;

    // DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
    bool TrafficDiagEnabled()
    {
        static const bool sbEnabled = (getenv("BRN_TRAFFIC_DIAG") != 0);
        return sbEnabled;
    }
}

// =================================================================================================
// CrashVehicleForSympatheticCrashState @0x8272BA08 (95)  -- DWARF :1587
//
// The commit. Every arm of UpdateSympatheticCrashing that decides the car has finished lining
// itself up ends here, and this is the ONLY producer of a SetTrafficCrashingEvent on the
// offline path.
//
// The asm, in order:
//   0x8272BA20  assert luVehicle < KU_MAX_TOTAL_TRAFFIC            (.h:2459)
//   0x8272BA48  lpVehicle = GetVehicle(luVehicle)   ((idx+0x55) << 7) + this
//   0x8272BA54  assert lpVehicle->IsPhysical()                     (.cpp:17094; mxFlags bit 3)
//   0x8272BA84  assert lpVehicle->IsAlive()                        (BrnTrafficVehicle.h:1798)
//   0x8272BAC0  miPhysicalReason (+0x39) = E_PHYSICALREASON_CRASHED(0)  -- an INLINED
//               SetPhysicalReason: the console stores the byte directly here, it does not call
//               the out-of-line setter it used four instructions earlier in its caller.
//   0x8272BAC4  if (IsRecoveringFromSlam())
//                   RecordTrafficVehicleIsPhysical(luVehicle, MakeTrafficEntityId(luVehicle),
//                                                  K_INVALID_ENTITY_ID, Standard(0), 0.0f, 0.0f)
//               -- r6 = -1 (`li r31,-1`), r7 = 0, and BOTH float arguments take flt_82001CC0
//               (`fmr f1, f2` after `lfs f2, flt_82001CC0`), i.e. 0.0f twice.
//   0x8272BB0C  assert luVehicle < (1 << KU_NUM_BITS_FOR_ENTITY_NUM)  (CgsEntityId.h:116)
//   0x8272BB34  id = (luVehicle << 10) | (TRAFFIC_VEHICLE << 24)
//   0x8272BB40  lpOutput->GetVehicleInputInterface()->SetTrafficCrashing(id)
//   0x8272BB4C  assert lpVehicle->IsAlive()                        (BrnTrafficVehicle.h:1831 --
//               the message is SetCrashTrafficType's own, so the store below is that setter
//               inlined)
//   0x8272BB78  muCrashTrafficType (+0x01) = eCrashTrafficType_Standard(0)
//
// ⚠️ THE GLOBAL id IS BUILT FROM luVehicle, NOT FROM THE PHYSICS SLOT. That is deliberate and
// it is what makes the un-gated ProcessSetTrafficCrashingEvents drain work: the drain maps a
// GLOBAL traffic entity index through mu8GlobalToPhysicalEntityIndexMap to reach the 20-slot
// physical pool.
// =================================================================================================
void TrafficEntityModule::CrashVehicleForSympatheticCrashState(
        u32 luVehicle,
        BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput)
{
    CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");   // .h:2459

    Vehicle* const lpVehicle = GetVehicle(luVehicle);

    CGS_ASSERT(lpVehicle->IsPhysical(), "lpVehicle->IsPhysical()");   // .cpp:17094
    CGS_ASSERT(lpVehicle->IsAlive(),    "IsAlive()");                 // BrnTrafficVehicle.h:1798

    lpVehicle->SetPhysicalReason(static_cast<s8>(E_PHYSICALREASON_CRASHED));

    if (lpVehicle->IsRecoveringFromSlam())
    {
        EntityId lInvalid;
        lInvalid.muValue = 0xFFFFFFFFu;   // `li r31, -1`
        RecordTrafficVehicleIsPhysical(luVehicle,
                                       MakeTrafficEntityId(luVehicle),
                                       lInvalid,
                                       BrnPhysics::Vehicle::eCrashTrafficType_Standard,
                                       0.0f, 0.0f);
    }

    CGS_ASSERT(luVehicle < (1u << 14), "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");  // CgsEntityId.h:116

    EntityId lGlobalTrafficId;
    lGlobalTrafficId.muValue = (luVehicle << 10) | (BrnPhysics::Vehicle::KU_ENTITYTYPE_TRAFFIC_VEHICLE << 24);

    lpOutput->GetVehicleInputInterface()->SetTrafficCrashing(lGlobalTrafficId);

    CGS_ASSERT(lpVehicle->IsAlive(),
               "IsAlive() || leType == BrnPhysics::Vehicle::eCrashTrafficType_Invalid");   // .h:1831
    lpVehicle->SetCrashTrafficTypeRaw(
        static_cast<u8>(BrnPhysics::Vehicle::eCrashTrafficType_Standard));

    // DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. The chain's payoff line: if this never
    // prints, no traffic car ever chain-crashed and the [T6-tevt] drain below it has nothing to
    // drain -- which is exactly the state this file was written to end.
    if (TrafficDiagEnabled() && CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[T6-symp] CHAIN CRASH committed vehicle=" << static_cast<s32>(luVehicle)
            << " globalId=" << static_cast<s32>(lGlobalTrafficId.muValue)
            << " [DELETE-WHEN-STABLE]\n";
    }
}

// =================================================================================================
// UpdateSympatheticCrashing @0x8273D378 (594)  -- DWARF :1394,
// asserts BrnTrafficEntityModule.cpp :16288/:16289/:16290/:16298/:16310/:16324/:16475
//
// Prologue 0x8273D390..0x8273D3A8 -- the parameter map, off the ASM, because Hex-Rays recovers
// none of it:
//     r3 this | r4 luVehicle | r5 lEntityId (the crash TARGET) | r6 lpOutput
//     r7 lpOutControls | f1 lfTimeStep
//
// SHAPE:
//   1. aim -- the vehicle's target position is replaced by the crash target's, when the target
//      still resolves (GetSympCrashingTargetPos returns false and leaves the buffer alone
//      otherwise, which is why the console reads the CURRENT target pos into that buffer first).
//   2. accumulate mfSympCrashTime by the timestep.
//   3. the GIVE-UP arm (dead on retail, see the banner).
//   4. a pre-switch fixup: in showtime an ACCELERATE car is demoted to HEADON.
//   5. the four-state machine, which is the whole feature:
//        HEADON(1)    steer at the wreck, drive at it, commit when the front stuck-timer trips
//        ACCELERATE(2) same steering, but commit by DISTANCE or TIMEOUT, and commit by rolling
//                      into HANDBRAKE or LOCKUP rather than crashing directly
//        HANDBRAKE(3) full gas + handbrake + full COUNTER-steer, then crash after 0.1 s
//        LOCKUP(4)    full gas + handbrake, no steer, then crash after 1.1 s
//      default: the console's streamed "Invalid sympathetic crashing state." assert.
// =================================================================================================
void TrafficEntityModule::UpdateSympatheticCrashing(
        u32 luVehicle,
        EntityId lEntityId,
        BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput,
        BrnPhysics::Vehicle::BrnTrafficDriverControls* lpOutControls,
        f32 lfTimeStep)
{
    CGS_ASSERT(lpOutput       != 0, "lpOutputBuffer");                // :16288
    CGS_ASSERT(lpOutControls  != 0, "lpOutControls");                 // :16289
    CGS_ASSERT(lEntityId.muValue != 0xFFFFFFFFu, "lTarget.IsValid()");// :16290
    CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");   // .h:2459

    Vehicle* const lpVehicle = GetVehicle(luVehicle);

    // ---- 1. aim -------------------------------------------------------------------------------
    // 0x8273D44C..0x8273D47C. The console seeds the out-slot with the CURRENT target position and
    // lets GetSympCrashingTargetPos overwrite it only on a hit, so a target that has already been
    // recycled leaves the car driving at whatever it was driving at.
    Vector3 lTargetPos = lpVehicle->GetTargetPos();
    GetSympCrashingTargetPos(lEntityId, &lTargetPos);
    lpVehicle->SetTargetPos(lTargetPos);

    CGS_ASSERT(rw::math::vpu::IsValid(lpVehicle->GetTargetPos()),
               "RwMath::IsValid( lpVehicle->GetTargetPos() )");       // :16298

    // ---- 2. geometry + the timer --------------------------------------------------------------
    // 0x8273D518..0x8273D57C. v123 is the transform's Pos row (+0x30), v126 its At row (+0x20);
    // the console fetches the transform TWICE rather than reusing it, which is free here.
    const Matrix44Affine lTransform = GetVehicleTransform(luVehicle);
    const Vector3 lvVehiclePos = lTransform.Pos();
    const Vector3 lvVehicleAt  = lTransform.At();

    const Vector3 lvToTarget = lpVehicle->GetTargetPos() - lvVehiclePos;

    lpVehicle->SetSympCrashTime(lpVehicle->GetSympCrashTime() + lfTimeStep);

    // The forward distance to the wreck -- `vmsum3fp128 v124, v127, v126`. It is the ACCELERATE
    // arm's commit test AND CalculateDriverGasBrake's first argument.
    const f32 lfForwardDistance = rw::math::vpu::Dot(lvToTarget, lvVehicleAt);

    TrafficPhysicsInfo* const lpPhysicsInfo = GetTrafficPhysicsInfoForVehicl(luVehicle);
    CGS_ASSERT(lpPhysicsInfo != 0, "lpPhysicsInfo");                  // :16310

    // DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. Where the crasher is, THIS frame --
    // published before the give-up arm so a give-up frame is stamped too.
    if (TrafficDiagEnabled())
    {
        PublishSympCrasher(luVehicle, static_cast<s32>(lpVehicle->GetSympCrashState()),
                           lvVehiclePos, false);
    }

    // ---- 3. the GIVE-UP arm (0x8273D5AC..0x8273D634) ------------------------------------------
    // Threshold RECOVERED 2026-08-30 from its dyn-init thunk (banner): 2 MPH in m/s. The
    // SetPhysicalReason(NORMAL) below is this state machine's ONLY non-crashing exit -- it is
    // what makes IsSympatheticallyCrashing() (miPhysicalReason == 3) go false next frame, which
    // hands the car to UpdateNormalPhysical -> DriveTowardsTarget ->
    // ReturnPhysicalVehicleToTraffic. Do not "simplify" the three stores; each is load-bearing.
    if (KF_SYMP_GIVE_UP_SPEED > lpVehicle->GetSpeed().x
        && !lpPhysicsInfo->mbIsFatallyCrashing)
    {
        lpVehicle->SetPhysicalReason(static_cast<s8>(E_PHYSICALREASON_NORMAL));
        lpVehicle->SetCrashTrafficTypeRaw(
            static_cast<u8>(BrnPhysics::Vehicle::eCrashTrafficType_Invalid));
        lpOutput->GetVehicleInputInterface()->SetTrafficNotCrashing(MakeTrafficEntityId(luVehicle));

        // DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. Prints BOTH inputs of the test
        // above, because a bare "the give-up arm fired" line cannot tell a correct threshold
        // from a wrong one -- that is exactly what caught the retracted 0.0f. `reason` is
        // printed AFTER the store, so a reader can see the exit actually taken: 3 -> 5 means
        // IsSympatheticallyCrashing() goes false next frame and the car is handed to
        // UpdateNormalPhysical. A SECOND line for the same vehicle would mean the latch is back.
        if (TrafficDiagEnabled() && CgsDev::Log::gpDebugPrint != 0)
        {
            static s32 siGiveUpLogged = 0;
            if (siGiveUpLogged < 24)
            {
                ++siGiveUpLogged;
                *CgsDev::Log::gpDebugPrint
                    << "[T6-symp] GIVE-UP vehicle=" << static_cast<s32>(luVehicle)
                    << " speed=" << lpVehicle->GetSpeed().x
                    << " threshold=" << KF_SYMP_GIVE_UP_SPEED
                    << " state=" << static_cast<s32>(lpVehicle->GetSympCrashState())
                    << " reasonAfter=" << static_cast<s32>(lpVehicle->GetPhysicalReason())
                    << " sympTime=" << lpVehicle->GetSympCrashTime()
                    << " [DELETE-WHEN-STABLE]\n";
            }
        }
        return;
    }

    // ---- 4. the showtime demotion (0x8273D638..0x8273D6BC) ------------------------------------
    CGS_ASSERT(luVehicle < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");  // .h:2350
    Param* const lpParam = GetParam(luVehicle);
    CGS_ASSERT(lpParam != 0, "lpParam");                               // :16324

    // `lbz r11, 0x6C(r28) ; clrlwi r11, r11, 31` -- bit 0 of Param::muExtraBehaviourFlags,
    // NOT mxFlags/IsAlive (that byte is +0x40). An ACCELERATE car carrying the bit restarts as
    // a HEADON one, timer included.
    // ⚠️ NOTHING IN THIS TREE SETS THAT BIT YET: Param::Construct writes muExtraBehaviourFlags
    // = 0 and it is the member's only writer here, so this arm is currently unreachable. It is
    // reproduced because the branch is in the binary; do not "simplify" it away.
    if ((lpParam->muExtraBehaviourFlags & 1u) != 0u
        && lpVehicle->GetSympCrashState() == Vehicle::E_SYMPATHETIC_ACCELERATE)
    {
        lpVehicle->SetSympCrashTime(KF_ZERO);
        lpVehicle->SetSympCrashState(Vehicle::E_SYMPATHETIC_HEADON);
    }

    // The steering direction every driving arm uses: the console normalises lvToTarget with
    // vrsqrtefp plus TWO Newton-Raphson refinement steps, which is what rw::math::vpu::Normalize
    // is.
    const Vector3 lvSteerDirection = rw::math::vpu::Normalize(lvToTarget);

    // DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. One line the FIRST time each state is
    // entered, so "the arm never ran" and "the arm ran and did nothing" are distinguishable.
    if (TrafficDiagEnabled() && CgsDev::Log::gpDebugPrint != 0)
    {
        static bool sbaStateLogged[5] = { false, false, false, false, false };
        const s32 liState = static_cast<s32>(lpVehicle->GetSympCrashState());
        if (liState >= 0 && liState < 5 && !sbaStateLogged[liState])
        {
            sbaStateLogged[liState] = true;
            *CgsDev::Log::gpDebugPrint
                << "[T6-symp] state=" << liState
                << " (1 HEADON 2 ACCEL 3 HANDBRAKE 4 LOCKUP) vehicle="
                << static_cast<s32>(luVehicle)
                << " fwdDist=" << lfForwardDistance
                << " speed=" << lpVehicle->GetSpeed().x
                << " [DELETE-WHEN-STABLE]\n";
        }
    }

    switch (lpVehicle->GetSympCrashState())
    {
    // ---------------------------------------------------------------------------------------
    case Vehicle::E_SYMPATHETIC_HEADON:      // jump table entry 0 -> 0x8273DA28
    {
        CalculateAndSetSteering(luVehicle, lvSteerDirection, lpOutControls,
                                rw::math::vpu::Splat(KF_ZERO));

        // 0x8273DA8C. Nose-first into something for long enough == the crash has happened.
        if (lpPhysicsInfo->mfStuckTimeFront >= KF_SYMP_STUCK_FRONT_GIVE_UP)
        {
            lpOutControls->mfGas   = KF_ZERO;
            lpOutControls->mfBrake = KF_ZERO;
            if (lpVehicle->GetPhysicalReason() != E_PHYSICALREASON_CRASHED)
            {
                // DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. HEADON's commit edge. It
                // has no lead time at all (the car is already nose-first into the wreck), so a
                // camera latched here films the aftermath rather than the approach -- still an
                // impact on film, just a later one than the ACCELERATE tail gives.
                if (TrafficDiagEnabled())
                {
                    PublishSympCrasher(luVehicle,
                                       static_cast<s32>(Vehicle::E_SYMPATHETIC_HEADON),
                                       lvVehiclePos, true);
                }
                CrashVehicleForSympatheticCrashState(luVehicle, lpOutput);
            }
            return;
        }

        ApplyDriverGasBrake(luVehicle, lfForwardDistance, lvVehiclePos, lpOutControls);
        return;
    }

    // ---------------------------------------------------------------------------------------
    case Vehicle::E_SYMPATHETIC_ACCELERATE:  // jump table entry 1 -> 0x8273D6F4
    {
        CalculateAndSetSteering(luVehicle, lvSteerDirection, lpOutControls,
                                rw::math::vpu::Splat(KF_ZERO));

        ApplyDriverGasBrake(luVehicle, lfForwardDistance, lvVehiclePos, lpOutControls);

        // 0x8273D864..0x8273D8D4. Close enough, or out of patience -> stop steering at the wreck
        // and pick the way this car is going to lose it.
        const f32 lfApproachDistance = mbPlayingShowtimeMode ? KF_SYMP_APPROACH_DIST_SHOWTIME
                                                             : KF_SYMP_APPROACH_DIST;
        if (!(lfApproachDistance >= lfForwardDistance)
            && lpVehicle->GetSympCrashTime() < KF_SYMP_ACCELERATE_TIMEOUT)
        {
            return;
        }

        // 0x8273D8D8..0x8273D958 -- one mEffectRand LCG step, reduced mod 101, split at 50.
        const s32 liRoll = static_cast<s32>(mEffectRand.RandomUInt() % KU_SYMP_PERCENT_MODULUS);
        lpVehicle->SetSympCrashTime(KF_ZERO);
        lpVehicle->SetSympCrashState(liRoll < KI_SYMP_HANDBRAKE_PERCENT
                                         ? Vehicle::E_SYMPATHETIC_HANDBRAKE
                                         : Vehicle::E_SYMPATHETIC_LOCKUP);

        // DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. THE COMMIT-BOUND EDGE: from here
        // the car crashes in 0.1 s (HANDBRAKE) or 1.1 s (LOCKUP) with no further test that can
        // call it off. This is the last instant a camera can be staged for the impact, so it is
        // where BRN_WORLD_CAMTRAFFIC=3 latches.
        if (TrafficDiagEnabled())
        {
            PublishSympCrasher(luVehicle, static_cast<s32>(lpVehicle->GetSympCrashState()),
                               lvVehiclePos, true);
            if (CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[T6-symp] COMMIT-BOUND vehicle=" << static_cast<s32>(luVehicle)
                    << " -> state=" << static_cast<s32>(lpVehicle->GetSympCrashState())
                    << " (3 HANDBRAKE 4 LOCKUP) fwdDist=" << lfForwardDistance
                    << " pos=(" << lvVehiclePos.x << "," << lvVehiclePos.y << ","
                    << lvVehiclePos.z << ") [DELETE-WHEN-STABLE]\n";
            }
        }
        return;
    }

    // ---------------------------------------------------------------------------------------
    case Vehicle::E_SYMPATHETIC_HANDBRAKE:   // jump table entry 2 -> 0x8273D970
    {
        // 0x8273D990..0x8273D9E0. Full gas, full handbrake, and full lock AGAINST whichever way
        // the car is already steering -- the spin.
        const f32 lfSteering = lpVehicle->GetSteering().x;

        lpOutControls->mfGas       = KF_ONE;
        lpOutControls->mfBrake     = KF_ZERO;
        lpOutControls->mfHandBrake = KF_ONE;
        lpOutControls->mfSteering  = (lfSteering > KF_ZERO) ? KF_MINUS_ONE : KF_ONE;

        if (lpVehicle->GetSympCrashTime() < KF_SYMP_HANDBRAKE_HOLD)
        {
            return;
        }
        if (lpVehicle->GetPhysicalReason() == E_PHYSICALREASON_CRASHED)
        {
            return;
        }
        CrashVehicleForSympatheticCrashState(luVehicle, lpOutput);
        return;
    }

    // ---------------------------------------------------------------------------------------
    case Vehicle::E_SYMPATHETIC_LOCKUP:      // jump table entry 3 -> 0x8273DBF4
    {
        // 0x8273DBF8..0x8273DC0C. The same pedals with no steering input at all.
        lpOutControls->mfBrake     = KF_ZERO;
        lpOutControls->mfGas       = KF_ONE;
        lpOutControls->mfHandBrake = KF_ONE;

        if (lpVehicle->GetSympCrashTime() < KF_SYMP_LOCKUP_HOLD)
        {
            return;
        }
        if (lpVehicle->GetPhysicalReason() == E_PHYSICALREASON_CRASHED)
        {
            return;
        }
        CrashVehicleForSympatheticCrashState(luVehicle, lpOutput);
        return;
    }

    default:
        // 0x8273DC54: the console streams the state into gpcMessageBuffer. Lowered to the plain
        // CGS_ASSERT per the standing rule; fire-and-continue, exactly as the console does.
        CGS_ASSERT(false, "Invalid sympathetic crashing state.");      // :16475
        return;
    }
}

// =================================================================================================
// The gas/brake tail both driving arms share, verbatim in both
// (0x8273D754..0x8273D860 and 0x8273DAD8..0x8273DBDC -- instruction-identical apart from the
// stack slots). Outlined here because that is what the original source must have looked like:
// two copies of 45 identical instructions inside one function is a compiler inlining a helper,
// not a programmer writing it twice.
//
//   * outside showtime, and only when the car is on the far side of the camera plane
//     (dot(cameraAt, vehiclePos - cameraPos) >= 0, i.e. IN FRONT of the camera), the param's own
//     velocity (direction * speed) is handed to CalculateDriverGasBrake; otherwise a zero vector
//     is. `mCameraLastFrame` is the +0x728C0 / +0x728B0 pair.
//   * the returned signed pedal is split into gas (positive half) and brake (negated positive
//     half), each clamped to [0,1] -- `vmaxfp(x,0)` then `vminfp(x,1)`, with the brake half
//     produced by `vxor` against the sign bit `vslw(-1,-1)`.
// =================================================================================================
void TrafficEntityModule::ApplyDriverGasBrake(
        u32 luVehicle,
        f32 lfForwardDistance,
        Vector3 lvVehiclePos,
        BrnPhysics::Vehicle::BrnTrafficDriverControls* lpOutControls)
{
    Vector3 lvParamVelocity = { KF_ZERO, KF_ZERO, KF_ZERO, KF_ZERO };

    if (!mbPlayingShowtimeMode)
    {
        const Vector3 lvCameraToVehicle = lvVehiclePos - mCameraLastFrame.GetPosition();
        if (!(KF_ZERO > rw::math::vpu::Dot(mCameraLastFrame.GetDirection(), lvCameraToVehicle)))
        {
            const ParamTransform* const lpParamTransform = GetParamTransform(luVehicle);
            const VecFloat lvfSpeed    = lpParamTransform->GetSpeed();
            const Vector3  lvDirection = lpParamTransform->GetDirection();
            lvParamVelocity = lvDirection * lvfSpeed;
        }
    }

    const VecFloat lvfPedal =
        CalculateDriverGasBrake(luVehicle, rw::math::vpu::Splat(lfForwardDistance), lvParamVelocity);

    const f32 lfPedal = lvfPedal.x;

    f32 lfGas = lfPedal;
    if (lfGas < KF_ZERO) { lfGas = KF_ZERO; }
    if (lfGas > KF_ONE)  { lfGas = KF_ONE;  }

    f32 lfBrake = -lfPedal;
    if (lfBrake < KF_ZERO) { lfBrake = KF_ZERO; }
    if (lfBrake > KF_ONE)  { lfBrake = KF_ONE;  }

    lpOutControls->mfGas   = lfGas;
    lpOutControls->mfBrake = lfBrake;
}

}   // namespace BrnTraffic
