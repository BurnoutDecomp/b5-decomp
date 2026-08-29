// ===================================================================================
// BrnGui::RoadPanel / BrnGui::RoadPanelData  -- implementation
//   class:BrnGui::RoadPanel
//   class:BrnGui::RoadPanelData
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   RoadPanelData::PanelBox::Construct @0x82418060
//   RoadPanelData::Construct           @0x824256E0
//   RoadPanel::Construct               @0x82425738  (DWARF cpp:127)
//   RoadPanel::AppendExpectedAptComponents @0x82418410 (cpp:172)
//   RoadPanel::SetRoadPanelData        -- inlined into CrashNavPanel::SetRoadPanelData
//                                         @0x8243A9E4..0x8243AA14 (cpp:203)
//   RoadPanel::SetCurrentRule          @0x8242D1B8 (cpp:226)
//   RoadPanel::SwitchScoreMode         @0x8243A3E0 (cpp:298)
//   RoadPanel::TransitionIn            @0x8243A490 (cpp:329)
//   RoadPanel::TransitionOut           @0x824184D0 (cpp:360)
//   RoadPanel::UpdateVisibleScores     @0x82425938 (private)
//   RoadPanel::GetSignColour           @0x82418550 (private)
//   RoadPanel::GetSelectedFriendName   @0x82417FF8
//
// Member access is BY NAME; the guest offsets in the comments are the proof, not the
// mechanism. See the header banner for the full member run.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/Components/BrnRoadPanel.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // the boundary one-shot logs
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // ParameterFormatType
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // VariableEventQueue::AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache

#include <cstring>   // std::strlen

namespace BrnGui
{
    typedef CgsLanguage::LanguageManager LM;

    // The build's shared empty string literal (X360 `&unk_820046A7`).
    static const char KAC_EMPTY_STRING[] = "";

    // Scratch capacity Construct hard-codes for the per-row clip names (`li r4, 0x1F`).
    static const s32 KI_ROW_NAME_CAPACITY = 31;
    static const s32 KI_ROW_NAME_BUFFER   = 32;

    // Clip names Construct builds / passes.
    static const char KAC_ROAD_SIGN_NAME[]   = "RoadSign";
    static const char KAC_ROW_NAME_FORMAT[]  = "%s%d";
    static const char KAC_ROW_NAME_STEM[]    = "Name";
    static const char KAC_ROW_SCORE_STEM[]   = "Score";
    static const char KAC_BACKING_ANIM_NAME[]= "BestTimeBackingAnimation";
    static const char KAC_TARGET_CAPTION_NAME[] = "BestScoreText";

    // The apt view-state channel + values the backing animation is driven with.
    static const char KAC_APT_TRANSITION[] = "apt_Transition";
    static const char KAC_VIEW_VISIBLE[]   = "visible";
    static const char KAC_VIEW_INVISIBLE[] = "invisible";

    // Named apt states the panel pushes on ITSELF.
    static const char KAC_STATE_TIME[]           = "Time";
    static const char KAC_STATE_CRASH[]          = "Crash";
    static const char KAC_STATE_TIME_TO_CRASH[]  = "TimeToCrash";
    static const char KAC_STATE_CRASH_TO_TIME[]  = "CrashToTime";
    static const char KAC_STATE_TRANS_IN_TIME[]  = "transInTime";
    static const char KAC_STATE_TRANS_IN_CRASH[] = "transInCrash";
    static const char KAC_STATE_TRANS_OUT_TIME[] = "transOutTime";
    static const char KAC_STATE_TRANS_OUT_CRASH[]= "transOutCrash";

    // Localisation keys for the "best score" caption.
    static const char KAC_CAPTION_BEAT_TIME[]    = "MAP_BEATTIME";
    static const char KAC_CAPTION_BEAT_CRASH[]   = "MAP_BEATCRASH";
    static const char KAC_CAPTION_FRIEND_TIME[]  = "MAP_FRIENDTIME";
    static const char KAC_CAPTION_FRIEND_CRASH[] = "MAP_FRIENDCRASH";

