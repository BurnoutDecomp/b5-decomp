#ifndef CGS_SERVER_INTERFACE_CUSTOM_COMMANDS_H
#define CGS_SERVER_INTERFACE_CUSTOM_COMMANDS_H

#include "types.hpp"

#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"

// ===========================================================================
// CgsNetwork::ServerInterfaceCustomCommands
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceCustomCommands.{h,cpp}
//
// The platform-shared "custom commands" server-interface component base. It sits
// between the polymorphic CgsNetwork::ServerInterfaceComponent and the Burnout
// leaf BrnNetwork::ServerInterfaceCustomCommands.
//
// SHAPE from DecFIGS DWARF (CgsServerInterfaceCustomCommands.h:48):
//     struct CgsNetwork::ServerInterfaceCustomCommands
//         : public CgsNetwork::ServerInterfaceComponent
//     {
//         virtual void Construct();
//         bool Prepare();
//         bool Release();
//         void Destruct();
//         virtual void OnEvent(EServerInterfaceEvent, void*);
//         virtual ~ServerInterfaceCustomCommands();
//     };
//
// It adds NO data members of its own -- the DWARF lists none and the X360 ARTIST
// asm for the Burnout leaf reads its first own field (mpServerInterface) at the
// 4-word component-base boundary (+0x10). So the object layout is identical to the
// component base; this class only interposes the custom-commands vtable overrides
// and the non-virtual Prepare/Release/Destruct lifecycle helpers.
//
// Only the declarations needed by the Burnout leaf are reconstructed here (Prepare /
// Release gate the leaf's own Prepare/Release in the X360 build); their bodies live
// in the dedicated CgsServerInterfaceCustomCommands.cpp TU.
// ===========================================================================

namespace CgsNetwork
{
    class ServerInterfaceCustomCommands : public ServerInterfaceComponent
    {
    public:
        // CgsServerInterfaceCustomCommands.cpp:39 -- vtable slot 0 (component-base reset).
        virtual void Construct();

        // CgsServerInterfaceCustomCommands.cpp:54 / :69 / :84 -- non-virtual lifecycle. The
        // Burnout leaf calls Prepare()/Release() first and only latches/clears its own state
        // when the base step succeeds. Bodied in the dedicated TU; declared-only here.
        bool Prepare();
        bool Release();
        void Destruct();

        // CgsServerInterfaceCustomCommands.cpp:101 -- vtable slot 2.
        virtual void OnEvent(EServerInterfaceEvent leEvent, void* lpData);
    };
}

#endif // CGS_SERVER_INTERFACE_CUSTOM_COMMANDS_H
