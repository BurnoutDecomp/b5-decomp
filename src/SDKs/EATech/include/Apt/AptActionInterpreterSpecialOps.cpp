// ===========================================================================
// EATech Apt -- small standalone ActionScript opcodes: the "push a special value"
// ops (this / _global / the timer), the register store, and the function return.
//   DECOMPILED from the X360 ARTIST:
//     PushThis           @0x82AF4018  (0x56)  -- push the string "this"
//     PushGlobal         @0x82AF40A0  (0x58)  -- push the string "_global"
//     PushThisVariable   @0x82B05550  (0x70)  -- push the resolved `this` value
//     PushGlobalVariable @0x82ADE798  (0x71)  -- push the _global object
//     GetTimer           @0x82AE9CA0  (0x34)  -- push AptInteger(elapsed ms)
//     StoreRegister      @0x82ADE468  (0x87)  -- store the stack top into a register
//     Return             @0x82AD9208  (0x3E)  -- end the run (set ctx.mbStop)
//
// PushThis/PushGlobal build an AptString holding the pooled key ("this"/"_global")
// -- the NAME, to be resolved by a following GetVariable. PushThisVariable resolves
// `this` directly through getVariable (run scope mpCIH + target slot). The standard
// SWF opcodes match exactly (GetTime 0x34, Return 0x3E, StoreRegister 0x87); the
// 0x56/0x58/0x70/0x71 are EA's extended set.
//
// FLAG -- string-pool constants + the host clock (wired at AptInit):
//   gAptKeyThis (unk_8324E6C0 "this"), gAptKeyGlobal (unk_8324E59C "_global") are
//   pooled EAStringC keys; gAptTimerMs (dword_8324D820) is the host millisecond
//   clock the runtime advances each frame. The AptString::Create seed (unk_820046A7)
//   is the console's seed-prefix const -- irrelevant here (the str is overwritten).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptTarget.h"            // gpAptTarget (the drag-state view)
#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"   // the director drag block
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"     // AptString::Create / GetInternalString
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"    // AptInteger::Create
#include "SDKs/EATech/include/Apt/AptString/EAString.h"     // EAStringC::operator=
#include "SDKs/EATech/include/Apt/AptObject.h"              // AptValueWithHash (gpAptGlobalFallback upcast)
#include "SDKs/EATech/include/Apt/AptGlobal.h"              // gpAptGlobalFallback
#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h"  // AptScriptFunctionBase::SetRegisterValue
#include "SDKs/EATech/include/Apt/AptCIH.h"                 // AptCIH : AptValueGC (mpCIH -> AptValue* upcast)
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"            // GetCharacterInst / GetTypeTag
#include "SDKs/EATech/include/Apt/AptCharacterSpriteInstBase.h"  // mnClipActionFlags / mDisplayList
#include "SDKs/EATech/include/Apt/AptDisplayList.h"              // removeClonedObject

#include <cstdint>

// FLAG (string-pool constants -- wired at AptInit; gAptKeyThis already used by
// AptScriptFunction2): the pooled "this"/"_global" keys.
extern const EAStringC gAptKeyThis;     // unk_8324E6C0
extern const EAStringC gAptKeyGlobal;   // unk_8324E59C

// FLAG (host clock -- advanced by the runtime each frame; wired at AptInit): the
// elapsed-milliseconds counter GetTimer reads (console dword_8324D820).
extern int32_t gAptTimerMs;

// ---------------------------------------------------------------------------
// PushThis @0x82AF4018 (0x56) -- push the string "this".
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushThis(AptActionInterpreter* pInterp,
                                                      LocalContextT* /*pContext*/)
{
    AptString* pStr = AptString::Create("");          // FLAG: seed const, overwritten below
    *pStr->GetInternalString() = gAptKeyThis;
    pInterp->mpStack[pInterp->mnStackTop++] = pStr;    // inlined stackPush
    pStr->AddRef();
}

