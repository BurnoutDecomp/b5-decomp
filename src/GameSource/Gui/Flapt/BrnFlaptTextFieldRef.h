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

        // SetColour(Vector4) -- set the field's RGBA colour from a Vector4 (the X360
        // passes the colour in a single VMX register / by value).
        void SetColour(Vector4 lv4Colour);

        // SetText(const char*, bool) -- set the displayed text; the bool selects
        // whether the string is looked up through the localisation manager.
        void SetText(const char* lpcText, bool lbLocalise);

        // SetAutoSize(bool) @ 0x8246D488 -- enable/disable the field's auto-size flag
        // (the text box grows to fit its content). Used by EATraxInGameComponent::
        // Prepare to make the artist-name field auto-size.
        void SetAutoSize(bool lbAutoSize);

        // SetLocalisedText(float, ParameterFormatType) @ 0x8246CE38 -- format a
        // numeric value into the field through the language manager. The format-type
        // enum (CgsLanguage::LanguageManager::ParameterFormatType) is passed as the
        // raw integer the X360 uses (2 == the timer/decimal format) to avoid pulling
        // the language-manager header into this widely-included declaration home;
        // documented external-API integer per the conventions.
        bool SetLocalisedText(f32 lfValue, s32 liFormatType);

        // SetLocalisedText(const char*, StringIdType) @ 0x8246CD00 -- look a localised
        // string up by id through the language manager and display it. The overload
        // taking a string id; liStringIdType is the raw id-type integer the X360 uses
        // (the body asserts it is < 21). Used by EATraxInGameComponent::
        // DisplayNewTrackNotification (passes type 9) when the supplied track text is a
        // localisation id rather than a literal.
        void SetLocalisedText(const char* lpcStringId, s32 liStringIdType);

        void* mpTextFieldInstance;   // +0x00
        void* mpParentMovie;         // +0x04
        void* mpTransform;           // +0x08
    };
}

#endif // BRN_FLAPT_TEXT_FIELD_REF_H
