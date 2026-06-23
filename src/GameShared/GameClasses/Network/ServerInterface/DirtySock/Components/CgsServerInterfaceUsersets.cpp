#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceUsersets.h"

#include <new>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceUsersets::~ServerInterfaceUsersets
//       ['vector deleting destructor' thunk] @ 0x827DE280
//
// MSVC's vector-deleting-destructor for the polymorphic ServerInterfaceUsersets
// component: it restores the shared server-interface-component vtable pointer
// (off_820CDBF8 -- the ServerInterfaceComponent slot this leaf inherits) at this+0 and,
// if the low "should-free" flag bit is set, calls operator delete on the object before
// returning `this`:
//
//   *result = &off_820CDBF8;            // restore the base vtable slot
//   if ( a2 & 1 ) operator delete(result);
//
// The thunk emits no member teardown, so the destructor body is empty; the compiler
// synthesises the vtable-store + conditional free shown above from this trivial virtual
// ~ServerInterfaceUsersets().
//
// The default constructor brings the inherited base members to their "no error" state
// (matching the sibling component ctors in this directory: ServerInterfaceComponent()
// sets mpcCurrentAction 0 / meStatus 2 / miLastError 0). The remaining lifecycle
// virtuals (Construct / OnEvent) and behavioural methods are owned by their own
// dossiers and are not bodied here; this TU owns only the constructor + destructor so
// the vtable slot is emitted once.

namespace CgsNetwork
{
    ServerInterfaceUsersets::ServerInterfaceUsersets()
        : ServerInterfaceComponent()
    {
    }

    ServerInterfaceUsersets::~ServerInterfaceUsersets()
    {
    }
}
