#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceQuickJoinParams.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf
#include "lobbytagfield.h"                                // DirtySDK TagFieldSetNumber
#include <cstring>                                        // std::memset

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceQuickJoinParamsBase::Prepare           @ 0x8287A398
//   CgsNetwork::ServerInterfaceQuickJoinParamsBase::SerialiseToString @ 0x8287A3E0
//
// Prepare resets the parameter list (count 0, all 16 slots -1) and clears the two
// flag bytes. SerialiseToString walks the (up to 16) populated slots and emits a
// GS<n> numeric field per used candidate game id, then the userset / force-leave
// markers. The X360 XMemSet(this+8, -1, 64) is a plain memset of the slot array.

namespace CgsNetwork
{

bool ServerInterfaceQuickJoinParamsBase::Prepare()
{
    miNumParameters = 0;
    mbJoinUserset   = false;   // asm stores 0 at +0x49
    mbRanked        = false;   // asm stores 0 at +0x48
    std::memset(maiQuickJoinParams, -1, sizeof(maiQuickJoinParams));   // 64 bytes
    return true;
}

void ServerInterfaceQuickJoinParamsBase::SerialiseToString(char* lpcRecord, s32 liRecLen) const
{
    s32 liEmitted = 0;
    for (s32 liSlot = 0; liSlot < KI_MAX_QUICK_JOIN_PARAMS; ++liSlot)
    {
        const s32 liGameId = maiQuickJoinParams[liSlot];
        if (liGameId != -1)
        {
            char lacFieldName[8];
            CgsCore::SPrintf(lacFieldName, 8, "GS%d", liSlot);
            TagFieldSetNumber(lpcRecord, liRecLen, lacFieldName, liGameId);
            if (miNumParameters == ++liEmitted)
            {
                break;
            }
        }
    }

    if (mbJoinUserset)
    {
        TagFieldSetNumber(lpcRecord, liRecLen, "SET", 1);
    }
    TagFieldSetNumber(lpcRecord, liRecLen, "FORCE_LEAVE", 1);
}

}
