#ifndef BRN_FLAPT_TEXT_FIELD_REF_H
#define BRN_FLAPT_TEXT_FIELD_REF_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector4 (SetColour takes one by value)

// ============================================================================
// GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h
//
// BrnFlapt::TextFieldRef — a handle onto a live text field within a Flapt movie.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The handle stores three pointers,
// proven by Construct @ 0x8246AFF0 (stw at +0, +4, +8):
//   +0x00  mpTextFieldInstance   ("lpTextFieldInst")
//   +0x04  mpParentMovie         ("lpParentMovie")
//   +0x08  mpTransform           ("lpTransform")
//
// Single declaration home for the type (shared by Construct and the TextFieldRef
// accessors homed in BrnFlaptTextFieldRef.cpp) so there is no ODR fork. The three
// referents are modeled as void* — Construct only stores them; their interiors
// are accessed by the other (out-of-scope) TextFieldRef methods.
//
// The mutator accessors below (SetColour / SetText / the float SetLocalisedText
// form) are X360-attested in the ledger and bodied in their own (sibling) TUs;
// they are declared here -- the single home -- so the FlaptComponents callers
// (FlaptTimerFieldComponent, FlaptHelpItem) resolve against one definition. Names/
// types follow the DecFIGS DWARF (BrnFlaptTextFieldRef.h).
// ============================================================================

namespace CgsLanguage { class LanguageManager; }   // returned by GetLanguageManager

namespace BrnFlapt
{
    struct TextFieldRef
    {
        // Construct @ 0x8246AFF0 : assert all three pointers non-null, then store
        // them. Returns the constructed TextFieldRef (the X360 returns `this`,
        // here via the caller-provided storage).
        TextFieldRef* Construct(void* lpTextFieldInstance,
                                void* lpParentMovie,
                                void* lpTransform);

        // GetLanguageManager @ 0x8246CB40 -- the language manager reached through the
        // apt render handler (AptAux singleton -> mRenderHandler -> mpLanguageManager),
        // asserting the handler (cpp:47) and the manager (cpp:48); `this` is never
        // touched. DWARF BrnFlaptTextFieldRef.h:72. Bodied in CgsAptAux.cpp -- the TU
        // that owns the AptAux/AptRenderHandler layout -- because this Ref's own TU
        // must see the REAL CgsAptString (via the text-field instance) which clashes
        // with the render handler's opaque pool stand-in (see CgsAptAux.cpp's bridge
        // note).
        CgsLanguage::LanguageManager* GetLanguageManager();

        // SetColour(Vector4) -- set the field's RGBA colour from a Vector4 (the X360
        // passes the colour in a single VMX register / by value).
        void SetColour(Vector4 lv4Colour);

        // SetText @ 0x8246CC48 -- set the displayed text. lbAlreadyLocalised == true
        // means the text is final ($/~ id lookup suppressed); false lets a $/~-lead
        // string resolve through the localisation manager (CgsAptString::SetText).
        void SetText(const char* lpacNewText, bool lbAlreadyLocalised);

        // ClearText @ 0x8246CBD8 -- blank the field's displayed text. Real X360
        // symbol (PlayerPositionSingleComponent::RenderValue's empty-value paths);
        // bodied in this TU's cpp (H1 wave).
        void ClearText();

        // SetAutoSize(bool) @ 0x8246D488 -- enable/disable the field's auto-size flag
        // (the text box grows to fit its content). Used by EATraxInGameComponent::
        // Prepare to make the artist-name field auto-size.
        void SetAutoSize(bool lbAutoSize);

        // SetLocalisedText(float, ParameterFormatType) @ 0x8246CE38 -- format a
        // numeric value into the field through the language manager. The format-type
        // enum (CgsLanguage::LanguageManager::ParameterFormatType) is passed as the
        // raw integer the X360 uses (2 == the timer/decimal format) to avoid pulling
        // the language-manager header into this widely-included declaration home;
        // documented external-API integer per the conventions. Bodied in this TU's
        // cpp (H2 wave: asserts format < 21, LanguageManager::FormatText(f32) into a
        // 64-cap stack buffer, SetText(buf, true)).
        bool SetLocalisedText(f32 lfValue, s32 liFormatType);

        // SetLocalisedText(int, ParameterFormatType) @ 0x8246CF18 -- the s32 sibling
        // of the float form above (same assert, LanguageManager::FormatText(s32),
        // SetText(buf, true)). X360 call site: RoadRuleComponent::RefreshBestData's
        // best-crash readout (value, 14 == the money format). Bodied in this TU's
        // cpp (H2 wave).
        bool SetLocalisedText(s32 liValue, s32 liFormatType);

