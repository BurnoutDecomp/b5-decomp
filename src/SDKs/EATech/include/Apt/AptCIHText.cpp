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
//   * The SERIALISED font-character record walked to resolve the font NAME is external
//     file data (the movie's widened native-8 character table). Its layout is fixed by
//     the data, not a C++ class, so it is read by documented native-8 offsets (the
//     AGENTS.md "external serialised data" exception). The console reads a 4-byte-slot
//     table + a console font-char name @+0x10; the widened native-8 bundle strides the
//     table 8 bytes and the type-3 font char's name pointer lands @+0x20 (widened char
//     header 0x20 + name slot 0 -- the same native-8 character layout the XB1
//     AptCharacterAnimation::Fixup / AptMovie::resolve64 walk; verify vs the XB1 char
//     record, never a converter script).
//   * The TextFormat overrides (colour/style/indent/margins/font name) read the
//     +0x68 slot as the embedded TextFormat RECORD by named member (AptTextFormat.h;
//     the member's AptValue* typing is the opaque pass-through FLAG). The console's
//     fixed offsets map onto those members; the font name is mFontName.GetBuffer().
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

#include "SDKs/EATech/include/Apt/AptCIH.h"
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"
#include "SDKs/EATech/include/Apt/AptCharacterTextInst.h"
#include "SDKs/EATech/include/Apt/AptRenderItemDynamicText.h"
#include "SDKs/EATech/include/Apt/AptCharacter.h"
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
    AptRenderItem* pRI0 = pTextInst ? pTextInst->GetRenderItem() : nullptr;
    AptCharacterDynamicText* pCharacter =
        static_cast<AptCharacterDynamicText*>(pRI0 ? pRI0->mpCharacter : nullptr);

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
    const bool bHasText = pResolvedText != nullptr && !pResolvedText->IsEmpty();

    if (!bHasText)
    {
        // ---- no renderable text: clear the handle to the empty sentinel + reset the
        //      cached layout scalars (X360 loc_82B0731C). ----
        AptRenderItemDynamicText* pWritable =
            static_cast<AptRenderItemDynamicText*>(GetCharacterInst()->GetRenderItemWritable());
        pWritable->SetZID(gAptEmptyTextRenderDataZID);

        // Box-alignment re-nudge of the field edges (unless box alignment == 3 "none").
        AptRenderItemDynamicText* pItem2 =
            static_cast<AptRenderItemDynamicText*>(pTextInst->GetRenderItem());
        const int nBoxAlign = (static_cast<int32_t>(pItem2->mFlagsAndBorderColor << 26)) >> 28;
        if ((nBoxAlign & 0xF) != 0xC)   // console rlwinm r11,0,26,29 ; cmpwi 0xC -> the "none" box
        {
            // The console collapses the empty field's box to a fixed 4-pixel edge inset:
            // right = left + 4.0, bottom = top + 4.0 (flt_82004EF4 == 4.0 -- the literal
            // is inline in the X360 pseudocode: `*(v36 + 80) + 4.0`).
            const float fEdgeInset = 4.0f;   // flt_82004EF4
            if (((pItem2->mFlagsAndBorderColor >> 1) & 1u) == 0u)   // not word-wrapped
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

        params.bMultiline = static_cast<int>(pItem->mFlagsAndBorderColor & 1u);         // GetMultiline
        params.bWordWrap  = static_cast<int>((pItem->mFlagsAndBorderColor >> 1) & 1u);  // GetWordWrap

        // Text colour: overridden by the TextFormat record when it carries a valid
        // colour (console +8 == mnColor; -1 == "no override"), else the render item's
        // authored text colour. The +0x68 slot holds the embedded TextFormat RECORD
        // (the AptValue* typing on the member is the opaque pass-through FLAG).
        const TextFormat* const pFmt = reinterpret_cast<const TextFormat*>(pTextFormat);
        unsigned int nColour = pItem->mTextColor;
        if (pFmt != nullptr && pFmt->mnColor != -1)
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
        if (pFmt != nullptr)
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
                                 ? nullptr
                                 : reinterpret_cast<AptAssetString>(
                                       static_cast<intptr_t>(pItem->mZID));

        // The font NAME: from the TextFormat record's mFontName when a format is set
        // (the console's `+8` in this arm is the name BUFFER inside the EAStringC's
        // internal node, i.e. mFontName.GetBuffer(), NOT a record field), else --
        // when the character carries a font index (>= 0) -- resolved off the parent
        // movie's font-character table; else null.
        const char* pFontName = nullptr;
        if (pFmt != nullptr)
        {
            pFontName = pFmt->mFontName.GetBuffer();
        }
        else if (pCharacter != nullptr && pCharacter->mnDefaultGlyphIndex >= 0 && pParent != nullptr)
        {
            // Resolve the font name off the PARENT's owning MOVIE def's character table.
            //   v11 = pParent->mpCharacterInst->mpRenderItem->mpCharacter->mpFixupLink
            //         -- the character's back-link to the ROOT movie character (charTable[0]);
            //   the def base is v11 + <embedded-movie offset> (console +0x10, native-8 +0x20 ==
            //   KU_AptEmbeddedMovieOff -- the same walk findCharacterInLibrary @0x82AFDF58 uses);
            //   fontChar = def->mpCharacterTable[fontIdx];  name = fontChar + <name slot>.
            // The def base + char table are NAMED AptCharacterAnimation members; the type-3 FONT
            // character record is SERIALISED native-8 data (name pointer at font-char +0x20,
            // relocated to a live pointer by Fixup case 3 -- documented offset access).
            AptCharacterInst* pParentCI = pParent->GetCharacterInst();
            AptRenderItem*    pParentRI = pParentCI ? pParentCI->GetRenderItem() : nullptr;
            AptCharacter*     pParentChar = pParentRI ? pParentRI->mpCharacter : nullptr;
            // X360: the character's mnDefaultGlyphIndex (*(v5+32)) is only the "has a
            // font" GATE; the table INDEX is the render item's own font id (*(v10+100)
            // == mFontID -- seeded from the character but overridable per item).
            const int32_t     nFontIdx = pItem->mFontID;
            if (pParentChar != nullptr && pParentChar->mpFixupLink != nullptr)
            {
                AptCharacterAnimation* pDef = reinterpret_cast<AptCharacterAnimation*>(
                    reinterpret_cast<char*>(pParentChar->mpFixupLink) + KU_AptEmbeddedMovieOff);
                if (pDef->mpCharacterTable != nullptr && nFontIdx < pDef->mnCharacterCount)
                {
                    const AptCharacter* pFontChar = pDef->mpCharacterTable[nFontIdx];
                    if (pFontChar != nullptr && pFontChar->mnType == 3)   // a FONT character
                    {
                        // type-3 font char: name pointer at font-char native-8 +0x20
                        // (console +0x10; relocated live by Fixup case 3).
                        pFontName = *reinterpret_cast<const char* const*>(
                            reinterpret_cast<const uint8_t*>(pFontChar) + 0x20);
                    }
                }
            }
        }

        // FLAG (bring-up fallback): CgsAptString::Prepare requires a NON-NULL font name (FindFont
        // asserts + strstr's it). When the walk could not resolve one (no parent / a non-font slot),
        // pass the empty string -- FindFont then falls through its fallback table to the first
        // registered typeface, which is the faithful degenerate result of its X360 body.
        params.szFontName = (pFontName != nullptr) ? pFontName : "";

        // pnNumLines is a by-ref out-slot the console leaves uninitialised (Prepare owns
        // it); model as null (no line-count output requested here).
        params.pnNumLines = nullptr;

        // The measured OUTPUT fields Prepare writes back (width/height/length/max-scroll).
        // Seed them to safe defaults so that -- if the host callback does NOT run / cannot
        // lay the string out (no FontCollection wired on this bring-up path -> pfnAllocateString
        // returns 0 without touching the params) -- the measure-fold below does not read
        // UNINITIALISED stack (the console relies on Prepare filling these; when it is skipped
        // they must not be garbage, or the SetProceduralProperty box nudge writes a wild matrix).
        params.fTextWidth  = params.x1 - params.x0;
        params.fTextHeight = params.y1 - params.y0;
        params.fStrLen     = 0.0f;
        params.nMaxScroll  = static_cast<uint32_t>(pTextInst->mMaxScroll);

        // ---- call the host string-layout callback (X360 dword_8324E860 == pfnAllocateString) ----
        AptAssetString hHandle = nullptr;
        if (gAptFuncs.pfnAllocateString != nullptr)
            hHandle = gAptFuncs.pfnAllocateString(&params);

        // Store the returned host render-data handle on the render item (X360 SetZID(v22)).
        // On x64 the handle is a host id (see AptRenderItemDynamicText.h mZID note); the
        // host returns a small slot-based id (see the draw/release hooks in
        // BrnGuiAptRuntime.cpp), so it fits the int32 mZID without truncation.
        AptRenderItemDynamicText* pWritable =
            static_cast<AptRenderItemDynamicText*>(GetCharacterInst()->GetRenderItemWritable());
        pWritable->SetZID(reinterpret_cast<intptr_t>(hHandle));

        // FLAG (bring-up boundary): when the host could NOT lay the string out (hHandle == 0 --
        // no FontCollection wired yet), Prepare never ran, so there is no measured geometry to
        // fold and no box nudge to apply. Skip the fold entirely -- doing it off unmeasured
        // params would move the field by a bogus offset (and the field has no glyphs to draw
        // anyway). The handle is stored (0 == unresolved) and the field marked valid at the end,
        // exactly as the console does once a real layout lands. When a FontCollection IS wired
        // this branch runs the full faithful fold.
        AptRenderItemDynamicText* pItem2 =
            static_cast<AptRenderItemDynamicText*>(pTextInst->GetRenderItem());
        const int nBoxAlign2 =
            (static_cast<int32_t>(pItem2->mFlagsAndBorderColor << 26)) >> 28;
        if (hHandle != nullptr && nBoxAlign2 != 3)
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
            const float fTx = (pItem2->mpPositionMatrix != nullptr)
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

        // ---- fold the measured layout back onto the render item (X360 LABEL_34) ----
        // Only when the host laid the string out (hHandle != 0 -> Prepare wrote the measured
        // width/height/length/max-scroll back into the params). Skipped on the unresolved
        // (no-FontCollection) path so no unmeasured value reaches the render item.
        if (hHandle != nullptr)
        {
            if (pItem2->mBounds.fLeft != params.x0)
                pWritable->mBounds.fLeft = params.x0;
            if (pItem2->mBounds.fRight != params.x1)
                pWritable->mBounds.fRight = params.x1;
            if (pItem2->mBounds.fTop != params.y0)
                pWritable->mBounds.fTop = params.y0;
            if (pItem2->mBounds.fBottom != params.y1)
                pWritable->mBounds.fBottom = params.y1;

            // Max-scroll (v63) -> AptCharacterTextInst::mMaxScroll; clamp the render item's
            // scroll down to it.
            const int nMaxScroll = static_cast<int>(params.nMaxScroll);
            if (pTextInst->mMaxScroll != nMaxScroll)
                pTextInst->mMaxScroll = nMaxScroll;
            if (static_cast<int>(pItem2->mScroll) > pTextInst->mMaxScroll)
                pWritable->mScroll = pTextInst->mMaxScroll;

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
