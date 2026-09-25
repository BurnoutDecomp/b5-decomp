#include "GameSource/Gui/Flow/HUD/Components/BrnPlayerPositionTable.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h"    // CgsDev::StrStream (SetupGameMode's streamed assert)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"      // CgsDev::Log (the [p0-postable] witness)
#include "GameShared/GameClasses/Network/CgsNetworkUtils.h"     // CgsNetwork::UsernameCompare
#include "GameSource/Gui/BrnGuiCache.h"                         // BrnGui::GuiCache (GetFreeburnChallengeManager / GetGameMode)

// includes folded in from the BrnPlayerPositionTable_w*.cpp partfiles (2026-09-15)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::OutputGuiEvent
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiAudioTriggerEvent
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"                    // MovieClipRef::FindChildMovieClipOnFrame / FindChildTextField
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"                    // TextFieldRef::SetText
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SnPrintf (the row names)
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // Start/StopMonitor (UpdatePositionDetails)
#include "GameSource/Gui/BrnGuiPerfmons.h"                                // GuiPerfmons::miPlayerPosTableUpdate
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // GuiEventRaceDistanceRemaining (GUI 239)
#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h"                // FreeburnChallengeManager (state / targets / contributions)
#include "GameSource/Gui/BrnGuiBurnoutSkillsManager.h"                    // BurnoutSkillsManager (the skill page)
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h" // InGamePlayerStatusData (the cached records)
#include "SharedClasses/DataLists/ChallengeListEntry.h"                    // ChallengeListEntryAction (the coop type)
#include <cstdlib>   // qsort
#include "GameShared/GameClasses/System/PC/BrnNetHarnessPC.h"  // BrnNetHarnessPC::WitnessTag (the [netui] table line)

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

        // [p0-postable] one-shot witness. NOT console code. The table is flag-gated by
        // RaceMainHudState's mbPlayerPositionTable, which is only raised for the ONLINE
        // game modes -- every offline mode either never sets it or has it cleared again by
        // the ladder tail -- so this line firing at all means an offline path reached the
        // component, which would be worth knowing.
        {
            static bool sbLoggedSetup = false;
            if (!sbLoggedSetup && CgsDev::Log::gpDebugPrint != 0)
            {
                sbLoggedSetup = true;
                *CgsDev::Log::gpDebugPrint
                    << "[p0-postable] PlayerPositionTableComponent::SetupGameMode -- mode "
                    << static_cast<s32>(meGameMode)
                    << ", title/skills text cleared\n";
            }
        }

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

// ============================================================================
// FOLDED FROM BrnPlayerPositionTable_wP0_01.cpp (wave P0) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// ============================================================================
// GameSource/Gui/Flow/HUD/Components/BrnPlayerPositionTable_wP0_01.cpp
//
// PlayerPositionTableComponent -- the two title-bar text setters, split out of
// the base TU so the base stays the reviewed 17/17 slice. Both are called from
// the base TU's SetupGameMode (and SetSkillsText additionally from
// ProcessOnlineFreeburnTable), which is why they are the mount's class-A holes.
//
// Both do the same thing: re-resolve a named child clip of mTitleBarMCR on its
// CURRENT frame, take the named text field inside it, latch that ref into the
// component's own member, then push the caller's string through it. The re-lookup
// is not redundant on console -- the title bar's frame changes with the game mode,
// so the field handle is only valid for the frame that is playing now.
// ============================================================================

namespace BrnGui
{
    // ------------------------------------------------ SetTitleText
    // Rebind mTitleText from mTitleBarMCR's "TitleText" child clip ("titleText_txt"
    // inside it), fire the "CodeTodaysBestFlips" GUI audio trigger, then set the text.
    //
    // The audio post is the compiler-inlined StateInterface::OutputGuiEvent<
    // GuiAudioTriggerEvent> (the console stack-builds the 112-byte record
    // {100, 457, 12} + the 100-byte payload and hands it to the out queue's AddEvent
    // on channel 40 -- exactly what the committed template emits for this type).
    // The component/movie names are the shared empty string in .rdata; the action
    // byte is 7, the same value every GuiAudioTriggerEvent::Construct site uses.
    void PlayerPositionTableComponent::SetTitleText(const char* lpcText)
    {
        BrnFlapt::MovieClipRef lTitleTextClipRef;
        const BrnFlapt::MovieClipRef lTitleTextClip =
            *mTitleBarMCR.FindChildMovieClipOnFrame(&lTitleTextClipRef, "TitleText");

        BrnFlapt::TextFieldRef lTitleTextFieldRef;
        mTitleText = *lTitleTextClip.FindChildTextField(&lTitleTextFieldRef, "titleText_txt");

        GuiAudioTriggerEvent lAudio;
        lAudio.Construct(7, "", "CodeTodaysBestFlips", "");
        mpStateInterface->OutputGuiEvent<GuiAudioTriggerEvent>(lAudio);

        mTitleText.SetText(lpcText, false);
    }

