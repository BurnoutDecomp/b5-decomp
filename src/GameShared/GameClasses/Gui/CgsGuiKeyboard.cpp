#include "GameShared/GameClasses/Gui/CgsGuiKeyboard.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   CgsGui::GuiKeyboard::Show   @ 0x8284CFD8
//   CgsGui::GuiKeyboard::Update @ 0x8284D0C0

// ---- XDK entry points (real prototypes live in the Xbox 360 XDK) -----------------
extern "C" u32 XShowKeyboardUI(u32 dwUserIndex, u32 dwFlags, const u16* wszDefaultText,
                               const u16* wszTitleText, const u16* wszDescriptionText,
                               u16* wszResultText, u32 cchResultText, void* pOverlapped);
extern "C" u32 XGetOverlappedResult(void* lpOverlapped, u32* lpdwResult, u32 bWait);

// Win32/XDK winerror.h literals consumed by Update's pending check.
#ifndef ERROR_IO_INCOMPLETE
#define ERROR_IO_INCOMPLETE 996u
#endif
#ifndef ERROR_IO_PENDING
#define ERROR_IO_PENDING    997u
#endif

// XShowKeyboardUI flags baked into the X360 call (lis 0x3000 | ori 0x64).
static const u32 KU_KEYBOARD_FLAGS = 0x30000064u;

namespace CgsSystem
{
    // The 28-byte XOVERLAPPED wrapper. Construct() (defined out-of-line in
    // System/X360/CgsXOverlappedX360.cpp) zero-fills the embedded XOVERLAPPED. Declared
    // here (matching that TU's local declaration) only so GuiKeyboard can name Construct;
    // the definition is resolved at link time.
    class CgsXOverlapped
    {
    public:
        void Construct();
    private:
        u8 maOverlapped[28];
    };
}

namespace CgsGui
{
    // @ 0x8284CFD8
    u32 GuiKeyboard::Show(u32 luUserIndex, const CgsUtf16* lpDefaultText,
                          const CgsUtf16* lpTitleText, const CgsUtf16* lpDescriptionText,
                          GuiKeyboardListener& lListener)
    {
        // X360: assert no dialog already active (mpListener at +0x41C is null), at :205.
        CGS_ASSERT(mpListener == nullptr, "GuiKeyboard::Show: already being shown.");

        mpListener = &lListener;

        CgsSystem::CgsXOverlapped* lpOverlapped =
            reinterpret_cast<CgsSystem::CgsXOverlapped*>(maOverlapped);
        lpOverlapped->Construct();

        return XShowKeyboardUI(luUserIndex, KU_KEYBOARD_FLAGS, lpDefaultText, lpTitleText,
                               lpDescriptionText, maResultBuffer, KU_KEYBOARD_MAX_CHARACTERS,
                               maOverlapped);
    }

    // @ 0x8284D0C0
    void GuiKeyboard::Update()
    {
        if (mpListener == nullptr)
        {
            return;
        }

        // Poll without waiting. While the op is still pending, leave it running.
        const u32 luResult = XGetOverlappedResult(maOverlapped, nullptr, 0);
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
        mpListener = nullptr;

        const u32 luFinal = XGetOverlappedResult(maOverlapped, nullptr, 0);
        const CgsUtf16* lpResultText = (luFinal != 0) ? nullptr : maResultBuffer;
        lpListener->KeyboardClosed(lpResultText);
    }
}
