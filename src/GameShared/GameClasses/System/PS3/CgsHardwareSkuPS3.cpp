#include "GameShared/GameClasses/System/CgsHardwareSku.h"
#include "GameShared/GameClasses/System/CgsHardwareLanguage.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsSystem
{
static const char KAC_ARTIST_HARDWARE_SKU_SOURCE[] =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\X360/CgsHardwareSkuX360.cpp";

// ARTIST 0x828D7128: retain every SKU branch and its original language allowlist.
const CgsLanguage::ELanguage HardwareSku::FindLanguage()
{
    const s32 liSku = static_cast<s32>(HardwareSku::GetSku());
    s32 liLanguage = HardwareLanguage::GetHardwareLanguage();

    switch (liSku)
    {
        case 0:
        case 4:
        case 5:
            return static_cast<CgsLanguage::ELanguage>(7);

        case 1:
            return static_cast<CgsLanguage::ELanguage>(8);

        case 2:
            switch (liLanguage)
            {
                case 10:
                case 11:
                case 15:
                case 22:
                    return static_cast<CgsLanguage::ELanguage>(liLanguage);
                default:
                    return static_cast<CgsLanguage::ELanguage>(10);
            }

        case 3:
            return static_cast<CgsLanguage::ELanguage>(16);

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
                    return static_cast<CgsLanguage::ELanguage>(liLanguage);
                default:
                    return static_cast<CgsLanguage::ELanguage>(7);
            }

        default:
            CGS_FAIL_AT("Unknown sku or no sku set", KAC_ARTIST_HARDWARE_SKU_SOURCE, 208);
            return static_cast<CgsLanguage::ELanguage>(7);
    }
}
}
