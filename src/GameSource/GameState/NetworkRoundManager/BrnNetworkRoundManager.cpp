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

// ===================================================================================================
// [stuntrace waveB fix round, 2026-08-26] THE ROUND-ACCESSOR RULING (verify batch 5, MF4).
// ===================================================================================================
// GetCurrentRound() and GetTotalRounds() were DECLARE-ONLY, and wave B started spelling the same
// console expression two mutually exclusive ways -- some bodies wrote `GetCurrentRound()` for
// NRM+300 - NRM+296 - 1, others wrote `GetTotalRounds() - GetCurrentRound() - 1` for it (which only
// works if GetCurrentRound() == miRoundsRemaining). Both cannot be right. RULING, and why:
//
//   * The console expression itself, re-dumped from ModeManager::TellGuiToShowOnlineFinalStandings
//     @0x82329B68 this pass:
//         addis r10, r11, 1 / addi r10, r10, -0x4608   -> r10 = gsm + 47608 == the NetworkRoundManager
//         lwz   r11, 0x12C(r10)                        -> miTotalRounds     (+300)
//         lwz   r10, 0x128(r10)                        -> miRoundsRemaining (+296)
//         subf  r11, r10, r11 / addi r5, r11, -1       -> total - remaining - 1
//   * The tree ALREADY de-inlined that exact expression once, before this wave, and named it:
//     RichPresenceManagerBase::SetRoundParameter (X360 0x8235A6C8,
//     BrnGameStateRichPresenceManagerBase.cpp:171-185) computes
//     `liCurrentRound = GetCurrentRound()` and then displays `liCurrentRound + 1`, i.e. it treats
//     GetCurrentRound() as the ZERO-BASED CURRENT ROUND INDEX. That committed body outranks a
//     de-inlining judgement made this wave.
// => GetCurrentRound() IS (miTotalRounds - miRoundsRemaining - 1). Any body that spells
//    `GetTotalRounds() - GetCurrentRound() - 1` for the console's round index is now WRONG (it
//    evaluates back to miRoundsRemaining) and must call GetCurrentRound() directly; any body that
//    wants the raw remaining count calls GetRoundsRemaining() below instead of open-coding it.
// ===================================================================================================

// X360-INLINED at every call site (no standalone export). The 0-based index of the round being
// played: with miTotalRounds seeded once and miRoundsRemaining decremented in OnRoundStart above,
// the first round gives total - (total-1) - 1 == 0.
s32 NetworkRoundManager::GetCurrentRound() const
{
    return miTotalRounds - miRoundsRemaining - 1;
}

// X360-INLINED. The raw counter at NRM+300.
s32 NetworkRoundManager::GetTotalRounds() const
{
    return miTotalRounds;
}

// X360-INLINED. The raw counter at NRM+296. ModeManager::SendModeStopMessages reads +296 directly
// at four sites; this is the named reader so no caller has to re-derive it from the other two.
s32 NetworkRoundManager::GetRoundsRemaining() const
{
    return miRoundsRemaining;
}

// ===================================================================================================
// The two embedded-event accessors + the player-join flag.
//
// [stuntrace waveB CLOSURE round, 2026-08-26] Bodied. GetStartingFreeburnLobbyDueToPlayerJoin and
// GetNetworkGameEvent were the cross-seam audit's two NEW unresolved externals -- neither appeared
// on any per-batch verifier's list, and between them they have five wave-B call sites:
//   GetStartingFreeburnLobbyDueToPlayerJoin  BrnModeManager_Prepare.cpp:578,
//                                            BrnModeManager_Start.cpp:572 and :769
//   GetNetworkGameEvent                      BrnModeManager_Prepare.cpp:659,
//                                            BrnOnlineFreeBurnLobbyMode.cpp:30,
//                                            BrnOnlineShowtimeMode.cpp:102,
//                                            BrnOnlineStuntRunMode.cpp:208
// GetNetworkRoundEvent (one caller, BrnOnlineStuntRunMode.cpp:259) is bodied with them rather than
// left as the odd one out of a symmetric pair.
//
// All three are X360-INLINED (no standalone exports), and all three offsets are read outright by
// ModeManager::SendModeStopMessages @0x8234BEC0, which holds the manager in r11 off
// ModeManager+0x6D64:
//   0x8234C66C  lbz r11, 0x130(r11)   -> mbStartingGameDueToPlayerJoin, stored straight into the
//                                        1-byte action-40 payload it posts at 0x8234C674
//                                        (`li r5, 0x28` == 40). That is the same +0x130 the header's
//                                        layout table pins and the same byte the Start.cpp call
//                                        sites annotate as "NRM+0x130".
// The two event accessors are address-of on the embedded members the header's layout table already
// pins at +0x000 and +0x100 (256 + 40 == 296, which is exactly where miRoundsRemaining lands, so
// the run is self-checking). Reached BY NAME -- no offset arithmetic survives into the source.
// ===================================================================================================

const GameStateModuleIO::StartNetworkGameEvent* NetworkRoundManager::GetNetworkGameEvent() const
{
    return &mStartNetworkGameEvent;
}

const GameStateModuleIO::StartNetworkRoundEvent* NetworkRoundManager::GetNetworkRoundEvent() const
{
    return &mStartNetworkRoundEvent;
}

bool NetworkRoundManager::GetStartingFreeburnLobbyDueToPlayerJoin()
{
    return mbStartingGameDueToPlayerJoin;
}

}
