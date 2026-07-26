// CgsGui::SaveLoadSystem -- wave-C fan-out partfile 01 (message / card lane).
// X360 ARTIST reconstruction of the two remaining Realmc result callbacks:
//   CardRemoved  (0x8284CC20), ClearMessage (0x8284CAD8).
// Bodies only -- the layout and every declaration are frozen in the headers.
//
// `this` convention: both are Realmc RESULT callbacks, so Hex-Rays shows them the +4
// interface sub-object and every displacement in the pseudocode is (real member offset - 4)
// -- e.g. raw 0x17C == mbField180 (+0x180), raw 0x194 == mMessageDisplayOptionFunc (+0x198).
// They are written here as plain member functions over the NAMED members; no this+/-4
// arithmetic and no raw offset casts appear anywhere below.
//
// Include-order rule (load-bearing): CgsSaveLoadX360.h FIRST -- it forward-declares
// ::SystemUserProfile / ::LanguageManager at GLOBAL scope; pulling a CgsGui/CgsLanguage
// header in first would flip the member types inside namespace CgsGui (ODR hazard).
#include "GameShared/GameClasses/Gui/CgsSaveLoadX360.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"            // CgsUnicode::ConvertUtf8ToUtf16
#include "GameShared/GameClasses/Language/CgsLanguageManager.h" // CgsLanguage::LanguageManager::FindString
#include "GameShared/GameClasses/System/CgsXOverlapped.h"       // CgsSystem::CgsXOverlapped::Construct

namespace
{
    // Localisation keys the card-removed prompt looks up. X360 rodata pointers (both are
    // entries of the maRealmemcardStringIDs table LocaleGetStrCallback indexes).
    const char* const KSTR_DEVICE_REMOVED_OR_CHANGED = "SAVELOAD_DEVICE_REMOVED_OR_CHANGED"; // off_82F33210
    const char* const KSTR_CONTINUE_WITHOUT_SAVING   = "SAVELOAD_CONTINUE_WITHOUT_SAVING";   // off_82F331C0

    // Xbox 360 XDK C-API system message box (precedent: the anon-namespace XShowSigninUI
    // declaration in CgsSaveLoadX360_wB_06.cpp). NINE parameters: the ninth -- the
    // XOVERLAPPED* that makes the call asynchronous -- is passed on the stack, so Hex-Rays
    // drops it from the pseudocode; the asm `stw r30,var_1FC(r1)` with r30 == &maOverlapped[0]
    // proves it is there.
    u32 XShowMessageBoxUI(u32 luUserIndex, const u16* lpwTitle, const u16* lpwText,
                          u32 luNumButtons, const u16** lppwButtons, u32 luFocusButton,
                          u32 luFlags, u32* lpuResult, void* lpOverlapped);
}

namespace CgsGui
{
    // X360 0x8284CC20. Realmc card-removed callback: disarm autosave, mark the overlapped op
    // in flight, stage the localised "device removed or changed" strings into UTF-16 stack
    // buffers and pop the system message box with a single CONTINUE-WITHOUT-SAVING button.
    // The call is expected to complete asynchronously (ERROR_IO_PENDING).
    void SaveLoadSystem::CardRemoved()
    {
        mbField180     = false;   // stb r10(0), raw 0x17C -> real 0x180
        mbAsyncOpState = 1;       // stb r9(1),  raw 0x220 -> real 0x224

        // Stack buffer sizes are the asm's: title 64 bytes, button 128 bytes, text 288 bytes.
        u16 laTitleText[32];      // var_1E0 (v6)
        u16 laMessageText[144];   // var_120 (v8)
        u16 laButtonText[64];     // var_1A0 (v7)

        CgsUnicode::ConvertUtf8ToUtf16(
            reinterpret_cast<const CgsUnicode::CgsUtf8*>("Warning"), laTitleText);

        // raw 0x184 -> real 0x188 (mpLanguageManager)
        const u8* lpMessageText = reinterpret_cast<CgsLanguage::LanguageManager*>(mpLanguageManager)
                                      ->FindString(KSTR_DEVICE_REMOVED_OR_CHANGED);
        CgsUnicode::ConvertUtf8ToUtf16(lpMessageText, laMessageText);

        const u8* lpButtonText = reinterpret_cast<CgsLanguage::LanguageManager*>(mpLanguageManager)
                                     ->FindString(KSTR_CONTINUE_WITHOUT_SAVING);
        CgsUnicode::ConvertUtf8ToUtf16(lpButtonText, laButtonText);

        const u16* lapwButtons[1] = { laButtonText };   // var_1F0 (v5)

        // raw 0x224 -> real 0x228: the embedded XOVERLAPPED gets zeroed before the async call.
        reinterpret_cast<CgsSystem::CgsXOverlapped*>(maOverlapped)->Construct();

        const u32 luResult = XShowMessageBoxUI(
            static_cast<u32>(miField210),                  // dwUserIndex, raw 0x20C -> real 0x210
            laTitleText,                                   // wszTitle
            laMessageText,                                 // wszText
            1,                                             // cButtons
            lapwButtons,                                   // pwszButtons
            0,                                             // dwFocusButton
            2,                                             // dwFlags
            reinterpret_cast<u32*>(&maOverlapped[0x1C]),   // pResult, raw 0x240 -> real 0x244
            maOverlapped);                                 // the stack-passed XOVERLAPPED*

        CGS_ASSERT(luResult == ERROR_IO_PENDING, "lResult == ERROR_IO_PENDING");
    }

    // X360 0x8284CAD8. If a pending message-display option member function is armed, hide the
    // prompt and clear the { func, delta } pair (the X360 zeroes both words with one 8-byte std).
    void SaveLoadSystem::ClearMessage()
    {
        // The test reads ONLY the func word (`lwz r11,0x194` -> real 0x198).
        if (mMessageDisplayOptionFunc.muFunc != 0)
        {
            // raw 0x130 -> real 0x134 (mpMessageDisplay), vtable slot +0x8 == HideMessage.
            mpMessageDisplay->HideMessage();

            mMessageDisplayOptionFunc.muFunc  = 0;
            mMessageDisplayOptionFunc.muDelta = 0;
        }
    }
}
