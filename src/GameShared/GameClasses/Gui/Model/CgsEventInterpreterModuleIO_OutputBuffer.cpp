#include "GameShared/GameClasses/Gui/Model/CgsEventInterpreterModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsGui::EventInterpreterModuleIO::OutputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU (class:CgsGui::EventInterpreterModuleIO::OutputBuffer)
// bodies the five X360-emitted OutputBuffer lock-checked handle accessors:
//
//   GetOutEventQueue() const      @ 0x8284E480 -> read-lock  (bit 4), &mGuiOutEvents   (this+4)
//   GetOutEventQueue()            @ 0x8284E528 -> write-lock (bit 3), &mGuiOutEvents   (this+4)
//   GetResourceEventQueue() const @ 0x8284E5D0 -> read-lock  (bit 4), &mGuiOutResource (this+0x4814 / 18452)
//   GetViewEventQueue() const     @ 0x8284E720 -> read-lock  (bit 4), &mGuiOutView      (this+0x5824 / 22564)
//   GetViewEventQueue()           @ 0x8284E7C8 -> write-lock (bit 3), &mGuiOutView      (this+0x5824 / 22564)
//
// (The non-const GetResourceEventQueue() write-lock overload is declared-only in the header;
// the X360 build does not emit it -- n_funcs == 5, matching the five bodies here.)
//
// X360 store-for-store notes (all five share the recurring IOBuffer lock-guard prologue):
//   - Each reads the 1-byte IOBuffer status (lbz 0(this)) and tests a single lock bit:
//       read  accessors: extrwi r11,r11,1,27 -> PPC MSB0 bit 27 == LSB bit 4 ==
//                        eStatusLockedForRead  ("Not locked for reading\n")
//       write accessors: extrwi r11,r11,1,28 -> PPC MSB0 bit 28 == LSB bit 3 ==
//                        eStatusLockedForWrite ("Not locked for writing\n")
//     -> CgsModule::IOBuffer::IsBufferLockedForReading()/IsBufferLockedForWriting().
//   - Each then returns a fixed member address:
//       0x8284E480 / 0x8284E528 : `addi r3, this, 4`      == this+4      == &mGuiOutEvents
//       0x8284E5D0              : `addi r3, this, 0x4814` == this+18452  == &mGuiOutResource
//       0x8284E720 / 0x8284E7C8 : `addi r3, this, 0x5824` == this+22564  == &mGuiOutView
//   - The X360-baked d:\p4 CgsEventInterpreterModuleIO.h file/line cites (h:146/162/178/210/226)
//     are discarded per project policy; CGS_ASSERT carries the stringized condition +
//     __FILE__/__LINE__.
//
// *** FLAG (committed-type size drift on mGuiOutEvents) ***
//   The X360 return-offsets prove mGuiOutResource starts at this+0x4814 (18452) and mGuiOutView
//   at this+0x5824 (22564). mGuiOutResource - mGuiOutEvents == 18452 - 4 == 18448, so the first
//   member mGuiOutEvents has sizeof 18448 -> its inline buffer is 18432 bytes
//   (round_up(1+18432,4) + 12 == 18448). The committed CgsEventInterpreterModuleIO.h currently
//   types mGuiOutEvents as VariableEventQueue<16384,16> (sizeof 16400), which would place
//   mGuiOutResource at this+16404 (0x4014) -- contradicting the X360 0x4814. The correct X360
//   type is VariableEventQueue<18432,16>, the same 18432-vs-16384 GUI-queue drift already
//   resolved (with a FLAG) in the committed CgsGuiModuleIO.h and CgsGuiResourceModuleIO.h. That
//   is a non-additive retype of a committed member, so it is NOT applied here; it is flagged for
//   the consolidator. The getters below return the queues member-by-name (semantically exact);
//   only the +4 offset of mGuiOutEvents is pinned (correct under either size). The +0x4814 /
//   +0x5824 offsets become exact once mGuiOutEvents is retyped to <18432,16>.

namespace CgsGui
{
namespace EventInterpreterModuleIO
{
    void OutputBuffer::_AssertLayout()
    {
        // mGuiOutEvents is the first member after the 1-byte IOBuffer base (+3 pad;
        // VariableEventQueue alignof == 4) -> +4, correct independent of its BUFSIZE.
        static_assert(offsetof(OutputBuffer, mGuiOutEvents) == 0x0004, "mGuiOutEvents @0x0004");
    }

    // X360 0x8284E480: read-lock (bit 4) const handle to the out-event queue (this+4).
    const OutputBuffer::GuiEventQueue* OutputBuffer::GetOutEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mGuiOutEvents;
    }

    // X360 0x8284E528: write-lock (bit 3) non-const handle to the out-event queue (this+4).
    OutputBuffer::GuiEventQueue* OutputBuffer::GetOutEventQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mGuiOutEvents;
    }

    // X360 0x8284E5D0: read-lock (bit 4) const handle to the resource-event queue
    // (this+0x4814 == 18452; exact once mGuiOutEvents is retyped to <18432,16> -- see FLAG).
    const OutputBuffer::GuiEventQueueSmall* OutputBuffer::GetResourceEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mGuiOutResource;
    }

    // X360 0x8284E720: read-lock (bit 4) const handle to the view-event queue
    // (this+0x5824 == 22564; exact once mGuiOutEvents is retyped to <18432,16> -- see FLAG).
    const OutputBuffer::GuiEventQueueLarge* OutputBuffer::GetViewEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mGuiOutView;
    }

    // X360 0x8284E7C8: write-lock (bit 3) non-const handle to the view-event queue
    // (this+0x5824 == 22564; exact once mGuiOutEvents is retyped to <18432,16> -- see FLAG).
    OutputBuffer::GuiEventQueueLarge* OutputBuffer::GetViewEventQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mGuiOutView;
    }
}
}
