#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceHttp.h"

#include <new>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceHttp::~ServerInterfaceHttp
//       ['vector deleting destructor' thunk] @ 0x827DE1F0
//
// MSVC's vector-deleting destructor for ServerInterfaceHttp. The asm stores the
// component vtable pointer (off_820CDBF8 -- the same slot the ServerInterfaceComponent
// destructor restores) at this+0 and, when the low "should-free" flag bit is set,
// calls operator delete on the object, returning `this`:
//
//   *result = &off_820CDBF8;
//   if ( a2 & 1 ) operator delete(result);
//
// ServerInterfaceHttp owns no heap members that it frees in this thunk, so the
// destructor body is empty; the compiler synthesises the vtable-store + conditional
// free from this trivial virtual ~ServerInterfaceHttp(). The component's other
// methods (Construct / Prepare / Update / ...) are reconstructed in their own TUs.

namespace CgsNetwork
{
    ServerInterfaceHttp::ServerInterfaceHttp()
        : mpServerInterface(0)
        , meCurrentAction(E_ACTION_DOWNLOADING)
        , mpHttp(0)
        , miAmountHttpDownloaded(0)
        , miHttpBufferSize(0)
        , mpcDownloadBuffer(0)
        , mi16Timeout(0)
    {
    }

    ServerInterfaceHttp::~ServerInterfaceHttp()
    {
    }
}
