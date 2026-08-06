#include "GameSource/Network/BrnServerInterface.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::BrnServerInterface::`scalar deleting destructor' @ 0x827E4090
//
// The X360 codegen at 0x827E4090 is MSVC's scalar-deleting destructor for the
// most-derived server-interface aggregate. It is the BrnServerInterfaceBase
// deleting destructor (@ 0x827E1710) with exactly ONE extra embedded-component
// vtable restore prepended at +0xC2C (mGames, the one component the X360 platform
// intermediate BrnServerInterfaceX360 adds over the Base -- this most-derived class
// adds no data of its own); the destructor then walks the eleven Base component
// restores, reinstalls
// the (shared) Base primary vtable (off_820CDBD0) at this+0, and -- on the
// delete-expression path (flag & 1) -- frees the object:
//
//   result[779] = &off_820CDBF8;   // 0xC2C  BrnServerInterfaceX360::mGames
//   result[746] = &off_820CDBF8;   // 0xBA8 ]
//   result[738] = &off_820CDBF8;   // 0xB88 ]
//   result[727] = &off_820CDBF8;   // 0xB5C ]
//   result[511] = &off_820CDBF8;   // 0x7FC ]
//   result[450] = &off_820CDBF8;   // 0x708 ]  identical to the Base dtor walk
//   result[443] = &off_820CDBF8;   // 0x6EC ]
//   result[434] = &off_820CDBF8;   // 0x6C8 ]
//   result[423] = &off_820CDBF8;   // 0x69C ]
//   result[ 67] = &off_820CDBF8;   // 0x10C ]
//   result[ 56] = &off_820CDBF8;   // 0x0E0 ]
//   result[ 48] = &off_820CDBF8;   // 0x0C0 ]
//   *result     =  off_820CDBD0;   // (shared) Base primary vtable
//   if ( flag & 1 ) operator delete(result);
//   return result;
//
// Each `&off_820CDBF8` store is the by-value member dtor of one embedded component
// walking back to the shared CgsNetwork::ServerInterfaceComponent base vtable as it
// tears down. The +0xC2C store is BrnServerInterfaceX360::mGames -- a by-value
// CgsNetwork::ServerInterfaceGamesX360, pinned by BrnNetworkManager's ctor
// (0x827E557C `stw off_820CFBF8, 0xC2C(r29)` installs the games leaf vtable there);
// being the last-declared member of the last base it is destroyed first. The eleven
// that follow are the BrnServerInterfaceBase components, and
// *result = off_820CDBD0 is the Base sub-object's own vtable
// reinstall. MSVC synthesises that whole vtable walk + the conditional free from
// this trivial out-of-line virtual destructor; only the (empty) body is
// hand-written, matching the established deleting-destructor convention (see
// BrnServerInterfaceBase.cpp and BrnServerInterfaceDownloadableConfig.cpp).
// Defining the destructor out-of-line here also anchors this class's
// deleting-destructor thunk to this TU.
//
// The X360 +0xC2C / +0xBA8.. member offsets are 32-bit-pointer layout facts; they
// are NOT reproduced or static_asserted on a 64-bit host (the vptr + pointers widen
// to 8 bytes there, so the embedded component slots land at different byte offsets
// while the by-name member walk -- and the codegen the compiler emits -- is
// identical).

namespace BrnNetwork
{
    BrnServerInterface::BrnServerInterface()
    {
    }

    BrnServerInterface::~BrnServerInterface()
    {
    }
}
