#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"

#include <new>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceComponent::~ServerInterfaceComponent
//       ['vector deleting destructor' thunk] @ 0x827DB3E8
//
// The X360 codegen at 0x827DB3E8 is MSVC's vector-deleting-destructor for the
// polymorphic ServerInterfaceComponent base: it stores the component vtable
// pointer (off_820CDBF8) at this+0 and, if the low "should-free" flag bit is set,
// calls operator delete on the object before returning `this`.
//
//   *result = &off_820CDBF8;            // restore the base vtable slot
//   if ( a2 & 1 ) operator delete(result);
//
// The component carries no owned heap members of its own, so the destructor body
// is empty; the compiler synthesises the vtable-store + conditional free shown
// above from this trivial virtual ~ServerInterfaceComponent().
//
// The bodies of Construct / ClearLastError / GetAndClearLastError / ConvertError
// live in the sibling CgsServerInterfaceComponent.cpp; this TU owns only the
// destructor so that its vtable slot is emitted exactly once.

namespace CgsNetwork
{
    ServerInterfaceComponent::ServerInterfaceComponent()
        : mpcCurrentAction(0)
        , meStatus(2)        // ServerInterfaceDirtySock::EStatus "no error"
        , miLastError(0)
    {
    }

    ServerInterfaceComponent::~ServerInterfaceComponent()
    {
    }
}
