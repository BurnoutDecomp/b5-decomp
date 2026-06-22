#ifndef CGS_SERVER_INTERFACE_RANKINGS_H
#define CGS_SERVER_INTERFACE_RANKINGS_H

#include "types.hpp"

// ===========================================================================
// CgsNetwork::ServerInterfaceRankings
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceRankings.{h,cpp}
//
// The rankings server-interface component (the E_PREPARESTAGE_RANKINGS_COMPONENT /
// E_RELEASESTAGE_RANKINGS_COMPONENT stage owner; embedded by value as
// BrnServerInterfaceBase::mRankings). Like every other DirtySock component it is a
// polymorphic leaf over the shared ServerInterfaceComponent base.
//
// LAYOUT: the leading members mirror the committed ServerInterfaceComponent base
// (CgsServerInterfaceComponent.h) exactly. Modelled here as the leading members --
// rather than via C++ inheritance -- to stay member-by-name without re-forking that
// type's standalone header, matching the existing sibling-component homes
// (CgsServerInterfaceServerInfo.h) in this directory:
//   +0x00  vptr
//   +0x04  mpcCurrentAction  (const char*)
//   +0x08  meStatus          (s32; 2 == "no error")
//   +0x0C  miLastError       (s32)
//
// The scalar deleting destructor @ 0x827DE238-sibling 0x827DE310 restores the shared
// component vtable slot (off_820CDBF8) at this+0, then conditionally frees -- i.e. the
// component carries no owned heap members of its own beyond the base layout. Any
// rankings-specific data members are not present in the available exports (the leaf's
// only emitted code path is the trivial scalar deleting destructor), so none are
// modelled here; they would GROW this home additively when their stores are recovered.
// ===========================================================================

namespace CgsNetwork
{
    // Event enum used by the OnEvent vtable slot (full set in the events home).
    enum EServerInterfaceEvent : s32;

    class ServerInterfaceRankings
    {
    public:
        ServerInterfaceRankings();

        // Scalar deleting destructor @ 0x827DE310 (restores off_820CDBF8, conditional free).
        virtual ~ServerInterfaceRankings();

        // Declared-only lifecycle virtuals (bodies owned by dedicated component TUs;
        // not bodied here). Mirror the ServerInterfaceComponent vtable order.
        virtual void Construct();
        virtual void OnEvent(EServerInterfaceEvent leEvent, void* lpData);

    private:
        // --- ServerInterfaceComponent base layout (see header note) ---
        const char* mpcCurrentAction;   // +0x04
        s32         meStatus;           // +0x08
        s32         miLastError;        // +0x0C
    };
}

#endif // CGS_SERVER_INTERFACE_RANKINGS_H
