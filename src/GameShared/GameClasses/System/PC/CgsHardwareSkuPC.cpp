#include "GameShared/GameClasses/System/CgsHardwareSku.h"
#include "GameShared/GameClasses/System/CgsHardwareLanguage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <Windows.h>
#include <mbstring.h>

namespace CgsSystem
{
    namespace
    {
        struct LocaleLanguage
        {
            const char* mpacLocale;
            s32 miLanguage;
        };

        // TUB WinMain @ 0x79D580 assigns these before HardwareInit::InitializeHardware:
        // lpSubKey at 0x79D5AA and lpValueName at 0x79D5B4.
        const char* const kLocaleRegistrySubKey =
            "SOFTWARE\\EA Games\\Burnout(TM) Paradise The Ultimate Box\\";
        const char* const kLocaleRegistryValueName = "locale";

        // TUB data at off_A5B938 / dword_A5B93C: 23 exact locale-to-ID pairs.
        const LocaleLanguage kLocaleLanguages[] =
        {
            { "pt_br", 20 },
            { "zh_cn", 2 },
            { "zh_tw", 3 },
            { "cs", 4 },
            { "da", 5 },
            { "nl", 6 },
            { "en_us", 7 },
            { "en_uk", 8 },
            { "fi", 9 },
            { "fr_fr", 10 },
            { "de", 11 },
            { "el", 12 },
            { "hu", 14 },
            { "it", 15 },
            { "ja", 16 },
            { "ko", 17 },
            { "pl", 19 },
            { "pt_pt", 21 },
            { "ru", 12 },
            { "es", 22 },
            { "sv", 23 },
            { "no", 18 },
            { "th", 24 },
        };

        // TUB getLocaleFromRegistry @ 0x838800, including its cached English-US fallback,
        // 64-byte registry read, and case-insensitive multibyte comparison.
        s32 GetLocaleFromRegistry()
        {
            static s32 liCachedLocale = -1;
            if (liCachedLocale != -1)
                return liCachedLocale;

            liCachedLocale = 7;
            char lacLocale[64] = {};
            DWORD luDataSize = sizeof(lacLocale);
            HKEY hKey = nullptr;

            if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, kLocaleRegistrySubKey, 0,
                              KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS)
            {
                RegQueryValueExA(hKey, kLocaleRegistryValueName, nullptr, nullptr,
                                 reinterpret_cast<LPBYTE>(lacLocale), &luDataSize);
                RegCloseKey(hKey);
                lacLocale[sizeof(lacLocale) - 1] = '\0';
            }

            for (const LocaleLanguage& lLocale : kLocaleLanguages)
            {
                if (_mbsicmp(reinterpret_cast<const unsigned char*>(lacLocale),
                             reinterpret_cast<const unsigned char*>(lLocale.mpacLocale)) == 0)
                {
                    liCachedLocale = lLocale.miLanguage;
                    break;
                }
            }

            return liCachedLocale;
        }
    }

    // FLAG PC-platform leaf: this port explicitly uses the target's E_SKU_DEFAULT
    // product policy. This is not a recovered PC GetSku body or a mapping from
    // TUB's different, unidentified literal SKU 11.
    const CgsLanguage::ESku HardwareSku::GetSku()
    {
        return CgsLanguage::E_SKU_DEFAULT;
    }

    // FLAG PC-platform leaf: use the attested TUB installer locale query as the
    // Windows raw hardware-language input. Keep its raw IDs, including 24; the
    // unchanged ARTIST HardwareSku::FindLanguage policy selects supported assets.
    // Do not apply TUB getLocaleThing's different supported-language allowlist.
    const CgsLanguage::ELanguage HardwareLanguage::GetHardwareLanguage()
    {
        return static_cast<CgsLanguage::ELanguage>(GetLocaleFromRegistry());
    }

    // ARTIST 0x828D7128: retain the complete selector and its original call order.
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
                CGS_ASSERT(false, "Unknown sku or no sku set");
                return static_cast<CgsLanguage::ELanguage>(7);
        }
    }
}