    // ------------------------------------------------ SetSkillsText
    // The same rebind against mTitleBarMCR's "SkillzText" child clip ("skillz_txt"
    // inside it) into mSkillzText, then set the text. No audio trigger on this one.
    void PlayerPositionTableComponent::SetSkillsText(const char* lpcText)
    {
        BrnFlapt::MovieClipRef lSkillsTextClipRef;
        const BrnFlapt::MovieClipRef lSkillsTextClip =
            *mTitleBarMCR.FindChildMovieClipOnFrame(&lSkillsTextClipRef, "SkillzText");

        BrnFlapt::TextFieldRef lSkillsTextFieldRef;
        mSkillzText = *lSkillsTextClip.FindChildTextField(&lSkillsTextFieldRef, "skillz_txt");

        mSkillzText.SetText(lpcText, false);
    }
}

// ============================================================================
// PlayerPositionTableComponent::Construct / ::Prepare -- the table's lifecycle pair, called
// unconditionally by RaceMainHudState's set-up (the component is only ticked on the online modes).
// ============================================================================
namespace BrnGui
{
    // Construct: the flapt base's inline Construct (the "lpStateInterface" tripwire, the channel,
    // the clip cleared), then the table's own resets in the console's order -- no cache, no
    // challenge manager, game mode NONE, skill state 14, challenge not running, challenge data 0 /
    // type 24, the four clip/text refs cleared -- the nine rows' Construct (no name, the same
    // channel, no parent), and ClearStoredData.
    void PlayerPositionTableComponent::Construct(const char* /*lacName*/,
                                                 CgsGui::StateInterface* lpStateInterface,
                                                 const char* /*lacParentName*/)
    {
        BrnFlaptComponent::Construct(lpStateInterface);

        mpCache                        = NULL;
        mpChallengeManager             = NULL;
        meGameMode                     = BrnGameState::GameStateModuleIO::E_MODE_NONE;
        meLastFrameSkillState          = 14;
        mbFreeburnChallengeRunning     = false;
        meCurrentChallengeDataType     = 24;
        miFreeburnChallengeCurrentData = 0;
        mTitleBarMCR.SetInvalid();
        mTitleText.SetInvalid();
        mSkillzText.SetInvalid();
        mPageIconMCR.SetInvalid();

        for (s32 liBar = 0; liBar < KI_MAX_BARS_NEEDED; ++liBar)
        {
            maPlayerComponents[liBar].Construct(NULL, lpStateInterface, NULL);
        }

        ClearStoredData();
    }

    // Prepare: bind the table's own clip (the flapt base's resolve / assert / timeline reset,
    // inlined by the console with no parent prefix), prepare the nine rows as
    // "<name>_Position_<i>" (128-byte buffer), then latch the page-icon and title-bar child clips.
    void PlayerPositionTableComponent::Prepare(const char* lacName, const BrnFlapt::FileRef& lFile)
    {
        BrnFlaptComponent::Prepare(lacName, lFile, NULL);

        for (s32 liBar = 0; liBar < KI_MAX_BARS_NEEDED; ++liBar)
        {
            char lacRowName[128];
            CgsCore::SnPrintf(lacRowName, sizeof(lacRowName), "%s_Position_%d", lacName, liBar);
            maPlayerComponents[liBar].Prepare(lacRowName, lFile);
        }

        BrnFlapt::MovieClipRef lPageIcon;
        mPageIconMCR = *mAptRef.FindChildMovieClip(&lPageIcon, "pageIcon_mc");
        BrnFlapt::MovieClipRef lTitleBar;
        mTitleBarMCR = *mAptRef.FindChildMovieClip(&lTitleBar, "TitleBar_mc");
    }
}

// ============================================================================
// The online position-table chain: RaceMainHudState's GUI 239 arm hands the per-frame
// race-distance record to UpdatePositionDetails, which (in the free-burn lobby modes) first
// re-titles the table for the challenge / Burnout Skillz page it is showing, then rebuilds
// one row per active race car, sorts the rows for the game mode, adds the challenge total row
// and pushes the rows into the nine bar components.
// ============================================================================
namespace BrnGui
{
    namespace
    {
        // The challenge target-type captions, indexed by the challenge data type (24 entries).
        const char* const KAPC_FREEBURN_CHALLENGE_TYPE_STRINGS[24] =
        {
            "$BURNOUT_SKILLS_CRASHES",         "$BURNOUT_SKILLS_NEAR_MISSES",
            "$BURNOUT_SKILLS_ONCOMING",        "$BURNOUT_SKILLS_DRIFT",
            "$BURNOUT_SKILLS_TIME_IN_AIR",     "$BURNOUT_SKILLS_AIR_DISTANCE",
            "$BURNOUT_SKILLS_BARREL_ROLLS",    "$BURNOUT_SKILLS_SPINS",
            "$BURNOUT_SKILLS_CARS_LEAPT",      "$BURNOUT_SKILLS_ROAD_RULE_TIME",
            "$BURNOUT_SKILLS_ROAD_RULE_CRASH", "$BURNOUT_SKILLS_LANDINGS",
            "$BURNOUT_SKILLS_BOOST_CHAIN",     "$BURNOUT_SKILLS_POWER_PARKING",
            "$BURNOUT_SKILLS_PERCENT",         "$BURNOUT_SKILLS_MEETUP",
            "$BURNOUT_SKILLS_BILLBOARDS",      "$BURNOUT_SKILLS_BOOST_TIME",
            "$BURNOUT_SKILLS_CONVOY_POS",      "$BURNOUT_SKILLS_DISTANCE",
            "$BURNOUT_SKILLS_CHAIN",           "$BURNOUT_SKILLS_MULTIPLIER",
            "$BURNOUT_SKILLS_STUNT_SCORE",     "$BURNOUT_SKILLS_CORKSCREWS",
        };
        const s32 KI_NUM_FREEBURN_CHALLENGE_TYPE_STRINGS =
            static_cast<s32>(sizeof(KAPC_FREEBURN_CHALLENGE_TYPE_STRINGS) /
                             sizeof(KAPC_FREEBURN_CHALLENGE_TYPE_STRINGS[0]));

