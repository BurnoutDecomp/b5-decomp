#define CGS_ARTIST_ABS_BRNAIDRIVER_H_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\world\\ai\\BrnAIDriver.h"
#define CGS_ARTIST_UNITY_BRNAIDRIVER_CPP_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../World/AI/BrnAIDriver.cpp"
#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameShared/GameClasses/Core/CgsCoreConstants.h"   // CgsCore::KF_MPH_TO_MPS (the mph tunables)
#include "rw/math/fpu/trig.h"                             // rw::math::fpu::DegToRad (the angle tunables)
#include "rw/math/fpu/scalar_operation.h"                 // rw::math::fpu::Cos
#include "GameSource/World/AI/BrnAICar.h"                  // AICar (named members + accessors)
#include "GameSource/World/AI/BrnAIUtils.h"                // StepTo / Find{Signed,Unsigned}AngleBetween2DVectors
#include "GameSource/World/AI/BrnAIAggression.h"           // AIAggression (the embedded sub-machine)
#include "GameSource/World/AI/Route/BrnRacingLine.h"       // RacingLine (the embedded sub-object)
#include "GameSource/World/AI/Route/BrnRoute.h"            // Route / RouteNode (the car's route, AICar+0)
#include "GameSource/World/AI/PID/BrnPIDController.h"      // PIDController (the two embedded controllers)

#include "GameShared/GameClasses/Core/CgsAssert.h"         // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <vectorintrinsics.h>
#include <cmath>   // std::sqrt (de-SIMD'd 2D normalise), std::isfinite (RwMath::IsValid)

// BrnAI::AIDriver -- the STEERING + CAR-CONTROL link. This TU (BrnAIDriver.cpp + the partfile
// BrnAIDriver_Update.cpp) bodies the members that turn a navigation target (heading + desired
// speed) into the car's physics inputs (steer / throttle / brake / hand-brake + boost).
// Reconstructed store-for-store from the X360 asm; the inlined SIMD 2D-vector normalise
// (vrsqrtefp + two Newton-Raphson refine steps) and the inlined vector lerps are reversed into
// the logical scalar/Vector2 maths they implement. Every call into a collaborator is a NAMED CALL.
//
// COLLABORATOR HOMES (all committed and reached BY NAME -- this file no longer performs any
// `this + <guest offset>` reinterpret_cast):
//   * AICar (BrnAICar.h) carries every field these bodies read as a named member.
//   * The embedded sub-objects are named members of AIDriver: mSteeringFan (@0x710),
//     mPIDController / mPIDControllerDrift (@0x1B34 / @0x1B98) sit at their guest offsets;
//     RacingLine and AIAggression are embedded at their native offsets and reached
//     through GetRacingLine() / GetAggression().
//   * The car's Route is the AICar's own base (AICar::GetRoute()); node x/y come from
//     Route::GetNode(i)->GetX()/GetY().
//   * StepTo and Find{Signed,Unsigned}AngleBetween2DVectors are declared in BrnAIUtils.h and
//     bodied in BrnAIUtils.cpp / BrnAIUtils_Angles.cpp.
//
// TUNABLES: the DWARF file-scope set of BrnAIDriver.cpp (:39..:161), defined below in its order.

namespace BrnAI
{
    // ====================================================================================
    // File-scope tunables, DWARF BrnAIDriver.cpp:39..:161 (names, order, const/volatile). ARTIST
    // compiled this file second in BrnWorldAIUnity (after BrnAICar.cpp), and keeps three kinds:
    //   * statically initialised consts in the TU's .rdata, in definition order from 0x820C41BC
    //     (:44 0.75, :51 4, :52 8, :56 0.85, :57 0.95, :58 0.75, :63 0.75, :68 25, :69 50, :76 8,
    //     :77 3, :82 1.5, :85 16, :86 75, :90 2, :91 5, :92 0.75, :96 40000, :103 0, :110 10,
    //     :116 1.1, :117 10, :119 0.9, .., :136 3 (0x820C421C), :139 1 (0x820C4220), :140 0.1,
    //     :141 0.02, :142 0.03, :152 5 (0x820C4230), :154 3). Unreferenced ones are not emitted,
    //     so the DWARF names with no slot (:45, :61, :65, :75, :113, :123/:133, :159..:161) have
    //     no recoverable value and are left out. Reads of a non-volatile one go to the TU's pooled
    //     copy of its value; the two volatiles are read from their own slots (0x820C41D4,
    //     0x820C420C, 0x820C4214).
    //   * the three braking angles, `DegToRad(n)` folded by the compiler into .data
    //     (0x82F3038C/90/94 = 30, 45, 90 degrees in radians);
    //   * dynamically initialised ones, written by the CRT init bank in declaration order
    //     (0x82C687C0..0x82C68B08) into .bss: `mph * CgsCore::KF_MPH_TO_MPS`, the range
    //     KF_MAX_BRAKING_ANGLE - KF_MIN_BRAKING_ANGLE (0x82C68820), the volatile DegToRad(4)
    //     (0x82C68840 stores the folded 0.0698132 at 0x8300D7AC) and three Cos(DegToRad(n))
    //     (XMVectorCos calls 0x82C68918/0x82C68968/0x82C68A18). Their readers load them from
    //     storage (e.g. UpdateStuck 0x827664C0 lfs 0x8300D818).
    // ====================================================================================
    const f32       KF_MIN_BRAKING_ANGLE         = rw::math::fpu::DegToRad(30.0f);   // :39 .data 0x82F3038C
    const f32       KF_SIGNIFICANT_BRAKING_ANGLE = rw::math::fpu::DegToRad(45.0f);   // :40 .data 0x82F30390
    const f32       KF_MAX_BRAKING_ANGLE         = rw::math::fpu::DegToRad(90.0f);   // :41 .data 0x82F30394

    const f32       KF_AI_MAX_BRAKING_SPEED_PROPORTION = 0.75f;                                  // :44
    const f32       KF_AI_MIN_BRAKING_SPEED            = 60.0f * CgsCore::KF_MPH_TO_MPS;         // :46 bss 0x8300D748

    const f32       KF_BOOST_TO_CLOSE_SPEED_GAP = 5.0f * CgsCore::KF_MPH_TO_MPS;                 // :48 bss 0x8300DB20

    const f32       K_PROXIMITY_CLOSE           = 4.0f;                                          // :51
    const f32       K_PROXIMITY_FAR             = 8.0f;                                          // :52
    const f32       K_PROXIMITY_SPEED_REDUCTION = 10.0f * CgsCore::KF_MPH_TO_MPS;                // :53 bss 0x8300DC34

    const f32       K_MIN_HARD_SHOULDER            = 0.85f;                                      // :56
    const f32       K_MAX_HARD_SHOULDER            = 0.95f;                                      // :57
    const f32       KF_AI_HARD_SHOULDER_PROPORTION = 0.75f;                                      // :58

    const f32       KF_BRAKING_ANGLE_RANGE = KF_MAX_BRAKING_ANGLE - KF_MIN_BRAKING_ANGLE;         // :60 bss 0x8300DC38

    volatile const f32       KF_BRAKE_DURING_DRIFT = 0.75f;                                      // :63 rdata 0x820C41D4

    const f32       K_ROAD_RAGE_SPREAD_HNG = 25.0f;                                              // :68
    const f32       K_NORMAL_SPREAD_HNG    = 50.0f;                                              // :69

    volatile const f32       KF_MAX_DRIFT_UNNECESSARY_ANGLE = rw::math::fpu::DegToRad(4.0f);     // :72 bss 0x8300D7AC

    const f32       KF_BE_KIND_AFTER_CRASH_DELAY     = 8.0f;                                     // :76
    const f32       KF_BE_KIND_AFTER_AI_DRIVEN_DELAY = 3.0f;                                     // :77

    const f32       KF_BRAKING_ANTICIPATION_TIME   = 1.5f;                                       // :82
    const f32       KF_BRAKING_MIN_LOOK_AHEAD_DIST = 16.0f;                                      // :85
    const f32       KF_BRAKING_MAX_LOOK_AHEAD_DIST = 75.0f;                                      // :86

    const f32       KF_MAX_SPEED_FOR_BEING_STUCK   = 10.0f * CgsCore::KF_MPH_TO_MPS;             // :89 bss 0x8300D818
    const f32       KF_AI_TIME_TO_START_TURNING    = 2.0f;                                       // :90
    const f32       KF_AI_TIME_FOR_BEING_STUCK     = 5.0f;                                       // :91
    const f32       KF_PLAYER_TIME_FOR_BEING_STUCK = 0.75f;                                      // :92

    const f32       KF_MAX_DIST_SQUARED_FOR_AI_AVOIDANCE = 40000.0f;                             // :96

    const f32       KF_BRAKING_START_DIFF = 20.0f * CgsCore::KF_MPH_TO_MPS;                      // :99 bss 0x8300D96C
    const f32       KF_BRAKING_MAX_DIFF   = 40.0f * CgsCore::KF_MPH_TO_MPS;                      // :100 bss 0x8300DB0C

    const f32       KF_ACCELERATION_START_DIFF = 0.0f;                                           // :103 rdata 0x820C4204
    const f32       KF_ACCELERATION_MAX_DIFF   = 20.0f * CgsCore::KF_MPH_TO_MPS;                 // :104 bss 0x8300D9A8

    const f32       KF_BOOST_UP_TO_DESIRED_SPEED_OFFSET = 50.0f * CgsCore::KF_MPH_TO_MPS;        // :107 bss 0x8300DB24

    const f32       KF_RACE_STUCK_FREE_TIME = 10.0f;                                             // :110

    volatile const f32       KF_TIME_LOOK_AHEAD_FOR_DRIFT = 1.1f;                                // :116 rdata 0x820C420C
    const f32       KF_MIN_DISTANT_LOOK_AHEAD_FOR_DRIFT   = 10.0f;                               // :117

    volatile const f32       K_DRIFT_TRIGGER = 0.9f;                                             // :119 rdata 0x820C4214
    const f32       K_MIN_DRIFT_SPEED        = 30.0f * CgsCore::KF_MPH_TO_MPS;                   // :120 bss 0x8300DBE4

    const f32       KF_TRIGGER_TURN = rw::math::fpu::Cos(rw::math::fpu::DegToRad(60.0f));       // :127 bss 0x8300D74C

    const f32       KF_QUICKTURN_DROP_OUT         = rw::math::fpu::Cos(rw::math::fpu::DegToRad(30.0f));   // :129 bss 0x8300DC00
    const f32       KF_QUICKTURN_SLOWNESS_DROPOUT = 10.0f * CgsCore::KF_MPH_TO_MPS;              // :130 bss 0x8300D6FC
    const f32       KF_QUICKTURN_SLOWNESS_DROPIN  = 30.0f * CgsCore::KF_MPH_TO_MPS;              // :131 bss 0x8300D960

    const f32       KF_SLOW_TURN_SPEED                 = 20.0f * CgsCore::KF_MPH_TO_MPS;         // :134 bss 0x8300D718
    const f32       KF_SLOW_TURN_DROP_OUT              = rw::math::fpu::Cos(rw::math::fpu::DegToRad(20.0f));  // :135 bss 0x8300D710
    const f32       KF_SLOW_TURN_DIRECTION_CHANGE_TIME = 3.0f;                                   // :136 rdata 0x820C421C

    const f32       KF_AI_STEERING_STEP                   = 1.0f;                                // :139
    const f32       KF_PLAYER_STEERING_STEP               = 0.1f;                                // :140
    const f32       KF_PLAYER_ROLLING_START_STEERING_STEP = 0.02f;                               // :141
    const f32       KF_PLAYER_DRIVE_THRU_STEERING_STEP    = 0.03f;                               // :142

    const f32       KF_STOP_BRAKE_SPEED = 5.0f * CgsCore::KF_MPH_TO_MPS;                         // :144 bss 0x8300D984

    const f32       KF_NO_BOOST_SPEED     = 40.0f * CgsCore::KF_MPH_TO_MPS;                      // :147 bss 0x8300D81C
    const f32       KF_ALWAYS_BOOST_SPEED = 130.0f * CgsCore::KF_MPH_TO_MPS;                     // :148 bss 0x8300D750
    const f32       KF_FORCED_BOOST_SPEED = 150.0f * CgsCore::KF_MPH_TO_MPS;                     // :149 bss 0x8300D7A4

    const f32       KF_PRE_AGGRESSION_DELAY = 5.0f;                                              // :152

    const f32       KF_MINIMUM_BOOST_TIME = 3.0f;                                                // :154

    const f32       KF_BEHIND_SHUNT_MARKED_MAN_MIN_SPEED  = 90.0f * CgsCore::KF_MPH_TO_MPS;      // :156 bss 0x8300D774
    const f32       KF_BEHIND_SHUNT_MIN_SPEED_DIFFERENCE  = 10.0f * CgsCore::KF_MPH_TO_MPS;      // :157 bss 0x8300D934

    // :171, rdata 0x820C4250 (the 6.0 UpdatePlayerTimers caps mfPlayerSlowSpeedTime with, read there
    // directly at 0x827703A8; the PID gains :2798.. follow it at 0x820C4254).
    const f32       KF_PLAYER_SLOW_SPEED_AGGRESSIVE_TIME = 6.0f;

    static inline f32 NativeVmxMadd(f32 a, f32 b, f32 c)
    {
        const __vector4 va = { a, a, a, a };
        const __vector4 vb = { b, b, b, b };
        const __vector4 vc = { c, c, c, c };
        return __vmaddfp(va, vb, vc).x;
    }
    static inline f32 NativeVmxSub(f32 a, f32 b)
    {
        const __vector4 va = { a, a, a, a };
        const __vector4 vb = { b, b, b, b };
        return __vsubfp(va, vb).x;
    }

    // ====================================================================================
    // TU-LOCAL helpers.
    // ====================================================================================

    // RwMath::IsValid( x ) -- the X360 check is a NaN SELF-COMPARE and nothing else.
    // ⛔ CORRECTED (issue #31): the second half of the old note ("the engine treats inf as invalid
    // too") was not true of the binary, and `std::isfinite` acted on it. Every recovered site is a
    // `vcmpeqfp. vX,vX,vX` (e.g. AIModule::StoreDrivenCarData's "Invalid car matrix"/"Invalid car
    // velocity"/"Invalid car speed" cascade @0x82795A04 / 0x82795BD4 / 0x82795C70, and
    // FindSignedAngleBetween2DVectors @0x82771730 / 0x827717C0), and `Inf == Inf` is TRUE -- so
    // ARTIST does NOT assert on an infinite value. `rw::math::vpu::IsValid` in the SDK headers is
    // already the self-compare; this local helper was the outlier.
    static inline bool IsFinite(f32 lfValue) { return lfValue == lfValue; }

    // de-inlined planar (x,y) normalise -- v / |v| over the (x,y) lanes (the rsqrt + 2x
    // Newton-Raphson refine idiom the X360 emits inline). NO ZERO GUARD at any console caller --
    // no vcmpeqfp/vsel follows the chain at CalculateSteeringAngle 0x8277CE8C / 0x8277D004,
    // ComputeRouteDirection 0x8276662C, GetTargetPosition 0x8277CCA0 or CorneringTopSpeed
    // 0x8277D17C / 0x8277D1E8 -- so a zero vector is NaN in every lane there: vrsqrtefp(+0) is
    // +inf and the first Newton step's `1 - lenSq * y0^2` (vnmsubfp) is 0 * inf = NaN. Here
    // 1 / sqrt(0) = +inf and 0 * inf = NaN give the same NaN lanes.
    // ⛔ CORRECTED 2026-09-24 (crash parity FX-TAILS-A item 6, FX-AINAN2 OPEN): this was
    // `lenSq > 0 ? 1 / sqrt : 0`, an invented guard that answered (0, 0).
    static Vector2 Normalize2D(Vector2 lVector)
    {
        // ARTIST 0x8277CE5C..0x8277CEB4: dot XY, two rsqrt refinements,
        // then multiply the complete input register (including Z and W).
        const __vector4 product = __vmulfp(lVector.mV, lVector.mV);
        const __vector4 dot = __vaddfp(__vspltw(product, 0), __vspltw(product, 1));
        const __vector4 one = __vcfsx(__vspltisw(1), 0);
        const __vector4 half = __vcfsx(__vspltisw(1), 1);
        __vector4 estimate = __vrsqrtefp(dot);
        __vector4 correction = __vnmsubfp(dot, __vmulfp(estimate, estimate), one);
        estimate = __vmaddfp(__vmulfp(estimate, half), correction, estimate);
        correction = __vnmsubfp(dot, __vmulfp(estimate, estimate), one);
        estimate = __vmaddfp(__vmulfp(estimate, half), correction, estimate);
        Vector2 result;
        result.mV = __vmulfp(lVector.mV, estimate);
        return result;
    }

    // 2D-flatten a Vector3 onto the XZ GROUND plane. This is BrnMath::Flatten @0x822CB8E8, which
    // the X360 calls out of line in several of these bodies and open-codes as a vrlimi pair in
    // others.
    //
    // ⛔ FIXED 2026-09-04 (aiwave R7): this used to write `lResult.y = lVector.y` -- the WORLD
    // HEIGHT lane -- so every AI 2D position/heading lived on the X/height plane instead of the
    // ground plane. Two independent X360 witnesses settle the lane mapping:
    //   * BrnMath::Flatten @0x822CB8E8 ends `lvx128 v0,&unk_82CDA450 ; vperm v1,v1,v1,v0`, and the
    //     mask bytes at 0x82CDA450 are 00 01 02 03 | 18 19 1A 1B | 00 01 02 03 | 00 01 02 03 --
    //     lane1 is sourced from bytes 0x18..0x1B, i.e. vB lane 2 == .z. (This retires the
    //     "the mask is an un-valued .rdata blob" FLAG in GameSource/Math/BrnMathUtils.cpp; that
    //     TU's XZ inference was correct.)
    //   * CalculateSteeringAngle @0x8277CD70..0x8277CD8C open-codes the same flatten inline:
    //     `vspltisw128 v127,0 ; vrlimi128 v127,v0,8,0 ; vrlimi128 v127,v0,4,1` == lane0 <- v0
    //     lane0 (.x), lane1 <- (v0 rotl 1 word) lane1 == v0 lane2 (.z), lanes 2/3 left ZERO.
    // Route nodes agree: RouteMapModule::AddExtrapolatedPortalNode stores {portal.x, portal.z}
    // and AICar::IsExtrapolatedRouteGettingOld reads a node back as {x, 0, y}.
    static Vector2 To2D(Vector3 lVector)
    {
        return Vector2(lVector.X().GetFloat(), lVector.Z().GetFloat());
    }

    // rw::math::fpu::Clamp<float>(x, lo, hi) exactly as the X360 inlines it -- the DecFIGS DWARF
    // lists fpu::Clamp<float> in CalculateSteeringAngle, AttemptToDriveAtDesiredSpeed,
    // ProximitySpeed and CorneringTopSpeed, and each is the same two-fsel ladder:
    //   fsubs t,lo,x ; fsel t,t,lo,x    t = (lo - x >= 0) ? lo : x   (lo == 0: `fneg ; fsel`)
    //   fsubs u,hi,t ; fsel r,u,t,hi    r = (hi - t >= 0) ? t  : hi
    // e.g. CalculateSteeringAngle 0x8277D0B8..0x8277D0C4 (lo -1.0 flt_820037C8, hi f31 == 1.0
    // flt_82001C98), AttemptToDriveAtDesiredSpeed 0x82770780..0x82770790 / 0x827707CC..0x827707DC.
    // fsel takes its THIRD operand when the test is unordered, so a NaN x passes the floor and
    // comes back as hi -- not as NaN, which is what the old `if (x < lo) ... if (x > hi) ...`
    // spelling returned (crash parity FX-AINAN2). Spelt here, in the TU, rather than through the
    // shared rw::math::fpu::Clamp, so the AI call sites keep the console form whatever that
    // shared header's spelling is.
    static inline f32 ClampFsel(f32 lfValue, f32 lfLow, f32 lfHigh)
    {
        const f32 lfFloored = ((lfLow - lfValue) >= 0.0f) ? lfLow : lfValue;
        return ((lfHigh - lfFloored) >= 0.0f) ? lfFloored : lfHigh;
    }