// ---------------------------------------------------------------------------
// PushGlobal @0x82AF40A0 (0x58) -- push the string "_global".
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushGlobal(AptActionInterpreter* pInterp,
                                                        LocalContextT* /*pContext*/)
{
    AptString* pStr = AptString::Create("");          // FLAG: seed const, overwritten below
    *pStr->GetInternalString() = gAptKeyGlobal;
    pInterp->mpStack[pInterp->mnStackTop++] = pStr;
    pStr->AddRef();
}

// ---------------------------------------------------------------------------
// PushThisVariable @0x82B05550 (0x70) -- resolve `this` and push it.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushThisVariable(AptActionInterpreter* pInterp,
                                                              LocalContextT* pContext)
{
    AptValue* pResult = pInterp->getVariable(pContext->mpCIH, pContext->mpPendingReleaseValue,
                                             &gAptKeyThis, 1, 1, 0);
    pInterp->mpStack[pInterp->mnStackTop++] = pResult;
    pResult->AddRef();
}

// ---------------------------------------------------------------------------
// PushGlobalVariable @0x82ADE798 (0x71) -- push the _global object directly.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPushGlobalVariable(AptActionInterpreter* pInterp,
                                                                LocalContextT* /*pContext*/)
{
    pInterp->mpStack[pInterp->mnStackTop++] = gpAptGlobalFallback;
    gpAptGlobalFallback->AddRef();
}

// ---------------------------------------------------------------------------
// GetTimer @0x82AE9CA0 (0x34) -- push the elapsed-milliseconds count.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionGetTimer(AptActionInterpreter* pInterp,
                                                      LocalContextT* /*pContext*/)
{
    AptInteger* pVal = AptInteger::Create(gAptTimerMs);
    pInterp->mpStack[pInterp->mnStackTop++] = pVal;
    pVal->AddRef();
}

// ---------------------------------------------------------------------------
// StoreRegister @0x82ADE468 (0x87) -- store the stack top into register N (an
// inline 4-byte-aligned register index; the value stays on the stack).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionStoreRegister(AptActionInterpreter* pInterp,
                                                           LocalContextT* pContext)
{
    const int32_t* pRegIndex = reinterpret_cast<const int32_t*>(
        (reinterpret_cast<uintptr_t>(pContext->mpProgramCounter) + 7) & ~static_cast<uintptr_t>(7));   // 8-aligned (GUIAPT64)
    pContext->mpProgramCounter = reinterpret_cast<const unsigned char*>(pRegIndex + 1);
    AptScriptFunctionBase::SetRegisterValue(*pRegIndex, pInterp->mpStack[pInterp->mnStackTop - 1]);
}

// ---------------------------------------------------------------------------
// Return @0x82AD9208 (0x3E) -- end the action run (the bounded sub-stream leaves
// the return value on the operand stack; runStream observes mbStop and unwinds).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionReturn(AptActionInterpreter* /*pInterp*/,
                                                    LocalContextT* pContext)
{
    pContext->mbStop = true;
}

// ===========================================================================
// Timeline / sprite-control opcodes (GotoFrame / NextFrame / Play / Stop /
// CloneSprite / RemoveSprite / SetTarget / SetTarget2 / TargetPath).
//   DECOMPILED from the X360 ARTIST:
//     GotoFrame    @0x82B0C480  (the inline-frame jump)
//     NextFrame    @0x82B0C3E0
//     Play         @0x82ADD690      Stop        @0x82ADD768
//     CloneSprite  @0x82B0DE08      RemoveSprite@0x82B08D28
//     SetTarget    @0x82B093C8      SetTarget2  @0x82B08A88
//     TargetPath   @0x82B09050
//
// These drive the scene node bound to the run (the run scope ctx.mpCIH and the
// "current target" slot ctx.mpPendingReleaseValue). The X360 reaches the play-head
// state through the node's character instance; when the type tag confirms a sprite/
// movie-clip the instance is an AptCharacterSpriteInstBase, so its play-head flags
// (mnClipActionFlags) / goto-frame (mnGotoFrame) / child display list (mDisplayList)
// are accessed by NAMED member here (the console reads them at +0x14/+0x10/+0x1C off
// the instance). The character-type tag check `(mTypeFlags & 0xFC000000) != 0x3C000000`
// is `GetTypeTag() != 15` (i.e. NOT a level instance).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

