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

// ---- SetLocalisedText(id, type, s32 value, valueFormat) @ 0x8246D2B0 --------
// [aimodule wave 2026-08-26] The single-INTEGER-parameter form, and the float form
// directly below is its line-for-line twin. The header already declared it (ADDITIVE
// GROW, "bodied in its own sibling TU") -- that TU never existed, so the declaration
// was a promise no compiler could keep and only the LINK could find it:
// BrnBoostMessageItem::SetText @0x82411B00 is its caller, and mounting that TU turned
// the promise into LNK2019. Reconstructed from the ARTIST body (an export HOLE --
// exported as sub_8246D2B0, identified by its baked assert path
// "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.cpp" line 254, which is why it sits HERE,
// ahead of the float sibling's 282).
//   assert the id (cpp:254) -> GetLanguageManager -> FormatTextFromInt(buf, 1024, id,
//   type, value, valueFormat) -> SetText(buf, /*already localised*/ true) -> return 1.
// ⚠️ The 1024 is the CONSOLE's: its stack slot is 1104 bytes but it passes 1024 as the
// cap, exactly as FormatTextFromFloat's caller does. Reproduced, not "fixed".
bool TextFieldRef::SetLocalisedText(const char* lpcStringId, s32 liStringIdType,
                                    s32 liValue, s32 liValueFormatType)
{
    CGS_ASSERT(lpcStringId != 0, "Text field is invalid in TextField::SetLocalisedText");

    CgsLanguage::LanguageManager* lpLanguageManager = GetLanguageManager();

    char lacBuffer[1024];
    lpLanguageManager->FormatTextFromInt(
        lacBuffer, 1024, lpcStringId,
        static_cast<CgsLanguage::LanguageManager::ParameterFormatType>(liStringIdType),
        liValue,
        static_cast<CgsLanguage::LanguageManager::ParameterFormatType>(liValueFormatType));

    SetText(lacBuffer, true);
    return true;
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

// ---- SetLocalisedText(f32 value, format) @ 0x8246CE38 -----------------------
// [H2 wave 2026-08-25] The bare-value float form (the timer field's readout path,
// format 2 == the timer format; RoadRuleComponent::RefreshBestData's best-time
// readout): range-check the format (cpp:114), render the value through the language
// manager's f32 formatter into a 64-cap stack buffer, set the resolved text
// (already localised), return 1 unconditionally.
bool TextFieldRef::SetLocalisedText(f32 lfValue, s32 liFormatType)
{
    CGS_ASSERT(liFormatType < 21,
               "Invalid Localisation Format supplied to TextField::SetLocalisedText");   // cpp:114 (non-gating)

    CgsLanguage::LanguageManager* lpLanguageManager = GetLanguageManager();

    char lacBuffer[64];
    lpLanguageManager->FormatText(
        lacBuffer, 64, lfValue,
        static_cast<CgsLanguage::LanguageManager::ParameterFormatType>(liFormatType));

    SetText(lacBuffer, true);
    return true;
}

// ---- SetLocalisedText(s32 value, format) @ 0x8246CF18 -----------------------
// [H2 wave 2026-08-25] The bare-value integer sibling (RoadRuleComponent::
// RefreshBestData's best-crash readout, format 14 == the money format): same shape
// as the float form over the s32 formatter (cpp:137 assert).
bool TextFieldRef::SetLocalisedText(s32 liValue, s32 liFormatType)
{
    CGS_ASSERT(liFormatType < 21,
               "Invalid Localisation Format supplied to TextField::SetLocalisedText");   // cpp:137 (non-gating)

    CgsLanguage::LanguageManager* lpLanguageManager = GetLanguageManager();

    char lacBuffer[64];
    lpLanguageManager->FormatText(
        lacBuffer, 64, liValue,
        static_cast<CgsLanguage::LanguageManager::ParameterFormatType>(liFormatType));

    SetText(lacBuffer, true);
    return true;
}

// ---- SetColour(Vector4) @ 0x8246E120 ----------------------------------------
// [H2 wave 2026-08-25] Repack the colour's RGB lanes (each lane * 255, truncated --
// the X360 fctidz chain) into the field instance's packed text colour
// (mAptString.mTextObject.mTextColour @ instance+0x14), leaving the ALPHA byte as it
// stands (the X360 never touches the top byte). The w lane is ignored.
void TextFieldRef::SetColour(Vector4 lv4Colour)
{
    CGS_ASSERT(mpTextFieldInstance != 0, "mpTextFieldInst");   // cpp:423 (non-gating)

    const u32 luRed   = static_cast<u32>(static_cast<s64>(lv4Colour.x * 255.0f)) & 0xFFu;
    const u32 luGreen = static_cast<u32>(static_cast<s64>(lv4Colour.y * 255.0f)) & 0xFFu;
    const u32 luBlue  = static_cast<u32>(static_cast<s64>(lv4Colour.z * 255.0f)) & 0xFFu;

    CgsGraphics::TextObject& lrTextObject =
        static_cast<TextFieldInstance*>(mpTextFieldInstance)->GetTextObject();
    lrTextObject.mTextColour = (lrTextObject.mTextColour & 0xFF000000u)
                             | (luBlue << 16) | (luGreen << 8) | luRed;
}

}
