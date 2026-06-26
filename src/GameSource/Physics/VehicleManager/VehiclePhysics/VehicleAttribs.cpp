#include "types.hpp"

#include <cstddef>

namespace rw
{
namespace math
{
namespace vpu
{
struct VecFloat
{
    f32 mfValue;

    VecFloat()
        : mfValue(0.0f)
    {
    }

    explicit VecFloat(f32 lfValue)
        : mfValue(lfValue)
    {
    }
};

struct alignas(16) Vector3
{
    f32 mfX;
    f32 mfY;
    f32 mfZ;
    f32 mfW;

    void SetX(VecFloat lValue)
    {
        mfX = lValue.mfValue;
    }

    void SetY(VecFloat lValue)
    {
        mfY = lValue.mfValue;
    }

    void SetZ(VecFloat lValue)
    {
        mfZ = lValue.mfValue;
    }
};

struct alignas(16) Vector4
{
    f32 mfX;
    f32 mfY;
    f32 mfZ;
    f32 mfW;

    void SetX(VecFloat lValue)
    {
        mfX = lValue.mfValue;
    }

    void SetY(VecFloat lValue)
    {
        mfY = lValue.mfValue;
    }

    void SetZ(VecFloat lValue)
    {
        mfZ = lValue.mfValue;
    }

    void SetW(VecFloat lValue)
    {
        mfW = lValue.mfValue;
    }
};
}
}
}

namespace BrnPhysics
{
class InterpedParam3
{
public:
    void Construct();
    void Prepare(f32 lfInputMin, f32 lfInputMax, f32 lfOutputAtMin);

private:
    rw::math::vpu::Vector4 mvParams;
};

namespace Vehicle
{
namespace EngineDefaults
{
extern const f32 KF_DEFAULT_GEAR_RATIO_0;
extern const f32 KF_DEFAULT_GEAR_RATIO_1;
extern const f32 KF_DEFAULT_GEAR_RATIO_2;
extern const f32 KF_DEFAULT_GEAR_RATIO_3;
extern const f32 KF_DEFAULT_GEAR_RATIO_4;
extern const f32 KF_DEFAULT_GEAR_RATIO_5;
extern const f32 KF_DEFAULT_DIFFERENTIAL;
extern const f32 KF_DEFAULT_TRANSMISSION_EFFICIENCY;
extern const f32 KF_DEFAULT_ENGINE_RESISTANCE;
extern const f32 KF_DEFAULT_GEAR_DOWN_RPM;
extern const f32 KF_DEFAULT_MAX_TORQUE;
extern const f32 KF_DEFAULT_TORQUE_FALL_OFF_RPM;
extern const f32 KF_DEFAULT_MAX_RPM;
extern const f32 KF_DEFAULT_LSDM_SPEED_TO_ALLOW_GEAR_CHANGES;
extern const f32 KF_DEFAULT_FLYWHEEL_INERTIA;
extern const f32 KF_DEFAULT_FLYWHEEL_FRICTION;
extern const f32 KF_DEFAULT_GEAR_CHANGE_TIME;
extern const f32 KF_DEFAULT_TORQUE_SCALE_0;
extern const f32 KF_DEFAULT_TORQUE_SCALE_1;
extern const f32 KF_DEFAULT_TORQUE_SCALE_2;
extern const f32 KF_DEFAULT_TORQUE_SCALE_3;
extern const f32 KF_DEFAULT_TORQUE_SCALE_4;
extern const f32 KF_DEFAULT_TORQUE_SCALE_5;
extern const f32 KF_DEFAULT_GEAR_UP_RPM_0;
extern const f32 KF_DEFAULT_GEAR_UP_RPM_1;
extern const f32 KF_DEFAULT_GEAR_UP_RPM_2;
extern const f32 KF_DEFAULT_GEAR_UP_RPM_3;
extern const f32 KF_DEFAULT_GEAR_UP_RPM_4;
extern const f32 KF_DEFAULT_GEAR_UP_RPM_5;

const f32 KF_DEFAULT_TORQUE_CURVE_INPUT_MIN = 37.0f;
const f32 KF_DEFAULT_TORQUE_CURVE_INPUT_MAX = 740.0f;
const f32 KF_DEFAULT_TORQUE_CURVE_OUTPUT_AT_MIN = 0.0f;
const f32 KF_DONUT_AI_MASS = 700.0f;
const f32 KF_DONUT_AI_DRIFT_MIN_SPEED = 700.0f;
const f32 KF_DONUT_AI_MAX_TORQUE = 0.0f;
}

using namespace EngineDefaults;

namespace Wheel
{
class TireAttribs
{
public:
    void PrepareFrontTireForDonutAI();
    void PrepareRearTireForDonutAI();

private:
    u8 mData[0x40];
};

static_assert(sizeof(TireAttribs) == 0x40, "TireAttribs layout drift");
}

class VehicleAttribs
{
public:
    struct VehicleBaseAttribs
    {
        rw::math::vpu::Vector3 mFrontRightWheelPos;
        rw::math::vpu::Vector3 mRearRightWheelPos;
        rw::math::vpu::Vector3 mCOMOffset;
        rw::math::vpu::Vector3 mCrashExtraVelocityFactors;
        rw::math::vpu::Vector4 mDrivetimeDeformLimits;
        s32                    miRaceCarID;
        u8                     mPad54[12];
        InterpedParam3         mBrakeScaleToFactorCurve;
        rw::math::vpu::Vector4 mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce;
        rw::math::vpu::Vector4 mvDownForceZOffset_MagicBrakeFactorTurning_MagicBrakeFactorStraightLine_BrakeScaleToLockWheels;
        rw::math::vpu::Vector4 mvTractionLineLength_LowSpeedDrivingMPH_LowSpeedTyreFrictionTractionControl_LowSpeedThrottleTractionControl;
        rw::math::vpu::Vector4 mvLinearDrag_AngularDrag_HighSpeedAngularDamping_FrontWheelMass;
        rw::math::vpu::Vector4 mvRearWheelMass_PowerToFront_PowerToRear_DownForceLiftCo;
        rw::math::vpu::Vector4 mvPitchDampingOnTakeOff_YawDampingOnTakeOff_RollDampingOnTakeOff_RollLimitOnTakeOff;
        rw::math::vpu::Vector4 mvFrontSurfaceGripFactor_RearSurfaceGripFactor_SurfaceRoughnessFactor_SurfaceLinearDragFactor;

