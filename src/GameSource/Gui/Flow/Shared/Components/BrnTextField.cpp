#include "GameSource/Gui/BrnGuiTextField.h"

#include <cstring>                                                    // std::memset / std::strlen
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                  // CgsGui::GuiAccessPointers (mpLanguageManager @+4)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // CgsGui::StateInterface (GetAccessPointers)

// BrnGui::TextField -- the out-of-line bodies for the committed BrnGuiTextField.h
// surface. Reconstructed from BURNOUT_X360_ARTIST.XEX (DWARF primary file
// GameSource/Gui/Flow/Shared/Components/BrnTextField.cpp).
//
// Bodied here (4 ledger functions):
//   TextField::Construct        @0x824E4FA8
//   TextField::SetDatabaseText  @0x824E5020
//   TextField::OutputAptData    @0x824E52B8
//   TextField::SetLocalisedText @0x824E7418
// (SetText @0x824E7240 / SetColour @0x82481E48 / operator= @0x824470F0 are their
// own ledger functions -- not part of this TU's export set.)

namespace BrnGui
{

// Resolve the interface's language manager the way the X360 inlines it: read the
// access-pointer block (guarded by the CgsGuiStateInterface.h:344 tripwire) and
// take its +4 manager pointer.
static CgsLanguage::LanguageManager* TextFieldGetLanguageManager(CgsGui::StateInterface* lpStateInterface)
{
    CgsGui::GuiAccessPointers* lpAccessPointers = lpStateInterface->GetAccessPointers();
    CGS_ASSERT(lpAccessPointers != 0, "mpAccessPointers != NULL");   // CgsGuiStateInterface.h:344 (non-gating)
    return lpAccessPointers->mpLanguageManager;
}

// @ 0x824E4FA8 -- base component Construct, wipe the text/colour state (default
// colour string = "%u" of the zeroed colour word). Store order per the asm.
void TextField::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                          const char* lpacParentName)
{
    CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);
    muTextColour = 0;
    std::memset(macText, 0, KU_MAX_TEXTFIELD_LEN);
    CgsCore::SPrintf(macColour, KU_MAX_COLOUR_LEN - 1, "%u", muTextColour);
    macColour[KU_MAX_COLOUR_LEN - 1] = 0;
    mbUseColour   = false;
    miScroll      = 0;
    mbResetScroll = false;
    mbAutosize    = false;
}

// @ 0x824E5020 -- cpp:102. Adopt already-resolved text. Over-long strings are
// registered in the localisation database under this component's name (the field
// then displays the "$<name>" database key); everything else is a bounded copy
// (CgsCore::StrCpy carries the CgsStringUtils.h:65 length tripwire the X360
// inlines here).
void TextField::SetDatabaseText(const char* lpcActualText)
{
    CGS_ASSERT(lpcActualText != 0, "Invalid Text sent to SetDatabaseText");   // :102 (streamed on the X360; folded static)

    if (std::strlen(lpcActualText) >= KU_MAX_TEXTFIELD_LEN)
    {
        CgsCore::SPrintf(macText, KU_MAX_TEXTFIELD_LEN, "$%s", macName);
        macText[KU_MAX_TEXTFIELD_LEN - 1] = 0;
        CgsLanguage::LanguageManager* lpLanguageManager =
            TextFieldGetLanguageManager(mpStateInterface);
        lpLanguageManager->AddString(macName, reinterpret_cast<const u8*>(lpcActualText));
    }
    else
    {
        CgsCore::StrCpy(macText, KU_MAX_TEXTFIELD_LEN, lpcActualText);
    }
}

// @ 0x824E52B8 -- push the field's current contents to its bound apt clip: the
// text always; colour / reset-scroll as consumed one-shot latches; autosize as a
// level (NOT cleared); the scroll cursor as "%d" (consumed) or "0".
void TextField::OutputAptData()
{
    AddOutputAptViewState("apt_text", macText, false);

    if (mbUseColour)
    {
        AddOutputAptViewState("apt_colour", macColour, false);
        AddOutputAptViewState("apt_useColour", "1", false);
        mbUseColour = false;
    }
    if (mbResetScroll)
    {
        AddOutputAptViewState("apt_resetScroll", "1", false);
        mbResetScroll = false;
    }
    if (mbAutosize)
        AddOutputAptViewState("apt_autosize", "1", false);

    if (miScroll != 0)
    {
        char lacScroll[48];   // X360 sp+0x50 local; format capped at 10
        CgsCore::SPrintf(lacScroll, 10, "%d", miScroll);
        AddOutputAptViewState("apt_scroll", lacScroll, false);
        miScroll = 0;
    }
    else
    {
        AddOutputAptViewState("apt_scroll", "0", false);
    }
}

// @ 0x824E7418 -- cpp:156/:157/:158. Resolve lpacText through the language
// manager's formatter (1KB cap) into a local, adopt it, and push the apt data.
// Always reports success (the X360 returns 1).
bool TextField::SetLocalisedText(const char* lpacText,
                                 CgsLanguage::LanguageManager::ParameterFormatType leFormat)
{
    CGS_ASSERT(lpacText != 0,
               "Text field is invalid in TextField::SetLocalisedText");                 // :156 (streamed; folded)
    CGS_ASSERT(std::strlen(lpacText) < KU_MAX_TEXTFIELD_LEN,
               "Text string too long in TextField::SetLocalisedText");                  // :157 (streamed; folded)
    CGS_ASSERT(leFormat < CgsLanguage::LanguageManager::E_FORMAT_COUNT,
               "Invalid Localisation Format supplied to TextField::SetLocalisedText");  // :158 (streamed; folded)

    CgsLanguage::LanguageManager* lpLanguageManager =
        TextFieldGetLanguageManager(mpStateInterface);

    char lacFormatted[1120];   // X360 sp+0x80 local (the formatter writes through a 1024 cap)
    lpLanguageManager->FormatText(lacFormatted, 1024, lpacText, leFormat);

    SetDatabaseText(lacFormatted);
    OutputAptData();
    return true;
}

}
