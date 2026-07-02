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

#include <new>   // placement new (the TextFormat copy-ctor into a pool block)

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
#include "SDKs/EATech/include/Apt/AptObject.h"                    // AptObject::Create (the getBounds result object)
#include "SDKs/EATech/include/Apt/AptTextFormat.h"                // TextFormat record (setTextFormat) + AptTextFormat
#include "SDKs/EATech/include/Apt/AptRenderItemDynamicText.h"     // mpTextFormat / mStateFlags / SetAlignment / SetTextFormat
#include "SDKs/EATech/include/Apt/AptDisplayListState.h"          // findInst / ChangeDepth / swapDepths (swapDepths)
#include "SDKs/EATech/include/Apt/AptStd/AptMatrix.h"             // mpPositionMatrix tx/ty (getBounds)
#include "SDKs/EATech/include/Apt/AptCharacterAnimationInst.h"    // mAnimationFilePtr (getBytesTotal)
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"           // AptString::str (the goto label)
#include "SDKs/EATech/include/Apt/AptValue/AptStringObject.h"     // mpValue (the boxed label form)
#include "SDKs/EATech/include/Apt/AptValue/AptValueVector.h"      // the operand-stack view (gotoAndX)
#include "SDKs/EATech/include/Apt/AptMovie.h"                     // mpLabelHash (label -> frame)
#include "SDKs/EATech/include/Apt/AptCharacter.h"                 // the movie character (KU_AptEmbeddedMovieOff)
#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"        // the def base (findCharacterInLibrary)
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"                 // AptFilePtr op= (the export file-assign)
#include <string.h>                                               // _stricmp (the library-name compares)
#include "SDKs/EATech/include/Apt/AptCharacterHelper.h"           // spDefaultMovieCharacter/CreateMovieCharacterInst (createEmptyMovieClip)
#include "SDKs/EATech/include/Apt/AptCharacterDynamicText.h"      // spDefaultTextCharacter -> AptCharacter upcast (createTextField)
#include "SDKs/EATech/include/Apt/AptTarget.h"                    // gpAptTarget->mpAnimationTarget/mpLinker (attachMovie/loadMovie)
#include "SDKs/EATech/include/Apt/AptLinker.h"                    // AptLinker::Load (loadMovie)
#include "SDKs/EATech/include/Apt/AptFile.h"                      // mFileName (getBytesTotal)
#include "SDKs/EATech/include/Apt/AptValue/AptFloat.h"            // AptFloat::Create (getBytesTotal)
#include "SDKs/EATech/Apt/DogmaAllocator.h"                       // gpAptPseudoDataPool (the TextFormat pool)

// ---------------------------------------------------------------------------
// FLAG (un-homed Apt behavioural callees -- bodies in their own TUs; declared so
// the AS movie-management methods below compile against the same entry points):
//   AptCIH::InsertChild @0x82B09CA0 -- place pCharacter into pNode's child display
//     list at nDepth under name pName (pSource = the cloned-from node or null,
//     pInitObject = an optional init object). Returns the inserted child CIH.
//   findCharacterInLibrary @0x82AD... -- resolve an exported library symbol name to
//     a character in pNode's movie (a3 = "search imports" flag).
//   AptAnimationTarget::TickNewInsts -- tick the just-inserted instances so they are
//     live this frame (drains the new-instance table off_8324E544).
//   AptHook_GetBytesTotal (gAptFuncs slot, X360 dword_8324E8AC) -- host query: total
//     byte size of a loaded .apt by file path.
// ---------------------------------------------------------------------------
extern AptCIH* AptCIH_InsertChild(AptCIH* pNode, AptCIH* pSource, AptCharacter* pCharacter,
                                  int nDepth, EAStringC* pName, AptValue* pInitObject);   // AptCIH::InsertChild
extern AptCharacter* findCharacterInLibrary(AptCIH* pNode, EAStringC* pName, char bSearchImports);
extern void AptAnimationTarget_TickNewInsts(AptAnimationTarget* pAnim);                   // AptAnimationTarget::TickNewInsts
extern int  AptHook_GetBytesTotal(const char* pcFilePath, int a2, double a3);             // dword_8324E8AC

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

    // Seek the node's play-head (the real member AptCIH::jumpToFrame, AptCIH.cpp).
    pNode->jumpToFrame(pSprite->mnGotoFrame + 1);

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
// ---------------------------------------------------------------------------
// AptCIH::_gotoAndX @0x82B0D2F0 -- the shared gotoAndPlay/gotoAndStop core.
// HOMED 2026-07-02 from the X360 asm (retiring the AptRenderLinkStubs null
// stub). With at least one argument on the operand stack:
//   * a level placeholder node (charInst mTypeFlags tag 15, 0x3C000000) is a
//     no-op;
//   * a STRING argument (the inlined isString(): vtbl 1/33 + defined) is a
//     frame LABEL: both string forms yield the embedded EAStringC (+8 -- the
//     AptString's `str`, or the StringObject's boxed value's), looked up in
//     the clip movie's label hash (movie word[2]); frame = hash hit
//     (an AptInteger) + 1, or 0 on a miss (-1 + 1);
//   * otherwise frame = toInteger(arg);
//   * AS frames are 1-based: frame -= 1; a negative result is a no-op;
//   * jumpToFrame(frame); the sprite's playing bit (mnClipActionFlags bit 6)
//     := bPlay; a STOP additionally calls SetDirtyState(1, 1).
// Returns the shared undefined singleton (off_8324D814) either way.
// ---------------------------------------------------------------------------
extern AptActionInterpreter gAptActionInterpreter;   // dword_8324E760 (the AS VM)

