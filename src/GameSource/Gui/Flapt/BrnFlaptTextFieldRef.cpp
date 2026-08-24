#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"

#include "GameSource/Gui/Flapt/BrnFlaptTextFieldInstance.h"       // BrnFlapt::TextFieldInstance (SetText)
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"   // CgsLanguage::LanguageManager (FormatText / Obsolete_FormatTextByArray)
#include "GameShared/GameClasses/Core/CgsAssert.h"                // CGS_ASSERT
#include <cstdarg>                                                // va_list (the varargs SetLocalisedText)

// BrnFlapt::TextFieldRef member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU (class:BrnFlapt::TextFieldRef) bodies:
//
//   Construct                                  @ 0x8246AFF0
//   SetText                                    @ 0x8246CC48
//   ClearText                                  @ 0x8246CBD8   [H1 wave]
//   SetLocalisedText(id, type)                 @ 0x8246CD00
//   SetLocalisedText(id, type, n, params, fmts) @ 0x8246D158
//   SetLocalisedText(id, type, n, ...)          @ 0x8246CFF0   [H1 wave]
//   SetLocalisedText(id, type, f32, valueFmt)   @ 0x8246D398   [H1 wave]
//
// (GetLanguageManager @0x8246CB40 belongs to this TU too, but is bodied in
// CgsAptAux.cpp: it walks the AptAux singleton -> render handler, whose header
// carries the opaque CgsAptString pool stand-in that clashes with the REAL
// CgsAptString this TU pulls in through the text-field instance.)
//
// Construct's X360 body: asserts each of the three incoming pointers is non-null
//   ("lpTextFieldInst", "lpParentMovie", "lpTransform")
// then stores them into the ref (stw at +0x00, +0x04, +0x08) and returns the ref.
// The X360-baked BrnFlaptTextFieldRef.h file/line cites are discarded per project
// convention.

