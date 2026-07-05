#include "GameSource/Gui/Flow/HUD/Components/BrnPlayerPositionTable.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT
#include "GameShared/GameClasses/Network/CgsNetworkUtils.h"     // CgsNetwork::UsernameCompare
#include "GameSource/Gui/BrnGuiCache.h"                         // BrnGui::GuiCache (GetFreeburnChallengeManager)

// BrnGui::PlayerPositionTableComponent -- reconstructed from BURNOUT_X360_ARTIST.XEX.
// Only the four asm-attested leaf functions land here so far (ClearStoredData /
// AddInvisibleTeamLine / FunctionSortTeamHighToLow / SetCache); the rest of the
// declared surface is bodied by later slices.

namespace BrnGui
{
    // ------------------------------------------------ AddInvisibleTeamLine @ 0x82413BC8
    // Find the first free slot in maPlayerSingleData (mePlayerType still
    // E_PLAYERTYPES_INVISIBLE) and park an invisible "Invisible" blue-team line there
    // (used by SortData to pad the team table). No free slot -> no-op.
    void PlayerPositionTableComponent::AddInvisibleTeamLine()
    {
        s32 liSlot = 0;
        while (maPlayerSingleData[liSlot].mePlayerType != E_PLAYERTYPES_INVISIBLE)
        {
            if (++liSlot >= KI_MAX_BARS_NEEDED)
                return;
        }

        PlayerPositionSingleData& lData = maPlayerSingleData[liSlot];
        lData.mPlayerName.Construct("Invisible");
        lData.mfTableValue         = -1.0f;
        lData.meTeam               = BrnGameState::GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM;
        lData.meActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(8);
    }

    // ------------------------------------------------ ClearStoredData @ 0x8241EEF0
    // Reset every row of maPlayerSingleData to the invisible default and clear the
    // used-bar count. (9x inlined PlayerPositionSingleData::Clear().)
    void PlayerPositionTableComponent::ClearStoredData()
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_BARS_NEEDED; ++liIndex)
        {
            PlayerPositionSingleData& lData = maPlayerSingleData[liIndex];
            lData.mPlayerName.macName[0] = 0;
            lData.meNameType           = 0;
            lData.mfTableValue         = 0.0f;
            lData.mePlayerType         = E_PLAYERTYPES_INVISIBLE;
            lData.meActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(-1);
            lData.meHeadsetStatus      = E_HEADSETSTATUS_INVISIBLE;
            lData.meRevengeStatus      = E_REVENGESTATUS_INVISIBLE;
            lData.meAwardState         = E_AWARDSTATUS_NONE;
            lData.meTeam               = BrnGameState::GameStateModuleIO::E_PLAYER_TEAM_NONE;
            // E_GUI_PLAYER_COLOURS_NONE == 0 (DWARF BrnGuiShared.h). mePlayerColour is a raw
            // s32 in the committed record (EGuiPlayerColours enum not yet committed); store the
            // literal 0 -- store-for-store faithful until the enum lands.
            lData.mePlayerColour       = 0;
            lData.miHoldingSlot        = -1;
        }

        miCurrentBarsUsed = 0;
    }

    // ------------------------------------------------ FunctionSortTeamHighToLow @ 0x82413A20
    // qsort() comparator: order the team table high-to-low. Empty rows sink; disconnected
    // rows tie-break alphabetically (CgsNetwork::UsernameCompare); within a team, higher
    // mfTableValue first; across teams, the blue team (2) sorts after the red.
    s32 PlayerPositionTableComponent::FunctionSortTeamHighToLow(const void* lp1, const void* lp2)
    {
        const PlayerPositionSingleData* lpPlayer1 =
            static_cast<const PlayerPositionSingleData*>(lp1);
        const PlayerPositionSingleData* lpPlayer2 =
            static_cast<const PlayerPositionSingleData*>(lp2);

        if (lpPlayer1->mPlayerName.macName[0] == 0)
            return lpPlayer2->mPlayerName.macName[0] != 0;
        if (lpPlayer2->mPlayerName.macName[0] == 0)
            return -1;

        // E_GUI_PLAYER_COLOURS_DISCONNECTED == 11 (DWARF BrnGuiShared.h). mePlayerColour
        // is a raw s32 in the committed record; compare against the literal until the
        // EGuiPlayerColours enum is committed to src.
        if (lpPlayer1->mePlayerColour == 11)
        {
            if (lpPlayer2->mePlayerColour == 11)
                return CgsNetwork::UsernameCompare(lpPlayer1->mPlayerName.macName,
                                                   lpPlayer2->mPlayerName.macName);
            return 1;
        }
        if (lpPlayer2->mePlayerColour == 11)
            return -1;

        if (lpPlayer1->meTeam == lpPlayer2->meTeam)
        {
            const f32 lfValue1 = lpPlayer1->mfTableValue;
            const f32 lfValue2 = lpPlayer2->mfTableValue;
            if (lfValue1 <= lfValue2)
                return lfValue1 < lfValue2;
            return -1;
        }
        if (lpPlayer1->meTeam != BrnGameState::GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM)
            return 1;
        return -1;
    }

    // ------------------------------------------------ SetCache @ 0x82473458
    // Latch the GuiCache into the table and every row component, then cache the freeburn
    // challenge manager pointer out of it.
    void PlayerPositionTableComponent::SetCache(GuiCache* lpCache)
    {
        CGS_ASSERT(lpCache != NULL, "lpCache != NULL");
        mpCache = lpCache;

        for (s32 liIndex = 0; liIndex < KI_MAX_BARS_NEEDED; ++liIndex)
        {
            CGS_ASSERT(lpCache != NULL, "lpCache != NULL");
            maPlayerComponents[liIndex].SetCache(lpCache);
        }

        CGS_ASSERT(mpCache->GetFreeburnChallengeManager() != NULL, "mpChallengeManager");
        mpChallengeManager = mpCache->GetFreeburnChallengeManager();
    }
}
