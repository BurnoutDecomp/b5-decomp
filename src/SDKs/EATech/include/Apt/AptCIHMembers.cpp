// ===========================================================================
// EATech Apt -- AptCIH member recognizers: the built-in MovieClip / TextField
// property + method routes.   DECOMPILED from the X360 ARTIST.XEX:
//     AptCIH::objectMemberLookup @ 0x82B0DF70
//     AptCIH::objectMemberSet    @ 0x82B09E58
//
// These are the AptValue object-model overrides the interpreter consults on
// every member access against a placed clip (GetMember/SetMember/Get-Set-
// Property -> get/setVariable -> objectMemberLookup / objectMemberSet, BEFORE
// findChild / the property hash). They recognize the member NAME through the
// gperf indexes (TextMembersIndex for a dynamic-text instance, SpriteMembers-
// Index for the clip family) and route it to the real engine state:
//   * the procedural transform properties (_x/_y/_xscale/.../_visible) through
//     Get/SetProceduralProperty (AptCIHBehaviour.cpp),
//   * the TextField surface (text/variable/colors/flags/bounds) through the
//     AptRenderItemDynamicText tick-writable render item,
//   * the MovieClip methods (gotoAndPlay/stop/attachMovie/...) as lazily
//     created AptNativeFunction singletons over the AptCIHNativeFunctionHelper
//     natives,
//   * the AS event-handler members (onLoad/onPress/...) into the per-instance
//     property hash + its event mask (+ the director's input set for the
//     mouse-driven family).
//
// The X360 dispatches each recognized id through .rdata jump tables; every
// table was extracted from the ARTIST image and each case body verified
// against the asm:
//     objectMemberLookup: byte_82145578 (text ids 1..21, base loc_82B0DF94),
//                         word_82145470 (sprite ids 1..131, base loc_82B0E3E0)
//     objectMemberSet:    word_82145448 (text ids 1..20,  base loc_82B09EDC),
//                         word_82145428 (sprite ids 1..14, base loc_82B0A2A8),
//                         byte_82145410 (event ids 200..217, base loc_82B0A924)
// The switches below reproduce the exact id -> case mapping those tables
// encode; ids whose table slot is the default land on the shared fall-through.
//
// A case returning a value (lookup) / true (set) is HANDLED -- the interpreter
// stops there. The null / false fall-throughs continue the console resolution
// (findChild on the read side, the property-hash store on the write side) --
// several set cases (multiline / scroll / _renderflags) deliberately apply
// their state AND still return false, so the value ALSO lands in the hash,
// exactly as shipped.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API
// exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptCIH.h"
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"
#include "SDKs/EATech/include/Apt/AptCharacterSpriteInstBase.h"   // mnGotoFrame / mnClipActionFlags
#include "SDKs/EATech/include/Apt/AptCharacterTextInst.h"          // the text-instance facade (mMaxScroll/mTextWidth/mTextHeight, SetText/UpdateText)
#include "SDKs/EATech/include/Apt/AptRenderItem.h"                 // mpCharacter / mpPositionMatrix
#include "SDKs/EATech/include/Apt/AptRenderItemSprite.h"           // mInstanceName (the _renderflags render-data hook name)
#include "SDKs/EATech/include/Apt/AptRenderItemDynamicText.h"      // the TextField render-item surface
#include "SDKs/EATech/include/Apt/AptMovie.h"                      // mnFrameCount (_totalframes/_framesloaded)
#include "SDKs/EATech/include/Apt/AptNativeHash.h"                 // Set/Unset + the event-handler mask
#include "SDKs/EATech/include/Apt/AptTextFormat.h"                 // TextFormat record (the textColor write-through)
#include "SDKs/EATech/include/Apt/AptTarget.h"                     // gpAptTarget (the director's input set)
#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"            // mInputSet (AptAnimationTargetSet)
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"          // getName/getName2/setVariable
#include "SDKs/EATech/include/Apt/AptRenderingContext.h"           // multMatrix (the _xmouse/_ymouse world walk)
#include "SDKs/EATech/include/Apt/AptStd/AptMatrix.h"             // AptMatrix
#include "SDKs/EATech/include/Apt/AptExtObject.h"                  // CreateNewAptFunction (the lazy method singletons)
#include "SDKs/EATech/include/Apt/AptNativeFunction.h"
#include "SDKs/EATech/include/Apt/AptCIHNativeFunctionHelper.h"    // the sMethod_* natives
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"
#include "SDKs/EATech/include/Apt/AptValue/AptStringObject.h"      // GetBoxedString (the _renderflags string unwrap)
#include "SDKs/EATech/include/Apt/AptValue/AptBoolean.h"
#include "SDKs/EATech/include/Apt/AptValue/AptFloat.h"
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"

#include "SDKs/Packages/Apt/2.00.00/source/AptValue/AptSprite.h"   // SpriteMembersIndex
#include "SDKs/EATech/Apt/AptTextMembersIndex.h"                   // TextMembersIndex

#include <cstring>   // stricmp (the console's vestigial "this" compare)

// ---------------------------------------------------------------------------
// Shared AS/runtime singletons (defined in AptGlobals.cpp / their owning TUs).
// ---------------------------------------------------------------------------
extern AptValue* gpUndefinedValue;                    // off_8324D814
extern AptActionInterpreter gAptActionInterpreter;    // &dword_8324E760
extern int32_t gAptMouseX;                            // dword_8324E534
extern int32_t gAptMouseY;                            // dword_8324E538
extern const AptMatrix gAptIdentityMatrix;            // flt_8324E2B0
extern const int32_t gAptMemberIndexToEventBit[];     // dword_82143BA8 (event id-200 -> handler bit)

// The world-bounds helper shared with getBounds/hitTest (sub_82AE2C58,
// AptCIHBehaviour.cpp).
extern void GetBoundingRectClamped(const AptCIH* pThis, float* pOutRect);

