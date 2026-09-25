// ---------------------------------------------------------------------------
// GameSource/GameState/RoadRules/BrnRoadRulesManager_wR_03.cpp
//
// RoadRulesManager, the event half: the road-limit crossing that starts and scores the time
// rule (OnRoadLimit), showtime starting on a road (OnShowtimeStart), the offline/online mode
// switch (SetRoadRulesMode), the profile-load quit (QuitAnyActiveRules), the GUI's rule pick
// (SetActiveRoadRule) and the GUI's request for a road's rule data (OnRoadRulesDataRequest).
//
// Every body is read off the console code of the function it defines. OnStartRule, OnEndRule
// and SendActiveRuleState are defined in their own partfile; OnEnterRoad and OnLeaveRoad in
// BrnRoadRulesManager.cpp.
// ---------------------------------------------------------------------------
#include "GameSource/GameState/RoadRules/BrnRoadRulesManager.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"                   // OutputBuffer::GetGameActionQueue
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"   // StreetManager::GetStreetData
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"     // TrainingManager::RequestTraining
#include "SharedClasses/StreetData/BrnStreetData.h"                      // StreetData / Road
#include "SharedClasses/Progression/BrnTrainingTypes.h"                  // BrnProgression::ETrainingType
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT, CgsDev::Assert
#include "GameShared/GameClasses/Development/CgsStrStream.h"             // CgsDev::StrStream (OnRoadLimit's message)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"               // the opt-in witness below
#include <cstdlib>                                                       // getenv