    // clamp(x, 0, 1) -- the fpu::Clamp<float>(x, 0.0f, 1.0f) ladder above (NaN -> 1.0).
    static inline f32 Saturate(f32 lfValue)
    {
        return ClampFsel(lfValue, 0.0f, 1.0f);
    }

    // ====================================================================================
    // Construct @0x82792B70
    //
    // Zero/seed the controller block at allocation: clears the stuck/distance/desired/top-speed
    // scalars, sets the invuln + finish-line-state timers to -1.0, NULLs mpCar, clears every
    // control output + flag, zeroes SIX 2D steering vectors (mFinalFacing@0x1CC0 is NOT cleared)
    // and constructs the embedded
    // AIAggression sub-machine (the X360 also registers two PerfMon timers once; that diagnostic
    // is presentation-only and intentionally omitted from this slice).
    // ====================================================================================
    void AIDriver::Construct()
    {
        mfStuckTime           = 0.0f;       // 0x1D04
        mfDistanceToPlayer    = 0.0f;       // 0x1D08
        mfDesiredSpeed        = 0.0f;       // 0x1D10
        mfCarSpeed            = 0.0f;       // 0x1D00
        mfAngleToBrakingTarget = 0.0f;      // 0x1CFC
        mfInvulnerableTime    = -1.0f;      // 0x1D0C
        mfStartLineWheelSpinTime = -1.0f;   // 0x1D20 (stfs flt_820037C8)
        mpCar             = 0;    // 0x1CE0
        mbIsRacingLineInitialised = 0;      // 0x1D68
        mbIsActive            = 0;          // 0x1D69
        mbCurrentRouteComplete = 0;         // 0x1D6A
        mfPlayerSlowSpeedTime = 0.0f;       // 0x1D58
        mfAccelerator         = 0.0f;       // 0x1D28
        miCurrentRacingLineNodeIndex = 0;   // 0x1D60
        mfBrake               = 0.0f;       // 0x1D2C
        mbUseForcedSpeed      = 0;          // 0x1D66
        mfForcedSpeed         = 0.0f;       // 0x1D18
        mbBoosting            = 0;          // 0x1D6B
        mfHandBrake           = 0.0f;       // 0x1D30
        mbWantToExitDrift     = 0;          // 0x1D64
        mfSteeringAngle       = 0.0f;       // 0x1D24
        mbWantToEnterDrift    = 0;          // 0x1D65

        // X360 zeroes SIX 2D vectors via stvx (r10=0x1C70, r9=0x1C80, r8=0x1C90, r7=0x1CA0,
        // r6=0x1CB0, r5=0x1CD0). mFinalFacing @0x1CC0 is deliberately NOT cleared here.
        mSteeringTargetVector.SetZero();    // 0x1C70
        mTargetRacingLinePos.SetZero();     // 0x1C80
        mTargetRoadDir.SetZero();           // 0x1C90
        mBrakingAnticipationPos.SetZero();  // 0x1CA0
        mBrakingRoadDir.SetZero();          // 0x1CB0
        m2DCarPos.SetZero();                // 0x1CD0

        GetAggression()->Construct(this);   // mAggression.Construct(this) (the host-side member)
    }

    // ====================================================================================
    // SetAICar @0x827963C8
    //
    // Bind this driver to a car: assert the car is non-NULL and not already bound, store mpCar,
    // clear the car's drive-state slot, wire the car back to this driver, initialise the racing
    // line, then seed the per-car timers/flags and reset the attrib-sys values.
    // ====================================================================================
    void AIDriver::SetAICar(AICar* lpCar)
    {
        CGS_ASSERT_AT(lpCar != 0, "Driver given NULL car", CGS_ARTIST_UNITY_BRNAIDRIVER_CPP_FILE, 838);
        CGS_ASSERT_AT(mpCar == 0,
                   "Shouldn't be reassigning an AI driver directly to another car.", CGS_ARTIST_UNITY_BRNAIDRIVER_CPP_FILE, 841);

        // DWARF calls AICar::SetState and ResetStuckTime (both header-inline). The car is reached
        // through mpCar after the store: ARTIST re-reads 0x1CE0 for SetDriver (0x82796440) and for the
        // aggression Prepare (0x82796458) because the state store through the car may alias it.
        mpCar = lpCar;
        mpCar->SetState(E_AI_CAR_STATE_IN_RANGE);           // *(car+0x14C8) = 0
        mpCar->SetDriver(this);                             // X360 @0x82796444
        InitialiseRacingLine();

        ResetStuckTime();                   // 0x1D04
        mbIsActive         = 1;             // 0x1D69 (driver becomes active on bind)
        mfDistanceToPlayer = 0.0f;          // 0x1D08
        mfInvulnerableTime = -1.0f;         // 0x1D0C

        // BLOCKED (out of this TU's file scope): X360 @0x82796480-0x827964B0 also seeds the
        // embedded AIAggression block here directly (mpCar, mfStateTime=-1, meAggressionState=0,
        // mpTargetCar=NULL, mpPlayerCar=NULL, mbTargetPosValid=false, meSpeedMatchType=Disabled,
        // mfRelativePositionAhead/mfRecentHitTimer/mfContinuousContactTimer/mfHangingAroundTimer=0,
        // mfNonSpeedMatchedSpeed=flt_8300DC64). Every one of those is a PRIVATE AIAggression data
        // member (BrnAIAggression.h has no public setter for this "seed for new car" bundle), so
        // this TU cannot write them without a new method declared on AIAggression -- out of scope.

        // 0x82796480..0x827964B0: the twelve aggression stores (mpCar = car, state OUT_OF_RANGE, timers) --
        // homed by lane A8 as AIAggression::Prepare(AICar*); the call was missing here (run4 AV:
        // AIAggression::Update @0x82799ABC `lwz r11,8(agg) ; lbz 0x1549(r11)` on a null mpCar).
        GetAggression()->Prepare(mpCar);

        ResetAttribSysValues();
    }

    // ====================================================================================
    // Prepare @0x82792CA8
    //
    // Per-race-entry init: store the sections-data pointer + active-car index, reset the
    // attrib-sys values, clear the per-race timers/flags/nearby-vehicle count/drift state, clear
    // the racing-line section cache, draw this driver's centre-line-ahead threshold from the
    // supplied Random (AIModule::mRandom, &AIModule+294784 from AIModule::Prepare @0x82798070)
    // into the embedded RacingLine, and prepare the embedded SteeringFan.
    // ====================================================================================
    void AIDriver::Prepare(AISectionsData* lpSectionsData, s32 leRelatedActiveCarIndex,
                           CgsNumeric::Random* lpRandom)
    {
        mpSectionsData = lpSectionsData;            // 0x1CE4
        ResetAttribSysValues();
        meRelatedActiveCarIndex = leRelatedActiveCarIndex; // 0x1CF4
        mfPerpendicularTarget   = 0.0f;             // 0x1D48
        mNearbyVehicles.Reset();                    // stw 0, 0x700(this)
        mfHandBrake             = 0.0f;             // 0x1D30
        mbWantToEnterDrift      = 0;                // 0x1D65
        mActiveRouteTimeStamp   = 0;                // 0x1D5C
        SetDriftState(E_DRIFT_STATE_NORMAL_DRIVING);   // stw 0, 0x1CEC(this)

        GetRacingLine().ClearSectionCache();           // mRacingLine.ClearSectionCache() (this+0xF20)

        // stb 0, 0x1AF0 / 0x1AF1 == mRacingLine.mbIsInitialised / mbCentreLineHereKnown
        // (RacingLine +0xBD0 / +0xBD1).
        GetRacingLine().mbIsInitialised       = false;
        GetRacingLine().mbCentreLineHereKnown = false;

        // 0x82792D10..0x82792DB8: CgsNumeric::Random::RandomFloat(CLOSE, FAR) inlined on lpRandom
        // (r30 = r6): lwz oldest +0x28 ; ld seed +0x20 ; lfsx f13 = ring[oldest] ; stwx
        // ring[oldest] = 0x3F800000 | (oldSeed.hi >> 9) (inslwi 23,9) ; std seed*0x5851F42D4C957F2D
        // + 1 ; stw (oldest+1)&7 ; fsubs t = f13 - 1.0 (flt_82001C98) ; then vsubfp FAR - CLOSE and
        // vmaddfp (FAR - CLOSE)*t + CLOSE (raw fields vD=0 vA=12 vB=0 vC=13). CLOSE = flt_820C3DDC
        // (0x3F7F5C29, 0.9975), FAR = 0x820C3DE0 (0x3F7C28F6, 0.985); the PS3 twin @0x9C30D4 names
        // them KF_CENTRE_LINE_AHEAD_CLOSE / _FAR. The embedded RacingLine is r28 = this+0xF20:
        //   0x82792DB0 stfs f13, 0xC04(r28)                    mfCentreLineAhead
        //   0x82792DAC fsubs 1.0 - v ; 0x82792DB4 fdivs ; 0x82792DB8 stfs 0xC08(r28)  mfCentreLineAheadRecip
        // The only other writer is AICar::OnModeStart's player-only pair (0x8277BEBC/0x8277BEC4),
        // and the only reader is SteeringFan::IncludeCentreLineTracking (0x82786D00/0x82786D04).
        const f32 lfCentreLineAhead = lpRandom->RandomFloat(KF_CENTRE_LINE_AHEAD_CLOSE, KF_CENTRE_LINE_AHEAD_FAR);
        GetRacingLine().mfCentreLineAhead      = lfCentreLineAhead;
        GetRacingLine().mfCentreLineAheadRecip = 1.0f / (1.0f - lfCentreLineAhead);

        mSteeringFan.Prepare();                      // @0x82778E40

        mfBoostTimeRemaining      = 0.0f;            // 0x1D44
        mfPlayerTimeSinceCrash    = 0.0f;            // 0x1D34
        mbBoosting                = 0;               // 0x1D6B
        mfPlayerTimeSinceAIDriven = 0.0f;            // 0x1D38
        mfPlayerSlowSpeedTime     = 0.0f;            // 0x1D58
        SetAggressionVictimCar(0);             // 0x1CE8 -- mpAggressionVictim = NULL
        meAggressionVictim        = -1;             // 0x1CF0 -- EGlobalRaceCarIndex victim = none
    }

    // ====================================================================================
    // Update @0x8279AEB0  -- the per-frame spine.
    //
    // Gate: only run when mbIsActive && a valid player car was supplied; otherwise clear the four
    // control outputs + their flags and bail. When active: refresh the player timers + choose/bias
    // the SteeringFan; (if the racing line is generating) sample the centre-line; if the car is
    // crashing, just decay the invuln timer and tick the behaviour once; else store the car's 2D
    // position + speed and -- unless showtime-frozen -- run the full pipeline (distance-from-player,
    // racing-line init, UpdateBehaviour, (re)generate the line + braking anticipation, then
    // CalculateCarControls) and assert the steering angle is finite.
    // ====================================================================================
    void AIDriver::Update(f32 lfTimeStep, bool lbLineUpdateToken, Vector3 lPlayerCarPosition,
                          AICar* lpPlayerCar, bool lbDoInRangeCatchup, CgsNumeric::Random* lpRandom)
    {
        // r5 (lbLineUpdateToken == (slot == AIModule::miLineUpdateTokenCounter)) is set up by the
        // caller but never read by the console body.
        (void)lbLineUpdateToken;

        if (mbIsActive && lpPlayerCar)
        {
            UpdatePlayerTimers(lfTimeStep, lpPlayerCar);
            SetDrivingFanBiases(lpPlayerCar);

            // 0x8279AF0C..0x8279AF50: while the racing line is initialised, cache the centre
            // line under the car; otherwise clear the "known" flag.
            if (GetRacingLine().mbIsInitialised)
            {
                // X360 @0x8279AF1C..0x8279AF44: v1 = BrnMath::Flatten(mpCar->GetPosition()) and
                // `vmr v2, v1` -- BOTH vector arguments are the flattened car position (the same
                // shape SteeringFan::CachePointAhead @0x8279145C uses, where both are
                // mCentreTarget). r5 = this+0x1B00 == RacingLine::mCentreHere, r6 = this+0x1B10 ==
                // RacingLine::mCentreAhead; r3 = this+0x1B30 (generator), r4 = this+0xF20 (line).
                const Vector2 lCarPos2D = To2D(mpCar->GetPosition());
                GetRacingLine().mbCentreLineHereKnown =
                    mRacingLineGenerator.GetCentreCentreLineHere(&GetRacingLine(),
                                                                 lCarPos2D,
                                                                 lCarPos2D,
                                                                 GetRacingLine().mCentreHere,
                                                                 GetRacingLine().mCentreAhead);
            }
            else
            {
                GetRacingLine().mbCentreLineHereKnown = false;   // stb 0, 0x1AF1
            }

            AICar* lpCar = mpCar;
            if (lpCar->IsCrashing())                       // *(car+0x1542)
            {
                UpdateBehaviour(lfTimeStep, lpPlayerCar);
                mfInvulnerableTime = 2.0f;  // 0x1D0C (flt_820C41F4)
            }
            else
            {
                if (mfInvulnerableTime > 0.0f)
                    mfInvulnerableTime = mfInvulnerableTime - lfTimeStep;

                m2DCarPos  = To2D(lpCar->GetPosition());   // 0x1CD0 (world x, world z)
                mfCarSpeed = mpCar->GetSpeed();            // 0x1D00

                // lbz 0x1541(car) == mbIsInAir -- an airborne car keeps its controls cleared.
                if (lpCar->mbIsInAir)
                {
                    mfAccelerator = 0.0f;  mbUseForcedSpeed = 0;
                    mfBrake       = 0.0f;  mbBoosting       = 0;
                    mfForcedSpeed = 0.0f;  mbWantToExitDrift = 0;
                    mfHandBrake   = 0.0f;  mbWantToEnterDrift = 0;
                    UpdateSteeringAngle(0.0f);
                }
                else
                {
                    CalcDistanceFromPlayer(lPlayerCarPosition);
                    InitialiseRacingLine();
                    UpdateBehaviour(lfTimeStep, lpPlayerCar);

                    if (mbIsRacingLineInitialised)
                    {
                        if (!mbCurrentRouteComplete)
                            GenerateRacingLine(lfTimeStep);
                        if (mbIsRacingLineInitialised)
                            UpdateBrakingAnticipationData();
                    }

                    CalculateCarControls(lfTimeStep, lPlayerCarPosition, lbDoInRangeCatchup, lpRandom);

                    CGS_ASSERT_AT(IsFinite(mfSteeringAngle), "RwMath::IsValid( mfSteeringAngle )", CGS_ARTIST_UNITY_BRNAIDRIVER_CPP_FILE, 823);
                }
            }
        }
        else
        {
            mfAccelerator = 0.0f;  mbUseForcedSpeed = 0;
            mfBrake       = 0.0f;  mbBoosting       = 0;
            mfForcedSpeed = 0.0f;  mbWantToExitDrift = 0;
            mfHandBrake   = 0.0f;  mbWantToEnterDrift = 0;
        }
    }

    // ====================================================================================
    // UpdateBehaviour @0x8279A680
    //
    // Tick the per-car behaviour state machine (meBehaviour @ car+5300). If the car is crashing,
    // force the CRASHING(7) behaviour first (saving the previous). Then dispatch on meBehaviour:
    //   0/9/10  -> clear the stuck timer;   1/2/8 -> no-op (return);
    //   3/4     -> stuck<->reverse toggle by the reverse flag, tick aggression + UpdateStuck;
    //   5       -> UpdateQuickTurn;         6 -> slow-turn behaviour + UpdateStuck;
    //   7       -> zero all controls + steering, then (if no longer crashing) restore behaviour 3;
    //   else    -> assert "Unknown AI behaviour".
    //
    // ROLLING_START (1) IS A NO-OP HERE and this file NEVER releases it: cases 1/2/8 branch
    // straight to the epilogue (asm 0x8279A724 -> loc_8279A86C). The driver cannot leave
    // behaviour 1 -- only the module can, through AICar::OnModeStartRacing @0x82765C50
    // (behaviour <- CRUISING, mbIsStartingRace <- 0), which AIModule::OnModeStartRacing
    // @0x8276E4B0 calls for every active in-mode car from exactly two places: the
    // full-rolling-start arm of E_ACTION_STOP_MODE_INTRO (asm 0x827922DC, `li r4, 1`) and
    // E_ACTION_START_PLAYING_MODE (asm 0x8279231C, `li r4, 0`). Nothing here waits on
    // mbIsStartingRace or IsOnStartLine. During behaviour 1 CalculateCarControls still runs
    // DoDrivingBehaviour, so the car DOES drive -- at AICar::CalcDesiredSpeed @0x82796078's
    // meBehaviour==1 arm (opponent-rank-scaled speed), which is what makes a rolling start roll.
    // ====================================================================================
    void AIDriver::UpdateBehaviour(f32 lfTimeStep, AICar* lpPlayerCar)
    {
        (void)lpPlayerCar;
        AICar* lpCar = mpCar;

        if (lpCar->IsCrashing())
        {
            const EAIBehaviour lePrev  = lpCar->meBehaviour;   // lwz 0x14B4
            lpCar->mfBehaviourTimer    = 0.0f;                 // stfs 0x14E0
            lpCar->mePreviousBehaviour = lePrev;               // stw  0x14B8
            lpCar->meBehaviour         = E_AI_BEHAVIOUR_CRASHING;   // stw 0x14B4, 7
        }

        switch (lpCar->meBehaviour)
        {
            case 0:
            case 9:
            case 10:
                mfStuckTime = 0.0f;                          // 0x1D04
                break;

            case 1:
            case 2:
            case 8:
                return;

            case 3:
                // lbz 0x1C60(this) == mAggression.mbIsSuitableForAggression (AIAggression +0x60).
                // The X360 spells the test `(_cntlzw(byte) & 0x20) == 0`, which for a byte load
                // is exactly "the byte is non-zero".
                if (GetAggression()->IsSuitableForAggressionFlag())
                {
                    lpCar->mfBehaviourTimer    = 0.0f;                     // stfs 0x14E0
                    lpCar->mePreviousBehaviour = E_AI_BEHAVIOUR_CRUISING;   // stw 0x14B8, 3
                    lpCar->meBehaviour         = E_AI_BEHAVIOUR_FIGHTING;   // stw 0x14B4, 4
                }
                if (meAggressionVictim != -1)   // lwz 0x1CF0 != -1
                    GetAggression()->Update(lfTimeStep, GetAggressionVictimCar());
                UpdateStuck(lfTimeStep);
                break;

            case 4:
                if (!GetAggression()->IsSuitableForAggressionFlag())
                {
                    lpCar->mfBehaviourTimer    = 0.0f;
                    lpCar->mePreviousBehaviour = E_AI_BEHAVIOUR_FIGHTING;   // stw 0x14B8, 4
                    lpCar->meBehaviour         = E_AI_BEHAVIOUR_CRUISING;   // stw 0x14B4, 3
                }
                GetAggression()->Update(lfTimeStep, GetAggressionVictimCar());
                UpdateStuck(lfTimeStep);
                break;

            case 5:
                UpdateQuickTurn();
                break;

            case 6:
                DoSlowTurnBehaviour();
                UpdateStuck(lfTimeStep);
                break;

            case 7:
                mfAccelerator = 0.0f;  mfBrake = 0.0f;
                mfForcedSpeed = 0.0f;  mfHandBrake = 0.0f;
                mbUseForcedSpeed = 0;  mbBoosting = 0;
                mbWantToExitDrift = 0; mbWantToEnterDrift = 0;
                UpdateSteeringAngle(0.0f);
                mfStuckTime = 0.0f;
                if (!lpCar->IsCrashing())
                {
                    const EAIBehaviour lePrev  = lpCar->meBehaviour;
                    lpCar->mfBehaviourTimer    = 0.0f;
                    lpCar->mePreviousBehaviour = lePrev;
                    lpCar->meBehaviour         = E_AI_BEHAVIOUR_CRUISING;
                }
                break;

            default:
                CGS_FAIL_AT("Unknown AI behaviour", CGS_ARTIST_UNITY_BRNAIDRIVER_CPP_FILE, 1318);
                break;
        }
    }

