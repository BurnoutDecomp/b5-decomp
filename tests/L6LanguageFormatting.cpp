// L6 (owner list 2026-09-28): the Driver Details panel's distances and money read in the wrong units / separators.
//
// The PRODUCTION body of
//   CgsLanguage::LanguageManager::PrepareFormattingStrings @0x82865B70
// and its anonymous-namespace ids / factors are extracted by run_l6_language_formatting.py into
// l6_language_formatting.inc and run on a fixture carrying the manager's members by their production
// names, whose FindString resolves an id exactly as the game does (the production
// CgsContainers::CgsHash::CalculateHash over the id, looked up in the loaded table). The table is the REAL
// LANGUAGE bundle the PC loads, parsed here from disk: 0002.bundle (language 8, English) and 0003.bundle
// (language 10, French).
//
// Every expectation is the ARTIST asm of 0x82865B70 (dumped 2026-09-28) applied to those tables:
//   the 21 string members take the table's string for their id (+0x6100 .. +0x6150);
//   DISTANCE_FORMAT_ISMETRIC '0' (English) -> the IMPERIAL templates, factors flt_820E6B74 = 0x3A22E385 and
//     flt_820E60D4 = 0x3F8BFB85, metric flag 0 (@0x82866000..0x82866020);
//   '1' (French) -> the metric templates, flt_82013F90 = 0x3A83126F and 1.0, flag 1 (@0x82865F88..0x82865FA8);
//   anything else -> with a QA mode on, the metric templates and no factor / flag store (0x828660F4..);
//     otherwise the cpp:2407 assert, templates / factors / flag left as they were.
// The pre-fix tree has no PrepareFormattingStrings at all: the numeric test cannot build there.
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsHash.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0;
static std::string gLastAssert;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int) { ++gAsserts; gLastAssert = lpcMessage ? lpcMessage : ""; return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; }
}

namespace CgsLanguage
{
    struct DebugComponentStandIn
    {
        bool mbShowKeys = false;
        bool mbShowLocalisedTextAsStars = false;
        bool ShowKeysOnly() const             { return mbShowKeys; }
        bool ShowLocalisedTextAsStars() const { return mbShowLocalisedTextAsStars; }
    };

    // The members PrepareFormattingStrings touches, by their production names and types.
    class LanguageManager
    {
    public:
        bool mbIsUsingMetricUnits      = true;
        f32  mrLargeDistanceConversion = -1.0f;
        f32  mrSmallDistanceConversion = -1.0f;
        const u8* mpGeneralDecimalSeparator   = nullptr;
        const u8* mpGeneralThousandsSeparator = nullptr;
        const u8* mpGeneralPercentage         = nullptr;
        const u8* mpGeneralXOverY             = nullptr;
        const u8* mpGeneralCurrencySeparator  = nullptr;
        const u8* mpGeneralCurrency           = nullptr;
        const u8* mpTimeFormatDate            = nullptr;
        const u8* mpTimeFormatAll             = nullptr;
        const u8* mpTimeFormatHrsMinsSecs     = nullptr;
        const u8* mpTimeFormatMinsSecsHnds    = nullptr;
        const u8* mpTimeFormatMinsSecs        = nullptr;
        const u8* mpTimeFormatSecsHnds        = nullptr;
        const u8* mpTimeFormatSecs            = nullptr;
        const u8* mpTimeFormatSecsLong        = nullptr;
        const u8* mpTimeFormatMinSecsMidText  = nullptr;
        const u8* mpTimeFormatMinsSecsMidText = nullptr;
        const u8* mpDistanceFormatShort       = nullptr;
        const u8* mpDistanceFormatShortL      = nullptr;
        const u8* mpDistanceFormatLong        = nullptr;
        const u8* mpDistanceFormatLongL       = nullptr;
        const u8* mpDistanceFormatIsMetric    = nullptr;
        DebugComponentStandIn mDebugComponent;

        // The loaded table: {hash, string}, as LoadStringTable installs it.
        std::vector<std::pair<u32, std::string> > mTable;

