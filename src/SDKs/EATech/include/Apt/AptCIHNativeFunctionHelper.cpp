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
#include "SDKs/EATech/include/Apt/AptValue/AptFloat.h"            // AptFloat::Create / GetFloat
#include "SDKs/EATech/include/Apt/AptNativeHash.h"                // localToGlobal _x/_y point hash
#include "SDKs/EATech/include/Apt/AptRenderingContext.h"          // multMatrix (local->world concat)
#include "SDKs/EATech/include/Apt/AptStd/AptMatrix.h"             // the localToGlobal world-matrix scratch
#include "SDKs/EATech/include/Apt/AptString/EAString.h"           // EAStringC (the AS-arg string scratch)
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"         // the AS VM (clone / loadVariables this-ptr)

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

// ---------------------------------------------------------------------------
// FLAG (homed by AptActionInterpreter, not yet built): the process-wide AS VM.
// The X360 passes its base address (&dword_8324E760 == the interpreter's
// mnStackTop slot [c:0x00]) as the `this` to the clone/loadVariables behavioural
// entry points below; declared as the extern global so the calls keep the exact
// shape. (The same singleton whose stacks back gppAptNativeArgStack /
// gnAptNativeArgCount above.)
// ---------------------------------------------------------------------------
extern AptActionInterpreter gAptActionInterpreter;   // &dword_8324E760

// FLAG: AptActionInterpreter::_doCloneSprite (the AS duplicateMovieClip core) is
// not yet homed; declared as an extern shim so this dispatcher keeps the exact
// (interpreter, owner, 0, parent, nameValue, depth, initObject) call shape.
extern AptValue* AptActionInterpreter_doCloneSprite(AptActionInterpreter* pInterp,
                                                    AptValue* pOwner, int a3, AptValue* pParent,
                                                    AptValue* pNameValue, int nDepth, AptValue* pInitObject);

// FLAG: AptActionInterpreter::loadVariables (the AS loadVariables core) is not yet
// homed; declared as an extern shim, preserving the exact (interpreter, node, 0,
// &urlString) call shape.
extern void AptActionInterpreter_loadVariables(AptActionInterpreter* pInterp,
                                               AptValue* pNode, int a3, EAStringC* pURL);

// ===========================================================================
// sMethod_duplicateMovieClip @0x82B0DEE8 -- AS duplicateMovieClip(name, depth
// [, initObject]): clone this node into its parent under a new instance name at a
// fresh depth (AS depth + the 0x4000 bias), forwarding to the interpreter's
// _doCloneSprite core. The init object (arg 2) is optional -- absent (< 3 args)
// it passes null. Returns the new clone handle (or undefined).
// ===========================================================================
AptValue* AptCIHNativeFunctionHelper::sMethod_duplicateMovieClip(AptValue* pContext, int nArgCount)
{
    AptValue* const pNameValue = gppAptNativeArgStack[gnAptNativeArgCount - 1];        // arg 0: new name
    AptValue* const pInitObject = (nArgCount < 3)                                       // arg 2: optional init object
        ? nullptr
        : gppAptNativeArgStack[gnAptNativeArgCount - 3];
    const int nDepth = gppAptNativeArgStack[gnAptNativeArgCount - 2]->toInteger();      // arg 1: AS depth

    return AptActionInterpreter_doCloneSprite(&gAptActionInterpreter,
                                              pContext, 0, pContext,
                                              pNameValue, nDepth + 0x4000, pInitObject);
}

// ===========================================================================
// sMethod_loadVariables @0x82B09C10 -- AS loadVariables(url): with at least one
// argument, render arg 0 to its URL string and hand it to the interpreter's
// loadVariables core (node, the resolved URL); the EAStringC scratch's lifetime
// matches the X360's explicit InitFromBuffer / DecreaseInternalRefCount pair.
// Always returns undefined.
// ===========================================================================
AptValue* AptCIHNativeFunctionHelper::sMethod_loadVariables(AptValue* pContext, int nArgCount)
{
    if (nArgCount > 0)
    {
        EAStringC strURL;                                                     // the X360 &unk_82F72FF8 empty sentinel
        gppAptNativeArgStack[gnAptNativeArgCount - 1]->toString(&strURL);     // arg 0 -> the URL
        AptActionInterpreter_loadVariables(&gAptActionInterpreter, pContext, 0, &strURL);
    }
    return gpUndefinedValue;
}

// ---------------------------------------------------------------------------
// FLAG: AptCIH::GetWorldBounds (X360 sub_82AE2C58, un-homed) -- compute a scene
// node's world-space AABB into pOutRect (left,top,right,bottom). Declared as an
// extern shim so the AS hitTest / getBounds keep the exact (node, &rect) call shape.
// ---------------------------------------------------------------------------
extern void AptCIH_GetWorldBounds(AptValue* pNode, float* pOutRect);   // sub_82AE2C58

// FLAG: the shape-precise point hit-test (X360 indirect through dword_8324E8A4, an
// AptCharacter render-method slot) -- "is local point (x,y) inside the node's drawn
// shape?". Declared as an extern shim (the indirect target is a render-data method
// not yet homed), preserving the (node, x, y) call shape.
extern int AptCIH_ShapeHitTest(AptValue* pNode, float fX, float fY);   // (*dword_8324E8A4)

