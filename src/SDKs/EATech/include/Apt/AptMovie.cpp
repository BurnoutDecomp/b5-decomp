// ===========================================================================
// EATech Apt -- AptMovie::labelToFrame.   DECOMPILED from the PS3 EXTERNAL ELF
// (@0x7F936C). Look the label up in the timeline's label hash and return its
// stored frame index (an AptInteger), or -1 when absent.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptMovie.h"
#include "SDKs/EATech/include/Apt/AptNativeHash.h"
#include "SDKs/EATech/include/Apt/AptString/EAString.h"

#include "types.hpp"

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
#include "SDKs/EATech/include/Apt/AptCharacterSpriteInstBase.h"
#include "SDKs/EATech/include/Apt/AptPseudoData.h"
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
    AptCIH* pPrev  = 0;   // the console's 12-byte prev triple; only the out-pointers matter
    AptCIH* pMatch = 0;
    pDisplayList->AsState()->findInst(nDepth, 0, &pPrev, &pMatch);
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
        return 0;

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
            ? pAnim->mpCharacterTable[nCharId] : 0;

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
    const EAStringC* pName = 0;
    EAStringC nameStr;
    if ((nFlags & 0x20u) != 0u)
    {
        const char* const pNamePtr = static_cast<const char*>(CmdPtr(pBody, 0x30));   // relocated name C-string
        if (pNamePtr != 0)
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
        ? reinterpret_cast<const float*>(pBody + 0x0C) : 0;
    const AptUint32CXForm* const pPackedColor = ((nFlags & 0x08u) != 0u)
        ? reinterpret_cast<const AptUint32CXForm*>(pBody + 0x24) : 0;
    const double fFrameValue = static_cast<double>(CmdF32(pBody, 0x2C));
    const int16_t nClipDepth = static_cast<int16_t>(CmdI32(pBody, 0x38));

    // The clipActions/handler-list block pointer: placed into the sprite inst's
    // mpClipEventHandlers slot (queueClipEvents scans it; _addToSetCaches folds its
    // masks). The block + each record's stream pointer are already relocated (and the
    // streams parsed) by resolve64's case-3 walk.
    const void* pClipActions = 0;
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
        AptCIH* pPrev = 0;
        AptCIH* pMatch = 0;
        AptDisplayListState* pState = pDisplayList->AsState();
        if (pState != 0)
            pState->findInst(nDepth, 0, &pPrev, &pMatch);
        if (pMatch != 0)
        {
            if (!pMatch->GetASChanged())   // x64 bit 0 (X360: sign test on reversed bit31)
            {
                return pDisplayList->placeObjectNCXForm(
                    /*pExistingNode*/ pMatch, /*nDepth*/ 0, /*pCharacter*/ 0,
                    /*pName*/ 0, pParent, /*bForceRemove*/ 0, /*nClipDepth*/ -1,
                    fFrameValue, pPosition, pClipActions, pPackedColor);
            }
            return 0;
        }
        // no existing node at that depth: fall through (fresh place when a character exists)
    }

    if (pCharacter == 0)
        return 0;   // (see the import-not-loaded FLAG above; also the charId==-1 Move miss)

    return pDisplayList->placeObjectNCXForm(
        /*pExistingNode*/ 0, nDepth, pCharacter, pName, pParent,
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

        AptCharacterInst* pInst = 0;
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
namespace
{
    // Native command records: ARTIST 0x82AF80B0 and 0x82AF4830.
    struct AptTimelineCommand { int32_t mnTag; };
    struct AptTimelineAction { int32_t mnTag; unsigned char* mpStream; };
    struct AptTimelineInitAction { int32_t mnTag; int32_t mnIndicator; unsigned char* mpStream; };
    template<class T> void AptTimelineRelocate(T*& p, intptr_t base, bool unresolve)
    {
        if (p) p = reinterpret_cast<T*>(unresolve ? reinterpret_cast<intptr_t>(p) - base : reinterpret_cast<intptr_t>(p) + base);
    }
}

void AptMovie::resolve(int nBase, AptConstFile* pConstFile, int32_t* pValueCount)
{
    void* memory = gpAptPseudoDataPool->Allocate(sizeof(AptNativeHash));
    mpLabelHash = memory ? ::new (memory) AptNativeHash(2) : 0;
    AptTimelineRelocate(mpFrames, nBase, false);
    for (int32_t f = 0; f < mnFrameCount; ++f)
    {
        AptMovieFrame& frame = mpFrames[f];
        AptTimelineRelocate(frame.mpCommands, nBase, false);
        for (int32_t i = 0; i < frame.mnCommandCount; ++i)
        {
            AptTimelineRelocate(frame.mpCommands[i], nBase, false);
            AptTimelineCommand* command = static_cast<AptTimelineCommand*>(frame.mpCommands[i]);
            switch (command->mnTag)
            {
            case 1:
            {
                AptTimelineAction* action = reinterpret_cast<AptTimelineAction*>(command);
                AptTimelineRelocate(action->mpStream, nBase, false);
                AptActionInterpreter::_parseStream(action->mpStream, nBase, pConstFile, pValueCount);
                break;
            }
            case 2:
            {
                AptTimelineAction* label = reinterpret_cast<AptTimelineAction*>(command);
                AptTimelineRelocate(label->mpStream, nBase, false);
                EAStringC name(reinterpret_cast<const char*>(label->mpStream));
                mpLabelHash->Set(name, AptInteger::Create(f));
                break;
            }
            case 3:
            {
                AptPlaceObjectBody_t* place = reinterpret_cast<AptPlaceObjectInfo_t*>(command)->GetBody();
                AptTimelineRelocate(place->mpName, nBase, false);
                AptTimelineRelocate(place->mpClipActions, nBase, false);
                AptClipEventHandlerList* list = static_cast<AptClipEventHandlerList*>(place->mpClipActions);
                if (list)
                {
                    AptTimelineRelocate(list->mpHandlers, nBase, false);
                    for (int32_t k = 0; k < list->mnCount; ++k)
                    {
                        AptTimelineRelocate(list->mpHandlers[k].mpActionStream, nBase, false);
                        AptActionInterpreter::_parseStream(const_cast<unsigned char*>(list->mpHandlers[k].mpActionStream), nBase, pConstFile, pValueCount);
                    }
                }
                break;
            }
            case 8:
            {
                AptTimelineInitAction* action = reinterpret_cast<AptTimelineInitAction*>(command);
                AptTimelineRelocate(action->mpStream, nBase, false);
                AptActionInterpreter::_parseStream(action->mpStream, nBase, pConstFile, pValueCount);
                break;
            }
            default: break;
            }
        }
    }
}

void AptMovie::unresolve(int nBase, int32_t* pValueCount)
{
    for (int32_t f = 0; f < mnFrameCount; ++f)
    {
        AptMovieFrame& frame = mpFrames[f];
        for (int32_t i = 0; i < frame.mnCommandCount; ++i)
        {
            AptTimelineCommand* command = static_cast<AptTimelineCommand*>(frame.mpCommands[i]);
            switch (command->mnTag)
            {
            case 1:
            {
                AptTimelineAction* action = reinterpret_cast<AptTimelineAction*>(command);
                AptActionInterpreter::_parseStream(action->mpStream, nBase, 0, pValueCount);
                AptTimelineRelocate(action->mpStream, nBase, true);
                break;
            }
            case 2:
                AptTimelineRelocate(reinterpret_cast<AptTimelineAction*>(command)->mpStream, nBase, true);
                break;
            case 3:
            {
                AptPlaceObjectBody_t* place = reinterpret_cast<AptPlaceObjectInfo_t*>(command)->GetBody();
                AptClipEventHandlerList* list = static_cast<AptClipEventHandlerList*>(place->mpClipActions);
                if (list)
                {
                    for (int32_t k = 0; k < list->mnCount; ++k)
                    {
                        AptActionInterpreter::_parseStream(const_cast<unsigned char*>(list->mpHandlers[k].mpActionStream), nBase, 0, pValueCount);
                        AptTimelineRelocate(list->mpHandlers[k].mpActionStream, nBase, true);
                    }
                    AptTimelineRelocate(list->mpHandlers, nBase, true);
                }
                AptTimelineRelocate(place->mpName, nBase, true);
                AptTimelineRelocate(place->mpClipActions, nBase, true);
                break;
            }
            case 8:
            {
                AptTimelineInitAction* action = reinterpret_cast<AptTimelineInitAction*>(command);
                AptActionInterpreter::_parseStream(action->mpStream, nBase, 0, pValueCount);
                AptTimelineRelocate(action->mpStream, nBase, true);
                if (action->mnIndicator < 0) action->mnIndicator = -action->mnIndicator;
                break;
            }
            default: break;
            }
            AptTimelineRelocate(frame.mpCommands[i], nBase, true);
        }
        AptTimelineRelocate(frame.mpCommands, nBase, true);
    }
    AptTimelineRelocate(mpFrames, nBase, true);
    if (mpLabelHash)
    {
        mpLabelHash->DestroyGCPointers();
        AptNativeHash* hash = mpLabelHash;
        hash->~AptNativeHash();
        gpAptPseudoDataPool->Deallocate(hash, sizeof(AptNativeHash));
        mpLabelHash = 0;
    }
}
