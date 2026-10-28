// ===========================================================================
// EATech Apt -- AptMovie::labelToFrame.   DECOMPILED from the PS3 EXTERNAL ELF
// (@0x7F936C). Look the label up in the timeline's label hash and return its
// stored frame index (an AptInteger), or -1 when absent.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptMovie.h"
#include "SDKs/EATech/include/Apt/AptNativeHash.h"
#include "SDKs/EATech/include/Apt/AptString/EAString.h"

#include <cstdint>

#include "SDKs/EATech/Apt/DogmaAllocator.h"                  // DOGMA_PoolManager
#include "SDKs/EATech/include/Apt/AptPseudoCIH.h"            // AptPseudoCIH_t, gpAptPseudoDataPool
#include "SDKs/EATech/include/Apt/AptPseudoDisplayList.h"    // AptPseudoDisplayList
#include "SDKs/EATech/include/Apt/AptActionQueue.h"          // AptActionQueueC
#include "SDKs/EATech/include/Apt/AptTarget.h"               // gpAptTarget (the director chain)
#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"      // AptAnimationTarget::mpActionQueue

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"    // AptActionInterpreter
#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h"   // PushStaticData/PopStaticData (the AS register window)
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"     // AptInteger::Create
#include "SDKs/EATech/include/Apt/AptDefine.h"               // gpNonGCPoolManager
#include "SDKs/EATech/include/Apt/AptString/EAString.h"

// ---- the native-8 place path (doFrameControls tag-3 + the place-command dispatcher) ----
#include "SDKs/EATech/include/Apt/AptDisplayList.h"           // AptDisplayList::placeObjectNCXForm (the homed place spine)
#include "SDKs/EATech/include/Apt/AptCIH.h"                   // AptCIH (the parent node + char-inst chain)
#include "SDKs/EATech/include/Apt/AptDisplayListState.h"      // findInst (MOVE tween keyframes + tag-4 removes)
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"         // AptCharacterInst::GetRenderItem
#include "SDKs/EATech/include/Apt/AptRenderItem.h"            // AptRenderItem::mpCharacter
#include "SDKs/EATech/include/Apt/AptCharacter.h"             // AptCharacter (the movie character + mpAnimationFile)
#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"    // AptCharacterAnimation (charTable / import table)
#include "SDKs/EATech/include/Apt/AptFile.h"                  // AptFile / AptFilePtr (the placed char's file bind)
#include "SDKs/EATech/include/Apt/AptStd/AptCXForm.h"         // AptUint32CXForm (the place record packed colour)
#include "SDKs/EATech/include/Apt/Apt.h"                      // gAptFuncs (tag-5 BackgroundColour -> pfnSetBackgroundColour @+0x10)

#include <new>   // placement new (the pool-allocated AptPseudoCIH_t nodes)

int AptMovie::labelToFrame(const EAStringC* pLabel) const
{
    if (pLabel && mpLabelHash)
    {
        AptValue* pValue = mpLabelHash->Lookup(*pLabel);
        if (pValue)
            return pValue->toInteger();
    }
    return -1;
}

// ===========================================================================
// The timeline driver / relocate methods.   DECOMPILED FAITHFULLY from the X360
// ARTIST.XEX (the authoritative spine for this TU; there is no leak/DWARF for it).
//
//   AptMovie::DoTemporaryFrameControls @ 0x82AEEB98
//   AptMovie::doFrameControls          @ 0x82B0B7A0
//   AptMovie::queueFrameActions        @ 0x82AE0228
//   AptMovie::resolve                  @ 0x82AF80B0
//   AptMovie::unresolve                @ 0x82AF4830
//
// These walk the timeline's per-frame display-list command records. Each frame is
// an AptMovieFrame {mnCommandCount @+0, mpCommands @+4}; mpCommands[i] points to a
// serialised command record whose leading dword is a tag (1=action / 2=label /
// 3=place / 4=remove / 5=back-to-script / 8=morph). Those records are the in-place
// .apt blob (no recovered struct), so they are addressed by their console byte
// offsets through the kCmd* helpers below, matching the asm's literal lwz/stw
// offsets exactly. FLAG: a typed runtime model for the command records + the
// AptActionInterpreter VM transcode are the shared follow-on with
// AptCharacterAnimation::Fixup (see that .cpp); the bodies here are the faithful
// asm decompilation that drives + relocates those raw records.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

// ---------------------------------------------------------------------------
// Sibling-owned globals / callees (all HOMED -- AptGlobals.cpp for the globals,
// noted per symbol for the callees): declared extern so this TU links; the
// single underlying object is shared.
// ---------------------------------------------------------------------------

