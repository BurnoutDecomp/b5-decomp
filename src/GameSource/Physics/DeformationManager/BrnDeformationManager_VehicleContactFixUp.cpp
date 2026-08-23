// ============================================================================
// GameSource/Physics/DeformationManager/BrnDeformationManager_VehicleContactFixUp.cpp
//
// BrnPhysics::Deformation::DeformationManager -- the vehicle-vs-vehicle contact
// FIX-UP family (the big-five #1 closure under PhysicsModule::FixUpVehicleContacts):
//     FixUpVehicleContactByInterpolation     @ 0x82604DE0   (389 insns)
//     FixUpVehicleContactWithBoxes           @ 0x825DBCA0   (966 insns)
//     FixUpVehicleContact                    @ 0x825DCBB8   (668 insns)
//     GetInterpolatedContactPointAndNormal   @ 0x82604948   (293 insns)
//     CalculateTangentPoints                 @ 0x825DB418   (545 insns)
//     ProjectSphereContactOntoBox            (no X360 out-of-line copy -- header-inline on
//                                             console, its :771/:810 asserts cite the .h;
//                                             PS3 DecFIGS out-of-line @0x7A5118)
//     ProjectLineOntoPlane                   (inlined on X360; PS3 out-of-line @0x6B80C8)
//
// SLICE TU (home BrnDeformationManager.cpp, still unmounted with ~25 unresolved);
// fold back when the home TU mounts. Same slice pattern as _Construct/_ContactFixups.
//
// SOURCES. Every body is reconstructed against the BURNOUT_X360_ARTIST.XEX asm
// (offsets, constants, assert lines); the PS3 DecFIGS internal build keeps the
// helpers OUT-OF-LINE with full debug names, which is what pins the parameter
// names, the helper-call structure (which X360 inlined) and the local names the
// X360's own streamed asserts corroborate (lVelocityA / lCarBNormal /
// lbRaceCarHitSideOfTraffic / lAToBDist / lCosTheta ...). The de-SIMD'd math is
// written over the vendor rw::math::vpu API (Dot/Normalize/TransformPoint/...),
// the same house style as CameraUtils.cpp / BrnDeformationSensor.cpp.
//
// VMX -> scalar notes (recurring idioms, cited once here):
//   vmsum3fp128 a,b            == Dot(a,b) (3-lane)
//   vrsqrtefp + 2x Newton      == 1/sqrt   (Normalize / Magnitude)
//   vrefp + 2x Newton          == 1/x      (ProjectLineOntoPlane's divide)
//   vandc x, sign-splat        == fabs(x)
//   vrlimi mask 4 with zero    == v.y = 0  (flatten to the horizontal plane)
//   vspltw lane3               == a sphere's .w radius broadcast
//   per-lane self-compare      == RwMathVPU::IsValid (NaN tripwire)
// ============================================================================

#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"

#include <cstdlib>   // getenv -- the opt-in [T4-boxfix] bring-up probe only

#include <cmath>   // std::acos / std::sin / std::cos / std::fabs -- stand-ins for the console's
                   // XMVectorACos call + inlined XM sin/cos minimax polynomials, per the
                   // CameraUtils.cpp precedent ("std::acos stands in for the external
                   // XMVectorACos call").

#include "rw/math/vpu/vector3_operation.h"                                                // Dot / Normalize / Negate / operator+-
#include "rw/math/vpu/matrix44affine_operation.h"                                         // TransformPoint / TransformVector / InverseOfMatrixWithOrthonormal3x3

#include "GameShared/GameClasses/Core/CgsAssert.h"                                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                                // gpDebugPrint -- the opt-in [T4-boxfix] probe only
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"                 // CgsDev::PerfMonCpu::Start/StopMonitor
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"                        // CgsGeometric::Sphere
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"             // PotentialContact
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h" // DeformableObject
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformationSensor.h" // DeformationSensor (mpSpec / GetLocalSpaceSphere)
#include "SharedClasses/Physics/Deformation/BrnSensorSpec.h"                              // SensorSpec (mau8NextBoundarySensor)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"              // VehiclePhysics (GetTransform/GetLinearVelocity)

namespace BrnPhysics
{
namespace Deformation
{
    namespace
    {
        // gJVector @0x82181510 == (0, 1, 0, 0) -- the world up axis the roll gates dot against.
        const Vector3 KV_AXIS_Y = { 0.0f, 1.0f, 0.0f, 0.0f };

        // rw::math::fpu::EPSILON, lane 0 of stru_8208F620 == FLT_EPSILON (1.1920929e-07f).
        const f32 KF_RW_EPSILON = 1.1920928955078125e-07f;

        // ✅ RECOVERED 2026-08-23 (was FLAGged 0.0f). The console keeps this as a dynamically-
        // initialised `const VecFloat BrnPhysics::Deformation::KVF_PROJECTSPHERE_RADIUS_PADDING`
        // (DWARF BrnDeformationManager.h:12, X360 .data @0x82FB9660), so the raw image bytes read
        // 0.0f -- the PRE-INIT value. Its static initialiser IS in the image, at 0x82C5B7E0:
        //     lis  r11, flt_820047C8@ha / lfs f0, flt_820047C8@l(r11) / stfs f0, -0x10(r1)
        //     lis  r11, unk_82FB9660@ha / lvlx v0 / addi r11, r11, unk_82FB9660@l
        //     vspltw v0, v0, 0          / stvx128 v0, r0, r11
        // and flt_820047C8 == 0x3D4CCCCD == 0.05f. (The earlier lis/addi scan missed it because
        // 0x9660 has the high bit set, so the pair is emitted as lis 0x82FC / addi -0x69A0.)
        //
        // It MATTERS: the out-normal band below is `radius - padding`, and the moved contact point
        // sits EXACTLY at `radius` along the dominant box axis by construction. With padding 0 the
        // dominant axis fails `|axisDot| > radius` on a coin-flip of FP rounding, the axis sum comes
        // out {0,0,0}, and Normalize(zero) returns zero -- a NON-UNIT contact normal that trips
        // :1568/:1580 here, BrnDeformationSensor_ValidateAndAddContact's |normal|==1 gate and the
        // bridge's "Un-normalised race car-traffic car contact". 0.05 m of slack is what makes the
        // face the point was projected onto always count.
        const f32 KF_PROJECTSPHERE_RADIUS_PADDING = 0.05f;   // flt_820047C8, via init @0x82C5B7E0

