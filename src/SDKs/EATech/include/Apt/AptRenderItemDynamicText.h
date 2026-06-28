#pragma once

// ===========================================================================
// EATech Apt -- AptRenderItemDynamicText: the renderable for a DYNAMIC (editable /
// data-bound) text field character. It is the AptRenderItem subtype that owns the
// per-tick mutable text state -- the rendered text + the bound ActionScript
// variable name, the font (id + size), the text/background/border colours, the
// alignment + box flags, the layout bounds rect, and the formatting object. The
// AptCharacterTextInst facade reads these straight through its mpRenderItem and
// writes them through the tick-writable item (AptRenderItem double-buffering).
//
// LAYOUT recovered from the X360 ARTIST AptCharacterTextInst accessors (the
// compiler inlined every field read/write, exposing the console offsets directly):
//   AptRenderItem base (console 52 bytes / 13 dwords), then --
//   [c:0x34] mTextValue        EAStringC -- the currently displayed text
//   [c:0x38] mVarValue         EAStringC -- the bound AS variable name ("$var")
//   [c:0x3C] mZID              i32   (the z id; Get/SetZID)
//   [c:0x40] mTextColor        u32   (text-colour read straight)
//   [c:0x44] mScroll           i32   (scroll offset)
//   [c:0x48] mFlagsAndBackColor PACKED: bits 3-6 alignment(signed) / bit 7
//                              drawsBackground / bits 8-31 background RGB
//   [c:0x4C] mFlagsAndBorderColor PACKED: bit 0 multiline / bit 1 wordWrap /
//                              bits 2-5 boxAlignment / bit 6 mouseWheelEnabled /
//                              bit 7 drawsBorder / bits 8-31 border RGB
//   [c:0x50] mBounds           AptRect (4 floats, 16 bytes)
//   [c:0x60] mFontSize         f32
//   [c:0x64] mFontID           i32
//   [c:0x68] mpTextFormat      AptValue* (the TextFormat object; opaque here)
//   [c:0x6C] mStateFlags       u32   (Set/ClearStateFlags; bit 0 + bit 1 used by
//                              UpdateText: clear bit0 / set bit1 when text changed)
//
// The console packs the two flag dwords with byte-aligned colour fields (the X360
// reads the low byte big-endian to preserve the flag bits while replacing the
// colour). De-optimised here to endian-safe 32-bit masked bitfield math + named
// members so the x64 build is correct (the colour occupies bits 8-31 logically).
//
// FLAG: this header reconstructs only the text-instance surface (the data members
// the AptCharacterTextInst facade touches + their inline accessors). The render
// item's own ctor / Render / the AptValue-backed TextFormat type are follow-ons.
// mpTextFormat is therefore an opaque AptValue* pass-through (forward-declared).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

#include "SDKs/EATech/include/Apt/AptRenderItem.h"          // base AptRenderItem
#include "SDKs/EATech/include/Apt/AptString/EAString.h"      // mTextValue / mVarValue
#include "SDKs/EATech/include/Apt/AptStd/AptRect.h"          // mBounds

class AptValue;   // mpTextFormat -- the TextFormat object (opaque pass-through here)

struct AptRenderItemDynamicText : public AptRenderItem
{
    EAStringC mTextValue;             // [c:0x34] the displayed text
    EAStringC mVarValue;              // [c:0x38] the bound AS variable name
    int32_t   mZID;                   // [c:0x3C] the z id
    uint32_t  mTextColor;             // [c:0x40]
    int32_t   mScroll;                // [c:0x44]
    uint32_t  mFlagsAndBackColor;     // [c:0x48] alignment/drawsBackground/backColor
    uint32_t  mFlagsAndBorderColor;   // [c:0x4C] multiline/wordWrap/boxAlign/mouse/drawsBorder/borderColor
    AptRect   mBounds;                // [c:0x50] (16 bytes)
    float     mFontSize;              // [c:0x60]
    int32_t   mFontID;                // [c:0x64]
    AptValue* mpTextFormat;           // [c:0x68] opaque (TextFormat object)
    uint32_t  mStateFlags;            // [c:0x6C]

    // ---- text / variable strings -----------------------------------------
    const EAStringC* GetTextValueConst() const { return &mTextValue; }   // @0x82AD5810 (facade)
    EAStringC*       GetTextValueWritable()     { return &mTextValue; }    // @0x82AE1DD0
    const EAStringC* GetVarValueConst() const  { return &mVarValue; }     // @0x82AD5820
    EAStringC*       GetVarValueWritable()       { return &mVarValue; }     // @0x82AE1DF8

    // ---- colours ----------------------------------------------------------
    int      GetTextColor() const { return static_cast<int>(mTextColor); }              // @0x82AD5840

    // Background colour lives in bits 8-31; reads force full alpha (0xFF000000).
    uint32_t GetBackgroundColor() const { return (mFlagsAndBackColor >> 8) | 0xFF000000u; }   // @0x82AD5870
    void     SetBackgroundColor(uint32_t uColor)                                               // @0x82AE1EB8
    {
        mFlagsAndBackColor = (mFlagsAndBackColor & 0x000000FFu) | (uColor << 8);
    }

    uint32_t GetBorderColor() const { return (mFlagsAndBorderColor >> 8) | 0xFF000000u; }     // @0x82AD5888
    void     SetBorderColor(uint32_t uColor)                                                   // @0x82AE1EF8
    {
        mFlagsAndBorderColor = (mFlagsAndBorderColor & 0x000000FFu) | (uColor << 8);
    }

