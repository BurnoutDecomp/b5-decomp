#ifndef CGS_SERVER_INTERFACE_PREPARE_PARAMS_H
#define CGS_SERVER_INTERFACE_PREPARE_PARAMS_H

#include "types.hpp"

// ===========================================================================
// CgsNetwork::ServerInterfacePrepareParams
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfacePrepareParams.{h,cpp}
//
// The prepare-params block passed (by pointer) into the server-interface Prepare
// chain: CgsNetwork::ServerInterface::Prepare(ServerInterfacePrepareParams*),
// ServerInterfaceDirtySock::Prepare(...), and the X360 leaf
// ServerInterfaceDirtySockX360::Prepare(...). Forward-declared as a struct in
// CgsServerInterfaceDirtySock.h:154 and CgsServerInterfaceDirtySockX360.h:26; this
// home gives it its layout + the Construct() zero-initialiser.
//
// LAYOUT (X360 asm @ 0x82580AF0 Construct, the sole emitted member). Construct does
// XMemSet(this, 0, 48) over the head block, then SIX word stores (stw) clear +0x30..+0x44
// and a single BYTE store (stb) clears +0x48 -- used size 0x49 (73B):
//   +0x00  maHead[48]   (u8 x48)   opaque head block, XMemSet to 0
//   +0x30  muField_30   (u32)      stored 0 (stw)
//   +0x34  muField_34   (u32)      stored 0 (stw)
//   +0x38  muField_38   (u32)      stored 0 (stw)
//   +0x3C  muField_3C   (u32)      stored 0 (stw)
//   +0x40  muField_40   (u32)      stored 0 (stw)
//   +0x44  muField_44   (u32)      stored 0 (stw)
//   +0x48  mu8Field_48  (u8)       stored 0 (stb -- single byte, NOT a word)
//
// The head block's internal sub-structure and the descriptive names of the seven
// trailing words are not present in the available exports (Construct only zeroes
// them, no individual reads/writes are reachable). They keep raw offset names and a
// byte head; recovering their meaning would GROW this home additively. The struct is
// non-polymorphic (Construct takes `this` as a plain pointer; no vtable store).
// ===========================================================================

namespace CgsNetwork
{
    struct ServerInterfacePrepareParams
    {
        // CgsServerInterfaceDirtySock.h Prepare-chain param block.
        void Construct();

        u8  maHead[48];      // +0x00  opaque head, zeroed by Construct
        u32 muField_30;      // +0x30
        u32 muField_34;      // +0x34
        u32 muField_38;      // +0x38
        u32 muField_3C;      // +0x3C
        u32 muField_40;      // +0x40
        u32 muField_44;      // +0x44
        u8  mu8Field_48;     // +0x48 (byte; asm stb, not a word)
    };
}

#endif // CGS_SERVER_INTERFACE_PREPARE_PARAMS_H
