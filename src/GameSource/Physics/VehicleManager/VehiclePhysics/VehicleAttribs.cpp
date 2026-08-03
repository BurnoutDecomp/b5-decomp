#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehicleAttribs.h"

#include "types.hpp"

#include <cstddef>

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

using namespace EngineDefaults;

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
}
}
