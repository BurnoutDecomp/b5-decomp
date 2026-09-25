// ===================================================================================
// BrnGui::GuiNetworkPlayerStats -- partfile: SetInfo and the row-text tables.
//   b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkPlayerStats_wN3_00.cpp
//
// Reconstructed from the console image (assembly listing authoritative). The
// tables are the image's static data, read back value for value.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkPlayerStats.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::GetLanguageManager
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // FormatText / AddString

namespace BrnGui
{
    namespace
    {
        typedef CgsLanguage::LanguageManager LM;

        // The stat each row shows, top to bottom.
        const BrnNetwork::NetworkPlayerStats::EStatsValue KAE_STATS_VALUE_LOOKUP[6] =
        {
            BrnNetwork::NetworkPlayerStats::E_STATS_VALUE_CHALLENGES_COMPLETED,
            BrnNetwork::NetworkPlayerStats::E_STATS_VALUE_TAKEDOWNS,
            BrnNetwork::NetworkPlayerStats::E_STATS_VALUE_ACHIEVEMENTS_EARNT,
            BrnNetwork::NetworkPlayerStats::E_STATS_VALUE_NUMBER_OF_RIVALS,
            BrnNetwork::NetworkPlayerStats::E_STATS_VALUE_TOTAL_GAMES_COMPLETED,
            BrnNetwork::NetworkPlayerStats::E_STATS_VALUE_WINS,
        };

        const s32 KI_NUM_STATS_VALUE_LOOKUPS =
            static_cast<s32>(sizeof(KAE_STATS_VALUE_LOOKUP) / sizeof(KAE_STATS_VALUE_LOOKUP[0]));

        const u32 KU_VALUE_BUFFER_SIZE     = 1024;
        const u32 KU_VALUE_ID_BUFFER_SIZE  = 32;
        const u32 KU_WORLD_RANK_PRINT_SIZE = 10;
    }

    const char* const GuiNetworkPlayerStats::KAPC_STAT_DESCRIPTION_STRING_ID[KI_MAX_STAT_ROWS] =
    {
        "$ONLINE_STAT_FREEBURN_CHALLENGES",
        "$ONLINE_STAT_NUM_TAKEDOWNS",
        "$ONLINE_STAT_NUM_ACHIEVEMENTS",
        "$ONLINE_STAT_NUM_RIVALS",
        "$ONLINE_STAT_NUM_RACES",
        "$ONLINE_STAT_PERCENT_WINS",
    };

    const char GuiNetworkPlayerStats::KAC_VALUE_STRING_ID_FORMAT[15]           = "$STAT_VALUE_%d";
    const char GuiNetworkPlayerStats::KAC_WORLD_RANK_DESCRIPTION_STRING_ID[19] = "$ONLINE_WORLD_RANK";
    const char GuiNetworkPlayerStats::KAC_WORLD_RANK_VALUE_STRING_ID[12]       = "$WORLD_RANK";
    const char GuiNetworkPlayerStats::KAC_NO_DATA_STRING_ID[30]                = "ONLINE_GAME_ROOM_NO_DATA_TEXT";

    // ------------------------------------------------------------ SetInfo
    // Fill the panel for one player: each row's description and value (the value text is
    // registered under the row's own string id), the player's name and, when asked, the
    // world-rank row. Nothing happens until the panel's apt components have loaded. The
    // image-index argument is not read; the displayed image index is reset to 0.
    void GuiNetworkPlayerStats::SetInfo(const char* lpacName, s32 liValue,
                                        const BrnNetwork::NetworkPlayerStats* lpStats,
                                        s32 liImageIndex, bool lbShowWorldRank, GuiCache* lpGuiCache)
    {
        (void)liImageIndex;

        if (!mbIsLoaded)
        {
            return;
        }

        char lacValue[KU_VALUE_BUFFER_SIZE];

        for (s32 liRow = 0; liRow < KI_MAX_STAT_ROWS; ++liRow)
        {
            char lacValueStringId[KU_VALUE_ID_BUFFER_SIZE];

            maStatRows[liRow].mDescription.SetText(KAPC_STAT_DESCRIPTION_STRING_ID[liRow]);

            CgsCore::SPrintf(lacValueStringId, KU_VALUE_ID_BUFFER_SIZE, KAC_VALUE_STRING_ID_FORMAT, liRow);
            maStatRows[liRow].mValue.SetText(lacValueStringId);

            if (lpStats != 0)
            {
                CGS_ASSERT(liRow < KI_NUM_STATS_VALUE_LOOKUPS, "Not enough elements in KAE_FORMAT_LOOKUP");
                FormatNetworkStats(lacValue, KU_VALUE_BUFFER_SIZE, lpStats, KAE_STATS_VALUE_LOOKUP[liRow], lpGuiCache);
            }
            else
            {
                mpStateInterface->GetLanguageManager()->FormatText(lacValue, KU_VALUE_BUFFER_SIZE,
                                                                   KAC_NO_DATA_STRING_ID, LM::E_FORMAT_ID_LOOKUP);
            }

            mpStateInterface->GetLanguageManager()->AddString(lacValueStringId + 1,
                                                              reinterpret_cast<const u8*>(lacValue));
        }

        mUserName.SetText(lpacName);

        if (lbShowWorldRank)
        {
            mWorldRank.mDescription.SetText(KAC_WORLD_RANK_DESCRIPTION_STRING_ID);
            mWorldRank.mValue.SetText(KAC_WORLD_RANK_VALUE_STRING_ID);

            // Without a stats block the value keeps the last row's text.
            if (lpStats != 0)
            {
                if (liValue < 1)
                {
                    mpStateInterface->GetLanguageManager()->FormatText(lacValue, KU_VALUE_BUFFER_SIZE,
                                                                       KAC_NO_DATA_STRING_ID, LM::E_FORMAT_ID_LOOKUP);
                }
                else
                {
                    CgsCore::SPrintf(lacValue, KU_WORLD_RANK_PRINT_SIZE, "%i", liValue);
                    mpStateInterface->GetLanguageManager()->FormatText(lacValue, KU_VALUE_BUFFER_SIZE,
                                                                       liValue, LM::E_FORMAT_INTEGER);
                }
            }

            mpStateInterface->GetLanguageManager()->AddString(KAC_WORLD_RANK_VALUE_STRING_ID + 1,
                                                              reinterpret_cast<const u8*>(lacValue));
        }
        else
        {
            mWorldRank.mDescription.SetText("");
            mWorldRank.mValue.SetText("");
        }

        mbIsDirty          = true;
        miPlayerImageIndex = 0;
    }
}
