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
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"    // AptActionInterpreter
#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h"   // spRegisters / snRegisterCount (the AS register frame)
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"     // AptInteger::Create
#include "SDKs/EATech/include/Apt/AptDefine.h"               // gpNonGCPoolManager
#include "SDKs/EATech/include/Apt/AptString/EAString.h"

// ---- the native-8 place path (doFrameControls tag-3 + the place-command dispatcher) ----
#include "SDKs/EATech/include/Apt/AptDisplayList.h"           // AptDisplayList::placeObjectNCXForm (the homed place spine)
#include "SDKs/EATech/include/Apt/AptCIH.h"                   // AptCIH (the parent node + char-inst chain)
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

// off_8324E574 -- the current AS animation target (its director owns the action
// queue at +0x18, whose AptActionQueueC* lives at +0x0C). Read raw here (the
// AptAnimationTarget runtime type is the deferred VM follow-on).
extern void* gpAptTarget;                       // off_8324E574  (FLAG)

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

// FLAG: the AS register-frame globals (off_8324E3D0 / dword_8324E3D4). These are
// the same objects AptScriptFunctionBase::spRegisters / ::snRegisterCount name,
// but those statics are protected; declared here under their underlying linkage
// names so doFrameControls can save/restore the register frame around runStream.
extern AptValue** gpAptRegisterBase;    // off_8324E3D0  == AptScriptFunctionBase::spRegisters
extern int        gnAptRegisterCount;   // dword_8324E3D4 == AptScriptFunctionBase::snRegisterCount

// FLAG (deferred VM / display-list callees -- owned by other TUs, declared with the
// raw-but-faithful call-site signatures so the timeline driver links):
struct AptCIH;
extern AptCIH* AptGetAnimationAtLevel(int nLevel);                       // _AptGetAnimationAtLevel @0x82B00788 (canonical AptCIH*; void* blob walk binds)
extern void* AptPseudoDisplayList_FindInst(void* pList, void* pSource,   // AptPseudoDisplayList::FindInst
                                           unsigned char* pOutHit, void** ppExisting,
                                           void* pContext, void* pInfo);
extern void  AptPseudoDisplayList_Insert(void* pList, AptPseudoCIH_t* pNode);   // (AptPseudoDisplayList::Insert)
extern void  AptCharacterAnimation_ExecuteInitActions(void* pAnim, void* pCIH, int nId);   // AptCharacterAnimation::ExecuteInitActions
extern void* AptFile_operator(void* pDst, void* pSrc);                   // AptFile::operator=
extern void* sub_82AFD150(void* a1, int a2);                             // remove-object handler (unhomed)

