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
// VALUE RECOVERED 2026-08-21 (wave T1 consolidation): X360 .data 0x8300D190 is zero in the
// image because it is seeded by an UNNAMED dynamic-initialiser thunk @0x82C67830 (found via
// xref walk in the .i64 -- export-based scans cannot see these, the wave-Q lesson again):
// lfs f0, flt_820054CC (= 20.0f); vspltw v0,v0,0; stvx128 -> all four lanes 20.0f.
const VecFloat KF_VEHICLE_UPDATE_MATRIX_OLD_UP_FACTOR = { 20.0f, 20.0f, 20.0f, 20.0f };

namespace
{
const f32 KF_AXLE_LINE_TEST_HALF_HEIGHT = 10.0f;
const f32 KF_TRIANGLE_INTERSECT_EPSILON = 1.0e-8f;
const f32 KF_TRIANGLE_INTERSECT_EDGE_EPSILON = 1.0e-5f;
const f32 KF_MIN_CORRECTION_DIST_SQ = 2.5e-5f;

// AUDIT 2026-08-21 (cluster C3): this anonymous namespace used to carry TU-local copies of
// KU_MAX_TRAFFIC(600) / KU_MAX_STANDARD_TRAFFIC(400) / KU_INVALID_VEHICLE(0xFFFF). All three
// now have a real home in BrnTrafficConstants.h (landed by the keystone cluster), so the
// forks are deleted and the file includes that header instead. KU_MAX_TRAFFIC was itself a
// mis-name for the pool-wide bound the X360 asserts against (0x258); its canonical spelling
// is KU_MAX_TOTAL_TRAFFIC. The baked assert STRINGS are left exactly as the XEX spells them.

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

// FLAG PC boot gates (wave T1 consolidation 2026-08-21): three TRAILER-path callees are
// NOT reconstructed -- Axle::ForceIntersectWithLane @0x8275ED98, Vehicle::SetSpeed, and
// Vehicle::CalcTowBarPos (the latter two are ship-only additions: absent from the Feb-2007
// leak AND from the ARTIST per-function exports -- suspected export holes, addresses
// unconfirmed). Their only reach is Vehicle::InitialiseAsTrailer, which cannot run before
// articulated traffic lands (wave 2+). Trap loud rather than fake motion; the real bodies
// replace these gates.
void Axle::ForceIntersectWithLane(const LaneRung& lRung0, const LaneRung& lRung1)
{
    (void)lRung0;
    (void)lRung1;
    CGS_ASSERT(false,
               "Axle::ForceIntersectWithLane @0x8275ED98 not reconstructed [FLAG PC boot gate]");
}

void Vehicle::SetSpeed(VecFloat lfSpeed)
{
    (void)lfSpeed;
    CGS_ASSERT(false,
               "Vehicle::SetSpeed not reconstructed (trailer path) [FLAG PC boot gate]");
}

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

// ---------------------------------------------------------------------------
// VehicleAxles::SetFromVehicleTransform (X360 @0x82756738, a leaf tail-called only by
// Vehicle::InitialiseAsStatic @0x827567F0 -- r3 this, r4 lTransform, r5 lpVehicleTypeRuntime,
// r6 lpVehicleTypeUpdate). The ship factored this out of the Feb-2007 InitialiseAsStatic
// body, which spelled the same six statements inline (leak BrnTrafficVehicle.cpp:407-414).
//
// Asm-attested, store for store:
//   v0  = *(r4+0x20)                        lTransform.At()
//   v13 = *(r4+0x30)                        lTransform.Pos()
//   v10 = splat(*(r5+0x20), lane 3)         VehicleTypeRuntime::GetForwardAxleOffset()
//   v12 = splat(*(r5+0x20), lane 2)         VehicleTypeRuntime::GetBackAxleOffset()
//   front = At*fwd + Pos ; back = At*back + Pos          (two vmaddfp)
//   each stored through `vrlimi128 <new>, <old>, 1, 0`  -- lane 3 (the "plus" lane) is taken
//   from the OLD value, i.e. exactly Vector3Plus::SetVector3 (xyz only).
//   then both plus lanes are overwritten from `lfs 0(r6)` == VehicleTypeUpdateData::
//   mfWheelRadius (offset 0) via the SDK's store/load round-trip == SetPlus().
//   finally both mUpAndDebug get lTransform.Up() with their own lane 3 preserved == SetUp().
// No lane intersection, no physics: this is what makes a parked car cheap.
// ---------------------------------------------------------------------------
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

// ===========================================================================
// Vehicle::InitialiseAsStatic (X360 @0x827567F0) -- THE function that makes a parked
// traffic car exist. No physics body, no lane Param, no plan: it stamps the vehicle
// record alive, copies the authored StaticTrafficVehicle transform straight out to the
// module's transform slot, and seats the two axles off that transform.
//
// SHIP vs Feb-2007 (leak BrnTrafficVehicle.cpp:383). The leak's six-parameter form grew
// three parameters and five stores:
//   + f32 lfRandomVal                 -> mfRandomVal            (stfs 0x3C, from f1)
//   + u32 luVehicle + VehicleSoaData& -> mAliveVehicles.SetBit  (the SoA bit the leak's
//                                        build did not have; asserted !IsBitSet first)
//   + miBrakelightState = 0           (stb 0x38)
//   + miPhysicalReason  = -1          (stb 0x39)
//   + miManoeuvre = E_MANOEUVRE_NONE  (stb 0x3A)
//   + mSympCrashTarget = invalid      (stw 0x40, li -1)
//   + mLinearVelocity = At * speed    (stvx 0x50)
//   and the leak's inline axle block became VehicleAxles::SetFromVehicleTransform.
// Everything the leak DOES have is present and matches byte for byte (mxFlags,
// muSpecies, muVehicleType, the transform copy, mxEffectState, muHeadlightWarmth=0,
// muHeadlightFlashPattern=E_HEADLIGHTFLASH_COUNT(3), muHeadlightFlashState=0xFF,
// the two vector zeroes, mfSwerveTime=0 via flt_82001CC0, muOtherHalfIndex=0xFFFF,
// muCrashTrafficType=0xFF).
//
// Assert order is the asm's (BrnTrafficVehicle.cpp lines 475/476/477/478/479 baked into
// the XEX): lpAxles, lpVehicleTypeRuntime, lpVehicleTypeUpdate, IsValid(lTransform),
// !mAliveVehicles.IsBitSet(luVehicle). The two CgsFastBitArray.h:396/:431 range asserts
// the asm also inlines belong to FastBitArray's own checked accessors, not to this body.
//
// mLinearVelocity: the asm keeps the just-zeroed speed lane in v0 and does
// `vmulfp128 v0, lTransform.At(), splat(speed)` -- i.e. the source multiplies the At axis
// by the (zero) speed exactly as InitialiseAsTrailer does, rather than storing a literal
// zero vector. Written that way so the intent survives, not the constant-folded form.
// ===========================================================================
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

// ===========================================================================
// Vehicle::UpdateMatrix (X360 @0x82757768) -- rebuild a traffic car's world transform
// from its two axle positions and axle up-vectors.
//   r3 this, r4 lpAxles, r5 lOutMatrix, r6 lpVehicleTypeRuntime, v1 lOldUp
// (DWARF BrnTrafficVehicle.h:310 -- the ship grew the `Vector3 lOldUp` fourth parameter the
// Feb-2007 three-parameter form did not have.)
//
// The recovered algorithm, asm line by asm line:
//   lAxleDelta = mFrontAxle.pos - mBackAxle.pos                    (vsubfp -> v125)
//   guard: at least one |component| exceeds an epsilon             (0x8275786C..0x827578A4)
//   lAt  = Normalize(lAxleDelta)                                   (vrsqrtefp + 2 Newton steps)
//   lUp  = Normalize(lOldUp*K + (frontUp + backUp))                (vmaddcfp128 @0x827579B4)
//   lOutMatrix.At()    = lAt                                       (stvx r20+0x20)
//   lOutMatrix.Pos()   = frontAxlePos - lAt*GetForwardAxleOffset() (stvx r20+0x30)
//   lOutMatrix.Right() = Normalize(Cross(lUp, lAt))                (stvx r20+0x00)
//   lOutMatrix.Up()    = Normalize(Cross(lAt, Right))              (stvx r20+0x10)
//   mLinearVelocity    = lAt * GetSpeed()                          (stvx this+0x50)
// Both crosses are the `vpermwi 0x63` (yzx) form: perm(a*perm(b) - perm(a)*b); expanding the
// lanes gives Cross(lUp,lAt) for the first and Cross(lAt,Right) for the second, i.e. the
// standard right-handed Gram-Schmidt for a Right/Up/At row order.
//
// FLAG -- ONE UNREAD DATUM, and it is deliberately NOT guessed.
// `KF_VEHICLE_UPDATE_MATRIX_OLD_UP_FACTOR` (declared in this TU's header) is the 16-byte
// lane register the asm loads from X360 .data 0x8300D190. It is initialised DATA: no
// function in the 30,093-function ARTIST export set references that address other than this
// one read, so there is no dynamic initialiser to decompile, and this repo carries no
// data-segment dump for the XEX (the existing dumps are produced by an IDA headless script
// and idat is not available here). Recovering it is one `idat -A -S dump_rodata.py` run over
// BURNOUT_X360_ARTIST.XEX.i64 for 0x8300D190 len 16, read big-endian. Until then the symbol
// stays DECLARED AND UNDEFINED on purpose: a loud unresolved-external naming the exact
// datum beats a silent placeholder zero, which in this tree has already cost a five-day
// shadow bug (KF_DEFAULT_ASPECTRATIO).
//
// FLAG (assert-only, behaviour-neutral): the X360's "Bad AT vector" guard compares each
// |lAxleDelta| lane against an explicit epsilon at rodata flt_820C0BC0, also unread. The
// guard is a debug assert with no live effect, so it is reproduced with the SDK's own
// default-tolerance IsZero() rather than a made-up constant.
// ===========================================================================
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

