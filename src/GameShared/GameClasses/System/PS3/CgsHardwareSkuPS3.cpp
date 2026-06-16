#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsSystem
{
class HardwareLanguage
{
public:
    static s32 GetHardwareLanguage();
};

class HardwareSku
{
public:
    static s32 GetSku();
    static s32 FindLanguage();
};

s32 HardwareSku::FindLanguage()
{
    const s32 liSku = HardwareSku::GetSku();
    s32 liLanguage = HardwareLanguage::GetHardwareLanguage();

    switch (liSku)
    {
        case 0:
        case 4:
        case 5:
            return 7;

        case 1:
            return 8;

        case 2:
            switch (liLanguage)
            {
                case 10:
                case 11:
                case 15:
                case 22:
                    return liLanguage;
                default:
                    return 10;
            }

        case 3:
            return 16;

        case 6:
            switch (liLanguage)
            {
                case 7:
                case 8:
                case 10:
                case 11:
                case 15:
                case 16:
                case 22:
                    return liLanguage;
                default:
                    return 7;
            }

        default:
            CGS_ASSERT(false, "Unknown sku or no sku set");
            return 7;
    }
}
}
