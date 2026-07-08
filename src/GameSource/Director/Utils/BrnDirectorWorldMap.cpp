// BrnDirectorWorldMap.cpp -- BrnDirector::WorldMap query bodies.
//   GetLanePositionNearestPoint                 @ 0x8221CE98
//   GetLanePositionNearestPointWithDisplacement @ 0x8221D078
//   GetInterestingPointNear                     @ 0x8221D2A0
//   WalkLaneLeft                                @ 0x821F7800
//   GetTrafficData                              @ 0x82219128
//   GetSafePositionNearest                      @ 0x8223B750
//   GetSafePositionNearestPointWithDisplacement @ 0x8223BA78
//
// Store-for-store to the X360 VMX128 bodies. The closeness/direction predicates are the
// broadcast-lane vcmpgtfp / vmsum3fp results, reproduced as scalar tests over the committed
// rw::math::vpu MagnitudeSquared / Dot helpers. The mpTrafficData.operator->() calls (PVS
// lookup, then hull-array read) are preserved as separate dereferences, matching the asm.
// The X360 CalcTransformAtParameter (the unnamed sub_82219030, asserting at
// BrnTrafficSection.h:675) is the LaneRung* overload -- it computes position + direction + up.

#include "GameSource/Director/Utils/BrnDirectorWorldMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"   // BrnTraffic::TrafficData (mpPvs, GetHull)
#include "SharedClasses/Traffic/BrnTrafficPvs.h"                // BrnTraffic::Pvs::GetHullIndexForPoint
// BrnTrafficSection.h MUST precede BrnTrafficHull.h: it sets BRNTRAFFIC_SECTION_DEFINED so the
// real 48-byte Section (with GetNumSegments/CalcPositionAtParameter) wins over Hull.h's placeholder.
#include "SharedClasses/Traffic/BrnTrafficSection.h"            // BrnTraffic::Section, Neighbour, Side
#include "SharedClasses/Traffic/BrnTrafficHull.h"               // BrnTraffic::Hull (GetSection, mpaRungs)
#include "SharedClasses/Trigger/BrnTriggerData.h"               // BrnTrigger::TriggerData (GetGenericRegion*)
#include "SharedClasses/Trigger/BrnGenericRegion.h"             // BrnTrigger::GenericRegion (+ TriggerRegion/BoxRegion)
#include "rw/math/vpu/vector3_operation.h"                      // operator-, operator*, Dot, MagnitudeSquared

namespace BrnDirector
{
    // Rung step (the X360 walks luRung as a u8 byte counter += 10 each iteration, and
    // the VMX lfParam accumulator += a broadcast 10.0f = unk_82FAAB20).
    static const u8       KU_RUNG_DISTANCE  = 10;
    static const VecFloat KVF_RUNG_DISTANCE = { 10.0f, 10.0f, 10.0f, 10.0f };

    // Drop the returned "safe" point this far below the lane along the lane's up vector
    // (X360 flt_82009A74 == 2.25, broadcast then vmulfp/vsubfp in GetSafePositionNearest).
    static const f32      KF_SAFE_POSITION_DROP = 2.25f;

    // Below this squared displacement magnitude, the displacement query degenerates to the
    // plain nearest-safe query (X360 flt_82001C98 == 1.0, vcmpgtfp vs |lDisplacement|^2).
    static const f32      KF_MIN_DISPLACEMENT_SQ = 1.0f;

