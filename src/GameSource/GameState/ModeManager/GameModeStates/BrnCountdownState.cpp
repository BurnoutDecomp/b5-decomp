#include "GameSource/GameState/ModeManager/GameModeStates/BrnCountdownState.h"

#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"
#include "GameSource/GameState/ModeManager/BrnModeManager.h"

#include <cmath>

namespace BrnGameState
{
// X360 0x82332D70. Seed the countdown for the mode we are entering: ask the ModeManager
// for the per-mode countdown duration and start the timer there, and reset the displayed
// whole-second value so the first Update tick always pushes a fresh value to the HUD.
void CountdownState::OnEnter()
{
    mfCountdownSeconds = mpGameMode->GetModeManager()->GetCountdownTimeForMode();
    miCountdownDisplay = -1;
}

// X360 0x82316518. Tick the countdown timer down by this frame's time step. When it
// reaches zero and the mode agrees the countdown should end, advance the state machine
// (E_GME_NEXT) and show "0"; otherwise push the ceil'd whole-second value to the GameMode
// for the HUD, but only when it actually changed since the last tick.
void CountdownState::Update()
{
    mfCountdownSeconds -= mpGameMode->GetUpdateTimeStep();

    s32 liCountdownDisplay;
    if (mfCountdownSeconds <= 0.0f && mpGameMode->ShouldCountdownEnd())
    {
        mpGameMode->SendEvent(E_GME_NEXT);
        liCountdownDisplay = 0;
    }
    else
    {
        liCountdownDisplay = static_cast<s32>(ceil(mfCountdownSeconds));
        if (liCountdownDisplay == miCountdownDisplay)
        {
            return;
        }
        miCountdownDisplay = liCountdownDisplay;
    }

    // Hand the new whole-second value to the mode for the HUD feed. SetCountdownDisplay
    // flags the value as changed iff it differs from the mode's previous value, which the
    // GUI then polls via GameMode::HasCountdownDisplayChanged.
    mpGameMode->SetCountdownDisplay(liCountdownDisplay);
}

// X360 0x823165F0. Raise the latch consumed by ModeManager::UpdateCurrentMode.
void CountdownState::OnLeave()
{
    mpGameMode->SetCountdownJustFinished(true);
}
}
