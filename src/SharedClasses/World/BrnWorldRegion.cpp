#include "SharedClasses/World/BrnWorldRegion.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnWorld
{
// Name strings live in the X360 .data segment; declared extern here and in the
// header (DWARF BrnWorldRegion.cpp hint lines 4/7/10). KAPC_COUNTY_NAMES_ALT is
// the alternate county-name table the X360 build emits alongside the primary one.
extern const char* KAPC_COUNTY_NAMES[E_COUNTY_COUNT];
extern const char* KAPC_COUNTY_NAMES_ALT[E_COUNTY_COUNT];
extern const char* const KAPC_DISTRICT_NAMES[E_DISTRICT_COUNT];

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

const char* WorldRegion::CountyToString(ECounty leCounty)
{
    CGS_ASSERT(leCounty < E_COUNTY_COUNT, "leCounty >= 0 && leCounty < E_COUNTY_COUNT");

    return KAPC_COUNTY_NAMES[leCounty];
}

const char* WorldRegion::DistrictToString(EDistrict leDistrict)
{
    CGS_ASSERT(static_cast<u32>(leDistrict) < (sizeof(KAPC_DISTRICT_NAMES) / sizeof(char*)),
               "static_cast<size_t>( leDistrict ) < sizeof(KAPC_DISTRICT_NAMES) / sizeof(char*)");

    return KAPC_DISTRICT_NAMES[leDistrict];
}
}
