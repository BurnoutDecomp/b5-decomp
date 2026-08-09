#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehicleAttribs.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"  // SimpleVehicleAttribs (DWARF home: this TU)
#include "GameSource/AttribSys/Generated/classes/physicsvehiclehandling.h"             // the handling wrapper (SetupAttribs' source)
#include "GameSource/AttribSys/Generated/classes/physicsvehiclebaseattribs.h"          // the base-attribs sub-record wrapper
#include "GameSource/AttribSys/Generated/classes/physicsvehiclesuspensionattribs.h"    // the suspension sub-record wrapper
#include "GameSource/AttribSys/Generated/classes/physicsvehiclesteeringattribs.h"      // the steering sub-record wrapper (SetupAttribs(handling))
#include "GameSource/AttribSys/Generated/classes/physicsvehicledriftattribs.h"         // the drift sub-record wrapper (SetupAttribs(handling))
#include "GameSource/AttribSys/Generated/classes/physicsvehiclecollisionattribs.h"     // the collision sub-record wrapper (SetupAttribs(handling))
#include "GameSource/AttribSys/Generated/classes/physicsvehicleboostattribs.h"         // the boost sub-record wrapper (SetupAttribs(handling))
#include "GameSource/AttribSys/Generated/classes/physicsvehiclebodyrollattribs.h"      // the body-roll sub-record wrapper (SetupAttribs(handling))
#include "GameSource/AttribSys/Generated/classes/physicsvehicleengineattribs.h"        // the engine sub-record wrapper (SetupAttribs(handling))

#include "types.hpp"

#include <cstddef>
#include <cstring>   // std::memcpy (SetupAttribsForAI's engine-block re-copy)

// This TU used to carry a private, self-contained copy of five types the tree already owns
// (rw::math::vpu::VecFloat/Vector3/Vector4, BrnPhysics::InterpedParam3, Wheel::TireAttribs) plus
// its own definition of VehicleAttribs. All six are retired in favour of VehicleAttribs.h; see
// that file's banner for the layout correction that came with the de-fork (mDriftAttribs and
// mEngineAttribs were transposed here).

