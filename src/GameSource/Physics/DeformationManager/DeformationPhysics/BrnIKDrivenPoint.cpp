#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnIKDrivenPoint.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                  // Subtract/Dot/Normalize/MagnitudeSquared/IsZero

// BrnPhysics::Deformation::IKDrivenPoint::Construct @ 0x82615468
// BrnPhysics::Deformation::IKDrivenPoint::Update    @ 0x825E7608
//
// The X360 build solves the two-bone IK constraint with VMX/AltiVec; the bodies below are the
// DE-SIMD'd vector equivalent (no __asm), operating BY MEMBER NAME on the named lanes of the
// committed flat Vector3/Vector3Plus aggregates (same de-modelling contract as BrnTagPoint.cpp
// and rw/math/vpu/vector3_operation.h: the console rsqrt-estimate-plus-refinement reciprocal
// magnitude is de-optimised to an exact 1/std::sqrt via Normalize; broadcast VecFloat dots are
// read as a single scalar lane). Member offsets used by the console asm are 32-bit-pointer
// offsets and are not reproduced byte-for-byte; the host recomputes them from the names.

namespace BrnPhysics
{
namespace Deformation
{

// Constraint solve shared by Construct (seed) and Update (re-solve). Given the live position
// lPosition of the driven point and the current endpoint position lEndpoint, slide the point
// back along the endpoint->point axis so it sits exactly lfDesiredDistance from the endpoint
// (console: vmsum3fp dot to find the along-axis component, then vrsqrtefp-normalize the
// residual and re-extend by the desired distance). Returns the corrected position.
//
// This is the named-member form of the X360 ResolveConstraint @ inlined-in-Update VMX block.
Vector3 IKDrivenPoint::ResolveConstraint(Vector3 lPosition, Vector3 lEndpoint, VecFloat lvfDesiredDistance)
{
    using namespace rw::math::vpu;

    // Direction from the endpoint to the current point; its length is the current distance.
    const Vector3 lDelta            = Subtract(lPosition, lEndpoint);
    const Vector3 lDirection        = Normalize(lDelta);
    const f32     lfDesiredDistance = lvfDesiredDistance.x;   // broadcast VecFloat -> one lane

    // Re-extend from the endpoint along the unit direction by the desired distance.
    return Vector3{ lEndpoint.x + lDirection.x * lfDesiredDistance,
                    lEndpoint.y + lDirection.y * lfDesiredDistance,
                    lEndpoint.z + lDirection.z * lfDesiredDistance,
                    lPosition.w };
}

void IKDrivenPoint::Construct(const IKDrivenPointSpec* lpSpec, const TagPoint* lpTagPointArrayBase)
{
    using namespace rw::math::vpu;

    // Cache the spec and resolve the two endpoint tag points from the array base by the spec's
    // two indices (console: base + 32*index, i.e. &base[index] at stride sizeof(TagPoint)).
    mpSpec      = lpSpec;
    mpTagPointA = &lpTagPointArrayBase[lpSpec->GetTagPointAIndex()];
    mpTagPointB = &lpTagPointArrayBase[lpSpec->GetTagPointBIndex()];

    // Seed the position lane from the streamed initial position; the w "plus" lane carries the
    // desired distance to endpoint A.
    const Vector3& lInitialPos       = lpSpec->GetInitialPos();
    mPositionPlusDistanceToA.x = lInitialPos.x;
    mPositionPlusDistanceToA.y = lInitialPos.y;
    mPositionPlusDistanceToA.z = lInitialPos.z;
    mPositionPlusDistanceToA.w = lpSpec->GetDesiredDistanceFromTagPointA();

    // Constraint direction = unit A->B between the two endpoints' rest positions; the w "plus"
    // lane carries the desired distance to endpoint B.
    const Vector3 lDirectionToB = Normalize(Subtract(mpTagPointB->GetInitialPosition(),
                                                      mpTagPointA->GetInitialPosition()));

    // Non-gating tripwire: the A->B direction must be unit length (BrnIKDrivenPoint.h:130).
    CGS_ASSERT(!IsZero(Vector3{ MagnitudeSquared(lDirectionToB) - 1.0f, 0.0f, 0.0f, 0.0f }),
               "!IsZero( RwMathVPU::MagnitudeSquared( lvDirectionToB ) - RwMathVPU::GetVecFloat_One() )");

    mDirectionPlusDistanceToB.x = lDirectionToB.x;
    mDirectionPlusDistanceToB.y = lDirectionToB.y;
    mDirectionPlusDistanceToB.z = lDirectionToB.z;
    mDirectionPlusDistanceToB.w = lpSpec->GetDesiredDistanceFromTagPointB();

    // Solve the constraint into mPositionPlusDistanceToA.
    Update();
}

void IKDrivenPoint::Update()
{
    using namespace rw::math::vpu;

    // Current driven-point position (xyz of mPositionPlusDistanceToA) and the two endpoints.
    Vector3 lPosition{ mPositionPlusDistanceToA.x, mPositionPlusDistanceToA.y,
                       mPositionPlusDistanceToA.z, mPositionPlusDistanceToA.w };

    const Vector3& lPosA = mpTagPointA->GetPosition();
    const Vector3& lPosB = mpTagPointB->GetPosition();

    VecFloat lvfDistanceToA; lvfDistanceToA.SetZero(); lvfDistanceToA.x = mPositionPlusDistanceToA.w;
    VecFloat lvfDistanceToB; lvfDistanceToB.SetZero(); lvfDistanceToB.x = mDirectionPlusDistanceToB.w;

    // Two-pass solve: pull to the desired distance from A, then from B (the console runs the
    // ResolveConstraint VMX block twice, once per endpoint), and write the corrected position
    // back into mPositionPlusDistanceToA (xyz; w "plus" lane preserved).
    lPosition = ResolveConstraint(lPosition, lPosA, lvfDistanceToA);
    lPosition = ResolveConstraint(lPosition, lPosB, lvfDistanceToB);

    mPositionPlusDistanceToA.x = lPosition.x;
    mPositionPlusDistanceToA.y = lPosition.y;
    mPositionPlusDistanceToA.z = lPosition.z;

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