    // BrnDirectorWorldMap.h:82 / body @0x8221CE98.
    // Sample the lane rungs of the traffic hull containing lPosition and return the rung
    // point closest to lPosition in squared distance (no direction bias -- the plain sibling
    // of GetLanePositionNearestPointWithDisplacement). Sections are sub-sampled with a stride
    // of max(numSections/50, 1); rungs are stepped KU_RUNG_DISTANCE at a time.
    WorldMap::LanePosition WorldMap::GetLanePositionNearestPoint(Vector3 lPosition) const
    {
        LanePosition lNearest;
        lNearest.mfSquaredDistance = 100000.0f;   // stfs 0x10 (flt_820080E8)
        lNearest.mbValid           = false;       // stb  0x1E

        // hullIndex = pvs->GetHullIndexForPoint(lPosition); pvs == trafficData->mpPvs (+8).
        lNearest.muHullIndex = reinterpret_cast<const BrnTraffic::Pvs*>(
            mpTrafficData.operator->()->mpPvs)->GetHullIndexForPoint(lPosition);

        const BrnTraffic::Hull* lpHull =
            mpTrafficData.operator->()->GetHull(lNearest.muHullIndex);

        if (lpHull->muNumSections != 0)
        {
            u8 luSection = 0;
            do
            {
                const BrnTraffic::Section* lpSection = lpHull->GetSection(luSection);

                u8 luRung = 0;
                while (luRung < lpSection->GetNumSegments())
                {
                    // The X360 broadcasts (float)luRung into the parameter lane each iteration.
                    const f32      lfParam = static_cast<f32>(luRung);
                    const VecFloat lParam  = { lfParam, lfParam, lfParam, lfParam };

                    Vector3 lProspectivePosition;
                    lpSection->CalcPositionAtParameter(lpHull->mpaRungs, lParam, luRung,
                                                       lProspectivePosition);

                    const f32 lfSqDistance =
                        rw::math::vpu::MagnitudeSquared(lPosition - lProspectivePosition);

                    if (lfSqDistance < lNearest.mfSquaredDistance)
                    {
                        lNearest.mfSquaredDistance   = lfSqDistance;
                        lNearest.muSection           = luSection;
                        lNearest.mfParamAlongSection = lfParam;
                        lNearest.muRung              = luRung;
                        lNearest.mPosition           = lProspectivePosition;
                        lNearest.mbValid             = true;
                    }

                    luRung = static_cast<u8>(luRung + KU_RUNG_DISTANCE);
                }

                u32 luStep = lpHull->muNumSections / 50u;
                if (static_cast<s32>(luStep) <= 1)
                    luStep = 1;
                luSection = static_cast<u8>(luSection + luStep);
            }
            while (luSection < lpHull->muNumSections);
        }

        return lNearest;
    }

    // BrnDirectorWorldMap.h:87 / body @0x8221D078.
    // Sample the lane rungs of the traffic hull containing lPosition and return the
    // rung point that is (a) closest to lPosition in squared distance and (b) ahead
    // of lPosition in the lDisplacement direction (Dot(lDisplacement, point-lPosition)
    // > 0). Sections are sub-sampled with a stride of max(numSections/50, 1); rungs
    // are stepped KU_RUNG_DISTANCE at a time.
    WorldMap::LanePosition WorldMap::GetLanePositionNearestPointWithDisplacement(
        Vector3 lPosition, Vector3 lDisplacement) const
    {
        LanePosition lNearest;
        lNearest.mfSquaredDistance = 100000.0f;   // stfs 0x10
        lNearest.mbValid           = false;       // stb  0x1E

        // hullIndex = pvs->GetHullIndexForPoint(lPosition); pvs == trafficData->mpPvs (+8).
        lNearest.muHullIndex = reinterpret_cast<const BrnTraffic::Pvs*>(
            mpTrafficData.operator->()->mpPvs)->GetHullIndexForPoint(lPosition);

        const BrnTraffic::Hull* lpHull =
            mpTrafficData.operator->()->GetHull(lNearest.muHullIndex);

        VecFloat lNearestDistance = { 100000.0f, 100000.0f, 100000.0f, 100000.0f };

        if (lpHull->muNumSections != 0)
        {
            u8 luSection = 0;
            do
            {
                const BrnTraffic::Section* lpSection = lpHull->GetSection(luSection);

                VecFloat lRung  = { 0.0f, 0.0f, 0.0f, 0.0f };
                u8       luRung = 0;
                while (luRung < lpSection->GetNumSegments())
                {
                    Vector3 lProspectivePosition;
                    lpSection->CalcPositionAtParameter(lpHull->mpaRungs, lRung, luRung,
                                                       lProspectivePosition);

                    const f32 lfSqDistance =
                        rw::math::vpu::MagnitudeSquared(lPosition - lProspectivePosition);

                    if (lNearestDistance.x > lfSqDistance)
                    {
                        if (rw::math::vpu::Dot(lDisplacement,
                                               lProspectivePosition - lPosition) > 0.0f)
                        {
                            lNearestDistance.x           = lfSqDistance;
                            lNearest.mPosition           = lProspectivePosition;
                            lNearest.muSection           = luSection;
                            lNearest.muRung              = luRung;
                            lNearest.mbValid             = true;
                            lNearest.mfParamAlongSection = static_cast<f32>(luRung);
                        }
                    }

                    lRung.x += KVF_RUNG_DISTANCE.x;
                    lRung.y += KVF_RUNG_DISTANCE.y;
                    lRung.z += KVF_RUNG_DISTANCE.z;
                    lRung.w += KVF_RUNG_DISTANCE.w;
                    luRung = static_cast<u8>(luRung + KU_RUNG_DISTANCE);
                }

                u32 luStep = lpHull->muNumSections / 50u;
                if (static_cast<s32>(luStep) <= 1)
                    luStep = 1;
                luSection = static_cast<u8>(luSection + luStep);
            }
            while (luSection < lpHull->muNumSections);
        }

        lNearest.mfSquaredDistance = lNearestDistance.x;
        return lNearest;
    }

