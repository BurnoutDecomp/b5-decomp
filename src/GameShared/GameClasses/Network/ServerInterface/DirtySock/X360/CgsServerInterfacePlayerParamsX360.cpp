#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/CgsServerInterfacePlayerParamsX360.h"
#include "lobbytagfield.h"   // DirtySDK TagFieldGetBinary
#include <cstring>           // std::memcpy / std::strchr
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Xbox platform entry points (platform lane; no home header yet).
extern "C"
{
    void* XMemSet(void* lpDest, s32 liValue, u32 luCount);
    u32   XOnlineGetNatType();
    u32   XNetGetTitleXnAddr(void* lpXnAddr);
}

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfacePlayerParamsX360::GetNetworkAddress    @ 0x8287A308
//   CgsNetwork::ServerInterfacePlayerParamsX360::SerialiseFromPlayer  @ 0x8287A340
//
// GetNetworkAddress is static: it copies lpSource's 36-byte secure network
// address (macNetworkAddress, +0x60) into the caller-supplied lpDestination
// buffer and returns that buffer. (ASM: the memcpy dest is the untouched
// incoming r3 and its src is r4+0x60 -- both are plain explicit arguments,
// there is no implicit `this` read here.)
//
// SerialiseFromPlayer chains to the shared base (which fills the name / ident /
// presence / addresses + parses the USERPARAMS structure), then finds the '^'
// structure marker inside the lobby record's machine-address text (record + 0x1C)
// and decodes the 36-byte binary network address into macNetworkAddress. When the
// marker is absent the record carries no address and the field is left untouched.

namespace CgsNetwork
{

bool ServerInterfacePlayerParamsX360::Prepare()
{
    XMemSet(macMachineAddress, 0, sizeof(macMachineAddress));
    mpcName           = 0;
    miIdent           = 0;
    miPresence        = 0;
    muExternalAddress = 0;
    muInternalAddress = 0;
    muFlags           = 0;
    meFirewallSetting = E_FIREWALL_OPEN;

    // NAT type: 1 open, 2 moderate, 3 strict.
    switch (XOnlineGetNatType())
    {
    case 1:
        meFirewallSetting = E_FIREWALL_OPEN;
        break;
    case 2:
        meFirewallSetting = E_FIREWALL_MODERATE;
        break;
    case 3:
        meFirewallSetting = E_FIREWALL_STRICT;
        break;
    default:
        CGS_ASSERT(false, "invalid X360 NAT type\n");
        break;
    }

    XNetGetTitleXnAddr(macNetworkAddress);
    return true;
}

void* ServerInterfacePlayerParamsX360::GetNetworkAddress(void* lpDestination, const ServerInterfacePlayerParamsX360* lpSource)
{
    std::memcpy(lpDestination, lpSource->macNetworkAddress, 36);
    return lpDestination;
}

void ServerInterfacePlayerParamsX360::SerialiseFromPlayer(const void* lpPlayer)
{
    ServerInterfacePlayerParamsBase::SerialiseFromPlayer(lpPlayer);

    const char* lpcRecord = reinterpret_cast<const char*>(lpPlayer);
    const char* lpcMarker  = std::strchr(lpcRecord + 0x1C, '^');
    if (lpcMarker != 0)
    {
        TagFieldGetBinary(lpcMarker, macNetworkAddress, 36);
    }
}

}
