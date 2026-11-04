// ===========================================================================
// EATech Apt -- AptCIH::EnsureStringAllocated: the deep dynamic-text build/layout
// path. DECOMPILED from the X360 ARTIST.XEX (@0x82B06F08).
//
// This is the per-frame refresh that ProcessTextInst (AptCIHBehaviour.cpp) calls
// on a dirty / unresolved dynamic-text node. It:
//   1. re-resolves the bound-variable text (AptCharacterTextInst::UpdateText, fed
//      the display-list parent so an inherited TextFormat scope resolves);
//   2. when the field carries text (a non-empty text-object slot on the movie's
//      constant-table), builds an AptAllocateStringParameters descriptor from the
//      render item's authored + resolved state (bounds, colours, alignment, font,
//      the resolved text string), calls the host string-layout callback
//      (gAptFuncs.pfnAllocateString) to lay the glyphs out + measure the field, and
//      stores the returned host render-data handle back on the render item via
//      AptRenderItemDynamicText::SetZID;
//   3. folds the measured text width/height/length/max-scroll back onto the render
//      item + the AptCharacterTextInst cache, re-aligns the field's box (the box
//      alignment procedural-property nudge), and marks the field's render data valid
//      (mStateFlags bit0);
//   4. when the field has NO renderable text (empty text-object slot), clears the
//      handle to the empty sentinel + resets the cached layout scalars.
//
// RECONSTRUCTION NOTES (source-of-truth ladder: X360 asm/pseudocode is the spine):
//   * The RUNTIME objects -- AptCIH / AptCharacterTextInst / AptRenderItemDynamicText
//     -- are accessed through their NAMED members (no offset hacks); the console
//     offsets the pseudocode reads map to those members (see AptRenderItemDynamicText.h).
//   * Font character names use the ARTIST-attested native pointer field on
//     AptCharacterFont, following the 16-byte AptCharacter header.
//   * The TextFormat overrides (colour/style/indent/margins/font name) read the
//     +0x68 slot as the embedded TextFormat RECORD by named member (AptTextFormat.h;
//     the member's AptValue* typing is the opaque pass-through FLAG). The console's
//     fixed offsets map onto those members; the font name is mFontName.GetBuffer().
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "types.hpp"

#include "SDKs/EATech/include/Apt/AptCIH.h"
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"
#include "SDKs/EATech/include/Apt/AptCharacterTextInst.h"
#include "SDKs/EATech/include/Apt/AptRenderItemDynamicText.h"
#include "SDKs/EATech/include/Apt/AptCharacter.h"
#include "SDKs/EATech/include/Apt/AptCharacterFont.h"
#include "SDKs/EATech/include/Apt/AptCharacterDynamicText.h"   // mnDefaultGlyphIndex (font index)
#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"     // the movie def (font-char table walk)
#include "SDKs/EATech/include/Apt/AptStd/AptRect.h"
#include "SDKs/EATech/include/Apt/AptStd/AptMatrix.h"       // mpPositionMatrix->tx (the box-align anchor)
#include "SDKs/EATech/include/Apt/AptTextFormat.h"          // the TextFormat record (mpTextFormat overrides)
#include "SDKs/EATech/include/Apt/Apt.h"                    // AptAllocateStringParameters + gAptFuncs

// ---------------------------------------------------------------------------
// The "unresolved / empty text render-data handle" sentinel (console &unk_82F72DB0),
// homed in AptGlobals.cpp. mZID equal to it (or 0) means the field has no resolved
// render data. Declared here (redundant extern is legal) so the SetZID sentinel
// stores + the "has text" test read the same value the render-item TU does.
extern intptr_t gAptEmptyTextRenderDataZID;   // &unk_82F72DB0

