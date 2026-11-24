#include "SharedClasses/World/BrnWorldRegion.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

namespace BrnWorld
{
void WorldRegion::Construct(EDistrict leDistrict)
{
    CGS_ASSERT(leDistrict < E_DISTRICT_COUNT,
        CgsDev::Assert::MessageAt(
            "leDistrict < E_DISTRICT_COUNT",
            "..\\..\\..\\SharedClasses\\World/BrnWorldRegion.h", 155 /* ARTIST 0x8229FE4C */));

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

// FOLDED FROM BrnWorldRegion_ToString.cpp
// ============================================================================
// BrnWorld::WorldRegion::CountyToString / ::DistrictToString.
//
// Split out of BrnWorldRegion.cpp (pose wave 2026-08-01) WITHOUT any change to either
// body. Reason: both read the X360 .data name tables KAPC_COUNTY_NAMES /
// KAPC_DISTRICT_NAMES, which are function-free .data and therefore carry NO definition
// in the function-only IDA export set -- they are declared extern in the header and
// defined nowhere in the tree. While they shared a TU with WorldRegion::Construct, any
// consumer that only wanted Construct (RaceCar::Construct does, and the race-car attach
// chain needs it) pulled two unresolvable externals into the link.
//
// Nothing here is fabricated and nothing is stubbed: the two functions keep their exact
// bodies, and this TU simply stays UNMOUNTED until the tables are recovered from the XEX
// .data (they are debug/diagnostic strings; nothing in the shipping path calls either
// function). Mount it together with a real definition of the two tables.
// ============================================================================

#include "SharedClasses/World/BrnWorldRegion.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

namespace BrnWorld
{
// Exact ARTIST tables at 0x82F2C90C and 0x820A7600. Their compact spellings are
// intentional: these are debug/serialization names rather than presentation text.
const char* KAPC_COUNTY_NAMES[E_COUNTY_COUNT] =
{
    "PalmBayHeights",
    "SilverLake",
    "HarborTown",
    "WhiteMountain",
    "DowntownParadise",
    "Anywhere",
};

const char* const KAPC_DISTRICT_NAMES[E_DISTRICT_COUNT] =
{
    "OceanView",
    "WestAcres",
    "TwinBridges",
    "BigSurfBeach",
    "EasternShore",
    "HillsidePass",
    "HeartbreakHills",
    "RockridgeCliffs",
    "SouthBay",
    "ParkVale",
    "ParadiseWharf",
    "CristalSummit",
    "LonePeaks",
    "SunsetValley",
    "Downtown",
    "RiverCity",
    "MotorCity",
    "Waterfront",
    "DistrictInvalid",
};

// KAPC_COUNTY_NAMES_ALT is not read by either ARTIST ToString routine.
extern const char* KAPC_COUNTY_NAMES_ALT[E_COUNTY_COUNT];

const char* WorldRegion::CountyToString(ECounty leCounty)
{
    CGS_ASSERT(leCounty < E_COUNTY_COUNT, "leCounty >= 0 && leCounty < E_COUNTY_COUNT");

    return KAPC_COUNTY_NAMES[leCounty];
}

const char* WorldRegion::DistrictToString(EDistrict leDistrict)
{
    CGS_ASSERT(static_cast<u32>(leDistrict) < (sizeof(KAPC_DISTRICT_NAMES) / sizeof(char*)),
        CgsDev::Assert::MessageAt(
            "static_cast<size_t>( leDistrict ) < sizeof(KAPC_DISTRICT_NAMES) / sizeof(char*)",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\"
            "../../SharedClasses/World/BrnWorldRegion.cpp", 188 /* ARTIST 0x8267652C */));

    return KAPC_DISTRICT_NAMES[leDistrict];
}
}
