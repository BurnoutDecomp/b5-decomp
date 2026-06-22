// Embed check for BrnNetwork::BrnServerInterfaceDownloadableConfig.
// Exercises the bodied ctor + virtual (vector deleting) destructor @ 0x827DE378
// through both the no-free and the heap-delete paths so the synthesised
// deleting-destructor thunk is referenced. Compile-only.
#include "GameSource/Network/Components/BrnServerInterfaceDownloadableConfig.h"

#include <new>

namespace
{
    void exercise()
    {
        // Stack instance: trivial destructor path (no operator delete).
        BrnNetwork::BrnServerInterfaceDownloadableConfig stackInstance;
        (void)stackInstance;

        // Heap instance through the component base pointer: drives the virtual
        // deleting destructor (vptr restore + conditional operator delete).
        CgsNetwork::ServerInterfaceComponent* lpComponent =
            new BrnNetwork::BrnServerInterfaceDownloadableConfig();
        delete lpComponent;
    }
}
