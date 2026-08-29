// ===================================================================================
// BrnGui::EventPanel  -- implementation
//   class:BrnGui::EventPanel
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   Construct                   @0x8243A2C0  (DWARF cpp:79)
//   AppendExpectedAptComponents @0x82417B40  (cpp:131)
//   SetEventData                @0x82430D70  (cpp:159)
//   GetStuntRunScore            @0x8242C888  (cpp:~455, private helper)
//   GetRoadRageTakedownScore    @0x8242CCB8  (cpp:~505, private helper)
//   SetCurrentGameMode          @0x82417BE0  (cpp:545)
//   TransitionIn                @0x82417D38  (cpp:602)
//   TransitionOut               @0x82417E98  (cpp:648)
//   SetPlayerRank               @0x82417A10
//   SetModeRanks                @0x82417A70
//   SetModeRankWins             -- inlined by CrashNavPanel::RecEvent @0x82442070
//   ConvertLocalEventDefToProgressionEventDef @0x824B3600
//
// Member access is BY NAME; the guest offsets in the comments are the proof, not the
// mechanism. See the header banner for the full member run.
//
// DROPPED, deliberately, in the two score helpers: the `CgsDev::Message::gxMessageFilterFlags
// & 1` debug-print blocks (seven / eight streamed lines each, plus -- in GetStuntRunScore --
// a DUPLICATE BrnMath::RoundWithNumSignificantFigures call at 0x8242CBxx that exists only to
// print its own result and is discarded). This is the SAME ruling the committed sibling
// BrnProgressionManager.cpp's GetStuntRunScoreTarget banner records for its identical block.
// No state depends on any of it. The local names below are the console's own -- they are the
// literal strings those dropped blocks streamed.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/Components/BrnEventPanel.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsIDConvertToString
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // ParameterFormatType
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // VariableEventQueue::AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiWorldDataController.h"                     // BrnGui::WorldDataController
#include "GameSource/Math/BrnMathUtils.h"                                 // BrnMath::RoundWithNumSignificantFigures
#include "SharedClasses/Progression/BrnProgressionData.h"                 // BrnProgression::ProgressionData
#include "SharedClasses/Progression/BrnProgressionRankData.h"             // BrnProgression::ProgressionRankData

#include <cmath>   // std::floor

namespace BrnGui
{
    typedef CgsLanguage::LanguageManager        LM;
    typedef BrnProgression::RaceEventData       RaceEventData;
    typedef BrnProgression::ProgressionData     ProgressionData;
    typedef BrnProgression::ProgressionRankData ProgressionRankData;

    // ---- static out-of-line definitions ---------------------------------------------
    // IMAGE-CITED: image.bin (file offset == VA - 0x82000000, big-endian). The table at
    // VA 0x82F25180 holds six pointers -- 0x82049480 / 0x8204946C / 0x82049454 / 0x82049440 /
    // 0x82049428 / 0x82049414 -- resolving to the strings below, one per EEventType.
    const char* EventPanel::KAPC_EVENT_FILTER_OPTIONS[EventPanel::E_EVENT_TYPE_COUNT] =
    {
        "$GAMEMODE_RACE",           // E_EVENT_TYPE_RACE
        "$GAMEMODE_ROADRAGE",       // E_EVENT_TYPE_ROAD_RAGE
        "$GAMEMODE_STUNTATTACK",    // E_EVENT_TYPE_STUNT_ATTACK
        "$GAMEMODE_SURVIVAL",       // E_EVENT_TYPE_SURVIVOR
        "$GAMEMODE_BURNINGROUTE",   // E_EVENT_TYPE_BURNING_ROUTE
        "$CN_LEGEND_ALL_ON",        // E_EVENT_TYPE_ALL
    };

    // ---- file-static tables ----------------------------------------------------------
    // The six child text fields' apt clip names. IMAGE-CITED: the table Construct walks lives
    // at VA 0x82F251B0 and its loop bound is the NEXT table (off_82F251C8, the "Star0".."Star3"
    // names), which is what fixes the count at six.
    static const char* const KAPC_TEXTFIELD_NAMES[EventPanel::E_TEXTFIELD_COUNT] =
    {
        "EventName", "EventStart", "GoalTitle", "GoalText", "AdditionalTitle", "AdditionalBody",
    };

    // The mode-logo icon's state-identifier table (X360 off_82F25198, passed as
    // IconComponent::Construct's table argument @0x8243A330). IMAGE-CITED, six entries -- one
    // per BrnProgression::RaceEventData::EModeType, which is exactly how SetEventData indexes
    // it (`lbz r4, 0xEC(event); bl IconComponent::SetState`).
    static const char* const KAPC_MODE_LOGO_STATES[RaceEventData::E_MODE_COUNT] =
    {
        "race",          // E_MODE_RACE
        "roadRage",      // E_MODE_ROAD_RAGE
        "freestyle",     // E_MODE_STUNT_ATTACK
        "markedMan",     // E_MODE_SURVIVOR
        "burningRoute",  // E_MODE_BURNING_ROUTE
        "pursuit",       // E_MODE_PURSUIT
    };

