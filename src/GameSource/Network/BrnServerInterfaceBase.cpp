#include "GameSource/Network/BrnServerInterfaceBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::BrnServerInterfaceBase::`scalar deleting destructor' @ 0x827E1710
//
// The X360 codegen at 0x827E1710 is MSVC's scalar-deleting destructor for the
// 11-component server-interface aggregate. As the destructor chain unwinds it
// reinstalls each embedded component sub-object's vtable pointer
// (off_820CDBF8 -- the shared CgsNetwork::ServerInterfaceComponent base vtable)
// at that sub-object's slot, reinstalls this class's own primary vtable
// (off_820CDBD0) at this+0, and finally -- on the delete-expression path
// (flag & 1) -- frees the object:
//
//   result[746] = &off_820CDBF8;   // 0xBA8  mServerInterfaceDebugComponent / trailing
//   result[738] = &off_820CDBF8;   // 0xB88
//   result[727] = &off_820CDBF8;   // 0xB5C
//   result[511] = &off_820CDBF8;   // 0x7FC
//   result[450] = &off_820CDBF8;   // 0x708
//   result[443] = &off_820CDBF8;   // 0x6EC
//   result[434] = &off_820CDBF8;   // 0x6C8
//   result[423] = &off_820CDBF8;   // 0x69C
//   result[ 67] = &off_820CDBF8;   // 0x10C
//   result[ 56] = &off_820CDBF8;   // 0x0E0
//   result[ 48] = &off_820CDBF8;   // 0x0C0
//   *result     =  off_820CDBD0;   // primary vtable
//   if ( flag & 1 ) operator delete(result);
//   return result;
//
// Each `&off_820CDBF8` store is the by-value member dtor of one of the eleven
// embedded components walking back to the shared ServerInterfaceComponent base
// vtable as it tears down; the eleven slots are mConnection / mPlayerInfo /
// mBroadcastMessages / mHttp / mServerInfo / mDownloadableConfig / mTelemetry /
// mRankings / mCustomCommands / mServerInterfaceDebugComponent (plus the base
// ServerInterface sub-object whose own vtable is off_820CDBD0). The compiler
// synthesises that whole vtable walk + the conditional free from this trivial
// out-of-line virtual destructor; only the (empty) body is hand-written, matching
// the established deleting-destructor convention (see CgsServerInterfaceDirtySock.cpp
// and CgsServerInterfaceComponentDtor.cpp). Defining the destructor out-of-line here
// also anchors this class's vtable (off_820CDBD0) to this TU.
//
// The X360 +0xC0.. member offsets are 32-bit-pointer layout facts; they are NOT
// reproduced or static_asserted on a 64-bit host (the vptr + pointers widen to 8
// bytes there, so the embedded component slots land at different byte offsets while
// the by-name member walk -- and the codegen the compiler emits -- is identical).

namespace BrnNetwork
{
    BrnServerInterfaceBase::~BrnServerInterfaceBase()
    {
    }
}
