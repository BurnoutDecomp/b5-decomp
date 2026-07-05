#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/CgsServerInterfaceGameParamsX360.h"

#include <string.h>   // memset (XMemSet)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceGameParamsX360::Prepare            @ 0x82877810
//   CgsNetwork::ServerInterfaceGameParamsX360::SerialiseFromGame  @ 0x828778E8
//   CgsNetwork::ServerInterfaceGameParamsX360::SerialiseToString  @ 0x828778E0
//   CgsNetwork::ServerInterfaceGameParamsX360::SetRankedGame      @ 0x825411B0
//   CgsNetwork::ServerInterfaceGameParamsX360::operator=          @ 0x82558DB0
//
// The X360 leaf adds the Xbox LIVE matchmaking context array on top of the shared
// game-parameter block. SerialiseFromGame / SerialiseToString are pure forwarders
// to the base (they occupy the leaf's vtable slots but add no work); Prepare seeds
// two default contexts; SetRankedGame re-publishes the ranked (0x800A) context.

namespace CgsNetwork
{

// LIVE context ids seeded by Prepare.
static const u32 KU_GAMEPARAMS_CONTEXT_RANKED   = 0x800Au;   // 32778
static const u32 KU_GAMEPARAMS_CONTEXT_GAMEMODE = 0x800Bu;   // 32779

// Prepare @ 0x82877810.
bool ServerInterfaceGameParamsX360::Prepare()
{
    if (!ServerInterfaceGameParamsBase::Prepare())
    {
        return false;
    }

    miContextCount  = 0;
    miPropertyCount = 0;
    memset(maRankedContexts, 0, sizeof(maRankedContexts));

    // Append the ranked (0x800A) context, value 0.
    maRankedContexts[miContextCount].muContextId = KU_GAMEPARAMS_CONTEXT_RANKED;
    maRankedContexts[miContextCount].muValue     = 0;
    ++miContextCount;

    // Append the game-mode (0x800B) context, value 0.
    maRankedContexts[miContextCount].muContextId = KU_GAMEPARAMS_CONTEXT_GAMEMODE;
    maRankedContexts[miContextCount].muValue     = 0;
    ++miContextCount;

    return true;
}

// SerialiseFromGame @ 0x828778E8 -- pure forward to the base.
void ServerInterfaceGameParamsX360::SerialiseFromGame(const void* lpGame)
{
    ServerInterfaceGameParamsBase::SerialiseFromGame(lpGame);
}

// SerialiseToString @ 0x828778E0 -- pure forward to the base.
void ServerInterfaceGameParamsX360::SerialiseToString(char* lpcRecord, s32 liRecLen) const
{
    ServerInterfaceGameParamsBase::SerialiseToString(lpcRecord, liRecLen);
}

// SetRankedGame @ 0x825411B0 -- set/clear the ranked bit in muGameFlags, then
// re-publish the 0x800A LIVE context (value = !lbRanked) via AmendRankedContexts.
void ServerInterfaceGameParamsX360::SetRankedGame(bool lbRanked)
{
    if (lbRanked)
    {
        muGameFlags |= KU_GAMEPARAMS_RANKED_FLAG;
    }
    else
    {
        muGameFlags &= ~KU_GAMEPARAMS_RANKED_FLAG;
    }

    AmendRankedContexts(static_cast<char>(lbRanked), &miContextCount, maRankedContexts);
}

// operator= @ 0x82558DB0 -- chain to the base member-wise copy, then copy the
// leaf's 10-entry context array followed by both counters, in that store order.
ServerInterfaceGameParamsX360&
ServerInterfaceGameParamsX360::operator=(const ServerInterfaceGameParamsX360& lrOther)
{
    ServerInterfaceGameParamsBase::operator=(lrOther);

    for (s32 li = 0; li < KI_GAMEPARAMS_MAX_CONTEXTS; ++li)
    {
        maRankedContexts[li] = lrOther.maRankedContexts[li];
    }
    miContextCount  = lrOther.miContextCount;
    miPropertyCount = lrOther.miPropertyCount;

    return *this;
}

}
