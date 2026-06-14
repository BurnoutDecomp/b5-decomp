#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsSystem::HardwareSku::FindLanguage @ 0x828D7128
//
// Picks the game language for the current hardware: starts from the detected hardware
// language and constrains it to the set valid for the console's SKU/region, falling back
// to language 7 (the default) when the detected language is not offered in that region.
// SKU and language values are kept as raw ints to match the sibling CgsHardwareLanguagePS3
// reconstruction (no ESku/ELanguage enum is recovered in the X360 ledger).

namespace CgsSystem
{
class HardwareLanguage
{
public:
    static int GetHardwareLanguage();
};

class HardwareSku
{
public:
    static int GetSku();        // other TU; declared for the compile gate
    static int FindLanguage();
};
}

namespace CgsDev
{
namespace Assert
{
    void BeginAssert();
    void FireAssert(const char* pacMessage, const char* pacFile, int liLine);
    void EndAssert();
}
}

namespace CgsSystem
{
int HardwareSku::FindLanguage()
{
    const int liSku      = GetSku();
    const int liLanguage = HardwareLanguage::GetHardwareLanguage();

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
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "Unknown sku or no sku set",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\X360/CgsHardwareSkuX360.cpp",
                208);
            CgsDev::Assert::EndAssert();
            return 7;
    }
}
}
