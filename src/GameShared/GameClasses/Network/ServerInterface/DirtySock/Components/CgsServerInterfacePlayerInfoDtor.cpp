#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h"

#include <new>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfacePlayerInfo::~ServerInterfacePlayerInfo
//       ['vector deleting destructor' thunk] @ 0x827DE160
//
// MSVC's vector-deleting-destructor for the polymorphic ServerInterfacePlayerInfo
// component: it restores the shared ServerInterfaceComponent vtable pointer
// (off_820CDBF8 -- the base slot this leaf inherits) at this+0 and, if the low
// "should-free" flag bit is set, calls operator delete on the object before returning
// `this`:
//
//   *result = &off_820CDBF8;            // restore the base vtable slot
//   if ( a2 & 1 ) operator delete(result);
//
// The component owns no heap members of its own (mpFindUser / mpStatbook are torn down in
// Release, not the destructor), so the destructor body is empty; the compiler synthesises
// the vtable-store + conditional free from this trivial virtual ~ServerInterfacePlayerInfo().
// Construct / Prepare / Release / GetPlayerXUIDByName / the lobby callbacks live in the
// sibling CgsServerInterfacePlayerInfo.cpp; this TU owns only the destructor so that its
// vtable slot is emitted exactly once.

namespace CgsNetwork
{
    ServerInterfacePlayerInfo::~ServerInterfacePlayerInfo()
    {
    }
}