// The clip's play-head "playing" bit (bIsPlaying, x64 mnClipActionFlags bit 25): set by Play,
// cleared by Stop / NextFrame / GotoFrame. (Console *(spriteBase + 0x14) & 0x40.)
static const uint32_t KU_CLIP_PLAYING = 0x2000000u;   // bIsPlaying, x64 bit 25 (X360 reversed bit 6)

// The level-instance character type tag (mTypeFlags >> 26 == 15): the timeline ops
// are no-ops on it. (Console: (mTypeFlags & 0xFC000000) == 0x3C000000.)
static const uint32_t KU_TYPETAG_LEVEL = 15u;

// ---------------------------------------------------------------------------
// Local: the X360 "defined movie-clip handle, or a CIHNone" predicate that gates
// the timeline ops -- `type == AptVFT_CharacterInstHandle && isDefined` or
// `type == AptVFT_CIHNone`. (Console: the low 7 bits of the AptValue bitfield word
// hold the value type, bit 27 the defined flag; modelled through the named accessors.)
// ---------------------------------------------------------------------------
static inline bool IsClipHandleOrCIHNone(const AptValue* pValue)
{
    const AptVirtualFunctionTable_Indices eType = pValue->getVtblIndex();
    return (eType == AptVFT_CharacterInstHandle && pValue->getIsDefined())
        || eType == AptVFT_CIHNone;
}

// FLAG (un-homed AptCIH play-head seek -- declared as an extern shim, matching the
// sibling AptCIHNativeFunctionHelper.cpp): seek the node's timeline to nFrame.
//   AptCIH::jumpToFrame @0x82B0C... (X360) -- now the real member, called directly.

// FLAG (un-homed AptActionInterpreter::valueToObject -- declared as an extern shim,
// matching the sibling AptCIHNativeFunctionHelper.cpp): coerce pValue to the object
// it designates under (pScope, pTarget), writing the result through *ppOut.
//   AptActionInterpreter::valueToObject @0x82B0... (X360).
// AptActionInterpreter::valueToObject is now a member (declared in AptActionInterpreter.h).

// FLAG (AptGC layer -- AptValueVector::ReleaseValues over off_8324E51C): drain the
// deferred-release value vector once the operand stack empties. Shared with the
// Var/Member/Control opcodes (declared there too); the host-side vector type/global
// are not reconstructed yet, so the flush is encapsulated.
extern void AptFlushDeferredReleases();

// FLAG (homed by the AS-globals layer): the shared "undefined" value (off_8324D814).
extern AptValue* gpUndefinedValue;

// FLAG (host path-context resolver -- console sub_82B02F80, the same one CallFunction
// uses): parse pName into (*ppOutContext, *pOutLeaf) under (pScope, pTarget). Shared
// with the StackOps call opcodes (declared there too).
extern void AptResolveTargetContext(AptValue* pScope, AptValue* pTarget,
                                           const EAStringC* pName,
                                           AptValue** ppOutContext, EAStringC* pOutLeaf);

// ---------------------------------------------------------------------------
// FLAG (the drag-state singleton + mouse globals -- host/AptApt layer, wired at
// AptInit; reached through the apt-context global off_8324E574 in the console).
// StartDragMovie writes the active drag target + its constrain rect + grab offset
// into the engine's single drag-state record. The host owns the record (it lives
// behind the apt-context global's +0x18 slot in the console); it is modelled here
// as a small named struct so the float-field stores stay faithful (the console
// reaches them at +0x3C/+0x40.. off the record) without raw offset arithmetic.
// ---------------------------------------------------------------------------
// The drag block lives ON the director (X360 StartDragMovie @0x82B03B00..:
// target @+0x3C, constrain L/T/R/B @+0x40..+0x4C, grab X/Y @+0x50/+0x54 --
// all off gpAptTarget->mpAnimationTarget). Declared as the natural x64 view
// over the director's mpDragMC..mGrabOffset member run (pointer + 6 floats,
// no console padding -- the old padded form baked the 32-bit offsets and
// broke over the widened director).
struct AptDragState
{
    AptValue* mpDragTarget;   // dir mpDragMC        [c:0x3C]
    float     mConstrainL;    // dir mConstrain[0]   [c:0x40] (default -9999)
    float     mConstrainT;    // dir mConstrain[1]   [c:0x44]
    float     mConstrainR;    // dir mConstrain[2]   [c:0x48]
    float     mConstrainB;    // dir mConstrain[3]   [c:0x4C]
    float     mGrabOffsetX;   // dir mGrabOffset[0]  [c:0x50] (default 0)
    float     mGrabOffsetY;   // dir mGrabOffset[1]  [c:0x54]
};

