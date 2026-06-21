#pragma once

#include "types.hpp"

// New header b5-decomp/src/SharedClasses/World/BrnWorldRegion.h. Sourced from
// references/DecFIGS/dwarfdump/SharedClasses/World/BrnWorldRegion.h:30-76,81-83
// and the Feb-2007 partial source SharedClasses/World/BrnWorldRegion.h. X360 binary
// authoritative on the 8-byte layout (Construct @0x8229FE20 writes meCounty@+0,
// meDistrict@+4). Enum *type* names are ECounty/EDistrict (DWARF + binary),
// NOT the leak's County/District (PS3/leak drift).
namespace BrnWorld
{
enum ECounty
{
    E_COUNTY_PALM_BAY_HEIGHTS = 0,
    E_COUNTY_SILVER_LAKE,
    E_COUNTY_HARBOR_TOWN,
    E_COUNTY_WHITE_MOUNTAIN,
    E_COUNTY_DOWNTOWN_PARADISE,

    E_COUNTY_VALID_COUNT,

    E_COUNTY_INVALID = E_COUNTY_VALID_COUNT,
    E_COUNTY_COUNT
};

enum EDistrict
{
    E_DISTRICT_OCEAN_VIEW = 0,
    E_DISTRICT_WEST_ACRES,
    E_DISTRICT_TWIN_BRIDGES,
    E_DISTRICT_BIG_SURF_BEACH,
    E_DISTRICT_EASTERN_SHORE,
    E_DISTRICT_HILLSIDE_PASS,
    E_DISTRICT_HEARTBREAK_HILLS,
    E_DISTRICT_ROCKRIDGE_CLIFFS,
    E_DISTRICT_SOUTH_BAY,
    E_DISTRICT_PARK_VALE,
    E_DISTRICT_PARADISE_WHARF,
    E_DISTRICT_CRISTAL_SUMMIT,
    E_DISTRICT_LONE_PEAKS,
    E_DISTRICT_SUNSET_VALLEY,
    E_DISTRICT_DOWNTOWN,
    E_DISTRICT_RIVER_CITY,
    E_DISTRICT_MOTOR_CITY,
    E_DISTRICT_WATERFRONT,

    E_DISTRICT_VALID_COUNT,

    E_DISTRICT_INVALID = E_DISTRICT_VALID_COUNT,
    E_DISTRICT_COUNT
};

// DWARF BrnWorldRegion.h:44 -- post-increment for county iteration loops.
ECounty operator++(ECounty& leCounty, int);

// Name tables (defined out-of-line in the X360 .data; declared here, defined as
// externs in BrnWorldRegion.cpp). DWARF BrnWorldRegion.h:81/83 + .cpp hint 4/7/10.
extern const char* KAPC_COUNTY_NAMES[E_COUNTY_COUNT];
extern const char* KAPC_COUNTY_NAMES_ALT[E_COUNTY_COUNT];
extern const char* const KAPC_DISTRICT_NAMES[E_DISTRICT_COUNT];

struct WorldRegion
{
public:
    void     Construct(EDistrict leDistrict);

    ECounty   GetCounty() const   { return meCounty; }
    EDistrict GetDistrict() const { return meDistrict; }

    static ECounty     StringToCounty(const char* lpcCountyName);
    static EDistrict   StringToDistrict(const char* lpcDistrictName);
    static const char* CountyToString(ECounty leCounty);
    static const char* DistrictToString(EDistrict leDistrict);
    static ECounty     DistrictToCounty(EDistrict leDistrict);

private:
    ECounty   meCounty;   // +0  (DWARF BrnWorldRegion.h:132)
    EDistrict meDistrict; // +4  (DWARF BrnWorldRegion.h:133)
};
}
