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

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnWorld
{
// Name strings live in the X360 .data segment; declared extern here and in the
// header (DWARF BrnWorldRegion.cpp hint lines 4/7/10). KAPC_COUNTY_NAMES_ALT is
// the alternate county-name table the X360 build emits alongside the primary one.
extern const char* KAPC_COUNTY_NAMES[E_COUNTY_COUNT];
extern const char* KAPC_COUNTY_NAMES_ALT[E_COUNTY_COUNT];
extern const char* const KAPC_DISTRICT_NAMES[E_DISTRICT_COUNT];

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