        // The Burnout Skillz page captions, indexed by the skill being shown (12 entries).
        const char* const KAPC_BURNOUT_SKILLS_TITLE_STRINGS[12] =
        {
            "$BURNOUT_SKILLS_TIME_IN_AIR",   "$BURNOUT_SKILLS_BARREL_ROLLS",
            "$BURNOUT_SKILLS_BOOST_CHAIN",   "$BURNOUT_SKILLS_DRIFT",
            "$BURNOUT_SKILLS_SPINS",         "$BURNOUT_SKILLS_AIR_DISTANCE",
            "$BURNOUT_SKILLS_NEAR_MISSES",   "$BURNOUT_SKILLS_ONCOMING",
            "$BURNOUT_SKILLS_POWER_PARKING", "$BURNOUT_SKILLS_TAKEDOWNS",
            "$BURNOUT_SKILLS_STUNT_SCORE",   "$BURNOUT_SKILLS_TOTAL",
        };

        // E_GUI_PLAYER_COLOURS_DISCONNECTED / _ELIMINATED (the colour word is a raw s32 in
        // the committed row record).
        const s32 KI_PLAYER_COLOUR_DISCONNECTED = 11;
        const s32 KI_PLAYER_COLOUR_ELIMINATED   = 10;

        // The free-burn lobby modes (15 / 16).
        bool IsOnlineFreeBurnLobby(s32 liGameMode)
        {
            return liGameMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY ||
                   liGameMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME;
        }

        // The challenge manager page state the lobby table compares against (`cmpwi 3`; one
        // past the reference enumerators, which end at E_PAGE_STATE_SELECT == 2).
        const s32 KI_CHALLENGE_PAGE_STATE_3 = 3;
    }

    // ------------------------------------------------ UpdatePositionDetails
    void PlayerPositionTableComponent::UpdatePositionDetails(const GuiEventRaceDistanceRemaining* lpDistanceEvent)
    {
        CgsDev::PerfMonCpu::StartMonitor(GuiPerfmons::miPlayerPosTableUpdate);

        CGS_ASSERT(lpDistanceEvent != NULL, "lpDistanceEvent");
        CGS_ASSERT(mpCache != NULL, "mpCache != NULL");

        if (IsOnlineFreeBurnLobby(mpCache->GetGameMode()))
            ProcessOnlineFreeburnTable();

        ClearStoredData();
        FillOutInActiveRaceCarOrder(lpDistanceEvent);
        SortData();
        CountLinesAndAddTotal();
        DisplayData();

        // [netui] witness (PC harness, LAN gated): one line per change of the row count or
        // the game mode. Not console code.
        {
            static s32 siLastBarsUsed = -1;
            static s32 siLastGameMode = -2;
            if (miCurrentBarsUsed != siLastBarsUsed || static_cast<s32>(meGameMode) != siLastGameMode)
            {
                siLastBarsUsed = miCurrentBarsUsed;
                siLastGameMode = static_cast<s32>(meGameMode);
                BrnNetHarnessPC::WitnessTag("netui", "position-table", "rows=%d mode=%d first=%.16s",
                                            miCurrentBarsUsed, siLastGameMode,
                                            maPlayerSingleData[0].mPlayerName.macName);
            }
        }

        CgsDev::PerfMonCpu::StopMonitor(GuiPerfmons::miPlayerPosTableUpdate);
    }

