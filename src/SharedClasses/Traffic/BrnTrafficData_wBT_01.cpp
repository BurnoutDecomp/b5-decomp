#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"

#include "SharedClasses/Traffic/BrnTrafficPvs.h"            // Pvs::GetHullIndexForPoint / GetCellSize
#include "SharedClasses/Traffic/BrnTrafficHull.h"           // Hull::GetSection / mpaRungs
#include "SharedClasses/Traffic/BrnTrafficSection.h"        // Section / LaneRung
#include "GameShared/GameClasses/Containers/CgsArray.h"     // Array<u16,400>
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                  // Dot / Normalize / IsValid / axes

#include <cmath>   // std::sqrt

// BrnTrafficData partfile (blocked-TU wave): the nearest-lane search, beside its home
// SharedClasses/Traffic/BrnTrafficData.cpp (asserts cite BrnTrafficData.cpp).

namespace BrnTraffic
{
    namespace
    {
        // The console's all-lanes self-equality test on a VecFloat (vcmpeqfp. v, v, v).
        inline bool IsValidVecFloat(VecFloat lfValue)
        {
            return lfValue.x == lfValue.x && lfValue.y == lfValue.y
                && lfValue.z == lfValue.z && lfValue.w == lfValue.w;
        }

        // Every lane of lfLhs strictly greater than the same lane of lfRhs (vcmpgtfp. "all true").
        inline bool AllGreater(VecFloat lfLhs, VecFloat lfRhs)
        {
            return lfLhs.x > lfRhs.x && lfLhs.y > lfRhs.y && lfLhs.z > lfRhs.z && lfLhs.w > lfRhs.w;
        }

        inline VecFloat SplatScalar(f32 lfValue) { return VecFloat{ lfValue, lfValue, lfValue, lfValue }; }
    }

    // BrnTrafficData.h. Search the hull under lPosition plus the hulls under the four
    // points lfMaxDistance away along +Z, +X, -Z and -X (each hull once), for the lane segment
    // whose start rung is nearest to lPosition within lfMaxDistance and whose direction is within
    // lfMinCosAngleToDir of lDirection. Each hull searched shrinks the radius to the best distance
    // so far. Sole caller: TrafficEntityModule::UpdateParams_TryToReinsertParam.
    bool TrafficData::FindNearestLaneForPoint(Vector3 lPosition, VecFloat lfMaxDistance, Vector3 lDirection,
                                              VecFloat lfMinCosAngleToDir, u32* lpuOutHull, u32* lpuOutSection,
                                              f32* lpfOutParam, u32* lpuOutSegment) const
    {
        CGS_ASSERT(lpuOutHull != 0, "lpuOutHull");
        CGS_ASSERT(lpuOutSection != 0, "lpuOutSection");
        CGS_ASSERT(lpfOutParam != 0, "lpfOutParam");
        CGS_ASSERT(lpuOutSegment != 0, "lpuOutSegment");
        CGS_ASSERT(AllGreater(mpPvs->GetCellSize(), lfMaxDistance), "lfMaxDistance < mpPvs->GetCellSize()");
        CGS_ASSERT(rw::math::vpu::IsValid(lPosition), "RwMath::IsValid( lPosition )");
        CGS_ASSERT(IsValidVecFloat(lfMaxDistance), "RwMath::IsValid( lfMaxDistance )");
        CGS_ASSERT(rw::math::vpu::IsValid(lDirection), "RwMath::IsValid( lDirection )");
        CGS_ASSERT(IsValidVecFloat(lfMinCosAngleToDir), "RwMath::IsValid( lfMinCosAngleToDir )");

        static const s32 KI_NUM_PROBES = 4;
        const Vector3 laProbeDirections[KI_NUM_PROBES] =
        {
            rw::math::vpu::GetVector3_ZAxis(),
            rw::math::vpu::GetVector3_XAxis(),
            -rw::math::vpu::GetVector3_ZAxis(),
            -rw::math::vpu::GetVector3_XAxis(),
        };

        Array<u16, 400> lHulls;
        lHulls.Clear();
        lHulls.Append(static_cast<u16>(mpPvs->GetHullIndexForPoint(lPosition)));

        for (s32 liProbe = 0; liProbe < KI_NUM_PROBES; ++liProbe)
        {
            const Vector3& lrDirection = laProbeDirections[liProbe];
            const Vector3 lProbe = { lfMaxDistance.x * lrDirection.x + lPosition.x,
                                     lfMaxDistance.y * lrDirection.y + lPosition.y,
                                     lfMaxDistance.z * lrDirection.z + lPosition.z,
                                     lfMaxDistance.w * lrDirection.w + lPosition.w };
            const u16 luHull = static_cast<u16>(mpPvs->GetHullIndexForPoint(lProbe));
            if (lHulls.FindFirstInstanceOf(luHull) == -1)
                lHulls.Append(luHull);
        }

        bool lbFound = false;
        VecFloat lfSearchDistance = lfMaxDistance;
        for (u32 luIndex = 0; luIndex < lHulls.GetLength(); ++luIndex)
        {
            const u32 luHull = lHulls.GetItem(luIndex);
            u32 luSection = 0;
            f32 lfParam = 0.0f;
            u32 luSegment = 0;
            if (FindNearestLaneForPointInHull(luHull, lPosition, lfSearchDistance, lDirection, lfMinCosAngleToDir,
                                              &luSection, &lfParam, &luSegment, lfSearchDistance))
            {
                lbFound        = true;
                *lpuOutHull    = luHull;
                *lpuOutSection = luSection;
                *lpfOutParam   = lfParam;
                *lpuOutSegment = luSegment;
            }
        }
        return lbFound;
    }

