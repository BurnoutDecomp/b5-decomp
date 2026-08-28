#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h"

#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameSource/Math/BrnMathUtils.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficPatternGenerator.h"
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
// X360 .data 0x8300D190. Zero in the image; an unnamed dynamic-initialiser thunk @0x82C67830
// seeds it: lfs f0, flt_820054CC (= 20.0f); vspltw v0,v0,0; stvx128 -> all four lanes 20.0f.
const VecFloat KF_VEHICLE_UPDATE_MATRIX_OLD_UP_FACTOR = { 20.0f, 20.0f, 20.0f, 20.0f };

namespace
{
const f32 KF_AXLE_LINE_TEST_HALF_HEIGHT = 10.0f;
const f32 KF_TRIANGLE_INTERSECT_EPSILON = 1.0e-8f;
const f32 KF_TRIANGLE_INTERSECT_EDGE_EPSILON = 1.0e-5f;
const f32 KF_MIN_CORRECTION_DIST_SQ = 2.5e-5f;
// Stand-in for the un-dumped flt_820C0BC0 (see ForceIntersectWithLane's FLAG); the value is
// the SDK's default IsZero tolerance.
const f32 KF_AXLE_LANE_NORMAL_EPSILON = 1.0e-6f;
// flt_82004270, splatted by InitialiseAsStandard @0x8275F828; Feb-2007 spells it
// KF_VEHICLE_START_DISTANCE_FROM_TARGET.
const f32 KF_VEHICLE_START_DISTANCE_FROM_TARGET = 3.0f;
// The ship folds Feb-2007's Lerp(KF_VEHICLE_MIN/MAX_INDICATOR_FLASH_TIME, 0.5f) to a literal
// 0.3f, and uses the 0.4f MAX value for the alarm / give-up blink period.
const f32 KF_VEHICLE_INDICATOR_FLASH_TIME = 0.30000001f;
const f32 KF_VEHICLE_ALARM_FLASH_TIME     = 0.40000001f;
// The alarm arm toggles horn|left|right|headlights-flashed; the give-up arm toggles the two
// indicator bits (hazards). Masks read straight off 0x82756DEC / 0x82756E5C.
const u8  KX_EFFECT_ALARM_TOGGLE_MASK  = 0x0F;
const u8  KX_EFFECT_HAZARD_TOGGLE_MASK = 0x06;
const s32 KI_BULB_WARMTH_MAX = 255;

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

// Axle::ForceIntersectWithLane @0x8275ED98. Drop the axle onto the plane of the lane
// quad, along the axle's own up vector, and adopt the lane normal as the new up.
// Reached by Vehicle::InitialiseAsStandard @0x8275F3A0 AND ::InitialiseAsTrailer, so it is
// NOT trailer-only; the ship form takes two rungs, not (hull, segment) as Feb-2007 did.
// The ship dropped Feb-2007's Abs(normal.y) and the +/-KF_AXLE_MAX_ADJUST_DISTANCE clamp:
// neither appears anywhere between the normal assert and the store.
void Axle::ForceIntersectWithLane(const LaneRung& lRung0, const LaneRung& lRung1)
{
    const Vector3 lCorner0 = lRung0.maPoints[0];
    const Vector3 lEdge0   = lRung0.maPoints[1] - lCorner0;
    const Vector3 lEdge1   = lRung1.maPoints[1] - lCorner0;

    const Vector3 lNormal = rw::math::vpu::Normalize(rw::math::vpu::Cross(lEdge1, lEdge0));
    CGS_ASSERT(rw::math::vpu::IsValid(lNormal), "Invalid normal on lane");

    const f32 lfPlaneDist   = rw::math::vpu::Dot(lNormal, lCorner0);
    const Vector3 lUp       = GetUp();
    const f32 lfUpDotNormal = rw::math::vpu::Dot(lNormal, lUp);

    // FLAG (assert-only): the console compares |lfUpDotNormal| against rodata flt_820C0BC0,
    // which is un-dumped; the SDK default tolerance stands in, as it already does for
    // UpdateMatrix's "Bad AT vector" guard. DELETE-WHEN that literal is dumped.
    CGS_ASSERT(lfUpDotNormal > KF_AXLE_LANE_NORMAL_EPSILON
                   || lfUpDotNormal < -KF_AXLE_LANE_NORMAL_EPSILON,
               "Traffic vehicle has axle at right angles to the lane");

    const Vector3 lOldPos = mPosAndWheelRadius.GetVector3();
    const f32 lfIsectParam =
        (lfPlaneDist - rw::math::vpu::Dot(lNormal, lOldPos)) / lfUpDotNormal;

    mPosAndWheelRadius.SetVector3(lOldPos + lUp * lfIsectParam);
    SetUp(lNormal);
}

// Vehicle::SetSpeed. DWARF BrnTrafficVehicle.h:321; EXPORT HOLE (no per-function JSON), so
// the lane comes from the committed GetSpeed pair: mSpeed_DistAcrossLane_SwerveAmount_W lane
// 0. Its hot caller is the per-vehicle driving update, not the trailer path.
void Vehicle::SetSpeed(VecFloat lfSpeed)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT(lfSpeed.x == lfSpeed.x, "RwMath::IsValid( lfValue )");
    mSpeed_DistAcrossLane_SwerveAmount_W.x = lfSpeed.x;
}