    //
    // CalculateSteeringAngle owns the heading and the target position as locals; UpdateSteeringAngle

    // ====================================================================================
    // UpdateSteeringAngle @0x827708F0  -- the STEER ACTUATOR.
    //
    // Step the stored steering angle toward lfTargetAngle. The target is FLIPPED when the car is
    // reversing (speed < 0). The per-frame step rate is behaviour-dependent (non-player cars use
    // 1.0; player cars pick a rate by meBehaviour). Then StepTo() the angle and store it.
    // NOTE the AI rate is 1.0, so for a rival StepTo SNAPS: mfSteeringAngle == lfTargetAngle.
    // ====================================================================================
    void AIDriver::UpdateSteeringAngle(f32 lfTargetAngle)
    {
        const f32 lfSpeed = mpCar->GetSpeed();
        if (lfSpeed < 0.0f)
            lfTargetAngle = -lfTargetAngle;     // reverse: flip the target

        f32 lfRate;
        if (!mpCar->IsPlayerCar())              // lbz 0x1549(car)
        {
            lfRate = KF_AI_STEERING_STEP;           // flt_82001C98 == 1.0
        }
        else
        {
            const s32 liBeh = static_cast<s32>(mpCar->meBehaviour);   // lwz 0x14B4(car)
            if (liBeh == 1 || liBeh == 9 || liBeh == 10)
                lfRate = KF_PLAYER_ROLLING_START_STEERING_STEP;   // flt_820C4228 == 0.02
            else if (liBeh == 2)
                lfRate = KF_PLAYER_DRIVE_THRU_STEERING_STEP;      // flt_820C422C == 0.03
            else
                lfRate = KF_PLAYER_STEERING_STEP;                 // flt_820C424C == 0.1
        }

        mfSteeringAngle = StepTo(mfSteeringAngle, lfTargetAngle, lfRate);

    }

    // ====================================================================================
    // CalculateSteeringAngle @0x8277CD18  -- THE CORE STEERING MATHS.
    //
    // When the racing line is initialised:
    //   1. heading = normalise2D( useful-or-plain car direction );
    //   2. target  = GetTargetPosition();  steeringTargetVector = normalise2D(target - 2DCarPos)
    //      (asserts the raw target delta is finite);
    //   3. angle   = FindSignedAngleBetween2DVectors( heading, steeringTargetVector ) -> stored to
    //      mfCalculatedSteeringAngle (asserts finite);
    //   4. PID step: choose the drift vs normal controller by the car's useful-direction flag;
    //      pid.Record(angle, dt); out = pid.GetOutput();
    //   5. mfPIDOutput = clamp( out, -1, +1 );  then UpdateSteeringAngle( mfPIDOutput ).
    // ====================================================================================
    void AIDriver::CalculateSteeringAngle(f32 lfTimeStep)
    {
        if (!mbIsRacingLineInitialised)
            return;

        const bool lbUseful = mpCar->mbIsDrifting;   // lbz 0x1544(car) == mbIsDrifting

        // 1. current heading. The X360 flattens FIRST (the vrlimi pair @0x8277CD78/8C -- see To2D),
        //    then tests BOTH ground lanes against FLT_EPSILON (flt_820C3B70 == 1.1920929e-7,
        //    `vandc` = fabs, `vcmpgtfp` per lane @0x8277CD94..0x8277CE28). When BOTH lanes are
        //    within epsilon the vector is REPLACED with (1, 0) before the normalise
        //    (@0x8277CE44..0x8277CE58 stores 1.0/0.0/0/0 over it). Added 2026-09-04 (aiwave R7):
        //    the host used to hand a degenerate vector to Normalize2D, which then returned (0,0)
        //    (NaN since FX-TAILS-A, as the console's unguarded rsqrt) and made the signed angle
        //    meaningless.
        Vector2 lHeadingRaw;
        if (lbUseful)
            lHeadingRaw = To2D(mpCar->GetUsefulDirection());
        else
            lHeadingRaw = To2D(mpCar->GetDirection());

        if (!(std::fabs(lHeadingRaw.mV.x) > 1.1920928955078125e-07f) &&
            !(std::fabs(lHeadingRaw.mV.y) > 1.1920928955078125e-07f))
        {
            lHeadingRaw = Vector2(1.0f, 0.0f);
        }
        const Vector2 lHeading = Normalize2D(lHeadingRaw);

        // 2. target delta -> steering target vector. The RAW delta is stored to
        //    mSteeringTargetVector first (`stvx128 v0, r0, r30` @0x8277CED8) and only overwritten
        //    with the normalised one on the non-degenerate path (@0x8277D030).
        const Vector2 lTarget = GetTargetPosition();
        Vector2 lDelta;
        lDelta.mV = __vsubfp(lTarget.mV, m2DCarPos.mV);
        CGS_ASSERT(IsFinite(lDelta.mV.x) && IsFinite(lDelta.mV.y), "Invalid target vector");
        mSteeringTargetVector = lDelta;                          // 0x1C70 (raw)

        // The SAME per-lane epsilon test on the delta (@0x8277CF48..0x8277CFE0), but here a
        // degenerate delta SKIPS the normalise AND the angle call entirely: the asm branches to
        // 0x8277D080 with f30 still holding flt_82001CC0 == 0.0, so the PID records a ZERO error.
        const bool lbDeltaDegenerate =
            !(std::fabs(lDelta.mV.x) > 1.1920928955078125e-07f) &&
            !(std::fabs(lDelta.mV.y) > 1.1920928955078125e-07f);

        f32 lfAngleToTarget = 0.0f;
        if (!lbDeltaDegenerate)
        {
            mSteeringTargetVector = Normalize2D(lDelta);         // 0x1C70 (normalised)

            // 3. signed planar angle heading -> steering target.
            lfAngleToTarget = FindSignedAngleBetween2DVectors(lHeading, mSteeringTargetVector);
            CGS_ASSERT(IsFinite(lfAngleToTarget), "RwMath::IsValid( lfAngleToTarget )");
        }
        mfCalculatedSteeringAngle = lfAngleToTarget;             // 0x1D50

        // 4. PID controller (the drift controller while the car is drifting, else the normal
        //    one). X360 @0x8277CE.. `addi r3, this, 0x1B98` vs `addi r3, this, 0x1B34`.
        PIDController& lrPid = lbUseful ? mPIDControllerDrift : mPIDController;
        lrPid.Record(lfAngleToTarget, lfTimeStep);
        const f32 lfPidOut = lrPid.GetOutput();

        // 5. clamp(pidOut, -1, +1) -> mfPIDOutput; steer toward it. The X360 stores the PID
        // output clamped DIRECTLY (asm: f0=flt_820037C8=-1.0; fsubs/fsel max(out,-1) then
        // min(.,+1); stfs 0x1D54). There is NO negation of the PID output.
        // fpu::Clamp<float> (DWARF) is the fsel ladder 0x8277D0B8..0x8277D0C4: a NaN output --
        // and a NaN recorded error keeps the PID integral NaN for good -- steers +1.0 on the
        // console. The old `if (< -1) ..; if (> 1) ..` passed the NaN on to the actuator.
        const f32 lfSteer = ClampFsel(lfPidOut, -1.0f, 1.0f);
        mfPIDOutput = lfSteer;                                   // 0x1D54
        UpdateSteeringAngle(lfSteer);
    }

    // ====================================================================================
    // CalculateCarControls @0x827998C0  -- THROTTLE/BRAKE/STEER DISPATCH.
    //
    // Clears the control outputs + flags, computes the desired speed (CalculateDesiredSpeed ->
    // mfDesiredSpeed), then dispatches on the CAR's behaviour (car+5300):
    //   0  (drive-to-line): CalculateSteeringAngle; if speed past KF_STOP_BRAKE_SPEED, force a
    //                       full brake (brake=1, forcedSpeed=0, useForcedSpeed=1);
    //   1-4,10 (driving)  : DoDrivingBehaviour;
    //   5  (quick turn)   : UpdateSteeringAngle(mQuickTurnSteeringLock);
    //   6  (slow turn)    : DoSlowTurn;
    //   7  (crashing)     : leave controls cleared;
    //   8  (launch/boost) : full accelerate (accel=1), forcedSpeed=-1, no boost/steer;
    //   9  (settle)       : CalculateSteeringAngle; light 0.2 brake;
    //   else              : assert "AI driver with unknown behaviour".
    // Finally asserts the steering angle is finite.
    // ====================================================================================
    void AIDriver::CalculateCarControls(f32 lfTimeStep, Vector3 lPlayerCarPosition,
                                        bool lbDoInRangeCatchup, CgsNumeric::Random* lpRandom)
    {
        // r6 (the module's Random) is set up by the caller and never read by the console body.
        (void)lpRandom;

        mfAccelerator      = 0.0f;  // 0x1D28
        mfBrake            = 0.0f;  // 0x1D2C
        mbUseForcedSpeed   = 0;     // 0x1D66
        mfForcedSpeed      = 0.0f;  // 0x1D18
        mbBoosting         = 0;     // 0x1D6B
        mfHandBrake        = 0.0f;  // 0x1D30
        mbWantToExitDrift  = 0;     // 0x1D64
        mbWantToEnterDrift = 0;     // 0x1D65

        // X360 @0x827998E4 `mr r4, r5` -- CalculateDesiredSpeed(v1 = the player position it was
        // handed, r4 = lbDoInRangeCatchup). Neither is read by that body (see the partfile).
        CalculateDesiredSpeed(lPlayerCarPosition, lbDoInRangeCatchup);   // -> mfDesiredSpeed

        switch (mpCar->meBehaviour)
        {
            case 0:
                CalculateSteeringAngle(lfTimeStep);
                if (mpCar->GetSpeed() > KF_STOP_BRAKE_SPEED)   // flt_8300D984 == 5 mph
                {
                    mfForcedSpeed    = 0.0f;       // 0x1D18
                    mfBrake          = 1.0f;       // 0x1D2C
                    mbUseForcedSpeed = 1;          // 0x1D66
                }
                break;

            case 1:
            case 2:
            case 3:
            case 4:
            case 10:
                DoDrivingBehaviour(lfTimeStep);
                break;

            case 5:
                UpdateSteeringAngle(mQuickTurnSteeringLock);     // 0x1CF8
                break;

            case 6:
                DoSlowTurn(lfTimeStep);
                break;

            case 7:
                break;

            case 8:
                mfBrake            = 0.0f;     // 0x1D2C (stfs f31)
                mbBoosting         = 0;        // 0x1D6B
                mbWantToExitDrift  = 0;        // 0x1D64
                mfAccelerator      = 1.0f;     // 0x1D28 (flt_82001C98 == 1.0)
                mbWantToEnterDrift = 1;        // 0x1D65
                mfSteeringAngle    = -1.0f;    // 0x1D24 (flt_820037C8 == -1.0) -- launch hard left
                break;

            case 9:
                CalculateSteeringAngle(lfTimeStep);
                mbUseForcedSpeed = 0;          // 0x1D66
                mfBrake          = 0.2f;   // 0x1D2C (flt_820C4300 == 0.2)
                break;

            default:
                CGS_FAIL_AT("AI driver with unknown behaviour", CGS_ARTIST_UNITY_BRNAIDRIVER_CPP_FILE, 2015);
                break;
        }

        CGS_ASSERT_AT(IsFinite(mfSteeringAngle), "RwMath::IsValid( mfSteeringAngle )", CGS_ARTIST_UNITY_BRNAIDRIVER_CPP_FILE, 2022);
    }

    // ====================================================================================
    // ComputeRouteDirection @0x82766500
    //
    // Where to aim along the ROUTE: needs a valid route with >1 node. Take the car's next route
    // node; if it is node 0 (or earlier) aim toward node 1, else aim FROM the previous node TO the
    // next node. The direction is normalise2D(toNode - fromNode), written to lrOutDirection.
    // Returns true on success, false when there is no usable route.
    // ====================================================================================
    bool AIDriver::ComputeRouteDirection(Vector2& lrOutDirection)
    {
        AICar* lpCar = mpCar;

        // 0x82766518..0x82766540: route validity == (meStatus (car+0x1408) != UNINITIALISED &&
        // miNodeCount (car+0x1400) > 0) -- HasValidRoute(), `cmpwi r11,0 ; bgt`, strictly > 0.
        const Route* lpRoute = lpCar->GetRoute();   // mRoute is the AICar's own base
        if (!(lpRoute->GetStatus() != Route::E_STATUS_UNINITIALISED && lpRoute->GetNodeCount() > 0))
        {
            return false;
        }

        // ⛔ ADDED 2026-09-04 (aiwave R7): the console then takes a SECOND, tighter bound the host
        // was missing -- `0x82766544 lwz r11,0x1400(r30) ; 0x82766548 cmpwi cr6,r11,1 ;
        // 0x8276654C ble cr6, loc_82766660` -- a one-node route returns false. Without it the
        // liNextNode <= 0 arm below reads Route::GetNode(1) off the end of a single-node route.
        if (lpRoute->GetNodeCount() <= 1)
        {
            return false;
        }

        const s32 liNextNode = lpCar->miNextRouteNodeIndex;   // lwz 0x1524(car)

        const RouteNode* lpNext = lpRoute->GetNode(liNextNode);
        const RouteNode* lpFrom;
        const RouteNode* lpTo;
        if (liNextNode <= 0)
        {
            lpFrom = lpNext;                 // node 0 -> aim at node 1
            lpTo   = lpRoute->GetNode(1);
        }
        else
        {
            lpFrom = lpRoute->GetNode(liNextNode - 1);
            lpTo   = lpNext;
        }

        const Vector2 lTo(lpTo->GetX(), lpTo->GetY());
        const Vector2 lFrom(lpFrom->GetX(), lpFrom->GetY());
        Vector2 lDir;
        lDir.mV = __vsubfp(lTo.mV, lFrom.mV); // ARTIST 0x82766604
        lrOutDirection = Normalize2D(lDir);
        return true;
    }

    // ====================================================================================
    // GetTargetPosition @0x8277CBF8  (sret Vector2)
    //
    // Where to aim THIS frame. When the car wants a direct-target line (mbIsDrivenByPlayer-style
    // flag @car+5450): aim at the car's position offset by its normalised facing (a unit-ahead
    // look point). Otherwise delegate to the SteeringFan's GetDrivingTarget. Returns the 2D point.
    // ====================================================================================
    Vector2 AIDriver::GetTargetPosition()
    {
        AICar* lpCar = mpCar;
        Vector2 lResult;

        if (lpCar->mbIsDrivenByPlayer)                           // lbz 0x154A(car)
        {
            // X360 @0x8277CC48..0x8277CC58: the POSITION is flattened with the same vrlimi pair
            // (lane0 <- .x, lane1 <- .z) before the unit facing is added
            // (`vmaddcfp128 v0, v13, v0, v127` @0x8277CCC4). ⛔ FIXED 2026-09-04 (aiwave R7): the
            // host read lPos.y (world height) into the ground lane.
            // The normalise's LAST multiply is fused with the position add: the rsqrt chain
            // (0x8277CC88..0x8277CCC0, `vmulfp128` squares, `vaddfp`, `vrsqrtefp` + two Newton
            // steps, no zero guard) leaves y2 in v0, then `vmaddcfp128 v0, v13(dir.xz), v0(y2),
            // v127(pos.xz)` @0x8277CCC4 == pos + dir * y2 with ONE rounding per lane.
            // [FX-AIBUZZ 2026-09-24 (FX-TAILS-A follow-up): was pos + Normalize2D(dir) -- the product
            //  dir * y2 rounded on its own, then the add]
            const Vector3 lPos = lpCar->GetPosition();
            const Vector3 lDirection = lpCar->GetDirection();
            const __vector4 pos = __vrlimi(lPos.mV, lPos.mV, 4, 1);
            const __vector4 dir = __vrlimi(lDirection.mV, lDirection.mV, 4, 1);
            const __vector4 product = __vmulfp(dir, dir);
            const __vector4 dot = __vaddfp(__vspltw(product, 0), __vspltw(product, 1));
            const __vector4 one = __vcfsx(__vspltisw(1), 0);
            const __vector4 half = __vcfsx(__vspltisw(1), 1);
            __vector4 estimate = __vrsqrtefp(dot);
            __vector4 correction = __vnmsubfp(dot, __vmulfp(estimate, estimate), one);
            estimate = __vmaddfp(__vmulfp(estimate, half), correction, estimate);
            correction = __vnmsubfp(dot, __vmulfp(estimate, estimate), one);
            estimate = __vmaddfp(__vmulfp(estimate, half), correction, estimate);
            lResult.mV = __vmaddfp(dir, estimate, pos);
        }
        else
        {
            // X360 asm: SteeringFan::GetDrivingTarget(out, this+1808 (== mSteeringFan @0x710),
            // car, this+3872 (== mRacingLine @0xF20), this+6960 (== mRacingLineGenerator @0x1B30),
            // this (the driver's NearbyVehicles @0x000), 0).
            lResult = mSteeringFan.GetDrivingTarget(lpCar,
                                                    &GetRacingLine(),
                                                    &mRacingLineGenerator,
                                                    GetNearbyVehicles(),
                                                    false);
        }



        return lResult;
    }

