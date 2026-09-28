// L6 (owner list 2026-09-28): "Lot of Driver details are missing or wrong" -- THE DRIVER DETAILS STAT PANEL.
//
// The PRODUCTION body of
//   CrashNavDriverDetails::HandleStatData @0x824B8618
// is extracted from the b5 sources by run_l6_driver_details_stats.py into l6_driver_details_stats.inc and
// hosted on a fixture that carries the screen's 33 stat fields and 3 x 5 district fields as recording
// TextFields (the production SetLocalisedText overload set, so every call binds as it does in the game).
// The record is the REAL GuiEventStatsResponse (BrnGuiEventStatsResponse.h), filled BY CONSOLE OFFSET.
//
// Every expectation is the ARTIST asm of 0x824B8618 (dumped 2026-09-28): the field is this+0x121C+0x128*i
// (districts this+0x38 / +0x600 / +0xBC8 + 0x128*d), the record load, the format id and the key string
// (@0x820665EC "CV_PANEL_EVENTS_SCORE", @0x82066598 "STAT_BURNOUTS", @0x820665B8 "STAT_DEGREES",
// @0x820665C8 "x %1", @0x82066604 "CV_PANEL_EVENTS_TD_COUNT", "%d" @0x8202E7C0). The two floats:
//   field 24 bestAirTime_cpt  `lfs f1, 0xF4(r31)` @0x824B8968 -> SetLocalisedText(f32, 5)
//   field 25 bestSpin_cpt     `lfs f0, 0xF8(r31) ; fctiwz ; stfiwx` @0x824B8978 -> "%d" of the truncation
// The pre-fix body read both through an s32 byte cursor and FAILS those two checks.
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/Gui/Events/BrnGuiEventStatsResponse.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

static unsigned gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; }
}

// ---- the language manager: the production enum (extracted by the runner), X/Y as "X/Y" ----------
namespace CgsLanguage
{
    class LanguageManager
    {
    public:
#include "l6_driver_details_format_enum.inc"
        void FormatXoverYString(char* lpcTarget, s32 liX, s32 liY, s32 liTargetSize) const
        {
            std::snprintf(lpcTarget, static_cast<size_t>(liTargetSize), "%d/%d", liX, liY);
        }
    };
}

namespace CgsCore
{
    void SnPrintf(char* buffer, u32 len, const char* fmt, ...)
    {
        va_list lArgs;
        va_start(lArgs, fmt);
        std::vsnprintf(buffer, len, fmt, lArgs);
        va_end(lArgs);
    }
}

namespace BrnGui
{
    typedef CgsLanguage::LanguageManager::ParameterFormatType PFT;

    enum ECallKind { K_NONE, K_TEXT, K_INT, K_FLOAT, K_KEY_INT, K_KEY_FLOAT, K_KEY_PARAMS, K_KEY_ARRAY };

    // A TextField that records its SetLocalisedText calls (the production overload set, same order).
    class TextField
    {
    public:
        s32   miCalls = 0;
        s32   meKind = K_NONE;
        char  macText[128] = {};
        s32   miValue = 0;
        f32   mfValue = 0.0f;
        s32   meFormat = -1;
        s32   meValueFormat = -1;
        s32   miNumParams = 0;
        char  macParam[128] = {};

        bool SetLocalisedText(const char* lpacText, PFT leFormat)
        { Note(K_TEXT, lpacText, leFormat); return true; }
        bool SetLocalisedText(s32 liValue, PFT leFormat)
        { Note(K_INT, "", leFormat); miValue = liValue; return true; }
        bool SetLocalisedText(f32 lfValue, PFT leFormat)
        { Note(K_FLOAT, "", leFormat); mfValue = lfValue; return true; }
        bool SetLocalisedText(const char* lpacText, PFT leFormat, s32 liValue, PFT leValueFormat)
        { Note(K_KEY_INT, lpacText, leFormat); miValue = liValue; meValueFormat = leValueFormat; return true; }
        bool SetLocalisedText(const char* lpacText, PFT leFormat, f32 lfValue, PFT leValueFormat)
        { Note(K_KEY_FLOAT, lpacText, leFormat); mfValue = lfValue; meValueFormat = leValueFormat; return true; }
        bool SetLocalisedText(const char* lpacText, PFT leFormat, s32 liNumParams, ...)
        {
            Note(K_KEY_PARAMS, lpacText, leFormat);
            miNumParams = liNumParams;
            va_list lArgs;
            va_start(lArgs, liNumParams);
            if (liNumParams >= 1)
            {
                const char* lpcParam = va_arg(lArgs, const char*);
                std::snprintf(macParam, sizeof(macParam), "%s", lpcParam ? lpcParam : "(null)");
                meValueFormat = va_arg(lArgs, int);
            }
            va_end(lArgs);
            return true;
        }
        bool SetLocalisedText(const char* lpacText, PFT leFormat, s32 liNumParams,
                              const char* const*, const PFT*)
        { Note(K_KEY_ARRAY, lpacText, leFormat); miNumParams = liNumParams; return true; }