        // SetInvalid -- clear all three handles. ADDITIVE GROW (the DWARF declares it,
        // BrnFlaptTextFieldRef.h:18/99; the X360 always inlines it -- e.g. the three
        // zero stores per text field in BrnGui::BaseOverlayState's reset path
        // @0x824B1F0C/@0x824B2020/@0x824B2E50).
        void SetInvalid()
        {
            mpTextFieldInstance = NULL;
            mpParentMovie       = NULL;
            mpTransform         = NULL;
        }

        // SetLocalisedText(const char*, StringIdType, count, params, formats)
        // @ 0x8246D158 -- look the string id up and format it with liNumParams
        // positional parameters: lppcParams are the parameter strings, lpeParamFormatTypes
        // the matching CgsLanguage::LanguageManager::ParameterFormatType per parameter
        // (raw integers per this home's house style). The body asserts the field is
        // valid ("Text field is invalid in TextField::SetLocalisedText", cpp:221) and
        // 0 < liNumParams < 4 ("Wrong number of Parameters int SetLocalisedText",
        // cpp:222). DWARF shape BrnFlaptTextFieldRef.h:54. Bodied in this TU's cpp;
        // used by BaseOverlayState::SetupOverlay for parameterised popup messages.
        bool SetLocalisedText(const char* lpcStringId, s32 liStringIdType,
                              s32 liNumParams, const char* const* lppcParams,
                              const s32* lpeParamFormatTypes);

        // SetLocalisedText(id, type, value, valueFormat) @ 0x8246D2B0 -- the
        // INTEGER-parameter variant (BoostMessageItem::SetText's boost-amount path:
        // r5 == 9 id-lookup, r6 == the integer value, r7 == 11 E_FORMAT_INTEGER).
        // ADDITIVE GROW; bodied in its own sibling TU.
        bool SetLocalisedText(const char* lpcStringId, s32 liStringIdType,
                              s32 liValue, s32 liValueFormatType);

        // SetLocalisedText(id, type, value, valueFormat) @ 0x8246D398 -- look the
        // string id up and format lfValue into its single positional parameter with
        // leValueFormatType (raw ParameterFormatType integers per this home's house
        // style). X360 call sites (PlayerPositionSingle::RenderValue): r5=9 id-lookup,
        // fp1=the value, r7=11 E_FORMAT_INTEGER. DWARF shape (const char*, PFT,
        // float32_t, PFT). Bodied in this TU's cpp (H1 wave).
        bool SetLocalisedText(const char* lpcStringId, s32 liStringIdType,
                              f32 lfValue, s32 liValueFormatType);

        // SetLocalisedText(id, type, numParams, ...) @ 0x8246CFF0 -- the varargs
        // positional-parameter form (pairs of value, ParameterFormatType). DWARF shape
        // (const char*, PFT, int32_t, ...). X360 call sites pass one (const char*,
        // format) pair (e.g. "STAT_SECONDS" + a preformatted buffer with format 0).
        // Bodied in this TU's cpp (H1 wave).
        bool SetLocalisedText(const char* lpcStringId, s32 liStringIdType,
                              s32 liNumParams, ...);

        // SetLocalisedText(const char*, StringIdType) @ 0x8246CD00 -- look a localised
        // string up by id through the language manager and display it. The overload
        // taking a string id; liStringIdType is the raw id-type integer the X360 uses
        // (the body asserts it is < 21). Bodied in this TU's cpp. Used by
        // EATraxInGameComponent::DisplayNewTrackNotification (passes type 9) when the
        // supplied track text is a localisation id rather than a literal.
        void SetLocalisedText(const char* lpcStringId, s32 liStringIdType);

        // GetText -- ADDITIVE GROW [friends wave 2026-08-26]: read back the field's
        // current text. X360 attested by BrnGui::FriendsListComponent's branch handlers,
        // which feed it straight into CgsNetwork::PlayerName::Construct
        // (@0x82438AF8/@0x82438B40/@0x82438BBC/@0x82438BFC/@0x82438C44). Declaration-only.
        const char* GetText() const;

        void* mpTextFieldInstance;   // +0x00
        void* mpParentMovie;         // +0x04
        void* mpTransform;           // +0x08
    };
}

#endif // BRN_FLAPT_TEXT_FIELD_REF_H