    // X360 streams "Bad AT vector in traffic. Vehicle type <t>, front axle = <v>,
    // back axle = <v>"; the stringized condition carries the same guard.
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

    // The asm really does call the checked GetSpeed() accessor here (bl @0x82757E1C) and
    // multiplies the At axis by the broadcast lane.
    mLinearVelocity = lAt * GetSpeed().x;
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
// Vehicle::SetHasEntity  @ 0x8270EB38   ⭐ LANDED 2026-08-21 (wave T1 round 4, item 2)
//
// The scene-entity half of the same pattern as SetPhysical/SetDead above: it flips
// E_FLAG_HASENTITY on the vehicle and the matching mVehiclesWithEntities bit in the shared
// VehicleSoaData, keeping the two in lockstep -- which is what the second assert exists to
// enforce. Its two callers are TrafficEntityModule::CreateNewVehicleEntities @0x8272FA30
// (with true) and TrafficEntityModule::KillDyingVehicleEntity @0x8272EB40 (with false).
//
// STORE MAP, read off the asm (r6 == &lSoaData, r15 == r6 + 0x50):
//   r6  + 0x00  lSoaData.mAliveVehicles          (the IsBitSet in the third assert)
//   r15 + 0x00  lSoaData.mVehiclesWithEntities   (the array actually mutated -- 0x50 == 80
//                                                 == sizeof(FastBitArray<601>), i.e. the
//                                                 SECOND member of VehicleSoaData)
//   this+ 0x05  mxFlags, `ori r11,r11,2` / `andi. r11,r11,0xFD` == E_FLAG_HASENTITY
//
// ⚠️ THE FIRST ASSERT'S POLARITY IS THE OPPOSITE OF WHAT THE ASM LOOKS LIKE. The console
// branches AROUND the FireAssert on `bne` (0x8270EB68), i.e. it fires when
// HasEntity() == lbHasEntity -- so the CONDITION is `HasEntity() != lbHasEntity`, exactly as
// the baked string says. Transcribing the branch instead of the string would invert it and
// turn a correct call into a guaranteed assert.
//
// ⚠️ THE FOUR "Index N is out of range (max bits: 600)" StrStream asserts the asm carries
// (CgsFastBitArray.h:396/:431/:452) are the container's own inlined bounds checks, which this
// tree's FastBitArray deliberately does not reproduce (see that header's policy banner). They
// are not dropped behaviour: the four calls below are the bit ops those checks guard.
// ---------------------------------------------------------------------------
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
        // `andi. r11, r11, 0xFD` -- clears ONLY bit 1; unlike SetDead/SetNotPhysical above
        // this arm touches no other flag.
        mxFlags &= static_cast<u8>(~static_cast<u8>(E_FLAG_HASENTITY));
        lSoaData.mVehiclesWithEntities.UnSetBit(luVehicle);
    }
}

