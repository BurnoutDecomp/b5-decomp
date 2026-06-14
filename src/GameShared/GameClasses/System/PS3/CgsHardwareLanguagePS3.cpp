#include "types.hpp"

extern "C" int XTLGetLanguage();
extern "C" int XGetGameRegion();

namespace CgsSystem
{
class HardwareLanguage
{
public:
    static int GetHardwareLanguage();
};

int HardwareLanguage::GetHardwareLanguage()
{
    switch (XTLGetLanguage())
    {
        case 1:
            return (XGetGameRegion() == 767) + 7;
        case 2:
            return 16;
        case 3:
            return 11;
        case 4:
            return 10;
        case 5:
            return 22;
        case 6:
            return 15;
        case 7:
            return 17;
        case 8:
            return 1;
        case 9:
            return 21;
        default:
            return 7;
    }
}
}
