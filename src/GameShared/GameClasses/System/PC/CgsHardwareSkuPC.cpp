#include "types.hpp"

// PC SKU / language detection. The console builds read the system language from the SKU
// (CgsHardwareSkuPS3.cpp); on PC the original reads the "locale" registry value written by
// the installer. Until that registry read is reconstructed this returns English, which is
// what the loading-screen renderer maps to its English text strip.
namespace CgsSystem
{
    namespace HardwareSku
    {
        s32 FindLanguage()
        {
            return 0;   // ELoadingLanguage English
        }
    }
}
