#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsGui::GuiResourceModuleIO::InputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the X360-emitted InputBuffer functions in
// the cgs-gui-resource group:
//
//   AddResourceRequests(const GuiEventQueueSmall*) @ 0x8285DA58 -> mLoadRequests.Append(*src), write-lock (bit 3)
//   GetLoadRequests() const                        @ 0x8284DE00 -> read-lock (bit 4), &mLoadRequests (this+4)
//     (linked by name from GuiResourceModule::ProcessIncomingLoadRequests @0x828581A8;
//      the body is the same read-lock accessor form as the sibling OutputBuffer
//      GetResourceRequestQueue() const @0x8284DEA8.)
//   Construct/Destruct: the X360 constructs this buffer transiently per frame through
//     IOBufferStack::CreateIOBuffer<InputBuffer> @0x8285A3D8 / DestroyIOBuffer @0x8285A678;
//     the bodies follow the attested OutputBuffer::Construct @0x828582A8 pattern (status
//     constructed-bit, then Construct + Clear the embedded queue).
//   GetLoadRequestsNonConst() is ADDITIVE [PC] (see the header note): the persistent PC
//     buffer needs a write-side handle to Clear the consumed queue each frame.
//
// X360 @0x8285DA58 (verified store-for-store against the assembly):
//   lbz    r11, 0(r23)       ; load the 1-byte IOBuffer status flags (a1)
//   extrwi r11, r11, 1,28    ; extract bit 3 (write-lock) -- PPC MSB0 bit 28 == LSB bit 3
//   cmplwi r11, 0
//   bne    ...skip           ; if write-locked, skip first assert
//   <BeginAssert/Clear/FireAssert/EndAssert, msg "Not locked for writing\n",
//    baked file CgsGuiResourceModuleIO.h line 0x118 (280)>
//   cmplwi r22, 0            ; null-test the source queue ptr (a2)
//   bne    ...skip           ; if non-null, skip second assert
//   <BeginAssert/Clear/FireAssert/EndAssert,
//    msg "Invalid queue sent to OutputBuffer::AddResourceRequests",
//    baked file CgsGuiResourceModuleIO.h line 0x119 (281)>
//   addi   r3, r23, 4        ; this + 4 (== &mLoadRequests)
//   mr     r4, r22           ; the source queue
//   bl     VariableEventQueue<18432,16>::Append<4096,16>(const VariableEventQueue<4096,16>&)
//   <return its result>
//
// The bit-3 (write-lock) test maps to CgsModule::IOBuffer::IsBufferLockedForWriting().
// The Append target symbol proves the embedded mLoadRequests queue is the 18432-byte
// specialisation (see the FLAG in CgsGuiResourceModuleIO.h); the source is the small
// GuiEventQueueBase<4096,16>. mLoadRequests is a GuiEventQueueBase<18432,16> (is-a
// VariableEventQueue<18432,16>) and the source is a GuiEventQueueBase<4096,16> (is-a
// VariableEventQueue<4096,16>), so mLoadRequests.Append(*src) binds exactly to the
// VariableEventQueue<18432,16>::Append<4096,16> the X360 calls. The X360-baked file/line
// are discarded per project convention. (The "OutputBuffer::AddResourceRequests" wording
// in the second assert string is the original's copy-pasted message text -- reproduced
// verbatim, as it is X360 rodata.)

namespace CgsGui
{
namespace GuiResourceModuleIO
{
    void InputBuffer::_AssertLayout()
    {
        static_assert(offsetof(InputBuffer, mLoadRequests) == 0x0004, "mLoadRequests @0x0004");
    }

    // X360 0x8285DA58.
    bool InputBuffer::AddResourceRequests(const GuiEventQueueSmall* lpRequests)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        CGS_ASSERT(lpRequests != 0, "Invalid queue sent to OutputBuffer::AddResourceRequests");
        return mLoadRequests.Append(*lpRequests);
    }

    // X360 0x8284DE00: read-lock (bit 4) const handle to the load-request queue (this+4).
    const InputBuffer::GuiEventQueue* InputBuffer::GetLoadRequests() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mLoadRequests;
    }

    // ADDITIVE [PC] (header note): write-lock handle for the persistent-buffer drain-clear.
    InputBuffer::GuiEventQueue* InputBuffer::GetLoadRequestsNonConst()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mLoadRequests;
    }

    // Per the transient CreateIOBuffer<InputBuffer> @0x8285A3D8 lifecycle (the attested
    // sibling OutputBuffer::Construct @0x828582A8 pattern): constructed-bit, queue up, clear.
    void InputBuffer::Construct()
    {
        CgsModule::IOBuffer::Construct();
        mLoadRequests.Construct();
        mLoadRequests.Clear();
    }

    void InputBuffer::Destruct()
    {
        CgsModule::IOBuffer::Destruct();
    }
}
}
