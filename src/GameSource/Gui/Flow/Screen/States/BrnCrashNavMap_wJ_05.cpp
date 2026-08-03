// ===================================================================================
// BrnGui::CrashNavMap -- wave-J partfile 05: the button-prompt state machine and the
// road-rule scoreboard builder.
//   UpdateButtonPrompts @0x824B68B8  (cpp:1010 / 1015 / 1016 assert sites)
//   UpdateRoadRule      @0x824B6DC0  (cpp:1747 / 1861 assert sites)
//
// Both bodies are landed. The shared-header declarations this group filed requests for
// have since been applied, so what follows is spelled the way the committed headers spell
// it. Where the wave-J request and the DWARF disagreed, the DWARF won:
//
//   * The panel-type word at CrashNavPanel+0x90 is `PanelType mePanelType`, read through
//     `PanelType GetPanelActiveFilterMode()` (DWARF BrnCrashNavPanel.h:239 / h:314), NOT
//     the `EShowingMode`/`GetPanelType()` spelling the request invented -- and the enum
//     has SEVEN values (E_PANEL_EVENT/DRIVETHRU/ROADSIGN/RIVALS/SELECTABLE_COUNT/GENERIC/
//     COUNT, DWARF h:69), not the two the request asked to add. DecFIGS lists exactly one
//     GetPanelActiveFilterMode call for this function (dwarfdump/_compile/
//     BrnGuiScreenUnity.cpp:3317), so the panel type is read ONCE per arm into a local --
//     the X360 sank that load into all three arms.
//   * `void SetRoadPanelData(const char*, RoadPanelData&)` takes a NON-const reference
//     (DWARF BrnCrashNavPanel.h, BrnCrashNavPanel.cpp:675).
//   * RoadPanelData / PanelBox carry the DWARF names (mPanels, mabPlayerBestScore,
//     mPlayerScores, mNames, mScores, KI_ROADRULE_COUNT, KI_PANEL_TEXT_LENGTH -- DWARF
//     BrnRoadPanel.h:52..85); the earlier draft had consumer-named every one of them.
//   * `bool IsRoadRuleFriendSelected() const` -- the `const` is DWARF-attested
//     (BrnCrashNavPanel.cpp:1152), not inferred.
//   * `ChallengeData::CompareScores` is static: the X360 function @0x82676640 takes no
//     `this` at all -- its whole body uses r3 as the comparator-table index and shifts
//     r4/r5 down before tail-calling.
//   * GuiCache's road-rule-friend-scores gate at +0x4B50 is read with **lbz** at both
//     sites, so the one-byte carve does not disturb mbOnlineMatchRanked @+0x4B51.
//
// The measurement dumps this wave produced live at scratchpad/waveJ/asm_updateroadrule.txt
// (the raw UpdateRoadRule listing Hex-Rays mangles), scratchpad/waveJ/g05_wJ.txt (the 0.001
// scale float, the CompareScores body, RoadPanelData/PanelBox/SetRoadPanelData),
// scratchpad/waveJ/g05_strs.txt (the strings IDA truncated) and
// scratchpad/waveJ/g05_panel.txt (the CrashNavPanel setters).
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CgsDev::Assert machinery
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"       // CgsGui::GuiComponent (AnimationComponent's base)
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // SatNavIconInfo::SatNavIconType
#include "GameSource/Gui/Flow/Screen/Components/BrnCrashNavPanel.h"       // BrnGui::CrashNavPanel

namespace BrnGui
{
    namespace
    {
        // @0x82F26EC0 -- the apt view-state name per EVisibleAnimationStates, dumped
        // big-endian (scratchpad/waveJ/crashnav_consts.txt). DWARF BrnCrashNavMap.cpp:93.
        const char* const KAPC_VISIBLE_ANIMATION_STATES[CrashNavMap::E_VISIBLE_ANIMATION_STATES_COUNT] =
        {
            "visible",     // E_VISIBLE_ANIMATION_STATES_VISIBLE
            "invisible",   // E_VISIBLE_ANIMATION_STATES_INVISIBLE
        };

