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

    // The server-interface result enum (full enumerator set homed in
    // CgsServerInterfaceErrors.h). Forward-declared with a fixed underlying type so the
    // ConvertError return type is well-formed without pulling in that header.
    enum EServerInterfaceError : s32;

    // { DirtySock error code -> EServerInterfaceError } mapping entry (home:
    // CgsServerInterfaceDirtySockErrorHelpers.h). Reached only by pointer here, so the
    // forward declaration is sufficient for the ConvertError signature.
    struct DSErrorToServerInterfaceError;

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

        // Reset the component's pending error/status back to idle (X360 @ 0x828766F0). Returns the
        // component pointer (the X360 ABI hands `this` back in r3). The body lives in the sibling
        // CgsServerInterfaceComponent.cpp (same mangled symbol); declared here so callers that hold
        // the component by name (e.g. the network debug components) can drive it without forking the
        // layout. Public to match the X360 call sites that invoke it through a base pointer.
        void* ClearLastError();

        // Current action status (== meStatus; ServerInterfaceDirtySock::EStatus, 1 == error, 2 ==
        // idle). Inline accessor over the named member so callers can branch on it without reaching
        // into protected state.
        s32 GetStatus() const { return meStatus; }

    protected:
        // ---- Action / error helpers shared by every leaf component ------------------
        // (Bodied in dedicated component TUs / CgsServerInterfaceComponent.cpp; declared
        //  here so derived leaves can drive an action through the base by name. In the
        //  Jan-2008 ARTIST build these helpers live on ServerInterfaceComponent -- they
        //  had migrated up from ServerInterfaceDirtySock since the Feb-2007 source.)

        // Begin an action: stash the human-readable action name and reset the error.
        void StartActionCore(const char* lpcAction);

        // End an action: record liError as the last error and mark the component idle.
        void EndActionCore(int liError);

        // Map a DirtySock error code to an EServerInterfaceError using the supplied table
        // (falling back to the shared default 'nfnd' table when the code is not present).
        EServerInterfaceError ConvertError(int liError,
                                           const DSErrorToServerInterfaceError* lpTable,
                                           int liCount) const;

        const char* mpcCurrentAction;   // +0x04
        s32         meStatus;           // +0x08
        s32         miLastError;        // +0x0C
    };
}

#endif // CGS_SERVER_INTERFACE_COMPONENT_H
