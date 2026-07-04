// ===========================================================================
// EATech Apt -- AptCharacterTextInst.   DECOMPILED from the X360 ARTIST.XEX.
//
// The per-instance facade for a DYNAMIC text field. Like the AptCharacterInst base
// it sits over an AptRenderItem -- here always an AptRenderItemDynamicText -- and
// exposes the text field's properties: the const reads go straight through the
// owned render item (mpRenderItem), the writes go through the tick-writable render
// item (GetRenderItemWritable, the AptRenderItem double-buffering). Four cached
// layout scalars (max-scroll, text width/height, length) live on the instance.
//
// The accessors in the X360 binary inline the render item's field reads/writes; the
// flag/colour packing was de-optimised here to endian-safe 32-bit bitfield math on
// the NAMED render-item members (AptRenderItemDynamicText), so the x64 build is
// correct (the console's big-endian low-byte trick is not portable). See
// AptRenderItemDynamicText.h for the per-field offset map.
//
// SetText / UpdateText drive the data-binding: resolve the AS variable named by the
// field's mVarValue against the nearest movie-clip scope ancestor, render it to the
// field text (and, for SetText, seed + write the variable back when it is undefined).
//
// Function addresses are noted per method. The compiler-generated scalar deleting
// destructor (@0x82AF7F78: rewrite vtable, run ~AptCharacterInst, pool-Deallocate
// size 32) is DROPPED per project policy -- the base teardown runs via the implicit
// chained ~AptCharacterInst().
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptCharacterTextInst.h"
#include "SDKs/EATech/include/Apt/AptRenderItemDynamicText.h"
#include "SDKs/EATech/include/Apt/AptCharacterDynamicText.h"     // mpCharacter default text
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"        // getVariable / setVariable
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"           // toString / getIsDefined
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"          // AptString::Create
#include "SDKs/EATech/include/Apt/AptString/EAString.h"          // EAStringC
#include "SDKs/EATech/include/Apt/AptCIH.h"                       // the scope node (display list)

// The single global ActionScript interpreter instance (console module-static at
// &dword_8324E760; this object IS the `this` the X360 passes to getVariable /
// setVariable). Defined in AptGlobals.cpp, initialize()d by AptInit.
extern AptActionInterpreter gAptActionInterpreter;   // &dword_8324E760

// ---------------------------------------------------------------------------
// own cached layout scalars -- the trivial direct reads/writes are inline in the
// header (GetTextWidth/Height/Length, SetTextWidth/Height/Length, SetMaxScroll).
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// const reads (straight through the const render item)
// ---------------------------------------------------------------------------
int      AptCharacterTextInst::GetTextColor() const        { return GetTextItemConst()->GetTextColor(); }          // @0x82AD5840
uint32_t AptCharacterTextInst::GetBackgroundColor() const  { return GetTextItemConst()->GetBackgroundColor(); }    // @0x82AD5870
uint32_t AptCharacterTextInst::GetBorderColor() const      { return GetTextItemConst()->GetBorderColor(); }        // @0x82AD5888
int      AptCharacterTextInst::GetAlignment() const        { return GetTextItemConst()->GetAlignment(); }          // @0x82AD58B8
int      AptCharacterTextInst::GetScroll() const           { return GetTextItemConst()->GetScroll(); }             // @0x82AD5860
int      AptCharacterTextInst::GetFontID() const           { return GetTextItemConst()->GetFontID(); }             // @0x82AD5920
float    AptCharacterTextInst::GetFontSize() const         { return GetTextItemConst()->GetFontSize(); }           // @0x82AD5910
bool     AptCharacterTextInst::GetDrawsBackground() const  { return GetTextItemConst()->GetDrawsBackground(); }    // @0x82AD5970
bool     AptCharacterTextInst::GetDrawsBorder() const      { return GetTextItemConst()->GetDrawsBorder(); }        // @0x82AD5960
bool     AptCharacterTextInst::GetMouseWheelEnabled() const{ return GetTextItemConst()->GetMouseWheelEnabled(); }  // @0x82AD5980
bool     AptCharacterTextInst::GetMultiline() const        { return GetTextItemConst()->GetMultiline(); }          // @0x82AD59A0
bool     AptCharacterTextInst::GetWordWrap() const         { return GetTextItemConst()->GetWordWrap(); }           // @0x82AD5990

// GetZID @0x82AD5830 : the X360 reads the dword at render-item +0x3C (the dynamic
// text item's mZID field), straight through the const item.
intptr_t AptCharacterTextInst::GetZID() const { return GetTextItemConst()->GetZID(); }

// GetCreatedDynamic @0x82AD5950 : bit 27 of the render item's mFlags (created-at-
// runtime marker) read straight through the const item.
bool AptCharacterTextInst::GetCreatedDynamic() const
{
    return ((mpRenderItem->mFlags >> 27) & 1u) != 0;
}