    // ====================================================================================
    // AttemptToDriveAtDesiredSpeed @0x827706D8  -- the SPEED ACTUATOR.
    //
    // lfSpeedDelta is this frame's allowed speed change. Clears accel/brake/handbrake. The
    // boost-time slot doubles as a force-boost timer: if >0, count it down by lfSpeedDelta and
    // mark not-boosting; else if CheckForBoosting(), latch a 3.0s boost window and set boosting;
    // else clear boosting. Then close the speed loop: if currentSpeed < desiredSpeed, ramp the
    // ACCELERATOR up across the KF_ACCELERATION_MAX_DIFF (20 mph) band; if currentSpeed >
    // desiredSpeed, ramp the BRAKE up across the [KF_BRAKING_START_DIFF, KF_BRAKING_MAX_DIFF]
    // (20 .. 40 mph) band. Each ramp = clamp(gap/width, 0, 1) --
    // i.e. the ramp GROWS with the speed gap (asm: fsel max(x,0) then fsel min(.,1); no '1-x').
    // ====================================================================================
    void AIDriver::AttemptToDriveAtDesiredSpeed(f32 lfTimeStep)
    {
        const f32 lfBoostTimer = mfBoostTimeRemaining;       // 0x1D44
        mfBrake       = 0.0f;                                 // 0x1D2C
        mfAccelerator = 0.0f;                                 // 0x1D28
        mfHandBrake   = 0.0f;                                 // 0x1D30

        // 0x82770700 `fcmpu timer, 0.0 ; ble` -> the CheckForBoosting arm. ble is bc 4,gt: taken
        // on <= AND on unordered, so only an ordered `timer > 0` counts down (FX-AINAN2; `<=`
        // sent a NaN timer to the count-down arm).
        if (!(lfBoostTimer > 0.0f))
        {
            if (CheckForBoosting())
            {
                mfBoostTimeRemaining = KF_MINIMUM_BOOST_TIME;  // flt_820C4154 == 3.0 s
                mbBoosting = 1;
            }
            else
            {
                mbBoosting = 0;
            }
        }
        else
        {
            mbBoosting = 1;
            mfBoostTimeRemaining = lfBoostTimer - lfTimeStep;
        }

        const f32 lfSpeed   = mpCar->GetSpeed();
        const f32 lfDesired = mfDesiredSpeed;                 // 0x1D10

        if (lfSpeed < lfDesired)
        {
            // under speed -> accelerate. ramp = clamp((desired - speed) / 20 mph, 0, 1).
            mfAccelerator = Saturate((lfDesired - lfSpeed) / KF_ACCELERATION_MAX_DIFF);   // 0x1D28
        }
        else if (lfSpeed > lfDesired)
        {
            // over speed -> brake across the [20 mph, 40 mph] over-speed band.
            mfBrake = Saturate(((lfSpeed - lfDesired) - KF_BRAKING_START_DIFF)
                               / (KF_BRAKING_MAX_DIFF - KF_BRAKING_START_DIFF));          // 0x1D2C
        }
    }

    // ====================================================================================
    // CorneringTopSpeed @0x8277D0F0
    //
    // Clamp the target speed by how sharp the upcoming corner is. Computes two unsigned planar
    // angles -- between the car's useful direction and mBrakingRoadDir, and between that SAME
    // useful direction and the (m2DCarPos -> mBrakingAnticipationPos) racing-line direction --
    // whose MAX is the cornering angle, stored to mfAngleToBrakingTarget (asm @0x8277D238/0x8277D258:
    // fsubs is only the fsel condition, fsel picks max(angle1, angle2), not the difference). A ramp
    // clamp((angle - BASE)/RANGE, 0, 1) lerps from inputSpeed toward (inputSpeed * SCALE) to form
    // the cornering speed cap, returned to the caller (X360 returns it via fp1).
    // ====================================================================================
    f32 AIDriver::CorneringTopSpeed(f32 lfInputSpeed)
    {
        const Vector2 lUseful = Normalize2D(To2D(mpCar->GetUsefulDirection()));
        const f32 lfAngle1 = FindUnsignedAngleBetween2DVectors(lUseful, mBrakingRoadDir); // 0x1CB0

        Vector2 lToAnticip;
        lToAnticip.mV = __vsubfp(mBrakingAnticipationPos.mV, m2DCarPos.mV);
        const Vector2 lAnticipDir = Normalize2D(lToAnticip);
        // X360: the SECOND FindUnsignedAngleBetween2DVectors call reuses the normalised useful
        // direction (v2 := v125, the first normalise result) as its first arg and the normalised
        // (anticipPos - carPos) direction as its second -- i.e. angle2 = angle(usefulDir,
        // anticipDir). (NOT angle(anticipDir, anticipDir), which would always be 0.)
        const f32 lfAngle2 = FindUnsignedAngleBetween2DVectors(lUseful, lAnticipDir);

        const f32 lfCorneringAngle = (lfAngle1 >= lfAngle2) ? lfAngle1 : lfAngle2;
        mfAngleToBrakingTarget = lfCorneringAngle;               // 0x1CFC

        const f32 lfRamp = Saturate((lfCorneringAngle - KF_MIN_BRAKING_ANGLE)   // flt_82F3038C, 30 deg
                                    / KF_BRAKING_ANGLE_RANGE);                 // flt_8300DC38, 60 deg

        const f32 lfScaled = lfInputSpeed * KF_AI_MAX_BRAKING_SPEED_PROPORTION;   // flt_820C41FC
        // cap = inputSpeed + (scaled - inputSpeed) * ramp -- lerps from inputSpeed (ramp==0, no
        // cornering yet) toward the scaled-down speed (ramp==1, sharp corner).
        // 0x8277D2A0 `vmaddfp v0, v12, v0, v13` (raw D,A,B,C: v0 = v12*v13 + v0 = (scaled - input) * ramp + input),
        // ONE rounding (ROUNDING_RULE 3; the vsubfp at 0x8277D254 and the fmuls at 0x8277D230 are rule 4).
        return NativeVmxMadd(NativeVmxSub(lfScaled, lfInputSpeed), lfRamp, lfInputSpeed);
    }

    // ====================================================================================
    // ProximitySpeed @0x82770800
    //
    // Clamp the target speed by how close the car is to its proximity reference. t = clamp(
    // (proximityRef - 4.0) * 0.25, 0, 1). v = max( (currentSpeed - speedRef - 10 mph),
    // lfMinSpeed*0.5 ). Returns the lerp v + (lfMinSpeed - v) * t (drops toward lfMinSpeed as the
    // proximity ref grows; asm @0x827708C4/0x827708C8: vsubfp minSpeed-v, vmaddfp (minSpeed-v)*t+v).
    // ====================================================================================
    f32 AIDriver::ProximitySpeed(f32 lfMinSpeed)
    {
        // this+0x1A3C / +0x1A40 == mRacingLine.mfImmediateDistanceToTrafficImpact /
        // mfImmmediateApproachSpeedOfTrafficAhead (RacingLine +0xB1C / +0xB20).
        const f32 lfProxRef  = GetRacingLine().mfImmediateDistanceToTrafficImpact;
        const f32 lfSpeedRef = GetRacingLine().mfImmmediateApproachSpeedOfTrafficAhead;

        const f32 lfT = Saturate((lfProxRef - K_PROXIMITY_CLOSE)       // pooled 4.0 0x820C41C0
                                 / (K_PROXIMITY_FAR - K_PROXIMITY_CLOSE));   // folded: * 0.25 (0x82003F40)

        const f32 lfSpeed = mpCar->GetSpeed();
        f32 lfV = (lfSpeed - lfSpeedRef) - K_PROXIMITY_SPEED_REDUCTION;   // flt_8300DC34 == 10 mph
        const f32 lfFloor = lfMinSpeed * 0.5f;    // flt_820C4168 == 0.5
        if (lfV < lfFloor)
            lfV = lfFloor;

        // 0x827708C8 `vmaddfp v0, v13, v0, v12` (raw D,A,B,C: v0 = v13*v12 + v0 = (minSpeed - v) * t + v), ONE
        // rounding (ROUNDING_RULE 3; the vsubfp at 0x827708C4 is rule 4).
        return NativeVmxMadd(NativeVmxSub(lfMinSpeed, lfV), lfT, lfV);
    }

    // ====================================================================================
    // ChooseRaceSteeringFan @0x82766370
    //
    // Pick the SteeringFan bias mode for RACE mode from the car's mode/relative state. Returns 0
    // (default) unless: the route-finding style is non-default (!=3 and !=0), the relative-location
    // is neither 0 nor 1, and a positive schedule gate puts the car ahead within
    // clamp(gate*0.1, 0, 1)*50 -> 1 (overtake bias); else if the relative-location is 3 and
    // (opponentIndex % 3) != 0 -> 5; else 0.
    // ====================================================================================
    s32 AIDriver::ChooseRaceSteeringFan(AICar* lpCar)
    {
        const s32 leStyle = static_cast<s32>(lpCar->GetRouteFindingStyle()); // car+0x14C0 (5312)
        if (leStyle == 3 || leStyle == 0)
            return 0;

        const s32 luRelLoc = static_cast<s32>(lpCar->meRelativeLocation);    // car+0x14D4 (5332)
        // asm @0x8276638C/0x82766394: two exact equality tests (==1, ==0), not a relational
        // compare -- a negative meRelativeLocation would fall through here in the binary.
        if (luRelLoc == 1 || luRelLoc == 0)
            return 0;

        const f32 lfScheduleGate = lpCar->mfScheduleOffset0;                 // lfs 0x1518(car)
        if (lfScheduleGate > 0.0f)
        {
            const f32 lfDistAhead = lpCar->mfDistanceAheadOfPlayer;          // lfs 0x14F8(car)
            const f32 lfClamped   = Saturate(lfScheduleGate * 0.1f);         // flt_820C424C == 0.1
            if (lfDistAhead > 0.0f)
            {
                if (lfDistAhead < lfClamped * K_NORMAL_SPREAD_HNG)           // flt_820C4244 == 50.0
                    return 1;
            }
        }

        if (luRelLoc != 3)
            return 0;

        const s8 liOpponent = lpCar->GetOpponentIndex();                     // car+0x153A (5434)
        if ((liOpponent % 3) == 0)
            return 0;
        return 5;
    }

    // ====================================================================================
    // SetDrivingFanBiases @0x82770428
    //
    // Select + weight the SteeringFan contributors for the car's current mode. Priority ladder
    // (each rung sets a bias-mode constant on the fan):
    //   - player-driven-AI-not-this, forced-standard or behaviour 2 -> bias 8;
    //   - behaviour 1                                                -> bias 4;
    //   - else by the aggression-machine state (this+0x1C00): state 3 & player NOT protected ->
    //     bias 2; state 13 -> bias 9; otherwise choose aggressive vs race fan from the car's
    //     route-finding style (style 2 or 6 -> aggressive, else race) and SetBiasMode with it.
    // ====================================================================================
    void AIDriver::SetDrivingFanBiases(AICar* lpPlayerCar)
    {
        AICar* lpCar = mpCar;

        const bool lbPlayer         = lpCar->IsPlayerCar();            // lbz 0x1549(car)
        const bool lbDrivenByPlayer = lpCar->mbIsDrivenByPlayer;      // lbz 0x154A(car)
        const bool lbForceStandard  = lpCar->ForceStandardRoute();    // lbz 0x1548(car)
        const s32  liBehaviour      = static_cast<s32>(lpCar->meBehaviour);   // lwz 0x14B4(car)



        if (lbPlayer && !lbDrivenByPlayer && (lbForceStandard || liBehaviour == 2))
        {
            mSteeringFan.SetBiasMode(static_cast<EBiasMode>(8));
            return;
        }

        if (liBehaviour == 1)
        {
            mSteeringFan.SetBiasMode(static_cast<EBiasMode>(4));
            return;
        }

        // lwz 0x1C00(this) == mAggression.meAggressionState (AIAggression +0x00).
        const EAIAggressionState leAggressionState = GetAggression()->GetAggressionState();
        if (leAggressionState == E_AI_AGGRESSION_STATE_ATTACK_SLAM && !IsPlayerProtected(lpPlayerCar))
        {
            mSteeringFan.SetBiasMode(static_cast<EBiasMode>(2));
            return;
        }

        if (leAggressionState == E_AI_AGGRESSION_STATE_VEER_EXTREME)
        {
            mSteeringFan.SetBiasMode(static_cast<EBiasMode>(9));
            return;
        }

        const s32 leStyle = static_cast<s32>(lpCar->GetRouteFindingStyle()); // lwz 0x14C0(car)
        const bool lbAggressive = (leStyle == 2 || leStyle == 6);
        const s32 leBias = lbAggressive ? ChooseAggressiveSteeringFan(lpPlayerCar)
                                        : ChooseRaceSteeringFan(lpCar);
        mSteeringFan.SetBiasMode(static_cast<EBiasMode>(leBias));
    }

    // ====================================================================================
    // IsInvulnerable @0x82765740
    //
    // Whether the AI car is currently protected from takedowns. Asserts mpCar is bound. When the
    // car is BEHIND the player (meRelativeLocation < 2 -- E_RELATIVE_BEHIND_* == 0/1): invulnerable
    // if it is more than 30m away OR the invuln timer is still positive. When AHEAD of the player
    // (meRelativeLocation >= 2): invulnerable only while the invuln timer is positive. The X360
    // tests meRelativeLocation for exact ==1 / ==0 (both branch to the distance arm), which is the
    // Hex-Rays `< 2u` form.
    // ====================================================================================
    bool AIDriver::IsInvulnerable() const
    {
        CGS_ASSERT_AT(mpCar != 0, "mpCar != NULL", CGS_ARTIST_ABS_BRNAIDRIVER_H_FILE, 930);

        if (mpCar->meRelativeLocation < 2)          // car+0x14D4 (behind player: 0 or 1)
        {
            if (mfDistanceToPlayer > 30.0f)   // flt_820C3FA8 == 30.0
                return true;
            if (mfInvulnerableTime > 0.0f)              // 0x1D0C (flt_82001CC0 == 0.0)
                return true;
            return false;
        }

        return mfInvulnerableTime > 0.0f;               // ahead of player: timer only
    }

    // ====================================================================================
    // IsOnStartLine @0x82765800
    //
    // Whether the AI car is sitting on the start line. Returns false when no car is bound. Asserts
    // the car is not INACTIVE (a start-line query on a dead car is a logic error), then returns the
    // car's own on-start-line flag (car+0x1547).
    // ====================================================================================
    bool AIDriver::IsOnStartLine()
    {
        AICar* lpCar = mpCar;                       // 0x1CE0
        if (!lpCar)
            return false;

        CGS_ASSERT(lpCar->meCarState != E_AI_CAR_STATE_INACTIVE,  // car+0x14C8 != 2
                   "GetState() != E_AI_CAR_STATE_INACTIVE");

        return lpCar->mbIsOnStartLine != 0;             // car+0x1547
    }
}

// FOLDED FROM BrnAIDriver_Update.cpp
#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameSource/World/AI/BrnAICar.h"                  // AICar (named members + accessors)
#include "GameSource/World/AI/BrnAIUtils.h"                // Find{Signed,Unsigned}AngleBetween2DVectors
#include "GameSource/World/AI/BrnAIAggression.h"           // AIAggression (the embedded sub-machine)
#include "GameSource/World/AI/Route/BrnRacingLine.h"       // RacingLine
#include "GameSource/World/AI/Route/BrnRoute.h"            // Route / RouteNode
#include "GameSource/World/AI/PID/BrnPIDController.h"      // PIDController::Prepare
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficAIInterfaces.h" // BrnTraffic::BrnTrafficIO::TrafficAIEntity

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"         // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/math/vpu/vector3_operation.h"                 // vpu::Magnitude / vpu::Dot
#include "rw/math/vpu/vector2_operation.h"                 // vpu::Magnitude (2-lane: GetIndexOfFurthestVehicle)
#include "rw/math/fpu/scalar_operation.h"                   // fpu::SgnNonZero (StartDrift)

#include "GameSource/AttribSys/Generated/classes/burnoutcarasset.h"
#include "GameSource/AttribSys/Generated/classes/physicsvehiclehandling.h"
#include "GameSource/AttribSys/Generated/classes/physicsvehiclesteeringattribs.h"
#include <cstdlib>
#include <cmath>   // std::sqrt, std::fabs, std::isfinite

// =================================================================================================
// BrnAIDriver.cpp PARTFILE -- the Update-path callees.
//
// BrnAIDriver.cpp bodies the spine (Construct / SetAICar / Prepare / Update / UpdateBehaviour /
// UpdateSteeringAngle / CalculateSteeringAngle / CalculateCarControls / ComputeRouteDirection /
// GetTargetPosition / AttemptToDriveAtDesiredSpeed / CorneringTopSpeed / ProximitySpeed /
// ChooseRaceSteeringFan / SetDrivingFanBiases / IsInvulnerable / IsOnStartLine). This file bodies
// everything that spine calls. The DWARF tunables are defined at the top of the file; nothing here duplicates a
// symbol from the other (one body per symbol -- LNK2005 checked).
//
// THE KEYSTONE'S CALLEE ORDER -- BrnAI::AIDriver::Update @0x8279AEB0, with the asm address of each
// call. Register map from the asm + AIModule::UpdateDrivers @0x8279B334:
//   r3 = this, f1 = lfTimeStep, r4 = SKIPPED (f1's GPR slot, PPC float-arg rule),
//   r5 = (slot == AIModule::miLineUpdateTokenCounter), v1 = player car position,
//   r6 = lpPlayerCar, r7 = mbDoInRangeCatchup, r8 = &AIModule::mRandom.
//
//   0x8279AEE8  if (!mbIsActive (0x1D69) || !lpPlayerCar) -> clear the 8 control fields, return
//   0x8279AF00  UpdatePlayerTimers(dt, lpPlayerCar)                        @0x82770320  [this file]
//   0x8279AF08  SetDrivingFanBiases(lpPlayerCar)                           @0x82770428  [spine]
//   0x8279AF10  if (mRacingLine.mbIsInitialised (0x1AF0)):
//   0x8279AF18    AICar::GetPosition(mpCar); BrnMath::Flatten
//   0x8279AF40    mRacingLine.mbCentreLineHereKnown (0x1AF1) =
//                   RacingLineGenerator::GetCentreCentreLineHere(this+0x1B30, this+0xF20,
//                                                                this+0x1B00, this+0x1B10)
//                 else mbCentreLineHereKnown = 0                                        [A6 gate]
//   0x8279AF60  if (mpCar->mbIsCrashing (0x1542)):
//   0x8279AF74      UpdateBehaviour(dt)                                    @0x8279A680  [spine]
//   0x8279AF84      mfInvulnerableTime (0x1D0C) = 2.0
//                 else:
//   0x8279AF9C      if (mfInvulnerableTime > 0) mfInvulnerableTime -= dt
//   0x8279AFB8      m2DCarPos (0x1CD0) = 2D(mpCar->GetPosition())
//   0x8279AFD8      mfCarSpeed (0x1D00) = mpCar->GetSpeed()
//   0x8279AFEC      if (mpCar->mbIsInAir (0x1541)) { clear the 8 control fields;
//   0x8279B020                                       UpdateSteeringAngle(0.0) }          [spine]
//                   else:
//   0x8279B03C        CalcDistanceFromPlayer(v1 = player position)         @0x8276E1C8  [this file]
//   0x8279B044        InitialiseRacingLine()                               @0x82792DF0  [this file]
//   0x8279B054        UpdateBehaviour(dt)                                  @0x8279A680  [spine]
//   0x8279B070        if (mbIsRacingLineInitialised (0x1D68)) {
//   0x8279B07C          if (!mbCurrentRouteComplete (0x1D6A)) GenerateRacingLine(dt) @0x8277C4E8 [this file]
//   0x8279B090          if (mbIsRacingLineInitialised) UpdateBrakingAnticipationData() @0x827964C0 [this file]
//                     }
//   0x8279B0A8        CalculateCarControls(dt, v1, r7, r8)                 @0x827998C0  [spine]
//   0x8279B0C4        assert(RwMath::IsValid(mfSteeringAngle))  (:823)
//
// and from there:
//   CalculateCarControls -> CalculateDesiredSpeed @0x827934C0 -> HardShoulderSpeed @0x827930B8
//                                                              -> ProximitySpeed   @0x82770800
//                                                              -> CorneringTopSpeed @0x8277D0F0
//                                                              -> SteeringFan::GetSpeedRatio @0x82779B90
//                        -> DoDrivingBehaviour @0x82799660 -> CalculateSteeringAngle @0x8277CD18
//                                                          -> CheckForSpeedMatch @0x82793020
//                                                          -> AttemptToDriveAtDesiredSpeed @0x827706D8
//                                                             -> CheckForBoosting @0x827705E0
//                                                          -> EstimateNeedForDrifting @0x82793438
//                                                          -> Determine180Turn @0x8277C758
//                                                          -> StartDrift @0x8277C870
//                                                          -> AttemptToDriveAtDesiredSpeedInDrift @0x8277C8E0
//                                                          -> DetermineDriftSteeringAngle @0x827931D0
//                        -> DoSlowTurn @0x8277CA88
//   UpdateBehaviour      -> UpdateStuck @0x82766440 -> (IsStuck @0x82766670 is called by AIModule)
//                        -> UpdateQuickTurn @0x8278B100 / DoSlowTurnBehaviour @0x8277C968
//                        -> AIAggression::Update
//
// FOUR of these have NO IDA export (.ida-exports has no .json and names.tsv has no row); their
// bodies below were read straight from the image bytes with capstone, and each carries its address
// range in the banner: CalculateDesiredSpeed @0x827934C0..0x82793560, DoDrivingBehaviour
// @0x82799660..0x827998BC, UpdateBrakingAnticipationData @0x827964C0..0x827965E4 and
// CalcDistanceFromPlayer @0x8276E1C8..0x8276E26C.
// =================================================================================================

