#include "GameShared/GameClasses/Gui/View/CgsGuiViewModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsGui::ViewIO::OutputBuffer accessor, reconstructed from BURNOUT_X360_ARTIST.XEX.
// This TU bodies the one X360-emitted OutputBuffer function in this group:
//
//   GetGuiEventQueue() const @ 0x8284F040 -> &mGuiEvents (+4), read-lock (status bit 4)
//
// The X360 body loads the 1-byte IOBuffer status (lbz r11,0(this)), extracts bit 4 of it
// (extrwi r11,r11,1,27 == 1 bit at MSB-position 27 == the read-lock bit, eStatusLockedForRead
// 0x10), asserts on a clear bit ("Not locked for reading\n"), then returns this + 4. The
// original streamed the d:\p4-baked file/line via CgsDev::Assert; CGS_ASSERT carries the
// stringized condition + __FILE__/__LINE__ (the baked path/line are dropped per policy).

namespace CgsGui
{
namespace ViewIO
{
    void OutputBuffer::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer, mGuiEvents) == 0x0004, "mGuiEvents @0x0004");
    }

    // X360 0x8284F040: read-lock handle to the embedded GUI event queue (this+4).
    const OutputBuffer::GuiEventQueue* OutputBuffer::GetGuiEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mGuiEvents;
    }
}
}