// AptApt_GetDragState inlined at its call site(s).

// FLAG (host mouse position -- the engine's current cursor coords, console
// dword_8324E534 / dword_8324E538, advanced by the input layer each frame; the
// console loads them as 64-bit then converts int->float). The grab-offset path
// reads them once.
extern int32_t gAptMouseX;   // dword_8324E534
extern int32_t gAptMouseY;   // dword_8324E538

// ---------------------------------------------------------------------------
// GotoFrame @0x82B0C480 -- jump the bound clip to the inline-encoded frame number.
// The target node is the "current target" (ctx.mpPendingReleaseValue) when it is a
// clip handle / CIHNone, else the run scope (ctx.mpCIH); a None-typed node is left
// alone. The frame number is a 4-byte (4-byte-aligned) inline operand. After the
// seek the clip's "playing" bit is cleared and -- once the operand stack has drained
// -- the deferred-release vector is flushed.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionGotoFrame(AptActionInterpreter* pInterp,
                                                       LocalContextT* pContext)
{
    // Align the PC up to 4, read the inline frame dword, advance the PC by 4.
    const int32_t* pFrame = reinterpret_cast<const int32_t*>(
        (reinterpret_cast<uintptr_t>(pContext->mpProgramCounter) + 7) & ~static_cast<uintptr_t>(7));   // 8-aligned (GUIAPT64)
    pContext->mpProgramCounter = reinterpret_cast<const unsigned char*>(pFrame + 1);

    // Pick the target node: the current-target slot when it is a clip / CIHNone,
    // otherwise the run scope.
    AptCIH* pNode = nullptr;
    if (pContext->mpPendingReleaseValue && IsClipHandleOrCIHNone(pContext->mpPendingReleaseValue))
        pNode = static_cast<AptCIH*>(pContext->mpPendingReleaseValue);
    else if (IsClipHandleOrCIHNone(pContext->mpCIH))
        pNode = pContext->mpCIH;

    if (pNode && pNode->getVtblIndex() != AptVFT_None)
    {
        pNode->jumpToFrame(*pFrame);          // real member (play-head seek)
        // clear the "playing" bit on the node's sprite instance.
        static_cast<AptCharacterSpriteInstBase*>(pNode->GetCharacterInst())->mnClipActionFlags
            &= ~KU_CLIP_PLAYING;
    }

    // Flush the AptGC deferred-release vector once the operand stack has drained
    // (console: off_8324E51C->count != 0 && mnStackTop == 0).
    if (pInterp->mnStackTop == 0)
        AptFlushDeferredReleases();   // FLAG: off_8324E51C / AptValueVector::ReleaseValues
}

// ---------------------------------------------------------------------------
// NextFrame @0x82B0C3E0 -- advance the run scope's play-head one frame
// (jumpToFrame(mnGotoFrame + 1)) and clear the clip's "playing" bit.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionNextFrame(AptActionInterpreter* /*pInterp*/,
                                                       LocalContextT* pContext)
{
    AptCIH* const pNode = pContext->mpCIH;
    AptCharacterSpriteInstBase* const pSprite =
        static_cast<AptCharacterSpriteInstBase*>(pNode->GetCharacterInst());

    pNode->jumpToFrame(pSprite->mnGotoFrame + 1);          // real member (play-head seek)
    pSprite->mnClipActionFlags &= ~KU_CLIP_PLAYING;
}