namespace BrnGameState
{

// The player crossed a road-limit region. A road carries two limit ids, one at each end: find
// the road that owns lRoadLimitId and remember the id at its other end. Crossing back out over
// the limit the time rule started from ends the attempt with scoring allowed; any other crossing
// ends it without a score. Crossing INTO a road starts a new time attempt (when a time rule is
// selected and the car is not crashing) and makes that road the current one.
void RoadRulesManager::OnRoadLimit(CgsID lRoadLimitId, bool lbEntryDirection,
                                   GameStateModuleIO::OutputBuffer* lpOutputBuffer, bool lbIsPlayerCarCrashing)
{
    CgsID lOtherRoadLimitId = 0;

    CGS_ASSERT(lpOutputBuffer, "lpOutputBuffer");
    GameStateModuleIO::GameActionQueue* lpGameActionQueue = lpOutputBuffer->GetGameActionQueue();

    // [FLAG PC witness] NOT IN THE CONSOLE. Opt-in (BRN_ROADRULES_DIAG), first 32 calls.
    {
        static const bool sbDiag = (std::getenv("BRN_ROADRULES_DIAG") != 0);
        static s32        siLeft = 32;
        if (sbDiag && siLeft > 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            --siLeft;
            *CgsDev::Log::gpDebugPrint
                << "[roadrules] limit " << lRoadLimitId << " entry " << static_cast<s32>(lbEntryDirection)
                << " crashing " << static_cast<s32>(lbIsPlayerCarCrashing) << " lastLimit " << mLastLimitId
                << " rule " << static_cast<s32>(meActiveRoadRule) << "\n";
        }
    }

    // The road count is re-read through the street-data resource on every test, as the console does.
    BrnStreetData::RoadIndex liRoadIndex;
    for (liRoadIndex = 0; liRoadIndex < mpStreetManager->GetStreetData()->GetRoadCount(); ++liRoadIndex)
    {
        const BrnStreetData::Road* lpRoad = mpStreetManager->GetStreetData()->GetRoad(liRoadIndex);

        if (lRoadLimitId == lpRoad->GetRoadLimitId0())
        {
            lOtherRoadLimitId = lpRoad->GetRoadLimitId1();
            break;
        }
        if (lRoadLimitId == lpRoad->GetRoadLimitId1())
        {
            lOtherRoadLimitId = lpRoad->GetRoadLimitId0();
            break;
        }
    }

    if (liRoadIndex == mpStreetManager->GetStreetData()->GetRoadCount())
    {
        return;   // no road owns this limit id
    }

    bool lbAllowScoring = false;
    if (lOtherRoadLimitId == mLastLimitId && IsTimeRuleActive())
    {
        // An attempt that finishes within 2.5 s of starting should be impossible. Written as
        // !(a > b) so a NaN clock fires the assert too, matching the console's unordered compare.
        if (!(mfTime > 2.5f))
        {
            CgsDev::Assert::BeginAssert();

            // The console streams into the shared assert message buffer; the tree's idiom is a
            // stack buffer of the same size. The longest message this can build is 232 bytes.
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "How the hell did this happen? Find Iain or Marti\xF1" "o straightaway and tell them "
                          "exactly what you did. road index "
                       << miLastRoadIndex
                       << " Challenge index = " << maiChallengeRoadIndex[BrnStreetData::E_SCORE_TYPE_TIME]
                       << " Road limit ID = " << lRoadLimitId
                       << " Other road limit ID = " << lOtherRoadLimitId
                       << "\n";

            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }
        lbAllowScoring = true;
    }

    OnEndRule(lpOutputBuffer, BrnStreetData::E_SCORE_TYPE_TIME, lbAllowScoring);

    if (lbEntryDirection)
    {
        if (IsRoadRuleTime(meActiveRoadRule) && !lbIsPlayerCarCrashing)
        {
            OnStartRule(lpGameActionQueue, liRoadIndex, BrnStreetData::E_SCORE_TYPE_TIME, false);
            mLastLimitId = lRoadLimitId;
        }

        if (liRoadIndex != miLastRoadIndex && !IsCrashRuleActive())
        {
            OnLeaveRoad(lpGameActionQueue, miLastRoadIndex);
            OnEnterRoad(lpGameActionQueue, liRoadIndex);
            miLastRoadIndex = liRoadIndex;
        }
    }
    else
    {
        mLastLimitId = 0;
    }
}

// Showtime began on the current road: any time attempt ends unscored and a crash attempt starts
// on the road the player is on.
void RoadRulesManager::OnShowtimeStart(GameStateModuleIO::OutputBuffer* lpOutputBuffer)
{
    CGS_ASSERT(lpOutputBuffer, "lpOutputBuffer");
    GameStateModuleIO::GameActionQueue* lpGameActionQueue = lpOutputBuffer->GetGameActionQueue();

    CGS_ASSERT(!IsValidRoad(maiChallengeRoadIndex[BrnStreetData::E_SCORE_TYPE_CRASH]),
               "!IsValidRoad( maiChallengeRoadIndex[BrnStreetData::E_SCORE_TYPE_CRASH] )");

    if (IsTimeRuleActive())
    {
        OnEndRule(lpOutputBuffer, BrnStreetData::E_SCORE_TYPE_TIME, false);
    }

    OnStartRule(lpGameActionQueue, miLastRoadIndex, BrnStreetData::E_SCORE_TYPE_CRASH, true);
    mpTrainingManager->RequestTraining(BrnProgression::E_TRAINING_TYPE_CRASH_ROAD_RULES_ON);
}

// Switch between the offline and online flavour of the selected rule. A running time attempt
// ends unscored when the time rule flips; the crash rule flips without ending anything. The
// active-rule state is re-sent whether or not the mode changed.
void RoadRulesManager::SetRoadRulesMode(GameStateModuleIO::OutputBuffer* lpOutputBuffer, bool lbMode)
{
    const bool lbHasModeChanged = (mbIsOnlineMode != lbMode);
    mbIsOnlineMode = lbMode;

    if (lbHasModeChanged)
    {
        switch (meActiveRoadRule)
        {
        case E_ACTIVE_ROAD_RULE_NONE:
        case E_ACTIVE_ROAD_RULE_COUNT:
            break;

        case E_ACTIVE_ROAD_RULE_OFFLINE_TIME:
            OnEndRule(lpOutputBuffer, BrnStreetData::E_SCORE_TYPE_TIME, false);
            mbAllowExitRoadRulesAfterTimeout = false;
            meActiveRoadRule = E_ACTIVE_ROAD_RULE_ONLINE_TIME;
            break;

        case E_ACTIVE_ROAD_RULE_ONLINE_TIME:
            OnEndRule(lpOutputBuffer, BrnStreetData::E_SCORE_TYPE_TIME, false);
            meActiveRoadRule = E_ACTIVE_ROAD_RULE_OFFLINE_TIME;
            mbAllowExitRoadRulesAfterTimeout = false;
            break;

        case E_ACTIVE_ROAD_RULE_OFFLINE_CRASH:
            meActiveRoadRule = E_ACTIVE_ROAD_RULE_ONLINE_CRASH;
            break;

        case E_ACTIVE_ROAD_RULE_ONLINE_CRASH:
            meActiveRoadRule = E_ACTIVE_ROAD_RULE_OFFLINE_CRASH;
            break;

        default:
            CGS_ASSERT(false, "How did it end up here?");
            break;
        }
    }

    SendActiveRuleState(lpOutputBuffer->GetGameActionQueue());
}

// A profile was loaded: end both attempts, neither scored.
void RoadRulesManager::QuitAnyActiveRules(GameStateModuleIO::OutputBuffer* lpOutputBuffer)
{
    if (IsTimeRuleActive())
    {
        OnEndRule(lpOutputBuffer, BrnStreetData::E_SCORE_TYPE_TIME, false);
    }
    if (IsCrashRuleActive())
    {
        OnEndRule(lpOutputBuffer, BrnStreetData::E_SCORE_TYPE_CRASH, false);
    }
}

void RoadRulesManager::SetActiveRoadRule(GameStateModuleIO::GameActionQueue* lpGameActionQueue,
                                         EActiveRoadRule leActiveRoadRule)
{
    meActiveRoadRule = leActiveRoadRule;
    mbAllowExitRoadRulesAfterTimeout = false;
    SendActiveRuleState(lpGameActionQueue);
}

// The GUI asked for a road's rule data: re-post the enter-road action for it. An id of 0 means
// the current road; an id no road carries posts nothing.
void RoadRulesManager::OnRoadRulesDataRequest(CgsID lRoadId, GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    if (lRoadId == 0)
    {
        OnEnterRoad(lpGameActionQueue, miLastRoadIndex);
        return;
    }

    for (BrnStreetData::RoadIndex liRoadIndex = 0;
         liRoadIndex < mpStreetManager->GetStreetData()->GetRoadCount();
         ++liRoadIndex)
    {
        const BrnStreetData::Road* lpRoad = mpStreetManager->GetStreetData()->GetRoad(liRoadIndex);
        if (lpRoad->GetId() == lRoadId)
        {
            OnEnterRoad(lpGameActionQueue, liRoadIndex);
            return;
        }
    }
}

} // namespace BrnGameState