        // @0x82F26EC8 -- the apt view-state name per EButtonPromptAnimationStates, dumped
        // big-endian (same dump). DWARF BrnCrashNavMap.cpp:99.
        const char* const KAPC_BUTTON_PROMPT_ANIMATION_STATES[CrashNavMap::E_BUTTON_PROMPT_ANIMATION_STATES_COUNT] =
        {
            "OfflineNone",                 //  0 OFFLINE_NONE_SELECTED
            "OfflineEvent",                //  1 OFFLINE_EVENT
            "RoadScoresOffline",           //  2 ROADS_OFFLINE_SCORES
            "RoadScoresOnline",            //  3 ROADS_ONLINE_SCORES
            "FreeburnLobby",               //  4 ONLINE
            "FreeburnLobbyPause",          //  5 ONLINE_PAUSE
            "FreeburnLobbyRival",          //  6 ONLINE_RIVAL
            "FreeburnLobbyPauseRival",     //  7 ONLINE_PAUSE_RIVAL
            "RoadScoresOfflineX360",       //  8 ROADS_OFFLINE_SCORES_X360
            "RoadScoresOfflineBack",       //  9 ROADS_OFFLINE_SCORES_BACK
            "RoadScoresOnlineBack",        // 10 ROADS_ONLINE_SCORES_BACK
            "RoadScoresOfflineX360Back",   // 11 ROADS_OFFLINE_SCORES_X360_BACK
        };

        // The two GuiEventSetRoadRuleScoreMode::ERoadPanelModes values this body branches
        // on (X360 `cmpwi r3, 1` @0x824B6B30 / `cmpwi r3, 0` @0x824B6B74). That enum is
        // catalogued but not reconstructed (BrnGuiDemangledEventTypes.h:172 carries only
        // the wire id 330 and an opaque 4-byte payload), and BrnGuiCache.cpp:1046 attests
        // only E_ROAD_PANEL_MODE_COUNT == 2, so the two members are un-named here.
        //
        // MEASURED, NOT EXPLAINED: score mode 1 selects the "RoadScoresOffline*" prompt
        // sets and score mode 0 selects the "RoadScoresOnline*" ones -- which is the
        // OPPOSITE polarity to BrnRoadPanel.h's KI_ROAD_PANEL_MODE_ONLINE == 1 (that
        // constant comes from RoadPanel::GetSelectedFriendName's own assert, so both
        // readings are attested). Flagged rather than reconciled; nothing here depends on
        // which name is right.
        const s32 KI_ROAD_PANEL_SCORE_MODE_OFFLINE_PROMPTS = 1;
        const s32 KI_ROAD_PANEL_SCORE_MODE_ONLINE_PROMPTS  = 0;

        const char* const KPC_ASSERT_FILE =
            "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnCrashNavMap.cpp";
    }

