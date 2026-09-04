#pragma once

// BrnAIDriver.cpp FILE-SCOPE CONSTANTS, shared by the two partfiles of the TU (BrnAIDriver.cpp
// and BrnAIDriver_Update.cpp). The DecFIGS DWARF for BrnAIDriver.cpp names 60 file-scope
// `const float32_t` tunables (lines 4..181); the X360 build keeps them in two places:
//
//   (a) RODATA immediates (0x8200xxxx / 0x820Cxxxx) and initialised .data (0x82F3xxxx) -- READ
//       from the image (scratch/postfx_step9_final/envfix/work/image.bin, big-endian, file offset
//       = VA - 0x82000000). Every value in the first block below carries its address.
//
//   (b) A .bss block 0x8300D6FC .. 0x8300DC64 that reads as ZERO in the image because it is
//       written at start-up by the unity-TU STATIC INITIALISERS, which IDA did not export as
//       functions. They ARE in the image: a run of ~100 tiny leaf routines at
//       0x82C67F00 .. 0x82C69500, each of the form
//           lis/lfs <mph rodata> ; fmuls f0, f0, flt_82F31928 ; stfs f0, <0x8300Dxxx> ; blr
//       with flt_82F31928 == 0.44704 (the mph -> m/s factor), plus a handful of the form
//           lfs <radians rodata> ; bl XMVectorCos @0x821F06B0 ; stfs f0, <0x8300Dxxx> ; blr
//       for the dot-product (cosine) thresholds. Every value in the second block below was
//       EVALUATED from those initialisers (capstone over image.bin), so the whole block is
//       CONSOLE-EXACT -- there are NO placeholder tunables left in this header.
//
//       Worked examples (bss address = initialiser = value):
//         0x8300D750 = 130 mph * 0.44704                    = 58.1152 m/s (KF_ALWAYS_BOOST_SPEED)
//         0x8300D7A4 = 150 mph * 0.44704                    = 67.0560 m/s (KF_FORCED_BOOST_SPEED)
//         0x8300D74C @0x82C68950 = cos(1.0471976 rad, 60d)  = 0.5         (KF_TRIGGER_TURN)
//         0x8300DC00 @0x82C689A0 = cos(0.5235988 rad, 30d)  = 0.8660254   (KF_QUICKTURN_DROP_OUT)
//         0x8300D710 @0x82C68A50 = cos(0.3490658 rad, 20d)  = 0.9396926   (KF_SLOW_TURN_DROP_OUT)
//
// NAMES come from the DWARF's file-scope list matched to the reader by role; the ADDRESS on each
// line is what the console body loads and is the load-bearing part.

#include "types.hpp"

