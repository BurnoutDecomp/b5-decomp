#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsGui::CgsGuiModuleIO::OutputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the 3 X360-emitted OutputBuffer functions:
//
//   GetGuiResourceRequestQueue() @ 0x8284F388  -> &mResourceRequestQueue (+4),  write-lock (bit 3)
//   GetOutEventQueue() const     @ 0x823B4130  -> &mOutEvents (+2068),          read-lock  (bit 4)
//   AddGuiOutEvents()            @ 0x8250C718  -> mOutEvents.Append(*src),       write-lock (bit 3)
//
// Each accessor first checks the IOBuffer lock-state flag and asserts on violation exactly as
// the X360 bodies do (the original streams the file/line via CgsDev::Assert; CGS_ASSERT carries
// the stringized condition + __FILE__/__LINE__). The X360 asserts use bit 3 (write) for
// GetGuiResourceRequestQueue/AddGuiOutEvents and bit 4 (read) for GetOutEventQueue -- matching
// CgsModule::IOBuffer::IsBufferLockedForWriting()/IsBufferLockedForReading().

namespace CgsGui
{
namespace CgsGuiModuleIO
{
    void OutputBuffer::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer, mResourceRequestQueue) == 0x0004, "mResourceRequestQueue @0x0004");
        static_assert(offsetof(OutputBuffer, mOutEvents)            == 0x0814, "mOutEvents @0x0814");
    }

    // X360 0x8284F388: write-lock handle to the resource-request queue. The pseudocode tests
    // ((*a1 >> 3) & 1) == write-lock bit; returns this + 4.
    OutputBuffer::GuiResourceRequestQueueStorage* OutputBuffer::GetGuiResourceRequestQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mResourceRequestQueue;
    }

    // X360 0x823B4130: read-lock handle to the out-event queue. Tests ((*a1 >> 4) & 1) ==
    // read-lock bit; returns this + 2068.
    const OutputBuffer::GuiEventQueue* OutputBuffer::GetOutEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mOutEvents;
    }

    // X360 0x8250C718: write-lock; asserts the source queue ptr is non-null, then bulk-appends
    // it into mOutEvents (VariableEventQueue<18432,16>::Append<4096,16>). The X360 emits both
    // asserts before the Append call, then returns the Append result.
    int OutputBuffer::AddGuiOutEvents(const GuiEventQueueSmall* lpSourceQueue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        CGS_ASSERT(lpSourceQueue != 0,
                   "Invalid event queue pointer sent to OutputBuffer::AddGuiOutEvents");
        return mOutEvents.Append(*lpSourceQueue) ? 1 : 0;
    }
}
}
