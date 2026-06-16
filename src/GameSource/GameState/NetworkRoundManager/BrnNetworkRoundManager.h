// ===== b5-decomp/src/GameSource/GameState/NetworkRoundManager/BrnNetworkRoundManager.h =====
#pragma once

#include "types.hpp"
#include "GameSource/GameState/BrnGameEvents.h"   // StartNetworkGameEvent, StartNetworkRoundEvent

// Unified owning header for BrnGameState::NetworkRoundManager (DWARF: BrnNetworkRoundManager.h:43,
// non-polymorphic). The two embedded events carry the bulk of the layout; their X360 byte sizes are
// load-bearing here because the X360 bodies index past them by raw offset:
//   mStartNetworkGameEvent  @ 0x000  (StartNetworkGameEvent, 256 bytes -- X360 memcpy size, 0x82358928)
//   mStartNetworkRoundEvent @ 0x100  (StartNetworkRoundEvent,  40 bytes -- X360 10-dword copy, 0x823589E8)
//   miRoundsRemaining       @ 0x128 (296)  -- X360 *(this+296) / this[74]
//   miTotalRounds           @ 0x12C (300)  -- X360 *(this+300)
//   mbStartingGameDueToPlayerJoin @ 0x130 (304) -- X360 *(this+304)
// 256 + 40 == 296, so the trailing scalars land exactly where the X360 code reads them. Only the
// three methods this surgery TU unblocks (NetworkGameStarted/NetworkRoundStarted/OnRoundStart) plus
// the DWARF-attested accessor spine are declared; the remaining methods are forward-declared exactly
// as the DWARF lists them so callers compile, with bodies arriving in their own slices.

namespace BrnGameState
{

class NetworkRoundManager
{
public:
    void Construct();                                                          // X360 (Construct)
    void Destruct();                                                           // X360 (Destruct)
    bool Prepare();                                                            // X360 (Prepare)
    bool Release();                                                            // X360 (Release)

    void NetworkGameStarted(const GameStateModuleIO::StartNetworkGameEvent* lpStartNetworkGameEvent);   // X360 0x82358928
    void NetworkRoundStarted(const GameStateModuleIO::StartNetworkRoundEvent* lpStartNetworkRoundEvent); // X360 0x823589E8
    void OnRoundStart();                                                       // X360 0x82358A68
    void PreparedForMode();                                                    // X360 (PreparedForMode)

    const GameStateModuleIO::StartNetworkGameEvent*  GetNetworkGameEvent() const;
    const GameStateModuleIO::StartNetworkRoundEvent* GetNetworkRoundEvent() const;
    s32  GetCurrentRound() const;
    s32  GetTotalRounds() const;
    bool IsLastRound();
    bool IsRankedMatch();
    bool GetStartingFreeburnLobbyDueToPlayerJoin();

private:
    GameStateModuleIO::StartNetworkGameEvent  mStartNetworkGameEvent;          // 0x000 (256 bytes)
    GameStateModuleIO::StartNetworkRoundEvent mStartNetworkRoundEvent;         // 0x100 ( 40 bytes)
    s32  miRoundsRemaining;                                                    // 0x128 (296)
    s32  miTotalRounds;                                                        // 0x12C (300)
    bool mbStartingGameDueToPlayerJoin;                                        // 0x130 (304)
};

}