    private:
        void Note(s32 leKind, const char* lpacText, PFT leFormat)
        {
            ++miCalls;
            meKind = leKind;
            meFormat = leFormat;
            std::snprintf(macText, sizeof(macText), "%s", lpacText ? lpacText : "(null)");
        }
    };

    struct GuiCache {};

    struct StateInterfaceStandIn
    {
        CgsLanguage::LanguageManager mLanguageManager;
        CgsLanguage::LanguageManager* GetLanguageManager() { return &mLanguageManager; }
    };

    // The members HandleStatData touches, by their production names.
    class CrashNavDriverDetails
    {
    public:
        static const s32 KI_NUM_STAT_TEXTFIELDS = 33;
        static const s32 KI_NUM_DISTRICTS       = 5;

        TextField maDistrictBillboardsTextfields[KI_NUM_DISTRICTS];
        TextField maDistrictJumpsTextfields[KI_NUM_DISTRICTS];
        TextField maDistrictSmashesTextfields[KI_NUM_DISTRICTS];
        TextField maStatTextfields[KI_NUM_STAT_TEXTFIELDS];
        StateInterfaceStandIn* mpStateInterface = nullptr;
        GuiCache*              mpGuiCache       = nullptr;

        void HandleStatData(const GuiEventStatsResponse* lpStatsEvent);
    };
}

// The production body under test (+ the pre-fix file-local byte cursor, when the revision has one).
#include "l6_driver_details_stats.inc"

using namespace BrnGui;

static void Check(bool lbPass, const char* lpcWhat)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lpcWhat);
    }
}

// The record, filled by CONSOLE OFFSET: every word gets a distinct value, the two floats their own.
static const f32 KF_AIRTIME = 7.433375835418701f;    // the owner's slot-0 copy: Profile+0x260 (image 0x250)
static const f32 KF_SPIN    = 373.14398193359375f;   // Profile+0x250 (image 0x240)

static s32 WordValue(u32 luOffset) { return static_cast<s32>(luOffset * 7u + 3u); }

static s32 ReadWord(const GuiEventStatsResponse& lrEvent, u32 luOffset)
{
    s32 li;
    std::memcpy(&li, reinterpret_cast<const u8*>(&lrEvent) + luOffset, 4);
    return li;
}

static void ExpectText(const TextField& lrField, const char* lpcName, s32 liX, s32 liY)
{
    char lacWant[64];
    std::snprintf(lacWant, sizeof(lacWant), "%d/%d", liX, liY);
    char lacWhat[256];
    std::snprintf(lacWhat, sizeof(lacWhat), "%s: one TEXT call, '%s', format 0 (got kind %d '%s' format %d, %d call(s))",
                  lpcName, lacWant, lrField.meKind, lrField.macText, lrField.meFormat, lrField.miCalls);
    Check(lrField.miCalls == 1 && lrField.meKind == K_TEXT && std::strcmp(lrField.macText, lacWant) == 0 &&
          lrField.meFormat == 0, lacWhat);
}

static void ExpectInt(const TextField& lrField, const char* lpcName, s32 liValue, s32 leFormat)
{
    char lacWhat[256];
    std::snprintf(lacWhat, sizeof(lacWhat), "%s: one INT call, %d, format %d (got kind %d %d format %d)",
                  lpcName, liValue, leFormat, lrField.meKind, lrField.miValue, lrField.meFormat);
    Check(lrField.miCalls == 1 && lrField.meKind == K_INT && lrField.miValue == liValue &&
          lrField.meFormat == leFormat, lacWhat);
}

static void ExpectFloat(const TextField& lrField, const char* lpcName, f32 lfValue, s32 leFormat)
{
    char lacWhat[256];
    std::snprintf(lacWhat, sizeof(lacWhat), "%s: one FLOAT call, %.9g, format %d (got kind %d %.9g format %d)",
                  lpcName, lfValue, leFormat, lrField.meKind, lrField.mfValue, lrField.meFormat);
    Check(lrField.miCalls == 1 && lrField.meKind == K_FLOAT &&
          std::memcmp(&lrField.mfValue, &lfValue, 4) == 0 && lrField.meFormat == leFormat, lacWhat);
}