    // ------------------------------------------------- UpdateButtonPrompts @0x824B68B8
    //
    // Choose the title-bar visibility state and the button-prompt set for the map screen,
    // and push them at the two animation components whenever either changes. Pure state
    // machine: no side effect other than the two latched members and the two apt
    // transitions.
    //
    // The decision is (screen type) x (panel type) x (what the cursor has selected), with
    // the road-rule scoreboard arm additionally keyed on the panel's score mode, a cache
    // gate byte and whether a friend row is selected. The three screen types share the
    // road-scores sub-tree's SHAPE but not its constants -- the offline and online screens
    // land on the plain prompt sets (2 / 3 / 8) and the from-pause screen on the "Back"
    // flavours (9 / 10 / 11) -- so the X360 has two copies of it and so does this.
    void CrashNavMap::UpdateButtonPrompts()
    {
        // `li r28, 2` / `li r29, 0xC` at 0x824B68CC. Both start at their COUNT sentinel;
        // the post-switch asserts below fire on any arm that fails to set them.
        EVisibleAnimationStates      leTitleButtonsState      = E_VISIBLE_ANIMATION_STATES_COUNT;
        EButtonPromptAnimationStates leNavigationButtonsState = E_BUTTON_PROMPT_ANIMATION_STATES_COUNT;

        switch (meScreenType)   // lwz +0x60DC
        {
        case E_SCREEN_TYPE_OFFLINE:   // loc_824B6B84
            {
                leTitleButtonsState = E_VISIBLE_ANIMATION_STATES_VISIBLE;   // li r28, 0

                const CrashNavPanel::PanelType lePanelType = mCrashNavPanel.GetPanelActiveFilterMode();   // lwz +0x770

                if (lePanelType == CrashNavPanel::E_PANEL_EVENT)
                {
                    // The X360 computes this with cntlzw/extrwi/xori -- the compiler's
                    // branchless form of `muHoveredEventID != 0`, de-optimised back.
                    leNavigationButtonsState = (muHoveredEventID != 0)
                        ? E_BUTTON_PROMPT_ANIMATION_STATES_OFFLINE_EVENT
                        : E_BUTTON_PROMPT_ANIMATION_STATES_OFFLINE_NONE_SELECTED;
                }
                else if (lePanelType == CrashNavPanel::E_PANEL_ROADSIGN &&
                         (mpLockedIconName != NULL ||          // lwz  +0x608C
                          mbLocalPlayerSelected ||             // lbz  +0x60D8
                          meCursorMode == E_CURSORMODE_PANNING))   // lwz +0x5F50, cmpwi 4
                {
                    // loc_824B6B24 -- the plain (non-"Back") road-scores prompt sets.
                    if (mCrashNavPanel.GetRoadPanelScoreMode() == KI_ROAD_PANEL_SCORE_MODE_OFFLINE_PROMPTS)
                    {
                        leNavigationButtonsState =
                            (mpGuiCache->AreRoadRuleFriendScoresAvailable() &&
                             mCrashNavPanel.IsRoadRuleFriendSelected())
                                ? E_BUTTON_PROMPT_ANIMATION_STATES_ROADS_OFFLINE_SCORES_X360
                                : E_BUTTON_PROMPT_ANIMATION_STATES_ROADS_OFFLINE_SCORES;
                    }
                    else if (mCrashNavPanel.GetRoadPanelScoreMode() == KI_ROAD_PANEL_SCORE_MODE_ONLINE_PROMPTS)
                    {
                        leNavigationButtonsState = E_BUTTON_PROMPT_ANIMATION_STATES_ROADS_ONLINE_SCORES;
                    }
                    // Any other score mode leaves the COUNT sentinel in place and trips
                    // the cpp:1016 assert below (X360: `bne loc_824B6928`).
                }
                else
                {
                    leNavigationButtonsState = E_BUTTON_PROMPT_ANIMATION_STATES_OFFLINE_NONE_SELECTED;
                }
            }
            break;

        case E_SCREEN_TYPE_ONLINE:   // loc_824B6A98
            {
                leTitleButtonsState = E_VISIBLE_ANIMATION_STATES_INVISIBLE;   // li r28, 1

                const CrashNavPanel::PanelType lePanelType = mCrashNavPanel.GetPanelActiveFilterMode();

                if (lePanelType == CrashNavPanel::E_PANEL_ROADSIGN)
                {
                    if (mpLockedIconName != NULL || mbLocalPlayerSelected ||
                        meCursorMode == E_CURSORMODE_PANNING)
                    {
                        // Shares loc_824B6B24 with the offline screen: same constants.
                        if (mCrashNavPanel.GetRoadPanelScoreMode() == KI_ROAD_PANEL_SCORE_MODE_OFFLINE_PROMPTS)
                        {
                            leNavigationButtonsState =
                                (mpGuiCache->AreRoadRuleFriendScoresAvailable() &&
                                 mCrashNavPanel.IsRoadRuleFriendSelected())
                                    ? E_BUTTON_PROMPT_ANIMATION_STATES_ROADS_OFFLINE_SCORES_X360
                                    : E_BUTTON_PROMPT_ANIMATION_STATES_ROADS_OFFLINE_SCORES;
                        }
                        else if (mCrashNavPanel.GetRoadPanelScoreMode() == KI_ROAD_PANEL_SCORE_MODE_ONLINE_PROMPTS)
                        {
                            leNavigationButtonsState = E_BUTTON_PROMPT_ANIMATION_STATES_ROADS_ONLINE_SCORES;
                        }
                    }
                    else
                    {
                        leNavigationButtonsState = E_BUTTON_PROMPT_ANIMATION_STATES_OFFLINE_NONE_SELECTED;
                    }
                }
                else if (lePanelType == CrashNavPanel::E_PANEL_RIVALS)   // loc_824B6AB8
                {
                    // A rival is "named" either because the player-name buffer is filled
                    // in or because the locked icon is a networked rival.
                    leNavigationButtonsState =
                        (mPlayerName.macName[0] != '\0' ||
                         mLockedIconInfo.GetIconType() ==
                             GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_NETWORKRIVAL)
                            ? E_BUTTON_PROMPT_ANIMATION_STATES_ONLINE_RIVAL
                            : E_BUTTON_PROMPT_ANIMATION_STATES_ONLINE;
                }
                else
                {
                    // MEASURED (`li r29, 5` @0x824B6AB0): the ONLINE screen's fall-through
                    // is the *_PAUSE prompt set, the same constant the ONLINE_FROM_PAUSE
                    // screen uses (@0x824B69CC), even though its own rival arm just above
                    // uses the non-pause 4. Transcribed as found; not "corrected".
                    leNavigationButtonsState = E_BUTTON_PROMPT_ANIMATION_STATES_ONLINE_PAUSE;
                }
            }
            break;

        case E_SCREEN_TYPE_ONLINE_FROM_PAUSE:   // loc_824B69B4
            {
                leTitleButtonsState = E_VISIBLE_ANIMATION_STATES_INVISIBLE;

                const CrashNavPanel::PanelType lePanelType = mCrashNavPanel.GetPanelActiveFilterMode();

                if (lePanelType == CrashNavPanel::E_PANEL_ROADSIGN)   // loc_824B6A08
                {
                    if (mpLockedIconName != NULL || mbLocalPlayerSelected ||
                        meCursorMode == E_CURSORMODE_PANNING)
                    {
                        // loc_824B6A38 -- the "Back" flavours of the same three states.
                        if (mCrashNavPanel.GetRoadPanelScoreMode() == KI_ROAD_PANEL_SCORE_MODE_OFFLINE_PROMPTS)
                        {
                            leNavigationButtonsState =
                                (mpGuiCache->AreRoadRuleFriendScoresAvailable() &&
                                 mCrashNavPanel.IsRoadRuleFriendSelected())
                                    ? E_BUTTON_PROMPT_ANIMATION_STATES_ROADS_OFFLINE_SCORES_X360_BACK
                                    : E_BUTTON_PROMPT_ANIMATION_STATES_ROADS_OFFLINE_SCORES_BACK;
                        }
                        else if (mCrashNavPanel.GetRoadPanelScoreMode() == KI_ROAD_PANEL_SCORE_MODE_ONLINE_PROMPTS)
                        {
                            leNavigationButtonsState = E_BUTTON_PROMPT_ANIMATION_STATES_ROADS_ONLINE_SCORES_BACK;
                        }
                    }
                    else
                    {
                        leNavigationButtonsState = E_BUTTON_PROMPT_ANIMATION_STATES_OFFLINE_NONE_SELECTED;
                    }
                }
                else if (lePanelType == CrashNavPanel::E_PANEL_RIVALS)   // loc_824B69D4
                {
                    leNavigationButtonsState =
                        (mPlayerName.macName[0] != '\0' ||
                         mLockedIconInfo.GetIconType() ==
                             GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_NETWORKRIVAL)
                            ? E_BUTTON_PROMPT_ANIMATION_STATES_ONLINE_PAUSE_RIVAL
                            : E_BUTTON_PROMPT_ANIMATION_STATES_ONLINE_PAUSE;
                }
                else
                {
                    leNavigationButtonsState = E_BUTTON_PROMPT_ANIMATION_STATES_ONLINE_PAUSE;
                }
            }
            break;

        default:
            // `cmplwi cr6, r11, 3` + `bge` -- anything from 3 up (unsigned) gets here and
            // leaves BOTH locals at their COUNT sentinel, which is why the X360 emits the
            // two asserts below unconditionally on this path.
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Unknown screen type!\n", KPC_ASSERT_FILE, 1010);   // li r5, 0x3F2
            CgsDev::Assert::EndAssert();
            break;
        }

        if (E_VISIBLE_ANIMATION_STATES_COUNT == leTitleButtonsState)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("E_VISIBLE_ANIMATION_STATES_COUNT != leTitleButtonsState",
                                       KPC_ASSERT_FILE, 1015);   // li r5, 0x3F7
            CgsDev::Assert::EndAssert();
        }