        inline f32 Sign(f32 lfValue)   // vcmpgtfp/vcmpgefp + vsel chain: >0 -> +1, ==0 -> 0, <0 -> -1
        {
            return lfValue > 0.0f ? 1.0f : (lfValue >= 0.0f ? 0.0f : -1.0f);
        }

        // The deformation sensor's local-space sphere as {centre.xyz, radius.w}. The sensor
        // holds a forward-declared BrnPhysics::Deformation::Sphere*; its leading 16 bytes are
        // the same {xyz centre, w radius} vector CgsGeometric::Sphere carries (the
        // BrnDeformableObject_BBox.cpp SphereVec precedent).
        inline const Vector4& LocalSphereVec(const DeformationSensor& lrSensor)
        {
            return *reinterpret_cast<const Vector4*>(lrSensor.GetLocalSpaceSphere());
        }

        inline Vector3 SphereCentre(const Vector4& lrPosRadius)
        {
            return Vector3{ lrPosRadius.x, lrPosRadius.y, lrPosRadius.z, 0.0f };
        }

        inline bool IsValid3(const Vector3& lrV)   // per-lane self-compare NaN tripwire
        {
            return lrV.x == lrV.x && lrV.y == lrV.y && lrV.z == lrV.z;
        }

        // v.y = 0 then Normalize -- the vrlimi(4, zero) + vrsqrtefp idiom both interpolation
        // fallback arms share.
        inline Vector3 NormalizeFlattened(Vector3 lVector)
        {
            lVector.y = 0.0f;
            return rw::math::vpu::Normalize(lVector);
        }
    }

    // ==========================================================================================
    // ProjectLineOntoPlane  (X360: inlined into ByInterpolation's tail; PS3 out-of-line @0x6B80C8)
    //
    //   t = Dot(lPointOnPlane - lPointOnLine, lPlaneNormal) / Dot(lLineDir, lPlaneNormal)
    //   return lPointOnLine + lLineDir * t
    // (the console divide is vrefp + two Newton refines).
    // ==========================================================================================
    Vector3 DeformationManager::ProjectLineOntoPlane(Vector3 lPointOnLine, Vector3 lLineDir,
                                                     Vector3 lPointOnPlane, Vector3 lPlaneNormal)
    {
        const f32 lfDirDot   = rw::math::vpu::Dot(lLineDir, lPlaneNormal);
        const f32 lfPointDot = rw::math::vpu::Dot(lPointOnPlane - lPointOnLine, lPlaneNormal);
        return lPointOnLine + lLineDir * (lfPointDot / lfDirDot);
    }

    // ==========================================================================================
    // ProjectSphereContactOntoBox  (header-inline on console -- its asserts cite
    // BrnDeformationManager.h:771/:810; PS3 out-of-line @0x7A5118 pins the signature)
    //
    // Slide the contact point along lNormal until it sits on the sphere's surface as measured
    // along the box axis most aligned with the normal, then report which box faces the moved
    // point pokes out of as the out normal.
    // ==========================================================================================
    Vector3 DeformationManager::ProjectSphereContactOntoBox(const CgsGeometric::Sphere& lWorldSphere,
                                                            Vector3 lNormal, Vector3 lContactPoint,
                                                            Matrix44Affine lBoxTransform,
                                                            Vector3* lpNormalOut) const
    {
        const Vector3 lSpherePos = SphereCentre(lWorldSphere.mPositionRadius);
        const f32     lfRadius   = lWorldSphere.mPositionRadius.w;

        CGS_ASSERT(rw::math::vpu::Dot(lNormal, lContactPoint - lSpherePos) > 0.0f,
                   "RwMathVPU::Dot( lNormal, lContactPoint - lWorldSphere.GetPosition() ) > RwMathVPU::GetVecFloat_Zero()");   // BrnDeformationManager.h:771

        // lDirectionToResolvePlusDot: the box axis row whose |dot lNormal| is largest, carrying
        // its dot in the "plus" lane (the vperm<0,1,2,7> + two vcmpgtfp/vsel chain).
        Vector3 lDirection = lBoxTransform.xAxis;
        f32     lfDot      = rw::math::vpu::Dot(lBoxTransform.xAxis, lNormal);
        {
            const f32 lfDotY = rw::math::vpu::Dot(lBoxTransform.yAxis, lNormal);
            if (std::fabs(lfDotY) > std::fabs(lfDot))
            {
                lDirection = lBoxTransform.yAxis;
                lfDot      = lfDotY;
            }
            const f32 lfDotZ = rw::math::vpu::Dot(lBoxTransform.zAxis, lNormal);
            if (std::fabs(lfDotZ) > std::fabs(lfDot))
            {
                lDirection = lBoxTransform.zAxis;
                lfDot      = lfDotZ;
            }
        }
        // The console compares |dot| against rw::math::fpu::EPSILON (stru_8208F620 lane 0).
        CGS_ASSERT(!(KF_RW_EPSILON >= std::fabs(lfDot)),
                   "!RwMathVPU::IsZero( lDirectionToResolvePlusDot.GetPlus() )");   // BrnDeformationManager.h:810

        // Move the point along lNormal by |(radius - |distance along the axis|) / dot|.
        const f32 lfDistAlong = std::fabs(rw::math::vpu::Dot(lContactPoint - lSpherePos, lDirection));
        const f32 lfStep      = std::fabs((lfRadius - lfDistAlong) * (1.0f / lfDot));
        const Vector3 lResult = lContactPoint + lNormal * lfStep;

        // Out normal: sum the box axes the moved point sits outside of (|axis dot| beyond
        // radius - KVF_PROJECTSPHERE_RADIUS_PADDING), each signed toward the point's side.
        const Vector3 lFromCentre = lResult - lSpherePos;
        const f32     lfLimit     = lfRadius - KF_PROJECTSPHERE_RADIUS_PADDING;

        Vector3 lSum = { 0.0f, 0.0f, 0.0f, 0.0f };
        const f32 lfAxisDotX = rw::math::vpu::Dot(lFromCentre, lBoxTransform.xAxis);
        if (std::fabs(lfAxisDotX) > lfLimit) lSum = lSum + lBoxTransform.xAxis * Sign(lfAxisDotX);
        const f32 lfAxisDotY = rw::math::vpu::Dot(lFromCentre, lBoxTransform.yAxis);
        if (std::fabs(lfAxisDotY) > lfLimit) lSum = lSum + lBoxTransform.yAxis * Sign(lfAxisDotY);
        const f32 lfAxisDotZ = rw::math::vpu::Dot(lFromCentre, lBoxTransform.zAxis);
        if (std::fabs(lfAxisDotZ) > lfLimit) lSum = lSum + lBoxTransform.zAxis * Sign(lfAxisDotZ);

        *lpNormalOut = rw::math::vpu::Normalize(lSum);
        return lResult;
    }

