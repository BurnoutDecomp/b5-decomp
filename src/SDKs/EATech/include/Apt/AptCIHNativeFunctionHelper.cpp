// ===========================================================================
// EATech Apt -- AptCIHNativeFunctionHelper: the AS movie-clip native methods.
//
// DECOMPILED from the X360 ARTIST.XEX (0x82AD6F80 .. 0x82B0DF68). Each sMethod_*
// is an ActionScript native-method callback the apt VM's CallMethod dispatch
// invokes as f(pContext, nArgCount), where pContext is the AptCIH the method was
// called on; arguments are read off the global native-method argument stack
// (gppAptNativeArgStack[gnAptNativeArgCount-1-i], i=0 the last pushed), and the
// result is an AptValue* (the shared `undefined` singleton when there is none).
//
// This file carries the subset whose callees are all already homed as named APIs
// (the trivial dispatchers + the predicate/accessor-driven methods). The methods
// that chase the not-yet-named internals of AptRenderItem / TextFormat / the
// display-list state machine, or that call the still-unhomed AptCIH /
// AptActionInterpreter behavioural entry points (InsertChild / SetProceduralProperty
// / findCharacterInLibrary / _doCloneSprite / loadVariables / getName /
// AptDisplayListState::*), are left for a follow-on pass so their offsets are not
// fabricated here.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptCIHNativeFunctionHelper.h"

#include "SDKs/EATech/include/Apt/AptCIH.h"                       // AptCIH (the scene node)
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"             // GetTypeTag / GetRenderItem*
#include "SDKs/EATech/include/Apt/AptCharacterSpriteInstBase.h"   // mnGotoFrame / mnClipActionFlags / mDisplayList
#include "SDKs/EATech/include/Apt/AptRenderItem.h"                // GetDepth (render-item depth)
#include "SDKs/EATech/include/Apt/AptDisplayList.h"               // removeClonedObject
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"          // AptInteger::Create

// ---------------------------------------------------------------------------
// FLAG (homed by the apt VM native-call dispatch): the global native-method arg
// stack (X360 off_8324E768 = gAptActionInterpreter.mpStack, dword_8324E760 = its
// mnStackTop). The i-th AS argument (i=0 = last pushed) is
// gppAptNativeArgStack[gnAptNativeArgCount - 1 - i]. (Committed externs in the
// sibling AptActionInterpreterBuiltins.cpp.)
// ---------------------------------------------------------------------------
extern AptValue** gppAptNativeArgStack;   // off_8324E768
extern int        gnAptNativeArgCount;    // dword_8324E760

// FLAG (homed by the AS-globals layer): the shared "undefined" value (off_8324D814).
extern AptValue* gpUndefinedValue;

// ---------------------------------------------------------------------------
// Local: the X360 "defined movie-clip handle, or a CIHNone" predicate that opens
// most of these methods -- `type == AptVFT_CharacterInstHandle && isDefined` or
// `type == AptVFT_CIHNone`. (The console packs the value type in the low 7 bits of
// the AptValue bitfield word and the defined flag at bit 27; modelled here through
// the named AptValue accessors.)
// ---------------------------------------------------------------------------
static inline bool IsClipHandleOrCIHNone(const AptValue* pValue)
{
    const AptVirtualFunctionTable_Indices eType = pValue->getVtblIndex();
    return (eType == AptVFT_CharacterInstHandle && pValue->getIsDefined())
        || eType == AptVFT_CIHNone;
}

// The clip's play-head "playing" bit (mnClipActionFlags bit 6 / 0x40); set by
// sMethod_play, cleared by sMethod_nextFrame. (Console *(spriteBase+0x14) & 0x40.)
static const uint32_t KU_CLIP_PLAYING = 0x40u;