// FLAG (un-homed AS global-function singletons; created by the deferred
// sub_82AF6B68 builtin-table init -- see the AptInit.cpp note): the console
// lookup returns these registered globals for the setInterval/clearInterval/
// isNaN/unescape/escape/Boolean member names. Null until that init homes
// (a null return simply continues the findChild resolution).
extern AptValue* gpAptFnSetInterval;     // off_8324D828
extern AptValue* gpAptFnClearInterval;   // off_8324D81C
extern AptValue* gpAptFnIsNaN;           // off_8324D824
extern AptValue* gpAptFnUnescape;        // off_8324E1FC
extern AptValue* gpAptFnEscape;          // off_8324D80C
extern AptValue* gpAptFnBoolean;         // off_8324D74C

// ---------------------------------------------------------------------------
// The AptMovie timeline embedded inside a sprite/animation AptCharacter --
// the same FLAGged reinterpret AptCIH.cpp's play-head methods centralise
// (console char+0x10; the 8-byte GUIAPT64 layout lands it at char+0x20 ==
// KU_AptEmbeddedMovieOff).
// ---------------------------------------------------------------------------
static AptMovie* GetClipMovieEmbedded(const AptCharacterInst* pInst)
{
    AptCharacter* pCharacter = pInst->mpRenderItem->mpCharacter;
    return reinterpret_cast<AptMovie*>(reinterpret_cast<char*>(pCharacter) + KU_AptEmbeddedMovieOff);
}

// ---------------------------------------------------------------------------
// The 27 lazily-created MovieClip method singletons -- the gpAptNativeFn_8324E4xx
// globals AptGlobals.cpp owns (the console .data slots 0x8324E42C..0x8324E494).
// Each is built once -- AptNativeFunction over the helper native, GC-rooted +
// pinned by one AddRef -- and returned on every later lookup, exactly the
// console's create-once pattern per case.
// ---------------------------------------------------------------------------
static AptValue* LookupMethodSingleton(AptValue*& pSlot,
                                              AptExtFunctionPtr pfnMethod)
{
    if (pSlot == nullptr)
    {
        pSlot = AptExtObject::CreateNewAptFunction(pfnMethod);   // operator new(0x24) + ctor
        if (pSlot != nullptr)
        {
            pSlot->setGCRoot(1);
            pSlot->AddRef();   // vtbl[0] -- the permanent singleton pin
        }
    }
    return pSlot;
}

extern AptValue* gpAptNativeFn_8324E444;   // attachMovie
extern AptValue* gpAptNativeFn_8324E450;   // duplicateMovieClip
extern AptValue* gpAptNativeFn_8324E440;   // gotoAndPlay
extern AptValue* gpAptNativeFn_8324E43C;   // gotoAndStop
extern AptValue* gpAptNativeFn_8324E448;   // loadMovie
extern AptValue* gpAptNativeFn_8324E47C;   // loadVariables
extern AptValue* gpAptNativeFn_8324E484;   // play
extern AptValue* gpAptNativeFn_8324E490;   // getBytesTotal
extern AptValue* gpAptNativeFn_8324E488;   // nextFrame
extern AptValue* gpAptNativeFn_8324E45C;   // createTextField
extern AptValue* gpAptNativeFn_8324E460;   // getDepth
extern AptValue* gpAptNativeFn_8324E478;   // createEmptyMovieClip
extern AptValue* gpAptNativeFn_8324E46C;   // getBounds
extern AptValue* gpAptNativeFn_8324E474;   // hitTest
extern AptValue* gpAptNativeFn_8324E458;   // removeTextField
extern AptValue* gpAptNativeFn_8324E464;   // swapDepths
extern AptValue* gpAptNativeFn_8324E468;   // setMask
extern AptValue* gpAptNativeFn_8324E430;   // getNewTextFormat
extern AptValue* gpAptNativeFn_8324E434;   // getTextFormat
extern AptValue* gpAptNativeFn_8324E42C;   // setTextFormat
extern AptValue* gpAptNativeFn_8324E470;   // startDrag
extern AptValue* gpAptNativeFn_8324E438;   // localToGlobal
extern AptValue* gpAptNativeFn_8324E454;   // removeMovieClip

// ---------------------------------------------------------------------------
// The director input-set registration for the mouse/key event-handler members
// (the console __::add / sub_82ADBD50 pair over gpAptTarget's animation
// director mInputSet -- the same AptAnimationTargetSet wrap-scan add the
// clip-event set-cache uses in AptDisplayList.cpp, and the same find + clear +
// Release remove ClearCIH's drain performs).
// ---------------------------------------------------------------------------
static void AddNodeToInputSet(AptCIH* pNode)
{
    AptAnimationTargetSet* const pSet = &gpAptTarget->GetAnimationTarget()->mInputSet;

    const uint32_t luCap = pSet->mnCapacity;
    if (luCap == 0)
        return;

    uint32_t luNext = (static_cast<uint32_t>(pSet->mnCount) + 1u) % luCap;
    uint32_t luScanned = 0u;
    while (luScanned < luCap && pSet->mppSlots[luNext] != nullptr)
    {
        luNext = (luNext + 1u) % luCap;
        ++luScanned;
    }
    if (pSet->mppSlots[luNext] == nullptr)   // found a free slot
    {
        pSet->mnCount = static_cast<uint16_t>(luNext);
        pSet->mppSlots[luNext] = static_cast<AptValue*>(pNode);
        pNode->AddRef();
    }
}

static void RemoveNodeFromInputSet(AptCIH* pNode)   // console sub_82ADBD50
{
    AptAnimationTargetSet* const pSet = &gpAptTarget->GetAnimationTarget()->mInputSet;
    for (uint32_t i = 0; i < pSet->mnCapacity; ++i)
    {
        if (pSet->mppSlots != nullptr &&
            reinterpret_cast<AptCIH*>(pSet->mppSlots[i]) == pNode)
        {
            pSet->mppSlots[i] = nullptr;
            pNode->Release();
            break;
        }
    }
}

