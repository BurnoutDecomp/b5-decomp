#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3, Vector3Plus
#include "SharedClasses/Physics/Deformation/BrnIKDrivenPointSpec.h"      // IKDrivenPointSpec (committed minimal home)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnTagPoint.h"  // TagPoint

// BrnPhysics::Deformation::IKDrivenPoint — a deformation point whose position is solved by
// a two-bone inverse-kinematics constraint: it is pulled to lie a desired distance from each
// of two tag points (A and B). Reconstructed from BURNOUT_X360_ARTIST.XEX (offset authority)
// with member names/types from the DecFIGS DWARF (BrnIKDrivenPoint.h:47).
//
// Layout (DWARF BrnIKDrivenPoint.h:93-98; console offsets the X360 asm uses noted per member,
// 32-bit-pointer offsets that need not reproduce byte-for-byte on the 64-bit host — every
// access here is BY MEMBER NAME, so the host recomputes the addresses and semantic parity is
// exact):
//
//   +0x00 Vector3Plus            mPositionPlusDistanceToA   (xyz = current position; w = desired distance to A)
//   +0x10 Vector3Plus            mDirectionPlusDistanceToB  (xyz = constraint direction; w = desired distance to B)
//   +0x20 const IKDrivenPointSpec* mpSpec                   (the streamed spec)
//   +0x24 const TagPoint*        mpTagPointA                (endpoint A; base + 32*specIndexA)
//   +0x28 const TagPoint*        mpTagPointB                (endpoint B; base + 32*specIndexB)
//   +0x2C f32                    mfScratchAmount            (blended scratch/damage weight)

namespace BrnPhysics
{
namespace Deformation
{
    struct IKDrivenPoint
    {
    public:
        // X360 0x82615468. Cache the spec + the two tag-point endpoints (selected from the
        // tag-point array base by the spec's two indices, stride sizeof(TagPoint)), seed the
        // position/direction lanes and the per-endpoint desired distances, then run one Update
        // to solve the constraint into mPositionPlusDistanceToA.
        void Construct(const IKDrivenPointSpec* lpSpec, const TagPoint* lpTagPointArrayBase);

        // X360 0x825E7608. Re-solve the two-bone IK constraint from the current tag-point
        // positions: project the driven point onto each endpoint's desired-distance sphere
        // (constraint direction = normalized A->B), then recompute the blended scratch amount.
        void Update();

        // ---- remaining authored API (DWARF lines 55-104; declared only, bodied elsewhere) --
        void           Destruct();
        bool           Prepare();
        bool           Release();
        const Vector3& GetPosition() const;
        const Vector3& GetDirection() const;
        const TagPoint* GetTagPointA() const;
        const TagPoint* GetTagPointB() const;
        void           SetPosition(Vector3);
        Vector3        GetOffsetFromInitialPosition() const;
        f32            GetScratchAmount() const;
        Vector3        GetOriginalPosition() const;

    private:
        Vector3                  ResolveConstraint(Vector3, Vector3, VecFloat);

        Vector3Plus              mPositionPlusDistanceToA;   // +0x00
        Vector3Plus              mDirectionPlusDistanceToB;  // +0x10
        const IKDrivenPointSpec* mpSpec;                     // +0x20
        const TagPoint*          mpTagPointA;                // +0x24
        const TagPoint*          mpTagPointB;                // +0x28
        f32                      mfScratchAmount;            // +0x2C
    };
}
}