namespace BrnPhysics
{
namespace Vehicle
{
namespace EngineDefaults
{
// The console's default-engine tuning constants, READ OUT OF THE IMAGE (2026-08-03).
//
// EngineAttribs::Construct @0x825B7B90 is 267 instructions of `lfs` + stack-scratch +
// `lvlx`/`vspltw` + `vrlimi128` lane inserts, and it REUSES its eight stack slots between
// inserts -- so which .rdata slot lands in which (register, lane) is a dataflow question, not a
// reading-the-listing question. It was answered by symbolically simulating the whole body
// (GPRs carrying this+off / &stackslot, VRs carrying four symbolic lanes, `vrlimi128` masks
// 8/4/2/1 == lanes x/y/z/w, `stvx128` committing). 31 stores, no lane left unresolved.
//
// Every value below was then read TWICE from the shipped X360 image and the two agree exactly:
//   * the self-calibrating .id1 reader (delta -1594, 9/9 prologue witnesses), and
//   * headless IDA 9.3 `ida_bytes.get_bytes` on the ARTIST .i64.
// All 23 distinct slots report `seg=.rdata perm=4` -- read-only, so none of them is the
// static-init `.data` family whose image bytes are meaningless.
//
// Independent confirmations that the simulation is right:
//   * Hex-Rays itself decodes nine of them inline in the pseudocode (v43=1.0, v44=1.6, v45=20.0,
//     v46=4400.0, v47=370.0, and `Prepare(a1, 37.0, 740.0, 0.0)`) -- all match.
//   * every lane the simulation leaves untouched is exactly a `Vector3`'s unused `.w`.
//   * role: gear ratio 0 is NEGATIVE (reverse) and 1..5 descend monotonically; every gear-up RPM
//     is below MaxRPM; GearDownRPM (1500) sits between idle (1000) and the up-shift band.
const f32 KF_DEFAULT_GEAR_RATIO_0 = -2.5f;                       // flt_8200D568 -- reverse gear
const f32 KF_DEFAULT_GEAR_RATIO_1 = 3.20999998f;                 // flt_820920B0
const f32 KF_DEFAULT_GEAR_RATIO_2 = 1.92999995f;                 // flt_820920AC
const f32 KF_DEFAULT_GEAR_RATIO_3 = 1.29999995f;                 // flt_8201ECC8
const f32 KF_DEFAULT_GEAR_RATIO_4 = 1.0f;                        // flt_82001C98
const f32 KF_DEFAULT_GEAR_RATIO_5 = 0.75f;                       // flt_82004018
const f32 KF_DEFAULT_DIFFERENTIAL = 4.11000013f;                 // flt_820920B4
const f32 KF_DEFAULT_TRANSMISSION_EFFICIENCY = 1.0f;             // flt_82001C98
// ⚠️ A REAL 0.0f, not a placeholder: flt_82001CC0 is `.rdata perm=4`, it sits in the compiler's
// shared scalar pool next to 1.0/2.0/0.5, and scratch/GVM/init_map_table.txt lists it only ever as
// a static-init SOURCE, never as a target. These are the *defaults*; EngineAttribs::
// InitializeFromAttribs @0x825CF278 overwrites them from the per-car data record.
const f32 KF_DEFAULT_ENGINE_RESISTANCE = 0.0f;                   // flt_82001CC0
const f32 KF_DEFAULT_GEAR_DOWN_RPM = 1500.0f;                    // flt_820266C4
const f32 KF_DEFAULT_MAX_TORQUE = 370.0f;                        // flt_820298E0
const f32 KF_DEFAULT_TORQUE_FALL_OFF_RPM = 4400.0f;              // flt_820543BC
const f32 KF_DEFAULT_MAX_RPM = 8000.0f;                          // flt_82019690
const f32 KF_DEFAULT_LSDM_SPEED_TO_ALLOW_GEAR_CHANGES = 20.0f;   // flt_8208F9D4
const f32 KF_DEFAULT_FLYWHEEL_INERTIA = 0.200000003f;            // flt_82004744
const f32 KF_DEFAULT_FLYWHEEL_FRICTION = 500.0f;                 // flt_8200A034
const f32 KF_DEFAULT_GEAR_CHANGE_TIME = 0.0f;                    // flt_82001CC0 (see the note above)
const f32 KF_DEFAULT_TORQUE_SCALE_0 = 1.60000002f;               // flt_8200C6C8 -- reverse
const f32 KF_DEFAULT_TORQUE_SCALE_1 = 1.0f;                      // flt_82001C98
const f32 KF_DEFAULT_TORQUE_SCALE_2 = 1.0f;                      // flt_82001C98
const f32 KF_DEFAULT_TORQUE_SCALE_3 = 1.0f;                      // flt_82001C98
const f32 KF_DEFAULT_TORQUE_SCALE_4 = 1.0f;                      // flt_82001C98
const f32 KF_DEFAULT_TORQUE_SCALE_5 = 1.0f;                      // flt_82001C98
const f32 KF_DEFAULT_GEAR_UP_RPM_0 = 6000.0f;                    // flt_820920A8
const f32 KF_DEFAULT_GEAR_UP_RPM_1 = 7000.0f;                    // flt_820920A4
const f32 KF_DEFAULT_GEAR_UP_RPM_2 = 5853.0f;                    // flt_820920A0
const f32 KF_DEFAULT_GEAR_UP_RPM_3 = 5945.0f;                    // flt_8209209C
const f32 KF_DEFAULT_GEAR_UP_RPM_4 = 6000.0f;                    // flt_820920A8 (same slot as gear 0)
const f32 KF_DEFAULT_GEAR_UP_RPM_5 = 6000.0f;                    // flt_820920A8 (same slot as gear 0)

// The torque-curve domain. ⚠️ The DWARF names InterpedParam3::Prepare's parameters generically
// (lParamA/lParamB/lParamC); the "input min / input max / output at min" reading is this tree's
// interpretation of the roles, not a console name. The VALUES are asm-literal.
const f32 KF_DEFAULT_TORQUE_CURVE_INPUT_MIN = 37.0f;             // flt_82092094
const f32 KF_DEFAULT_TORQUE_CURVE_INPUT_MAX = 740.0f;            // flt_82092098
const f32 KF_DEFAULT_TORQUE_CURVE_OUTPUT_AT_MIN = 0.0f;          // flt_82001CC0

// SetupAttribsForDonutAI @0x825F6298 loads ONE .rdata float (flt_8205820C) and stores it to two
// places -- the engine's MaxTorque lane and the base block's Mass lane. Modelled as the single
// constant the asm actually has, not as two same-valued names.
const f32 KF_DONUT_AI_MASS_AND_MAX_TORQUE = 700.0f;   // flt_8205820C
const f32 KF_DONUT_AI_MIN_SPEED_FOR_DRIFT = 0.0f;     // flt_82001CC0
}

namespace HandlingDefaults
{
// The console's DEFAULT handling attributes -- every constant `VehicleAttribs::Construct`
// @0x825F3FB8 (840 instructions) writes.
//
// HOW THESE WERE RECOVERED. The body is 111 `stfs` into SIXTEEN stack float slots that are
// REUSED throughout, 93 `lvlx`+`vspltw` broadcasts of those slots, and 94 `vrlimi128` single-lane
// inserts committed by 100 `stvx128`. Which .rdata slot lands in which (register, lane) is a
// dataflow question; reading the listing top-to-bottom answers it wrongly. It was answered by
// symbolically replaying the whole body (`scratchpad\vmx_sim.py`: GPRs carrying `this+off` /
// `&stackslot`, VRs carrying four symbolic lanes, `vrlimi128` masks 8/4/2/1 == lanes x/y/z/w,
// `stvx128` committing). 100 vector stores, 0 unresolved lanes.
//
// Every value was read from the shipped X360 image with headless IDA 9.3 `ida_bytes.get_bytes`;
// all 54 distinct scalar slots report `seg=.rdata perm=4` (read-only), so none of them is the
// static-init `.data` family whose image bytes are meaningless. The ONE `.data` slot the body
// touches is handled explicitly below (KF_MPH_TO_MPS).
//
// ⭐ FOUR INDEPENDENT CONFIRMATIONS that this landed on the right layout:
//   1. every out-of-line call lands exactly on a member base --
//        InterpedParam3::Construct/Prepare(this+0x60)  == mBaseAttribs.mBrakeScaleToFactorCurve
//        InterpedParam3::Construct/Prepare(this+0x100) == mDriftAttribs.mDriftScaleToYawTorque
//        EngineAttribs::Construct(this+0x190)          == mEngineAttribs
//        TireAttribs::PrepareDefaultFrontTire(this+0x2D0) / ...RearTire(this+0x310)
//        stw 0 @this+0x50 == mBaseAttribs.miRaceCarID ; stb 0 @this+0x360 == mbIsValid
//   2. the asm's single `fdivs` computes `1.0f / <the .y lane of the register at +0xE0>` and
//      inserts the quotient into that same register's .z lane -- and the DWARF names those two
//      lanes `SpeedForMinAngle` and `SpeedForMinAngleRecip`.
//   3. SIX of the seven registers whose DWARF name lists FEWER than four lanes are exactly the
//      registers `Construct` leaves partly untouched (+0xF0 2 lanes, +0x180 3, +0x270 2,
//      +0x280 2 named + 2 `Spare`, +0x2B0 2). Names and stores agree six for six.
//   4. role: wheel positions +-0.85 track / +1.1 front / -1.5 rear == a 2.6 m wheelbase;
//      FrontWheelMass (+0xA0 .w) and RearWheelMass (+0xB0 .x) are both 30 kg across the
//      register boundary; mass 1560 kg; max speed 185.
const f32 KF_WHEEL_POS_X                       = 0.850000024f;  // flt_82013A78
const f32 KF_WHEEL_POS_Y                       = -0.200000003f; // flt_82020A84
const f32 KF_FRONT_WHEEL_POS_Z                 = 1.10000002f;   // flt_82004A1C
const f32 KF_REAR_WHEEL_POS_Z                  = -1.5f;         // flt_8200D538
const f32 KF_ZERO                              = 0.0f;          // flt_82001CC0
const f32 KF_COM_OFFSET_Y                      = -0.400000006f; // flt_82012EF8
const f32 KF_COM_OFFSET_Z                      = 0.300000012f;  // flt_82004740
const f32 KF_CRASH_EXTRA_VELOCITY_FACTOR       = 0.300000012f;  // flt_82004740 (y, z and w lanes)
const f32 KF_DRIVETIME_DEFORM_LIMIT_X          = 0.300000012f;  // flt_82004740
const f32 KF_DRIVETIME_DEFORM_LIMIT_Y          = 0.200000003f;  // flt_82004744
const f32 KF_DRIVETIME_DEFORM_LIMIT_Z          = 0.400000006f;  // flt_8200473C
const f32 KF_DRIVETIME_DEFORM_LIMIT_W          = 0.400000006f;  // flt_8200473C

// mBrakeScaleToFactorCurve: Construct() then Prepare(a, b, c) with f1/f2/f3 asm-literal.
// The asm loads flt_82097A38 ONCE and `fmr f2, f3` -- ParamB and ParamC are the same slot.
const f32 KF_BRAKE_CURVE_PARAM_A               = 0.00999999978f; // flt_82002138
const f32 KF_BRAKE_CURVE_PARAM_BC              = 5.5f;           // flt_82097A38 (f3, then fmr f2,f3)

const f32 KF_MASS                              = 1560.0f;       // flt_82096C9C
const f32 KF_TIME_FOR_FULL_BRAKE_RECIP         = 0.666666687f;  // flt_8200AECC (== 1/1.5 s)
const f32 KF_MAX_SPEED                         = 185.0f;        // flt_8202E7A0
const f32 KF_DOWN_FORCE                        = 40.0f;         // flt_8208FBD0
const f32 KF_DOWN_FORCE_Z_OFFSET               = 0.0700000003f; // flt_8200D528
const f32 KF_ONE                               = 1.0f;          // flt_82001C98
const f32 KF_TRACTION_LINE_LENGTH              = 0.400000006f;  // flt_8200473C
const f32 KF_LOW_SPEED_DRIVING_MPH             = 70.0f;         // flt_820051BC
const f32 KF_LOW_SPEED_TYRE_FRICTION_TC        = 25.0f;         // flt_82004FD8
const f32 KF_LOW_SPEED_THROTTLE_TC             = 20.0f;         // flt_8208F9D4
const f32 KF_LINEAR_DRAG                       = 0.100000001f;  // flt_82004014
const f32 KF_HIGH_SPEED_ANGULAR_DAMPING        = 0.0299999993f; // flt_82009B8C
const f32 KF_WHEEL_MASS                        = 30.0f;         // flt_82004F5C (front .w and rear .x)
const f32 KF_POWER_TO_FRONT                    = 0.0f;          // flt_82001CC0
const f32 KF_POWER_TO_REAR                     = 1.0f;          // flt_82001C98
const f32 KF_DOWN_FORCE_LIFT_CO                = 0.239999995f;  // flt_8200D57C
const f32 KF_PITCH_DAMPING_ON_TAKE_OFF         = 0.699999988f;  // flt_82004C68
const f32 KF_YAW_DAMPING_ON_TAKE_OFF           = 0.100000001f;  // flt_82004014
const f32 KF_ROLL_DAMPING_ON_TAKE_OFF          = 0.25f;         // flt_8208F834
const f32 KF_ROLL_LIMIT_ON_TAKE_OFF            = 1.0f;          // flt_82001C98
const f32 KF_SURFACE_FACTOR                    = 0.0f;          // flt_82001CC0 (all four lanes)

const f32 KF_STEERING_REACTION_PER_SEC         = 2.5f;          // flt_82005548
const f32 KF_STEERING_SPEED_FOR_MIN_ANGLE      = 150.0f;        // flt_82006530
const f32 KF_STEERING_MIN_ANGLE                = 1.79999995f;   // flt_82013A80
const f32 KF_STEERING_MAX_ANGLE                = 20.0f;         // flt_8208F9D4
const f32 KF_STEERING_STRAIGHT_REACTION_BIAS   = 1.5f;          // flt_820945DC

// mDriftScaleToYawTorque: Construct() then Prepare(a, b, c) -- three DISTINCT slots here.
const f32 KF_DRIFT_YAW_CURVE_PARAM_A           = 20000.0f;      // flt_820468AC
const f32 KF_DRIFT_YAW_CURVE_PARAM_B           = 29000.0f;      // flt_82097A2C
const f32 KF_DRIFT_YAW_CURVE_PARAM_C           = 40000.0f;      // flt_8201C220

const f32 KF_MIN_SPEED_FOR_DRIFT               = 40.0f;         // flt_8208FBD0
const f32 KF_STEERING_DRIFT_SCALE_FACTOR       = 0.800000012f;  // flt_8208F9C8
const f32 KF_COUNTER_STEERING_DRIFT_SCALE      = 2.4000001f;    // flt_82097A34
const f32 KF_BASE_COUNTER_STEERING_DRIFT_SCALE = 0.400000006f;  // flt_8200473C
const f32 KF_BRAKING_DRIFT_SCALE_FACTOR        = 1.0f;          // flt_82001C98
const f32 KF_GAS_DRIFT_SCALE_FACTOR            = 1.0f;          // flt_82001C98
const f32 KF_TIME_TO_CAP_SCALE                 = 0.25f;         // flt_8208F834
const f32 KF_CAPPED_SCALE                      = 1.0f;          // flt_82001C98
const f32 KF_DRIFT_TORQUE_FALL_OFF             = 0.100000001f;  // flt_82004014
const f32 KF_GRIP_FROM_STEERING                = 0.0f;          // flt_82001CC0
const f32 KF_GRIP_FROM_BRAKE                   = 0.0f;          // flt_82001CC0
const f32 KF_TIME_FOR_NATURAL_DRIFT            = 10.0f;         // flt_82004A20
const f32 KF_NEUTRAL_TIME_TO_REDUCE_DRIFT      = 0.5f;          // flt_82001DA0
const f32 KF_SIDE_FORCE_DRIFT_SCALE_CUT_OFF    = 0.699999988f;  // flt_82004C68
const f32 KF_SIDE_FORCE_DRIFT_ANGLE_CUT_OFF    = 50.0f;         // flt_820138DC
const f32 KF_SIDE_FORCE_DRIFT_SPEED_CUT_OFF    = 100.0f;        // flt_820049E0
const f32 KF_SIDE_FORCE_PEAK_DRIFT_ANGLE       = 25.0f;         // flt_82004FD8
const f32 KF_SIDE_FORCE_MAGNITUDE              = 15.0f;         // flt_820047C4
const f32 KF_NATURAL_DRIFT_DECAY               = 0.959999979f;  // flt_82097A30
const f32 KF_NATURAL_DRIFT_DECAY_POWER         = 7.0f;          // flt_820054D0
const f32 KF_NATURAL_YAW_TORQUE                = 1000.0f;       // flt_82009E10
const f32 KF_NATURAL_YAW_TORQUE_CUT_OFF_ANGLE  = 25.0f;         // flt_82004FD8
const f32 KF_TORQUE_KICK_FROM_GAS_LET_OFF      = 0.0f;          // flt_82001CC0
const f32 KF_DRIFT_SIDEWAYS_DAMPING            = 0.0199999996f; // flt_82005574
const f32 KF_DRIFT_ANGULAR_DAMPING             = 0.150000006f;  // flt_82094574
const f32 KF_MAX_DRIFT_ANGLE                   = 45.0f;         // flt_82009B80
const f32 KF_COUNTER_STEER_TORQUE_SCALE_FACTOR = 0.5f;          // flt_82001DA0
const f32 KF_DRIFT_PUSH_TIME                   = 0.300000012f;  // flt_82004740
const f32 KF_DRIFT_PUSH_SCALE_LIMIT            = 0.300000012f;  // flt_82004740
const f32 KF_DRIFT_PUSH_BASE_FACTOR            = 0.0500000007f; // flt_820047C8
const f32 KF_MAX_POWER_SLIDE_FACTOR            = 0.0f;          // flt_82001CC0

const f32 KF_SUSPENSION_REST_DISPLACEMENT      = 0.100000001f;  // flt_82004014
const f32 KF_SUSPENSION_DAMPENING              = 8.0f;          // flt_82004C88
const f32 KF_SUSPENSION_UPWARD_MOVEMENT        = 0.109999999f;  // flt_820047C0
const f32 KF_SUSPENSION_DOWNWARD_MOVEMENT      = 0.125f;        // flt_82004010
const f32 KF_FRONT_WHEEL_HEIGHT_OFFSET         = 0.0f;          // flt_82001CC0
const f32 KF_REAR_WHEEL_HEIGHT_OFFSET          = 0.0f;          // flt_82001CC0
const f32 KF_IN_AIR_DAMPING                    = 30.0f;         // flt_82004F5C
const f32 KF_MAX_PITCH_DAMPING_ON_LANDING      = 0.600000024f;  // flt_82004D00
const f32 KF_MAX_YAW_DAMPING_ON_LANDING        = 1.0f;          // flt_82001C98
const f32 KF_MAX_ROLL_DAMPING_ON_LANDING       = 1.0f;          // flt_82001C98
const f32 KF_MAX_VERT_VELOCITY_DAMPING         = 0.100000001f;  // flt_82004014
const f32 KF_TIME_TO_DAMP_AFTER_LANDING        = 0.100000001f;  // flt_82004014

const f32 KF_WEIGHT_TRANSFER_DECAY_X           = 0.100000001f;  // flt_82004014
const f32 KF_WEIGHT_TRANSFER_DECAY_Z           = 0.800000012f;  // flt_8208F9C8
const f32 KF_FACTOR_OF_WEIGHT_X                = 0.100000001f;  // flt_82004014
const f32 KF_FACTOR_OF_WEIGHT_Z                = 0.300000012f;  // flt_82004740
const f32 KF_WHEEL_LONG_FORCE_HEIGHT_OFFSET    = 0.100000001f;  // flt_82004014
const f32 KF_WHEEL_LAT_FORCE_HEIGHT_OFFSET     = 0.100000001f;  // flt_82004014

// ⚠️⚠️ THE SILENT-ZERO SLOT. The asm forms the default crash speed as
//     vmulfp128 v0, [unk_83017FE0], splat(150.0f)
// and `unk_83017FE0` reads **all-zero in the image** (`.data perm=6`). It is NOT zero: an
// IDA-unmarked static-init thunk at 0x82C6D160 does
//     lis r11, flt_82F31928@ha ; lvlx v0 ; vspltw v0,v0,0 ; stvx128 v0, unk_83017FE0
// and flt_82F31928 == 0.447039992f -- exactly 1 mph in m/s. (Its `.data` neighbours are
// 1.5707964 == pi/2 and 6.2831855 == 2*pi, i.e. it is the engine's unit-conversion table, and
// twenty functions across World/GameState/Physics/Sound `lfs` from it.) The DWARF names the
// destination lane `mvCrashSpeedMPS` -- MPS -- so the product IS "150 mph expressed in m/s".
// Left at the image's 0.0f the default crash speed would be **0 m/s** and every contact would
// read as a crash.
const f32 KF_MPH_TO_MPS                        = 0.447039992f;  // unk_83017FE0 <- flt_82F31928
const f32 KF_CRASH_SPEED_MPH                   = 150.0f;        // flt_82006530
const f32 KF_CAR_ANGULAR_IMPULSE_SCALE         = 1.0f;          // flt_82001C98

const f32 KF_BOOST_BASE                        = 1.0f;          // flt_82001C98
const f32 KF_MAX_BOOST_SPEED                   = 170.0f;        // flt_82022E14
const f32 KF_BOOST_LINEAR_DRAG                 = 0.0500000007f; // flt_820047C8
const f32 KF_NORMAL_BOOST_HEIGHT_OFFSET        = 0.0f;          // flt_82001CC0
const f32 KF_NORMAL_BOOST_ACCELERATION         = 6.0f;          // flt_8208F9C4
const f32 KF_BOOST_KICK_MAX_START_SPEED        = 100.0f;        // flt_820049E0
const f32 KF_BOOST_KICK_MAX_TIME               = 0.0f;          // flt_82001CC0
const f32 KF_BOOST_KICK_MIN_TIME               = 0.0f;          // flt_82001CC0
const f32 KF_BOOST_KICK_ACCELERATION           = 30.0f;         // flt_82004F5C
const f32 KF_BOOST_KICK_HEIGHT_OFFSET          = -0.899999976f; // flt_8200D5A4
}

using namespace EngineDefaults;
using namespace HandlingDefaults;

namespace
{
// The console builds every scalar-to-lane setter argument with `lfs` + `vspltw` -- a broadcast of
// one float across the whole register -- and the setter then `vrlimi`s lane 0 into place. This
// helper is that vspltw.
inline VecFloat KVF(f32 lfValue)
{
    return VecFloat{ lfValue, lfValue, lfValue, lfValue };
}
}

// @0x825B7B90 (267 instrs)  BrnPhysics::Vehicle::VehicleAttribs::EngineAttribs::Construct
//
// Build the default engine attributes. This is the symbol Engine::Construct @0x825F3EE8 calls
// (nested under VehicleAttribs, NOT at namespace scope).
//
// The setter ORDER below is the asm's `stvx128` order, recovered by simulating the body (the
// previous reconstruction had SetMaxTorque three calls too early and SetFlyWheelInertia before the
// torque curve instead of after it). Every store targets a distinct lane, so the order is not
// behavioural -- it is recorded because it is what the console does.
void VehicleAttribs::EngineAttribs::Construct()
{
    SetGearRatio(0, KVF(KF_DEFAULT_GEAR_RATIO_0));     // 0x825B7C6C  [this+0x40].x
    SetGearRatio(1, KVF(KF_DEFAULT_GEAR_RATIO_1));     // 0x825B7C88  [this+0x50].x
    SetGearRatio(2, KVF(KF_DEFAULT_GEAR_RATIO_2));     // 0x825B7CA0  [this+0x60].x
    SetGearRatio(3, KVF(KF_DEFAULT_GEAR_RATIO_3));     // 0x825B7CB8  [this+0x70].x
    SetGearRatio(4, KVF(KF_DEFAULT_GEAR_RATIO_4));     // 0x825B7CCC  [this+0x80].x
    SetGearRatio(5, KVF(KF_DEFAULT_GEAR_RATIO_5));     // 0x825B7CD8  [this+0x90].x

    SetDifferential(KVF(KF_DEFAULT_DIFFERENTIAL));                       // 0x825B7D04  [+0x10].x
    SetTransmissionEfficiency(KVF(KF_DEFAULT_TRANSMISSION_EFFICIENCY));  // 0x825B7D40  [+0x10].y
    SetEngineResistance(KVF(KF_DEFAULT_ENGINE_RESISTANCE));              // 0x825B7D84  [+0x10].z

    SetGearUpRPM(0, KVF(KF_DEFAULT_GEAR_UP_RPM_0));    // 0x825B7DA4  [this+0x40].z
    SetGearUpRPM(1, KVF(KF_DEFAULT_GEAR_UP_RPM_1));    // 0x825B7DC4  [this+0x50].z
    SetGearUpRPM(2, KVF(KF_DEFAULT_GEAR_UP_RPM_2));    // 0x825B7DE0  [this+0x60].z
    SetGearUpRPM(3, KVF(KF_DEFAULT_GEAR_UP_RPM_3));    // 0x825B7DF8  [this+0x70].z
    SetGearUpRPM(4, KVF(KF_DEFAULT_GEAR_UP_RPM_4));    // 0x825B7E10  [this+0x80].z
    SetGearUpRPM(5, KVF(KF_DEFAULT_GEAR_UP_RPM_5));    // 0x825B7E50  [this+0x90].z

    SetGearDownRPM(KVF(KF_DEFAULT_GEAR_DOWN_RPM));                                  // 0x825B7E78 [+0x10].w
    SetMaxTorque(KVF(KF_DEFAULT_MAX_TORQUE));                                       // 0x825B7E8C [+0x20].x
    SetTorqueFallOffRPM(KVF(KF_DEFAULT_TORQUE_FALL_OFF_RPM));                       // 0x825B7E94 [+0x20].y
    SetLSDMSpeedToAllowGearChanges(KVF(KF_DEFAULT_LSDM_SPEED_TO_ALLOW_GEAR_CHANGES)); // 0x825B7E9C [+0x20].w

    SetTorqueScale(0, KVF(KF_DEFAULT_TORQUE_SCALE_0)); // 0x825B7EA8  [this+0x40].y
    SetTorqueScale(1, KVF(KF_DEFAULT_TORQUE_SCALE_1)); // 0x825B7EBC  [this+0x50].y
    SetTorqueScale(2, KVF(KF_DEFAULT_TORQUE_SCALE_2)); // 0x825B7ED8  [this+0x60].y
    SetTorqueScale(3, KVF(KF_DEFAULT_TORQUE_SCALE_3)); // 0x825B7EEC  [this+0x70].y
    SetTorqueScale(4, KVF(KF_DEFAULT_TORQUE_SCALE_4)); // 0x825B7EF8  [this+0x80].y
    SetTorqueScale(5, KVF(KF_DEFAULT_TORQUE_SCALE_5)); // 0x825B7F04  [this+0x90].y

    // 0x825B7F08 / 0x825B7F24 -- both called with r3 == this, because mTorqueCurve is the leading
    // member of EngineAttribs (&mTorqueCurve == this).
    mTorqueCurve.Construct();
    mTorqueCurve.Prepare(
        KF_DEFAULT_TORQUE_CURVE_INPUT_MIN,
        KF_DEFAULT_TORQUE_CURVE_INPUT_MAX,
        KF_DEFAULT_TORQUE_CURVE_OUTPUT_AT_MIN);

    SetMaxRPM(KVF(KF_DEFAULT_MAX_RPM));                       // 0x825B7F84  [+0x20].z
    SetFlyWheelInertia(KVF(KF_DEFAULT_FLYWHEEL_INERTIA));     // 0x825B7F90  [+0x30].x
    SetFlyWheelFriction(KVF(KF_DEFAULT_FLYWHEEL_FRICTION));   // 0x825B7F98  [+0x30].y
    SetGearChangeTime(KVF(KF_DEFAULT_GEAR_CHANGE_TIME));      // 0x825B7FAC  [+0x30].z
}

// @0x825CF278  BrnPhysics::Vehicle::VehicleAttribs::EngineAttribs::InitializeFromAttribs
//
// A pure data-marshalling lane-scatter: it streams the engine tuning out of a loaded
// `physicsvehicleengineattribs` data wrapper (the X360 reaches it via *(a2+4)) into this packed
// EngineAttribs. The X360 does it with lvsl/vperm/vrlimi single-lane inserts; de-SIMD'd, each
// insert is one scalar copy. The source-wrapper byte offsets are EXACT (asm-confirmed); the
// wrapper's own type is un-homed, so it is read here as a byte-addressed float source.
//
// Source -> destination map (asm-confirmed):
//   per-gear ratio  (gear[g].x): src+0x50 lanes -> gears 0,1,2 ; src+0x40 lanes -> gears 3,4,5
//   per-gear up-RPM (gear[g].z): src+0x30 lanes -> gears 0,1,2 ; src+0x20 lanes -> gears 3,4,5
//   per-gear torque (gear[g].y): src+0x10 lanes -> gears 0,1,2 ; src+0x00 lanes -> gears 3,4,5
//   scalars:  src+0x8C -> Differential(@+0x10.x)             (lvlx r9=0x8C -> lane0/.x)
//             src+0x60 -> TransmissionEfficiency(@+0x10.y)    (lvlx r8=0x60 -> lane1/.y)
//             src+0x80 -> EngineResistance(@+0x10.z)          (lvlx r7=0x80 -> lane2/.z)
//             src+0x68 -> GearDownRPM(@+0x10.w)               (lvlx r6=0x68 -> lane0 store, mask8)
//             src+0x64 -> MaxTorque(@+0x20.x)                 (lvlx r5=0x64 -> lane1)
//             src+0x84 -> MaxRPM(@+0x20.z)  [drives torque-curve domain = MaxRPM*0.5]
//             src+0x6C -> (@+0x20.z scatter, lane2)           src+0x70 -> (@+0x20.w, lane3? mask1)
//             src+0x78 -> FlyWheelInertia(@+0x30.x)           src+0x7C -> FlyWheelFriction(@+0x30.y)
//             src+0x74 -> GearChangeTime(@+0x30.z)
//   then mTorqueCurve.Prepare(domainMin, domainMax) with the data-driven domain derived from
//   src+0x84 (MaxRPM) scaled by 0.5 (flt_82001DA0) and 2.0 (flt_82001D9C) -- the InterpedParam3
//   input domain. (flt_82001D9C=2.0 / flt_82001DA0=0.5 are resolved .rdata literals.)
//
// FLAG: the lvsl/vperm element-within-block selection picks one of the four floats inside each
// 16-byte source block; the per-gear scalar->lane assignment is reproduced exactly as the asm
// indexes it. The findings doc flags the gear lane semantics as "unresolved"; this body matches
// the asm reads literally and does not reinterpret them.
void VehicleAttribs::EngineAttribs::InitializeFromAttribs(const void* lpSourceWrapper)
{
    const f32* lpSrc = static_cast<const f32*>(lpSourceWrapper);
    // byte-offset helper (offset is in BYTES; the source floats are 4-byte).
    #define BP_SRC_F(byteOff) (lpSrc[(byteOff) >> 2])

    // --- per-gear gear ratios (gear[g].x) : src+0x50 -> 0,1,2 ; src+0x40 -> 3,4,5 ---
    SetGearRatio(0, KVF(BP_SRC_F(0x50)));
    SetGearRatio(1, KVF(BP_SRC_F(0x54)));
    SetGearRatio(2, KVF(BP_SRC_F(0x58)));
    SetGearRatio(3, KVF(BP_SRC_F(0x40)));
    SetGearRatio(4, KVF(BP_SRC_F(0x44)));
    SetGearRatio(5, KVF(BP_SRC_F(0x48)));

    // --- inline scalar lanes streamed before the torque-curve prepare ---
    SetDifferential(KVF(BP_SRC_F(0x8C)));            // @+0x10.x
    SetTransmissionEfficiency(KVF(BP_SRC_F(0x60)));  // @+0x10.y
    SetEngineResistance(KVF(BP_SRC_F(0x80)));        // @+0x10.z
    SetGearDownRPM(KVF(BP_SRC_F(0x68)));             // @+0x10.w
    SetMaxTorque(KVF(BP_SRC_F(0x64)));               // @+0x20.x

    // --- torque-curve domain (InterpedParam3::Prepare). Domain derived from src+0x84 (MaxRPM):
    //     the asm forms  domainHi = MaxRPM * 0.5  and feeds (domainLo, domainHi) to Prepare. ---
    static const f32 KF_TORQUE_DOMAIN_SCALE = 0.5f;       // flt_82001DA0 (resolved)
    const f32 lfMaxRPM       = BP_SRC_F(0x84);
    const f32 lfDomainHigh   = lfMaxRPM * KF_TORQUE_DOMAIN_SCALE;
    const f32 lfDomainLow    = 0.0f;                      // zeroed scratch (stw r28=0 cascade)
    mTorqueCurve.Prepare(lfDomainLow, lfDomainHigh, 0.0f);

    // --- per-gear torque scales (gear[g].y) : src+0x30 -> 0,1,2 ; src+0x20 -> 3,4,5 ---
    SetTorqueScale(0, KVF(BP_SRC_F(0x30)));
    SetTorqueScale(1, KVF(BP_SRC_F(0x34)));
    SetTorqueScale(2, KVF(BP_SRC_F(0x38)));
    SetTorqueScale(3, KVF(BP_SRC_F(0x20)));
    SetTorqueScale(4, KVF(BP_SRC_F(0x24)));
    SetTorqueScale(5, KVF(BP_SRC_F(0x28)));

    // --- per-gear up-RPMs (gear[g].z) : src+0x10 -> 0,1,2 ; src+0x00 -> 3,4,5 ---
    SetGearUpRPM(0, KVF(BP_SRC_F(0x10)));
    SetGearUpRPM(1, KVF(BP_SRC_F(0x14)));
    SetGearUpRPM(2, KVF(BP_SRC_F(0x18)));
    SetGearUpRPM(3, KVF(BP_SRC_F(0x00)));
    SetGearUpRPM(4, KVF(BP_SRC_F(0x04)));
    SetGearUpRPM(5, KVF(BP_SRC_F(0x08)));

    // --- trailing scalar lanes (@+0x20.z/.w and @+0x30.x/.y/.z) ---
    SetMaxRPM(KVF(BP_SRC_F(0x6C)));                  // @+0x20.z (lvlx r9=0x6C, mask2)
    SetLSDMSpeedToAllowGearChanges(KVF(BP_SRC_F(0x70)));  // @+0x20.w (lvlx r10=0x70, mask1)
    SetFlyWheelInertia(KVF(BP_SRC_F(0x78)));        // @+0x30.x (lvlx r8=0x78, mask8)
    SetFlyWheelFriction(KVF(BP_SRC_F(0x7C)));       // @+0x30.y (lvlx r7=0x7C, mask4)
    SetGearChangeTime(KVF(BP_SRC_F(0x74)));         // @+0x30.z (lvlx r6=0x74, mask2)

    #undef BP_SRC_F
}

// @0x825F3FB8 (840 instrs)  BrnPhysics::Vehicle::VehicleAttribs::Construct
//
// ⚠️ ABSENT from `.ida-exports` (an export-set hole: `Engine::Prepare` @0x825F3F38 ends at
// 0x825F3FB4 and the next indexed symbol is 0x825F4CD8). Pulled from `BURNOUT_X360_ARTIST.XEX.i64`
// with headless IDA 9.3; `ida_funcs.get_func` bounds it at 0x825F3FB8..0x825F4CD4.
//
// Build the DEFAULT handling attribute block. Every constant, and the derivation of which lane
// each one lands in, is documented on the HandlingDefaults block above.
//
// ORDER. The sequence of out-of-line CALLS and the sequence of REGISTERS is the console's.
// Within one register the console's four single-lane `vrlimi128` inserts are in an arbitrary
// scheduler order (e.g. +0x70 goes x,z,y,w and +0xE0 goes y,z,w,x); since the four lanes are
// disjoint that order is not behavioural, so the lanes are written x,y,z,w here. Where the
// console emits ONE whole-vector `stvx128` (a register it built entirely on the stack first)
// the assignment below is a whole-vector assignment.
//
// ⚠️ ONE LANE IS DELIBERATELY NOT WRITTEN: mvLinearDrag_AngularDrag_... `.y` (AngularDrag,
// this+0xA4). The body touches that register exactly three times (mask 2 -> .z, mask 1 -> .w,
// mask 8 -> .x) and never mask 4. It is NOT an omission in this transcription -- do not "fix" it.
// Every other unwritten lane in the block is a lane the DWARF register name does not name at all
// (+0xF0 .z/.w, +0x180 .w, +0x270 .z/.w, +0x280 .z/.w == the two `Spare`s, +0x2B0 .z/.w), plus
// miBoostRule (+0x2C0), miRaceCarID (+0x350) and mAttribsKey (+0x358), which `Construct` also
// leaves alone -- `SetupAttribs` streams those from the per-car `physicsvehiclehandling` record.
void VehicleAttribs::Construct()
{
    // ---- mBaseAttribs: the geometry, as four whole-register stores -------------------------
    mBaseAttribs.mFrontRightWheelPos = Vector3{ KF_WHEEL_POS_X, KF_WHEEL_POS_Y, KF_FRONT_WHEEL_POS_Z, KF_ZERO };
    mBaseAttribs.mRearRightWheelPos  = Vector3{ KF_WHEEL_POS_X, KF_WHEEL_POS_Y, KF_REAR_WHEEL_POS_Z,  KF_ZERO };

    mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x = KF_MASS;
    mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.y = KF_TIME_FOR_FULL_BRAKE_RECIP;
    mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.z = KF_MAX_SPEED;
    mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.w = KF_DOWN_FORCE;

    mBaseAttribs.mvDownForceZOffset_MagicBrakeFactorTurning_MagicBrakeFactorStraightLine_BrakeScaleToLockWheels.x = KF_DOWN_FORCE_Z_OFFSET;
    mBaseAttribs.mvDownForceZOffset_MagicBrakeFactorTurning_MagicBrakeFactorStraightLine_BrakeScaleToLockWheels.y = KF_ONE;
    mBaseAttribs.mvDownForceZOffset_MagicBrakeFactorTurning_MagicBrakeFactorStraightLine_BrakeScaleToLockWheels.z = KF_ONE;
    mBaseAttribs.mvDownForceZOffset_MagicBrakeFactorTurning_MagicBrakeFactorStraightLine_BrakeScaleToLockWheels.w = KF_ONE;

    mBaseAttribs.mvTractionLineLength_LowSpeedDrivingMPH_LowSpeedTyreFrictionTractionControl_LowSpeedThrottleTractionControl.x = KF_TRACTION_LINE_LENGTH;
    mBaseAttribs.mvTractionLineLength_LowSpeedDrivingMPH_LowSpeedTyreFrictionTractionControl_LowSpeedThrottleTractionControl.y = KF_LOW_SPEED_DRIVING_MPH;
    mBaseAttribs.mvTractionLineLength_LowSpeedDrivingMPH_LowSpeedTyreFrictionTractionControl_LowSpeedThrottleTractionControl.z = KF_LOW_SPEED_TYRE_FRICTION_TC;
    mBaseAttribs.mvTractionLineLength_LowSpeedDrivingMPH_LowSpeedTyreFrictionTractionControl_LowSpeedThrottleTractionControl.w = KF_LOW_SPEED_THROTTLE_TC;

    // 0x825F41DC / 0x825F41F8 -- both with r3 == this+0x60.
    mBaseAttribs.mBrakeScaleToFactorCurve.Construct();
    mBaseAttribs.mBrakeScaleToFactorCurve.Prepare(KF_BRAKE_CURVE_PARAM_A,
                                                  KF_BRAKE_CURVE_PARAM_BC,
                                                  KF_BRAKE_CURVE_PARAM_BC);

    mBaseAttribs.miRaceCarID = 0;                                     // 0x825F4250 stw r28,0x50(r31)

    // ⚠️ .y (AngularDrag) intentionally absent -- see the banner.
    mBaseAttribs.mvLinearDrag_AngularDrag_HighSpeedAngularDamping_FrontWheelMass.x = KF_LINEAR_DRAG;
    mBaseAttribs.mvLinearDrag_AngularDrag_HighSpeedAngularDamping_FrontWheelMass.z = KF_HIGH_SPEED_ANGULAR_DAMPING;
    mBaseAttribs.mvLinearDrag_AngularDrag_HighSpeedAngularDamping_FrontWheelMass.w = KF_WHEEL_MASS;

    mBaseAttribs.mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo.x = KF_WHEEL_MASS;
    mBaseAttribs.mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo.y = KF_POWER_TO_FRONT;
    mBaseAttribs.mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo.z = KF_POWER_TO_REAR;
    mBaseAttribs.mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo.w = KF_DOWN_FORCE_LIFT_CO;

    mBaseAttribs.mCOMOffset = Vector3{ KF_ZERO, KF_COM_OFFSET_Y, KF_COM_OFFSET_Z, KF_ZERO };

    mBaseAttribs.mvPitchDampingOnTakeOff_YawDampingOnTakeOff_RollDampingOnTakeOff_RollLimitOnTakeOff.x = KF_PITCH_DAMPING_ON_TAKE_OFF;
    mBaseAttribs.mvPitchDampingOnTakeOff_YawDampingOnTakeOff_RollDampingOnTakeOff_RollLimitOnTakeOff.y = KF_YAW_DAMPING_ON_TAKE_OFF;
    mBaseAttribs.mvPitchDampingOnTakeOff_YawDampingOnTakeOff_RollDampingOnTakeOff_RollLimitOnTakeOff.z = KF_ROLL_DAMPING_ON_TAKE_OFF;
    mBaseAttribs.mvPitchDampingOnTakeOff_YawDampingOnTakeOff_RollDampingOnTakeOff_RollLimitOnTakeOff.w = KF_ROLL_LIMIT_ON_TAKE_OFF;

    mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.x = KF_SURFACE_FACTOR;
    mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.y = KF_SURFACE_FACTOR;
    mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.z = KF_SURFACE_FACTOR;
    mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.w = KF_SURFACE_FACTOR;

    mBaseAttribs.mCrashExtraVelocityFactors = Vector3Plus{ KF_ZERO,
                                                           KF_CRASH_EXTRA_VELOCITY_FACTOR,
                                                           KF_CRASH_EXTRA_VELOCITY_FACTOR,
                                                           KF_CRASH_EXTRA_VELOCITY_FACTOR };
    mBaseAttribs.mDrivetimeDeformLimits = Vector4{ KF_DRIVETIME_DEFORM_LIMIT_X,
                                                   KF_DRIVETIME_DEFORM_LIMIT_Y,
                                                   KF_DRIVETIME_DEFORM_LIMIT_Z,
                                                   KF_DRIVETIME_DEFORM_LIMIT_W };

    // 0x825F444C -- r3 == this+0x190.
    mEngineAttribs.Construct();

    // ---- mSteeringAttribs ------------------------------------------------------------------
    // ⭐ the asm's single `fdivs f0, f30, f0` is literally 1.0f / <the .y lane just written>,
    // and its quotient goes into the .z lane -- SpeedForMinAngle / SpeedForMinAngleRecip.
    mSteeringAttribs.mvReactionPerSec_SpeedForMinAngle_SpeedForMinAngleRecip_MinAngle.x = KF_STEERING_REACTION_PER_SEC;
    mSteeringAttribs.mvReactionPerSec_SpeedForMinAngle_SpeedForMinAngleRecip_MinAngle.y = KF_STEERING_SPEED_FOR_MIN_ANGLE;
    mSteeringAttribs.mvReactionPerSec_SpeedForMinAngle_SpeedForMinAngleRecip_MinAngle.z =
        KF_ONE / mSteeringAttribs.mvReactionPerSec_SpeedForMinAngle_SpeedForMinAngleRecip_MinAngle.y;
    mSteeringAttribs.mvReactionPerSec_SpeedForMinAngle_SpeedForMinAngleRecip_MinAngle.w = KF_STEERING_MIN_ANGLE;

    mSteeringAttribs.mvMaxAngle_StraightReactionBias.x = KF_STEERING_MAX_ANGLE;
    mSteeringAttribs.mvMaxAngle_StraightReactionBias.y = KF_STEERING_STRAIGHT_REACTION_BIAS;

    // ---- mDriftAttribs ---------------------------------------------------------------------
    mDriftAttribs.mvDriftTorqueFallOff_GripFromSteering_GripFromBrake_TimeForNaturalDrift.x = KF_DRIFT_TORQUE_FALL_OFF;
    mDriftAttribs.mvDriftTorqueFallOff_GripFromSteering_GripFromBrake_TimeForNaturalDrift.y = KF_GRIP_FROM_STEERING;
    mDriftAttribs.mvDriftTorqueFallOff_GripFromSteering_GripFromBrake_TimeForNaturalDrift.z = KF_GRIP_FROM_BRAKE;
    mDriftAttribs.mvDriftTorqueFallOff_GripFromSteering_GripFromBrake_TimeForNaturalDrift.w = KF_TIME_FOR_NATURAL_DRIFT;

    mDriftAttribs.mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale.x = KF_BRAKING_DRIFT_SCALE_FACTOR;
    mDriftAttribs.mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale.y = KF_GAS_DRIFT_SCALE_FACTOR;
    mDriftAttribs.mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale.z = KF_TIME_TO_CAP_SCALE;
    mDriftAttribs.mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale.w = KF_CAPPED_SCALE;

    mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.x = KF_MIN_SPEED_FOR_DRIFT;
    mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.y = KF_STEERING_DRIFT_SCALE_FACTOR;
    mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.z = KF_COUNTER_STEERING_DRIFT_SCALE;
    mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.w = KF_BASE_COUNTER_STEERING_DRIFT_SCALE;

    mDriftAttribs.mvDriftPushScaleLimit_DriftPushBaseFactor_MaxPowerSlideFactor.x = KF_DRIFT_PUSH_SCALE_LIMIT;
    mDriftAttribs.mvDriftPushScaleLimit_DriftPushBaseFactor_MaxPowerSlideFactor.y = KF_DRIFT_PUSH_BASE_FACTOR;
    mDriftAttribs.mvDriftPushScaleLimit_DriftPushBaseFactor_MaxPowerSlideFactor.z = KF_MAX_POWER_SLIDE_FACTOR;

    mDriftAttribs.mvDriftAngularDamping_MaxDriftAngle_CounterSteerTorqueScaleFactor_DriftPushTime.x = KF_DRIFT_ANGULAR_DAMPING;
    mDriftAttribs.mvDriftAngularDamping_MaxDriftAngle_CounterSteerTorqueScaleFactor_DriftPushTime.y = KF_MAX_DRIFT_ANGLE;
    mDriftAttribs.mvDriftAngularDamping_MaxDriftAngle_CounterSteerTorqueScaleFactor_DriftPushTime.z = KF_COUNTER_STEER_TORQUE_SCALE_FACTOR;
    mDriftAttribs.mvDriftAngularDamping_MaxDriftAngle_CounterSteerTorqueScaleFactor_DriftPushTime.w = KF_DRIFT_PUSH_TIME;

    mDriftAttribs.mvNeutralTimeToReduceDrift_SideForceDriftScaleCutOff_SideForceDriftAngleCutOff_SideForceDriftSpeedCutOff.x = KF_NEUTRAL_TIME_TO_REDUCE_DRIFT;
    mDriftAttribs.mvNeutralTimeToReduceDrift_SideForceDriftScaleCutOff_SideForceDriftAngleCutOff_SideForceDriftSpeedCutOff.y = KF_SIDE_FORCE_DRIFT_SCALE_CUT_OFF;
    mDriftAttribs.mvNeutralTimeToReduceDrift_SideForceDriftScaleCutOff_SideForceDriftAngleCutOff_SideForceDriftSpeedCutOff.z = KF_SIDE_FORCE_DRIFT_ANGLE_CUT_OFF;
    mDriftAttribs.mvNeutralTimeToReduceDrift_SideForceDriftScaleCutOff_SideForceDriftAngleCutOff_SideForceDriftSpeedCutOff.w = KF_SIDE_FORCE_DRIFT_SPEED_CUT_OFF;

    mDriftAttribs.mvSideForcePeakDriftAngle_SideForceMagnitude_NaturalDriftDecay_NaturalDriftDecayPower.x = KF_SIDE_FORCE_PEAK_DRIFT_ANGLE;
    mDriftAttribs.mvSideForcePeakDriftAngle_SideForceMagnitude_NaturalDriftDecay_NaturalDriftDecayPower.y = KF_SIDE_FORCE_MAGNITUDE;
    mDriftAttribs.mvSideForcePeakDriftAngle_SideForceMagnitude_NaturalDriftDecay_NaturalDriftDecayPower.z = KF_NATURAL_DRIFT_DECAY;
    mDriftAttribs.mvSideForcePeakDriftAngle_SideForceMagnitude_NaturalDriftDecay_NaturalDriftDecayPower.w = KF_NATURAL_DRIFT_DECAY_POWER;

    mDriftAttribs.mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping.x = KF_NATURAL_YAW_TORQUE;
    mDriftAttribs.mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping.y = KF_NATURAL_YAW_TORQUE_CUT_OFF_ANGLE;

    // 0x825F484C / 0x825F486C -- both with r3 == this+0x100.
    mDriftAttribs.mDriftScaleToYawTorque.Construct();
    mDriftAttribs.mDriftScaleToYawTorque.Prepare(KF_DRIFT_YAW_CURVE_PARAM_A,
                                                 KF_DRIFT_YAW_CURVE_PARAM_B,
                                                 KF_DRIFT_YAW_CURVE_PARAM_C);

    mDriftAttribs.mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping.z = KF_TORQUE_KICK_FROM_GAS_LET_OFF;
    mDriftAttribs.mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping.w = KF_DRIFT_SIDEWAYS_DAMPING;

    // ---- mSuspensionAttribs ----------------------------------------------------------------
    mSuspensionAttribs.mvRestDisplacement_Dampening_UpwardMovement_DownwardMovement.x = KF_SUSPENSION_REST_DISPLACEMENT;
    mSuspensionAttribs.mvRestDisplacement_Dampening_UpwardMovement_DownwardMovement.y = KF_SUSPENSION_DAMPENING;
    mSuspensionAttribs.mvRestDisplacement_Dampening_UpwardMovement_DownwardMovement.z = KF_SUSPENSION_UPWARD_MOVEMENT;
    mSuspensionAttribs.mvRestDisplacement_Dampening_UpwardMovement_DownwardMovement.w = KF_SUSPENSION_DOWNWARD_MOVEMENT;

    mSuspensionAttribs.mvFrontWheelHeightOffset_RearWheelHeightOffset_InAirDamping_MaxPitchDampingOnLanding.x = KF_FRONT_WHEEL_HEIGHT_OFFSET;
    mSuspensionAttribs.mvFrontWheelHeightOffset_RearWheelHeightOffset_InAirDamping_MaxPitchDampingOnLanding.y = KF_REAR_WHEEL_HEIGHT_OFFSET;
    mSuspensionAttribs.mvFrontWheelHeightOffset_RearWheelHeightOffset_InAirDamping_MaxPitchDampingOnLanding.z = KF_IN_AIR_DAMPING;
    mSuspensionAttribs.mvFrontWheelHeightOffset_RearWheelHeightOffset_InAirDamping_MaxPitchDampingOnLanding.w = KF_MAX_PITCH_DAMPING_ON_LANDING;

    mSuspensionAttribs.mvMaxYawDampingOnLanding_MaxRollDampingOnLanding_MaxVertVelocityDampingOnLanding_TimeToDampAfterLanding.x = KF_MAX_YAW_DAMPING_ON_LANDING;
    mSuspensionAttribs.mvMaxYawDampingOnLanding_MaxRollDampingOnLanding_MaxVertVelocityDampingOnLanding_TimeToDampAfterLanding.y = KF_MAX_ROLL_DAMPING_ON_LANDING;
    mSuspensionAttribs.mvMaxYawDampingOnLanding_MaxRollDampingOnLanding_MaxVertVelocityDampingOnLanding_TimeToDampAfterLanding.z = KF_MAX_VERT_VELOCITY_DAMPING;
    mSuspensionAttribs.mvMaxYawDampingOnLanding_MaxRollDampingOnLanding_MaxVertVelocityDampingOnLanding_TimeToDampAfterLanding.w = KF_TIME_TO_DAMP_AFTER_LANDING;

    // ---- mBodyRollAttribs ------------------------------------------------------------------
    mBodyRollAttribs.mvWeightTransferDecayX_WeightTransferDecayZ_FactorOfWeightX_FactorOfWeightZ.x = KF_WEIGHT_TRANSFER_DECAY_X;
    mBodyRollAttribs.mvWeightTransferDecayX_WeightTransferDecayZ_FactorOfWeightX_FactorOfWeightZ.y = KF_WEIGHT_TRANSFER_DECAY_Z;
    mBodyRollAttribs.mvWeightTransferDecayX_WeightTransferDecayZ_FactorOfWeightX_FactorOfWeightZ.z = KF_FACTOR_OF_WEIGHT_X;
    mBodyRollAttribs.mvWeightTransferDecayX_WeightTransferDecayZ_FactorOfWeightX_FactorOfWeightZ.w = KF_FACTOR_OF_WEIGHT_Z;

    mBodyRollAttribs.mvWheelLongForceHeightOffset_WheelLatForceHeightOffset.x = KF_WHEEL_LONG_FORCE_HEIGHT_OFFSET;
    mBodyRollAttribs.mvWheelLongForceHeightOffset_WheelLatForceHeightOffset.y = KF_WHEEL_LAT_FORCE_HEIGHT_OFFSET;

    // ---- mCollisionAttribs -----------------------------------------------------------------
    // 0x825F4BD0 `vmulfp128 v0, [unk_83017FE0], splat(150.0f)` -- see the KF_MPH_TO_MPS note.
    mCollisionAttribs.mvCrashSpeedMPS_CarAngularImpulseScale_Spare_Spare.x = KF_CRASH_SPEED_MPH * KF_MPH_TO_MPS;
    mCollisionAttribs.mvCrashSpeedMPS_CarAngularImpulseScale_Spare_Spare.y = KF_CAR_ANGULAR_IMPULSE_SCALE;

    // ---- mBoostAttribs ---------------------------------------------------------------------
    mBoostAttribs.mvBoostBase_MaxBoostSpeed_BoostLinearDrag_NormalBoostHeightOffset.x = KF_BOOST_BASE;
    mBoostAttribs.mvBoostBase_MaxBoostSpeed_BoostLinearDrag_NormalBoostHeightOffset.y = KF_MAX_BOOST_SPEED;
    mBoostAttribs.mvBoostBase_MaxBoostSpeed_BoostLinearDrag_NormalBoostHeightOffset.z = KF_BOOST_LINEAR_DRAG;
    mBoostAttribs.mvBoostBase_MaxBoostSpeed_BoostLinearDrag_NormalBoostHeightOffset.w = KF_NORMAL_BOOST_HEIGHT_OFFSET;

    mBoostAttribs.mvNormalBoostAcceleration_BoostKickMaxStartSpeed_BoostKickMaxTime_BoostKickMinTime.x = KF_NORMAL_BOOST_ACCELERATION;
    mBoostAttribs.mvNormalBoostAcceleration_BoostKickMaxStartSpeed_BoostKickMaxTime_BoostKickMinTime.y = KF_BOOST_KICK_MAX_START_SPEED;
    mBoostAttribs.mvNormalBoostAcceleration_BoostKickMaxStartSpeed_BoostKickMaxTime_BoostKickMinTime.z = KF_BOOST_KICK_MAX_TIME;
    mBoostAttribs.mvNormalBoostAcceleration_BoostKickMaxStartSpeed_BoostKickMaxTime_BoostKickMinTime.w = KF_BOOST_KICK_MIN_TIME;

    mBoostAttribs.mvBoostKickAcceleration_BoostKickHeightOffset.x = KF_BOOST_KICK_ACCELERATION;
    mBoostAttribs.mvBoostKickAcceleration_BoostKickHeightOffset.y = KF_BOOST_KICK_HEIGHT_OFFSET;

    // ---- the two tires + the validity byte --------------------------------------------------
    mFrontTireAttribs.PrepareDefaultFrontTire();                      // 0x825F4CB4  r3 == this+0x2D0
    mRearTireAttribs.PrepareDefaultRearTire();                        // 0x825F4CBC  r3 == this+0x310
    mbIsValid = false;                                                // 0x825F4CC0  stb r28,0x360(r31)
}

// @0x825F4CD8 (770 instrs)  BrnPhysics::Vehicle::VehicleAttribs::SetupAttribs(handling)
//
// THE STREAMED-ATTRIBUTE LOADER (attribs-data wave, 2026-08-09). Streams the whole per-car
// tuning set out of a loaded AttribSys handling record: for each of the eight sub-record
// RefSpecs it constructs the generated wrapper from the RefSpec's collection and lane-scatters
// the record into the packed destination registers (the same lvlx/vspltw/vrlimi128 single-lane
// insert idiom as EngineAttribs::InitializeFromAttribs; each insert de-SIMDs to one scalar copy).
//
// PROVENANCE: the whole body was symbolically emulated instruction-by-instruction from the raw
// image bytes (per-lane provenance tags on every store to `this`), so every source byte offset
// below is asm-EXACT, and the destination lane names are this type's own DWARF names. Source
// field names in the comments are the schema's names at those offsets (schema.vlt); two lanes
// where the shipped read disagrees with the schema's primary offset are flagged inline.
// The function is STRAIGHT-LINE except one branch (the CarAngularImpulseScale clamp below).
//
// ⚠️ LANES THE CONSOLE DOES NOT STREAM (left at their Construct values -- do NOT "complete"):
//   * mvLinearDrag_AngularDrag_HighSpeedAngularDamping_FrontWheelMass.y  (AngularDrag)
//   * mvSideForcePeakDriftAngle_SideForceMagnitude_NaturalDriftDecay_NaturalDriftDecayPower.w
//   * mvDriftPushScaleLimit_DriftPushBaseFactor_MaxPowerSlideFactor.x / .y
//   * the body-roll spring block (record +0x10..0x1C) and mBoostAttribs.miBoostRule
//   * miRaceCarID / mAttribsKey
//
// The DWARF passes the handling BY VALUE (PS3 6D41E0 `..12SetupAttribsEN6Attrib3Gen22
// physicsvehiclehandlingE`); spelled const-ref per the SimpleVehicleAttribs precedent -- the
// caller owns the explicit copy, and the console's tail-destroy of the by-value parameter
// becomes the caller's copy destroying at its own scope end.
void VehicleAttribs::SetupAttribs(const Attrib::Gen::physicsvehiclehandling& lrHandling)
{
    using Attrib::Gen::physicsvehiclebaseattribs;
    using Attrib::Gen::physicsvehiclecollisionattribs;
    using Attrib::Gen::physicsvehicleboostattribs;
    using Attrib::Gen::physicsvehiclebodyrollattribs;
    using Attrib::Gen::physicsvehiclesuspensionattribs;
    using Attrib::Gen::physicsvehiclesteeringattribs;
    using Attrib::Gen::physicsvehicledriftattribs;
    using Attrib::Gen::physicsvehicleengineattribs;

    #define BP_VA_SRC_F(base, byteOff) ((base)[(byteOff) >> 2])

    // ---- the base-attribs record (data+0xA8 RefSpec) ---------------------------------------
    {
        physicsvehiclebaseattribs lBase(
            const_cast<Attrib::Collection*>(
                const_cast<Attrib::RefSpec&>(lrHandling.PhysicsVehicleBaseAttribs())
                    .GetCollection()),
            NULL);
        const f32* lpData = static_cast<const f32*>(lBase.GetLayoutPointer());

        // 0x825F4D14/0x825F4D64: the brake curve (record +0x30 BrakeScaleToFactor x/y/z).
        mBaseAttribs.mBrakeScaleToFactorCurve.Construct();
        mBaseAttribs.mBrakeScaleToFactorCurve.Prepare(BP_VA_SRC_F(lpData, 0x30),
                                                      BP_VA_SRC_F(lpData, 0x34),
                                                      BP_VA_SRC_F(lpData, 0x38));

        mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x
            = BP_VA_SRC_F(lpData, 0x104);                             // DrivingMass
        // the single fdivs (f31 == 1.0, loaded @0x825F4D98): the reciprocal is LIVE, not baked.
        mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.y
            = 1.0f / BP_VA_SRC_F(lpData, 0x48);                       // 1 / TimeForFullBrake
        mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.z
            = BP_VA_SRC_F(lpData, 0xA8);                              // MaxSpeed
        mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.w
            = BP_VA_SRC_F(lpData, 0x11C);                             // DownForce

        // the wheel positions come through SWAPPED relative to the record (same as the
        // SimpleVehicleAttribs streamer): dest FRONT <- record+0x10, dest REAR <- record+0x00.
        mBaseAttribs.mFrontRightWheelPos = *reinterpret_cast<const Vector3*>(lpData + (0x10 >> 2)); // FrontRightWheelPosition
        mBaseAttribs.mRearRightWheelPos  = *reinterpret_cast<const Vector3*>(lpData + (0x00 >> 2)); // RearRightWheelPosition

        mBaseAttribs.mCOMOffset = *reinterpret_cast<const Vector3*>(lpData + (0x20 >> 2));          // CoMOffset
        mBaseAttribs.mCOMOffset.x = 0.0f;   // the x lane is FORCED to 0 (stack round-trip), as in the SVA streamer

        mBaseAttribs.mvDownForceZOffset_MagicBrakeFactorTurning_MagicBrakeFactorStraightLine_BrakeScaleToLockWheels.x
            = BP_VA_SRC_F(lpData, 0x118);                             // DownForceZOffset
        mBaseAttribs.mvDownForceZOffset_MagicBrakeFactorTurning_MagicBrakeFactorStraightLine_BrakeScaleToLockWheels.y
            = BP_VA_SRC_F(lpData, 0xAC);                              // MagicBrakeFactorTurning
        mBaseAttribs.mvDownForceZOffset_MagicBrakeFactorTurning_MagicBrakeFactorStraightLine_BrakeScaleToLockWheels.z
            = BP_VA_SRC_F(lpData, 0xB0);                              // MagicBrakeFactorStraightLine
        mBaseAttribs.mvDownForceZOffset_MagicBrakeFactorTurning_MagicBrakeFactorStraightLine_BrakeScaleToLockWheels.w
            = BP_VA_SRC_F(lpData, 0xC0);                              // LockBrakeScale

        // fsel: negative traction-line lengths clamp to 0 (same idiom as the SVA streamer).
        {
            const f32 lfLen = BP_VA_SRC_F(lpData, 0x44);              // TractionLineLength
            mBaseAttribs.mvTractionLineLength_LowSpeedDrivingMPH_LowSpeedTyreFrictionTractionControl_LowSpeedThrottleTractionControl.x
                = (lfLen >= 0.0f) ? lfLen : 0.0f;
        }
        mBaseAttribs.mvTractionLineLength_LowSpeedDrivingMPH_LowSpeedTyreFrictionTractionControl_LowSpeedThrottleTractionControl.y
            = BP_VA_SRC_F(lpData, 0xBC);                              // LowSpeedDrivingSpeed
        mBaseAttribs.mvTractionLineLength_LowSpeedDrivingMPH_LowSpeedTyreFrictionTractionControl_LowSpeedThrottleTractionControl.z
            = BP_VA_SRC_F(lpData, 0xB4);                              // LowSpeedTyreFrictionTractionControl
        mBaseAttribs.mvTractionLineLength_LowSpeedDrivingMPH_LowSpeedTyreFrictionTractionControl_LowSpeedThrottleTractionControl.w
            = BP_VA_SRC_F(lpData, 0xB8);                              // LowSpeedThrottleTractionControl

        // ⚠️ record +0xC4 is LinearDrag's shipped seat (the schema's ALT offset 196; the primary
        // +0x04 seat is NOT what the console reads). The .y lane (AngularDrag) is NOT streamed.
        mBaseAttribs.mvLinearDrag_AngularDrag_HighSpeedAngularDamping_FrontWheelMass.x
            = BP_VA_SRC_F(lpData, 0xC4);                              // LinearDrag (ALT seat 0xC4)
        mBaseAttribs.mvLinearDrag_AngularDrag_HighSpeedAngularDamping_FrontWheelMass.z
            = BP_VA_SRC_F(lpData, 0xC8);                              // HighSpeedAngularDamping
        mBaseAttribs.mvLinearDrag_AngularDrag_HighSpeedAngularDamping_FrontWheelMass.w
            = BP_VA_SRC_F(lpData, 0xCC);                              // FrontWheelMass

        mBaseAttribs.mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo.x
            = BP_VA_SRC_F(lpData, 0x64);                              // RearWheelMass
        mBaseAttribs.mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo.y
            = BP_VA_SRC_F(lpData, 0xA0);                              // PowerToFront
        mBaseAttribs.mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo.z
            = BP_VA_SRC_F(lpData, 0x9C);                              // PowerToRear
        // (.w DownForceLiftCo is streamed from the DRIFT record below.)

        mBaseAttribs.mvPitchDampingOnTakeOff_YawDampingOnTakeOff_RollDampingOnTakeOff_RollLimitOnTakeOff.x
            = BP_VA_SRC_F(lpData, 0xA4);                              // PitchDampingOnTakeOff
        mBaseAttribs.mvPitchDampingOnTakeOff_YawDampingOnTakeOff_RollDampingOnTakeOff_RollLimitOnTakeOff.y
            = BP_VA_SRC_F(lpData, 0x40);                              // YawDampingOnTakeOff
        mBaseAttribs.mvPitchDampingOnTakeOff_YawDampingOnTakeOff_RollDampingOnTakeOff_RollLimitOnTakeOff.z
            = BP_VA_SRC_F(lpData, 0x60);                              // RollDampingOnTakeOff
        mBaseAttribs.mvPitchDampingOnTakeOff_YawDampingOnTakeOff_RollDampingOnTakeOff_RollLimitOnTakeOff.w
            = BP_VA_SRC_F(lpData, 0x5C);                              // RollLimitOnTakeOff

        mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.x
            = BP_VA_SRC_F(lpData, 0x54);                              // SurfaceFrontGripFactor
        mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.y
            = BP_VA_SRC_F(lpData, 0x50);                              // SurfaceRearGripFactor
        mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.z
            = BP_VA_SRC_F(lpData, 0x4C);                              // SurfaceRoughnessFactor
        mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.w
            = BP_VA_SRC_F(lpData, 0x58);                              // SurfaceDragFactor

        // crash-extra factors: the x (pitch) lane is loaded from record +0x128, then
        // OVERWRITTEN with 0 -- both stores are the console's own.
        mBaseAttribs.mCrashExtraVelocityFactors.x = BP_VA_SRC_F(lpData, 0x128);   // CrashExtraPitchVelocityFactor
        mBaseAttribs.mCrashExtraVelocityFactors.y = BP_VA_SRC_F(lpData, 0x120);   // CrashExtraYawVelocityFactor
        mBaseAttribs.mCrashExtraVelocityFactors.z = BP_VA_SRC_F(lpData, 0x124);   // CrashExtraRollVelocityFactor
        mBaseAttribs.mCrashExtraVelocityFactors.w = BP_VA_SRC_F(lpData, 0x12C);   // CrashExtraLinearVelocityFactor
        mBaseAttribs.mCrashExtraVelocityFactors.x = 0.0f;                          // the pitch lane is zeroed

        mBaseAttribs.mDrivetimeDeformLimits.x = BP_VA_SRC_F(lpData, 0x108);       // DriveTimeDeformLimitX
        mBaseAttribs.mDrivetimeDeformLimits.y = BP_VA_SRC_F(lpData, 0x114);       // DriveTimeDeformLimitNegY
        mBaseAttribs.mDrivetimeDeformLimits.z = BP_VA_SRC_F(lpData, 0x10C);       // DriveTimeDeformLimitPosZ
        mBaseAttribs.mDrivetimeDeformLimits.w = BP_VA_SRC_F(lpData, 0x110);       // DriveTimeDeformLimitNegZ

        // ⚠️ AS SHIPPED: the drift block's TorqueKickFromGasLetOff lane is streamed from the
        // BASE record's word +0x20 -- the word the schema names CoMOffset.x (whose own dest
        // lane above is force-zeroed). Transcribed exactly; do not "fix" to a drift field.
        mDriftAttribs.mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping.z
            = BP_VA_SRC_F(lpData, 0x20);

        // 0x825F5138/0x825F5144: the two per-car tire scatters (REAL -- Wheel.cpp).
        mFrontTireAttribs.PrepareFrontTire(lBase);                    // this+0x2D0
        mRearTireAttribs.PrepareRearTire(lBase);                      // this+0x310
    }

    // ---- the collision record (data+0x60 RefSpec) ------------------------------------------
    {
        physicsvehiclecollisionattribs lColl(
            const_cast<Attrib::Collection*>(
                const_cast<Attrib::RefSpec&>(lrHandling.PhysicsVehicleCollisionAttribs())
                    .GetCollection()),
            NULL);
        const f32* lpData = static_cast<const f32*>(lColl.GetLayoutPointer());

        // 0x825F51A0 `vmulfp128 v0, splat(record+0x00), [unk_83017FE0]`: the record's crash
        // speed is authored in MPH and stored in m/s. unk_83017FE0 is HOMED: .bss, splatted at
        // static-init by the writer @0x82C6D160 from flt_82F31928 == 0.44704 == KF_MPH_TO_MPS.
        mCollisionAttribs.mvCrashSpeedMPS_CarAngularImpulseScale_Spare_Spare.x
            = BP_VA_SRC_F(lpData, 0x00) * KF_MPH_TO_MPS;              // CrashSpeed (MPH -> m/s)

        // the ONE branch in the whole function (vcmpgtfp/beq @0x825F51E0): an authored scale
        // above the 0.01 epsilon (flt_82002138) is clamped to [0,1]; otherwise the default 1.0
        // (f31) stands -- i.e. "unset" records get full scale.
        {
            const f32 lfScale = BP_VA_SRC_F(lpData, 0x04);            // CarAngularImpulseScale
            f32 lfLane;
            if (lfScale > 0.01f)                                      // flt_82002138
            {
                lfLane = lfScale;
                if (lfLane < 0.0f) lfLane = 0.0f;                     // vmaxfp vs vspltisw 0
                if (lfLane > 1.0f) lfLane = 1.0f;                     // vminfp vs vcfsx(1)
            }
            else
            {
                lfLane = 1.0f;                                        // f31 == flt_82001C98
            }
            mCollisionAttribs.mvCrashSpeedMPS_CarAngularImpulseScale_Spare_Spare.y = lfLane;
        }
    }

    // ---- the boost record (data+0x78 RefSpec) ----------------------------------------------
    {
        physicsvehicleboostattribs lBoost(
            const_cast<Attrib::Collection*>(
                const_cast<Attrib::RefSpec&>(lrHandling.PhysicsVehicleBoostAttribs())
                    .GetCollection()),
            NULL);
        const f32* lpData = static_cast<const f32*>(lBoost.GetLayoutPointer());

        mBoostAttribs.mvBoostBase_MaxBoostSpeed_BoostLinearDrag_NormalBoostHeightOffset.x
            = BP_VA_SRC_F(lpData, 0x28);                              // BoostBase
        mBoostAttribs.mvBoostBase_MaxBoostSpeed_BoostLinearDrag_NormalBoostHeightOffset.y
            = BP_VA_SRC_F(lpData, 0x00);                              // MaxBoostSpeed
        // (.z BoostLinearDrag is DERIVED below, after the drift block, from the streamed
        //  LinearDrag; .w here:)
        mBoostAttribs.mvBoostBase_MaxBoostSpeed_BoostLinearDrag_NormalBoostHeightOffset.w
            = BP_VA_SRC_F(lpData, 0x24);                              // BoostHeightOffset

        mBoostAttribs.mvNormalBoostAcceleration_BoostKickMaxStartSpeed_BoostKickMaxTime_BoostKickMinTime.x
            = BP_VA_SRC_F(lpData, 0x2C);                              // BoostAcceleration
        mBoostAttribs.mvNormalBoostAcceleration_BoostKickMaxStartSpeed_BoostKickMaxTime_BoostKickMinTime.y
            = BP_VA_SRC_F(lpData, 0x14);                              // BoostKickMaxStartSpeed
        mBoostAttribs.mvNormalBoostAcceleration_BoostKickMaxStartSpeed_BoostKickMaxTime_BoostKickMinTime.z
            = BP_VA_SRC_F(lpData, 0x10);                              // BoostKickMaxTime
        mBoostAttribs.mvNormalBoostAcceleration_BoostKickMaxStartSpeed_BoostKickMaxTime_BoostKickMinTime.w
            = BP_VA_SRC_F(lpData, 0x0C);                              // BoostKickMinTime

        mBoostAttribs.mvBoostKickAcceleration_BoostKickHeightOffset.x
            = BP_VA_SRC_F(lpData, 0x1C);                              // BoostKickAcceleration
        mBoostAttribs.mvBoostKickAcceleration_BoostKickHeightOffset.y
            = BP_VA_SRC_F(lpData, 0x18);                              // BoostKickHeightOffset
        // (miBoostRule / BoostKickTime / BoostKick / the Blue* block are NOT streamed.)
    }

    // ---- the body-roll record (data+0x90 RefSpec) ------------------------------------------
    {
        physicsvehiclebodyrollattribs lRoll(
            const_cast<Attrib::Collection*>(
                const_cast<Attrib::RefSpec&>(lrHandling.PhysicsVehicleBodyRollAttribs())
                    .GetCollection()),
            NULL);
        const f32* lpData = static_cast<const f32*>(lRoll.GetLayoutPointer());

        mBodyRollAttribs.mvWeightTransferDecayX_WeightTransferDecayZ_FactorOfWeightX_FactorOfWeightZ.x
            = BP_VA_SRC_F(lpData, 0x0C);                              // WeightTransferDecayX
        mBodyRollAttribs.mvWeightTransferDecayX_WeightTransferDecayZ_FactorOfWeightX_FactorOfWeightZ.y
            = BP_VA_SRC_F(lpData, 0x08);                              // WeightTransferDecayZ
        mBodyRollAttribs.mvWeightTransferDecayX_WeightTransferDecayZ_FactorOfWeightX_FactorOfWeightZ.z
            = BP_VA_SRC_F(lpData, 0x24);                              // FactorOfWeightX
        mBodyRollAttribs.mvWeightTransferDecayX_WeightTransferDecayZ_FactorOfWeightX_FactorOfWeightZ.w
            = BP_VA_SRC_F(lpData, 0x20);                              // FactorOfWeightZ

        mBodyRollAttribs.mvWheelLongForceHeightOffset_WheelLatForceHeightOffset.x
            = BP_VA_SRC_F(lpData, 0x00);                              // WheelLongForceHeightOffset
        mBodyRollAttribs.mvWheelLongForceHeightOffset_WheelLatForceHeightOffset.y
            = BP_VA_SRC_F(lpData, 0x04);                              // WheelLatForceHeightOffset
        // (the spring block, record +0x10..0x1C, is NOT streamed.)
    }

    // ---- the suspension record (data+0x00 RefSpec) -----------------------------------------
    {
        physicsvehiclesuspensionattribs lSusp(
            const_cast<Attrib::Collection*>(
                const_cast<Attrib::RefSpec&>(lrHandling.PhysicsVehicleSuspensionAttribs())
                    .GetCollection()),
            NULL);
        const f32* lpData = static_cast<const f32*>(lSusp.GetLayoutPointer());

        mSuspensionAttribs.mvRestDisplacement_Dampening_UpwardMovement_DownwardMovement.x
            = BP_VA_SRC_F(lpData, 0x0C);                              // SpringLength
        mSuspensionAttribs.mvRestDisplacement_Dampening_UpwardMovement_DownwardMovement.y
            = BP_VA_SRC_F(lpData, 0x30);                              // Dampening
        mSuspensionAttribs.mvRestDisplacement_Dampening_UpwardMovement_DownwardMovement.z
            = BP_VA_SRC_F(lpData, 0x00);                              // UpwardMovement
        mSuspensionAttribs.mvRestDisplacement_Dampening_UpwardMovement_DownwardMovement.w
            = BP_VA_SRC_F(lpData, 0x2C);                              // DownwardMovement

        mSuspensionAttribs.mvFrontWheelHeightOffset_RearWheelHeightOffset_InAirDamping_MaxPitchDampingOnLanding.x
            = BP_VA_SRC_F(lpData, 0x28);                              // FrontHeight
        mSuspensionAttribs.mvFrontWheelHeightOffset_RearWheelHeightOffset_InAirDamping_MaxPitchDampingOnLanding.y
            = BP_VA_SRC_F(lpData, 0x10);                              // RearHeight
        mSuspensionAttribs.mvFrontWheelHeightOffset_RearWheelHeightOffset_InAirDamping_MaxPitchDampingOnLanding.z
            = BP_VA_SRC_F(lpData, 0x24);                              // InAirDamping
        mSuspensionAttribs.mvFrontWheelHeightOffset_RearWheelHeightOffset_InAirDamping_MaxPitchDampingOnLanding.w
            = BP_VA_SRC_F(lpData, 0x20);                              // MaxPitchDampingOnLanding

        mSuspensionAttribs.mvMaxYawDampingOnLanding_MaxRollDampingOnLanding_MaxVertVelocityDampingOnLanding_TimeToDampAfterLanding.x
            = BP_VA_SRC_F(lpData, 0x14);                              // MaxYawDampingOnLanding
        mSuspensionAttribs.mvMaxYawDampingOnLanding_MaxRollDampingOnLanding_MaxVertVelocityDampingOnLanding_TimeToDampAfterLanding.y
            = BP_VA_SRC_F(lpData, 0x1C);                              // MaxRollDampingOnLanding
        mSuspensionAttribs.mvMaxYawDampingOnLanding_MaxRollDampingOnLanding_MaxVertVelocityDampingOnLanding_TimeToDampAfterLanding.z
            = BP_VA_SRC_F(lpData, 0x18);                              // MaxVertVelocityDampingOnLanding
        mSuspensionAttribs.mvMaxYawDampingOnLanding_MaxRollDampingOnLanding_MaxVertVelocityDampingOnLanding_TimeToDampAfterLanding.w
            = BP_VA_SRC_F(lpData, 0x04);                              // TimeToDampAfterLanding
        // (record +0x08 Strength is NOT streamed.)
    }

    // ---- the steering record (data+0x18 RefSpec) -------------------------------------------
    {
        physicsvehiclesteeringattribs lSteer(
            const_cast<Attrib::Collection*>(
                const_cast<Attrib::RefSpec&>(lrHandling.PhysicsVehicleSteeringAttribs())
                    .GetCollection()),
            NULL);
        const f32* lpData = static_cast<const f32*>(lSteer.GetLayoutPointer());

        mSteeringAttribs.mvReactionPerSec_SpeedForMinAngle_SpeedForMinAngleRecip_MinAngle.y
            = BP_VA_SRC_F(lpData, 0x08);                              // SpeedForMinAngle
        mSteeringAttribs.mvReactionPerSec_SpeedForMinAngle_SpeedForMinAngleRecip_MinAngle.w
            = BP_VA_SRC_F(lpData, 0x10);                              // MinAngle
        // the two LIVE fdivs (f31 == 1.0): the reaction rate is 1/TimeForLock, and the
        // min-angle speed's reciprocal rides in its own lane.
        mSteeringAttribs.mvReactionPerSec_SpeedForMinAngle_SpeedForMinAngleRecip_MinAngle.z
            = 1.0f / BP_VA_SRC_F(lpData, 0x08);                       // 1 / SpeedForMinAngle
        mSteeringAttribs.mvReactionPerSec_SpeedForMinAngle_SpeedForMinAngleRecip_MinAngle.x
            = 1.0f / BP_VA_SRC_F(lpData, 0x00);                       // 1 / TimeForLock

        mSteeringAttribs.mvMaxAngle_StraightReactionBias.x
            = BP_VA_SRC_F(lpData, 0x14);                              // MaxAngle
        mSteeringAttribs.mvMaxAngle_StraightReactionBias.y
            = BP_VA_SRC_F(lpData, 0x04);                              // StraightReactionBias
    }

    // ---- the drift record (data+0x48 RefSpec) ----------------------------------------------
    {
        physicsvehicledriftattribs lDrift(
            const_cast<Attrib::Collection*>(
                const_cast<Attrib::RefSpec&>(lrHandling.PhysicsVehicleDriftAttribs())
                    .GetCollection()),
            NULL);
        const f32* lpData = static_cast<const f32*>(lDrift.GetLayoutPointer());

        mBaseAttribs.mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo.w
            = BP_VA_SRC_F(lpData, 0x10);   // ⚠️ AS SHIPPED: DownForceLiftCo <- the drift record's
                                           // +0x10 (the word the schema names WheelSlip)

        mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.x
            = BP_VA_SRC_F(lpData, 0x4C);                              // MinSpeedForDrift
        mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.y
            = BP_VA_SRC_F(lpData, 0x1C);                              // SteeringDriftScaleFactor
        mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.z
            = BP_VA_SRC_F(lpData, 0x88);                              // CounterSteeringDriftScaleFactor
        mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.w
            = BP_VA_SRC_F(lpData, 0x94);                              // BaseCounterSteeringDriftScaleFactor

        mDriftAttribs.mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale.x
            = BP_VA_SRC_F(lpData, 0x90);                              // BrakingDriftScaleFactor
        mDriftAttribs.mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale.y
            = BP_VA_SRC_F(lpData, 0x6C);                              // GasDriftScaleFactor
        mDriftAttribs.mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale.z
            = BP_VA_SRC_F(lpData, 0x14);                              // TimeToCapScale
        mDriftAttribs.mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale.w
            = BP_VA_SRC_F(lpData, 0x8C);                              // CappedScale

        mDriftAttribs.mvDriftTorqueFallOff_GripFromSteering_GripFromBrake_TimeForNaturalDrift.x
            = BP_VA_SRC_F(lpData, 0x78);                              // DriftTorqueFallOff
        mDriftAttribs.mvDriftTorqueFallOff_GripFromSteering_GripFromBrake_TimeForNaturalDrift.y
            = BP_VA_SRC_F(lpData, 0x60);                              // GripFromSteering
        mDriftAttribs.mvDriftTorqueFallOff_GripFromSteering_GripFromBrake_TimeForNaturalDrift.z
            = BP_VA_SRC_F(lpData, 0x68);                              // GripFromBrake
        mDriftAttribs.mvDriftTorqueFallOff_GripFromSteering_GripFromBrake_TimeForNaturalDrift.w
            = BP_VA_SRC_F(lpData, 0x18);                              // TimeForNaturalDrift

        mDriftAttribs.mvNeutralTimeToReduceDrift_SideForceDriftScaleCutOff_SideForceDriftAngleCutOff_SideForceDriftSpeedCutOff.x
            = BP_VA_SRC_F(lpData, 0x34);                              // NeutralTimeToReduceDrift
        mDriftAttribs.mvNeutralTimeToReduceDrift_SideForceDriftScaleCutOff_SideForceDriftAngleCutOff_SideForceDriftSpeedCutOff.y
            = BP_VA_SRC_F(lpData, 0x30);                              // SideForceDirftScaleCutOff [sic]
        mDriftAttribs.mvNeutralTimeToReduceDrift_SideForceDriftScaleCutOff_SideForceDriftAngleCutOff_SideForceDriftSpeedCutOff.z
            = BP_VA_SRC_F(lpData, 0x2C);                              // SideForceDriftAngleCutOff
        mDriftAttribs.mvNeutralTimeToReduceDrift_SideForceDriftScaleCutOff_SideForceDriftAngleCutOff_SideForceDriftSpeedCutOff.w
            = BP_VA_SRC_F(lpData, 0x28);                              // SideForceDriftSpeedCutOff

        mDriftAttribs.mvSideForcePeakDriftAngle_SideForceMagnitude_NaturalDriftDecay_NaturalDriftDecayPower.x
            = BP_VA_SRC_F(lpData, 0x20);                              // SideForcePeakDriftAngle
        mDriftAttribs.mvSideForcePeakDriftAngle_SideForceMagnitude_NaturalDriftDecay_NaturalDriftDecayPower.y
            = BP_VA_SRC_F(lpData, 0x24);                              // SideForceMagnitude
        mDriftAttribs.mvSideForcePeakDriftAngle_SideForceMagnitude_NaturalDriftDecay_NaturalDriftDecayPower.z
            = BP_VA_SRC_F(lpData, 0x48);                              // NaturalDriftScaleDecay
        // (.w NaturalDriftDecayPower is NOT streamed.)

        mDriftAttribs.mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping.x
            = BP_VA_SRC_F(lpData, 0x3C);                              // NaturalYawTorque
        mDriftAttribs.mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping.y
            = BP_VA_SRC_F(lpData, 0x38);                              // NaturalYawTorqueCutOffAngle
        // (.z TorqueKickFromGasLetOff was streamed from the BASE record above.)
        mDriftAttribs.mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping.w
            = BP_VA_SRC_F(lpData, 0x7C);                              // DriftSidewaysDamping

        mDriftAttribs.mvDriftAngularDamping_MaxDriftAngle_CounterSteerTorqueScaleFactor_DriftPushTime.x
            = BP_VA_SRC_F(lpData, 0x84);                              // DriftAngularDamping
        mDriftAttribs.mvDriftAngularDamping_MaxDriftAngle_CounterSteerTorqueScaleFactor_DriftPushTime.y
            = BP_VA_SRC_F(lpData, 0x80);                              // DriftMaxAngle
        mDriftAttribs.mvDriftAngularDamping_MaxDriftAngle_CounterSteerTorqueScaleFactor_DriftPushTime.z
            = BP_VA_SRC_F(lpData, 0x74);                              // ForcedDriftStartSlip
        mDriftAttribs.mvDriftAngularDamping_MaxDriftAngle_CounterSteerTorqueScaleFactor_DriftPushTime.w
            = BP_VA_SRC_F(lpData, 0x50);                              // InitialDriftPushTime

        mDriftAttribs.mvDriftPushScaleLimit_DriftPushBaseFactor_MaxPowerSlideFactor.z
            = BP_VA_SRC_F(lpData, 0x58);                              // InitialDriftPushDynamicInc
        // (.x DriftPushScaleLimit / .y DriftPushBaseFactor are NOT streamed.)

        // 0x825F5578/0x825F5580: the drift yaw-torque curve (record +0x00 DriftScaleToYawTorque x/y/z).
        mDriftAttribs.mDriftScaleToYawTorque.Construct();
        mDriftAttribs.mDriftScaleToYawTorque.Prepare(BP_VA_SRC_F(lpData, 0x00),
                                                     BP_VA_SRC_F(lpData, 0x04),
                                                     BP_VA_SRC_F(lpData, 0x08));
    }

    // 0x825F5888 `vmulfp128`: BoostLinearDrag is DERIVED, not authored -- 0.75 (flt_82004018)
    // of the LinearDrag lane streamed above.
    mBoostAttribs.mvBoostBase_MaxBoostSpeed_BoostLinearDrag_NormalBoostHeightOffset.z
        = mBaseAttribs.mvLinearDrag_AngularDrag_HighSpeedAngularDamping_FrontWheelMass.x * 0.75f;

    // ---- the engine record (data+0x30 RefSpec) ---------------------------------------------
    {
        physicsvehicleengineattribs lEng(
            const_cast<Attrib::Collection*>(
                const_cast<Attrib::RefSpec&>(lrHandling.PhysicsVehicleEngineAttribs())
                    .GetCollection()),
            NULL);
        // the console passes the WRAPPER and the callee reads *(a2+4); the committed
        // InitializeFromAttribs body takes the data pointer directly, so hand it the
        // wrapper's layout block -- behaviour-identical.
        mEngineAttribs.InitializeFromAttribs(lEng.GetLayoutPointer());
    }

    #undef BP_VA_SRC_F

    mbIsValid = true;                                                 // li 1; stb -> +0x360
}

// @0x825F6298 (40 instrs)  BrnPhysics::Vehicle::VehicleAttribs::SetupAttribsForDonutAI
//
// ⚠️ CORRECTED with the layout de-fork. The whole function is five stores, and the asm names its
// destinations by absolute offset:
//     [this+0x110].x = 0.0f              (flt_82001CC0)
//     [this+0x1B0].x = flt_8205820C
//     PrepareFrontTireForDonutAI(this+0x2D0)
//     PrepareRearTireForDonutAI (this+0x310)
//     [this+0x70].x  = flt_8205820C      (the SAME register, one lfs, two stores)
//
// With mDriftAttribs @0x100 and mEngineAttribs @0x190 those are MinSpeedForDrift (drift+0x10),
// MaxTorque (engine+0x20) and Mass (base+0x70) -- a donut AI that may drift at any speed, with the
// torque and the mass both set from one constant.
//
// The previous body, written against the transposed layout, called SetMinSpeedForDrift(700) and
// SetDifferential(0) instead: a car that must reach 700 mph to drift and has no drive at all. The
// committed comment there argued the +0x110 write was "EngineAttribs+0x10 (SetDifferential), NOT
// +0x20 (SetMaxTorque)"; with the engine block at its real base the write IS +0x20.
void VehicleAttribs::SetupAttribsForDonutAI()
{
    mDriftAttribs.SetMinSpeedForDrift(KVF(KF_DONUT_AI_MIN_SPEED_FOR_DRIFT));   // [this+0x110].x
    mEngineAttribs.SetMaxTorque(KVF(KF_DONUT_AI_MASS_AND_MAX_TORQUE));         // [this+0x1B0].x
    mFrontTireAttribs.PrepareFrontTireForDonutAI();                            // this+0x2D0
    mRearTireAttribs.PrepareRearTireForDonutAI();                              // this+0x310
    mBaseAttribs.SetMass(KVF(KF_DONUT_AI_MASS_AND_MAX_TORQUE));                // [this+0x70].x
}

// @0x825B2B68 (92 instrs)  BrnPhysics::Vehicle::VehicleAttribs::operator=
//
// The X360 leaf copies the whole attribute block from source to destination: a memcpy of the
// leading 0xE0 bytes (== sizeof VehicleBaseAttribs) followed by per-sub-aggregate copies for the
// steering/drift/engine blocks, the suspension/bodyroll/collision/boost span, and the two
// TireAttribs (the two trailing 8-store do/while loops copy 0x40 bytes each). Every member is a
// trivially-copyable POD slice, so a memberwise assignment is store-for-store equivalent to the
// blob copy. The Hex-Rays artifacts of the form *(*(a2+632)+24)=*(a2+632) are mis-rendered
// 16-byte vector stores, not pointer writes.
// AFTER the two tire copies the asm has three more stores: lwz/stw @0x350 (4-byte word copy),
// ld/std @0x358 (8-byte copy), and `li r10,1; stb r10,0x360` -- the byte at 0x360 (mbIsValid) is
// set to the CONSTANT 1, not copied (the Hex-Rays BYTE3(v32) rendering is the li-1 misread).
VehicleAttribs& VehicleAttribs::operator=(const VehicleAttribs& lrSource)
{
    mBaseAttribs       = lrSource.mBaseAttribs;
    mSteeringAttribs   = lrSource.mSteeringAttribs;
    mDriftAttribs      = lrSource.mDriftAttribs;
    mEngineAttribs     = lrSource.mEngineAttribs;
    mSuspensionAttribs = lrSource.mSuspensionAttribs;
    mBodyRollAttribs   = lrSource.mBodyRollAttribs;
    mCollisionAttribs  = lrSource.mCollisionAttribs;
    mBoostAttribs      = lrSource.mBoostAttribs;
    mFrontTireAttribs  = lrSource.mFrontTireAttribs;
    mRearTireAttribs   = lrSource.mRearTireAttribs;
    miRaceCarID        = lrSource.miRaceCarID;   // lwz/stw @0x350
    mAttribsKey        = lrSource.mAttribsKey;   // ld/std   @0x358
    mbIsValid          = true;                   // li 1; stb @0x360 (set unconditionally)
    return *this;
}

// @0x825F58E0 (622 instrs)  BrnPhysics::Vehicle::VehicleAttribs::SetupAttribsForAI
//
// Derive the plain-AI attribute set from a source set (the donut-LEAVE leg of
// SwitchAIDonuttingAttribs; VehiclePhysics::Prepare's AI seat). Shape: operator=(source)
// first, then a long fixed-constant respray of the base/steering/drift/bodyroll lanes with a
// handful of source-derived scalings, the two AI tire preps, and the engine block re-copied.
//
// Decoded from the X360 asm store-for-store (every `lfs`+`lvlx`/`vspltw`+`vrlimi128` lane
// insert dataflow-tracked through its stack slot; vrlimi masks 8/4/2/1 == lanes x/y/z/w).
// Every constant below was read out of the shipped image with the self-calibrating .id1
// reader (10/10 self-test). ⚠️ SEVERAL STORES ARE REDUNDANT BY CONSTRUCTION: operator=
// already copied the whole 0x370 block, and the console then RE-copies individual source
// lanes (+0xD0 surface factors, the whole suspension block, the boost block, the engine
// memcpy) -- inlined SetX(source.GetX()) patterns. They are transcribed as shipped, marked
// [re-copy]; removing them would not change the result.
namespace AIDefaults
{
const f32 KF_AI_DOWNFORCE                 = 30.0f;           // flt_82004F5C
const f32 KF_AI_MASS_PRESEED              = 2000.0f;         // stru_8208F640 (overwritten below
                                                             //   by source Mass -- as shipped)
const f32 KF_AI_MAX_SPEED_PRESEED         = 200.0f;          // flt_8201A1F0 (overwritten below
                                                             //   by 1.2 * source MaxSpeed)
const f32 KF_AI_TIME_FOR_FULL_BRAKE_RECIP = 0.666666687f;    // flt_8200AECC
const f32 KF_AI_BRAKE_SCALE_TO_LOCK       = 1.0f;            // flt_82001C98
const f32 KF_AI_TRACTION_LINE_LENGTH      = 0.100000001f;    // flt_82004014
const f32 KF_AI_LOW_SPEED_DRIVING_MPH     = 70.0f;           // flt_820051BC
const f32 KF_AI_LOW_SPEED_TYRE_FRICTION_TC = 25.0f;          // flt_82004FD8
const f32 KF_AI_LOW_SPEED_THROTTLE_TC     = 20.0f;           // flt_8208F9D4
const f32 KF_AI_HIGH_SPEED_ANGULAR_DAMPING = 0.100000001f;   // flt_82004014
const f32 KF_AI_WHEEL_MASS                = 20.0f;           // flt_8208F9D4 (front AND rear)
const f32 KF_AI_PITCH_DAMPING_ON_TAKEOFF  = 0.699999988f;    // flt_82004C68
const f32 KF_AI_YAW_DAMPING_ON_TAKEOFF    = 0.100000001f;    // flt_82004014
const f32 KF_AI_ROLL_DAMPING_ON_TAKEOFF   = 0.25f;           // flt_8208F834
const f32 KF_AI_CRASH_EXTRA_VEL_FACTOR    = 0.300000012f;    // flt_82004740 (lanes y/z/w)
const f32 KF_AI_DEFORM_LIMIT_X            = 0.300000012f;    // flt_82004740
const f32 KF_AI_DEFORM_LIMIT_Y            = 0.200000003f;    // flt_82004744
const f32 KF_AI_DEFORM_LIMIT_ZW           = 0.400000006f;    // flt_8200473C
const f32 KF_AI_MAX_SPEED_SCALE           = 1.20000005f;     // flt_82009B84
const f32 KF_AI_MAX_BOOST_SPEED_SCALE     = 1.10000002f;     // flt_82004A1C
const f32 KF_AI_BOOST_LINEAR_DRAG_SCALE   = 0.75f;           // flt_82004018 (x this LinearDrag)
const f32 KF_AI_DOWNFORCE_LIFT_CO         = 0.239999995f;    // flt_8200D57C
const f32 KF_AI_STEERING_MIN_ANGLE        = 5.0f;            // flt_8200426C
const f32 KF_AI_STEERING_MAX_ANGLE        = 30.0f;           // flt_82004F5C
const f32 KF_AI_SPEED_FOR_MIN_ANGLE       = 70.0f;           // flt_820051BC
const f32 KF_AI_STEERING_REACTION_PER_SEC = 10.0f;           // flt_82004A20
const f32 KF_AI_MIN_SPEED_FOR_DRIFT       = 20.0f;           // flt_8208F9D4
const f32 KF_AI_STEERING_DRIFT_SCALE      = 0.800000012f;    // flt_8208F9C8
const f32 KF_AI_COUNTER_STEERING_DRIFT_SCALE = 2.4000001f;   // flt_82097A34
const f32 KF_AI_BASE_COUNTER_STEERING_DRIFT_SCALE = 0.400000006f;  // flt_8200473C
const f32 KF_AI_DRIFT_TORQUE_FALL_OFF     = 0.100000001f;    // flt_82004014
const f32 KF_AI_NEUTRAL_TIME_TO_REDUCE_DRIFT = 0.5f;         // flt_82001DA0
const f32 KF_AI_SIDE_FORCE_DRIFT_SCALE_CUTOFF = 15.0f;       // flt_820047C4
const f32 KF_AI_SIDE_FORCE_MAGNITUDE      = 0.699999988f;    // flt_82004C68
const f32 KF_AI_NATURAL_DRIFT_DECAY       = 0.939999998f;    // flt_8200D58C
const f32 KF_AI_DRIFT_SIDEWAYS_DAMPING    = 0.0199999996f;   // flt_82005574
const f32 KF_AI_DRIFT_ANGULAR_DAMPING     = 0.159999996f;    // flt_82097A3C
const f32 KF_AI_COUNTER_STEER_TORQUE_SCALE = 0.5f;           // flt_82001DA0
// The two InterpedParam3 curve domains (parameters named lParamA/B/C in the DWARF).
const f32 KF_AI_BRAKE_CURVE_A             = 0.0f;            // flt_82001CC0
const f32 KF_AI_BRAKE_CURVE_B             = 5.0f;            // flt_8200426C
const f32 KF_AI_BRAKE_CURVE_C             = 10.0f;           // flt_82004A20
const f32 KF_AI_DRIFT_YAW_TORQUE_CURVE_A  = 20000.0f;        // flt_820468AC
const f32 KF_AI_DRIFT_YAW_TORQUE_CURVE_B  = 35000.0f;        // flt_82097A40
const f32 KF_AI_DRIFT_YAW_TORQUE_CURVE_C  = 50000.0f;        // flt_82097A44
}

void VehicleAttribs::SetupAttribsForAI(VehicleAttribs* lpSource)
{
    using namespace AIDefaults;

    // 0x825F5900: copy the whole block, then respray.
    *this = *lpSource;

    // 0x825F5908..0x825F5938: the brake-scale curve (this+0x60).
    mBaseAttribs.mBrakeScaleToFactorCurve.Construct();
    mBaseAttribs.mBrakeScaleToFactorCurve.Prepare(KF_AI_BRAKE_CURVE_A, KF_AI_BRAKE_CURVE_B,
                                                  KF_AI_BRAKE_CURVE_C);

    // ---- mBaseAttribs (+0x70..+0xD0) -- fixed AI lanes -------------------------------------
    mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.w = KF_AI_DOWNFORCE;      // vrlimi(1)
    mBaseAttribs.mvDownForceZOffset_MagicBrakeFactorTurning_MagicBrakeFactorStraightLine_BrakeScaleToLockWheels.x = 0.0f;
    mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x = KF_AI_MASS_PRESEED;   // (overwritten below)
    mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.z = KF_AI_MAX_SPEED_PRESEED;  // (overwritten below)
    mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.y = KF_AI_TIME_FOR_FULL_BRAKE_RECIP;
    mBaseAttribs.mCOMOffset = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };   // 0x825F5A34 (re-copied below)
    mBaseAttribs.mvDownForceZOffset_MagicBrakeFactorTurning_MagicBrakeFactorStraightLine_BrakeScaleToLockWheels.w = KF_AI_BRAKE_SCALE_TO_LOCK;
    mBaseAttribs.mvTractionLineLength_LowSpeedDrivingMPH_LowSpeedTyreFrictionTractionControl_LowSpeedThrottleTractionControl.x = KF_AI_TRACTION_LINE_LENGTH;
    mBaseAttribs.mvTractionLineLength_LowSpeedDrivingMPH_LowSpeedTyreFrictionTractionControl_LowSpeedThrottleTractionControl.y = KF_AI_LOW_SPEED_DRIVING_MPH;
    mBaseAttribs.mvTractionLineLength_LowSpeedDrivingMPH_LowSpeedTyreFrictionTractionControl_LowSpeedThrottleTractionControl.z = KF_AI_LOW_SPEED_TYRE_FRICTION_TC;
    mBaseAttribs.mvTractionLineLength_LowSpeedDrivingMPH_LowSpeedTyreFrictionTractionControl_LowSpeedThrottleTractionControl.w = KF_AI_LOW_SPEED_THROTTLE_TC;
    mBaseAttribs.mvLinearDrag_AngularDrag_HighSpeedAngularDamping_FrontWheelMass.z = KF_AI_HIGH_SPEED_ANGULAR_DAMPING;
    mBaseAttribs.mvLinearDrag_AngularDrag_HighSpeedAngularDamping_FrontWheelMass.w = KF_AI_WHEEL_MASS;
    mBaseAttribs.mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo.x = KF_AI_WHEEL_MASS;
    mBaseAttribs.mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo.y = 0.0f;   // PowerToFront
    mBaseAttribs.mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo.z = 1.0f;   // PowerToRear
    mBaseAttribs.mvLinearDrag_AngularDrag_HighSpeedAngularDamping_FrontWheelMass.x = 0.0f;  // LinearDrag
    mBaseAttribs.mvPitchDampingOnTakeOff_YawDampingOnTakeOff_RollDampingOnTakeOff_RollLimitOnTakeOff.x = KF_AI_PITCH_DAMPING_ON_TAKEOFF;
    mBaseAttribs.mvPitchDampingOnTakeOff_YawDampingOnTakeOff_RollDampingOnTakeOff_RollLimitOnTakeOff.y = KF_AI_YAW_DAMPING_ON_TAKEOFF;
    mBaseAttribs.mvPitchDampingOnTakeOff_YawDampingOnTakeOff_RollDampingOnTakeOff_RollLimitOnTakeOff.z = KF_AI_ROLL_DAMPING_ON_TAKEOFF;

