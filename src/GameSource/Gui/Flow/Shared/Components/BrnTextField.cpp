#include "GameSource/Gui/BrnGuiTextField.h"

#include <cstring>                                                    // std::memset / std::strlen
#include <cstdarg>                                                    // va_list (the positional-parameter SetLocalisedText)
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                  // CgsGui::GuiAccessPointers (mpLanguageManager @+4)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // CgsGui::StateInterface (GetAccessPointers)

// BrnGui::TextField -- the out-of-line bodies for the committed BrnGuiTextField.h
// surface. Reconstructed from BURNOUT_X360_ARTIST.XEX (DWARF primary file
// GameSource/Gui/Flow/Shared/Components/BrnTextField.cpp).
//
// Bodied here (6 ledger functions):
//   TextField::Construct              @0x824E4FA8
//   TextField::SetDatabaseText        @0x824E5020
//   TextField::OutputAptData          @0x824E52B8
//   TextField::SetText                @0x824E7240
//   TextField::SetLocalisedText       @0x824E7418
//   TextField::SetLocalisedText(int)  @0x824E7708 (ledger-unnamed sub_; + its free
//                                      forwarder spelling)
// (SetColour @0x82481E48 / operator= @0x824470F0 are their own ledger functions --
// not part of this TU's export set.)

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

// @ 0x824E7240 -- cpp:86/:87. Adopt literal text (fits-in-field asserted) into
// macText via the "%s" print, then push the apt data. Both asserts stream the
// component name / offending text on the console; folded static per convention.
void TextField::SetText(const char* lpacText)
{
    CGS_ASSERT(lpacText != 0, "Text passed in is invalid for : ");                       // :86 (streamed; folded)
    CGS_ASSERT(std::strlen(lpacText) < KU_MAX_TEXTFIELD_LEN - 1,
               "Text string too long : ");                                               // :87 (streamed; folded)

    CgsCore::SPrintf(macText, KU_MAX_TEXTFIELD_LEN, "%s", lpacText);
    OutputAptData();
}

// @ 0x824E7708 -- cpp:224. The INTEGER variant (ledger-unnamed sub_824E7708):
// format liValue under leFormat straight into macText through the language
// manager's integer formatter (128 cap), then push the apt data. Always reports
// success (the X360 returns 1).
bool TextField::SetLocalisedText(s32 liValue,
                                 CgsLanguage::LanguageManager::ParameterFormatType leFormat)
{
    CGS_ASSERT(leFormat < CgsLanguage::LanguageManager::E_FORMAT_COUNT,
               "Invalid Localisation Format supplied to TextField::SetLocalisedText");  // :224 (streamed; folded)

    CgsLanguage::LanguageManager* lpLanguageManager =
        TextFieldGetLanguageManager(mpStateInterface);

    lpLanguageManager->FormatText(macText, KU_MAX_TEXTFIELD_LEN, liValue, leFormat);
    OutputAptData();
    return true;
}

// @ 0x824E7608 -- cpp:194. The FLOAT variant (ledger-unnamed sub_824E7608), the sibling of
// the integer one above: format lfValue under leFormat straight into macText through the
// language manager's float formatter (128 cap), then push the apt data. Always reports
// success (the X360 returns 1).
// ⚠️ SIGNATURE FROM THE ASM, NOT THE PSEUDOCODE. The prologue is `mr r27, r5` (the format)
// and `fmr f31, f1` (the value): the f32 argument rides f1 and CONSUMES the r4 GPR slot, so
// the format lands in r5 -- which is why Hex-Rays prints a phantom `int a3` between them.
// The call sites confirm it: CrashNavDriverDetails::HandleStatData converts each integer
// stat word up with extsw/std/lfd/fcfid/frsp before every one of its five calls here.
bool TextField::SetLocalisedText(f32 lfValue,
                                 CgsLanguage::LanguageManager::ParameterFormatType leFormat)
{
    CGS_ASSERT(leFormat < CgsLanguage::LanguageManager::E_FORMAT_COUNT,
               "Invalid Localisation Format supplied to TextField::SetLocalisedText");  // :194 (streamed; folded)

    CgsLanguage::LanguageManager* lpLanguageManager =
        TextFieldGetLanguageManager(mpStateInterface);

    lpLanguageManager->FormatText(macText, KU_MAX_TEXTFIELD_LEN, lfValue, leFormat);
    OutputAptData();
    return true;
}

