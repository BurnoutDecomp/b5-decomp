
#include "GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkPlayerStats.h"

#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface::GetLanguageManager
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // CgsLanguage::LanguageManager (FormatText + ParameterFormatType)
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache::GetFreeburnChallengeList
#include "SharedClasses/DataLists/ChallengeList.h"                        // BrnResource::ChallengeList::GetChallengeCount

namespace BrnGui
{
    // X360 .rdata @0x8204C6B4 -- MEASURED (dumped from BURNOUT_X360_ARTIST.XEX; the five raw
    // dwords are 11, 0, 14, 3, 13). Indexed by BrnNetwork::NetworkPlayerStats::EStatType; the
    // element count is X360-attested twice over -- the DWARF declares [5] and the next named
    // rodata head (KAE_STATS_VALUE_LOOKUP @0x8204C6C8) starts exactly 5 dwords later.
    // Sole consumer: FormatNetworkStats @0x82416ED4/EE0/EE8.
    const CgsLanguage::LanguageManager::ParameterFormatType
    GuiNetworkPlayerStats::KAE_FORMAT_LOOKUP[BrnNetwork::NetworkPlayerStats::E_STAT_TYPE_COUNT] =
    {
        CgsLanguage::LanguageManager::E_FORMAT_INTEGER,          // 0 E_STAT_TYPE_NUMBER  (raw 11)
        CgsLanguage::LanguageManager::E_FORMAT_TEXT,             // 1 E_STAT_TYPE_STRING  (raw 0)
        CgsLanguage::LanguageManager::E_FORMAT_MONEY,            // 2 E_STAT_TYPE_MONEY   (raw 14)
        CgsLanguage::LanguageManager::E_FORMAT_MINUTES_SECONDS,  // 3 E_STAT_TYPE_TIME    (raw 3)
        CgsLanguage::LanguageManager::E_FORMAT_PERCENTAGE,       // 4 E_STAT_TYPE_PERCENT (raw 13)
    };

    // @0x82416E88 -- render one network stat into lpacBuffer through the LanguageManager.
    //
    // The stat's own textual value is always fetched first (GetStatAsString into an
    // 11-byte scratch buffer -- the asm passes the literal 0xB, which is exactly
    // NetworkPlayerStats::KI_MININUM_CHAR_BUFFER_SIZE). The freeburn-challenge stat is the
    // one special case: it is displayed as "<completed> of <total>" through the
    // ONLINE_STAT_X_OF_Y loc-string, whose two positional parameters are the stat value and
    // the total challenge count taken from the GUI cache's freeburn challenge list. Every
    // other stat is formatted straight through its EStatType's ParameterFormatType.
    //
    // Return: the X360 tail-returns the FormatText result in r3, but the DWARF declares this
    // void (cpp:193) and the sole caller (SetInfo) ignores it -- the declaration shape wins.
    void GuiNetworkPlayerStats::FormatNetworkStats(char* lpacBuffer, int liBufferSize,
                                                   const BrnNetwork::NetworkPlayerStats* lpStats,
                                                   BrnNetwork::NetworkPlayerStats::EStatsValue leValue,
                                                   GuiCache* lpGuiCache)
    {
        char lacStatValue[BrnNetwork::NetworkPlayerStats::KI_MININUM_CHAR_BUFFER_SIZE];

        lpStats->GetStatAsString(leValue, lacStatValue, BrnNetwork::NetworkPlayerStats::KI_MININUM_CHAR_BUFFER_SIZE);

        if (leValue == BrnNetwork::NetworkPlayerStats::E_STATS_VALUE_CHALLENGES_COMPLETED)
        {
            char lacTotalChallenges[BrnNetwork::NetworkPlayerStats::KI_MININUM_CHAR_BUFFER_SIZE];

            // X360 assert message is literally "lpGuiCache" (cpp:205).
            CGS_ASSERT(lpGuiCache, "lpGuiCache");

            // The X360 reads the count inline as *(list + 0x32E0) -- that CONSOLE offset is
            // ChallengeList::miCount, i.e. the inlined body of the declared accessor
            // GetChallengeCount(); reconstructed as the call, never as host pointer arithmetic.
            CgsCore::SPrintf(lacTotalChallenges, BrnNetwork::NetworkPlayerStats::KI_MININUM_CHAR_BUFFER_SIZE,
                             "%d", lpGuiCache->GetFreeburnChallengeList()->GetChallengeCount());

            mpStateInterface->GetLanguageManager()->FormatTextV(lpacBuffer, liBufferSize, "ONLINE_STAT_X_OF_Y",
                CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 2,
                lacStatValue,        CgsLanguage::LanguageManager::E_FORMAT_INTEGER,
                lacTotalChallenges,  CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
        }
        else
        {
            mpStateInterface->GetLanguageManager()->FormatText(
                lpacBuffer, liBufferSize, lacStatValue,
                KAE_FORMAT_LOOKUP[lpStats->GetStatType(leValue)]);
        }
    }
}