// ---------------------------------------------------------------------------
// PrevFrame (0x05; PS3 @0x8293BC) -- step the run scope's play-head back one frame
// (jumpToFrame(mnGotoFrame - 1)) and clear the clip's "playing" bit. NextFrame's
// exact mirror.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPrevFrame(AptActionInterpreter* /*pInterp*/,
                                                       LocalContextT* pContext)
{
    AptCIH* const pNode = pContext->mpCIH;
    AptCharacterSpriteInstBase* const pSprite =
        static_cast<AptCharacterSpriteInstBase*>(pNode->GetCharacterInst());

    pNode->jumpToFrame(pSprite->mnGotoFrame - 1);          // real member (play-head seek)
    pSprite->mnClipActionFlags &= ~KU_CLIP_PLAYING;
}

// ---------------------------------------------------------------------------
// Play @0x82ADD690 -- start the play-head of a (non-level) clip. When the run scope
// is a defined node whose instance is not a level instance: prefer the "current
// target" slot (ctx.mpPendingReleaseValue) when it is a clip / CIHNone -- set its
// "playing" bit + dirty it; otherwise act on the run scope itself.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionPlay(AptActionInterpreter* /*pInterp*/,
                                                  LocalContextT* pContext)
{
    AptCIH* const pNode = pContext->mpCIH;
    if (!pNode->getIsDefined())
        return;

    AptCharacterInst* const pInst = pNode->GetCharacterInst();
    if (pInst->GetTypeTag() == KU_TYPETAG_LEVEL)
        return;

    AptValue* const pTarget = pContext->mpPendingReleaseValue;
    if (pTarget && IsClipHandleOrCIHNone(pTarget))
    {
        AptCIH* const pTargetNode = static_cast<AptCIH*>(pTarget);
        static_cast<AptCharacterSpriteInstBase*>(pTargetNode->GetCharacterInst())->mnClipActionFlags
            |= KU_CLIP_PLAYING;
        pTargetNode->SetDirtyState(true, true);
    }
    else if (IsClipHandleOrCIHNone(pNode))
    {
        static_cast<AptCharacterSpriteInstBase*>(pInst)->mnClipActionFlags |= KU_CLIP_PLAYING;
        pNode->SetDirtyState(true, true);
    }
}

// ---------------------------------------------------------------------------
// Stop @0x82ADD768 -- clear the run scope clip's "playing" bit (a defined,
// non-level node). The console reads the type tag before the null check; preserved.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionStop(AptActionInterpreter* /*pInterp*/,
                                                  LocalContextT* pContext)
{
    AptCIH* const pNode = pContext->mpCIH;
    if (!pNode->getIsDefined())
        return;

    AptCharacterInst* const pInst = pNode->GetCharacterInst();
    if (pInst->GetTypeTag() != KU_TYPETAG_LEVEL && pInst != nullptr)
        static_cast<AptCharacterSpriteInstBase*>(pInst)->mnClipActionFlags &= ~KU_CLIP_PLAYING;
}

// ---------------------------------------------------------------------------
// CloneSprite @0x82B0DE08 -- AS duplicateMovieClip opcode: clone the run scope's
// node under a new name + depth from the top three operands (top-3 = parent value,
// top-2 = name value, top-1 = depth), forwarding to the interpreter's clone core,
// then pop the three operands.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionCloneSprite(AptActionInterpreter* pInterp,
                                                         LocalContextT* pContext)
{
    AptValue* const pNameValue   = pInterp->mpStack[pInterp->mnStackTop - 2];
    AptValue* const pParentValue = pInterp->mpStack[pInterp->mnStackTop - 3];
    const int       nDepth       = pInterp->mpStack[pInterp->mnStackTop - 1]->toInteger();

    // AptActionInterpreter::_doCloneSprite (the AS duplicateMovieClip core; homed in
    // AptActionInterpreterInterpHelpers.cpp). Canonical signature (interpreter, AptCIH*
    // scope, AptValue* target, parent, name, depth, initObject=null).
        // AptActionInterpreter::_doCloneSprite is now a member (declared in the header).
        pInterp->_doCloneSprite(pContext->mpCIH, pContext->mpPendingReleaseValue,
                            pParentValue, pNameValue, nDepth, nullptr);

    pInterp->stackSafePop(3);   // console Burnout_X360_Artist_01e3_0(this, 3)
}