// off_8324D808 -- the shared Apt DOGMA pool the 20-byte pseudo nodes come from
// (AptPseudoCIH.h declares it as gpAptPseudoDataPool; reused by name).

// off_8324E574 -- the current AS animation target: gpAptTarget (typed AptTarget*,
// declared by AptTarget.h; its director @+0x18 owns the AptActionQueueC* @+0x0C --
// both real members now, read through the types).

// dword_8324E514 -- the current frame's queued-action sequence id passed as
// AddActionBack's 4th arg.
extern int   gnAptActionFrameId;                // dword_8324E514 (AptGlobals.cpp)

// &dword_8324E760 -- the process-wide AS action interpreter instance.
extern AptActionInterpreter gAptActionInterpreter;   // off_8324E760  (AptGlobals.cpp)

// byte_8324D807 -- the BACKGROUND-COLOUR once-latch: a tag-5 (BackgroundColour)
// frame command fires the host callback at most once per loaded animation
// ("bg color can only be set once" -- the SDK's gbBackgroundColorSet semantics);
// AptLoadAnimation @0x82B07AC8 resets it (stb 0 @0x82B07AF4) so the NEXT loaded
// movie's first tag-5 wins. (The old "back-to-script one-shot" reading of this
// latch was wrong -- RETIRED 2026-07-12. The callee at dword_8324E828 is not a
// standalone hook: 0x8324E828 == &gAptFuncs (dword_8324E818) + 0x10 ==
// gAptFuncs.pfnSetBackgroundColour, the render-family host callback the movie's
// doFrameControls tag-5 arm dispatches the authored colour word through.)
extern unsigned char gbAptBackgroundColourSet;    // byte_8324D807

// The AS register-window globals (off_8324E3D0 / dword_8324E3D4) are
// AptScriptFunctionBase::spRegBlockCurrentFrameBase / snRegBlockCurrentFrameCount;
// save/restore around runStream is the public PushStaticData()/PopStaticData() pair
// (the duplicate gpAptRegisterBase/gnAptRegisterCount externs -- the latter also
// colliding with AptRegister.cpp's config global of the same name -- are retired).

// FLAG (deferred VM / display-list callees -- owned by other TUs, declared with the
// raw-but-faithful call-site signatures so the timeline driver links):
struct AptCIH;
extern AptCIH* AptGetAnimationAtLevel(int nLevel);                       // _AptGetAnimationAtLevel @0x82B00788 (canonical AptCIH*; void* blob walk binds)
// (The invented 6-arg AptPseudoDisplayListFindInst extern was RETIRED 2026-07-10:
// the console DoTemporaryFrameControls place arm calls the real 3-arg member
// AptPseudoDisplayList::FindInst(key, &existing, &pred) -- asm-verified r4=depth
// key, r5=&pred, r6=&existing. The stub it bound to nulled every lookup, so a
// jumpToFrame replay re-created every already-live pseudo node.)
// AptPseudoDisplayList_Insert retired: the real member AptPseudoDisplayList::Insert is called directly.

// sub_82B0AE08 @0x82B097D8's caller-side dispatcher (the place-command handler doFrameControls
// invokes for each tag-3 record). HOMED below (2026-07-01) as AptDispatchPlaceCommand -- it reads
// the serialised place-info record + calls the faithfully-homed AptDisplayList::placeObjectNCXForm.
static AptCIH* AptDispatchPlaceCommand(AptDisplayList* pDisplayList, const void* pPlaceInfo, AptCIH* pParent);

// sub_82AFD150 @0x82AFD150 -- the remove-command dispatcher (doFrameControls' tag-4 twin of
// AptDispatchPlaceCommand).
// X360 body (two calls, both already homed):
//   AptDisplayListState::findInst(*a1, a2, 0, &prev, &match);   // r3=*list (the state), r4=depth
//   AptDisplayList::removeObject(a1, match);
static void AptDispatchRemoveCommand(AptDisplayList* pDisplayList, int nDepth)
{
    AptCIH* pPrev  = nullptr;   // the console's 12-byte prev triple; only the out-pointers matter
    AptCIH* pMatch = nullptr;
    pDisplayList->AsState()->findInst(nDepth, nullptr, &pPrev, &pMatch);
    pDisplayList->removeObject(pMatch);
}