static void ExpectKeyParam(const TextField& lrField, const char* lpcName, const char* lpcKey, s32 leFormat,
                           const char* lpcParam)
{
    char lacWhat[320];
    std::snprintf(lacWhat, sizeof(lacWhat),
                  "%s: one KEY+PARAMS call, '%s' format %d, 1 param '%s' format 11 (got kind %d '%s' %d, %d '%s' %d)",
                  lpcName, lpcKey, leFormat, lpcParam, lrField.meKind, lrField.macText, lrField.meFormat,
                  lrField.miNumParams, lrField.macParam, lrField.meValueFormat);
    Check(lrField.miCalls == 1 && lrField.meKind == K_KEY_PARAMS && std::strcmp(lrField.macText, lpcKey) == 0 &&
          lrField.meFormat == leFormat && lrField.miNumParams == 1 && std::strcmp(lrField.macParam, lpcParam) == 0 &&
          lrField.meValueFormat == 11, lacWhat);
}

#define CHECK_OFFSET(member, offset) \
    Check(offsetof(GuiEventStatsResponse, member) == (offset), "GuiEventStatsResponse::" #member " sits at console +" #offset)

int main()
{
    // ---- the record layout the by-name reads rely on (the console offsets HandleStatData loads) ----
    Check(sizeof(GuiEventStatsResponse) == 432, "GuiEventStatsResponse is 432 bytes (AddGuiEvent li r6, 0x1B0)");
    CHECK_OFFSET(miDistanceOffline, 0x1C);           CHECK_OFFSET(miTimePlayed, 0x20);
    CHECK_OFFSET(miCarsCollected, 0x24);             CHECK_OFFSET(miCarsTotal, 0x28);
    CHECK_OFFSET(miPowerParkingBest, 0x2C);          CHECK_OFFSET(miJumps, 0x68);
    CHECK_OFFSET(miJumpTot, 0x6C);                   CHECK_OFFSET(miSmashes, 0x70);
    CHECK_OFFSET(miSmashTot, 0x74);                  CHECK_OFFSET(miStunts, 0x78);
    CHECK_OFFSET(miStuntTot, 0x7C);                  CHECK_OFFSET(miTotalTakedowns, 0x88);
    CHECK_OFFSET(mRoadsRuledTime, 0xAC);             CHECK_OFFSET(mRoadsRuledCrash, 0xB0);
    CHECK_OFFSET(mRoadsRuledComplete, 0xB4);         CHECK_OFFSET(mNumberOfRoads, 0xB8);
    CHECK_OFFSET(miCarsToShutdown, 0xC0);            CHECK_OFFSET(miPercentageComplete, 0xC4);
    CHECK_OFFSET(miRacesWon, 0xCC);                  CHECK_OFFSET(miRoadRagesWon, 0xD0);
    CHECK_OFFSET(miMarkedManWon, 0xD4);              CHECK_OFFSET(miChallengesWon, 0xD8);
    CHECK_OFFSET(miStuntRunsWon, 0xDC);              CHECK_OFFSET(miBestShowtime, 0xE0);
    CHECK_OFFSET(miBestRoadRageTakedownCount, 0xE4); CHECK_OFFSET(miBestBoostChain, 0xE8);
    CHECK_OFFSET(miBestDrift, 0xEC);                 CHECK_OFFSET(miBestOncoming, 0xF0);
    CHECK_OFFSET(mfBestAirtime, 0xF4);               CHECK_OFFSET(mfBestSpin, 0xF8);
    CHECK_OFFSET(miBestNumBarrelRolls, 0xFC);        CHECK_OFFSET(miHighestStuntScore, 0x100);
    CHECK_OFFSET(miEventsFound, 0x104);              CHECK_OFFSET(miTotalEvents, 0x108);
    CHECK_OFFSET(miBodyShopsFound, 0x10C);           CHECK_OFFSET(miGasStationsFound, 0x110);
    CHECK_OFFSET(miPaintShopsFound, 0x114);          CHECK_OFFSET(miJunkYardsFound, 0x118);
    CHECK_OFFSET(miBodyShopsTotal, 0x11C);           CHECK_OFFSET(miGasStationsTotal, 0x120);
    CHECK_OFFSET(miPaintShopsTotal, 0x124);          CHECK_OFFSET(miJunkYardsTotal, 0x128);
    CHECK_OFFSET(miTotalDriveThrus, 0x12C);          CHECK_OFFSET(miTotalDriveThrusFound, 0x130);
    CHECK_OFFSET(mBillboardStunts, 0x134);           CHECK_OFFSET(mJumpStunts, 0x148);
    CHECK_OFFSET(mSmashStunts, 0x15C);               CHECK_OFFSET(mMaxBillboardStunts, 0x170);
    CHECK_OFFSET(mMaxJumpStunts, 0x184);             CHECK_OFFSET(mMaxSmashStunts, 0x198);

    // ---- the record, by console offset ----
    GuiEventStatsResponse lEvent;
    std::memset(&lEvent, 0, sizeof(lEvent));
    for (u32 luOffset = 0x18; luOffset + 4 <= 0x1AC; luOffset += 4)
    {
        const s32 liValue = WordValue(luOffset);
        std::memcpy(reinterpret_cast<u8*>(&lEvent) + luOffset, &liValue, 4);
    }
    std::memcpy(reinterpret_cast<u8*>(&lEvent) + 0xF4, &KF_AIRTIME, 4);
    std::memcpy(reinterpret_cast<u8*>(&lEvent) + 0xF8, &KF_SPIN, 4);

    StateInterfaceStandIn lStateInterface;
    GuiCache lCache;
    static CrashNavDriverDetails sScreen;
    sScreen.mpStateInterface = &lStateInterface;
    sScreen.mpGuiCache = &lCache;
    sScreen.HandleStatData(&lEvent);

    const TextField* S = sScreen.maStatTextfields;
    const GuiEventStatsResponse& E = lEvent;
    char lacNum[32];

    // ---- the 33 stat fields, the console's sequence 0x824B8680..0x824B8B74 ----
    ExpectFloat(S[0], "hoursPlayed_cpt (lwz 0x20, fcfid)", static_cast<f32>(ReadWord(E, 0x20)), 1);
    ExpectInt  (S[1], "percentageComplete_cpt (lwz 0xC4)", ReadWord(E, 0xC4), 13);
    ExpectFloat(S[2], "totalMileage_cpt (lwz 0x1C, fcfid)", static_cast<f32>(ReadWord(E, 0x1C)), 16);
    ExpectText (S[3], "carsWon_cpt (0x24 / 0x28)", ReadWord(E, 0x24), ReadWord(E, 0x28));
    ExpectInt  (S[4], "carsToShutdown_cpt (lwz 0xC0)", ReadWord(E, 0xC0), 11);
    ExpectText (S[5], "roadsRuledAmount_cpt (0xB4 / 0xB8)", ReadWord(E, 0xB4), ReadWord(E, 0xB8));
    ExpectText (S[6], "roadRulesTime_cpt (0xAC / 0xB8)", ReadWord(E, 0xAC), ReadWord(E, 0xB8));
    ExpectText (S[7], "roadRulesShowtime_cpt (0xB0 / 0xB8)", ReadWord(E, 0xB0), ReadWord(E, 0xB8));
    ExpectText (S[8], "eventsFound_cpt (0x104 / 0x108)", ReadWord(E, 0x104), ReadWord(E, 0x108));
    ExpectText (S[9], "driveThrusFound_cpt (0x130 / 0x12C)", ReadWord(E, 0x130), ReadWord(E, 0x12C));
    ExpectText (S[10], "smashes_cpt (0x70 / 0x74)", ReadWord(E, 0x70), ReadWord(E, 0x74));
    ExpectText (S[11], "stunts_cpt (0x78 / 0x7C)", ReadWord(E, 0x78), ReadWord(E, 0x7C));
    ExpectText (S[12], "jumps_cpt (0x68 / 0x6C)", ReadWord(E, 0x68), ReadWord(E, 0x6C));
    ExpectInt  (S[13], "racesOne_cpt (lwz 0xCC)", ReadWord(E, 0xCC), 11);
    ExpectInt  (S[14], "roadRagesOne_cpt (lwz 0xD0)", ReadWord(E, 0xD0), 11);
    ExpectInt  (S[15], "markedManOne_cpt (lwz 0xD4)", ReadWord(E, 0xD4), 11);
    ExpectInt  (S[16], "challengesOne_cpt (lwz 0xD8)", ReadWord(E, 0xD8), 11);
    ExpectInt  (S[17], "stuntRunsOne_cpt (lwz 0xDC)", ReadWord(E, 0xDC), 11);
    ExpectInt  (S[18], "bestShowtime_cpt (lwz 0xE0, MONEY)", ReadWord(E, 0xE0), 14);
    std::snprintf(lacNum, sizeof(lacNum), "%d", ReadWord(E, 0x100));
    ExpectKeyParam(S[19], "bestStuntRun_cpt (0x100)", "CV_PANEL_EVENTS_SCORE", 9, lacNum);
    ExpectInt  (S[20], "totalTakedowns_cpt (lwz 0x88)", ReadWord(E, 0x88), 11);
    ExpectFloat(S[21], "bestDrift_cpt (lwz 0xEC, fcfid)", static_cast<f32>(ReadWord(E, 0xEC)), 16);
    std::snprintf(lacNum, sizeof(lacNum), "%d", ReadWord(E, 0xE8));
    ExpectKeyParam(S[22], "bestBoostChain_cpt (0xE8)", "STAT_BURNOUTS", 9, lacNum);
    ExpectFloat(S[23], "bestOncoming_cpt (lwz 0xF0, fcfid)", static_cast<f32>(ReadWord(E, 0xF0)), 16);
    ExpectFloat(S[24], "bestAirTime_cpt is the FLOAT at 0xF4 (lfs f1 @0x824B8968), format 5", KF_AIRTIME, 5);
    ExpectKeyParam(S[25], "bestSpin_cpt is \"%d\" of the TRUNCATED float at 0xF8 (lfs/fctiwz @0x824B8978)",
                   "STAT_DEGREES", 9, "373");
    std::snprintf(lacNum, sizeof(lacNum), "%d", ReadWord(E, 0xFC));
    ExpectKeyParam(S[26], "bestBarrelRoll_cpt (0xFC)", "x %1", 0, lacNum);
    ExpectInt  (S[27], "bestPowerParking_cpt (lwz 0x2C, PERCENTAGE)", ReadWord(E, 0x2C), 13);
    {
        const TextField& F = S[28];
        char lacWhat[256];
        std::snprintf(lacWhat, sizeof(lacWhat),
                      "bestRoadRage_cpt: one KEY+INT call, 'CV_PANEL_EVENTS_TD_COUNT' format 9, %d format 11 "
                      "(got kind %d '%s' %d, %d %d)", ReadWord(E, 0xE4), F.meKind, F.macText, F.meFormat, F.miValue,
                      F.meValueFormat);
        Check(F.miCalls == 1 && F.meKind == K_KEY_INT && std::strcmp(F.macText, "CV_PANEL_EVENTS_TD_COUNT") == 0 &&
              F.meFormat == 9 && F.miValue == ReadWord(E, 0xE4) && F.meValueFormat == 11, lacWhat);
    }
    ExpectText (S[29], "bodyShopsFound_cpt (0x10C / 0x11C)", ReadWord(E, 0x10C), ReadWord(E, 0x11C));
    ExpectText (S[30], "gasStationsFound_cpt (0x110 / 0x120)", ReadWord(E, 0x110), ReadWord(E, 0x120));
    ExpectText (S[31], "paintShopsFound_cpt (0x114 / 0x124)", ReadWord(E, 0x114), ReadWord(E, 0x124));
    ExpectText (S[32], "carParksFound_cpt (0x118 / 0x128, the junk-yard pair)", ReadWord(E, 0x118), ReadWord(E, 0x128));

    // ---- the district loop 0x824B8A18..0x824B8AAC: cursor event+0x134, +0x14 / +0x28, totals +0x3C/+0x50/+0x64 ----
    for (u32 luDistrict = 0; luDistrict < 5; ++luDistrict)
    {
        const u32 luCursor = 0x134u + 4u * luDistrict;
        char lacName[64];
        std::snprintf(lacName, sizeof(lacName), "bb district %u (0x%X / 0x%X)", luDistrict, luCursor, luCursor + 0x3C);
        ExpectText(sScreen.maDistrictBillboardsTextfields[luDistrict], lacName,
                   ReadWord(E, luCursor), ReadWord(E, luCursor + 0x3C));
        std::snprintf(lacName, sizeof(lacName), "jmp district %u (0x%X / 0x%X)", luDistrict, luCursor + 0x14,
                      luCursor + 0x50);
        ExpectText(sScreen.maDistrictJumpsTextfields[luDistrict], lacName,
                   ReadWord(E, luCursor + 0x14), ReadWord(E, luCursor + 0x50));
        std::snprintf(lacName, sizeof(lacName), "smh district %u (0x%X / 0x%X)", luDistrict, luCursor + 0x28,
                      luCursor + 0x64);
        ExpectText(sScreen.maDistrictSmashesTextfields[luDistrict], lacName,
                   ReadWord(E, luCursor + 0x28), ReadWord(E, luCursor + 0x64));
    }

    std::printf("L6DriverDetailsStats: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
