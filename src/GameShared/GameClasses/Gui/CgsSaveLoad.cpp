// CgsSaveLoad.cpp
// Home for the two on-screen-keyboard listener completion handlers the X360 ARTIST build
// folds into this TU because they call CgsGui::ConvertWideCharToAsciiSafe (declared in
// CgsSaveLoad.h):
//
//   BrnGui::CrashNavMapEventKeyboardListener::KeyboardClosed     @ 0x824C1820
//   BrnGui::CrashNavProductCodeKeyboardListener::KeyboardClosed  @ 0x824C1850
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX. Both listeners derive from
// CgsGui::GuiKeyboardListener (vptr @0); only the members each KeyboardClosed touches are
// modelled here, at their X360-proven store displacements. The owning CrashNav screen
// states (BrnCrashNavMapEvent / BrnCrashNavSettings) are out of this TU's scope -- these
// listeners are small standalone helper classes whose only in-scope member is the
// KeyboardClosed override.

#include "GameShared/GameClasses/Gui/CgsSaveLoad.h"        // CgsGui::ConvertWideCharToAsciiSafe
#include "GameShared/GameClasses/Gui/CgsGuiKeyboard.h"     // CgsGui::GuiKeyboardListener, CgsUtf16

namespace BrnGui
{
    // ---- BrnCrashNavMapEvent.h:64..66 ----
    // macKeyboardString[32] @+0x04, mbKeyboardClosed @+0x24, mbNewData @+0x25
    // (vptr @0; the keyboard result text is converted straight into macKeyboardString).
    struct CrashNavMapEventKeyboardListener : public CgsGui::GuiKeyboardListener
    {
        // @ 0x824C1820
        virtual void KeyboardClosed(const CgsGui::CgsUtf16* lpResultText);

        char macKeyboardString[32];   // +0x04 .. 0x23
        bool mbKeyboardClosed;        // +0x24
        bool mbNewData;               // +0x25
    };

    // @ 0x824C1820 -- mark the dialog closed; if a result was returned, flag new data and
    // copy it (up to 32 chars) into macKeyboardString. With no result, just clear mbNewData.
    void CrashNavMapEventKeyboardListener::KeyboardClosed(const CgsGui::CgsUtf16* lpResultText)
    {
        mbKeyboardClosed = true;                       // stb 1, 0x24
        if (lpResultText != nullptr)
        {
            mbNewData = true;                          // stb 1, 0x25
            CgsGui::ConvertWideCharToAsciiSafe(
                macKeyboardString,
                reinterpret_cast<const wchar_t*>(lpResultText),
                32);
        }
        else
        {
            mbNewData = false;                         // stb 0, 0x25
        }
    }

    // ---- BrnCrashNavSettings.h:65..69 ----
    // KI_PRODUCT_CODE_LENGTH = 20; macKeyboardString[20] @+0x04, mbKeyboardClosed @+0x18,
    // mbNewData @+0x19 (vptr @0).
    struct CrashNavProductCodeKeyboardListener : public CgsGui::GuiKeyboardListener
    {
        static const s32 KI_PRODUCT_CODE_LENGTH = 20;  // BrnCrashNavSettings.h:65

        // @ 0x824C1850
        virtual void KeyboardClosed(const CgsGui::CgsUtf16* lpResultText);

        char macKeyboardString[KI_PRODUCT_CODE_LENGTH];  // +0x04 .. 0x17
        bool mbKeyboardClosed;                           // +0x18
        bool mbNewData;                                  // +0x19
    };

    // @ 0x824C1850 -- mark the dialog closed; if a result was returned, scan the first 20
    // UTF-16 code units for a terminator. Only when a terminator is found within those 20
    // units (the code fits) is new data flagged and converted (20 chars) into
    // macKeyboardString; otherwise mbNewData stays clear and no conversion happens.
    void CrashNavProductCodeKeyboardListener::KeyboardClosed(const CgsGui::CgsUtf16* lpResultText)
    {
        mbKeyboardClosed = true;                       // stb 1, 0x18
        if (lpResultText != nullptr)
        {
            mbNewData = false;                         // stb 0, 0x19 (cleared before the scan)

            const CgsGui::CgsUtf16* lpCursor = lpResultText;
            const CgsGui::CgsUtf16* lpEnd     = lpResultText + KI_PRODUCT_CODE_LENGTH;
            while (lpCursor != lpEnd)
            {
                if (*lpCursor == 0)
                    break;
                ++lpCursor;
            }

            if (lpCursor != lpEnd)
            {
                mbNewData = true;                      // stb 1, 0x19
                CgsGui::ConvertWideCharToAsciiSafe(
                    macKeyboardString,
                    reinterpret_cast<const wchar_t*>(lpResultText),
                    KI_PRODUCT_CODE_LENGTH);
            }
        }
        else
        {
            mbNewData = false;                         // stb 0, 0x19
        }
    }
}