// ---------------------------------------------------------------------------
// Command-record byte accessors. The serialised .apt command record has no
// recovered struct, so it is addressed by the console byte offsets the asm uses
// (lwz/stw at literal offsets off the record base). Helpers keep the access
// 8-byte-pointer-safe on x64 while documenting the [c:0xNN] console offset.
// ---------------------------------------------------------------------------
namespace
{
    inline int32_t  CmdI32(void* pRec, int nOff)  { return *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(pRec) + nOff); }
    inline void     CmdSetI32(void* pRec, int nOff, int32_t v) { *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(pRec) + nOff) = v; }
    inline void*    CmdPtr(void* pRec, int nOff)  { return *reinterpret_cast<void**>(reinterpret_cast<char*>(pRec) + nOff); }
    inline float    CmdF32(void* pRec, int nOff)  { return *reinterpret_cast<float*>(reinterpret_cast<char*>(pRec) + nOff); }

    // TRANSITION-ERA dual-format payload read (FLAG): real native-8 (XB1-verified)
    // carries the tag-4 depth / tag-5 payload at cmd+8 with cmd+4 the tag PAD
    // (always 0); the retired apt_convert bundles carried it at cmd+4 (nonzero --
    // authored depths are >= 1). Both GUIAPT sets are still in circulation while
    // the import-EXPORT resolution lands, so discriminate on the +4 word. Remove
    // this (fixed +8) when the converter set is finally retired.
    inline int32_t  CmdPayloadI32(void* pRec)
    {
        const int32_t nAt4 = CmdI32(pRec, 0x04);
        return (nAt4 != 0) ? nAt4 : CmdI32(pRec, 0x08);
    }
}



