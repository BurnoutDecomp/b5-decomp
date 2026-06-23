#include "GameShared/GameClasses/Gui/Model/CgsEventInterpreterModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsGui::EventInterpreterModuleIO::InputBuffer::AddGuiEvents, reconstructed from
// BURNOUT_X360_ARTIST.XEX @ 0x8285B720.
//
// X360 0x8285B720 body (store-for-store):
//   1. assert the buffer is write-locked -- tests ((*this >> 3) & 1), IOBuffer bit 3
//      (eStatusLockedForWrite) -- message "Not locked for writing\n".
//   2. bulk-append the source GUI event queue into mGuiEvents (this+4) via
//      VariableEventQueue<32768,16>::Append<32768,16>(const VariableEventQueue<32768,16>&)
//      and return its result.
// The assert forwards through CGS_ASSERT (Begin/Fire/EndAssert); the X360-baked d:\p4
// file path / line number (83) is dropped per project policy.

namespace CgsGui
{
namespace EventInterpreterModuleIO
{
    int InputBuffer::AddGuiEvents(const GuiEventInputQueue& lrEventQueue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return mGuiEvents.Append(lrEventQueue) ? 1 : 0;
    }
}
}