        if (E_BUTTON_PROMPT_ANIMATION_STATES_COUNT == leNavigationButtonsState)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("E_BUTTON_PROMPT_ANIMATION_STATES_COUNT != leNavigationButtonsState",
                                       KPC_ASSERT_FILE, 1016);   // li r5, 0x3F8, loc_824B6928
            CgsDev::Assert::EndAssert();
        }

        // Latch + push. Both comparisons are SIGNED (`cmpw`), and the index used to pick
        // the view-state string is the NEW value, stored first (stw then lwzx).
        //
        // INLINING REVERSED. The X360 emits `AddOutputAptViewState("apt_Transition",
        // <view state>, false)` at both sites (0x824B6978 / 0x824B69A8, with r4 =
        // aAptTransition_1 and `li r6, 0` hoisted above the branch at 0x824B6950), but the
        // clip name and the immediate flag are constants at EVERY call site precisely
        // because they belong to the callee: DecFIGS lists this function's callees as
        // `AnimationComponent::Run` TWICE and does not mention AddOutputAptViewState at all
        // (dwarfdump/_compile/BrnGuiScreenUnity.cpp:3315-3316), and DWARF
        // BrnAnimationComponent.h:64 gives Run exactly one `const char*` parameter. So the
        // source called Run and the compiler folded its one-line body in. Run has no X360
        // symbol of its own -- it is a LINK-TIME EXTERNAL that grows a body with the
        // AnimationComponent TU.
        if (leTitleButtonsState != meTitleButtonsState)   // lwz +0x60E0
        {
            meTitleButtonsState = leTitleButtonsState;
            mTitleButtonsAnimation.Run(   // r3 = this + 0x5F54
                KAPC_VISIBLE_ANIMATION_STATES[leTitleButtonsState]);
        }

        if (leNavigationButtonsState != meNavigationButtonsState)   // lwz +0x60E4
        {
            meNavigationButtonsState = leNavigationButtonsState;
            mButtonPromptsAnimation.Run(   // r3 = this + 0x5FE0
                KAPC_BUTTON_PROMPT_ANIMATION_STATES[leNavigationButtonsState]);
        }
    }
}

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CgsDev::Assert machinery
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream (streamed assert)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameSource/GameState/BrnCgsPlayerName.h"                        // CgsNetwork::PlayerName
#include "GameSource/Gui/Flow/Screen/Components/BrnCrashNavPanel.h"       // CrashNavPanel::SetRoadPanelData
#include "GameSource/Gui/Flow/Screen/Components/BrnRoadPanel.h"           // BrnGui::RoadPanelData
#include "SharedClasses/StreetData/BrnChallengeData.h"                    // ChallengeData::CompareScores / ScoreType

