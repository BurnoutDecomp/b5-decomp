#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnIKDrivenPoint.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                  // Subtract/Dot/Normalize/MagnitudeSquared/IsZero

// BrnPhysics::Deformation::IKDrivenPoint::Construct @ 0x82615468
// BrnPhysics::Deformation::IKDrivenPoint::Update    @ 0x825E7608
//
// The X360 solves the constraint in VMX registers. The reconstruction uses native vector
// storage; existing Normalize and scalar checks retain their numerical limitations.

namespace BrnPhysics
{
namespace Deformation
{

// stru_8208F620 lane 0 (image-read 2026-08-14: {FLT_EPSILON, 0.001, 1e-5, 1e-5}) -- the
// tolerance the Construct rest-delta tripwire compares |magSq - 1| against.
static const f32 KF_UNIT_DELTA_EPSILON = 1.1920928955078125e-07f;

// Constraint solve shared by Construct (seed) and Update (re-solve). Given the live position
// lPosition of the driven point and the current endpoint position lEndpoint, slide the point
// back along the endpoint->point axis so it sits exactly lfDesiredDistance from the endpoint.
//
// ⭐⭐ CORRECTED 2026-09-02 (rest-rows wave) against the raw X360 words of Update @0x825E7608
// (VMX128 operands decoded from the image -- IDA/capstone print the fused ops in field order
// D,A,B,C, so `vmaddfp v13,v13,v5,v9` is v13*v9 + v5). Per endpoint the console runs
//     v    = pos - endpoint                       0x825E7630 vsubfp   (0x825E767C for B)
//     proj = dot3(n, v)                           0x825E7648 vmsum3fp128 v12, v0(n), v13
//     proj = min(proj, 0)                         0x825E764C vminfp v12, v12, v11(0)
//     v    = v - n * proj                         0x825E7650 vmulfp128 / 0x825E7654 vsubfp
//     pos' = endpoint + normalize(v) * distance   0x825E7678 vmaddfp v13, v13(unit), v5(dist), v9(A)
// where n is mDirectionPlusDistanceToB.xyz (`lvx128 v0, r10(this+0x10)` @0x825E7638) -- the unit
// PERPENDICULAR from the A-B axis toward the point's rest position (see Construct). The clamp
// throws away any component of (pos - endpoint) that points AGAINST n, i.e. it forbids the
// solve from carrying the point through the A-B axis onto the far side of the panel. The old
// body had no clamp at all (a driven point could mirror through the axis) -- a dropped term.
// The console's normalize is vrsqrtefp + one Newton step with NO zero guard; the house
// Normalize guards zero (an authored point never coincides with its endpoint).
Vector3 IKDrivenPoint::ResolveConstraint(Vector3 lPosition, Vector3 lEndpoint, VecFloat lvfDesiredDistance)
{
    using rw::math::vpu::Subtract;
    using rw::math::vpu::Normalize;
    using rw::math::vpu::MagnitudeSquared;
    using rw::math::vpu::Dot;

    // v = pos - endpoint, then drop its against-the-rest-side component: v -= n * min(dot(n, v), 0).
    Vector3 lDelta = Subtract(lPosition, lEndpoint);
    const Vector3& lrSide = reinterpret_cast<const Vector3&>(mDirectionPlusDistanceToB);
    f32 lfProj = Dot(lrSide, lDelta);
    lfProj = (lfProj < 0.0f) ? lfProj : 0.0f;
    const VecFloat lvfProj(lfProj);
    lDelta = Vector3(__vsubfp(lDelta.mV, __vmulfp(lrSide.mV, lvfProj.mV)));

    const Vector3 lDirection = Normalize(lDelta);
    return Vector3(__vmaddfp(lDirection.mV, lvfDesiredDistance.mV, lEndpoint.mV));
}

void IKDrivenPoint::Construct(const IKDrivenPointSpec* lpSpec, const TagPoint* lpTagPointArrayBase)
{
    using rw::math::vpu::Subtract;
    using rw::math::vpu::Normalize;
    using rw::math::vpu::MagnitudeSquared;
    using rw::math::vpu::Dot;

    // Cache the spec and resolve the two endpoint tag points from the array base by the spec's
    // two indices (console: base + 32*index, i.e. &base[index] at stride sizeof(TagPoint)).
    mpSpec      = lpSpec;
    mpTagPointA = &lpTagPointArrayBase[lpSpec->GetTagPointAIndex()];
    mpTagPointB = &lpTagPointArrayBase[lpSpec->GetTagPointBIndex()];

    // Seed the position lane from the streamed initial position; the w "plus" lane carries the
    // desired distance to endpoint A.
    const Vector3& lInitialPos       = lpSpec->GetInitialPos();
    mPositionPlusDistanceToA.SetVector(lInitialPos.GetVector());
    mPositionPlusDistanceToA.SetPlus(lpSpec->GetDesiredDistanceFromTagPointA());

    // Constraint direction: raw A->B between the two endpoints' rest positions, then normalised;
    // the w "plus" lane carries the desired distance to endpoint B.
    const Vector3 lRawDeltaToB = Subtract(mpTagPointB->GetInitialPosition(),
                                          mpTagPointA->GetInitialPosition());

    // Non-gating tripwire, ⭐ CORRECTED 2026-08-14 (deformation-mount wave) against the asm
    // (0x82615540..0x82615564): the console tests the RAW delta BEFORE normalising --
    //   vsubfp; vmsum3fp128 magSq; vsubfp magSq-1; vandc |.|; vcmpgtfp vs stru_8208F620 lane 0
    //   (image-read: FLT_EPSILON 1.1920929e-07); FireAssert when NOT greater
    // i.e. it demands the streamed rest positions are NOT accidentally a pre-normalised unit
    // pair (a data-sanity check on GetInitialPosition), and with authored positions it never
    // fires. The previous transcription applied it to the NORMALISED direction -- unit by
    // construction, so the "tripwire" fired for every well-formed driven point (93 per create,
    // boot-measured 04:56 run). This TU had never executed before the deformation-manager mount;
    // the gate was stale, not dead.
    CGS_ASSERT(((MagnitudeSquared(lRawDeltaToB) - 1.0f) > KF_UNIT_DELTA_EPSILON) ||
               ((MagnitudeSquared(lRawDeltaToB) - 1.0f) < -KF_UNIT_DELTA_EPSILON),
               "!IsZero( RwMathVPU::MagnitudeSquared( lvDirectionToB ) - RwMathVPU::GetVecFloat_One() )");

    const Vector3 lDirectionToB = Normalize(lRawDeltaToB);

    // ⭐⭐ CORRECTED 2026-09-02 (rest-rows wave): the stored direction is NOT the A->B axis. The
    // console (0x82615560..0x826155F0, VMX128 words decoded from the image) projects the point's
    // rest position onto the A-B axis and keeps the unit vector from that foot OUT to the point:
    //     t    = dot3(p0 - initA, dirAB)            vmsum3fp128 v10, v8, v0
    //     foot = initA + dirAB * t                  vmaddfp128 v124, v0, v10, v124
    //     n    = p0 - foot                          vsubfp128 v0, v12, v124
    //     if (any lane |n| > FLT_EPSILON) n = normalize(n)   (vandc / vcmpgtfp. vs stru_8208F620
    //                                                          lane 0; cr6 all-false skips the rsqrt)
    //     this+0x10 = { n.xyz, w kept } ; then w = spec+20 (distance to B)
    // It is the panel's "outward" side; Update's per-endpoint clamp uses it to keep the point on
    // the side of the axis it was authored on. The old body stored dirAB itself -- the wrong
    // vector, and one for which the (then missing) clamp would have been meaningless.
    const Vector3 lToPoint = Subtract(lInitialPos, mpTagPointA->GetInitialPosition());
    const f32     lfAlong  = Dot(lToPoint, lDirectionToB);
    const VecFloat lvfAlong(lfAlong);
    Vector3 lSide(__vsubfp(lToPoint.mV, __vmulfp(lDirectionToB.mV, lvfAlong.mV)));
    {
        const f32 lfAx = ( lSide.GetX() < 0.0f ) ? -lSide.GetX() : lSide.GetX();
        const f32 lfAy = ( lSide.GetY() < 0.0f ) ? -lSide.GetY() : lSide.GetY();
        const f32 lfAz = ( lSide.GetZ() < 0.0f ) ? -lSide.GetZ() : lSide.GetZ();
        if ( lfAx > KF_UNIT_DELTA_EPSILON || lfAy > KF_UNIT_DELTA_EPSILON || lfAz > KF_UNIT_DELTA_EPSILON )
        {
            lSide = Normalize(lSide);
        }
    }

    mDirectionPlusDistanceToB.SetVector(lSide.GetVector());
    mDirectionPlusDistanceToB.SetPlus(lpSpec->GetDesiredDistanceFromTagPointB());

    // Solve the constraint into mPositionPlusDistanceToA.
    Update();
}

void IKDrivenPoint::Update()
{
    using rw::math::vpu::Subtract;
    using rw::math::vpu::Normalize;
    using rw::math::vpu::MagnitudeSquared;
    using rw::math::vpu::Dot;

    // Current driven-point position (xyz of mPositionPlusDistanceToA) and the two endpoints.
    Vector3 lPosition(mPositionPlusDistanceToA.GetVector());

    const Vector3& lPosA = mpTagPointA->GetPosition();
    const Vector3& lPosB = mpTagPointB->GetPosition();

    const VecFloat lvfDistanceToA(mPositionPlusDistanceToA.GetPlus());
    const VecFloat lvfDistanceToB(mDirectionPlusDistanceToB.GetPlus());

    // Two-pass solve: pull to the desired distance from A, then from B (the console runs the
    // ResolveConstraint VMX block twice, once per endpoint), and write the corrected position
    // back into mPositionPlusDistanceToA (xyz; w "plus" lane preserved).
    lPosition = ResolveConstraint(lPosition, lPosA, lvfDistanceToA);
    lPosition = ResolveConstraint(lPosition, lPosB, lvfDistanceToB);

    mPositionPlusDistanceToA.SetX(lPosition.GetX());
    mPositionPlusDistanceToA.SetY(lPosition.GetY());
    mPositionPlusDistanceToA.SetZ(lPosition.GetZ());

    // Blended scratch/damage amount: lerp(A.scratch, B.scratch, distA / (distA + distB)).
    // Console (Update tail): f0 = distA/(distA+distB); *(this+0x2C) =
    //   (1.0 - f0) * tagA.mfScratchAmount + tagB.mfScratchAmount * f0.
    const f32 lfDistA = mpSpec->GetDesiredDistanceFromTagPointA();
    const f32 lfDistB = mpSpec->GetDesiredDistanceFromTagPointB();
    const f32 lfT     = lfDistA / (lfDistB + lfDistA);

    mfScratchAmount = (1.0f - lfT) * mpTagPointA->GetScratchAmount()
                    + mpTagPointB->GetScratchAmount() * lfT;
}

}
}
