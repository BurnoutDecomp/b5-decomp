#pragma once

#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"
#include "GameSource/World/Traffic/BrnVehicleSoaData.h"
#include "SharedClasses/Traffic/BrnTrafficVehicleType.h"

namespace CgsNumeric { class Random; }

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

    // ---- inline state predicates (X360 inlines these flag/species reads at every
    // call site; the bodies below assert against them by NAME). ----
    bool IsAlive() const { return (mxFlags & E_FLAG_ALIVE) != 0; }
    bool IsPhysical() const { return (mxFlags & E_FLAG_PHYSICAL) != 0; }
    bool IsOfTrailerSpecies() const { return (muSpecies & 0xF) == E_SPECIES_TRAILER; }
    Manoeuvre GetCurrentManoeuvre() const;

    // ---- per-frame state / identity accessors (out-of-line, bodied in the .cpp) ----
    VecFloat GetSpeed() const;
    void SetSpeed(VecFloat lfSpeed);
    VecFloat GetSwerveAmount() const;
    VecFloat GetDistAcrossLane() const;
    VecFloat GetSteering() const;
    VecFloat GetWheelRot() const;
    Vector4 GetPitch_Roll_Steering_WheelRot() const;
    Vector3 GetTargetPos() const;
    Vector3 GetLinearVelocity() const;
    f32 GetSwerveTime() const;
    f32 GetRandomVal() const;
    s32 GetPhysicalReason() const;
    s32 GetCurrentManoeuvrePhase() const;
    EntityId GetSympatheticCrashTarget() const;
    f32 GetHeadlightWarmth() const;
    f32 GetIndicatorBulbWarmth() const;

    bool IsHornOn() const;
    bool IsAlarmOn() const;
    bool IsRightIndicatorOn() const;
    bool IsIndicatingLeft() const;
    bool IsIndicatingRight() const;
    bool IsFlashingHeadlights() const;
    bool AreHeadlightsFlashed() const;
    bool AreBrakelightsOn() const;
    bool IsCrashing() const;
    bool IsRecoveringFromSlam() const;
    bool IsExtremeSwerving() const;
    bool IsBeingChecked() const;
    bool IsNormalPhysical() const;

    void SetSteering(f32 lfValue);
    void SetWheelRot(f32 lfValue);
    void SetSwerveAmount(f32 lfValue);
    void SetSwerveTime(f32 lfValue);
    void SetTargetPos(Vector3 lTargetPos);
    void SetLinearVelocity(Vector3 lLinearVelocity);
    void SetPitch_Roll_Steering_WheelRot(Vector4 lValues);

    void SetHornOn(bool lbOn);
    void SetHeadlightsFlashed(bool lbOn);
    void SetLeftIndicatorOn(bool lbOn);
    void SetRightIndicatorOn(bool lbOn);
    void ToggleLeftIndicatorOn();
    void ToggleRightIndicatorOn();
    void SetBrakelightsOn(bool lbOn);
    void SetIndicatingLeft(bool lbOn);
    void SetIndicatingRight(bool lbOn);
    void SetFlashingHeadlights(bool lbOn, CgsNumeric::Random* lpRand);

    void SetPhysicalReason(s8 liReason);
    void SetSympatheticCrashTarget(EntityId lEntityId);
    void SetOrphan();
    void SetFrozen(bool lbFrozen);
    void SetCurrentManoeuvre(Manoeuvre leManoeuvre);
    void SetCurrentManoeuvrePhase(s8 liPhase);
    void SetWantsToExtremeSwerve(bool lbWants);
    void StartGiveUpManoeuvre();
    void CopyEffectsFromCab(const Vehicle* lpCab);

    void SetPhysical(s8 liPartsIndex, u32 luVehicle, VehicleSoaData& lSoaData);
    void SetNotPhysical(u32 luVehicle, VehicleSoaData& lSoaData);
    void SetDead(u32 luVehicle, VehicleSoaData& lSoaData);
    Vector3 CalcTowBarPos(Matrix44Affine lTransform,
                          const VehicleTypeRuntime* lpVehicleTypeRuntime) const;
    Vector3 CalcFrontAxlePos(Matrix44Affine lTransform,
                            Vector3 lArticulationPoint,
                            const VehicleTypeRuntime* lpVehicleTypeRuntime) const;
    void UpdateMatrix(const VehicleAxles* lpAxles,
                      Matrix44Affine& lOutMatrix,
                      const VehicleTypeRuntime* lpVehicleTypeRuntime,
                      Vector3 lOldUp);

    // Accessors used by BrnReplays::TrafficEntitySerialiser::SetVehicleData @0x82714060
    // (reads mxFlags at this+5 and the physical-parts index when E_FLAG_PHYSICAL is set).
    u8  GetFlags() const { return mxFlags; }
    s32 GetPhysicalPartsIndex() const { return miPhysicalPartsIndex; }

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
