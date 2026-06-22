#include "GameSource/Network/Components/BrnServerInterfaceDownloadableConfig.h"

#include <new>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::BrnServerInterfaceDownloadableConfig
//       ['vector deleting destructor'] @ 0x827DE378
//
// The X360 codegen at 0x827DE378 is MSVC's vector-deleting-destructor for the
// polymorphic downloadable-config component: it restores the component base
// vtable pointer (off_820CDBF8) at this+0 and, if the low "should-free" flag
// bit is set, calls operator delete on the object before returning `this`.
//
//   *result = &off_820CDBF8;            // restore the base vtable slot
//   if ( a2 & 1 ) operator delete(result);
//
// The component carries no owned heap members of its own, so both the ctor and
// dtor bodies are empty; the compiler synthesises the vtable-store + conditional
// free from this trivial virtual ~BrnServerInterfaceDownloadableConfig().

namespace BrnNetwork
{
    BrnServerInterfaceDownloadableConfig::BrnServerInterfaceDownloadableConfig()
    {
    }

    BrnServerInterfaceDownloadableConfig::~BrnServerInterfaceDownloadableConfig()
    {
    }
}