        void SetMass(rw::math::vpu::VecFloat lValue)
        {
            mvMass_TimeForFullBrakeRecip_MaxSpeed_DownForce.SetX(lValue);
        }
    };

    struct SteeringAttribs
    {
        rw::math::vpu::Vector4 mvReactionPerSec_SpeedForMinAngle_SpeedForMinAngleRecip_MinAngle;
        rw::math::vpu::Vector4 mvMaxAngle_StraightReactionBias;
    };

    struct EngineAttribs
    {
        enum
        {
            KI_MAX_NUM_GEARS = 6
        };

        void Construct();

        // @0x825CF278: stream the engine tuning out of a loaded `physicsvehicleengineattribs` data
        // wrapper into the packed lanes (bodied out-of-line below). FLAG: the source wrapper member
        // offsets are recovered from the asm but its type is un-homed -- modelled as a byte-addressed
        // source. Returns the InterpedParam3::Prepare result (an int on X360; void here).
        void InitializeFromAttribs(const void* lpSourceWrapper);

        void SetDifferential(rw::math::vpu::VecFloat lValue)
        {
            mvDifferential_TransmissionEfficiency_EngineResistance_GearDownRPM.SetX(lValue);
        }

        void SetTransmissionEfficiency(rw::math::vpu::VecFloat lValue)
        {
            mvDifferential_TransmissionEfficiency_EngineResistance_GearDownRPM.SetY(lValue);
        }

        void SetEngineResistance(rw::math::vpu::VecFloat lValue)
        {
            mvDifferential_TransmissionEfficiency_EngineResistance_GearDownRPM.SetZ(lValue);
        }

        void SetGearDownRPM(rw::math::vpu::VecFloat lValue)
        {
            mvDifferential_TransmissionEfficiency_EngineResistance_GearDownRPM.SetW(lValue);
        }

        void SetMaxTorque(rw::math::vpu::VecFloat lValue)
        {
            mvMaxTorque_TorqueFallOffRPM_MaxRPM_LSDMSpeedToAllowGearChanges.SetX(lValue);
        }

        void SetTorqueFallOffRPM(rw::math::vpu::VecFloat lValue)
        {
            mvMaxTorque_TorqueFallOffRPM_MaxRPM_LSDMSpeedToAllowGearChanges.SetY(lValue);
        }

