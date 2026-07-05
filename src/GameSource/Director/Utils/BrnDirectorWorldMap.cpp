// BrnDirectorWorldMap.cpp -- BrnDirector::WorldMap query bodies.
//   GetLanePositionNearestPointWithDisplacement @ 0x8221D078
//   GetTrafficData                              @ 0x82219128
//
// Store-for-store to the X360 VMX128 bodies. The two closeness/direction
// predicates are the broadcast-lane vcmpgtfp results, reproduced as scalar > tests
// over the committed rw::math::vpu MagnitudeSquared / Dot helpers. The two
// mpTrafficData.operator->() calls (PVS lookup, then hull-array read) are preserved
// as separate dereferences, matching the two operator-> calls in the asm.

#include "GameSource/Director/Utils/BrnDirectorWorldMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"   // BrnTraffic::TrafficData (mpPvs, GetHull)
#include "SharedClasses/Traffic/BrnTrafficPvs.h"                // BrnTraffic::Pvs::GetHullIndexForPoint
// BrnTrafficSection.h MUST precede BrnTrafficHull.h: it sets BRNTRAFFIC_SECTION_DEFINED so the
// real 48-byte Section (with GetNumSegments/CalcPositionAtParameter) wins over Hull.h's placeholder.
#include "SharedClasses/Traffic/BrnTrafficSection.h"            // BrnTraffic::Section (GetNumSegments, CalcPositionAtParameter)
#include "SharedClasses/Traffic/BrnTrafficHull.h"               // BrnTraffic::Hull (GetSection, mpaRungs)
#include "rw/math/vpu/vector3_operation.h"                      // operator-, Dot, MagnitudeSquared

namespace BrnDirector
{
    // Rung step (the X360 walks luRung as a u8 byte counter += 10 each iteration, and
    // the VMX lfParam accumulator += a broadcast 10.0f = unk_82FAAB20).
    static const u8       KU_RUNG_DISTANCE  = 10;
    static const VecFloat KVF_RUNG_DISTANCE = { 10.0f, 10.0f, 10.0f, 10.0f };

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
}