    // ------------------------------------------------ ProcessOnlineFreeburnTable
    // The lobby table shows either the running free-burn challenge (no road rule active and
    // the challenge manager RUNNING or RESULTS) or the Burnout Skillz page the skills manager
    // is rotating through. Titles, captions and the page icon change only on a change of page.
    // The challenge arm never latches meCurrentChallengeDataType: the console re-sets the
    // caption on every frame the target type differs from the stored one.
    void PlayerPositionTableComponent::ProcessOnlineFreeburnTable()
    {
        CGS_ASSERT(IsOnlineFreeBurnLobby(mpCache->GetGameMode()) == true,
                   "GsmIO::IsOnlineFreeBurnLobby( mpCache->GetGameMode() ) == true");

        if (mpCache->GetActiveRoadRule() == 0 && mpChallengeManager->IsStarted())
        {
            if (!mbFreeburnChallengeRunning)
            {
                mbFreeburnChallengeRunning = true;
                const bool lbTwoTargetTitle =
                    (mpChallengeManager->GetTargetsCount() > 1) &&
                    (static_cast<s32>(mpChallengeManager->mePageState) != KI_CHALLENGE_PAGE_STATE_3);
                mTitleBarMCR.GotoAndPlayLabel(lbTwoTargetTitle ? "skills" : "basic");
                SetTitleText("$FREEBURN_CHALLENGE");
                mPageIconMCR.GotoAndPlayLabel("invisible");
                miFreeburnChallengeCurrentData = -1;
                if (!(mpChallengeManager->GetTargetsCount() > 0))
                    SetSkillsText(" - ");
                SetValuesDirty();
            }

            if (mpChallengeManager->GetTargetsCount() > 0 &&
                meCurrentChallengeDataType != static_cast<s32>(mpChallengeManager->GetCurrentTargetType()))
            {
                CGS_ASSERT(static_cast<s32>(mpChallengeManager->GetCurrentTargetType()) < KI_NUM_FREEBURN_CHALLENGE_TYPE_STRINGS,
                           "mpChallengeManager->GetCurrentTargetType() < BrnResource::ChallengeListEntryAction::E_CHALLENGE_DATA_TYPE_COUNT");
                CGS_ASSERT(static_cast<s32>(mpChallengeManager->GetCurrentTargetType()) >= 0,
                           "mpChallengeManager->GetCurrentTargetType() >= 0");
                SetSkillsText(KAPC_FREEBURN_CHALLENGE_TYPE_STRINGS[mpChallengeManager->GetCurrentTargetType()]);
                SetValuesDirty();
            }
            return;
        }

        const s32 liSkill = static_cast<s32>(mpCache->GetBurnoutSkillsManager()->GetCurrentSkill());
        if (mbFreeburnChallengeRunning)
        {
            meCurrentChallengeDataType = KI_NUM_FREEBURN_CHALLENGE_TYPE_STRINGS;
            mbFreeburnChallengeRunning = false;
        }
        else if (meLastFrameSkillState == liSkill)
        {
            return;
        }

        if (liSkill == BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_COUNT)
        {
            mTitleBarMCR.GotoAndPlayLabel("invisible");
            mPageIconMCR.GotoAndPlayLabel("invisible");
            SetTitleText("");
            SetSkillsText("");
            SetValuesDirty();
            meLastFrameSkillState = BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_COUNT;
        }
        else if (liSkill == BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_EXTRA_12 ||
                 liSkill == BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_EXTRA_13)
        {
            mTitleBarMCR.GotoAndPlayLabel("skillsRR");
            mPageIconMCR.GotoAndPlayLabel("invisible");
            SetTitleText("$BURNOUT_SKILLS_TITLE_ON_ROAD");
            SetSkillsText("");
            SetValuesDirty();
            meLastFrameSkillState = liSkill;
        }
        else
        {
            char lacPageIcon[16];
            CgsCore::SnPrintf(lacPageIcon, sizeof(lacPageIcon), "Skill_%d",
                              (liSkill > BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_POWER_PARKING)
                                  ? static_cast<s32>(BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_POWER_PARKING)
                                  : liSkill);
            mTitleBarMCR.GotoAndPlayLabel("skills");
            mPageIconMCR.GotoAndPlayLabel(lacPageIcon);
            SetTitleText("$BURNOUT_SKILLS_TITLE");
            SetSkillsText(KAPC_BURNOUT_SKILLS_TITLE_STRINGS[liSkill]);
            SetValuesDirty();
            meLastFrameSkillState = liSkill;
        }
    }

    // ------------------------------------------------ FillOutInActiveRaceCarOrder
    // Online only (the cache's online byte): one row per active race-car lane, in lane order.
    void PlayerPositionTableComponent::FillOutInActiveRaceCarOrder(const GuiEventRaceDistanceRemaining* lpDistanceEvent)
    {
        if (!mpCache->IsOnline())
            return;

        for (EActiveRaceCarIndex leCar = E_ACTIVE_RACE_CAR_INDEX_0;
             leCar < E_ACTIVE_RACE_CAR_INDEX_COUNT;
             leCar++)
        {
            FillOutOnlineData(leCar);
            FillOutOnlineValueData(leCar, lpDistanceEvent);
        }
    }