    // Construct's initial road-sign icon selection (`li r11, 64; stw r11, 0x2A4`). The
    // BrnGui::ERoadIcon enumerator name for 64 is not attested in scope -- FLAG: the VALUE is
    // measured, the name is not.
    static const s32 KI_DEFAULT_ROAD_ICON = 64;

    // ---- static out-of-line definitions ---------------------------------------------
    // IMAGE-CITED: image.bin (file offset == VA - 0x82000000, big-endian). The table at
    // VA 0x82F251D8 holds the two pointers 0x82049368 / 0x82049350, which resolve to the
    // strings below -- one per BrnStreetData::ScoreType.
    const char* RoadPanel::KAPC_RR_FILTER_OPTIONS[RoadPanel::KI_SCORE_TYPE_COUNT] =
    {
        "$CN_LEGEND_RR_TIMES",     // KI_SCORE_TYPE_TIME
        "$CN_LEGEND_RR_CRASHES",   // KI_SCORE_TYPE_CRASH
    };

    // ---------------------------------------------------------------------------------
    // The {4, 330, 12, <mode>} record SwitchScoreMode and TransitionIn post on the state
    // interface's output queue as channel 40, record size 16 (`v6 = {4, 330, 12, mode};
    // AddEvent(si + 12, v6, 40, 16)` @0x8243A3xx / @0x8243A4xx). The committed
    // BrnGuiDemangledEventTypes.h models GuiEventSetRoadRuleScoreMode as the RAW 4-byte payload
    // (id 330) -- that spelling posts only the payload with 330 as the CHANNEL, which is not the
    // wire record the console writes. This is the "consumers that need the exact X360 wire
    // record build it themselves and post it through GetOutputEventQueue()->AddEvent()"
    // accommodation the CgsGuiStateInterface.h banner names as the standing rule, so the exact
    // record is built here. Name kept distinct from the demangled type so neither shadows the
    // other.
    // FLAG: 330 is the X360 WIRE id (which happens to match the demangled table's id here).
    // ---------------------------------------------------------------------------------
    struct GuiEventSetRoadRuleScoreModeRecord : public CgsGui::GuiEvent<330>
    {
        explicit GuiEventSetRoadRuleScoreModeRecord(s32 liScoreMode)
            : CgsGui::GuiEvent<330>(4, 12)   // muHeader0 == payload size, muHeader2 == its offset
            , miScoreMode(liScoreMode)
        {
        }
        s32 miScoreMode;   // record +0x0C
    };
    static_assert(sizeof(GuiEventSetRoadRuleScoreModeRecord) == 16,
                  "the road-rule score-mode record is the 16 bytes AddEvent is given (li r6, 0x10)");

    static const s32 KI_OUT_CHANNEL_GUI_EVENT = 40;
    static const s32 KI_SCORE_MODE_RECORD_SIZE = 16;

    // =================================================================================
    // ⛔ BrnGui::RoadSignIcon BOUNDARY -- FLAG, INERT STAND-IN, DELETE-WHEN.
    //
    // RoadPanel embeds a BrnGui::RoadSignIcon (mRoadSign, the reserved carve at +0xA0) and
    // calls exactly four of its entry points. RoadSignIcon has NO HOME in the tree: its
    // canonical one is GameSource/Gui/SatNav/BrnRoadSignIconManager.{h,cpp} (the file path in
    // its own asserts, RoadSignIcon::Construct @0x824F5170 line 73/74), and homing it means
    // landing RoadSignIconManager's eight siblings as well -- a separate TU this wave does not
    // own. Calling the real methods from here would either fork the type or leave four
    // unresolved externals, so each call is routed through the inert stand-in below.
    //
    // The X360 originals, for whoever lands the real home:
    //   RoadSignIcon::Construct        @0x824F5170  (name, si, parentName, bool)
    //   RoadSignIcon::SetColour        @0x824F52B8  (colour)
    //   RoadSignIcon::FindRoadFromName @0x824F53B8  (name) -> road index
    //   RoadSignIcon::DisplayRoad      -- CrashNavPanel's inline calls sub_82502A88(icon, road, 0)
    //
    // CHOSEN INERT RETURN: FindRoadFromName answers -1, the "no such road" index every
    // RoadSignIcon consumer already tests for -- so DisplayRoad is handed a miss and the sign
    // simply shows nothing, which is the caller-safe outcome. The three void entry points do
    // nothing. CONSEQUENCE on this build: the road panel's TEXT is fully correct (that is all
    // panel-owned state); only the road-sign ARTWORK inside it stays blank and uncoloured.
    //
    // DELETE-WHEN BrnGui::RoadSignIcon lands in BrnRoadSignIconManager.h: retype
    // RoadPanel::maRoadSignReserved to `RoadSignIcon mRoadSign` and replace every
    // RoadSignIconBoundary::* call below with the real method.
    // =================================================================================
    namespace RoadSignIconBoundary
    {
        static const s32 KI_NO_ROAD = -1;

