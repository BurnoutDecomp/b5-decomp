#include "SharedClasses/World/BrnWorldRegion.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnWorld
{
void WorldRegion::Construct(EDistrict leDistrict)
{
    CGS_ASSERT(leDistrict < E_DISTRICT_COUNT, "leDistrict < E_DISTRICT_COUNT");

    meDistrict = leDistrict;
    meCounty   = DistrictToCounty(meDistrict);
}

ECounty WorldRegion::DistrictToCounty(EDistrict leDistrict)
{
    switch (leDistrict)
    {
        case E_DISTRICT_OCEAN_VIEW:
        case E_DISTRICT_WEST_ACRES:
        case E_DISTRICT_TWIN_BRIDGES:
        case E_DISTRICT_BIG_SURF_BEACH:
            return E_COUNTY_PALM_BAY_HEIGHTS;
        case E_DISTRICT_EASTERN_SHORE:
        case E_DISTRICT_HILLSIDE_PASS:
        case E_DISTRICT_HEARTBREAK_HILLS:
            return E_COUNTY_SILVER_LAKE;
        case E_DISTRICT_ROCKRIDGE_CLIFFS:
        case E_DISTRICT_SOUTH_BAY:
        case E_DISTRICT_PARK_VALE:
        case E_DISTRICT_PARADISE_WHARF:
            return E_COUNTY_HARBOR_TOWN;
        case E_DISTRICT_CRISTAL_SUMMIT:
        case E_DISTRICT_LONE_PEAKS:
        case E_DISTRICT_SUNSET_VALLEY:
            return E_COUNTY_WHITE_MOUNTAIN;
        case E_DISTRICT_DOWNTOWN:
        case E_DISTRICT_RIVER_CITY:
        case E_DISTRICT_MOTOR_CITY:
        case E_DISTRICT_WATERFRONT:
            return E_COUNTY_DOWNTOWN_PARADISE;
        default:
            return E_COUNTY_INVALID;
    }
}

// CountyToString / DistrictToString moved to BrnWorldRegion_ToString.cpp (pose wave
// 2026-08-01) -- bodies unchanged. They are the ONLY users of the two extern .data name
// tables above, which have no definition in the tree (function-only IDA exports carry no
// .data), so keeping them here made this TU unlinkable for every consumer that only needs
// Construct/DistrictToCounty. See that file's banner.
}
