#ifndef CGS_SERVER_INTERFACE_PLAYER_PARAMS_X360_H
#define CGS_SERVER_INTERFACE_PLAYER_PARAMS_X360_H

#include "types.hpp"
#include "../Components/CgsServerInterfacePlayerParams.h"   // ServerInterfacePlayerParamsBase

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
//     +0x60  macNetworkAddress[36]   (char) -- the 36-byte secure network address
//
// GetNetworkAddress copies the 36 bytes at +0x60 into the caller's buffer;
// SerialiseFromPlayer chains to the base then locates the '^' structure marker in
// the lobby record and decodes the 36-byte binary blob into macNetworkAddress.
// ===========================================================================

namespace CgsNetwork
{
    struct ServerInterfacePlayerParamsX360 : public ServerInterfacePlayerParamsBase
    {
    public:
        ServerInterfacePlayerParamsX360();

        // @ X360 0x8287A340 -- chain to the base, then parse the 36-byte network
        // address out of the lobby record. Returns the parsed blob pointer (the
        // TagFieldGetBinary result), or NULL if the record has no '^' marker.
        virtual void SerialiseFromPlayer(const void* lpPlayer);

        // @ X360 0x8287A308 -- copy the 36-byte secure network address into
        // lpDestination. Reached from BrnNetwork::CameraX360::PlayerFinalised.
        void* GetNetworkAddress(void* lpDestination) const;

    protected:
        // CgsServerInterfacePlayerParamsX360 leaf member, +0x60 (after the base).
        char macNetworkAddress[36];
    };
}

#endif // CGS_SERVER_INTERFACE_PLAYER_PARAMS_X360_H