// ===========================================================================
// AptDispatchPlaceCommand -- sub_82B0AE08 @0x82B0AE08 (the place-command dispatcher the
// timeline path invokes for each tag-3 record). DECOMPILED FAITHFULLY from the X360
// ARTIST.XEX. It reads the serialised PlaceObject record and calls the faithfully-homed
// AptDisplayList::placeObjectNCXForm to create/update the placed AptCharacterInst at the
// record's depth + insert it into the display list. This is the decisive leaf -- it is
// what puts a character on screen.
//
// RECORD LAYOUT (native-8 GUIAPT "1:7:8", from the libapt2 PlaceObject::Write). The record
// is {tag@0, <align to pointer size>, body...}: FrameItem::Write writes the u32 tag then
// Align()s to the pointer size, so on native-8 the BODY starts at align8(record + 4). The
// caller passes pPlaceInfo == record + 4 (the X360 r31 == cmd+4), so the body base is
// align8(pPlaceInfo). Body-relative field offsets (VERIFIED vs TITLE_SCREEN02.bundle frame-0):
//   +0x00  flags   bit0 Move / bit1 HasCharacter / bit2 HasMatrix / bit3 HasColorTransform /
//                   bit4 HasRatio / bit5 HasName / bit6 HasClipDepth / bit7 HasClipActions
//   +0x04  depth (i32)          +0x08  charId (i32) -> movie->charTable[charId]
//   +0x0C  matrix (mat3x2, 6 floats)                   +0x24  colorMult (u8vec4)
//   +0x28  colorAdd (u8vec4)    +0x2C  ratio (f32)
//   +0x30  name pointer (8B, relocated by resolve64)   +0x38  clipDepth (i32)
//   +0x3C..+0x40 (align) clipActions pointer (8B)
// The colour transform in the file is the PC-format {colorMult, colorAdd} pair at body+0x24
// (an AptUint32CXForm). Console offsets (4-byte pointers) put name@+0x30/clipDepth@+0x34;
// the 8-byte name pointer shifts clipDepth to +0x38 on native-8.
//
// The X360 places when the record HasCharacter (bit1) -- the plain new-place -- or Move
// (bit0). It resolves the character, builds the EAStringC name (when HasName), and calls
// placeObjectNCXForm. Reconstructed by ROLE using the homed placeObjectNCXForm (named types).
// ===========================================================================
static AptCIH* AptDispatchPlaceCommand(AptDisplayList* pDisplayList, const void* pPlaceInfo, AptCIH* pParent)
{
    // The record body is pointer-aligned after the tag (FrameItem::Write Align()); the caller
    // passes record+4, so align it up to the 8-byte pointer boundary to reach the body base.
    const uintptr_t luBody = (reinterpret_cast<uintptr_t>(pPlaceInfo) + 7u) & ~static_cast<uintptr_t>(7u);
    char* const pBody = reinterpret_cast<char*>(luBody);

    const uint32_t nFlags = static_cast<uint32_t>(CmdI32(pBody, 0x00));   // serialized .apt place record

    // Place when the record carries a character (bit1 HasCharacter) or is a Move (bit0);
    // otherwise it is a no-op (the X360's `(flags&2)==0 && (flags&1)==0 -> 0`).
    const bool bHasCharacter = (nFlags & 0x02u) != 0u;
    const bool bMove         = (nFlags & 0x01u) != 0u;
    if (!bHasCharacter && !bMove)
        return nullptr;

    const int32_t nDepth  = CmdI32(pBody, 0x04);
    const int32_t nCharId = CmdI32(pBody, 0x08);

    // The placed character = the owning movie's charTable[charId]. The owner movie char-anim
    // is reached through the parent CIH's named char-inst chain (parent->charInst->renderItem
    // ->mpCharacter is the owning movie character; its embedded AptCharacterAnimation is at
    // char + KU_AptEmbeddedMovieOff). (Console: *(*(*(*(*(*(a3+32)+4)+4)+4)+32) + 4*charId).)
    AptCharacter* const pOwnerChar = pParent->GetCharacterInst()->GetRenderItem()->mpCharacter;
    // Resolve against the ROOT movie's shared charTable via the mpFixupLink back-link (char+0x08),
    // matching the X360 `*(*(*(*(a3+32)+4)+4)+4)+0x10` chain (the console comment above): the nested
    // sprite's own +0x20 has no charTable. mpFixupLink is charTable[0] (the root char), written by
    // Fixup for every character (@0x82AFF268). native-8: mpFixupLink@0x08, embedded movie @+0x20.
    AptCharacter* const pRootChar = pOwnerChar->mpFixupLink;
    AptCharacterAnimation* const pAnim = reinterpret_cast<AptCharacterAnimation*>(
        reinterpret_cast<char*>(pRootChar) + KU_AptEmbeddedMovieOff);
    // Same deferred deep-nesting guard as doFrameControls (see the FLAG there): a deep nested
    // container's back-link may not reach a charTable-bearing root (garbage mpCharacterTable /
    // mnCharacterCount), so validate the table before indexing it.
    const uintptr_t luAnimTablePtr = reinterpret_cast<uintptr_t>(pAnim->mpCharacterTable);
    const bool bAnimSane =
        (pAnim->mnCharacterCount > 0 && pAnim->mnCharacterCount <= 0x10000) &&
        (luAnimTablePtr >= 0x10000u) && ((luAnimTablePtr >> 47) == 0u);
    AptCharacter* const pCharacter =
        (bAnimSane && nCharId >= 0 && nCharId < pAnim->mnCharacterCount)
            ? pAnim->mpCharacterTable[nCharId] : nullptr;

    // FLAG (import not loaded): a null charTable entry is an IMPORTED character (from another
    // GUIAPT bundle) whose id is in this movie's import table -- e.g. TITLE_SCREEN02 charId 30 ==
    // import 'B5HelperComponents::TransitionComponent'. Fixup pass-3 (the import .apt load) is the
    // DEFERRED bring-up boundary, so imported characters are not resolved and their table slot stays
    // null. The console would place the resolved import here; on our bring-up we skip the FRESH
    // place for that one character (the null guard below the Move branch -- placing a null
    // character would AV in instantiateCharacter). A MOVE record (charId == -1) must NOT bail
    // here: it targets the EXISTING node at its depth (the tween-keyframe path below). The
    // movie's OWN (non-import) characters still place, so the title composes fully.

    // The instance name (when HasName, bit5): the record's name pointer @body+0x30 was relocated
    // to a live C string by resolve64. Build a bracketed EAStringC over it.
    const EAStringC* pName = nullptr;
    EAStringC nameStr;
    if ((nFlags & 0x20u) != 0u)
    {
        const char* const pNamePtr = static_cast<const char*>(CmdPtr(pBody, 0x30));   // relocated name C-string
        if (pNamePtr != nullptr)
        {
            nameStr = EAStringC(pNamePtr);
            pName = &nameStr;
        }
    }

    // The place payload (each gated by its flag bit):
    //   position matrix @body+0x0C (HasMatrix bit2)   colour @body+0x24 (HasColorTransform bit3)
    //   ratio @body+0x2C (HasRatio bit4)              clipDepth @body+0x38 (native-8, 8-byte name ptr)
    //   clipActions block @body+0x40 (HasClipActions bit7; the slot resolve64 relocated)
    const float* const pPosition = ((nFlags & 0x04u) != 0u)
        ? reinterpret_cast<const float*>(pBody + 0x0C) : nullptr;
    const AptUint32CXForm* const pPackedColor = ((nFlags & 0x08u) != 0u)
        ? reinterpret_cast<const AptUint32CXForm*>(pBody + 0x24) : nullptr;
    const double fFrameValue = static_cast<double>(CmdF32(pBody, 0x2C));
    const int16_t nClipDepth = static_cast<int16_t>(CmdI32(pBody, 0x38));

    // The clipActions/handler-list block pointer: placed into the sprite inst's
    // mpClipEventHandlers slot (queueClipEvents scans it; _addToSetCaches folds its
    // masks). The block + each record's stream pointer are already relocated (and the
    // streams parsed) by resolve64's case-3 walk.
    const void* pClipActions = nullptr;
    if ((nFlags & 0x80u) != 0u)
        pClipActions = CmdPtr(pBody, 0x40);   // clipActions block (resolve64-relocated)

    // ---- MOVE (bit0 without bit1 HasCharacter): the TWEEN-KEYFRAME path ----------------
    // The X360 `(flags & 2) == 0` branch: findInst the EXISTING node at this depth and
    // re-place it (pExistingNode set, no character, no name, depth 0, clipDepth -1) so the
    // record's matrix/colour land on the live item -- these charId==-1 records are every
    // animation's tween keyframes (dropping them froze all the title transitions after
    // their discrete PLACE frames). Only a node whose ASChanged flag is clear moves (the
    // console `*(v18+12) >= 0` sign test on the reversed X360 bit31; x64 bit 0); a
    // missing node falls through to the
    // fresh-place path (which the null-character guard below then skips, as the console's
    // charTable[-1] junk-place never composes on our bring-up either).
    if (bMove && !bHasCharacter)
    {
        AptCIH* pPrev = nullptr;
        AptCIH* pMatch = nullptr;
        AptDisplayListState* pState = pDisplayList->AsState();
        if (pState != nullptr)
            pState->findInst(nDepth, nullptr, &pPrev, &pMatch);
        if (pMatch != nullptr)
        {
            if (!pMatch->GetASChanged())   // x64 bit 0 (X360: sign test on reversed bit31)
            {
                return pDisplayList->placeObjectNCXForm(
                    /*pExistingNode*/ pMatch, /*nDepth*/ 0, /*pCharacter*/ nullptr,
                    /*pName*/ nullptr, pParent, /*bForceRemove*/ 0, /*nClipDepth*/ -1,
                    fFrameValue, pPosition, pClipActions, pPackedColor);
            }
            return nullptr;
        }
        // no existing node at that depth: fall through (fresh place when a character exists)
    }

    if (pCharacter == nullptr)
        return nullptr;   // (see the import-not-loaded FLAG above; also the charId==-1 Move miss)

    return pDisplayList->placeObjectNCXForm(
        /*pExistingNode*/ nullptr, nDepth, pCharacter, pName, pParent,
        /*bForceRemove*/  0, nClipDepth, fFrameValue, pPosition,
        /*pPlacementClipActions*/ pClipActions, pPackedColor);
}