namespace BrnFlapt
{

// ---- Construct @ 0x8246AFF0 ----------------------------------------------
TextFieldRef* TextFieldRef::Construct(void* lpTextFieldInstance,
                                      void* lpParentMovie,
                                      void* lpTransform)
{
    CGS_ASSERT(lpTextFieldInstance != 0, "lpTextFieldInst");
    CGS_ASSERT(lpParentMovie != 0, "lpParentMovie");
    CGS_ASSERT(lpTransform != 0, "lpTransform");

    mpTextFieldInstance = lpTextFieldInstance;
    mpParentMovie       = lpParentMovie;
    mpTransform         = lpTransform;
    return this;
}

// ---- SetText @ 0x8246CC48 -------------------------------------------------
// Assert the text and the handle, then hand the text to the referenced instance
// (whose header-inline SetText -- the h:120 "lpNewText" tripwire the X360 carries
// inlined here -- re-points + re-measures the embedded apt string into the
// instance's own persistent buffer).
void TextFieldRef::SetText(const char* lpacNewText, bool lbAlreadyLocalised)
{
    CGS_ASSERT(lpacNewText != 0, "lpacNewText");
    CGS_ASSERT(mpTextFieldInstance != 0, "mpTextFieldInst");

    static_cast<TextFieldInstance*>(mpTextFieldInstance)->SetText(
        reinterpret_cast<const CgsUnicode::CgsUtf8*>(lpacNewText), lbAlreadyLocalised);
}

// ---- SetAutoSize @ 0x8246D488 --------------------------------------------
void TextFieldRef::SetAutoSize(bool lbAutoSize)
{
    CGS_ASSERT(mpTextFieldInstance != 0,
               "Textfield must be constructed before we can set it to autosize");
    static_cast<TextFieldInstance*>(mpTextFieldInstance)->SetAutoSize(lbAutoSize);
}

// ---- ClearText @ 0x8246CBD8 ------------------------------------------------
// [H1 wave 2026-08-25] Assert the handle, then blank the instance's text: the X360
// body calls the embedded apt string's SetText with the empty string (the same
// CgsAptString::SetText the instance-level SetText rides), not-already-localised.
// The old header note said "bodied in its own sibling TU" -- no such TU existed;
// the link found it the moment JunctionInfoComponent::SetEventNameText mounted.
void TextFieldRef::ClearText()
{
    CGS_ASSERT(mpTextFieldInstance != 0, "mpTextFieldInst");
    static_cast<TextFieldInstance*>(mpTextFieldInstance)->SetText(
        reinterpret_cast<const CgsUnicode::CgsUtf8*>(""), false);
}

// ---- SetLocalisedText(id, type, numParams, ...) @ 0x8246CFF0 ----------------
// [H1 wave 2026-08-25] The varargs positional-parameter form: the X360 body spills
// the (const char* text, ParameterFormatType) pairs and forwards the va_list into
// LanguageManager::FormatTextV (@0x828651E8), then sets the resolved text
// (already localised) and returns 1 unconditionally. Asserts cpp:179/180.
bool TextFieldRef::SetLocalisedText(const char* lpcStringId, s32 liStringIdType,
                                    s32 liNumParams, ...)
{
    CGS_ASSERT(lpcStringId != 0, "Text field is invalid in TextField::SetLocalisedText");
    CGS_ASSERT(liNumParams > 0 && liNumParams < 4, "Wrong number of Parameters int SetLocalisedText");

    CgsLanguage::LanguageManager* lpLanguageManager = GetLanguageManager();

    va_list lArguments;
    va_start(lArguments, liNumParams);
    char lacBuffer[1024];
    lpLanguageManager->FormatTextV(
        lacBuffer, 1024, lpcStringId,
        static_cast<CgsLanguage::LanguageManager::ParameterFormatType>(liStringIdType),
        liNumParams, lArguments);
    va_end(lArguments);

    SetText(lacBuffer, true);
    return true;
}

// ---- SetLocalisedText(id, type) @ 0x8246CD00 -------------------------------
// Look the string id up / format it through the language manager into a 1KB local,
// then set the resolved text (already localised). The X360 body asserts the id
// pointer with the "Text field is invalid..." message (cpp:91 -- original-source
// quirk, reproduced) and range-checks the format type (cpp:92); both stream extra
// context on the console, folded static per convention.
void TextFieldRef::SetLocalisedText(const char* lpcStringId, s32 liStringIdType)
{
    CGS_ASSERT(lpcStringId != 0, "Text field is invalid in TextField::SetLocalisedText");
    CGS_ASSERT(liStringIdType < 21, "Invalid Localisation Format supplied to TextField::SetLocalisedText");

    CgsLanguage::LanguageManager* lpLanguageManager = GetLanguageManager();

    char lacBuffer[1024];
    lpLanguageManager->FormatText(
        lacBuffer, 1024, lpcStringId,
        static_cast<CgsLanguage::LanguageManager::ParameterFormatType>(liStringIdType));

    SetText(lacBuffer, true);
}

// ---- SetLocalisedText(id, type, f32 value, valueFormat) @ 0x8246D398 --------
// [H1 wave 2026-08-25] The single-float-parameter form (the odometer's mileage
// readout path): assert the id (cpp:282), resolve + format through
// LanguageManager::FormatTextFromFloat (@0x82865878), set the resolved text
// (already localised), return 1 unconditionally.
bool TextFieldRef::SetLocalisedText(const char* lpcStringId, s32 liStringIdType,
                                    f32 lfValue, s32 liValueFormatType)
{
    CGS_ASSERT(lpcStringId != 0, "Text field is invalid in TextField::SetLocalisedText");

    CgsLanguage::LanguageManager* lpLanguageManager = GetLanguageManager();

    char lacBuffer[1024];
    lpLanguageManager->FormatTextFromFloat(
        lacBuffer, 1024, lpcStringId,
        static_cast<CgsLanguage::LanguageManager::ParameterFormatType>(liStringIdType),
        lfValue,
        static_cast<CgsLanguage::LanguageManager::ParameterFormatType>(liValueFormatType));

    SetText(lacBuffer, true);
    return true;
}

// ---- SetLocalisedText(id, type, n, params, formats) @ 0x8246D158 ------------
// The positional-parameter array form: resolve + format the id with liNumParams
// (1..3) parameters through the language manager's array formatter, then set the
// resolved text (already localised). Asserts per the X360 (cpp:221/222; streamed
// context folded static). Always returns true (the X360 returns 1 regardless of
// the formatter's own result).
bool TextFieldRef::SetLocalisedText(const char* lpcStringId, s32 liStringIdType,
                                    s32 liNumParams, const char* const* lppcParams,
                                    const s32* lpeParamFormatTypes)
{
    CGS_ASSERT(lpcStringId != 0, "Text field is invalid in TextField::SetLocalisedText");
    CGS_ASSERT(liNumParams > 0 && liNumParams < 4, "Wrong number of Parameters int SetLocalisedText");

    CgsLanguage::LanguageManager* lpLanguageManager = GetLanguageManager();

    char lacBuffer[1024];
    lpLanguageManager->Obsolete_FormatTextByArray(
        lacBuffer, 1024, lpcStringId,
        static_cast<CgsLanguage::LanguageManager::ParameterFormatType>(liStringIdType),
        liNumParams, lppcParams,
        reinterpret_cast<const CgsLanguage::LanguageManager::ParameterFormatType*>(lpeParamFormatTypes));

    SetText(lacBuffer, true);
    return true;
}

}
