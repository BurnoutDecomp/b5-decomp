#include "GameSource/Gui/Flow/HUD/Components/BrnPlayerPositionTable.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h"    // CgsDev::StrStream (SetupGameMode's streamed assert)
#include "GameShared/GameClasses/Network/CgsNetworkUtils.h"     // CgsNetwork::UsernameCompare
#include "GameSource/Gui/BrnGuiCache.h"                         // BrnGui::GuiCache (GetFreeburnChallengeManager / GetGameMode)

// BrnGui::PlayerPositionTableComponent -- reconstructed from BURNOUT_X360_ARTIST.XEX.
// Six asm-attested functions land here so far (ClearStoredData / AddInvisibleTeamLine /
// FunctionSortTeamHighToLow / SetCache / SetupGameMode, plus the compiler-inlined
// SetValuesDirty that SetupGameMode restores as a call); the rest of the declared
// surface is bodied by later slices.

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

    // ------------------------------------------------ SetValuesDirty (compiler-inlined)
    // DWARF BrnPlayerPositionTable.cpp:1334 / h:158. No X360 symbol -- every call site
    // inlined it -- but SetupGameMode emits the loop verbatim twice, so it is restored
    // here as a real call per the house rule on inlined callees:
    //     addi r10, r31, 0xCC ; li r11, 9 ; li r9, 1
    //     stb r9, 0(r10) ; addi r10, r10, 0xCC   (@0x8243E7A8 and @0x8243E818)
    // -- r31+0xCC == maPlayerComponents[0] (+0x0C) + 0xC0 == mbValueChanged, stride
    // 0xCC == sizeof(PlayerPositionSingleComponent), nine rows.
    void PlayerPositionTableComponent::SetValuesDirty()
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_BARS_NEEDED; ++liIndex)
        {
            maPlayerComponents[liIndex].SetValueChanged();
        }
    }

    // ------------------------------------------------ SetupGameMode @ 0x8243E6F8
    // Point the table's three clips at the frames the current game mode wants, blank the
    // title/skills text and mark every row's value dirty so the next DisplayData re-renders
    // it. ONLY the online modes (E_MODE_ONLINE_MODE_START 10 .. E_MODE_ONLINE_MODE_END 17)
    // have a table layout: the console tests `(u32)(meGameMode - 10) > 7` and, for anything
    // else, plays "invisible" on the component's own clip instead of "event" and then fires
    // the streamed "not setup for this game mode" assert (X360 cpp:260) AFTER doing the same
    // blanking work. Both arms are otherwise identical, store for store.
    //
    // NOTE the mode gate is on meGameMode, which this function latches FIRST from the cache
    // (`lwzx r11, mpCache, 0x9E58` == GuiCache::GetGameMode) -- the member is written on
    // every path, including the asserting one, and the assert streams the freshly-stored
    // value. Do not hoist the store below the test.
    void PlayerPositionTableComponent::SetupGameMode()
    {
        CGS_ASSERT(mpCache != NULL, "mpCache");   // X360 cpp:196 (non-gating: the load below follows regardless)

        meGameMode = static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(mpCache->GetGameMode());

        const bool lbOnlineMode =
            (meGameMode >= BrnGameState::GameStateModuleIO::E_MODE_ONLINE_MODE_START &&
             meGameMode <= BrnGameState::GameStateModuleIO::E_MODE_ONLINE_MODE_END);

        mAptRef.GotoAndPlayLabel(lbOnlineMode ? "event" : "invisible");   // @0x8243E764 / @0x8243E7DC
        mTitleBarMCR.GotoAndPlayLabel("invisible");                       // @0x8243E778 / @0x8243E7E8
        mPageIconMCR.GotoAndPlayLabel("invisible");                       // @0x8243E784 / @0x8243E7F4

        // unk_820046A7 -- the shared empty string (a lone NUL byte in .rdata).
        SetTitleText("");    // @0x8243E798 / @0x8243E808
        SetSkillsText("");   // @0x8243E7A4 / @0x8243E814

        SetValuesDirty();    // the inlined nine-row raise loop

        if (!lbOnlineMode)
        {
            // The console streams into the GLOBAL CgsDev::Assert::gpcMessageBuffer
            // (@0x8243E838..0x8243E8CC) and hands that pointer to FireAssert; this port uses
            // the tree's stack-buffer StrStream idiom (BrnGuiFsmController.cpp:33) and drops
            // the X360-baked file/line per the standing convention. Message text is verbatim.
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Position table is not setup for this game mode : "
                       << static_cast<s32>(meGameMode)
                       << "\nContact a gui programmer.";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);   // X360 cpp:260
            CgsDev::Assert::EndAssert();
        }
    }
}
