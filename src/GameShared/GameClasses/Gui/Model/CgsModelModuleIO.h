#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer (1-byte FlagSet base)
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"       // GuiEventQueueBase<N,16>, GuiEventQueueSmall, GuiEventLoadRequest

// CgsGui::ModelIO - the per-frame IO buffers the GUI model module exchanges with the
// view/loader. Layout + method set recovered from the DecFIGS DWARF
// (CgsModelModuleIO.h:36-91) grounded against the X360 ARTIST binary. This slice homes
// the InputBuffer (the two ledger TUs below); OutputBuffer is forward-declared only.
//
// InputBuffer (DWARF CgsModelModuleIO.h:47) : public CgsModule::IOBuffer
//   mGuiEvents     [+0x0004]  GuiEventInputQueue (GuiEventQueueBase<32768,16>)   (h:90)
//   mLoadRequests  [+0x8014]  GuiEventQueueSmall (GuiEventQueueBase<4096,16>)    (h:91)
// The embedded mGuiEvents queue lands at this+4 (1-byte IOBuffer base + 3 pad,
// VariableEventQueue alignof == 4); mLoadRequests follows at +32788 (0x8014), matching the
// X360 `this + 32788` loads in AddResourceRequests (@0x8250C658) / GetLoadRequests
// (@0x824F7490). sizeof(GuiEventQueueBase<32768,16>) == 32784 (bool + 32768-byte buffer +
// three s32 bookkeeping words), so 4 + 32784 == 32788.
namespace CgsGui
{
namespace ModelIO
{
    struct OutputBuffer; // homed by its own TUs

    // CgsModelModuleIO.h:47 (DWARF).
    struct InputBuffer : public CgsModule::IOBuffer
    {
        // CgsModelModuleIO.h:90 + CgsGuiEvent.h GuiEventInputQueue typedef.
        typedef CgsGui::GuiEventQueueBase<32768, 16> GuiEventInputQueue;
        // CgsModelModuleIO.h:91 -- the small load-request queue.
        typedef CgsGui::GuiEventQueueSmall           GuiEventQueueSmall;

        // CgsModelModuleIO.h:79 (DWARF). X360 0x8250C658: asserts this buffer is
        // locked-for-writing (status bit 3, "Not locked for writing\n"), then pushes the
        // request onto mLoadRequests via VariableEventQueue<4096,16>::AddEvent(&request,
        // /*type*/39, /*size*/24). Returns the AddEvent result.
        bool AddResourceRequests(const GuiEventLoadRequest& lrRequest);

        // CgsModelModuleIO.h:87 (DWARF, non-const overload). X360 0x824F7490: asserts this
        // buffer is locked-for-writing (status bit 3, "Not locked for writing\n"), returns
        // the handle to the load-request queue at this+32788 (&mLoadRequests).
        GuiEventQueueSmall* GetLoadRequests();

        // CgsModelModuleIO.h:83 (DWARF, const overload). Declared for completeness (read-lock
        // handle); homed elsewhere -- not attested in this batch.
        const GuiEventQueueSmall* GetLoadRequests() const;

        // Byte-offset pin (mLoadRequests at this+32788).
        static void _AssertLayout();

    private:
        u8                 maStatusPad[3]; // +0x0001..+0x0003 (force mGuiEvents to +0x0004 like the X360)
        GuiEventInputQueue mGuiEvents;     // +0x0004  (DWARF CgsModelModuleIO.h:90)
        GuiEventQueueSmall mLoadRequests;  // +0x8014  (DWARF CgsModelModuleIO.h:91)
    };
}
}