namespace BrnAI
{
    // ================================================================================
    // RECOVERED (rodata / initialised .data, read from the image)
    // ================================================================================
    const f32 KF_AI_STEERING_STEP                    = 1.0f;         // flt_82001C98 -- UpdateSteeringAngle: non-player cars
    const f32 KF_PLAYER_STEERING_STEP                = 0.1f;         // flt_820C424C -- UpdateSteeringAngle: player car, default
    const f32 KF_PLAYER_ROLLING_START_STEERING_STEP  = 0.02f;        // flt_820C4228 -- UpdateSteeringAngle: player car, beh 1/9/10
    const f32 KF_PLAYER_DRIVE_THRU_STEERING_STEP     = 0.03f;        // flt_820C422C -- UpdateSteeringAngle: player car, beh 2
    const f32 KF_MIN_BRAKING_ANGLE                   = 0.5235988f;   // flt_82F3038C == 30 degrees -- CorneringTopSpeed ramp start
    const f32 KF_AI_MAX_BRAKING_SPEED_PROPORTION     = 0.75f;        // flt_820C41FC -- CorneringTopSpeed: speed cap at full ramp
    const f32 KF_AI_HARD_SHOULDER_PROPORTION         = 0.75f;        // flt_820C41FC -- HardShoulderSpeed: speed cap on the shoulder
    const f32 KF_HARD_SHOULDER_START                 = 0.85f;        // flt_820C7EC8 -- HardShoulderSpeed: ramp start (road %)
    const f32 KF_HARD_SHOULDER_RAMP                  = 10.000004f;   // flt_820C822C -- HardShoulderSpeed: ramp slope
    const f32 KF_BRAKING_ANTICIPATION_TIME           = 1.5f;         // flt_82004D04  -- UpdateBrakingAnticipationData
    const f32 KF_BRAKING_MAX_LOOK_AHEAD_DIST         = 75.0f;        // flt_820C41F0  -- UpdateBrakingAnticipationData
    const f32 KF_BRAKING_MIN_LOOK_AHEAD_DIST         = 16.0f;        // flt_82004000  -- UpdateBrakingAnticipationData
    const f32 KF_AI_TIME_TO_START_TURNING            = 2.0f;         // flt_820C41F4  -- UpdateStuck: stuck time before a slow turn
    const f32 KF_AI_TIME_FOR_BEING_STUCK             = 5.0f;         // immediate     -- IsStuck: AI threshold
    const f32 KF_PLAYER_TIME_FOR_BEING_STUCK         = 0.75f;        // flt_820C41FC  -- IsStuck: player-car threshold
    const f32 KF_RACE_STUCK_FREE_TIME                = 10.0f;        // flt_820C4150  -- IsStuck: no stuck reports in the first 10 s of a race
    const f32 KF_PRE_AGGRESSION_DELAY                = 5.0f;         // flt_820C488C  -- IsPlayerProtected / ChooseAggressiveSteeringFan
    const f32 KF_BE_KIND_AFTER_CRASH_DELAY           = 8.0f;         // flt_820C41E0  -- IsPlayerProtected
    const f32 KF_BE_KIND_AFTER_AI_DRIVEN_DELAY       = 3.0f;         // flt_820C4154  -- IsPlayerProtected
    const f32 KF_PLAYER_SLOW_SPEED_TIME_CAP          = 6.0f;         // flt_820C4250  -- UpdatePlayerTimers: mfPlayerSlowSpeedTime cap
    const f32 KF_FORCED_BOOST_TIME                   = 3.0f;         // flt_820C4154  -- AttemptToDriveAtDesiredSpeed: boost window
    const f32 KF_INVULNERABLE_AFTER_CRASH_TIME       = 2.0f;         // flt_820C41F4  -- Update: invuln timer while crashing
    const f32 KF_INVULNERABLE_BEHIND_DISTANCE        = 30.0f;        // flt_820C3FA8  -- IsInvulnerable
    const f32 KF_DRIFT_TRIGGER_ANGLE                 = 0.9f;         // immediate     -- EstimateNeedForDrifting: |angle| > 0.9
    const f32 KF_DRIFT_LOOK_AHEAD_SPEED_SCALE        = 1.1f;         // immediate     -- FindFinalDriftDirection: speed * 1.1
    const f32 KF_DRIFT_LOOK_AHEAD_MIN                = 10.0f;        // flt_820C4150  -- FindFinalDriftDirection: min look-ahead
    const f32 KF_DRIFT_SPEED_RATIO_THRESHOLD         = 0.75f;        // flt_820C41FC  -- AttemptToDriveAtDesiredSpeedInDrift
    const f32 KF_SLOW_TURN_PHASE_RATE                = 0.33333334f;  // flt_820065E0  -- DoSlowTurn: phase = int(behaviourTimer / 3)
    const f32 KF_PROXIMITY_CLOSE                     = 4.0f;         // flt_820C41C0  -- ProximitySpeed
    const f32 KF_PROXIMITY_FAR_RECIP                 = 0.25f;        // flt_82003F40  -- ProximitySpeed: 1 / (far - close)
    const f32 KF_PROXIMITY_MIN_SPEED_SCALE           = 0.5f;         // flt_820C4168  -- ProximitySpeed
    const f32 KF_DEFAULT_TIME_TO_LOOK_AHEAD_FOR_DRIFT= 1.1f;         // flt_820C420C  -- ResetAttribSysValues (no car)
    const f32 KF_DEFAULT_MIN_DIST_LOOK_AHEAD_FOR_DRIFT = 10.0f;      // flt_820C4150  -- ResetAttribSysValues (no car)
    const f32 KF_SPEED_RATIO_SCALE                   = 0.75f;        // flt_820C41FC  -- CalculateDesiredSpeed: desired *= ratio*0.75 + 0.25
    const f32 KF_SPEED_RATIO_BASE                    = 0.25f;        // flt_82003F40
    const f32 KF_AGGRESSIVE_FAN_BUZZ_DISTANCE        = 10.0f;        // flt_820C4150  -- ChooseAggressiveSteeringFan
    const f32 KF_AGGRESSIVE_FAN_MIN_DISTANCE         = 5.0f;         // flt_820C488C
    const f32 KF_AGGRESSIVE_FAN_MID_DISTANCE         = 15.0f;        // flt_820C4238
    const f32 KF_AGGRESSIVE_FAN_MAX_DISTANCE         = 100.0f;       // flt_820C3FAC
    const f32 KF_AGGRESSIVE_FAN_SLOW_PLAYER_TIME     = 6.0f;         // flt_820C4250
    const f32 K_ROAD_RAGE_SPREAD_HNG                 = 25.0f;        // flt_820C4870  -- InitialiseRacingLine (FIGHTING with a target)
    const f32 K_NORMAL_SPREAD_HNG                    = 50.0f;        // flt_820C4244  -- InitialiseRacingLine
    const f32 KF_POST_RACE_BRAKE                     = 0.2f;         // flt_820C4300  -- CalculateCarControls case 9