// @ 0x824E7C30 -- cpp:356/:357. The ONE-INTEGER-PARAMETER variant (ledger-unnamed
// sub_824E7C30): resolve lpacText under leFormat with liValue substituted as its single
// parameter under leValueFormat, through LanguageManager::FormatTextFromInt into the same
// 1024-capped scratch the positional forms use, then adopt the result with SetDatabaseText
// and push the apt data. Always reports success (the X360 returns 1).
// ⚠️ Not the variadic form: this one's third argument is a VALUE, not a parameter count.
bool TextField::SetLocalisedText(const char* lpacText,
                                 CgsLanguage::LanguageManager::ParameterFormatType leFormat,
                                 s32 liValue,
                                 CgsLanguage::LanguageManager::ParameterFormatType leValueFormat)
{
    CGS_ASSERT(lpacText != 0,
               "Text field is invalid in TextField::SetLocalisedText");                 // :356 (streamed; folded)
    CGS_ASSERT(std::strlen(lpacText) < KU_MAX_TEXTFIELD_LEN - 1,
               "Text string too long in TextField::SetLocalisedText");                  // :357 (streamed; folded)

    CgsLanguage::LanguageManager* lpLanguageManager =
        TextFieldGetLanguageManager(mpStateInterface);

    char lacFormatted[1136];   // X360 sp+0x70 local (the formatter writes through a 1024 cap)

    lpLanguageManager->FormatTextFromInt(lacFormatted, 1024, lpacText, leFormat,
                                         liValue, leValueFormat);

    SetDatabaseText(lacFormatted);
    OutputAptData();
    return true;
}

// @ 0x824E7800 -- cpp:264/:265/:266. The POSITIONAL-PARAMETER variant (ledger-unnamed
// sub_824E7800). The X360 spills its variadic register block and hands the cursor straight
// to LanguageManager::FormatTextV, which walks liNumParams (const char*, format) pairs;
// the 1024-byte formatted result is then adopted through SetDatabaseText (NOT SetText --
// over-long results go into the localisation database under this component's name) and the
// apt data is pushed. Always reports success (the X360 returns 1).
bool TextField::SetLocalisedText(const char* lpacText,
                                 CgsLanguage::LanguageManager::ParameterFormatType leFormat,
                                 s32 liNumParams, ...)
{
    CGS_ASSERT(lpacText != 0,
               "Text field is invalid in TextField::SetLocalisedText");                 // :264 (streamed; folded)
    CGS_ASSERT(std::strlen(lpacText) < KU_MAX_TEXTFIELD_LEN - 1,
               "Text string too long in TextField::SetLocalisedText");                  // :265 (streamed; folded)
    CGS_ASSERT(liNumParams > 0 && liNumParams < 4,
               "Wrong number of Parameters int SetLocalisedText");                      // :266 (streamed; folded)

    CgsLanguage::LanguageManager* lpLanguageManager =
        TextFieldGetLanguageManager(mpStateInterface);

    char lacFormatted[1136];   // X360 sp+0x90 local (the formatter writes through a 1024 cap)

    va_list lArguments;
    va_start(lArguments, liNumParams);
    lpLanguageManager->FormatTextV(lacFormatted, 1024, lpacText, leFormat, liNumParams, lArguments);
    va_end(lArguments);

    SetDatabaseText(lacFormatted);
    OutputAptData();
    return true;
}

// @ 0x824E7A20 -- cpp:314/:315/:316. The ARRAY form (ledger-unnamed sub_824E7A20): the same
// three asserts, then LanguageManager::Obsolete_FormatTextByArray over the two parallel
// parameter arrays. Same SetDatabaseText + OutputAptData tail, same constant 1 return.
bool TextField::SetLocalisedText(const char* lpacText,
                                 CgsLanguage::LanguageManager::ParameterFormatType leFormat,
                                 s32 liNumParams, const char* const* lppacParams,
                                 const CgsLanguage::LanguageManager::ParameterFormatType* lpeParamFormats)
{
    CGS_ASSERT(lpacText != 0,
               "Text field is invalid in TextField::SetLocalisedText");                 // :314 (streamed; folded)
    CGS_ASSERT(std::strlen(lpacText) < KU_MAX_TEXTFIELD_LEN - 1,
               "Text string too long in TextField::SetLocalisedText");                  // :315 (streamed; folded)
    CGS_ASSERT(liNumParams > 0 && liNumParams < 4,
               "Wrong number of Parameters int SetLocalisedText");                      // :316 (streamed; folded)

    CgsLanguage::LanguageManager* lpLanguageManager =
        TextFieldGetLanguageManager(mpStateInterface);

    char lacFormatted[1152];   // X360 sp+0x80 local (the formatter writes through a 1024 cap)
    lpLanguageManager->Obsolete_FormatTextByArray(lacFormatted, 1024, lpacText, leFormat,
                                                  liNumParams, lppacParams, lpeParamFormats);

    SetDatabaseText(lacFormatted);
    OutputAptData();
    return true;
}

// The ledger-named free shape of the integer SetLocalisedText: the consumer TU
// (BrnShowtimeInstantResults.cpp) declared the un-named export as a free function
// over the field pointer before the member homing landed; forward it so both
// spellings resolve to the one body above.
int sub_824E7708(TextField* lpField, s32 liValue, s32 liFormatType)
{
    return lpField->SetLocalisedText(
               liValue,
               static_cast<CgsLanguage::LanguageManager::ParameterFormatType>(liFormatType))
               ? 1
               : 0;
}

}
