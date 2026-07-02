#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h"

#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameSource/Math/BrnMathUtils.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficParam.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicleTypeRuntime.h"
#include "GameSource/World/Traffic/BrnVehicleSoaData.h"
#include "SharedClasses/Traffic/BrnTrafficHull.h"
#include "SharedClasses/Traffic/BrnTrafficSection.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/vector4_operation.h"

namespace BrnTraffic
{
namespace
{
const f32 KF_AXLE_LINE_TEST_HALF_HEIGHT = 10.0f;
const f32 KF_TRIANGLE_INTERSECT_EPSILON = 1.0e-8f;
const f32 KF_TRIANGLE_INTERSECT_EDGE_EPSILON = 1.0e-5f;
const f32 KF_MIN_CORRECTION_DIST_SQ = 2.5e-5f;
const u32 KU_MAX_TRAFFIC = 600;
const u32 KU_MAX_STANDARD_TRAFFIC = 400;
const u16 KU_INVALID_VEHICLE = 0xFFFF;

bool TriangleLineSegIntersect(
    Vector3 lV0,
    Vector3 lV1,
    Vector3 lV2,
    Vector3 lLineStart,
    Vector3 lLineDelta,
    Vector3& lPosition,
    Vector3& lTriNormal)
{
    const Vector3 lEdge1 = lV1 - lV0;
    const Vector3 lEdge2 = lV2 - lV0;
    const Vector3 lPVec = rw::math::vpu::Cross(lLineDelta, lEdge2);
    const f32 lfDeterminant = rw::math::vpu::Dot(lEdge1, lPVec);

    if (lfDeterminant <= KF_TRIANGLE_INTERSECT_EPSILON)
    {
        return false;
    }

    const f32 lfLow = -lfDeterminant * KF_TRIANGLE_INTERSECT_EDGE_EPSILON;
    const f32 lfHigh = lfDeterminant - lfLow;
    const Vector3 lTVec = lLineStart - lV0;
    const f32 lfU = rw::math::vpu::Dot(lTVec, lPVec);
    if (lfU < lfLow || lfU > lfHigh)
    {
        return false;
    }

    const Vector3 lQVec = rw::math::vpu::Cross(lTVec, lEdge1);
    const f32 lfV = rw::math::vpu::Dot(lLineDelta, lQVec);
    if (lfV < lfLow || lfU + lfV > lfHigh)
    {
        return false;
    }

    f32 lfLineParam = rw::math::vpu::Dot(lEdge2, lQVec);
    if (lfLineParam < lfLow || lfLineParam > lfHigh)
    {
        return false;
    }

    const f32 lfReciprocalDeterminant = 1.0f / lfDeterminant;
    lfLineParam *= lfReciprocalDeterminant;
    lPosition = lLineStart + lLineDelta * lfLineParam;
    lTriNormal = rw::math::vpu::Normalize(rw::math::vpu::Cross(lEdge1, lEdge2));
    return true;
}
}

bool Axle::TryIntersectWithLane(const LaneRung& lRung0, const LaneRung& lRung1)
{
    const Vector3 lOldPos = mPosAndWheelRadius.GetVector3();
    const Vector3 lUp = GetUp();
    const Vector3 lLineTop = lOldPos + lUp * KF_AXLE_LINE_TEST_HALF_HEIGHT;
    const Vector3 lLineBottom = lOldPos - lUp * KF_AXLE_LINE_TEST_HALF_HEIGHT;
    const Vector3 lLineDelta = lLineBottom - lLineTop;

    Vector3 lNewPos;
    Vector3 lNormal;
    if (!TriangleLineSegIntersect(
            lRung0.maPoints[0],
            lRung1.maPoints[1],
            lRung0.maPoints[1],
            lLineTop,
            lLineDelta,
            lNewPos,
            lNormal)
        && !TriangleLineSegIntersect(
            lRung0.maPoints[0],
            lRung1.maPoints[0],
            lRung1.maPoints[1],
            lLineTop,
            lLineDelta,
            lNewPos,
            lNormal))
    {
        return false;
    }

    if (rw::math::vpu::MagnitudeSquared(lNewPos - lOldPos) > KF_MIN_CORRECTION_DIST_SQ)
    {
        mPosAndWheelRadius.SetVector3(lNewPos);
    }

    CGS_ASSERT(BrnMath::IsNormal(lNormal),
               "Non unit-length normal returned from line-tri intersection");
    CGS_ASSERT(lNormal.y > 0.0f,
               "Upside-down normal being returned from axle-lane intersection");
    SetUp(lNormal);
    return true;
}

void VehicleAxles::UpdateRearAxleForRoadCollision(const Param* lpParam, Hull** lpapHulls)
{
    CGS_ASSERT(lpParam != nullptr, "lpParam");
    CGS_ASSERT(lpapHulls != nullptr, "lpapHulls");

    for (u32 luHistoryIndex = 0;
         luHistoryIndex < KU_PARAM_NUM_SEGMENTS_TO_REMEMBER - 1;
         ++luHistoryIndex)
    {
        u32 luSegmentIndex;
        u32 luHullIndex;
        lpParam->GetHistoryEntry(luHistoryIndex, &luSegmentIndex, &luHullIndex);

        const LaneRung& lRung0 = lpapHulls[luHullIndex]->mpaRungs[luSegmentIndex];
        const LaneRung& lRung1 = lpapHulls[luHullIndex]->mpaRungs[luSegmentIndex + 1];
        if (mBackAxle.TryIntersectWithLane(lRung0, lRung1))
        {
            return;
        }
    }
}

void Vehicle::Construct(VehicleAxles* lpAxles, Matrix44Affine& lOutMatrix)
{
    CGS_ASSERT(lpAxles != nullptr, "lpAxles");

    lpAxles->mFrontAxle.Initialise();
    lpAxles->mBackAxle.Initialise();
    lOutMatrix.SetIdentity();

    mxFlags = 0;
    mfPhysicalTime = 0.0f;
    mxEffectState = 0;
    mfSympCrashTime = 0.0f;
    muOtherHalfIndex = KU_INVALID_VEHICLE;
    muCrashTrafficType = static_cast<u8>(BrnPhysics::Vehicle::eCrashTrafficType_Invalid);
    mSympCrashTarget.muValue = 0xFFFFFFFF;
    meSympCrashState = E_SYMPATHETIC_NONE;
}

void Vehicle::InitialiseAsTrailer(
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
    u16 luCabIndex)
{
    CGS_ASSERT(lpAxles != nullptr, "lpAxles");
    CGS_ASSERT(lpParam != nullptr, "lpParam");
    CGS_ASSERT(lpapHulls != nullptr, "lpapHulls");
    CGS_ASSERT(lpVehicleTypeRuntime != nullptr, "lpVehicleTypeRuntime");
    CGS_ASSERT(lpVehicleTypeUpdate != nullptr, "lpVehicleTypeUpdate");
    CGS_ASSERT(lpCabVehicle != nullptr, "lpCabVehicle");
    CGS_ASSERT(lpCabVehicleTypeUpdate != nullptr, "lpCabVehicleTypeUpdate");
    CGS_ASSERT(lpCabVehicleTypeRuntime != nullptr, "lpCabVehicleTypeRuntime");
    CGS_ASSERT(luVehicle < KU_MAX_TRAFFIC, "luVehicle < KU_MAX_TRAFFIC");
    CGS_ASSERT(!lVehicleSoaData.mAliveVehicles.IsBitSet(luVehicle),
               "!lVehicleSoaData.mAliveVehicles.IsBitSet( luVehicle )");
    CGS_ASSERT(luCabIndex < KU_MAX_STANDARD_TRAFFIC,
               "luCabIndex < KU_MAX_STANDARD_TRAFFIC");

    mxFlags = E_FLAG_ALIVE;
    lVehicleSoaData.mAliveVehicles.SetBit(luVehicle);
    mfRandomVal = lfRandomVal;
    muSpecies = E_SPECIES_TRAILER;
    muVehicleType = static_cast<u8>(luVehicleType);

    Matrix44Affine lTransform = lCabTransform;
    const Vector3 lArticulationPoint =
        lpCabVehicle->CalcTowBarPos(lCabTransform, lpCabVehicleTypeRuntime);
    const Vector3 lFrontAxlePos =
        CalcFrontAxlePos(lTransform, lArticulationPoint, lpVehicleTypeRuntime);
    const Vector3 lBackAxlePos =
        lFrontAxlePos
        - lTransform.At()
            * (lpVehicleTypeRuntime->GetForwardAxleOffset()
               - lpVehicleTypeRuntime->GetBackAxleOffset());

    lpAxles->mFrontAxle.mPosAndWheelRadius.SetVector3(lFrontAxlePos);
    lpAxles->mBackAxle.mPosAndWheelRadius.SetVector3(lBackAxlePos);
    lpAxles->mFrontAxle.mPosAndWheelRadius.SetPlus(lpVehicleTypeUpdate->mfWheelRadius);
    lpAxles->mBackAxle.mPosAndWheelRadius.SetPlus(lpVehicleTypeUpdate->mfWheelRadius);

    u32 luSegmentIndex;
    u32 luHullIndex;
    lpParam->GetHistoryEntry(0, &luSegmentIndex, &luHullIndex);
    const LaneRung& lRung0 = lpapHulls[luHullIndex]->mpaRungs[luSegmentIndex];
    const LaneRung& lRung1 = lpapHulls[luHullIndex]->mpaRungs[luSegmentIndex + 1];
    lpAxles->mFrontAxle.ForceIntersectWithLane(lRung0, lRung1);
    lpAxles->mBackAxle.ForceIntersectWithLane(lRung0, lRung1);

    UpdateMatrix(
        lpAxles,
        lTransform,
        lpVehicleTypeRuntime,
        Vector3{ 0.0f, 1.0f, 0.0f, 0.0f });
    lOutMatrix = lTransform;

    mxEffectState = 0;
    muHeadlightWarmth = 0xFF;
    mPitch_Roll_Steering_WheelRot.SetZero();
    muHeadlightFlashState = 0xFF;
    mSpeed_DistAcrossLane_SwerveAmount_W.SetZero();
    miBrakelightState = 0;
    muHeadlightFlashPattern = 3;
    miPhysicalReason = -1;
    SetSpeed(lpCabVehicle->GetSpeed());
    miManoeuvre = E_MANOEUVRE_NONE;
    muOtherHalfIndex = luCabIndex;
    mfSwerveTime = 0.0f;
    mLinearVelocity = lOutMatrix.At() * mSpeed_DistAcrossLane_SwerveAmount_W.x;

    lVehicleSoaData.mArticulatedVehicles.SetBit(luVehicle);
    muCrashTrafficType = static_cast<u8>(BrnPhysics::Vehicle::eCrashTrafficType_Invalid);
    mSympCrashTarget.muValue = 0xFFFFFFFF;
}

void Vehicle::OnPhysical(BrnPhysics::Vehicle::eCrashTrafficType leCrashTrafficType)
{
    muCrashTrafficType = static_cast<u8>(leCrashTrafficType);
    mfPhysicalTime = 0.0f;

    if (leCrashTrafficType == BrnPhysics::Vehicle::eCrashTrafficType_Slammed)
    {
        mxFlags |= E_FLAG_LEFT_SLAMMED;
    }
}

// ---------------------------------------------------------------------------
// Front-axle position bridge (X360 @0x82753428): articulation point pushed
// forward along the transform's At() axis by (fwd axle offset - trailer pivot).
// The asm validates lTransform (per-row x==x NaN check) then does a single
// vmaddfp: lFrontAxlePos = lArticulationPoint + At() * (fwdAxleOffset - trailerPivot).
// ---------------------------------------------------------------------------
Vector3 Vehicle::CalcFrontAxlePos(Matrix44Affine lTransform,
                                  Vector3 lArticulationPoint,
                                  const VehicleTypeRuntime* lpVehicleTypeRuntime) const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT(lpVehicleTypeRuntime != nullptr, "lpVehicleTypeRuntime");
    CGS_ASSERT(rw::math::vpu::IsValid(lTransform), "RwMath::IsValid( lTransform )");

