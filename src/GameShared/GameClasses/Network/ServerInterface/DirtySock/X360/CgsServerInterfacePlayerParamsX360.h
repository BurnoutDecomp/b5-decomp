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
// GetNetworkAddress is static: it copies the 36 bytes at lpSource's +0x60 into
// the caller-supplied lpDestination buffer; SerialiseFromPlayer chains to the
// base then locates the '^' structure marker in the lobby record and decodes
// the 36-byte binary blob into this object's own macNetworkAddress.
// ===========================================================================

namespace CgsNetwork
{
    struct ServerInterfacePlayerParamsX360 : public ServerInterfacePlayerParamsBase
    {
    public:
        ServerInterfacePlayerParamsX360();

        // @ X360 0x8287A228 -- the platform prepare that BrnNetwork::
        // PlayerParamsBase::Prepare (@ 0x8258A818) chains to first with a direct
        // base-qualified call (bl CgsNetwork__ServerInterfacePlayerParamsX360__Prepare,
        // r3 == this). Declared so that call binds to the platform override rather
        // than the shared base body; the body itself is not yet recovered (no
        // per-function export for 0x8287A228).
        virtual bool Prepare();

        // @ X360 0x8287A340 -- chain to the base, then parse the 36-byte network
        // address out of the lobby record. Returns the parsed blob pointer (the
        // TagFieldGetBinary result), or NULL if the record has no '^' marker.
        virtual void SerialiseFromPlayer(const void* lpPlayer);

        // @ X360 0x8287A308 -- static: copy the 36-byte secure network address
        // out of lpSource->macNetworkAddress into the raw lpDestination buffer.
        // Reached from BrnNetwork::CameraX360::PlayerFinalised. (ASM: memcpy's
        // dest is the untouched incoming r3, its src is r4+0x60 -- i.e. both
        // pointers are explicit arguments; there is no implicit `this` here.)
        static void* GetNetworkAddress(void* lpDestination, const ServerInterfacePlayerParamsX360* lpSource);

    protected:
        // CgsServerInterfacePlayerParamsX360 leaf member, +0x60 (after the base).
        char macNetworkAddress[36];
    };
}

#endif // CGS_SERVER_INTERFACE_PLAYER_PARAMS_X360_H