    // ==========================================================================================
    // CalculateTangentPoints @ 0x825DB418
    //
    // The external tangent line between two spheres, in the horizontal plane: flatten both
    // centres to their common average height, rotate the A->B direction about the world Y axis
    // by theta = acos((rA - rB) / |AToB|) toward lPointOnSide, and emit the two touch points
    // (centre + dir * radius) plus the tangent direction. The console's acos is a real
    // XMVectorACos call and the sin/cos pair an inlined XM minimax polynomial; std:: stands in
    // per the CameraUtils.cpp precedent.
    // ==========================================================================================
    void DeformationManager::CalculateTangentPoints(CgsGeometric::Sphere lSphereAIn,
                                                    CgsGeometric::Sphere lSphereBIn,
                                                    Vector3 lPointOnSide, Vector3* lpResultA,
                                                    Vector3* lpResultB, Vector3* lpOutNormal)
    {
        // Flatten both centres to the average height (vrefp(2.0) reciprocal -> * 0.5).
        const f32 lfAverageY = (lSphereAIn.mPositionRadius.y + lSphereBIn.mPositionRadius.y) * 0.5f;
        const f32 lfRadiusA  = lSphereAIn.mPositionRadius.w;
        const f32 lfRadiusB  = lSphereBIn.mPositionRadius.w;
        const Vector3 lCentreA = { lSphereAIn.mPositionRadius.x, lfAverageY, lSphereAIn.mPositionRadius.z, 0.0f };
        const Vector3 lCentreB = { lSphereBIn.mPositionRadius.x, lfAverageY, lSphereBIn.mPositionRadius.z, 0.0f };

        const Vector3 lAToB     = lCentreB - lCentreA;
        const f32     lfAToBDistSq = rw::math::vpu::Dot(lAToB, lAToB);
        const f32     lfAToBDist   = lfAToBDistSq == 0.0f ? 0.0f : std::sqrt(lfAToBDistSq);   // vsel zero guard
        const Vector3 lAToBDir  = lAToB * (1.0f / lfAToBDist);                                 // vrefp + 2x NR

        // lCosTheta = (rA - rB) / dist, clamped to [-1, 1] (vmaxfp -1 / vminfp +1), then acos.
        f32 lfCosTheta = (lfRadiusA - lfRadiusB) * (1.0f / lfAToBDist);
        if (lfCosTheta < -1.0f) lfCosTheta = -1.0f;
        if (lfCosTheta >  1.0f) lfCosTheta =  1.0f;
        const f32 lfTheta = std::acos(lfCosTheta);   // XMVectorACos @0x825DB4xx (real call)

        // Rotate lAToBDir about the world Y axis by theta; if the rotated direction points away
        // from lPointOnSide (Dot(lPointOnSide - lCentreA, rotated) < 0), rotate by -theta instead
        // (the console re-runs the sin/cos polynomial on the negated angle).
        f32 lfSin = std::sin(lfTheta);
        const f32 lfCos = std::cos(lfTheta);
        Vector3 lRotated = { lAToBDir.x * lfCos + lAToBDir.z * lfSin,
                             lAToBDir.y,
                             lAToBDir.z * lfCos - lAToBDir.x * lfSin, 0.0f };
        if (rw::math::vpu::Dot(lPointOnSide - lCentreA, lRotated) < 0.0f)
        {
            lfSin    = -lfSin;   // sin(-theta); cos unchanged
            lRotated = Vector3{ lAToBDir.x * lfCos + lAToBDir.z * lfSin,
                                lAToBDir.y,
                                lAToBDir.z * lfCos - lAToBDir.x * lfSin, 0.0f };
        }

        // Flatten + normalize the tangent direction, emit the touch points.
        const Vector3 lTangentDir = NormalizeFlattened(lRotated);
        *lpResultA  = lCentreA + lTangentDir * lfRadiusA;
        *lpResultB  = lCentreB + lTangentDir * lfRadiusB;
        *lpOutNormal = lTangentDir;

        // Console streams "Bad normal: " + SphereA/SphereB/PointOnSide/lAToB/lAToBDist/lAToBDir/
        // lCosTheta/lTheta/ResultA/lResultB through gpcMessageBuffer; lowered to CGS_ASSERT with
        // the static prefix per the standing rule.
        CGS_ASSERT(IsValid3(lTangentDir), "Bad normal: ");   // BrnDeformationManager.cpp:1409
    }