// ===========================================================================
// sMethod_getDepth @0x82AED6D8 -- AS getDepth(): the node's display depth (the
// render item's depth minus the 0x4000 AS depth bias), or undefined when the
// receiver is not a placed movie-clip / CIHNone.
// ===========================================================================
AptValue* AptCIHNativeFunctionHelper::sMethod_getDepth(AptValue* pContext, int /*nArgCount*/)
{
    AptCIH* const pNode = static_cast<AptCIH*>(pContext);
    if (IsClipHandleOrCIHNone(pNode))
        return AptInteger::Create(pNode->GetCharacterInst()->GetRenderItem()->GetDepth() - 0x4000);
    return gpUndefinedValue;
}

// ===========================================================================
// sMethod_play @0x82AE2AC8 -- AS play(): start the play-head of a movie-clip /
// animation node (character type tag 5 or 9). Sets the clip's "playing" bit
// (mnClipActionFlags |= 0x40) and dirties the node. Always returns undefined.
// ===========================================================================
AptValue* AptCIHNativeFunctionHelper::sMethod_play(AptValue* pContext, int /*nArgCount*/)
{
    AptCIH* const pNode = static_cast<AptCIH*>(pContext);
    AptCharacterInst* const pInst = pNode->GetCharacterInst();
    const uint32_t nTypeTag = pInst->GetTypeTag();

    if (nTypeTag == 5 || nTypeTag == 9)
    {
        // type tag 5/9 -> the instance is an AptCharacterSpriteInstBase.
        AptCharacterSpriteInstBase* const pSprite = static_cast<AptCharacterSpriteInstBase*>(pInst);
        pSprite->mnClipActionFlags |= KU_CLIP_PLAYING;
        pNode->SetDirtyState(true, true);
    }
    return gpUndefinedValue;
}

// ===========================================================================
// sMethod_nextFrame @0x82B0D568 -- AS nextFrame(): advance the play-head one frame
// (jumpToFrame(mnGotoFrame + 1)) and clear the clip's "playing" bit
// (mnClipActionFlags &= ~0x40). Always returns undefined.
// ===========================================================================
AptValue* AptCIHNativeFunctionHelper::sMethod_nextFrame(AptValue* pContext, int /*nArgCount*/)
{
    AptCIH* const pNode = static_cast<AptCIH*>(pContext);
    AptCharacterSpriteInstBase* const pSprite =
        static_cast<AptCharacterSpriteInstBase*>(pNode->GetCharacterInst());

    // FLAG: AptCIH::jumpToFrame (un-homed play-head seek) -- declared as an extern
    // shim so this method compiles with the exact (node, mnGotoFrame + 1) seek.
    extern void AptCIH_jumpToFrame(AptCIH* pNode, int nFrame);   // AptCIH::jumpToFrame @0x82B0...
    AptCIH_jumpToFrame(pNode, pSprite->mnGotoFrame + 1);

    pSprite->mnClipActionFlags &= ~KU_CLIP_PLAYING;
    return gpUndefinedValue;
}

// ===========================================================================
// sMethod_setMask @0x82AF8EE0 -- AS setMask(maskClip): make the single argument the
// mask for this node. With exactly one argument (or when the receiver itself is not
// a clip handle / CIHNone), the argument is read as the candidate; it must be a clip
// handle / CIHNone, and -- only when it is a DEFINED clip handle -- AptCIH::SetMask
// is invoked with it. Always returns undefined.
// ===========================================================================
AptValue* AptCIHNativeFunctionHelper::sMethod_setMask(AptValue* pContext, int nArgCount)
{
    // The X360 short-circuits straight to the argument path when there is exactly
    // one argument; otherwise it first requires the receiver to be a clip / CIHNone.
    if (nArgCount != 1 && IsClipHandleOrCIHNone(pContext))
        return gpUndefinedValue;

    AptValue* const pArg = gppAptNativeArgStack[gnAptNativeArgCount - 1];
    if (IsClipHandleOrCIHNone(pArg) && pArg->getIsDefined())   // defined clip OR defined CIHNone (asm has no vtbl re-test)
    {
        // The receiver is the MASTER; SetMask takes the slave node it masks.
        static_cast<AptCIH*>(pContext)->SetMask(static_cast<AptCIH*>(pArg));
    }
    return gpUndefinedValue;
}