    // 0x825F5BE4..0x825F5C90: [re-copy] the four surface-factor lanes from the source, one
    // vspltw/vrlimi pair per lane (redundant after operator= -- transcribed as shipped).
    mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.x
        = lpSource->mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.x;
    mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.y
        = lpSource->mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.y;
    mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.z
        = lpSource->mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.z;
    mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.w
        = lpSource->mBaseAttribs.mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor.w;

    // 0x825F5C94..0x825F5CD0: the source-derived seats.
    mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x
        = lpSource->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x;   // Mass [re-copy]
    mBaseAttribs.mCOMOffset = lpSource->mBaseAttribs.mCOMOffset;                      // [re-copy]
    mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.z
        = lpSource->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.z
          * KF_AI_MAX_SPEED_SCALE;                              // MaxSpeed = 1.2 * source's
    mBaseAttribs.mCrashExtraVelocityFactors = Vector3Plus{ 0.0f, KF_AI_CRASH_EXTRA_VEL_FACTOR,
                                                           KF_AI_CRASH_EXTRA_VEL_FACTOR,
                                                           KF_AI_CRASH_EXTRA_VEL_FACTOR };  // +0x30
    mBaseAttribs.mDrivetimeDeformLimits = Vector4{ KF_AI_DEFORM_LIMIT_X, KF_AI_DEFORM_LIMIT_Y,
                                                   KF_AI_DEFORM_LIMIT_ZW, KF_AI_DEFORM_LIMIT_ZW };  // +0x40