    const f32 lfReach = lpVehicleTypeRuntime->GetForwardAxleOffset()
                        - lpVehicleTypeRuntime->GetTrailerPivotDistance();
    return lArticulationPoint + lTransform.At() * lfReach;
}

// ---------------------------------------------------------------------------
// Per-frame book-keeping vector accessors. The four packed lanes are:
//   mSpeed_DistAcrossLane_SwerveAmount_W = { Speed, DistAcrossLane, SwerveAmount, - }
//   mPitch_Roll_Steering_WheelRot        = { Pitch, Roll, Steering, WheelRot }
// Each getter broadcasts one lane into a VecFloat (vspltw); setters insert one
// lane (vrlimi) leaving the others intact.
// ---------------------------------------------------------------------------
VecFloat Vehicle::GetSpeed() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    return rw::math::vpu::Splat(mSpeed_DistAcrossLane_SwerveAmount_W.x);
}

VecFloat Vehicle::GetDistAcrossLane() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    return rw::math::vpu::Splat(mSpeed_DistAcrossLane_SwerveAmount_W.y);
}

VecFloat Vehicle::GetSwerveAmount() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    return rw::math::vpu::Splat(mSpeed_DistAcrossLane_SwerveAmount_W.z);
}

VecFloat Vehicle::GetSteering() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    return rw::math::vpu::Splat(mPitch_Roll_Steering_WheelRot.z);
}

