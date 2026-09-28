#include "GameSource/Director/Camera/Utils/BrnVehicleCollisionPredictor.h"

#include <cmath>     // std::fma, std::sqrt
#include <cstring>   // std::memcpy (lane bits)

#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h"                               // AllVehicleData
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficDirectorInterfaces.h"   // TrafficDirectorEntity

// ============================================================================
// BrnDirector::Camera::Utils::VehicleCollisionPredictor::Update @ 0x822230D8 (BrnVehicleCollisionPredictor.cpp:71;
// 2026-09-28, owner's list, lane L2 CAMCOLLIDE, piece 5 -- "the camera tends to be behind walls or below the map").
//
// VisibilityCollisionPolicy::ProcessSceneQueryResults @0x82224530 calls it every frame (0x822245F8) with the
// director's AllVehicleData (shared info +0x1C), the E_WORLD_NO_SLOMO step splatted (+0x64), the camera's position
// (camera +0x30) and the policy's velocity (+0x220). For every traffic record it asks whether the camera's line --
// through its position, along its velocity RELATIVE to that vehicle -- meets the vehicle's ellipsoid (the record's
// transform, its half extents as radii): CgsGeometric::Ellipsoid::Set and CgsGeometric::TestInfiniteLineEllipsoid
// (CgsLineTests.cpp, DWARF; no standalone X360 body -- inlined here, their asserts naming CgsLineTests.cpp:51 / 52 /
// 54). A hit sets the flag and stores the time |p - cam| / (v_rel . normalised(p - cam)) -- the LAST hit's, the
// console keeps no minimum. The step (v1) is never read: `vspltw v1, v6, 0` @0x822232B4 overwrites it.
//
// Lane-exact, one console instruction per step (scratch/CRASHPARITY_0922/ROUNDING_RULE.md): vmulfp128 / vsubfp
// round separately (rule 4); vmaddfp / vmaddfp128 / vnmsubfp128 once (rule 3; the classic `vmaddfp D,A,B,C` is
// D = A*C + B, `vmaddfp128 D,A,B` is D = A*B + D, `vnmsubfp128 D,A,B` is D = -(A*B - D)); vmsum3fp128 one rounding
// of the f64 sum, splatted (rule 1, FLAG model); vrefp / vrsqrtefp at their correctly rounded values (rule 5, FLAG
// model), their Newton steps as written. FLAG (rule 6): the VMX denormal flush is not modelled.
// ============================================================================

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{

namespace
{
    // The .rdata words (tools/re/x360rd.py).
    const f32 KF_TEST_EPSILON = 1.1920928955078125e-07f;   // flt_82001770 == 0x34000000 (FLT_EPSILON)

    // Four lanes, as a VMX register holds them.
    struct Lanes
    {
        f32 v[4];
    };

    inline Lanes Make(f32 afX, f32 afY, f32 afZ, f32 afW)
    {
        const Lanes l = { { afX, afY, afZ, afW } };
        return l;
    }
    inline Lanes Load(const Vector3& arV)             { return Make(arV.x, arV.y, arV.z, arV.w); }
    inline Lanes Splat(f32 af)                        { return Make(af, af, af, af); }
    inline Lanes Splat(const Lanes& arA, u32 luLane)  { return Splat(arA.v[luLane]); }

    // vsubfp / vmulfp128: each lane rounded on its own.
    inline Lanes Sub(const Lanes& arA, const Lanes& arB)
    {
        return Make(arA.v[0] - arB.v[0], arA.v[1] - arB.v[1], arA.v[2] - arB.v[2], arA.v[3] - arB.v[3]);
    }
    inline Lanes Mul(const Lanes& arA, const Lanes& arB)
    {
        return Make(arA.v[0] * arB.v[0], arA.v[1] * arB.v[1], arA.v[2] * arB.v[2], arA.v[3] * arB.v[3]);
    }
    // a*b + c, ONE rounding per lane.
    inline Lanes Madd(const Lanes& arA, const Lanes& arB, const Lanes& arC)
    {
        return Make(std::fma(arA.v[0], arB.v[0], arC.v[0]), std::fma(arA.v[1], arB.v[1], arC.v[1]),
                    std::fma(arA.v[2], arB.v[2], arC.v[2]), std::fma(arA.v[3], arB.v[3], arC.v[3]));
    }
    // -(a*b - c): the difference rounded ONCE, then negated (an exact cancellation is -0; a NaN keeps its sign).
    inline f32 Nmsub1(f32 afA, f32 afB, f32 afC)
    {
        const f32 lfDifference = std::fma(afA, afB, -afC);
        return (lfDifference != lfDifference) ? lfDifference : -lfDifference;
    }
    inline Lanes Nmsub(const Lanes& arA, const Lanes& arB, const Lanes& arC)
    {
        return Make(Nmsub1(arA.v[0], arB.v[0], arC.v[0]), Nmsub1(arA.v[1], arB.v[1], arC.v[1]),
                    Nmsub1(arA.v[2], arB.v[2], arC.v[2]), Nmsub1(arA.v[3], arB.v[3], arC.v[3]));
    }
    // vmsum3fp128: the xyz dot as ONE rounding of the exact f64 sum, in every lane.
    inline Lanes Dot3(const Lanes& arA, const Lanes& arB)
    {
        return Splat(static_cast<f32>(static_cast<f64>(arA.v[0]) * arB.v[0] + static_cast<f64>(arA.v[1]) * arB.v[1]
                                      + static_cast<f64>(arA.v[2]) * arB.v[2]));
    }
    // vmrghw / vmrglw: (a0, b0, a1, b1) / (a2, b2, a3, b3).
    inline Lanes MergeHigh(const Lanes& arA, const Lanes& arB) { return Make(arA.v[0], arB.v[0], arA.v[1], arB.v[1]); }
    inline Lanes MergeLow(const Lanes& arA, const Lanes& arB)  { return Make(arA.v[2], arB.v[2], arA.v[3], arB.v[3]); }
    // vrefp / vrsqrtefp -- FLAG (model): the correctly rounded value.
    inline Lanes RecipEstimate(const Lanes& arA)
    {
        return Make(static_cast<f32>(1.0 / static_cast<f64>(arA.v[0])), static_cast<f32>(1.0 / static_cast<f64>(arA.v[1])),
                    static_cast<f32>(1.0 / static_cast<f64>(arA.v[2])), static_cast<f32>(1.0 / static_cast<f64>(arA.v[3])));
    }
    inline f32 RsqrtEstimate1(f32 af) { return static_cast<f32>(1.0 / std::sqrt(static_cast<f64>(af))); }
    inline Lanes RsqrtEstimate(const Lanes& arA)
    {
        return Make(RsqrtEstimate1(arA.v[0]), RsqrtEstimate1(arA.v[1]), RsqrtEstimate1(arA.v[2]),
                    RsqrtEstimate1(arA.v[3]));
    }
    // The sign bit cleared (vandc x, 0x80000000) / flipped (vxor x, 0x80000000).
    inline f32 AbsBits(f32 af)
    {
        u32 lu;
        std::memcpy(&lu, &af, 4);
        lu &= 0x7FFFFFFFu;
        std::memcpy(&af, &lu, 4);
        return af;
    }
    inline f32 NegBits(f32 af)
    {
        u32 lu;
        std::memcpy(&lu, &af, 4);
        lu ^= 0x80000000u;
        std::memcpy(&af, &lu, 4);
        return af;
    }
    // A lane that is not a NaN (`vcmpeqfp. v, v, v` on its splat, CR6 "all").
    inline bool IsNumber(f32 af) { return af == af; }
}

void VehicleCollisionPredictor::Update(const AllVehicleData& lAllVehicleData, VecFloat lTimestep, Vector3 lPosition,
                                       Vector3 lVelocity)
{
    (void)lTimestep;   // v1 is overwritten before any read (0x822232B4)

    // GetTraffic, inlined with its BrnDirectorAllVehicleData.h:96 assert (0x822230FC..0x82223128).
    const Array<BrnTraffic::BrnTrafficIO::TrafficDirectorEntity, 32u>* lpTraffic = lAllVehicleData.GetTraffic();
    CGS_ASSERT(lpTraffic != 0, "mpTrafficVehicleArray != NULL");

    mbHasPredictedCollision = false;                                   // stb r28 (0), 0(r27) @0x82223144

    const Lanes lvPosition = Load(lPosition);                          // v118
    const Lanes lvVelocity = Load(lVelocity);                          // v117
    const Lanes lvZero = Splat(0.0f);                                  // v127 (vspltisw128 0)
    const Lanes lvOne  = Splat(1.0f);                                  // v125 (vcsxwfp128 of splat 1)
    const Lanes lvEpsilon = Splat(KF_TEST_EPSILON);                    // v1 / v13 (lvlx flt_82001770 ; vspltw)
    const Lanes lvIdentity0 = Make(1.0f, 0.0f, 0.0f, 0.0f);            // gIVector       (lvx128 v10, r23)
    const Lanes lvIdentity1 = Make(0.0f, 1.0f, 0.0f, 0.0f);            // unk_82181510   (lvx128 v9, r24)
    const Lanes lvIdentity2 = Make(0.0f, 0.0f, 1.0f, 0.0f);            // unk_82181520   (lvx128 v8, r25)

    for (u32 luLoop = 0; luLoop < lpTraffic->GetLength(); ++luLoop)   // 0x822231C0..0x822231EC ; 0x822237E8
    {
        const BrnTraffic::BrnTrafficIO::TrafficDirectorEntity& lrEntity = (*lpTraffic)[luLoop];   // bl 0x821FB940

        const Lanes lvVelocityToVehicle = Sub(lvVelocity, Load(lrEntity.mVelocity));   // vsubfp128 v120, v117, v0
        const Lanes lvHalfExtents = Load(lrEntity.mHalfExtents);                        // v125 (+0x50)
        const Lanes lvRow0 = Load(lrEntity.mLocalTransform.xAxis);                      // v0   (+0x00)
        const Lanes lvRow1 = Load(lrEntity.mLocalTransform.yAxis);                      // v126 (+0x10)
        const Lanes lvRow2 = Load(lrEntity.mLocalTransform.zAxis);                      // v13  (+0x20)
        const Lanes lvRow3 = Load(lrEntity.mLocalTransform.wAxis);                      // v121 (+0x30)

        // ---- Ellipsoid::Set: IsOrthogonal3x3(transform) (CgsLineTests.cpp:51; 0x82223248..0x82223320) ----
        // The columns (vmrghw / vmrglw), each row pushed through them, minus the identity row, squared norms
        // d0 / d1 / d2; the assert fires when |d0^2 + d1^2 + d2^2| > FLT_EPSILON.
        const Lanes lvLow02  = MergeLow(lvRow0, lvRow2);               // v123 (r0.z, r2.z, r0.w, r2.w)
        const Lanes lvHigh02 = MergeHigh(lvRow0, lvRow2);              // v124 (r0.x, r2.x, r0.y, r2.y)
        {
            const Lanes lvLow11  = MergeLow(lvRow1, lvRow1);           // v12  (r1.z, r1.z, r1.w, r1.w)
            const Lanes lvHigh11 = MergeHigh(lvRow1, lvRow1);          // v11  (r1.x, r1.x, r1.y, r1.y)
            const Lanes lvColumn2 = MergeHigh(lvLow02, lvLow11);       // v0   (r0.z, r1.z, r2.z, r1.z)
            const Lanes lvColumn0 = MergeHigh(lvHigh02, lvHigh11);     // v12  (r0.x, r1.x, r2.x, r1.x)
            const Lanes lvColumn1 = MergeLow(lvHigh02, lvHigh11);      // v11  (r0.y, r1.y, r2.y, r1.y)

            Lanes lvM0 = Mul(lvColumn0, Splat(lvRow0, 0));             // vmulfp128 v7, v12, v7
            Lanes lvM1 = Mul(lvColumn0, Splat(lvRow1, 0));             // vmulfp128 v4, v12, v4
            lvM0 = Madd(lvColumn1, Splat(lvRow0, 1), lvM0);            // vmaddfp v7, v11, v7, v6
            lvM1 = Madd(lvColumn1, Splat(lvRow1, 1), lvM1);            // vmaddfp v4, v11, v4, v6
            lvM0 = Madd(lvColumn2, Splat(lvRow0, 2), lvM0);            // vmaddfp v7, v0, v7, v5
            Lanes lvM2 = Mul(lvColumn0, Splat(lvRow2, 0));             // vmulfp128 v5, v12, v5
            lvM1 = Madd(lvColumn2, Splat(lvRow1, 2), lvM1);            // vmaddfp v12, v0, v4, v12
            lvM2 = Madd(lvColumn1, Splat(lvRow2, 1), lvM2);            // vmaddfp v11, v11, v5, v3
            lvM2 = Madd(lvColumn2, Splat(lvRow2, 2), lvM2);            // vmaddfp v11, v0, v11, v2
            const Lanes lvD0 = Dot3(Sub(lvM0, lvIdentity0), Sub(lvM0, lvIdentity0));   // vsubfp v13 ; vmsum3fp128 v10
            const Lanes lvD1 = Dot3(Sub(lvM1, lvIdentity1), Sub(lvM1, lvIdentity1));   // vsubfp v0 ; vmsum3fp128 v0
            const Lanes lvD2 = Dot3(Sub(lvM2, lvIdentity2), Sub(lvM2, lvIdentity2));   // vsubfp v13 ; vmsum3fp128 v13
            const Lanes lvDs = Make(lvD0.v[0], lvD1.v[0], lvD2.v[0], lvD0.v[0]);        // vperm (unk_82CDA350) ; vrlimi128 2
            const f32 lfError = AbsBits(Dot3(lvDs, lvDs).v[0]);                         // vmsum3fp128 ; vandc
            CGS_ASSERT(!(lfError > lvEpsilon.v[0]), "IsOrthogonal3x3(lEllipsoid.GetTransform())");   // :51
        }

        // ---- !IsZero(MagnitudeSquared(radii)) (CgsLineTests.cpp:52; 0x8222333C..0x82223398) ----
        {
            const f32 lfRadiiSq = AbsBits(Dot3(lvHalfExtents, lvHalfExtents).v[0]);      // vmsum3fp128 ; vandc
            CGS_ASSERT(lfRadiiSq > lvEpsilon.v[0], "!IsZero(MagnitudeSquared(lEllipsoid.GetRadii()))");   // :52
        }

        // ---- the ellipsoid-to-world inverse, scaled by 1 / radii (0x8222339C..0x822234C0) ----
        // 1 / radii: vrefp128 and three Newton steps (vnmsubfp128 r = 1 - h*est ; vmaddfp est = est*r + est).
        const Lanes lvEstimate0 = RecipEstimate(lvHalfExtents);                           // vrefp128 v0, v125
        const Lanes lvEstimate1 = Madd(lvEstimate0, Nmsub(lvHalfExtents, lvEstimate0, lvOne), lvEstimate0);
        const Lanes lvEstimate2 = Madd(lvEstimate1, Nmsub(lvHalfExtents, lvEstimate1, lvOne), lvEstimate1);
        const Lanes lvInverseRadii = Madd(lvEstimate2, Nmsub(lvHalfExtents, lvEstimate2, lvOne), lvEstimate2);
        const Lanes lvInverseX = Make(lvInverseRadii.v[0], 0.0f, 0.0f, 0.0f);            // vrlimi128 v6, v11, 8
        const Lanes lvInverseY = Make(0.0f, lvInverseRadii.v[1], 0.0f, 0.0f);            // vrlimi128 v5, v11, 4
        const Lanes lvInverseZ = Make(0.0f, 0.0f, lvInverseRadii.v[2], 0.0f);            // vrlimi128 v4, v11, 2

        // The transposed rotation, w lanes zero (vmrghw128 / vmrglw128 against v127 = 0).
        const Lanes lvLow1Zero  = MergeLow(lvRow1, lvZero);            // v13 (r1.z, 0, r1.w, 0)
        const Lanes lvHigh1Zero = MergeHigh(lvRow1, lvZero);           // v12 (r1.x, 0, r1.y, 0)
        const Lanes lvTransposed2 = MergeHigh(lvLow02, lvLow1Zero);    // v0  (r0.z, r1.z, r2.z, 0)
        const Lanes lvTransposed0 = MergeHigh(lvHigh02, lvHigh1Zero);  // v13 (r0.x, r1.x, r2.x, 0)
        const Lanes lvTransposed1 = MergeLow(lvHigh02, lvHigh1Zero);   // v12 (r0.y, r1.y, r2.y, 0)
        const Lanes lvNegatedPosition = Sub(lvZero, lvRow3);           // vsubfp128 v8, v127, v121

        // Rows 0..2: column i of the rotation scaled lane-wise by 1 / radii.
        Lanes lvInverse0 = Mul(Splat(lvTransposed0, 0), lvInverseX);                // vmulfp128 v5, v5, v11
        Lanes lvInverse2 = Mul(Splat(lvTransposed2, 0), lvInverseX);                // vmulfp128 v1, v1, v11
        Lanes lvInverse1 = Mul(Splat(lvTransposed1, 0), lvInverseX);                // vmulfp128 v2, v2, v11
        lvInverse0 = Madd(Splat(lvTransposed0, 1), lvInverseY, lvInverse0);         // vmaddfp v5, v4, v5, v10
        lvInverse2 = Madd(Splat(lvTransposed2, 1), lvInverseY, lvInverse2);         // vmaddfp v1, v30, v1, v10
        Lanes lvTranslation = Mul(Splat(lvNegatedPosition, 2), lvTransposed2);      // vmulfp128 v7, v7, v0
        lvTranslation = Madd(Splat(lvNegatedPosition, 1), lvTransposed1, lvTranslation);   // vmaddfp v12, v6, v7, v12
        lvInverse1 = Madd(Splat(lvTransposed1, 1), lvInverseY, lvInverse1);         // vmaddfp v2, v31, v2, v10
        lvInverse0 = Madd(Splat(lvTransposed0, 2), lvInverseZ, lvInverse0);         // vmaddfp128 v5, v3, v9, v5
        lvInverse2 = Madd(Splat(lvTransposed2, 2), lvInverseZ, lvInverse2);         // vmaddfp128 v1, v0, v9, v1
        lvInverse1 = Madd(Splat(lvTransposed1, 2), lvInverseZ, lvInverse1);         // vmaddfp128 v2, v4, v9, v2
        lvTranslation = Madd(Splat(lvNegatedPosition, 0), lvTransposed0, lvTranslation);   // vmaddfp v0, v8, v12, v13
        // Row 3: the rotated, negated translation scaled lane-wise by 1 / radii.
        Lanes lvInverse3 = Madd(Splat(lvTranslation, 0), lvInverseX, lvZero);       // vmaddfp128 v8, v13, v11, v8
        lvInverse3 = Madd(Splat(lvTranslation, 1), lvInverseY, lvInverse3);         // vmaddfp v13, v12, v8, v10
        lvInverse3 = Madd(Splat(lvTranslation, 2), lvInverseZ, lvInverse3);         // vmaddfp128 v13, v0, v9, v13

        // IsValid(lInverseEllipsoidToWorldScaled) (CgsLineTests.cpp:54; 0x82223494..0x8222366C): no NaN in the
        // xyz lanes of the four rows.
        {
            const Lanes* lapRows[4] = { &lvInverse0, &lvInverse1, &lvInverse2, &lvInverse3 };
            bool lbValid = true;
            for (u32 luRow = 0; luRow < 4u; ++luRow)
            {
                const bool lbRowValid = IsNumber(lapRows[luRow]->v[0]) && IsNumber(lapRows[luRow]->v[1])
                                     && IsNumber(lapRows[luRow]->v[2]);
                lbValid = lbValid && lbRowValid;
            }
            CGS_ASSERT(lbValid, "IsValid(lInverseEllipsoidToWorldScaled)");          // :54
        }

        // ---- TestInfiniteLineEllipsoid: the line in the unit sphere's space (0x82223670..0x82223734) ----
        Lanes lvDirection = Mul(lvInverse0, Splat(lvVelocityToVehicle, 0));          // vmulfp128 v0, v126, v0
        Lanes lvStart = Madd(lvInverse0, Splat(lvPosition, 0), lvInverse3);          // vmaddfp128 v121, v126, v11
        lvDirection = Madd(lvInverse1, Splat(lvVelocityToVehicle, 1), lvDirection);  // vmaddfp128 v0, v124, v13
        lvStart = Madd(lvInverse1, Splat(lvPosition, 1), lvStart);                   // vmaddfp128 v121, v124, v11
        lvDirection = Madd(lvInverse2, Splat(lvVelocityToVehicle, 2), lvDirection);  // vmaddfp128 v0, v123, v12
        lvStart = Madd(lvInverse2, Splat(lvPosition, 2), lvStart);                   // vmaddfp128 v13, v123, v10
        const Lanes lvDirectionSq = Dot3(lvDirection, lvDirection);                  // vmsum3fp128 v12, v0, v0
        const Lanes lvNegatedStart = Make(NegBits(lvStart.v[0]), NegBits(lvStart.v[1]), NegBits(lvStart.v[2]),
                                          NegBits(lvStart.v[3]));                    // vxor v11, v13, 0x80000000
        const Lanes lvStartSq = Dot3(lvStart, lvStart);                              // vmsum3fp128 v10, v13, v13
        const Lanes lvApproach = Dot3(lvNegatedStart, lvDirection);                  // vmsum3fp128 v11, v11, v0
        const bool lbOutside = lvStartSq.v[0] > lvOne.v[0];                          // vcmpgtfp128 v10, v10, v125
        const Lanes lvScaled = Mul(lvDirection, lvApproach);                         // vmulfp128 v6, v0, v11
        // 1 / |d|^2: vrefp and three Newton steps.
        const Lanes lvRecip0 = RecipEstimate(lvDirectionSq);                         // vrefp v12, v12
        const Lanes lvRecip1 = Madd(lvRecip0, Nmsub(lvDirectionSq, lvRecip0, lvOne), lvRecip0);   // 0x822236DC / E4
        const Lanes lvRecip2 = Madd(lvRecip1, Nmsub(lvDirectionSq, lvRecip1, lvOne), lvRecip1);   // 0x822236EC / F0
        const Lanes lvRecip3 = Madd(lvRecip2, Nmsub(lvDirectionSq, lvRecip2, lvOne), lvRecip2);   // 0x822236FC / 704
        const Lanes lvClosest = Madd(lvScaled, lvRecip3, lvStart);                   // vmaddfp v0, v6, v13, v0
        const bool lbApproaching = lvApproach.v[0] > lvZero.v[0];                    // vcmpgtfp128 v13, v11, v127
        const bool lbNear = !(Dot3(lvClosest, lvClosest).v[0] > lvOne.v[0]);         // vmsum3fp128 ; vcmpgtfp128 ; vnot
        const bool lbIntersection = (lbApproaching && lbNear) || !lbOutside;         // vand ; vor (vnot v10)
        if (!lbIntersection)                                                         // vcmpeqfp128. ; bne @0x82223734
        {
            continue;
        }

        // ---- the hit (0x82223738..0x822237E4) ----
        mbHasPredictedCollision = true;                                              // stb r16 (1) @0x82223760
        const Lanes lvPositionToVehicle = Sub(Load(lrEntity.mLocalTransform.wAxis), lvPosition);   // vsubfp128 v12
        // NormalizeReturnMagnitude: vrsqrtefp and two steps (sq = est*est, half = est*0.5, r = 1 - x*sq,
        // est = half*r + est); the magnitude x * rsqrt, 0 when x is 0 (vcmpeqfp128 ; vsel).
        const Lanes lvLengthSq = Dot3(lvPositionToVehicle, lvPositionToVehicle);     // vmsum3fp128 v0, v12, v12
        const Lanes lvHalf = Splat(0.5f);                                            // vcsxwfp128 v11, v119, 1
        const Lanes lvRsqrt0 = RsqrtEstimate(lvLengthSq);                            // vrsqrtefp v13, v0
        const Lanes lvRsqrt1 = Madd(Mul(lvRsqrt0, lvHalf), Nmsub(lvLengthSq, Mul(lvRsqrt0, lvRsqrt0), lvOne), lvRsqrt0);
        const Lanes lvRsqrt2 = Madd(Mul(lvRsqrt1, lvHalf), Nmsub(lvLengthSq, Mul(lvRsqrt1, lvRsqrt1), lvOne), lvRsqrt1);
        const Lanes lvPositionToVehicleNormalised = Mul(lvPositionToVehicle, lvRsqrt2);   // vmulfp128 v12, v12, v13
        Lanes lvDistance = Mul(lvLengthSq, lvRsqrt2);                                // vmulfp128 v13, v0, v13
        if (lvLengthSq.v[0] == 0.0f)                                                 // vcmpeqfp128 v7 ; vsel
        {
            lvDistance = lvZero;
        }
        // lDistance / Dot(lVelocityToVehicle, normalised): vrefp and two steps, times the distance.
        const Lanes lvClosing = Dot3(lvVelocityToVehicle, lvPositionToVehicleNormalised);   // vmsum3fp128 v0, v120, v12
        const Lanes lvClosingRecip0 = RecipEstimate(lvClosing);                      // vrefp v0, v0
        const Lanes lvClosingRecip1 = Madd(lvClosingRecip0, Nmsub(lvClosingRecip0, lvClosing, lvOne), lvClosingRecip0);
        const Lanes lvClosingRecip2 = Madd(lvClosingRecip1, Nmsub(lvClosingRecip1, lvClosing, lvOne), lvClosingRecip1);
        mSoonestPredictedCollision.mfTimeUntilCollision = Mul(lvClosingRecip2, lvDistance).v[0];   // stfs 4(r11) @0x822237E4
    }
}

}
}
}
