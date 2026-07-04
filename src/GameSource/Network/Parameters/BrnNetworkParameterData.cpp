// ============================================================================
// GameSource/Network/Parameters/BrnNetworkParameterData.cpp
// ============================================================================
// BrnNetwork matchmaking/session-context free helpers, reconstructed store-for-store
// from BURNOUT_X360_ARTIST.XEX. They amend / seed the LIVE session-context array (the
// { muContextId, muValue } id/value pair list the matchmaking layer publishes).
//
//   BrnNetwork::TranslateGameMode     @ 0x82587450
//   BrnNetwork::AmendGameModeContexts @ 0x82587600   (game-mode context, id 3)
//   BrnNetwork::AmendRankedContexts   @ 0x825876C0   (ranked context, id 0x800A; FIRST-match)
//   BrnNetwork::SetUpContexts         @ 0x82587738
//
// AmendRankedContexts here is a DISTINCT function from the CgsNetwork 0x828778F0
// LAST-match twin (this one is the FIRST-match scan). The streamed/file-line asserts are
// reduced to CGS_ASSERT per project convention.

#include "GameSource/Network/Parameters/BrnNetworkParameterData.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnNetwork
{
    // Maps the game-params object to its game-mode context value (X360 sub_82587518,
    // called with the first arg). Homed in the GameParams behavioural TU; forward-declared
    // here so AmendGameModeContexts links. (Real symbol name unrecovered.)
    s32 GetGameModeContextValue(void* lpParams);

    // ---- TranslateGameMode @ 0x82587450 --------------------------------------
    // Maps a game-side game-mode id (1..4) to its LIVE game-mode context value
    // (jump-table switch on (liGameMode - 1), cmplwi 3 bound). Unknown modes trip a
    // non-gating assert and translate to 0.
    s32 TranslateGameMode(s32 liGameMode)
    {
        switch (liGameMode)
        {
        case 1:  return 1;
        case 2:  return 2;
        case 3:  return 5;
        case 4:  return 6;
        default:
            CGS_ASSERT(false, "Unknown game mode");
            return 0;
        }
    }

    // ---- AmendGameModeContexts @ 0x82587600 -----------------------------------
    // Sets (or overwrites) the game-mode context entry (id 3) in lpaContexts. Scans the
    // existing entries; asserts at most one already exists; if absent appends a fresh slot
    // (post-incrementing the count). Writes the game-mode id and GetGameModeContextValue()
    // as its value, and returns that value (the asm threads r3 straight through).
    s32 AmendGameModeContexts(void* lpParams, s32* lpiCount, MatchmakingContext* lpaContexts)
    {
        s32 liGameModeIndex = -1;

        if (*lpiCount > 0)
        {
            s32 li = 0;
            do
            {
                if (lpaContexts[li].muContextId == KU_CONTEXT_GAME_MODE)
                {
                    CGS_ASSERT(liGameModeIndex == -1, "liGameModeIndex == -1");
                    liGameModeIndex = li;
                }
                ++li;
            }
            while (li < *lpiCount);
        }

        if (liGameModeIndex == -1)
        {
            liGameModeIndex = *lpiCount;
            *lpiCount = liGameModeIndex + 1;
        }

        MatchmakingContext& lSlot = lpaContexts[liGameModeIndex];
        lSlot.muContextId = KU_CONTEXT_GAME_MODE;
        s32 liValue = GetGameModeContextValue(lpParams);
        lSlot.muValue = static_cast<u32>(liValue);
        return liValue;
    }

    // ---- AmendRankedContexts @ 0x825876C0 -------------------------------------
    // Sets (or overwrites) the ranked context entry (id 0x800A) in lpaContexts.
    // FIRST-match scan (breaks on first equal, beq loc_825876F8); if absent, appends a
    // fresh slot (post-inc *lpiCount). Writes the ranked id and value (lbRanked==0) -- 1
    // when unranked, 0 when ranked (clrlwi/cntlzw/extrwi zero-test). Returns lbRanked (r3
    // threaded).
    char AmendRankedContexts(char lbRanked, s32* lpiCount, MatchmakingContext* lpaContexts)
    {
        const s32 liCount = *lpiCount;
        s32 liSlot = 0;

        if (liCount > 0)
        {
            while (lpaContexts[liSlot].muContextId != KU_CONTEXT_RANKED)
            {
                ++liSlot;
                if (liSlot >= liCount)
                {
                    liSlot = liCount;
                    *lpiCount = liCount + 1;
                    break;
                }
            }
        }
        else
        {
            liSlot = liCount;
            *lpiCount = liCount + 1;
        }

        MatchmakingContext& lSlot = lpaContexts[liSlot];
        lSlot.muContextId = KU_CONTEXT_RANKED;
        lSlot.muValue     = (lbRanked == 0) ? 1u : 0u;
        return lbRanked;
    }

    // ---- SetUpContexts @ 0x82587738 ------------------------------------------
    // Seeds the X360 LIVE matchmaking context array with the three standard game-search
    // contexts, appending each {id,value} pair and advancing the count word. Returns the
    // translated game-mode value, or the incoming liGameMode when the game-mode context is
    // skipped (r3 is threaded through).
    s32 SetUpContexts(s32 liGameMode, char lbRanked, s32* lpiCount,
                      MatchmakingContext* lpaContexts)
    {
        *lpiCount = 0;

        lpaContexts[*lpiCount].muContextId = KU_CONTEXT_RANKED;
        lpaContexts[*lpiCount].muValue     = (lbRanked == 0) ? 1u : 0u;
        ++*lpiCount;

        lpaContexts[*lpiCount].muContextId = KU_CONTEXT_UNRANKED;
        lpaContexts[*lpiCount].muValue     = 0u;
        ++*lpiCount;

        s32 liResult = liGameMode;
        if (liGameMode != 0)
        {
            lpaContexts[*lpiCount].muContextId = KU_CONTEXT_GAME_MODE;
            liResult = TranslateGameMode(liGameMode);
            lpaContexts[*lpiCount].muValue = static_cast<u32>(liResult);
            ++*lpiCount;
        }
        return liResult;
    }
}