    // ================================================================================
    // RECOVERED from the .bss static initialisers (0x82C67F00..0x82C69500). Every value below
    // is the CONSOLE value; the comment carries the .bss address the reader loads, the
    // initialiser's source constant and the arithmetic (mph * 0.44704, or XMVectorCos(radians)).
    // ================================================================================
    const f32 KF_QUICKTURN_SLOWNESS_DROPOUT          = 4.4704f;      // flt_8300D6FC =  10 mph -- UpdateQuickTurn: speed < K -> drop out
    const f32 KF_SLOW_TURN_DROP_OUT                  = 0.9396926f;   // flt_8300D710 = cos(20 deg) -- DoSlowTurnBehaviour: dot(route, facing) > K
    const f32 KF_SLOW_TURN_SPEED                     = 8.9408f;      // flt_8300D718 =  20 mph -- DoSlowTurn: speed / K
    const f32 KF_TRIGGER_TURN                        = 0.5f;         // flt_8300D74C = cos(60 deg) -- Determine180Turn: dot(route, facing) < -K
    const f32 KF_ALWAYS_BOOST_SPEED                  = 58.1152f;     // flt_8300D750 = 130 mph -- CheckForBoosting: desired > K
    const f32 KF_AGGRESSIVE_FAN_MARKED_MAN_PLAYER_SPEED = 40.2336f;  // flt_8300D774 =  90 mph -- ChooseAggressiveSteeringFan (MARKED_MAN arm)
    const f32 KF_FORCED_BOOST_SPEED                  = 67.056f;      // flt_8300D7A4 = 150 mph -- CheckForBoosting (forced speed): speed > K
    const f32 KF_MAX_DRIFT_UNNECESSARY_ANGLE         = 0.06981317f;  // flt_8300D7AC = 4 deg   -- DoDrivingBehaviour: |drift angle| < K -> exit
    const f32 KF_MAX_SPEED_FOR_BEING_STUCK           = 4.4704f;      // flt_8300D818 =  10 mph -- UpdateStuck: speed >= K -> not stuck
    const f32 KF_NO_BOOST_SPEED                      = 17.8816f;     // flt_8300D81C =  40 mph -- CheckForBoosting: speed > K
    const f32 KF_AGGRESSIVE_FAN_SPEED_ADVANTAGE      = 4.4704f;      // flt_8300D934 =  10 mph -- ChooseAggressiveSteeringFan
    const f32 KF_QUICKTURN_SLOWNESS_DROPIN           = 13.4112f;     // flt_8300D960 =  30 mph -- Determine180Turn: speed >= K -> quick turn
    const f32 KF_BRAKING_START_DIFF                  = 8.9408f;      // flt_8300D96C =  20 mph -- AttemptToDriveAtDesiredSpeed: brake ramp start
    const f32 KF_STOP_BRAKE_SPEED                    = 2.2352f;      // flt_8300D984 =   5 mph -- CalculateCarControls case 0: full brake above K
    const f32 KF_ACCELERATION_MAX_DIFF               = 8.9408f;      // flt_8300D9A8 =  20 mph -- AttemptToDriveAtDesiredSpeed: accel ramp width
    const f32 KF_BRAKING_MAX_DIFF                    = 17.8816f;     // flt_8300DB0C =  40 mph -- AttemptToDriveAtDesiredSpeed: brake ramp end
    const f32 KF_BOOST_TO_CLOSE_SPEED_GAP            = 2.2352f;      // flt_8300DB20 =   5 mph -- CheckForBoosting (forced speed)
    const f32 KF_BOOST_UP_TO_DESIRED_SPEED_OFFSET    = 22.352f;      // flt_8300DB24 =  50 mph -- CheckForBoosting
    const f32 K_MIN_DRIFT_SPEED                      = 13.4112f;     // flt_8300DBE4 =  30 mph -- EstimateNeedForDrifting: speed < K -> no drift
    const f32 KF_QUICKTURN_DROP_OUT                  = 0.8660254f;   // flt_8300DC00 = cos(30 deg) -- UpdateQuickTurn: dot(facing, toTarget) > K
    const f32 K_PROXIMITY_SPEED_REDUCTION            = 4.4704f;      // flt_8300DC34 =  10 mph -- ProximitySpeed
    const f32 KF_BRAKING_ANGLE_RANGE                 = 1.0471976f;   // flt_8300DC38 = 60 deg  -- CorneringTopSpeed ramp width

