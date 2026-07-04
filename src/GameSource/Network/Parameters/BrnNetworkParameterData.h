#ifndef GAMESOURCE_NETWORK_PARAMETERS_BRNNETWORKPARAMETERDATA_H
#define GAMESOURCE_NETWORK_PARAMETERS_BRNNETWORKPARAMETERDATA_H

#include "types.hpp"

// ============================================================================
// GameSource/Network/Parameters/BrnNetworkParameterData.h
//
// BrnNetwork matchmaking/session context helpers (BURNOUT_X360_ARTIST.XEX). These
// free functions publish the X360 LIVE session-context array -- the id/value pair
// list the matchmaking layer advertises for a hosted or searched game -- keyed by
// the standard game-search context ids.
//
//   SetUpContexts          @ 0x82587738  seed the search context array (ranked/
//                                         unranked/game-mode).
//   TranslateGameMode      @ 0x82587450  game-side game-mode id (1..4) -> LIVE value.
//   AmendGameModeContexts  @ 0x82587600  set/overwrite the game-mode context (id 3).
//   AmendRankedContexts    @ 0x825876C0  set/overwrite the ranked context (id 0x800A).
// ============================================================================

namespace BrnNetwork
{
    // A single LIVE context entry: an id paired with its value (8-byte stride).
    struct MatchmakingContext
    {
        u32 muContextId;   // +0x00
        u32 muValue;       // +0x04
    };

    // Standard game-search context ids (X360 LIVE property ids).
    static const u32 KU_CONTEXT_GAME_MODE = 3;        // Burnout game-mode property.
    static const u32 KU_CONTEXT_RANKED    = 0x800A;   // ranked-vs-unranked property (32778).
    static const u32 KU_CONTEXT_UNRANKED  = 0x800B;   // paired unranked property (32779).

    // Maps a game-side game-mode id (1..4) to its LIVE game-mode context value.
    s32 TranslateGameMode(s32 liGameMode);

    // Seeds the LIVE matchmaking context array with the three standard game-search
    // contexts, advancing *lpiCount. Returns the translated game-mode value (or the
    // incoming liGameMode when the game-mode context is skipped).
    s32 SetUpContexts(s32 liGameMode, char lbRanked, s32* lpiCount,
                      MatchmakingContext* lpaContexts);

    // Set (or overwrite) the game-mode context entry (id 3) in lpaContexts, appending a
    // fresh slot when absent. Returns the game-mode value written.
    s32 AmendGameModeContexts(void* lpParams, s32* lpiCount, MatchmakingContext* lpaContexts);

    // Set (or overwrite) the ranked context entry (id 0x800A) in lpaContexts, appending a
    // fresh slot when absent. Returns lbRanked.
    char AmendRankedContexts(char lbRanked, s32* lpiCount, MatchmakingContext* lpaContexts);
}

#endif // GAMESOURCE_NETWORK_PARAMETERS_BRNNETWORKPARAMETERDATA_H