// FLAG PC boot gate: Vehicle::CalcTowBarPos is not reconstructed. No ARTIST per-function
// export and no Feb-2007 counterpart; only Vehicle::InitialiseAsTrailer reaches it, and the
// driving-traffic maker keeps its trailer leg gated for that reason.
// DELETE-WHEN articulated traffic lands.
Vector3 Vehicle::CalcTowBarPos(Matrix44Affine lTransform, const VehicleTypeRuntime* lpTypeRuntime) const
{
    (void)lTransform;
    (void)lpTypeRuntime;
    CGS_ASSERT(false,
               "Vehicle::CalcTowBarPos not reconstructed (trailer path) [FLAG PC boot gate]");
    return Vector3{ 0.0f, 0.0f, 0.0f };
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

// VehicleAxles::SetFromVehicleTransform (X360 @0x82756738), tail-called only by
// Vehicle::InitialiseAsStatic @0x827567F0. No lane intersection and no physics.
// Lane facts: front/back = At*axleOffset + Pos, stored xyz-only (vrlimi keeps the old lane 3);
// both plus lanes then take VehicleTypeUpdateData::mfWheelRadius; both ups take Up() the same
// xyz-only way. The offsets are splats of lanes 3 and 2 of the type runtime's pivot vector.
void VehicleAxles::SetFromVehicleTransform(Matrix44Affine lTransform,
                                           const VehicleTypeRuntime* lpVehicleTypeRuntime,
                                           const VehicleTypeUpdateData* lpVehicleTypeUpdate)
{
    const Vector3 lAt = lTransform.At();
    const Vector3 lPos = lTransform.Pos();

    mFrontAxle.mPosAndWheelRadius.SetVector3(
        lPos + lAt * lpVehicleTypeRuntime->GetForwardAxleOffset());
    mBackAxle.mPosAndWheelRadius.SetVector3(
        lPos + lAt * lpVehicleTypeRuntime->GetBackAxleOffset());

    mFrontAxle.mPosAndWheelRadius.SetPlus(lpVehicleTypeUpdate->mfWheelRadius);
    mBackAxle.mPosAndWheelRadius.SetPlus(lpVehicleTypeUpdate->mfWheelRadius);

    mFrontAxle.SetUp(lTransform.Up());
    mBackAxle.SetUp(lTransform.Up());
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
    muOtherHalfIndex = static_cast<u16>(KU_INVALID_VEHICLE);
    muCrashTrafficType = static_cast<u8>(BrnPhysics::Vehicle::eCrashTrafficType_Invalid);
    mSympCrashTarget.muValue = 0xFFFFFFFF;
    meSympCrashState = E_SYMPATHETIC_NONE;
}

// Vehicle::InitialiseAsStandard (X360 @0x8275F3A0) -- makes a DRIVING traffic car exist on a
// lane param. Assert order is the asm's, baked at BrnTrafficVehicle.cpp:390-399.
//
// TWO ASM-FORCED DIVERGENCES FROM Feb-2007: the ship writes 0.0f into the W lane of
// mSpeed_DistAcrossLane_SwerveAmount_W where the leak wrote lpVehicleTraits->GetSwervingModifier()
// (arg_5C is read nowhere but the null assert), and it seats the axles with
// ForceIntersectWithLane against the param's history rung pair instead of a plain Y-axis up.
void Vehicle::InitialiseAsStandard(
    VehicleAxles* lpAxles,
    Matrix44Affine& lOutMatrix,
    const Param* lpParam,
    f32 lfRandomVal,
    Hull** lpapHulls,
    u32 luVehicleType,
    const VehicleTypeRuntime* lpVehicleTypeRuntime,
    const VehicleTypeUpdateData* lpVehicleTypeUpdate,
    const VehicleTraits* lpVehicleTraits,
    f32 lfDistAcrossLane,
    f32 lfSpeed,
    Vector3 lParamPos,
    Vector3 lParamDirection,
    u32 luVehicle,
    VehicleSoaData& lVehicleSoaData,
    u16 luTrailerIndex)
{
    CGS_ASSERT(lpAxles != nullptr, "lpAxles");
    CGS_ASSERT(lpParam != nullptr, "lpParam");
    CGS_ASSERT(lpapHulls != nullptr, "lpapHulls");
    CGS_ASSERT(lpVehicleTypeRuntime != nullptr, "lpVehicleTypeRuntime");
    CGS_ASSERT(lpVehicleTypeUpdate != nullptr, "lpVehicleTypeUpdate");
    CGS_ASSERT(lpVehicleTraits != nullptr, "lpVehicleTraits");
    CGS_ASSERT(rw::math::vpu::IsValid(lParamPos), "RwMath::IsValid( lParamPos )");
    CGS_ASSERT(rw::math::vpu::IsValid(lParamDirection), "RwMath::IsValid( lParamDirection )");
    CGS_ASSERT(!lVehicleSoaData.mAliveVehicles.IsBitSet(luVehicle),
               "!lVehicleSoaData.mAliveVehicles.IsBitSet( luVehicle )");
    CGS_ASSERT((luTrailerIndex >= KU_TRAILER_TRAFFIC_OFFSET
                && luTrailerIndex < KU_MAX_TOTAL_TRAFFIC)
                   || luTrailerIndex == KU_INVALID_VEHICLE,
               "( luTrailerIndex >= KU_TRAILER_TRAFFIC_OFFSET && luTrailerIndex < "
               "KU_MAX_TOTAL_TRAFFIC ) || luTrailerIndex == KU_INVALID_VEHICLE");
    (void)lpVehicleTraits;   // assert-only in the ship body

    mxFlags = E_FLAG_ALIVE;
    lVehicleSoaData.mAliveVehicles.SetBit(luVehicle);

    const Vector3 lFrontAxlePos =
        lParamPos - lParamDirection * KF_VEHICLE_START_DISTANCE_FROM_TARGET;
    const Vector3 lBackAxlePos =
        lFrontAxlePos
        - lParamDirection * (lpVehicleTypeRuntime->GetForwardAxleOffset()
                             - lpVehicleTypeRuntime->GetBackAxleOffset());

    mfRandomVal   = lfRandomVal;
    muSpecies     = E_SPECIES_STANDARD;
    muVehicleType = static_cast<u8>(luVehicleType);

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

    UpdateMatrix(lpAxles, lOutMatrix, lpVehicleTypeRuntime,
                 Vector3{ 0.0f, 1.0f, 0.0f, 0.0f });

    mxEffectState           = 0;
    miBrakelightState       = 0;
    miPhysicalReason        = -1;
    muHeadlightWarmth       = 0xFF;
    muHeadlightFlashPattern = PatternGenerator::E_HEADLIGHTFLASH_COUNT;
    muHeadlightFlashState   = 0xFF;

    mPitch_Roll_Steering_WheelRot.SetZero();
    mSpeed_DistAcrossLane_SwerveAmount_W =
        Vector4{ lfSpeed, lfDistAcrossLane, 0.0f, 0.0f };

    miManoeuvre      = E_MANOEUVRE_NONE;
    mfSwerveTime     = 0.0f;
    muOtherHalfIndex = luTrailerIndex;
    mLinearVelocity  = lOutMatrix.At() * mSpeed_DistAcrossLane_SwerveAmount_W.x;

    if (luTrailerIndex != KU_INVALID_VEHICLE)
    {
        lVehicleSoaData.mArticulatedVehicles.SetBit(luVehicle);
    }

    muCrashTrafficType = static_cast<u8>(BrnPhysics::Vehicle::eCrashTrafficType_Invalid);
    mSympCrashTarget.muValue = 0xFFFFFFFF;
}

// Vehicle::InitialiseAsStatic (X360 @0x827567F0) -- makes a parked traffic car exist. No
// physics body, no lane Param, no plan: stamp the record alive, copy the authored
// StaticTrafficVehicle transform to the module's transform slot, seat both axles off it.
//
// Assert order is the asm's, baked at BrnTrafficVehicle.cpp:475-479. The two
// CgsFastBitArray.h:396/:431 range asserts the asm also inlines belong to FastBitArray's own
// checked accessors, not to this body.
//
// mLinearVelocity: the asm multiplies the At axis by the just-zeroed speed lane rather than
// storing a literal zero vector. Kept in that form so the intent survives.
void Vehicle::InitialiseAsStatic(
    VehicleAxles* lpAxles,
    Matrix44Affine& lOutMatrix,
    f32 lfRandomVal,
    u32 luVehicleType,
    const VehicleTypeRuntime* lpVehicleTypeRuntime,
    const VehicleTypeUpdateData* lpVehicleTypeUpdate,
    Matrix44Affine lTransform,
    u32 luVehicle,
    VehicleSoaData& lVehicleSoaData)
{
    CGS_ASSERT(lpAxles != nullptr, "lpAxles");
    CGS_ASSERT(lpVehicleTypeRuntime != nullptr, "lpVehicleTypeRuntime");
    CGS_ASSERT(lpVehicleTypeUpdate != nullptr, "lpVehicleTypeUpdate");
    CGS_ASSERT(rw::math::vpu::IsValid(lTransform), "RwMath::IsValid( lTransform )");
    CGS_ASSERT(!lVehicleSoaData.mAliveVehicles.IsBitSet(luVehicle),
               "!lVehicleSoaData.mAliveVehicles.IsBitSet( luVehicle )");

    mxFlags = E_FLAG_ALIVE;
    lVehicleSoaData.mAliveVehicles.SetBit(luVehicle);

    mfRandomVal = lfRandomVal;
    muSpecies = E_SPECIES_STATIC;
    muVehicleType = static_cast<u8>(luVehicleType);

    lOutMatrix = lTransform;

    mxEffectState = 0;
    muHeadlightWarmth = 0;
    muHeadlightFlashPattern = PatternGenerator::E_HEADLIGHTFLASH_COUNT;
    muHeadlightFlashState = 0xFF;
    miBrakelightState = 0;
    miPhysicalReason = -1;

    lpAxles->SetFromVehicleTransform(lTransform, lpVehicleTypeRuntime, lpVehicleTypeUpdate);

    mPitch_Roll_Steering_WheelRot.SetZero();
    mSpeed_DistAcrossLane_SwerveAmount_W.SetZero();

    miManoeuvre = E_MANOEUVRE_NONE;
    mfSwerveTime = 0.0f;
    muOtherHalfIndex = static_cast<u16>(KU_INVALID_VEHICLE);
    muCrashTrafficType = static_cast<u8>(BrnPhysics::Vehicle::eCrashTrafficType_Invalid);

    mLinearVelocity = lTransform.At() * mSpeed_DistAcrossLane_SwerveAmount_W.x;
    mSympCrashTarget.muValue = 0xFFFFFFFF;
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
    // The baked string spells the bound KU_MAX_TRAFFIC; KU_MAX_TOTAL_TRAFFIC is its home name.
    CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luVehicle < KU_MAX_TRAFFIC");
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

// Vehicle::UpdateMatrix (X360 @0x82757768) -- rebuild a traffic car's world transform from
// its two axle positions and axle up-vectors. DWARF BrnTrafficVehicle.h:310; the ship grew
// the `Vector3 lOldUp` fourth parameter the Feb-2007 three-parameter form lacked.
//
// Lane facts, in asm order: At = Normalize(frontPos - backPos); Up = Normalize(lOldUp*K +
// frontUp + backUp); Pos = frontPos - At*GetForwardAxleOffset(); Right = Normalize(Cross(Up,
// At)); matrix Up = Normalize(Cross(At, Right)); mLinearVelocity = At * GetSpeed(). Both
// crosses use the vpermwi 0x63 (yzx) form, i.e. right-handed Gram-Schmidt for Right/Up/At
// row order.
//
// FLAG (assert-only, behaviour-neutral): the "Bad AT vector" guard compares each |lAxleDelta|
// lane against an explicit epsilon at rodata flt_820C0BC0, which is unread. Reproduced with
// the SDK's default-tolerance IsZero() rather than a made-up constant. DELETE-WHEN that
// literal is dumped out of the .i64.
void Vehicle::UpdateMatrix(const VehicleAxles* lpAxles,
                           Matrix44Affine& lOutMatrix,
                           const VehicleTypeRuntime* lpVehicleTypeRuntime,
                           Vector3 lOldUp)
{
    CGS_ASSERT(lpAxles != nullptr, "lpAxles");
    CGS_ASSERT(rw::math::vpu::IsValid(lOldUp), "IsValid(lOldUp)");

    const Vector3 lFrontAxlePos = lpAxles->mFrontAxle.mPosAndWheelRadius.GetVector3();
    const Vector3 lBackAxlePos  = lpAxles->mBackAxle.mPosAndWheelRadius.GetVector3();
    const Vector3 lAxleDelta    = lFrontAxlePos - lBackAxlePos;

    // X360 streams "Bad AT vector in traffic. Vehicle type <t>, front axle = <v>, back axle
    // = <v>"; the stringized condition carries the same guard.
    CGS_ASSERT(!rw::math::vpu::IsZero(lAxleDelta), "Bad AT vector in traffic");

    const Vector3 lAt = rw::math::vpu::Normalize(lAxleDelta);

    const Vector3 lAxleUpSum = lpAxles->mFrontAxle.GetUp() + lpAxles->mBackAxle.GetUp();
    const Vector3 lUp = rw::math::vpu::Normalize(
        lOldUp * KF_VEHICLE_UPDATE_MATRIX_OLD_UP_FACTOR.x + lAxleUpSum);

    // X360 streams "Invalid UP calculating traffic matrix: <up> (front axle UP = <v>,
    // rear axle UP = <v> )".
    CGS_ASSERT(rw::math::vpu::IsValid(lUp), "Invalid UP calculating traffic matrix");

    lOutMatrix.At() = lAt;
    lOutMatrix.Pos() = lFrontAxlePos - lAt * lpVehicleTypeRuntime->GetForwardAxleOffset();
    lOutMatrix.Right() = rw::math::vpu::Normalize(rw::math::vpu::Cross(lUp, lAt));
    lOutMatrix.Up() =
        rw::math::vpu::Normalize(rw::math::vpu::Cross(lAt, lOutMatrix.Right()));

    // X360 streams "Invalid traffic vehicle transform: <matrix>".
    CGS_ASSERT(rw::math::vpu::IsValid(lOutMatrix), "Invalid traffic vehicle transform");

    // The asm calls the checked GetSpeed() accessor here (bl @0x82757E1C).
    mLinearVelocity = lAt * GetSpeed().x;
}

// Front-axle position bridge (X360 @0x82753428): one vmaddfp pushing the articulation point
// forward along At() by (forward axle offset - trailer pivot), after a per-row NaN check.
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

// Vehicle::UpdateEffects (X360 @0x82756D48, DWARF BrnTrafficVehicle.h:302) -- one blink step
// for the headlight-flash pattern, the indicators and the two bulb-warmth ramps.
//
// The ship grew three things over Feb-2007: the crash prologue, an ALARM arm and a GIVE_UP
// arm. Both new arms run the indicator timer on the 0.4f period, toggle a fixed effect-state
// mask, then SKIP the flash/indicator work and carry IsLeftIndicatorOn() forward as the
// indicator-bulb condition.
void Vehicle::UpdateEffects(f32 lfTimeDelta, s32 liBulbWarmthDelta, CgsNumeric::Random* lpRand)
{
    bool lbIndicatorLit = false;

    if (IsCrashing() || IsSympatheticallyCrashing())
    {
        SetFlashingHeadlights(false, lpRand);
    }

    if (IsAlarmOn())
    {
        mfIndicatorTimeToFlash -= lfTimeDelta;
        if (mfIndicatorTimeToFlash < 0.0f)
        {
            mfIndicatorTimeToFlash += KF_VEHICLE_ALARM_FLASH_TIME;
            mxEffectState ^= KX_EFFECT_ALARM_TOGGLE_MASK;
        }
        lbIndicatorLit = IsLeftIndicatorOn();
    }
    else if (GetCurrentManoeuvre() == E_MANOEUVRE_GIVE_UP)
    {
        mfIndicatorTimeToFlash -= lfTimeDelta;
        if (mfIndicatorTimeToFlash < 0.0f)
        {
            mfIndicatorTimeToFlash += KF_VEHICLE_ALARM_FLASH_TIME;
            mxEffectState ^= KX_EFFECT_HAZARD_TOGGLE_MASK;
        }
        lbIndicatorLit = IsLeftIndicatorOn();
    }
    else
    {
        if (IsFlashingHeadlights())
        {
            bool lbHeadlightsFlashed = AreHeadlightsFlashed();

            // PatternGenerator carries no state; the console passes luPattern in r3 with no
            // `this`, and this tree models the stepper as a member of the empty struct.
            PatternGenerator lPatternGenerator;
            const bool lbPatternPlaying = lPatternGenerator.UpdateHeadlightFlash(
                muHeadlightFlashPattern, lfTimeDelta, &mfHeadlightTimeToFlash,
                &muHeadlightFlashState, &lbHeadlightsFlashed);

            SetHeadlightsFlashed(lbHeadlightsFlashed);

            if (!lbPatternPlaying)
            {
                SetFlashingHeadlights(false, lpRand);
            }
        }

        if (IsIndicatingLeft() || IsIndicatingRight())
        {
            mfIndicatorTimeToFlash -= lfTimeDelta;
            if (mfIndicatorTimeToFlash < 0.0f)
            {
                if (IsIndicatingLeft())
                {
                    ToggleLeftIndicatorOn();
                }
                if (IsIndicatingRight())
                {
                    ToggleRightIndicatorOn();
                }
                mfIndicatorTimeToFlash += KF_VEHICLE_INDICATOR_FLASH_TIME;
            }
        }
    }

    CGS_ASSERT(IsAlive(), "IsAlive()");   // baked BrnTrafficVehicle.h:1743

    s32 liHeadlightWarmth = muHeadlightWarmth;
    if (AreHeadlightsFlashed())
    {
        liHeadlightWarmth -= liBulbWarmthDelta;
        if (liHeadlightWarmth < 0) { liHeadlightWarmth = 0; }
    }
    else
    {
        liHeadlightWarmth += liBulbWarmthDelta;
        if (liHeadlightWarmth > KI_BULB_WARMTH_MAX) { liHeadlightWarmth = KI_BULB_WARMTH_MAX; }
    }
    muHeadlightWarmth = static_cast<u8>(liHeadlightWarmth);

    s32 liIndicatorWarmth = muIndicatorBulbWarmth;
    if (lbIndicatorLit
        || (IsIndicatingLeft() && IsLeftIndicatorOn())
        || (IsIndicatingRight() && IsRightIndicatorOn()))
    {
        liIndicatorWarmth += liBulbWarmthDelta;
        if (liIndicatorWarmth > KI_BULB_WARMTH_MAX) { liIndicatorWarmth = KI_BULB_WARMTH_MAX; }
    }
    else
    {
        liIndicatorWarmth -= liBulbWarmthDelta;
        if (liIndicatorWarmth < 0) { liIndicatorWarmth = 0; }
    }
    muIndicatorBulbWarmth = static_cast<u8>(liIndicatorWarmth);
}

// Packed lanes:
//   mSpeed_DistAcrossLane_SwerveAmount_W = { Speed, DistAcrossLane, SwerveAmount, - }
//   mPitch_Roll_Steering_WheelRot        = { Pitch, Roll, Steering, WheelRot }
// Getters broadcast one lane (vspltw); setters insert one lane (vrlimi), others intact.
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

// Vehicle+0x44 / +0x60 accessors. The console reads/accumulates both fields inline
// (DriveTowardsTarget @0x8273E27C / @0x8273E664, GenerateDriverInputs @0x827492A8 /
// @0x827492D4); the fields are private, so these are the by-name spellings.
f32 Vehicle::GetPhysicalTime() const
{
    return mfPhysicalTime;
}

f32 Vehicle::GetManoeuvreTime() const
{
    return mfManoeuvreTime;
}

void Vehicle::AddPhysicalTime(f32 lfDelta)
{
    mfPhysicalTime += lfDelta;
}

void Vehicle::AddManoeuvreTime(f32 lfDelta)
{
    mfManoeuvreTime += lfDelta;
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

// Warmth is a u8 [0,255]; the asm returns the byte times 1/255 (0.0039215689f).
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
// muCrashTrafficType (+0x01, `lbz r11,1`); IsExtremeSwerving/IsNormalPhysical test
// miPhysicalReason (+0x39). The IsPhysical() assert fires only on the hit path. ----
// EXPORT HOLE (UpdateEffects @0x82756D48 calls it by name). Bit 1 of mxEffectState, the
// partner of the bit-2 IsRightIndicatorOn; SetLeftIndicatorOn / ToggleLeftIndicatorOn in this
// file already write that bit.
bool Vehicle::IsLeftIndicatorOn() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    return ((mxEffectState >> 1) & 1) != 0;
}

// FLAG: DWARF BrnTrafficVehicle.h:393, EXPORT HOLE -- the body is REASONED, not attested.
// meSympCrashState is the only sympathetic-crash state field and Construct seeds it
// E_SYMPATHETIC_NONE. DELETE-WHEN a per-function export or an inlined copy turns up.
bool Vehicle::IsSympatheticallyCrashing() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    return meSympCrashState != E_SYMPATHETIC_NONE;
}

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

// Vehicle::SetAlarmOn @0x8270FC10. Bit 0x10 of mxEffectState; turning the alarm ON also seeds
// mfIndicatorTimeToFlash (+0x34) = 0.4f; any change clears flash/indicators/horn.
void Vehicle::SetAlarmOn(bool lbOn)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");   // BrnTrafficVehicle.h:1498
    if (IsAlarmOn() != lbOn)
    {
        if (lbOn)
        {
            mxEffectState |= 0x10;
            mfIndicatorTimeToFlash = 0.4f;
        }
        else
        {
            mxEffectState &= 0xEF;
        }
        SetHeadlightsFlashed(false);
        SetLeftIndicatorOn(false);
        SetRightIndicatorOn(false);
        SetHornOn(false);
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

// Brakelight state is a signed [-6,6] hysteresis counter.
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

// Physical-state transitions. Each keeps an mxFlags bit and the matching FastBitArray bit in
// the shared VehicleSoaData in lockstep, which is what the paired asserts enforce.
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

// ----------------------------------------------------------------------------
// Vehicle::SetCollidable @0x8271BB30 -- the COLLISION twin of SetHasEntity below, and the
// blocker that kept TrafficEntityModule::UpdateCollidableVehicles out of the tree.
//
// Store map off the asm:
//   set   arm 0x82731xxx..: `*(this+5) |= 4`   then  field |= mask   at soa+160+8*(idx>>6)
//   clear arm 0x82732530..: `*(this+5) &= ~4`  then  field &= ~mask  (`v70 = a4 + 160`)
// soa+160 is mCollidableVehicles, the third FastBitArray<601> member (0/80/160/240 = alive /
// withEntities / collidable / physical, and 0x282D0+0xA0 == the 0x506E*8 field base the
// caller's own out-of-sync check reads).
//
// The iterator, not an index: r5 is a FastBitArray<600>::Iterator by reference. miIndex is the
// vehicle index, mxMask is the precomputed 1<<(miIndex&63), and the console ORs/ANDCs THAT
// mask rather than recomputing it. SetBit/UnSetBit(miIndex) is value-identical --
// Iterator::Advance maintains mxMask as exactly 1<<(miIndex%64) -- so the bit math is written
// through the container's own named API here and the iterator supplies the index.
//
// The four "Index N is out of range (max bits: 600)" CgsFastBitArray.h asserts the asm carries
// (h:235/374/415/442/463) are the container's own inlined bounds checks, which this tree's
// FastBitArray deliberately does not reproduce; the ONE bound that is not the container's --
// the caller-visible `miIndex < KU_MAX_TOTAL_TRAFFIC` -- is kept.
// ----------------------------------------------------------------------------
void Vehicle::SetCollidable(bool lbCollidable,
                            const CgsContainers::FastBitArray<VehicleSoaData::KU_MAX_VEHICLES>::Iterator& lrVehicleIt,
                            VehicleSoaData& lSoaData)
{
    const u32 luVehicle = static_cast<u32>(lrVehicleIt.GetIndex());
    CGS_ASSERT(luVehicle < 600u, "luIndex < KU_MAX_TOTAL_TRAFFIC");

    // 0x8271BE4C-ish: the SoA bit and the flag must agree BEFORE the write (the console's
    // `((field & mask) != 0) != ((mxFlags >> 2) & 1)` tripwire).
    CGS_ASSERT(lSoaData.mCollidableVehicles.IsBitSet(luVehicle) == IsCollidable(),
               "lSoaData.mCollidableVehicles.IsBitSet( luVehicle ) == IsCollidable()");

    if (lbCollidable)
    {
        CGS_ASSERT(IsAlive(), "IsAlive()");
        CGS_ASSERT(lSoaData.mAliveVehicles.IsBitSet(luVehicle),
                   "lSoaData.mAliveVehicles.IsBitSet( luVehicle )");

        mxFlags |= static_cast<u8>(E_FLAG_COLLIDABLE);
        lSoaData.mCollidableVehicles.SetBit(luVehicle);
    }
    else
    {
        // `andi.` clears only bit 2; unlike SetDead / SetNotPhysical this arm touches no
        // other flag, and it does NOT assert IsAlive() -- the console clears the collision
        // state of a vehicle that is on its way out.
        mxFlags &= static_cast<u8>(~static_cast<u8>(E_FLAG_COLLIDABLE));
        lSoaData.mCollidableVehicles.UnSetBit(luVehicle);
    }
}

// Vehicle::SetHasEntity @0x8270EB38 -- the scene-entity twin of SetPhysical/SetDead above.
// Store map off the asm: mxFlags at this+0x05 (`ori 2` / `andi. 0xFD`), and the mutated
// array is lSoaData + 0x50 == mVehiclesWithEntities, the second FastBitArray<601> member.
//
// The first assert's polarity is the opposite of what the branch looks like: the console
// branches around FireAssert on `bne` @0x8270EB68, so the condition is the baked string's
// `HasEntity() != lbHasEntity`. Transcribing the branch instead inverts it.
//
// The four "Index N is out of range (max bits: 600)" asserts the asm carries
// (CgsFastBitArray.h:396/:431/:452) are the container's own inlined bounds checks, which this
// tree's FastBitArray deliberately does not reproduce. No behaviour is dropped.
void Vehicle::SetHasEntity(bool lbHasEntity, u32 luVehicle, VehicleSoaData& lSoaData)
{
    CGS_ASSERT(HasEntity() != lbHasEntity, "HasEntity() != lbHasEntity");            // .h:979
    CGS_ASSERT(lSoaData.mVehiclesWithEntities.IsBitSet(luVehicle) == HasEntity(),
               "lSoaData.mVehiclesWithEntities.IsBitSet( luVehicle ) == HasEntity()"); // .h:980

    if (lbHasEntity)
    {
        CGS_ASSERT(IsAlive(), "IsAlive()");                                          // .h:984
        CGS_ASSERT(lSoaData.mAliveVehicles.IsBitSet(luVehicle),
                   "lSoaData.mAliveVehicles.IsBitSet( luVehicle )");                 // .h:985

        mxFlags |= E_FLAG_HASENTITY;
        lSoaData.mVehiclesWithEntities.SetBit(luVehicle);
    }
    else
    {
        // `andi. r11, r11, 0xFD` clears only bit 1. Unlike SetDead / SetNotPhysical above,
        // this arm touches no other flag.
        mxFlags &= static_cast<u8>(~static_cast<u8>(E_FLAG_HASENTITY));
        lSoaData.mVehiclesWithEntities.UnSetBit(luVehicle);
    }
}

// Vehicle::GetCabIndex @0x8270E4C8, DWARF BrnTrafficVehicle.h:339. Assert then one
// `lhz 2(this)`. muOtherHalfIndex is the TRAILER's view of the pair; the cab's twin
// accessor GetTrailerIndex (DWARF :338) reads the same member and is not landed here.
u16 Vehicle::GetCabIndex() const
{
    CGS_ASSERT(IsOfTrailerSpecies(), "IsOfTrailerSpecies()");   // BrnTrafficVehicle.h:778
    return muOtherHalfIndex;
}

// Vehicle::DetachArticulation @0x8270F6C8 (151 insns). Breaks a cab/trailer pair. The 151
// instructions are almost entirely the two inlined CgsFastBitArray range asserts (h:396 for
// the IsBitSet and h:452 for the UnSetBit, each streaming "Index N is out of range (max bits:
// 600)"); the work is four lines. `a3 + 320` is lSoaData + 0x140 == mArticulatedVehicles, the
// fourth 80-byte set. Landed 2026-08-28 -- it is the third and last named blocker on
// TrafficEntityModule::RemoveVehicle @0x8272E370, the junction-FUP relief valve; it is also
// cited by _wT1_01.cpp, _wT2_01.cpp and BrnTrafficEntityModule_KillDyingVehicleEntities.cpp.
// No live caller yet, so this changes nothing today.
void Vehicle::DetachArticulation(u32 luVehicle, VehicleSoaData& lSoaData)
{
    CGS_ASSERT(muOtherHalfIndex != static_cast<u16>(KU_INVALID_VEHICLE),
               "muOtherHalfIndex != KU_INVALID_VEHICLE");                    // .h 1079 (0x437)
    CGS_ASSERT(lSoaData.mArticulatedVehicles.IsBitSet(luVehicle),
               "lSoaData.mArticulatedVehicles.IsBitSet( luVehicle )");       // .h 1080 (0x438)

    muOtherHalfIndex = static_cast<u16>(KU_INVALID_VEHICLE);   // `li r11,-1 ; sth r11,2(r15)`
    lSoaData.mArticulatedVehicles.UnSetBit(luVehicle);
}

// Vehicle::GetTrailerIndex @0x8270E468, DWARF BrnTrafficVehicle.h:338. GetCabIndex's twin:
// the same `lhz 2(this)` behind the MIRRORED assert -- IsOfStandardSpecies(), baked at header
// line 0x302 == 770. Only a cab may ask for its trailer. Landed 2026-08-28: it was named as
// the blocker by three separate park notes (_wT3_01.cpp:225, _wT3_04.cpp:44, _wT2_01.cpp:611)
// and by TrafficEntityModule::RemoveVehicle @0x8272E370, and it is twenty-three instructions.
u16 Vehicle::GetTrailerIndex() const
{
    CGS_ASSERT(IsOfStandardSpecies(), "IsOfStandardSpecies()");  // BrnTrafficVehicle.h:770
    return muOtherHalfIndex;
}

// SetFlashingHeadlights (X360 @0x827536D0). The asm inlines
// CgsNumeric::Random::RandomUInt() (muSeed >> 32, then muSeed = muSeed * 0x5851F42D4C957F2D
// + 1 @0x827537D0..E4) and the unsigned %3 reduction (mulhwu 0xAAAAAAAB @0x827537E8). The
// IsAlive assert inside the inlined IsFlashingHeadlights() is baked at header line 0x6DD.
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

// Layout pins. NEVER CALLED -- static member functions so offsetof() reaches the private
// members. Absolute offsets are safe here because Axle, VehicleAxles and Vehicle are
// POINTER-FREE (lane registers, bytes, floats, one EntityId, one enum), so widening a host
// pointer cannot move anything and the console offsets are the host offsets. Sources:
//   Vehicle::Construct          @0x82752080  5 / 0x44 / 7 / 0x4C / 2 / 1 / 0x40 / 0x48
//   Vehicle::InitialiseAsStatic @0x827567F0  0 / 4 / 8 / 0xC / 0xE / 0xF / 0x10 / 0x20 /
//                                            0x38 / 0x39 / 0x3A / 0x3C / 0x50
//   Vehicle::GetPhysicalPartsIndex @0x8270F928 -> 6
//   Vehicle::GetIndicatorBulbWarmth @0x82705618 -> 0xD
//   Vehicle::SetFlashingHeadlights @0x827536D0 -> 0x30
//   Vehicle::SetIndicatingLeft     @0x8270FCF8 -> 0x34
//   Vehicle::SetCurrentManoeuvrePhase @0x8270E7C8 -> 0x3B
//   Vehicle::SetCurrentManoeuvre   @0x8270E648 -> 0x60
//   Vehicle::GetTargetPos          @0x8270E200 -> 0x70
//   VehicleAxles::SetFromVehicleTransform @0x82756738 -> 0/0x10/0x20/0x30 within the pair
// sizeof: the Feb-2007 CheckClassSize<> guards pin Axle at 32 and VehicleAxles at 64;
// Vehicle at 128 comes from TrafficEntityModule::Construct's per-type walk stride.
void Axle::_AssertLayout()
{
    static_assert(offsetof(Axle, mPosAndWheelRadius) == 0x00, "Axle::mPosAndWheelRadius");
    static_assert(offsetof(Axle, mUpAndDebug) == 0x10, "Axle::mUpAndDebug");
    static_assert(sizeof(Axle) == 32, "sizeof(Axle)");
}

void VehicleAxles::_AssertLayout()
{
    static_assert(offsetof(VehicleAxles, mFrontAxle) == 0x00, "VehicleAxles::mFrontAxle");
    static_assert(offsetof(VehicleAxles, mBackAxle) == 0x20, "VehicleAxles::mBackAxle");
    static_assert(sizeof(VehicleAxles) == 64, "sizeof(VehicleAxles)");
}

void Vehicle::_AssertLayout()
{
    static_assert(offsetof(Vehicle, muVehicleType) == 0x00, "muVehicleType");
    static_assert(offsetof(Vehicle, muCrashTrafficType) == 0x01, "muCrashTrafficType");
    static_assert(offsetof(Vehicle, muOtherHalfIndex) == 0x02, "muOtherHalfIndex");
    static_assert(offsetof(Vehicle, muSpecies) == 0x04, "muSpecies");
    static_assert(offsetof(Vehicle, mxFlags) == 0x05, "mxFlags");
    static_assert(offsetof(Vehicle, miPhysicalPartsIndex) == 0x06, "miPhysicalPartsIndex");
    static_assert(offsetof(Vehicle, mxEffectState) == 0x07, "mxEffectState");
    static_assert(offsetof(Vehicle, mfSwerveTime) == 0x08, "mfSwerveTime");
    static_assert(offsetof(Vehicle, muHeadlightWarmth) == 0x0C, "muHeadlightWarmth");
    static_assert(offsetof(Vehicle, muIndicatorBulbWarmth) == 0x0D, "muIndicatorBulbWarmth");
    static_assert(offsetof(Vehicle, muHeadlightFlashPattern) == 0x0E, "muHeadlightFlashPattern");
    static_assert(offsetof(Vehicle, muHeadlightFlashState) == 0x0F, "muHeadlightFlashState");
    static_assert(offsetof(Vehicle, mSpeed_DistAcrossLane_SwerveAmount_W) == 0x10, "mSpeed_...");
    static_assert(offsetof(Vehicle, mPitch_Roll_Steering_WheelRot) == 0x20, "mPitch_...");
    static_assert(offsetof(Vehicle, mfHeadlightTimeToFlash) == 0x30, "mfHeadlightTimeToFlash");
    static_assert(offsetof(Vehicle, mfIndicatorTimeToFlash) == 0x34, "mfIndicatorTimeToFlash");
    static_assert(offsetof(Vehicle, miBrakelightState) == 0x38, "miBrakelightState");
    static_assert(offsetof(Vehicle, miPhysicalReason) == 0x39, "miPhysicalReason");
    static_assert(offsetof(Vehicle, miManoeuvre) == 0x3A, "miManoeuvre");
    static_assert(offsetof(Vehicle, miManoeuvrePhase) == 0x3B, "miManoeuvrePhase");
    static_assert(offsetof(Vehicle, mfRandomVal) == 0x3C, "mfRandomVal");
    static_assert(offsetof(Vehicle, mSympCrashTarget) == 0x40, "mSympCrashTarget");
    static_assert(offsetof(Vehicle, mfPhysicalTime) == 0x44, "mfPhysicalTime");
    static_assert(offsetof(Vehicle, meSympCrashState) == 0x48, "meSympCrashState");
    static_assert(offsetof(Vehicle, mfSympCrashTime) == 0x4C, "mfSympCrashTime");
    static_assert(offsetof(Vehicle, mLinearVelocity) == 0x50, "mLinearVelocity");
    static_assert(offsetof(Vehicle, mfManoeuvreTime) == 0x60, "mfManoeuvreTime");
    static_assert(offsetof(Vehicle, mTargetPos) == 0x70, "mTargetPos");
    static_assert(sizeof(Vehicle) == 128, "sizeof(Vehicle)");
}
}