    // ==========================================================================================
    // GetInterpolatedContactPointAndNormal @ 0x82604948
    //
    // In the model's local space: pick the sensor's boundary neighbour on the side of the other
    // car; if none faces it, report the plain sphere-surface point (FALSE). If the other car sits
    // closer to the neighbour's sphere than to this one's, keep the incoming point (FALSE).
    // Otherwise interpolate: the external tangent line between the two sensor spheres through
    // the contact point (CalculateTangentPoints), returning its touch point + direction (TRUE).
    // Outputs are transformed back to world space either way.
    // ==========================================================================================
    bool DeformationManager::GetInterpolatedContactPointAndNormal(DeformableObject* lpModel,
                                                                  s32 liSensorIndex,
                                                                  Vector3 lWorldPosIn,
                                                                  Vector3 lOtherSensorPos,
                                                                  Vector3* lpOutWorldPos,
                                                                  Vector3* lpInOutNormal)
    {
        CGS_ASSERT(IsValid3(*lpInOutNormal), "RwMathVPU::IsValid(*lpInOutNormal)");   // :1238

        const Matrix44Affine& lTransform = lpModel->GetVehiclePhysics()->GetTransform();
        const Matrix44Affine  lInverse   = rw::math::vpu::InverseOfMatrixWithOrthonormal3x3(lTransform);
        const Vector3 lLocalPosIn    = rw::math::vpu::TransformPoint(lInverse, lWorldPosIn);
        const Vector3 lLocalOtherPos = rw::math::vpu::TransformPoint(lInverse, lOtherSensorPos);

        const DeformationSensor& lrSensor      = lpModel->GetSensorDebug(liSensorIndex);
        const Vector4&           lLocalSphere  = LocalSphereVec(lrSensor);
        const Vector3            lLocalCentre  = SphereCentre(lLocalSphere);

        // Neighbour pick: boundary neighbour 0, else 1, each accepted only when its sphere lies
        // on the other car's side (Dot(neighbourCentre - centre, localOther - centre) >= 0).
        const DeformationSensor* lpNeighbour = nullptr;
        const u8 lu8Neighbour0 = lrSensor.mpSpec->mau8NextBoundarySensor[0];
        if (static_cast<s32>(lu8Neighbour0) != liSensorIndex)
        {
            const Vector3 lToOther = lLocalOtherPos - lLocalCentre;

            const DeformationSensor& lrCandidate0 = lpModel->GetSensorDebug(lu8Neighbour0);
            if (rw::math::vpu::Dot(SphereCentre(LocalSphereVec(lrCandidate0)) - lLocalCentre, lToOther) >= 0.0f)
            {
                lpNeighbour = &lrCandidate0;
            }
            else
            {
                const u8 lu8Neighbour1 = lrSensor.mpSpec->mau8NextBoundarySensor[1];
                if (static_cast<s32>(lu8Neighbour1) != liSensorIndex)
                {
                    const DeformationSensor& lrCandidate1 = lpModel->GetSensorDebug(lu8Neighbour1);
                    if (rw::math::vpu::Dot(SphereCentre(LocalSphereVec(lrCandidate1)) - lLocalCentre, lToOther) >= 0.0f)
                    {
                        lpNeighbour = &lrCandidate1;
                    }
                }
            }
        }

        Vector3 lLocalPoint;
        Vector3 lLocalNormal;
        bool    lbInterpolated = false;

        if (lpNeighbour == nullptr)
        {
            // No neighbour faces the other car: plain sphere-surface point along the flattened
            // direction from the sensor centre to the incoming point.
            lLocalNormal = NormalizeFlattened(lLocalPosIn - lLocalCentre);
            lLocalPoint  = lLocalCentre + lLocalNormal * lLocalSphere.w;
        }
        else
        {
            const Vector4& lNeighbourSphere = LocalSphereVec(*lpNeighbour);
            const Vector3  lNeighbourCentre = SphereCentre(lNeighbourSphere);

            // Which sphere is the other car closer to (flattened distances to each surface)?
            Vector3 lToThis = lLocalOtherPos - lLocalCentre;      lToThis.y = 0.0f;
            Vector3 lToNbr  = lLocalOtherPos - lNeighbourCentre;  lToNbr.y  = 0.0f;
            const f32 lfDistThisSq = rw::math::vpu::Dot(lToThis, lToThis);
            const f32 lfDistNbrSq  = rw::math::vpu::Dot(lToNbr, lToNbr);
            const f32 lfDistThis   = (lfDistThisSq == 0.0f ? 0.0f : std::sqrt(lfDistThisSq)) - lLocalSphere.w;
            const f32 lfDistNbr    = (lfDistNbrSq  == 0.0f ? 0.0f : std::sqrt(lfDistNbrSq))  - lNeighbourSphere.w;

            if (lfDistThis > lfDistNbr)
            {
                // The other car is closer to the neighbour: keep the incoming point (FALSE arm;
                // the console keeps lLocalPosIn verbatim, `vmr v11, v1`).
                lLocalNormal = NormalizeFlattened(lLocalPosIn - lLocalCentre);
                lLocalPoint  = lLocalPosIn;
            }
            else
            {
                CgsGeometric::Sphere lSphereA;
                lSphereA.mPositionRadius = Vector4{ lLocalCentre.x, lLocalCentre.y, lLocalCentre.z, lLocalSphere.w };
                CgsGeometric::Sphere lSphereB;
                lSphereB.mPositionRadius = Vector4{ lNeighbourCentre.x, lNeighbourCentre.y, lNeighbourCentre.z, lNeighbourSphere.w };

                Vector3 lResultB;   // the neighbour-sphere touch point -- computed, unused (console keeps it stack-only)
                CalculateTangentPoints(lSphereA, lSphereB, lLocalPosIn,
                                       &lLocalPoint, &lResultB, &lLocalNormal);
                lbInterpolated = true;
            }
        }

        *lpOutWorldPos  = rw::math::vpu::TransformPoint(lTransform, lLocalPoint);
        *lpInOutNormal  = rw::math::vpu::TransformVector(lTransform, lLocalNormal);

        CGS_ASSERT(IsValid3(*lpInOutNormal), "RwMathVPU::IsValid(*lpInOutNormal)");   // :1337
        return lbInterpolated;
    }

