#ifndef BRN_FLAPT_TEXT_FIELD_REF_H
#define BRN_FLAPT_TEXT_FIELD_REF_H

#include "types.hpp"

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

        void* mpTextFieldInstance;   // +0x00
        void* mpParentMovie;         // +0x04
        void* mpTransform;           // +0x08
    };
}

#endif // BRN_FLAPT_TEXT_FIELD_REF_H