VecFloat Vehicle::GetWheelRot() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    return rw::math::vpu::Splat(mPitch_Roll_Steering_WheelRot.w);
}

Vector4 Vehicle::GetPitch_Roll_Steering_WheelRot() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    return mPitch_Roll_Steering_WheelRot;
}

Vector3 Vehicle::GetTargetPos() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    return mTargetPos;
}

Vector3 Vehicle::GetLinearVelocity() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT(rw::math::vpu::IsValid(mLinearVelocity), "IsValid( mLinearVelocity )");
    return mLinearVelocity;
}

f32 Vehicle::GetSwerveTime() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    return mfSwerveTime;
}

f32 Vehicle::GetRandomVal() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    return mfRandomVal;
}

s32 Vehicle::GetPhysicalReason() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    return miPhysicalReason;
}

EntityId Vehicle::GetSympatheticCrashTarget() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT(mSympCrashTarget.muValue != 0xFFFFFFFF, "mSympCrashTarget.IsValid()");
    return mSympCrashTarget;
}

// Headlight/indicator-bulb warmth are stored as u8 [0,255]; the asm returns
// the byte multiplied by 1/255 (0.0039215689f) as a normalised [0,1] float.
f32 Vehicle::GetHeadlightWarmth() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    return muHeadlightWarmth * (1.0f / 255.0f);
}

