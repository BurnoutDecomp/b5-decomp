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

    // X360 0x8285B448-adjacent read twin (original CgsGuiModuleIO.cpp:92): read-lock (bit 4)
    // handle to the inbound GUI event queue at this+4. The consumer side of the pair --
    // GuiModule::Update / BridgeFromInputToView walk it via GetFirstEvent/GetNextEvent.
    const InputBuffer::GuiEventInputQueue* InputBuffer::GetGuiEvents() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mInputQueue;
    }

    // X360 0x8284F238 (original CgsGuiModuleIO.cpp:110): write-lock (bit 3) handle to the
    // inbound GUI event queue at this+4. The producer side -- every
    // GuiModule::AddGuiEvent<T> instantiation fetches the queue through this before its
    // AddEvent push ("Not locked for writing\n").
    InputBuffer::GuiEventInputQueue* InputBuffer::GetGuiEvents()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mInputQueue;
    }
}
}
