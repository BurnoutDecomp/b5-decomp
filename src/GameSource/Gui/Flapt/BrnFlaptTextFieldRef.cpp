#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"

#include "GameSource/Gui/Flapt/BrnFlaptTextFieldInstance.h"       // BrnFlapt::TextFieldInstance (SetText)
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"   // CgsLanguage::LanguageManager (FormatText / Obsolete_FormatTextByArray)
#include "GameShared/GameClasses/Core/CgsAssert.h"                // CGS_ASSERT

// BrnFlapt::TextFieldRef member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU (class:BrnFlapt::TextFieldRef) bodies:
//
//   Construct                                  @ 0x8246AFF0
//   SetText                                    @ 0x8246CC48
//   SetLocalisedText(id, type)                 @ 0x8246CD00
//   SetLocalisedText(id, type, n, params, fmts) @ 0x8246D158
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
