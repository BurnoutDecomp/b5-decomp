#include "GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkPlayerStats.h"
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gui/Flow/Screen/Components/BrnGuiNetworkPlayerStats.cpp"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"                   // BrnGui::GuiCache (AppendExpectedAptComponent)

// ===================================================================================
// BrnGui::GuiNetworkPlayerStats -- the online per-player stats panel.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct                 @0x82416CA8
//   Destruct                  @0x82416DE0
//   SetState                  @0x82416E00
//   Update                    @0x82416FA8
//   AppendExpectedAptComponent@0x82417040
//
// FormatNetworkStats (@0x82416E88) IS reconstructed -- it lives in the sibling file
// BrnGuiNetworkPlayerStats_wL_01.cpp, which also carries the definition of the static
// KAE_FORMAT_LOOKUP table. Do NOT add a second definition of either here.
//
// (An earlier revision of this banner said FormatNetworkStats could not be reconstructed
// because KAE_FORMAT_LOOKUP @0x8204C6B4 was "unrecoverable without the binary". That is
// false and must not be restored: the table was dumped from BURNOUT_X360_ARTIST.XEX --
// five dwords 11/0/14/3/13, the only two xrefs to it being 0x82416EE0 and 0x82416EE8,
// both inside FormatNetworkStats, and its 5-element bound is pinned both by the DWARF
// (ParameterFormatType[5], cpp:62) and by the next named head KAE_STATS_VALUE_LOOKUP
// @0x8204C6C8 starting exactly five dwords later.)
// ===================================================================================

namespace BrnGui
{
    // ---- static data (X360 rodata this TU owns; values from the Construct/Update asm) ----
    const char GuiNetworkPlayerStats::KAC_DESCRIPTION_COMPONENT[14]            = "Description%d";
    const char GuiNetworkPlayerStats::KAC_VALUE_COMPONENT[8]                   = "Value%d";
    const char GuiNetworkPlayerStats::KAC_USERNAME_COMPONENT[9]                = "UserName";
    const char GuiNetworkPlayerStats::KAC_WORLD_RANK_DESCRIPTION_COMPONENT[21] = "WorldRankDescription";
    const char GuiNetworkPlayerStats::KAC_WORLD_RANK_VALUE_COMPONENT[15]       = "WorldRankValue";
    const char GuiNetworkPlayerStats::KAC_APT_STATE[10]                        = "apt_state";
    const char GuiNetworkPlayerStats::KAC_PLAYER_IMAGE_INDEX[17]               = "PlayerImageIndex";

    const char* const GuiNetworkPlayerStats::KAPC_STATE_ID[E_STATE_COUNT] =
    {
        "visible",     // E_STATE_VISIBLE   (off_82F25028[0] -> 0x82049C28, X360-attested)
        "invisible",   // E_STATE_INVISIBLE (off_82F2502C  -> 0x8204B4F8, X360-attested)
    };

    // @0x82416CA8
    void GuiNetworkPlayerStats::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                                          const char* lpacParentName)
    {
        GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);

        // Each stat row's description + value field is named "Description<i>" / "Value<i>" and
        // parented to this panel's own name (the X360 passes lpacName, not lpacParentName, here).
        for (s32 liRow = 0; liRow < KI_MAX_STAT_ROWS; ++liRow)
        {
            char acComponentName[32];

            CgsCore::SPrintf(acComponentName, 32, KAC_DESCRIPTION_COMPONENT, liRow);
            maStatRows[liRow].mDescription.Construct(acComponentName, lpStateInterface, lpacName);

            CgsCore::SPrintf(acComponentName, 32, KAC_VALUE_COMPONENT, liRow);
            maStatRows[liRow].mValue.Construct(acComponentName, lpStateInterface, lpacName);
        }

        mUserName.Construct(KAC_USERNAME_COMPONENT, lpStateInterface, lpacName);
        mWorldRank.mDescription.Construct(KAC_WORLD_RANK_DESCRIPTION_COMPONENT, lpStateInterface, lpacName);
        mWorldRank.mValue.Construct(KAC_WORLD_RANK_VALUE_COMPONENT, lpStateInterface, lpacName);

        mbIsLoaded         = false;
        mbIsDirty          = false;
        miPlayerImageIndex = 0;
        meState            = E_STATE_INVISIBLE;
    }

    // @0x82416DE0 -- reset the state block to the post-Construct defaults.
    void GuiNetworkPlayerStats::Destruct()
    {
        mbIsLoaded         = false;
        meState            = E_STATE_INVISIBLE;
        mbIsDirty          = false;
        miPlayerImageIndex = 0;
    }

    // @0x82416E00 -- set the visibility state and mark the panel dirty for the next Update.
    void GuiNetworkPlayerStats::SetState(EState leState)
    {
        if (leState == E_STATE_VISIBLE)
        {
            meState   = E_STATE_VISIBLE;
            mbIsDirty = true;
        }
        else if (leState == E_STATE_INVISIBLE)
        {
            meState   = E_STATE_INVISIBLE;
            mbIsDirty = true;
        }
        else
        {
            CGS_ASSERT_AT(false, "Invalid state", CGS_ARTIST_FILE, 174);
        }
    }

    // @0x82416FA8 -- flush the dirty state to the bound apt clip: the player image index and
    // the visibility view-state, then clear the dirty flag.
    void GuiNetworkPlayerStats::Update()
    {
        if (mbIsDirty)
        {
            char acPlayerImageIndex[32];

            mbIsDirty = false;
            CgsCore::SPrintf(acPlayerImageIndex, 32, "%d", miPlayerImageIndex);

            AddOutputAptViewState(KAC_PLAYER_IMAGE_INDEX, acPlayerImageIndex, false);
            AddOutputAptViewState(KAC_APT_STATE, KAPC_STATE_ID[meState], false);
        }
    }

    // @0x82417040 -- register every owned text field, by name, as an expected apt component on
    // the given flow layer (so the cache waits for each to initialise). Order mirrors the X360:
    // self, user-name, world-rank value, world-rank description, then each row's description+value.
    void GuiNetworkPlayerStats::AppendExpectedAptComponent(GuiFlow leFlow, GuiCache* lpGuiCache)
    {
        lpGuiCache->AppendExpectedAptComponent(leFlow, GetName());
        lpGuiCache->AppendExpectedAptComponent(leFlow, mUserName.GetName());
        lpGuiCache->AppendExpectedAptComponent(leFlow, mWorldRank.mValue.GetName());
        lpGuiCache->AppendExpectedAptComponent(leFlow, mWorldRank.mDescription.GetName());

        for (s32 liRow = 0; liRow < KI_MAX_STAT_ROWS; ++liRow)
        {
            lpGuiCache->AppendExpectedAptComponent(leFlow, maStatRows[liRow].mDescription.GetName());
            lpGuiCache->AppendExpectedAptComponent(leFlow, maStatRows[liRow].mValue.GetName());
        }
    }
}

// FOLDED FROM BrnGuiNetworkPlayerStats_wL_01.cpp

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
            CGS_ASSERT_AT(lpGuiCache, "lpGuiCache", CGS_ARTIST_FILE, 205);

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

// FOLDED FROM BrnGuiNetworkPlayerStats_wN3_00.cpp
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