    // ------------------------------------------------ FillOutOnlineData
    // Fill row leCurrentActiveRaceCar from the cached in-game record of the player driving that
    // car (nothing when no player drives it): headset, name, live-revenge arrow, team,
    // challenge progress, and the row colour. The record lookup is the inlined by-race-car
    // scan of the cache's eight player records.
    void PlayerPositionTableComponent::FillOutOnlineData(EActiveRaceCarIndex leCurrentActiveRaceCar)
    {
        CGS_ASSERT(mpCache != NULL, "mpCache");

        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerInfo = NULL;
        for (s32 liPlayer = 0; liPlayer < 8; ++liPlayer)
        {
            const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpCandidate =
                mpCache->GetOnlinePlayerInfo(liPlayer);
            if (lpCandidate->meActiveRaceCarIndex == leCurrentActiveRaceCar)
            {
                lpPlayerInfo = lpCandidate;
                break;
            }
        }
        if (lpPlayerInfo == NULL)
            return;

        PlayerPositionSingleData& lrData = maPlayerSingleData[leCurrentActiveRaceCar];

        switch (static_cast<u32>(lpPlayerInfo->meVOIPStatus))
        {
        case 0:  lrData.meHeadsetStatus = E_HEADSETSTATUS_OFF;       break;
        case 1:  lrData.meHeadsetStatus = E_HEADSETSTATUS_ON;        break;
        case 2:  lrData.meHeadsetStatus = E_HEADSETSTATUS_ACTIVE;    break;
        default: lrData.meHeadsetStatus = E_HEADSETSTATUS_INVISIBLE; break;
        }
        lrData.meAwardState = E_AWARDSTATUS_NONE;
        lrData.mPlayerName.Construct(lpPlayerInfo->mPlayerName.macName);
        lrData.meNameType = 0;

        if (lpPlayerInfo->mbMarkedMan)
            lrData.meRevengeStatus = E_REVENGESTATUS_MARKEDMAN;
        else if (lpPlayerInfo->mLiveRevengeRelationship.IsPlayerAheadInCurrentRelationship())
            lrData.meRevengeStatus = E_REVENGESTATUS_UP;
        else if (lpPlayerInfo->mLiveRevengeRelationship.IsRivalAheadInCurrentRelationship())
            lrData.meRevengeStatus = E_REVENGESTATUS_DOWN;
        else
            lrData.meRevengeStatus = E_REVENGESTATUS_INVISIBLE;

        lrData.meTeam = static_cast<BrnGameState::GameStateModuleIO::EPlayerTeam>(
            mpCache->GetCurrentOnlinePlayerTeam(leCurrentActiveRaceCar));

        if (mpChallengeManager->IsStarted())
        {
            lrData.meRevengeStatus = E_REVENGESTATUS_INVISIBLE;
            switch (mpChallengeManager->GetCurrentSuccessForARCI(leCurrentActiveRaceCar))
            {
            case FreeburnChallengeManager::E_FREEBURN_CHALLENGE_SUCCESS_NONE:
                lrData.mePlayerType = E_PLAYERTYPES_FREEBURN_CHALLENGE_OFF;
                break;
            case FreeburnChallengeManager::E_FREEBURN_CHALLENGE_SUCCESS_NOT_IN_CHALLENGE:
                lrData.mePlayerType = E_PLAYERTYPES_FREEBURN_CHALLENGE_NOT_ACTIVE;
                break;
            case FreeburnChallengeManager::E_FREEBURN_CHALLENGE_SUCCESS_CONTRIBUTING:
                lrData.mePlayerType = E_PLAYERTYPES_FREEBURN_CHALLENGE_IN_PROGRESS;
                break;
            case FreeburnChallengeManager::E_FREEBURN_CHALLENGE_SUCCESS_DONE:
                lrData.mePlayerType = E_PLAYERTYPES_FREEBURN_CHALLENGE_COMPLETE;
                break;
            default:
                CGS_ASSERT(false, "Invalid type of challenge success : ");
                lrData.mePlayerType = E_PLAYERTYPES_BASIC;
                break;
            }
        }
        else
        {
            lrData.mePlayerType = E_PLAYERTYPES_BASIC;
        }

        lrData.meActiveRaceCarIndex = leCurrentActiveRaceCar;

        // The race and the two free-burn lobby modes always take the lobby colour; every other
        // mode first shows disconnected / eliminated players in their own colours.
        const s32 liGameMode = mpCache->GetGameMode();
        const bool lbLobbyColour =
            (liGameMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_RACE) ||
            IsOnlineFreeBurnLobby(liGameMode);
        if (!lbLobbyColour)
        {
            if (mpCache->GetOnlinePlayerDisconnected(leCurrentActiveRaceCar))
            {
                lrData.mePlayerColour = KI_PLAYER_COLOUR_DISCONNECTED;
                return;
            }
            if (mpCache->IsOnlinePlayerEliminated(leCurrentActiveRaceCar) || lpPlayerInfo->mbIsEliminated)
            {
                lrData.mePlayerColour = KI_PLAYER_COLOUR_ELIMINATED;
                return;
            }
        }
        lrData.mePlayerColour = mpCache->GetOnlinePlayerColourFromARCI(leCurrentActiveRaceCar);
    }