    // ==========================================================================================
    // FixUpVehicleContactByInterpolation @ 0x82604DE0
    //
    // The racecar-vs-vehicle dispatcher: bail to the box fix-up when either car is rolled over;
    // otherwise try to interpolate each side's contact point/normal across its sensor-sphere
    // chain (only where the contact normal is near-horizontal against that car's up axis and
    // the sensor has distinct boundary neighbours); if neither side even attempts, fall back to
    // the box fix-up. Otherwise re-seat the contact from the deeper side's sphere and project
    // both points back onto the (new-normal) contact plane along the ORIGINAL normal.
    // ==========================================================================================
    void DeformationManager::FixUpVehicleContactByInterpolation(
        CgsSceneManager::SceneManagerIO::PotentialContact& lrContact,
        CgsSceneManager::VolumeInstanceId lRaceCarVolInstId,
        CgsSceneManager::VolumeInstanceId lTrafficVolInstId)
    {
        CgsDev::PerfMonCpu::StartMonitor(miFixUpRaceCarTrafficContact);   // this+76724

        const u32 luOwnerA = static_cast<u32>(lRaceCarVolInstId.muId >> 56) & 0xFFu;
        const u32 luOwnerB = static_cast<u32>(lTrafficVolInstId.muId >> 56) & 0xFFu;
        CGS_ASSERT(luOwnerA == 1u && (luOwnerB == 2u || luOwnerB == 1u),
                   "(lRaceCarVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR && lTrafficVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE)"
                   " || (lRaceCarVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR && lTrafficVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR)");   // :1606

        const Vector3 lNormal = lrContact.mNormal;

        const s32 liRaceCarModelIndex =
            FindModelIndexByEntityID(EntityId{ static_cast<u32>(lRaceCarVolInstId.muId >> 32) });
        const s32 liTrafficModelIndex =
            FindModelIndexByEntityID(EntityId{ static_cast<u32>(lTrafficVolInstId.muId >> 32) });
        CGS_ASSERT(liRaceCarModelIndex != -1, "liRaceCarModelIndex != -1");   // :1631
        CGS_ASSERT(liTrafficModelIndex != -1, "liTrafficModelIndex != -1");   // :1632

        DeformableObject* lpRaceCarModel = &mpaModels[liRaceCarModelIndex];
        DeformableObject* lpTrafficModel = &mpaModels[liTrafficModelIndex];

        const CgsGeometric::Sphere lTrafficSphere =
            lpTrafficModel->GetDeformationSphereFromVolumeInstance(lTrafficVolInstId);
        const CgsGeometric::Sphere lRaceCarSphere =
            lpRaceCarModel->GetDeformationSphereFromVolumeInstance(lRaceCarVolInstId);

        const Matrix44Affine& lRaceCarTransform = lpRaceCarModel->GetVehiclePhysics()->GetTransform();
        const Matrix44Affine& lTrafficTransform = lpTrafficModel->GetVehiclePhysics()->GetTransform();

        // Roll gate: either car's up row dotted with world up (yAxis.y) under 0.5 -> boxes.
        if (rw::math::vpu::Dot(lRaceCarTransform.yAxis, KV_AXIS_Y) < 0.5f ||
            rw::math::vpu::Dot(lTrafficTransform.yAxis, KV_AXIS_Y) < 0.5f)
        {
            FixUpVehicleContactWithBoxes(lrContact, lRaceCarVolInstId, lTrafficVolInstId);
            CgsDev::PerfMonCpu::StopMonitor(miFixUpRaceCarTrafficContact);
            return;
        }

        // Defaults (overwritten by a successful interpolation on each side).
        Vector3 lTrafficPoint  = lrContact.mPointOnB;
        Vector3 lRaceCarPoint  = lrContact.mPointOnA;
        Vector3 lTrafficNormal = lNormal;
        Vector3 lRaceCarNormal = rw::math::vpu::Negate(lNormal);
        bool    lbUseBoxes     = true;

        // ---- traffic side ----------------------------------------------------------------
        const s32 liTrafficSensorIndex =
            static_cast<s32>(static_cast<u8>(lTrafficVolInstId.muId)) - 1;
        if (std::fabs(rw::math::vpu::Dot(lNormal, lTrafficTransform.yAxis)) < 0.5f)
        {
            const DeformationSensor& lrSensor = lpTrafficModel->GetSensorDebug(liTrafficSensorIndex);
            if (!(static_cast<s32>(lrSensor.mpSpec->mau8NextBoundarySensor[0]) == liTrafficSensorIndex &&
                  static_cast<s32>(lrSensor.mpSpec->mau8NextBoundarySensor[1]) == liTrafficSensorIndex))
            {
                lbUseBoxes = false;
                if (!GetInterpolatedContactPointAndNormal(lpTrafficModel, liTrafficSensorIndex,
                                                          lrContact.mPointOnB,
                                                          SphereCentre(lRaceCarSphere.mPositionRadius),
                                                          &lTrafficPoint, &lTrafficNormal))
                {
                    lTrafficNormal = NormalizeFlattened(lTrafficNormal);
                    CGS_ASSERT(IsValid3(lTrafficNormal), "RwMathVPU::IsValid(lTrafficNormal)");   // :1685
                }
            }
        }

        // ---- racecar side ----------------------------------------------------------------
        const s32 liRaceCarSensorIndex =
            static_cast<s32>(static_cast<u8>(lRaceCarVolInstId.muId)) - 1;
        if (std::fabs(rw::math::vpu::Dot(lRaceCarNormal, lRaceCarTransform.yAxis)) < 0.5f)
        {
            const DeformationSensor& lrSensor = lpRaceCarModel->GetSensorDebug(liRaceCarSensorIndex);
            if (!(static_cast<s32>(lrSensor.mpSpec->mau8NextBoundarySensor[0]) == liRaceCarSensorIndex &&
                  static_cast<s32>(lrSensor.mpSpec->mau8NextBoundarySensor[1]) == liRaceCarSensorIndex))
            {
                lbUseBoxes = false;
                if (!GetInterpolatedContactPointAndNormal(lpRaceCarModel, liRaceCarSensorIndex,
                                                          lrContact.mPointOnA,
                                                          SphereCentre(lTrafficSphere.mPositionRadius),
                                                          &lRaceCarPoint, &lRaceCarNormal))
                {
                    lRaceCarNormal = NormalizeFlattened(lRaceCarNormal);
                    CGS_ASSERT(IsValid3(lRaceCarNormal), "RwMathVPU::IsValid(lRaceCarNormal)");   // :1710
                }
            }
        }

        if (lbUseBoxes)
        {
            FixUpVehicleContactWithBoxes(lrContact, lRaceCarVolInstId, lTrafficVolInstId);
            CgsDev::PerfMonCpu::StopMonitor(miFixUpRaceCarTrafficContact);
            return;
        }

        // ---- commit: pick the deeper side, then re-project both points ---------------------
        const Vector3 lRaceCarCentre = SphereCentre(lRaceCarSphere.mPositionRadius);
        const Vector3 lTrafficCentre = SphereCentre(lTrafficSphere.mPositionRadius);
        const f32     lfRaceCarRadius = lRaceCarSphere.mPositionRadius.w;
        const f32     lfTrafficRadius = lTrafficSphere.mPositionRadius.w;

        lrContact.mPointOnB = lTrafficPoint;
        lrContact.mPointOnA = lRaceCarPoint;

        const f32 lfDepthB = rw::math::vpu::Dot(lTrafficPoint - lRaceCarCentre,
                                                rw::math::vpu::Negate(lTrafficNormal)) - lfRaceCarRadius;
        const f32 lfDepthA = rw::math::vpu::Dot(lRaceCarPoint - lTrafficCentre,
                                                rw::math::vpu::Negate(lRaceCarNormal)) - lfTrafficRadius;

        Vector3 lChosenNormal;
        if (lfDepthA > lfDepthB)
        {
            lChosenNormal       = lTrafficNormal;
            lrContact.mNormal   = lTrafficNormal;
            lrContact.mPointOnA = lRaceCarCentre - lTrafficNormal * lfRaceCarRadius;
        }
        else
        {
            lChosenNormal       = rw::math::vpu::Negate(lRaceCarNormal);
            lrContact.mNormal   = lChosenNormal;
            lrContact.mPointOnB = lTrafficCentre + lChosenNormal * lfTrafficRadius;
        }

        // Both points slid onto the new contact plane ALONG THE ORIGINAL normal (the two
        // inlined ProjectLineOntoPlane instances, vrefp + 2x Newton divides).
        lrContact.mPointOnA = ProjectLineOntoPlane(lRaceCarCentre, rw::math::vpu::Negate(lNormal),
                                                   lrContact.mPointOnA, lChosenNormal);
        lrContact.mPointOnB = ProjectLineOntoPlane(lTrafficCentre, lNormal,
                                                   lrContact.mPointOnB, rw::math::vpu::Negate(lChosenNormal));

        CgsDev::PerfMonCpu::StopMonitor(miFixUpRaceCarTrafficContact);
    }

