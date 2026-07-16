#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"

#include <cstring>   // std::memset (clear the parked takedown record)

#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"                // GuiCache::IsOnlineStartInProgress

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX (wave-B).
//
// Bodied here (ledger group 2):
//   HudMessageAnalyzer::HandleCrashedEvent              @0x8251C7C0
//   HudMessageAnalyzer::HandleNetworkPlayerCrashedEvent @0x8251CBF0
//
// (HandleWreckedEvent @0x8251CA88 is NOT bodied here -- its case
// E_WRECKED_STATE_NOT_WRECKED reads a GuiCache field at +0x9FD4 that the frozen
// BrnGuiCache.h leaves unnamed (mPad_9FD4[12]); reported as blocked.)

namespace BrnGui
{

// @ 0x8251C7C0
// Crash-bar state machine: when the crash bar drops the parked takedown / dirty-trick
// / delayed-revenge messages are fired. Param is the new crash-bar state by value.
void HudMessageAnalyzer::HandleCrashedEvent(
        GuiPlayerCrashingStateChangeEvent::CrashBarState leCrashBarState)
{
    // Non-gating range/pointer tripwires (cpp:2719-2721).
    CGS_ASSERT(leCrashBarState >= 0, "lCrashingState >= 0");
    CGS_ASSERT(leCrashBarState < GuiPlayerCrashingStateChangeEvent::E_CRASHBARSTATE_COUNT,
               "lCrashingState < BrnGui::GuiPlayerCrashingStateChangeEvent::E_CRASHBARSTATE_COUNT");
    CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

    GuiHudMessage lMessage;

    switch (leCrashBarState)
    {
    case GuiPlayerCrashingStateChangeEvent::E_CRASHBARSTATE_LEAVE_CRASHED:   // 1
    case GuiPlayerCrashingStateChangeEvent::E_CRASHBARSTATE_LEAVE_TAKEDOWN:  // 3
        // Leaving the crash bar: flush the message that was parked for after the
        // crash cam, then the road-rule time / critical-damage follow-ups.
        if (mbDelayedForAfterCrashHudMessagePending)
        {
            TriggerMessage(&mDelayedForAfterCrashHudMessage);
            mbDelayedForAfterCrashHudMessagePending = false;
        }
        if (mbTimeExtMsgPending)
        {
            mbTimeExtMsgPending = false;
            lMessage.Construct("EventRRTime");
            TriggerMessage(&lMessage);
        }
        if (mbRoadOneMoreCrashToTotalledPending)
        {
            mbRoadOneMoreCrashToTotalledPending = false;
            lMessage.Construct("RRDamCrit");
            TriggerMessage(&lMessage);
        }
        break;

    case GuiPlayerCrashingStateChangeEvent::E_CRASHBARSTATE_START_TAKEDOWN:  // 2
        // Entering a takedown crash bar: announce the shutdown line, then fire the
        // takedown message parked while the bar was up, and clear the parked record.
        if (mShutdownPending != 0)
            TriggerMessage("TDGdShutD");

        if (mbTakedownMessagePending)
        {
            ConstructTakedownMessage(&lMessage, &mPendingTakedownEvent);
            TriggerMessage(&lMessage);
        }

        mbTakedownMessagePending = false;
        std::memset(&mPendingTakedownEvent, 0, sizeof(GuiTakedownEvent));
        break;

    case GuiPlayerCrashingStateChangeEvent::E_CRASHBARSTATE_START_CRASHED:   // 0
        if (meDirtyTrickPending == static_cast<BrnNetwork::EPaybackType>(3))  // X360 'none' sentinel
        {
            // No dirty trick pending: on a wreck-in-progress, announce the wreck.
            if (mePlayersCurrentlyWreckState == E_WRECKED_STATE_WRECKED_STUNT)
            {
                lMessage.Construct("StuntWrecked");
                TriggerMessage(&lMessage);
                mePlayersCurrentlyWreckState = E_WRECKED_STATE_WRECK_HANDLED;
            }
            else if (mePlayersCurrentlyWreckState == E_WRECKED_STATE_WRECKED)
            {
                lMessage.Construct("GameWrecked");
                TriggerMessage(&lMessage);
                mePlayersCurrentlyWreckState = E_WRECKED_STATE_WRECK_HANDLED;
            }
        }
        else
        {
            // A dirty trick was parked: fire its "bad" payback line tagged with the
            // stored victim name, then reset the pending trick to the 'none' sentinel.
            CGS_ASSERT(meDirtyTrickPending >= BrnNetwork::E_PAYBACK_TYPE_START,
                       "meDirtyTrickPending >= BrnNetwork::E_PAYBACK_TYPE_START");
            CGS_ASSERT(meDirtyTrickPending < 3,
                       "meDirtyTrickPending < BrnNetwork::E_PAYBACK_TYPE_COUNT");

            lMessage.Construct(KAPC_PAYBACK_TAKEDOWN_BD_MESSAGES[meDirtyTrickPending]);

            CGS_ASSERT(macDTPendingVictim != NULL, "macDTPendingVictim");
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1, macDTPendingVictim);
            TriggerMessage(&lMessage);

            meDirtyTrickPending = static_cast<BrnNetwork::EPaybackType>(3);  // X360 'none' sentinel
        }
        break;

    default:
        break;
    }
}

// @ 0x8251CBF0
// "A rival is crashing" chatter -- only while an online session is starting, tagged
// with the crashing player's gamer tag.
void HudMessageAnalyzer::HandleNetworkPlayerCrashedEvent(
        const GuiNetworkPlayerCrashingEvent* lpEvent)
{
    // Non-gating pointer/range tripwires (cpp:2920-2922).
    CGS_ASSERT(lpEvent != NULL, "lpNetworkPlayerCrashedEvent");
    CGS_ASSERT(lpEvent->meNetworkPlayerActiveRaceCarIndex >= 0,
               "lpNetworkPlayerCrashedEvent->meNetworkPlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(lpEvent->meNetworkPlayerActiveRaceCarIndex < 8,
               "lpNetworkPlayerCrashedEvent->meNetworkPlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    GuiHudMessage lMessage;
    lMessage.Construct("AggDrCrashes");

    if (mpGuiCache->IsOnlineStartInProgress())
    {
        if (AddGamerTagToMessage(&lMessage, "TEMP_ONLINE_NAME",
                                 lpEvent->meNetworkPlayerActiveRaceCarIndex))
        {
            TriggerMessage(&lMessage);
        }
    }
}

}