// ---------------------------------------------------------------------------
// RemoveSprite @0x82B08D28 -- AS removeMovieClip opcode: when the top operand is a
// defined value, resolve it to its backing object and -- when that is a clip handle
// / CIHNone -- drop the cloned node from its display-list parent's child list. The
// top operand is then popped.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionRemoveSprite(AptActionInterpreter* pInterp,
                                                          LocalContextT* pContext)
{
    AptValue* const pTop = pInterp->mpStack[pInterp->mnStackTop - 1];
    if (pTop->getIsDefined())
    {
        AptValue* pResolved = nullptr;
        pInterp->valueToObject(pContext->mpCIH, pContext->mpPendingReleaseValue,
                                           pTop, &pResolved);   // FLAG: un-homed valueToObject
        if (pResolved && IsClipHandleOrCIHNone(pResolved))
        {
            // The cloned node lives in its display-list parent's child display list
            // (the parent sprite instance's embedded AptDisplayList, console +0x1C);
            // remove the node at this clone's depth from it.
            AptCIH* const pCIH = static_cast<AptCIH*>(pResolved);
            AptCharacterSpriteInstBase* const pParentSprite =
                static_cast<AptCharacterSpriteInstBase*>(pCIH->GetDisplayListParent()->GetCharacterInst());
            pParentSprite->mDisplayList.removeClonedObject(pCIH);
        }
    }
    pInterp->stackPop();
}

// FLAG (the drag target's clip matrix translation -- the console reaches it through
// the resolved value's boxed object (+0x20) -> character instance (+4) -> position
// matrix (+8), reading the translate components at the matrix's +0x10/+0x14, with a
// null-matrix fallback to the engine's identity translate (flt_8324E2B0). AptMatrix
// is not yet a reconstructed named type, so the deref chain is encapsulated here):
// write the drag target's clip translate (x,y) through the out params.
extern void AptApt_GetDragTargetTranslate(AptValue* pDragTarget, float* pOutX, float* pOutY);

// ---------------------------------------------------------------------------
// StartDragMovie @0x82B03A20 -- AS startDrag opcode: begin dragging a movie clip.
// The top operand names the drag target (a clip handle directly, or a string path
// resolved through the path context + getVariable). The drag-state singleton records
// the target, resets its constrain rect (to the -9999 "unbounded" sentinel) and grab
// offset, then -- unless the "lockcenter" operand (top-2, an integer when present) is
// set -- computes the cursor->clip grab offset from the clip's matrix translate and
// the current mouse position. When the "constrain" operand (top-3) is an integer the
// constrain rect is filled from the four bound operands (top-4..top-7). Finally the
// consumed operands are popped (3 by default, 7 with the constrain rect).
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionStartDragMovie(AptActionInterpreter* pInterp,
                                                            LocalContextT* pContext)
{
    AptValue* pTarget = pInterp->mpStack[pInterp->mnStackTop - 1];   // console Variable (top)

    // A string-typed name (raw type 1 / boxed 33) is resolved through the path context.
    if (pTarget->isString())
    {
        EAStringC leafName;                                          // console v23 scratch
        AptValue* pResolvedContext = nullptr;                        // console HIDWORD(v24)

        // Boxed strings (type != 1) expose their AptString behind +0x20.
        const EAStringC* pName = (pTarget->getVtblIndex() == AptVFT_StringValue)
            ? reinterpret_cast<AptString*>(pTarget)->GetInternalString()
            : (*reinterpret_cast<AptString**>(reinterpret_cast<char*>(pTarget) + 0x20))
                  ->GetInternalString();   // FLAG: type-33 box (console *(Variable+32) then +8)

        AptResolveTargetContext(pContext->mpCIH, pContext->mpPendingReleaseValue,
                                       pName, &pResolvedContext, &leafName);   // FLAG: sub_82B02F80
        pTarget = pInterp->getVariable(pResolvedContext, pContext->mpPendingReleaseValue,
                                       &leafName, 1, 1, 0);
    }

    int nPopCount = 3;                                  // console v8 = 3 (becomes 7 with bounds)
    pTarget->AddRef();                                  // console (**Variable)(Variable)

    AptDragState* const pDrag = reinterpret_cast<AptDragState*>(&gpAptTarget->mpAnimationTarget->mpDragMC);  // host drag-state view off the director
    pDrag->mpDragTarget = pTarget;
    pDrag->mGrabOffsetX = 0.0f;
    pDrag->mGrabOffsetY = 0.0f;
    pDrag->mConstrainL  = -9999.0f;
    pDrag->mConstrainT  = -9999.0f;
    pDrag->mConstrainR  = -9999.0f;
    pDrag->mConstrainB  = -9999.0f;

    // "lockcenter" (top-2): when it is NOT an integer, drag relative to the grab point
    // -- the cursor->clip offset is (mouse - clip-translate).
    if (!pInterp->mpStack[pInterp->mnStackTop - 2]->isInteger())
    {
        float fTransX = 0.0f;
        float fTransY = 0.0f;
        AptApt_GetDragTargetTranslate(pTarget, &fTransX, &fTransY);   // FLAG: clip matrix translate

        pDrag->mGrabOffsetX = static_cast<float>(gAptMouseX) - fTransX;   // console dword_8324E534
        pDrag->mGrabOffsetY = static_cast<float>(gAptMouseY) - fTransY;   // console dword_8324E538
    }

    // "constrain" (top-3): an integer enables the constrain rect, read from the four
    // bound operands (top-4 = left .. top-7 = bottom, per the console's slot order).
    if (pInterp->mpStack[pInterp->mnStackTop - 3]->isInteger())
    {
        nPopCount = 7;
        pDrag->mConstrainB = pInterp->mpStack[pInterp->mnStackTop - 4]->toFloat();
        pDrag->mConstrainR = pInterp->mpStack[pInterp->mnStackTop - 5]->toFloat();
        pDrag->mConstrainT = pInterp->mpStack[pInterp->mnStackTop - 6]->toFloat();
        pDrag->mConstrainL = pInterp->mpStack[pInterp->mnStackTop - 7]->toFloat();
    }

    pInterp->stackSafePop(nPopCount);   // console Burnout_X360_Artist_01e3_0(this, v8)
}

