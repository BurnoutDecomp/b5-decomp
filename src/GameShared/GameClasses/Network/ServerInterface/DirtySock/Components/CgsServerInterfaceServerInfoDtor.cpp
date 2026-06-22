#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceServerInfo.h"

#include <new>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceServerInfo::~ServerInterfaceServerInfo
//       ['scalar deleting destructor' thunk] @ 0x827DE238
//
// MSVC's scalar-deleting-destructor for the polymorphic ServerInterfaceServerInfo
// component: it restores the server-interface-component vtable pointer
// (off_820CDBF8 -- the shared ServerInterfaceComponent slot this leaf inherits) at
// this+0 and, if the low "should-free" flag bit is set, calls operator delete on the
// object before returning `this`:
//
//   *result = &off_820CDBF8;            // restore the base vtable slot
//   if ( a2 & 1 ) operator delete(result);
//
// The component owns no heap members of its own, so the destructor body is empty;
// the compiler synthesises the vtable-store + conditional free shown above from this
// trivial virtual ~ServerInterfaceServerInfo().
//
// Construct / Destruct / Prepare / Release / FindUrl / GetTosUrl /
// GetTelemetryAuthString / GetStringFromClientConfig / IsNewsUpdated live in the
// sibling CgsServerInterfaceServerInfo.cpp; this TU owns only the destructor so that
// its vtable slot is emitted exactly once.

namespace CgsNetwork
{
    ServerInterfaceServerInfo::~ServerInterfaceServerInfo()
    {
    }
}