    static const char KAC_MODE_LOGO_NAME[] = "modeLogo_cpt";
    static const char KAC_CAR_ICON_NAME[]  = "carIcon_cpt";

    // Named apt states the panel pushes on ITSELF (all through IconComponent::SetState).
    static const char KAC_STATE_NONE[]              = "none";
    static const char KAC_STATE_SEARCHING[]         = "Searching";
    static const char KAC_STATE_SIMPLE[]            = "simple";
    static const char KAC_STATE_DETAILED[]          = "detailed";
    static const char KAC_STATE_TRANS_IN_NONE[]     = "transInNone";
    static const char KAC_STATE_TRANS_IN_SIMPLE[]   = "transInSimple";
    static const char KAC_STATE_TRANS_IN_DETAILED[] = "transInDetailed";
    static const char KAC_STATE_TRANS_OUT_NONE[]    = "transOutNone";
    static const char KAC_STATE_TRANS_OUT_SIMPLE[]  = "transOutSimple";
    static const char KAC_STATE_TRANS_OUT_DETAILED[]= "transOutDetailed";

    // Localisation keys SetEventData formats / looks up.
    static const char KAC_EVENT_NAME_KEY_FORMAT[]  = "EV_%06u";
    static const char KAC_EVENT_START_KEY_FORMAT[] = "ST_%06u";
    static const char KAC_LANDMARK_KEY_FORMAT[]    = "LM_LOWER_%llu";
    static const char KAC_CAR_STATE_FORMAT[]       = "CAR_%s";
    static const char KAC_GOAL_END[]               = "CN_PANEL_EVENTS_END";
    static const char KAC_GOAL_TAKEDOWNS[]         = "CN_PANEL_EVENTS_TAKEDOWNS";
    static const char KAC_GOAL_SCORE[]             = "CN_PANEL_EVENTS_SCORE";
    static const char KAC_GOAL_TIME[]              = "CN_PANEL_EVENTS_TIME";
    static const char KAC_VALUE_TD_COUNT[]         = "CV_PANEL_EVENTS_TD_COUNT";
    static const char KAC_VALUE_SCORE[]            = "CV_PANEL_EVENTS_SCORE";

    // The scratch capacity the console hard-codes (SPrintf is always called with 32 and the
    // buffer's byte 31 is cleared right after).
    static const s32 KI_SCRATCH_LEN = 32;

    // Milliseconds -> seconds, for a challenged-event time score (`fmuls` against
    // flt_820DB5CC @0x824312CC; IMAGE-CITED: image.bin @0xDB5CC == 3A 83 12 6F == 0.001f).
    static const f32 KF_MILLISECONDS_TO_SECONDS = 0.001f;

    // BrnMath::RoundWithNumSignificantFigures' second argument on the stunt-attack path
    // (`lfs f2, flt_82001D9C`; IMAGE-CITED: image.bin @0x1D9C == 40 00 00 00 == 2.0f exactly) --
    // the same constant the committed ProgressionManager::GetStuntRunScoreTarget uses.
    static const f32 KF_STUNT_TARGET_SIGNIFICANT_FIGURES = 2.0f;

    // ---------------------------------------------------------------------------------
    // The {1, 437, 12} "give me the player's rank progress" command Construct posts on the
    // state interface's output queue as channel 40, record size 16
    // (`v10 = {1, 0x1B5, 12}; AddEvent(si + 12, v10, 40, 16)` @0x8243A3B0..0x8243A3C8).
    // The answer arrives as the id-438 GuiEventRankProgressResponse that
    // CrashNavPanel::RecEvent already handles (and feeds back through SetPlayerRank /
    // SetModeRanks / SetModeRankWins).
    //
    // ⭐ THIS CLOSES A COMMITTED "FLAG NOT REPRODUCED". BrnCrashNavPanel.cpp's RecEvent banner
    // records the SAME record being posted from the id-64 cache-bind arm (@0x82442008) and left
    // out because "no committed event type carries X360 id 437". The panel posts it itself at
    // Construct time, and the committed BrnCrashNavStats.cpp precedent (GuiEventSetupDone,
    // `GuiEvent<435>(1, 12)` on the same queue and channel) is the shape to model it with.
    // FLAG: 437 is the X360 WIRE id; the DWARF `GuiEvent<N>` template id is not derivable from
    // the call site (see the "GuiEvent<450>; X360 id 455" note in BrnGuiEventTypeDefs.h). The
    // wire id is the self-consistent choice because this tree's own consumers compare against
    // wire ids (CrashNavPanel::RecEvent tests 436 / 438).
    // ---------------------------------------------------------------------------------
    struct GuiEventRankProgressRequest : public CgsGui::GuiEvent<437>
    {
        GuiEventRankProgressRequest() : CgsGui::GuiEvent<437>(1, 12) {}
    };
    static const s32 KI_OUT_CHANNEL_GUI_EVENT = 40;
    static const s32 KI_REQUEST_RECORD_SIZE   = 16;