const EAStringC* AptCharacterTextInst::GetTextValueConst() const { return GetTextItemConst()->GetTextValueConst(); }  // @0x82AD5810
const EAStringC* AptCharacterTextInst::GetVarValueConst() const  { return GetTextItemConst()->GetVarValueConst(); }   // @0x82AD5820
const AptRect*   AptCharacterTextInst::GetBoundsConst() const    { return GetTextItemConst()->GetBoundsConst(); }     // @0x82AD5900
AptValue*        AptCharacterTextInst::GetTextFormatConst() const{ return GetTextItemConst()->GetTextFormatConst(); } // @0x82AD5930

// ---------------------------------------------------------------------------
// writable reads (through the tick-writable render item)
// ---------------------------------------------------------------------------
EAStringC* AptCharacterTextInst::GetTextValueWritable()   { return GetTextItemWritable()->GetTextValueWritable(); }   // @0x82AE1DD0
EAStringC* AptCharacterTextInst::GetVarValueWritable()    { return GetTextItemWritable()->GetVarValueWritable(); }    // @0x82AE1DF8
AptRect*   AptCharacterTextInst::GetBoundsWritable()      { return GetTextItemWritable()->GetBoundsWritable(); }      // @0x82AE1FA8
AptValue*  AptCharacterTextInst::GetTextFormatWritable()  { return GetTextItemWritable()->GetTextFormatWritable(); }  // @0x82AE2050

// ---------------------------------------------------------------------------
// writes (through the tick-writable render item)
// ---------------------------------------------------------------------------
void AptCharacterTextInst::SetBackgroundColor(int nColor)     { GetTextItemWritable()->SetBackgroundColor(static_cast<uint32_t>(nColor)); }  // @0x82AE1EB8
void AptCharacterTextInst::SetBorderColor(int nColor)         { GetTextItemWritable()->SetBorderColor(static_cast<uint32_t>(nColor)); }      // @0x82AE1EF8
void AptCharacterTextInst::SetAlignment(int nAlignment)       { GetTextItemWritable()->SetAlignment(nAlignment); }            // @0x82AE1F70
void AptCharacterTextInst::SetBoxAlignment(int nBoxAlignment) { GetTextItemWritable()->SetBoxAlignment(nBoxAlignment); }      // @0x82AE1F38
void AptCharacterTextInst::SetScroll(int nScroll)            { GetTextItemWritable()->SetScroll(nScroll); }                  // @0x82AE1E88
void AptCharacterTextInst::SetFontSize(float fSize)          { GetTextItemWritable()->SetFontSize(fSize); }                  // @0x82AE2020
void AptCharacterTextInst::SetDrawsBackground(bool bDraw)    { GetTextItemWritable()->SetDrawsBackground(bDraw); }           // @0x82AE2170
void AptCharacterTextInst::SetDrawsBorder(bool bDraw)        { GetTextItemWritable()->SetDrawsBorder(bDraw); }               // @0x82AE2130
void AptCharacterTextInst::SetMouseWheelEnabled(bool bEnabled){ GetTextItemWritable()->SetMouseWheelEnabled(bEnabled); }     // @0x82AE21B0
void AptCharacterTextInst::SetMultiline(bool bMultiline)     { GetTextItemWritable()->SetMultiline(bMultiline); }            // @0x82AE2230
void AptCharacterTextInst::SetWordWrap(bool bWrap)          { GetTextItemWritable()->SetWordWrap(bWrap); }                  // @0x82AE21F0

EAStringC& AptCharacterTextInst::SetTextValue(const EAStringC& strText)   // @0x82AE66C0
{
    EAStringC& rText = *GetTextItemWritable()->GetTextValueWritable();
    rText = strText;
    return rText;
}

EAStringC& AptCharacterTextInst::SetVarValue(const EAStringC& strVar)     // @0x82AE66F8
{
    EAStringC& rVar = *GetTextItemWritable()->GetVarValueWritable();
    rVar = strVar;
    return rVar;
}

void AptCharacterTextInst::SetBounds(const AptRect& rBounds)              // @0x82AE1FD0
{
    GetTextItemWritable()->SetBounds(rBounds);
}

void AptCharacterTextInst::SetStateFlags(uint32_t uFlags)    { GetTextItemWritable()->SetStateFlags(uFlags); }    // @0x82AE20B0
void AptCharacterTextInst::ClearStateFlags(uint32_t uFlags)  { GetTextItemWritable()->ClearStateFlags(uFlags); }  // @0x82AE2078

// SetTextFormat @0x82AECE08 : forward to the render item's TextFormat layer.
void AptCharacterTextInst::SetTextFormat(AptValue* pTextFormat)
{
    GetTextItemWritable()->SetTextFormat(pTextFormat);
}

// SetZID @0x82AE1E20 : forward to the render item's z-id setter.
void AptCharacterTextInst::SetZID(intptr_t nZID)
{
    GetTextItemWritable()->SetZID(nZID);
}

