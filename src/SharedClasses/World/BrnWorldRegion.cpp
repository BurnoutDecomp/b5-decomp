#include "types.hpp"

namespace CgsDev
{
class Assert
{
public:
    static void BeginAssert();
    static void FireAssert(const char* lpcExpression, const char* lpcFile, int liLine);
    static void EndAssert();
};
}

namespace BrnWorld::WorldRegion
{
extern const char* gapCountyNames[6];
extern const char* gapDistrictNames[19];

const char* CountyToString(u32 luCounty)
{
    if (luCounty >= 6)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("leCounty >= 0 && leCounty < E_COUNTY_COUNT", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../../SharedClasses/World/BrnWorldRegion.cpp", 171);
        CgsDev::Assert::EndAssert();
    }

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
    if (luDistrict >= 19)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("static_cast<size_t>( leDistrict ) < sizeof(KAPC_DISTRICT_NAMES) / sizeof(char*)", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../../SharedClasses/World/BrnWorldRegion.cpp", 188);
        CgsDev::Assert::EndAssert();
    }

    return gapDistrictNames[luDistrict];
}
}