AptValue* AptCIH_gotoAndX(AptValue* pContext, int nArgCount, int bPlay)
{
    AptCIH* const pNode = static_cast<AptCIH*>(pContext);
    if (nArgCount >= 1)
    {
        // The operand-stack top (the frame number or label).
        AptValueVector* pStack =
            reinterpret_cast<AptValueVector*>(&gAptActionInterpreter.mnStackTop);
        AptValue* const pArg = pStack->mppItems[pStack->mnTop - 1];

        AptCharacterInst* const pInst = pNode->GetCharacterInst();   // +0x20
        if ((pInst->mTypeFlags & 0xFC000000u) != 0x3C000000u)        // tag 15 -> no-op
        {
            int32_t nFrame;
            if (pArg->isString())
            {
                // Both string forms end at the embedded EAStringC (object +8).
                AptString* const pStr =
                    (pArg->getVtblIndex() == AptVFT_StringValue)
                        ? static_cast<AptString*>(pArg)
                        : static_cast<AptString*>(
                              static_cast<AptStringObject*>(pArg)->GetBoxedString());
                // The clip movie's label hash: charInst -> render item ->
                // character -> embedded movie (X360 char+0x10 -> label hash
                // word[2] == the +0x18 read; native-8 via the typed members).
                const AptCharacter* const pChar = pInst->GetRenderItem()->mpCharacter;
                const AptMovie* const pMovie = reinterpret_cast<const AptMovie*>(
                    reinterpret_cast<const char*>(pChar) + KU_AptEmbeddedMovieOff);
                AptNativeHash* const pLabels = pMovie->mpLabelHash;
                AptValue* const pHit =
                    pLabels ? pLabels->Lookup(*pStr->GetInternalString()) : nullptr;
                nFrame = (pHit ? pHit->toInteger() : -1) + 1;
            }
            else
            {
                nFrame = pArg->toInteger();
            }

            nFrame -= 1;   // AS frames are 1-based
            if (nFrame >= 0)
            {
                pNode->jumpToFrame(nFrame);
                // playing := bPlay (the sprite-base inst's bit 6).
                AptCharacterSpriteInstBase* const pSprite =
                    static_cast<AptCharacterSpriteInstBase*>(pNode->GetCharacterInst());
                pSprite->mnClipActionFlags =
                    (pSprite->mnClipActionFlags & ~0x40u) | (bPlay ? 0x40u : 0u);
                if (!bPlay)
                    pNode->SetDirtyState(true, true);   // X360 (r4=1, r5=1) on the stop arm
            }
        }
    }
    return gpUndefinedValue;   // off_8324D814
}

// ---------------------------------------------------------------------------
// AptInterp_BuildTargetPath -- the recursive display-path builder (X360
// sub_82AF7400) behind AptActionInterpreter::getName @0x82AF75C8 (HOMED
// 2026-07-02, retiring the {} stub). Walks up the display-parent chain: the
// ROOT frame (no parent) renders as "_level%d" of its render-item depth (a
// dotted build always writes it; the slash form only when the depth is
// non-zero); every child level appends the separator ("." or "/") plus its
// instance name -- or "instance%ld" of its depth when the name is the shared
// empty string.
// ---------------------------------------------------------------------------
static void AptInterp_BuildTargetPath(AptCIH* pNode, EAStringC* pOut, int bDots)
{
    const char* const pSep = bDots ? "." : "/";

    AptCIH* const pParent = pNode->GetDisplayListParent();
    if (pParent == nullptr)
    {
        const int nDepth = pNode->GetCharacterInst()->GetRenderItem()->GetDepth();
        if ((bDots == 0 && nDepth != 0) || bDots == 1)
        {
            EAStringC lLevel;
            lLevel.Format("_level%d", nDepth);
            *pOut = lLevel;
        }
        return;
    }

    AptInterp_BuildTargetPath(pParent, pOut, bDots);

    const EAStringC& rName = pNode->GetInstanceName();
    if (!rName.IsEmpty())
    {
        *pOut += (pSep + rName);   // the free operator+(const char*, EAStringC)
    }
    else
    {
        const int nDepth = pNode->GetCharacterInst()->GetRenderItem()->GetDepth();
        *pOut += pSep;
        EAStringC lInst;
        lInst.Format("instance%ld", static_cast<long>(nDepth));
        *pOut += lInst;
    }
}

// AptActionInterpreter::getName @0x82AF75C8 -- clear the out-string, then
// build the dotted target path of pNode ("_level0.child.instance3"-style).
void AptActionInterpreter_getName(AptCIH* pNode, EAStringC* pOut)
{
    *pOut = EAStringC("");
    AptInterp_BuildTargetPath(pNode, pOut, 1);
}

// ---------------------------------------------------------------------------
// AptCIH_ShapeHitTest (HOMED 2026-07-02, retiring the return-0 stub). The
// X360 sMethod_hitTest's shape-precise arm @0x82AED868 is a HOST-CALLBACK
// dispatch: `gAptFuncs.pfnPointHitTest(x, y, node)` (dword_8324E8A4 == the
// user-function table +0x8C; PPC f1/f2 + r5 == the (float, float, clip) C
// signature). The engine itself has no shape rasterisation -- precise hit
// testing is the host renderer's job. FLAG (PC bring-up boundary): the X360
// calls the slot unguarded (CgsAptAux always installs the full table); our
// bring-up has not installed a point-hit-test callback yet, so a null slot
// answers 0 (miss) -- the honest un-installed-host state, same convention as
// the other un-wired gAptFuncs families.
// ---------------------------------------------------------------------------
extern AptUserFunctions gAptFuncs;   // dword_8324E818 (CgsAptAux.cpp)

int AptCIH_ShapeHitTest(AptValue* pNode, float fX, float fY)
{
    if (gAptFuncs.pfnPointHitTest != nullptr)
    {
        // FLAG (x64 handle width): AptAssetMoiveClip is the DWARF 'int' handle --
        // console-width for the clip pointer. No PC host installs this slot yet;
        // when one does, the typedef must widen (intptr_t) with the host. The
        // truncating cast documents the boundary rather than hiding it.
        return gAptFuncs.pfnPointHitTest(
            fX, fY,
            static_cast<AptAssetMoiveClip>(reinterpret_cast<uintptr_t>(pNode)));
    }
    return 0;
}

