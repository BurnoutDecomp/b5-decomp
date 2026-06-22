#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3

// BrnPhysics::Deformation::IKDrivenPointSpec — the streamed (resource-baked) description
// of one IK-driven deformation point: its rest position, the two tag points it is
// constrained to, and the desired distance to each. Reconstructed with member names/types
// from the DecFIGS DWARF (BrnIKDrivenPointSpec.h:46) and consumed (read-only, by accessor)
// by IKDrivenPoint::Construct / IKDrivenPoint::Update.
//
// Layout faithfulness: the X360 IKDrivenPoint asm reads the two desired distances and the
// two tag-point indices off this record; the absolute console offsets are 32-bit-pointer
// offsets that do not (and need not) reproduce byte-for-byte on a 64-bit host. The consumers
// touch this record ONLY by the named accessors below, so the host compiler recomputes the
// correct addresses; semantic parity (which field, which distance, which index) is exact.
//
// MINIMAL OWNING SLICE: only the members + the read accessors the IKDrivenPoint TU needs are
// bodied. Construct/Destruct/Prepare/Release/SetInitialPosition/FixUp/FixDown (DWARF lines
// 54-88) are declared HONESTLY but left for IKDrivenPointSpec's own TU; their bodies are not
// required to build IKDrivenPoint.

namespace BrnPhysics
{
namespace Deformation
{
    struct IKDrivenPointSpec
    {
    public:
        // BrnIKDrivenPointSpec.h:82 — the streamed rest position.
        const Vector3& GetInitialPos() const { return mInitialPos; }

        // BrnIKDrivenPointSpec.h:76/79 — desired rest distances to the two tag points.
        f32 GetDesiredDistanceFromTagPointA() const { return mfDistanceFromA; }
        f32 GetDesiredDistanceFromTagPointB() const { return mfDistanceFromB; }

        // BrnIKDrivenPointSpec.h:70/73 — indices into the owning DeformableObject's tag-point
        // array selecting the two endpoints this driven point is constrained between. The
        // DWARF returns int32_t; the storage is the 16-bit streamed field.
        s32 GetTagPointAIndex() const { return miTagPointIndexA; }
        s32 GetTagPointBIndex() const { return miTagPointIndexB; }

        // ---- remaining authored API (declared only; bodies in IKDrivenPointSpec's own TU) --
        void Construct(Vector3, s16, s16, f32, f32);
        void Destruct();
        bool Prepare();
        bool Release();
        void SetInitialPosition(Vector3);
        void FixUp(void*);
        void FixDown(void*);

    private:
        Vector3 mInitialPos;       // BrnIKDrivenPointSpec.h:92
        f32     mfDistanceFromA;   // :93
        f32     mfDistanceFromB;   // :94
        s16     miTagPointIndexA;  // :95
        s16     miTagPointIndexB;  // :96
    };
}
}