// ===========================================================================
// objectMemberLookup @0x82B0DF70 -- resolve a built-in member READ on a clip.
// ===========================================================================
AptValue* AptCIH::objectMemberLookup(AptValue* const pThis,
                                     const AptNativeString* const pName) const
{
    AptCIH* const pNode = static_cast<AptCIH*>(pThis);

    // The empty AptCIHNone placeholder resolves every member to `undefined`.
    if (pNode->getVtblIndex() == AptVFT_CIHNone)
        return gpUndefinedValue;

    AptCharacterInst* const pInst = pNode->mpCharacterInst;   // (no null guard, as shipped)

    // ---- the TextField members (dynamic-text instances only) ---------------
    if (pInst->GetTypeTag() == 2)
    {
        const TextMembersIndex::Entry* const pEntry =
            TextMembersIndex::in_word_set(pName->GetBuffer(), pName->GetLength());
        if (pEntry != nullptr &&
            static_cast<uint32_t>(pEntry->miData) - 1u <= 20u)   // ids 1..21 (byte_82145578)
        {
            AptCharacterTextInst* const pTextInst = static_cast<AptCharacterTextInst*>(pInst);
            const AptRenderItemDynamicText* const pText =
                static_cast<const AptRenderItemDynamicText*>(pInst->mpRenderItem);

            switch (pEntry->miData)
            {
            case 1:   // "autoSize" @0x82B0E004 -- the box-align field as a keyword
            {
                // The keyword-per-value table mirrors the setter @0x82B09EDC (see its
                // proof note): 0=Left->"left", 1=Right->"right", 2=Center->"center",
                // 3=None->"none" (asm string refs 0x8324E638/0x8324E6A4/0x8324E60C/
                // 0x8324E644 in the alphabetised pool).
                AptString* const pStr = AptString::Create("");
                const int32_t nBoxAlign = pText->GetBoxAlignment();
                if (nBoxAlign < 1)
                    *pStr->GetInternalString() = EAStringC("left");
                else if (nBoxAlign == 1)
                    *pStr->GetInternalString() = EAStringC("right");
                else if (nBoxAlign < 3)
                    *pStr->GetInternalString() = EAStringC("center");
                else
                    *pStr->GetInternalString() = EAStringC("none");
                return pStr;
            }

            case 2:   // "background" @0x82B0E07C
                return AptBoolean::Create(pText->GetDrawsBackground());

            case 3:   // "backgroundColor" @0x82B0E09C -- the packed RGB above the flag byte
                return AptInteger::Create(
                    static_cast<int>((pText->mFlagsAndBackColor >> 8) & 0x00FFFFFFu));

            case 4:   // "border" @0x82B0E0B8
                return AptBoolean::Create(pText->GetDrawsBorder());

            case 5:   // "borderColor" @0x82B0E0CC
                return AptInteger::Create(
                    static_cast<int>((pText->mFlagsAndBorderColor >> 8) & 0x00FFFFFFu));

            case 7:   // "length" @0x82B0E0D8 -- refresh the bound text, then its length
                pTextInst->UpdateText(pNode);
                return AptInteger::Create(static_cast<int>(pText->mTextValue.GetLength()));

            case 8:   // "maxChars" @loc_82B0DF94 -- not tracked: always `undefined`
                return gpUndefinedValue;

            case 9:   // "maxscroll" @0x82B0E0F4 -- relayout if dirty, then the facade value
                if ((pText->mStateFlags & 4u) != 0)
                    pNode->EnsureStringAllocated(pNode->mpDisplayListParent);
                return AptInteger::Create(pTextInst->mMaxScroll);

            case 10:  // "multiline" @0x82B0E118
                return AptBoolean::Create(pText->GetMultiline());

            case 11:  // "scroll" @0x82B0E128
                if ((pText->mStateFlags & 4u) != 0)
                    pNode->EnsureStringAllocated(pNode->mpDisplayListParent);
                return AptInteger::Create(pText->mScroll);

            case 12:  // "text" @0x82B0E150 -- refresh the bound text, box a copy
            {
                pTextInst->UpdateText(pNode);
                AptString* const pStr = AptString::Create("");
                *pStr->GetInternalString() = pText->mTextValue;
                return pStr;
            }

            case 13:  // "textColor" @0x82B0E184 (24-bit RGB; stored with forced alpha)
                return AptInteger::Create(static_cast<int>(pText->mTextColor & 0x00FFFFFFu));

            case 14:  // "textHeight" @0x82B0E190 -- 0 for empty text, else the laid-out extent
                if (pText->mTextValue.GetLength() == 0)   // console: the shared empty-string node
                    return AptFloat::Create(0.0f);
                if ((pText->mStateFlags & 4u) != 0)
                    pNode->EnsureStringAllocated(pNode->mpDisplayListParent);
                return AptFloat::Create(pTextInst->mTextHeight);

            case 15:  // "textWidth" @0x82B0E1D8
                if (pText->mTextValue.GetLength() == 0)
                    return AptFloat::Create(0.0f);
                if ((pText->mStateFlags & 4u) != 0)
                    pNode->EnsureStringAllocated(pNode->mpDisplayListParent);
                return AptFloat::Create(pTextInst->mTextWidth);

            case 16:  // "type" @0x82B0E210 -- a dynamic-text field is always "dynamic"
            {
                AptString* const pStr = AptString::Create("");
                *pStr->GetInternalString() = EAStringC("dynamic");
                return pStr;
            }

            case 17:  // "variable" @0x82B0E248 -- the bound AS variable name (or undefined)
            {
                if (pText->mVarValue.GetLength() == 0)   // console: the shared empty-string node
                    return gpUndefinedValue;
                AptString* const pStr = AptString::Create("");
                *pStr->GetInternalString() = pText->mVarValue;
                return pStr;
            }

            case 18:  // "wordWrap" @0x82B0E278
                return AptBoolean::Create(pText->GetWordWrap());

            case 19:  // "_height" @0x82B0E288 -- world bounds for justify/word-wrap boxes,
            {         // else the laid-out text height + the 4.0 box gutter (flt_82004EF4)
                if ((pText->mStateFlags & 4u) != 0)
                    pNode->EnsureStringAllocated(pNode->mpDisplayListParent);
                if ((pText->mFlagsAndBorderColor & 0x3Cu) == 0x0Cu || pText->GetWordWrap())
                {
                    float afRect[4];
                    GetBoundingRectClamped(pNode, afRect);
                    const float fHeight = afRect[3] - afRect[1];
                    return AptFloat::Create((fHeight >= 0.0f) ? fHeight : 0.0f);
                }
                return AptFloat::Create(pTextInst->mTextHeight + 4.0f);
            }

            case 20:  // "_width" @0x82B0E2FC (the mirror of _height)
            {
                if ((pText->mStateFlags & 4u) != 0)
                    pNode->EnsureStringAllocated(pNode->mpDisplayListParent);
                if ((pText->mFlagsAndBorderColor & 0x3Cu) == 0x0Cu || pText->GetWordWrap())
                {
                    float afRect[4];
                    GetBoundingRectClamped(pNode, afRect);
                    const float fWidth = afRect[2] - afRect[0];
                    return AptFloat::Create((fWidth >= 0.0f) ? fWidth : 0.0f);
                }
                return AptFloat::Create(pTextInst->mTextWidth + 4.0f);
            }

            case 21:  // "mouseWheelEnabled" @0x82B0E354
                return AptBoolean::Create(pText->GetMouseWheelEnabled());

            case 6:   // "hscroll" -- table default: falls to the sprite recognizer
            default:
                break;
            }
        }
    }

    // ---- the MovieClip members (a live, defined clip value) ----------------
    const SpriteMembersIndex::Entry* pSprEntry = nullptr;
    if (pNode->getVtblIndex() == AptVFT_CharacterInstHandle && pNode->getIsDefined())
    {
        pSprEntry = SpriteMembersIndex::in_word_set(pName->GetBuffer(), pName->GetLength());
    }
    if (pSprEntry != nullptr &&
        static_cast<uint32_t>(pSprEntry->muMemberIndex) - 1u <= 0x82u)   // ids 1..131 (word_82145470)
    {
        switch (pSprEntry->muMemberIndex)
        {
        case 1:   // "_x" @0x82B0E3E0 / case 2: "_y" @0x82B0E454 -- the procedural
        case 2:   // translate, plus the text box offset on a text instance
        {
            if (pInst->GetTypeTag() == 2)
            {
                const AptRenderItemDynamicText* const pText =
                    static_cast<const AptRenderItemDynamicText*>(pInst->mpRenderItem);
                const int32_t nBoxAlign = pText->GetBoxAlignment();
                if (nBoxAlign != 3 && nBoxAlign != 0 && (pText->mStateFlags & 4u) != 0)
                    pNode->EnsureStringAllocated(pNode->mpDisplayListParent);
            }
            float fValue = pNode->GetProceduralProperty((pSprEntry->muMemberIndex == 1) ? 0u : 1u);
            if (pInst->GetTypeTag() == 2)
            {
                const AptRenderItemDynamicText* const pText =
                    static_cast<const AptRenderItemDynamicText*>(pInst->mpRenderItem);
                fValue += (pSprEntry->muMemberIndex == 1) ? pText->mBounds.fLeft : pText->mBounds.fTop;
            }
            return AptFloat::Create(fValue);
        }

        case 3:   return AptFloat::Create(pNode->GetProceduralProperty(2u));   // "_xscale"  @0x82B0E59C
        case 4:   return AptFloat::Create(pNode->GetProceduralProperty(3u));   // "_yscale"  @0x82B0E5A4
        case 7:   return AptFloat::Create(pNode->GetProceduralProperty(7u));   // "_alpha"   @0x82B0E4B0
        case 9:   return AptFloat::Create(pNode->GetProceduralProperty(4u));   // "_width"   @0x82B0E58C
        case 10:  return AptFloat::Create(pNode->GetProceduralProperty(5u));   // "_height"  @0x82B0E594
        case 11:  return AptFloat::Create(pNode->GetProceduralProperty(6u));   // "_rotation"@0x82B0E5AC

        case 8:   // "_visible" @0x82B0E4C0 -- the procedural read against the true/false pair
            return AptBoolean::Create(pNode->GetProceduralProperty(11u) == 1.0f);

        case 5:   // "_currentframe" @0x82B0E4E8 -- the live play-head, 1-based
            return AptFloat::Create(static_cast<float>(
                static_cast<AptCharacterSpriteInstBase*>(pInst)->mnGotoFrame + 1));

        case 6:   // "_totalframes" @0x82B0E5B4 / "_framesloaded" @0x82B0E5CC -- both read
        case 13:  // the embedded movie's frame count (nothing streams, so loaded == total)
            return AptFloat::Create(static_cast<float>(
                GetClipMovieEmbedded(pInst)->mnFrameCount));

        case 12:  // "_target" @0x82B0E550 -- the slash path name
        {
            EAStringC strPath;
            gAptActionInterpreter.getName2(pNode, &strPath);
            AptString* const pStr = AptString::Create("");
            *pStr->GetInternalString() = strPath;
            return pStr;
        }

        case 14:  // "_name" @0x82B0EFC4 -- the instance name
        {
            AptString* const pStr = AptString::Create("");
            *pStr->GetInternalString() = pNode->mInstanceName;
            return pStr;
        }

        case 16:  // "_url" @0x82B0EFD8 -- the dot path name
        {
            EAStringC strPath;
            gAptActionInterpreter.getName(pNode, &strPath);
            AptString* const pStr = AptString::Create("");
            *pStr->GetInternalString() = strPath;
            return pStr;
        }

        case 21:  // "_xmouse" @0x82B0E5E4 / case 22: "_ymouse" @0x82B0E684 -- the global
        case 22:  // mouse position pulled into this clip's space (world-matrix walk up)
        {
            AptMatrix worldMatrix = gAptIdentityMatrix;
            for (AptCIH* pWalk = pNode; pWalk != nullptr; pWalk = pWalk->mpDisplayListParent)
            {
                const AptMatrix* pPos = pWalk->mpCharacterInst->mpRenderItem->mpPositionMatrix;
                if (pPos == nullptr)
                    pPos = &gAptIdentityMatrix;
                AptRenderingContext::multMatrix(&worldMatrix, pPos, &worldMatrix);
            }
            float fValue;
            if (pSprEntry->muMemberIndex == 21)
                fValue = (static_cast<float>(gAptMouseX) - worldMatrix.tx) * worldMatrix.a
                       - (static_cast<float>(gAptMouseY) - worldMatrix.ty) * worldMatrix.b;
            else
                fValue = (static_cast<float>(gAptMouseY) - worldMatrix.ty) * worldMatrix.d
                       + (static_cast<float>(gAptMouseX) - worldMatrix.tx) * worldMatrix.c;
            return AptFloat::Create(fValue);
        }

        // ---- the pre-registered AS global-function singletons ---------------
        case 24:  return gpAptFnSetInterval;     // "setInterval"   @0x82B0E508
        case 25:  return gpAptFnClearInterval;   // "clearInterval" @0x82B0E514
        case 26:  return gpAptFnIsNaN;           // "isNaN"         @0x82B0E520
        case 27:  return gpAptFnUnescape;        // "unescape"      @0x82B0E52C
        case 28:  return gpAptFnEscape;          // "escape"        @0x82B0E538
        case 29:  return gpAptFnBoolean;         // "Boolean"       @0x82B0E544

        // ---- the lazily-created MovieClip method singletons -----------------
        case 100: return LookupMethodSingleton(gpAptNativeFn_8324E444,
                      reinterpret_cast<AptExtFunctionPtr>(&AptCIHNativeFunctionHelper::sMethod_attachMovie));            // @0x82B0E8F0
        case 101: return LookupMethodSingleton(gpAptNativeFn_8324E450,
                      reinterpret_cast<AptExtFunctionPtr>(&AptCIHNativeFunctionHelper::sMethod_duplicateMovieClip));     // @0x82B0EB74
        case 103: return LookupMethodSingleton(gpAptNativeFn_8324E440,
                      reinterpret_cast<AptExtFunctionPtr>(&AptCIHNativeFunctionHelper::sMethod_gotoAndPlay));            // @0x82B0E838
        case 104: return LookupMethodSingleton(gpAptNativeFn_8324E43C,
                      reinterpret_cast<AptExtFunctionPtr>(&AptCIHNativeFunctionHelper::sMethod_gotoAndStop));            // @0x82B0E894
        case 105: return LookupMethodSingleton(gpAptNativeFn_8324E448,
                      reinterpret_cast<AptExtFunctionPtr>(&AptCIHNativeFunctionHelper::sMethod_loadMovie));              // @0x82B0EC2C
        case 106: return LookupMethodSingleton(gpAptNativeFn_8324E47C,
                      reinterpret_cast<AptExtFunctionPtr>(&AptCIHNativeFunctionHelper::sMethod_loadVariables));          // @0x82B0E7DC
        case 107: return LookupMethodSingleton(gpAptNativeFn_8324E484,
                      reinterpret_cast<AptExtFunctionPtr>(&AptCIHNativeFunctionHelper::sMethod_play));                   // @0x82B0E94C
        case 109: return LookupMethodSingleton(gpAptNativeFn_8324E454,
                      reinterpret_cast<AptExtFunctionPtr>(&AptCIHNativeFunctionHelper::sMethod_removeMovieClip));        // @0x82B0EBD0
        case 112: return LookupMethodSingleton(gpAptNativeFn_8324E490,
                      reinterpret_cast<AptExtFunctionPtr>(&AptCIHNativeFunctionHelper::sMethod_getBytesTotal));          // @0x82B0EABC
        case 113: return LookupMethodSingleton(gpAptNativeFn_8324E488,
                      reinterpret_cast<AptExtFunctionPtr>(&AptCIHNativeFunctionHelper::sMethod_nextFrame));              // @0x82B0EA60
        case 114: return LookupMethodSingleton(gpAptNativeFn_8324E45C,
                      reinterpret_cast<AptExtFunctionPtr>(&AptCIHNativeFunctionHelper::sMethod_createTextField));        // @0x82B0ECE4
        case 115: return LookupMethodSingleton(gpAptNativeFn_8324E460,
                      reinterpret_cast<AptExtFunctionPtr>(&AptCIHNativeFunctionHelper::sMethod_getDepth));               // @0x82B0ED9C
        case 116: return LookupMethodSingleton(gpAptNativeFn_8324E478,
                      reinterpret_cast<AptExtFunctionPtr>(&AptCIHNativeFunctionHelper::sMethod_createEmptyMovieClip));   // @0x82B0EE54
        case 117: return LookupMethodSingleton(gpAptNativeFn_8324E46C,
                      reinterpret_cast<AptExtFunctionPtr>(&AptCIHNativeFunctionHelper::sMethod_getBounds));              // @0x82B0EDF8
        case 118: return LookupMethodSingleton(gpAptNativeFn_8324E474,
                      reinterpret_cast<AptExtFunctionPtr>(&AptCIHNativeFunctionHelper::sMethod_hitTest));                // @0x82B0E724
        case 119: return LookupMethodSingleton(gpAptNativeFn_8324E458,
                      reinterpret_cast<AptExtFunctionPtr>(&AptCIHNativeFunctionHelper::sMethod_removeTextField));        // @0x82B0ED40
        case 120: return LookupMethodSingleton(gpAptNativeFn_8324E464,
                      reinterpret_cast<AptExtFunctionPtr>(&AptCIHNativeFunctionHelper::sMethod_swapDepths));             // @0x82B0EEB0
        case 122: return LookupMethodSingleton(gpAptNativeFn_8324E468,
                      reinterpret_cast<AptExtFunctionPtr>(&AptCIHNativeFunctionHelper::sMethod_setMask));                // @0x82B0EF0C
        case 123: return LookupMethodSingleton(gpAptNativeFn_8324E430,
                      reinterpret_cast<AptExtFunctionPtr>(&AptCIHNativeFunctionHelper::sMethod_getNewTextFormat));       // @0x82B0F088
        case 124: return LookupMethodSingleton(gpAptNativeFn_8324E434,
                      reinterpret_cast<AptExtFunctionPtr>(&AptCIHNativeFunctionHelper::sMethod_getTextFormat));          // @0x82B0F02C
        case 125: return LookupMethodSingleton(gpAptNativeFn_8324E42C,
                      reinterpret_cast<AptExtFunctionPtr>(&AptCIHNativeFunctionHelper::sMethod_setTextFormat));          // @0x82B0F0E4
        case 126: return LookupMethodSingleton(gpAptNativeFn_8324E470,
                      reinterpret_cast<AptExtFunctionPtr>(&AptCIHNativeFunctionHelper::sMethod_startDrag));              // @0x82B0E780
        case 127: return LookupMethodSingleton(gpAptNativeFn_8324E438,
                      reinterpret_cast<AptExtFunctionPtr>(&AptCIHNativeFunctionHelper::sMethod_localToGlobal));          // @0x82B0EF68

        // FLAG (deferred natives -- the console creators bind sMethod_ bodies not yet
        // reconstructed in AptCIHNativeFunctionHelper.cpp; a null return continues the
        // findChild resolution, so the member reads `undefined` until they land):
        //   108 "prevFrame"      @0x82B0EA04 (off_8324E48C)
        //   110 "stop"           @0x82B0E9A8 (off_8324E480)
        //   111 "getBytesLoaded" @0x82B0EB18 (off_8324E494)
        //   121 "unloadMovie"    @0x82B0EC88 (off_8324E44C)
        case 108: case 110: case 111: case 121:
            return nullptr;

        case 131: // "_renderflags" @0x82B0E484 -- the render item's render-data hook name
        {
            AptRenderItemSprite* const pSprite =
                static_cast<AptRenderItemSprite*>(pInst->GetRenderItemWritable());
            AptString* const pStr = AptString::Create("");
            *pStr->GetInternalString() = pSprite->mInstanceName;
            return pStr;
        }

        default:   // table default (loc_82B0F140) -- getURL / _z / _xrotation / ...
            break;
        }
    }

    return nullptr;   // continue the findChild resolution
}

