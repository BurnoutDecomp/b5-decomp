#include "GameSource/GameState/ModeManager/GameModeStates/BrnOutroState.h"

#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"

namespace BrnGameState
{
// Default outro length (seconds). Matches the literal returned by the base
// GameMode::GetOutroTimeout() (0.1). DWARF declares this extern const at BrnOutroState.h
// file scope; it is defined here.
const f32 KF_OUTRO_TIME_SECONDS = 0.1f;

// Entering the outro: latch the countdown duration from the owning mode. Each concrete
// mode reports its own outro length via the GetOutroTimeout() virtual (the base returns
// KF_OUTRO_TIME_SECONDS). X360: OnEnter @0x82316610 --
//   *(this + 12) = (*(*mpGameMode->vtable + 64))(mpGameMode);
// i.e. mfOutroSeconds = mpGameMode->GetOutroTimeout() (vtable slot 16).
void OutroState::OnEnter()
{
    mfOutroSeconds = mpGameMode->GetOutroTimeout();
}

// Ticking the outro: count the latched timer down by the current frame time-step and, the
// moment it expires, advance the mode's state machine with the NEXT event. X360: Update
// @0x82316650 --
//   *(this + 12) -= *(mpGameMode + 164);            // mfOutroSeconds -= time-step
//   if (*(this + 12) <= 0.0) (*(*mpGameMode->vtable + 48))(mpGameMode, 1);
// The +164 field read is the inlined GetUpdateTimeStep() accessor (the standalone getter
// was inlined away, so it is not in the X360 ledger); the slot-48 call with arg 1 is the
// SendEvent(E_GME_NEXT) virtual. This mirrors the sibling IntroState::Update exactly,
// minus Intro's extra "intro skippable" guards.
void OutroState::Update()
{
    mfOutroSeconds -= mpGameMode->GetUpdateTimeStep();

    if (mfOutroSeconds <= 0.0f)
    {
        mpGameMode->SendEvent(E_GME_NEXT);
    }
}
}