// ---------------------------------------------------------------------------
// StopDragMovie (0x28; PS3 @0x7E9FA8) -- end the current drag: Release the held
// drag target (the console's target->[+0x18]->[+0x3C] slot, the same slot
// StartDragMovie's AptDragState::mpDragTarget encapsulates) and reset it to the
// `undefined` singleton.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionStopDragMovie(AptActionInterpreter* /*pInterp*/,
                                                           LocalContextT* /*pContext*/)
{
    AptDragState* const pDrag = reinterpret_cast<AptDragState*>(&gpAptTarget->mpAnimationTarget->mpDragMC);  // host drag-state view off the director
    if (pDrag->mpDragTarget)
        pDrag->mpDragTarget->Release();
    pDrag->mpDragTarget = gpUndefinedValue;
}

// ---------------------------------------------------------------------------
// SetTarget @0x82B093C8 -- AS SetTarget opcode: redirect the run's "current target"
// to the inline-encoded path. An empty path clears the target (releasing the prior
// one). A path beginning with '/' or '.' is walked relative to the run scope ('..'
// climbs the display-list parent chain); any other path is resolved through getObject
// (after trimming a trailing '/'). The resolved node is AddRef'd into the target slot.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionSetTarget(AptActionInterpreter* pInterp,
                                                       LocalContextT* pContext)
{
    // Align the PC up to 4, read the inline string pointer, advance the PC by 4.
    const unsigned char* const pAligned = reinterpret_cast<const unsigned char*>(
        (reinterpret_cast<uintptr_t>(pContext->mpProgramCounter) + 7) & ~static_cast<uintptr_t>(7));   // 8-aligned (GUIAPT64)
    const char* const szPath = *reinterpret_cast<const char* const*>(pAligned);
    pContext->mpProgramCounter = pAligned + 4;

    if (szPath[0] == '\0')
    {
        // empty path -> drop the current target.
        if (pContext->mpPendingReleaseValue)
            pContext->mpPendingReleaseValue->Release();
        pContext->mpPendingReleaseValue = nullptr;
        return;
    }

    EAStringC strPath(szPath);   // console EAStringC::InitFromBuffer scratch (RAII Decrease)
    AptValue* pTarget = nullptr;
    if (szPath[0] == '/' || szPath[0] == '.')
    {
        // relative walk: each leading ".." climbs to the display-list parent.
        AptCIH* pNode = pContext->mpCIH;
        const char* pCursor = szPath;
        while (pCursor[0] == '.' && pCursor[1] == '.' && pNode->GetDisplayListParent())
        {
            pCursor += 2;
            pNode = pNode->GetDisplayListParent();
        }
        pTarget = pNode;
    }
    else
    {
        strPath.TrimRight("/");
        pTarget = pInterp->getObject(pContext->mpCIH, nullptr, &strPath);
    }

    pContext->mpPendingReleaseValue = pTarget;
    pContext->mpPendingReleasePC = nullptr;
    pTarget->AddRef();   // console (**pTarget)(pTarget) -> vtbl[0] AddRef
}

