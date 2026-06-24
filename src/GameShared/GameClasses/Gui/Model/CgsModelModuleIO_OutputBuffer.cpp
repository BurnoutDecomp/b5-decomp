#include "GameShared/GameClasses/Gui/Model/CgsModelModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsGui::ModelIO::OutputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU (class:CgsGui::ModelIO::OutputBuffer) bodies the seven
// X360-emitted OutputBuffer functions -- four lock-checked handle accessors and three bulk
// appenders:
//
//   GetGuiResourceRequestQueue() const @ 0x8284F940 -> read-lock  (bit 4), &mGuiResourceRequests (this+4)
//   GetGuiOutEvents() const            @ 0x823B1590 -> read-lock  (bit 4), &mGuiOutEvents        (this+0x814)
//   GetViewOutEvents() const           @ 0x8284E148 -> read-lock  (bit 4), &mViewOutEvents       (this+0x5024)
//   GetLoadNotifications() const       @ 0x824F7538 -> read-lock  (bit 4), &mLoadNotifications   (this+0x15034)
//   AddGuiOutEvents(src)               @ 0x8285ACC0 -> write-lock (bit 3), mGuiOutEvents.Append<18432,16>(src)
//   AddViewOutEvents(src)              @ 0x8285AD70 -> write-lock (bit 3), mViewOutEvents.Append<65536,16>(src)
//   AddLoadNotifications(src)          @ 0x8285DB70 -> write-lock (bit 3), mLoadNotifications.Append<18432,16>(src)
//
// X360 store-for-store notes:
//   - Every function reads the 1-byte IOBuffer status (lbz 0(this)) and tests one lock bit:
//       read  accessors:  extrwi r11,r11,1,27 -> PPC MSB0 bit 27 == LSB bit 4 ==
//                         eStatusLockedForRead  ("Not locked for reading\n").
//       write appenders:  extrwi r11,r11,1,28 -> PPC MSB0 bit 28 == LSB bit 3 ==
//                         eStatusLockedForWrite ("Not locked for writing\n").
//     -> CgsModule::IOBuffer::IsBufferLockedForReading()/IsBufferLockedForWriting().
//   - Accessor return-offsets:
//       0x8284F940 : `addi r3, this, 4`                       == this+4      == &mGuiResourceRequests
//       0x823B1590 : `addi r3, this, 0x814`                   == this+2068   == &mGuiOutEvents
//       0x8284E148 : `addi r3, this, 0x5024`                  == this+20516  == &mViewOutEvents
//       0x824F7538 : `addis r3,this,1; addi r3,r3,0x5034`     == this+0x15034 == this+86068 == &mLoadNotifications
//   - Appenders target the same members via VariableEventQueue::Append, return its result:
//       0x8285ACC0 : `addi r3,this,0x814`               -> VariableEventQueue<18432,16>::Append<18432,16>
//       0x8285AD70 : `addi r3,this,0x5024`              -> VariableEventQueue<65536,16>::Append<65536,16>
//       0x8285DB70 : `addis r3,this,1; addi r3,r3,0x5034` -> VariableEventQueue<4096,16>::Append<18432,16>
//     (mLoadNotifications is the small 4096 queue; the appended source is an 18432-byte queue,
//     hence the <4096,16>::Append<18432,16> instantiation -- the member type & the argument type
//     pick the exact X360 template.) The X360 returns the bool result in r3 (an int), so each
//     wrapper returns Append(...) ? 1 : 0, matching the sibling AddGuiEvents recovery.
//   - The X360-baked d:\p4 CgsModelModuleIO.h/.cpp file/line cites (h:248/282/315, cpp:235,
//     h:232/265/299) are discarded per project policy; CGS_ASSERT supplies the condition +
//     __FILE__/__LINE__.

namespace CgsGui
{
namespace ModelIO
{
    void OutputBuffer::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer, mGuiResourceRequests) == 0x00004, "mGuiResourceRequests @0x0004");
        static_assert(offsetof(OutputBuffer, mGuiOutEvents)        == 0x00814, "mGuiOutEvents @0x0814 (2068)");
        static_assert(offsetof(OutputBuffer, mViewOutEvents)       == 0x05024, "mViewOutEvents @0x5024 (20516)");
        static_assert(offsetof(OutputBuffer, mLoadNotifications)   == 0x15034, "mLoadNotifications @0x15034 (86068)");
    }

    // X360 0x8284F940: read-lock (bit 4) const handle to the GUI resource-request queue (this+4).
    const OutputBuffer::GuiResourceRequestQueue* OutputBuffer::GetGuiResourceRequestQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mGuiResourceRequests;
    }

    // X360 0x823B1590: read-lock (bit 4) const handle to the GUI out-event queue (this+0x814).
    const OutputBuffer::GuiEventQueue* OutputBuffer::GetGuiOutEvents() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mGuiOutEvents;
    }

    // X360 0x8284E148: read-lock (bit 4) const handle to the view-event queue (this+0x5024).
    const OutputBuffer::GuiViewEventQueue* OutputBuffer::GetViewOutEvents() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mViewOutEvents;
    }

    // X360 0x824F7538: read-lock (bit 4) const handle to the load-notification queue (this+0x15034).
    const OutputBuffer::GuiNotificationQueue* OutputBuffer::GetLoadNotifications() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mLoadNotifications;
    }

    // X360 0x8285ACC0: bulk-append into mGuiOutEvents (VariableEventQueue<18432,16>::Append<18432,16>).
    int OutputBuffer::AddGuiOutEvents(const GuiEventQueue& lrSource)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return mGuiOutEvents.Append(lrSource) ? 1 : 0;
    }

    // X360 0x8285AD70: bulk-append into mViewOutEvents (VariableEventQueue<65536,16>::Append<65536,16>).
    int OutputBuffer::AddViewOutEvents(const GuiViewEventQueue& lrSource)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return mViewOutEvents.Append(lrSource) ? 1 : 0;
    }

    // X360 0x8285DB70: bulk-append an 18432-byte source into mLoadNotifications
    // (VariableEventQueue<4096,16>::Append<18432,16>).
    int OutputBuffer::AddLoadNotifications(const GuiEventQueue& lrSource)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return mLoadNotifications.Append(lrSource) ? 1 : 0;
    }
}
}