    // 0x825F5CD0..0x825F5CD8: the AI tire preps.
    mFrontTireAttribs.PrepareFrontTireForAI();                  // this+0x2D0
    mRearTireAttribs.PrepareRearTireForAI();                    // this+0x310

    // 0x825F5CDC..0x825F5D00: [re-copy] the whole BoostAttribs block (8 x ld/std, 0x40 bytes).
    mBoostAttribs = lpSource->mBoostAttribs;

    // 0x825F5D04..0x825F5DB8: MaxBoostSpeed = 1.1 * source's (the vspltw-lane-1 * splat(1.1)).
    mBoostAttribs.mvBoostBase_MaxBoostSpeed_BoostLinearDrag_NormalBoostHeightOffset.y
        = lpSource->mBoostAttribs.mvBoostBase_MaxBoostSpeed_BoostLinearDrag_NormalBoostHeightOffset.y
          * KF_AI_MAX_BOOST_SPEED_SCALE;
    mBoostAttribs.mvBoostKickAcceleration_BoostKickHeightOffset.y = 0.0f;   // +0x2B0 vrlimi(4)

    // ---- mBodyRollAttribs (+0x260/+0x270): all six live lanes zeroed ------------------------
    mBodyRollAttribs.mvWeightTransferDecayX_WeightTransferDecayZ_FactorOfWeightX_FactorOfWeightZ.w = 0.0f;
    mBodyRollAttribs.mvWeightTransferDecayX_WeightTransferDecayZ_FactorOfWeightX_FactorOfWeightZ.z = 0.0f;
    mBodyRollAttribs.mvWeightTransferDecayX_WeightTransferDecayZ_FactorOfWeightX_FactorOfWeightZ.y = 0.0f;
    mBodyRollAttribs.mvWeightTransferDecayX_WeightTransferDecayZ_FactorOfWeightX_FactorOfWeightZ.x = 0.0f;
    mBodyRollAttribs.mvWheelLongForceHeightOffset_WheelLatForceHeightOffset.x = 0.0f;
    mBodyRollAttribs.mvWheelLongForceHeightOffset_WheelLatForceHeightOffset.y = 0.0f;

