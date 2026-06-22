#include "CgsServerInterfaceDirtySock.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceDirtySock::GetMessageBuffer            @ 0x82580B48
//   CgsNetwork::ServerInterfaceDirtySock::`vector deleting destructor'@ 0x827DB3A0
//
// ---------------------------------------------------------------------------
// GetMessageBuffer @ 0x82580B48
//   lwz   r11, 0x94(this)          ; r11 = mpacMessageBuffer
//   if ( mpacMessageBuffer == 0 )  ; cmplwi/bne
//   {
//       BeginAssert();
//       FireAssert("mpacMessageBuffer",
//                  "..\\..\\..\\GameShared\\GameClasses\\Network/ServerInterface"
//                  "\\DirtySock\\CgsServerInterfaceDirtySock.h", 719);
//       EndAssert();
//   }
//   return mpacMessageBuffer;       ; lwz r3, 0x94(this)
//
// The const accessor asserts the 2 KB message buffer has been allocated, then
// returns it. Member pinned BY NAME (mpacMessageBuffer); the +0x94 X360 offset is
// not reproduced/asserted on a 64-bit host.
//
// ---------------------------------------------------------------------------
// `vector deleting destructor' @ 0x827DB3A0
//   stw  off_820CDBD0, 0(this)     ; reinstall this class's vptr as teardown
//   if ( (flag & 1) != 0 ) operator delete(this);
//   return this;
//
// MSVC's compiler-emitted deleting-destructor thunk for ServerInterfaceDirtySock:
// it reinstalls the vtable pointer (off_820CDBD0) as the dtor chain unwinds to this
// sub-object, then conditionally frees on the delete-expression path. That thunk
// half is compiler codegen, not source, and is regenerated automatically from the
// out-of-line virtual destructor declared below -- so only the (empty) destructor
// body is hand-written, matching the established deleting-destructor convention
// (see CgsServerInterfaceStructureInterface.cpp). Defining the destructor here also
// anchors the vtable emission to this TU, mirroring where the X360 build placed
// off_820CDBD0.
// ---------------------------------------------------------------------------

namespace CgsNetwork
{
    ServerInterfaceDirtySock::~ServerInterfaceDirtySock()
    {
    }

    char* ServerInterfaceDirtySock::GetMessageBuffer() const
    {
        CGS_ASSERT(mpacMessageBuffer != nullptr, "mpacMessageBuffer");
        return mpacMessageBuffer;
    }
}
