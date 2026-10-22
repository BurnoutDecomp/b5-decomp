// BrnGuiKeyboardHost.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX. BrnGui::KeyboardHost is the GUI module's
// keyboard holder: Prepare latches the active keyboard pointer, Release clears it. Both
// run a single non-fatal CGS_ASSERT pointer guard (the X360 returns 1 regardless) and
// touch only the keyboard pointer slot. Original diagnostics and continuation are retained.

#include "GameSource/Gui/BrnGuiKeyboardHost.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsSaveLoad.h"  // CgsGui::ConvertWideCharToAsciiSafe
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags

namespace BrnGui
{

// ARTIST825187D4 initializes the real host pointer.
void KeyboardHost::Construct()
{
    mpGuiKeyboard = 0;
}

// ARTIST825077D0 clears the real host pointer.
void KeyboardHost::Destruct()
{
    mpGuiKeyboard = 0;
}

// @ 0x824ED010
//   cmplwi r31(lpGuiKeyboard),0; bne skip; <assert "lpGuiKeyboard">; skip:
//   li r3,1; stw r31,4(r30) ; return 1
bool KeyboardHost::Prepare(BrnGuiKeyboard* lpGuiKeyboard)
{
    if (lpGuiKeyboard == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpGuiKeyboard", "..\\..\\..\\GameSource\\Gui/BrnGuiModule.h", 739);
        CgsDev::Assert::EndAssert();
    }
    mpGuiKeyboard = lpGuiKeyboard;
    return true;
}

// @ 0x824ED078
//   lwz r11,4(r31); cmplwi r11,0; bne skip; <assert "mpGuiKeyboard">; skip:
//   li r11,0; li r3,1; stw r11,4(r31) ; return 1
bool KeyboardHost::Release()
{
    if (mpGuiKeyboard == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpGuiKeyboard", "..\\..\\..\\GameSource\\Gui/BrnGuiModule.h", 754);
        CgsDev::Assert::EndAssert();
    }
    mpGuiKeyboard = 0;
    return true;
}

// The keyboard reported back: a null result is a cancel; otherwise narrow the UTF-16 text
// into a 1024-byte buffer and log it. Both lines are gated on message filter bit 0.
void KeyboardHost::KeyboardClosed(const CgsGui::CgsUtf16* lpResultText)
{
    if (lpResultText == 0)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "GuiKeyboard - cancelled\n";
        }
        return;
    }

    char lacText[1024];
    // CgsUtf16 and the host's wchar_t are the same 16-bit code unit.
    CgsGui::ConvertWideCharToAsciiSafe(lacText, reinterpret_cast<const wchar_t*>(lpResultText), sizeof(lacText));
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "GuiKeyboard - entered text: \"" << lacText << "\"\n";
    }
}

} // namespace BrnGui