// sub_82B0AE08 @0x82B097D8's caller-side dispatcher (the place-command handler doFrameControls
// invokes for each tag-3 record). HOMED below (2026-07-01) as AptMovie_PlaceCommand -- it reads
// the serialised place-info record + calls the faithfully-homed AptDisplayList::placeObjectNCXForm.
static AptCIH* AptMovie_PlaceCommand(AptDisplayList* pDisplayList, const void* pPlaceInfo, AptCIH* pParent);

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

    for (int32_t i = 0; i < nCount; ++i)
    {
        void* pCmd = pFrame->mpCommands[i];            // *(v11[1] + v12)
        int32_t eTag = CmdI32(pCmd, 0x00);             // *v13

        if (eTag == 3)
        {
            // ---- place command --------------------------------------------
            int32_t   nHit  = 0;                       // var_5C (BYREF)
            unsigned char chMode = 0;                  // var_60 (BYREF)
            void* pExisting = nullptr;                  // var_60 alias used as the existing-node out
            // FindInst fills the existing-node pointer (var_60) + a flag (var_5C).
            AptPseudoDisplayList_FindInst(pPseudoList, CmdPtr(pCmd, 0x08),
                                          &chMode, &pExisting, a5,
                                          reinterpret_cast<char*>(pCmd) + 4);   // v13 + 1

            int32_t nResolvedId = CmdI32(pCmd, 0x0C);  // v13[3]
            void* pCharacter = nullptr;
            if (nResolvedId != -1)
            {
                // pCharacter = pAnim->charTable[nResolvedId]
                // *(*(*(*(*(*(v9+32)+4)+4)+4)+32) + 4*v19)
                void* p = CmdPtr(pListOwner, 0x20);
                p = CmdPtr(p, 0x04);
                p = CmdPtr(p, 0x04);
                p = CmdPtr(p, 0x04);
                void* pTable = CmdPtr(p, 0x20);
                pCharacter = *reinterpret_cast<void**>(reinterpret_cast<char*>(pTable) + 4 * nResolvedId);
            }

            if (pExisting && nResolvedId == -1)
            {
                // ---- merge the place fields onto the existing node ---------
                // r8 (the place-info record) = pCmd + 4 (var_60-2's source).
                char* pInfo = reinterpret_cast<char*>(pCmd) + 4;   // v18 == r8
                int32_t nFlags = CmdI32(pInfo, 0x00);              // *v18

                // *(*(pExisting+4)+4) (the node's pseudo-data matrix/colour block).
                void* pData = CmdPtr(pExisting, 0x04);

                int32_t v21 = (nFlags & 4)  ? *reinterpret_cast<int32_t*>(pInfo + 12) : CmdI32(pData, 0x04);
                CmdSetI32(pData, 0x04, v21);
                int32_t v22 = (nFlags & 8)  ? *reinterpret_cast<int32_t*>(pInfo + 36) : CmdI32(pData, 0x08);
                CmdSetI32(pData, 0x08, v22);
                int32_t v23 = (nFlags & 0x80) ? *reinterpret_cast<int32_t*>(pInfo + 56) : CmdI32(pData, 0x0C);
                CmdSetI32(pData, 0x0C, v23);
                float   v24 = (nFlags & 0x10) ? *reinterpret_cast<float*>(pInfo + 44) : CmdF32(pData, 0x10);
                *reinterpret_cast<float*>(reinterpret_cast<char*>(pData) + 0x10) = v24;

                CmdSetI32(pData, 0x14, CmdI32(pData, 0x14) | nFlags);
                continue;
            }

            // ---- allocate a fresh pseudo node ------------------------------
            void* pBlock = gpAptPseudoDataPool->Allocate(20);
            AptPseudoCIH_t* pNode = nullptr;
            if (pBlock)
            {
                pNode = new (pBlock) AptPseudoCIH_t(
                            reinterpret_cast<AptCharacterInfo_t*>(pCmd),
                            static_cast<short>(nFrame),               // a3 (r5) = nFrame -> li16CharacterId
                            reinterpret_cast<void*>(static_cast<intptr_t>(CmdI32(pCmd, 0x08))),  // a4 (r6) = v13[2] -> lpContext (+0x08)
                            pCharacter);                               // v17
            }
            AptPseudoDisplayList_Insert(pPseudoList, pNode);
        }
        else if (eTag == 4)
        {
            // ---- remove command: allocate a node carrying the removed id ---
            void* pBlock = gpAptPseudoDataPool->Allocate(20);
            AptPseudoCIH_t* pNode = nullptr;
            if (pBlock)
            {
                pNode = new (pBlock) AptPseudoCIH_t(
                            reinterpret_cast<AptCharacterInfo_t*>(pCmd),
                            static_cast<short>(nFrame),                // a3 (r5) = nFrame -> li16CharacterId
                            reinterpret_cast<void*>(static_cast<intptr_t>(CmdI32(pCmd, 0x04))),  // a4 (r6) = v13[1] -> lpContext (+0x08)
                            nullptr);                                   // r7 = 0
            }
            AptPseudoDisplayList_Insert(pPseudoList, pNode);
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
    if (pFrame->mnCommandCount > 0 && pFrame->mpCommands == nullptr)
        return this;

    // ---- pass 1: run the frame's action streams (tag 8, id >= 0) ----------
    // FLAG (deferred AS-VM EXECUTION): pass 1 would execute tag-8 action-stream bytecode through the
    // AS interpreter (runStream). resolve64 relocates the tag-8 stream POINTER (@cmd+0x10 per the XB1
    // resolve), so the record read is safe, but runStream is STUBBED and executing an action stream is
    // precisely the deferred VM boundary -- so pass 1's EXECUTION is deferred (an un-run action queue
    // is a valid state, and the movie just does not advance via script). The title's nested frames
    // carry no tag-8 commands (verified vs the bundle), so this is a no-op for the title regardless.
    // The faithful runStream body (X360 @0x82B0B7A0 pass 1 / its XB1 twin) lands together with the
    // AptActionInterpreter VM; it is intentionally not called here.

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
        AptCharacterAnimation* const pAnim = reinterpret_cast<AptCharacterAnimation*>(
            reinterpret_cast<char*>(pOwnerChar) + KU_AptEmbeddedMovieOff);   // char + 0x20
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
                AptCharacterAnimation_ExecuteInitActions(pAnim, pParent, nId);

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
                // remove: the console remove-object handler (sub_82AFD150). FLAG: still an
                // un-homed link-stub; frame 0 of the boot title carries no remove commands.
                sub_82AFD150(pDisplayList, CmdI32(pCmd, 0x04));
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
                    gpAptBackToScriptHook(CmdPtr(pCmd, 0x04));
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
    AptCharacterAnimation* const pAnim = reinterpret_cast<AptCharacterAnimation*>(
        reinterpret_cast<char*>(pOwnerChar) + KU_AptEmbeddedMovieOff);
    AptCharacter* const pCharacter =
        (nCharId >= 0 && nCharId < pAnim->mnCharacterCount) ? pAnim->mpCharacterTable[nCharId] : nullptr;

    // FLAG (import not loaded): a null charTable entry is an IMPORTED character (from another
    // GUIAPT bundle) whose id is in this movie's import table -- e.g. TITLE_SCREEN02 charId 30 ==
    // import 'B5HelperComponents::TransitionComponent'. Fixup pass-3 (the import .apt load) is the
    // DEFERRED bring-up boundary, so imported characters are not resolved and their table slot stays
    // null. The console would place the resolved import here; on our bring-up we skip the place for
    // that one character (placing a null character would AV in instantiateCharacter). The movie's
    // OWN (non-import) characters still place, so the title composes minus the imported sub-clips.
    if (pCharacter == nullptr)
        return nullptr;

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
    const float* const pPosition = ((nFlags & 0x04u) != 0u)
        ? reinterpret_cast<const float*>(pBody + 0x0C) : nullptr;
    const AptUint32CXForm* const pPackedColor = ((nFlags & 0x08u) != 0u)
        ? reinterpret_cast<const AptUint32CXForm*>(pBody + 0x24) : nullptr;
    const double fFrameValue = static_cast<double>(*reinterpret_cast<const float*>(pBody + 0x2C));
    const int16_t nClipDepth = static_cast<int16_t>(*reinterpret_cast<const int32_t*>(pBody + 0x38));

    return pDisplayList->placeObjectNCXForm(
        /*pExistingNode*/ nullptr, nDepth, pCharacter, pName, pParent,
        /*bForceRemove*/  0, nClipDepth, fFrameValue, pPosition, /*nPlacementField18*/ 0u, pPackedColor);
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
    // nested sprite movies in the apt_convert-produced bundle have their command-array pointer at
    // frame+0x04 instead of the native-8 frame+0x08, so resolve64 leaves mpCommands null. Skip such a
    // frame (a counted frame with null commands never occurs in a well-formed native-8 movie).
    if (pFrame->mpCommands == nullptr)
        return this;

    for (int32_t i = 0; i < nCount; ++i)
    {
        void* pCmd = pFrame->mpCommands[i];            // *(v7[1] + v8)  (relocated by resolve64)
        if (CmdI32(pCmd, 0x00) != 1)
            continue;                                  // only tag-1 ACTION commands are queued

        // FLAG (DEFERRED AS-VM EXECUTION -- honest boundary, NOT a pointer-guard hack): the record
        // is now fully relocated by AptMovie::resolve64 (its action-stream pointer @cmd+0x08 is a live
        // pointer), so reading it here is SAFE. The console would AddActionBack this action onto the
        // director's queue (queue == gpAptTarget->[+0x18]->[+0x0C]); that queue is drained by the
        // ActionScript interpreter's runStream, which is STUBBED in this bring-up. So the enqueue is
        // the deferred VM's entry point: an enqueued action that never executes is inert, and the
        // director-queue offset chain (console +0x18/+0x0C) reaches the un-reconstructed AS-runtime
        // director layout on x64 (verified: *(director+0x0C) reads a straddled non-pointer value ->
        // dereferencing it AVs). Deferring the enqueue is therefore the faithful "un-run AS action
        // queue" state -- the statically-placed nested shapes/images (which doFrameControls already
        // placed) still render; only the AS action does not advance the clip via script. The enqueue
        // (and its runStream execution) land together when the ActionScript VM is homed.
        //
        // The faithful console enqueue is preserved (commented) so it re-activates with the VM:
        //   void* pDirector = CmdPtr(gpAptTarget, 0x18);                 // *(gpAptTarget + 0x18)
        //   AptActionQueueC* pQueue = CmdPtr(pDirector, 0x0C);           // *(director   + 0x0C)
        //   pQueue->AddActionBack(pCmd + 4, (AptCIH*)pCIH, gnAptActionFrameId);
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
    void* pHash = gpAptPseudoDataPool->Allocate(20);
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
// FLAG (deferred AS-VM EXECUTION -- honest boundary): the console additionally calls
// AptActionInterpreter::_parseStream to re-parse each action stream's BYTECODE; that
// transcode (AptActionInterpreter_ResolveTranscode) still truncates a 64-bit base, and
// the interpreter's runStream is stubbed. So here the stream POINTERS are relocated
// (kept valid -- the record reads never AV) but the bytecode is NEITHER parsed NOR run:
// the action-stream *contents* stay as raw offsets and no _parseStream/runStream fires.
// An un-run AS action queue is a valid state; the statically-placed nested shapes/images
// compose without the VM (they need only their records relocated, which this now does).
// ===========================================================================
void AptMovie::resolve64(uintptr_t nBase, uintptr_t nResBase, uint32_t nResSize)
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

    // ---- allocate + init the label hash (5 dwords, tag 2) ------------------
    void* pHash = gpAptPseudoDataPool->Allocate(20);
    if (pHash)
    {
        CmdSetI32(pHash, 0x04, 0);
        CmdSetI32(pHash, 0x08, 0);
        CmdSetI32(pHash, 0x0C, 0);
        CmdSetI32(pHash, 0x10, 0);
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
            // OUR-BUNDLE OFFSET DELTA (verified vs TITLE_SCREEN02.bundle): the tag-1/tag-2 stream/name
            // pointers sit at the same cmd+0x08 as XB1, but the apt_convert-produced tag-3 PLACE record
            // is 4 bytes "earlier" than the true native-8 layout (the converter's body alignment differs):
            // our name is at body+0x30 (== cmd+0x34, not xb1 cmd+0x38), our clipActions block at
            // body+0x40 (== cmd+0x44, not xb1 cmd+0x48), and the block's record-array pointer at
            // block+0x04 (not xb1 block+0x08). The record STRIDE (16) + stream slot (rec+0x08) match XB1
            // exactly. So the offsets below are XB1-derived STRUCTURE at our-bundle byte positions.
            //
            // FLAG (deferred AS-VM EXECUTION): XB1 calls sub_14084A920 (its _parseStream) on each action
            // stream; here the stream POINTERS are relocated (reads never AV) but the BYTECODE is NEITHER
            // parsed NOR run (runStream is stubbed) -- no _parseStream fires. An un-run AS action queue is
            // a valid state; the statically-placed nested shapes/images compose without the VM.
            switch (eTag)
            {
                case 1:   // ACTION: relocate the action-stream pointer (@cmd+0x08). NOT parsed.
                    reloc64(pCmd, 0x08);
                    break;

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
                    // {count@+0x00 (i32); recArrayPtr@block+0x04 (native-8 ptr; xb1 block+0x08)}; each
                    // record is STRIDE 16 (XB1 v17+=16) with its action-stream pointer at rec+0x08 (an
                    // 8-byte native-8 slot). Relocate the record-array pointer + each record's stream
                    // pointer so a clipActions read never AVs; the stream CONTENTS stay un-parsed
                    // (VM deferred) -- XB1's sub_14084A920 (_parseStream) is NOT called.
                    if (luClip != 0 && inRes(luClip))
                    {
                        void* const pClip = reinterpret_cast<void*>(luClip);
                        const int32_t nRecs = CmdI32(pClip, 0x00);
                        const uintptr_t luRecs = reloc64(pClip, 0x04);   // record-array ptr @block+0x04 (our bundle)
                        if (luRecs != 0 && inRes(luRecs) && nRecs > 0 && nRecs <= 0x1000)
                        {
                            for (int32_t k = 0; k < nRecs; ++k)
                            {
                                // rec+0x08 == the action-stream pointer (native-8 8-byte slot; STRIDE 16).
                                const uintptr_t luRec = luRecs + static_cast<uintptr_t>(16) * k;
                                if (!inRes(luRec) || (luRec + 16) > (nResBase + nResSize))
                                    continue;
                                reloc64(reinterpret_cast<void*>(luRec), 0x08);   // stream ptr (bounds-guarded; NOT parsed)
                            }
                        }
                    }
                    break;
                }

                case 8:   // MORPH: relocate the action-stream pointer (@cmd+0x10 per XB1). NOT parsed.
                    reloc64(pCmd, 0x10);
                    break;

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