        void SetMaxRPM(rw::math::vpu::VecFloat lValue)
        {
            mvMaxTorque_TorqueFallOffRPM_MaxRPM_LSDMSpeedToAllowGearChanges.SetZ(lValue);
        }

        void SetLSDMSpeedToAllowGearChanges(rw::math::vpu::VecFloat lValue)
        {
            mvMaxTorque_TorqueFallOffRPM_MaxRPM_LSDMSpeedToAllowGearChanges.SetW(lValue);
        }

        void SetFlyWheelInertia(rw::math::vpu::VecFloat lValue)
        {
            mvFlyWheelInertia_FlyWheelFriction_GearChangeTime.SetX(lValue);
        }

        void SetFlyWheelFriction(rw::math::vpu::VecFloat lValue)
        {
            mvFlyWheelInertia_FlyWheelFriction_GearChangeTime.SetY(lValue);
        }

        void SetGearChangeTime(rw::math::vpu::VecFloat lValue)
        {
            mvFlyWheelInertia_FlyWheelFriction_GearChangeTime.SetZ(lValue);
        }

        void SetGearRatio(s32 liGear, rw::math::vpu::VecFloat lValue)
        {
            mavGearRatios_TorqueScales_GearUpRPMs[liGear].SetX(lValue);
        }

        void SetTorqueScale(s32 liGear, rw::math::vpu::VecFloat lValue)
        {
            mavGearRatios_TorqueScales_GearUpRPMs[liGear].SetY(lValue);
        }

        void SetGearUpRPM(s32 liGear, rw::math::vpu::VecFloat lValue)
        {
            mavGearRatios_TorqueScales_GearUpRPMs[liGear].SetZ(lValue);
        }

        InterpedParam3 mTorqueCurve;
        rw::math::vpu::Vector4 mvDifferential_TransmissionEfficiency_EngineResistance_GearDownRPM;
        rw::math::vpu::Vector4 mvMaxTorque_TorqueFallOffRPM_MaxRPM_LSDMSpeedToAllowGearChanges;
        rw::math::vpu::Vector4 mvFlyWheelInertia_FlyWheelFriction_GearChangeTime;
        rw::math::vpu::Vector3 mavGearRatios_TorqueScales_GearUpRPMs[KI_MAX_NUM_GEARS];
    };

    struct DriftAttribs
    {
        InterpedParam3         mDriftScaleToYawTorque;
        rw::math::vpu::Vector4 mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor;
        rw::math::vpu::Vector4 mvBrakingDriftScaleFactor_GasDriftScaleFactor_TimeToCapScale_CappedScale;
        rw::math::vpu::Vector4 mvDriftTorqueFallOff_GripFromSteering_GripFromBrake_TimeForNaturalDrift;
        rw::math::vpu::Vector4 mvNeutralTimeToReduceDrift_SideForceDriftScaleCutOff_SideForceDriftAngleCutOff_SideForceDriftSpeedCutOff;
        rw::math::vpu::Vector4 mvSideForcePeakDriftAngle_SideForceMagnitude_NaturalDriftDecay_NaturalDriftDecayPower;
        rw::math::vpu::Vector4 mvNaturalYawTorque_NaturalYawTorqueCutOffAngle_TorqueKickFromGasLetOff_DriftSidewaysDamping;
        rw::math::vpu::Vector4 mvDriftAngularDamping_MaxDriftAngle_CounterSteerTorqueScaleFactor_DriftPushTime;
        rw::math::vpu::Vector4 mvDriftPushScaleLimit_DriftPushBaseFactor_MaxPowerSlideFactor;

        void SetMinSpeedForDrift(rw::math::vpu::VecFloat lValue)
        {
            mvMinSpeedForDrift_SteeringDriftScaleFactor_CounterSteeringDriftScaleFactor_BaseCounterSteeringDriftScaleFactor.SetX(lValue);
        }
    };

    void SetupAttribsForDonutAI();

    // @0x825B2B68  BrnPhysics::Vehicle::VehicleAttribs::operator=
    // Full-object copy assignment (additive grow of this committed home -- declared here, bodied
    // out-of-line below). Used by VehicleAttribs::SetupAttribsForAI and VehiclePhysics::Prepare.
    VehicleAttribs& operator=(const VehicleAttribs& lrSource);

