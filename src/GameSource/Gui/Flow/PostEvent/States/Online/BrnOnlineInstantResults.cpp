// BrnGui::OnlineInstantResultsState -- the online "instant results" post-event state. The
// resource accessor is the header's inline; this TU holds the class's static resource list and
// award ticker table (both read from the image) and FillOutTicker.

#include "GameSource/Gui/Flow/PostEvent/States/Online/BrnOnlineInstantResults.h"

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
}
