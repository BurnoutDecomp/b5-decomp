#include "GameSource/GameState/ModeManager/GameModeStates/BrnIntroState.h"

#include "GameSource/GameState/ModeManager/BrnModeManager.h"
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"

namespace BrnGameState
{
using GameStateModuleIO::EGameModeType;

namespace
{
// When an online free-burn-lobby / showtime mode is flagged to start without a timed
// intro, the X360 build collapses the countdown to this near-zero value (flt_82023BA4)
// rather than literally 0, so the single Update tick that follows still drives the state
// machine forward through the normal countdown-elapsed path.
const f32 KF_INSTANT_INTRO_SECONDS = 1.9999999e-05f;
}

// X360: BrnGameState::IntroState::OnEnter (0x823163C8). Sets up the intro countdown when
// the mode enters its intro state. The countdown length comes from the owning GameMode;
// online lobby/showtime modes flagged for an instant intro collapse it to ~0. The intro is
// only actually timed (mbUseCountdown) for online modes and offline showtime -- other
// offline modes leave it untimed and advance by other means. Finally the owning mode's
// "finished" flag is cleared, since we are (re)starting the event.
void IntroState::OnEnter()
{
    mfCountdownSeconds = mpGameMode->GetIntroDurationSeconds();

    if (mpModeManager->IsOnlineModeWithInstantIntro())
    {
        mfCountdownSeconds = KF_INSTANT_INTRO_SECONDS;
    }

    const EGameModeType leModeType        = mpModeManager->GetCurrentGameModeType();
    const GameMode*     lpCurrentGameMode = mpModeManager->GetCurrentGameMode();
    const bool          lbCurrentIsOnline = lpCurrentGameMode != nullptr
                                            && lpCurrentGameMode->IsOnline();

    mbUseCountdown = lbCurrentIsOnline
                     || (leModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME);

    mpGameMode->SetFinished(false);
}

// X360: BrnGameState::IntroState::Update (0x823164A0). While the intro is timed, decrement
// the countdown by the frame's time step; when it elapses, advance the mode's state machine
// to the next state -- unless the mode is still waiting on its data to stream in, in which
// case we hold in the intro until loading completes.
void IntroState::Update()
{
    if (!mbUseCountdown)
    {
        return;
    }

    mfCountdownSeconds -= mpGameMode->GetUpdateTimeStep();

    if (mfCountdownSeconds <= 0.0f && !mpModeManager->IsWaitingForModeDataToLoad())
    {
        mpGameMode->SendEvent(E_GME_NEXT);
    }
}

// X360: BrnGameState::IntroState::OnLeave (0x82316508). Leaving the intro means the intro
// has just finished; flag the owning GameMode so the ModeManager stops the mode-intro
// presentation on its next update.
void IntroState::OnLeave()
{
    mpGameMode->SetIntroJustFinished(true);
}
}
