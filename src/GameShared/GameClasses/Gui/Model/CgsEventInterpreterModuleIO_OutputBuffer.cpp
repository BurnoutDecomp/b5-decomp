#include "GameShared/GameClasses/Gui/Model/CgsEventInterpreterModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsGui::EventInterpreterModuleIO::OutputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU (class:CgsGui::EventInterpreterModuleIO::OutputBuffer)
// homes the original OutputBuffer lifecycle and six lock-checked accessors:
//
//   GetOutEventQueue() const      @ 0x8284E480 -> read-lock  (bit 4), &mGuiOutEvents   (this+4)
//   GetOutEventQueue()            @ 0x8284E528 -> write-lock (bit 3), &mGuiOutEvents   (this+4)
//   GetResourceEventQueue() const @ 0x8284E5D0 -> read-lock  (bit 4), &mGuiOutResource (this+0x4814 / 18452)
//   GetViewEventQueue() const     @ 0x8284E720 -> read-lock  (bit 4), &mGuiOutView      (this+0x5824 / 22564)
//   GetViewEventQueue()           @ 0x8284E7C8 -> write-lock (bit 3), &mGuiOutView      (this+0x5824 / 22564)
//
// The resource write accessor at 0x8284E678 is recovered from raw ARTIST bytes;
// it was absent from the function exports. Lifecycle operations are inlined in
// CreateIOBuffer<OutputBuffer> 0x8285A1C8 and DestroyIOBuffer 0x8285A2E0.
//
// X360 store-for-store notes (all six share the recurring IOBuffer lock-guard prologue):
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
// The original first queue contains 18432 payload bytes. Both the getter
// return addresses and CreateIOBuffer's 0x15834 allocation attest that capacity.

namespace CgsGui
{
namespace EventInterpreterModuleIO
{
    // Inlined at ARTIST 0x8285A284..0x8285A2C4.
    void OutputBuffer::Construct()
    {
        CgsModule::IOBuffer::Construct();
        mGuiOutEvents.Construct();
        mGuiOutResource.Construct();
        mGuiOutView.Construct();
        mGuiOutEvents.Clear();
        mGuiOutResource.Clear();
        mGuiOutView.Clear();
    }

    // Inlined at ARTIST 0x8285A380..0x8285A3A0.
    void OutputBuffer::Destruct()
    {
        mGuiOutEvents.Destruct();
        mGuiOutResource.Destruct();
        mGuiOutView.Destruct();
        CgsModule::IOBuffer::Destruct();
    }

    void OutputBuffer::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer, mGuiOutEvents) == 0x0004, "mGuiOutEvents @0x0004");
        static_assert(offsetof(OutputBuffer, mGuiOutResource) == 0x4814, "mGuiOutResource @0x4814");
        static_assert(offsetof(OutputBuffer, mGuiOutView) == 0x5824, "mGuiOutView @0x5824");
        static_assert(sizeof(OutputBuffer) == 0x15834, "ARTIST interpreter output size");
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
    // (this+0x4814 == 18452).
    const OutputBuffer::GuiEventQueueSmall* OutputBuffer::GetResourceEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mGuiOutResource;
    }

    // ARTIST raw bytes 0x8284E678: write-lock handle to the resource queue.
    OutputBuffer::GuiEventQueueSmall* OutputBuffer::GetResourceEventQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mGuiOutResource;
    }

    // X360 0x8284E720: read-lock (bit 4) const handle to the view-event queue
    // (this+0x5824 == 22564).
    const OutputBuffer::GuiEventQueueLarge* OutputBuffer::GetViewEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mGuiOutView;
    }

    // X360 0x8284E7C8: write-lock (bit 3) non-const handle to the view-event queue
    // (this+0x5824 == 22564).
    OutputBuffer::GuiEventQueueLarge* OutputBuffer::GetViewEventQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mGuiOutView;
    }
}
}
