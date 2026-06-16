#include "GameSource/GameState/ModeManager/GameModes/BrnStuntAttackMode.h"

namespace BrnGameState
{
const f32 StuntAttackMode::KF_INTRO_TIME_SECONDS = 6.0f;

// X360: 0x827E2528.
const char* StuntAttackMode::GetName() const
{
    return "Stunt Race";
}

// X360: 0x827E2538. Returns the inlined KF_INTRO_TIME_SECONDS (6.0). DWARF shape is f32.
f32 StuntAttackMode::GetIntroDurationSeconds() const
{
    return KF_INTRO_TIME_SECONDS;
}

// X360: 0x827E2548. Stunt Attack has no outro hold, so the timeout is zero.
f32 StuntAttackMode::GetOutroTimeout() const
{
    return 0.0f;
}

// X360: 0x827E2558. The countdown-end hook CountdownState polls: hold the player on the line
// until they face the required start direction. Hex-Rays renders this as a raw read of the bool
// member at +212 (mbPlayerPointingInStartDirection), restored here to the named member.
bool StuntAttackMode::ShouldCountdownEnd() const
{
    return mbPlayerPointingInStartDirection;
}
}