namespace BrnGui
{
    namespace
    {
        // ---- in-queue payload view (the state's input queue hands the header-stripped
        // payload, per the wave-H/I convention) ----
        //
        // Event 334 (road-rule scoreboard data). Field placement is measured off this
        // function's own reads; the shape corroborates the PS3 DWARF GuiEventRoadRuleData
        // = GuiEvent<330>{ CgsID; RoadRuleType[2] }.
        //
        // NOTE: BrnGuiDemangledEventTypes.h:148 catalogues id 334 as a 12-byte header plus
        // a 76-byte opaque payload. The reads here span 88 bytes of payload (rule 1's
        // mRivalId is the last 8 bytes at payload +80), so that catalogue entry is 12
        // bytes short -- the same kind of mismatch the wave-J spec already recorded for
        // id 332. The asm wins; this view stays file-local.
        struct GuiEventRoadRuleDataPayload : public CgsModule::Event
        {
            struct RoadRuleType
            {
                // [0] the current road ruler's (rival's) score, [1] the local player's
                // own score, [2] the selected friend's score. Indices are measured from
                // which value feeds which PanelBox field, below.
                s32                    maiScores[3];   // rule +0x00 / +0x04 / +0x08
                CgsNetwork::PlayerName mFriendName;    // rule +0x0C
                CgsID                  mRivalId;       // rule +0x20 (ld / cmpldi)
            };

            CgsID        mRoadID;                                        // payload +0x00
            RoadRuleType maRules[BrnStreetData::E_SCORE_TYPE_COUNT];     // payload +0x08, 40-byte stride
        };

        // @0x820662C4 -- which scoreboard box each score type is drawn in. DWARF
        // BrnCrashNavMap.cpp:47. Dumped big-endian: { 0, 1 }
        // (scratchpad/waveJ/crashnav_consts.txt).
        const s32 KAI_ROAD_RULE_DISPLAY_POS[BrnStreetData::E_SCORE_TYPE_COUNT] = { 0, 1 };

        // @0x8204D370 -- the one numeric format this function uses, for every score of
        // both score types.
        const char* const KPC_SCORE_FORMAT = "%3.3f";