    // ==========================================================================================
    // FixUpVehicleContactWithBoxes @ 0x825DBCA0
    //
    // The box fallback: project both contact points onto their car's box along +-mNormal, then
    // choose the final normal -- if the (projected) racecar point sits INSIDE the traffic box
    // on the side the normal points to (lbRaceCarHitSideOfTraffic), snap the normal to the
    // traffic box's lateral X axis; otherwise keep whichever projected face normal opposes the
    // relative velocity harder.
    // ==========================================================================================
    void DeformationManager::FixUpVehicleContactWithBoxes(
        CgsSceneManager::SceneManagerIO::PotentialContact& lrContact,
        CgsSceneManager::VolumeInstanceId lRaceCarVolInstId,
        CgsSceneManager::VolumeInstanceId lTrafficVolInstId)
    {
        const u32 luOwnerA = static_cast<u32>(lRaceCarVolInstId.muId >> 56) & 0xFFu;
        const u32 luOwnerB = static_cast<u32>(lTrafficVolInstId.muId >> 56) & 0xFFu;
        CGS_ASSERT(luOwnerA == 1u && (luOwnerB == 2u || luOwnerB == 1u),
                   "lRaceCarVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR && (lTrafficVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE || "
                   "lTrafficVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR)");   // :1456

        const s32 liRaceCarModelIndex =
            FindModelIndexByEntityID(EntityId{ static_cast<u32>(lRaceCarVolInstId.muId >> 32) });
        const s32 liTrafficModelIndex =
            FindModelIndexByEntityID(EntityId{ static_cast<u32>(lTrafficVolInstId.muId >> 32) });
        CGS_ASSERT(liRaceCarModelIndex != -1, "liRaceCarModelIndex != -1");   // :1484
        CGS_ASSERT(liTrafficModelIndex != -1, "liTrafficModelIndex != -1");   // :1485

        DeformableObject* lpRaceCarModel = &mpaModels[liRaceCarModelIndex];
        DeformableObject* lpTrafficModel = &mpaModels[liTrafficModelIndex];

        const CgsGeometric::Sphere lTrafficSphere =
            lpTrafficModel->GetDeformationSphereFromVolumeInstance(lTrafficVolInstId);
        const CgsGeometric::Sphere lRaceCarSphere =
            lpRaceCarModel->GetDeformationSphereFromVolumeInstance(lRaceCarVolInstId);

        const Matrix44Affine& lRaceCarTransform = lpRaceCarModel->GetVehiclePhysics()->GetTransform();
        const Matrix44Affine& lTrafficTransform = lpTrafficModel->GetVehiclePhysics()->GetTransform();

        const Vector3 lNormal = lrContact.mNormal;
        Vector3 lRaceCarNormal = { 0.0f, 0.0f, 0.0f, 0.0f };
        Vector3 lTrafficNormal = { 0.0f, 0.0f, 0.0f, 0.0f };

        lrContact.mPointOnA = ProjectSphereContactOntoBox(lRaceCarSphere, rw::math::vpu::Negate(lNormal),
                                                          lrContact.mPointOnA, lRaceCarTransform,
                                                          &lRaceCarNormal);
        lrContact.mPointOnB = ProjectSphereContactOntoBox(lTrafficSphere, lNormal,
                                                          lrContact.mPointOnB, lTrafficTransform,
                                                          &lTrafficNormal);

        // The projected racecar point + the contact normal in TRAFFIC local space, tested
        // against the traffic body's box half extents (vehPhys+0x6A0).
        const Matrix44Affine lTrafficInverse =
            rw::math::vpu::InverseOfMatrixWithOrthonormal3x3(lTrafficTransform);
        const Vector3 lPointOnRaceCarLocal =
            rw::math::vpu::TransformPoint(lTrafficInverse, lrContact.mPointOnA);
        const Vector3 lNormalTrafficLocal =
            rw::math::vpu::TransformVector(lTrafficInverse, lrContact.mNormal);
        const Vector3 lHalfExtents = lpTrafficModel->GetHalfExtents();

        // ⚠️ THE X TEST IS NOT THE Y/Z TEST -- the console's three vcmpgefp. have DIFFERENT operand
        // order and the tree had all three the same way round. Verbatim:
        //   0x825DC508  vcmpgefp. v0, v12(|local.x|), v0(halfExtents.x)   -> |local.x| >= hx
        //   0x825DC538  vcmpgefp. v0, v0(halfExtents.y), v12(|local.y|)   -> hy >= |local.y|
        //   0x825DC568  vcmpgefp. v0, v0(halfExtents.z), v13(|local.z|)   -> hz >= |local.z|
        // which is what the name says: the race car's point is BEYOND the traffic box laterally
        // while still level with it and alongside it -- it hit the SIDE. (Inside-the-box on all
        // three axes would be a containment test, and would snap the normal to +-xAxis for
        // head-on hits too.)
        const bool lbRaceCarHitSideOfTraffic =
            std::fabs(lPointOnRaceCarLocal.x) >= lHalfExtents.x &&
            lHalfExtents.y >= std::fabs(lPointOnRaceCarLocal.y) &&
            lHalfExtents.z >= std::fabs(lPointOnRaceCarLocal.z) &&
            (lPointOnRaceCarLocal.x * lNormalTrafficLocal.x) > 0.0f;

        f32 lfDiagDotA = 0.0f;   // [T4-boxfix] probe only -- the console keeps these inside the else
        f32 lfDiagDotB = 0.0f;
        const char* lpcDiagPick = "SIDE(traffic.xAxis * Sign)";

        if (lbRaceCarHitSideOfTraffic)
        {
            // Snap the normal to the traffic box's lateral axis, signed by which side was hit.
            lrContact.mNormal = lTrafficTransform.xAxis * Sign(lPointOnRaceCarLocal.x);
        }
        else
        {
            const Vector3 lVelocityA = lpRaceCarModel->GetVehiclePhysics()->GetLinearVelocity();
            const Vector3 lVelocityB = lpTrafficModel->GetVehiclePhysics()->GetLinearVelocity();
            const Vector3 lRelativeVelocity = lVelocityB - lVelocityA;

            const f32 lvfRelativeVelocityDotA = std::fabs(rw::math::vpu::Dot(lRelativeVelocity, lRaceCarNormal));
            const f32 lvfRelativeVelocityDotB = std::fabs(rw::math::vpu::Dot(lRelativeVelocity, lTrafficNormal));

            lrContact.mNormal = lvfRelativeVelocityDotB > lvfRelativeVelocityDotA
                                    ? rw::math::vpu::Negate(lRaceCarNormal)
                                    : lTrafficNormal;

            lfDiagDotA  = lvfRelativeVelocityDotA;
            lfDiagDotB  = lvfRelativeVelocityDotB;
            lpcDiagPick = lvfRelativeVelocityDotB > lvfRelativeVelocityDotA
                              ? "VEL(-lRaceCarNormal)" : "VEL(lTrafficNormal)";

            // Console streams mNormal/lNormal/the dots/velocities/lPointOnRaceCarLocal/
            // lRaceCarNormal/lTrafficNormal/lbRaceCarHitSideOfTraffic/lTrafficTransform; lowered
            // per the standing rule.
            CGS_ASSERT(rw::math::vpu::Dot(lrContact.mNormal, lNormal) > 0.0f,
                       "lPotentialContact.mNormal: ");   // :1568
        }

        // ---- [T4-boxfix] bring-up probe -- NOT IN THE X360 BINARY. One shot, opt-in on
        // BRN_TRAFFIC_DIAG. This is the hop that OVERWRITES the queue-[8] normal, so it names
        // whether a non-unit / wrong-signed mNormal was born here: it prints the incoming normal,
        // both projected face normals with their magnitudes (a zero one means the axis sum missed
        // -- the KVF_PROJECTSPHERE_RADIUS_PADDING failure), the branch taken, and the final normal
        // with |normal| and Dot(final, incoming). DELETE-WHEN-STABLE.
        {
            static const bool skbBoxDiag = ( getenv( "BRN_TRAFFIC_DIAG" ) != 0 );
            static bool sbLoggedBox = false;
            if ( skbBoxDiag && !sbLoggedBox && CgsDev::Log::gpDebugPrint != 0 )
            {
                sbLoggedBox = true;
                const Vector3 lFinal = lrContact.mNormal;
                const f32 lfMagA = std::sqrt( rw::math::vpu::Dot( lRaceCarNormal, lRaceCarNormal ) );
                const f32 lfMagB = std::sqrt( rw::math::vpu::Dot( lTrafficNormal, lTrafficNormal ) );
                const f32 lfMagF = std::sqrt( rw::math::vpu::Dot( lFinal, lFinal ) );
                *CgsDev::Log::gpDebugPrint
                    << "[T4-boxfix] FIRST race car-traffic box fix-up:"
                    << " incoming nrm " << lNormal.x << " " << lNormal.y << " " << lNormal.z
                    << " | lRaceCarNormal " << lRaceCarNormal.x << " " << lRaceCarNormal.y << " "
                    << lRaceCarNormal.z << " |A| " << lfMagA
                    << " | lTrafficNormal " << lTrafficNormal.x << " " << lTrafficNormal.y << " "
                    << lTrafficNormal.z << " |B| " << lfMagB
                    << " | localPtOnRaceCar " << lPointOnRaceCarLocal.x << " "
                    << lPointOnRaceCarLocal.y << " " << lPointOnRaceCarLocal.z
                    << " | halfExtents " << lHalfExtents.x << " " << lHalfExtents.y << " "
                    << lHalfExtents.z
                    << " | radii A " << lRaceCarSphere.mPositionRadius.w
                    << " B " << lTrafficSphere.mPositionRadius.w
                    << " | hitSide " << ( lbRaceCarHitSideOfTraffic ? 1 : 0 )
                    << " dotA " << lfDiagDotA << " dotB " << lfDiagDotB
                    << " | PICK " << lpcDiagPick
                    << " | FINAL " << lFinal.x << " " << lFinal.y << " " << lFinal.z
                    << " |n| " << lfMagF
                    << " Dot(final,incoming) " << rw::math::vpu::Dot( lFinal, lNormal )
                    << "\n";
            }
        }

        // Common exit gate -- streams the full local set incl. both spheres and
        // "lpTrafficModel->GetHalfExtents(): "; lowered per the standing rule.
        CGS_ASSERT(rw::math::vpu::Dot(lrContact.mNormal, lNormal) > 0.0f,
                   "\n lPotentialContact.mNormal: ");   // :1580
    }

