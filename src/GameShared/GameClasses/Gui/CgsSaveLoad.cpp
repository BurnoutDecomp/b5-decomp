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
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavSettings.h"   // BrnGui::CrashNavProductCodeKeyboardListener (its real home)
#include "GameShared/GameClasses/Core/CgsAssert.h"         // CGS_ASSERT
// ⛔ DO NOT include GameSource/Gui/Flow/Screen/States/BrnCrashNavMapEvent.h here. See the
// FIX1 note at the CrashNavMapEventKeyboardListener declaration below.

namespace CgsGui
{
    // @ 0x824C0590 (CgsSaveLoad.h:305). Copy the wide-character string lpwSource into the
    // ASCII buffer lpacDest, one byte per character. Two guards fire first, faithful to the
    // X360 asserts:
    //   1. a terminator must appear within luMaxLen wide characters (else "source string is
    //      too long", baked CgsSaveLoad.h:307);
    //   2. every wide character must be 7/8-bit (< 0x100), i.e. representable in one ASCII
    //      byte (else "source string is not ascii", baked CgsSaveLoad.h:308).
    // The copy then writes the low byte of each wide character until the source terminator,
    // and null-terminates the destination. Wide reads are 16-bit (X360 lhz) -- wchar_t is a
    // 16-bit code unit on both the X360 and the PC target.
    void ConvertWideCharToAsciiSafe(char* lpacDest, const wchar_t* lpwSource, size_t luMaxLen)
    {
        // (1) require a terminator within luMaxLen units.
        const wchar_t* lpEnd    = lpwSource + luMaxLen;
        const wchar_t* lpScan   = lpwSource;
        while (lpScan != lpEnd && *lpScan != 0)
            ++lpScan;
        CGS_ASSERT(lpScan != lpEnd, "source string is too long");

        // (2) require every character to be representable as a single ASCII byte (< 0x100).
        bool lbIsAscii = true;
        if (*lpwSource != 0)
        {
            lbIsAscii = false;
            for (const wchar_t* lpCheck = lpwSource; ; )
            {
                const u32 luChar = static_cast<u32>(static_cast<u16>(*lpCheck));
                ++lpCheck;
                if (luChar >= 0x100)
                    break;
                if (*lpCheck == 0)
                {
                    lbIsAscii = true;
                    break;
                }
            }
        }
        CGS_ASSERT(lbIsAscii, "source string is not ascii");

        // Copy the low byte of each character, then null-terminate.
        const wchar_t* lpRead = lpwSource;
        char*          lpWrite = lpacDest;
        while (*lpRead != 0)
        {
            *lpWrite++ = static_cast<char>(*lpRead++);
        }
        *lpWrite = 0;
    }

    // @ 0x8284BC98 (CgsSaveLoad.h:326). The reverse conversion: copy the null-terminated
    // ASCII string lpacSource into the wide-character buffer lpwDest, sign-extending each
    // byte (X360 extsb) to a 16-bit wide character. The single guard requires the source
    // length (excluding the terminator) to be strictly less than luMaxLen (the destination
    // capacity); otherwise it fires "source string is too long" (baked CgsSaveLoad.h:326).
    void ConvertAsciiToWideCharSafe(wchar_t* lpwDest, const char* lpacSource, size_t luMaxLen)
    {
        // Measure the source (length excludes the terminator) and require it to fit.
        const char* lpScan = lpacSource;
        while (*lpScan++ != 0)
            ;
        const size_t luLength = static_cast<size_t>(lpScan - lpacSource - 1);
        CGS_ASSERT(luLength < luMaxLen, "source string is too long");

        // Sign-extend each byte into a wide character, then null-terminate.
        const char* lpRead  = lpacSource;
        wchar_t*    lpWrite = lpwDest;
        while (*lpRead != 0)
        {
            *lpWrite++ = static_cast<wchar_t>(static_cast<signed char>(*lpRead++));
        }
        *lpWrite = 0;
    }
}

namespace BrnGui
{
    // =================================================================================
    // ⭐⭐ FIX1 (2026-08-29): FILE-LOCAL AGAIN, ON PURPOSE. ⚠️ DELETE-WHEN NOTE BELOW.
    //
    // Earlier this wave this declaration was replaced by an #include of the listener's
    // DWARF home, BrnCrashNavMapEvent.h. That was a LIVE ODR FORK, not a de-fork: this
    // TU is MOUNTED (tools/build/build_game_exe.bat), the header drags in the real
    // ~25KB `BrnGui::CrashNavMapEvent : public CrashNavMap`, and the mounted
    // States/BrnScreenStatesLinkStubs.h declares a 56-byte
    // `BrnGui::CrashNavMapEvent : public CgsGui::State` placeholder for the same name --
    // two incompatible definitions in one linked image. The Event TU itself stays
    // UNMOUNTED (six of its methods are still blocked), so the header cannot yet be the
    // single home for anything a mounted TU sees.
    //
    // This TU needs the type for exactly one thing: the KeyboardClosed @0x824C1820 body
    // below, which touches mbKeyboardClosed (+0x24), mbNewData (+0x25) and
    // macKeyboardString (+0x04). Nothing here needs Construct/FillString/HasJustClosed or
    // the owning screen state, so the minimal local shape is declared instead -- the same
    // treatment CrashNavProductCodeKeyboardListener already gets a few lines down. The
    // layout is IDENTICAL to the header's (same DWARF rows h:64/:65/:66), so the two
    // declarations agree byte-for-byte; only the surplus members differ.
    //
    // ⚠️ DELETE WHEN: BrnCrashNavMapEvent.cpp is added to tools/build/build_game_exe.bat
    // and the CrashNavMapEvent placeholder is removed from BrnScreenStatesLinkStubs.{h,cpp}.
    // At that moment -- and NOT before -- delete this struct, restore the
    // `#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMapEvent.h"` at the top, and
    // the header becomes the single definition for real. Mount and de-fork together, or
    // neither.
    //
    // ---- BrnCrashNavMapEvent.h:42 / h:64..:66 (DWARF) ----
    // vptr @0; macKeyboardString[32] @+0x04, mbKeyboardClosed @+0x24, mbNewData @+0x25.
    // Independently confirmed by FillString @0x824B7568, which reads +0x24/+0x25 and
    // returns `this + 4`.
    // =================================================================================
    struct CrashNavMapEventKeyboardListener : public CgsGui::GuiKeyboardListener
    {
        // @ 0x824C1820
        virtual void KeyboardClosed(const CgsGui::CgsUtf16* lpResultText);

        char macKeyboardString[32];   // +0x04 .. +0x23
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
    // ⭐ NO LOCAL RE-DECLARATION ANY MORE (2026-08-29). CrashNavProductCodeKeyboardListener
    // has a real home -- GameSource/Gui/Flow/Screen/States/BrnCrashNavSettings.h, included
    // above -- and that home landed with the CrashNavSettings TU. The copy that used to sit
    // here declared the same BrnGui:: qualified name with its own layout, i.e. an ODR fork
    // that no compile gate can see and only a link (or a layout drift) would ever surface.
    // KI_PRODUCT_CODE_LENGTH = 20; macKeyboardString[20] @+0x04, mbKeyboardClosed @+0x18,
    // mbNewData @+0x19 (vptr @0) -- all still X360-proven, now stated once.

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
