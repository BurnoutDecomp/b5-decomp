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

#include <new>   // placement new (the pool-allocated AptPseudoCIH_t nodes)

int AptMovie::labelToFrame(const EAStringC* pLabel) const
{
    if (pLabel && mpLabelHash)
    {
        AptValue* pValue = mpLabelHash->Lookup(*pLabel);
        if (pValue)
            return AptValue_toInteger(pValue);
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
// FLAG (un-homed globals / callees owned by the Apt VM + display-list boot TUs):
// declared extern so this TU links. Names reused from the sibling Apt TUs that
// already home them; the single underlying object is shared.
// ---------------------------------------------------------------------------

// off_8324D808 -- the shared Apt DOGMA pool the 20-byte pseudo nodes come from
// (AptPseudoCIH.h declares it as gpAptPseudoDataPool; reused by name).

// off_8324E574 -- the current AS animation target: gpAptTarget (typed AptTarget*,
// declared by AptTarget.h; its director @+0x18 owns the AptActionQueueC* @+0x0C --
// both real members now, read through the types).

// dword_8324E514 -- the current frame's queued-action sequence id passed as
// AddActionBack's 4th arg.
extern int   gnAptActionFrameId;                // dword_8324E514 (FLAG)

// &dword_8324E760 -- the process-wide AS action interpreter instance.
extern AptActionInterpreter gAptActionInterpreter;   // off_8324E760  (FLAG)

// dword_8324D807 -- one-shot latch: a "back-to-script" (tag 5) command fires its
// host callback at most once per frame-control pass.
extern unsigned char gbAptBackToScriptFired;    // byte_8324D807  (FLAG)

// dword_8324E828 -- the host "back-to-script" callback (installed by the host; a
// raw function pointer invoked through the ctr in the asm).
extern void (*gpAptBackToScriptHook)(void* pPayload);   // dword_8324E828 (FLAG)

// The AS register-window globals (off_8324E3D0 / dword_8324E3D4) are
// AptScriptFunctionBase::spRegBlockCurrentFrameBase / snRegBlockCurrentFrameCount;
// save/restore around runStream is the public PushStaticData()/PopStaticData() pair
// (the duplicate gpAptRegisterBase/gnAptRegisterCount externs -- the latter also
// colliding with AptRegister.cpp's config global of the same name -- are retired).

// FLAG (deferred VM / display-list callees -- owned by other TUs, declared with the
// raw-but-faithful call-site signatures so the timeline driver links):
struct AptCIH;
extern AptCIH* AptGetAnimationAtLevel(int nLevel);                       // _AptGetAnimationAtLevel @0x82B00788 (canonical AptCIH*; void* blob walk binds)
extern void* AptPseudoDisplayList_FindInst(void* pList, void* pSource,   // AptPseudoDisplayList::FindInst
                                           unsigned char* pOutHit, void** ppExisting,
                                           void* pContext, void* pInfo);
// AptPseudoDisplayList_Insert retired: the real member AptPseudoDisplayList::Insert is called directly.
extern void  AptCharacterAnimation_ExecuteInitActions(void* pAnim, void* pCIH, int nId);   // AptCharacterAnimation::ExecuteInitActions (FLAG deferred; see AptRenderLinkStubs.cpp)
extern void* AptFile_operator(void* pDst, void* pSrc);                   // AptFile::operator=

// sub_82B0AE08 @0x82B097D8's caller-side dispatcher (the place-command handler doFrameControls
// invokes for each tag-3 record). HOMED below (2026-07-01) as AptMovie_PlaceCommand -- it reads
// the serialised place-info record + calls the faithfully-homed AptDisplayList::placeObjectNCXForm.
static AptCIH* AptMovie_PlaceCommand(AptDisplayList* pDisplayList, const void* pPlaceInfo, AptCIH* pParent);

// sub_82AFD150 @0x82AFD150 -- the remove-command dispatcher (doFrameControls' tag-4 twin of
// AptMovie_PlaceCommand). HOMED (2026-07-04; was an AptRenderLinkStubs no-op, which left every
// timeline-removed element on screen -- the title menu items stacked their whole state band).
// X360 body (two calls, both already homed):
//   AptDisplayListState::findInst(*a1, a2, 0, &prev, &match);   // r3=*list (the state), r4=depth
//   AptDisplayList::removeObject(a1, match);
static void AptMovie_RemoveCommand(AptDisplayList* pDisplayList, int nDepth)
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
// DoTemporaryFrameControls @ 0x82AEEB98 -- build/refresh the interpreter's pseudo
// display list for frame nFrame: walk that frame's commands, and for each place
// (tag 3) / remove (tag 4) command, insert or merge an AptPseudoCIH_t pseudo node
// into the pseudo display list pPseudoList (an action/label command at tag 3 ends
// the walk for that record). Used by AptCIH::jumpToFrame for the "skip" path.
//
// `this` is the AptMovie* (mpFrames @[1]); a2 = the AptPseudoDisplayList*, nFrame
// the frame index, a4 unused here, a5 the place/remove info context. Faithful to
// the asm: the frame record stride is 8 (an AptMovieFrame), the command pointers
// are read through mpCommands; the pseudo nodes are 20-byte DOGMA allocations.
// ===========================================================================
AptMovie* AptMovie::DoTemporaryFrameControls(AptPseudoDisplayList* pPseudoList, int nFrame, int /*a4*/, void* a5)
{
    if (nFrame < 0)
        return this;

    AptMovieFrame* pFrame = &mpFrames[nFrame];        // 8 * nFrame + this[1]
    void* pListOwner = pPseudoList->mpOwner;           // *(a2 + 4)

    int32_t nCount = pFrame->mnCommandCount;           // *v11
    if (nCount <= 0)
        return this;

    // NATIVE-8 PORT (2026-07-02; was the console 4-byte transliteration whose
    // straddled reads AV'd -- the reason jumpToFrame's replay was boundary-skipped).
    // The place-record body offsets are the SAME native-8 map PlaceCommand /
    // resolve64 case-3 use: body = align8(cmd+4); {flags@0, depth@+4, charId@+8,
    // matrix@+0x0C, colour@+0x24, ratio@+0x2C, name@+0x30, clipDepth@+0x38,
    // clipActions@+0x40}. The console shadow offsets are kept in comments.
    for (int32_t i = 0; i < nCount; ++i)
    {
        void* pCmd = pFrame->mpCommands[i];            // *(v11[1] + v12)
        // The same converter-malformed command-pointer guard as doFrameControls.
        {
            const uintptr_t luCmd = reinterpret_cast<uintptr_t>(pCmd);
            if (luCmd < 0x10000u || (luCmd >> 47) != 0u || (luCmd & 0xFFFFFFFFull) == 0u)
                continue;
        }
        int32_t eTag = CmdI32(pCmd, 0x00);             // *v13

        if (eTag == 3)
        {
            // ---- place command --------------------------------------------
            const uintptr_t luBody =
                (reinterpret_cast<uintptr_t>(pCmd) + 4u + 7u) & ~static_cast<uintptr_t>(7u);
            char* const pBody = reinterpret_cast<char*>(luBody);

            const int32_t nFlags = *reinterpret_cast<const int32_t*>(pBody + 0x00);
            const int32_t nDepth = *reinterpret_cast<const int32_t*>(pBody + 0x04);   // [c: cmd+0x08]

            unsigned char chMode = 0;                  // var_60 (BYREF)
            void* pExisting = nullptr;                  // the existing-node out
            // FindInst keys by the placement depth (the console passes the depth word).
            AptPseudoDisplayList_FindInst(pPseudoList,
                                          reinterpret_cast<void*>(static_cast<intptr_t>(nDepth)),
                                          &chMode, &pExisting, a5, pBody);

            int32_t nResolvedId = *reinterpret_cast<const int32_t*>(pBody + 0x08);   // [c: cmd+0x0C]
            void* pCharacter = nullptr;
            if (nResolvedId != -1)
            {
                // pCharacter = the owning movie's charTable[nResolvedId] -- the same
                // native-8 owner chain PlaceCommand uses (owner char -> mpFixupLink ->
                // embedded anim @+0x20), with its sane-table guard.
                AptCIH* pOwnerCIH = static_cast<AptCIH*>(pListOwner);
                AptCharacterInst* pOwnerInst =
                    (pOwnerCIH != nullptr) ? pOwnerCIH->mpCharacterInst : nullptr;
                AptCharacter* pOwnerChar =
                    (pOwnerInst != nullptr && pOwnerInst->mpRenderItem != nullptr)
                        ? pOwnerInst->mpRenderItem->mpCharacter : nullptr;
                AptCharacter* pRootChar =
                    (pOwnerChar != nullptr) ? pOwnerChar->mpFixupLink : nullptr;
                if (pRootChar != nullptr)
                {
                    AptCharacterAnimation* pAnim = reinterpret_cast<AptCharacterAnimation*>(
                        reinterpret_cast<char*>(pRootChar) + KU_AptEmbeddedMovieOff);
                    const uintptr_t luTablePtr =
                        reinterpret_cast<uintptr_t>(pAnim->mpCharacterTable);
                    const bool bSane =
                        (pAnim->mnCharacterCount > 0 && pAnim->mnCharacterCount <= 0x10000) &&
                        (luTablePtr >= 0x10000u) && ((luTablePtr >> 47) == 0u);
                    if (bSane && nResolvedId >= 0 && nResolvedId < pAnim->mnCharacterCount)
                        pCharacter = pAnim->mpCharacterTable[nResolvedId];
                }
            }

            if (pExisting && nResolvedId == -1)
            {
                // ---- merge the place fields onto the existing node ---------
                // XB1-VERIFIED (sub_1408363A0): the merge stores the ADDRESSES of this
                // record's matrix/colour fields (`lea rcx,[rbx+14h]`-family) into the
                // node's typed AptPseudoData_t snapshot -- NOT 4-byte field VALUES at
                // console offsets (the prior transliteration corrupted the x64 node on
                // any MOVE-record merge). clipActions (bit 0x80): the native-8 record
                // carries a POINTER @body+0x40 that the console's 4-byte slot cannot
                // hold and no pun consumer reads -- unchanged (see AptPseudoData.h FLAG).
                AptPseudoData_t* pData =
                    static_cast<AptPseudoCIH_t*>(pExisting)->mpPseudoData;
                if (pData != nullptr)
                {
                    if (nFlags & 0x04)
                        pData->mpMatrix = const_cast<char*>(pBody) + 0x0C;
                    if (nFlags & 0x08)
                        pData->mpColorTransform = const_cast<char*>(pBody) + 0x24;
                    if (nFlags & 0x10)
                        pData->mfRatio = *reinterpret_cast<const float*>(pBody + 0x2C);
                    pData->muxFlags |= static_cast<u32>(nFlags);
                }
                continue;
            }

            // FLAG (import not loaded -- the same deferred boundary as the live place path):
            // a non-MOVE place whose charTable slot is null is an UNRESOLVED IMPORT (real
            // native-8 bundles keep imports as genuine import-table entries). Feeding a
            // null-character node into the replay would hand mergeState's AddToDisplayList
            // a null character to instantiate (AV -- reproduced on the drive bundles' menu
            // state jumps). Skip it, exactly as AptMovie_PlaceCommand skips the fresh place.
            if (nResolvedId != -1 && pCharacter == nullptr)
                continue;

            // ---- allocate a fresh pseudo node ------------------------------
            void* pBlock = gpAptPseudoDataPool->Allocate(sizeof(AptPseudoCIH_t));   // [c: 20]
            AptPseudoCIH_t* pNode = nullptr;
            if (pBlock)
            {
                pNode = new (pBlock) AptPseudoCIH_t(
                            reinterpret_cast<AptCharacterInfo_t*>(pCmd),
                            static_cast<short>(nFrame),               // a3 (r5) = nFrame -> li16CharacterId
                            reinterpret_cast<void*>(static_cast<intptr_t>(nDepth)),  // a4 (r6) = the depth [c: cmd+0x08]
                            pCharacter);                               // v17
            }
            pPseudoList->Insert(pNode);   // real member
        }
        else if (eTag == 4)
        {
            // ---- remove command: allocate a node carrying the removed id ---
            void* pBlock = gpAptPseudoDataPool->Allocate(sizeof(AptPseudoCIH_t));   // [c: 20]
            AptPseudoCIH_t* pNode = nullptr;
            if (pBlock)
            {
                pNode = new (pBlock) AptPseudoCIH_t(
                            reinterpret_cast<AptCharacterInfo_t*>(pCmd),
                            static_cast<short>(nFrame),                // a3 (r5) = nFrame -> li16CharacterId
                            reinterpret_cast<void*>(static_cast<intptr_t>(CmdPayloadI32(pCmd))),  // a4 (r6) = the removed DEPTH (dual-format; native-8 @cmd+8)
                            nullptr);                                   // r7 = 0
            }
            pPseudoList->Insert(pNode);   // real member
        }
        // any other tag (incl. the tag==3 break-without-place asm fallthrough):
        // continue to the next command.
    }
    return this;
}

// ===========================================================================
// doFrameControls @ 0x82B0B7A0 -- run a frame's commands against a live CIH: first
// pass runs every action (tag 8 with a non-negative id) through the AS interpreter
// (saving/restoring the register frame), second pass applies the place (tag 3) /
// remove (tag 4) / back-to-script (tag 5) commands to the display list.
//
// `this` is the AptMovie*; pParent the place-object target CIH (a3); pInst the
// owning sprite CIH (a4 unused for the asm's r5 path here -> the char inst);
// nFrame the frame index. Faithful to the asm.
// ===========================================================================
AptMovie* AptMovie::doFrameControls(AptDisplayList* pDisplayList, AptCIH* pParent, int nFrame)
{
    AptMovieFrame* pFrame = &mpFrames[nFrame];         // 16*nFrame + mpFrames (native-8, relocated)

    // FLAG (converter-malformed frame table -- honest data boundary, NOT invention): a well-formed
    // native-8 frame with mnCommandCount > 0 always has a non-null mpCommands (resolve64 relocated
    // its offset -> pointer). A few NESTED sprite movies in the current apt_convert-produced bundle
    // (verified: char[15]/[23]/[36] of TITLE_SCREEN02) have a MALFORMED frame table -- the command-
    // array pointer was written at frame+0x04 (4-byte) instead of the native-8 frame+0x08, so their
    // frame+0x08 slot reads 0 and resolve64 (correctly reading the native-8 slot) leaves mpCommands
    // null. Dereferencing mpCommands[i] on such a frame is a NULL read (confirmed via crash dump:
    // doFrameControls+0xF9, `mov rax,[rcx+rax*8]` with rcx=mpCommands=0). This is a CONVERTER data
    // bug (the same 4-byte pointer-widening misalignment class as the char[1] fix), not a resolve64
    // gap -- resolve64 is XB1-verified. Skip the frame: the 16 well-formed nested movies still tick
    // + place their content; only the 3 malformed movies are skipped (their sub-content stays
    // unplaced). The real fix is in the offline bundle byte-patcher (out of this slice's scope).
    // The malformation also appears as a NON-NULL but INVALID command-array pointer: a few DEEP nested
    // movies in the current bundle have frame+0x08 holding garbage (observed 0x0000000100000000 -- a
    // 4-byte value straddled into the high half of the native-8 slot, the same 4->8 widening class),
    // which resolve64 leaves as a bogus pointer. Dereferencing mpCommands[i] then AVs (crash dump:
    // doFrameControls, `mov rax,[mpCommands+i*8]` with mpCommands=0x100000000). Guard BOTH null and a
    // non-plausible pointer (below the heap floor, or with any bit above bit-47 set -- no user-mode x64
    // heap/resource pointer looks like that). Skip such a frame (same deferred converter-data boundary).
    {
        // A well-formed relocated command pointer = (resource/heap base) + offset. On Win64 the .apt's
        // resource span + the game heap sit in the terabyte range (observed bases 0x00007FF6xxxxxxxx and
        // 0x000002xx_xxxxxxxx), so a valid mpCommands is ALWAYS well above 4 GB. The straddle garbage is
        // exactly 0x0000000100000000 (a 4-byte '1' landing in the high dword, low dword 0) -- it passes a
        // naive "in canonical range" test, so discriminate on the terabyte floor + a non-zero low dword.
        const uintptr_t luCmds = reinterpret_cast<uintptr_t>(pFrame->mpCommands);
        const bool bCmdsPlausible =
            (luCmds > 0x0000000200000000ull) && ((luCmds >> 47) == 0u) &&
            ((luCmds & 0xFFFFFFFFull) != 0u);
        if (pFrame->mnCommandCount > 0 && !bCmdsPlausible)
            return this;
    }

    // ---- pass 1: run the frame's init-action streams (tag 8, id >= 0) -----
    // UN-DEFERRED 2026-07-05 (runStream has been the live dispatch loop since 07-01;
    // the old "runStream is STUBBED" note was stale). Faithful to X360 @0x82B0B7A0
    // pass 1: for each tag-8 command whose character id is non-negative --
    //   * save the static register frame (off_8324E3D0/dword_8324E3D4 ==
    //     AptScriptFunctionBase::PushStaticData);
    //   * resolve the run instance: a CIHNone scope resolves to the level-0 root,
    //     else walk the display-list parents to the enclosing anim(9)/custom-
    //     control(15) node (the same walk ExecuteScriptFunction performs);
    //   * execute the stream TOP-LEVEL through runStream(stream, pParent, -1, inst);
    //   * NEGATE the id (the console's run-once latch: `v11[1] = -v11[1]`);
    //   * restore the register frame (CleanupAfterExecution == PopStaticData).
    // The title timelines carry no tag-8 commands; MAIN.bundle's frame 0 carries 88
    // of them (one class-definition init stream per exported framework sprite) --
    // this pass IS the AS-framework bootstrap. Record shape (native-8, XB1-verified
    // resolve): {tag@0, id@+0x08 (i64), stream ptr @+0x10 (relocated by resolve64)}.
    // FLAG (bring-up gate, 2026-07-05): the pass EXECUTES faithfully (MAIN's class-
    // definition streams run: DefineFunction2 -> SetMember chains reach deep into the
    // VM), but a script-function LIFETIME defect kills the boot mid-bootstrap
    // (cdb: SetMember -> stackPop -> Release-to-zero -> ForceDelete ->
    // AptScriptFunctionBase::DestroyGCPointers AV -- the stored function should have
    // survived the pop; SetMember/DefineFunction2 refcount semantics vs the X360 are
    // the open follow-on). Gated OFF so the title boot stays green; flip to continue.
    static const bool KB_RUN_INITACTIONS = true;
    if (KB_RUN_INITACTIONS)
    {
        const int32_t nCount1 = pFrame->mnCommandCount;
        for (int32_t i = 0; i < nCount1; ++i)
        {
            unsigned char* const pCmd = static_cast<unsigned char*>(pFrame->mpCommands[i]);
            if (CmdI32(pCmd, 0x00) != 8)
                continue;
            int64_t* const pnId = reinterpret_cast<int64_t*>(pCmd + 0x08);
            if (*pnId < 0)
                continue;   // already run (the negated-id latch)

            const unsigned char* const pStream =
                *reinterpret_cast<const unsigned char* const*>(pCmd + 0x10);
            // Same relocated-pointer plausibility screen as the command array above
            // (a malformed/unrelocated slot must not be executed).
            const uintptr_t luStream = reinterpret_cast<uintptr_t>(pStream);
            if (!(luStream > 0x0000000200000000ull && (luStream >> 47) == 0u
                  && (luStream & 0xFFFFFFFFull) != 0u))
                continue;

            AptValue** const lppSavedRegs = AptScriptFunctionBase::PushStaticData();

            // Resolve the run instance (console: CIHNone -> level 0; else parent walk).
            AptCharacterInst* pRunInst = nullptr;
            if (pParent != nullptr)
            {
                AptCIH* pNode = pParent;
                if (pNode->getVtblIndex() == AptVFT_CIHNone)
                    pNode = AptGetAnimationAtLevel(0);
                else
                {
                    while (pNode != nullptr)
                    {
                        AptCharacterInst* const pCI = pNode->GetCharacterInst();
                        if (pCI == nullptr)
                            break;
                        const uint32_t nT = pCI->GetTypeTag();
                        if (nT == 9u || nT == 0xFu)
                            break;
                        pNode = pNode->GetDisplayListParent();
                    }
                }
                pRunInst = pNode ? pNode->GetCharacterInst() : nullptr;
            }

            gAptActionInterpreter.runStream(pStream, pParent, -1, pRunInst);
            *pnId = -*pnId;   // run-once latch (console `v11[1] = -v11[1]`)

            AptScriptFunctionBase::PopStaticData(lppSavedRegs);
        }
    }

    // ---- pass 2: apply place / remove / back-to-script (native-8) ---------
    // The movie's character-animation def-base is reached through the parent CIH's named
    // char-inst chain: parent->charInst->renderItem->mpCharacter is the owning movie
    // character, and its embedded AptCharacterAnimation is at (character + KU_AptEmbedded
    // MovieOff == +0x20). charTable/importTable are read as named members off that def-base.
    int32_t nCount = pFrame->mnCommandCount;
    if (nCount > 0)
    {
        AptCharacterInst* const pOwnerInst = pParent->GetCharacterInst();
        AptCharacter*     const pOwnerChar = pOwnerInst->GetRenderItem()->mpCharacter;
        // The placed charIds resolve against the ROOT movie's SHARED charTable, reached through the
        // owning char's mpFixupLink back-link (char+0x08, which Fixup writes = charTable[0], the root
        // char) -- NOT the nested sprite's own +0x20 timeline, which carries no charTable. This is the
        // console's 4th deref: X360 doFrameControls @0x82B0B7A0 does `*(mpCharacter+4)` (= mpFixupLink)
        // then `+0x10` (= embedded movie) -> charTable (asm: three `lwz r11,4(r11)` + `addi r11,0x10`).
        // native-8: mpFixupLink@0x08, embedded movie @+0x20. (No-op for the root char, whose
        // mpFixupLink points at itself; corrects every nested sprite, whose link points at the root.)
        AptCharacter* const pRootChar = pOwnerChar->mpFixupLink;
        AptCharacterAnimation* const pAnim = reinterpret_cast<AptCharacterAnimation*>(
            reinterpret_cast<char*>(pRootChar) + KU_AptEmbeddedMovieOff);   // rootChar + 0x20

        // FLAG (deferred DEEP-NESTING boundary -- honest data guard, NOT a logic change): the
        // mpFixupLink back-link reaches a root/import-root movie with a SHARED charTable for the
        // level-1 containers (verified: charCount 41 local / 7 import, a live charTable pointer). For
        // some DEEPER nested containers (a type-5 sprite placed INSIDE another container), the native-8
        // back-link does not reach a charTable-bearing root -- pAnim->mpCharacterTable reads garbage
        // (observed 0x100000000) and mnCharacterCount is absurd, so indexing charTable[charId] AVs.
        // Reconstructing the deep-nesting charTable resolution (the render-tree-manager's per-level
        // owner chain) is a follow-on; until then, skip a frame whose owner movie has no sane charTable
        // (the same offset-vs-pointer / plausibility discrimination the Apt bring-up uses for deferred
        // data, e.g. CgsAptAux gAptResourceSpanBase). The level-1 containers (valid charTable) still
        // place their nested content; only the deep containers' sub-content stays unplaced.
        {
            const int32_t   nSanityCount = pAnim->mnCharacterCount;
            const uintptr_t luTablePtr   = reinterpret_cast<uintptr_t>(pAnim->mpCharacterTable);
            const bool bSaneCharTable =
                (nSanityCount > 0 && nSanityCount <= 0x10000) &&
                (luTablePtr >= 0x10000u) && ((luTablePtr >> 47) == 0u);   // a real heap/resource ptr
            if (!bSaneCharTable)
                return this;   // deferred deep-nesting owner -- skip this frame's placements (safe)
        }

        for (int32_t i = 0; i < nCount; ++i)
        {
            void* pCmd = pFrame->mpCommands[i];
            const int32_t eTag = CmdI32(pCmd, 0x00);

            if (eTag == 3)
            {
                // The PlaceObject record body is pointer-aligned after the tag (native-8):
                // body = align8(cmd+4); the placed-char id lives at body+0x08.
                const uintptr_t luBody =
                    (reinterpret_cast<uintptr_t>(pCmd) + 4u + 7u) & ~static_cast<uintptr_t>(7u);
                const int32_t nId = *reinterpret_cast<const int32_t*>(luBody + 0x08);

                // ---- place: run the placed character's init actions -----------
                AptCharacterAnimation_ExecuteInitActions(pAnim, pParent, nId);   // FLAG deferred: the real member runs away at boot frame 3 (init-action VM path incomplete) -- see AptRenderLinkStubs.cpp

                // Bind the placed character's animation file (native-8 named members): a
                // placed char with no file yet takes the import-table entry matching its id,
                // else the owning movie's own file. (The console open-codes this; it mirrors
                // AptDisplayList::AddToDisplayList's import bind, which is already homed.)
                if (nId != -1 && nId >= 0 && nId < pAnim->mnCharacterCount)
                {
                    AptCharacter* const pPlacedChar = pAnim->mpCharacterTable[nId];
                    if (pPlacedChar != nullptr && pPlacedChar->mpAnimationFile == nullptr)
                    {
                        AptFilePtr* pSrc = reinterpret_cast<AptFilePtr*>(&pOwnerChar->mpAnimationFile);
                        const int32_t nImports = pAnim->mnImportCount;
                        for (int32_t k = 0; k < nImports; ++k)
                        {
                            if (pAnim->mpImportTable[k].mnId == nId)
                            {
                                pSrc = reinterpret_cast<AptFilePtr*>(&pAnim->mpImportTable[k].mpFile);
                                break;
                            }
                        }
                        // AptFilePtr assign (counted) into the placed char's file slot.
                        *reinterpret_cast<AptFilePtr*>(&pPlacedChar->mpAnimationFile) = *pSrc;
                    }
                }

                // Dispatch the place: the info record is at cmd+4 (the console r31 base).
                AptMovie_PlaceCommand(pDisplayList, reinterpret_cast<char*>(pCmd) + 4, pParent);
            }
            else if (eTag == 4)
            {
                // remove: drop the node at the command's depth (sub_82AFD150; XB1 inlines
                // the same walk). Native-8 payload @cmd+8 (XB1 asm `mov r8d,[rbx+8]`);
                // dual-format read while converter-era bundles are still staged.
                AptMovie_RemoveCommand(pDisplayList, CmdPayloadI32(pCmd));
            }
            else if (eTag == 5 && !gbAptBackToScriptFired)
            {
                // FLAG (host hook): gpAptBackToScriptHook (dword_8324E828) is the host
                // "back to script" callback. It is installed by the game's GUI/HUD host on
                // the console; our Apt bring-up does NOT install it (the FSM host callback
                // set is out of this slice's scope), so it is null here -- guard the call
                // (the console always has it installed). Skipping it is faithful for the
                // title timeline (the tag-5 command hands control back to the host Lua FSM,
                // which our bring-up drives separately).
                if (gpAptBackToScriptHook != nullptr)
                    gpAptBackToScriptHook(CmdPtr(pCmd, 0x08));   // native-8 payload @cmd+8
                gbAptBackToScriptFired = 1;
            }
        }
    }
    return this;
}

// ===========================================================================
// AptMovie_PlaceCommand -- sub_82B0AE08 @0x82B0AE08 (the place-command dispatcher the
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
static AptCIH* AptMovie_PlaceCommand(AptDisplayList* pDisplayList, const void* pPlaceInfo, AptCIH* pParent)
{
    // The record body is pointer-aligned after the tag (FrameItem::Write Align()); the caller
    // passes record+4, so align it up to the 8-byte pointer boundary to reach the body base.
    const uintptr_t luBody = (reinterpret_cast<uintptr_t>(pPlaceInfo) + 7u) & ~static_cast<uintptr_t>(7u);
    const char* const pBody = reinterpret_cast<const char*>(luBody);

    const uint32_t nFlags = *reinterpret_cast<const uint32_t*>(pBody + 0x00);

    // Place when the record carries a character (bit1 HasCharacter) or is a Move (bit0);
    // otherwise it is a no-op (the X360's `(flags&2)==0 && (flags&1)==0 -> 0`).
    const bool bHasCharacter = (nFlags & 0x02u) != 0u;
    const bool bMove         = (nFlags & 0x01u) != 0u;
    if (!bHasCharacter && !bMove)
        return nullptr;

    const int32_t nDepth  = *reinterpret_cast<const int32_t*>(pBody + 0x04);
    const int32_t nCharId = *reinterpret_cast<const int32_t*>(pBody + 0x08);

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
        const char* const pNamePtr = *reinterpret_cast<const char* const*>(pBody + 0x30);
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
    const double fFrameValue = static_cast<double>(*reinterpret_cast<const float*>(pBody + 0x2C));
    const int16_t nClipDepth = static_cast<int16_t>(*reinterpret_cast<const int32_t*>(pBody + 0x38));

    // The clipActions/handler-list block pointer: placed into the sprite inst's
    // mpClipEventHandlers slot (queueClipEvents scans it; _addToSetCaches folds its
    // masks). The block + each record's stream pointer are already relocated (and the
    // streams parsed) by resolve64's case-3 walk.
    const void* pClipActions = nullptr;
    if ((nFlags & 0x80u) != 0u)
        pClipActions = *reinterpret_cast<void* const*>(pBody + 0x40);

    // ---- MOVE (bit0 without bit1 HasCharacter): the TWEEN-KEYFRAME path ----------------
    // The X360 `(flags & 2) == 0` branch: findInst the EXISTING node at this depth and
    // re-place it (pExistingNode set, no character, no name, depth 0, clipDepth -1) so the
    // record's matrix/colour land on the live item -- these charId==-1 records are every
    // animation's tween keyframes (dropping them froze all the title transitions after
    // their discrete PLACE frames). Only a node whose mFlagsA bit31 is clear moves (the
    // console `*(v18+12) >= 0` pending-remove gate); a missing node falls through to the
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
            if (static_cast<int32_t>(pMatch->mFlagsA) >= 0)
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

// ===========================================================================
// queueFrameActions @ 0x82AE0228 -- for every action (tag 1) command of frame nFrame,
// enqueue its action-stream onto the current animation target's director action queue
// (`gpAptTarget->[+0x18]->[+0x0C]`, an AptActionQueueC), deferred until the AS VM drains
// the queue. pCIH (a2) is the bound target/this. Faithful to the asm; the enqueue itself
// is the DEFERRED AS-VM boundary (see the FLAG below).
// ===========================================================================
AptMovie* AptMovie::queueFrameActions(void* pCIH, int nFrame)
{
    AptMovieFrame* pFrame = &mpFrames[nFrame];         // *(this+4) + 8*nFrame

    const int32_t nCount = pFrame->mnCommandCount;
    if (nCount <= 0)
        return this;

    // FLAG (converter-malformed frame table -- same honest data boundary as doFrameControls): a few
    // nested sprite movies in the apt_convert-produced bundle have a MALFORMED command-array pointer
    // (null, OR non-null garbage like 0x100000000 -- the 4->8 widening straddle). Skip such a frame
    // (a counted frame with a non-plausible command pointer never occurs in a well-formed native-8
    // movie); guard both null and a non-plausible pointer, matching doFrameControls.
    {
        const uintptr_t luCmds = reinterpret_cast<uintptr_t>(pFrame->mpCommands);
        if (luCmds <= 0x0000000200000000ull || (luCmds >> 47) != 0u ||
            (luCmds & 0xFFFFFFFFull) == 0u)
            return this;
    }

    for (int32_t i = 0; i < nCount; ++i)
    {
        void* pCmd = pFrame->mpCommands[i];            // *(v7[1] + v8)  (relocated by resolve64)
        if (CmdI32(pCmd, 0x00) != 1)
            continue;                                  // only tag-1 ACTION commands are queued

        // ACTIVATED (2026-07-01, with the live VM): enqueue the action onto the director's
        // deferred queue -- the console's AddActionBack(cmd+4, pCIH, gnAptActionFrameId) with
        // the director chain typed (gpAptTarget->mpAnimationTarget @+0x18, ->mpActionQueue
        // @+0x0C -- both real members now). The queued "event id" is the ADDRESS of the
        // command's action-stream pointer slot: console cmd+0x04; native-8 cmd+0x08 (the slot
        // resolve64 relocated + _parseStream parsed). RunActions dereferences it at drain time.
        // FLAG (converter data boundary -- the same 4-byte-straddle family as the
        // malformed frame tables): a deep-nested movie's tag-1 record can carry a
        // never-relocated/shifted stream slot (observed *slot == 0x2_00000000).
        // Skip the enqueue for an implausible stream pointer -- the drain would
        // dereference it into a wild runStream PC.
        const uintptr_t luStream = *reinterpret_cast<const uintptr_t*>(
            static_cast<char*>(pCmd) + 0x08);
        const bool lbStreamSane =
            luStream >= 0x10000u && (luStream >> 47) == 0u &&
            (luStream & 0xFFFFFFFFull) != 0u;
        if (lbStreamSane
            && gpAptTarget != nullptr && gpAptTarget->mpAnimationTarget != nullptr
            && gpAptTarget->mpAnimationTarget->mpActionQueue != nullptr)
        {
            gpAptTarget->mpAnimationTarget->mpActionQueue->AddActionBack(
                static_cast<char*>(pCmd) + 0x08,
                static_cast<AptCIH*>(pCIH), gnAptActionFrameId);
        }
    }
    return this;
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
                            int32_t v41 = *reinterpret_cast<int32_t*>(pRec + 8);
                            int32_t v42 = v41 ? (v41 + nBase) : 0;
                            *reinterpret_cast<int32_t*>(pRec + 8) = v42;
                            result = const_cast<unsigned char*>(gAptActionInterpreter._parseStream(
                                         reinterpret_cast<const unsigned char*>(
                                             *reinterpret_cast<void**>(pRec + 8)),
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
// resolve64 -- the x64 native-8 fork of resolve @0x82AF80B0.
//
// The GUIAPT "Apt Data:1:7:8" bundle keeps the serialised timeline in the native
// 8-byte pointer format: every "pointer" slot is a full 8-byte word holding a
// file-relative OFFSET, so relocation is `slot = slot + base` in place (offset ->
// absolute pointer). This is what the console-32 resolve() cannot do on x64 (its
// `int nBase` truncates the high x64 base). VERIFIED vs TITLE_SCREEN02.bundle: the
// ROOT movie's framesOffset lives at movie+0x08 (== 0x5180 file-relative), the frame
// table stride is 16 (AptMovieFrame {mnCommandCount@0, mpCommands@8}) and each frame's
// command-pointer array is a stride-8 array of command-record offsets.
//
// The relocation walk is structurally identical to the console resolve() above (the
// same label-hash build + the same per-tag inner-pointer relocations), only widened
// to 8-byte slots / native strides. It now relocates EVERY per-frame command record's
// pointer slots at the native-8 offsets -- tag-1 Action (action-stream ptr @cmd+0x08),
// tag-2 FrameLabel (name ptr @cmd+0x08, registered into the label hash), tag-3
// PlaceObject (name @body+0x30 + clipActions block @body+0x40, and the clipActions
// record-array + per-record stream pointers), tag-8 Morph (stream ptr @cmd+0x08) --
// so that after resolve64 NO command record (root OR imported) holds an un-relocated
// file offset, and doFrameControls / queueFrameActions / placeObject read them safely.
//
// STATUS (ACTIVATED 2026-07-01): _parseStream (the homed XB1 sub_14084A920) is now
// CALLED on every action/clipActions/morph stream below -- serialized inline operands
// (dictionary strings, function records, branch targets) resolve into live pointers/
// values at movie-resolve time, and the runtime handlers read the parsed 8-aligned
// GUIAPT64 operand form. The VM chain is complete end-to-end.
// ===========================================================================
void AptMovie::resolve64(uintptr_t nBase, uintptr_t nResBase, uint32_t nResSize,
                         AptConstFile* pConstFile, int64_t* pnParsedValues)
{
    // Relocate a native-8 offset slot IN PLACE, but only when it holds a plausible
    // file-relative offset (0 < off < nResSize). A slot already carrying an absolute
    // pointer (>= nResBase) or garbage is left untouched -- so a re-entrant call, an
    // already-relocated field, or a non-pointer field (e.g. clipDepth == -1) never
    // produces a wild-pointer write. Returns the resulting 64-bit pointer (or 0).
    auto reloc64 = [nBase, nResSize](void* pRec, int nOff) -> uintptr_t
    {
        uintptr_t* pSlot = reinterpret_cast<uintptr_t*>(reinterpret_cast<char*>(pRec) + nOff);
        const uintptr_t luVal = *pSlot;
        if (luVal != 0 && luVal < nResSize)   // a live file-relative offset
        {
            *pSlot = nBase + luVal;
            return *pSlot;
        }
        return (luVal != 0 && luVal >= nBase) ? luVal : 0;   // already-abs (kept) else none
    };
    // True iff luPtr is a live pointer inside the resource (safe to dereference).
    auto inRes = [nResBase, nResSize](uintptr_t luPtr) -> bool
    {
        return luPtr >= nResBase && luPtr < nResBase + nResSize;
    };

    // The parse ctx + resolved-value counter (the XB1 resolve twin sub_1408567D0's
    // a3/a4, threaded from Fixup case-5/9): pConstFile = the "Apt constant file" chunk
    // (record table @ctx+0x28, relocated around Fixup by Resolve; string payloads rebase
    // ctx-relative), pnParsedValues = the root def's +0x50 accumulator (CompleteLoad
    // zeroes it). Layout verified vs the shipped bundle (libapt2 Const::Write: magic+
    // align8, movieOffset@+0x18, itemCount@+0x20, itemStart@+0x28, 16-byte records).
    void* const pParseCtx = pConstFile;
    int64_t lnFallbackCount = 0;
    int64_t* const pnCount = pnParsedValues ? pnParsedValues : &lnFallbackCount;

    // ---- allocate + init the label hash (5 dwords, tag 2) ------------------
    void* pHash = gpAptPseudoDataPool->Allocate(sizeof(AptPseudoCIH_t));   // [c: 20]
    if (pHash)
    {
        // XB1 (sub_1408567D0) zeroes the four 8-BYTE member slots of the 0x28-byte
        // AptNativeHash (mpTable/mp__Proto__/mpPrototype/mnEventHandlerMask @+0x08..
        // +0x20) then stamps the type word. The prior 4-byte zeroing left the upper
        // halves (and +0x18..) as pool garbage on x64.
        char* const pcHash = reinterpret_cast<char*>(pHash);
        *reinterpret_cast<uint64_t*>(pcHash + 0x08) = 0;
        *reinterpret_cast<uint64_t*>(pcHash + 0x10) = 0;
        *reinterpret_cast<uint64_t*>(pcHash + 0x18) = 0;
        *reinterpret_cast<uint64_t*>(pcHash + 0x20) = 0;
        CmdSetI32(pHash, 0x00, 2);
    }
    this->mpLabelHash = reinterpret_cast<AptNativeHash*>(pHash);

    // Relocate the frame-table base (mpFrames) IN PLACE: framesOffset -> pointer.
    const int32_t nFrames = this->mnFrameCount;
    const uintptr_t luFrames = reloc64(this, 0x08);   // mpFrames @movie+0x08
    if (nFrames <= 0 || luFrames == 0 || !inRes(luFrames))
        return;

    for (int32_t f = 0; f < nFrames; ++f)
    {
        AptMovieFrame* pFrame = &mpFrames[f];   // 16 * f + mpFrames (native-8 stride 16)

        // relocate the frame's command-pointer array (mpCommands @frame+0x08).
        const uintptr_t luCmds = reloc64(pFrame, 0x08);
        const int32_t nCmds = pFrame->mnCommandCount;
        if (luCmds == 0 || !inRes(luCmds) || nCmds <= 0)
            continue;

        for (int32_t i = 0; i < nCmds; ++i)
        {
            // relocate the command-pointer slot itself (8-byte offset -> pointer).
            const uintptr_t luCmd = reloc64(&pFrame->mpCommands[i], 0);
            if (luCmd == 0 || !inRes(luCmd))
                continue;
            void* pCmd = reinterpret_cast<void*>(luCmd);
            const int32_t eTag = CmdI32(pCmd, 0x00);

            // Relocate EVERY per-command inner POINTER slot -- so that after resolve64 NO command
            // record (root OR imported) holds an un-relocated file offset, and doFrameControls /
            // queueFrameActions / placeObject can read them without an AV. The relocation walk +
            // per-tag pointer SET is decompiled from the FAITHFUL native-64-bit build: the Xbox One
            // remaster's AptMovie unresolve twin sub_14085EE40 (@0x14085EE40, .ida-exports/
            // Burnout_External_Xbox_One.exe) -- the native-8 build that loads these SAME GUIAPT
            // bundles single-path. It walks frames (stride 16), commands (stride 8), and switches on
            // the tag with these inner-pointer relocations (XB1 offsets shown as [xb1:0xNN]):
            //   case 1 ACTION : stream ptr [xb1:cmd+0x08] + _parseStream(stream)
            //   case 2 LABEL  : name ptr   [xb1:cmd+0x08]
            //   case 3 PLACE  : clipActions block ptr [xb1:cmd+0x48]; walk block {count@+0x00;
            //                   recArrayPtr@+0x08}, records STRIDE 16 with stream ptr @rec+0x08 +
            //                   _parseStream(each); then name ptr [xb1:cmd+0x38]
            //   case 8 MORPH  : stream ptr [xb1:cmd+0x10] + _parseStream(stream); un-negate id@+0x08
            //
            // BUNDLE PROVENANCE (2026-07-04): the staged GUIAPT bundles are now the REAL
            // native-8 set (JeBobs' GUIAPT64 drive drop), which matches the XB1 layout
            // EXACTLY: place body = cmd+8 (align8(cmd+4) lands there), name @body+0x30
            // (cmd+0x38), clipActions block @body+0x40 (cmd+0x48), block recArrayPtr
            // @block+0x08. The apt_convert-era "-4" deltas are retired with the converter.
            //
            // FLAG (deferred _parseStream): XB1 calls sub_14084A920 (its _parseStream) on each action
            // stream; here the stream POINTERS are relocated (reads never AV) but the BYTECODE stays
            // un-parsed (raw serialized operands). The VM is live (2026-07-01) -- _parseStream is the
            // remaining gate; see the resolve64 header FLAG above.
            switch (eTag)
            {
                case 1:   // ACTION: relocate the action-stream pointer (@cmd+0x08), then
                {         // PARSE it (the XB1 sub_14084A920 call -- resolve the stream's
                          // serialized inline operands into live pointers/values).
                    const uintptr_t luStream = reloc64(pCmd, 0x08);
                    if (luStream != 0 && inRes(luStream))
                        AptActionInterpreter::_parseStream(
                            reinterpret_cast<unsigned char*>(luStream), nBase,
                            pParseCtx, pnCount);
                    break;
                }

                case 2:   // LABEL: relocate the name pointer (@cmd+0x08) + register name -> frame f.
                {
                    const uintptr_t luName = reloc64(pCmd, 0x08);
                    if (luName != 0 && inRes(luName) && this->mpLabelHash != nullptr)
                    {
                        // Console/XB1: InitFromBuffer(&scratch, name); Set(hash, scratch, AptInteger::Create(f)).
                        // Structural (the label hash the labelToFrame lookup reads), NOT VM.
                        EAStringC label(reinterpret_cast<const char*>(luName));
                        AptInteger* pIdx = AptInteger::Create(f);
                        this->mpLabelHash->Set(label, pIdx);
                    }
                    break;
                }

                case 3:   // PLACE: relocate the instance-NAME (@body+0x30) + the clipActions block (@body+0x40).
                {
                    const uintptr_t luBody =
                        (reinterpret_cast<uintptr_t>(pCmd) + 4u + 7u) & ~static_cast<uintptr_t>(7u);
                    void* const pBody = reinterpret_cast<void*>(luBody);
                    reloc64(pBody, 0x30);                          // place record instance-name
                    const uintptr_t luClip = reloc64(pBody, 0x40); // place record clipActions/init-action block

                    // Walk the clipActions block's records (XB1 case-3 inner loop): the block is
                    // {count@+0x00 (i32); recArrayPtr@block+0x08 (XB1/native-8)}; each
                    // record is STRIDE 16 (XB1 v17+=16) with its action-stream pointer at rec+0x08 (an
                    // 8-byte native-8 slot). Relocate the record-array pointer + each record's stream
                    // pointer so a clipActions read never AVs; the stream CONTENTS stay un-parsed
                    // (VM deferred) -- XB1's sub_14084A920 (_parseStream) is NOT called.
                    if (luClip != 0 && inRes(luClip))
                    {
                        void* const pClip = reinterpret_cast<void*>(luClip);
                        const int32_t nRecs = CmdI32(pClip, 0x00);
                        // record-array ptr @block+0x08 on XB1/native-8 (+4 = pad 0);
                        // @block+0x04 (nonzero) on the converter-era bundles.
                        const uintptr_t luRecs = (CmdI32(pClip, 0x04) != 0)
                            ? reloc64(pClip, 0x04) : reloc64(pClip, 0x08);
                        if (luRecs != 0 && inRes(luRecs) && nRecs > 0 && nRecs <= 0x1000)
                        {
                            for (int32_t k = 0; k < nRecs; ++k)
                            {
                                // rec+0x08 == the action-stream pointer (native-8 8-byte slot; STRIDE 16).
                                const uintptr_t luRec = luRecs + static_cast<uintptr_t>(16) * k;
                                if (!inRes(luRec) || (luRec + 16) > (nResBase + nResSize))
                                    continue;
                                const uintptr_t luClipStream =
                                    reloc64(reinterpret_cast<void*>(luRec), 0x08);   // stream ptr (bounds-guarded)
                                if (luClipStream != 0 && inRes(luClipStream))
                                    AptActionInterpreter::_parseStream(
                                        reinterpret_cast<unsigned char*>(luClipStream), nBase,
                                        pParseCtx, pnCount);
                            }
                        }
                    }
                    break;
                }

                case 8:   // MORPH: relocate the action-stream pointer (@cmd+0x10 per XB1) + parse it.
                {
                    const uintptr_t luStream = reloc64(pCmd, 0x10);
                    if (luStream != 0 && inRes(luStream))
                        AptActionInterpreter::_parseStream(
                            reinterpret_cast<unsigned char*>(luStream), nBase,
                            pParseCtx, pnCount);
                    break;
                }

                default:  // tag 4 REMOVE / tag 5 BACK-TO-SCRIPT carry no relocatable inner pointer.
                    break;
            }
        }
    }
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
                                reinterpret_cast<const unsigned char*>(*reinterpret_cast<void**>(pRec + 8)),
                                nBase, nullptr, a3);
                            int32_t v22 = *reinterpret_cast<int32_t*>(pRec + 8);
                            int32_t v23 = v22 ? (v22 - nBase) : 0;
                            *reinterpret_cast<int32_t*>(pRec + 8) = v23;
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
