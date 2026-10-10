#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficFuzzyLogicBehaviours.h"

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficTweakConstants.h" // TweakValues
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"            // gxMessageFilterFlags / gpDebugPrint
#include "GameShared/GameClasses/Development/CgsStrStream.h"          // CgsDev::StrStreamBase

#include <cstdio>    // fopen / fgets / sscanf / fclose
#include <cstring>   // strlen
#include <string.h>  // _stricmp / _strnicmp

// BrnTrafficFuzzyLogicBehaviours partfile (blocked-TU wave): the debug reload of the tunable
// behaviour file and the name tables it parses against, beside their home BrnTrafficFuzzyLogicBehaviours.cpp. Table contents are the
// console's own data.

namespace BrnTraffic
{
namespace Fuzzy
{
    const char* KPAC_BEHAVIOUR_FILE = "d:\\TrafficBehaviour.txt";

    const char* KAPC_DISTANCE_CATEGORY_NAMES[3]      = { "VERY_NEAR", "NEAR", "MIDDLE" };
    const char* KAPC_HEIGHT_CATEGORY_NAMES[1]        = { "NEAR" };
    const char* KAPC_CLOSINGSPEED_CATEGORY_NAMES[4]  = { "AWAY", "SLOW", "TOWARDS", "FAST_TOWARDS" };
    const char* KAPC_LANEPOSITION_CATEGORY_NAMES[3]  = { "IN_LANE", "IN_SWERVE_RANGE", "EXTREME_SWERVE" };
    const char* KAPC_ABSOLUTESPEED_CATEGORY_NAMES[2] = { "AWAY", "TOWARDS" };
    const char* KAPC_TRAFFICLIGHT_CATEGORY_NAMES[1]  = { "IN_RANGE" };
    const char* KAPC_NEXTPARAMDIST_CATEGORY_NAMES[1] = { "IN_RANGE" };
    const char* KAPC_SWERVEDIST_CATEGORY_NAMES[2]    = { "IN_RANGE", "EXTREME_RANGE" };
    const char* KAPC_SWERVEANGLE_CATEGORY_NAMES[1]   = { "ONCOMING" };
    const char* KAPC_TIMEQUEUEING_CATEGORY_NAMES[1]  = { "TOO_LONG" };

    // In SetupEnvelope's category order.
    const char* KAPC_CATEGORY_NAMES[10] =
    {
        "DISTANCE", "HEIGHT", "CLOSING_SPEED", "LANEPOS", "ABSOLUTE_SPEED",
        "TRAF_LIGHT_DIST", "NEXT_PARAM_DIST", "TIME_QUEUEING", "SWERVE_DIST", "SWERVE_ANGLE",
    };

    const char** KAPAPC_CATEGORY_NAME_LISTS[10] =
    {
        KAPC_DISTANCE_CATEGORY_NAMES,      KAPC_HEIGHT_CATEGORY_NAMES,
        KAPC_CLOSINGSPEED_CATEGORY_NAMES,  KAPC_LANEPOSITION_CATEGORY_NAMES,
        KAPC_ABSOLUTESPEED_CATEGORY_NAMES, KAPC_TRAFFICLIGHT_CATEGORY_NAMES,
        KAPC_NEXTPARAMDIST_CATEGORY_NAMES, KAPC_TIMEQUEUEING_CATEGORY_NAMES,
        KAPC_SWERVEDIST_CATEGORY_NAMES,    KAPC_SWERVEANGLE_CATEGORY_NAMES,
    };

    s32 KAI_CATEGORY_NAME_LIST_LENGTHS[10] = { 3, 1, 4, 3, 2, 1, 1, 1, 2, 1 };

    // In TweakValues member order (SetConstantValue's index).
    const char* KAPC_MEGATWEEK_CONSTANT_NAMES[21] =
    {
        "STOPLINE_VARIATION",   "RACE_CAR_STOP_DIST",      "GAP_CLOSING_FACTOR",
        "MIN_NORMAL_ACCELERATION", "MAX_NORMAL_ACCELERATION", "MIN_ACCELERATION",
        "MAX_ACCELERATION",     "MIN_SPEED_FOR_CUTOFF",    "MIN_STOP_DIST",
        "SWERVE_SCALE",         "EXTREME_SWERVE_SCALE",    "EXTREME_SWERVE_STICKINESS",
        "EXTREME_SWERVE_MIN_TIME", "SPIN_AIRRAM_MAG_MIN",  "SPIN_AIRRAM_MAG_MAX",
        "SPIN_AIRRAM_DECAY",    "SPIN_AIRRAM_ZDIST",       "ROLL_AIRRAM_MAG_MIN",
        "ROLL_AIRRAM_MAG_MAX",  "ROLL_AIRRAM_DECAY",       "ROLL_AIRRAM_SIDE_DIST",
    };

    // . Case-insensitive index of lpcString in the list, or -1.
    s32 _FindStringInList(const char* lpcString, const char** lpacStringList, s32 liNumStrings)
    {
        CGS_ASSERT(lpcString != 0, "lpcString");
        CGS_ASSERT(lpacStringList != 0, "lpacStringList");

        for (s32 liIndex = 0; liIndex < liNumStrings; ++liIndex)
        {
            if (_stricmp(lpcString, lpacStringList[liIndex]) == 0)
                return liIndex;
        }
        return -1;
    }

