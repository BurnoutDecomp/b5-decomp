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
    if (!lpStartNetworkGameEvent)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpStartNetworkGameEvent",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/NetworkRoundManager/BrnNetworkRoundManager.cpp",
            112);
        CgsDev::Assert::EndAssert();
    }

    mStartNetworkGameEvent = *lpStartNetworkGameEvent;

    miRoundsRemaining = lpStartNetworkGameEvent->miNumRounds;
    miTotalRounds     = lpStartNetworkGameEvent->miNumRounds;
    mbStartingGameDueToPlayerJoin = lpStartNetworkGameEvent->mbIsStartingGameAfterPlayerJoin;

    if (miRoundsRemaining <= 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "miRoundsRemaining > 0",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/NetworkRoundManager/BrnNetworkRoundManager.cpp",
            118);
        CgsDev::Assert::EndAssert();
    }
    if (miTotalRounds <= 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "miTotalRounds > 0",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/NetworkRoundManager/BrnNetworkRoundManager.cpp",
            119);
        CgsDev::Assert::EndAssert();
    }
}

// X360 0x823589E8. Cache the incoming start-network-round event. The X360 copies 10 dwords (40
// bytes) from the event into this+64 dwords (byte offset 256) -- i.e. a member-wise copy of the
// whole StartNetworkRoundEvent into the embedded mStartNetworkRoundEvent. The null-pointer assert
// uses the build-baked file/line. Note the X360 dereferences/copies the event regardless of the
// assert outcome (the assert is non-fatal in retail), so the copy runs unconditionally, matching
// the original.
void NetworkRoundManager::NetworkRoundStarted(const GameStateModuleIO::StartNetworkRoundEvent* lpStartNetworkRoundEvent)
{
    if (!lpStartNetworkRoundEvent)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpStartNetworkRoundEvent",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/NetworkRoundManager/BrnNetworkRoundManager.cpp",
            134);
        CgsDev::Assert::EndAssert();
    }

    mStartNetworkRoundEvent = *lpStartNetworkRoundEvent;
}

// X360 0x82358A68. Consume one round at round start: assert at least one round remains, then
// decrement the remaining-round counter. result[74] is dword index 74 == byte offset 296 ==
// miRoundsRemaining. Assert uses the build-baked file/line.
void NetworkRoundManager::OnRoundStart()
{
    if (miRoundsRemaining <= 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "miRoundsRemaining > 0",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/NetworkRoundManager/BrnNetworkRoundManager.cpp",
            149);
        CgsDev::Assert::EndAssert();
    }

    --miRoundsRemaining;
}

}