    // BrnDirectorWorldMap.h:90 / body @0x82219128.
    // Assert the world map has finished loading, then hand back the main-memory
    // TrafficData resource. mpTrafficData is the first member, so GetMemoryResource
    // is called on `this`+0 in the X360.
    const BrnTraffic::TrafficData* WorldMap::GetTrafficData() const
    {
        CGS_ASSERT(meLoadingState == E_LOADING_STATE_LOADED,
                   "meLoadingState == E_LOADING_STATE_LOADED");
        return mpTrafficData.GetMemoryResource();
    }

    // BrnDirectorWorldMap.h:121 / body @0x821F7800.
    // Follow left-lane neighbours across hull-section joins. Each hop rewrites the current
    // (section, rung, parameter) into the neighbour section's frame -- new = theirStartRung +
    // (old - ourStartRung) -- and stops when the section has no further left neighbour for the
    // current rung (FindNeighbourForRung returns 0xFFFF).
    void WorldMap::WalkLaneLeft(const BrnTraffic::Hull* lpHull, u8* lpu8Section,
                                u8* lpu8Rung, f32* lpfParameterOnSection) const
    {
        const BrnTraffic::Section* lpSection = lpHull->GetSection(*lpu8Section);

        while (true)
        {
            // fctidz: the rung index is the parameter truncated toward zero.
            const u16 luNeighbour = lpSection->FindNeighbourForRung(
                static_cast<u32>(*lpfParameterOnSection), BrnTraffic::E_LEFT, lpHull);
            if (luNeighbour == 0xFFFFu)
                break;

            CGS_ASSERT(lpHull->mpaNeighbourData != nullptr, "mpaNeighbourData");
            CGS_ASSERT(luNeighbour < lpHull->muNumNeighbours, "luIndex < muNumNeighbours");
            const BrnTraffic::Neighbour* lpNeighbour = &lpHull->mpaNeighbourData[luNeighbour];

            // Cross into the neighbour section (GetSection asserts luIndex < muNumSections).
            *lpu8Section = lpNeighbour->muSection;
            lpSection    = lpHull->GetSection(*lpu8Section);

            // Rung, in the neighbour's frame (two stores, matching the two stb in the asm).
            *lpu8Rung = static_cast<u8>(*lpu8Rung - lpNeighbour->muOurStartRung);
            *lpu8Rung = static_cast<u8>(lpNeighbour->muTheirStartRung + *lpu8Rung);
            CGS_ASSERT(*lpu8Rung <= lpSection->muNumRungs,
                       "*lpu8Rung <= lpSection->muNumRungs");

            // Parameter, likewise (two stfs).
            *lpfParameterOnSection = *lpfParameterOnSection
                                   - static_cast<f32>(lpNeighbour->muOurStartRung);
            *lpfParameterOnSection = static_cast<f32>(lpNeighbour->muTheirStartRung)
                                   + *lpfParameterOnSection;
            CGS_ASSERT(*lpfParameterOnSection <= static_cast<f32>(lpSection->muNumRungs),
                       "*lpfParameterOnSection <= (float32_t)lpSection->muNumRungs");
        }
    }