// ===========================================================================
// sMethod_gotoAndPlay @0x82B0D438 / sMethod_gotoAndStop @0x82B0D430 -- AS
// gotoAndPlay/gotoAndStop(frame): tail-call AptCIH::_gotoAndX with the play flag
// (1 = play, 0 = stop).
// ===========================================================================
AptValue* AptCIHNativeFunctionHelper::sMethod_gotoAndPlay(AptValue* pContext, int nArgCount)
{
    // FLAG: AptCIH::_gotoAndX (the shared goto-frame core) is not yet homed;
    // declared as an extern shim so this dispatcher keeps the exact
    // (pContext, nArgCount, bPlay=1) tail-call shape.
    extern AptValue* AptCIH_gotoAndX(AptValue* pContext, int nArgCount, int bPlay);   // AptCIH::_gotoAndX
    return AptCIH_gotoAndX(pContext, nArgCount, 1);
}

AptValue* AptCIHNativeFunctionHelper::sMethod_gotoAndStop(AptValue* pContext, int nArgCount)
{
    // FLAG: see sMethod_gotoAndPlay -- the same un-homed _gotoAndX core, bPlay=0.
    extern AptValue* AptCIH_gotoAndX(AptValue* pContext, int nArgCount, int bPlay);   // AptCIH::_gotoAndX
    return AptCIH_gotoAndX(pContext, nArgCount, 0);
}

// ===========================================================================
// sMethod_startDrag @0x82AD6F80 -- AS startDrag(): the X360 body indirect-calls the
// receiver's first vtable slot ((**pContext)(pContext)) and returns undefined. That
// vtable slot is AddRef (AptValue vtbl[0]); the call is a no-result self-AddRef.
// ===========================================================================
AptValue* AptCIHNativeFunctionHelper::sMethod_startDrag(AptValue* pContext, int /*nArgCount*/)
{
    // FLAG: the X360 dispatches through pContext's vtbl[0] verbatim. On PC the first
    // virtual is AddRef; the faithful call is the same indirect dispatch expressed
    // through the named virtual.
    pContext->AddRef();
    return gpUndefinedValue;
}

// ===========================================================================
// sMethod_removeMovieClip @0x82B09AF0 -- AS removeMovieClip(): resolve the receiver
// to its backing object, and (when it is a defined clip handle / CIHNone) drop the
// cloned node from its display-list parent's child list. Always returns undefined.
// ===========================================================================
AptValue* AptCIHNativeFunctionHelper::sMethod_removeMovieClip(AptValue* pContext, int /*nArgCount*/)
{
    AptValue* pResolved = nullptr;
    // FLAG: AptActionInterpreter::valueToObject (un-homed; coerces the receiver to
    // its object value, writing the result through the out-param) -- declared as an
    // extern shim, preserving the exact (pInterp=pContext, 0, pContext, &out) shape.
    extern void AptActionInterpreter_valueToObject(AptValue* pInterp, int a2,
                                                   AptValue* pValue, AptValue** ppOut);
    AptActionInterpreter_valueToObject(pContext, 0, pContext, &pResolved);

    if (pResolved && IsClipHandleOrCIHNone(pResolved))
    {
        // The cloned node lives in its display-list parent's child display list
        // (the sprite-base instance's embedded AptDisplayList, console offset +0x1C);
        // remove the node at this clone's depth from it.
        AptCIH* const pCIH = static_cast<AptCIH*>(pResolved);
        AptCharacterSpriteInstBase* const pParentSprite =
            static_cast<AptCharacterSpriteInstBase*>(pCIH->GetDisplayListParent()->GetCharacterInst());
        pParentSprite->mDisplayList.removeClonedObject(pCIH);
    }
    return gpUndefinedValue;
}

// ===========================================================================
// sMethod_removeTextField @0x82B09B80 -- AS removeTextField(): byte-identical body
// to removeMovieClip (the X360 emits the same resolve-then-removeClonedObject path).
// ===========================================================================
AptValue* AptCIHNativeFunctionHelper::sMethod_removeTextField(AptValue* pContext, int nArgCount)
{
    return sMethod_removeMovieClip(pContext, nArgCount);
}
