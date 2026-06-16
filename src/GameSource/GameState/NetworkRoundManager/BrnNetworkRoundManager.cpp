#include "GameSource/GameState/NetworkRoundManager/BrnNetworkRoundManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnGameState
{

// X360 0x82358928. Cache the incoming start-network-game event and latch its round counters.
// The X360 does memcpy(this, lpEvent, 256) -> a member-wise copy of the whole 256-byte event into
// the embedded mStartNetworkGameEvent (it sits at offset 0). miNumRounds lives at event offset 8
// (*(a2+8)); it seeds both the remaining- and total-round counters. mbIsStartingGameAfterPlayerJoin
// lives at event offset 248 (*(a2+248)) and is mirrored into the manager flag. Asserts use the
// build-baked file/line via raw Begin/Fire/End (CGS_ASSERT would inject __FILE__/__LINE__).
void NetworkRoundManager::NetworkGameStarted(const GameStateModuleIO::StartNetworkGameEvent* lpStartNetworkGameEvent)
{
    CGS_ASSERT(lpStartNetworkGameEvent, "lpStartNetworkGameEvent");

    mStartNetworkGameEvent = *lpStartNetworkGameEvent;

    miRoundsRemaining = lpStartNetworkGameEvent->miNumRounds;
    miTotalRounds     = lpStartNetworkGameEvent->miNumRounds;
    mbStartingGameDueToPlayerJoin = lpStartNetworkGameEvent->mbIsStartingGameAfterPlayerJoin;

    CGS_ASSERT(miRoundsRemaining > 0, "miRoundsRemaining > 0");
    CGS_ASSERT(miTotalRounds > 0, "miTotalRounds > 0");
}

// X360 0x823589E8. Cache the incoming start-network-round event. The X360 copies 10 dwords (40
// bytes) from the event into this+64 dwords (byte offset 256) -- i.e. a member-wise copy of the
// whole StartNetworkRoundEvent into the embedded mStartNetworkRoundEvent. The null-pointer assert
// uses the build-baked file/line. Note the X360 dereferences/copies the event regardless of the
// assert outcome (the assert is non-fatal in retail), so the copy runs unconditionally, matching
// the original.
void NetworkRoundManager::NetworkRoundStarted(const GameStateModuleIO::StartNetworkRoundEvent* lpStartNetworkRoundEvent)
{
    CGS_ASSERT(lpStartNetworkRoundEvent, "lpStartNetworkRoundEvent");

    mStartNetworkRoundEvent = *lpStartNetworkRoundEvent;
}

// X360 0x82358A68. Consume one round at round start: assert at least one round remains, then
// decrement the remaining-round counter. result[74] is dword index 74 == byte offset 296 ==
// miRoundsRemaining. Assert uses the build-baked file/line.
void NetworkRoundManager::OnRoundStart()
{
    CGS_ASSERT(miRoundsRemaining > 0, "miRoundsRemaining > 0");

    --miRoundsRemaining;
}

}