    // BrnDirectorWorldMap.h:109 / body @0x8221D2A0.
    // Scan the trigger data's generic regions for a Picture-Paradise region (type 18) whose
    // box centre is within lfRadius of lNearTo. On the first hit, write the region box's
    // dimensions to lpExtentsOut and its world transform to lpTransformOut, and return true.
    bool WorldMap::GetInterestingPointNear(Vector3 lNearTo, f32 lfRadius,
                                           Vector3* lpExtentsOut,
                                           Matrix44Affine* lpTransformOut) const
    {
        CGS_ASSERT(lpExtentsOut  != nullptr, "lpExtentsOut != NULL");
        CGS_ASSERT(lpTransformOut != nullptr, "lpTransformOut != NULL");

        const s32 liGenericRegionCount =
            mpTriggerData.operator->()->GetGenericRegionCount();
        const f32 lfSquaredRadius = lfRadius * lfRadius;   // fmuls f31,f31, broadcast v127

        for (s32 liLoop = 0; liLoop < liGenericRegionCount; ++liLoop)
        {
            const BrnTrigger::GenericRegion* lpRegion =
                mpTriggerData.operator->()->GetGenericRegion(liLoop);
            if (lpRegion->GetType() != BrnTrigger::GenericRegion::E_TYPE_PICTURE_PARADISE)
                continue;

            const Vector3 lPosition = mpTriggerData.operator->()->GetGenericRegion(liLoop)
                                          ->GetBoxRegion()->GetPosition();
            if (rw::math::vpu::MagnitudeSquared(lPosition - lNearTo) <= lfSquaredRadius)
            {
                *lpExtentsOut = mpTriggerData.operator->()->GetGenericRegion(liLoop)
                                    ->GetBoxRegion()->GetDimensions();
                *lpTransformOut = mpTriggerData.operator->()->GetGenericRegion(liLoop)
                                      ->GetBoxRegion()->ComputeTransform();
                return true;
            }
        }

        return false;
    }