f32 Vehicle::GetIndicatorBulbWarmth() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    return muIndicatorBulbWarmth * (1.0f / 255.0f);
}

// ---- effect-state bit predicates (mxEffectState @+7) ----
//   bit0 = horn, bit1 = left-indicator, bit2 = right-indicator, bit3 = headlights-flashed,
//   bit4 = alarm, bit5 = indicating-left, bit6 = indicating-right, bit7 = flashing-headlights
bool Vehicle::IsHornOn() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    return (mxEffectState & 0x01) != 0;
}

bool Vehicle::IsAlarmOn() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    return ((mxEffectState >> 4) & 1) != 0;
}

bool Vehicle::IsRightIndicatorOn() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    return ((mxEffectState >> 2) & 1) != 0;
}

bool Vehicle::IsIndicatingLeft() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    return ((mxEffectState >> 5) & 1) != 0;
}

bool Vehicle::IsIndicatingRight() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    return ((mxEffectState >> 6) & 1) != 0;
}

bool Vehicle::IsFlashingHeadlights() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    return (mxEffectState >> 7) != 0;
}

bool Vehicle::AreHeadlightsFlashed() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    return ((mxEffectState >> 3) & 1) != 0;
}

bool Vehicle::AreBrakelightsOn() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    return miBrakelightState > 0;
}

// ---- crash/physical classifiers. IsCrashing/IsRecoveringFromSlam/IsBeingChecked test
// muCrashTrafficType (offset 1, asm `lbz r11,1`); IsExtremeSwerving/IsNormalPhysical test
// miPhysicalReason (offset 0x39). The IsPhysical() assert fires only on the hit path. ----
bool Vehicle::IsCrashing() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    if (muCrashTrafficType != 0)
    {
        return false;
    }
    CGS_ASSERT(IsPhysical(), "IsPhysical()");
    return true;
}

bool Vehicle::IsRecoveringFromSlam() const
{
    if (muCrashTrafficType != 3)
    {
        return false;
    }
    CGS_ASSERT(IsPhysical(), "IsPhysical()");
    return true;
}

bool Vehicle::IsExtremeSwerving() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    if (miPhysicalReason != 4)
    {
        return false;
    }
    CGS_ASSERT(IsPhysical(), "IsPhysical()");
    return true;
}