    // The sentinel Construct parks every rank/win slot and the current event id at
    // (`li r11, -1` then nine `stw`s plus the id store).
    static const s32 KI_RANK_UNSET = -1;

// @ 0x8243A2C0 -----------------------------------------------------------------------
    void EventPanel::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName)
    {
        // Base IconComponent construct: no state-identifier table for the panel itself.
        IconComponent::Construct(lpacName, lpStateInterface, 0, lpacParentName);

        // The six text fields, each parented under this panel's own name (the console
        // dispatches each through the field's vtable slot 0).
        for (s32 liField = 0; liField < E_TEXTFIELD_COUNT; ++liField)
        {
            maTextfields[liField].Construct(KAPC_TEXTFIELD_NAMES[liField],
                                            lpStateInterface, GetName());
        }

        // The mode logo DOES get a state table (one entry per progression mode); the car icon
        // does not (its states are formatted "CAR_<id>" strings).
        mModeLogo.Construct(KAC_MODE_LOGO_NAME, lpStateInterface, KAPC_MODE_LOGO_STATES, GetName());
        mCarIcon.Construct(KAC_CAR_ICON_NAME, lpStateInterface, 0, GetName());

        miPlayerRank             = KI_RANK_UNSET;   // stw -1, 0x8B4
        miCurrentRaceRank        = KI_RANK_UNSET;   // 0x8B8
        miCurrentRoadRageRank    = KI_RANK_UNSET;   // 0x8BC
        miCurrentStuntAttackRank = KI_RANK_UNSET;   // 0x8C0
        miCurrentMarkedManRank   = KI_RANK_UNSET;   // 0x8C4
        miOfflineRaceRankWins    = KI_RANK_UNSET;   // 0x8C8
        miRoadRageRankWins       = KI_RANK_UNSET;   // 0x8CC
        miStuntAttackRankWins    = KI_RANK_UNSET;   // 0x8D0
        miMarkedManRankWins      = KI_RANK_UNSET;   // 0x8D4
        muCurrentEventID         = static_cast<u32>(KI_RANK_UNSET);   // stw -1, 0x8B0
        meCurrentGameMode        = E_EVENT_TYPE_COUNT;                // stw 6,  0x8AC

        GuiEventRankProgressRequest lRankRequest;
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRankRequest),
            KI_OUT_CHANNEL_GUI_EVENT, KI_REQUEST_RECORD_SIZE);

        mbActive = false;   // stb 0, 0x8D8 -- after the post, exactly as the asm orders it
    }

// @ 0x82417B40 -----------------------------------------------------------------------
    void EventPanel::AppendExpectedAptComponents(GuiFlow leFlow, GuiCache* lpGuiCache)
    {
        CGS_ASSERT(lpGuiCache != 0, "lpGuiCache");   // cpp:134

        lpGuiCache->AppendExpectedAptComponent(leFlow, GetNameHash());     // lwz +0x84

        for (s32 liField = 0; liField < E_TEXTFIELD_COUNT; ++liField)      // li r31, 6
        {
            lpGuiCache->AppendExpectedAptComponent(leFlow, maTextfields[liField].GetNameHash());
        }

        // The two icons go through the BY-NAME overload (sub_824F87C0, which hashes the string
        // itself) rather than the by-hash one -- the console passes `addi r5, this, 0x788` /
        // `0x81C`, i.e. the icons' macName addresses, not their cached hashes.
        lpGuiCache->AppendExpectedAptComponent(leFlow, mModeLogo.GetName());
        lpGuiCache->AppendExpectedAptComponent(leFlow, mCarIcon.GetName());
    }