// ---------------------------------------------------------------------------
// Vehicle::GetCabIndex  @ 0x8270E4C8   ⭐ LANDED 2026-08-21 (wave T1 round 4, item 2)
// DWARF BrnTrafficVehicle.h:339. Assert then one `lhz 2(this)`; muOtherHalfIndex is the
// TRAILER's view of the pair (the cab's twin accessor GetTrailerIndex, DWARF :338, reads the
// same member from the cab side -- it is not landed here because nothing in this round calls
// it and its own species assert is the mirror image, i.e. it needs its own attestation).
// ---------------------------------------------------------------------------
u16 Vehicle::GetCabIndex() const
{
    CGS_ASSERT(IsOfTrailerSpecies(), "IsOfTrailerSpecies()");   // BrnTrafficVehicle.h:778
    return muOtherHalfIndex;
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

// ---------------------------------------------------------------------------
// Layout pins. NEVER CALLED -- a static member function so offsetof() reaches Vehicle's
// private members.
//
// Why absolute offsets are legitimate here (they are NOT in most of this tree): Axle,
// VehicleAxles and Vehicle are POINTER-FREE records -- four 16-byte lane registers, some
// bytes, some floats, one 32-bit EntityId and one 32-bit enum. Widening a pointer cannot
// move anything, so the console footprint and the host footprint are identical and every
// offset below is the one the X360 asm stores to. Sources, one per pin group:
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
// The Feb-2007 leak independently pins Axle at 32 and VehicleAxles at 64 via its
// CheckClassSize<> guards; sizeof(Vehicle) == 128 is pinned by TrafficEntityModule::
// Construct's per-type walk stride and by the module's vehicle array in the keystone header.
// ---------------------------------------------------------------------------
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