bool Vehicle::IsBeingChecked() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    if (muCrashTrafficType != 1)
    {
        return false;
    }
    CGS_ASSERT(IsPhysical(), "IsPhysical()");
    return true;
}

bool Vehicle::IsNormalPhysical() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    if (miPhysicalReason != 5)
    {
        return false;
    }
    CGS_ASSERT(IsPhysical(), "IsPhysical()");
    return true;
}

s32 Vehicle::GetCurrentManoeuvrePhase() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT(miManoeuvre >= E_MANOEUVRE_NONE, "miManoeuvre >= E_MANOEUVRE_NONE");
    CGS_ASSERT(miManoeuvre < E_MANOEUVRE_COUNT, "miManoeuvre < E_MANOEUVRE_COUNT");
    // X360 returns the phase byte zero-extended (lbz with no extsb @0x8270E7A8).
    return static_cast<u8>(miManoeuvrePhase);
}

Vehicle::Manoeuvre Vehicle::GetCurrentManoeuvre() const
{
    return static_cast<Manoeuvre>(miManoeuvre);
}

// ---- single-lane vector setters (insert one lane, NaN-validate the scalar) ----
void Vehicle::SetSteering(f32 lfValue)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT(lfValue == lfValue, "RwMath::IsValid( lfValue )");
    mPitch_Roll_Steering_WheelRot.z = lfValue;
}

void Vehicle::SetWheelRot(f32 lfValue)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT(lfValue == lfValue, "RwMath::IsValid( lfValue )");
    mPitch_Roll_Steering_WheelRot.w = lfValue;
}

void Vehicle::SetSwerveAmount(f32 lfValue)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT(lfValue == lfValue, "RwMath::IsValid( lfValue )");
    mSpeed_DistAcrossLane_SwerveAmount_W.z = lfValue;
}

void Vehicle::SetSwerveTime(f32 lfValue)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    mfSwerveTime = lfValue;
}

void Vehicle::SetTargetPos(Vector3 lTargetPos)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT(rw::math::vpu::IsValid(lTargetPos), "IsValid( lTargetPos )");
    mTargetPos = lTargetPos;
}

void Vehicle::SetLinearVelocity(Vector3 lLinearVelocity)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT(rw::math::vpu::IsValid(lLinearVelocity), "IsValid( lLinearVelocity )");
    mLinearVelocity = lLinearVelocity;
}

void Vehicle::SetPitch_Roll_Steering_WheelRot(Vector4 lValues)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT(lValues.x == lValues.x && lValues.y == lValues.y
                   && lValues.z == lValues.z && lValues.w == lValues.w,
               "RwMath::IsValid( lValues )");
    mPitch_Roll_Steering_WheelRot = lValues;
}

// ---- effect-state bit setters / togglers ----
void Vehicle::SetHornOn(bool lbOn)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    if (lbOn)
    {
        mxEffectState |= 0x01;
    }
    else
    {
        mxEffectState &= 0xFE;
    }
}

void Vehicle::SetHeadlightsFlashed(bool lbOn)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    if (lbOn)
    {
        mxEffectState |= 0x08;
    }
    else
    {
        mxEffectState &= 0xF7;
    }
}

void Vehicle::SetLeftIndicatorOn(bool lbOn)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    if (lbOn)
    {
        mxEffectState |= 0x02;
    }
    else
    {
        mxEffectState &= 0xFD;
    }
}

void Vehicle::SetRightIndicatorOn(bool lbOn)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    if (lbOn)
    {
        mxEffectState |= 0x04;
    }
    else
    {
        mxEffectState &= 0xFB;
    }
}

void Vehicle::ToggleLeftIndicatorOn()
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    mxEffectState ^= 0x02;
}

void Vehicle::ToggleRightIndicatorOn()
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    mxEffectState ^= 0x04;
}

// Brakelight state is a signed [-6,6] hysteresis counter: each "on" call steps
// it up and clamps to +6 once positive, each "off" call steps down to -6.
void Vehicle::SetBrakelightsOn(bool lbOn)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    if (lbOn)
    {
        miBrakelightState = static_cast<s8>(miBrakelightState + 1);
        if (miBrakelightState > 0)
        {
            miBrakelightState = 6;
        }
    }
    else
    {
        miBrakelightState = static_cast<s8>(miBrakelightState - 1);
        if (miBrakelightState < 0)
        {
            miBrakelightState = -6;
        }
    }
}

