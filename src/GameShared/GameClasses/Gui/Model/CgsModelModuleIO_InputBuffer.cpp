#include "GameShared/GameClasses/Gui/Model/CgsModelModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsGui::ModelIO::InputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU (class:CgsGui::ModelIO::InputBuffer) bodies the two
// X360-emitted InputBuffer functions:
//
//   AddResourceRequests(const GuiEventLoadRequest&) @ 0x8250C658
//       -> write-lock guard, then mLoadRequests.AddEvent(&request, 39, 24)
//   GetLoadRequests() (non-const)                   @ 0x824F7490
//       -> write-lock guard, then return &mLoadRequests (this + 32788)
//
// X360 store-for-store notes:
//   - Both test IOBuffer status bit 3 (lbz 0(this); extrwi r11,r11,1,28 -> PPC MSB0 bit 28
//     == LSB bit 3, the locked-for-write bit) and, when clear, fire the "Not locked for
//     writing\n" assert. This maps to CgsModule::IOBuffer::IsBufferLockedForWriting().
//   - AddResourceRequests: `addis r3,this,1; addi r3,r3,-0x7FEC` == this + 0x8014 (32788) ==
//     &mLoadRequests; `li r5,0x27`(39, the event type) `li r6,0x18`(24, the record size);
//     r4 is the request. The call target is VariableEventQueue<4096,16>::AddEvent, so it is
//     mLoadRequests.AddEvent(&request, 39, 24).
//   - GetLoadRequests: returns `this + 0x8014` == &mLoadRequests.
//   - The X360-baked CgsModelModuleIO.h file/line (182/214) are discarded per project
//     convention; the assert string is X360 rodata, reproduced verbatim.

namespace CgsGui
{
namespace ModelIO
{
    void InputBuffer::_AssertLayout()
    {
        static_assert(offsetof(InputBuffer, mGuiEvents)    == 0x0004, "mGuiEvents @ +0x0004");
        static_assert(offsetof(InputBuffer, mLoadRequests) == 0x8014, "mLoadRequests @ +0x8014 (32788)");
    }

    // X360 0x8250C658. Event-type id 39 and record size 24 are the X360 AddEvent literals.
    bool InputBuffer::AddResourceRequests(const GuiEventLoadRequest& lrRequest)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return mLoadRequests.AddEvent(static_cast<const CgsModule::Event*>(&lrRequest), 39, 24);
    }

    // X360 0x824F7490 (non-const overload -- write-lock guarded).
    InputBuffer::GuiEventQueueSmall* InputBuffer::GetLoadRequests()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mLoadRequests;
    }
}
}
