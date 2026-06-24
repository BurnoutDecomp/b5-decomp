#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsGui::GuiResourceModuleIO::OutputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU (class:CgsGui::GuiResourceModuleIO::OutputBuffer) bodies
// the five X360-emitted OutputBuffer functions NOT already homed in
// CgsGuiResourceModuleIO.cpp (which bodies GetResourceRequestQueue() const @0x8284DEA8):
//
//   GetResourceRequestQueue()         @ 0x8284DF50 -> write-lock (bit 3), &mRequestQueue (this+4)
//   GetLoadNotifications() const      @ 0x8284DFF8 -> read-lock  (bit 4), &mLoadNotifications (this+0x814)
//   AddLoadNotification(ev)           @ 0x8285AA98 -> write-lock (bit 3), mLoadNotifications.AddEvent(ev,14,16)
//   AddUnloadNotification(ev)         @ 0x8285AB50 -> write-lock (bit 3), mLoadNotifications.AddEvent(ev,16,12)
//   AddUnloadRequestNotification(ev)  @ 0x8285AC08 -> write-lock (bit 3), mLoadNotifications.AddEvent(ev,15,12)
//
// X360 store-for-store notes:
//   - Every function reads the 1-byte IOBuffer status (lbz 0(this)) and tests one lock bit:
//       read  accessor (GetLoadNotifications const): extrwi r11,r11,1,27 -> PPC MSB0 bit 27 ==
//         LSB bit 4 == eStatusLockedForRead  ("Not locked for reading\n").
//       write accessors (everything else):           extrwi r11,r11,1,28 -> PPC MSB0 bit 28 ==
//         LSB bit 3 == eStatusLockedForWrite ("Not locked for writing\n").
//     -> CgsModule::IOBuffer::IsBufferLockedForReading()/IsBufferLockedForWriting().
//   - GetResourceRequestQueue() (non-const): `addi r3, this, 4` == this+4 == &mRequestQueue.
//   - GetLoadNotifications() const:          `addi r3, this, 0x814` == this+2068 == &mLoadNotifications.
//   - The three Add* functions all target the SAME member (this+0x814 == &mLoadNotifications)
//     via VariableEventQueue<18432,16>::AddEvent(lpEvent, liType, liSize) and return its result.
//     The (liType, liSize) register loads taken store-for-store from the assembly:
//       AddLoadNotification          @0x8285AB34: li r5,0xE  (14), li r6,0x10 (16)
//       AddUnloadNotification        @0x8285ABEC: li r5,0x10 (16), li r6,0xC  (12)
//       AddUnloadRequestNotification @0x8285ACA4: li r5,0xF  (15), li r6,0xC  (12)
//     (Hex-Rays renders the call as AddEvent(a1+2068, a2, type, size); a1+2068 is the implicit
//     `this` of mLoadNotifications, a2 the event pointer.) mLoadNotifications is the GuiEventQueue
//     == GuiEventQueueBase<18432,16> (is-a VariableEventQueue<18432,16>), so the member call binds
//     exactly to the X360 VariableEventQueue<18432,16>::AddEvent target.
//   - The X360-baked d:\p4 CgsGuiResourceModuleIO.h file/line cites (h:329/346/364/381/397) are
//     discarded per project policy; CGS_ASSERT carries the stringized condition + __FILE__/__LINE__.

namespace CgsGui
{
namespace GuiResourceModuleIO
{
    // NOTE: OutputBuffer::_AssertLayout() (the offset static_asserts for mRequestQueue@+4 /
    // mLoadNotifications@+0x814) is defined ONCE in the pre-existing CgsGuiResourceModuleIO.cpp;
    // it is intentionally NOT re-defined here (two definitions of the same external-linkage
    // static member across TUs would be a multiply-defined symbol at link / LNK2005).

    // X360 0x8284DF50: write-lock (bit 3) non-const handle to the resource-request queue (this+4).
    OutputBuffer::GuiResourceRequestQueue* OutputBuffer::GetResourceRequestQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mRequestQueue;
    }

    // X360 0x8284DFF8: read-lock (bit 4) const handle to the load-notification queue (this+0x814).
    const OutputBuffer::GuiEventQueue* OutputBuffer::GetLoadNotifications() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mLoadNotifications;
    }

    // X360 0x8285AA98: AddEvent type 14, size 16.
    bool OutputBuffer::AddLoadNotification(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return mLoadNotifications.AddEvent(lpEvent, 14, 16);
    }

    // X360 0x8285AB50: AddEvent type 16, size 12.
    bool OutputBuffer::AddUnloadNotification(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return mLoadNotifications.AddEvent(lpEvent, 16, 12);
    }

    // X360 0x8285AC08: AddEvent type 15, size 12.
    bool OutputBuffer::AddUnloadRequestNotification(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return mLoadNotifications.AddEvent(lpEvent, 15, 12);
    }
}
}
