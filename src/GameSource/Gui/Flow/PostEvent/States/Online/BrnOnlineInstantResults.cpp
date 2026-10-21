// ARTIST POST_ON_IR: original typed lifecycle, table, input and leave owner.

#include "GameSource/Gui/Flow/PostEvent/States/Online/BrnOnlineInstantResults.h"
#include <cstring>
#include "GameSource/Gui/BrnGuiShared.h"
#include "GameSource/Input/GameInputActions.h"
#include "GameSource/Network/SharedIO/BrnNetworkModuleOnlineLobbyPlayerStatusInterface.h"

#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SnPrintf / SPrintf
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEventWrapper
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                     // CgsNumeric::Random::RandomInt
#include "GameSource/Gui/BrnGuiCache.h"                                   // GuiCache::GetOnlinePlayerInfo / GetRandomNumberGenerator
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // GuiEventTickerClearMessages / GuiEventTickerCustomMessage
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"  // InGamePlayerStatusData

int MaybeDrawText(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText,
                  f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA lColour, bool lbCheckScreen);

namespace BrnGui
{
    const CgsGui::sResourceTuple OnlineInstantResultsState::maResourceTuplesToLoad[] =
        { { 226, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const s32 OnlineInstantResultsState::miNumResourcesToLoad = 1;

    const OnlineInstantResultsState::AwardData
        OnlineInstantResultsState::KA_AWARD_OVERALL[OnlineInstantResultsState::KI_NUM_ONLINE_AWARD_TYPES] =
    {
        { 1, 1,
          { "ONLINE_AWARD_WINS_RACE_OVERALL", "ONLINE_AWARD_WINS_RACE_OVERALL_1", "ONLINE_AWARD_WINS_RACE_OVERALL_2",
            "ONLINE_AWARD_WINS_RACE_OVERALL_3", "ONLINE_AWARD_WINS_RACE_OVERALL_4", 0 },
          { "ONLINE_AWARD_WINS_RACE_OVERALL", "ONLINE_AWARD_WINS_RACE_OVERALL_1", "ONLINE_AWARD_WINS_RACE_OVERALL_2",
            "ONLINE_AWARD_WINS_RACE_OVERALL_3", "ONLINE_AWARD_WINS_RACE_OVERALL_4", 0 } },
        { 2, 1,
          { "ONLINE_AWARD_MOST_TAKEDOWNS_FOR_OVERALL", "ONLINE_AWARD_MOST_TAKEDOWNS_FOR_OVERALL_1",
            "ONLINE_AWARD_MOST_TAKEDOWNS_FOR_OVERALL_2", "ONLINE_AWARD_MOST_TAKEDOWNS_FOR_OVERALL_3", 0, 0 },
          { "ONLINE_AWARD_MOST_TAKEDOWNS_FOR_OVERALL_SINGLE", "ONLINE_AWARD_MOST_TAKEDOWNS_FOR_OVERALL_SINGLE_1", 0, 0, 0, 0 } },
        { 2, 1,
          { "ONLINE_AWARD_MOST_TAKEDOWNS_AGAINST_OVERALL", "ONLINE_AWARD_MOST_TAKEDOWNS_AGAINST_OVERALL_1",
            "ONLINE_AWARD_MOST_TAKEDOWNS_AGAINST_OVERALL_2", 0, 0, 0 },
          { "ONLINE_AWARD_MOST_TAKEDOWNS_AGAINST_OVERALL_SINGLE", 0, 0, 0, 0, 0 } },
        { 2, 1,
          { "ONLINE_AWARD_MOST_CRASHES_OVERALL", "ONLINE_AWARD_MOST_CRASHES_OVERALL_1", "ONLINE_AWARD_MOST_CRASHES_OVERALL_2",
            "ONLINE_AWARD_MOST_CRASHES_OVERALL_3", 0, 0 },
          { "ONLINE_AWARD_MOST_CRASHES_OVERALL_SINGLE", "ONLINE_AWARD_MOST_CRASHES_OVERALL_SINGLE_1", 0, 0, 0, 0 } },
        { 0, 0,
          { "ONLINE_AWARD_FASTEST_LAP_OVERALL", 0, 0, 0, 0, 0 },
          { "ONLINE_AWARD_FASTEST_LAP_OVERALL", 0, 0, 0, 0, 0 } },
        { 1, 1,
          { "ONLINE_AWARD_SHORTEST_ROUTE_OVERALL", "ONLINE_AWARD_SHORTEST_ROUTE_OVERALL_1", 0, 0, 0, 0 },
          { "ONLINE_AWARD_SHORTEST_ROUTE_OVERALL", "ONLINE_AWARD_SHORTEST_ROUTE_OVERALL_1", 0, 0, 0, 0 } },
        { 1, 1,
          { "ONLINE_AWARD_LONGEST_ROUTE_OVERALL", "ONLINE_AWARD_LONGEST_ROUTE_OVERALL_1", "ONLINE_AWARD_LONGEST_ROUTE_OVERALL_2",
            0, 0, 0 },
          { "ONLINE_AWARD_LONGEST_ROUTE_OVERALL", "ONLINE_AWARD_LONGEST_ROUTE_OVERALL_1", "ONLINE_AWARD_LONGEST_ROUTE_OVERALL_2",
            0, 0, 0 } },
        { 1, 1,
          { "ONLINE_AWARD_LONGEST_TIME_IN_FIRST_PLACE_OVERALL", "ONLINE_AWARD_LONGEST_TIME_IN_FIRST_PLACE_OVERALL_1",
            "ONLINE_AWARD_LONGEST_TIME_IN_FIRST_PLACE_OVERALL_2", "ONLINE_AWARD_LONGEST_TIME_IN_FIRST_PLACE_OVERALL_3", 0, 0 },
          { "ONLINE_AWARD_LONGEST_TIME_IN_FIRST_PLACE_OVERALL", "ONLINE_AWARD_LONGEST_TIME_IN_FIRST_PLACE_OVERALL_1",
            "ONLINE_AWARD_LONGEST_TIME_IN_FIRST_PLACE_OVERALL_2", "ONLINE_AWARD_LONGEST_TIME_IN_FIRST_PLACE_OVERALL_3", 0, 0 } },
        { 1, 1,
          { "ONLINE_AWARD_MOST_TIME_IN_LAST_PLACE_OVERALL", "ONLINE_AWARD_MOST_TIME_IN_LAST_PLACE_OVERALL_1",
            "ONLINE_AWARD_MOST_TIME_IN_LAST_PLACE_OVERALL_2", "ONLINE_AWARD_MOST_TIME_IN_LAST_PLACE_OVERALL_3", 0, 0 },
          { "ONLINE_AWARD_MOST_TIME_IN_LAST_PLACE_OVERALL", "ONLINE_AWARD_MOST_TIME_IN_LAST_PLACE_OVERALL_1",
            "ONLINE_AWARD_MOST_TIME_IN_LAST_PLACE_OVERALL_2", "ONLINE_AWARD_MOST_TIME_IN_LAST_PLACE_OVERALL_3", 0, 0 } },
        { 1, 1,
          { "ONLINE_AWARD_MOST_TIME_BOOSTING_OVERALL", "ONLINE_AWARD_MOST_TIME_BOOSTING_OVERALL_1",
            "ONLINE_AWARD_MOST_TIME_BOOSTING_OVERALL_2", "ONLINE_AWARD_MOST_TIME_BOOSTING_OVERALL_3", 0, 0 },
          { "ONLINE_AWARD_MOST_TIME_BOOSTING_OVERALL", "ONLINE_AWARD_MOST_TIME_BOOSTING_OVERALL_1",
            "ONLINE_AWARD_MOST_TIME_BOOSTING_OVERALL_2", "ONLINE_AWARD_MOST_TIME_BOOSTING_OVERALL_3", 0, 0 } },
        { 2, 2,
          { "ONLINE_AWARD_LONGEST_DRIFT_OVERALL", "ONLINE_AWARD_LONGEST_DRIFT_OVERALL_1", "ONLINE_AWARD_LONGEST_DRIFT_OVERALL_2",
            "ONLINE_AWARD_LONGEST_DRIFT_OVERALL_3", "ONLINE_AWARD_LONGEST_DRIFT_OVERALL_4", 0 },
          { "ONLINE_AWARD_LONGEST_DRIFT_OVERALL_SINGLE", 0, 0, 0, 0, 0 } },
    };

    namespace
    {
        // Exact ARTIST XEX tables: evidence/original_data_receipt.json.
        const s32 maiEventToObserve[12] = { 21, 64, 318, 26, 321, 133, 322, 189, 6, 269, 44, 274 };
        const TableCell::TableCellComponentTypes maTableRowComponentTypes[6] =
        {
            TableCell::E_TABLECELLCOMPONENTTYPES_TEXTFIELD,
            TableCell::E_TABLECELLCOMPONENTTYPES_ICON,
            TableCell::E_TABLECELLCOMPONENTTYPES_TEXTFIELD,
            TableCell::E_TABLECELLCOMPONENTTYPES_TEXTFIELD,
            TableCell::E_TABLECELLCOMPONENTTYPES_TEXTFIELD,
            TableCell::E_TABLECELLCOMPONENTTYPES_TEXTFIELD
        };
        const char* const maIconStates[3] = { "Red", "Blue", "Invisible" };
        const u32 KAU_TEAM_COLOURS[9] = { 0x00000000u, 0x00FF1000u, 0x000094FFu, 0x00FFFC00u, 0x00FD75FFu, 0x0060E924u, 0x00F47F18u, 0x00973AE1u, 0x001BF9E2u };
        const char* const KAPC_ANIMATION_STATES[6] =
        { "instant", "current", "final", "StuntRun_Instant", "StuntRun_Current", "StuntRun_Current" };
        const char* const KAPC_TEAM_NAMES[9] =
        { "", "$TEAM_RED", "$TEAM_BLUE", "$TEAM_YELLOW", "$TEAM_PINK", "$TEAM_GREEN", "$TEAM_ORANGE", "$TEAM_PURPLE", "$TEAM_CYAN" };
        const char* const KAPC_BUTTON_ANIMATION_STATES[4] =
        { "visible_no_continue", "visible_continue", "visible_all", "invisible" };
        const char* const KAC_POSITION_NAMES[9] =
        { "EVENT_POSITION_INVALID", "$EVENT_POSITION_FIRST", "$EVENT_POSITION_SECOND", "$EVENT_POSITION_THIRD", "$EVENT_POSITION_FOURTH", "$EVENT_POSITION_FIFTH", "$EVENT_POSITION_SIXTH", "$EVENT_POSITION_SEVENTH", "$EVENT_POSITION_EIGHTH" };
        const char* const KAPC_FINISH_STAT_STRINGID[8] =
        { "$FINISH_STAT_0", "$FINISH_STAT_1", "$FINISH_STAT_2", "$FINISH_STAT_3", "$FINISH_STAT_4", "$FINISH_STAT_5", "$FINISH_STAT_6", "$FINISH_STAT_7" };
        const f32 KF_STANDINGS_UPDATE_TIMEOUT = 2.0f;       // 8205FB74
        const f32 KF_UNORDERED_RESULTS_TIMEOUT = 15.0f;     // 8205EED0
        const f32 KF_TABLE_DATA_MIN_DURATION = 0.5f;         // 82001DA0

        bool IsStuntRunResultsMode(s32 liMode)
        {
            // These are the ARTIST switch values, including target-only mode17.
            return liMode == 14 || liMode == 12 || liMode == 17;
        }

        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;
        struct CachePayload : CgsModule::Event { GuiCache* mpCache; };
        struct ControllerButtonPayload : CgsModule::Event { s32 miPadId; s32 miButtonId; };
        struct TimeInfoPayload : CgsModule::Event { f32 mfTimeStep; f32 mfTime; };
        struct OverlayCompletePayload : CgsModule::Event
        {
            CgsID mOverlayId;
            GuiOverlayCompleteEvent::LeaveMethod meLeaveMethod;
        };
        struct StopModePayload : CgsModule::Event
        {
            s32 meGameMode;
            s32 miNumUnsuccessfulAttempts;
            u8 mbTimedOut;
            u8 mbNoRoundsRemaining;
            u8 mbModeStartedFromRegion;
            u8 muPad0B;
        };
        static_assert(sizeof(StopModePayload) == 12, "ARTIST GUI322 payload");
        static_assert(sizeof(ControllerButtonPayload) == 8, "ARTIST GUI6 stripped payload");

        // ARTIST writes type/size/offset and leaves the one-byte tag payload untouched.
        struct PostEventTag290 : CgsModule::Event { s32 GetEventType() const { return 290; } };
        struct PostEventTag292 : CgsModule::Event { s32 GetEventType() const { return 292; } };
        struct PostEventTag293 : CgsModule::Event { s32 GetEventType() const { return 293; } };
        struct PostEventTag294 : CgsModule::Event { s32 GetEventType() const { return 294; } };

        typedef BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData OnlinePlayerStatus;
        const OnlinePlayerStatus* FindOnlinePlayerStatus(const GuiCache* lpCache, s32 liActiveIndex)
        {
            // The original inlined lookup always searches all eight cache slots.
            for (s32 liPlayer = 0; liPlayer < 8; ++liPlayer)
            {
                const OnlinePlayerStatus* lpStatus = lpCache->GetOnlinePlayerInfo(liPlayer);
                if (static_cast<s32>(lpStatus->meActiveRaceCarIndex) == liActiveIndex)
                    return lpStatus;
            }
            return 0;
        }

        void OutputRaceResultAudio(CgsGui::StateInterface* lpInterface)
        {
            GuiAudioTriggerEvent lEvent;
            lEvent.Construct(0, "", "RaceResult", "");
            // The existing PC event owns a header; ARTIST emits its 100-byte body as457.
            struct AudioWire
            {
                s32 miSize, miType, miOffset;
                GuiAudioTriggerWirePayload457 mPayload;
            } lWire = { 100, 457, 12, {} };
            static_assert(sizeof(AudioWire) == 112, "ARTIST457 wrapper");
            std::memcpy(&lWire.mPayload, lEvent.macComponent, sizeof(lWire.mPayload));
            lpInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWire), 40, sizeof(lWire));
        }

        // The GUI output channel the state's out-queue events go on.
        const s32 KI_CHANNEL_GUI_OUT = 40;

        // The ticker string kinds AddString tags each line with.
        const s32 KI_TICKER_STRING_PARAMETER = 1;
        const s32 KI_TICKER_STRING_ID        = 2;

        // The award id the post event leaves in an unused slot.
        const s32 KI_ONLINE_AWARD_INVALID = -1;

        // An award value of exactly one picks the singular strings.
        const s32 KI_SINGLE_AWARD_VALUE = 1;

        // The number of online player records the GUI cache holds.
        const s32 KI_MAX_ONLINE_PLAYERS = 8;

        template <class T>
        void OutputGuiEvent(CgsGui::StateInterface* lpStateInterface, T& lrEvent)
        {
            CgsGui::GuiEventWrapper<T, KI_CHANNEL_GUI_OUT> lRecord(lrEvent);
            lpStateInterface->GetOutputEventQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRecord),
                                                              KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(lRecord)));
        }

        // ARTIST184 uses an explicit offset16 even though the committed request's
        // byte-array representation is only4-aligned. Preserve its304-byte wrapper.
        void OutputGuiEvent(CgsGui::StateInterface* lpInterface, GuiOverlayRequest& lrRequest)
        {
            struct OverlayRequestWire : CgsGui::GuiEvent<184>
            {
                u32 muPad0C;
                GuiOverlayRequest mRequest;
                explicit OverlayRequestWire(const GuiOverlayRequest& lrSource)
                    : CgsGui::GuiEvent<184>(288, 16), muPad0C(0), mRequest(lrSource) {}
            } lWire(lrRequest);
            static_assert(sizeof(OverlayRequestWire) == 304, "ARTIST184 wrapper");
            lpInterface->GetOutputEventQueue()->AddEvent(&lWire, 40, sizeof(lWire));
        }
    }

    // Clear the ticker. In the standings-update sub-state, post one looping ticker message per
    // award given: one of the award's interchangeable string ids picked at random (the singular
    // set when the award value is exactly one), then -- as that award's strings require -- the
    // winner's quoted name and the value.
    void OnlineInstantResultsState::FillOutTicker()
    {
        GuiEventTickerClearMessages lClearMessages = { { 0, 0 } };
        OutputGuiEvent(mpStateInterface, lClearMessages);

        if (meCurrentState != E_ONLINE_INSTANT_RESULTS_STANDINGSUPDATE)
        {
            // The console streams the sub-state after the text.
            CGS_ASSERT(false, "Ticker not set up for current substate : ");
            return;
        }

        for (s32 liAward = 0; liAward < mOnlinePostEvent.miNumAwardsGiven; ++liAward)
        {
            const GuiEventOnlinePostEvent::OnlineAward& lrAward = mOnlinePostEvent.maOnlineAwards[liAward];
            const s32 liAwardId = lrAward.meOnlineAwardID;
            CGS_ASSERT(liAwardId != KI_ONLINE_AWARD_INVALID, "liAwardId != BrnGameState::E_ONLINE_AWARD_INVALID");

            GuiEventTickerCustomMessage lMessage;
            lMessage.Construct(true, false, true, false);

            const AwardData& lrAwardData = KA_AWARD_OVERALL[liAwardId];
            s32 liNumParams;
            if (lrAward.miAwardVariable == KI_SINGLE_AWARD_VALUE)
            {
                const s32 liNumStrings = lrAwardData.CountStringsSingle();
                const s32 liString = mpGuiCache->GetRandomNumberGenerator()->RandomInt(0, liNumStrings - 1);
                lMessage.AddString(lrAwardData.mapcSingleString[liString], KI_TICKER_STRING_ID);
                liNumParams = lrAwardData.miSingleParamCount;
            }
            else
            {
                const s32 liNumStrings = lrAwardData.CountStringsPlural();
                const s32 liString = mpGuiCache->GetRandomNumberGenerator()->RandomInt(0, liNumStrings - 1);
                lMessage.AddString(lrAwardData.mapcMultiString[liString], KI_TICKER_STRING_ID);
                liNumParams = lrAwardData.miMultiParamCount;
            }

            if (liNumParams > 0)
            {
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

                // The cache's online player record for the awarded car (inlined lookup).
                const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerStatusData = 0;
                for (s32 liPlayer = 0; liPlayer < KI_MAX_ONLINE_PLAYERS; ++liPlayer)
                {
                    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerInfo =
                        mpGuiCache->GetOnlinePlayerInfo(liPlayer);
                    if (static_cast<s32>(lpPlayerInfo->meActiveRaceCarIndex) == lrAward.mePlayerActiveRaceCarIndex)
                    {
                        lpPlayerStatusData = lpPlayerInfo;
                        break;
                    }
                }
                CGS_ASSERT(lpPlayerStatusData != 0, "lpPlayerStatusData");

                char lacPlayerName[32];
                CgsCore::SnPrintf(lacPlayerName, sizeof(lacPlayerName), "''%s''",
                                  lpPlayerStatusData->mPlayerName.GetPlayerName());
                lacPlayerName[sizeof(lacPlayerName) - 1] = 0;
                lMessage.AddString(lacPlayerName, KI_TICKER_STRING_PARAMETER);
            }

            if (liNumParams > 1)
            {
                char lacAwardValue[10];
                CgsCore::SPrintf(lacAwardValue, 9, "%i", lrAward.miAwardVariable);
                lacAwardValue[9] = 0;
                lMessage.AddString(lacAwardValue, KI_TICKER_STRING_PARAMETER);
            }

            OutputGuiEvent(mpStateInterface, lMessage);
        }
    }
    void OnlineInstantResultsState::RenderDebug(CgsDev::Debug2DImmediateRender* lpDisplay)
    {
        char lacText[1024];
        char lacStatus[1064];

        CgsCore::SPrintf(lacText, 1024, "HELLO!");
        MaybeDrawText(lpDisplay, lacText, 80.0f, 100.0f, 20.0f,
                      static_cast<CgsDev::RGBA>(-1), false);

        const s32 liNumPlayers = mOnlinePostEvent.miNumPlayersInEvent;
        if (liNumPlayers > 0)
        {
            f32 lfRowY = 124.0f;
            for (s32 liRecord = 0; liRecord < liNumPlayers; ++liRecord)
            {
                const GuiEventOnlinePostEvent::Record& lrRecord =
                    mOnlinePostEvent.maRecords[liRecord];
                if (lrRecord.miIndex != -1)
                {
                    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerInfo = 0;
                    for (s32 liPlayer = 0; liPlayer < KI_MAX_ONLINE_PLAYERS; ++liPlayer)
                    {
                        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpCandidate =
                            mpGuiCache->GetOnlinePlayerInfo(liPlayer);
                        if (static_cast<s32>(lpCandidate->meActiveRaceCarIndex) == lrRecord.miIndex)
                        {
                            lpPlayerInfo = lpCandidate;
                            break;
                        }
                    }

                    if (lpPlayerInfo == 0)
                    {
                        CgsCore::SPrintf(lacText, 1024,
                                         "NO PLAYER INFO FOR ACTIVE RACE CAR %d",
                                         lrRecord.miIndex);
                    }
                    else
                    {
                        const char* lpcStatus;
                        if (lrRecord.mbFlag35)
                        {
                            lpcStatus = "DISCONNECTED";
                        }
                        else if (lrRecord.mbFlag34)
                        {
                            lpcStatus = "TIMED OUT";
                        }
                        else
                        {
                            const f32 lfTime = lrRecord.mTime.GetFloatVal();
                            if (lfTime <= 0.0f)
                            {
                                lpcStatus = "PLAYING";
                            }
                            else
                            {
                                CgsCore::SPrintf(lacStatus, 1024, "%.2f", lfTime);
                                lpcStatus = lacStatus;
                            }
                        }

                        CgsCore::SPrintf(lacText, 1024, "Name=%s %s TABLEPOS=%d",
                                         lpPlayerInfo->mPlayerName.GetPlayerName(),
                                         lpcStatus, static_cast<s32>(lrRecord.muValue1C));
                    }
                    MaybeDrawText(lpDisplay, lacText, 80.0f, lfRowY, 20.0f,
                                  static_cast<CgsDev::RGBA>(-1), false);
                }
                lfRowY += 24.0f;
            }

            CgsCore::SPrintf(lacText, 1024, "%d PLAYERS IN GAME", liNumPlayers);
            MaybeDrawText(lpDisplay, lacText, 80.0f, lfRowY, 20.0f,
                          static_cast<CgsDev::RGBA>(-1), false);
            CgsCore::SPrintf(lacText, 1024, "STATE=%d", static_cast<s32>(meCurrentState));
            MaybeDrawText(lpDisplay, lacText, 80.0f, lfRowY + 24.0f, 20.0f,
                          static_cast<CgsDev::RGBA>(-1), false);
        }
    }

    // ARTIST82508DC8: typed members supply the original constructors/vtables and Time defaults.
    OnlineInstantResultsState::OnlineInstantResultsState()
    {
    }

    // ARTIST82494418. Build every owned component before accepting a cache/load completion.
    void OnlineInstantResultsState::OnEnter()
    {
        mOnlinePostEvent.Clear();
        meCurrentState = E_ONLINE_INSTANT_RESULTS_LOADING_SCREEN;
        mpGuiCache = 0;
        for (s32 liPlayer = 0; liPlayer < 8; ++liPlayer)
            mOnlineFinishingOrder.maeActiveRaceCarIndexForFinishPosition[liPlayer] =
                E_ACTIVE_RACE_CAR_INDEX_INVALID;
        mpStateInterface->RegisterForEvents(maiEventToObserve, 12);

        s8 liTextFieldCount = 0;
        s8 liIconCount = 0;
        muNumTextFilesToLoad = 0;
        for (s32 liRow = 0; liRow < 8; ++liRow)
        {
            char lacRowName[64];
            CgsCore::SPrintf(lacRowName, 64, "%s_%d", "NameRow", liRow);
            lacRowName[63] = 0;
            for (s32 liColumn = 0; liColumn < 6; ++liColumn)
            {
                char lacComponentName[64];
                CgsCore::SPrintf(lacComponentName, 64, "%s_%d", "CellComponent", liColumn);
                lacComponentName[63] = 0;
                if (maTableRowComponentTypes[liColumn] == TableCell::E_TABLECELLCOMPONENTTYPES_ICON)
                {
                    CGS_ASSERT(liIconCount < Table::KI_MAX_ROWS_PER_TABLE,
                               "liIconCount < Table::KI_MAX_ROWS_PER_TABLE");
                    IconComponent* lpIcon = &maIcons[liIconCount++];
                    lpIcon->Construct(lacComponentName, mpStateInterface, maIconStates, lacRowName);
                    maapTableCellComponentPtrs[liRow][liColumn] = lpIcon;
                }
                else if (maTableRowComponentTypes[liColumn] == TableCell::E_TABLECELLCOMPONENTTYPES_TEXTFIELD)
                {
                    CGS_ASSERT(liTextFieldCount < 40,
                               "liTextFieldCount < KI_NUM_ONLINE_INSTANT_RESULTS_TEXT_FIELDS");
                    TextField* lpText = &maTextFields[liTextFieldCount++];
                    lpText->Construct(lacComponentName, mpStateInterface, lacRowName);
                    maapTableCellComponentPtrs[liRow][liColumn] = lpText;
                }
            }
        }
        muNumTextFilesToLoad = liTextFieldCount;
        mTableData.Construct();
        for (s32 liRow = 0; liRow < 8; ++liRow)
        {
            char lacRow[8];
            CgsCore::SPrintf(lacRow, 8, "Row_%d", liRow);
            lacRow[7] = 0;
            maTableRowDataSets[liRow].Clear();
            maTableRowDataSets[liRow].SetText(0, lacRow);
            for (s32 liColumn = 0; liColumn < 6; ++liColumn)
                maTableRowDataSets[liRow].SetInteger(liColumn, liColumn);
            mTableData.AddRowData(&maTableRowDataSets[liRow]);
        }
        mTable.Construct("NameRow", mpStateInterface, "Cell", maapTableCellComponentPtrs,
                         maTableRowComponentTypes, 8, 6, 0, 0xFFFFFFFFull);
        if (mTable.mbWrapped)
        {
            mTable.mbWrapped = false;
            mTable.SetDirty();
        }
        mTable.SetEnableShowingAnim(true);
        mfUpdateTimer = 0.0f;
        mfTableHadDataDuration = 0.0f;
        miItemsLoaded = 0;
        mbLeaveNextUpdate = false;
        meCurrentGameMode = BrnGameState::GameStateModuleIO::E_MODE_NONE;
        mbPlayerCanLeave = false;
        mbAbortDueToDisconnects = false;
        mbCachedDisconnectEvent = false;
        for (s32 liPlayer = 0; liPlayer < 8; ++liPlayer)
            mabDirty[liPlayer] = false;

        GuiEventTickerClearMessages lClear = { { 1, 0 } };
        OutputGuiEvent(mpStateInterface, lClear);
        mTitleTextfield.Construct("Title_mc", mpStateInterface, 0);
        mWinnerGamertag.Construct("gamertag_mc", mpStateInterface, 0);
        mCurrentRound.Construct("RoundText", mpStateInterface, 0);
        mRevengeColumnHeading.Construct("RevengeText", mpStateInterface, 0);
        mResultColumnHeading.Construct("TimeText", mpStateInterface, 0);
        mPointsColumnHeading.Construct("PointsText", mpStateInterface, 0);
        mAnimationComponent.Construct("ResultsAnim", mpStateInterface, 0);
        mButtonAnimationComponent.Construct("ButtonAnim", mpStateInterface, 0);
        mGamerTagColourAnimationComponent.Construct("Anim_GamerTag_Colour", mpStateInterface, 0);
        mYourTeamScoreHeading.Construct("YourTeamScoreHeading_mc", mpStateInterface, 0);
        mRivalTeamScoreHeading.Construct("RivalTeamScoreHeading_mc", mpStateInterface, 0);
        mYourTeamScore.Construct("YourTeamScore_mc", mpStateInterface, 0);
        mRivalTeamScore.Construct("RivalTeamScore_mc", mpStateInterface, 0);
        mYourTeamIs.Construct("YourTeamIs_mc", mpStateInterface, 0);
        PostEventTag290 lEntered;
        OutputGuiEvent(mpStateInterface, lEntered);
        mbReceivedFinishedModeResults = false;
    }

    // ARTIST82494F30. Keep the original cache write-back and both GUI output routes.
    void OnlineInstantResultsState::OnLeave()
    {
        GuiEventTickerClearMessages lClear = { { 1, 0 } };
        OutputGuiEvent(mpStateInterface, lClear);
        mpGuiCache->RecEvent(reinterpret_cast<const CgsModule::Event*>(&mOnlineFinishingOrder), 317);
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, 12);
        mpStateInterface->PlayAptMovie("", 3);
        GuiEventNetworkOutputPlayerTexture lTexture = { GuiEventNetworkOutputPlayerTexture::E_OUTPUT_OFF, -1 };
        OutputGuiEvent(mpStateInterface, lTexture);
        GuiRequestCarControlChangeEvent lControl = { { 1 } };
        OutputGuiEvent(mpStateInterface, lControl);
        PostEventTag294 lLeft;
        OutputGuiEvent(mpStateInterface, lLeft);
    }

    // ARTIST824A25A0: resource completion then the actual expected-component set.
    void OnlineInstantResultsState::CheckForCompletedLoads()
    {
        if (meCurrentState == E_ONLINE_INSTANT_RESULTS_LOADING_SCREEN)
        {
            if (mpGuiCache->EnsureResourcesAreLoaded(maResourceTuplesToLoad, 1))
            {
                mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[226], 3);
                meCurrentState = E_ONLINE_INSTANT_RESULTS_LOADING_COMPONENTS;
            }
        }
        else if (meCurrentState == E_ONLINE_INSTANT_RESULTS_LOADING_COMPONENTS && mpGuiCache &&
                 mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        {
            OutputRaceResultAudio(mpStateInterface);
            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
            if (IsStuntRunResultsMode(meCurrentGameMode))
            {
                mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mTitleTextfield.GetName());
                mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mWinnerGamertag.GetName());
                mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mCurrentRound.GetName());
                mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mResultColumnHeading.GetName());
                mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mRevengeColumnHeading.GetName());
                mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mPointsColumnHeading.GetName());
                mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mYourTeamScoreHeading.GetName());
                mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mRivalTeamScoreHeading.GetName());
                mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mYourTeamScore.GetName());
                mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mRivalTeamScore.GetName());
            }
            UpdateCurrentScreenDisplay();
            mTable.SetupTable(&mTableData, true, true);
            for (s32 liRow = 0; liRow < 8; ++liRow)
            {
                mTable.GetSelectable(liRow)->SetActive(false);
                mTable.GetSelectable(liRow)->SetHighlighted(false);
                mTable.SetIconState(liRow, 1, 2);
            }
            mTable.Update();
            mTitleTextfield.SetText("$ONLINE_RESULTS_RESULTS_TITLE");
            mPointsColumnHeading.SetText("$POINTS_RESULTS");
            if (meCurrentGameMode == 13)
                mRevengeColumnHeading.SetText("");
            else
                mRevengeColumnHeading.SetLocalisedText("REVENGE_RESULTS", CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
            SetResultColumnHeading();
            PopulateNameAndTimeTableCells(mpGuiCache->GetOnlinePostEventData());
            SetRoundText();
            if (IsStuntRunResultsMode(meCurrentGameMode))
                UpdateStuntRunColoursAndScores();
            if (mbReceivedFinishedModeResults)
            {
                meCurrentState = E_ONLINE_INSTANT_RESULTS_STANDINGSUPDATE;
                FillOutTicker();
            }
            else
                meCurrentState = E_ONLINE_INSTANT_RESULTS_UPDATING;
        }
        else if (meCurrentState == E_ONLINE_INSTANT_RESULTS_LOADING_STANDINGS && mpGuiCache &&
                 mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        {
            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
            ReorderResultsTable();
            if (meCurrentGameMode == 12 && mpGuiCache->miOnlineRoundIndex < mpGuiCache->miOnlineNumRounds - 1)
                SetResultColumnHeading();
            else
                mResultColumnHeading.SetText("");
            mRevengeColumnHeading.SetText("");
            SetRoundText();
        }
    }

    // ARTIST82483C28. An absent table position asserts and returns the original fallback 0.
    s32 OnlineInstantResultsState::GetOnlinePlayerIndexFromTablePosition(s32 liTablePosition)
    {
        for (s32 liPlayer = 0; liPlayer < mOnlinePostEvent.miNumPlayersInEvent; ++liPlayer)
            if (static_cast<s32>(mOnlinePostEvent.maRecords[liPlayer].muValue1C) == liTablePosition)
                return liPlayer;
        CGS_ASSERT(false, "Could not find results for table position");
        return 0;
    }

    // DecFIGS h:803/819; ARTIST inlines these addresses at82494B00/8248B428.
    GuiEventOnlinePostEvent::Record* OnlineInstantResultsState::GetOnlinePlayerFromTablePosition(s32 liPosition)
    {
        return &mOnlinePostEvent.maRecords[GetOnlinePlayerIndexFromTablePosition(liPosition)];
    }
    GuiEventOnlinePostEvent::Record* OnlineInstantResultsState::GetOnlinePlayerFromSortedTablePosition(s32 liPosition)
    {
        return &mOnlinePostEvent.maRecords[maiOnlinePlayersIndexFromTablePosition[liPosition]];
    }

    // ARTIST82483D70: old/new table-position changes and newly appended records stay dirty.
    void OnlineInstantResultsState::UpdateDirtyFlags(const GuiEventOnlinePostEvent* lpEvent)
    {
        s32 liPlayer = 0;
        for (; liPlayer < mOnlinePostEvent.miNumPlayersInEvent && liPlayer < lpEvent->miNumPlayersInEvent; ++liPlayer)
            if (mOnlinePostEvent.maRecords[liPlayer].muValue1C != lpEvent->maRecords[liPlayer].muValue1C)
                mabDirty[liPlayer] = true;
        for (; liPlayer < lpEvent->miNumPlayersInEvent; ++liPlayer)
            mabDirty[liPlayer] = true;
    }

    // ARTIST82483E10. The last two stunt-run labels intentionally have identical text.
    void OnlineInstantResultsState::UpdateCurrentScreenDisplay()
    {
        const bool lbStuntRun = IsStuntRunResultsMode(meCurrentGameMode);
        if (meCurrentState == E_ONLINE_INSTANT_RESULTS_LOADING_COMPONENTS)
        {
            mAnimationComponent.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[lbStuntRun ? 3 : 0], false);
            mButtonAnimationComponent.AddOutputAptViewState("apt_Transition", KAPC_BUTTON_ANIMATION_STATES[3], false);
        }
        else
        {
            const bool lbFinal = mpGuiCache->miOnlineRoundIndex == mpGuiCache->miOnlineNumRounds - 1;
            const s32 liState = lbStuntRun ? (lbFinal ? 5 : 4) : (lbFinal ? 2 : 1);
            mAnimationComponent.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[liState], false);
        }
    }

    // ARTIST82483FA8. Preserve the target's mode switch, including12/14/17.
    void OnlineInstantResultsState::SetResultColumnHeading()
    {
        switch (meCurrentGameMode)
        {
        case 10: mResultColumnHeading.SetText("$TIME_RESULTS"); break;
        case 11: mResultColumnHeading.SetText("$ONLINE_RESULTS_RR_RESULT_COLUMN_TITLE"); break;
        case 12: case 14: case 17:
            mResultColumnHeading.SetText("$ONLINE_RESULTS_STUNT_RUN_RESULT_COLUMN_TITLE"); break;
        case 13: mResultColumnHeading.SetText("$ONLINE_RESULTS_BHR_RESULT_COLUMN_TITLE"); break;
        default: CGS_ASSERT(false, "Invalid online game mode of:"); break;
        }
    }

    // ARTIST82484118. The visible round number is the original zero-based index plus1.
    void OnlineInstantResultsState::SetRoundText()
    {
        const char* lpcSource = meCurrentState < E_ONLINE_INSTANT_RESULTS_STANDINGSUPDATE
            ? "ONLINE_RESULTS_ROUND_NUMBER" : (mpGuiCache->miOnlineRoundIndex != 0
                ? "ONLINE_RESULTS_AFTER_ROUNDS" : "ONLINE_RESULTS_AFTER_ROUND");
        char lacRound[64];
        CgsCore::SPrintf(lacRound, 64, "%d", mpGuiCache->miOnlineRoundIndex + 1);
        mpStateInterface->GetLanguageManager()->FormatAndAddText(
            "CURRENT_ROUND", lpcSource, CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
            1, lacRound, CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
        mCurrentRound.SetText("$CURRENT_ROUND");
    }

    // ARTIST824A29C0/824955A0: only final-standings states receive table/accept inputs.
    void OnlineInstantResultsState::HandleControllerInputPressed(const CgsGui::GuiEventControllerInputPressed* lpEvent)
    {
        CGS_ASSERT(lpEvent, "Invalid event sent to OnlineInstantResultsState::HandleControllerInputPressed");
        if (meCurrentState == E_ONLINE_INSTANT_RESULTS_FINAL_STANDINGSUPDATE ||
            meCurrentState == E_ONLINE_INSTANT_RESULTS_FINAL_STANDINGS)
            HandleControllerInputFinalStandingsSubState(lpEvent);
    }
    void OnlineInstantResultsState::HandleControllerInputFinalStandingsSubState(
        const CgsGui::GuiEventControllerInputPressed* lpEvent)
    {
        switch (lpEvent->miButtonId)
        {
        case E_GAMEINPUTACTIONS_GUI_UP: mTable.HighlightPrevious(); break;
        case E_GAMEINPUTACTIONS_GUI_DOWN: mTable.HighlightNext(); break;
        case E_GAMEINPUTACTIONS_GUI_SELECT:
            if (mbPlayerCanLeave) mbLeaveNextUpdate = true;
            break;
        case E_GAMEINPUTACTIONS_GUI_OPTION0:
            if (static_cast<u8>(mTable.GetHighlightedIndex()) < 128)
            {
                GuiEventNetworkSelectedPlayerOption lOption;
                lOption.meOptionSelected = GuiEventNetworkSelectedPlayerOption::E_OPTION_SELECTED_VIEW_GAMERCARD;
                lOption.mSelectedPlayerID = static_cast<s32>(mTable.GetHighlightedId());
                OutputGuiEvent(mpStateInterface, lOption);
            }
            break;
        default: break;
        }
    }

    // ARTIST824844D0: cache a disconnect until the final standings can expose its leave path.
    void OnlineInstantResultsState::HandleDisconnectedEvent(const CgsGui::GuiEventNetworkDisconnected* lpEvent)
    {
        if (meCurrentState == E_ONLINE_INSTANT_RESULTS_FINAL_STANDINGS)
        {
            mpStateInterface->GetAccessPointers()->GetGuiCache()->SetDoDisconnectPopup(
                reinterpret_cast<const CgsModule::Event*>(lpEvent));
            mbPlayerCanLeave = true;
            mbAbortDueToDisconnects = true;
            mbCachedDisconnectEvent = false;
            mButtonAnimationComponent.AddOutputAptViewState("apt_Transition", KAPC_BUTTON_ANIMATION_STATES[2], false);
        }
        else
        {
            mbCachedDisconnectEvent = true;
            mDisconnectedEvent = *lpEvent;
        }
    }

    // ARTIST824956D8. The normal and disconnect outcomes retain their original consumers.
    void OnlineInstantResultsState::LeaveScreen()
    {
        if (mbAbortDueToDisconnects)
        {
            CGS_ASSERT(mpGuiCache, "mpGuiCache");
            GuiOverlayRequest lOverlay;
            const s32 liError = mpGuiCache->GetDoDisconnectPopupError();
            if (liError != 0)
            {
                lOverlay.Construct(liError == 19 ? "CNLobbyDiscD" : "CNLobbyDisc");
                mpGuiCache->SetDoDisconnectPopup(0);
            }
            else
            {
                const bool lbPlayerDisconnected = mpGuiCache->GetOnlinePlayerDisconnected(
                    static_cast<EActiveRaceCarIndex>(mpGuiCache->GetPlayerActiveRaceCarIndex()));
                lOverlay.Construct(lbPlayerDisconnected ? "GMModeDisc" : "GMModeNoPlys");
            }
            OutputGuiEvent(mpStateInterface, lOverlay);
            GuiEventRunFsm lRun;
            lRun.mFsmId = CgsIDCompress("BRNFBFSM");
            lRun.mInitialStateId = CgsIDCompress("PAUSED");
            lRun.meFsmToRun = E_GUI_HUD_FREEBURN;
            lRun.meFlowToUse = E_GUIFLOW_HUD;
            OutputGuiEvent(mpStateInterface, lRun);
            mpStateInterface->PlayAptMovie("", 3);
        }
        else
        {
            PostEventTag293 lLeftPostEvent;
            OutputGuiEvent(mpStateInterface, lLeftPostEvent);
            GuiEventRunFsm lRun;
            lRun.mInitialStateId = 0;
            if (mpGuiCache->GetCurrentGameModeType() == -1)
            {
                lRun.mFsmId = CgsIDCompress("BRNFBFSM");
                lRun.meFsmToRun = E_GUI_HUD_FREEBURN;
            }
            else
            {
                lRun.mFsmId = CgsIDCompress("BRNEVENTFSM");
                lRun.meFsmToRun = E_GUI_HUD_EVENT;
            }
            lRun.meFlowToUse = E_GUIFLOW_HUD;
            OutputGuiEvent(mpStateInterface, lRun);
            SendStateEvent("TO_GAME_ROOM");
        }
    }

    // ARTIST824AB808. Process the registered original events, then loading/timer/leave work.
    void OnlineInstantResultsState::Update()
    {
        StateInputQueue* lpQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liEventId = lpQueue->GetFirstEvent(&lpEvent, &liSize);
        for (s32 liPlayer = 0; liPlayer < mOnlinePostEvent.miNumPlayersInEvent; ++liPlayer)
        {
            CGS_ASSERT(static_cast<s32>(mOnlinePostEvent.maRecords[liPlayer].muValue1C) >= 0,
                       "mOnlinePostEvent.maOnlinePlayers[liPositionIndex].miResultsTablePosition >= 0");
            CGS_ASSERT(static_cast<s32>(mOnlinePostEvent.maRecords[liPlayer].muValue1C) < mOnlinePostEvent.miNumPlayersInEvent,
                       "mOnlinePostEvent.maOnlinePlayers[liPositionIndex].miResultsTablePosition < mOnlinePostEvent.miNumPlayersInEvent");
        }
        mTable.Update();
        while (lpEvent)
        {
            switch (liEventId)
            {
            case 6:
            {
                // The queue strips its header. Adapt the two actual payload words to the
                // committed typed input event before calling the original typed listener.
                const ControllerButtonPayload* lpInput = static_cast<const ControllerButtonPayload*>(lpEvent);
                CgsGui::GuiEventControllerInputPressed lInput;
                lInput.miPadId = lpInput->miPadId;
                lInput.miButtonId = lpInput->miButtonId;
                HandleControllerInputPressed(&lInput);
                break;
            }
            case 21: break;
            case 26:
                if (meCurrentState == E_ONLINE_INSTANT_RESULTS_STANDINGSUPDATE ||
                    meCurrentState == E_ONLINE_INSTANT_RESULTS_CURRENT_STANDINGSUPDATE ||
                    meCurrentState == E_ONLINE_INSTANT_RESULTS_FINAL_STANDINGSUPDATE)
                    mfUpdateTimer += static_cast<const TimeInfoPayload*>(lpEvent)->mfTimeStep;
                break;
            case 44:
                HandleDisconnectedEvent(reinterpret_cast<const CgsGui::GuiEventNetworkDisconnected*>(lpEvent));
                break;
            case 64:
                if (!mpGuiCache)
                {
                    mpGuiCache = static_cast<const CachePayload*>(lpEvent)->mpCache;
                    meCurrentGameMode = static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(
                        mpGuiCache->GetCurrentGameModeType());
                    CGS_ASSERT(meCurrentGameMode != BrnGameState::GameStateModuleIO::E_MODE_NONE,
                               "meCurrentGameMode != GsmIO::E_MODE_NONE");
                    mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
                    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mAnimationComponent.GetName());
                    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mButtonAnimationComponent.GetName());
                    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mTitleTextfield.GetName());
                    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mWinnerGamertag.GetName());
                    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mCurrentRound.GetName());
                    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mResultColumnHeading.GetName());
                    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mRevengeColumnHeading.GetName());
                    if (IsStuntRunResultsMode(meCurrentGameMode))
                        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mPointsColumnHeading.GetName());
                    mTable.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
                }
                break;
            case 133:
            {
                CGS_ASSERT(lpEvent, "lpInviteFailedEvent");
                const s32 liFailure = *reinterpret_cast<const s32*>(lpEvent);
                if (liFailure == 1)
                {
                    GuiOverlayRequest lOverlay;
                    lOverlay.Construct("CNInvSmGame");
                    OutputGuiEvent(mpStateInterface, lOverlay);
                }
                break;
            }
            case 189:
            {
                CGS_ASSERT(lpEvent, "lpCompleteEvent");
                const OverlayCompletePayload* lpComplete = static_cast<const OverlayCompletePayload*>(lpEvent);
                if ((lpComplete->mOverlayId == CgsIDCompress("GMModeDisc") ||
                     lpComplete->mOverlayId == CgsIDCompress("GMModeNoPlys") ||
                     lpComplete->mOverlayId == CgsIDCompress("CNLobbyDiscD") ||
                     lpComplete->mOverlayId == CgsIDCompress("CNLobbyDisc")) &&
                    lpComplete->meLeaveMethod == GuiOverlayCompleteEvent::E_LEAVEMETHOD_OK)
                    SendStateEvent("DISCONNECT");
                break;
            }
            case 269:
                if (reinterpret_cast<const GuiEventNetworkSplashEvent*>(lpEvent)->meSplashState ==
                    GuiEventNetworkSplashEvent::E_SPLASH_STATE_STOP)
                {
                    GuiEventNetworkSplashEvent lSplash = { GuiEventNetworkSplashEvent::E_SPLASH_STATE_FINISHED };
                    OutputGuiEvent(mpStateInterface, lSplash);
                }
                break;
            case 274:
            {
                CGS_ASSERT(lpEvent, "lpPostGameProcessingFinished");
                const GuiEventNetworkPostGameProcessingFinished* lpFinished =
                    reinterpret_cast<const GuiEventNetworkPostGameProcessingFinished*>(lpEvent);
                mbPlayerCanLeave = true;
                mbAbortDueToDisconnects = lpFinished->maData[0] == 0;
                mButtonAnimationComponent.AddOutputAptViewState("apt_Transition", KAPC_BUTTON_ANIMATION_STATES[2], false);
                break;
            }
            case 318:
                if (meCurrentState != E_ONLINE_INSTANT_RESULTS_FINAL_STANDINGSUPDATE &&
                    meCurrentState != E_ONLINE_INSTANT_RESULTS_FINAL_STANDINGS)
                    PopulateNameAndTimeTableCells(reinterpret_cast<const GuiEventOnlinePostEvent*>(lpEvent));
                break;
            case 321:
                if (meCurrentState > E_ONLINE_INSTANT_RESULTS_LOADING_COMPONENTS)
                {
                    meCurrentState = E_ONLINE_INSTANT_RESULTS_STANDINGSUPDATE;
                    FillOutTicker();
                }
                else
                    mbReceivedFinishedModeResults = true;
                break;
            case 322:
            {
                const StopModePayload* lpStopped = static_cast<const StopModePayload*>(lpEvent);
                if (lpStopped->meGameMode < 10 || lpStopped->meGameMode == 15 || lpStopped->meGameMode == 16)
                {
                    mbPlayerCanLeave = true;
                    mbAbortDueToDisconnects = true;
                    mButtonAnimationComponent.AddOutputAptViewState("apt_Transition", KAPC_BUTTON_ANIMATION_STATES[2], false);
                    GuiRequestCarControlChangeEvent lControl = { { 0 } };
                    OutputGuiEvent(mpStateInterface, lControl);
                }
                else if (lpStopped->mbModeStartedFromRegion || lpStopped->mbNoRoundsRemaining)
                {
                    mbPlayerCanLeave = false;
                    mbAbortDueToDisconnects = lpStopped->mbModeStartedFromRegion != 0;
                    GuiRequestCarControlChangeEvent lControl = { { 0 } };
                    OutputGuiEvent(mpStateInterface, lControl);
                }
                else
                    SendStateEvent("TO_ON_PLAY");
                break;
            }
            default:
                CGS_ASSERT(false, "Unexpected event in OnlineInstantResultsState::Update");
                break;
            }
            liEventId = lpQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }
        lpQueue->Clear();
        CheckForCompletedLoads();

        if (meCurrentState == E_ONLINE_INSTANT_RESULTS_STANDINGSUPDATE &&
            mfUpdateTimer > KF_STANDINGS_UPDATE_TIMEOUT && mpGuiCache &&
            mOnlinePostEvent.miNumPlayersInEvent > 0)
        {
            if (meCurrentGameMode == 17)
            {
                if (mpGuiCache->miOnlineRoundIndex == mpGuiCache->miOnlineNumRounds - 1)
                    mfUpdateTimer = 16.0f; // ARTIST82004000, deliberately passes the 15-second leg below.
                mButtonAnimationComponent.AddOutputAptViewState("apt_Transition", KAPC_BUTTON_ANIMATION_STATES[2], false);
                mbPlayerCanLeave = true;
            }
            else
            {
                UpdateCurrentScreenDisplay();
                mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
                mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mTitleTextfield.GetName());
                mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mWinnerGamertag.GetName());
                mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mCurrentRound.GetName());
                mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mResultColumnHeading.GetName());
                mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mRevengeColumnHeading.GetName());
                if (IsStuntRunResultsMode(meCurrentGameMode))
                {
                    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mPointsColumnHeading.GetName());
                    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mYourTeamScoreHeading.GetName());
                    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mRivalTeamScoreHeading.GetName());
                    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mYourTeamScore.GetName());
                    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mRivalTeamScore.GetName());
                    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mYourTeamIs.GetName());
                }
                mTable.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
                meCurrentState = E_ONLINE_INSTANT_RESULTS_LOADING_STANDINGS;
            }
        }
        if (mfUpdateTimer > KF_UNORDERED_RESULTS_TIMEOUT)
        {
            PostEventTag292 lFinished;
            OutputGuiEvent(mpStateInterface, lFinished);
            mfUpdateTimer = 0.0f;
            meCurrentState = E_ONLINE_INSTANT_RESULTS_FINAL_STANDINGS;
        }
        if (mpGuiCache)
        {
            for (s32 liPlayer = 0; liPlayer < mpGuiCache->miLobbyNumPlayers; ++liPlayer)
            {
                // A typed copy from the existing pointer-free cache mirror; no console offsets.
                BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData lLobbyPlayer;
                std::memcpy(&lLobbyPlayer, mpGuiCache->maLobbyPlayerInfo[liPlayer], sizeof(lLobbyPlayer));
                if (lLobbyPlayer.meReadyStatus == BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData::E_READY_STATUS_HOST)
                {
                    mbLeaveNextUpdate = true;
                    break;
                }
            }
        }
        if (mbLeaveNextUpdate && mbPlayerCanLeave)
        {
            if (mbCachedDisconnectEvent)
                HandleDisconnectedEvent(&mDisconnectedEvent);
            LeaveScreen();
            mbLeaveNextUpdate = false;
        }
    }

    // ARTIST82494A68: snapshot/dirty tracking, winner picture, then ordered finished rows.
    void OnlineInstantResultsState::PopulateNameAndTimeTableCells(const GuiEventOnlinePostEvent* lpEvent)
    {
        UpdateDirtyFlags(lpEvent);
        mOnlinePostEvent = *lpEvent;
        if (mTable.HasData() && mpGuiCache)
            mfTableHadDataDuration += mpGuiCache->GetTimeStep();
        else
            mfTableHadDataDuration = 0.0f;

        if (meCurrentState == E_ONLINE_INSTANT_RESULTS_UPDATING && mabDirty[0] == true)
        {
            const GuiEventOnlinePostEvent::Record* lpPlayer = GetOnlinePlayerFromTablePosition(0);
            CGS_ASSERT(lpPlayer, "lpOnlinePlayer");
            CGS_ASSERT(mpGuiCache, "mpGuiCache");
            CGS_ASSERT(lpPlayer->miIndex >= 0, "lpOnlinePlayer->meActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(lpPlayer->miIndex < 8, "lpOnlinePlayer->meActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
            const OnlinePlayerStatus* lpStatus = FindOnlinePlayerStatus(mpGuiCache, lpPlayer->miIndex);
            CGS_ASSERT(lpStatus, "lpInGamePlayerStatusData");
            GuiEventNetworkOutputPlayerTexture lPicture =
                { GuiEventNetworkOutputPlayerTexture::E_OUTPUT_WINNER_IMAGE, lpStatus->mNetworkPlayerID };
            OutputGuiEvent(mpStateInterface, lPicture);
        }
        if ((meCurrentState == E_ONLINE_INSTANT_RESULTS_UPDATING ||
             meCurrentState == E_ONLINE_INSTANT_RESULTS_SHOWING) &&
            mfTableHadDataDuration > KF_TABLE_DATA_MIN_DURATION)
        {
            s32 liFinishedPlayers = 0;
            s32 liUnfinishedPlayers = 0;
            for (s32 liPosition = 0; liPosition < mOnlinePostEvent.miNumPlayersInEvent; ++liPosition)
            {
                const GuiEventOnlinePostEvent::Record* lpPlayer = GetOnlinePlayerFromTablePosition(liPosition);
                CGS_ASSERT(lpPlayer, "lpOnlinePlayer");
                if (lpPlayer->mTime > CgsSystem::Time(0.0f) || lpPlayer->mbFlag35 ||
                    lpPlayer->mbFlag34 || lpPlayer->mbFlag36)
                {
                    ++liFinishedPlayers;
                    CGS_ASSERT(liUnfinishedPlayers == 0, "liUnfinishedPlayers == 0");
                }
                else
                    ++liUnfinishedPlayers;
            }
            CGS_ASSERT(liFinishedPlayers + liUnfinishedPlayers == mOnlinePostEvent.miNumPlayersInEvent,
                       "(liFinishedPlayers + liUnfinishedPlayers) == mOnlinePostEvent.miNumPlayersInEvent");
            for (s32 liPosition = 0; liPosition < liFinishedPlayers; ++liPosition)
            {
                const GuiEventOnlinePostEvent::Record* lpPlayer = GetOnlinePlayerFromTablePosition(liPosition);
                const s32 liPlayerIndex = GetOnlinePlayerIndexFromTablePosition(liPosition);
                if (mabDirty[liPlayerIndex])
                {
                    SetAptData(liPosition, lpPlayer, true, false, false);
                    mabDirty[liPlayerIndex] = false;
                }
            }
            const GuiEventOnlinePostEvent::Record* lpWinner = GetOnlinePlayerFromTablePosition(0);
            const OnlinePlayerStatus* lpStatus = FindOnlinePlayerStatus(mpGuiCache, lpWinner->miIndex);
            mWinnerGamertag.SetText(lpStatus->mPlayerName.GetPlayerName());
            if (IsStuntRunResultsMode(meCurrentGameMode))
                UpdateStuntRunColoursAndScores();
            if (liFinishedPlayers == mOnlinePostEvent.miNumPlayersInEvent)
                meCurrentState = E_ONLINE_INSTANT_RESULTS_SHOWING;
        }
    }

    // ARTIST8248A808. r6=this-round, r7=hide repeated team, r8=team results.
    void OnlineInstantResultsState::SetAptData(s32 liRow, const GuiEventOnlinePostEvent::Record* lpPlayer,
                                               bool lbResultsForThisRoundOnly, bool lbHideRepeatedTeam,
                                               bool lbTeamResults)
    {
        CGS_ASSERT(mpGuiCache, "mpGuiCache");
        CGS_ASSERT(lpPlayer, "lpPostEventOnlinePlayer");
        CGS_ASSERT(lpPlayer->miIndex >= 0, "lpPostEventOnlinePlayer->meActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(lpPlayer->miIndex < 8, "lpPostEventOnlinePlayer->meActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        const OnlinePlayerStatus* lpStatus = FindOnlinePlayerStatus(mpGuiCache, lpPlayer->miIndex);
        CGS_ASSERT(lpStatus, "lpInGamePlayerStatusData");
        CGS_ASSERT(mTable.HasData(), "mTable.HasData() == true");
        mTable.GetSelectable(liRow)->SetActive(true);
        const s32 liTeam = mpGuiCache->GetCurrentOnlinePlayerTeam(static_cast<EActiveRaceCarIndex>(lpPlayer->miIndex));
        if (IsStuntRunResultsMode(meCurrentGameMode))
            mTable.SetColourValue(liRow, 0, KAU_TEAM_COLOURS[liTeam]);
        mTable.SetText(liRow, 0, lpStatus->mPlayerName.GetPlayerName());

        CgsLanguage::LanguageManager* lpLanguage = mpStateInterface->GetLanguageManager();
        const char* lpcStat = KAPC_FINISH_STAT_STRINGID[liRow];
        char lacValue[64];
        if (lpPlayer->mbFlag35)
        {
            lpLanguage->AddString(lpcStat + 1, lpLanguage->FindString("EVENT_POSITION_DISCONNECTED"));
        }
        else if (meCurrentGameMode == 13)
        {
            if (lpPlayer->muValue2C == 0)
                lpLanguage->AddString(lpcStat + 1, lpLanguage->FindString("ONLINE_RESULTS_HOME_RUN"));
            else
            {
                const char* lpcSource = lpPlayer->muValue28 == 1 ? "ONLINE_RESULTS_NUM_LANDMARK" : "ONLINE_RESULTS_NUM_LANDMARKS";
                CgsCore::SPrintf(lacValue, 64, "%d", static_cast<s32>(lpPlayer->muValue30));
                lpLanguage->FormatAndAddText(lpcStat + 1, lpcSource,
                    CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 1, lacValue, CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
            }
        }
        else if (meCurrentGameMode == 11 &&
                 mpGuiCache->GetCurrentOnlinePlayerTeam(static_cast<EActiveRaceCarIndex>(lpPlayer->miIndex)) == 1)
        {
            const char* lpcSource = lpPlayer->muValue24 == 1 ? "ONLINE_RESULTS_NUM_ELIMINATION" : "ONLINE_RESULTS_NUM_ELIMINATIONS";
            CgsCore::SPrintf(lacValue, 64, "%d", static_cast<s32>(lpPlayer->muValue24));
            lpLanguage->FormatAndAddText(lpcStat + 1, lpcSource,
                CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 1, lacValue, CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
        }
        else if (IsStuntRunResultsMode(meCurrentGameMode))
        {
            CgsCore::SPrintf(lacValue, 64, "%d", static_cast<s32>(
                lbTeamResults ? mOnlinePostEvent.maHeader[liTeam] : lpPlayer->muValue14));
            lpLanguage->FormatAndAddText(lpcStat + 1, "%1", CgsLanguage::LanguageManager::E_FORMAT_TEXT,
                1, lacValue, CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
        }
        else if (lpPlayer->mbFlag36 || lpPlayer->mbFlag34)
        {
            CgsCore::SPrintf(lacValue, 64, "%f", static_cast<double>(lpPlayer->mfValue10));
            lpLanguage->FormatAndAddText(lpcStat + 1,
                lpPlayer->mbFlag36 ? "ONLINE_RESULTS_ELIMINATED" : "ONLINE_RESULTS_DID_NOT_FINISH",
                CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 1, lacValue, CgsLanguage::LanguageManager::E_FORMAT_SMALL_DISTANCE);
        }
        else
        {
            CgsCore::SPrintf(lacValue, 64, "%f", static_cast<double>(lpPlayer->mTime.GetFloatVal()));
            lpLanguage->FormatAndAddText(lpcStat + 1, "%1", CgsLanguage::LanguageManager::E_FORMAT_TEXT,
                1, lacValue, CgsLanguage::LanguageManager::E_FORMAT_MINUTES_SECONDS_HUNDREDTHS);
        }
        const bool lbHideStat = lbHideRepeatedTeam ||
            (meCurrentState == E_ONLINE_INSTANT_RESULTS_CURRENT_STANDINGSUPDATE && !lbTeamResults) ||
            meCurrentState == E_ONLINE_INSTANT_RESULTS_FINAL_STANDINGSUPDATE ||
            meCurrentState == E_ONLINE_INSTANT_RESULTS_FINAL_STANDINGS;
        mTable.SetText(liRow, 3, lbHideStat ? "" : lpcStat);

        if (lbHideRepeatedTeam || meCurrentGameMode == 13)
        {
            mTable.SetIconState(liRow, 1, 2);
            mTable.SetText(liRow, 2, " ");
        }
        else
        {
            char lacRevenge[8];
            const s32 liRevenge = lpStatus->mLiveRevengeRelationship.GetCurrentScoreForLocalPlayer();
            s32 liIcon;
            if (liRevenge > 0)
            {
                CgsCore::SPrintf(lacRevenge, 8, "%d", liRevenge);
                liIcon = 0;
            }
            else if (liRevenge < 0)
            {
                CgsCore::SPrintf(lacRevenge, 8, "%d", -liRevenge);
                liIcon = 1;
            }
            else
            {
                CgsCore::SPrintf(lacRevenge, 8, " ");
                liIcon = 2;
            }
            mTable.SetIconState(liRow, 1, liIcon);
            lacRevenge[7] = 0;
            mTable.SetText(liRow, 2, lacRevenge);
        }
        char lacPoints[8];
        CgsCore::SPrintf(lacPoints, 8, "%d", static_cast<s32>(
            lbResultsForThisRoundOnly ? lpPlayer->muValue18 : lpPlayer->muValue20));
        lacPoints[7] = 0;
        const bool lbHidePoints = lbHideRepeatedTeam ||
            ((meCurrentGameMode == 12 || meCurrentGameMode == 17) && meCurrentState <= E_ONLINE_INSTANT_RESULTS_STANDINGSUPDATE);
        mTable.SetText(liRow, 4, lbHidePoints ? "" : lacPoints);
        const bool lbShowPosition = (lbHideRepeatedTeam && !IsStuntRunResultsMode(meCurrentGameMode)) ||
            meCurrentState < E_ONLINE_INSTANT_RESULTS_STANDINGSUPDATE;
        mTable.SetText(liRow, 5, lbShowPosition ? KAC_POSITION_NAMES[liRow + 1] : "");
        if (!lbHideRepeatedTeam && lbTeamResults && meCurrentGameMode == 12 &&
            meCurrentState >= E_ONLINE_INSTANT_RESULTS_STANDINGSUPDATE)
        {
            mTable.SetColourValue(liRow, 5, KAU_TEAM_COLOURS[liTeam]);
            mTable.SetText(liRow, 5, KAPC_TEAM_NAMES[liTeam]);
        }
        if (mTable.GetSelectable(liRow)->IsHighlighted())
            mTable.GetSelectable(liRow)->SetHighlighted(false);
    }

    // ARTIST8248AFF8: descending cumulative points; original team repetition/tie rules.
    void OnlineInstantResultsState::ReorderResultsTable()
    {
        for (s32 liPlayer = 0; liPlayer < mOnlinePostEvent.miNumPlayersInEvent; ++liPlayer)
            maiOnlinePlayersIndexFromTablePosition[liPlayer] = liPlayer;
        for (s32 liRow = 0; liRow < mOnlinePostEvent.miNumPlayersInEvent; ++liRow)
            for (s32 liOther = liRow + 1; liOther < mOnlinePostEvent.miNumPlayersInEvent; ++liOther)
                if (static_cast<s32>(GetOnlinePlayerFromSortedTablePosition(liRow)->muValue20) <
                    static_cast<s32>(GetOnlinePlayerFromSortedTablePosition(liOther)->muValue20))
                {
                    const s32 liIndex = maiOnlinePlayersIndexFromTablePosition[liRow];
                    maiOnlinePlayersIndexFromTablePosition[liRow] = maiOnlinePlayersIndexFromTablePosition[liOther];
                    maiOnlinePlayersIndexFromTablePosition[liOther] = liIndex;
                }
        if (mpGuiCache)
        {
            mTable.SetupTable(&mTableData, true, true);
            for (s32 liRow = 0; liRow < 8; ++liRow)
            {
                mTable.GetSelectable(liRow)->SetActive(false);
                mTable.GetSelectable(liRow)->SetHighlighted(false);
                mTable.SetIconState(liRow, 1, 2);
            }
            mTable.Update();
            if (mOnlinePostEvent.miNumPlayersInEvent > 0)
            {
                if (mpGuiCache->miOnlineRoundIndex == mpGuiCache->miOnlineNumRounds - 1)
                {
                    mTitleTextfield.SetText("$ONLINE_RESULTS_FINAL_STANDINGS_TITLE");
                    if (!IsStuntRunResultsMode(meCurrentGameMode))
                        mButtonAnimationComponent.AddOutputAptViewState("apt_Transition", KAPC_BUTTON_ANIMATION_STATES[0], false);
                    meCurrentState = E_ONLINE_INSTANT_RESULTS_FINAL_STANDINGSUPDATE;
                    if (meCurrentGameMode != 13)
                    {
                        mRevengeColumnHeading.SetLocalisedText("REVENGE_RESULTS", CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
                        for (s32 liRow = 0; liRow < mOnlinePostEvent.miNumPlayersInEvent; ++liRow)
                        {
                            const GuiEventOnlinePostEvent::Record* lpPlayer = GetOnlinePlayerFromSortedTablePosition(liRow);
                            CGS_ASSERT(lpPlayer, "lpPostEventOnlinePlayer");
                            const OnlinePlayerStatus* lpStatus = FindOnlinePlayerStatus(mpGuiCache, lpPlayer->miIndex);
                            CGS_ASSERT(lpStatus, "lpInGamePlayerStatusData");
                            const s32 liRevenge = lpStatus->mLiveRevengeRelationship.GetCurrentScoreForLocalPlayer();
                            char lacRevenge[8];
                            s32 liIcon;
                            if (liRevenge > 0)
                            {
                                CgsCore::SPrintf(lacRevenge, 8, "%d", liRevenge);
                                liIcon = 0;
                            }
                            else if (liRevenge < 0)
                            {
                                CgsCore::SPrintf(lacRevenge, 8, "%d", -liRevenge);
                                liIcon = 1;
                            }
                            else
                            {
                                CgsCore::SPrintf(lacRevenge, 8, " ");
                                liIcon = 2;
                            }
                            mTable.SetIconState(liRow, 1, liIcon);
                            lacRevenge[7] = 0;
                            mTable.SetText(liRow, 2, lacRevenge);
                        }
                    }
                }
                else
                {
                    mTitleTextfield.SetText("$ONLINE_RESULTS_CURRENT_STANDINGS_TITLE");
                    meCurrentState = E_ONLINE_INSTANT_RESULTS_CURRENT_STANDINGSUPDATE;
                }
            }
            const GuiEventOnlinePostEvent::Record* lpFirstPlayer = 0;
            const OnlinePlayerStatus* lpFirstStatus = 0;
            s32 liPreviousTeam = 0;
            for (s32 liRow = 0; liRow < mOnlinePostEvent.miNumPlayersInEvent; ++liRow)
            {
                GuiEventOnlinePostEvent::Record* lpPlayer = GetOnlinePlayerFromSortedTablePosition(liRow);
                if (!lpPlayer->mbFlag35 && !lpPlayer->mbFlag34 && lpPlayer->mTime.GetFloatVal() == 0.0f)
                    lpPlayer->mbFlag34 = true;
                bool lbHideRepeatedTeam = false;
                bool lbTeamResults = false;
                if (meCurrentGameMode == 12)
                {
                    lbTeamResults = true;
                    const s32 liTeam = mpGuiCache->GetCurrentOnlinePlayerTeam(static_cast<EActiveRaceCarIndex>(lpPlayer->miIndex));
                    if (liTeam == liPreviousTeam)
                        lbHideRepeatedTeam = true;
                    else
                        liPreviousTeam = liTeam;
                }
                SetAptData(liRow, lpPlayer, false, lbHideRepeatedTeam, lbTeamResults);
                CGS_ASSERT(mpGuiCache, "mpGuiCache");
                const OnlinePlayerStatus* lpStatus = FindOnlinePlayerStatus(mpGuiCache, lpPlayer->miIndex);
                CGS_ASSERT(lpStatus, "lpInGamePlayerStatusData");
                if (liRow == 0)
                {
                    lpFirstPlayer = lpPlayer;
                    lpFirstStatus = lpStatus;
                }
                else if (liRow == 1 && lpPlayer->muValue20 == lpFirstPlayer->muValue20)
                    lpFirstPlayer = 0;
                mTable.GetSelectable(liRow)->SetHighlightable(true);
                TableRow* lpRow = mTable.GetSelectable(liRow);
                lpRow->mu64SelectedId = static_cast<u64>(static_cast<s64>(lpStatus->mNetworkPlayerID));
                lpRow->SetDirty();
            }
            if (lpFirstPlayer)
            {
                CGS_ASSERT(lpFirstStatus, "lpFirstPlayerStatusData");
                mWinnerGamertag.SetText(lpFirstStatus->mPlayerName.GetPlayerName());
            }
            else
                mWinnerGamertag.SetText("$ONLINE_RESULTS_TIED");
            if (meCurrentState == E_ONLINE_INSTANT_RESULTS_FINAL_STANDINGSUPDATE && mOnlinePostEvent.miNumPlayersInEvent > 0)
                mTable.HighlightIndex(0);
        }
        if (IsStuntRunResultsMode(meCurrentGameMode))
            UpdateStuntRunColoursAndScores();
    }

    // ARTIST8248A2A0. Target team score/colour presentation, including the early score read.
    void OnlineInstantResultsState::UpdateStuntRunColoursAndScores()
    {
        if (!IsStuntRunResultsMode(meCurrentGameMode))
        {
            CGS_ASSERT(false, "Invalid game mode in call to UpdateStuntRunColoursAndScores");
            return;
        }
        const s32 liScoreCurrent = mpGuiCache->GetCurrentScoreInEvent();
        const s32 liScoreTarget = mpGuiCache->GetTargetScoreInEvent();
        s32 liYourTeamTotal = 0;
        s32 liRivalTeamTotal = 0;
        if (meCurrentState < E_ONLINE_INSTANT_RESULTS_STANDINGSUPDATE)
        {
            if (meCurrentState >= E_ONLINE_INSTANT_RESULTS_UPDATING)
            {
                mPointsColumnHeading.SetText("");
                const GuiEventOnlinePostEvent::Record* lpWinner = GetOnlinePlayerFromTablePosition(0);
                const s32 liTeam = mpGuiCache->GetCurrentOnlinePlayerTeam(static_cast<EActiveRaceCarIndex>(lpWinner->miIndex));
                mWinnerGamertag.SetColour(KAU_TEAM_COLOURS[liTeam]);
                mYourTeamScore.SetLocalisedText(liScoreCurrent, CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
                mYourTeamScoreHeading.SetText("$RESULTS_YOURTEAM_POINTS_HEADING");
                mRivalTeamScore.SetLocalisedText(liScoreTarget, CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
                mRivalTeamScoreHeading.SetText("$RESULTS_RIVALTEAM_POINTS_HEADING");
            }
        }
        else
        {
            mPointsColumnHeading.SetText("$POINTS_RESULTS");
            for (s32 liRow = 0; liRow < mOnlinePostEvent.miNumPlayersInEvent; ++liRow)
            {
                const GuiEventOnlinePostEvent::Record* lpPlayer = GetOnlinePlayerFromSortedTablePosition(liRow);
                CGS_ASSERT(lpPlayer, "lpOnlinePlayer");
                const s32 liTeam = mpGuiCache->GetCurrentOnlinePlayerTeam(static_cast<EActiveRaceCarIndex>(lpPlayer->miIndex));
                const s32 liTotal = static_cast<s32>(lpPlayer->muValue20);
                const s32 liLocalTeam = mpGuiCache->GetCurrentOnlinePlayerTeam(
                    static_cast<EActiveRaceCarIndex>(mpGuiCache->GetPlayerActiveRaceCarIndex()));
                if (liTeam == liLocalTeam)
                {
                    if (liTotal > liYourTeamTotal) liYourTeamTotal = liTotal;
                }
                else if (liTotal > liRivalTeamTotal)
                    liRivalTeamTotal = liTotal;
            }
            mYourTeamScore.SetLocalisedText(liYourTeamTotal, CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
            mYourTeamScoreHeading.SetText("$RESULTS_YOURTEAM_POINTS_HEADING");
            mRivalTeamScoreHeading.SetText("$RESULTS_RIVALTEAM_POINTS_HEADING");
            mRivalTeamScore.SetLocalisedText(liRivalTeamTotal, CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
        }
        const s32 liLocalTeam = mpGuiCache->GetCurrentOnlinePlayerTeam(
            static_cast<EActiveRaceCarIndex>(mpGuiCache->GetPlayerActiveRaceCarIndex()));
        mYourTeamScoreHeading.SetColour(KAU_TEAM_COLOURS[liLocalTeam]);
        mRivalTeamScoreHeading.SetColour(KAU_TEAM_COLOURS[(liLocalTeam == 1) + 1]);
        if (meCurrentState == E_ONLINE_INSTANT_RESULTS_STANDINGSUPDATE ||
            meCurrentState == E_ONLINE_INSTANT_RESULTS_CURRENT_STANDINGSUPDATE)
            mYourTeamIs.SetText(liYourTeamTotal == liRivalTeamTotal ? "$ONLINE_STANDINGS_STUNTRUN_TIED" :
                (liYourTeamTotal > liRivalTeamTotal ? "$ONLINE_STANDINGS_STUNTRUN_WINNING" : "$ONLINE_STANDINGS_STUNTRUN_LOSING"));
        else if (meCurrentState == E_ONLINE_INSTANT_RESULTS_FINAL_STANDINGSUPDATE ||
                 meCurrentState == E_ONLINE_INSTANT_RESULTS_FINAL_STANDINGS)
            mYourTeamIs.SetText(liYourTeamTotal == liRivalTeamTotal ? "$ONLINE_RESULTS_STUNTRUN_TIED" :
                (liYourTeamTotal > liRivalTeamTotal ? "$ONLINE_RESULTS_STUNTRUN_WON" : "$ONLINE_RESULTS_STUNTRUN_LOST"));
        mTitleTextfield.SetText("$ONLINE_RESULTS_RESULTS_TITLE");
        SetResultColumnHeading();
        SetRoundText();
    }
}
