#pragma once

// ===========================================================================
// EATech Apt -- AptCharacterTextInst: the per-instance character data for a
// DYNAMIC text field. One of the type-tagged leaves of the AptCharacterInst family
// (AptCharacterInst::CreateCharacterInst builds it for a text-field character). It
// is the text-layout / formatting facade in the render spine:
//   AptCIH -> AptCharacterTextInst -> AptRenderItemDynamicText.
//
// Like the base AptCharacterInst, the const reads go straight through the owned
// render item (mpRenderItem, the inherited [1] slot, which for this leaf is always
// an AptRenderItemDynamicText) and the writes go through the tick-writable render
// item (GetRenderItemWritable, the AptRenderItem double-buffering). The bulk of the
// state therefore lives on the render item; this class adds only four cached layout
// scalars of its own.
//
// LAYOUT recovered from the X360 ARTIST (the accessors expose the offsets, and the
// scalar deleting destructor @0x82AF7F78 deallocates console size 32 = base 16 + 16
// of own data):
//   AptCharacterInst base (console 16 bytes / 4 dwords: vtable / mpRenderItem /
//   mTypeFlags / mpProperties), then --
//   [c:0x10] mMaxScroll    i32   (SetMaxScroll)
//   [c:0x14] mTextWidth    f32   (Get/SetTextWidth)
//   [c:0x18] mTextHeight   f32   (Get/SetTextHeight)
//   [c:0x1C] mLength       f32   (Get/SetLength)
//
// VTABLE / POLYMORPHISM: the binary emits a vtable (the scalar deleting destructor
// writes off_82145F84 at +0) but the committed AptCharacterInst base models its
// offset-0 vtable slot as the plain member mpVTable_unused (non-polymorphic). To
// keep the exact layout (no second vptr) this leaf is likewise modelled
// non-polymorphic: it inherits that offset-0 slot and adds its four scalars right
// after the base. The compiler-generated deleting-destructor thunk (the vtable
// rewrite + the pool Deallocate(size 32)) is DROPPED per project policy; the base
// teardown runs via the implicit chained ~AptCharacterInst().
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

#include "SDKs/EATech/include/Apt/AptCharacterInst.h"             // base AptCharacterInst (16 bytes)
#include "SDKs/EATech/include/Apt/AptRenderItemDynamicText.h"      // the owned render item subtype

class EAStringC;          // SetTextValue / SetVarValue operands (string assignment)
class AptValue;           // text-format object pass-through
struct AptCIH;             // SetText / UpdateText scope node (the display-list node)

struct AptCharacterTextInst : public AptCharacterInst
{
    int32_t mMaxScroll;    // [c:0x10]
    float   mTextWidth;    // [c:0x14]
    float   mTextHeight;   // [c:0x18]
    float   mLength;       // [c:0x1C]

    // ---- own cached layout scalars (read/written directly on the instance) ----
    float GetTextWidth() const { return mTextWidth; }            // @0x82AD58D0
    void  SetTextWidth(float fWidth) { mTextWidth = fWidth; }    // @0x82AD58D8
    float GetTextHeight() const { return mTextHeight; }          // @0x82AD58E0
    void  SetTextHeight(float fHeight) { mTextHeight = fHeight; }// @0x82AD58E8
    float GetLength() const { return mLength; }                  // @0x82AD58F0
    void  SetLength(float fLength) { mLength = fLength; }        // @0x82AD58F8
    void  SetMaxScroll(int nMaxScroll) { mMaxScroll = nMaxScroll; }  // @0x82AD5858

    // ---- const reads (straight through the render item) -----------------------
    int  GetTextColor() const;                  // @0x82AD5840
    uint32_t GetBackgroundColor() const;        // @0x82AD5870
    uint32_t GetBorderColor() const;            // @0x82AD5888
    int  GetAlignment() const;                  // @0x82AD58B8
    int  GetScroll() const;                     // @0x82AD5860
    intptr_t GetZID() const;                    // @0x82AD5830 (pointer-width handle on x64; XB1-verified)
    int  GetFontID() const;                     // @0x82AD5920
    float GetFontSize() const;                  // @0x82AD5910
    bool GetCreatedDynamic() const;             // @0x82AD5950
    bool GetDrawsBackground() const;            // @0x82AD5970
    bool GetDrawsBorder() const;                // @0x82AD5960
    bool GetMouseWheelEnabled() const;          // @0x82AD5980
    bool GetMultiline() const;                  // @0x82AD59A0
    bool GetWordWrap() const;                   // @0x82AD5990

    const EAStringC* GetTextValueConst() const; // @0x82AD5810
    EAStringC*       GetTextValueWritable();     // @0x82AE1DD0
    const EAStringC* GetVarValueConst() const;  // @0x82AD5820
    EAStringC*       GetVarValueWritable();      // @0x82AE1DF8

    const AptRect* GetBoundsConst() const;      // @0x82AD5900
    AptRect*       GetBoundsWritable();          // @0x82AE1FA8

    AptValue* GetTextFormatConst() const;       // @0x82AD5930
    AptValue* GetTextFormatWritable();          // @0x82AE2050

    // ---- writes (through the writable render item) ----------------------------
    void SetBackgroundColor(int nColor);        // @0x82AE1EB8
    void SetBorderColor(int nColor);            // @0x82AE1EF8
    void SetAlignment(int nAlignment);          // @0x82AE1F70
    void SetBoxAlignment(int nBoxAlignment);    // @0x82AE1F38
    void SetScroll(int nScroll);                // @0x82AE1E88
    void SetFontSize(float fSize);              // @0x82AE2020
    void SetDrawsBackground(bool bDraw);        // @0x82AE2170
    void SetDrawsBorder(bool bDraw);            // @0x82AE2130
    void SetMouseWheelEnabled(bool bEnabled);   // @0x82AE21B0
    void SetMultiline(bool bMultiline);         // @0x82AE2230
    void SetWordWrap(bool bWrap);               // @0x82AE21F0

    EAStringC& SetTextValue(const EAStringC& strText);   // @0x82AE66C0
    EAStringC& SetVarValue(const EAStringC& strVar);     // @0x82AE66F8
    void       SetBounds(const AptRect& rBounds);        // @0x82AE1FD0

    void SetStateFlags(uint32_t uFlags);        // @0x82AE20B0
    void ClearStateFlags(uint32_t uFlags);      // @0x82AE2078

    void SetTextFormat(AptValue* pTextFormat);  // @0x82AECE08
    void SetZID(intptr_t nZID);                 // @0x82AE1E20

    // ---- bound-variable text resolution ---------------------------------------
    // SetText: bind/store the field text from the AS variable named by mVarValue,
    // resolved against the display-list scope node pScope. @0x82B05EF0
    void SetText(AptCIH* pScope);
    // UpdateText: like SetText but only restamps the render item (and marks it
    // dirty) when the resolved text actually changed. @0x82B06068
    void UpdateText(AptCIH* pScope);

private:
    // The owned render item is always an AptRenderItemDynamicText for this leaf.
    AptRenderItemDynamicText* GetTextItemConst() const
    {
        return static_cast<AptRenderItemDynamicText*>(mpRenderItem);
    }
    AptRenderItemDynamicText* GetTextItemWritable()
    {
        return static_cast<AptRenderItemDynamicText*>(GetRenderItemWritable());
    }
};