    // ------------------------------------------------ FillOutOnlineValueData
    // The number a row sorts and shows, per game mode.
    void PlayerPositionTableComponent::FillOutOnlineValueData(EActiveRaceCarIndex leCurrentActiveRaceCar,
                                                              const GuiEventRaceDistanceRemaining* lpDistanceEvent)
    {
        f32& lrfValue = maPlayerSingleData[leCurrentActiveRaceCar].mfTableValue;

        switch (meGameMode)
        {
        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_RACE:
            lrfValue = lpDistanceEvent->mafDistanceToFinish[leCurrentActiveRaceCar];
            break;

        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE:
            if (mpCache->GetCurrentOnlinePlayerTeam(leCurrentActiveRaceCar) ==
                BrnGameState::GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM)
                lrfValue = lpDistanceEvent->mafDistanceToFinish[leCurrentActiveRaceCar];
            else
                lrfValue = 0.0f;
            break;

        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FUGITIVE:
        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN:
        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_MODE_END:
            lrfValue = static_cast<f32>(lpDistanceEvent->maiOnlineStuntScore[leCurrentActiveRaceCar]);
            break;

        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN:
            lrfValue = static_cast<f32>(static_cast<s32>(mpCache->GetNumActiveLandmarks()) -
                                        mpCache->GetNumRemainingCheckpoints());
            break;

        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY:
        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME:
            if (mpCache->GetActiveRoadRule() == 0 && mpChallengeManager->IsStarted())
            {
                lrfValue = mpChallengeManager->GetCurrentContributionForARCI(leCurrentActiveRaceCar);
            }
            else
            {
                const BurnoutSkillsManager* lpSkillsManager = mpCache->GetBurnoutSkillsManager();
                if (lpSkillsManager->GetCurrentSkill() == BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_COUNT)
                    lrfValue = lpDistanceEvent->mafDistanceToFinish[leCurrentActiveRaceCar];
                else
                    lrfValue = lpSkillsManager->GetBurnoutSkillForARC(lpSkillsManager->GetCurrentSkill(),
                                                                      leCurrentActiveRaceCar);
            }
            break;

        default:
            CGS_ASSERT(false, "Unhandled game mode : ");
            lrfValue = 0.0f;
            break;
        }
    }

    // ------------------------------------------------ SortData
    // Sort the eight car rows (the total row is added after the sort) with the game mode's
    // comparator.
    void PlayerPositionTableComponent::SortData()
    {
        s32 (*lpfnCompare)(const void*, const void*) = FunctionSortHighToLow;

        switch (meGameMode)
        {
        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_RACE:
            lpfnCompare = FunctionSortLowToHigh;
            break;
        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE:
        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN:
            lpfnCompare = FunctionSortTeamLowToHigh;
            break;
        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FUGITIVE:
            AddInvisibleTeamLine();
            lpfnCompare = FunctionSortTeamHighToLow;
            break;
        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN:
        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_MODE_END:
            lpfnCompare = FunctionSortHighToLow;
            break;
        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY:
        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME:
            CGS_ASSERT(mpCache != NULL, "mpCache");
            if (mpCache->GetActiveRoadRule() == 0 && mpChallengeManager->IsStarted())
                lpfnCompare = FunctionSortAlphabetical;
            else if (mpCache->GetBurnoutSkillsManager()->GetCurrentSkill() ==
                     BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_EXTRA_12)
                lpfnCompare = FunctionSortLowToHighZeroInvalid;
            else
                lpfnCompare = FunctionSortHighToLow;
            break;
        default:
            CGS_ASSERT(false, "Unhandled game mode : ");
            lpfnCompare = FunctionSortHighToLow;
            break;
        }

        qsort(maPlayerSingleData, 8, sizeof(PlayerPositionSingleData), lpfnCompare);
    }

