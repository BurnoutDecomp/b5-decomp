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

    VehicleBaseAttribs mBaseAttribs;
    SteeringAttribs    mSteeringAttribs;
    EngineAttribs      mEngineAttribs;
    DriftAttribs       mDriftAttribs;
    u8                 mPadAfterDrift[0xA0];
    Wheel::TireAttribs mFrontTireAttribs;
    Wheel::TireAttribs mRearTireAttribs;
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

void VehicleAttribs::SetupAttribsForDonutAI()
{
    mDriftAttribs.SetMinSpeedForDrift(rw::math::vpu::VecFloat(KF_DONUT_AI_DRIFT_MIN_SPEED));
    mEngineAttribs.SetMaxTorque(rw::math::vpu::VecFloat(KF_DONUT_AI_MAX_TORQUE));
    mFrontTireAttribs.PrepareFrontTireForDonutAI();
    mRearTireAttribs.PrepareRearTireForDonutAI();
    mBaseAttribs.SetMass(rw::math::vpu::VecFloat(KF_DONUT_AI_MASS));
}
}
}