// Asserts pass __FILE__; ARTIST's reads "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../World/AI/BrnAIDriver.cpp".

namespace BrnAI
{
    // TU-local de-SIMD helpers (same shape as BrnAIDriver.cpp's; static, so no ODR clash).
    namespace
    {
        // RwMath::IsValid( x ) -- a NaN SELF-COMPARE, exactly as `vcmpeqfp. vX,vX,vX`.
        // ⛔ CORRECTED (issue #31): was `std::isfinite`, which also rejects +-Inf; ARTIST does not.
        // See the banner on BrnAIDriver.cpp's IsFinite for the recovered compare sites.
        inline bool IsFiniteU(f32 lfValue) { return lfValue == lfValue; }

        // The planar (x,y) normalise: vrsqrtefp + two Newton-Raphson steps + vmulfp, with NO zero
        // guard at any console caller (DetermineDriftSteeringAngle 0x827933DC, GetQuickTurnSteering
        // 0x8277C67C, UpdateQuickTurn 0x8278B168, UpdateBrakingAnticipationData 0x827965AC -- no
        // vcmpeqfp/vsel after the chain). A zero vector is NaN in every lane on the console
        // (vrsqrtefp(+0) = +inf; the Newton step's `1 - lenSq * y0^2` is 0 * inf = NaN); here
        // 1 / sqrt(0) = +inf and 0 * inf = NaN give the same NaN lanes.
        // ⛔ CORRECTED 2026-09-24 (crash parity FX-TAILS-A item 6, FX-AINAN2 OPEN): this was
        // `lenSq > 0 ? 1 / sqrt : 0`, an invented guard that answered (0, 0).
        Vector2 Normalize2DU(Vector2 lVector)
        {
            // ARTIST 0x827933B0..F0 and following refinement: preserve all lanes.
            const __vector4 product = __vmulfp(lVector.mV, lVector.mV);
            const __vector4 dot = __vaddfp(__vspltw(product, 0), __vspltw(product, 1));
            const __vector4 one = __vcfsx(__vspltisw(1), 0);
            const __vector4 half = __vcfsx(__vspltisw(1), 1);
            __vector4 estimate = __vrsqrtefp(dot);
            __vector4 correction = __vnmsubfp(dot, __vmulfp(estimate, estimate), one);
            estimate = __vmaddfp(__vmulfp(estimate, half), correction, estimate);
            correction = __vnmsubfp(dot, __vmulfp(estimate, estimate), one);
            estimate = __vmaddfp(__vmulfp(estimate, half), correction, estimate);
            Vector2 result;
            result.mV = __vmulfp(lVector.mV, estimate);
            return result;
        }

        // BrnMath::Flatten @0x822CB8E8 -- project onto the XZ GROUND plane (drop the height lane).
        // ⛔ FIXED 2026-09-04 (aiwave R7): was `lResult.y = lVector.y` (world HEIGHT). The vperm
        // mask Flatten loads (unk_82CDA450 = 00 01 02 03 | 18 19 1A 1B | 00 01 02 03 | 00 01 02 03)
        // sources lane1 from vB bytes 0x18..0x1B == lane 2 == .z, and the open-coded twin in
        // CalculateSteeringAngle (`vrlimi128 v127,v0,8,0 ; vrlimi128 v127,v0,4,1` @0x8277CD78/8C)
        // does the same. See the long banner on BrnAIDriver.cpp's To2D.
        Vector2 To2DU(Vector3 lVector)
        {
            return Vector2(lVector.X().GetFloat(), lVector.Z().GetFloat());
        }

        // rw::math::fpu::Clamp<float>(x, 0.0f, 1.0f) as the console inlines it (DWARF: HardShoulderSpeed
        // and DoSlowTurn list fpu::Clamp / vpu::Clamp): `fneg t,x ; fsel t,t,0.0,x` then
        // `fsubs u,1.0,t ; fsel r,u,t,1.0` -- HardShoulderSpeed 0x82793110/0x8279311C, DoSlowTurn
        // 0x8277CB68/0x8277CB74 and 0x8277CBA8/0x8277CBB4. fsel takes its THIRD operand on an
        // unordered test, so a NaN comes back as 1.0; the old `if (< 0) .. if (> 1) ..` returned
        // the NaN (crash parity FX-AINAN2). Same ladder as BrnAIDriver.cpp's ClampFsel/Saturate.
        inline f32 SaturateU(f32 lfValue)
        {
            const f32 lfFloored = ((0.0f - lfValue) >= 0.0f) ? 0.0f : lfValue;
            return ((1.0f - lfFloored) >= 0.0f) ? lfFloored : 1.0f;
        }

        inline f32 Dot2D(Vector2 lA, Vector2 lB) { return rw::math::vpu::Dot(lA, lB).GetFloat(); }

        // The two half-extent vectors the avoidance box is built from. On the X360 these are two
        // lazily-initialised statics (unk_8300DC90 == (2,0,..), unk_8300DC80 == (0,2,..); the
        // `dword_8300DCA0 & 1/2` bits are the one-shot guards). Const data on the host.
        Vector2 MakeHalfExtentX()
        {
            return Vector2(2.0f, 0.0f);
        }
        Vector2 MakeHalfExtentY()
        {
            return Vector2(0.0f, 2.0f);
        }

        // Fill one NearbyVehicle's four HNG boundary lines from a closed 4-corner quad, in the
        // console's order: line[i] runs corner[i] -> corner[i+1] (wrapping).
        void FillHNGQuad(NearbyVehicle* lpVehicle, Vector2 lC0, Vector2 lC1, Vector2 lC2, Vector2 lC3)
        {
            lpVehicle->maHNGLines[0].mfStartX = lC0.mV.x; lpVehicle->maHNGLines[0].mfStartY = lC0.mV.y;
            lpVehicle->maHNGLines[0].mfEndX   = lC1.mV.x; lpVehicle->maHNGLines[0].mfEndY   = lC1.mV.y;
            lpVehicle->maHNGLines[1].mfStartX = lC1.mV.x; lpVehicle->maHNGLines[1].mfStartY = lC1.mV.y;
            lpVehicle->maHNGLines[1].mfEndX   = lC2.mV.x; lpVehicle->maHNGLines[1].mfEndY   = lC2.mV.y;
            lpVehicle->maHNGLines[2].mfStartX = lC2.mV.x; lpVehicle->maHNGLines[2].mfStartY = lC2.mV.y;
            lpVehicle->maHNGLines[2].mfEndX   = lC3.mV.x; lpVehicle->maHNGLines[2].mfEndY   = lC3.mV.y;
            lpVehicle->maHNGLines[3].mfStartX = lC3.mV.x; lpVehicle->maHNGLines[3].mfStartY = lC3.mV.y;
            lpVehicle->maHNGLines[3].mfEndX   = lC0.mV.x; lpVehicle->maHNGLines[3].mfEndY   = lC0.mV.y;
        }

    }

    // ================================================================================
    // NearbyVehicles::Next @0x827667D8
    //
    // Claim the next free slot: both asserts, then the unconditional `++miCount`.
    // ================================================================================
    void NearbyVehicles::Next()
    {
        CGS_ASSERT_AT(miCount >= 0, "miCount >= 0", CGS_ARTIST_UNITY_BRNAIDRIVER_CPP_FILE, 2921);
        CGS_ASSERT_AT(miCount < KI_MAX_NEARBY_VEHICLES, "miCount < KI_MAX_NEARBY_TRAFFIC", CGS_ARTIST_UNITY_BRNAIDRIVER_CPP_FILE, 2922);
        ++miCount;                                                                // stw 0x700
    }

    // ================================================================================
    // NearbyVehicles::GetVehiclePointer @0x82766870
    //
    // &mVehicle[liEntry] (stride 0x70 on both console and host -- NearbyVehicle is pointer-free).
    // ================================================================================
    NearbyVehicle* NearbyVehicles::GetVehiclePointer(s32 liEntry)
    {
        CGS_ASSERT_AT(liEntry >= 0,
            "liEntry >= 0", __FILE__, 2942 /* ARTIST 0x8276689C */);                                 // :2942
        CGS_ASSERT_AT(liEntry < KI_MAX_NEARBY_VEHICLES,
            "liEntry < KI_MAX_NEARBY_TRAFFIC", __FILE__, 2943 /* ARTIST 0x827668C0 */);  // :2943

        return &mVehicle[liEntry];                                                // mulli 0x70; add
    }

    // ================================================================================
    // UpdatePlayerTimers @0x82770320
    //
    // Three per-frame timers about the PLAYER, kept on this driver:
    //   mfPlayerSlowSpeedTime  -- climbs (capped at 6 s) while the player is slower than THIS
    //                             car's "decent" speed and is not protected; decays to 0 otherwise
    //                             (the two fsel pairs @0x827703B0 / @0x827703C0 are min/max).
    //   mfPlayerTimeSinceCrash -- 0 while the player is crashing, else += dt.
    //   mfPlayerTimeSinceAIDriven -- += dt while the player car is AI-driven, else 0.
    // With no player car both "time since" timers are zeroed and nothing else runs.
    // ================================================================================
    void AIDriver::UpdatePlayerTimers(f32 lfTimeStep, AICar* lpPlayerCar)
    {
        if (lpPlayerCar == 0)
        {
            mfPlayerTimeSinceCrash    = 0.0f;   // 0x1D34
            mfPlayerTimeSinceAIDriven = 0.0f;   // 0x1D38
            return;
        }

        const f32 lfPlayerSpeed = lpPlayerCar->GetSpeed();
        const f32 lfDecentSpeed = mpCar->GetDecentSpeed();

        // 0x82770378 `fcmpu playerSpeed, decent ; bge` -> the decay arm WITHOUT asking
        // IsPlayerProtected. bge is bc 4,lt: taken on >= AND on unordered, so only an ordered
        // `speed < decent` gets as far as the protection test (FX-AINAN2; `>=` sent a NaN on).
        if (!(lfPlayerSpeed < lfDecentSpeed) || IsPlayerProtected(lpPlayerCar))
        {
            // decay, floored at 0 (fsel f0, f0, f0, 0.0)
            mfPlayerSlowSpeedTime = rw::math::fpu::Max(mfPlayerSlowSpeedTime - lfTimeStep, 0.0f);
        }
        else
        {
            // climb, capped at 6 s (fsel f0, (sum - 6.0), 6.0, sum); DWARF calls fpu::Max then fpu::Min
            mfPlayerSlowSpeedTime = rw::math::fpu::Min(mfPlayerSlowSpeedTime + lfTimeStep, KF_PLAYER_SLOW_SPEED_AGGRESSIVE_TIME);
        }

        if (lpPlayerCar->IsCrashing())              // lbz 0x1542
            mfPlayerTimeSinceCrash = 0.0f;
        else
            mfPlayerTimeSinceCrash = mfPlayerTimeSinceCrash + lfTimeStep;

        if (lpPlayerCar->mbIsDrivenByPlayer)        // lbz 0x154A
            mfPlayerTimeSinceAIDriven = mfPlayerTimeSinceAIDriven + lfTimeStep;
        else
            mfPlayerTimeSinceAIDriven = 0.0f;
    }

    // ================================================================================
    // IsPlayerProtected @0x827660C8
    //
    // "Leave the player alone for now." Two arms, both keyed on the player's
    // mbIsDrivenByPlayer and on THIS car's mfRaceTimer (car+0x14FC):
    //   * early-out FALSE when the player is a ROAD-RAGE-style route finder (meRouteFindingStyle
    //     == 2), this car is still in the first 5 s of its race, and the player car is NOT
    //     AI-driven -- i.e. a human player at the start of a road rage IS fair game;
    //   * otherwise protected while the player car is not AI-driven, or this car is still in its
    //     first 5 s, or the player crashed less than 8 s ago, or has been AI-driven for under 3 s.
    // ================================================================================
    bool AIDriver::IsPlayerProtected(AICar* lpPlayerCar)
    {
        if (lpPlayerCar->GetRouteFindingStyle() == static_cast<ERouteFindingStyle>(2)   // lwz 0x14C0
            && mpCar->mfRaceTimer < KF_PRE_AGGRESSION_DELAY                          // flt_820C488C == 5.0
            && !lpPlayerCar->mbIsDrivenByPlayer)                                         // lbz 0x154A
        {
            return false;
        }

        if (!lpPlayerCar->mbIsDrivenByPlayer)                       return true;
        if (mpCar->mfRaceTimer < KF_PRE_AGGRESSION_DELAY)       return true;
        if (mfPlayerTimeSinceCrash < KF_BE_KIND_AFTER_CRASH_DELAY)  return true;   // flt_820C41E0 == 8.0
        if (mfPlayerTimeSinceAIDriven < KF_BE_KIND_AFTER_AI_DRIVEN_DELAY) return true;  // flt_820C4154 == 3.0
        return false;
    }

    // ================================================================================
    // ChooseAggressiveSteeringFan @0x82766150
    //
    // The bias-mode ladder for an AGGRESSIVE route-finding style (ROAD_RAGE / MARKED_MAN). Every
    // "% 2" / "% 3" below is the console's own opponent-index stagger, which keeps the pack from
    // all attacking on the same frame (the asm spells them as the mulhw/subfe reciprocal idioms).
    // ================================================================================
    s32 AIDriver::ChooseAggressiveSteeringFan(AICar* lpPlayerCar)
    {
        CGS_ASSERT_AT(lpPlayerCar != 0, "lpPlayerCar", __FILE__, 493);                     // :493

        // The player is crashing and this car drew an odd opponent index -> pile in (bias 2).
        if (lpPlayerCar->IsCrashing() && (mpCar->miOpponentIndex % 2) != 0)
        {
            return 2;
        }

        if (IsPlayerProtected(lpPlayerCar))
        {
            if (mpCar->mfBuzzDistanceToPlayer < 10.0f)  // flt_820C4150 == 10
                return 0;
            if (mpCar->meRelativeLocation == E_RELATIVE_BEHIND_APPROACHING)       // lwz 0x14D4 == 0
                return 0;
        }

        // 0x82766200: behind-approaching (0) or in-front-approaching (2) -> consider a spurt.
        const s32 liRelative = static_cast<s32>(mpCar->meRelativeLocation);
        if (liRelative == 0 || liRelative == 2)
        {
            bool lbSpurt = false;
            // 0x82766220 `fcmpu slowTime, 6.0 ; blt -> skip`: taken only on an ordered <, so a
            // NaN slow-speed time is considered (FX-AINAN2; `>=` skipped it).
            if (!(mfPlayerSlowSpeedTime < 6.0f))        // flt_820C4250 == 6.0
            {
                const f32 lfMySpeed = mpCar->GetSpeed();
                if (lfMySpeed > lpPlayerCar->GetSpeed() + KF_BEHIND_SHUNT_MIN_SPEED_DIFFERENCE)  // flt_8300D934 == 10 mph
                    lbSpurt = true;
            }
            if (!lbSpurt
                && mpCar->GetRouteFindingStyle() == static_cast<ERouteFindingStyle>(6)
                && lpPlayerCar->GetSpeed() < KF_BEHIND_SHUNT_MARKED_MAN_MIN_SPEED)       // flt_8300D774 == 90 mph
            {
                lbSpurt = true;
            }
            if (lbSpurt)
            {
                if (mpCar->mfBuzzDistanceToPlayer < 10.0f)
                    return 7;
                return 2;
            }
        }

        // 0x8276629C: only the in-front-separating case (3) gets the distance-staggered fans.
        const s32 liRelative2 = static_cast<s32>(mpCar->meRelativeLocation);
        if (liRelative2 == 2 || liRelative2 != 3)
        {
            return 0;
        }

        const f32 lfDistance = mpCar->mfBuzzDistanceToPlayer;
        if (!(lfDistance > 5.0f))                          // flt_820C488C == 5
            return 0;
        if (mpCar->GetRouteFindingStyle() == static_cast<ERouteFindingStyle>(6))
            return 0;
        if (lfDistance > 100.0f)                             // flt_820C3FAC == 100
            return 0;

        const s32 liOpponent = static_cast<s32>(mpCar->miOpponentIndex);
        if (lfDistance > 15.0f)                             // flt_820C4238 == 15
            return ((liOpponent % 3) == 0) ? 0 : 5;                                  // andi. r3, r11, 5
        return ((liOpponent % 2) == 0) ? 0 : 6;                                      // rlwinm r3, r11, 0,29,30
    }

    // ================================================================================
    // UpdateStuck @0x82766440
    //
    // Accumulate mfStuckTime while the car is crawling; once it has been stuck for 2 s (and it is
    // not already doing a slow turn, and its behaviour is not 0) push the car into behaviour 6
    // (SLOW TURN). The accumulate arm is skipped only for a DEFAULT-style (style 0) car that is
    // already in behaviour 6.
    //
    // THIS IS THE ONLY WAY INTO SLOW_TURN FOR A CAR WITH NO ROUTE. The other producer,
    // Determine180Turn @0x8277C758, opens with ComputeRouteDirection @0x82766500 and returns
    // early when the route has no nodes (asm 0x8277C788), so with `route 0` it can never fire.
    // Run6 (scratch/aiwave/run6/BrnGame.log): rival slot 1 entered FIGHTING at t~4.6 s at
    // 2.3 m/s, never passed 4.12 m/s (KF_MAX_SPEED_FOR_BEING_STUCK is 4.4704 m/s == 10 mph), so
    // mfStuckTime ran uninterrupted and this arm fired 2 s later, exactly as the console would.
    // With a real desired speed the car clears 10 mph inside the first second and mfStuckTime is
    // reset to 0 every frame, so the console never reaches this arm off a standing start.
    // ================================================================================
    void AIDriver::UpdateStuck(f32 lfTimeStep)
    {
        AICar* lpCar = mpCar;
        const f32 lfSpeed = lpCar->GetSpeed();
        const s32 liBehaviour = static_cast<s32>(lpCar->meBehaviour);     // lwz 0x14B4

        // 0x82766494 `fcmpu stuckTime, 2.0 ; blt -> skip` and 0x827664C8 `fcmpu speed, 10 mph ;
        // bge -> reset`: blt is not taken on unordered and bge is, so a NaN stuck time turns the
        // car and a NaN speed resets the timer (FX-AINAN2; both `>=` did the opposite).
        if (liBehaviour != 6 && liBehaviour != 0 && !(mfStuckTime < KF_AI_TIME_TO_START_TURNING))  // 2.0
        {
            lpCar->mfBehaviourTimer    = 0.0f;                             // stfs 0x14E0
            lpCar->mePreviousBehaviour = static_cast<EAIBehaviour>(liBehaviour);  // stw 0x14B8
            lpCar->meBehaviour         = static_cast<EAIBehaviour>(6);      // stw 0x14B4, 6
        }

        if (lpCar->GetRouteFindingStyle() != static_cast<ERouteFindingStyle>(0) || liBehaviour != 6)
        {
            // The accumulate arm comes first (ARTIST 0x827664CC bge -> the reset placed after it).
            if (lfSpeed < KF_MAX_SPEED_FOR_BEING_STUCK)                    // flt_8300D818 == 10 mph
                mfStuckTime = mfStuckTime + lfTimeStep;
            else
                mfStuckTime = 0.0f;                                        // 0x1D04
        }
    }