void Vehicle::SetIndicatingLeft(bool lbOn)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT(!IsIndicatingRight() || lbOn == false,
               "!IsIndicatingRight() || lbOn == false");
    if (IsIndicatingLeft() != lbOn)
    {
        if (lbOn)
        {
            mfIndicatorTimeToFlash = 0.2f;
            mxEffectState |= 0x20;
            SetLeftIndicatorOn(true);
        }
        else
        {
            mxEffectState &= 0xDF;
            SetLeftIndicatorOn(false);
        }
    }
}

void Vehicle::SetIndicatingRight(bool lbOn)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT(!IsIndicatingLeft() || lbOn == false,
               "!IsIndicatingLeft() || lbOn == false");
    if (IsIndicatingRight() != lbOn)
    {
        if (lbOn)
        {
            mfIndicatorTimeToFlash = 0.2f;
            mxEffectState |= 0x40;
            SetRightIndicatorOn(true);
        }
        else
        {
            mxEffectState &= 0xBF;
            SetRightIndicatorOn(false);
        }
    }
}

void Vehicle::SetPhysicalReason(s8 liReason)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    miPhysicalReason = liReason;
}

void Vehicle::SetSympatheticCrashTarget(EntityId lEntityId)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT(lEntityId.muValue != 0xFFFFFFFF, "lEntityId.IsValid()");
    mSympCrashTarget = lEntityId;
}

void Vehicle::SetOrphan()
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    mxFlags |= E_FLAG_ORPHAN;
}

void Vehicle::SetFrozen(bool lbFrozen)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT(IsPhysical(), "IsPhysical()");
    if (lbFrozen)
    {
        mxFlags |= E_FLAG_FROZEN;
    }
    else
    {
        mxFlags &= 0xEF;
    }
}

void Vehicle::SetCurrentManoeuvre(Manoeuvre leManoeuvre)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT(static_cast<s32>(leManoeuvre) >= E_MANOEUVRE_NONE,
               "(int32_t)leManoeuvre >= E_MANOEUVRE_NONE");
    CGS_ASSERT(static_cast<s32>(leManoeuvre) < E_MANOEUVRE_COUNT,
               "(int32_t)leManoeuvre < E_MANOEUVRE_COUNT");
    if (miManoeuvre != static_cast<s8>(leManoeuvre))
    {
        miManoeuvrePhase = 0;
    }
    miManoeuvre = static_cast<s8>(leManoeuvre);
    mfManoeuvreTime = 0.0f;
}

void Vehicle::SetCurrentManoeuvrePhase(s8 liPhase)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT(miManoeuvre >= E_MANOEUVRE_NONE, "miManoeuvre >= E_MANOEUVRE_NONE");
    CGS_ASSERT(miManoeuvre < E_MANOEUVRE_COUNT, "miManoeuvre < E_MANOEUVRE_COUNT");
    miManoeuvrePhase = liPhase;
}

// Begin/cancel an extreme swerve only when no other manoeuvre is in progress:
// on requests EXTREME_SWERVE iff currently NONE; off reverts to NONE iff swerving.
void Vehicle::SetWantsToExtremeSwerve(bool lbWants)
{
    if (lbWants)
    {
        if (GetCurrentManoeuvre() != E_MANOEUVRE_NONE)
        {
            return;
        }
        miManoeuvre = E_MANOEUVRE_EXTREME_SWERVE;
    }
    else
    {
        if (GetCurrentManoeuvre() != E_MANOEUVRE_EXTREME_SWERVE)
        {
            return;
        }
        miManoeuvre = E_MANOEUVRE_NONE;
    }
}

void Vehicle::StartGiveUpManoeuvre()
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    if (miManoeuvre != E_MANOEUVRE_GIVE_UP)
    {
        miManoeuvrePhase = 0;
    }
    mfManoeuvreTime = 0.0f;
    miManoeuvre = E_MANOEUVRE_GIVE_UP;
    mfIndicatorTimeToFlash = 0.40000001f;
    SetHeadlightsFlashed(false);
    SetLeftIndicatorOn(false);
    SetRightIndicatorOn(false);
}