        // FindString @0x828646A0's normal branch: hash the key, look it up.
        const u8* FindString(const char* lpcKey) const
        {
            const unsigned int luHash = CgsContainers::CgsHash::CalculateHash(
                const_cast<char*>(lpcKey), static_cast<int>(std::strlen(lpcKey)));
            for (size_t li = 0; li < mTable.size(); ++li)
                if (mTable[li].first == luHash)
                    return reinterpret_cast<const u8*>(mTable[li].second.c_str());
            return nullptr;
        }

        void PrepareFormattingStrings();
    };
}

#include "l6_language_formatting_paths.inc"   // KAC_BUNDLE_ENGLISH / KAC_BUNDLE_FRENCH
#include "l6_language_formatting.inc"         // the production ids, factors and body

using CgsLanguage::LanguageManager;

static void Check(bool lbPass, const char* lpcWhat)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lpcWhat);
    }
}

// The bnd2 language bundle as the PC converter writes it: data block 0 at the header's +0x18, the
// LanguageResource there {u32 id, s32 count, u64 entries}, entries {u64 hash, u64 string} relative to block 0.
static bool LoadBundle(const char* lpcPath, LanguageManager& lrManager, u32& lruLanguage)
{
    FILE* lpFile = std::fopen(lpcPath, "rb");
    if (lpFile == nullptr)
        return false;
    std::vector<u8> lData;
    u8 lacChunk[65536];
    size_t luRead;
    while ((luRead = std::fread(lacChunk, 1, sizeof(lacChunk), lpFile)) > 0)
        lData.insert(lData.end(), lacChunk, lacChunk + luRead);
    std::fclose(lpFile);
    if (lData.size() < 0x80 || std::memcmp(lData.data(), "bnd2", 4) != 0)
        return false;
    u32 luBase;
    std::memcpy(&luBase, &lData[0x18], 4);
    u32 luCount;
    u64 luEntries;
    std::memcpy(&lruLanguage, &lData[luBase], 4);
    std::memcpy(&luCount, &lData[luBase + 4], 4);
    std::memcpy(&luEntries, &lData[luBase + 8], 8);
    lrManager.mTable.clear();
    for (u32 li = 0; li < luCount; ++li)
    {
        u64 luHash, luString;
        std::memcpy(&luHash, &lData[luBase + luEntries + 16u * li], 8);
        std::memcpy(&luString, &lData[luBase + luEntries + 16u * li + 8], 8);
        const char* lpcString = reinterpret_cast<const char*>(&lData[luBase + luString]);
        lrManager.mTable.push_back(std::make_pair(static_cast<u32>(luHash), std::string(lpcString)));
    }
    return luCount > 4000;
}

static void CheckString(const u8* lpValue, const char* lpcWant, const char* lpcWhat)
{
    char lac[256];
    std::snprintf(lac, sizeof(lac), "%s = '%s' (got '%s')", lpcWhat, lpcWant,
                  lpValue ? reinterpret_cast<const char*>(lpValue) : "(null)");
    Check(lpValue != nullptr && std::strcmp(reinterpret_cast<const char*>(lpValue), lpcWant) == 0, lac);
}

static void CheckBits(f32 lfValue, u32 luBits, const char* lpcWhat)
{
    u32 luGot;
    std::memcpy(&luGot, &lfValue, 4);
    char lac[256];
    std::snprintf(lac, sizeof(lac), "%s = 0x%08X (got 0x%08X, %.9g)", lpcWhat, luBits, luGot, lfValue);
    Check(luGot == luBits, lac);
}