// @ 0x82430D70 -----------------------------------------------------------------------
    void EventPanel::SetEventData(u32 luEventId, const void* lpChallengedScores,
                                  const GuiCache* lpGuiCache, bool lbShowPanel)
    {
        CGS_ASSERT(lpGuiCache != 0, "lpGuiCache");   // cpp:163

        // Same event AND no challenged-score override -> nothing changed.
        if (luEventId == muCurrentEventID && lpChallengedScores == 0)
        {
            return;
        }
        muCurrentEventID = luEventId;   // stw 0x8B0

        if (!lbShowPanel)
        {
            if (mbActive)
            {
                SetState(KAC_STATE_NONE);
            }
            return;
        }

        if (luEventId == 0)
        {
            // The map has no event highlighted yet -- park on the "searching" artwork.
            if (mbActive)
            {
                SetState(KAC_STATE_SEARCHING);
            }
            return;
        }

        char lacScratch[KI_SCRATCH_LEN];
        char lacCarId[KI_SCRATCH_LEN];

        CgsCore::SPrintf(lacScratch, KI_SCRATCH_LEN, KAC_EVENT_NAME_KEY_FORMAT, luEventId);
        lacScratch[KI_SCRATCH_LEN - 1] = 0;
        maTextfields[E_TEXTFIELD_EVENT_NAME].SetLocalisedText(lacScratch, LM::E_FORMAT_ID_LOOKUP);

        CgsCore::SPrintf(lacScratch, KI_SCRATCH_LEN, KAC_EVENT_START_KEY_FORMAT, luEventId);
        lacScratch[KI_SCRATCH_LEN - 1] = 0;
        maTextfields[E_TEXTFIELD_EVENT_START].SetLocalisedText(lacScratch, LM::E_FORMAT_ID_LOOKUP);

        CGS_ASSERT(lpGuiCache->GetWorldDataController() != 0,
                   "lpGuiCache->GetWorldDataController()");   // cpp:211
        const WorldDataController* const lpWorldDataController = lpGuiCache->GetWorldDataController();

        const RaceEventData* const lpEventData =
            lpWorldDataController->GetEventInfoFromEventId(luEventId);
        CGS_ASSERT(lpEventData != 0, "lpEventData");           // cpp:216

        // The logo artwork is picked straight off the event's mode byte.
        mModeLogo.SetState(static_cast<u32>(lpEventData->GetMode()));

        switch (static_cast<RaceEventData::EModeType>(lpEventData->GetMode()))
        {
        case RaceEventData::E_MODE_RACE:
        case RaceEventData::E_MODE_SURVIVOR:
        {
            // Both "get from A to B" modes show the END LANDMARK's lower-case name as the goal.
            maTextfields[E_TEXTFIELD_GOAL_TITLE].SetLocalisedText(KAC_GOAL_END, LM::E_FORMAT_ID_LOOKUP);

            const BrnProgression::CheckpointData* const lpEndCheckpoint =
                lpEventData->GetCheckpointData(lpEventData->GetCheckpointCount() - 1);
            CGS_ASSERT(lpEndCheckpoint != 0, "lpEndCheckpoint");   // cpp:229 (race) / cpp:251 (survivor)

            GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
            lpGuiCache->GetLandmarkInfoFromID(lpEndCheckpoint->GetLandmarkId(), &lLandmarkInfo);

            CgsCore::SPrintf(lacScratch, KI_SCRATCH_LEN, KAC_LANDMARK_KEY_FORMAT,
                             lLandmarkInfo.GetCgsId());
            lacScratch[KI_SCRATCH_LEN - 1] = 0;
            maTextfields[E_TEXTFIELD_GOAL_TEXT].SetLocalisedText(lacScratch, LM::E_FORMAT_ID_LOOKUP);

            maTextfields[E_TEXTFIELD_ADDITIONAL_TITLE].ClearText();
            maTextfields[E_TEXTFIELD_ADDITIONAL_TITLE].OutputAptData();
            maTextfields[E_TEXTFIELD_ADDITIONAL_BODY].ClearText();
            maTextfields[E_TEXTFIELD_ADDITIONAL_BODY].OutputAptData();
            break;
        }

        case RaceEventData::E_MODE_ROAD_RAGE:
        {
            CGS_ASSERT(miCurrentRoadRageRank >= 0, "0 <= miCurrentRoadRageRank");   // cpp:269

            maTextfields[E_TEXTFIELD_GOAL_TITLE].SetLocalisedText(KAC_GOAL_TAKEDOWNS,
                                                                 LM::E_FORMAT_ID_LOOKUP);

            // The console re-resolves the rank record purely to assert it before asking for the
            // target; the target helper resolves it again for itself.
            const ProgressionData* const lpProgressionData =
                lpWorldDataController->GetProgressionData();
            CGS_ASSERT(lpProgressionData->GetProgressionRankData(
                           static_cast<u32>(miCurrentRoadRageRank)) != 0, "lpRankData"); // cpp:276

            CgsCore::SPrintf(lacScratch, KI_SCRATCH_LEN, "%d",
                             GetRoadRageTakedownScore(lpWorldDataController));
            lacScratch[KI_SCRATCH_LEN - 1] = 0;
            maTextfields[E_TEXTFIELD_GOAL_TEXT].SetLocalisedText(
                KAC_VALUE_TD_COUNT, LM::E_FORMAT_ID_LOOKUP, 1, lacScratch, LM::E_FORMAT_INTEGER);

            maTextfields[E_TEXTFIELD_ADDITIONAL_TITLE].ClearText();
            maTextfields[E_TEXTFIELD_ADDITIONAL_TITLE].OutputAptData();
            maTextfields[E_TEXTFIELD_ADDITIONAL_BODY].ClearText();
            maTextfields[E_TEXTFIELD_ADDITIONAL_BODY].OutputAptData();
            break;
        }

        case RaceEventData::E_MODE_STUNT_ATTACK:
        {
            CGS_ASSERT(miCurrentStuntAttackRank >= 0, "0 <= miCurrentStuntAttackRank"); // cpp:297

            maTextfields[E_TEXTFIELD_GOAL_TITLE].SetLocalisedText(KAC_GOAL_SCORE,
                                                                 LM::E_FORMAT_ID_LOOKUP);

            // A challenged event carries the score to beat in its record's leading word; without
            // one the panel computes the rank-interpolated target itself.
            const s32 liScore = (lpChallengedScores != 0)
                ? *reinterpret_cast<const s32*>(lpChallengedScores)
                : GetStuntRunScore(lpWorldDataController, lpEventData);

            CgsCore::SPrintf(lacScratch, KI_SCRATCH_LEN, "%d", liScore);
            lacScratch[KI_SCRATCH_LEN - 1] = 0;
            maTextfields[E_TEXTFIELD_GOAL_TEXT].SetLocalisedText(
                KAC_VALUE_SCORE, LM::E_FORMAT_ID_LOOKUP, 1, lacScratch, LM::E_FORMAT_INTEGER);

            maTextfields[E_TEXTFIELD_ADDITIONAL_TITLE].ClearText();
            maTextfields[E_TEXTFIELD_ADDITIONAL_TITLE].OutputAptData();
            maTextfields[E_TEXTFIELD_ADDITIONAL_BODY].ClearText();
            maTextfields[E_TEXTFIELD_ADDITIONAL_BODY].OutputAptData();
            break;
        }

        case RaceEventData::E_MODE_BURNING_ROUTE:
        {
            // The only DETAILED arm: destination + target time + the required car.
            maTextfields[E_TEXTFIELD_GOAL_TITLE].SetLocalisedText(KAC_GOAL_END,
                                                                  LM::E_FORMAT_ID_LOOKUP);

            const BrnProgression::CheckpointData* const lpEndCheckpoint =
                lpEventData->GetCheckpointData(lpEventData->GetCheckpointCount() - 1);
            CGS_ASSERT(lpEndCheckpoint != 0, "lpEndCheckpoint");   // cpp:334

            GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
            lpGuiCache->GetLandmarkInfoFromID(lpEndCheckpoint->GetLandmarkId(), &lLandmarkInfo);

            CgsCore::SPrintf(lacScratch, KI_SCRATCH_LEN, KAC_LANDMARK_KEY_FORMAT,
                             lLandmarkInfo.GetCgsId());
            lacScratch[KI_SCRATCH_LEN - 1] = 0;
            maTextfields[E_TEXTFIELD_GOAL_TEXT].SetLocalisedText(lacScratch, LM::E_FORMAT_ID_LOOKUP);

            maTextfields[E_TEXTFIELD_ADDITIONAL_TITLE].SetLocalisedText(KAC_GOAL_TIME,
                                                                        LM::E_FORMAT_ID_LOOKUP);

            // A challenged time arrives in MILLISECONDS in the record's leading word; the
            // event's own limit is already a float in seconds (`lfs f1, 0x24(event)`).
            const f32 lfTargetTime = (lpChallengedScores != 0)
                ? (static_cast<f32>(*reinterpret_cast<const s32*>(lpChallengedScores)) *
                   KF_MILLISECONDS_TO_SECONDS)
                : lpEventData->GetTimeLimitSlow();

            CgsCore::SPrintf(lacScratch, KI_SCRATCH_LEN, "%f", lfTargetTime);
            lacScratch[KI_SCRATCH_LEN - 1] = 0;
            maTextfields[E_TEXTFIELD_ADDITIONAL_BODY].SetLocalisedText(
                lacScratch, LM::E_FORMAT_MINUTES_SECONDS);

            // The car the burning route must be driven in (`ld r3, 0x10(event)`).
            CgsIDConvertToString(lpEventData->GetSpecialEventCarId(), lacCarId);
            lacCarId[KI_SCRATCH_LEN - 1] = 0;
            CgsCore::SPrintf(lacScratch, KI_SCRATCH_LEN, KAC_CAR_STATE_FORMAT, lacCarId);
            lacScratch[KI_SCRATCH_LEN - 1] = 0;
            mCarIcon.SetState(lacScratch);
            break;
        }

        default:
            // X360 streamed "Invalid mode type in panel (" + mode + ") for event with id " + id
            // + "\n" into the assert message buffer; collapsed to one CGS_ASSERT keeping the
            // leading rodata literal (the tree-wide convention for the streamed asserts).
            CGS_ASSERT(false, "Invalid mode type in panel (");   // cpp:373
            break;
        }

        // Finally re-park the panel on whichever layout this mode uses. BURNING ROUTE is the
        // only "detailed" one (`cmplwi r11, 4` @0x824313C8).
        if (mbActive)
        {
            SetState(lpEventData->GetMode() == RaceEventData::E_MODE_BURNING_ROUTE
                         ? KAC_STATE_DETAILED : KAC_STATE_SIMPLE);
        }
    }