        // @0x8206796C -- the road-ruler name line: the locked road-sign icon's name run
        // through the language table.
        const char* const KPC_ROAD_RULER_NAME_FORMAT = "$RULERQ_%s";

        // @0x82025D48 -- the placeholder drawn for an absent friend / absent road ruler.
        const char* const KPC_NO_ENTRY = "-";

        // f31, loaded once at 0x824B6E28 from flt_82013F90 (0x3A83126F). Road-rule TIME
        // scores are stored in milliseconds and shown in seconds.
        // FLAG consumer-named: the float is a bare rodata literal with no DWARF symbol.
        const f32 KF_MILLISECONDS_TO_SECONDS = 0.001f;

        // `li r4, 0x40` at every SPrintf call site in this function.
        const s32 KI_SCORE_TEXT_LEN = 64;

        // KPC_ASSERT_FILE is already defined by the UpdateButtonPrompts block above --
        // this partfile concatenates two functions that each carried their own
        // file-local block; one definition per TU is enough.
    }

    // ------------------------------------------------------- UpdateRoadRule @0x824B6DC0
    //
    // Turn an incoming road-rule scoreboard payload into the two-row panel the crash-nav
    // map shows for a selected road sign: one row per score type (TIME then CRASH), each
    // row holding the local player's score plus the friend row and the road-ruler row,
    // and a "you have beaten this" flag for each of those two.
    //
    // The loop index doubles as the BrnStreetData::ScoreType -- it is what the X360 hands
    // CompareScores as its comparator-table index (see banner note (3)) -- which is also
    // why only iteration 0 rescales its scores from milliseconds to seconds.
    void CrashNavMap::UpdateRoadRule(const CgsModule::Event* lpEvent)
    {
        if (lpEvent == NULL)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpRoadRule", KPC_ASSERT_FILE, 1747);   // li r5, 0x6D3
            CgsDev::Assert::EndAssert();
        }

        const GuiEventRoadRuleDataPayload* const lpRoadRule =
            static_cast<const GuiEventRoadRuleDataPayload*>(lpEvent);

        RoadPanelData lRoadPanelData;
        lRoadPanelData.Construct();   // @0x824B6E08