        static void LogOnce(const char* lpacWhich)
        {
            static bool gsbWarned = false;
            if (!gsbWarned && CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[UI-gate] PARK: BrnGui::RoadSignIcon has no home on this build; "
                       "RoadPanel's road-sign artwork is inert (first hit: ";
                *CgsDev::Log::gpDebugPrint << lpacWhich;
                *CgsDev::Log::gpDebugPrint << ")\n";
                gsbWarned = true;
            }
        }

        static void Construct(void* /*lpRoadSign*/, const char* /*lpacName*/,
                              CgsGui::StateInterface* /*lpStateInterface*/,
                              const char* /*lpacParentName*/, bool /*lbFlag*/)
        {
            LogOnce("Construct");
        }

        static void SetColour(void* /*lpRoadSign*/, s32 /*leSignColour*/)
        {
            LogOnce("SetColour");
        }

        static s32 FindRoadFromName(void* /*lpRoadSign*/, const char* /*lpacRoadName*/)
        {
            LogOnce("FindRoadFromName");
            return KI_NO_ROAD;
        }

        static void DisplayRoad(void* /*lpRoadSign*/, s32 /*liRoadIndex*/, bool /*lbImmediate*/)
        {
            LogOnce("DisplayRoad");
        }
    }

    // @ 0x82418060 -------------------------------------------------------------------
    void RoadPanelData::PanelBox::Construct(const char* lpacYourScore, const char* lpacName1,
                                           const char* lpacScore1, const char* lpacName2,
                                           const char* lpacScore2, bool lbBeaten1, bool lbBeaten2)
    {
        CGS_ASSERT(lpacYourScore && lpacName1 && lpacName2 && lpacScore1 && lpacScore2,
                   "lpYourScore && lpName1 && lpName2 && lpScore1 && lpScore2");   // cpp:99

        // The console measures each string with an inline `while (*p++)` and compares the
        // resulting length INCLUDING the terminator against 32 (`cmpwi 0x20` on p - start).
        // Reproduced with strlen() + 1 so the same strings trip the same assert.
        CGS_ASSERT(std::strlen(lpacYourScore) + 1 <= KI_PANEL_TEXT_LENGTH, "Name too long"); // cpp:100
        CGS_ASSERT(std::strlen(lpacName1)     + 1 <= KI_PANEL_TEXT_LENGTH, "Name too long"); // cpp:101
        CGS_ASSERT(std::strlen(lpacName2)     + 1 <= KI_PANEL_TEXT_LENGTH, "Name too long"); // cpp:102
        CGS_ASSERT(std::strlen(lpacScore1)    + 1 <= KI_PANEL_TEXT_LENGTH, "Name too long"); // cpp:103
        CGS_ASSERT(std::strlen(lpacScore2)    + 1 <= KI_PANEL_TEXT_LENGTH, "Name too long"); // cpp:104

        // X360 store order (@0x82418394..0x824183E4): +2, +66, +34, +130, +98.
        CgsCore::SPrintf(mPlayerScores, KI_PANEL_TEXT_LENGTH, "%s", lpacYourScore);   // box +0x02
        CgsCore::SPrintf(mNames[1],     KI_PANEL_TEXT_LENGTH, "%s", lpacName1);       // box +0x42
        CgsCore::SPrintf(mNames[0],     KI_PANEL_TEXT_LENGTH, "%s", lpacName2);       // box +0x22
        CgsCore::SPrintf(mScores[1],    KI_PANEL_TEXT_LENGTH, "%s", lpacScore1);      // box +0x82
        CgsCore::SPrintf(mScores[0],    KI_PANEL_TEXT_LENGTH, "%s", lpacScore2);      // box +0x62

        mabPlayerBestScore[1] = lbBeaten1;   // stb this+1 (@0x824183EC)
        mabPlayerBestScore[0] = lbBeaten2;   // stb this+0 (@0x824183F0)
    }