    // ================================================================================
    // Additional address-attested rodata the Update-path bodies load.
    // ================================================================================
    const f32 KF_NEARBY_AI_MAX_DISTANCE_SQ           = 40000.0f;     // flt_8201C220 -- AddNearbyAIToAvoidance (200 m, squared)
    const f32 KF_NEARBY_VEHICLE_HALF_EXTENT          = 2.0f;         // unk_8300DC80 / unk_8300DC90 -- the HNG box half-extents
    const f32 KF_QUICK_TURN_STEERING_LOCK            = 1.0f;         // flt_82001C98 / flt_820037C8 -- GetQuickTurnSteering returns +-K
    const f32 KF_PID_NORMAL_P                        = 1.5f;         // flt_82004D04 -- ResetPIDTuningState mPIDController {P,I,D}
    const f32 KF_PID_NORMAL_I                        = 0.0f;         // flt_82001CC0
    const f32 KF_PID_NORMAL_D                        = 0.5f;         // flt_820C4168
    const f32 KF_PID_DRIFT_P                         = 2.0f;         // flt_820C41F4 -- ResetPIDTuningState mPIDControllerDrift {P,I,D}
    const f32 KF_PID_DRIFT_I                         = 0.0f;         // flt_82001CC0
    const f32 KF_PID_DRIFT_D                         = 1.0f;         // flt_82001C98
    const f32 KF_CENTRE_LINE_AHEAD                   = 0.9975f;      // AICar::OnModeStart @0x8277BE30 -> RacingLine::mfCentreLineAhead
    const f32 KF_CENTRE_LINE_AHEAD_RECIP             = 400.0004f;    // ... and 1 / (1 - 0.9975) -> RacingLine::mfCentreLineAheadRecip

    // ADDITIVE (aiwave R7 2026-09-04): the per-lane "is this 2D vector degenerate" epsilon
    // CalculateSteeringAngle @0x8277CD98 / @0x8277CF50 splats out of flt_820C3B70. Image bytes at
    // 0x820C3B70 == 0x34000000 == 1.1920928955078125e-07 (FLT_EPSILON). Both tests are strict
    // `fabs(lane) > eps` (vandc sign-clear then vcmpgtfp), so equality counts as degenerate.
    const f32 KF_STEERING_VECTOR_EPSILON             = 1.1920928955078125e-07f;   // flt_820C3B70
}
