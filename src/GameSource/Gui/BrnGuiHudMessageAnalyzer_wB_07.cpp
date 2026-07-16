#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"   // brings BrnGuiEventTypeDefs + PlayerName

#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h" // CgsCore::SPrintf
#include "GameSource/Gui/BrnGuiCache.h"                // GuiCache accessors
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h" // InGamePlayerStatusData (online name)

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (wave-B partfile 07). Road-rules / showtime chatter handlers, driven from Update's
// event drain:
//   HandleNewRoadRulesHighScore @0x824F32A0  (cpp:4646-)
//   HandleRoadRuleFailed        @0x8251F020  (cpp:4855-)
//   HandleShowtimeModeSwitch    @0x8251F240  (cpp:4923-)

namespace BrnGui
{

// @ 0x824F32A0
// Park the whole 48-byte high-score record and (re)start the road-rules message
// delay timer; Update's deferred pass fires the announce once the timer elapses.
void HudMessageAnalyzer::HandleNewRoadRulesHighScore(const GuiEventRoadRuleNewHighScore* lpEvent)
{
    // Non-gating tripwires (cpp:4646/4647; both streamed on the X360, folded static).
    CGS_ASSERT(lpEvent != NULL, "lpEvent");
    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

    mfRoadRulesMessageTimeout = 0.0f;   // flt_82001CC0 == 0.0f
    mbNewRoadRulesHighScores  = true;
    mRoadRuleHighScoreData    = *lpEvent;  // 48-byte record copy (6-qword loop @0x824F332C)
}

// @ 0x8251F020
// Road-rule failure chatter. The road id leads the record and is SPrintf'd into a
// numeric string; the message id and its parameters depend on the failed rule type
// (time vs crash) and, for time rules, who now rules the road.
void HudMessageAnalyzer::HandleRoadRuleFailed(const GuiEventRoadRuleFail* lpEvent)
{
    // Non-gating tripwire (cpp:4855; streamed on the X360, folded static).
    CGS_ASSERT(lpEvent != NULL, "lpEvent");

    char lacRoadNumber[16];
    CgsCore::SPrintf(lacRoadNumber, 12, "%llu", lpEvent->mRoadID);
    lacRoadNumber[12] = 0;

    GuiHudMessage lMessage;

    if (lpEvent->meRuleType == 0)
    {
        // Time rule.
        if (lpEvent->mbRoadRuledByLocalPlayer)
        {
            lMessage.Construct("RRFailTimeU");
            TriggerMessage(&lMessage);
            return;
        }

        if (lpEvent->mbRoadRuledByAI)
        {
            lMessage.Construct("RRFailTimeAI");
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1,
                              lpEvent->macCurrentRulerName);
        }
        else
        {
            lMessage.Construct("RRFailTime");
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1,
                              lpEvent->macCurrentRulerName);
        }
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 2, lacRoadNumber);
        TriggerMessage(&lMessage);
        return;
    }

    if (lpEvent->meRuleType == 1)
    {
        // Showtime / crash rule.
        lMessage.Construct("RRFailCrash");
        TriggerMessage(&lMessage);
        return;
    }

    // Unknown rule type -- assert then fall back to the time-rule presentation
    // (cpp:4887; streamed on the X360, folded static).
    CGS_ASSERT(false, "Unknown road rule type failed:  Showing time by default");

    if (lpEvent->mbRoadRuledByLocalPlayer)
    {
        lMessage.Construct("RRFailTimeU");
        TriggerMessage(&lMessage);
        return;
    }

    CgsGui::HudMessageParamTypes leNameParamType;
    if (lpEvent->mbRoadRuledByAI)
    {
        lMessage.Construct("RRFailTimeAI");
        leNameParamType = CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID;
    }
    else
    {
        lMessage.Construct("RRFailTime");
        leNameParamType = CgsGui::E_HUDMESSAGEPARAMTYPES_STRING;
    }
    lMessage.AddParam(leNameParamType, 1, lpEvent->macCurrentRulerName);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1, lacRoadNumber);
    TriggerMessage(&lMessage);
}

// @ 0x8251F240
// Showtime entry/exit chatter for a REMOTE player (a different ARCI than the local
// player, and only while an online start is in progress); "ShowStart" when the peer
// is entering showtime, "ShowStop" otherwise, tagged with the peer's online name.
void HudMessageAnalyzer::HandleShowtimeModeSwitch(const GuiShowtimeModeSwitch* lpEvent)
{
    // Non-gating tripwires (cpp:4923/4924; both streamed on the X360, folded static).
    CGS_ASSERT(lpEvent != NULL, "lpEvent");
    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

    if (lpEvent->meActiveRaceCarIndex !=
            static_cast<EActiveRaceCarIndex>(mpGuiCache->GetPlayerActiveRaceCarIndex())
        && mpGuiCache->IsOnlineStartInProgress())
    {
        GuiHudMessage lMessage;
        lMessage.Construct(lpEvent->mbEnteringShowtime ? "ShowStart" : "ShowStop");

        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerInfo =
            mpGuiCache->GetOnlinePlayerInfoFromPlayerId(lpEvent->mNetworkPlayerID);
        if (lpPlayerInfo != NULL)
        {
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0,
                              lpPlayerInfo->mPlayerName.macName);
            TriggerMessage(&lMessage);
        }
    }
}

} // namespace BrnGui
