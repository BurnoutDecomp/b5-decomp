#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsGui::GuiResourceModuleIO::OutputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the one X360-emitted OutputBuffer accessor in
// the cgs-gui-resource group:
//
//   GetResourceRequestQueue() const @ 0x8284DEA8 -> &mRequestQueue (this+4), read-lock (bit 4)
//
// X360 @0x8284DEA8 (verified store-for-store):
//   lbz   r11, 0(r28)        ; load the 1-byte IOBuffer status flags
//   extrwi r11, r11, 1,27    ; extract bit 4 (read-lock) -- PPC MSB0 bit 27 == LSB bit 4
//   cmplwi r11, 0
//   bne   ...skip-assert     ; if read-locked, skip
//   <BeginAssert/Clear/FireAssert/EndAssert, msg "Not locked for reading\n",
//    baked file CgsGuiResourceModuleIO.h line 0x139 (313)>
//   addi  r3, r28, 4         ; return this + 4 (== &mRequestQueue)
// The bit-4 (read-lock) test maps to CgsModule::IOBuffer::IsBufferLockedForReading();
// the "Not locked for reading" message is the const handle's read-lock guard. The
// X360-baked file/line are discarded per project convention (CGS_ASSERT supplies them).

namespace CgsGui
{
namespace GuiResourceModuleIO
{
    void OutputBuffer::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer, mRequestQueue) == 0x0004, "mRequestQueue @0x0004");
    }

    // X360 0x8284DEA8: read-lock (bit 4) const handle to the resource-request queue;
    // returns this + 4.
    const OutputBuffer::GuiResourceRequestQueue* OutputBuffer::GetResourceRequestQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mRequestQueue;
    }
}
}