    // 0x825F5E18..0x825F5F36: [re-copy] the whole SuspensionAttribs block, lane by lane
    // (twelve vspltw/vrlimi pairs -- redundant after operator=, transcribed as shipped).
    mSuspensionAttribs.mvRestDisplacement_Dampening_UpwardMovement_DownwardMovement
        = lpSource->mSuspensionAttribs.mvRestDisplacement_Dampening_UpwardMovement_DownwardMovement;
    mSuspensionAttribs.mvFrontWheelHeightOffset_RearWheelHeightOffset_InAirDamping_MaxPitchDampingOnLanding
        = lpSource->mSuspensionAttribs.mvFrontWheelHeightOffset_RearWheelHeightOffset_InAirDamping_MaxPitchDampingOnLanding;
    mSuspensionAttribs.mvMaxYawDampingOnLanding_MaxRollDampingOnLanding_MaxVertVelocityDampingOnLanding_TimeToDampAfterLanding
        = lpSource->mSuspensionAttribs.mvMaxYawDampingOnLanding_MaxRollDampingOnLanding_MaxVertVelocityDampingOnLanding_TimeToDampAfterLanding;

    // ---- mSteeringAttribs (+0xE0/+0xF0) ----------------------------------------------------
    mSteeringAttribs.mvReactionPerSec_SpeedForMinAngle_SpeedForMinAngleRecip_MinAngle.w = KF_AI_STEERING_MIN_ANGLE;
    mSteeringAttribs.mvMaxAngle_StraightReactionBias.x = KF_AI_STEERING_MAX_ANGLE;
    mSteeringAttribs.mvReactionPerSec_SpeedForMinAngle_SpeedForMinAngleRecip_MinAngle.y = KF_AI_SPEED_FOR_MIN_ANGLE;
    // 0x825F5F64..0x825F5F7C: recip computed from the lane just written (fdivs 1.0 / 70.0).
    mSteeringAttribs.mvReactionPerSec_SpeedForMinAngle_SpeedForMinAngleRecip_MinAngle.z
        = 1.0f / mSteeringAttribs.mvReactionPerSec_SpeedForMinAngle_SpeedForMinAngleRecip_MinAngle.y;
    mSteeringAttribs.mvReactionPerSec_SpeedForMinAngle_SpeedForMinAngleRecip_MinAngle.x = KF_AI_STEERING_REACTION_PER_SEC;

