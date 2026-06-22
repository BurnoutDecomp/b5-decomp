#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceRankings.h"

#include <new>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceRankings::~ServerInterfaceRankings
//       ['scalar deleting destructor' thunk] @ 0x827DE310
//
// MSVC's scalar-deleting-destructor for the polymorphic ServerInterfaceRankings
// component: it restores the shared server-interface-component vtable pointer
// (off_820CDBF8 -- the ServerInterfaceComponent slot this leaf inherits) at this+0
// and, if the low "should-free" flag bit is set, calls operator delete on the object
// before returning `this`:
//
//   *result = &off_820CDBF8;            // restore the base vtable slot
//   if ( a2 & 1 ) operator delete(result);
//
// The component owns no heap members of its own, so the destructor body is empty; the
// compiler synthesises the vtable-store + conditional free shown above from this
// trivial virtual ~ServerInterfaceRankings().
//
// The default constructor brings the base members to their "no error" state (matching
// the sibling component ctors in this directory: ServerInterfaceComponent() sets
// mpcCurrentAction 0 / meStatus 2 / miLastError 0). The remaining lifecycle virtuals
// (Construct / OnEvent) are owned by their own dossiers and are not bodied here; this
// TU owns only the constructor + destructor so the vtable slot is emitted once.

namespace CgsNetwork
{
    ServerInterfaceRankings::ServerInterfaceRankings()
        : mpcCurrentAction(0)
        , meStatus(2)        // ServerInterfaceDirtySock::EStatus "no error"
        , miLastError(0)
    {
    }

    ServerInterfaceRankings::~ServerInterfaceRankings()
    {
    }
}