    // @ 0x824256E0 -------------------------------------------------------------------
    void RoadPanelData::Construct()
    {
        for (s32 liBox = 0; liBox < KI_ROADRULE_COUNT; ++liBox)   // li r30, 2 @0x824256F4
        {
            mPanels[liBox].Construct(KAC_EMPTY_STRING, KAC_EMPTY_STRING, KAC_EMPTY_STRING,
                                     KAC_EMPTY_STRING, KAC_EMPTY_STRING, false, false);
        }
    }

    // @ 0x82425738 -------------------------------------------------------------------
    void RoadPanel::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                              const char* lpacParentName)
    {
        // Base IconComponent construct: no state-identifier table for the panel itself.
        IconComponent::Construct(lpacName, lpStateInterface, 0, lpacParentName);

        RoadSignIconBoundary::Construct(maRoadSignReserved, KAC_ROAD_SIGN_NAME,
                                        lpStateInterface, GetName(), true);

        meIcon = KI_DEFAULT_ROAD_ICON;   // stw 64, 0x2A4 -- BEFORE the row loop, as the asm has it

        // The four scoreboard rows: clip names are built as "<stem><1-based row>", and the
        // NAME and SCORE fields for a row are constructed back to back (the console walks two
        // cursors 0x4A0 apart, both stepping 0x128).
        char lacRowName[KI_ROW_NAME_BUFFER];
        for (s32 liRow = 0; liRow < E_ROW_COUNT; ++liRow)
        {
            CgsCore::SPrintf(lacRowName, KI_ROW_NAME_CAPACITY, KAC_ROW_NAME_FORMAT,
                             KAC_ROW_NAME_STEM, liRow + 1);
            lacRowName[KI_ROW_NAME_BUFFER - 1] = 0;
            mNames[liRow].Construct(lacRowName, lpStateInterface, GetName());

            CgsCore::SPrintf(lacRowName, KI_ROW_NAME_CAPACITY, KAC_ROW_NAME_FORMAT,
                             KAC_ROW_SCORE_STEM, liRow + 1);
            lacRowName[KI_ROW_NAME_BUFFER - 1] = 0;
            mScores[liRow].Construct(lacRowName, lpStateInterface, GetName());
        }

        mBestScoreBackingAnimation.Construct(KAC_BACKING_ANIM_NAME, lpStateInterface, GetName());
        mTargetCaption.Construct(KAC_TARGET_CAPTION_NAME, lpStateInterface, GetName());

        mRoadPanelData.Construct();

        // COUNT, i.e. "no road rule chosen yet" -- which is why SetCurrentRule has a third
        // from-state below the two real ones.
        meCurrentRule = KI_SCORE_TYPE_COUNT;   // stw 2, 0xD9C

        CGS_ASSERT(mpStateInterface->GetAccessPointers() != 0, "mpAccessPointers != NULL");
        CGS_ASSERT(mpStateInterface->GetAccessPointers()->GetGuiCache() != 0, "mpGuiCache");

        // The panel adopts whichever scoring mode the cache is already in; the console asserts
        // the cache's mode is not the COUNT sentinel (BrnGuiCache.h:4210) -- which is exactly
        // what GuiCache::GetActiveRoadRuleScoringMode() @0x8240FC28 is (that read plus that
        // assert), so it is called by name rather than re-poking +0xAC40.
        mbActive           = false;   // stb 0, 0xDA4
        meCurrentScoreMode =
            mpStateInterface->GetAccessPointers()->GetGuiCache()->GetActiveRoadRuleScoringMode();
    }

    // @ 0x82418410 -------------------------------------------------------------------
    void RoadPanel::AppendExpectedAptComponents(GuiFlow leFlow, GuiCache* lpGuiCache)
    {
        CGS_ASSERT(lpGuiCache != 0, "lpGuiCache");   // cpp:174

        lpGuiCache->AppendExpectedAptComponent(leFlow, GetNameHash());          // a1[33]  == +0x84
        // a1[73] == +0x124 == maRoadSignReserved(+0x00A0) + GuiComponent::muHashedName(+0x84)
        // -- the ROAD SIGN's own name hash, not mTargetCaption (which lives at +0x0BE8, so
        // its hash is +0x0C6C == a1[795], the tail append below).
        // CORRECTED 2026-08-29: mRoadSign has no committed type (the reserved carve), so the
        // hash is read at its GuiComponent-relative offset rather than by member name.
        lpGuiCache->AppendExpectedAptComponent(
            leFlow,
            *reinterpret_cast<const u32*>(&maRoadSignReserved[0x84]));   // a1[73] == +0x124

        for (s32 liRow = 0; liRow < E_ROW_COUNT; ++liRow)                       // li r30/r31 pair
        {
            lpGuiCache->AppendExpectedAptComponent(leFlow, mNames[liRow].GetNameHash());
            lpGuiCache->AppendExpectedAptComponent(leFlow, mScores[liRow].GetNameHash());
        }

        // The console's last two arguments are a1[795] (a HASH load) and `a1 + 837` (an
        // ADDRESS, i.e. the by-name overload). 837*4 == 0xD14 ==
        // mBestScoreBackingAnimation(+0x0D10).macName(+0x04).
        //
        // CORRECTED 2026-08-29 -- the old banner here claimed 795*4 == 0xC6C was
        // "mScores[3] + 0x84" and called the result "a genuine console double-registration".
        // That arithmetic was wrong and so was the conclusion: mScores[3] is at +0x0AC0, so
        // its hash is +0x0B44 == a1[721] -- the LAST element the loop above already covers
        // (the loop walks a1[499], a1[573], a1[647], a1[721] at the 0x128 TextField stride).
        // +0x0C6C is mTargetCaption(+0x0BE8) + muHashedName(+0x84). The console registers
        // each component exactly once; the duplicate that fired
        // BrnGuiCache.cpp:723 ("Appending a component to the list that already exists") was
        // this reconstruction re-appending mScores[3].
        lpGuiCache->AppendExpectedAptComponent(leFlow, mTargetCaption.GetNameHash());   // a1[795] == +0x0C6C
        lpGuiCache->AppendExpectedAptComponent(leFlow, mBestScoreBackingAnimation.GetName());
    }

    // Inlined into CrashNavPanel::SetRoadPanelData @0x8243A9E4..0x8243AA14 ------------
    void RoadPanel::SetRoadPanelData(const char* lpacRoadName, RoadPanelData& lrData)
    {
        // Point the sign at the named road (a miss answers KI_NO_ROAD and simply shows nothing).
        const s32 liRoadIndex = RoadSignIconBoundary::FindRoadFromName(maRoadSignReserved,
                                                                       lpacRoadName);
        RoadSignIconBoundary::DisplayRoad(maRoadSignReserved, liRoadIndex, false);

        mRoadPanelData = lrData;   // the `memcpy 0x144` -- a pointer-free scalar record

        UpdateVisibleScores();
    }

    // @ 0x8242D1B8 -------------------------------------------------------------------
    void RoadPanel::SetCurrentRule(s32 leRule)
    {
        CGS_ASSERT(leRule >= KI_SCORE_TYPE_TIME && leRule < KI_SCORE_TYPE_COUNT,
                   "BrnStreetData::E_SCORE_TYPE_START <= leNewRule && "
                   "BrnStreetData::E_SCORE_TYPE_COUNT > leNewRule");   // cpp:228

        if (leRule == meCurrentRule)
        {
            return;
        }

        // The cross-fade is only animated when the panel is on screen; the state latch and the
        // repaint happen either way.
        if (mbActive)
        {
            const char* lpacState = 0;

            if (meCurrentRule == KI_SCORE_TYPE_TIME)
            {
                CGS_ASSERT(leRule == KI_SCORE_TYPE_CRASH,
                           "Trying to select unknown type of road rule (");   // cpp:241
                lpacState = KAC_STATE_TIME_TO_CRASH;
            }
            else if (meCurrentRule == KI_SCORE_TYPE_CRASH)
            {
                CGS_ASSERT(leRule == KI_SCORE_TYPE_TIME,
                           "Trying to select unknown type of road rule (");   // cpp:250
                lpacState = KAC_STATE_CRASH_TO_TIME;
            }
            else if (meCurrentRule == KI_SCORE_TYPE_COUNT)
            {
                // First selection after Construct: there is nothing to cross-fade FROM, so the
                // panel snaps straight to the destination rule's artwork.
                if (leRule == KI_SCORE_TYPE_TIME)
                {
                    lpacState = KAC_STATE_TIME;
                }
                else
                {
                    CGS_ASSERT(leRule == KI_SCORE_TYPE_CRASH,
                               "Trying to select unknown type of road rule (");   // cpp:266
                    lpacState = KAC_STATE_CRASH;
                }
            }
            else
            {
                // meCurrentRule > COUNT -- the console asserts and then falls straight through
                // to the latch + repaint WITHOUT pushing any state.
                CGS_ASSERT(false,
                           "Unknown type of road rule currently selected: Type ");   // cpp:275
            }

            if (lpacState != 0)
            {
                SetState(lpacState);
            }
        }

        meCurrentRule = leRule;   // stw 0xD9C
        UpdateVisibleScores();
    }

    // @ 0x8243A3E0 -------------------------------------------------------------------
    void RoadPanel::SwitchScoreMode()
    {
        // A two-way toggle: OFFLINE <-> ONLINE. Anything else is asserted and treated as
        // ONLINE (the console's non-zero arm).
        if (meCurrentScoreMode != KI_ROAD_PANEL_MODE_OFFLINE)
        {
            CGS_ASSERT(meCurrentScoreMode == KI_ROAD_PANEL_MODE_ONLINE,
                       "GuiEventSetRoadRuleScoreMode::E_ROAD_PANEL_MODE_ONLINE == "
                       "meCurrentScoreMode");   // cpp:306
            meCurrentScoreMode = KI_ROAD_PANEL_MODE_OFFLINE;
        }
        else
        {
            meCurrentScoreMode = KI_ROAD_PANEL_MODE_ONLINE;
        }

        UpdateVisibleScores();

        // Tell the rest of the GUI which scoreboard the road panel is now showing.
        GuiEventSetRoadRuleScoreModeRecord lScoreModeEvent(meCurrentScoreMode);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lScoreModeEvent),
            KI_OUT_CHANNEL_GUI_EVENT, KI_SCORE_MODE_RECORD_SIZE);
    }

    // @ 0x8243A490 -------------------------------------------------------------------
    void RoadPanel::TransitionIn()
    {
        if (meCurrentRule == KI_SCORE_TYPE_TIME)
        {
            SetState(KAC_STATE_TRANS_IN_TIME);
        }
        else
        {
            CGS_ASSERT(meCurrentRule == KI_SCORE_TYPE_CRASH,
                       "BrnStreetData::E_SCORE_TYPE_CRASH == meCurrentRule");   // cpp:337
            SetState(KAC_STATE_TRANS_IN_CRASH);
        }

        mbActive = true;   // stb 1, 0xDA4

        // Showing the panel re-announces the scoring mode (same record as SwitchScoreMode).
        GuiEventSetRoadRuleScoreModeRecord lScoreModeEvent(meCurrentScoreMode);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lScoreModeEvent),
            KI_OUT_CHANNEL_GUI_EVENT, KI_SCORE_MODE_RECORD_SIZE);
    }

    // @ 0x824184D0 -------------------------------------------------------------------
    void RoadPanel::TransitionOut()
    {
        if (meCurrentRule == KI_SCORE_TYPE_TIME)
        {
            SetState(KAC_STATE_TRANS_OUT_TIME);
        }
        else
        {
            CGS_ASSERT(meCurrentRule == KI_SCORE_TYPE_CRASH,
                       "BrnStreetData::E_SCORE_TYPE_CRASH == meCurrentRule");   // cpp:368
            SetState(KAC_STATE_TRANS_OUT_CRASH);
        }

        mbActive = false;   // stb 0, 0xDA4
    }

    // @ 0x82425938 -------------------------------------------------------------------
    // Repaint the whole panel: the "best score" caption + its backing animation, the two
    // player rows, the two opposing rows, and the sign colour.
    void RoadPanel::UpdateVisibleScores()
    {
        const s32 liSignColour = GetSignColour();

        // ---- the caption over the boxes ------------------------------------------------
        // Only the two REAL rules have a caption; while meCurrentRule is still COUNT the
        // console touches neither the caption nor its backing animation.
        if (meCurrentRule == KI_SCORE_TYPE_TIME || meCurrentRule == KI_SCORE_TYPE_CRASH)
        {
            const char* lpacCaptionKey = 0;

            if (meCurrentScoreMode != KI_ROAD_PANEL_MODE_OFFLINE)
            {
                CGS_ASSERT(meCurrentScoreMode == KI_ROAD_PANEL_MODE_ONLINE,
                           "GuiEventSetRoadRuleScoreMode::E_ROAD_PANEL_MODE_ONLINE == "
                           "meCurrentScoreMode");   // cpp:410 (time) / cpp:434 (crash)
                lpacCaptionKey = (meCurrentRule == KI_SCORE_TYPE_TIME) ? KAC_CAPTION_FRIEND_TIME
                                                                      : KAC_CAPTION_FRIEND_CRASH;
            }
            else if (liSignColour == KI_SIGN_COLOUR_NONE)
            {
                // Offline and nothing beaten yet -> "beat this".
                lpacCaptionKey = (meCurrentRule == KI_SCORE_TYPE_TIME) ? KAC_CAPTION_BEAT_TIME
                                                                      : KAC_CAPTION_BEAT_CRASH;
            }

            if (lpacCaptionKey != 0)
            {
                mTargetCaption.SetLocalisedText(lpacCaptionKey, LM::E_FORMAT_ID_LOOKUP);
                mBestScoreBackingAnimation.AddOutputAptViewState(KAC_APT_TRANSITION,
                                                                 KAC_VIEW_VISIBLE, false);
            }
            else
            {
                mBestScoreBackingAnimation.AddOutputAptViewState(KAC_APT_TRANSITION,
                                                                 KAC_VIEW_INVISIBLE, false);
                mTargetCaption.SetText(KAC_EMPTY_STRING);
            }
        }

        // ---- the two LOCAL PLAYER rows -------------------------------------------------
        CGS_ASSERT(mpStateInterface->GetAccessPointers() != 0, "mpAccessPointers != NULL");
        CGS_ASSERT(mpStateInterface->GetAccessPointers()->GetGuiCache() != 0, "mpGuiCache");

        mNames[E_ROW_BOX0_PLAYER].SetLocalisedText(
            mpStateInterface->GetAccessPointers()->GetGuiCache()->GetPlayerName(),
            LM::E_FORMAT_ID_LOOKUP);
        mNames[E_ROW_BOX1_PLAYER].SetLocalisedText(
            mpStateInterface->GetAccessPointers()->GetGuiCache()->GetPlayerName(),
            LM::E_FORMAT_ID_LOOKUP);

        // Box 0 is the TIME board (mm:ss.hh), box 1 the CRASH board (a money-formatted score).
        mScores[E_ROW_BOX0_PLAYER].SetLocalisedText(mRoadPanelData.mPanels[0].mPlayerScores,
                                                    LM::E_FORMAT_MINUTES_SECONDS_HUNDREDTHS);
        mScores[E_ROW_BOX1_PLAYER].SetLocalisedText(mRoadPanelData.mPanels[1].mPlayerScores,
                                                    LM::E_FORMAT_MONEY);

        // ---- the two OPPOSING rows -----------------------------------------------------
        if (meCurrentScoreMode != KI_ROAD_PANEL_MODE_OFFLINE)
        {
            CGS_ASSERT(meCurrentScoreMode == KI_ROAD_PANEL_MODE_ONLINE,
                       "GuiEventSetRoadRuleScoreMode::E_ROAD_PANEL_MODE_ONLINE == "
                       "meCurrentScoreMode");   // cpp:477

            // Online: the FRIEND row (names go through SetText -- they are already display
            // strings, not localisation ids).
            mNames[E_ROW_BOX0_OPPONENT].SetText(mRoadPanelData.mPanels[0].mNames[KI_FRIEND_ROW]);
            mNames[E_ROW_BOX1_OPPONENT].SetText(mRoadPanelData.mPanels[1].mNames[KI_FRIEND_ROW]);
            mScores[E_ROW_BOX0_OPPONENT].SetLocalisedText(
                mRoadPanelData.mPanels[0].mScores[KI_FRIEND_ROW],
                LM::E_FORMAT_MINUTES_SECONDS_HUNDREDTHS);
            mScores[E_ROW_BOX1_OPPONENT].SetLocalisedText(
                mRoadPanelData.mPanels[1].mScores[KI_FRIEND_ROW], LM::E_FORMAT_MONEY);
        }
        else if (liSignColour == KI_SIGN_COLOUR_NONE)
        {
            // Offline and not beaten yet: the ROAD RULER row.
            mNames[E_ROW_BOX0_OPPONENT].SetText(mRoadPanelData.mPanels[0].mNames[KI_RULER_ROW]);
            mNames[E_ROW_BOX1_OPPONENT].SetText(mRoadPanelData.mPanels[1].mNames[KI_RULER_ROW]);
            mScores[E_ROW_BOX0_OPPONENT].SetLocalisedText(
                mRoadPanelData.mPanels[0].mScores[KI_RULER_ROW],
                LM::E_FORMAT_MINUTES_SECONDS_HUNDREDTHS);
            mScores[E_ROW_BOX1_OPPONENT].SetLocalisedText(
                mRoadPanelData.mPanels[1].mScores[KI_RULER_ROW], LM::E_FORMAT_MONEY);
        }
        else
        {
            // Offline and already beaten: there is no one to chase, so the rows are blanked.
            mNames[E_ROW_BOX0_OPPONENT].SetText(KAC_EMPTY_STRING);
            mNames[E_ROW_BOX1_OPPONENT].SetText(KAC_EMPTY_STRING);
            mScores[E_ROW_BOX0_OPPONENT].SetText(KAC_EMPTY_STRING);
            mScores[E_ROW_BOX1_OPPONENT].SetText(KAC_EMPTY_STRING);
        }

        // The colour is re-read (a second GetSignColour call in the asm, not the cached one).
        RoadSignIconBoundary::SetColour(maRoadSignReserved, GetSignColour());
    }

    // @ 0x82418550 -------------------------------------------------------------------
    s32 RoadPanel::GetSignColour() const
    {
        // The "player has beaten it" flag of each box, for the CURRENT scoring mode.
        const s32 liMode = meCurrentScoreMode;

        if (mRoadPanelData.mPanels[0].mabPlayerBestScore[liMode] &&
            mRoadPanelData.mPanels[1].mabPlayerBestScore[liMode])
        {
            return KI_SIGN_COLOUR_BOTH;
        }

        // ⚠️ FAITHFUL, AND THE CONSOLE'S OWN WINDOW: the box index here is meCurrentRule
        // (`mulli 162 * *(this + 0xD9C)`), which Construct parks at KI_SCORE_TYPE_COUNT until
        // the first SetCurrentRule. Every reachable caller (SetRoadPanelData / SetCurrentRule /
        // SwitchScoreMode) either sets the rule first or is driven by a screen that has, so the
        // COUNT case is unreachable in practice; it is NOT clamped here because clamping would
        // be inventing behaviour the console does not have.
        return mRoadPanelData.mPanels[meCurrentRule].mabPlayerBestScore[liMode]
                   ? KI_SIGN_COLOUR_ONE : KI_SIGN_COLOUR_NONE;
    }

    // @ 0x82417FF8 -------------------------------------------------------------------
    const char* RoadPanel::GetSelectedFriendName() const
    {
        CGS_ASSERT(meCurrentScoreMode == KI_ROAD_PANEL_MODE_ONLINE,
                   "GetScoringMode() == GuiEventSetRoadRuleScoreMode::E_ROAD_PANEL_MODE_ONLINE"); // @0x82418018

        // X360: 0xA2*meCurrentRule + this + 0x1A2 == mPanels[meCurrentRule].mNames[1].
        return mRoadPanelData.mPanels[meCurrentRule].mNames[KI_FRIEND_ROW];
    }
}
