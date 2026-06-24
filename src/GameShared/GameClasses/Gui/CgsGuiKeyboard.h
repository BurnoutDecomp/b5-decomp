#pragma once

#include "types.hpp"

// CgsGui::GuiKeyboard — the X360 on-screen (virtual) keyboard front end. Recovered from
// BURNOUT_X360_ARTIST.XEX. The X360 build drives the XDK system keyboard UI
// (XShowKeyboardUI) asynchronously through an embedded CgsXOverlapped and reports the
// result back to a GuiKeyboardListener when the async operation completes.
//
//   Show   @ 0x8284CFD8 — kick off XShowKeyboardUI for a listener.
//   Update @ 0x8284D0C0 — poll the overlapped op; when it finishes, notify the listener.
//
// LAYOUT (X360, asm-authoritative — differs from the PS3 CellOskDialog DWARF):
//   +0x000  maResultBuffer[512]  (CgsUtf16/u16)   the UTF-16 result text buffer
//   +0x400  mOverlapped          (CgsXOverlapped) the async I/O handle (28 bytes)
//   +0x41C  mpListener           (GuiKeyboardListener*) notified on completion
//
// CgsUtf16 is the engine's UTF-16 code unit (u16). The result buffer holds up to 512
// code units (KU_MAX_CHARACTERS) — XShowKeyboardUI is told cchResultText = 512.

namespace CgsGui
{
    typedef u16 CgsUtf16;

    static const u32 KU_KEYBOARD_MAX_CHARACTERS = 512;

    // The completion callback. The X360 build invokes the first vtable slot,
    // KeyboardClosed(resultText), passing the result buffer (or null on error).
    class GuiKeyboardListener
    {
    public:
        virtual void KeyboardClosed(const CgsUtf16* lpResultText) = 0;
    };

    class GuiKeyboard
    {
    public:
        // @ 0x8284CFD8 — assert no dialog is already shown, store the listener, init the
        // overlapped handle, and call XShowKeyboardUI (flags 0x30000064). Returns the
        // XShowKeyboardUI status.
        u32 Show(u32 luUserIndex, const CgsUtf16* lpDefaultText, const CgsUtf16* lpTitleText,
                 const CgsUtf16* lpDescriptionText, GuiKeyboardListener& lListener);

        // @ 0x8284D0C0 — if a dialog is active, poll the overlapped op; while it is still
        // pending (ERROR_IO_PENDING/ERROR_IO_INCOMPLETE) do nothing. Once it completes,
        // clear mpListener and call listener->KeyboardClosed(resultText) — passing the
        // result buffer on success or null when the op errored.
        void Update();

    private:
        CgsUtf16             maResultBuffer[512];  // +0x000
        // CgsXOverlapped (28-byte XOVERLAPPED wrapper, homed in System/X360). Stored as a
        // raw 28-byte block so this header does not depend on the X360-only wrapper class.
        u8                   maOverlapped[28];     // +0x400
        GuiKeyboardListener* mpListener;           // +0x41C
    };
}