// @ 0x8242CCB8 -----------------------------------------------------------------------
    s32 EventPanel::GetRoadRageTakedownScore(const WorldDataController* lpWorldDataController) const
    {
        const ProgressionData* const lpProgressionData = lpWorldDataController->GetProgressionData();
        CGS_ASSERT(lpProgressionData != 0, "lpProgressionData != NULL");   // cpp:511

        const s32 liLastRank =
            static_cast<s32>(lpProgressionData->GetProgressionRankCount()) - 1;
        const ProgressionRankData* const lpProgressionRankDataLastRank =
            lpProgressionData->GetProgressionRankData(static_cast<u32>(liLastRank));
        CGS_ASSERT(lpProgressionRankDataLastRank != 0, "lpProgressionRankDataLastRank"); // cpp:516

        const s32 liCurrentRank = miCurrentRoadRageRank;

        // Top rank: nothing above to interpolate towards, so that rank's own target IS the
        // answer (`result = _restfpr_26(*(v13 + 80))`).
        if (liCurrentRank >= liLastRank)
        {
            return static_cast<s32>(lpProgressionRankDataLastRank->GetRoadRageTakedownTarget());
        }

        const ProgressionRankData* const lpProgressionRankDataThisRank =
            lpProgressionData->GetProgressionRankData(static_cast<u32>(liCurrentRank));
        CGS_ASSERT(lpProgressionRankDataThisRank != 0, "lpProgressionRankDataThisRank");  // cpp:526
        const ProgressionRankData* const lpProgressionRankDataNextRank =
            lpProgressionData->GetProgressionRankData(static_cast<u32>(liCurrentRank + 1));
        CGS_ASSERT(lpProgressionRankDataNextRank != 0, "lpProgressionRankDataNextRank");  // cpp:529

        // How far the player is through this rank's road-rage win requirement (rank byte +0x62,
        // muNumWinsToRankUpRoadRage -- the console re-reads it off the rank ARRAY rather than
        // through the two pointers above, which is the same value).
        const f32 lfTotalNumberOfWinsForThisRank =
            static_cast<f32>(lpProgressionRankDataThisRank->GetNumWinsToRankUpRoadRage());
        const f32 lfTotalNumberOfWinsForNextRank =
            static_cast<f32>(lpProgressionRankDataNextRank->GetNumWinsToRankUpRoadRage());
        const f32 lfCurrentEventWins = static_cast<f32>(miRoadRageRankWins);

        const f32 lfThisRoadRageTakedownTarget =
            static_cast<f32>(lpProgressionRankDataThisRank->GetRoadRageTakedownTarget());
        const f32 lfNextRoadRageTakedownTarget =
            static_cast<f32>(lpProgressionRankDataNextRank->GetRoadRageTakedownTarget());

        // `((wins - thisWins) / (nextWins - thisWins)) * (nextTarget - thisTarget) + thisTarget`,
        // then + 0.5f and the PPC fsel/floor idiom that closes the function -- i.e. round
        // half-up to a whole takedown count.
        const f32 lfRoadRageFinalTarget =
            ((lfCurrentEventWins - lfTotalNumberOfWinsForThisRank) /
             (lfTotalNumberOfWinsForNextRank - lfTotalNumberOfWinsForThisRank)) *
                (lfNextRoadRageTakedownTarget - lfThisRoadRageTakedownTarget) +
            lfThisRoadRageTakedownTarget + 0.5f;

        return static_cast<s32>(std::floor(lfRoadRageFinalTarget));
    }