    // ------------------------------------------------ CountLinesAndAddTotal
    // Count the rows that carry a race car, then (free-burn lobby, no road rule, challenge
    // RUNNING or RESULTS, and a cumulative action) append the "FBC_TOTAL" row with the overall
    // contribution.
    void PlayerPositionTableComponent::CountLinesAndAddTotal()
    {
        for (s32 liRow = 0; liRow < KI_MAX_BARS_NEEDED; ++liRow)
        {
            if (maPlayerSingleData[liRow].meActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID)
                ++miCurrentBarsUsed;
        }

        if (meGameMode != BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY)
            return;
        if (mpCache->GetActiveRoadRule() != 0 || !mpChallengeManager->IsStarted())
            return;
        if (mpChallengeManager->GetCurrentAction()->GetCoopType() !=
            BrnResource::ChallengeListEntryAction::E_CHALLENGE_COOP_TYPE_CUMULATIVE)
            return;

        PlayerPositionSingleData& lrTotal = maPlayerSingleData[miCurrentBarsUsed];
        lrTotal.mPlayerName.Construct("FBC_TOTAL");
        lrTotal.meNameType           = 9;
        lrTotal.mfTableValue         = mpChallengeManager->GetCurrentContributionOverall();
        lrTotal.meHeadsetStatus      = E_HEADSETSTATUS_INVISIBLE;
        lrTotal.meAwardState         = E_AWARDSTATUS_NONE;
        lrTotal.meTeam               = BrnGameState::GameStateModuleIO::E_PLAYER_TEAM_NONE;
        lrTotal.mePlayerColour       = 9;
        lrTotal.mePlayerType         = E_PLAYERTYPES_FREEBURN_CHALLENGE_TOTAL;
        lrTotal.meActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_COUNT;
        lrTotal.meRevengeStatus      = E_REVENGESTATUS_INVISIBLE;
        ++miCurrentBarsUsed;
    }

    // ------------------------------------------------ DisplayData
    // Push the used rows into the bar components (update, then re-render the value); the
    // unused bars get an invisible row.
    void PlayerPositionTableComponent::DisplayData()
    {
        s32 liBar = 0;
        for (; liBar < miCurrentBarsUsed; ++liBar)
        {
            maPlayerComponents[liBar].Update(&maPlayerSingleData[liBar]);
            maPlayerComponents[liBar].RenderValue();
        }
        for (; liBar < KI_MAX_BARS_NEEDED; ++liBar)
        {
            PlayerPositionSingleData lInvisible;
            lInvisible.mPlayerName.macName[0] = 0;
            lInvisible.meNameType             = 0;
            lInvisible.mfTableValue           = 0.0f;
            lInvisible.mePlayerType           = E_PLAYERTYPES_INVISIBLE;
            lInvisible.meActiveRaceCarIndex   = E_ACTIVE_RACE_CAR_INDEX_INVALID;
            lInvisible.meHeadsetStatus        = E_HEADSETSTATUS_INVISIBLE;
            lInvisible.meRevengeStatus        = E_REVENGESTATUS_INVISIBLE;
            lInvisible.meAwardState           = E_AWARDSTATUS_NONE;
            lInvisible.meTeam                 = BrnGameState::GameStateModuleIO::E_PLAYER_TEAM_NONE;
            lInvisible.mePlayerColour         = 0;
            lInvisible.miHoldingSlot          = -1;
            maPlayerComponents[liBar].Update(&lInvisible);
        }
    }

    // ------------------------------------------------ the qsort comparators
    // Shared lead-in: empty rows sink; two disconnected rows compare by name; a disconnected
    // row sinks below a connected one.
    s32 PlayerPositionTableComponent::FunctionSortHighToLow(const void* lp1, const void* lp2)
    {
        const PlayerPositionSingleData* lpPlayer1 = static_cast<const PlayerPositionSingleData*>(lp1);
        const PlayerPositionSingleData* lpPlayer2 = static_cast<const PlayerPositionSingleData*>(lp2);

        if (lpPlayer1->mPlayerName.macName[0] == 0)
            return lpPlayer2->mPlayerName.macName[0] != 0;
        if (lpPlayer2->mPlayerName.macName[0] == 0)
            return -1;
        if (lpPlayer1->mePlayerColour == KI_PLAYER_COLOUR_DISCONNECTED)
        {
            if (lpPlayer2->mePlayerColour == KI_PLAYER_COLOUR_DISCONNECTED)
                return CgsNetwork::UsernameCompare(lpPlayer1->mPlayerName.macName,
                                                   lpPlayer2->mPlayerName.macName);
            return 1;
        }
        if (lpPlayer2->mePlayerColour == KI_PLAYER_COLOUR_DISCONNECTED)
            return -1;

        if (lpPlayer1->mfTableValue < lpPlayer2->mfTableValue)
            return 1;
        if (lpPlayer1->mfTableValue > lpPlayer2->mfTableValue)
            return -1;
        return 0;
    }

    s32 PlayerPositionTableComponent::FunctionSortLowToHigh(const void* lp1, const void* lp2)
    {
        const PlayerPositionSingleData* lpPlayer1 = static_cast<const PlayerPositionSingleData*>(lp1);
        const PlayerPositionSingleData* lpPlayer2 = static_cast<const PlayerPositionSingleData*>(lp2);

        if (lpPlayer1->mPlayerName.macName[0] == 0)
            return lpPlayer2->mPlayerName.macName[0] != 0;
        if (lpPlayer2->mPlayerName.macName[0] == 0)
            return -1;
        if (lpPlayer1->mePlayerColour == KI_PLAYER_COLOUR_DISCONNECTED)
        {
            if (lpPlayer2->mePlayerColour == KI_PLAYER_COLOUR_DISCONNECTED)
                return CgsNetwork::UsernameCompare(lpPlayer1->mPlayerName.macName,
                                                   lpPlayer2->mPlayerName.macName);
            return 1;
        }
        if (lpPlayer2->mePlayerColour == KI_PLAYER_COLOUR_DISCONNECTED)
            return -1;

        if (lpPlayer1->mfTableValue > lpPlayer2->mfTableValue)
            return 1;
        if (lpPlayer1->mfTableValue < lpPlayer2->mfTableValue)
            return -1;
        return 0;
    }