    // ================================================================================
    // IsStuck @0x82766670
    //
    // Has this car been crawling long enough to be reported stuck? Never in the first 10 s of a
    // race for a non-default route finder, never while inactive, and the threshold is 0.75 s for
    // the (AI-shadowed) PLAYER car and 5 s for everyone else.
    // ================================================================================
    bool AIDriver::IsStuck()
    {
        CGS_ASSERT_AT(mpCar != 0, "mpCar != NULL", __FILE__, 2519);                       // :2519

        AICar* lpCar = mpCar;
        f32 lfThreshold = KF_AI_TIME_FOR_BEING_STUCK;                      // 5.0

        if (lpCar->GetRouteFindingStyle() != static_cast<ERouteFindingStyle>(0)
            && lpCar->mfRaceTimer < KF_RACE_STUCK_FREE_TIME)               // flt_820C4150 == 10.0
        {
            return false;
        }

        if (lpCar->IsPlayerCar() && !lpCar->mbIsDrivenByPlayer)
            lfThreshold = KF_PLAYER_TIME_FOR_BEING_STUCK;                  // flt_820C41FC == 0.75

        if (!mbIsActive)                                                   // lbz 0x1D69
            return false;

        return mfStuckTime > lfThreshold;
    }

    // ================================================================================
    // CheckForBoosting @0x827705E0
    //
    // Three arms:
    //   * driving a FORCED speed  -> boost when already above 150 mph, or when the forced speed
    //                                is more than 5 mph above the current speed;
    //   * below the desired speed and above 40 mph -> boost when the desired speed is above
    //                                130 mph, or the gap to it is more than 50 mph;
    //   * otherwise -> boost only while the aggression machine is SPURT_FORWARD (state 12).
    // ================================================================================
    bool AIDriver::CheckForBoosting()
    {
        const f32 lfSpeed = mpCar->GetSpeed();

        if (mbUseForcedSpeed)                                              // lbz 0x1D66
        {
            return lfSpeed > KF_FORCED_BOOST_SPEED                          // flt_8300D7A4 == 150 mph
                || mfForcedSpeed > (KF_BOOST_TO_CLOSE_SPEED_GAP + lfSpeed); // flt_8300DB20 == 5 mph
        }

        const f32 lfDesired = mfDesiredSpeed;                              // 0x1D10
        if (lfSpeed < lfDesired && lfSpeed > KF_NO_BOOST_SPEED)            // flt_8300D81C == 40 mph
        {
            return lfDesired > KF_ALWAYS_BOOST_SPEED                        // flt_8300D750 == 130 mph
                || lfSpeed < (mfDesiredSpeed - KF_BOOST_UP_TO_DESIRED_SPEED_OFFSET);  // flt_8300DB24 == 50 mph
        }

        // lwz 0x1C00(this) == mAggression.meAggressionState
        return GetAggression()->GetAggressionState() == E_AI_AGGRESSION_STATE_SPURT_FORWARD;
    }

    // ================================================================================
    // CheckForSpeedMatch @0x82793020
    //
    // When the car has a speed-selection method AND the aggression machine is speed-matching
    // (mAggression.meSpeedMatchType != Disabled, driver+0x1C58), take the matched speed but never
    // let it fall below this car's own "decent" speed, publish it as mfForcedSpeed and -- only
    // while the desired speed is below the car's top speed -- also drive the desired speed with
    // it. mbUseForcedSpeed records the answer.
    // ================================================================================
    bool AIDriver::CheckForSpeedMatch(f32 lfTimeStep)
    {
        u8 lbUseForced = 0;

        if (mpCar->meSpeedSelectionMethod != 0                       // lwz 0x14BC(car)
            && GetAggression()->IsSpeedMatchingType())                       // lwz 0x1C58 != 0
        {
            // X360 @0x82793058: f1 = mfDesiredSpeed, f2 = dt (the FIRST float slot carries the
            // target speed, the second the time step -- read off the register set-up, not the
            // DWARF parameter names).
            const f32 lfMatched = GetAggression()->CalcSpeedMatchSpeed(mfDesiredSpeed, lfTimeStep);
            const f32 lfDecent  = mpCar->GetDecentSpeed();
            const bool lbBelowTop = (mfDesiredSpeed < mfTopSpeed);       // 0x1D10 < 0x1D14

            // fsel f0, (decent - matched), decent, matched == max(decent, matched)
            const f32 lfSpeed = ((lfDecent - lfMatched) >= 0.0f) ? lfDecent : lfMatched;
            mfForcedSpeed = lfSpeed;                                     // 0x1D18
            if (lbBelowTop)
                mfDesiredSpeed = lfSpeed;                                // 0x1D10
            lbUseForced = 1;
        }

        mbUseForcedSpeed = lbUseForced;                                  // stb 0x1D66
        return lbUseForced != 0;
    }

    // ================================================================================
    // CalculateDesiredSpeed @0x827934C0 .. 0x82793560  (NO IDA export -- image bytes)
    //
    // The speed the controls chase this frame. Start at the car's own desired speed
    // (AICar::mfSpeedOutOfRange, the CalcDesiredSpeed result), cap it by the three limiters in
    // turn (hard shoulder, proximity, cornering) and finally scale by how straight the chosen
    // steering-fan ray is:
    //   mfTopSpeed      = mpCar->mfSpeedOutOfRange
    //   f31 = HardShoulderSpeed(mfTopSpeed) ; f30 = ProximitySpeed(mfTopSpeed)
    //   f1  = CorneringTopSpeed(mfTopSpeed)
    //   mfDesiredSpeed  = min(f1, f31)                      (fsel @0x82793514)
    //   mfDesiredSpeed  = min(mfDesiredSpeed, f30)          (fsel @0x82793520)
    //   mfDesiredSpeed *= mSteeringFan.GetSpeedRatio() * 0.75 + 0.25
    // Neither parameter is read by the console body (the caller sets them up because the DWARF
    // signature is CalculateDesiredSpeed(Vector3, bool)).
    // ================================================================================
    void AIDriver::CalculateDesiredSpeed(Vector3 lPlayerCarPosition, bool lbDoInRangeCatchup)
    {
        (void)lPlayerCarPosition;
        (void)lbDoInRangeCatchup;

        mfTopSpeed = mpCar->GetDesiredSpeed();            // lfs 0x14E8(car) -> stfs 0x1D14

        // DWARF (_compile/BrnWorldAIUnity.cpp) locals :2488/:2491/:2494/:2500 and two
        // rw::math::fpu::Min<float> calls (fsel 0x82793514 / 0x82793520; the console stores
        // mfDesiredSpeed after each, 0x82793518 / 0x82793524).
        const f32 lfHardShoulderTopSpeed = HardShoulderSpeed(mfTopSpeed);
        const f32 lfProximityTopSpeed    = ProximitySpeed(mfTopSpeed);
        const f32 lfCorneringTopSpeed    = CorneringTopSpeed(mfTopSpeed);

        mfDesiredSpeed = rw::math::fpu::Min(lfCorneringTopSpeed, lfHardShoulderTopSpeed);
        mfDesiredSpeed = rw::math::fpu::Min(mfDesiredSpeed, lfProximityTopSpeed);

        const f32 lfSteeringFan = mSteeringFan.GetSpeedRatio();     // @0x82779B90 (r3 = this + 0x710)
        mfDesiredSpeed = (lfSteeringFan * 0.75f + 0.25f) * mfDesiredSpeed;
    }

    // ================================================================================
    // HardShoulderSpeed @0x827930B8
    //
    // Slow to 75 % of the input speed as the car drifts onto the hard shoulder: t = clamp(
    // (roadPercentage - 0.85) * 10.000004, 0, 1); return lerp(inputSpeed, inputSpeed * 0.75, t).
    // ================================================================================
    f32 AIDriver::HardShoulderSpeed(f32 lfInputSpeed)
    {
        // fmsubs f12, f31, 0.75, f31 -- the lerp's (target - source) term, computed before the call.
        const f32 lfDelta = lfInputSpeed * KF_AI_HARD_SHOULDER_PROPORTION - lfInputSpeed;

        const f32 lfRoadPercentage =
            mRacingLineGenerator.GetRoadPositionAsPercentage(&GetRacingLine(), mpCar);

        const f32 lfRamp = SaturateU((lfRoadPercentage - K_MIN_HARD_SHOULDER)   // 0.85 (0x820C7EC8)
                                     / (K_MAX_HARD_SHOULDER - K_MIN_HARD_SHOULDER));   // folded: * 10.000004 (0x820C822C)
        return lfRamp * lfDelta + lfInputSpeed;                                    // fmadds
    }

    // ================================================================================
    // DoDrivingBehaviour @0x82799660 .. 0x827998BC  (NO IDA export -- image bytes)
    //
    // The drift state machine that runs under behaviours 1-4 and 10. Always steers first, then
    // dispatches on meDriftState (0x1CEC):
    //   0 NORMAL_DRIVING: speed-match + drive at the desired speed; if the car IS drifting go to
    //                     DRIFTING; if a drift is wanted go to START_DRIFT; then Determine180Turn.
    //   1 START_DRIFT   : same speed work; car drifting -> DRIFTING; drift no longer wanted ->
    //                     NORMAL_DRIVING; else StartDrift().
    //   2 DRIFTING      : latch mbWantToEnterDrift, drive-at-speed-in-drift, drop to
    //                     NORMAL_DRIVING if the car stopped drifting, and go to EXIT_DRIFTING
    //                     once the drift steering angle falls under 4 degrees.
    //   3 EXIT_DRIFTING : clear every control + drift flag, raise mbWantToExitDrift, and return
    //                     to NORMAL_DRIVING once the car has stopped drifting.
    //   default         : assert "AI Driver in bad Drift state" (:1817).
    // ================================================================================
    void AIDriver::DoDrivingBehaviour(f32 lfTimeStep)
    {
        CalculateSteeringAngle(lfTimeStep);                       // 0x82799678

        switch (GetDriftState())                                  // lwz 0x1CEC
        {
            case E_DRIFT_STATE_NORMAL_DRIVING:                    // 0x827996B0
                CheckForSpeedMatch(lfTimeStep);
                AttemptToDriveAtDesiredSpeed(lfTimeStep);
                if (mpCar->mbIsDrifting)                      // lbz 0x1544
                    SetDriftState(E_DRIFT_STATE_DRIFTING);
                if (EstimateNeedForDrifting())
                    SetDriftState(E_DRIFT_STATE_START_DRIFT);
                Determine180Turn();
                break;

            case E_DRIFT_STATE_START_DRIFT:                       // 0x82799710
                CheckForSpeedMatch(lfTimeStep);
                AttemptToDriveAtDesiredSpeed(lfTimeStep);
                if (mpCar->mbIsDrifting)
                {
                    SetDriftState(E_DRIFT_STATE_DRIFTING);
                    break;
                }
                if (!EstimateNeedForDrifting())
                {
                    SetDriftState(E_DRIFT_STATE_NORMAL_DRIVING);
                    break;
                }
                StartDrift();
                break;

            case E_DRIFT_STATE_DRIFTING:                          // 0x82799784
            {
                mbWantToEnterDrift = 1;                           // stb 1, 0x1D65
                mbWantToExitDrift  = 0;                           // stb 0, 0x1D64
                AttemptToDriveAtDesiredSpeedInDrift();
                if (!mpCar->mbIsDrifting)
                    SetDriftState(E_DRIFT_STATE_NORMAL_DRIVING);

                const f32 lfDriftAngle =
                    DetermineDriftSteeringAngle(E_DRIFT_DIRECTION_SELECTION_CAR_MOVING);   // r4 = 0
                if (std::fabs(lfDriftAngle) < KF_MAX_DRIFT_UNNECESSARY_ANGLE)              // flt_8300D7AC
                    SetDriftState(E_DRIFT_STATE_EXIT_DRIFTING);
                break;
            }

            case E_DRIFT_STATE_EXIT_DRIFTING:                     // 0x827997F4
                mfBrake            = 0.0f;   // 0x1D2C
                mbUseForcedSpeed   = 0;      // 0x1D66
                mfAccelerator      = 0.0f;   // 0x1D28
                mbBoosting         = 0;      // 0x1D6B
                mfHandBrake        = 0.0f;   // 0x1D30
                mbDriftingRequired = 0;      // 0x1D67
                mfAngleForDrift    = 0.0f;   // 0x1D4C
                mbWantToEnterDrift = 0;      // 0x1D65
                mbWantToExitDrift  = 1;      // 0x1D64
                if (!mpCar->mbIsDrifting)
                    SetDriftState(E_DRIFT_STATE_NORMAL_DRIVING);
                break;

            default:
                CGS_FAIL("AI Driver in bad Drift state");   // :1817
                break;
        }
    }

    // ================================================================================
    // EstimateNeedForDrifting @0x82793438
    //
    // Below 30 mph never drift. Otherwise measure the drift steering angle from the car's MOVING
    // direction, cache it in mfAngleForDrift, and want a drift once |angle| exceeds 0.9 rad.
    // ================================================================================
    bool AIDriver::EstimateNeedForDrifting()
    {
        if (mpCar->GetSpeed() < K_MIN_DRIFT_SPEED)              // flt_8300DBE4 == 30 mph
            return false;

        const f32 lfAngle = DetermineDriftSteeringAngle(E_DRIFT_DIRECTION_SELECTION_CAR_MOVING);
        mfAngleForDrift = lfAngle;                                  // stfs 0x1D4C
        return std::fabs(lfAngle) > K_DRIFT_TRIGGER;         // 0.89999998
    }

    // ================================================================================
    // DetermineDriftSteeringAngle @0x827931D0
    //
    // Signed planar angle from the car's chosen direction (velocity for CAR_MOVING, facing for
    // CAR_FACING) to the racing line's final drift direction. Every early-out returns an
    // EXPLICIT 0.0 (loc_827931F4: `lfs f1, flt_82001CC0`):
    //   0x827931E8  racing line not initialised (lbz 0x1AF0)             -> 0.0
    //   0x8279322C  CAR_MOVING: lFrom = (vel.x, vel.z) (vrlimi128 8,0 / 4,1); when BOTH lanes fail
    //               |lane| > flt_820C3B70 (vandc sign mask, vcmpgtfp, 0x82793270..0x827932E0) the
    //               `b 0x827932F8` at 0x827932E4 takes the FACING instead
    //   0x827932F8  CAR_FACING (and that fallback): lFrom = (dir.x, dir.z) from GetDirection
    //   0x82793310..0x827933AC  the SAME per-lane test on the chosen vector, both selections:
    //               degenerate -> `bne loc_827931F4` -> 0.0, BEFORE FindFinalDriftDirection
    //   0x827933F8  FindFinalDriftDirection(this+0x1C70 == mSteeringTargetVector) false -> 0.0
    //   0x82793420  FindSignedAngleBetween2DVectors(normalised lFrom, mSteeringTargetVector)
    // (The rsqrt normalise is split around the FindFinalDriftDirection call on the console; it is
    // a pure function of lFrom, so it is written once here.) `!(|x| > eps)` keeps vcmpgtfp's NaN
    // lanes on the degenerate side.
    // ================================================================================
    f32 AIDriver::DetermineDriftSteeringAngle(EDriftDirectionSelection leSelection)
    {
        if (!GetRacingLine().mbIsInitialised)                       // lbz 0x1AF0
            return 0.0f;                                            // loc_827931F4

        Vector2 lFrom;
        if (leSelection == E_DRIFT_DIRECTION_SELECTION_CAR_MOVING)  // cmpwi r4,0
        {
            lFrom = To2DU(mpCar->GetVelocity());                // @0x8276B570
            if (!(std::fabs(lFrom.mV.x) > 1.1920928955078125e-07f) &&
                !(std::fabs(lFrom.mV.y) > 1.1920928955078125e-07f))
                lFrom = To2DU(mpCar->GetDirection());           // b 0x827932F8 -> @0x8276B488
        }
        else
            lFrom = To2DU(mpCar->GetDirection());               // @0x8276B488

        if (!(std::fabs(lFrom.mV.x) > 1.1920928955078125e-07f) &&
            !(std::fabs(lFrom.mV.y) > 1.1920928955078125e-07f))
            return 0.0f;                                            // bne loc_827931F4 @0x827933AC
        lFrom = Normalize2DU(lFrom);

        // The out slot the X360 hands FindFinalDriftDirection is this+7280 == mSteeringTargetVector.
        if (!FindFinalDriftDirection(mSteeringTargetVector))
            return 0.0f;

        return FindSignedAngleBetween2DVectors(lFrom, mSteeringTargetVector);
    }

    // ================================================================================
    // FindFinalDriftDirection @0x82793138
    //
    // Ask the racing-line generator for a point (speed * 1.1) metres ahead, floored at 10 m,
    // measured from m2DCarPos (this+7376).
    // ================================================================================
    bool AIDriver::FindFinalDriftDirection(Vector2& lrOutDirection)
    {
        f32 lfLookAhead = mpCar->GetSpeed() * KF_TIME_LOOK_AHEAD_FOR_DRIFT;   // * 1.1
        if (lfLookAhead < KF_MIN_DISTANT_LOOK_AHEAD_FOR_DRIFT)                                   // flt_820C4150 == 10
            lfLookAhead = KF_MIN_DISTANT_LOOK_AHEAD_FOR_DRIFT;

        // DWARF/X360 argument order (CachePointAhead @0x82791424..44 is the attesting call site):
        // (lpRacingLine r4, lfDistanceAhead f1, lFrom2D v1, lrOutPosition r6, lrOutDirection r7).
        Vector2 lPosition;
        return mRacingLineGenerator.GetPointFarAhead(&GetRacingLine(), lfLookAhead, m2DCarPos,
                                                     lPosition, lrOutDirection);
    }

    // ================================================================================
    // FindPositionInFuture @0x82792F80
    //
    // Where the car will be lfTime seconds from now, clamped into [lfMinDistance, lfMaxDistance]:
    //   d = clamp(speed * lfTime, lfMinDistance, lfMaxDistance)   (the two fsel pairs)
    // then hand that distance and m2DCarPos (this+7376) to the racing-line generator. Returns
    // false outright when the racing line is not initialised (lbz 0x1AF0).
    // ================================================================================
    bool AIDriver::FindPositionInFuture(Vector2& lrOutPosition, Vector2& lrOutDirection,
                                        f32 lfTime, f32 lfMaxDistance, f32 lfMinDistance)
    {
        if (!GetRacingLine().mbIsInitialised)                       // lbz 0x1AF0 (this+6896)
            return false;

        const f32 lfSpeed = mpCar->GetSpeed();
        f32 lfDistance = lfSpeed * lfTime;
        // fpu::Clamp<float> (DWARF), spelt as the two fsels because fsel picks its THIRD operand
        // on an unordered test (f30 = lfMinDistance, f31 = lfMaxDistance):
        // 0x82792FFC fsel f0, (min - dist), min, dist  == max(dist, min)  -- NaN keeps the NaN
        lfDistance = ((lfMinDistance - lfDistance) >= 0.0f) ? lfMinDistance : lfDistance;
        // 0x82793004 fsel f1, (max - dist), dist, max  == min(dist, max)  -- NaN becomes max
        // (FX-AINAN2: the old `if ((max - dist) < 0) dist = max` kept a NaN distance)
        lfDistance = ((lfMaxDistance - lfDistance) >= 0.0f) ? lfDistance : lfMaxDistance;

        return mRacingLineGenerator.GetPointFarAhead(&GetRacingLine(), lfDistance, m2DCarPos,
                                                     lrOutPosition, lrOutDirection);
    }

