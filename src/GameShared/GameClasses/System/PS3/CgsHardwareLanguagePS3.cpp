#include "GameShared/GameClasses/System/CgsHardwareLanguage.h"

// The Windows raw input is provided in CgsHardwareSkuPC.cpp. Keep the existing

extern "C" int XTLGetLanguage();
extern "C" int XGetGameRegion();

namespace CgsSystem
{
const CgsLanguage::ELanguage HardwareLanguage::GetHardwareLanguage()
{
    switch (XTLGetLanguage())
    {
        case 1:
            return static_cast<CgsLanguage::ELanguage>((XGetGameRegion() == 767) + 7);
        case 2:
            return static_cast<CgsLanguage::ELanguage>(16);
        case 3:
            return static_cast<CgsLanguage::ELanguage>(11);
        case 4:
            return static_cast<CgsLanguage::ELanguage>(10);
        case 5:
            return static_cast<CgsLanguage::ELanguage>(22);
        case 6:
            return static_cast<CgsLanguage::ELanguage>(15);
        case 7:
            return static_cast<CgsLanguage::ELanguage>(17);
        case 8:
            return static_cast<CgsLanguage::ELanguage>(1);
        case 9:
            return static_cast<CgsLanguage::ELanguage>(21);
        default:
            return static_cast<CgsLanguage::ELanguage>(7);
    }
}
}
