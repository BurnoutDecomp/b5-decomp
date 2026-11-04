#include "GameShared/GameClasses/System/CgsHardwareSku.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <xtl.h>

namespace CgsSystem
{
    // ARTIST 828D6F80. Resolve the disc SKU once from the console region and stream headers.
    const CgsLanguage::ESku HardwareSku::GetSku()
    {
        static bool sbSkuSet = false;
        static CgsLanguage::ESku seSku = CgsLanguage::E_SKU_DEFAULT;
        if (sbSkuSet)
            return seSku;

        CgsLanguage::ESku leSku;
        const char* lpcMessage;
        switch (XGetGameRegion())
        {
        case 0x2FF:
        case 0x201:
        case 0x2FE:
            if (GetFileAttributesA("D:\\SOUND\\STREAMS\\STREAMHEADERS_FR.BUNDLE") != static_cast<DWORD>(-1))
            {
                leSku = CgsLanguage::E_SKU_EURO_2;
                lpcMessage = "***** SKU: EURO 1 (FIGS)\n";
            }
            else
            {
                leSku = CgsLanguage::E_SKU_EURO_1;
                lpcMessage = "***** SKU: EURO 2 (UK PAL ENG)\n";
            }
            break;
        case 0xFF:
            leSku = CgsLanguage::E_SKU_US;
            lpcMessage = "***** SKU: US\n";
            break;
        case 0x1F8:
        case 0x104:
        case 0x3FF:
            leSku = CgsLanguage::E_SKU_ROA;
            lpcMessage = "***** SKU: ROA\n";
            break;
        default:
            if (GetFileAttributesA("D:\\SOUND\\STREAMS\\STREAMHEADERS_JP.BUNDLE") != static_cast<DWORD>(-1))
            {
                leSku = CgsLanguage::E_SKU_JAPAN;
                lpcMessage = "***** SKU: JAPAN\n";
            }
            else
            {
                leSku = CgsLanguage::E_SKU_ROA;
                lpcMessage = "***** SKU: ROA\n";
            }
            break;
        }
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << lpcMessage;
        seSku = leSku;
        sbSkuSet = true;
        return seSku;
    }
}
