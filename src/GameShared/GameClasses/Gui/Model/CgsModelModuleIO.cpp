#include "GameShared/GameClasses/Gui/Model/CgsModelModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsGui::ModelIO catch-all member functions, reconstructed from BURNOUT_X360_ARTIST.XEX.
// The InputBuffer / OutputBuffer ctors-destructors-appenders are homed by the per-class split
// TUs (CgsModelModuleIO_InputBuffer.cpp / CgsModelModuleIO_OutputBuffer.cpp); this catch-all TU
// (CgsModelModuleIO.cpp) homes the remaining three X360-emitted functions:
//
//   InputBuffer::GetEventQueue() const          @ 0x8284F7F0 -> read-lock  (bit 4), &mGuiEvents (this+4)
//   InputBuffer::GetEventQueueNonConst()        @ 0x8284F898 -> write-lock (bit 3), &mGuiEvents (this+4)
//   OutputBuffer::SetGuiResourceRequestQueue(src)@ 0x8285B370 -> write-lock (bit 3) + null-assert,
//                                                  then mResourceRequestQueue.Append(*src)
//
// X360 store-for-store notes:
//   - GetEventQueue (const): `lbz 0(this); extrwi r11,r11,1,27` -> PPC MSB0 bit 27 == LSB bit 4 ==
//     eStatusLockedForRead. When clear, fire "Not locked for reading\n". Returns `addi r3,this,4`
//     == this+4 == &mGuiEvents -> IsBufferLockedForReading().
//   - GetEventQueueNonConst: `extrwi r11,r11,1,28` -> PPC MSB0 bit 28 == LSB bit 3 ==
//     eStatusLockedForWrite. When clear, fire "Not locked for writing\n". Returns this+4 ==
//     &mGuiEvents -> IsBufferLockedForWriting().
//   - SetGuiResourceRequestQueue: tests the write-lock bit (3) and fires "Not locked for writing\n"
//     when clear, then null-asserts the source ("lpResourceRequestQueue != NULL"), then calls
//     VariableEventQueue<2048,16>::Append<2048,16>(this+4, src) -- i.e. mResourceRequestQueue (the
//     ResourceRequestQueue<2048> at this+4, an IS-A VariableEventQueue<2048,16>) bulk-appends the
//     source queue's events. The X360 returns the Append result in r3; the declared return is void.
//   - The X360-baked CgsModelModuleIO.cpp file/line cites (122/139/218/219) are discarded per
//     project policy; CGS_ASSERT supplies the condition + __FILE__/__LINE__.

namespace CgsGui
{
namespace ModelIO
{
    // X360 0x8284F7F0: read-lock (bit 4) const handle to the GUI-event queue (this+4).
    const InputBuffer::GuiEventInputQueue* InputBuffer::GetEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mGuiEvents;
    }

    // X360 0x8284F898: write-lock (bit 3) handle to the GUI-event queue (this+4).
    InputBuffer::GuiEventInputQueue* InputBuffer::GetEventQueueNonConst()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mGuiEvents;
    }

    // X360 0x8285B370: write-lock (bit 3) + null-assert, then bulk-append the source queue's
    // events into mResourceRequestQueue (VariableEventQueue<2048,16>::Append<2048,16>).
    void OutputBuffer::SetGuiResourceRequestQueue(const GuiResourceRequestQueue* lpResourceRequestQueue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        CGS_ASSERT(lpResourceRequestQueue != nullptr, "lpResourceRequestQueue != NULL");
        mResourceRequestQueue.Append(*lpResourceRequestQueue);
    }
}
}