// ---------------------------------------------------------------------------
// findCharacterInLibrary @0x82AFDF58 (HOMED 2026-07-02, retiring the null
// stub). Resolve an exported library symbol name to its AptCharacter: walk
// pNode's display-parent chain; at each node, take its character's owning def
// base (the character's mpFixupLink back-link -- charTable[0], the root --
// plus KU_AptEmbeddedMovieOff), then
//   * scan the EXPORT list (mpInitList: name @+0, id @+8, stride 0x10) with a
//     case-insensitive compare against pName; a hit returns
//     charTable[id], first assigning the STARTING node's character's
//     AptFile into the hit's null mpAnimationFile slot (the ref-counted
//     AptFilePtr assignment -- console AptFile::operator=(char+0xC, ...));
//   * when bSearchImports (always true past the first node), scan the IMPORT
//     table (mpImportTable: class name @+8, id @+0x10, stride 0x20); a hit
//     returns charTable[id] directly.
// Null when the chain is exhausted.
// ---------------------------------------------------------------------------
AptCharacter* findCharacterInLibrary(AptCIH* pNode, EAStringC* pName, char bSearchImports)
{
    // The starting node's character (the export-hit file-assign source).
    const AptCharacter* const pSourceChar =
        pNode->GetCharacterInst()->GetRenderItem()->mpCharacter;

    for (AptCIH* pWalk = pNode; pWalk != nullptr;
         pWalk = pWalk->GetDisplayListParent(), bSearchImports = 1)
    {
        const AptCharacter* const pChar =
            pWalk->GetCharacterInst()->GetRenderItem()->mpCharacter;
        AptCharacterAnimation* const pDef =
            reinterpret_cast<AptCharacterAnimation*>(
                reinterpret_cast<char*>(pChar->mpFixupLink) + KU_AptEmbeddedMovieOff);

        // ---- the EXPORT list --------------------------------------------------
        for (int32_t i = 0; i < pDef->mnInitListCount; ++i)
        {
            const AptInitEntry& rEntry = pDef->mpInitList[i];
            if (_stricmp(pName->GetBuffer(),
                         static_cast<const char*>(rEntry.mpInitObject)) == 0)
            {
                AptCharacter* const pFound =
                    pDef->mpCharacterTable[rEntry.mnIndicator];
                if (pFound->mpAnimationFile == nullptr)
                {
                    // The ref-counted AptFilePtr assignment (IncRef the source).
                    reinterpret_cast<AptFilePtr*>(&pFound->mpAnimationFile)
                        ->operator=(
                            *reinterpret_cast<const AptFilePtr*>(
                                &pSourceChar->mpAnimationFile));
                }
                return pFound;
            }
        }

        // ---- the IMPORT table (from the second node on) -----------------------
        if (bSearchImports)
        {
            for (int32_t i = 0; i < pDef->mnImportCount; ++i)
            {
                const AptImportEntry& rEntry = pDef->mpImportTable[i];
                if (_stricmp(pName->GetBuffer(),
                             static_cast<const char*>(rEntry.mpClassName)) == 0)
                    return pDef->mpCharacterTable[rEntry.mnId];
            }
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// AptInterp_LabelToFrame (HOMED 2026-07-02, retiring the stub -1). The frame-
// label resolve the GotoLabel/GotoFrame2 opcode handlers extracted: the node's
// clip movie's label hash (the X360 @0x82B0C618 chain: charInst -> render item
// -> character -> embedded movie word[2]), Lookup(label) -> toInteger, or -1
// on a miss / no hash. Returns the 0-based frame (the callers' >= 0 gate and
// the +1/-1 label arithmetic live at the call sites, matching the asm).
// ---------------------------------------------------------------------------
int AptInterp_LabelToFrame(AptCIH* pNode, const EAStringC* pLabel)
{
    const AptCharacter* const pChar =
        pNode->GetCharacterInst()->GetRenderItem()->mpCharacter;
    const AptMovie* const pMovie = reinterpret_cast<const AptMovie*>(
        reinterpret_cast<const char*>(pChar) + KU_AptEmbeddedMovieOff);
    AptNativeHash* const pLabels = pMovie->mpLabelHash;
    AptValue* const pHit = pLabels ? pLabels->Lookup(*pLabel) : nullptr;
    return pHit ? pHit->toInteger() : -1;
}

AptValue* AptCIHNativeFunctionHelper::sMethod_gotoAndPlay(AptValue* pContext, int nArgCount)
{
    return AptCIH_gotoAndX(pContext, nArgCount, 1);
}

AptValue* AptCIHNativeFunctionHelper::sMethod_gotoAndStop(AptValue* pContext, int nArgCount)
{
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
    // AptActionInterpreter::valueToObject (homed in AptActionInterpreterInterpHelpers.cpp;
    // coerces the value to the object it designates under (scope, target), writing it
    // through the out-param). Canonical signature (scope, target, value, ppOut); the
    // console reaches it here with target == 0 (a null AptValue*), receiver == pContext.
    extern void AptActionInterpreter_valueToObject(AptValue* pScope, AptValue* pTarget,
                                                   AptValue* pValue, AptValue** ppOut);
    AptActionInterpreter_valueToObject(pContext, nullptr, pContext, &pResolved);

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

// AptActionInterpreter::_doCloneSprite (the AS duplicateMovieClip core; homed in
// AptActionInterpreterInterpHelpers.cpp). Canonical signature (interpreter, AptCIH*
// scope, AptValue* target, parent, nameValue, depth, initObject); the console reaches
// it here with scope == pContext (a CIH node) and target == 0.
extern AptValue* AptActionInterpreter_doCloneSprite(AptActionInterpreter* pInterp,
                                                    AptCIH* pScope, AptValue* pTarget, AptValue* pParent,
                                                    AptValue* pNameValue, int nDepth, AptValue* pInitObject);

// AptActionInterpreter::loadVariables (the AS loadVariables core; homed in
// AptActionInterpreterInterpHelpers.cpp). Canonical signature (interpreter, scope,
// target, &urlString); the console reaches it here with target == 0.
extern void AptActionInterpreter_loadVariables(AptActionInterpreter* pInterp,
                                               AptValue* pScope, AptValue* pTarget, EAStringC* pURL);

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
                                              static_cast<AptCIH*>(pContext), nullptr, pContext,
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
        AptActionInterpreter_loadVariables(&gAptActionInterpreter, pContext, nullptr, &strURL);
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

// ---------------------------------------------------------------------------
// FLAG (homed by the AS fixed-size pool layer): the shared Apt pseudo-data DOGMA
// pool (X360 off_8324D808) the text-format records are allocated from -- the same
// gpAptPseudoDataPool the sibling Apt TUs (AptActionQueue / AptAnimationTarget)
// declare. Wired at AptInit.
// ---------------------------------------------------------------------------
extern DOGMA_PoolManager* gpAptPseudoDataPool;   // off_8324D808

// ---------------------------------------------------------------------------
// FLAG: TextFormat::copyTextFormatObj (X360 callee of get/setTextFormat) overlays
// pSource's format fields onto pDest in place (the non-allocating field copy, as
// opposed to the TextFormat copy-ctor). Declared as an extern shim taking the
// named TextFormat record so setTextFormat keeps the exact (dest, &src) call shape
// without re-deriving the field offsets here; bodied in the AptTextFormat TU.
// ---------------------------------------------------------------------------
extern void TextFormat_copyTextFormatObj(TextFormat* pDest, const TextFormat* pSource);   // TextFormat::copyTextFormatObj

// ===========================================================================
// sMethod_getBounds @0x82AF5E28 -- AS getBounds([targetCoordSpace]): the receiver's
// bounding box, optionally expressed in another node's coordinate space.
//   .getBounds()        -- bounds in this node's own parent space.
//   .getBounds(target)  -- bounds relative to `target`'s local origin (target must
//                          be a DEFINED value); >1 arg -> undefined.
// Returns a fresh AS Object { xMax, xMin, yMax, yMin } (the four edge floats), each
// offset by the coordinate-space node's local translation (its render item's
// position matrix tx/ty). Undefined when the (single) target argument is undefined.
// ===========================================================================
AptValue* AptCIHNativeFunctionHelper::sMethod_getBounds(AptValue* pContext, int nArgCount)
{
    if (nArgCount > 1)
        return gpUndefinedValue;

    // The coordinate-space node: the receiver by default, or the (defined) target arg.
    AptValue* pSpaceValue = pContext;
    if (nArgCount == 1)
    {
        AptValue* const pArg = gppAptNativeArgStack[gnAptNativeArgCount - 1];   // arg 0: target space
        if (!pArg->getIsDefined())
            return gpUndefinedValue;
        pSpaceValue = pArg;
    }

    // Fresh AS Object (operator new(32) + AptValueWithHash(AptVFT_Object, 8) + the
    // AptObject vtable; mClassFlags = 0) -- the X360 inlines exactly AptObject::Create.
    AptObject* const pResult = AptObject::Create();

    // The coordinate-space node's local origin (its render item's position matrix
    // translation; null -> identity, matching localToGlobal's flt_8324E2B0 fallback).
    AptCIH* const pSpaceNode = static_cast<AptCIH*>(pSpaceValue);
    // The X360 reads the render item's mpPositionMatrix field directly and falls
    // back to the identity matrix (flt_8324E2B0) when it is null -- the same raw
    // read + flt_8324E2B0 fallback as localToGlobal above.
    const AptMatrix* pPos = pSpaceNode->GetCharacterInst()->GetRenderItem()->mpPositionMatrix;
    if (!pPos)
        pPos = &gAptIdentityMatrix;

    // The receiver's world AABB (left, top, right, bottom), shifted into the
    // coordinate-space node's local frame.
    float fRect[4];   // [0]=left [1]=top [2]=right [3]=bottom
    AptCIH_GetWorldBounds(pContext, fRect);
    fRect[2] -= pPos->tx;   // right  - tx
    fRect[0] -= pPos->tx;   // left   - tx
    fRect[1] -= pPos->ty;   // top    - ty
    fRect[3] -= pPos->ty;   // bottom - ty

    // The four AS edge members (X360 unk_8324E6CC/6D0/6D8/6DC), in the asm's order.
    pResult->Set(EAStringC("xMax"), AptFloat::Create(fRect[2]));   // right
    pResult->Set(EAStringC("xMin"), AptFloat::Create(fRect[0]));   // left
    pResult->Set(EAStringC("yMax"), AptFloat::Create(fRect[3]));   // bottom
    pResult->Set(EAStringC("yMin"), AptFloat::Create(fRect[1]));   // top
    return pResult;
}

// ===========================================================================
// sMethod_setTextFormat @0x82AED470 -- AS setTextFormat(format): apply the
// TextFormat argument's defined fields to the receiver dynamic-text field. With at
// most 3 args, when arg 0 is a DEFINED TextFormat value, its format record overlays
// the text item's own TextFormat (creating one if the item has none), then each of
// the source's non-inherit fields (style flags, font name, size, alignment, indent)
// is pushed onto the render item with the matching dirty-state flags. Always returns
// undefined. (The console packs the per-field "needs re-layout" markers into the
// render item's mStateFlags; restored to the named accessors.)
// ===========================================================================
AptValue* AptCIHNativeFunctionHelper::sMethod_setTextFormat(AptValue* pContext, int nArgCount)
{
    if (nArgCount > 3)
        return gpUndefinedValue;

    AptValue* const pArg = gppAptNativeArgStack[gnAptNativeArgCount - 1];   // arg 0: the TextFormat
    if (!pArg->isTextFormat())   // (defined-bit) && meValueType == AptVFT_TextFormat (0x1C)
        return gpUndefinedValue;

    // The argument's embedded format record (the AptTextFormat wrapper's mFormat).
    // Non-const: the X360 clamps the source's mfSize in place (stfs into v2+0x24).
    TextFormat* const pSrc = &static_cast<AptTextFormat*>(pArg)->mFormat;

    AptCharacterInst* const pInst = static_cast<AptCIH*>(pContext)->GetCharacterInst();

    // The text item's current TextFormat object (X360 mpTextFormat @ +0x68, typed
    // AptValue* in the shared header but holding a TextFormat* here -- the cast keeps
    // the shared header untouched).
    AptRenderItemDynamicText* pItem =
        static_cast<AptRenderItemDynamicText*>(pInst->GetRenderItem());

    uint32_t uMergedStyle;
    if (reinterpret_cast<TextFormat*>(pItem->mpTextFormat))
    {
        // The item already has a TextFormat: merge in place.
        uMergedStyle = reinterpret_cast<TextFormat*>(pItem->mpTextFormat)->mnStyleFlags | pSrc->mnStyleFlags;
        pItem = static_cast<AptRenderItemDynamicText*>(pInst->GetRenderItemWritable());
        TextFormat_copyTextFormatObj(reinterpret_cast<TextFormat*>(pItem->mpTextFormat), pSrc);
    }
    else
    {
        // No TextFormat yet: allocate a fresh one (copy-ctor from the source) and set it.
        void* const lpMem = gpAptPseudoDataPool->Allocate(32);
        TextFormat* const pNewFmt = lpMem ? new (lpMem) TextFormat(pSrc) : nullptr;
        pItem = static_cast<AptRenderItemDynamicText*>(pInst->GetRenderItemWritable());
        pItem->SetTextFormat(reinterpret_cast<AptValue*>(pNewFmt));
        uMergedStyle = reinterpret_cast<TextFormat*>(pItem->mpTextFormat)->mnStyleFlags | pSrc->mnStyleFlags;
    }

    // Store the merged style word back into the (writable) item's TextFormat.
    pItem = static_cast<AptRenderItemDynamicText*>(pInst->GetRenderItemWritable());
    reinterpret_cast<TextFormat*>(pItem->mpTextFormat)->mnStyleFlags = uMergedStyle;

    // Font name (when the source font is not the inherit/empty sentinel).
    if (!pSrc->mFontName.IsEmpty())   // X360: *(v2+32) != &unk_82F72FF8 (the empty-string sentinel, not a content test)
    {
        pItem = static_cast<AptRenderItemDynamicText*>(pInst->GetRenderItemWritable());
        reinterpret_cast<TextFormat*>(pItem->mpTextFormat)->mFontName = pSrc->mFontName;
    }

    // Colour provided (-1 == inherit): mark the "format changed" dirty bits. (The
    // colour itself is carried through the copied TextFormat record; here it only
    // drives the re-layout flags, matching the X360 *(v2+0x28) test.)
    if (pSrc->mnColor != -1)
    {
        pItem = static_cast<AptRenderItemDynamicText*>(pInst->GetRenderItemWritable());
        pItem->ClearStateFlags(1u);
        pItem->SetStateFlags(0x10400u);
    }

    // Size provided (-1.0 == inherit): clamp non-positive to 1.0, write mFontSize + dirty.
    if (pSrc->mfSize != -1.0f)
    {
        if (pSrc->mfSize <= 0.0f)
            pSrc->mfSize = 1.0f;
        pItem = static_cast<AptRenderItemDynamicText*>(pInst->GetRenderItemWritable());
        pItem->mFontSize = pSrc->mfSize;
        pItem->ClearStateFlags(1u);
        pItem->SetStateFlags(0x10004u);
    }

    // Alignment provided (3 == inherit): write the packed alignment field + dirty.
    if (pSrc->mnAlign != 3)
    {
        pItem = static_cast<AptRenderItemDynamicText*>(pInst->GetRenderItemWritable());
        pItem->SetAlignment(pSrc->mnAlign);
        pItem->ClearStateFlags(1u);
        pItem->SetStateFlags(0x20004u);
    }

    // Colour again (the X360 re-tests mnColor at the tail for the layout-only flag).
    if (pSrc->mnColor != -1)
    {
        pItem = static_cast<AptRenderItemDynamicText*>(pInst->GetRenderItemWritable());
        pItem->SetStateFlags(0x400u);
    }

    return gpUndefinedValue;
}

// ===========================================================================
// sMethod_swapDepths @0x82AFBE10 -- AS swapDepths(target | depth): exchange this
// node's depth with another node's in the parent's display list.
//   .swapDepths(clip)   -- target a clip/CIHNone handle directly.
//   .swapDepths(name)   -- target the node placed under that instance name.
//   .swapDepths(depth)  -- target the node at that AS depth (+ the 0x4000 bias).
// When the resolved target is a defined, distinct node and the receiver is a
// sprite/animation instance, the two are swapped (list position + render depths);
// otherwise the receiver is moved to the requested depth (ChangeDepth). Returns
// undefined.
// ===========================================================================
AptValue* AptCIHNativeFunctionHelper::sMethod_swapDepths(AptValue* pContext, int nArgCount)
{
    AptCIH* const pNode = static_cast<AptCIH*>(pContext);

    // With more than one argument the receiver itself must be a clip handle / CIHNone
    // (X360: a2 != 1 gates on IsClipHandleOrCIHNone(this) -- when it is, bail).
    if (nArgCount != 1 && IsClipHandleOrCIHNone(pContext))
        return gpUndefinedValue;

    AptValue* const pArg = gppAptNativeArgStack[gnAptNativeArgCount - 1];   // arg 0: the swap target

    // The parent's child display-list state (parent->charInst->mDisplayList head),
    // reinterpreted as its AptDisplayListState (both are a lone AptCIH* head).
    AptCharacterSpriteInstBase* const pParentSprite =
        static_cast<AptCharacterSpriteInstBase*>(pNode->GetDisplayListParent()->GetCharacterInst());
    AptDisplayListState* const pList = pParentSprite->mDisplayList.AsState();

    AptCIH* pTarget = nullptr;   // the resolved node to swap with (X360 v29)
    AptCIH* pPrevSlot = nullptr; // findInst's insert-after out (X360 v31 scratch)

    if (IsClipHandleOrCIHNone(pArg))
    {
        // Directly a (defined) clip handle / CIHNone -> it IS the target node.
        pTarget = static_cast<AptCIH*>(pArg);
    }
    else if (pArg->isString())
    {
        // A name -> locate the listed node by that instance name.
        EAStringC strName;
        pArg->toString(&strName);
        pList->findInst(0, &strName, &pPrevSlot, &pTarget);
    }
    else if (pArg->isInteger() || pArg->isFloat())
    {
        // A depth value -> the node at that AS depth (+ the 0x4000 bias). No-op when
        // it is already this node's own depth.
        const int nDepth = pArg->toInteger() + 0x4000;
        if (nDepth == pNode->GetCharacterInst()->GetRenderItem()->GetDepth())
            return gpUndefinedValue;
        pList->findInst(nDepth, nullptr, &pPrevSlot, &pTarget);
    }

    // A defined, distinct target node + a sprite(5)/animation(9) PARENT -> swap.
    if (pTarget && pTarget->getIsDefined() && pTarget != pNode)
    {
        AptCharacterInst* const pParentInst = pNode->GetDisplayListParent()->GetCharacterInst();
        const uint32_t nTypeTag = pParentInst->GetTypeTag();   // *(charInst+8) >> 26
        AptDisplayListState* const pSwapList =
            (nTypeTag == 5 || nTypeTag == 9)
                ? static_cast<AptCharacterSpriteInstBase*>(pParentInst)->mDisplayList.AsState()
                : nullptr;
        if (pSwapList)
            pSwapList->swapDepths(pTarget, pNode);
        return gpUndefinedValue;
    }

    // Otherwise (no swappable target): when the argument is an integer/float depth,
    // move the receiver to that depth in the parent's list (X360 dereferences the
    // parent charInst's display list directly).
    if (pArg->isInteger() || pArg->isFloat())
    {
        AptDisplayListState* const pParentList =
            static_cast<AptCharacterSpriteInstBase*>(pNode->GetDisplayListParent()->GetCharacterInst())
                ->mDisplayList.AsState();
        const int nDepth = pArg->toInteger() + 0x4000;
        pParentList->ChangeDepth(static_cast<int16_t>(nDepth), pNode);
    }
    return gpUndefinedValue;
}

// ===========================================================================
// sMethod_getBytesTotal @0x82AED8E8 -- AS getBytesTotal(): the byte size of the
// movie loaded into this node. Only an imported-animation node (character type tag
// 9) carries a source .apt file; its name keys the host byte-size query. A node
// with no character instance, or a non-animation node, yields 0.
// ===========================================================================
AptValue* AptCIHNativeFunctionHelper::sMethod_getBytesTotal(AptValue* pContext, int /*nArgCount*/)
{
    AptCIH* const pNode = static_cast<AptCIH*>(pContext);
    AptCharacterInst* const pInst = pNode->GetCharacterInst();
    if (!pInst)
        return AptFloat::Create(0.0f);

    // Build the source .apt path: only a live animation node (tag 9) has one, held
    // in its AptCharacterAnimationInst::mAnimationFilePtr's AptFile::mFileName.
    EAStringC lFilePath;
    if (IsClipHandleOrCIHNone(pNode) && pInst->GetTypeTag() == 9)
    {
        AptCharacterAnimationInst* const pAnim = static_cast<AptCharacterAnimationInst*>(pInst);
        lFilePath += pAnim->mAnimationFilePtr.pData->mFileName;
    }

    float fBytesTotal = 0.0f;
    if (pInst->GetTypeTag() == 9)
        fBytesTotal = static_cast<float>(AptHook_GetBytesTotal(lFilePath.GetBuffer(), 0, 0.0));

    return AptFloat::Create(fBytesTotal);
}

// ===========================================================================
// sMethod_createEmptyMovieClip @0x82B0BC38 -- AS createEmptyMovieClip(name, depth):
// place a fresh empty movie-clip child (the shared default movie-clip character
// template) at depth+0x4000 under `name`, flag it created-dynamically, and return
// it. Requires exactly 2 args and a placed receiver (its render item has a backing
// character); otherwise undefined.
// ===========================================================================
AptValue* AptCIHNativeFunctionHelper::sMethod_createEmptyMovieClip(AptValue* pContext, int nArgCount)
{
    if (nArgCount != 2)
        return gpUndefinedValue;

    AptValue* const pNameArg  = gppAptNativeArgStack[gnAptNativeArgCount - 1];
    AptValue* const pDepthArg = gppAptNativeArgStack[gnAptNativeArgCount - 2];
    const int nDepth = pDepthArg->toInteger();

    EAStringC lName;
    pNameArg->toString(&lName);

    AptCIH* const pNode = static_cast<AptCIH*>(pContext);
    if (!pNode->GetCharacterInst()->GetRenderItem()->mpCharacter)
        return gpUndefinedValue;   // not a placed node

    // Lazily build the shared default movie-clip character template.
    if (!AptCharacterHelper::spDefaultMovieCharacter)
        AptCharacterHelper::CreateMovieCharacterInst();

    AptCIH* const pInserted = AptCIH_InsertChild(
        pNode, nullptr, AptCharacterHelper::spDefaultMovieCharacter,
        nDepth + 0x4000, &lName, nullptr);

    if (IsClipHandleOrCIHNone(pInserted))
        static_cast<AptCharacterSpriteInstBase*>(pInserted->GetCharacterInst())->SetCreatedDynamic(true);

    return pInserted;
}

// ===========================================================================
// sMethod_attachMovie @0x82B0D440 -- AS attachMovie(libraryName, newName, depth
// [, initObject]): resolve the library symbol to a character, place it as a child
// at depth+0x4000 under newName (with the optional 4th-arg init object), tick the
// newly-created instances so they are live this frame, and return the inserted
// child. Undefined when the library symbol is unknown.
// ===========================================================================
AptValue* AptCIHNativeFunctionHelper::sMethod_attachMovie(AptValue* pContext, int nArgCount)
{
    AptValue* const pLibName = gppAptNativeArgStack[gnAptNativeArgCount - 1];
    AptValue* const pNewName = gppAptNativeArgStack[gnAptNativeArgCount - 2];
    AptValue* const pDepth   = gppAptNativeArgStack[gnAptNativeArgCount - 3];
    AptValue* const pInitObj = (nArgCount < 4) ? nullptr
                                               : gppAptNativeArgStack[gnAptNativeArgCount - 4];

    EAStringC lLibName;
    pLibName->toString(&lLibName);

    AptCIH* const pNode = static_cast<AptCIH*>(pContext);

    // A node flagged "resolve symbols against the parent's library" (render-item
    // mFlags bit 27) looks the symbol up in its display-list parent instead.
    AptCIH* pScope = pNode;
    if ((pNode->GetCharacterInst()->GetRenderItem()->mFlags >> 27) & 1u)
        pScope = pNode->GetDisplayListParent();

    AptCharacter* const pCharacter = findCharacterInLibrary(pScope, &lLibName, 1);
    if (!pCharacter)
        return gpUndefinedValue;

    EAStringC lNewName;
    pNewName->toString(&lNewName);
    const int nDepth = pDepth->toInteger() + 0x4000;

    AptCIH* const pInserted =
        AptCIH_InsertChild(pNode, pNode, pCharacter, nDepth, &lNewName, pInitObj);
    AptAnimationTarget_TickNewInsts(gpAptTarget->mpAnimationTarget);

    return pInserted ? pInserted : gpUndefinedValue;
}

// FLAG (un-homed AptCIH behavioural callee): set a procedural display property
// (_x/_y/_rotation/_alpha/...) by id on the node. @0x82AE... -- declared so the AS
// creation/positioning methods compile against the same entry point.
extern void AptCIH_SetProceduralProperty(AptCIH* pNode, int nProperty, double fValue);

// ===========================================================================
// sMethod_createTextField @0x82B0BA40 -- AS createTextField(name, depth, x, y,
// width, height): place a fresh empty dynamic-text field (the shared default
// dynamic-text character) at depth+0x4000 under `name`. When the inserted node is a
// dynamic-text instance (type tag 2), seed its render item: mark it
// created-dynamically (mFlags bit 27) + visible, set the editable state bits
// (mStateFlags |= 6), position it at (x, y) with the authored +2px inset via the
// procedural _x/_y, and size its bounds rect to (width, height). Requires exactly 6
// args; always returns undefined.
// ===========================================================================
AptValue* AptCIHNativeFunctionHelper::sMethod_createTextField(AptValue* pContext, int nArgCount)
{
    if (nArgCount != 6)
        return gpUndefinedValue;

    AptValue* const pName   = gppAptNativeArgStack[gnAptNativeArgCount - 1];
    AptValue* const pDepth  = gppAptNativeArgStack[gnAptNativeArgCount - 2];
    AptValue* const pX      = gppAptNativeArgStack[gnAptNativeArgCount - 3];
    AptValue* const pY      = gppAptNativeArgStack[gnAptNativeArgCount - 4];
    AptValue* const pWidth  = gppAptNativeArgStack[gnAptNativeArgCount - 5];
    AptValue* const pHeight = gppAptNativeArgStack[gnAptNativeArgCount - 6];

    const int    nDepth  = pDepth->toInteger();
    const double fX      = pX->toFloat();
    const double fY      = pY->toFloat();
    const double fWidth  = pWidth->toFloat();
    const double fHeight = pHeight->toFloat();

    EAStringC lName;
    pName->toString(&lName);

    // Lazily build the shared default dynamic-text character template.
    if (!AptCharacterHelper::spDefaultTextCharacter)
        AptCharacterHelper::CreateTextCharacterInst();

    AptCIH* const pNode = static_cast<AptCIH*>(pContext);
    AptCIH* const pInserted = AptCIH_InsertChild(
        pNode, nullptr, AptCharacterHelper::spDefaultTextCharacter,
        nDepth + 0x4000, &lName, nullptr);

    AptCharacterInst* const pInst = pInserted->GetCharacterInst();
    if (pInst->GetTypeTag() == 2)   // dynamic-text instance
    {
        // The X360 re-fetches the writable render item per access; it is idempotent
        // within a tick, so fetch once and reuse (faithful de-optimisation).
        AptRenderItem* const pRI = pInst->GetRenderItemWritable();
        pRI->mFlags |= 0x08000000u;                 // bit 27: created-dynamic text field
        pRI->SetIsVisible(true);

        AptRenderItemDynamicText* const pTextRI = static_cast<AptRenderItemDynamicText*>(pRI);
        pTextRI->mStateFlags |= 6u;                 // editable + needs-layout state bits

        // Position at (x, y) with the authored +2px inset, then size the bounds rect.
        AptCIH_SetProceduralProperty(pInserted, 0, fX + 2.0);   // _x
        AptCIH_SetProceduralProperty(pInserted, 1, fY + 2.0);   // _y
        pTextRI->mBounds.fRight  = static_cast<float>(pTextRI->mBounds.fLeft + fWidth);
        pTextRI->mBounds.fBottom = static_cast<float>(pTextRI->mBounds.fTop + fHeight);
    }

    return gpUndefinedValue;
}

// FLAG (un-homed AS-VM callee): AptActionInterpreter::getName -- resolve the AS path
// name of a node into pOut. Declared so loadMovie compiles against the same entry.
extern void AptActionInterpreter_getName(AptCIH* pNode, EAStringC* pOut);   // AptActionInterpreter::getName

// ===========================================================================
// sMethod_loadMovie @0x82B06D10 -- AS loadMovie(url): load an external movie into
// this clip. The url must be empty or end in ".swf" (case-insensitive); the ".swf"
// suffix is stripped to form the load name, the node's AS path is resolved as the
// load target, and the linker is asked to load it. Always returns undefined.
// ===========================================================================
AptValue* AptCIHNativeFunctionHelper::sMethod_loadMovie(AptValue* pContext, int /*nArgCount*/)
{
    AptValue* const pUrlArg = gppAptNativeArgStack[gnAptNativeArgCount - 1];
    EAStringC lUrl;
    pUrlArg->toString(&lUrl);

    // Accept only an empty url or one ending in ".swf" (case-insensitive on s/w/f).
    const int         nLen  = lUrl.Size();
    const char* const pcBuf = lUrl.GetBuffer();
    bool bAccept = (nLen == 0);
    if (!bAccept && nLen >= 4)
    {
        const char* const pcExt = pcBuf + (nLen - 4);
        bAccept = pcExt[0] == '.'
               && (pcExt[1] == 's' || pcExt[1] == 'S')
               && (pcExt[2] == 'w' || pcExt[2] == 'W')
               && (pcExt[3] == 'f' || pcExt[3] == 'F');
    }

    if (bAccept)
    {
        // Strip the ".swf" suffix to form the load name.
        EAStringC lFileName = lUrl;
        if (nLen >= 4)
            lFileName.Delete(nLen - 4, 4);

        // The node's AS path is the load target name.
        EAStringC lTargetName;
        AptActionInterpreter_getName(static_cast<AptCIH*>(pContext), &lTargetName);

        // Console arg order: Load(a2 = stripped name, a3 = target path).
        gpAptTarget->mpLinker->Load(&lFileName, &lTargetName);
    }
    return gpUndefinedValue;
}

// ---------------------------------------------------------------------------
// FLAG (un-homed TextFormat-layer callees -- bodies in the AptTextFormat TU):
//   AptTextFormat_ConstructRecord (sub_82AFAEB8) -- the AS `TextFormat(...)` field
//     ctor: builds a 32-byte TextFormat record, setting mFontName (from font.toString),
//     mfSize, mnColor, mnStyleFlags (from bold/italic/underline), mnIndent/mnLeftMargin/
//     mnRightMargin, and mnAlign (from align.toString vs "left"/"right"/"true"/"center").
//     -1 / the undefined value == inherit. (Field map decoded from the asm stores
//     +0x00..+0x1C; the getters call it only with all-inherit defaults.)
//   AptTextFormat_ConstructDefault (sub_82AFB2A8) -- allocate+base-construct an empty
//     AptTextFormat (AptValueWithHash base) into the given GC block.
//   AptResolveTextFieldFontName -- the serialised .apt default-font name for a text
//     inst ("" when none); the same font-table-walk family as the static-text
//     AptResolveTextFontCharacter resolver.
// ---------------------------------------------------------------------------
extern TextFormat*    AptTextFormat_ConstructRecord(TextFormat* pRecord, AptValue* pFont,
                         float fSize, int nColor, int nBold, int nItalic, int nUnderline,
                         int nLeftMargin, int nRightMargin, int nIndent, AptValue* pAlign);  // sub_82AFAEB8
extern AptTextFormat* AptTextFormat_ConstructDefault(void* pBlock, AptValue* pSource, double dArg);  // sub_82AFB2A8
extern const char*    AptResolveTextFieldFontName(AptCharacterInst* pTextInst);

static const float KF_TEXTFORMAT_INHERIT_SIZE = -1.0f;   // flt_820037C8

// Build the field's default (all-inherit) TextFormat record when it has none, and
// install it. Shared by getNewTextFormat / getTextFormat (X360: Allocate(32) ->
// sub_82AFAEB8(undefined,-1.0,-1,...) -> AptRenderItemDynamicText::SetTextFormat).
static void EnsureFieldTextFormat(AptCIH* pNode, AptRenderItemDynamicText* pField)
{
    if (pField->mpTextFormat)
        return;
    TextFormat* pDefault = nullptr;
    if (void* pBlock = gpAptPseudoDataPool->Allocate(sizeof(TextFormat)))   // X360 Allocate(32)
        pDefault = AptTextFormat_ConstructRecord(static_cast<TextFormat*>(pBlock),
            gpUndefinedValue, KF_TEXTFORMAT_INHERIT_SIZE, -1, -1, -1, -1, -1, -1, -1, gpUndefinedValue);
    static_cast<AptRenderItemDynamicText*>(pNode->GetCharacterInst()->GetRenderItemWritable())
        ->SetTextFormat(reinterpret_cast<AptValue*>(pDefault));
}

// Overlay the field's live colour (when the format still inherits), default font name,
// alignment, and font size onto pResult's record. Shared tail of both getters.
static void OverlayFieldTextAttributes(AptCIH* pNode, AptRenderItemDynamicText* pField,
                                       AptTextFormat* pResult, bool bMaskColor)
{
    if (pResult->mFormat.mnColor == -1)
    {
        const uint32_t uColor = bMaskColor ? (pField->mTextColor & 0xFFFFFFu) : pField->mTextColor;
        pResult->mFormat.mnColor = static_cast<int32_t>(uColor);
    }

    // Default font name from the serialised .apt font table.
    EAStringC lFontName(AptResolveTextFieldFontName(pNode->GetCharacterInst()));
    pResult->mFormat.mFontName = lFontName;

    // Alignment = the field's packed alignment (mFlagsAndBackColor bits 3-6, signed).
    pResult->mFormat.mnAlign = static_cast<int32_t>(pField->mFlagsAndBackColor << 25) >> 28;
    pResult->mFormat.mfSize  = pField->mFontSize;
}

// ===========================================================================
// sMethod_getNewTextFormat @0x82AFB9F8 -- AS getNewTextFormat(): return a fresh
// TextFormat copy-constructed from this text field's current format, with the live
// colour/font/align/size overlaid. Takes no args (undefined otherwise).
// ===========================================================================
AptValue* AptCIHNativeFunctionHelper::sMethod_getNewTextFormat(AptValue* pContext, int nArgCount)
{
    if (nArgCount > 0)
        return gpUndefinedValue;

    AptCIH* const pNode = static_cast<AptCIH*>(pContext);
    AptRenderItemDynamicText* const pField =
        static_cast<AptRenderItemDynamicText*>(pNode->GetCharacterInst()->GetRenderItem());

    EnsureFieldTextFormat(pNode, pField);

    // Result copy-constructed from the field's current TextFormat record (the field's
    // mpTextFormat is a bare TextFormat record, opaquely typed AptValue* in the header).
    AptTextFormat* pResult = nullptr;
    if (void* pBlock = AptTextFormat::operator new(sizeof(AptTextFormat)))   // X360 operator new(64)
        pResult = ::new (pBlock) AptTextFormat(reinterpret_cast<const TextFormat*>(pField->mpTextFormat));

    OverlayFieldTextAttributes(pNode, pField, pResult, /*bMaskColor*/false);
    return pResult;
}

// ===========================================================================
// sMethod_getTextFormat @0x82AFBBC0 -- AS getTextFormat(): like getNewTextFormat but
// the result starts as a base-constructed empty TextFormat, copies the field's format
// in, and force-marks bold/italic/underline as "defined". Accepts up to 2 args.
// ===========================================================================
AptValue* AptCIHNativeFunctionHelper::sMethod_getTextFormat(AptValue* pContext, int nArgCount)
{
    if (nArgCount > 2)
        return gpUndefinedValue;

    AptCIH* const pNode = static_cast<AptCIH*>(pContext);
    AptRenderItemDynamicText* const pField =
        static_cast<AptRenderItemDynamicText*>(pNode->GetCharacterInst()->GetRenderItem());

    AptTextFormat* pResult = nullptr;
    if (void* pBlock = AptTextFormat::operator new(sizeof(AptTextFormat)))   // X360 operator new(64)
        pResult = AptTextFormat_ConstructDefault(pBlock, gpUndefinedValue, 0.0);

    EnsureFieldTextFormat(pNode, pField);

    // Copy the field's current format into the result, then force the three style
    // flags to "defined" so the returned object reports concrete bold/italic/underline.
    TextFormat_copyTextFormatObj(&pResult->mFormat,
                                 reinterpret_cast<const TextFormat*>(pField->mpTextFormat));
    pResult->mFormat.mnStyleFlags |= TextFormat::KU_ITALIC_DEFINED;
    pResult->mFormat.mnStyleFlags |= TextFormat::KU_UNDERLINE_DEFINED;
    pResult->mFormat.mnStyleFlags |= TextFormat::KU_BOLD_DEFINED;

    OverlayFieldTextAttributes(pNode, pField, pResult, /*bMaskColor*/true);
    return pResult;
}
