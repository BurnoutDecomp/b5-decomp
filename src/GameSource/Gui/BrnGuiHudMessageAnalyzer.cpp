#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"

#include <cstring>   // std::memcpy (the delayed-message adopt)

#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"                // GuiCache accessors
#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h" // IsRunning/IsShowingResults
#include "GameSource/Gui/BrnGuiHudMessageDirector.h"   // HudMessageDirector::AddMessage

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (3 ledger functions + one unnamed sibling, DWARF primary file
// GameSource/Gui/BrnGuiHudMessageAnalyzer.h / .cpp):
//   HudMessageAnalyzer::TriggerMessage(const char*)       @0x825179E8  (h:~737)
//   HudMessageAnalyzer::TriggerMessage(const GuiHudMessage*) @0x82517AF8 (h:~764;
//     the ledger row is unnamed -- recovered alongside its caller)
//   HudMessageAnalyzer::HandlePlayerEliminated            @0x8251B058
//   HudMessageAnalyzer::HandleLiveRevengeUpdate           @0x8251E1F0  (cpp:3590s)

namespace BrnGui
{

// @ 0x825179E8
void HudMessageAnalyzer::TriggerMessage(const char* lpcMessageId)
{
    // Non-gating tripwires (h:737/738).
    CGS_ASSERT(NULL != lpcMessageId, "NULL != lpcMessageId");
    CGS_ASSERT('\0' != lpcMessageId[0], "'\\0' != lpcMessageId[0]");

    GuiHudMessage lMessage;
    lMessage.Construct(lpcMessageId);

    // h:747 -- the X360 streams "Invalid HUD message director"; folded static.
    CGS_ASSERT(mpHudMessageDirector != NULL, "Invalid HUD message director");
    mpHudMessageDirector->AddMessage(&lMessage, false);
}

// @ 0x82517AF8
void HudMessageAnalyzer::TriggerMessage(const GuiHudMessage* lpMessage)
{
    // Non-gating tripwires (h:764 "Invalid HUD message" / h:~770 director check;
    // both streamed on the X360, folded static).
    CGS_ASSERT(NULL != lpMessage, "Invalid HUD message");
    CGS_ASSERT(mpHudMessageDirector != NULL, "Invalid HUD message director");
    mpHudMessageDirector->AddMessage(lpMessage, false);
}

// @ 0x8251B058
void HudMessageAnalyzer::HandlePlayerEliminated(const s32* lpiEventPayload)
{
    // The road-rage elimination message, tagged with the eliminated player's name
    // (the payload's leading word is the eliminated player's ARCI).
    GuiHudMessage lMessage;
    lMessage.Construct("OnlRRPlyElim");

    bool lbNameValid = true;
    const char* lpcOnlineName =
        GetOnlineName(static_cast<EActiveRaceCarIndex>(*lpiEventPayload), &lbNameValid);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0, lpcOnlineName);

    if (lbNameValid)
        TriggerMessage(&lMessage);
}

// @ 0x8251E1F0
void HudMessageAnalyzer::HandleLiveRevengeUpdate(const GuiLiveRevengeUpdateEvent* lpEvent)
{
    // Non-gating tripwires (cpp:3593-3597; the first streamed, folded static).
    CGS_ASSERT(lpEvent != NULL, "Invalid Live Revenge Update Event");
    CGS_ASSERT(lpEvent->meAggressorActiveRaceCarIndex >= 0,
               "lpEvent->meAggressorActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(lpEvent->meAggressorActiveRaceCarIndex < 8,
               "lpEvent->meAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    CGS_ASSERT(lpEvent->meVictimActiveRaceCarIndex >= 0,
               "lpEvent->meVictimActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(lpEvent->meVictimActiveRaceCarIndex < 8,
               "lpEvent->meVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    // A running freeburn challenge suppresses the revenge chatter (the accessor
    // carries the inlined "mpChallengeManager" assert, BrnGuiCache.h:2390).
    const FreeburnChallengeManager* lpChallengeManager =
        mpGuiCache->GetFreeburnChallengeManager();
    if (lpChallengeManager->IsRunning() || lpChallengeManager->IsShowingResults())
        return;

    const EActiveRaceCarIndex leLocalPlayer =
        static_cast<EActiveRaceCarIndex>(mpGuiCache->GetPlayerActiveRaceCarIndex());

    GuiHudMessage lMessage;
    bool lbNameValid = true;
    EActiveRaceCarIndex leNamedPlayer;

    if (lpEvent->meAggressorActiveRaceCarIndex == leLocalPlayer)
    {
        // The local player dealt the takedown.
        switch (lpEvent->meNewStatus)
        {
        case 0: lMessage.Construct("LRTkdnNoRel"); break;
        case 1: lMessage.Construct("LRTkdnReig");  break;
        case 2: lMessage.Construct("LRTkdnScrSt"); break;
        case 3:
            lMessage.Construct("LRTkdnAhead");
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0, lpEvent->miDifference);
            break;
        default:
            return;
        }
        leNamedPlayer = lpEvent->meVictimActiveRaceCarIndex;
    }
    else if (lpEvent->meVictimActiveRaceCarIndex == leLocalPlayer)
    {
        // The local player was taken down.
        switch (lpEvent->meNewStatus)
        {
        case 0: lMessage.Construct("LRTkndnNoRel"); break;
        case 1: lMessage.Construct("LRTkndnReig");  break;
        case 2: lMessage.Construct("LRTkndnScrSt"); break;
        case 4:
            lMessage.Construct("LRTkndnBehnd");
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0, lpEvent->miDifference);
            break;
        default:
            return;
        }
        leNamedPlayer = lpEvent->meAggressorActiveRaceCarIndex;
    }
    else
    {
        return;
    }

    const char* lpcOnlineName = GetOnlineName(leNamedPlayer, &lbNameValid);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1, lpcOnlineName);

    if (lbNameValid)
    {
        // While a wreck is on screen (crash-bar states 0/2) the message is parked
        // for the after-crash flow instead of firing immediately (the X360 copies
        // the whole 840-byte record).
        if (meCrashEntryState == 0 || meCrashEntryState == 2)
        {
            std::memcpy(&mDelayedForAfterCrashHudMessage, &lMessage, sizeof(GuiHudMessage));
            mbDelayedForAfterCrashHudMessagePending = true;
        }
        else
        {
            TriggerMessage(&lMessage);
        }
    }
}

}