    // ================================================================================
    // StartDrift @0x8277C870
    //
    // Enter a drift: full brake, latch the enter flag, steer hard toward the side the cached
    // drift angle points at (fsel on mfAngleForDrift -> +-1), release the hand brake, then full
    // throttle. NOTE the store order -- the accelerator is written AFTER UpdateSteeringAngle.
    // ================================================================================
    void AIDriver::StartDrift()
    {
        mfBrake            = 1.0f;   // stfs flt_82001C98, 0x1D2C
        mbWantToEnterDrift = 1;      // stb  1, 0x1D65
        mbWantToExitDrift  = 0;      // stb  0, 0x1D64

        mfHandBrake = 0.0f;          // stfs flt_82001CC0, 0x1D30
        // DWARF BrnAIDriver.cpp:1450: rw::math::fpu::SgnNonZero<float> (fsel f1, a, 1.0, -1.0).
        UpdateSteeringAngle(rw::math::fpu::SgnNonZero(mfAngleForDrift));
        mfAccelerator = 1.0f;        // stfs flt_82001C98, 0x1D28
    }

    // ================================================================================
    // AttemptToDriveAtDesiredSpeedInDrift @0x8277C8E0
    //
    // While drifting the throttle is binary: no forced speed, no boost, and full throttle while
    // the fan's speed ratio says the line ahead is straight enough (>= 0.75), else full brake.
    // ================================================================================
    void AIDriver::AttemptToDriveAtDesiredSpeedInDrift()
    {
        mbUseForcedSpeed = 0;        // stb 0, 0x1D66
        mbBoosting       = 0;        // stb 0, 0x1D6B

        // 0x8277C90C `fcmpu ratio, flt_820C41D4 (0.75) ; bge` -> the full-throttle arm; bge is taken
        // on >= AND on unordered (FX-AINAN2), so a NaN ratio takes the else arm. The brake arm is
        // the if-arm: ARTIST falls through into it (0x8277C914).
        if (mSteeringFan.GetSpeedRatio() < KF_BRAKE_DURING_DRIFT)  // flt_820C41D4 == 0.75
        {
            mfBrake       = 1.0f;    // 0x1D2C
            mfAccelerator = 0.0f;    // 0x1D28
        }
        else
        {
            mfBrake       = 0.0f;
            mfAccelerator = 1.0f;
        }
    }

    // ================================================================================
    // Determine180Turn @0x8277C758
    //
    // When the route doubles back on the car (dot(routeDirection, carFacing) < -0.5) push it into
    // a QUICK TURN (behaviour 5, above 30 mph, with the steering lock GetQuickTurnSteering picks)
    // or a SLOW TURN (behaviour 6, below it). The behaviour is written directly, NOT through
    // AICar::SetBehaviour (the console stores mePreviousBehaviour / meBehaviour / the timer here).
    // ================================================================================
    void AIDriver::Determine180Turn()
    {
        Vector2 lRouteDirection;
        if (!ComputeRouteDirection(lRouteDirection))
            return;

        const Vector2 lFacing = To2DU(mpCar->GetDirection());       // GetDirection + Flatten
        const f32 lfDot = Dot2D(lRouteDirection, lFacing);

        // vcmpgtfp v13(-KF_TRIGGER_TURN), v0(dot) -> -K > dot
        if (!(-KF_TRIGGER_TURN > lfDot))                                // flt_8300D74C == cos(60 deg)
            return;

        AICar* lpCar = mpCar;
        s32 liNewBehaviour;
        if (lpCar->GetSpeed() >= KF_QUICKTURN_SLOWNESS_DROPIN)          // flt_8300D960 == 30 mph
        {
            mQuickTurnSteeringLock = GetQuickTurnSteering(lRouteDirection);   // stfs 0x1CF8
            liNewBehaviour = 5;
        }
        else
        {
            liNewBehaviour = 6;
        }

        const s32 liOldBehaviour = static_cast<s32>(lpCar->meBehaviour);
        lpCar->mePreviousBehaviour = lpCar->meBehaviour;                // stw 0x14B8
        lpCar->meBehaviour         = static_cast<EAIBehaviour>(liNewBehaviour);   // stw 0x14B4
        lpCar->mfBehaviourTimer    = 0.0f;                              // stfs 0x14E0
    }

    // ================================================================================
    // GetQuickTurnSteering @0x8277C600
    //
    // Which way to spin the wheel for a 180. Returns +1 or -1 (fsel on flt_82001C98 / flt_820037C8).
    // With the centre line under the car known, the sign is the 2D cross of the car's normalised
    // facing with (mRacingLine.mCentreHere - carPosition2D); otherwise it is the sign of the
    // signed angle from that facing to lVectorToTarget.
    // ================================================================================
    f32 AIDriver::GetQuickTurnSteering(Vector2 lVectorToTarget)
    {
        const Vector3 lDirection3 = mpCar->GetDirection();
        Vector2 lDirection2;
        lDirection2.mV = __vrlimi(lDirection3.mV, lDirection3.mV, 4, 1);
        const Vector2 lFacing = Normalize2DU(lDirection2);

        f32 lfSelector;
        if (GetRacingLine().mbCentreLineHereKnown)                      // lbz 0x1AF1 (this+6897)
        {
            const Vector3 lCarPos = mpCar->GetPosition();
            const __vector4 position = __vrlimi(lCarPos.mV, lCarPos.mV, 4, 1);
            const __vector4 delta = __vsubfp(GetRacingLine().mCentreHere.mV, position);
            const __vector4 left = __vmulfp(__vspltw(lFacing.mV, 0), __vspltw(delta, 1));
            const __vector4 right = __vmulfp(__vspltw(lFacing.mV, 1), __vspltw(delta, 0));
            lfSelector = __vsubfp(left, right).x; // ARTIST 0x8277C6DC..F4
        }
        else
        {
            lfSelector = FindSignedAngleBetween2DVectors(lFacing, lVectorToTarget);
        }

        return (lfSelector >= 0.0f) ? 1.0f : -1.0f;
    }

    // ================================================================================
    // UpdateQuickTurn @0x8278B100
    //
    // Leave the quick turn once the car's normalised USEFUL direction points at the driving
    // target (dot > cos(30 deg)) or the car has fallen below 10 mph. NOTE the dot is taken
    // against the UNNORMALISED (target - carPosition) delta -- that is the console's own maths.
    // ================================================================================
    void AIDriver::UpdateQuickTurn()
    {
        const Vector3 lUseful3 = mpCar->GetUsefulDirection();
        Vector2 lUseful2;
        lUseful2.mV = __vrlimi(lUseful3.mV, lUseful3.mV, 4, 1);
        const Vector2 lUseful = Normalize2DU(lUseful2);
        const Vector2 lTarget = GetTargetPosition();
        const Vector3 lCarPos = mpCar->GetPosition();
        __vector4 lFlat = __vrlimi(lTarget.mV, lCarPos.mV, 8, 0);
        lFlat = __vrlimi(lFlat, lCarPos.mV, 4, 1);
        Vector2 lDelta;
        lDelta.mV = __vsubfp(lTarget.mV, lFlat); // ARTIST 0x8278B1A8..C0

        const f32 lfDot = Dot2D(lUseful, lDelta);

        if (lfDot > KF_QUICKTURN_DROP_OUT                                // flt_8300DC00 == cos(30 deg)
            || mpCar->GetSpeed() < KF_QUICKTURN_SLOWNESS_DROPOUT)    // flt_8300D6FC == 10 mph
        {
            CGS_ASSERT_AT(mpCar->mePreviousBehaviour != static_cast<EAIBehaviour>(5),
                       "Nested quick turns!\n", CGS_ARTIST_UNITY_BRNAIDRIVER_CPP_FILE, 1151);                          // :1151
            const s32 liOldBehaviour = static_cast<s32>(mpCar->meBehaviour);
            const s32 liNewBehaviour = static_cast<s32>(mpCar->mePreviousBehaviour);
            mpCar->SetBehaviour(mpCar->mePreviousBehaviour);      // @0x82764DE0
        }
    }

    // ================================================================================
    // DoSlowTurnBehaviour @0x8277C968
    //
    // Leave the slow turn once the car's facing has come back within 20 degrees of the route
    // direction: restore behaviour 3 (CRUISING), clear the stuck timer and centre the wheel.
    //
    // THE EXIT IS GATED ON THE ROUTE. asm 0x8277C97C..0x8277C988: ComputeRouteDirection first,
    // and on false the body branches to loc_8277CA7C -- the epilogue -- WITHOUT testing
    // KF_SLOW_TURN_DROP_OUT. So a car that is put into SLOW_TURN while its route has no nodes
    // stays in SLOW_TURN for ever; there is no timer, no drop-out angle and no speed test that
    // can rescue it. That is console behaviour, not a host deviation: on the console a routeless
    // stuck car is recovered from OUTSIDE the driver, by AIModule::ProcessRequestInterface
    // @0x8278A7A8's 8-slot sweep, which sees AIDriver::IsStuck() and pushes a synthetic
    // ResetOnTrackRequest, then zeroes mfStuckTime (asm 0x8278A86C..0x8278A92C).
    // ================================================================================
    void AIDriver::DoSlowTurnBehaviour()
    {
        Vector2 lRouteDirection;
        if (!ComputeRouteDirection(lRouteDirection))
            return;

        AICar* lpCar = mpCar;
        const Vector2 lFacing = To2DU(lpCar->GetDirection());          // GetDirection + Flatten
        const f32 lfDot = Dot2D(lRouteDirection, lFacing);

        // vcmpgtfp v0(dot), v13(KF_SLOW_TURN_DROP_OUT)
        if (!(lfDot > KF_SLOW_TURN_DROP_OUT))                          // flt_8300D710 == cos(20 deg)
            return;

        CGS_ASSERT(lpCar->mePreviousBehaviour != static_cast<EAIBehaviour>(6),
                   "Nested slow turns!\n");                            // :1866

        const s32 liOldBehaviour = static_cast<s32>(lpCar->meBehaviour);
        lpCar->mePreviousBehaviour = lpCar->meBehaviour;               // stw 0x14B8
        lpCar->mfBehaviourTimer    = 0.0f;                             // stfs 0x14E0
        lpCar->meBehaviour         = static_cast<EAIBehaviour>(3);     // stw 0x14B4, 3
        mfStuckTime                = 0.0f;                             // stfs 0x1D04
        UpdateSteeringAngle(0.0f);
    }

    // ================================================================================
    // DoSlowTurn @0x8277CA88
    //
    // The behaviour-6 controls: steer AWAY from the side the route is on (the sign of
    // dot(routeDirection, carRight), negated by the vxor with the sign mask), then alternate
    // between a throttle phase and a brake phase every 3 seconds of behaviour time:
    //   phase = (s32)(mpCar->mfBehaviourTimer * 0.33333334)
    //   phase even -> mfAccelerator = 1 - clamp( speed / 20 mph, 0, 1)
    //   phase odd  -> mfBrake       = 1 - clamp(-speed / 20 mph, 0, 1)   (the console negates)
    // With no usable route the wheel is simply centred.
    //
    // NOTE the no-route arm (asm 0x8277CBC4) calls UpdateSteeringAngle(0.0f) and jumps to the
    // epilogue: it never reaches either throttle phase, so mfAccelerator and mfBrake keep the
    // zeroes CalculateCarControls wrote at its head. A routeless car in SLOW_TURN therefore
    // reports gas 0 / brake 0 / steer 0 every frame and coasts to a stop -- run6's `behaviour 6
    // ... gas 0.000000 ... speed 0.076035` line, reproduced exactly.
    // ================================================================================
    void AIDriver::DoSlowTurn(f32 lfTimeStep)
    {
        (void)lfTimeStep;   // the console reads only mpCar->mfBehaviourTimer here

        AICar* lpCar = mpCar;

        Vector2 lRouteDirection;
        if (!ComputeRouteDirection(lRouteDirection))
        {
            UpdateSteeringAngle(0.0f);                                 // 0x8277CBC4
            return;
        }

        const Vector2 lRight = To2DU(lpCar->GetRight());               // GetRight + Flatten
        const f32 lfDot = Dot2D(lRouteDirection, lRight);
        // rw::math::fpu::SgnNonZero -> +-1, then vxor with the 0x80000000 sign mask == negate.
        const f32 lfSteer = -((lfDot >= 0.0f) ? 1.0f : -1.0f);
        UpdateSteeringAngle(lfSteer);

        const s32 liPhase = static_cast<s32>(lpCar->mfBehaviourTimer * 0.33333334f);
        if ((liPhase % 2) == 0)
        {
            const f32 lfT = SaturateU(lpCar->GetSpeed() / KF_SLOW_TURN_SPEED);      // flt_8300D718
            mfAccelerator = 1.0f - lfT;                                             // stfs 0x1D28
        }
        else
        {
            const f32 lfT = SaturateU(-(lpCar->GetSpeed() / KF_SLOW_TURN_SPEED));   // the console's fneg
            mfBrake = 1.0f - lfT;                                                   // stfs 0x1D2C
        }
    }

    // ================================================================================
    // InitialiseRacingLine @0x82792DF0
    //
    // Bring the racing line up for the car's CURRENT route. Clears the three "line is up" flags
    // and the last-known section, then (only with a usable route) drops the section cache when
    // the route time stamp moved, zeroes the four steering/braking 2D vectors, picks the HNG
    // spread distance (25 m while FIGHTING with a valid aggression target, else 50 m), asks the
    // generator to build the line and raises mbIsRacingLineInitialised.
    // ================================================================================
    void AIDriver::InitialiseRacingLine()
    {
        AICar* lpCar = mpCar;

        mbIsRacingLineInitialised = 0;                        // stb 0, 0x1D68
        mbCurrentRouteComplete    = 0;                        // stb 0, 0x1D6A
        GetRacingLine().mbIsInitialised = false;              // stb 0, 0x1AF0
        GetRacingLine().miLastKnownSectionID = 0x7FFF;        // stw 0x7FFF, 0x1B20 (RacingLine +0xC00)

        const Route* lpRoute = lpCar->GetRoute();
        // v3[1282] / v3[1280]: meStatus (car+0x1408) != UNINITIALISED and miNodeCount > 0.
        if (!(lpRoute->GetStatus() != Route::E_STATUS_UNINITIALISED && lpRoute->GetNodeCount() > 0))
            return;

        if (lpCar->miRouteTimeStamp != mActiveRouteTimeStamp)  // lwz 0x1528(car) vs 0x1D5C
        {
            GetRacingLine().ClearSectionCache();               // @0x8276E090
            GetRacingLine().mbIsInitialised = false;
            mActiveRouteTimeStamp = lpCar->miRouteTimeStamp;
        }

        CGS_ASSERT_AT(lpCar != 0, "AI car doesn't have a route", __FILE__, 905);   // :905

        if (lpRoute->GetStatus() == Route::E_STATUS_UNINITIALISED)
            return;

        miCurrentRacingLineNodeIndex = 0;                      // stw 0, 0x1D60
        mfAngleToBrakingTarget       = 0.0f;                   // stfs 0x1CFC

        mTargetRacingLinePos.SetZero();                        // stvx 0x1C80
        mBrakingAnticipationPos.SetZero();                     // stvx 0x1CA0
        mBrakingRoadDir.SetZero();                             // stvx 0x1CB0
        mTargetRoadDir.SetZero();                              // stvx 0x1C90

        // this+7236 == mAggression.mbTargetPosValid (AIAggression +0x44).
        if (lpCar->meBehaviour == static_cast<EAIBehaviour>(4) && GetAggression()->IsTargetPosValid())
            GetRacingLine().mfSpreadDistance = K_ROAD_RAGE_SPREAD_HNG;   // 25.0 -> 0x1AEC
        else
            GetRacingLine().mfSpreadDistance = K_NORMAL_SPREAD_HNG;      // 50.0

        // X360 @0x82792EE8..0x82792F30: r3 = this+0x1B30 (generator), r4 = this+0xF20 (line),
        // r5 = mpCar (0x1CE0), r6 = mpCar->miNextRouteNodeIndex (car+0x1524) - 2,
        // r7 = mpSectionsData (0x1CE4).
        mRacingLineGenerator.InitialiseRacingLine(&GetRacingLine(), lpCar,
                                                  lpCar->miNextRouteNodeIndex - 2,
                                                  mpSectionsData);   // @0x8278FB20

        mSteeringTargetVector.SetZero();                       // stvx 0x1C70
        mbIsRacingLineInitialised = 1;                         // stb 1, 0x1D68

        CGS_ASSERT_AT(GetRacingLine().mbIsInitialised, "Racing line not initialised properly", __FILE__, 954);  // :954
    }

    // ================================================================================
    // GenerateRacingLine @0x8277C4E8
    //
    // Snapshot the car's position into the racing line and either hand the line the aggression
    // target (FIGHTING with a valid target -> "default to slam player", and while the machine is
    // ATTACK_SLAM copy the default perpendicular offset into the road placement) or reset the
    // offset. Either way the route is marked complete so the line is generated once per route.
    // ================================================================================
    void AIDriver::GenerateRacingLine(f32 lfTimeStep)
    {
        (void)lfTimeStep;   // the console body takes no float

        CGS_ASSERT_AT(mpCar != 0, "AI driver trying to make a racing line without a car", __FILE__, 993);   // :993
        CGS_ASSERT_AT(GetRacingLine().mbIsInitialised, "Racing line not initialised properly", __FILE__, 994);  // :994

        // stvx128 v0, r31, 6736 == this+0x1A50 == mRacingLine.mCarPos (RacingLine +0xB30)
        GetRacingLine().mCarPos = mpCar->GetPosition();

        if (mpCar->meBehaviour == static_cast<EAIBehaviour>(4) && GetAggression()->IsTargetPosValid())
        {
            mbCurrentRouteComplete = 1;                                  // stb 1, 0x1D6A
            const Vector3 lTargetPos = GetAggression()->GetTargetPos();  // @0x827656D0
            mRacingLineGenerator.RaceLineDefaultsToSlamPlayer(&GetRacingLine(), lTargetPos);
            if (GetAggression()->GetAggressionState() == E_AI_AGGRESSION_STATE_ATTACK_SLAM)
            {
                // this+6724 = this+6704 == mfRoadPlacement = mfDefaultPerpendicularOffset
                GetRacingLine().mfRoadPlacement = GetRacingLine().mfDefaultPerpendicularOffset;
            }
        }
        else
        {
            mbCurrentRouteComplete = 1;                                  // stb 1, 0x1D6A
            GetRacingLine().mfDefaultPerpendicularOffset = 0.0f;         // stfs 0x1A30
            GetRacingLine().mbDefiniteDestination        = false;        // stw  0x1A34
        }
    }

