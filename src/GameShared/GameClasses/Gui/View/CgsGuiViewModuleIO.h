// ============================================================================
// b5-decomp/src/GameShared/GameClasses/Gui/View/CgsGuiViewModuleIO.h
//
// Canonical (DWARF) home for the GUI view module's IO buffers (the X360 cites
// gameshared\gameclasses\gui\view\CgsGuiViewModuleIO.h). This is a MINIMAL slice
// covering only the X360-emitted OutputBuffer accessor owned by this group:
//   CgsGui::ViewIO::OutputBuffer::GetGuiEventQueue() @ 0x8284F040 (read-lock handle)
//
// LAYOUT (authoritative -- pinned by the X360 OutputBuffer bodies):
//   base  CgsModule::IOBuffer                       (1-byte FlagSet status; +1..+3 pad)
//   +4    GuiEventQueue mGuiEvents (VariableEventQueue<18432,16>)
// Proven by:
//   Construct @ 0x82858E40  -- *a1 = 1 (eStatusConstructed), then
//                              CgsModule::VariableEventQueue<18432,16>::Construct(a1 + 4)
//   Destruct  @ 0x82858E58  -- VariableEventQueue<18432,16>::Destruct(a1 + 4) then
//                              IOBuffer::Destruct(a1)
//   GetGuiEventQueue @ 0x8284F040 -- asserts read-lock (status bit 4: lbz + extrwi 1,27),
//                              returns this + 4 (addi r3, r28, 4)
//
// The queue is the same 18432-byte GUI event-queue specialisation the committed
// CgsGuiModuleIO.h / CgsGuiResourceModuleIO.h homes settled on (GuiEventQueueBase<18432,16>
// == VariableEventQueue<18432,16>); the X360 Construct/Destruct targets prove the size here.
#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"  // CgsModule::IOBuffer (1-byte FlagSet base)
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"      // CgsGui::GuiEventQueueBase<N,16>

namespace CgsGui
{
namespace ViewIO
{
    // The view->output GUI event buffer. Derives CgsModule::IOBuffer (the 1-byte status
    // FlagSet: bit 3 = locked-for-write, bit 4 = locked-for-read) and embeds a single GUI
    // event queue at this+4 that the view module fills (write lock) and the GUI module
    // bridges out of (read lock, via GuiModule::BridgeFromViewToOutput).
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        // GuiEventQueue == GuiEventQueueBase<18432,16> (X360 Construct/Destruct @0x82858E40/E58).
        typedef CgsGui::GuiEventQueueBase<18432, 16> GuiEventQueue;

        // X360 0x82858E40 / 0x82858E58 -- declared here for the member surface; bodied in
        // their own homes (the per-TU compile gate does not link).
        void Construct();
        void Destruct();

        // X360 0x8284F040. Asserts this buffer is locked-for-reading (status bit 4), then
        // returns the read handle to the embedded event queue at this+4.
        const GuiEventQueue* GetGuiEventQueue() const;

        // Byte-offset pin (the embedded queue lands at this+4: 1-byte IOBuffer base + 3 pad,
        // VariableEventQueue alignof == 4).
        static void _AssertLayout();

    private:
        u8            maStatusPad[3]; // +1..+3 (force the queue to +4 like the X360)
        GuiEventQueue mGuiEvents;     // +4 (DWARF CgsGuiViewModuleIO.h)
    };
}
}
