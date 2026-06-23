#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceQuickJoinParams.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf
#include "lobbytagfield.h"                                // DirtySDK TagFieldSetNumber
#include <cstring>                                        // std::memset

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceQuickJoinParamsBase::Prepare           @ 0x8287A398
//   CgsNetwork::ServerInterfaceQuickJoinParamsBase::SerialiseToString @ 0x8287A3E0
//   CgsNetwork::ServerInterfaceQuickJoinParamsBase::~...(vec del dtor) @ 0x82541550
//   CgsNetwork::ServerInterfaceQuickJoinParamsX360::Prepare           @ 0x8287A4A8
//
// Prepare resets the parameter list (count 0, all 16 slots -1) and clears the two
// flag bytes. SerialiseToString walks the (up to 16) populated slots and emits a
// GS<n> numeric field per used candidate game id, then the userset / force-leave
// markers. The X360 XMemSet(this+8, -1, 64) is a plain memset of the slot array.
//
// The X360 leaf Prepare @ 0x8287A4A8 does the same base reset (count 0, both flag
// bytes 0, 16 slots XMemSet to -1) then additionally zeroes the 80-byte X360 payload
// at +0x4C (XMemSet(this+0x4C, 0, 80)) and the trailing word at +0x9C, returning true.
// The vector-deleting destructor @ 0x82541550 is the compiler-emitted thunk over the
// base's virtual dtor (set vptr, conditionally operator delete).

namespace CgsNetwork
{

ServerInterfaceQuickJoinParamsBase::ServerInterfaceQuickJoinParamsBase()
{
}

ServerInterfaceQuickJoinParamsBase::~ServerInterfaceQuickJoinParamsBase()
{
}

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

ServerInterfaceQuickJoinParamsX360::ServerInterfaceQuickJoinParamsX360()
{
}

bool ServerInterfaceQuickJoinParamsX360::Prepare()
{
    // Base list reset (asm: stw 0 @+4; stb 0 @+0x49; stb 0 @+0x48; XMemSet(+8,-1,64)).
    miNumParameters = 0;
    mbJoinUserset   = false;   // stb 0 @+0x49
    mbRanked        = false;   // stb 0 @+0x48
    std::memset(maiQuickJoinParams, -1, sizeof(maiQuickJoinParams));   // XMemSet(+8,-1,64)

    // X360-only payload (asm: stw 0 @+0x9C; XMemSet(+0x4C, 0, 80)).
    muX360Field_9C = 0;
    std::memset(maX360Payload, 0, sizeof(maX360Payload));
    return true;
}

}