// ---------------------------------------------------------------------------
// Physical-state transitions. Each mirrors a FastBitArray bit in the shared
// VehicleSoaData: SetPhysical sets the mPhysicalVehicles bit + stores the parts
// index; SetNotPhysical / SetDead clear their respective bits and reset indices.
// ---------------------------------------------------------------------------
void Vehicle::SetPhysical(s8 liPartsIndex, u32 luVehicle, VehicleSoaData& lSoaData)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT(!IsPhysical(), "!IsPhysical()");
    CGS_ASSERT(lSoaData.mAliveVehicles.IsBitSet(luVehicle),
               "lSoaData.mAliveVehicles.IsBitSet( luVehicle )");
    CGS_ASSERT(lSoaData.mPhysicalVehicles.IsBitSet(luVehicle) == IsPhysical(),
               "lSoaData.mPhysicalVehicles.IsBitSet( luVehicle ) == IsPhysical()");
    CGS_ASSERT(liPartsIndex >= 0, "liPartsIndex >= 0");

    mxFlags |= E_FLAG_PHYSICAL;
    lSoaData.mPhysicalVehicles.SetBit(luVehicle);
    miPhysicalPartsIndex = liPartsIndex;
}

void Vehicle::SetNotPhysical(u32 luVehicle, VehicleSoaData& lSoaData)
{
    CGS_ASSERT(IsPhysical(), "IsPhysical()");
    CGS_ASSERT(lSoaData.mPhysicalVehicles.IsBitSet(luVehicle) == IsPhysical(),
               "lSoaData.mPhysicalVehicles.IsBitSet( luVehicle ) == IsPhysical()");
    CGS_ASSERT(miPhysicalPartsIndex >= 0, "miPhysicalPartsIndex >= 0");

    mxFlags &= 0xE7;
    lSoaData.mPhysicalVehicles.UnSetBit(luVehicle);
    miPhysicalPartsIndex = -1;
    miPhysicalReason = -1;
    muCrashTrafficType = static_cast<u8>(-1);
}

void Vehicle::SetDead(u32 luVehicle, VehicleSoaData& lSoaData)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT(lSoaData.mAliveVehicles.IsBitSet(luVehicle),
               "lSoaData.mAliveVehicles.IsBitSet( luVehicle )");

    mxFlags &= 0xDE;
    lSoaData.mAliveVehicles.UnSetBit(luVehicle);
}

// ---------------------------------------------------------------------------
// SetFlashingHeadlights (X360 @0x827536D0): starting a flash resets the flash
// timer, raises effect-state bit7, zeroes the flash state, and draws a fresh
// three-way flash pattern from the shared RNG. The asm inlines
// CgsNumeric::Random::RandomUInt() (return muSeed >> 32, then
// muSeed = muSeed * 0x5851F42D4C957F2D + 1 @0x827537D0..E4) followed by the
// unsigned %3 reduction (mulhwu 0xAAAAAAAB idiom @0x827537E8). The if-condition
// reads the bit via inlined IsFlashingHeadlights() (its IsAlive assert fires at
// header line 0x6DD @0x82753754). Both paths end in SetHeadlightsFlashed(false).
// ---------------------------------------------------------------------------
void Vehicle::SetFlashingHeadlights(bool lbOn, CgsNumeric::Random* lpRand)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT(lpRand != nullptr, "lpRand");

    if (IsFlashingHeadlights() != lbOn)
    {
        if (lbOn)
        {
            mfHeadlightTimeToFlash = 0.0f;
            mxEffectState |= 0x80;
            muHeadlightFlashState = 0;
            muHeadlightFlashPattern = static_cast<u8>(lpRand->RandomUInt() % 3);
            SetHeadlightsFlashed(false);
        }
        else
        {
            mxEffectState &= 0x7F;
            SetHeadlightsFlashed(false);
        }
    }
}

// CopyEffectsFromCab (X360 @0x82704E50): a trailer mirrors its cab's effect
// state byte and brakelight counter so its lights stay in sync.
void Vehicle::CopyEffectsFromCab(const Vehicle* lpCab)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT(IsOfTrailerSpecies(), "IsOfTrailerSpecies()");
    CGS_ASSERT(lpCab != nullptr, "lpCab");

    mxEffectState = lpCab->mxEffectState;
    miBrakelightState = lpCab->miBrakelightState;
}
}