// @ 0x8242C888 -----------------------------------------------------------------------
    s32 EventPanel::GetStuntRunScore(const WorldDataController* lpWorldDataController,
                                     const RaceEventData* lpEventData) const
    {
        const ProgressionData* const lpProgressionData = lpWorldDataController->GetProgressionData();
        CGS_ASSERT(lpProgressionData != 0, "lpProgressionData != NULL");   // cpp:460

        const s32 liLastRank =
            static_cast<s32>(lpProgressionData->GetProgressionRankCount()) - 1;
        const s32 liCurrentRank = miCurrentStuntAttackRank;

        // Top rank: the event's own score for that rank IS the target.
        if (liCurrentRank >= liLastRank)
        {
            return lpEventData->GetRankScore(static_cast<u32>(miCurrentStuntAttackRank));
        }

        // How far the player is through this rank's stunt win requirement (rank byte +0x61,
        // muNumWinsToRankUpStunt).
        const f32 lfTotalNumberOfWinsForThisRank = static_cast<f32>(
            lpProgressionData->GetProgressionRankData(static_cast<u32>(liCurrentRank))
                ->GetNumWinsToRankUpStunt());
        const f32 lfTotalNumberOfWinsForNextRank = static_cast<f32>(
            lpProgressionData->GetProgressionRankData(static_cast<u32>(liCurrentRank + 1))
                ->GetNumWinsToRankUpStunt());
        const f32 lfCurrentEventWins = static_cast<f32>(miStuntAttackRankWins);

        // The same fraction applied to the EVENT's two neighbouring rank scores.
        const f32 lfThisStuntRunScore =
            static_cast<f32>(lpEventData->GetRankScore(static_cast<u32>(liCurrentRank)));
        const f32 lfNextStuntRunScore =
            static_cast<f32>(lpEventData->GetRankScore(static_cast<u32>(liCurrentRank + 1)));

        const f32 lfStuntRunScoreFinalTarget =
            ((lfCurrentEventWins - lfTotalNumberOfWinsForThisRank) /
             (lfTotalNumberOfWinsForNextRank - lfTotalNumberOfWinsForThisRank)) *
                (lfNextStuntRunScore - lfThisStuntRunScore) +
            lfThisStuntRunScore;

        return BrnMath::RoundWithNumSignificantFigures(lfStuntRunScoreFinalTarget,
                                                       KF_STUNT_TARGET_SIGNIFICANT_FIGURES);
    }