    // ---- mBaseAttribs DownForceLiftCo + mDriftAttribs --------------------------------------
    mBaseAttribs.mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo.w = KF_AI_DOWNFORCE_LIFT_CO;
    mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.x = KF_AI_MIN_SPEED_FOR_DRIFT;
    mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.y = KF_AI_STEERING_DRIFT_SCALE;
    mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.w = KF_AI_BASE_COUNTER_STEERING_DRIFT_SCALE;
    mDriftAttribs.mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.z = KF_AI_COUNTER_STEERING_DRIFT_SCALE;
    mDriftAttribs.mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale.x = 1.0f;
    mDriftAttribs.mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale.y = 1.0f;
    mDriftAttribs.mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale.z = 1.0f;
    mDriftAttribs.mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale.w = 1.0f;
    mDriftAttribs.mvDriftTorqueFallOff_GripFromSteering_GripFromBrake_TimeForNaturalDrift.x = KF_AI_DRIFT_TORQUE_FALL_OFF;
    mDriftAttribs.mvNeutralTimeToReduceDrift_SideForceDriftScaleCutOff_SideForceDriftAngleCutOff_SideForceDriftSpeedCutOff.x = KF_AI_NEUTRAL_TIME_TO_REDUCE_DRIFT;
    mDriftAttribs.mvSideForcePeakDriftAngle_SideForceMagnitude_NaturalDriftDecay_NaturalDriftDecayPower.z = KF_AI_NATURAL_DRIFT_DECAY;
    mDriftAttribs.mvNeutralTimeToReduceDrift_SideForceDriftScaleCutOff_SideForceDriftAngleCutOff_SideForceDriftSpeedCutOff.y = KF_AI_SIDE_FORCE_DRIFT_SCALE_CUTOFF;
    mDriftAttribs.mvSideForcePeakDriftAngle_SideForceMagnitude_NaturalDriftDecay_NaturalDriftDecayPower.y = KF_AI_SIDE_FORCE_MAGNITUDE;

