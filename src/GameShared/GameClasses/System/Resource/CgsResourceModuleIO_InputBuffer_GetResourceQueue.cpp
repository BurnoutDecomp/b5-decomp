#include "types.hpp"

#include "GameShared/GameClasses/System/Resource/CgsResourceModuleIO.h"  // InputBuffer + ResourceRequestQueue
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// THIS TU (class:CgsResource::ResourceIO::InputBuffer) -- the NON-CONST, write-lock-gated
// queue accessor:
//   CgsResource::ResourceIO::InputBuffer::GetResourceQueue() @ 0x82663B38
// (X360 abbreviated symbol "GetR".) Distinct from the CONST read-lock-gated overload, which is
// reconstructed in CgsResourceModuleIO.cpp. This overload hands back a writable queue, so it
// gates on the IOBuffer WRITE lock (status flag bit3, eStatusLockedForWrite) and asserts
// "Not locked for writing"; it then returns the embedded queue (X360 this+4). The asm streams
// the assert message through the StrStream/BasePriorityQueue debug-buffer machinery -- expressed
// here via CGS_ASSERT, which forwards the same Begin/Fire/End assert sequence.

namespace CgsResource
{
    namespace ResourceIO
    {
        // -------- InputBuffer::GetResourceQueue() @ 0x82663B38 --------
        // Write-lock gate ((mxStatusFlags >> 3) & 1), then return the embedded request queue.
        ResourceRequestQueue<16384>* InputBuffer::GetResourceQueue()
        {
            CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");   // CgsResourceModuleIO.h:81
            return &mResourceQueue;
        }
    }
}