// @ 0x82417BE0 -----------------------------------------------------------------------
    void EventPanel::SetCurrentGameMode(EventPanel::EEventType leGameMode)
    {
        CGS_ASSERT(static_cast<s32>(leGameMode) >= E_EVENT_TYPE_RACE &&
                       static_cast<s32>(leGameMode) < E_EVENT_TYPE_COUNT,
                   "E_EVENT_TYPE_RACE <= leNewMode && E_EVENT_TYPE_COUNT > leNewMode"); // cpp:574

        if (leGameMode == meCurrentGameMode)
        {
            return;
        }

        const bool lbWasActive = mbActive;   // lbz 0x8D8, read BEFORE the store
        meCurrentGameMode = leGameMode;      // stw 0x8AC

        if (!lbWasActive)
        {
            return;   // hidden panel: latch the mode, animate nothing
        }

        if (muCurrentEventID == 0)
        {
            SetState(KAC_STATE_NONE);
            return;
        }

        switch (leGameMode)
        {
        case E_EVENT_TYPE_RACE:
        case E_EVENT_TYPE_ROAD_RAGE:
        case E_EVENT_TYPE_STUNT_ATTACK:
        case E_EVENT_TYPE_SURVIVOR:
        case E_EVENT_TYPE_ALL:
            SetState(KAC_STATE_SIMPLE);
            break;

        case E_EVENT_TYPE_BURNING_ROUTE:
            SetState(KAC_STATE_DETAILED);
            break;

        default:
            // X360 streamed "Unknown type of event currently selected: Type " + mode + "\n".
            CGS_ASSERT(false, "Unknown type of event currently selected: Type ");   // cpp:612
            break;
        }
    }

// @ 0x82417D38 -----------------------------------------------------------------------
    void EventPanel::TransitionIn()
    {
        if (muCurrentEventID == 0)
        {
            SetState(KAC_STATE_TRANS_IN_NONE);
            mbActive = true;
            return;
        }

        switch (meCurrentGameMode)
        {
        case E_EVENT_TYPE_RACE:
        case E_EVENT_TYPE_ROAD_RAGE:
        case E_EVENT_TYPE_STUNT_ATTACK:
        case E_EVENT_TYPE_SURVIVOR:
        case E_EVENT_TYPE_ALL:
            SetState(KAC_STATE_TRANS_IN_SIMPLE);
            break;

        case E_EVENT_TYPE_BURNING_ROUTE:
            SetState(KAC_STATE_TRANS_IN_DETAILED);
            break;

        default:
            CGS_ASSERT(false, "Unknown type of event currently selected: Type ");   // cpp:658
            break;
        }

        // Every arm -- the assert arm included -- raises the active flag.
        mbActive = true;
    }