    // BrnDirectorWorldMap.h:61 / body @0x8223B750.
    // Nearest safe camera position to lPosition. Find the lane rung nearest lPosition, then
    // walk that section forward or backward (depending on which way the lane tangent points
    // relative to lPosition) to the rung whose lane frame straddles lPosition, cross to the
    // leftmost lane, and drop the result KF_SAFE_POSITION_DROP below the lane along its up.
    bool WorldMap::GetSafePositionNearest(Vector3 lPosition, Vector3& lOut) const
    {
        CGS_ASSERT(meLoadingState == E_LOADING_STATE_LOADED,
                   "meLoadingState == E_LOADING_STATE_LOADED");

        lOut = lPosition;   // stvx128 v127 -> lOut (initial store, overwritten below)

        LanePosition lNearest = GetLanePositionNearestPoint(lPosition);
        const BrnTraffic::Hull* lpHull =
            mpTrafficData.operator->()->GetHull(lNearest.muHullIndex);

        Vector3 lResult = lNearest.mPosition;   // used verbatim when the rung is not valid

        if (lNearest.mbValid)
        {
            const BrnTraffic::Section* lpSection = lpHull->GetSection(lNearest.muSection);

            Vector3 lUp = { 0.0f, 0.0f, 0.0f, 0.0f };   // v55 (up) zeroed
            Vector3 lDirection;

            const VecFloat lParam0 = { lNearest.mfParamAlongSection, lNearest.mfParamAlongSection,
                                       lNearest.mfParamAlongSection, lNearest.mfParamAlongSection };
            lpSection->CalcDirectionAtParameter(lpHull->mpaRungs, lParam0, lNearest.muRung,
                                                lDirection);

            if (rw::math::vpu::Dot(lDirection, lPosition - lNearest.mPosition) > 0.0f)
            {
                // Tangent points "behind" lPosition -> advance to later rungs.
                while (static_cast<u32>(lNearest.muRung + 1) < lpSection->GetNumSegments())
                {
                    lNearest.muRung              = static_cast<u8>(lNearest.muRung + 1);
                    lNearest.mfParamAlongSection = static_cast<f32>(lNearest.muRung);

                    const VecFloat lParam = { lNearest.mfParamAlongSection, lNearest.mfParamAlongSection,
                                              lNearest.mfParamAlongSection, lNearest.mfParamAlongSection };
                    lpSection->CalcTransformAtParameter(lpHull->mpaRungs, lParam, lNearest.muRung,
                                                        lNearest.mPosition, lDirection, lUp);

                    if (rw::math::vpu::Dot(lDirection, lPosition - lNearest.mPosition) <= 0.0f)
                        break;
                }
            }
            else
            {
                // Tangent points "ahead" of lPosition -> step back to earlier rungs.
                while (lNearest.muRung > 1)
                {
                    lNearest.muRung              = static_cast<u8>(lNearest.muRung - 1);
                    lNearest.mfParamAlongSection = static_cast<f32>(lNearest.muRung);

                    const VecFloat lParam = { lNearest.mfParamAlongSection, lNearest.mfParamAlongSection,
                                              lNearest.mfParamAlongSection, lNearest.mfParamAlongSection };
                    lpSection->CalcTransformAtParameter(lpHull->mpaRungs, lParam, lNearest.muRung,
                                                        lNearest.mPosition, lDirection, lUp);

                    if (rw::math::vpu::Dot(lDirection, lPosition - lNearest.mPosition) >= 0.0f)
                        break;
                }
            }

            WalkLaneLeft(lpHull, &lNearest.muSection, &lNearest.muRung,
                         &lNearest.mfParamAlongSection);

            const BrnTraffic::Section* lpFinalSection = lpHull->GetSection(lNearest.muSection);
            const VecFloat lParamF = { lNearest.mfParamAlongSection, lNearest.mfParamAlongSection,
                                       lNearest.mfParamAlongSection, lNearest.mfParamAlongSection };
            lpFinalSection->CalcTransformAtParameter(lpHull->mpaRungs, lParamF, lNearest.muRung,
                                                     lNearest.mPosition, lDirection, lUp);

            lResult = lNearest.mPosition - lUp * KF_SAFE_POSITION_DROP;
        }

        lOut = lResult;
        return lNearest.mbValid;
    }

