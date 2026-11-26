#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/CgsServerInterfacePlayerParamsX360.h"
#include "lobbyapi.h"        // LobbyApiPlayerT
#include "lobbytagfield.h"   // DirtySDK TagFieldGetBinary
#include <cstring>           // std::memcpy / std::strchr
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <xtl.h>             // XMemSet, XOnlineGetNatType (xonline.h), XNetGetTitleXnAddr (winsockx.h)

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

    // XONLINE_NAT_TYPE is an enum: ARTIST compares it signed (cmpwi @0x8287A270).
    switch (XOnlineGetNatType())
    {
    case XONLINE_NAT_OPEN:
        meFirewallSetting = E_FIREWALL_OPEN;
        break;
    case XONLINE_NAT_MODERATE:
        meFirewallSetting = E_FIREWALL_MODERATE;
        break;
    case XONLINE_NAT_STRICT:
        meFirewallSetting = E_FIREWALL_STRICT;
        break;
    default:
        // ARTIST names the shared player-params file (li r5,0xA3 @0x8287A290).
        CGS_ASSERT_AT(false, "invalid X360 NAT type\n",
                      "..\\..\\..\\GameShared\\GameClasses\\Network\\ServerInterface\\DirtySock\\Components\\CgsServerInterfacePlayerParams.cpp",
                      163);
        break;
    }

    // The 36-byte secure address is the XNADDR the XDK fills in place.
    XNetGetTitleXnAddr(reinterpret_cast<XNADDR*>(macNetworkAddress));
    return true;
}

void* ServerInterfacePlayerParamsX360::GetNetworkAddress(void* lpDestination, const ServerInterfacePlayerParamsX360* lpSource)
{
    std::memcpy(lpDestination, lpSource->macNetworkAddress, 36);
    return lpDestination;
}

void ServerInterfacePlayerParamsX360::SerialiseFromPlayer(const LobbyApiPlayerT* lpPlayer)
{
    ServerInterfacePlayerParamsBase::SerialiseFromPlayer(lpPlayer);

    const char* lpcMarker  = std::strchr(lpPlayer->strMachineAddr, '^');
    if (lpcMarker != 0)
    {
        TagFieldGetBinary(lpcMarker, macNetworkAddress, 36);
    }
}

}