    // Low-to-high, but a zero value (no score yet) sinks below every scored row.
    s32 PlayerPositionTableComponent::FunctionSortLowToHighZeroInvalid(const void* lp1, const void* lp2)
    {
        const PlayerPositionSingleData* lpPlayer1 = static_cast<const PlayerPositionSingleData*>(lp1);
        const PlayerPositionSingleData* lpPlayer2 = static_cast<const PlayerPositionSingleData*>(lp2);

        if (lpPlayer1->mPlayerName.macName[0] == 0)
            return lpPlayer2->mPlayerName.macName[0] != 0;
        if (lpPlayer2->mPlayerName.macName[0] == 0)
            return -1;
        if (lpPlayer1->mePlayerColour == KI_PLAYER_COLOUR_DISCONNECTED)
        {
            if (lpPlayer2->mePlayerColour == KI_PLAYER_COLOUR_DISCONNECTED)
                return CgsNetwork::UsernameCompare(lpPlayer1->mPlayerName.macName,
                                                   lpPlayer2->mPlayerName.macName);
            return 1;
        }
        if (lpPlayer2->mePlayerColour == KI_PLAYER_COLOUR_DISCONNECTED)
            return -1;

        if (lpPlayer1->mfTableValue == 0.0f)
            return 1;
        if (lpPlayer2->mfTableValue == 0.0f)
            return -1;
        if (lpPlayer1->mfTableValue > lpPlayer2->mfTableValue)
            return 1;
        if (lpPlayer1->mfTableValue < lpPlayer2->mfTableValue)
            return -1;
        return 0;
    }

    // Blue team first, then low-to-high within a team.
    s32 PlayerPositionTableComponent::FunctionSortTeamLowToHigh(const void* lp1, const void* lp2)
    {
        const PlayerPositionSingleData* lpPlayer1 = static_cast<const PlayerPositionSingleData*>(lp1);
        const PlayerPositionSingleData* lpPlayer2 = static_cast<const PlayerPositionSingleData*>(lp2);

        if (lpPlayer1->mPlayerName.macName[0] == 0)
            return lpPlayer2->mPlayerName.macName[0] != 0;
        if (lpPlayer2->mPlayerName.macName[0] == 0)
            return -1;
        if (lpPlayer1->mePlayerColour == KI_PLAYER_COLOUR_DISCONNECTED)
        {
            if (lpPlayer2->mePlayerColour == KI_PLAYER_COLOUR_DISCONNECTED)
                return CgsNetwork::UsernameCompare(lpPlayer1->mPlayerName.macName,
                                                   lpPlayer2->mPlayerName.macName);
            return 1;
        }
        if (lpPlayer2->mePlayerColour == KI_PLAYER_COLOUR_DISCONNECTED)
            return -1;

        if (lpPlayer1->meTeam != lpPlayer2->meTeam)
        {
            if (lpPlayer1->meTeam == BrnGameState::GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM)
                return -1;
            return 1;
        }
        if (lpPlayer1->mfTableValue > lpPlayer2->mfTableValue)
            return 1;
        if (lpPlayer1->mfTableValue < lpPlayer2->mfTableValue)
            return -1;
        return 0;
    }

    // By name (the challenge page lists players alphabetically).
    s32 PlayerPositionTableComponent::FunctionSortAlphabetical(const void* lp1, const void* lp2)
    {
        const PlayerPositionSingleData* lpPlayer1 = static_cast<const PlayerPositionSingleData*>(lp1);
        const PlayerPositionSingleData* lpPlayer2 = static_cast<const PlayerPositionSingleData*>(lp2);

        if (lpPlayer1->mPlayerName.macName[0] == 0)
            return lpPlayer2->mPlayerName.macName[0] != 0;
        if (lpPlayer2->mPlayerName.macName[0] == 0)
            return -1;
        if (lpPlayer1->mePlayerColour == KI_PLAYER_COLOUR_DISCONNECTED)
        {
            if (lpPlayer2->mePlayerColour == KI_PLAYER_COLOUR_DISCONNECTED)
                return CgsNetwork::UsernameCompare(lpPlayer1->mPlayerName.macName,
                                                   lpPlayer2->mPlayerName.macName);
            return 1;
        }
        if (lpPlayer2->mePlayerColour == KI_PLAYER_COLOUR_DISCONNECTED)
            return -1;

        return CgsNetwork::UsernameCompare(lpPlayer1->mPlayerName.macName,
                                           lpPlayer2->mPlayerName.macName);
    }
}