        for (s32 liScoreType = 0; liScoreType < BrnStreetData::E_SCORE_TYPE_COUNT; ++liScoreType)
        {
            // `cmpwi r28, 2` at 0x824B711C -- the loop really does run over the two score
            // types, and maRules / KAI_ROAD_RULE_DISPLAY_POS are both sized by them.
            const GuiEventRoadRuleDataPayload::RoadRuleType& lRule = lpRoadRule->maRules[liScoreType];

            bool lbScore1Beaten = false;   // r24 -- the friend row
            bool lbScore2Beaten = false;   // r25 -- the road-ruler row

            char lacName1[KI_SCORE_TEXT_LEN];
            char lacName2[KI_SCORE_TEXT_LEN];
            char lacYourScore[KI_SCORE_TEXT_LEN];
            char lacScore1[KI_SCORE_TEXT_LEN];
            char lacScore2[KI_SCORE_TEXT_LEN];

            s32 liScore1 = 0;   // r27 -- the friend's score, 0 when there is no friend row
            s32 liScore2 = 0;   // r30 -- the road ruler's score, 0 when there is none

            // -------- row 1: the selected friend --------
            if (lRule.maiScores[2] > 0)
            {
                // The name is passed to SPrintf as the FORMAT string, not as a "%s"
                // argument -- exactly as the X360 does it (banner note above).
                CgsCore::SPrintf(lacName1, KI_SCORE_TEXT_LEN, lRule.mFriendName.macName);
                liScore1 = lRule.maiScores[2];
                lacName1[KI_SCORE_TEXT_LEN - 1] = '\0';

                if (lRule.maiScores[1] > 0 &&
                    BrnStreetData::ChallengeData::CompareScores(
                        static_cast<BrnStreetData::ScoreType>(liScoreType),
                        lRule.maiScores[1], lRule.maiScores[2]) <= 0)
                {
                    // CompareScores returns <= 0 when the first score is the better one
                    // (the polarity every committed caller uses), so the local player has
                    // matched or beaten the friend.
                    lbScore1Beaten = true;
                }
            }
            else
            {
                // No friend score on record: any score of the player's own counts.
                if (lRule.maiScores[1] > 0)
                {
                    lbScore1Beaten = true;
                }

                CgsCore::SPrintf(lacName1, KI_SCORE_TEXT_LEN, KPC_NO_ENTRY);
                liScore1 = 0;
                lacName1[KI_SCORE_TEXT_LEN - 1] = '\0';
            }

            // -------- row 2: the current road ruler --------
            if (lRule.mRivalId != 0)
            {
                CgsCore::SPrintf(lacName2, KI_SCORE_TEXT_LEN, KPC_ROAD_RULER_NAME_FORMAT, mpLockedIconName);
                liScore2 = lRule.maiScores[0];
                lacName2[KI_SCORE_TEXT_LEN - 1] = '\0';

                if (lRule.maiScores[1] > 0 &&
                    BrnStreetData::ChallengeData::CompareScores(
                        static_cast<BrnStreetData::ScoreType>(liScoreType),
                        lRule.maiScores[1], lRule.maiScores[0]) <= 0)
                {
                    lbScore2Beaten = true;
                }
            }
            else
            {
                if (lRule.maiScores[1] > 0)
                {
                    lbScore2Beaten = true;
                }

                CgsCore::SPrintf(lacName2, KI_SCORE_TEXT_LEN, KPC_NO_ENTRY);
                liScore2 = 0;
                lacName2[KI_SCORE_TEXT_LEN - 1] = '\0';
            }

            // -------- the three score strings --------
            if (liScoreType == BrnStreetData::E_SCORE_TYPE_TIME)
            {
                // Times are held in milliseconds; "%3.3f" of value/1000 renders seconds.
                CgsCore::SPrintf(lacYourScore, KI_SCORE_TEXT_LEN, KPC_SCORE_FORMAT,
                                 static_cast<f32>(lRule.maiScores[1]) * KF_MILLISECONDS_TO_SECONDS);
                CgsCore::SPrintf(lacScore1, KI_SCORE_TEXT_LEN, KPC_SCORE_FORMAT,
                                 static_cast<f32>(liScore1) * KF_MILLISECONDS_TO_SECONDS);
                CgsCore::SPrintf(lacScore2, KI_SCORE_TEXT_LEN, KPC_SCORE_FORMAT,
                                 static_cast<f32>(liScore2) * KF_MILLISECONDS_TO_SECONDS);
            }
            else
            {
                // Same float format, no rescale (X360: the fcfid/frsp chain without the
                // fmuls). Not an integer format -- see the banner.
                CgsCore::SPrintf(lacYourScore, KI_SCORE_TEXT_LEN, KPC_SCORE_FORMAT,
                                 static_cast<f32>(lRule.maiScores[1]));
                CgsCore::SPrintf(lacScore1, KI_SCORE_TEXT_LEN, KPC_SCORE_FORMAT,
                                 static_cast<f32>(liScore1));
                CgsCore::SPrintf(lacScore2, KI_SCORE_TEXT_LEN, KPC_SCORE_FORMAT,
                                 static_cast<f32>(liScore2));
            }

            lacScore2[KI_SCORE_TEXT_LEN - 1] = '\0';
            lacScore1[KI_SCORE_TEXT_LEN - 1] = '\0';

            if (lacName1[0] == '\0')
            {
                // The X360 opens the assert FIRST and streams into the global
                // CgsDev::Assert::gpcMessageBuffer (sized by dword_82F32264); the
                // committed convention -- and the wave-J sibling parked files -- use a
                // stack buffer instead, which is cosmetic. The message carries no streamed
                // value, but the X360 still builds it through the StrStream sink, so the
                // stream form is kept.
                CgsDev::Assert::BeginAssert();

                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStream << "Name for the first player not set";

                CgsDev::Assert::FireAssert(lacMessageBuffer, KPC_ASSERT_FILE, 1861);   // li r5, 0x745
                CgsDev::Assert::EndAssert();
            }

            lRoadPanelData.mPanels[KAI_ROAD_RULE_DISPLAY_POS[liScoreType]].Construct(
                lacYourScore, lacName1, lacScore1, lacName2, lacScore2,
                lbScore1Beaten, lbScore2Beaten);
        }

        // The panel only takes the payload while a road sign is actually locked on and
        // this screen flavour draws road signs at all.
        if (mpLockedIconName != NULL && mbUseRoadSigns)
        {
            mCrashNavPanel.SetRoadPanelData(mpLockedIconName, lRoadPanelData);
        }
    }
}
