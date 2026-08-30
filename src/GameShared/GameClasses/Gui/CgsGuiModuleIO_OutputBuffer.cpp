#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsGui::CgsGuiModuleIO::OutputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the OutputBuffer lifecycle and accessors:
//
//   Construct()                  @ 0x82857420  -> base + all three embedded queues
//   Clear()                      @ 0x82852BF8  -> clear all three embedded queues
//   Destruct()                   ICF @ 0x822DC3D0 -> base IOBuffer teardown
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
    // X360 0x82857420: store the constructed status byte, then construct the resource,
    // GUI-event, and game-action queues at +0x4, +0x814, and +0x5024 respectively.
    void OutputBuffer::Construct()
    {
        CgsModule::IOBuffer::Construct();
        mResourceRequestQueue.CgsModule::VariableEventQueue<2048, 16>::Construct();
        mOutEvents.CgsModule::VariableEventQueue<18432, 16>::Construct();
        mGameActionQueue.CgsModule::VariableEventQueue<13312, 16>::Construct();
    }

    // X360 0x82852BF8: identical member order to Construct, with no status change.
    void OutputBuffer::Clear()
    {
        mResourceRequestQueue.CgsModule::VariableEventQueue<2048, 16>::Clear();
        mOutEvents.CgsModule::VariableEventQueue<18432, 16>::Clear();
        mGameActionQueue.CgsModule::VariableEventQueue<13312, 16>::Clear();
    }

    // The X360 DestroyIOBuffer instantiation calls the ICF representative
    // BrnWorld::PropEntityIO::OutputBuffer_PreScene::Destruct @0x822DC3D0. DecFIGS names
    // this exact fold as OutputBuffer::Destruct and confirms it is base-only.
    void OutputBuffer::Destruct()
    {
        CgsModule::IOBuffer::Destruct();
    }

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

    // X360 0x8285AE20: the GuiModule publication path uses the full-size source
    // queue. Its body is otherwise identical to the small-queue overload above.
    int OutputBuffer::AddGuiOutEvents(const GuiEventQueue* lpSourceQueue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        CGS_ASSERT(lpSourceQueue != 0,
                   "Invalid event queue pointer sent to OutputBuffer::AddGuiOutEvents");
        return mOutEvents.Append(*lpSourceQueue) ? 1 : 0;
    }
}
}
