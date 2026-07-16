#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"          // CgsIDUnCompress
#include "GameSource/Gui/BrnGuiCache.h"                 // GuiCache accessors

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Takedown family (wave-B group 1):
//   HudMessageAnalyzer::HandleTakedown              @0x8251C3C0
//   HudMessageAnalyzer::ConstructTakedownMessage    @0x824F9C28
//
// (HandleTraitorousTakedown @0x8251B0E0 is BLOCKED: the frozen header declares it
//  `const`, but its faithful body must call the non-const TriggerMessage(const
//  GuiHudMessage*) overload -- a const method cannot. Reported in funcs_blocked.)

namespace BrnGui
{

// @ 0x8251C3C0
void HudMessageAnalyzer::HandleTakedown(const GuiTakedownEvent* lpTakedown)
{
    // Non-gating tripwires (cpp:2412-2414; all streamed on the X360, folded static).
    CGS_ASSERT(lpTakedown != NULL, "Invalid takedown message");
    CGS_ASSERT(lpTakedown->meTakedownType > BrnGameState::E_TAKEDOWN_NONE, "Takedown type invalid");
    CGS_ASSERT(lpTakedown->meTakedownType < BrnGameState::E_TAKEDOWN_COUNT, "Takedown type invalid");

    const EActiveRaceCarIndex leLocalPlayer =
        static_cast<EActiveRaceCarIndex>(mpGuiCache->GetPlayerActiveRaceCarIndex());

    if (leLocalPlayer == lpTakedown->meAggressorIndex)
    {
        // The local player dealt the takedown. Abort an in-progress wreck message.
        if (mePlayersCurrentlyWreckState > E_WRECKED_STATE_NOT_WRECKED &&
            mePlayersCurrentlyWreckState < E_WRECKED_STATE_WRECK_HANDLED)
        {
            mePlayersCurrentlyWreckState = E_WRECKED_STATE_WRECK_ABORTED;
        }

        if (lpTakedown->mbMarkedManTakeDown)
        {
            // Marked-man scalp collected.
            GuiHudMessage lMessage;
            bool lbNameValid = true;
            lMessage.Construct("DTGotScalp");
            const char* lpcOnlineName = GetOnlineName(lpTakedown->meVictimIndex, &lbNameValid);
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1, lpcOnlineName);
            if (lbNameValid)
                TriggerMessage(&lMessage);
        }
        else if (meCrashEntryState == 2 || mpGuiCache->IsOnlineStartInProgress())
        {
            // Fire immediately.
            GuiHudMessage lMessage;
            ConstructTakedownMessage(&lMessage, lpTakedown);
            TriggerMessage(&lMessage);
            mbTakedownMessagePending = false;
        }
        else
        {
            // A crash bar is up: park the takedown until it drops.
            mbTakedownMessagePending = true;
            mPendingTakedownEvent = *lpTakedown;
        }
    }
    else if (lpTakedown->meVictimIndex == leLocalPlayer)
    {
        // The local player was taken down. Abort an in-progress wreck message.
        if (mePlayersCurrentlyWreckState > E_WRECKED_STATE_NOT_WRECKED &&
            mePlayersCurrentlyWreckState < E_WRECKED_STATE_WRECK_HANDLED)
        {
            mePlayersCurrentlyWreckState = E_WRECKED_STATE_WRECK_ABORTED;
        }

        if (lpTakedown->mbMarkedManTakeDown)
        {
            // Scalped by a marked man.
            GuiHudMessage lMessage;
            bool lbNameValid = true;
            lMessage.Construct("DTScalped");
            const char* lpcOnlineName = GetOnlineName(lpTakedown->meAggressorIndex, &lbNameValid);
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1, lpcOnlineName);
            if (lbNameValid)
                TriggerMessage(&lMessage);
        }
        else
        {
            char lacAggressorName[16];
            CgsIDUnCompress(lpTakedown->mAggressorCarID, lacAggressorName);
            GuiHudMessage lMessage;
            lMessage.Construct("TDBdGeneral");
            TriggerMessage(&lMessage);
        }
    }
    else if (!mpGuiCache->IsOnlineStartInProgress() || mpGuiCache->GetNumActivePlayers() != 0)
    {
        // A third party's takedown seen from a spectating local player.
        const char* lpcMessageId;
        if (lpTakedown->meTakedownType == BrnGameState::E_TAKEDOWN_VERTICAL)
            lpcMessageId = "AggDrFlttns";
        else if (lpTakedown->mbSettledScore)
            lpcMessageId = "AggDrGtRevOn";
        else
            lpcMessageId = "AggDrTksDown";

        GuiHudMessage lMessage;
        lMessage.Construct(lpcMessageId);

        CGS_ASSERT(lpTakedown->meAggressorIndex != lpTakedown->meVictimIndex,
                   "lpTakedown->meAggressorIndex != lpTakedown->meVictimIndex");

        if (mpGuiCache->IsOnlineStartInProgress())
        {
            if (AddGamerTagToMessage(&lMessage, "TEMP_ONLINE_NAME", lpTakedown->meAggressorIndex) &&
                AddGamerTagToMessage(&lMessage, "TEMP_ONLINE_NAME2", lpTakedown->meVictimIndex))
            {
                TriggerMessage(&lMessage);
            }
        }
    }
}

// @ 0x824F9C28
void HudMessageAnalyzer::ConstructTakedownMessage(GuiHudMessage* lpHudMessage,
                                                  const GuiTakedownEvent* lpTakedown)
{
    // Non-gating tripwires (cpp:2576/2577).
    CGS_ASSERT(lpHudMessage != NULL, "lpHudMessage");
    CGS_ASSERT(lpTakedown != NULL, "lpTakedown");

    // Multi-takedowns get their own dedicated message elsewhere.
    if (lpTakedown->miMultipleTakedownCount <= 1)
    {
        const s32 liChainCount = lpTakedown->miTakedownChainCount;
        if (liChainCount < 2 || lpTakedown->meTakedownType != BrnGameState::E_TAKEDOWN_STANDARD)
        {
            lpHudMessage->Construct(KAPC_TAKEDOWN_TYPES[lpTakedown->meTakedownType]);
        }
        else if (liChainCount - 2 < 9)
        {
            // cpp:2631 (streamed on the X360 @0x824F9CE8 -> folded static; a provably-dead
            // guard -- this lane is only reached when liChainCount >= 2).
            CGS_ASSERT(liChainCount - 2 >= 0, "liChainIndex >= 0 && liChainIndex < liChainCount");
            lpHudMessage->Construct(KAPC_TAKEDOWN_CHAIN[liChainCount - 2]);
        }
        else
        {
            lpHudMessage->Construct(KAPC_TAKEDOWN_CHAIN[8]);
        }
    }
}
}   // namespace BrnGui
