#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h"

#include "GameSource/Math/BrnMathUtils.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficParam.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicleTypeRuntime.h"
#include "SharedClasses/Traffic/BrnTrafficHull.h"
#include "SharedClasses/Traffic/BrnTrafficSection.h"
#include "rw/math/vpu/vector3_operation.h"

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
}