// ---------------------------------------------------------------------------
// AptResolveTextFieldFontName -- the text field's AUTHORED font name, resolved via
// its font id through the owning movie's character table (original-source corroboration:
// AptCIH::getTextFormat resolves texFormat->pFontName as
//   pParentAnim->animation.apCharacters[text.nFontID]->font.szName
// when the id is in range and the slot is a FONT character). The walk is the SAME
// native chain EnsureStringAllocated's draw path uses below: the field char's
// mpFixupLink back-link -> the def base at +KU_AptEmbeddedMovieOff ->
// mpCharacterTable[fontId] -> the type-3 font char's name pointer @+0x10
// (Fixup case 3 relocated it live). A negative id
// (setTextFormat's "explicit font installed" latch), an out-of-range id, or a
// non-font slot yields "" -- the contract's no-font value (the getTextFormat
// overlay then KEEPS the record's copied font name).
// ---------------------------------------------------------------------------
const char* AptResolveTextFieldFontName(AptCharacterInst* pTextInst)
{
    AptRenderItem* const pItem = pTextInst ? pTextInst->GetRenderItem() : 0;
    AptCharacter*  const pChar = pItem ? pItem->mpCharacter : 0;
    if (pChar == 0 || pChar->mpFixupLink == 0)
        return "";

    const int32_t nFontIdx =
        static_cast<const AptRenderItemDynamicText*>(pItem)->mFontID;
    if (nFontIdx < 0)
        return "";

    AptCharacterAnimation* const pDef = reinterpret_cast<AptCharacterAnimation*>(
        reinterpret_cast<char*>(pChar->mpFixupLink) + KU_AptEmbeddedMovieOff);
    if (pDef->mpCharacterTable == 0 || nFontIdx >= pDef->mnCharacterCount)
        return "";

    const AptCharacter* const pFontChar = pDef->mpCharacterTable[nFontIdx];
    if (pFontChar == 0 || pFontChar->mnType != 3)   // not a FONT character
        return "";

    // Font name follows the 16-byte character header.
    const char* const pName = static_cast<const AptCharacterFont*>(pFontChar)->mpName;
    return (pName != 0) ? pName : "";
}

