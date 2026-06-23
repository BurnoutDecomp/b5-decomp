#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsGui::CgsGuiModuleIO::OutputBuffer::SetGuiResourceRequestQueue, reconstructed from
// BURNOUT_X360_ARTIST.XEX @ 0x8285AFB0. The OutputBuffer's other X360-emitted accessors
// (GetGuiResourceRequestQueue / GetOutEventQueue / AddGuiOutEvents) live in the sibling
// CgsGuiModuleIO_OutputBuffer.cpp; this TU bodies the resource-request setter.
//
// X360 0x8285AFB0 body (store-for-store):
//   1. assert the incoming source-queue pointer is non-null ("lpRequestQueue != NULL").
//   2. assert the buffer is write-locked  -- tests ((*this >> 3) & 1), IOBuffer bit 3
//      (eStatusLockedForWrite) -- message "Not locked for writing\n".
//   3. bulk-append the source queue into mResourceRequestQueue (this+4) via
//      VariableEventQueue<2048,16>::Append<2048,16>(const VariableEventQueue<2048,16>&)
//      and return its result.
// The two asserts forward through CGS_ASSERT (Begin/Fire/EndAssert); the X360-baked
// d:\p4 file path / line numbers (181, 182) are dropped per project policy -- CGS_ASSERT
// carries the stringized condition + __FILE__/__LINE__.

namespace CgsGui
{
namespace CgsGuiModuleIO
{
    int OutputBuffer::SetGuiResourceRequestQueue(const GuiResourceRequestQueueStorage* lpRequestQueue)
    {
        CGS_ASSERT(lpRequestQueue != 0, "lpRequestQueue != NULL");
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return mResourceRequestQueue.Append(*lpRequestQueue) ? 1 : 0;
    }
}
}