    // 0x825F6164..0x825F6184: the drift-scale-to-yaw-torque curve (this+0x100).
    mDriftAttribs.mDriftScaleToYawTorque.Construct();
    mDriftAttribs.mDriftScaleToYawTorque.Prepare(KF_AI_DRIFT_YAW_TORQUE_CURVE_A,
                                                 KF_AI_DRIFT_YAW_TORQUE_CURVE_B,
                                                 KF_AI_DRIFT_YAW_TORQUE_CURVE_C);

    // 0x825F6188..0x825F6254: the remaining drift lanes.
    mDriftAttribs.mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping.w = KF_AI_DRIFT_SIDEWAYS_DAMPING;
    mDriftAttribs.mvDriftAngularDamping_MaxDriftAngle_CounterSteerTorqueScaleFactor_DriftPushTime.x = KF_AI_DRIFT_ANGULAR_DAMPING;
    mDriftAttribs.mvDriftAngularDamping_MaxDriftAngle_CounterSteerTorqueScaleFactor_DriftPushTime.z = KF_AI_COUNTER_STEER_TORQUE_SCALE;
    mDriftAttribs.mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping.z = 0.0f;  // TorqueKickFromGasLetOff
    mDriftAttribs.mvDriftPushScaleLimit_DriftPushBaseFactor_MaxPowerSlideFactor.z = 0.0f;       // MaxPowerSlideFactor