// ---------------------------------------------------------------------------
// SetTarget2 @0x82B08A88 -- the dynamic SetTarget: the path comes off the operand
// stack (the top value, coerced to a string) rather than inline. An empty path
// clears the target; otherwise getObject resolves it under the run scope + current
// target. The operand is then popped.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionSetTarget2(AptActionInterpreter* pInterp,
                                                        LocalContextT* pContext)
{
    AptValue* const pTop = pInterp->mpStack[pInterp->mnStackTop - 1];
    EAStringC scratch;
    const EAStringC* const pPath = AptValue::Get_ToString(pTop, &scratch);

    if (pPath->GetInternalSize() != 0)   // console: *(strData + 2) (m_uSize) != 0 -> non-empty path
    {
        AptValue* const pTarget =
            pInterp->getObject(pContext->mpCIH, pContext->mpPendingReleaseValue, pPath);
        pContext->mpPendingReleaseValue = pTarget;
        pContext->mpPendingReleasePC = nullptr;
        pTarget->AddRef();   // console (**pTarget)(pTarget) -> vtbl[0] AddRef
    }
    else
    {
        // empty path -> drop the current target.
        if (pContext->mpPendingReleaseValue)
            pContext->mpPendingReleaseValue->Release();
        pContext->mpPendingReleaseValue = nullptr;
    }

    pInterp->stackPop();
}

// ---------------------------------------------------------------------------
// TargetPath @0x82B09050 -- AS targetPath(obj): resolve the top operand to its
// backing object and push that object's slash/dot path name as a string. A clip-
// handle / CIHNone object yields its getName path; any other object yields the
// special "/" path (the console seeds the scratch from the single-char constant at
// 0x82143A2F). A non-object operand pushes `undefined`. The operand is popped.
// ---------------------------------------------------------------------------
void AptActionInterpreter::_FunctionAptActionTargetPath(AptActionInterpreter* pInterp,
                                                        LocalContextT* pContext)
{
    AptValue* const pTop = pInterp->mpStack[pInterp->mnStackTop - 1];

    AptValue* pResolved = nullptr;
    pInterp->valueToObject(pContext->mpCIH, pContext->mpPendingReleaseValue,
                                       pTop, &pResolved);   // FLAG: un-homed valueToObject

    if (pResolved)
    {
        EAStringC scratch;
        if (IsClipHandleOrCIHNone(pResolved))
        {
            pInterp->getName(pResolved, &scratch);   // the slash/dot path of the node
        }
        else
        {
            scratch = EAStringC("/");   // console InitFromBuffer(&unk_82143A2F == "/") -> scratch
        }

        AptString* pStr = AptString::Create("");    // FLAG: seed const @0x820046A7 (""), overwritten
        *pStr->GetInternalString() = scratch;
        pInterp->stackPop();
        pInterp->mpStack[pInterp->mnStackTop++] = pStr;   // inlined stackPush
        pStr->AddRef();
    }
    else
    {
        // not an object -> push `undefined`.
        pInterp->stackPop();
        pInterp->mpStack[pInterp->mnStackTop++] = gpUndefinedValue;   // console off_8324D814
        gpUndefinedValue->AddRef();
    }
}
