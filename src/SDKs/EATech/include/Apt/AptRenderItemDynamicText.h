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
//   [c:0x3C] mZID              i32 console / POINTER-WIDTH x64 (the host render-data
//            handle; Get/SetZID). XB1-VERIFIED (?GetZID@AptCharacterTextInst@@QEBAQEAXXZ
//            does an 8-byte load at item+0x68): on the 64-bit ABI this slot is a void*-
//            width handle, which shifts the following block to the XB1 offsets
//            mTextColor@0x70 mScroll@0x74 flags@0x78/0x7C mBounds@0x80 mFontSize@0x90
//            mFontID@0x94 (mpTextFormat@0x98/mStateFlags@0xA0 re-sync).
//   [c:0x40] mTextColor        u32   (text-colour read straight)
//   [c:0x44] mScroll           i32   (scroll offset)
//   [c:0x48] mFlagsAndBackColor PACKED (x64, @+0x78): bits 0-23 background RGB /
//                              bit 24 drawsBackground / bits 25-28 alignment(signed)
//   [c:0x4C] mFlagsAndBorderColor PACKED (x64, @+0x7C): bits 0-23 border RGB /
//                              bit 24 drawsBorder / bit 25 mouseWheelEnabled /
//                              bits 26-29 boxAlignment(signed) / bit 30 wordWrap /
//                              bit 31 multiline
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
// This header models the text-instance surface (the data members the
// AptCharacterTextInst facade touches + their inline accessors); the ctors /
// Render / SetZID / SetTextFormat bodies are homed in AptRenderItemDynamicText.cpp.
// mpTextFormat stays an opaque AptValue* pass-through HERE (the TextFormat record
// type lives in AptTextFormat.h; the .cpp casts at the deep-copy/teardown sites).
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
    intptr_t  mZID;                   // [c:0x3C] pointer-width on x64 (XB1: void* @0x68)
    uint32_t  mTextColor;             // [c:0x40]
    int32_t   mScroll;                // [c:0x44]
    uint32_t  mFlagsAndBackColor;     // [c:0x48] alignment/drawsBackground/backColor
    uint32_t  mFlagsAndBorderColor;   // [c:0x4C] multiline/wordWrap/boxAlign/mouse/drawsBorder/borderColor
    AptRect   mBounds;                // [c:0x50] (16 bytes)
    float     mFontSize;              // [c:0x60]
    int32_t   mFontID;                // [c:0x64]
    AptValue* mpTextFormat;           // [c:0x68] opaque (TextFormat object)
    uint32_t  mStateFlags;            // [c:0x6C]

    // ---- ctors / virtual surface (bodies in AptRenderItemDynamicText.cpp) -
    // Factory ctor (Manager_CreateItem case 2 @0x82AEC0D0): base render item + the
    // dynamic-text render-type flag (bit 19), then the authored text-field defaults
    // copied from the AptCharacterDynamicText (margins -> bounds, font, colours).
    AptRenderItemDynamicText(AptCharacter* pCharacter, int nCreatedOnTick);   // @0x82AEC0D0
    // Clone copy-ctor (sub_82AEF678): base clone copy + re-stamp the render-type
    // flag; the data members copy from the source, the handle + TextFormat start fresh.
    AptRenderItemDynamicText(const AptRenderItemDynamicText* pSource, int nCreatedOnTick, bool bCopyExtended);

    virtual AptRenderItem* Clone(int nCreatedOnTick, bool bCopyExtended) override;   // @0x82AEFE38
    virtual void Render(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick) const override;  // @0x82AEFAD0
    virtual ~AptRenderItemDynamicText();   // (frees mpTextFormat + the render-data handle)

    // ---- text / variable strings -----------------------------------------
    const EAStringC* GetTextValueConst() const { return &mTextValue; }   // @0x82AD5810 (facade)
    EAStringC*       GetTextValueWritable()     { return &mTextValue; }    // @0x82AE1DD0
    const EAStringC* GetVarValueConst() const  { return &mVarValue; }     // @0x82AD5820
    EAStringC*       GetVarValueWritable()       { return &mVarValue; }     // @0x82AE1DF8

    // ---- colours ----------------------------------------------------------
    int      GetTextColor() const { return static_cast<int>(mTextColor); }              // @0x82AD5840

    // Background colour lives in bits 8-31; reads force full alpha (0xFF000000).
    // x64 packing (?GetBackgroundColor@ @0x140838230 etc.): colour RGB in the LOW 24
    // bits, flags in the HIGH byte. (The old low-byte-flags form was the X360
    // big-endian allocation.) Colour reads force full alpha (| 0xFF000000).
    uint32_t GetBackgroundColor() const { return (mFlagsAndBackColor & 0xFFFFFFu) | 0xFF000000u; }   // @0x82AD5870
    void     SetBackgroundColor(uint32_t uColor)                                               // @0x82AE1EB8
    {
        mFlagsAndBackColor = (mFlagsAndBackColor & 0xFF000000u) | (uColor & 0xFFFFFFu);
    }

    uint32_t GetBorderColor() const { return (mFlagsAndBorderColor & 0xFFFFFFu) | 0xFF000000u; }     // @0x82AD5888
    void     SetBorderColor(uint32_t uColor)                                                   // @0x82AE1EF8
    {
        mFlagsAndBorderColor = (mFlagsAndBorderColor & 0xFF000000u) | (uColor & 0xFFFFFFu);
    }

    // ---- alignment (x64: SIGNED 4-bit field at bits 25-28) ----------------
    int  GetAlignment() const                                                                 // @0x82AD58B8
    {
        // x64 ?GetAlignment@ @0x140838080: `shl 3 / sar 28` -- sign-extend bits 25..28.
        return static_cast<int32_t>(mFlagsAndBackColor << 3) >> 28;
    }
    void SetAlignment(int nAlignment)                                                          // @0x82AE1F70
    {
        mFlagsAndBackColor = ((static_cast<uint32_t>(nAlignment) & 0xFu) << 25)
                           | (mFlagsAndBackColor & ~0x1E000000u);
    }

    // ---- background / border draw flags (x64 bit 24 of each word) ---------
    bool GetDrawsBackground() const { return (mFlagsAndBackColor & 0x1000000u) != 0; }          // @0x82AD5970
    void SetDrawsBackground(bool bDraw)                                                         // @0x82AE2170
    {
        mFlagsAndBackColor = (bDraw ? 0x1000000u : 0u)
                           | (mFlagsAndBackColor & ~0x1000000u);
    }

    bool GetDrawsBorder() const { return (mFlagsAndBorderColor & 0x1000000u) != 0; }            // @0x82AD5960
    void SetDrawsBorder(bool bDraw)                                                             // @0x82AE2130
    {
        mFlagsAndBorderColor = (bDraw ? 0x1000000u : 0u)
                             | (mFlagsAndBorderColor & ~0x1000000u);
    }

    // ---- box flags (x64 high bits of the border word) ---------------------
    bool GetMultiline() const { return (mFlagsAndBorderColor & 0x80000000u) != 0; }            // @0x82AD59A0 (x64 bit 31)
    void SetMultiline(bool bMultiline)
    {
        mFlagsAndBorderColor = (mFlagsAndBorderColor & ~0x80000000u)
                             | (bMultiline ? 0x80000000u : 0u);
    }

    bool GetWordWrap() const { return (mFlagsAndBorderColor & 0x40000000u) != 0; }             // @0x82AD5990 (x64 bit 30)
    void SetWordWrap(bool bWrap)
    {
        mFlagsAndBorderColor = (bWrap ? 0x40000000u : 0u)
                             | (mFlagsAndBorderColor & ~0x40000000u);
    }

    void SetBoxAlignment(int nBoxAlignment)                                                     // @0x82AE1F38
    {
        mFlagsAndBorderColor = ((static_cast<uint32_t>(nBoxAlignment) & 0xFu) << 26)
                             | (mFlagsAndBorderColor & ~0x3C000000u);
    }
    // Sign-extended read of the 4-bit box-align field (x64 ?GetBoxAlignment@
    // @0x1408384C0: `shl 2 / sar 28` -- bits 26..29).
    int GetBoxAlignment() const
    {
        return static_cast<int32_t>(mFlagsAndBorderColor << 2) >> 28;
    }

    bool GetMouseWheelEnabled() const { return (mFlagsAndBorderColor & 0x2000000u) != 0; }       // @0x82AD5980 (x64 bit 25)
    void SetMouseWheelEnabled(bool bEnabled)                                                     // @0x82AE21B0
    {
        mFlagsAndBorderColor = (bEnabled ? 0x2000000u : 0u)
                             | (mFlagsAndBorderColor & ~0x2000000u);
    }

    // ---- scroll -----------------------------------------------------------
    int  GetScroll() const { return mScroll; }                 // @0x82AD5860
    void SetScroll(int nScroll) { mScroll = nScroll; }         // @0x82AE1E88

    // ---- z id -------------------------------------------------------------
    intptr_t GetZID() const { return mZID; }                   // @0x82AD5830 (facade reads mpRenderItem+0x3C; XB1 8-byte load)
    // The facade SetZID (@0x82AE1E20) calls this OUT-OF-LINE render-item method;
    // body HOMED in AptRenderItemDynamicText.cpp (@0x82ADB578: release the old
    // handle through the host hook, then store the new one).
    void SetZID(intptr_t nZID);                                 // (callee of AptCharacterTextInst::SetZID)

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
    // AptRenderItemDynamicText::SetTextFormat (the @0x82AECE08-callee) -- body
    // HOMED in AptRenderItemDynamicText.cpp (@0x82AEC1D0: release + pool-free the
    // old record, then store the new one).
    void SetTextFormat(AptValue* pTextFormat);   // (callee of AptCharacterTextInst::SetTextFormat)
};