    // ---- alignment (signed 4-bit field at bits 3-6) -----------------------
    int  GetAlignment() const                                                                 // @0x82AD58B8
    {
        // (x << 25) >> 28 : sign-extend bits 3..6.
        return static_cast<int32_t>(mFlagsAndBackColor << 25) >> 28;
    }
    void SetAlignment(int nAlignment)                                                          // @0x82AE1F70
    {
        mFlagsAndBackColor = ((static_cast<uint32_t>(nAlignment) << 3) & 0x78u)
                           | (mFlagsAndBackColor & 0xFFFFFF87u);
    }

    // ---- background / border draw flags -----------------------------------
    bool GetDrawsBackground() const { return ((mFlagsAndBackColor >> 7) & 1u) != 0; }          // @0x82AD5970
    void SetDrawsBackground(bool bDraw)                                                         // @0x82AE2170
    {
        mFlagsAndBackColor = ((static_cast<uint32_t>(bDraw ? 1u : 0u) << 7) & 0x80u)
                           | (mFlagsAndBackColor & 0xFFFFFF7Fu);
    }

    bool GetDrawsBorder() const { return ((mFlagsAndBorderColor >> 7) & 1u) != 0; }            // @0x82AD5960
    void SetDrawsBorder(bool bDraw)                                                             // @0x82AE2130
    {
        mFlagsAndBorderColor = ((static_cast<uint32_t>(bDraw ? 1u : 0u) << 7) & 0x80u)
                             | (mFlagsAndBorderColor & 0xFFFFFF7Fu);
    }

    // ---- box flags (offset 0x4C) ------------------------------------------
    bool GetMultiline() const { return (mFlagsAndBorderColor & 1u) != 0; }                     // @0x82AD59A0
    void SetMultiline(bool bMultiline)                                                          // @0x82AE2230
    {
        mFlagsAndBorderColor = (mFlagsAndBorderColor & 0xFFFFFFFEu)
                             | (static_cast<uint32_t>(bMultiline ? 1u : 0u) & 1u);
    }

    bool GetWordWrap() const { return ((mFlagsAndBorderColor >> 1) & 1u) != 0; }               // @0x82AD5990
    void SetWordWrap(bool bWrap)                                                                // @0x82AE21F0
    {
        mFlagsAndBorderColor = ((static_cast<uint32_t>(bWrap ? 1u : 0u) << 1) & 0x2u)
                             | (mFlagsAndBorderColor & 0xFFFFFFFDu);
    }

    void SetBoxAlignment(int nBoxAlignment)                                                     // @0x82AE1F38
    {
        mFlagsAndBorderColor = ((static_cast<uint32_t>(nBoxAlignment) << 2) & 0x3Cu)
                             | (mFlagsAndBorderColor & 0xFFFFFFC3u);
    }

    bool GetMouseWheelEnabled() const { return ((mFlagsAndBorderColor >> 6) & 1u) != 0; }       // @0x82AD5980
    void SetMouseWheelEnabled(bool bEnabled)                                                     // @0x82AE21B0
    {
        mFlagsAndBorderColor = ((static_cast<uint32_t>(bEnabled ? 1u : 0u) << 6) & 0x40u)
                             | (mFlagsAndBorderColor & 0xFFFFFFBFu);
    }

    // ---- scroll -----------------------------------------------------------
    int  GetScroll() const { return mScroll; }                 // @0x82AD5860
    void SetScroll(int nScroll) { mScroll = nScroll; }         // @0x82AE1E88

    // ---- z id -------------------------------------------------------------
    int  GetZID() const { return mZID; }                       // @0x82AD5830 (facade reads mpRenderItem+0x3C)
    // FLAG: the facade SetZID (@0x82AE1E20) calls this OUT-OF-LINE render-item method
    // (not inlined), so its body lives with the render-item TU; declared here so the
    // facade can name it. (Its effect is the mZID write; body deferred, not guessed.)
    void SetZID(int nZID);                                      // (callee of AptCharacterTextInst::SetZID)

    // ---- font -------------------------------------------------------------
    float GetFontSize() const { return mFontSize; }            // @0x82AD5910
    void  SetFontSize(float fSize) { mFontSize = fSize; }      // @0x82AE2020
    int   GetFontID() const { return mFontID; }                // @0x82AD5920

    // ---- text-format object (opaque pass-through) -------------------------
    AptValue* GetTextFormatConst() const { return mpTextFormat; }      // @0x82AD5930
    AptValue* GetTextFormatWritable() { return mpTextFormat; }         // @0x82AE2050

    // ---- bounds rect ------------------------------------------------------
    const AptRect* GetBoundsConst() const { return &mBounds; }         // @0x82AD5900
    AptRect*       GetBoundsWritable() { return &mBounds; }            // @0x82AE1FA8
    void SetBounds(const AptRect& rBounds) { mBounds = rBounds; }      // @0x82AE1FD0

    // ---- state flags ------------------------------------------------------
    void SetStateFlags(uint32_t uFlags) { mStateFlags |= uFlags; }                 // @0x82AE20B0
    void ClearStateFlags(uint32_t uFlags) { mStateFlags &= ~uFlags; }              // @0x82AE2078

    // ---- text-format (delegated to the formatting object) -----------------
    // FLAG: AptRenderItemDynamicText::SetTextFormat (the @0x82AECE08-callee) writes
    // mpTextFormat through the TextFormat layer; its body is the formatting follow-on.
    // Declared so the facade can name it.
    void SetTextFormat(AptValue* pTextFormat);   // (callee of AptCharacterTextInst::SetTextFormat)
};
