#ifndef CGS_SERVER_INTERFACE_COMPONENT_H
#define CGS_SERVER_INTERFACE_COMPONENT_H

#include "types.hpp"

// ===========================================================================
// CgsNetwork::ServerInterfaceComponent
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceComponent.{h,cpp}
//
// Polymorphic base for every DirtySock server-interface "component". This header
// models the type as the X360 build lays it out, grounded in the
// CgsServerInterfaceComponent.h DWARF:
//     +0x00  vptr
//     +0x04  mpcCurrentAction  (const char*)
//     +0x08  meStatus          (s32; ServerInterfaceDirtySock::EStatus, 2 == ok)
//     +0x0C  miLastError       (s32)
//
// The non-trivial behaviour of this component (Construct / ClearLastError /
// GetAndClearLastError / ConvertError) is reconstructed in the sibling
// CgsServerInterfaceComponent.cpp, which deliberately keeps a self-contained
// raw-word view of the same layout. THIS header exists so that the polymorphic
// vector-deleting destructor (X360 @ 0x827DB3E8) and the derived leaf component
// homes (ServerInterfaceHttp, etc.) can refer to the type and its vtable slot by
// name without re-forking the layout.
//
// The vtable, in DWARF order:
//     virtual void Construct();                                  (cpp:63)
//     virtual ~ServerInterfaceComponent();                       (h:55)
//     virtual void OnEvent(EServerInterfaceEvent, void*);        (h:61)
// (Construct / OnEvent are left as pure-declaration virtuals here -- their bodies
// live in the dedicated component TUs; only the destructor is bodied in this TU.)
// ===========================================================================

namespace CgsNetwork
{
    // Event enum used by the OnEvent vtable slot. The full enumerator set lives in the
    // CgsServerInterfaceEvents home; declared here with a fixed underlying type so the
    // OnEvent signature is well-formed without pulling in that header.
    enum EServerInterfaceEvent : s32;

    class ServerInterfaceComponent
    {
    public:
        ServerInterfaceComponent();

        // CgsServerInterfaceComponent.h:55 -- vector deleting destructor @ 0x827DB3E8.
        virtual ~ServerInterfaceComponent();

        // Declared-only virtual (body owned by a dedicated component TU; not bodied here).
        virtual void Construct();

        // CgsServerInterfaceComponent.h:61
        virtual void OnEvent(EServerInterfaceEvent leEvent, void* lpData);

    protected:
        const char* mpcCurrentAction;   // +0x04
        s32         meStatus;           // +0x08
        s32         miLastError;        // +0x0C
    };
}

#endif // CGS_SERVER_INTERFACE_COMPONENT_H
