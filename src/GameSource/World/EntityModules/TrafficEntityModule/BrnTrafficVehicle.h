#pragma once

#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"
#include "GameSource/World/Traffic/BrnVehicleSoaData.h"
#include "SharedClasses/Traffic/BrnTrafficVehicleType.h"

namespace BrnTraffic
{
class Hull;
class Param;
class VehicleTypeRuntime;
struct LaneRung;

struct Axle
{
    Vector3Plus mPosAndWheelRadius;
    Vector3Plus mUpAndDebug;

    void Initialise()
    {
        mPosAndWheelRadius.SetZero();
        mUpAndDebug = Vector3Plus{ 0.0f, 1.0f, 0.0f, 0.0f };
    }

    bool TryIntersectWithLane(const LaneRung& lRung0, const LaneRung& lRung1);
    void ForceIntersectWithLane(const LaneRung& lRung0, const LaneRung& lRung1);

    void SetUp(Vector3 lUp) { mUpAndDebug.SetVector3(lUp); }
    Vector3 GetUp() const { return mUpAndDebug.GetVector3(); }
};

struct VehicleAxles
{
    Axle mFrontAxle;
    Axle mBackAxle;

    void UpdateRearAxleForRoadCollision(const Param* lpParam, Hull** lpapHulls);
};

class Vehicle
{
public:
    enum Flags
    {
        E_FLAG_ALIVE         = 0x01,
        E_FLAG_HASENTITY     = 0x02,
        E_FLAG_COLLIDABLE    = 0x04,
        E_FLAG_PHYSICAL      = 0x08,
        E_FLAG_FROZEN        = 0x10,
        E_FLAG_ORPHAN        = 0x20,
        E_FLAG_LEFT_SLAMMED  = 0x40
    };

    enum Species
    {
        E_SPECIES_STANDARD = 0,
        E_SPECIES_STATIC   = 1,
        E_SPECIES_TRAILER  = 2
    };

    enum Manoeuvre
    {
        E_MANOEUVRE_NONE = 0,
        E_MANOEUVRE_EXTREME_SWERVE,
        E_MANOEUVRE_3_POINT_TURN,
        E_MANOEUVRE_GIVE_UP,
        E_MANOEUVRE_STUCK_REVERSE,
        E_MANOEUVRE_COUNT
    };

    enum SympatheticCrashState
    {
        E_SYMPATHETIC_NONE = 0,
        E_SYMPATHETIC_HEADON,
        E_SYMPATHETIC_ACCELERATE,
        E_SYMPATHETIC_HANDBRAKE,
        E_SYMPATHETIC_LOCKUP
    };

    void Construct(VehicleAxles* lpAxles, Matrix44Affine& lOutMatrix);

    void InitialiseAsTrailer(
        VehicleAxles* lpAxles,
        Matrix44Affine& lOutMatrix,
        const Param* lpParam,
        f32 lfRandomVal,
        Hull** lpapHulls,
        u32 luVehicleType,
        const VehicleTypeRuntime* lpVehicleTypeRuntime,
        const VehicleTypeUpdateData* lpVehicleTypeUpdate,
        const Vehicle* lpCabVehicle,
        Matrix44Affine lCabTransform,
        const VehicleTypeUpdateData* lpCabVehicleTypeUpdate,
        const VehicleTypeRuntime* lpCabVehicleTypeRuntime,
        u32 luVehicle,
        VehicleSoaData& lVehicleSoaData,
        u16 luCabIndex);

    void OnPhysical(BrnPhysics::Vehicle::eCrashTrafficType leCrashTrafficType);

    VecFloat GetSpeed() const;
    void SetSpeed(VecFloat lfSpeed);
    Vector3 CalcTowBarPos(Matrix44Affine lTransform,
                          const VehicleTypeRuntime* lpVehicleTypeRuntime) const;
    Vector3 CalcFrontAxlePos(Matrix44Affine lTransform,
                            Vector3 lArticulationPoint,
                            const VehicleTypeRuntime* lpVehicleTypeRuntime) const;
    void UpdateMatrix(const VehicleAxles* lpAxles,
                      Matrix44Affine& lOutMatrix,
                      const VehicleTypeRuntime* lpVehicleTypeRuntime,
                      Vector3 lOldUp);

private:
    u8 muVehicleType;
    u8 muCrashTrafficType;
    u16 muOtherHalfIndex;
    u8 muSpecies;
    u8 mxFlags;
    s8 miPhysicalPartsIndex;
    u8 mxEffectState;
    f32 mfSwerveTime;
    u8 muHeadlightWarmth;
    u8 muIndicatorBulbWarmth;
    u8 muHeadlightFlashPattern;
    u8 muHeadlightFlashState;
    Vector4 mSpeed_DistAcrossLane_SwerveAmount_W;
    Vector4 mPitch_Roll_Steering_WheelRot;
    f32 mfHeadlightTimeToFlash;
    f32 mfIndicatorTimeToFlash;
    s8 miBrakelightState;
    s8 miPhysicalReason;
    s8 miManoeuvre;
    s8 miManoeuvrePhase;
    f32 mfRandomVal;
    EntityId mSympCrashTarget;
    f32 mfPhysicalTime;
    SympatheticCrashState meSympCrashState;
    f32 mfSympCrashTime;
    Vector3 mLinearVelocity;
    f32 mfManoeuvreTime;
    Vector3 mTargetPos;
};
}
