#ifndef CGS_SERVER_INTERFACE_PLAYER_PARAMS_X360_H
#define CGS_SERVER_INTERFACE_PLAYER_PARAMS_X360_H

#include "types.hpp"
#include "../Components/CgsServerInterfacePlayerParams.h"   // ServerInterfacePlayerParamsBase
#include <xtl.h>                                             // XNADDR (winsockx.h)

// ===========================================================================
// CgsNetwork::ServerInterfacePlayerParamsX360
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/
//         CgsServerInterfacePlayerParamsX360.{h,cpp}
//
// The X360 (and PC-remaster) platform leaf of ServerInterfacePlayerParamsBase. It
// adds the Xbox secure network-address blob (the 36-byte XNADDR/host-address the
// peers exchange to reach each other) on top of the shared per-player parameter
// block, and overrides SerialiseFromPlayer to additionally parse / re-emit that
// address through the lobby tagfield record.
//
// LAYOUT (X360 asm @ 0x8287A308 GetNetworkAddress / 0x8287A340 SerialiseFromPlayer):
//   the base ends at +0x60 (meFirewallSetting at +0x5C); the X360 leaf adds:
//     +0x60  mNetworkAddress   XNADDR -- the 36-byte secure network address
//
// GetNetworkAddress returns the address by value (the caller's return slot in r3, this
// in r4: ARTIST 0x8287A308 copies r4+0x60 into r3); SerialiseFromPlayer chains to the
// base then locates the '^' structure marker in the lobby record and decodes the
// 36-byte binary blob into mNetworkAddress.
// ===========================================================================

namespace CgsNetwork
{
    struct ServerInterfacePlayerParamsX360 : public ServerInterfacePlayerParamsBase
    {
    public:
        // Inlined into the owning game-side constructor (vtable store only).
        ServerInterfacePlayerParamsX360() {}

        // The platform prepare the game-side player params chain to first: reset every
        // field, derive the firewall setting from the NAT type and read this console's
        // secure network address.
        virtual bool Prepare();

        // @ X360 0x8287A340 -- chain to the base, then parse the 36-byte network
        // address out of the lobby record. Returns the parsed blob pointer (the
        // TagFieldGetBinary result), or NULL if the record has no '^' marker.
        virtual void SerialiseFromPlayer(const LobbyApiPlayerT* lpPlayer);

        // @ X360 0x8287A308 -- the secure network address, by value. Its one caller,
        // BrnNetwork::CameraX360::PlayerFinalised (0x825951D4), passes a stack return slot
        // in r3 and copies the result out of it (memcpy 0x24, 0x825951E4).
        XNADDR GetNetworkAddress() const;

    protected:
        // CgsServerInterfacePlayerParamsX360 leaf member, +0x60 (after the base).
        XNADDR mNetworkAddress;
    };
}

#endif // CGS_SERVER_INTERFACE_PLAYER_PARAMS_X360_H