// Local: the X360 hitTest receiver/arg type gate -- value type 12 (CharacterInst
// handle) OR 37 (CIHNone), WITHOUT the defined bit (the asm tests only meValueType,
// unlike IsClipHandleOrCIHNone). A defined-or-not clip handle / none counts.
static inline bool IsClipHandleOrCIHNoneAnyState(const AptValue* pValue)
{
    const AptVirtualFunctionTable_Indices eType = pValue->getVtblIndex();
    return eType == AptVFT_CharacterInstHandle || eType == AptVFT_CIHNone;
}

// ===========================================================================
// sMethod_hitTest @0x82AED730 -- AS hitTest(): two overloads.
//   .hitTest(target)        (1 arg)  -- bounding-box overlap with another clip.
//   .hitTest(x, y [,shape]) (>=2)    -- is the world point (x,y) over this clip;
//                                       with a truthy 3rd arg, shape-precise.
// Returns an AptInteger 0/1.
// ===========================================================================
AptValue* AptCIHNativeFunctionHelper::sMethod_hitTest(AptValue* pContext, int nArgCount)
{
    int nResult;

    if (nArgCount == 1)
    {
        AptValue* const pTarget = gppAptNativeArgStack[gnAptNativeArgCount - 1];   // arg 0: the other clip
        nResult = 0;
        if (IsClipHandleOrCIHNoneAnyState(pTarget))
        {
            float fThis[4];     // this node's world AABB (left,top,right,bottom)
            float fTarget[4];   // the target's world AABB
            AptCIH_GetWorldBounds(pContext, fThis);
            AptCIH_GetWorldBounds(pTarget, fTarget);
            // box overlap: target.left <= this.right && target.right >= this.left
            //           && target.bottom >= this.top && target.top <= this.bottom
            if (fTarget[0] <= fThis[2] && fTarget[2] >= fThis[0]
                && fTarget[3] >= fThis[1] && fTarget[1] <= fThis[3])
            {
                nResult = 1;
            }
        }
    }
    else if (nArgCount <= 1)
    {
        nResult = 0;   // no args
    }
    else
    {
        const float fX = gppAptNativeArgStack[gnAptNativeArgCount - 1]->toFloat();   // arg 0: x
        const float fY = gppAptNativeArgStack[gnAptNativeArgCount - 2]->toFloat();   // arg 1: y

        if (nArgCount > 2 && gppAptNativeArgStack[gnAptNativeArgCount - 3]->toInteger())
        {
            nResult = AptCIH_ShapeHitTest(pContext, fX, fY);   // arg 2 truthy -> shape-precise
        }
        else
        {
            float fThis[4];   // this node's world AABB
            AptCIH_GetWorldBounds(pContext, fThis);
            nResult = 1;
            if (fX < fThis[0] || fX > fThis[2] || fY < fThis[1] || fY > fThis[3])
                nResult = 0;
        }
    }
    return AptInteger::Create(nResult);
}

// ===========================================================================
// sMethod_localToGlobal @0x82AF5CE8 -- AS localToGlobal(point): transform the
// point object's {x,y} from this node's local space into global (stage) space.
// Reads the point's "x"/"y" hash entries, builds a translation matrix, concatenates
// every display-list ancestor's position transform up the tree, then writes the
// transformed translation back into the point's "x"/"y". Returns undefined.
// ===========================================================================
AptValue* AptCIHNativeFunctionHelper::sMethod_localToGlobal(AptValue* pContext, int nArgCount)
{
    if (nArgCount == 0)
        return gpUndefinedValue;

    AptValue* const pPoint = gppAptNativeArgStack[gnAptNativeArgCount - 1];   // arg 0: the point object

    // The point object's "x"/"y" property keys (X360 unk_82143BF4 / unk_82143BF8).
    EAStringC strKeyX("x");
    EAStringC strKeyY("y");

    // FLAG: the point's embedded per-instance native hash (the X360 reads it at the
    // value's +8 -- an AptValueWithHash's hash sub-object). Declared as an extern
    // shim so the key lookups/stores stay typed without re-narrowing the value here.
    extern AptNativeHash* AptValue_EmbeddedNativeHash(AptValue* pValue);   // pValue + 8
    AptNativeHash* const pHash = AptValue_EmbeddedNativeHash(pPoint);

    AptValue* const pValX = pHash->Lookup(strKeyX);
    AptValue* const pValY = pHash->Lookup(strKeyY);

    // Seed the local matrix from identity, then poke in the point's local (x,y) as
    // the translation (tx/ty); the 2x2 stays identity.
    AptMatrix mWorld;
    mWorld.AptMatrixCopy(&gAptIdentityMatrix);   // X360 flt_8324E2B0 seed
    mWorld.tx = pValX->c_float()->GetFloat();
    mWorld.ty = pValY->c_float()->GetFloat();

    // Concatenate every ancestor's position transform, walking up the display list.
    for (AptValue* pAncestor = pContext; pAncestor; )
    {
        AptCIH* const pNode = static_cast<AptCIH*>(pAncestor);
        const AptMatrix* pPos = pNode->GetCharacterInst()->GetRenderItem()->GetPositionMatrixConst();
        if (!pPos)
            pPos = &gAptIdentityMatrix;   // null position matrix -> identity (flt_8324E2B0)
        AptRenderingContext::multMatrix(pPos, &mWorld, &mWorld);
        pAncestor = pNode->GetDisplayListParent();
    }

    // Write the transformed world (x,y) back into the point's "x"/"y".
    pHash->Set(strKeyX, AptFloat::Create(mWorld.tx));
    pHash->Set(strKeyY, AptFloat::Create(mWorld.ty));
    return gpUndefinedValue;
}