    // 0x825F6258..0x825F6274: BoostLinearDrag = 0.75 * this LinearDrag. ⚠️ AS SHIPPED this is
    // 0.75 * 0.0 == 0.0 -- LinearDrag was zeroed above -- but the console computes the product
    // from the live lane, so the dependence is kept.
    mBoostAttribs.mvBoostBase_MaxBoostSpeed_BoostLinearDrag_NormalBoostHeightOffset.z
        = mBaseAttribs.mvLinearDrag_AngularDrag_HighSpeedAngularDamping_FrontWheelMass.x
          * KF_AI_BOOST_LINEAR_DRAG_SCALE;

    // 0x825F6278: [re-copy] the whole EngineAttribs block (memcpy 0xA0 == sizeof, verified
    // against the host layout -- NOT a carried console literal: static_assert below pins 0xA0).
    std::memcpy(&mEngineAttribs, &lpSource->mEngineAttribs, sizeof(EngineAttribs));

    mbIsValid = true;   // li 1 ; stb -> +0x360
}

// @0x825E6580 (125 instrs, leaf)  BrnPhysics::Vehicle::SimpleVehicleAttribs::Construct
//
// The default simple-attribs initialiser. NOT just a zero-fill: it seeds a default car
// geometry. Every constant image-read (x360rd, 10/10 self-test); every lane insert
// dataflow-tracked (vrlimi masks 8/4/2/1 == x/y/z/w). mAttribsKey is NOT initialised here --
// as shipped.
void SimpleVehicleAttribs::Construct()
{
    const f32 KF_DEFAULT_MASS                = 1560.0f;    // flt_82096C9C
    const f32 KF_DEFAULT_TRACTION_LINE       = 0.4f;       // flt_8200473C (0.400000006)
    const f32 KF_DEFAULT_WHEEL_MASS          = 30.0f;      // flt_82004F5C (front AND rear)
    const f32 KF_DEFAULT_WHEEL_X             = 0.85f;      // flt_82013A78 (0.850000024)
    const f32 KF_DEFAULT_WHEEL_Y             = -0.2f;      // flt_82020A84 (-0.200000003)
    const f32 KF_DEFAULT_FRONT_WHEEL_Z       = 1.1f;       // flt_82004A1C (1.10000002)
    const f32 KF_DEFAULT_REAR_WHEEL_Z        = -1.5f;      // flt_8200D538
    const f32 KF_DEFAULT_COM_Y               = -0.4f;      // flt_82012EF8 (-0.400000006)
    const f32 KF_DEFAULT_COM_Z               = 0.3f;       // flt_82004740 (0.300000012)
    const f32 KF_DEFAULT_UPWARD_MOVEMENT     = 0.11f;      // flt_820047C0 (0.109999999)
    const f32 KF_DEFAULT_DOWNWARD_MOVEMENT   = 0.125f;     // flt_82004010

    mvUpwardMovement_DownwardMovement_Mass_TractionLineLength.z = KF_DEFAULT_MASS;
    mvUpwardMovement_DownwardMovement_Mass_TractionLineLength.w = KF_DEFAULT_TRACTION_LINE;
    miRaceCarID = 0;                                                       // stw 0 -> +0xE0
    mFrontRightWheelPos = Vector3{ KF_DEFAULT_WHEEL_X, KF_DEFAULT_WHEEL_Y,
                                   KF_DEFAULT_FRONT_WHEEL_Z, 0.0f };       // +0xB0
    mRearRightWheelPos  = Vector3{ KF_DEFAULT_WHEEL_X, KF_DEFAULT_WHEEL_Y,
                                   KF_DEFAULT_REAR_WHEEL_Z, 0.0f };        // +0xC0
    mvFrontWheelMass_RearWheelMass_FrontWheelHeightOffset_RearWheelHeightOffset.x = KF_DEFAULT_WHEEL_MASS;
    mvFrontWheelMass_RearWheelMass_FrontWheelHeightOffset_RearWheelHeightOffset.y = KF_DEFAULT_WHEEL_MASS;
    mCOMOffset = Vector3{ 0.0f, KF_DEFAULT_COM_Y, KF_DEFAULT_COM_Z, 0.0f };  // +0xD0
    mvFrontWheelMass_RearWheelMass_FrontWheelHeightOffset_RearWheelHeightOffset.z = 0.0f;
    mvFrontWheelMass_RearWheelMass_FrontWheelHeightOffset_RearWheelHeightOffset.w = 0.0f;
    mvUpwardMovement_DownwardMovement_Mass_TractionLineLength.x = KF_DEFAULT_UPWARD_MOVEMENT;
    mvUpwardMovement_DownwardMovement_Mass_TractionLineLength.y = KF_DEFAULT_DOWNWARD_MOVEMENT;

    // 0x825E6748..0x825E6764: eight stvx128 of the zero vector -- both tires cleared whole.
    std::memset(&mFrontTireAttribs, 0, sizeof(mFrontTireAttribs));         // +0x20..+0x5F
    std::memset(&mRearTireAttribs,  0, sizeof(mRearTireAttribs));          // +0x60..+0x9F

    mbIsValid = false;                                                     // stb 0 -> +0xE4
}

// @0x825BE0C8 (81 instrs, leaf)  BrnPhysics::Vehicle::SimpleVehicleAttribs::SetupAttribs
//
// Stream the simple set out of a full VehicleAttribs. Callers: SimpleVehiclePhysics::Prepare,
// SimpleVehiclePhysics::SwitchAttribs, VehiclePhysics::SetAttributes(3-arg) @0x8262E140.
// miRaceCarID is NOT copied -- as shipped.
void SimpleVehicleAttribs::SetupAttribs(const VehicleAttribs* lpSource)
{
    mvUpwardMovement_DownwardMovement_Mass_TractionLineLength.z
        = lpSource->mBaseAttribs.mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.x;      // Mass
    mFrontRightWheelPos = lpSource->mBaseAttribs.mFrontRightWheelPos;                    // +0xB0 <- src+0x00
    mRearRightWheelPos  = lpSource->mBaseAttribs.mRearRightWheelPos;                     // +0xC0 <- src+0x10
    mCOMOffset          = lpSource->mBaseAttribs.mCOMOffset;                             // +0xD0 <- src+0x20
    mvUpwardMovement_DownwardMovement_Mass_TractionLineLength.w
        = lpSource->mBaseAttribs.mvTractionLineLength_LowSpeedDrivingMPH_LowSpeedTyreFrictionTractionControl_LowSpeedThrottleTractionControl.x;
    mvFrontWheelMass_RearWheelMass_FrontWheelHeightOffset_RearWheelHeightOffset.x
        = lpSource->mBaseAttribs.mvLinearDrag_AngularDrag_HighSpeedAngularDamping_FrontWheelMass.w;
    mvFrontWheelMass_RearWheelMass_FrontWheelHeightOffset_RearWheelHeightOffset.y
        = lpSource->mBaseAttribs.mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo.x;
    mAttribsKey = lpSource->mAttribsKey;                                                 // ld/std +0xA0 <- src+0x358
    mvFrontWheelMass_RearWheelMass_FrontWheelHeightOffset_RearWheelHeightOffset.z
        = lpSource->mSuspensionAttribs.mvFrontWheelHeightOffset_RearWheelHeightOffset_InAirDamping_MaxPitchDampingOnLanding.x;
    mvFrontWheelMass_RearWheelMass_FrontWheelHeightOffset_RearWheelHeightOffset.w
        = lpSource->mSuspensionAttribs.mvFrontWheelHeightOffset_RearWheelHeightOffset_InAirDamping_MaxPitchDampingOnLanding.y;
    mvUpwardMovement_DownwardMovement_Mass_TractionLineLength.x
        = lpSource->mSuspensionAttribs.mvRestDisplacement_Dampening_UpwardMovement_DownwardMovement.z;
    mvUpwardMovement_DownwardMovement_Mass_TractionLineLength.y
        = lpSource->mSuspensionAttribs.mvRestDisplacement_Dampening_UpwardMovement_DownwardMovement.w;

    // The two 8 x ld/std copy loops (0x40 bytes each).
    mFrontTireAttribs = lpSource->mFrontTireAttribs;                                     // +0x20 <- src+0x2D0
    mRearTireAttribs  = lpSource->mRearTireAttribs;                                      // +0x60 <- src+0x310

    mbIsValid = true;                                                                    // stb 1 -> +0xE4
}

// @0x825E6778 (104 instrs)  BrnPhysics::Vehicle::SimpleVehicleAttribs::SetupAttribs(handling)
//
// Stream the simple set out of a loaded AttribSys handling record: construct the
// physicsvehiclebaseattribs wrapper from the handling's BaseAttribs RefSpec (data+0xA8 ==
// KU_PHYSICSVEHICLEBASEATTRIBS_OFFSET) and the suspension wrapper from data+0x00
// (KU_PHYSICSVEHICLESUSPENSIONATTRIBS_OFFSET), then lane-scatter their records. Source byte
// offsets are asm-exact; the records' own field names are not recovered (same convention as
// EngineAttribs::InitializeFromAttribs above -- byte-addressed float source, offsets EXACT).
// The destination lane names that follow are this type's own DWARF names, so every read is
// role-named on the destination side.
void SimpleVehicleAttribs::SetupAttribs(const Attrib::Gen::physicsvehiclehandling& lrHandling)
{
    using Attrib::Gen::physicsvehiclebaseattribs;
    using Attrib::Gen::physicsvehiclesuspensionattribs;

    #define BP_SVA_SRC_F(base, byteOff) ((base)[(byteOff) >> 2])

    // ---- the base-attribs record (data+0xA8 RefSpec) ---------------------------------------
    {
        // The DWARF passes the handling BY VALUE, so the console callee owns a mutable copy and
        // the RefSpec resolve caches into it; with the const-ref spelling that mutability is
        // restored explicitly (GetCollection resolves + AddRefs + caches -- it is non-const).
        physicsvehiclebaseattribs lBase(
            const_cast<Attrib::Collection*>(
                const_cast<Attrib::RefSpec&>(lrHandling.PhysicsVehicleBaseAttribs())
                    .GetCollection()),
            NULL);
        const f32* lpData = static_cast<const f32*>(lBase.GetLayoutPointer());

        mvUpwardMovement_DownwardMovement_Mass_TractionLineLength.z
            = BP_SVA_SRC_F(lpData, 0x104);                            // Mass        <- data+0x104
        mFrontRightWheelPos = *reinterpret_cast<const Vector3*>(lpData + (0x10 >> 2));  // <- data+0x10
        mRearRightWheelPos  = *reinterpret_cast<const Vector3*>(lpData + (0x00 >> 2));  // <- data+0x00
        mCOMOffset          = *reinterpret_cast<const Vector3*>(lpData + (0x20 >> 2));  // <- data+0x20

        // fsel f0,f0,f0,0.0 -- negative traction-line lengths clamp to 0.
        {
            const f32 lfLen = BP_SVA_SRC_F(lpData, 0x44);
            mvUpwardMovement_DownwardMovement_Mass_TractionLineLength.w
                = (lfLen >= 0.0f) ? lfLen : 0.0f;                     // TractionLineLength
        }
        mvFrontWheelMass_RearWheelMass_FrontWheelHeightOffset_RearWheelHeightOffset.x
            = BP_SVA_SRC_F(lpData, 0xCC);                             // FrontWheelMass
        mvFrontWheelMass_RearWheelMass_FrontWheelHeightOffset_RearWheelHeightOffset.y
            = BP_SVA_SRC_F(lpData, 0x64);                             // RearWheelMass

        // The stack round-trip that zeroes the COM x lane (stvx -> stfs 0.0 -> lvx -> stvx).
        mCOMOffset.x = 0.0f;

        // ⭐ 2026-08-09 (attribs-data wave): the two tire scatters are REAL (the permute table
        // is homed and both bodies are image-emulated -- see the Wheel.cpp banner).
        mFrontTireAttribs.PrepareFrontTire(lBase);                    // this+0x20
        mRearTireAttribs.PrepareRearTire(lBase);                      // this+0x60
    }

    // ---- the suspension record (data+0x00 RefSpec) -----------------------------------------
    {
        physicsvehiclesuspensionattribs lSusp(
            const_cast<Attrib::Collection*>(
                const_cast<Attrib::RefSpec&>(lrHandling.PhysicsVehicleSuspensionAttribs())
                    .GetCollection()),
            NULL);
        const f32* lpData = static_cast<const f32*>(lSusp.GetLayoutPointer());

        mvFrontWheelMass_RearWheelMass_FrontWheelHeightOffset_RearWheelHeightOffset.z
            = BP_SVA_SRC_F(lpData, 0x28);                             // FrontWheelHeightOffset
        mvFrontWheelMass_RearWheelMass_FrontWheelHeightOffset_RearWheelHeightOffset.w
            = BP_SVA_SRC_F(lpData, 0x10);                             // RearWheelHeightOffset
        mvUpwardMovement_DownwardMovement_Mass_TractionLineLength.x
            = BP_SVA_SRC_F(lpData, 0x00);                             // UpwardMovement
        mvUpwardMovement_DownwardMovement_Mass_TractionLineLength.y
            = BP_SVA_SRC_F(lpData, 0x2C);                             // DownwardMovement
    }

    #undef BP_SVA_SRC_F

    mbIsValid = true;                                                 // stb 1 -> +0xE4
    // (The console callee also destroys its BY-VALUE handling parameter here; with the
    // const-ref spelling the caller's explicit copy is destroyed at its own scope end.)
}
}
}