    VehicleBaseAttribs mBaseAttribs;
    SteeringAttribs    mSteeringAttribs;
    EngineAttribs      mEngineAttribs;
    DriftAttribs       mDriftAttribs;
    u8                 mPadAfterDrift[0xA0];
    Wheel::TireAttribs mFrontTireAttribs;
    Wheel::TireAttribs mRearTireAttribs;
    // Trailing attribute words past the two tire blocks (no DWARF field structure for this
    // TU; named byte-accurate fields so operator= copies them BY NAME, no raw-offset casts).
    u32                muTrailingWord;     // @0x350 (asm lwz/stw, 4-byte)
    u8                 mPad354[4];         // @0x354 : gap to the 8-byte field at 0x358
    u64                muTrailingData;     // @0x358 (asm ld/std, 8-byte)
    bool               mbIsValid;          // @0x360 : operator= sets this true (asm li 1; stb)
};

static_assert(sizeof(rw::math::vpu::Vector3) == 16, "Vector3 layout drift");
static_assert(sizeof(rw::math::vpu::Vector4) == 16, "Vector4 layout drift");
static_assert(sizeof(InterpedParam3) == 16, "InterpedParam3 layout drift");
static_assert(sizeof(VehicleAttribs::VehicleBaseAttribs) == 0xE0, "VehicleBaseAttribs layout drift");
static_assert(sizeof(VehicleAttribs::SteeringAttribs) == 0x20, "SteeringAttribs layout drift");
static_assert(sizeof(VehicleAttribs::EngineAttribs) == 0xA0, "EngineAttribs layout drift");
static_assert(sizeof(VehicleAttribs::DriftAttribs) == 0x90, "DriftAttribs layout drift");
static_assert(offsetof(VehicleAttribs, mBaseAttribs) == 0x0, "mBaseAttribs offset drift");
static_assert(offsetof(VehicleAttribs, mEngineAttribs) == 0x100, "mEngineAttribs offset drift");
static_assert(offsetof(VehicleAttribs, mDriftAttribs) == 0x1A0, "mDriftAttribs offset drift");
static_assert(offsetof(VehicleAttribs, mFrontTireAttribs) == 0x2D0, "mFrontTireAttribs offset drift");
static_assert(offsetof(VehicleAttribs, mRearTireAttribs) == 0x310, "mRearTireAttribs offset drift");

void VehicleAttribs::EngineAttribs::Construct()
{
    SetGearRatio(0, rw::math::vpu::VecFloat(KF_DEFAULT_GEAR_RATIO_0));
    SetGearRatio(1, rw::math::vpu::VecFloat(KF_DEFAULT_GEAR_RATIO_1));
    SetGearRatio(2, rw::math::vpu::VecFloat(KF_DEFAULT_GEAR_RATIO_2));
    SetGearRatio(3, rw::math::vpu::VecFloat(KF_DEFAULT_GEAR_RATIO_3));
    SetGearRatio(4, rw::math::vpu::VecFloat(KF_DEFAULT_GEAR_RATIO_4));
    SetGearRatio(5, rw::math::vpu::VecFloat(KF_DEFAULT_GEAR_RATIO_5));

    SetDifferential(rw::math::vpu::VecFloat(KF_DEFAULT_DIFFERENTIAL));
    SetMaxTorque(rw::math::vpu::VecFloat(KF_DEFAULT_MAX_TORQUE));
    SetTransmissionEfficiency(rw::math::vpu::VecFloat(KF_DEFAULT_TRANSMISSION_EFFICIENCY));
    SetEngineResistance(rw::math::vpu::VecFloat(KF_DEFAULT_ENGINE_RESISTANCE));

    SetGearUpRPM(0, rw::math::vpu::VecFloat(KF_DEFAULT_GEAR_UP_RPM_0));
    SetGearUpRPM(1, rw::math::vpu::VecFloat(KF_DEFAULT_GEAR_UP_RPM_1));
    SetGearUpRPM(2, rw::math::vpu::VecFloat(KF_DEFAULT_GEAR_UP_RPM_2));
    SetGearUpRPM(3, rw::math::vpu::VecFloat(KF_DEFAULT_GEAR_UP_RPM_3));
    SetGearUpRPM(4, rw::math::vpu::VecFloat(KF_DEFAULT_GEAR_UP_RPM_4));
    SetGearUpRPM(5, rw::math::vpu::VecFloat(KF_DEFAULT_GEAR_UP_RPM_5));
    SetGearDownRPM(rw::math::vpu::VecFloat(KF_DEFAULT_GEAR_DOWN_RPM));

    SetTorqueFallOffRPM(rw::math::vpu::VecFloat(KF_DEFAULT_TORQUE_FALL_OFF_RPM));
    SetLSDMSpeedToAllowGearChanges(rw::math::vpu::VecFloat(KF_DEFAULT_LSDM_SPEED_TO_ALLOW_GEAR_CHANGES));
    SetTorqueScale(0, rw::math::vpu::VecFloat(KF_DEFAULT_TORQUE_SCALE_0));
    SetTorqueScale(1, rw::math::vpu::VecFloat(KF_DEFAULT_TORQUE_SCALE_1));
    SetTorqueScale(2, rw::math::vpu::VecFloat(KF_DEFAULT_TORQUE_SCALE_2));
    SetTorqueScale(3, rw::math::vpu::VecFloat(KF_DEFAULT_TORQUE_SCALE_3));
    SetTorqueScale(4, rw::math::vpu::VecFloat(KF_DEFAULT_TORQUE_SCALE_4));
    SetTorqueScale(5, rw::math::vpu::VecFloat(KF_DEFAULT_TORQUE_SCALE_5));

    SetFlyWheelInertia(rw::math::vpu::VecFloat(KF_DEFAULT_FLYWHEEL_INERTIA));
    mTorqueCurve.Construct();
    mTorqueCurve.Prepare(
        KF_DEFAULT_TORQUE_CURVE_INPUT_MIN,
        KF_DEFAULT_TORQUE_CURVE_INPUT_MAX,
        KF_DEFAULT_TORQUE_CURVE_OUTPUT_AT_MIN);
    SetMaxRPM(rw::math::vpu::VecFloat(KF_DEFAULT_MAX_RPM));
    SetFlyWheelFriction(rw::math::vpu::VecFloat(KF_DEFAULT_FLYWHEEL_FRICTION));
    SetGearChangeTime(rw::math::vpu::VecFloat(KF_DEFAULT_GEAR_CHANGE_TIME));
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

    using rw::math::vpu::VecFloat;

    // --- per-gear gear ratios (gear[g].x) : src+0x50 -> 0,1,2 ; src+0x40 -> 3,4,5 ---
    SetGearRatio(0, VecFloat(BP_SRC_F(0x50)));
    SetGearRatio(1, VecFloat(BP_SRC_F(0x54)));
    SetGearRatio(2, VecFloat(BP_SRC_F(0x58)));
    SetGearRatio(3, VecFloat(BP_SRC_F(0x40)));
    SetGearRatio(4, VecFloat(BP_SRC_F(0x44)));
    SetGearRatio(5, VecFloat(BP_SRC_F(0x48)));

    // --- inline scalar lanes streamed before the torque-curve prepare ---
    SetDifferential(VecFloat(BP_SRC_F(0x8C)));            // @+0x10.x
    SetTransmissionEfficiency(VecFloat(BP_SRC_F(0x60)));  // @+0x10.y
    SetEngineResistance(VecFloat(BP_SRC_F(0x80)));        // @+0x10.z
    SetGearDownRPM(VecFloat(BP_SRC_F(0x68)));             // @+0x10.w
    SetMaxTorque(VecFloat(BP_SRC_F(0x64)));               // @+0x20.x

    // --- torque-curve domain (InterpedParam3::Prepare). Domain derived from src+0x84 (MaxRPM):
    //     the asm forms  domainHi = MaxRPM * 0.5  and feeds (domainLo, domainHi) to Prepare. ---
    static const f32 KF_TORQUE_DOMAIN_SCALE = 0.5f;       // flt_82001DA0 (resolved)
    const f32 lfMaxRPM       = BP_SRC_F(0x84);
    const f32 lfDomainHigh   = lfMaxRPM * KF_TORQUE_DOMAIN_SCALE;
    const f32 lfDomainLow    = 0.0f;                      // zeroed scratch (stw r28=0 cascade)
    mTorqueCurve.Prepare(lfDomainLow, lfDomainHigh, 0.0f);

    // --- per-gear torque scales (gear[g].y) : src+0x30 -> 0,1,2 ; src+0x20 -> 3,4,5 ---
    SetTorqueScale(0, VecFloat(BP_SRC_F(0x30)));
    SetTorqueScale(1, VecFloat(BP_SRC_F(0x34)));
    SetTorqueScale(2, VecFloat(BP_SRC_F(0x38)));
    SetTorqueScale(3, VecFloat(BP_SRC_F(0x20)));
    SetTorqueScale(4, VecFloat(BP_SRC_F(0x24)));
    SetTorqueScale(5, VecFloat(BP_SRC_F(0x28)));

    // --- per-gear up-RPMs (gear[g].z) : src+0x10 -> 0,1,2 ; src+0x00 -> 3,4,5 ---
    SetGearUpRPM(0, VecFloat(BP_SRC_F(0x10)));
    SetGearUpRPM(1, VecFloat(BP_SRC_F(0x14)));
    SetGearUpRPM(2, VecFloat(BP_SRC_F(0x18)));
    SetGearUpRPM(3, VecFloat(BP_SRC_F(0x00)));
    SetGearUpRPM(4, VecFloat(BP_SRC_F(0x04)));
    SetGearUpRPM(5, VecFloat(BP_SRC_F(0x08)));

    // --- trailing scalar lanes (@+0x20.z/.w and @+0x30.x/.y/.z) ---
    SetMaxRPM(VecFloat(BP_SRC_F(0x6C)));                  // @+0x20.z (lvlx r9=0x6C, mask2)
    SetLSDMSpeedToAllowGearChanges(VecFloat(BP_SRC_F(0x70)));  // @+0x20.w (lvlx r10=0x70, mask1)
    SetFlyWheelInertia(VecFloat(BP_SRC_F(0x78)));        // @+0x30.x (lvlx r8=0x78, mask8)
    SetFlyWheelFriction(VecFloat(BP_SRC_F(0x7C)));       // @+0x30.y (lvlx r7=0x7C, mask4)
    SetGearChangeTime(VecFloat(BP_SRC_F(0x74)));         // @+0x30.z (lvlx r6=0x74, mask2)

    #undef BP_SRC_F
}

void VehicleAttribs::SetupAttribsForDonutAI()
{
    mDriftAttribs.SetMinSpeedForDrift(rw::math::vpu::VecFloat(KF_DONUT_AI_DRIFT_MIN_SPEED));
    mEngineAttribs.SetMaxTorque(rw::math::vpu::VecFloat(KF_DONUT_AI_MAX_TORQUE));
    mFrontTireAttribs.PrepareFrontTireForDonutAI();
    mRearTireAttribs.PrepareRearTireForDonutAI();
    mBaseAttribs.SetMass(rw::math::vpu::VecFloat(KF_DONUT_AI_MASS));
}

// @0x825B2B68  BrnPhysics::Vehicle::VehicleAttribs::operator=
//
// The X360 leaf copies the whole attribute block from source to destination: a memcpy of the
// leading 0xE0 bytes (== sizeof VehicleBaseAttribs) followed by per-sub-aggregate copies for the
// steering/engine/drift blocks, the inter-block padding, and the two TireAttribs (the two
// trailing 8-store do/while loops copy 0x40 bytes each). Every member is a trivially-copyable POD
// slice (Vector3/Vector4/InterpedParam3/TireAttribs/padding), so a memberwise assignment is
// store-for-store equivalent to the blob copy. The Hex-Rays artifacts of the form
// *(*(a2+632)+24)=*(a2+632) are mis-rendered 16-byte vector stores, not pointer writes.
// AFTER the two tire copies the asm has three more stores: lwz/stw @0x350 (4-byte word copy),
// ld/std @0x358 (8-byte copy), and `li r10,1; stb r10,0x360` -- the byte at 0x360 (mbIsValid) is
// set to the CONSTANT 1, not copied (the Hex-Rays BYTE3(v32) rendering is the li-1 misread).
VehicleAttribs& VehicleAttribs::operator=(const VehicleAttribs& lrSource)
{
    mBaseAttribs     = lrSource.mBaseAttribs;
    mSteeringAttribs = lrSource.mSteeringAttribs;
    mEngineAttribs   = lrSource.mEngineAttribs;
    mDriftAttribs    = lrSource.mDriftAttribs;
    for ( u32 luByte = 0; luByte < sizeof(mPadAfterDrift); ++luByte )
    {
        mPadAfterDrift[luByte] = lrSource.mPadAfterDrift[luByte];
    }
    mFrontTireAttribs = lrSource.mFrontTireAttribs;
    mRearTireAttribs  = lrSource.mRearTireAttribs;
    muTrailingWord    = lrSource.muTrailingWord;   // stw @0x350
    muTrailingData    = lrSource.muTrailingData;   // std @0x358
    mbIsValid         = true;                      // li 1; stb @0x360 (set unconditionally)
    return *this;
}
}
}