    // Debug tool (BrnTraffic::DebugComponent::ReloadBehaviourData). Parse the behaviour file line
    // by line: '#' lines and lines shorter than five characters are skipped;
    //   FUZZY <category> <envelope> <attack start> <attack stop> <decay start> <decay stop>
    // re-seats one envelope of one category, and
    //   SET <constant> <value>
    // writes one mega-tweek constant. Everything is echoed under message filter bit 0 (the FUZZY
    // echo prints the attack stop ahead of the attack start, as the console does). Afterwards the
    // cached score vector picks up the (possibly re-tuned) extreme-swerve stickiness.
    void FuzzyBehaviourLogic::ReloadBehaviours()
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "Reloading traffic behaviour data from '"
                                       << (KPAC_BEHAVIOUR_FILE != 0 ? KPAC_BEHAVIOUR_FILE : "<NULLSTRING>")
                                       << "'\n";
        }

        FILE* lpFile = std::fopen(KPAC_BEHAVIOUR_FILE, "r");
        if (lpFile == 0)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                *CgsDev::Log::gpDebugPrint << "Unable to open traffic behaviour file '" << KPAC_BEHAVIOUR_FILE << "'\n";
            return;
        }

        char lacLine[1024];
        while (std::fgets(lacLine, 1023, lpFile) != 0)
        {
            if (lacLine[0] == '#' || std::strlen(lacLine) < 5)
                continue;

            const bool lbLog = (CgsDev::Message::gxMessageFilterFlags & 1) != 0;

            if (_strnicmp(lacLine, "FUZZY", 5) == 0)
            {
                char lacCategory[128];
                char lacEnvelope[128];
                f32  lfAttackStart = 0.0f;
                f32  lfAttackStop  = 0.0f;
                f32  lfDecayStart  = 0.0f;
                f32  lfDecayStop   = 0.0f;
                if (std::sscanf(lacLine + 6, "%s %s %f %f %f %f", lacCategory, lacEnvelope,
                                &lfAttackStart, &lfAttackStop, &lfDecayStart, &lfDecayStop) != 6)
                {
                    if (lbLog)
                        *CgsDev::Log::gpDebugPrint << "Warning: Invalid FUZZY command in traffic behaviour file: '" << lacLine << "'\n";
                    continue;
                }

                if (lbLog)
                {
                    *CgsDev::Log::gpDebugPrint << "GOT: 'FUZZY' '" << lacCategory << "' '" << lacEnvelope << "' : "
                                               << lfAttackStop << ", " << lfAttackStart << ", "
                                               << lfDecayStart << ", " << lfDecayStop << "\n";
                }

                const s32 liCategory = _FindStringInList(lacCategory, KAPC_CATEGORY_NAMES, 10);
                if (liCategory < 0)
                {
                    if (lbLog)
                        *CgsDev::Log::gpDebugPrint << "Unknown category '" << lacCategory << "'\n";
                    continue;
                }

                const s32 liEnvelope = _FindStringInList(lacEnvelope, KAPAPC_CATEGORY_NAME_LISTS[liCategory],
                                                         KAI_CATEGORY_NAME_LIST_LENGTHS[liCategory]);
                if (liEnvelope >= 0)
                {
                    SetupEnvelope(liCategory, liEnvelope, lfAttackStart, lfAttackStop, lfDecayStart, lfDecayStop);
                }
                else if (lbLog)
                {
                    *CgsDev::Log::gpDebugPrint << "Unknown envelope '" << lacEnvelope << "' for category '"
                                               << lacCategory << "'\n";
                }
            }
            else if (_strnicmp(lacLine, "SET", 3) == 0)
            {
                char lacConstant[128];
                f32  lfValue = 0.0f;
                if (std::sscanf(lacLine + 4, "%s %f", lacConstant, &lfValue) != 2)
                {
                    if (lbLog)
                        *CgsDev::Log::gpDebugPrint << "Warning: Invalid SET command in traffic behaviour file: '" << lacLine << "'\n";
                    continue;
                }

                if (lbLog)
                    *CgsDev::Log::gpDebugPrint << "GOT: 'SET' '" << lacConstant << "' : " << lfValue << "\n";

                const s32 liValueIndex = _FindStringInList(lacConstant, KAPC_MEGATWEEK_CONSTANT_NAMES, 21);
                if (liValueIndex >= 0)
                {
                    SetConstantValue(liValueIndex, lfValue);
                }
                else if (lbLog)
                {
                    *CgsDev::Log::gpDebugPrint << "Unknown value '" << lacConstant << "'\n";
                }
            }
            else if (lbLog)
            {
                *CgsDev::Log::gpDebugPrint << "Warning: Unknown command in traffic behaviour file: '" << lacLine << "'\n";
            }
        }

        std::fclose(lpFile);

        Vector4 lNormalScore = mNormalScore_ExtremeSwerveStickiness_Z_W;
        lNormalScore.y = mpTweakValues->GetExtremeSwerveStickiness();
        mNormalScore_ExtremeSwerveStickiness_Z_W = lNormalScore;
    }
}
}