int main()
{
    // ---- English (LANGUAGE/0002.bundle, the table the PC loads) -----------------------------------
    {
        static LanguageManager sManager;
        u32 luLanguage = 0;
        Check(LoadBundle(KAC_BUNDLE_ENGLISH, sManager, luLanguage), "LANGUAGE/0002.bundle parses (4707 entries)");
        Check(luLanguage == 8, "0002.bundle is language 8");
        gAsserts = 0;
        sManager.PrepareFormattingStrings();
        Check(gAsserts == 0, "English: no assert (every id is in the table)");
        CheckString(sManager.mpGeneralDecimalSeparator,   ".",               "English +0x6100 GENERAL_DECIMAL_SEPARATOR");
        CheckString(sManager.mpGeneralThousandsSeparator, ",",               "English +0x6104 GENERAL_THOUSANDS_SEPARATOR");
        CheckString(sManager.mpGeneralPercentage,         "%1%",             "English +0x6108 GENERAL_PERCENTAGE_DISPLAY");
        CheckString(sManager.mpGeneralXOverY,             "%1/%2",           "English +0x610C GENERAL_X_OVER_Y_DISPLAY");
        CheckString(sManager.mpGeneralCurrencySeparator,  ",",               "English +0x6110 GENERAL_CURRENCY_SEPARATOR (the default is '.')");
        CheckString(sManager.mpGeneralCurrency,           "$%1",             "English +0x6114 GENERAL_CURRENCY_NO_DECIMAL");
        CheckString(sManager.mpTimeFormatDate,            "%2/%1/%3",        "English +0x6118 TIME_FORMAT_DATE");
        CheckString(sManager.mpTimeFormatAll,             "%1:%2",           "English +0x611C TIME_FORMAT_ALL");
        CheckString(sManager.mpTimeFormatHrsMinsSecs,     "%1:%2:%3",        "English +0x6120 TIME_FORMAT_HRS_MINS_SECS");
        CheckString(sManager.mpTimeFormatMinsSecsHnds,    "%1:%2.%3",        "English +0x6124 TIME_FORMAT_MINS_SECS_HNDS");
        CheckString(sManager.mpTimeFormatMinsSecs,        "%1:%2",           "English +0x6128 TIME_FORMAT_MINS_SECS");
        CheckString(sManager.mpTimeFormatSecsHnds,        "%1.%2",           "English +0x612C TIME_FORMAT_SECS_HNDS");
        CheckString(sManager.mpTimeFormatSecs,            "%1",              "English +0x6130 TIME_FORMAT_SECS");
        CheckString(sManager.mpTimeFormatSecsLong,        "%1 Seconds",      "English +0x6134 TIME_FORMAT_SECS_L");
        CheckString(sManager.mpTimeFormatMinSecsMidText,  "1 Min %1 Secs",   "English +0x6138 TIME_FORMAT_MIN_SECS_MID_TEXT");
        CheckString(sManager.mpTimeFormatMinsSecsMidText, "%1 Min %2 Secs",  "English +0x613C TIME_FORMAT_MINS_SECS_MID_TEXT");
        CheckString(sManager.mpDistanceFormatIsMetric,    "0",               "English +0x6150 DISTANCE_FORMAT_ISMETRIC");
        CheckString(sManager.mpDistanceFormatShort,       "%1 yds",          "English +0x6140 = DISTANCE_FORMAT_SHORT_IMPERIAL");
        CheckString(sManager.mpDistanceFormatShortL,      "%1 Yards",        "English +0x6144 = DISTANCE_FORMAT_SHORT_IMPERIAL_L");
        CheckString(sManager.mpDistanceFormatLong,        "%1 mi",           "English +0x6148 = DISTANCE_FORMAT_LONG_IMPERIAL");
        CheckString(sManager.mpDistanceFormatLongL,       "%1 Miles",        "English +0x614C = DISTANCE_FORMAT_LONG_IMPERIAL_L");
        Check(!sManager.mbIsUsingMetricUnits, "English: mbIsUsingMetricUnits = 0 (stb r10=0 @0x82866018)");
        CheckBits(sManager.mrLargeDistanceConversion, 0x3A22E385u, "English mrLargeDistanceConversion (flt_820E6B74, metres -> miles)");
        CheckBits(sManager.mrSmallDistanceConversion, 0x3F8BFB85u, "English mrSmallDistanceConversion (flt_820E60D4, metres -> yards)");
    }

    // ---- French (LANGUAGE/0003.bundle): the flag follows the table, it is not forced -------------
    {
        static LanguageManager sManager;
        u32 luLanguage = 0;
        Check(LoadBundle(KAC_BUNDLE_FRENCH, sManager, luLanguage), "LANGUAGE/0003.bundle parses");
        Check(luLanguage == 10, "0003.bundle is language 10");
        sManager.mbIsUsingMetricUnits = false;
        gAsserts = 0;
        sManager.PrepareFormattingStrings();
        Check(gAsserts == 0, "French: no assert");
        CheckString(sManager.mpDistanceFormatIsMetric,   "1",                   "French DISTANCE_FORMAT_ISMETRIC");
        CheckString(sManager.mpGeneralCurrencySeparator, " ",                   "French GENERAL_CURRENCY_SEPARATOR");
        CheckString(sManager.mpGeneralDecimalSeparator,  ",",                   "French GENERAL_DECIMAL_SEPARATOR");
        CheckString(sManager.mpDistanceFormatShort,      "%1 m",                "French +0x6140 = DISTANCE_FORMAT_SHORT");
        CheckString(sManager.mpDistanceFormatShortL,     "%1 m\xC3\xA8tre(s)",  "French +0x6144 = DISTANCE_FORMAT_SHORT_L");
        CheckString(sManager.mpDistanceFormatLong,       "%1 km",               "French +0x6148 = DISTANCE_FORMAT_LONG");
        CheckString(sManager.mpDistanceFormatLongL,      "%1 km",               "French +0x614C = DISTANCE_FORMAT_LONG_L");
        Check(sManager.mbIsUsingMetricUnits, "French: mbIsUsingMetricUnits = 1 (stb r10=1 @0x82865FA0)");
        CheckBits(sManager.mrLargeDistanceConversion, 0x3A83126Fu, "French mrLargeDistanceConversion (flt_82013F90)");
        CheckBits(sManager.mrSmallDistanceConversion, 0x3F800000u, "French mrSmallDistanceConversion (flt_82001C98)");
    }

    // ---- a table whose switch is neither '0' nor '1' ---------------------------------------------
    {
        static LanguageManager sManager;
        u32 luLanguage = 0;
        LoadBundle(KAC_BUNDLE_ENGLISH, sManager, luLanguage);
        const unsigned int luMetricHash = CgsContainers::CgsHash::CalculateHash(
            const_cast<char*>("DISTANCE_FORMAT_ISMETRIC"), 24);
        for (size_t li = 0; li < sManager.mTable.size(); ++li)
            if (sManager.mTable[li].first == luMetricHash)
                sManager.mTable[li].second = "2";

        const u8 lau8Sentinel[] = "sentinel";
        sManager.mpDistanceFormatShort = sManager.mpDistanceFormatShortL = lau8Sentinel;
        sManager.mpDistanceFormatLong = sManager.mpDistanceFormatLongL = lau8Sentinel;
        sManager.mbIsUsingMetricUnits = false;
        sManager.mrLargeDistanceConversion = 5.0f;
        sManager.mrSmallDistanceConversion = 6.0f;
        gAsserts = 0;
        sManager.PrepareFormattingStrings();
        Check(gAsserts == 1 && gLastAssert == "DISTANCE_FORMAT_ISMETRIC needs to be set to either 0 or 1",
              "'2': the cpp:2407 assert fires, once");
        Check(sManager.mpDistanceFormatShort == lau8Sentinel && sManager.mpDistanceFormatLongL == lau8Sentinel,
              "'2': the distance templates are left as they were");
        Check(!sManager.mbIsUsingMetricUnits && sManager.mrLargeDistanceConversion == 5.0f &&
              sManager.mrSmallDistanceConversion == 6.0f, "'2': flag and factors are left as they were");

        sManager.mDebugComponent.mbShowKeys = true;
        gAsserts = 0;
        sManager.PrepareFormattingStrings();
        Check(gAsserts == 0, "'2' with ShowKeysOnly: no assert (0x828660F4 returns before them)");
        CheckString(sManager.mpDistanceFormatShort,  "%1 m",          "'2' with ShowKeysOnly: +0x6140 = DISTANCE_FORMAT_SHORT");
        CheckString(sManager.mpDistanceFormatLongL,  "%1 Kilometers", "'2' with ShowKeysOnly: +0x614C = DISTANCE_FORMAT_LONG_L");
        Check(!sManager.mbIsUsingMetricUnits && sManager.mrLargeDistanceConversion == 5.0f,
              "'2' with ShowKeysOnly: flag and factors still untouched");
    }

    std::printf("L6LanguageFormatting: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