    // BrnTrafficData.h. Within one hull: for every lane segment (rung i -> rung i+1 of
    // every section) measure the squared distance from lPosition to the segment's start rung
    // point; a candidate nearer than the best so far (initially lfMaxDistance squared) counts
    // when the segment's direction makes a cosine of at least lfMinCosAngleToDir with lDirection
    // (a zero-length segment scores -1). On success the section, segment, the segment index as
    // the parameter, and the best distance (0 for a coincident point) are handed back.
    bool TrafficData::FindNearestLaneForPointInHull(u32 luHull, Vector3 lPosition, VecFloat lfMaxDistance,
                                                    Vector3 lDirection, VecFloat lfMinCosAngleToDir,
                                                    u32* lpuOutSection, f32* lpfOutParam, u32* lpuOutSegment,
                                                    VecFloat& lfOutDistance) const
    {
        const Hull* lpHull = mpapHulls[luHull];

        f32  lfBestDistanceSquared = lfMaxDistance.x * lfMaxDistance.x;
        u32  luBestSection = 255;
        u32  luBestSegment = 0xFFFFFFFFu;
        bool lbFound = false;

        for (u32 luSection = 0; luSection < lpHull->muNumSections; ++luSection)
        {
            const Section* lpSection = lpHull->GetSection(luSection);
            const LaneRung* lpRung = &lpHull->mpaRungs[lpSection->muRungOffset];

            for (u32 luSegment = 0; luSegment < lpSection->GetNumSegments(); ++luSegment, ++lpRung)
            {
                const Vector3 lToPosition = lPosition - lpRung[0].maPoints[0];
                const f32 lfDistanceSquared = rw::math::vpu::Dot(lToPosition, lToPosition);
                if (!(lfDistanceSquared >= lfBestDistanceSquared))
                {
                    const Vector3 lSegment = lpRung[1].maPoints[0] - lpRung[0].maPoints[0];
                    const f32 lfSegmentLengthSquared = rw::math::vpu::Dot(lSegment, lSegment);
                    const f32 lfCosAngle = (lfSegmentLengthSquared == 0.0f)
                                         ? -1.0f
                                         : rw::math::vpu::Dot(rw::math::vpu::Normalize(lSegment), lDirection);
                    if (!AllGreater(lfMinCosAngleToDir, SplatScalar(lfCosAngle)))
                    {
                        lfBestDistanceSquared = lfDistanceSquared;
                        luBestSection = luSection;
                        luBestSegment = luSegment;
                        lbFound = true;
                    }
                }
            }
        }

        if (lbFound)
        {
            *lpuOutSection = luBestSection;
            *lpuOutSegment = luBestSegment;
            *lpfOutParam   = static_cast<f32>(luBestSegment);

            const f32 lfDistance = (lfBestDistanceSquared == 0.0f) ? 0.0f : std::sqrt(lfBestDistanceSquared);
            lfOutDistance = SplatScalar(lfDistance);
        }
        return lbFound;
    }
}
