#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnWorld::WorldRegion
{
extern const char* gapCountyNames[6];
extern const char* gapDistrictNames[19];

const char* CountyToString(u32 luCounty)
{
    CGS_ASSERT(luCounty < 6, "leCounty >= 0 && leCounty < E_COUNTY_COUNT");

    return gapCountyNames[luCounty];
}

int DistrictToCounty(int liDistrict)
{
    switch (liDistrict)
    {
        case 0:
        case 1:
        case 2:
        case 3:
            return 0;
        case 4:
        case 5:
        case 6:
            return 1;
        case 7:
        case 8:
        case 9:
        case 10:
            return 2;
        case 11:
        case 12:
        case 13:
            return 3;
        case 14:
        case 15:
        case 16:
        case 17:
            return 4;
        default:
            return 5;
    }
}

const char* DistrictToString(u32 luDistrict)
{
    CGS_ASSERT(luDistrict < 19, "static_cast<size_t>( leDistrict ) < sizeof(KAPC_DISTRICT_NAMES) / sizeof(char*)");

    return gapDistrictNames[luDistrict];
}
}
