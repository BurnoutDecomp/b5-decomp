#ifndef BRN_FLAPT_TEXT_FIELD_INSTANCE_H
#define BRN_FLAPT_TEXT_FIELD_INSTANCE_H

#include "types.hpp"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptString.h"  // CgsGui::CgsAptString (mAptString)
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"                    // CgsUnicode::CgsUtf8 (macStringData)
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT (SetText's h:120 tripwire)
#include "SharedClasses/Gui/Flapt/BrnFlaptFile.h"                       // BrnFlapt::MovieClip / TextField / FontStyle
#include "GameSource/Gui/Flapt/BrnFlaptRenderer.h"                      // BrnFlapt::FlaptRenderer (mpFonts read by Construct)

// Pointer-only collaborators (used by Construct's signature only):
struct RGBA;                                  // alternate-text-colour table
struct AptAllocateStringParameters;           // SetUpAptStringParams fills it

// ============================================================================
// GameSource/Gui/Flapt/BrnFlaptTextFieldInstance.h
//
// BrnFlapt::TextFieldInstance — a live, instanced static text field within a Flapt
// movie. Reconstructed from BURNOUT_X360_ARTIST.XEX. The instance embeds a
// CgsGui::CgsAptString (the apt text object) at +0x00 and owns a 256-byte UTF-8
// scratch buffer for the laid-out string.
//
// Attested offsets (X360 ARTIST, TextFieldInstance::Construct @ 0x8246F548):
//   +0x00  mAptString     (CgsAptString; Construct/Prepare receive `this`)
//   +0x84  macStringData  (CgsUtf8[256]; addi r6, r31, 0x84 -> Prepare's text buffer)
//
// Members are accessed BY NAME; on the PC (x64) target CgsAptString's pointer-width
// fields make its size differ from the X360's 0x84, so macStringData simply follows
// mAptString in declaration order (the X360 +0x84 displacement is recorded above per
// the project's x64 semantic-parity-by-named-members rule).
// ============================================================================

namespace BrnFlapt
{
    class TextFieldInstance
    {
    public:
        // Construct @ 0x8246F548 : assert the movie clip and the in-range field index,
        // construct the embedded apt string with the alternate-colour table, fill the
        // apt string-allocation parameters from the clip's TextField/FontStyle/initial
        // string, then Prepare the apt string into macStringData. DWARF returns void.
        void Construct(const MovieClip* lpMovieClip,
                       u32 luTextFieldIndex,
                       const FlaptRenderer* lpRenderer,
                       const RGBA* lpAlternateTextColours,
                       s32 liNumAlternateColours);

        // SetText -- X360 header-inline (this header, :120 -- the "lpNewText" tripwire
        // TextFieldRef::SetText @0x8246CC48 carries inlined): assert the text, then
        // re-point + re-measure the embedded apt string into this instance's own
        // persistent buffer (CgsAptString::SetText @0x82855648).
        void SetText(const CgsUnicode::CgsUtf8* lpNewText, bool lbAlreadyLocalised)
        {
            CGS_ASSERT(lpNewText != 0, "lpNewText");   // BrnFlaptTextFieldInstance.h:120
            mAptString.SetText(lpNewText, macStringData, lbAlreadyLocalised);
        }

        void SetAutoSize(bool lbAutoSize)
        {
            CgsGraphics::TextObject& lrTextObject = mAptString.mTextObject;
            lrTextObject.mbAutosize = lbAutoSize;
            lrTextObject.mpfCurrentFontHeight = lbAutoSize
                ? &lrTextObject.mfAutosizedFontHeight
                : &lrTextObject.mfFontHeight;
        }

    private:
        // SetUpAptStringParams @ 0x8246DF58 : populate lpOutAptStringParams from a
        // TextField + FontStyle + the initial text. The X360 calling convention takes
        // exactly these four pointers in r3..r6 (no `this`), so it is a static helper.
        static void SetUpAptStringParams(const TextField* lpTextField,
                                         const FontStyle* lpFontStyle,
                                         const CgsUnicode::CgsUtf8* lpInitialText,
                                         AptAllocateStringParameters* lpOutAptStringParams);

        CgsGui::CgsAptString mAptString;              // +0x00
        CgsUnicode::CgsUtf8  macStringData[256];      // +0x84 (laid-out UTF-8 string)
    };
}

#endif // BRN_FLAPT_TEXT_FIELD_INSTANCE_H
