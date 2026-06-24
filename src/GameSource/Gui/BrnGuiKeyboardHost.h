#pragma once

// ===================================================================================
// BrnGui::KeyboardHost  -- owning header
//   b5-decomp/src/GameSource/Gui/BrnGuiKeyboardHost.h
//
// The GUI module's keyboard "host": a tiny holder that owns the pointer to the active
// on-screen GUI keyboard. GuiModule::Prepare latches the keyboard into the host
// (Prepare) and GuiModule::Release clears it (Release). The X360 asserts the
// "lpGuiKeyboard"/"mpGuiKeyboard" pointer-slot pre/post-conditions
// (Gui/BrnGuiModule.h:739 / :754).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Prepare @ 0x824ED010 -- assert(lpGuiKeyboard != NULL); *(this+4) = lpGuiKeyboard; return 1
//   Release @ 0x824ED078 -- assert(*(this+4) != NULL);     *(this+4) = NULL;          return 1
//
// The only field the reconstructed methods touch is the keyboard pointer at object
// +0x04 (the X360 stw/lwz @4(this)). The leading +0x00 word is a separate construction-
// time slot the host carries but neither method touches; it is named so the keyboard
// pointer lands at its binary offset without raw-offset casting. All access is by name.
// ===================================================================================

#include "types.hpp"

namespace BrnGui
{
    // The on-screen GUI keyboard this host owns a pointer to. Only the pointer slot is
    // referenced by this TU; the keyboard type itself is not modelled here.
    class GuiKeyboard;

    class KeyboardHost
    {
    public:
        // @ 0x824ED010 -- latch the (non-NULL) keyboard pointer. Returns 1 (X360 li r3,1).
        s32 Prepare(GuiKeyboard* lpGuiKeyboard);

        // @ 0x824ED078 -- clear the (must be non-NULL) keyboard pointer. Returns 1.
        s32 Release();

    private:
        // +0x00: a construction-time slot the host carries but neither Prepare nor Release
        // touches. Named so mpGuiKeyboard lands at object +0x04.
        u32          muReservedHead;     // +0x00

        GuiKeyboard* mpGuiKeyboard;      // +0x04 -- set by Prepare, cleared by Release
    };
}
