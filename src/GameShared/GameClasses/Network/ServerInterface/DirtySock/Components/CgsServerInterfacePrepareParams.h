#ifndef CGS_SERVER_INTERFACE_PREPARE_PARAMS_H
#define CGS_SERVER_INTERFACE_PREPARE_PARAMS_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"   // LobbyPrepareParams, ConnAPIPrepareParams

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
// DECLARATION SHAPE -- AUTHORITATIVE, from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameShared/GameClasses/Network/ServerInterface/
//  DirtySock/CgsServerInterfaceDirtySock.h:352, original source line 172):
//
//     struct CgsNetwork::ServerInterfacePrepareParams {
//         CgsNetwork::ServerInterfaceComponent *[12] mapComponents;  // src :177
//         LobbyPrepareParams                         mLobbyParams;   // src :178
//         ConnAPIPrepareParams                       mConnAPIParams; // src :179
//         void Construct();                                          // src :175
//     };
//
// LAYOUT (X360 asm @ 0x82580AF0 Construct, the sole emitted member of this struct).
// Construct does XMemSet(this, 0, 48) then SIX word stores (stw) clearing +0x30..+0x44
// and a single BYTE store (stb) clearing +0x48 -- used size 0x49 (73B). Every one of
// those writes lines up 1:1 with the DWARF shape above:
//   +0x00  mapComponents[12]  (ServerInterfaceComponent* x12) 12 * 4 == the XMemSet's 48
//   +0x30  mLobbyParams.miLanguage    (int32)  stw
//   +0x34  mLobbyParams.mpcVersion    (char*)  stw
//   +0x38  mLobbyParams.mpcSKU        (char*)  stw
//   +0x3C  mLobbyParams.mpcSLUS       (char*)  stw
//   +0x40  mConnAPIParams.miPort      (int32)  stw
//   +0x44  mConnAPIParams.miMaxPlayers(int32)  stw
//   +0x48  mConnAPIParams.mbPeerToPeer(bool)   stb  <-- the lone byte store
// (the byte store at +0x48 is what identifies the tail as ConnAPIPrepareParams, whose
// third and last member is the only `bool` in the block.)
//
// The two trailing blocks are the LobbyPrepareParams and ConnAPIPrepareParams structs of
// CgsServerInterfaceDirtySock.h, embedded by value.
//
// The struct is non-polymorphic (Construct takes `this` as a plain pointer; no vtable
// store).
//
// HOST vs CONSOLE: every offset above is a CONSOLE (32-bit) offset and appears in
// COMMENTS ONLY. On the LLP64 host each ServerInterfaceComponent* is 8 bytes, so
// mapComponents alone is 96 bytes and nothing after +0x00 sits where the console put it.
// Access members by name; never index or offset this block with a console literal.
// ===========================================================================

namespace CgsNetwork
{
    // Forward -- the components the Prepare chain hands round. Matches the class-key
    // used by its home (CgsServerInterfaceComponent.h:52) and by the sibling forwards in
    // CgsServerInterfaceDirtySock.h:84 / CgsServerInterface.h:41.
    class ServerInterfaceComponent;

    struct ServerInterfacePrepareParams
    {
        // CgsServerInterfaceDirtySock.h Prepare-chain param block.
        void Construct();

        // +0x00 (console) -- DWARF `CgsNetwork::ServerInterfaceComponent *[12]
        // mapComponents`. Indexed by CgsNetwork::EComponents (E_COMPONENTS_COUNT == 12,
        // CgsServerInterfaceDirtySock.h:69); the bound is spelled 12 here rather than
        // E_COMPONENTS_COUNT, as the console array length. Each platform Prepare leaf
        // parks the component it contributes in its
        // own slot before chaining on, e.g. BrnServerInterfaceX360::Prepare @0x8258B200
        // `addi r30, r31, 0xC2C ; stw r30, 4(r4)` == mapComponents[E_COMPONENTS_GAMES]
        // (console slot 1) = &mGames.
        ServerInterfaceComponent* mapComponents[12];

        LobbyPrepareParams   mLobbyParams;     // +0x30
        ConnAPIPrepareParams mConnAPIParams;   // +0x40
    };
}

#endif // CGS_SERVER_INTERFACE_PREPARE_PARAMS_H
