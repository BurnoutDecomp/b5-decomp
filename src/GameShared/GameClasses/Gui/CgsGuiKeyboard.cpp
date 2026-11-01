#include "GameShared/GameClasses/Gui/CgsGuiKeyboard.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   CgsGui::GuiKeyboard::Show   @ 0x8284CFD8
//   CgsGui::GuiKeyboard::Update @ 0x8284D0C0

// Win32/XDK winerror.h literals consumed by Update's pending check.
#ifndef ERROR_IO_INCOMPLETE
#define ERROR_IO_INCOMPLETE 996u
#endif
#ifndef ERROR_IO_PENDING
#define ERROR_IO_PENDING    997u
#endif

// XShowKeyboardUI flags baked into the X360 call (lis 0x3000 | ori 0x64).
static const u32 KU_KEYBOARD_FLAGS = 0x30000064u;

// CgsXOverlapped has one native data member and no bases or virtual functions;
// the size check also requires that member to begin at offset zero.
CGS_STATIC_ASSERT((sizeof(CgsSystem::CgsXOverlapped) == sizeof(XOVERLAPPED)),
              "keyboard wrapper begins with and exactly contains the native overlapped member");

namespace CgsGui
{
    void GuiKeyboard::Construct()
    {
        mpListener = 0;
    }

    // Inlined in ARTIST824F5B20..B28, with the DecFIGS const declaration.
    bool GuiKeyboard::IsShown() const
    {
        return mpListener != 0;
    }

    // @ 0x8284CFD8
    u32 GuiKeyboard::Show(u32 luUserIndex, const CgsUtf16* lpDefaultText,
                          const CgsUtf16* lpTitleText, const CgsUtf16* lpDescriptionText,
                          GuiKeyboardListener& lListener)
    {
        // X360: assert no dialog already active (mpListener at +0x41C is null), at :205.
        CGS_ASSERT(mpListener == 0, "GuiKeyboard::Show: already being shown.");

        mpListener = &lListener;

        mOverlapped.Construct();

        return XShowKeyboardUI(luUserIndex, KU_KEYBOARD_FLAGS,
                               reinterpret_cast<LPCWSTR>(lpDefaultText),
                               reinterpret_cast<LPCWSTR>(lpTitleText),
                               reinterpret_cast<LPCWSTR>(lpDescriptionText),
                               reinterpret_cast<LPWSTR>(maResultBuffer), KU_KEYBOARD_MAX_CHARACTERS,
                               reinterpret_cast<PXOVERLAPPED>(&mOverlapped));
    }

    // @ 0x8284D0C0
    void GuiKeyboard::Update()
    {
        if (mpListener == 0)
        {
            return;
        }

        // Poll without waiting. While the op is still pending, leave it running.
        const u32 luResult = XGetOverlappedResult(reinterpret_cast<PXOVERLAPPED>(&mOverlapped), 0, 0);
        const bool lbStillPending =
            (luResult == ERROR_IO_PENDING) || (luResult == ERROR_IO_INCOMPLETE);
        if (lbStillPending)
        {
            return;
        }

        // The op has completed (success or error). Detach the listener first, then
        // re-query: a non-zero code means the op errored, so hand the listener null;
        // ERROR_SUCCESS (0) hands it the populated result buffer.
        GuiKeyboardListener* lpListener = mpListener;
        mpListener = 0;

        const u32 luFinal = XGetOverlappedResult(reinterpret_cast<PXOVERLAPPED>(&mOverlapped), 0, 0);
        const CgsUtf16* lpResultText = (luFinal != 0) ? 0 : maResultBuffer;
        lpListener->KeyboardClosed(lpResultText);
    }
}