// @ 0x82417E98 -----------------------------------------------------------------------
    void EventPanel::TransitionOut()
    {
        if (muCurrentEventID == 0)
        {
            SetState(KAC_STATE_TRANS_OUT_NONE);
        }
        else
        {
            switch (meCurrentGameMode)
            {
            case E_EVENT_TYPE_RACE:
            case E_EVENT_TYPE_ROAD_RAGE:
            case E_EVENT_TYPE_STUNT_ATTACK:
            case E_EVENT_TYPE_SURVIVOR:
            case E_EVENT_TYPE_ALL:
                SetState(KAC_STATE_TRANS_OUT_SIMPLE);
                break;

            case E_EVENT_TYPE_BURNING_ROUTE:
                SetState(KAC_STATE_TRANS_OUT_DETAILED);
                break;

            default:
                CGS_ASSERT(false, "Unknown type of event currently selected: Type ");
                break;
            }
        }

        // Every arm clears BOTH the event id and the active flag (`stw r27, 0x8B0` /
        // `stb r27, 0x8D8` with r27 == 0, repeated in each arm).
        muCurrentEventID = 0;
        mbActive         = false;
    }

// Inlined by CrashNavPanel::RecEvent @0x82442070..0x8244208C ---------------------------
    void EventPanel::SetModeRankWins(s32 liRaceWins, s32 liRoadRageWins,
                                     s32 liStuntAttackWins, s32 liMarkedManWins)
    {
        miOfflineRaceRankWins = liRaceWins;        // +0x8C8
        miRoadRageRankWins    = liRoadRageWins;    // +0x8CC
        miStuntAttackRankWins = liStuntAttackWins; // +0x8D0
        miMarkedManRankWins   = liMarkedManWins;   // +0x8D4
    }

// @ 0x82417A10 -----------------------------------------------------------------------
    void EventPanel::SetPlayerRank(s32 iRank)
    {
        CGS_ASSERT(iRank >= 0, "0<=liCurrentRank");   // @0x82417A4C  h:258

        miPlayerRank = iRank;   // +0x8B4
    }

// @ 0x82417A70 -----------------------------------------------------------------------
    void EventPanel::SetModeRanks(s32 iRaceRank, s32 iRoadRageRank,
                                  s32 iStuntAttackRank, s32 iMarkedManRank)
    {
        CGS_ASSERT(iRaceRank >= 0,        "0<=liCurrentRaceRank");        // @0x82417AB4  h:279
        CGS_ASSERT(iRoadRageRank >= 0,    "0<=liCurrentRoadRageRank");    // @0x82417AD8  h:280
        CGS_ASSERT(iStuntAttackRank >= 0, "0<=liCurrentStuntAttackRank"); // @0x82417AFC  h:281
        CGS_ASSERT(iMarkedManRank >= 0,   "0<=liCurrentMarkedManRank");   // @0x82417B20  h:282

        miCurrentRaceRank        = iRaceRank;        // +0x8B8
        miCurrentRoadRageRank    = iRoadRageRank;    // +0x8BC
        miCurrentStuntAttackRank = iStuntAttackRank; // +0x8C0
        miCurrentMarkedManRank   = iMarkedManRank;   // +0x8C4
    }

// @ 0x824B3600 -----------------------------------------------------------------------
    BrnProgression::RaceEventData::EModeType
    EventPanel::ConvertLocalEventDefToProgressionEventDef(EventPanel::EEventType eLocalType)
    {
        // Map the panel's local event-filter type onto a progression race-event mode. The
        // mapping is identity for the five concrete modes; E_EVENT_TYPE_ALL (5) and any
        // unknown value collapse to E_MODE_COUNT (X360 sets r27 = 6 at entry and returns it
        // for both the case-5 arm and the default arm).
        switch (eLocalType)
        {
            case E_EVENT_TYPE_RACE:          return RaceEventData::E_MODE_RACE;          // 0 -> 0
            case E_EVENT_TYPE_ROAD_RAGE:     return RaceEventData::E_MODE_ROAD_RAGE;     // 1 -> 1
            case E_EVENT_TYPE_STUNT_ATTACK:  return RaceEventData::E_MODE_STUNT_ATTACK;  // 2 -> 2
            case E_EVENT_TYPE_SURVIVOR:      return RaceEventData::E_MODE_SURVIVOR;      // 3 -> 3
            case E_EVENT_TYPE_BURNING_ROUTE: return RaceEventData::E_MODE_BURNING_ROUTE; // 4 -> 4
            case E_EVENT_TYPE_ALL:           return RaceEventData::E_MODE_COUNT;         // 5 -> 6
            default:
                // X360 streamed "Unknown event type " + (int)eLocalType + "\n" into the assert
                // message buffer; collapse to one CGS_ASSERT keeping only the rodata literal.
                CGS_ASSERT(false, "Unknown event type ");
                return RaceEventData::E_MODE_COUNT;   // r27 = 6
        }
    }

}
