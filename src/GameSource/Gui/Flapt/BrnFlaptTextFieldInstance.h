#ifndef BRN_FLAPT_TEXT_FIELD_INSTANCE_H
#define BRN_FLAPT_TEXT_FIELD_INSTANCE_H

#include "types.hpp"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptString.h"  // CgsGui::CgsAptString (mAptString)
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"                    // CgsUnicode::CgsUtf8 (macStringData)
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT (ChangeText's h:120 tripwire)
#include "SharedClasses/Gui/Flapt/BrnFlaptFile.h"                       // BrnFlapt::MovieClip / TextField / FontStyle
#include "GameSource/Gui/Flapt/BrnFlaptRenderer.h"                      // BrnFlapt::FlaptRenderer (mpFonts read by Construct)

// Pointer-only collaborators (used by Construct's signature only):
struct RGBA;                                  // alternate-text-colour table
struct AptAllocateStringParameters;           // SetUpAptStringParams fills it

// ============================================================================
// GameSource/Gui/Flapt/BrnFlaptTextFieldInstance.h
//
// BrnFlapt::TextFieldInstance -- a live, instanced static text field within a Flapt
// movie. Reconstructed from BURNOUT_X360_ARTIST.XEX. The instance embeds a
// CgsGui::CgsAptString (the apt text object) at +0x00 and owns a 256-byte UTF-8
// scratch buffer for the laid-out string.
//
// Attested offsets (X360 ARTIST, TextFieldInstance::Construct @ 0x8246F548):
//   +0x00  mAptString     (CgsAptString; Construct/Prepare receive `this`)
//   +0x84  macStringData  (CgsUtf8[256]; addi r6, r31, 0x84 -> Prepare's text buffer)
//
// SOURCE LINES. The class and the DWARF-declared members sit on their DecFIGS lines
// (class h:48, Construct h:58, GetTextObject h:62, ChangeText h:68, members h:80/h:81,
// SetUpAptStringParams h:90), and ChangeText's header-inline body follows the class so
// that its "lpNewText" assert keeps the original line h:120, which ARTIST bakes into
// every inlined copy (li r5,0x78 before FireAssert, TextFieldRef::SetText @0x8246CCC8).
// ============================================================================

namespace BrnFlapt
{
    // DWARF BrnFlaptTextFieldInstance.h:48.
    //
    //
    //
    //
    //
    //
    //
    //
    //
    //
    //
    class TextFieldInstance
    {
    public:
        // Construct @ 0x8246F548 : assert the movie clip and the in-range field index,
        // construct the embedded apt string with the alternate-colour table, fill the
        // apt string-allocation parameters from the clip's TextField/FontStyle/initial
        // string, then Prepare the apt string into macStringData. DWARF returns void.
        //
        //
        //
        void Construct(const MovieClip* lpMovieClip, u32 luTextFieldIndex, const FlaptRenderer* lpRenderer,
                       const RGBA* lpAlternateTextColours, s32 liNumAlternateColours);

        // The embedded apt string's laid-out text object (the glyph run the renderer submits).
        CgsGraphics::TextObject& GetTextObject() { return mAptString.mTextObject; }
        const CgsGraphics::TextObject& GetTextObject() const { return mAptString.mTextObject; }

        // ChangeText -- header-inline (body below): assert the text, then re-point and
        // re-measure the embedded apt string into this instance's own persistent buffer
        // (CgsGui::CgsAptString::SetText @0x82855648).
        void ChangeText(const CgsUnicode::CgsUtf8* lpNewText, bool lbAlreadyLocalised);

        // ADDITIVE GROW: the embedded apt string itself. TextFieldRef::GetText @0x8246E0C0
        // hands the INSTANCE pointer straight to CgsGui::CgsAptString::GetText @0x8246D9A0
        // (`lwz r3, 0(r31)` then `bl`): the instance and its apt string share address +0x00.
        const CgsGui::CgsAptString& GetAptString() const { return mAptString; }

        // ADDITIVE GROW: raise or drop the text object's autosize flag.
        void SetAutoSize(bool lbAutoSize);


    private:
        CgsGui::CgsAptString mAptString;              // +0x00
        CgsUnicode::CgsUtf8  macStringData[256];      // +0x84 (laid-out UTF-8 string)

        // SetUpAptStringParams @ 0x8246DF58 : populate lpOutAptStringParams from a
        // TextField + FontStyle + the initial text. The X360 calling convention takes
        // exactly these four pointers in r3..r6 (no `this`), so it is a static helper.
        //
        //
        //
        //
        static void SetUpAptStringParams(const TextField* lpTextField, const FontStyle* lpFontStyle,
                                         const CgsUnicode::CgsUtf8* lpInitialText,
                                         AptAllocateStringParameters* lpOutAptStringParams);
    };

    inline void TextFieldInstance::SetAutoSize(bool lbAutoSize)
    {
        CgsGraphics::TextObject& lrTextObject = mAptString.mTextObject;
        lrTextObject.mbAutosize = lbAutoSize;
        lrTextObject.mpfCurrentFontHeight = lbAutoSize
            ? &lrTextObject.mfAutosizedFontHeight
            : &lrTextObject.mfFontHeight;
    }

    // ------------------------------------------------------------------------
    // Header-inline definitions after the class, as in the original header: the
    // assert below must stay on line 120 (see SOURCE LINES above).
    // ------------------------------------------------------------------------
    //
    //
    //
    //
    //
    //
    //
    //
    //
    //
    inline void TextFieldInstance::ChangeText(const CgsUnicode::CgsUtf8* lpNewText, bool lbAlreadyLocalised)
    {
        CGS_ASSERT(lpNewText != 0, "lpNewText");
        mAptString.SetText(lpNewText, macStringData, lbAlreadyLocalised);
    }
}

#endif // BRN_FLAPT_TEXT_FIELD_INSTANCE_H