// ---------------------------------------------------------------------------
// AptCIH::EnsureStringAllocated @0x82B06F08.
//
// a1 (r3) = this (the dynamic-text CIH node); a2 (r4) = pParent (the display-list
// parent CIH, the inherited-TextFormat scope). The X360 body walks:
//   v4  = this->mpCharacterInst            (the AptCharacterTextInst)
//   v5  = v4->mpRenderItem->mpCharacter    (the AptCharacterDynamicText -- v5+0x20 == font idx)
//   v7/v10 = v4->mpRenderItem              (the AptRenderItemDynamicText)
// ---------------------------------------------------------------------------
void AptCIH::EnsureStringAllocated(AptCIH* pParent)
{
    AptCharacterTextInst* pTextInst = static_cast<AptCharacterTextInst*>(mpCharacterInst);

    // v5 = the authored AptCharacterDynamicText (the render item's character); its
    // console +0x20 (mnDefaultGlyphIndex) is the font index (< 0 -> "no font").
    AptRenderItem* pRI0 = pTextInst ? pTextInst->GetRenderItem() : 0;
    AptCharacterDynamicText* pCharacter =
        static_cast<AptCharacterDynamicText*>(pRI0 ? pRI0->mpCharacter : 0);

    // 1. Re-resolve the bound-variable text into the render item (feeds the parent scope).
    pTextInst->UpdateText(pParent);

    // v7 = the render item (AptRenderItemDynamicText). Only rebuild when the render
    // data is still flagged invalid (mStateFlags bit0 clear) -- the ProcessTextInst gate.
    AptRenderItemDynamicText* pItem =
        static_cast<AptRenderItemDynamicText*>(pTextInst->GetRenderItem());
    if ((pItem->mStateFlags & 1u) != 0u)
        return;   // already valid -- nothing to (re)build (X360 bne loc_82B073A8)

    // If the field already holds a live host handle (not null / not the empty sentinel),
    // release it before we (re)allocate (X360: SetZID(item, 0) drops the old handle via
    // the release hook).
    const intptr_t nOldZID = pItem->mZID;
    if (nOldZID != 0 && nOldZID != gAptEmptyTextRenderDataZID)
    {
        static_cast<AptRenderItemDynamicText*>(GetCharacterInst()->GetRenderItemWritable())
            ->SetZID(0);
    }

    // XB1 140836A79 reloads the current item after SetZID's writable-clone path.
    pItem = static_cast<AptRenderItemDynamicText*>(pTextInst->GetRenderItem());

    // The current text object slot on the movie's constant table. The console reads
    // the AptCharacterTextInst's render item TextFormat slot (v10+0x68 == mpTextFormat);
    // "has renderable text" == (slot != the empty-text sentinel  &&  *(slot+8) != 0).
    // Modelled through the named mpTextFormat member (opaque value slot).
    AptValue* pTextFormat = pItem->mpTextFormat;

    // FLAG (deep text-object emptiness gate): the console tests the movie's shared
    // empty-text-object sentinel (&unk_82F72FF8) + the object's byte @+8. With the
    // TextFormat layer still opaque, the observable "has text" here is: the resolved
    // text string is non-empty. An empty string -> the "no text" branch (below).
    const EAStringC* pResolvedText = pItem->GetTextValueConst();
    const bool bHasText = pResolvedText != 0 && !pResolvedText->IsEmpty();

    if (!bHasText)
    {
        // ---- no renderable text: clear the handle to the empty sentinel + reset the
        //      cached layout scalars (X360 loc_82B0731C). ----
        AptRenderItemDynamicText* pWritable =
            static_cast<AptRenderItemDynamicText*>(GetCharacterInst()->GetRenderItemWritable());
        pWritable->SetZID(gAptEmptyTextRenderDataZID);

        // Collapse the empty field's box ONLY when the field is auto-sized: the X360 tests
        // the packed dword IN PLACE -- `(*(v36+76) & 0x3C) != 0xC` -- i.e. the 4-bit box-
        // alignment field (bits 2-5) != 3 == AptStringAlignment_None (the authored default
        // the ctor seeds; enum value corroborated by the 2.07/3.02.02 Apt SDK source, which
        // guards this exact block with `GetBoxAlignment() != AptStringAlignment_None`).
        // A default (non-auto-sized) field KEEPS its authored box while empty -- that is what
        // preserves a bound-variable multiline field's height (e.g. the SaveLoadComponent
        // "$message" prompt, authored 668x128, empty until its text resolves) across the
        // empty pass.
        AptRenderItemDynamicText* pItem2 =
            static_cast<AptRenderItemDynamicText*>(pTextInst->GetRenderItem());
        const int nBoxAlign = (static_cast<int32_t>(pItem2->mFlagsAndBorderColor << 26)) >> 28;
        if (nBoxAlign != 3)   // != AptStringAlignment_None (X360: (flags & 0x3C) != 0xC)
        {
            // The console collapses the empty auto-sized field's box to a fixed 4-pixel edge
            // inset: right = left + 4.0 (unless word-wrapped), bottom = top + 4.0 (always --
            // flt_82004EF4 == 4.0; the literal is inline in the X360 pseudocode).
            const float fEdgeInset = 4.0f;   // flt_82004EF4
            if ((pItem2->mFlagsAndBorderColor & 2u) == 0u)   // not word-wrapped
            {
                pWritable->mBounds.fRight = pItem2->mBounds.fLeft + fEdgeInset;
            }
            pWritable->mBounds.fBottom = pItem2->mBounds.fTop + fEdgeInset;
        }

        // Reset the AptCharacterTextInst cached scalars (X360: *(v4+0x14)=0, *(v4+0x10)=1,
        // *(v4+0x18)=0 -> text width 0, max-scroll 1, text height 0).
        pTextInst->SetTextWidth(0.0f);
        pTextInst->SetMaxScroll(1);
        pTextInst->SetTextHeight(0.0f);
    }
    else
    {
        // ---- build + lay out the string (X360 main body up to LABEL_57) ----
        AptAllocateStringParameters params;

        // Alignment fields (from the render item's two packed flag dwords).
        const int nAlignment =
            (static_cast<int32_t>(pItem->mFlagsAndBackColor << 25)) >> 28;   // GetAlignment
        const int nBoxAlignment =
            (static_cast<int32_t>(pItem->mFlagsAndBorderColor << 26)) >> 28; // box alignment (bits 2-5)
        params.eAlignment    = static_cast<AptStringAlignment>(nAlignment);
        params.eBoxAlignment = static_cast<AptStringAlignment>(nBoxAlignment);

        params.bMultiline = static_cast<int>((pItem->mFlagsAndBorderColor >> 0) & 1u); // GetMultiline
        params.bWordWrap  = static_cast<int>((pItem->mFlagsAndBorderColor >> 1) & 1u);  // GetWordWrap

        // Text colour: overridden by the TextFormat record when it carries a valid
        // colour (console +8 == mnColor; -1 == "no override"), else the render item's
        // authored text colour. The +0x68 slot holds the embedded TextFormat RECORD
        // (the AptValue* typing on the member is the opaque pass-through FLAG).
        const TextFormat* const pFmt = reinterpret_cast<const TextFormat*>(pTextFormat);
        unsigned int nColour = pItem->mTextColor;
        if (pFmt != 0 && pFmt->mnColor != -1)
            nColour = static_cast<unsigned int>(pFmt->mnColor) | 0xFF000000u;
        params.nColour = nColour;

        params.nBackColor   = (pItem->mFlagsAndBackColor   >> 8) | 0xFF000000u;   // GetBackgroundColor
        params.nBorderColor = (pItem->mFlagsAndBorderColor >> 8) | 0xFF000000u;   // GetBorderColor
        params.bBackground  = static_cast<int>((pItem->mFlagsAndBackColor  >> 7) & 1u); // GetDrawsBackground
        params.bBorder      = static_cast<int>((pItem->mFlagsAndBorderColor >> 7) & 1u); // GetDrawsBorder

        // Field-edge rect (the layout bounds; console x0=left y0=top x1=right y1=bottom).
        params.x0 = pItem->mBounds.fLeft;
        params.y0 = pItem->mBounds.fTop;
        params.x1 = pItem->mBounds.fRight;
        params.y1 = pItem->mBounds.fBottom;

        params.fFontHeight = pItem->mFontSize;                       // (v52)
        params.nLineOffset = static_cast<uint32_t>(pItem->mScroll);  // (v53)

        // The resolved text buffer (the render item's mTextValue char storage).
        params.szString = pResolvedText->GetBuffer();
        params.eFlags   = pItem->mStateFlags;

        // Font style / indent / margins: overridden by the TextFormat record (console
        // style @+16 == mnStyleFlags, indent @+20, left/right margins @+24/+28), else
        // the "no format" defaults (style 0, indent/margins -1).
        if (pFmt != 0)
        {
            const int nStyle = pFmt->mnStyleFlags;
            params.nFontStyle   = static_cast<unsigned int>((nStyle == 2) ? 0 : nStyle);
            params.nIndent      = pFmt->mnIndent;
            params.nLeftMargin  = pFmt->mnLeftMargin;
            params.nRightMargin = pFmt->mnRightMargin;
        }
        else
        {
            params.nFontStyle   = 0;
            params.nIndent      = -1;
            params.nLeftMargin  = -1;
            params.nRightMargin = -1;
        }

        // The current host handle (carried so the host can reuse / free it); the empty
        // sentinel is passed as null.
        params.pCurrString = (pItem->mZID == gAptEmptyTextRenderDataZID)
                                 ? 0
                                 : reinterpret_cast<AptAssetString>(
                                       static_cast<intptr_t>(pItem->mZID));

        // The font NAME: from the TextFormat record's mFontName when a format is set
        // (the console's `+8` in this arm is the name BUFFER inside the EAStringC's
        // internal node, i.e. mFontName.GetBuffer(), NOT a record field), else --
        // when the character carries a font index (>= 0) -- resolved off the parent
        // movie's font-character table; else null.
        const char* pFontName = 0;
        if (pFmt != 0 && !pFmt->mFontName.IsEmpty())
        {
            pFontName = pFmt->mFontName.GetBuffer();
        }
        else if (pCharacter != 0 && pCharacter->mnDefaultGlyphIndex >= 0 && pParent != 0)
        {
            // Resolve the font name off the PARENT's owning MOVIE def's character table.
            //   v11 = pParent->mpCharacterInst->mpRenderItem->mpCharacter->mpFixupLink
            //         -- the character's back-link to the ROOT movie character (charTable[0]);
            //   the def base is v11 + <embedded-movie offset> (console +0x10, native-8 +0x20 ==
            //   KU_AptEmbeddedMovieOff -- the same walk findCharacterInLibrary @0x82AFDF58 uses);
            //   fontChar = def->mpCharacterTable[fontIdx];  name = fontChar + <name slot>.
            // The def base + char table are NAMED AptCharacterAnimation members; the type-3 FONT
            // character record exposes its ARTIST name pointer at +0x10.
            AptCharacterInst* pParentCI = pParent->GetCharacterInst();
            AptRenderItem*    pParentRI = pParentCI ? pParentCI->GetRenderItem() : 0;
            AptCharacter*     pParentChar = pParentRI ? pParentRI->mpCharacter : 0;
            // X360: the character's mnDefaultGlyphIndex (*(v5+32)) is only the "has a
            // font" GATE; the table INDEX is the render item's own font id (*(v10+100)
            // == mFontID -- seeded from the character but overridable per item).
            const int32_t     nFontIdx = pItem->mFontID;
            if (pParentChar != 0 && pParentChar->mpFixupLink != 0)
            {
                AptCharacterAnimation* pDef = reinterpret_cast<AptCharacterAnimation*>(
                    reinterpret_cast<char*>(pParentChar->mpFixupLink) + KU_AptEmbeddedMovieOff);
                if (pDef->mpCharacterTable != 0 && nFontIdx < pDef->mnCharacterCount)
                {
                    const AptCharacter* pFontChar = pDef->mpCharacterTable[nFontIdx];
                    if (pFontChar != 0 && pFontChar->mnType == 3)   // a FONT character
                    {
                        // The font name follows its 16-byte character header.
                        pFontName = static_cast<const AptCharacterFont*>(pFontChar)->mpName;
                    }
                }
            }
        }

        params.szFontName = pFontName;  // ARTIST 82B06F08: first callback parameter

        // ARTIST calls the layout hook unconditionally; its measured outputs are
        // written by the callback and then consumed below.
        AptAssetString hHandle = gAptFuncs.pfnAllocateString(&params);

        // Store the returned host render-data handle on the render item (X360 SetZID(v22)).
        // On x64 the handle is a host id (see AptRenderItemDynamicText.h mZID note); the
        // host returns a small slot-based id (see the draw/release hooks in
        // BrnGuiAptRuntime.cpp), so it fits the int32 mZID without truncation.
        AptRenderItemDynamicText* pWritable =
            static_cast<AptRenderItemDynamicText*>(GetCharacterInst()->GetRenderItemWritable());
        pWritable->SetZID(reinterpret_cast<intptr_t>(hHandle));
        AptRenderItemDynamicText* pItem2 =
            static_cast<AptRenderItemDynamicText*>(pTextInst->GetRenderItem());

        const int nBoxAlign2 =
            (static_cast<int32_t>(pItem2->mFlagsAndBorderColor << 26)) >> 28;
        if (nBoxAlign2 != 3)
        {
            // X360 fold operands:
            //   v26        = the item's CURRENT field width (item +0x58 - +0x50 re-read
            //                AFTER the host call -- the pre-layout box);
            //   (v41 - v39) = the PARAMS' field width (x1 - x0 -- the box the host layout
            //                wrote back);
            //   v27[4]     = the item's position matrix tx (mpPositionMatrix, defaulting
            //                to the identity matrix flt_8324E2B0 whose tx is 0) -- the
            //                clip's authored x anchor the nudge is relative to.
            const float fItemWidth   = pItem2->mBounds.fRight - pItem2->mBounds.fLeft;
            const float fParamsWidth = params.x1 - params.x0;
            const float fTx = (pItem2->mpPositionMatrix != 0)
                                  ? pItem2->mpPositionMatrix->tx
                                  : 0.0f;   // gAptIdentityMatrix.tx (flt_8324E2B0[4])
            if (nBoxAlign2 == 2)         // centre: -(((newW - curW) * 0.5) - tx)
            {
                SetProceduralProperty(0, -(((fParamsWidth - fItemWidth) * 0.5f) - fTx), true);
            }
            else if (nBoxAlign2 == 1)    // right: (tx + curW) - newW
            {
                SetProceduralProperty(0, (fTx + fItemWidth) - fParamsWidth, true);
            }
        }

        // Fold the measured layout back onto the render item (X360 LABEL_34).
        {
            if (static_cast<AptRenderItemDynamicText*>(pTextInst->GetRenderItem())->mBounds.fLeft != params.x0)
                static_cast<AptRenderItemDynamicText*>(pTextInst->GetRenderItemWritable())->mBounds.fLeft = params.x0;
            if (static_cast<AptRenderItemDynamicText*>(pTextInst->GetRenderItem())->mBounds.fRight != params.x1)
                static_cast<AptRenderItemDynamicText*>(pTextInst->GetRenderItemWritable())->mBounds.fRight = params.x1;
            if (static_cast<AptRenderItemDynamicText*>(pTextInst->GetRenderItem())->mBounds.fTop != params.y0)
                static_cast<AptRenderItemDynamicText*>(pTextInst->GetRenderItemWritable())->mBounds.fTop = params.y0;
            if (static_cast<AptRenderItemDynamicText*>(pTextInst->GetRenderItem())->mBounds.fBottom != params.y1)
                static_cast<AptRenderItemDynamicText*>(pTextInst->GetRenderItemWritable())->mBounds.fBottom = params.y1;

            // Max-scroll (v63) -> AptCharacterTextInst::mMaxScroll; clamp the render item's
            // scroll down to it.
            const int nMaxScroll = static_cast<int>(params.nMaxScroll);
            if (pTextInst->mMaxScroll != nMaxScroll)
                pTextInst->mMaxScroll = nMaxScroll;
            if (static_cast<unsigned int>(static_cast<AptRenderItemDynamicText*>(pTextInst->GetRenderItem())->mScroll) > static_cast<unsigned int>(pTextInst->mMaxScroll))
                static_cast<AptRenderItemDynamicText*>(pTextInst->GetRenderItemWritable())->mScroll = pTextInst->mMaxScroll;

            // Measured text width / height / length -> the text-inst cache (v60/v61/v62).
            if (pTextInst->mTextWidth != params.fTextWidth)
                pTextInst->mTextWidth = params.fTextWidth;
            if (pTextInst->mTextHeight != params.fTextHeight)
                pTextInst->mTextHeight = params.fTextHeight;
            if (pTextInst->mLength != params.fStrLen)
                pTextInst->mLength = params.fStrLen;
        }
    }

    // ---- mark the field's render data valid (X360 LABEL_57: *(item+0x6C) |= 1) ----
    static_cast<AptRenderItemDynamicText*>(GetCharacterInst()->GetRenderItemWritable())
        ->SetStateFlags(1u);
}