// ---------------------------------------------------------------------------
// runFrameActions @PS3 0x820FA4 -- run frame nFrame's tag-1 ACTION commands
// IMMEDIATELY on the VM singleton (the CallFrame opcode's synchronous sibling of
// queueFrameActions): for each action command, push a fresh register-block window
// (the console's PrepareForExecution is a pure thunk to PushStaticData), runStream
// the command's action stream with pCIH's root-animation character-inst as the run
// scope (GetRootAnimation()[8] on the console), then pop the window back
// (CleanupAfterExecution). The console re-reads the frame record each iteration
// (the stream may mutate the timeline); reproduced. Returns `this` (the console's
// incidental r3; the CallFrame caller ignores it).
// ---------------------------------------------------------------------------
const AptMovie* AptMovie::runFrameActions(AptCIH* pCIH, int nFrame) const
{
    for (int32_t i = 0; ; ++i)
    {
        // Re-read the frame record every iteration (console: reloads *(this+4)+8*n).
        const AptMovieFrame& lFrame = mpFrames[nFrame];
        if (i >= lFrame.mnCommandCount)
            break;

        void* pCmd = lFrame.mpCommands[i];
        // FLAG (converter data boundary): per-entry plausibility screen, matching
        // doFrameControls/queueFrameActions (null / straddle-garbage slots skipped).
        {
            const uintptr_t luCmd = reinterpret_cast<uintptr_t>(pCmd);
            if (!(luCmd > 0x0000000200000000ull && (luCmd >> 47) == 0u &&
                  (luCmd & 0xFFFFFFFFull) != 0u))
                continue;
        }
        if (CmdI32(pCmd, 0x00) != 1)
            continue;                              // only tag-1 ACTION commands run

        // PrepareForExecution(&gAptActionInterpreter, &setup) == PushStaticData.
        void* pSavedBase = AptScriptFunctionBase::PushStaticData();

        AptCharacterInst* pInst = nullptr;
        if (pCIH)
            pInst = pCIH->GetRootAnimation()->GetCharacterInst();   // console [8] on the root

        // The action stream pointer: console cmd[1] (+4); native-8 record slot +0x08
        // (relocated live by resolve64 -- same slot queueFrameActions reads).
        gAptActionInterpreter.runStream(
            static_cast<const unsigned char*>(CmdPtr(pCmd, 0x08)), pCIH, -1, pInst);

        gAptActionInterpreter.CleanupAfterExecution(pSavedBase);
    }
    return this;
}