    // ================================================================================
    // UpdateBrakingAnticipationData @0x827964C0 .. 0x827965E4  (NO IDA export -- image bytes)
    //
    // Where the car will be in 1.5 s (clamped into 16..75 m along the racing line) and which way
    // the road runs there; those two feed CorneringTopSpeed. With no racing line, fall back on
    // "one car-length straight ahead" along the car's USEFUL direction. Either way the road
    // direction is 2D-normalised on the way out (the vrsqrtefp + 2 refine steps at 0x827965AC).
    // ================================================================================
    void AIDriver::UpdateBrakingAnticipationData()
    {
        if (!FindPositionInFuture(mBrakingAnticipationPos, mBrakingRoadDir,
                                  KF_BRAKING_ANTICIPATION_TIME,        // flt_82004D04 == 1.5
                                  KF_BRAKING_MAX_LOOK_AHEAD_DIST,      // flt_820C41F0 == 75
                                  KF_BRAKING_MIN_LOOK_AHEAD_DIST))     // flt_82004000 == 16
        {
            CGS_ASSERT(mpCar != 0, "No car available");            // :1058

            const Vector3 lUseful   = mpCar->GetUsefulDirection();  // @0x82770028
            const Vector3 lPosition = mpCar->GetPosition();         // @0x8276B1F0

            // Calibrated ARTIST 0x82796568..80: store all four lanes and
            // call GetUsefulDirection a second time for the road direction.
            mBrakingAnticipationPos.mV = __vaddfp(lPosition.mV, lUseful.mV);
            mBrakingRoadDir.mV = mpCar->GetUsefulDirection().mV;
        }

        mBrakingRoadDir = Normalize2DU(mBrakingRoadDir);
    }

    // ================================================================================
    // CalcDistanceFromPlayer @0x8276E1C8 .. 0x8276E26C  (NO IDA export -- image bytes)
    //
    // mfDistanceToPlayer = |mpCar->GetPosition() - lPlayerCarPosition| (the vrsqrtefp + 2 refine
    // magnitude idiom, with the vsel that yields 0 for a zero-length delta).
    // ================================================================================
    void AIDriver::CalcDistanceFromPlayer(Vector3 lPlayerCarPosition)
    {
        // No local: ARTIST subtracts straight off GetPosition's returned temporary (lvx128 0(r3)).
        mfDistanceToPlayer = rw::math::vpu::Magnitude(mpCar->GetPosition() - lPlayerCarPosition);   // stfs 0x1D08
    }

    // ================================================================================
    // ResetAttribSysValues @0x8278B2B0
    //
    // Re-seed the per-car steering tunables from the AttribSys. Always resets the two PID
    // controllers first; then, with a car bound, reads the burnoutcarasset -> physicsvehiclehandling
    // (RefSpec @+0x158) -> physicsvehiclesteeringattribs (RefSpec @+0x18) chain and takes
    //   mfTimeToLookAheadForDrift        = steering data +0x34
    //   mfMinDistanceToLookAheadForDrift = steering data +0x30
    // With no car it uses the two rodata defaults (1.1 / 10.0).
    // ================================================================================
    void AIDriver::ResetAttribSysValues()
    {
        ResetPIDTuningState();                                    // @0x8277DA48

        // `NULL != GetCar()`: ARTIST's signed compare (cmpwi 0x8278B2CC), as in ResetPIDTuningState.
        if (NULL != GetCar())
        {
            // ARTIST8278B2D4..8278B344: keyed car -> handling -> steering. DWARF locals
            // lBaseCarAsset (cpp:2774), lHandling (:2775), lSteeringAttribs (:2776).
            Attrib::Gen::burnoutcarasset lBaseCarAsset(mpCar->mCarAssetAttribKey, 0);
            Attrib::Gen::physicsvehiclehandling lHandling(
                const_cast<Attrib::Collection*>(const_cast<Attrib::RefSpec&>(lBaseCarAsset.PhysicsVehicleHandlingAsset()).GetCollection()), 0);
            Attrib::Gen::physicsvehiclesteeringattribs lSteeringAttribs(
                const_cast<Attrib::Collection*>(lHandling.PhysicsVehicleSteeringAttribs().GetCollection()), 0);
            mfTimeToLookAheadForDrift = lSteeringAttribs.GetDriverTimeToLookAheadForDrift();
            mfMinDistanceToLookAheadForDrift = lSteeringAttribs.GetDriverMinDistanceToLookAheadForDrift();

            return;
        }

        mfMinDistanceToLookAheadForDrift = KF_MIN_DISTANT_LOOK_AHEAD_FOR_DRIFT;   // stfs 0x1D40
        mfTimeToLookAheadForDrift        = KF_TIME_LOOK_AHEAD_FOR_DRIFT;    // stfs 0x1D3C
    }

    // ================================================================================
    // ResetPIDTuningState @0x8277DA48
    //
    // Re-Prepare the two steering PID controllers with their fixed {P, I, D} triples:
    //   mPIDController      (this+0x1B34) = {1.5, 0.0, 0.5}
    //   mPIDControllerDrift (this+0x1B98) = {2.0, 0.0, 1.0}
    // The console ALSO constructs and immediately destructs the burnoutcarasset ->
    // physicsvehiclehandling -> physicsvehiclesteeringattribs Attrib chain when a car is bound
    // (0x8277DA68..0x8277DAC4) WITHOUT reading a single field out of it -- the whole block is the
    // Attrib layer's own validation/lifetime sequence, preserved before the PID prepares.
    // ================================================================================
    // DWARF BrnAIDriver.cpp:2798..2804: the gains are file-scope constants defined just above this
    // function (ARTIST AI unity .rdata 0x820C4254..0x820C4268 = 1.5, 0.0, 0.5, 2.0, 0.0, 1.0).
    const f32 KF_P      = 1.5f;     // :2798
    const f32 KF_I      = 0.0f;     // :2799
    const f32 KF_D      = 0.5f;     // :2800
    const f32 KF_DRIFTP = 2.0f;     // :2802
    const f32 KF_DRIFTI = 0.0f;     // :2803
    const f32 KF_DRIFTD = 1.0f;     // :2804

    void AIDriver::ResetPIDTuningState()
    {
        // `NULL != GetCar()` (DWARF: GetCar is called) is the signed compare ARTIST has (cmpwi
        // 0x8277DA60); DWARF locals lBaseCarAsset (cpp:2812), lHandling (:2813), lSteeringAttribs (:2814).
        if (NULL != GetCar())
        {
            //8277DA68..8277DAC4: preserve both validation and balanced instance lifetimes.
            Attrib::Gen::burnoutcarasset lBaseCarAsset(mpCar->mCarAssetAttribKey, 0);
            Attrib::Gen::physicsvehiclehandling lHandling(
                const_cast<Attrib::Collection*>(const_cast<Attrib::RefSpec&>(lBaseCarAsset.PhysicsVehicleHandlingAsset()).GetCollection()), 0);
            Attrib::Gen::physicsvehiclesteeringattribs lSteeringAttribs(
                const_cast<Attrib::Collection*>(lHandling.PhysicsVehicleSteeringAttribs().GetCollection()), 0);
        }

        // DWARF: non-const float32_t[3] lafPIDCoefficients (cpp:2817) / lafDriftPIDCoefficients (:2818).
        f32 lafPIDCoefficients[3]      = { KF_P, KF_I, KF_D };
        f32 lafDriftPIDCoefficients[3] = { KF_DRIFTP, KF_DRIFTI, KF_DRIFTD };

        mPIDController.Prepare(lafPIDCoefficients);           // r3 = this + 0x1B34
        mPIDControllerDrift.Prepare(lafDriftPIDCoefficients); // r3 = this + 0x1B98
    }

    // ================================================================================
    // DoRoundRobinWork @0x82796340
    //
    // The AIModule hands each active driver a slice of the per-frame line work:
    //   E_ROUND_ROBIN_FAN (0): one full SteeringFan::UpdateWeightings pass, worth 1 unit;
    //   E_ROUND_ROBIN_HNG (1): up to FOUR RacingLineGenerator::SpreadHNGBackOneStep steps,
    //                          stopping early when the spread reports it is done; the return is
    //                          the number of steps actually taken (the loop counts BEFORE the
    //                          break, so a finished spread contributes 0).
    // ================================================================================
    // DWARF locals: liIterations (cpp:360) for the whole body; liRepeatSpread (:364) and the
    // const KI_STEPS_PER_FRAME (:366) in the HNG block. The FAN pass counts as one iteration:
    // ARTIST returns `li r3,1` after UpdateWeightings (0x827963BC).
    s32 AIDriver::DoRoundRobinWork(ERoundRobinType leType)
    {
        s32 liIterations = 0;

        if (leType == E_ROUND_ROBIN_HNG)
        {
            const s32 KI_STEPS_PER_FRAME = 3;   // `cmpwi 3 / ble` at 0x82796388: up to four steps
            for (s32 liRepeatSpread = 0; liRepeatSpread <= KI_STEPS_PER_FRAME; ++liRepeatSpread)
            {
                if (mRacingLineGenerator.SpreadHNGBackOneStep(&GetRacingLine()))
                    break;
                ++liIterations;
            }
        }
        else if (leType == E_ROUND_ROBIN_FAN)
        {
            // X360: UpdateWeightings(this+1808 == mSteeringFan, mpCar (0x1CE0), this+3872 ==
            // mRacingLine, this+6960 == mRacingLineGenerator, this == &mNearbyVehicles,
            // *(this+7408) == meAggressionVictim (0x1CF0)).
            mSteeringFan.UpdateWeightings(mpCar, &GetRacingLine(), &mRacingLineGenerator,
                                          GetNearbyVehicles(),
                                          static_cast<EGlobalRaceCarIndex>(meAggressionVictim));
            ++liIterations;
        }

        return liIterations;
    }

    // ================================================================================
    // GetIndexOfFurthestVehicle @0x8277D2E0 (DWARF BrnAIDriver.cpp:2549..2566)
    //
    // Nominates the avoidance slot to evict when the 16-slot list is full (AddNearbyTraffic-
    // ToAvoidance `bl` @0x8277D594, AddNearbyAIToAvoidance @0x8277D834; both drop the candidate on
    // -1). Export hole, read with ppcdis + vmx128.py raw fields; the PS3 twin @0xA02810 is the same
    // code:
    //   0x8277D2FC  vmr128 v123, v1                         lNewPosition
    //   0x8277D308  bl AICar::GetPosition (mpCar @+0x1CE0)  lCarPosition
    //   0x8277D330  vrlimi128 v12,v13,8,0 (0x19886F10) ; 0x8277D344 vrlimi128 v124,v13,4,1
    //               (0x1B846F5C)                            lCar2DPosition = (pos.x, pos.z)
    //   0x8277D34C  vsubfp128 v0 = v123 - v124 ; x*x + y*y ; vrsqrtefp + 2 Newton-Raphson ; * lenSq ;
    //               vsel 0 when lenSq == 0 -> f31           the candidate's distance from the car
    //   0x8277D318  li r23,-1 ; 0x8277D320 li r30,0 ; 0x8277D324 r24 = this+0x10 (&mVehicle[0].mCentre)
    //   0x8277D3CC  loop while r30 < miCount (lwz 0x700 every pass: NearbyVehicles::GetCount inlined
    //               with its :2912/:2913 asserts; GetVehiclePointer's :2942/:2943 asserts inside)
    //   0x8277D460  the same 2D magnitude of mVehicle[i].mCentre - lCar2DPosition
    //   0x8277D4C8  fcmpu f0,f31 ; 0x8277D4CC ble skip (raw 0x40990008 = bc 4,cr6.gt) ;
    //               mr r23,r30  (r24 += 0x70, r30 += 1)
    //   0x8277D4E0  mr r3,r23
    // f31 is NEVER rewritten inside the loop (PS3 identical), so the answer is the LAST entry
    // farther from the car than the candidate -- not the farthest one -- and -1 when none is.
    // ble is taken whenever GT is clear -- on <, == AND UNORDERED -- so only an ORDERED `d > ref`
    // nominates: a NaN entry distance (or a NaN candidate distance) is skipped. (FX-NANPOL
    // 2026-09-24: the earlier `!(d <= ref)` nominated an unordered entry.)
    // ================================================================================
    s32 AIDriver::GetIndexOfFurthestVehicle(Vector2 lNewPosition)
    {
        const Vector3 lCarPosition   = mpCar->GetPosition();
        Vector2 lCar2DPosition;
        lCar2DPosition.mV = __vrlimi(lCarPosition.mV, lCarPosition.mV, 4, 1); // ARTIST 0x8277D330..44

        Vector2 lToCandidate;
        lToCandidate.mV = __vsubfp(lNewPosition.mV, lCar2DPosition.mV); // ARTIST 0x8277D34C
        const f32 lDistanceToDriver = rw::math::vpu::Magnitude(lToCandidate);

        s32       lCacheIndex = -1;
        const f32 lfFurthest  = lDistanceToDriver;
        for (s32 lNewIndex = 0; lNewIndex < mNearbyVehicles.GetCount(); ++lNewIndex)
        {
            const NearbyVehicle* lpVehicle = mNearbyVehicles.GetVehiclePointer(lNewIndex);

            Vector2 lToEntry;
            lToEntry.mV = __vsubfp(lpVehicle->mCentre.mV, lCar2DPosition.mV);
            const f32 lfNewSeperation = rw::math::vpu::Magnitude(lToEntry);

            if (lfNewSeperation > lfFurthest)
                lCacheIndex = lNewIndex;
        }
        return lCacheIndex;
    }

    // ================================================================================
    // AddNearbyTrafficToAvoidance @0x8277D4F8
    //
    // Copy one traffic entity into the avoidance list: its velocity and centre verbatim, type
    // E_NEARBY_TRAFFIC(0), no race-car index (-1), and four HNG boundary lines around the closed
    // quad formed by bounding-box corners 2, 3, 7 and 6 (the entity reads at +80 / +96 / +160 /
    // +144 == maBBCorners[2] / [3] / [7] / [6], each flattened to 2D).
    // ================================================================================
    bool AIDriver::AddNearbyTrafficToAvoidance(const BrnTraffic::BrnTrafficIO::TrafficAIEntity* lpEntity)
    {
        CGS_ASSERT_AT(mNearbyVehicles.GetCount() >= 0, "mNearbyVehicles.GetCount() >= 0", __FILE__, 2591);   // :2591
        CGS_ASSERT_AT(mNearbyVehicles.GetCount() <= NearbyVehicles::KI_MAX_NEARBY_VEHICLES,
                   "mNearbyVehicles.GetCount() <= KI_MAX_NEARBY_TRAFFIC", __FILE__, 2592);                // :2592

        s32 liIndex;
        if (mNearbyVehicles.miCount < NearbyVehicles::KI_MAX_NEARBY_VEHICLES)   // lwz 0x700
        {
            liIndex = mNearbyVehicles.GetCount();
            mNearbyVehicles.Next();
        }
        else
        {
            liIndex = GetIndexOfFurthestVehicle(lpEntity->mCentre);             // v1 = entity+16
            if (liIndex == -1)
                return false;
        }

        NearbyVehicle* lpVehicle = mNearbyVehicles.GetVehiclePointer(liIndex);

        lpVehicle->mCentre   = lpEntity->mCentre;                 // stvx v0, r3, 16
        lpVehicle->mVelocity = lpEntity->mVelocity;               // stvx v0, r0, r3
        lpVehicle->meGlobalRaceCarIndex = static_cast<EGlobalRaceCarIndex>(-1);   // stw -1, +36
        lpVehicle->mType                = E_NEARBY_TRAFFIC;                       // stw  0, +32

        FillHNGQuad(lpVehicle,
                    To2DU(lpEntity->maBBCorners[2]),              // lvx r31, 80
                    To2DU(lpEntity->maBBCorners[3]),              // lvx r31, 96
                    To2DU(lpEntity->maBBCorners[7]),              // lvx r31, 160
                    To2DU(lpEntity->maBBCorners[6]));             // lvx r31, 144
        return true;
    }

    // ================================================================================
    // AddNearbyAIToAvoidance @0x8277D6E0
    //
    // Copy one rival (or the player) into the avoidance list. Rejected outright when the other car
    // is more than 200 m away, or is BEHIND this car (dot of the separation with this car's useful
    // direction < 0) and is not the player. The stored velocity is the other car's velocity, or
    // its useful direction when it is barely moving (|v|^2 < 1). The type is E_NEARBY_PLAYER(2)
    // for the player car and E_NEARBY_AI(1) otherwise, and the four HNG lines box the car's centre
    // by +-2 m in x and y.
    // ================================================================================
    bool AIDriver::AddNearbyAIToAvoidance(const AICar* lpCar)
    {
        const Vector3 lMyPosition    = mpCar->GetPosition();
        const Vector3 lOtherPosition = lpCar->GetPosition();
        const Vector3 lSeparation    = lOtherPosition - lMyPosition;

        if (rw::math::vpu::Dot(lSeparation, lSeparation) > 40000.0f)  // flt_8201C220
            return false;

        const Vector3 lUseful = mpCar->GetUsefulDirection();
        if (rw::math::vpu::Dot(lSeparation, lUseful) < 0.0f && !lpCar->mbIsPlayer)         // lbz 0x1549
            return false;

        CGS_ASSERT_AT(mNearbyVehicles.GetCount() >= 0, "mNearbyVehicles.GetCount() >= 0", __FILE__, 2674);    // :2674
        CGS_ASSERT_AT(mNearbyVehicles.GetCount() <= NearbyVehicles::KI_MAX_NEARBY_VEHICLES,
                   "mNearbyVehicles.GetCount() <= KI_MAX_NEARBY_TRAFFIC", __FILE__, 2675);                 // :2675

        const Vector2 lCentre = To2DU(lpCar->GetPosition());       // the console re-fetches it

        s32 liIndex;
        if (mNearbyVehicles.miCount < NearbyVehicles::KI_MAX_NEARBY_VEHICLES)
        {
            liIndex = mNearbyVehicles.GetCount();
            mNearbyVehicles.Next();
        }
        else
        {
            liIndex = GetIndexOfFurthestVehicle(lCentre);
            if (liIndex == -1)
                return false;
        }

        NearbyVehicle* lpVehicle = mNearbyVehicles.GetVehiclePointer(liIndex);

        const Vector3 lVelocity = lpCar->GetVelocity();
        // vcmpgtfp 1.0 > |v|^2 -> the car is barely moving, use its useful direction instead.
        const Vector3 lHeading = (1.0f > rw::math::vpu::Dot(lVelocity, lVelocity))
                                     ? lpCar->GetUsefulDirection()
                                     : lVelocity;
        lpVehicle->mVelocity = To2DU(lHeading);                    // stvx v13, r0, r31
        lpVehicle->mCentre   = lCentre;                            // stvx v127, r31, 16

        lpVehicle->mType = lpCar->mbIsPlayer ? E_NEARBY_PLAYER : E_NEARBY_AI;      // stw +32 (2 / 1)
        lpVehicle->meGlobalRaceCarIndex =
            static_cast<EGlobalRaceCarIndex>(lpCar->miRaceCarIndex);               // stw +36 (car+0x14C4)

        // The two lazily-built half-extent vectors: X == (2, 0), Y == (0, 2).
        const Vector2 lHalfX = MakeHalfExtentX();
        const Vector2 lHalfY = MakeHalfExtentY();

        // ARTIST 0x8277D9B4..D4 computes the four corners in full registers.
        Vector2 lMinusX; lMinusX.mV = __vsubfp(lCentre.mV, lHalfX.mV);
        Vector2 lPlusX;  lPlusX.mV = __vaddfp(lCentre.mV, lHalfX.mV);
        Vector2 lC0; lC0.mV = __vaddfp(lMinusX.mV, lHalfY.mV);
        Vector2 lC1; lC1.mV = __vaddfp(lPlusX.mV, lHalfY.mV);
        Vector2 lC2; lC2.mV = __vsubfp(lPlusX.mV, lHalfY.mV);
        Vector2 lC3; lC3.mV = __vsubfp(lMinusX.mV, lHalfY.mV);

        FillHNGQuad(lpVehicle, lC0, lC1, lC2, lC3);
        return true;
    }
}