// ===========================================================================
// objectMemberSet @0x82B09E58 -- apply a built-in member WRITE on a clip.
// Returns true when the member was consumed here; false lets setVariable
// continue into the property-hash store (several cases deliberately BOTH
// apply state and return false, as shipped).
// ===========================================================================
bool AptCIH::objectMemberSet(AptValue* const pThis,
                             const AptNativeString* const pName,
                             AptValue* const pValue)
{
    AptCIH* const pNode = static_cast<AptCIH*>(pThis);
    AptCharacterInst* const pInst = pNode->mpCharacterInst;

    // ---- the TextField members (dynamic-text instances only) ---------------
    if (pInst->GetTypeTag() == 2)
    {
        const TextMembersIndex::Entry* const pEntry =
            TextMembersIndex::in_word_set(pName->GetBuffer(), pName->GetLength());
        if (pEntry != nullptr &&
            static_cast<uint32_t>(pEntry->miData) - 1u <= 19u)   // ids 1..20 (word_82145448)
        {
            AptCharacterTextInst* const pTextInst = static_cast<AptCharacterTextInst*>(pInst);
            const AptRenderItemDynamicText* const pText =
                static_cast<const AptRenderItemDynamicText*>(pInst->mpRenderItem);
            AptRenderItemDynamicText* const pWritable =
                static_cast<AptRenderItemDynamicText*>(pInst->GetRenderItemWritable());

            switch (pEntry->miData)
            {
            case 1:   // "autoSize" @0x82B09EDC -- keyword -> the box-align field
            {
                // Keyword identities PROVEN from the alphabetised .data string pool the
                // asm addresses (dword_8324E580 block: 0x8324E60C="center" < 0x8324E620=
                // "false" < 0x8324E638="left" < 0x8324E644="none" < 0x8324E6A4="right" <
                // 0x8324E6C4="true") cross-checked against the getter @0x82B0E004 (which
                // returns the SAME strings for align 0/1/2/3) AND the Apt SDK source
                // (AptCIH.cpp AptTextPropertyautoSize: left|true->Left(0), center->
                // Center(2), right->Right(1), false|none->None(3)).
                EAStringC strValue;
                pValue->toString(&strValue);
                strValue.MakeLower();

                // The relayout-direction flag: bit 3 when the box is CURRENTLY None-
                // aligned (the SDK's `szBuf != "false" || szBuf != "none"` condition is
                // always true -- the asm compiles that shape verbatim), bit 4 otherwise.
                if ((pText->mFlagsAndBorderColor & 0x3Cu) == 0x0Cu
                    && (!(strValue == EAStringC("false")) || !(strValue == EAStringC("none"))))
                    pWritable->SetStateFlags(8u);
                else
                    pWritable->SetStateFlags(0x10u);

                if (strValue == EAStringC("left") || strValue == EAStringC("true"))
                    pWritable->SetBoxAlignment(0);   // AptStringAlignment_Left
                else if (strValue == EAStringC("center"))
                    pWritable->SetBoxAlignment(2);   // AptStringAlignment_Center
                else if (strValue == EAStringC("right"))
                    pWritable->SetBoxAlignment(1);   // AptStringAlignment_Right
                else if (strValue == EAStringC("false") || strValue == EAStringC("none"))
                    pWritable->SetBoxAlignment(3);   // AptStringAlignment_None
                // (any other keyword leaves the field unchanged)

                pWritable->ClearStateFlags(1u);
                pWritable->SetStateFlags(4u);
                return true;
            }

            case 2:   // "background" @0x82B0A074
                pWritable->SetDrawsBackground(pValue->toBool());
                pWritable->ClearStateFlags(1u);
                pWritable->SetStateFlags(0x20u);
                return true;

            case 3:   // "backgroundColor" @0x82B0A0D4 (RGB above the preserved flag byte)
                pWritable->SetBackgroundColor(static_cast<uint32_t>(pValue->toInteger()));
                pWritable->ClearStateFlags(1u);
                pWritable->SetStateFlags(0x40u);
                return true;

            case 4:   // "border" @0x82B0A12C
                pWritable->SetDrawsBorder(pValue->toBool());
                pWritable->ClearStateFlags(1u);
                pWritable->SetStateFlags(0x80u);
                return true;

            case 5:   // "borderColor" @0x82B0A184
                pWritable->SetBorderColor(static_cast<uint32_t>(pValue->toInteger()));
                pWritable->ClearStateFlags(1u);
                pWritable->SetStateFlags(0x100u);
                return true;

            case 6:   // "hscroll" / case 8 "maxChars" / case 16 "type" @0x82B0A06C --
            case 8:   // accepted but not tracked: consumed with no state change
            case 16:
                return true;

            case 10:  // "multiline" @0x82B0A1DC -- applied, then STILL falls through to
                      // the clip recognizer (so the value also lands in the hash)
                pWritable->SetMultiline(pValue->toBool());
                break;

            case 11:  // "scroll" @0x82B0A2A8 -- clamped to [1, maxscroll]; falls through
            {
                const int nScroll = pValue->toInteger();
                const int nOldScroll = pText->mScroll;
                if ((pText->mStateFlags & 4u) != 0)
                    pNode->EnsureStringAllocated(pNode->mpDisplayListParent);
                pWritable->mScroll = nScroll;
                if (pText->mScroll > pTextInst->mMaxScroll)
                    pWritable->mScroll = pTextInst->mMaxScroll;
                if (pText->mScroll < 1)
                    pWritable->mScroll = 1;
                if (nOldScroll != pText->mScroll)
                {
                    pWritable->ClearStateFlags(1u);
                    pWritable->SetStateFlags(0x204u);
                }
                break;
            }

            case 12:  // "text" @0x82B0A360 -- THE TextField text store
            {
                EAStringC strValue;
                pValue->toString(&strValue);
                if (!(pText->mTextValue == strValue))   // unchanged text is a no-op
                {
                    *pWritable->GetTextValueWritable() = strValue;

                    // A bound "variable" mirrors the store into the owning clip's
                    // AS variable (resolved against the nearest sprite/animation
                    // ancestor scope).
                    if (pText->mVarValue.GetLength() != 0)
                    {
                        AptCIH* pOwner = pNode;
                        for (;;)
                        {
                            const uint32_t nOwnerTag = pOwner->mpCharacterInst->GetTypeTag();
                            if (nOwnerTag == 5 || nOwnerTag == 9)
                                break;
                            if (pOwner->mpDisplayListParent == nullptr)
                                break;
                            pOwner = pOwner->mpDisplayListParent;
                        }
                        AptString* const pBoxed = AptString::Create("");
                        *pBoxed->GetInternalString() = strValue;
                        gAptActionInterpreter.setVariable(pOwner, nullptr,
                            pWritable->GetVarValueWritable(), pBoxed, 1, 1, 0);
                    }

                    pWritable->ClearStateFlags(1u);
                    pWritable->SetStateFlags(0x204u);
                }
                return true;
            }

            case 13:  // "textColor" @0x82B0A474 -- stored with forced alpha, mirrored
            {         // into the attached TextFormat record's color when one exists
                const int nColor = pValue->toInteger();
                pWritable->mTextColor = static_cast<uint32_t>(nColor) | 0xFF000000u;
                pWritable->ClearStateFlags(1u);
                pWritable->SetStateFlags(0x400u);
                if (pText->mpTextFormat != nullptr)
                {
                    // The +0x68 slot holds the embedded TextFormat RECORD (the console
                    // stores straight into its mnColor at +8; the AptValue* typing in
                    // the header is the opaque pass-through FLAG).
                    reinterpret_cast<TextFormat*>(pWritable->GetTextFormatWritable())->mnColor =
                        static_cast<int32_t>(pText->mTextColor & 0x00FFFFFFu);
                }
                return true;
            }

            case 17:  // "variable" @0x82B0A4EC -- rebind + re-resolve the field text
            {
                EAStringC strValue;
                pValue->toString(&strValue);
                if (!(pText->mVarValue == strValue))
                {
                    *pWritable->GetVarValueWritable() = strValue;
                    pTextInst->SetText(pNode);
                    pWritable->ClearStateFlags(1u);
                    pWritable->SetStateFlags(0x204u);
                }
                return true;
            }

            case 18:  // "wordWrap" @0x82B0A570
                pWritable->SetWordWrap(pValue->toBool());
                pWritable->ClearStateFlags(1u);
                pWritable->SetStateFlags(0x1004u);
                return true;

            case 19:  // "_height" @0x82B0A5C8 -- grow the layout box (negative rejected)
            {
                if (!pValue->getIsDefined())
                    return false;
                const float fHeight = pValue->toFloat();
                if (fHeight < 0.0f)
                    return true;
                pWritable->mBounds.fBottom = pText->mBounds.fTop + fHeight;
                pWritable->ClearStateFlags(1u);
                pWritable->SetStateFlags(0x2004u);
                return true;
            }

            case 20:  // "_width" @0x82B0A644
            {
                if (!pValue->getIsDefined())
                    return false;
                const float fWidth = pValue->toFloat();
                if (fWidth < 0.0f)
                    return true;
                pWritable->mBounds.fRight = pText->mBounds.fLeft + fWidth;
                pWritable->ClearStateFlags(1u);
                pWritable->SetStateFlags(0x4004u);
                return true;
            }

            case 7:   // "length" / "maxscroll" / "textHeight" / "textWidth" -- read-only:
            case 9:   // table default (loc_82B0A1FC), falls to the clip recognizer
            case 14:
            case 15:
            default:
                break;
            }
        }
    }

    // ---- the MovieClip members (loc_82B0A1FC: sprite/animation, button, ------
    // dynamic-text or level instances)
    const uint32_t nTypeTag = pInst->GetTypeTag();
    if (nTypeTag == 5 || nTypeTag == 9 || nTypeTag == 4 || nTypeTag == 2 || nTypeTag == 15)
    {
        const SpriteMembersIndex::Entry* const pSprEntry =
            SpriteMembersIndex::in_word_set(pName->GetBuffer(), pName->GetLength());
        if (pSprEntry == nullptr)
        {
            // The console compares the unrecognized name against "this" and DISCARDS
            // the result (loc_82B0ABE4 -- a vestigial call kept for fidelity).
            (void)_stricmp(pName->GetBuffer(), "this");
            return false;
        }

        const int32_t nId = static_cast<int32_t>(pSprEntry->muMemberIndex);

        // ---- the AS event-handler members (ids 200..217, byte_82145410) -----
        if (nId > 131)
        {
            if (static_cast<uint32_t>(nId - 200) <= 17u)
            {
                // onKillFocus (206) / onSetFocus (216): table -> the return-0 arm.
                if (nId == 206 || nId == 216)
                    return false;

                // Both handler arms reject text instances outright.
                if (nTypeTag == 2)
                    return false;

                const int32_t nEventBit = gAptMemberIndexToEventBit[nId - 200];

                // Store the handler function under its member name (the per-
                // instance property hash, vtbl slot GetNativeHashVirtual).
                pNode->GetNativeHashVirtual()->Set(*pName, pValue);

                if (nId == 200 || nId == 203 || nId == 207 || nId == 217)
                {
                    // onData / onEnterFrame / onLoad / onUnload @loc_82B0A924.
                    if (pValue != nullptr && pValue->getIsDefined())
                    {
                        pNode->SetEventHandler(nEventBit);
                        if (nId == 203)   // onEnterFrame drives a per-frame tick
                            pNode->SetDirtyState(true, true);
                    }
                    else
                    {
                        pNode->RemoveEventHandler(nEventBit);
                    }
                    return true;
                }

                // The mouse / key / drag family @loc_82B0A9EC.
                if (pValue != nullptr && pValue->getIsDefined())
                {
                    pNode->SetEventHandler(nEventBit);
                    // A responding sprite / custom control / animation joins the
                    // director's input set (idempotent).
                    if (nTypeTag == 5 || nTypeTag == 16 || nTypeTag == 9)
                        AddNodeToInputSet(pNode);
                    return true;
                }

                pNode->RemoveEventHandler(nEventBit);
                if (nTypeTag == 5 || nTypeTag == 16)
                {
                    // With no remaining mouse-ish clip events (the authored clip-
                    // action mask) and no remaining AS handler (mask 0x200C0), the
                    // node leaves the input set.
                    AptCharacterSpriteInstBase* const pSpriteInst =
                        static_cast<AptCharacterSpriteInstBase*>(pInst);
                    if ((pSpriteInst->mnClipActionFlags & 0x0200C000u) == 0
                        && pNode->HasEventMember(0x200C0) == 0)
                        RemoveNodeFromInputSet(pNode);
                }
                return true;
            }
            return false;   // e.g. onMouseWheel (218)
        }

        // ---- "_renderflags" (id 131, inline arm) ----------------------------
        if (nId == 131)
        {
            AptRenderItemSprite* const pSprite =
                static_cast<AptRenderItemSprite*>(pInst->GetRenderItemWritable());
            if (pValue != nullptr && pValue->isString())
            {
                AptValue* pStrValue = pValue;
                if (pValue->getVtblIndex() != AptVFT_StringValue)   // boxed String object
                    pStrValue = static_cast<AptStringObject*>(pValue)->GetBoxedString();
                pSprite->mInstanceName =
                    *static_cast<AptString*>(pStrValue)->GetInternalString();
            }
            return false;   // applied, but NOT consumed (also stored to the hash)
        }

        // ---- the procedural properties + _name (ids 1..14, word_82145428) ---
        if (static_cast<uint32_t>(nId - 1) <= 13u)
        {
            switch (nId)
            {
            case 1:   // "_x" @0x82B0A6C0 / case 2: "_y" @0x82B0A708 -- a text
            case 2:   // instance's store lands past its 2.0 box inset (flt_82001D9C)
            {
                if (!pValue->getIsDefined())
                    return false;
                float fValue = pValue->toFloat();
                if (pInst->GetTypeTag() == 2)
                    fValue += 2.0f;
                pNode->SetProceduralProperty((nId == 1) ? 0u : 1u, fValue, true);
                return true;
            }

            case 3:   // "_xscale"  @0x82B0A77C
            case 4:   // "_yscale"  @0x82B0A798
            case 7:   // "_alpha"   @0x82B0A760
            case 8:   // "_visible" @0x82B0A7B4
            case 9:   // "_width"   @0x82B0A7D0
            case 10:  // "_height"  @0x82B0A7EC
            case 11:  // "_rotation"@0x82B0A744
            {
                if (!pValue->getIsDefined())
                    return false;
                const float fValue = pValue->toFloat();
                uint32_t nSelector;
                switch (nId)
                {
                case 3:  nSelector = 2u;  break;
                case 4:  nSelector = 3u;  break;
                case 7:  nSelector = 7u;  break;
                case 8:  nSelector = 11u; break;
                case 9:  nSelector = 4u;  break;
                case 10: nSelector = 5u;  break;
                default: nSelector = 6u;  break;   // 11 (_rotation)
                }
                pNode->SetProceduralProperty(nSelector, fValue, true);
                return true;
            }

            case 14:  // "_name" @0x82B0A808 -- re-key the parent's property hash
            {
                if (!pValue->getIsDefined())
                    return false;
                AptCIH* const pParent = pNode->mpDisplayListParent;
                if (pParent != nullptr)
                {
                    pNode->AddRef();   // keep alive across the un-key / re-key
                    pParent->mpCharacterInst->mpProperties->Unset(pNode->mInstanceName);
                    pValue->toString(&pNode->mInstanceName);
                    pParent->mpCharacterInst->mpProperties->Set(pNode->mInstanceName, pNode);
                    pNode->Release();
                }
                else
                {
                    pValue->toString(&pNode->mInstanceName);
                }
                return true;
            }

            case 5:   // "_currentframe" / "_totalframes" / "_target" /
            case 6:   // "_framesloaded" -- read-only (table -> the return-0 arm)
            case 12:
            case 13:
            default:
                return false;
            }
        }
        return false;   // ids 15..130: no table entry
    }

    return false;
}