    // BrnDirectorWorldMap.h:67 / body @0x8223BA78.
    // As GetSafePositionNearest, but biases the lane walk by lDisplacement. For a negligible
    // displacement it forwards to GetSafePositionNearest; otherwise the direction test adds the
    // displacement, so the walk settles where the lane frame straddles lPosition + lDisplacement.
    bool WorldMap::GetSafePositionNearestPointWithDisplacement(Vector3 lPosition,
                                                               Vector3 lDisplacement,
                                                               Vector3& lOut) const
    {
        CGS_ASSERT(meLoadingState == E_LOADING_STATE_LOADED,
                   "meLoadingState == E_LOADING_STATE_LOADED");

        lOut = lPosition;   // stvx128 v126 -> lOut (initial store)

        if (rw::math::vpu::MagnitudeSquared(lDisplacement) < KF_MIN_DISPLACEMENT_SQ)
            return GetSafePositionNearest(lPosition, lOut);

        LanePosition lNearest =
            GetLanePositionNearestPointWithDisplacement(lPosition, lDisplacement);
        const BrnTraffic::Hull* lpHull =
            mpTrafficData.operator->()->GetHull(lNearest.muHullIndex);

        Vector3 lResult = lNearest.mPosition;

        if (lNearest.mbValid)
        {
            const BrnTraffic::Section* lpSection = lpHull->GetSection(lNearest.muSection);

            Vector3 lUp = { 0.0f, 0.0f, 0.0f, 0.0f };
            Vector3 lDirection;

            const VecFloat lParam0 = { lNearest.mfParamAlongSection, lNearest.mfParamAlongSection,
                                       lNearest.mfParamAlongSection, lNearest.mfParamAlongSection };
            lpSection->CalcDirectionAtParameter(lpHull->mpaRungs, lParam0, lNearest.muRung,
                                                lDirection);

            // vmsum3fp(dir,disp) - vmsum3fp(dir, pos - lPosition) == Dot(dir, disp + lPosition - pos).
            if (rw::math::vpu::Dot(lDirection, lDisplacement)
                    - rw::math::vpu::Dot(lDirection, lNearest.mPosition - lPosition) > 0.0f)
            {
                while (static_cast<u32>(lNearest.muRung + 1) < lpSection->GetNumSegments())
                {
                    lNearest.muRung              = static_cast<u8>(lNearest.muRung + 1);
                    lNearest.mfParamAlongSection = static_cast<f32>(lNearest.muRung);

                    const VecFloat lParam = { lNearest.mfParamAlongSection, lNearest.mfParamAlongSection,
                                              lNearest.mfParamAlongSection, lNearest.mfParamAlongSection };
                    lpSection->CalcTransformAtParameter(lpHull->mpaRungs, lParam, lNearest.muRung,
                                                        lNearest.mPosition, lDirection, lUp);

                    if (rw::math::vpu::Dot(lDirection, lDisplacement)
                            - rw::math::vpu::Dot(lDirection, lNearest.mPosition - lPosition) <= 0.0f)
                        break;
                }
            }
            else
            {
                while (lNearest.muRung > 1)
                {
                    lNearest.muRung              = static_cast<u8>(lNearest.muRung - 1);
                    lNearest.mfParamAlongSection = static_cast<f32>(lNearest.muRung);

                    const VecFloat lParam = { lNearest.mfParamAlongSection, lNearest.mfParamAlongSection,
                                              lNearest.mfParamAlongSection, lNearest.mfParamAlongSection };
                    lpSection->CalcTransformAtParameter(lpHull->mpaRungs, lParam, lNearest.muRung,
                                                        lNearest.mPosition, lDirection, lUp);

                    if (rw::math::vpu::Dot(lDirection, lDisplacement)
                            - rw::math::vpu::Dot(lDirection, lNearest.mPosition - lPosition) >= 0.0f)
                        break;
                }
            }

            WalkLaneLeft(lpHull, &lNearest.muSection, &lNearest.muRung,
                         &lNearest.mfParamAlongSection);

            const BrnTraffic::Section* lpFinalSection = lpHull->GetSection(lNearest.muSection);
            const VecFloat lParamF = { lNearest.mfParamAlongSection, lNearest.mfParamAlongSection,
                                       lNearest.mfParamAlongSection, lNearest.mfParamAlongSection };
            lpFinalSection->CalcTransformAtParameter(lpHull->mpaRungs, lParamF, lNearest.muRung,
                                                     lNearest.mPosition, lDirection, lUp);

            lResult = lNearest.mPosition - lUp * KF_SAFE_POSITION_DROP;
        }

        lOut = lResult;
        return lNearest.mbValid;
    }
}
