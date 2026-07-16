#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"   // brings BrnGuiEventTypeDefs + PlayerName
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"          // GuiOnlineTeamChangeEvent
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"    // frozen demangled placeholders
#include "GameSource/Gui/BrnGuiCache.h"                  // GetGameMode / GetCurrentOnlinePlayerTeam / GetPlayerActiveRaceCarIndex

#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT

// BrnGui::HudMessageAnalyzer -- Burning Home Run team-change chatter and the
// online event-finisher ("winner") message, reconstructed from
// BURNOUT_X360_ARTIST.XEX.  Group hint 10.

namespace BrnGui
{

// @ 0x82525E10 -- Update-dispatched entry for the online team-change event (id 445).
// Only forwards to the Burning Home Run handler when the current mode is
// GsmIO::E_MODE_ONLINE_BURNING_HOME_RUN (== 13; cache meGameModeType @0x9E58).
void HudMessageAnalyzer::HandleOnlineTeamChangeMessage(const GuiOnlineTeamChangeEvent* lpEvent)
{
    CGS_ASSERT(lpEvent != NULL, "lpEvent");
    CGS_ASSERT(lpEvent->meActiveRaceCarIndex >= 0,
               "lpEvent->meActiveRaceCarIndex >= 0");
    CGS_ASSERT(lpEvent->meActiveRaceCarIndex < 8,
               "lpEvent->meActiveRaceCarIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");
    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

    if (mpGuiCache->GetGameMode() == 13)   // GsmIO::E_MODE_ONLINE_BURNING_HOME_RUN
        HandleBurningHomeRunTeamChange(lpEvent->meActiveRaceCarIndex);
}

// @ 0x8251C110 -- fires the Burning Home Run "runner" / "traitor runner" messages
// when a player's team changes.
void HudMessageAnalyzer::HandleBurningHomeRunTeamChange(EActiveRaceCarIndex leActiveRaceCarIndex)
{
    CGS_ASSERT(leActiveRaceCarIndex >= 0, "leActiveRaceCarIndex >= 0");
    CGS_ASSERT(leActiveRaceCarIndex < 8,
               "leActiveRaceCarIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");
    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

    const s32 leOnlinePlayerTeam = mpGuiCache->GetCurrentOnlinePlayerTeam(leActiveRaceCarIndex);
    CGS_ASSERT(leOnlinePlayerTeam != 0, "GsmIO::E_PLAYER_TEAM_NONE != leOnlinePlayerTeam");

    // NOTE: on the non-runner paths the X360 fires lMessage WITHOUT Construct-ing it
    // first -- LABEL_15 in the ARTIST asm (@0x8251C25C) is reachable with this stack
    // message left uninitialised.  Reproduced faithfully below (same TriggerMessage
    // call tail, no sanitising early-out).
    GuiHudMessage lMessage;

    if (leActiveRaceCarIndex == mpGuiCache->GetPlayerActiveRaceCarIndex())
    {
        // The local player's team changed.
        if (leOnlinePlayerTeam == 2)
            lMessage.Construct("OnlBHRRunner");
        TriggerMessage(&lMessage);
        return;
    }

    // A remote player's team changed.
    if (leOnlinePlayerTeam != 2)
    {
        TriggerMessage(&lMessage);   // LABEL_15 (unconstructed message on the X360)
        return;
    }

    lMessage.Construct("OnlBHRTrRnnr");

    bool lbNameValid = true;
    const char* lpcOnlineName = GetOnlineName(leActiveRaceCarIndex, &lbNameValid);
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0, lpcOnlineName);

    if (lbNameValid)
        TriggerMessage(&lMessage);
}

// @ 0x8251E790 -- deferred pass from Update: announces the online event winner.
void HudMessageAnalyzer::TriggerEventFinisher()
{
    CGS_ASSERT(mbOnlineWinnerWaiting, "mbOnlineWinnerWaiting");
    mbOnlineWinnerWaiting = false;

    GuiHudMessage lMessage;
    lMessage.Construct("EventRvlWins");
    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1, mOnlineWinnerName.macName);
    TriggerMessage(&lMessage);
}

} // namespace BrnGui