// ---------------------------------------------------------------------------
// SetText @0x82B05EF0 -- bind the field text to the AS variable named by mVarValue.
//
//   * empty var name  -> nothing to do;
//   * "$..." literal  -> the var name itself is shown as the text (mTextValue =
//                        mVarValue);
//   * otherwise       -> walk pScope up the display list to the nearest movie-clip
//                        ancestor (character type 5 or 9), resolve the variable
//                        there; if it is defined, render it into mTextValue; if not,
//                        seed mTextValue (and the variable) from the asset's default
//                        text.
// ---------------------------------------------------------------------------
void AptCharacterTextInst::SetText(AptCIH* pScope)
{
    // The bound-variable name is read straight from the const render item (mpRenderItem).
    const EAStringC& rVarName = *GetTextItemConst()->GetVarValueConst();

    // Empty bound-variable name: nothing to bind.
    if (rVarName.IsEmpty())
        return;

    // A "$"-prefixed literal is shown verbatim (no resolution).
    if (rVarName.GetBuffer()[0] == '$')
    {
        *GetTextItemWritable()->GetTextValueWritable() = rVarName;
        return;
    }

    // Walk the display-list scope up to the nearest movie-clip-like ancestor.
    AptCIH* pNode = pScope;
    if (pNode)
    {
        while (true)
        {
            const int nType =
                static_cast<int32_t>(pNode->mpCharacterInst->mTypeFlags) >> 26;
            const bool bScope = (nType == 5 || nType == 9);
            if (bScope || !pNode->mpDisplayListParent)
                break;
            pNode = pNode->mpDisplayListParent;
        }
    }

    AptValue* pVariable = gAptActionInterpreter.getVariable(pNode, 0, &rVarName, 1, 1, 0);

    if (pVariable->getIsDefined())
    {
        // The variable resolved -> render its value into the field text.
        pVariable->toString(GetTextItemWritable()->GetTextValueWritable());
    }
    else
    {
        // Undefined -> seed from the asset's default text, store it, and write the
        // variable back so the binding has a value.
        AptString* pSeed = AptString::Create("");   // FLAG [unrecoverable literal @0x820046A7]

        const char* pDefaultText =
            static_cast<AptCharacterDynamicText*>(mpRenderItem->mpCharacter)->mpDefaultText;
        if (!pDefaultText)
            pDefaultText = "";   // FLAG [empty-string fallback @0x82143A36]

        EAStringC defaultText(pDefaultText);
        *pSeed->GetInternalString() = defaultText;

        *GetTextItemWritable()->GetTextValueWritable() = *pSeed->GetInternalString();

        gAptActionInterpreter.setVariable(pNode, 0, &rVarName, pSeed, 1, 1, 0);
    }
}

// ---------------------------------------------------------------------------
// UpdateText @0x82B06068 -- like SetText, but only restamps the render item (and
// flags it dirty) when the resolved text actually changed; never writes the
// variable back. Skips entirely when the var name is empty or a "$..." literal.
// ---------------------------------------------------------------------------
void AptCharacterTextInst::UpdateText(AptCIH* pScope)
{
    // The bound-variable name is read straight from the const render item (mpRenderItem).
    const EAStringC& rVarName = *GetTextItemConst()->GetVarValueConst();

    // Nothing to do for an empty name or a "$..." literal.
    if (rVarName.IsEmpty() || rVarName.GetBuffer()[0] == '$')
        return;

    EAStringC resolvedText;   // default = empty

    // Walk the display-list scope up to the nearest movie-clip-like ancestor.
    AptCIH* pNode = pScope;
    if (pNode)
    {
        while (true)
        {
            const int nType =
                static_cast<int32_t>(pNode->mpCharacterInst->mTypeFlags) >> 26;
            const bool bScope = (nType == 5 || nType == 9);
            if (bScope || !pNode->mpDisplayListParent)
                break;
            pNode = pNode->mpDisplayListParent;
        }
    }

    AptValue* pVariable = gAptActionInterpreter.getVariable(pNode, 0, &rVarName, 1, 1, 0);

    if (pVariable->getIsDefined())
    {
        pVariable->toString(&resolvedText);
    }
    else
    {
        const char* pDefaultText =
            static_cast<AptCharacterDynamicText*>(mpRenderItem->mpCharacter)->mpDefaultText;
        if (!pDefaultText)
            pDefaultText = "";   // FLAG [empty-string fallback @0x82143A37]
        EAStringC defaultText(pDefaultText);
        resolvedText = defaultText;
    }

    // Only restamp + mark dirty when the displayed text actually changed.
    if (!(*GetTextItemConst()->GetTextValueConst() == resolvedText))
    {
        AptRenderItemDynamicText* pWritable = GetTextItemWritable();
        *pWritable->GetTextValueWritable() = resolvedText;
        pWritable->ClearStateFlags(1u);   // clear "valid" bit 0
        pWritable->SetStateFlags(2u);     // set "changed/dirty" bit 1
    }
}