    // ==========================================================================================
    // FixUpVehicleContact @ 0x825DCBB8
    //
    // The car-vs-car (or traffic-vs-traffic) box fix-up: project both contact points onto their
    // car's box along +-mNormal, then keep whichever projected face normal opposes the relative
    // velocity harder. (The traffic-vs-traffic contacts arrive here from the driver's queue-[0]
    // walk with both ids already rewritten to physics space.)
    // ==========================================================================================
    void DeformationManager::FixUpVehicleContact(
        CgsSceneManager::SceneManagerIO::PotentialContact& lrContact,
        CgsSceneManager::VolumeInstanceId lCarAVolInstId,
        CgsSceneManager::VolumeInstanceId lCarBVolInstId)
    {
        const u32 luOwnerA = static_cast<u32>(lCarAVolInstId.muId >> 56) & 0xFFu;
        const u32 luOwnerB = static_cast<u32>(lCarBVolInstId.muId >> 56) & 0xFFu;
        CGS_ASSERT((luOwnerA == 1u && luOwnerB == 1u) || (luOwnerA == 2u && luOwnerB == 2u),
                   "( lCarAVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR && lCarBVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR ) || "
                   "( lCarAVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE && lCarBVolInstId.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE )");   // :1772

        const s32 liCarAModelIndex =
            FindModelIndexByEntityID(EntityId{ static_cast<u32>(lCarAVolInstId.muId >> 32) });
        const s32 liCarBModelIndex =
            FindModelIndexByEntityID(EntityId{ static_cast<u32>(lCarBVolInstId.muId >> 32) });
        CGS_ASSERT(liCarAModelIndex != -1, "liCarAModelIndex != -1");   // :1798
        CGS_ASSERT(liCarBModelIndex != -1, "liCarBModelIndex != -1");   // :1799

        DeformableObject* lpCarAModel = &mpaModels[liCarAModelIndex];
        DeformableObject* lpCarBModel = &mpaModels[liCarBModelIndex];

        const CgsGeometric::Sphere lCarBSphere =
            lpCarBModel->GetDeformationSphereFromVolumeInstance(lCarBVolInstId);
        const CgsGeometric::Sphere lCarASphere =
            lpCarAModel->GetDeformationSphereFromVolumeInstance(lCarAVolInstId);

        const Matrix44Affine& lCarATransform = lpCarAModel->GetVehiclePhysics()->GetTransform();
        const Matrix44Affine& lCarBTransform = lpCarBModel->GetVehiclePhysics()->GetTransform();

        const Vector3 lNormal = lrContact.mNormal;
        Vector3 lCarANormal = { 0.0f, 0.0f, 0.0f, 0.0f };
        Vector3 lCarBNormal = { 0.0f, 0.0f, 0.0f, 0.0f };

        lrContact.mPointOnA = ProjectSphereContactOntoBox(lCarASphere, rw::math::vpu::Negate(lNormal),
                                                          lrContact.mPointOnA, lCarATransform,
                                                          &lCarANormal);
        lrContact.mPointOnB = ProjectSphereContactOntoBox(lCarBSphere, lNormal,
                                                          lrContact.mPointOnB, lCarBTransform,
                                                          &lCarBNormal);

        const Vector3 lVelocityA = lpCarAModel->GetVehiclePhysics()->GetLinearVelocity();
        const Vector3 lVelocityB = lpCarBModel->GetVehiclePhysics()->GetLinearVelocity();
        const Vector3 lRelativeVelocity = lVelocityB - lVelocityA;

        const f32 lvfRelativeVelocityDotA = std::fabs(rw::math::vpu::Dot(lRelativeVelocity, lCarANormal));
        const f32 lvfRelativeVelocityDotB = std::fabs(rw::math::vpu::Dot(lRelativeVelocity, lCarBNormal));

        lrContact.mNormal = lvfRelativeVelocityDotB > lvfRelativeVelocityDotA
                                ? rw::math::vpu::Negate(lCarANormal)
                                : lCarBNormal;

        // Console streams mNormal/lNormal/the dots/lRelativeVelocity/lVelocityA/lVelocityB/
        // lCarANormal/lCarBNormal/lCarATransform/lCarBTransform; lowered per the standing rule.
        CGS_ASSERT(rw::math::vpu::Dot(lrContact.mNormal, lNormal) > 0.0f,
                   "lPotentialContact.mNormal: ");   // :1848
    }
}
}