// ===========================================================================
// resolve @ 0x82AF80B0 -- relocate the (just-loaded) timeline against the load base
// nBase: allocate the label hash, add the base to every file-relative pointer slot
// in the timeline (frame table, command list, per-command pointers), and re-parse
// each action stream (AptActionInterpreter::_parseStream) + register each label.
//
// `this`/a1 is the AptMovie*; nBase the relocation base (a2); a3/a4 the parse
// context. Returns the last value cl held in r3 (the last sub-call result), faithful
// to the asm.
// ===========================================================================
void* AptMovie::resolve(int nBase, void* a3, int a4)
{
    void* result;

    // ---- allocate + init the label hash (5 dwords, tag 2) ------------------
    void* pHash = gpAptPseudoDataPool->Allocate(sizeof(AptPseudoCIH_t));   // [c: 20]
    void* pHashOut;
    if (pHash)
    {
        CmdSetI32(pHash, 0x04, 0);
        CmdSetI32(pHash, 0x08, 0);
        CmdSetI32(pHash, 0x0C, 0);
        CmdSetI32(pHash, 0x10, 0);
        CmdSetI32(pHash, 0x00, 2);
        pHashOut = pHash;
    }
    else
    {
        pHashOut = nullptr;
    }
    result = pHashOut;

    // a1[2] = hash; a1[1] = relocate(a1[1]) (the frame table base).
    int32_t nFrames = this->mnFrameCount;              // *a1
    this->mpLabelHash = reinterpret_cast<AptNativeHash*>(pHashOut);
    {
        int32_t v10 = reinterpret_cast<intptr_t>(this->mpFrames);   // a1[1]
        int32_t v11 = v10 ? (v10 + nBase) : 0;
        this->mpFrames = reinterpret_cast<AptMovieFrame*>(static_cast<intptr_t>(v11));
    }

    if (nFrames <= 0)
        return result;

    for (int32_t f = 0; f < nFrames; ++f)
    {
        AptMovieFrame* pFrame = &mpFrames[f];          // v15 + a1[1]

        // relocate the frame's command-list pointer.
        {
            int32_t v17 = reinterpret_cast<intptr_t>(pFrame->mpCommands);
            int32_t v18 = v17 ? (v17 + nBase) : 0;
            pFrame->mpCommands = reinterpret_cast<void**>(static_cast<intptr_t>(v18));
        }

        int32_t nCmds = pFrame->mnCommandCount;        // *(v15 + a1[1])
        for (int32_t i = 0; i < nCmds; ++i)
        {
            // relocate the command-pointer slot itself.
            void** pSlot = &pFrame->mpCommands[i];      // *(v21 + v20)
            {
                int32_t v22 = reinterpret_cast<intptr_t>(*pSlot);
                int32_t v23 = v22 ? (v22 + nBase) : 0;
                *pSlot = reinterpret_cast<void*>(static_cast<intptr_t>(v23));
            }

            void* pCmd = *pSlot;                         // v24
            int32_t eTag = CmdI32(pCmd, 0x00);           // *v24

            switch (eTag)
            {
                case 1:   // action command
                {
                    // relocate the inline action-stream pointer (+4), then re-parse.
                    int32_t v46 = CmdI32(pCmd, 0x04);
                    int32_t v47 = v46 ? (v46 + nBase) : 0;
                    CmdSetI32(pCmd, 0x04, v47);
                    result = const_cast<unsigned char*>(gAptActionInterpreter._parseStream(
                                 reinterpret_cast<const unsigned char*>(CmdPtr(pCmd, 0x04)),
                                 nBase, reinterpret_cast<AptValue*>(a3),
                                 a4));
                    break;
                }
                case 2:   // label command
                {
                    // relocate the label-name pointer (+4), register name -> frame f.
                    int32_t v43 = CmdI32(pCmd, 0x04);
                    int32_t v44 = v43 ? (v43 + nBase) : 0;
                    CmdSetI32(pCmd, 0x04, v44);

                    // console: InitFromBuffer(&scratch, name) ... Set ...
                    // DecreaseInternalRefCount(scratch) -- the RAII EAStringC pair.
                    EAStringC label(reinterpret_cast<const char*>(CmdPtr(pCmd, 0x04)));
                    AptInteger* pIdx = AptInteger::Create(f);
                    this->mpLabelHash->Set(label, pIdx);
                    result = pIdx;
                    break;
                }
                case 3:   // place command
                {
                    // relocate the place record's name pointer (+0x34) and its
                    // init-action block pointer (+0x3C); parse each init stream.
                    int32_t v29 = CmdI32(pCmd, 0x34);
                    int32_t v30 = v29 ? (v29 + nBase) : 0;
                    CmdSetI32(pCmd, 0x34, v30);

                    int32_t v32 = CmdI32(pCmd, 0x3C);
                    int32_t v33 = v32 ? (v32 + nBase) : 0;
                    CmdSetI32(pCmd, 0x3C, v33);

                    void* pInit = CmdPtr(pCmd, 0x3C);    // v34
                    if (pInit)
                    {
                        // relocate the init block's body pointer (+4), then walk
                        // its records (12 bytes each), relocating + parsing the
                        // stream pointer at +8 of each.
                        int32_t v35 = CmdI32(pInit, 0x04);
                        int32_t v36 = v35 ? (v35 + nBase) : 0;
                        int32_t v37 = CmdI32(pInit, 0x00);
                        CmdSetI32(pInit, 0x04, v36);
                        for (int32_t k = 0; k < v37; ++k)
                        {
                            char* pRec = reinterpret_cast<char*>(CmdPtr(pInit, 0x04)) + 12 * k;
                            int32_t v41 = CmdI32(pRec, 8);   // serialized .apt init-block record: stream ptr @+8
                            int32_t v42 = v41 ? (v41 + nBase) : 0;
                            CmdSetI32(pRec, 8, v42);
                            result = const_cast<unsigned char*>(gAptActionInterpreter._parseStream(
                                         reinterpret_cast<const unsigned char*>(CmdPtr(pRec, 8)),
                                         nBase, reinterpret_cast<AptValue*>(a3),
                                         a4));
                        }
                    }
                    break;
                }
                case 8:   // morph / action-bearing command
                {
                    // relocate the inline stream pointer (+8), then parse it.
                    int32_t v26 = CmdI32(pCmd, 0x08);
                    int32_t v27 = v26 ? (v26 + nBase) : 0;
                    CmdSetI32(pCmd, 0x08, v27);
                    result = const_cast<unsigned char*>(gAptActionInterpreter._parseStream(
                                 reinterpret_cast<const unsigned char*>(CmdPtr(pCmd, 0x08)),
                                 nBase, reinterpret_cast<AptValue*>(a3),
                                 a4));
                    break;
                }
                default:
                    break;
            }
        }
    }
    return result;
}

