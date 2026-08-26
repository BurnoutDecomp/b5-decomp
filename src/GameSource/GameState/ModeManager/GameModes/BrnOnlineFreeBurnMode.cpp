#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineFreeBurnMode.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnGameState
{
// EGameModeState (GameStateModuleIO) state-machine ids (named file-local, mirroring BrnGameMode.cpp).
enum
{
    KI_GMS_COUNTDOWN      = 0,
    KI_GMS_INTRO          = 1,
    KI_GMS_IN_PROGRESS    = 2,
    KI_GMS_QUIT           = 5,
    KI_GMS_ONLINE_LOADING = 6,
    KI_GMS_ONLINE_SPLASH  = 7
};

// X360: BrnGameState::OnlineFreeBurnMode::GetName.
const char* OnlineFreeBurnMode::GetName() const
{
    return "OnlineFreeBurn";
}

// X360: BrnGameState::OnlineFreeBurnMode::Start (0x823160C8). On this X360 build the override is
// an unconditional assert: starting online FreeBurn through this mode object is a programmer error
// (the mode is brought up another way). All three parameters are ignored. (The PS3 DecFIGS body
// shows a full set-up; the X360 ledger is authoritative and is solely this assert stub.)
void OnlineFreeBurnMode::Start(const StartGameModeParams* /*lpStartGameModeParams*/,
                               GameModeParams* /*lpGameModeParams*/,
                               ScoringSystem* /*lpScoringSystem*/)
{
    CGS_ASSERT(false, "Trying to start online freeburn mode when it doesn't exist!\n");
}

// X360: BrnGameState::OnlineFreeBurnMode::SendEvent (0x82331460). The online-freeburn state
// machine. ABORT -> Quit; RESTART -> OnlineLoading; the per-state flow is the freeburn-specific
// OnlineLoading->Intro->OnlineSplash->Countdown->InProgress->Quit. SetCurrentState returns void.
// (The default arm's assert message text is a copy/paste artifact baked into the freeburn TU.)
void OnlineFreeBurnMode::SendEvent(EGameModeEvent leEvent)
{
    if (leEvent == E_GME_ABORT)
    {
        SetCurrentState(KI_GMS_QUIT);
        return;
    }
    if (leEvent == E_GME_RESTART)
    {
        SetCurrentState(KI_GMS_ONLINE_LOADING);
        return;
    }

    switch (meCurrentState)
    {
        case KI_GMS_COUNTDOWN:
            if (leEvent == E_GME_NEXT)
            {
                SetCurrentState(KI_GMS_IN_PROGRESS);
            }
            break;
        case KI_GMS_INTRO:
            if (leEvent == E_GME_NEXT)
            {
                SetCurrentState(KI_GMS_ONLINE_SPLASH);
            }
            break;
        case KI_GMS_IN_PROGRESS:
            if (leEvent == E_GME_NEXT)
            {
                SetCurrentState(KI_GMS_QUIT);
            }
            break;
        case KI_GMS_QUIT:
            break;
        case KI_GMS_ONLINE_LOADING:
            if (leEvent == E_GME_NEXT)
            {
                SetCurrentState(KI_GMS_INTRO);
            }
            break;
        case KI_GMS_ONLINE_SPLASH:
            if (leEvent == E_GME_NEXT)
            {
                SetCurrentState(KI_GMS_COUNTDOWN);
            }
            break;
        default:
            CGS_ASSERT(false, "Should not be in this state in Showtime mode!");
            break;
    }
}

// X360 vtable slot 23 (vtbl+92), folded leaf 0x827E2F38 == `li r3,0; blr` at slot 23 of vtable
// 0x820D09E8; the GameMode base is 0x82C296C8 == `li r3,1`. SetupGameMode @0x8234B158 gates the
// WaitForStreaming path on this.
bool OnlineFreeBurnMode::RequiresStreaming() const
{
    return false;
}
}
