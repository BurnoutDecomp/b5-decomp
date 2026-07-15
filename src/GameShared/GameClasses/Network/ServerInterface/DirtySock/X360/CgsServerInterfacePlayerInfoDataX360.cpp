#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/CgsServerInterfacePlayerInfoDataX360.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfacePlayerInfoDataX360::Prepare           @ 0x82879E80
//   CgsNetwork::ServerInterfacePlayerInfoDataX360::SerialiseFromUser @ 0x82879EE0
//
// The X360 leaf extends the shared player-info record with the 8-byte secure host
// address (muXUID, +0xF8). Prepare chains to the base and, on success, clears that
// field; SerialiseFromUser chains to the base and then decodes the address out of
// the lobby user record via DirtyAddrToHostAddr. Both are trivial two-step wrappers
// over ServerInterfacePlayerInfoDataBase.

namespace CgsNetwork
{

// DirtySDK helper: decode an 8-byte host address (XUID) out of a lobby record
// field into pHostAddr. Homed with the DirtySock ServerInterface (see
// CgsServerInterfacePlayerInfo.cpp / CgsServerInterfaceGamesX360.cpp).
s32 DirtyAddrToHostAddr(void* pHostAddr, s32 iLen, const void* pField);

// Prepare @ 0x82879E80 -- chain to the base; if it fails, return false; otherwise
// clear the 8-byte secure host address (asm: std r11=0, 0xF8(r31)) and return true.
bool ServerInterfacePlayerInfoDataX360::Prepare()
{
    if (!ServerInterfacePlayerInfoDataBase::Prepare())
    {
        return false;
    }

    muXUID = 0;
    return true;
}

// SerialiseFromUser @ 0x82879EE0 -- chain to the base to fill the shared record,
// then decode the 8-byte secure host address out of the lobby user struct
// (lpUser + 0x1D0) into muXUID. The asm tail-returns the DirtyAddrToHostAddr
// result (r3 = this + 0xF8, r4 = 8, r5 = lpUser + 0x1D0).
bool ServerInterfacePlayerInfoDataX360::SerialiseFromUser(const void* lpUser)
{
    ServerInterfacePlayerInfoDataBase::SerialiseFromUser(lpUser);

    const void* lpAddrField = static_cast<const u8*>(lpUser) + 0x1D0;
    return DirtyAddrToHostAddr(&muXUID, 8, lpAddrField) != 0;
}

}