// ===========================================================================
// unresolve @ 0x82AF4830 -- the inverse of resolve: walk the timeline subtracting
// nBase from every relocated pointer slot (un-parsing each action stream first),
// then tear down the label hash. `this`/a1 is the AptMovie*, nBase the base (a2),
// a3 the parse context. Returns the (deleted) label-hash pointer, faithful to the
// asm (the X360 returns r3 from the scalar-deleting destructor / the relocated
// frame-table pointer when no hash existed).
// ===========================================================================
void* AptMovie::unresolve(int nBase, int a3)
{
    int32_t nFrames = this->mnFrameCount;              // *a1
    if (nFrames > 0)
    {
        for (int32_t f = 0; f < nFrames; ++f)
        {
            AptMovieFrame* pFrame = &mpFrames[f];      // v7 + a1[1]

            int32_t nCmds = pFrame->mnCommandCount;    // *(v7 + a1[1])
            for (int32_t i = 0; i < nCmds; ++i)
            {
                void* pCmd = pFrame->mpCommands[i];    // *(*(v7+a1[1]+4) + v9)
                int32_t eTag = CmdI32(pCmd, 0x00);     // *v10

                if (eTag == 1)
                {
                    // action: un-parse then un-relocate the stream pointer (+4).
                    gAptActionInterpreter._parseStream(
                        reinterpret_cast<const unsigned char*>(CmdPtr(pCmd, 0x04)),
                        nBase, nullptr, a3);
                    int32_t v34 = CmdI32(pCmd, 0x04);
                    int32_t v35 = v34 ? (v34 - nBase) : 0;
                    CmdSetI32(pCmd, 0x04, v35);
                }
                else if (eTag == 2)
                {
                    // label: un-relocate the name pointer (+4).
                    int32_t v10 = CmdI32(pCmd, 0x04);
                    int32_t v17 = v10 ? (v10 - nBase) : 0;
                    CmdSetI32(pCmd, 0x04, v17);
                }
                else if (eTag == 3)
                {
                    // place: un-parse + un-relocate the init-action block (+0x3C),
                    // then un-relocate the place name pointers (+0x34, +0x3C).
                    void* pInit = CmdPtr(pCmd, 0x3C);  // v18
                    if (pInit)
                    {
                        int32_t v19 = CmdI32(pInit, 0x00);
                        for (int32_t k = 0; k < v19; ++k)
                        {
                            char* pRec = reinterpret_cast<char*>(CmdPtr(pInit, 0x04)) + 12 * k;
                            gAptActionInterpreter._parseStream(
                                reinterpret_cast<const unsigned char*>(CmdPtr(pRec, 8)),   // serialized .apt init record stream ptr @+8
                                nBase, nullptr, a3);
                            int32_t v22 = CmdI32(pRec, 8);
                            int32_t v23 = v22 ? (v22 - nBase) : 0;
                            CmdSetI32(pRec, 8, v23);
                        }
                        int32_t v24 = CmdI32(pInit, 0x04);
                        int32_t v25 = v24 ? (v24 - nBase) : 0;
                        CmdSetI32(pInit, 0x04, v25);
                    }

                    int32_t v27 = CmdI32(pCmd, 0x34);
                    int32_t v28 = v27 ? (v27 - nBase) : 0;
                    CmdSetI32(pCmd, 0x34, v28);

                    int32_t v30 = CmdI32(pCmd, 0x3C);
                    int32_t v31 = v30 ? (v30 - nBase) : 0;
                    CmdSetI32(pCmd, 0x3C, v31);
                }
                else if (eTag == 8)
                {
                    // morph: un-parse + un-relocate the stream pointer (+8); if the
                    // command's id (+4) is negative (run this frame), restore it.
                    gAptActionInterpreter._parseStream(
                        reinterpret_cast<const unsigned char*>(CmdPtr(pCmd, 0x08)),
                        nBase, nullptr, a3);
                    int32_t v13 = CmdI32(pCmd, 0x08);
                    int32_t v14 = v13 ? (v13 - nBase) : 0;
                    CmdSetI32(pCmd, 0x08, v14);

                    void* pCmd2 = pFrame->mpCommands[i];
                    if (CmdI32(pCmd2, 0x04) < 0)
                        CmdSetI32(pCmd2, 0x04, -CmdI32(pCmd2, 0x04));
                }

                // un-relocate the command-pointer slot itself (subf; note the asm
                // here guards on the ORIGINAL value being non-zero before write).
                void** pSlot = &pFrame->mpCommands[i];
                int32_t v37 = reinterpret_cast<intptr_t>(*pSlot) - nBase;
                if (*pSlot == nullptr)
                    v37 = 0;
                *pSlot = reinterpret_cast<void*>(static_cast<intptr_t>(v37));
            }

            // un-relocate the frame's command-list pointer.
            int32_t v39 = reinterpret_cast<intptr_t>(pFrame->mpCommands);
            int32_t v40 = v39 ? (v39 - nBase) : 0;
            pFrame->mpCommands = reinterpret_cast<void**>(static_cast<intptr_t>(v40));
        }
    }

    // un-relocate the frame-table base, then destroy + free the label hash.
    int32_t v41 = reinterpret_cast<intptr_t>(this->mpFrames);
    int32_t v42 = v41 ? (v41 - nBase) : 0;
    void* result = this->mpLabelHash;                  // a1[2]
    this->mpFrames = reinterpret_cast<AptMovieFrame*>(static_cast<intptr_t>(v42));
    if (this->mpLabelHash)
    {
        AptNativeHash* pHash = this->mpLabelHash;
        pHash->DestroyGCPointers();
        // X360: the 'scalar deleting destructor' (@0x82AF0918) = ~AptNativeHash()
        // (a no-op now mpTable is freed) + Deallocate(this, sizeof). It returns the
        // freed pointer in r3; reproduced as that pointer.
        pHash->~AptNativeHash();
        gpNonGCPoolManager->Deallocate(pHash, sizeof(AptNativeHash));
        result = pHash;
        this->mpLabelHash = nullptr;
    }
    return result;
}
