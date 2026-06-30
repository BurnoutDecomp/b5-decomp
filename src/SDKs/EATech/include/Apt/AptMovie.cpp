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
extern void* sub_82B0AE08(void* a1, float* a2, void* a3);               // place-object handler (unhomed)

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
AptMovie* AptMovie::doFrameControls(void* a2, void* pParent, int nFrame)
{
    AptMovieFrame* pFrame = &mpFrames[nFrame];         // 8*nFrame + this[1]

    // ---- pass 1: run the frame's action streams (tag 8, id >= 0) ----------
    int32_t nCount = pFrame->mnCommandCount;
    if (nCount > 0)
    {
        for (int32_t i = 0; i < nCount; ++i)
        {
            void* pCmd = pFrame->mpCommands[i];        // *(v9[1] + v10)
            if (CmdI32(pCmd, 0x00) == 8 && CmdI32(pCmd, 0x04) >= 0)
            {
                // Reserve a fresh AS register frame: save the base, advance it
                // past the live count, then reset the live count to 0.
                AptValue** pSavedRegs = gpAptRegisterBase;            // off_8324E3D0
                gpAptRegisterBase = pSavedRegs + gnAptRegisterCount;  // += 4*count
                gnAptRegisterCount = 0;

                // Resolve the char inst the action runs against (walk pParent's
                // display-list chain to the first animation/level node).
                void* pCharInst = nullptr;
                if (pParent)
                {
                    void* pNode = pParent;
                    if ((CmdI32(pParent, 0x04) & 0x7F) == 0x25)
                    {
                        pNode = AptGetAnimationAtLevel(0);
                    }
                    else
                    {
                        for (void* pTagSrc = CmdPtr(pParent, 0x20); ; pTagSrc = CmdPtr(pNode, 0x20))
                        {
                            int32_t nKind = CmdI32(pTagSrc, 0x08) >> 26;   // srawi 0x1A
                            if (nKind == 9 || nKind == 15)
                                break;
                            pNode = CmdPtr(pNode, 0x1C);
                        }
                    }
                    pCharInst = CmdPtr(pNode, 0x20);   // *(AnimationAtLevel + 32)
                }

                gAptActionInterpreter.runStream(
                    reinterpret_cast<const unsigned char*>(CmdPtr(pCmd, 0x08)),   // v11[2]
                    reinterpret_cast<AptCIH*>(pParent),
                    -1,
                    reinterpret_cast<AptCharacterInst*>(pCharInst));

                // Negate the command's id so it is not re-run this frame.
                CmdSetI32(pCmd, 0x04, -CmdI32(pCmd, 0x04));

                gAptActionInterpreter.CleanupAfterExecution(
                    reinterpret_cast<AptScriptFunctionBase::SavedExecutionState*>(pSavedRegs));
            }
        }
    }

    // ---- pass 2: apply place / remove / back-to-script --------------------
    nCount = pFrame->mnCommandCount;
    if (nCount > 0)
    {
        for (int32_t i = 0; i < nCount; ++i)
        {
            void* pCmd = pFrame->mpCommands[i];        // *(v18[1] + v19)
            int32_t eTag = CmdI32(pCmd, 0x00);

            if (eTag == 3)
            {
                // ---- place: run init actions for the placed character ------
                // pAnim = *(*(*(*(a3+32)+4)+4)+4) + 16
                void* p = CmdPtr(pParent, 0x20);
                p = CmdPtr(p, 0x04);
                p = CmdPtr(p, 0x04);
                p = CmdPtr(p, 0x04);
                void* pAnim = reinterpret_cast<char*>(p) + 0x10;   // r31

                int32_t nId = CmdI32(pCmd, 0x0C);                  // v20[3]
                AptCharacterAnimation_ExecuteInitActions(pAnim, pParent, nId);

                int32_t nId2 = CmdI32(pCmd, 0x0C);
                if (nId2 != -1)
                {
                    // pSlot = *(charTable[nId2]) + 12  (an AptFile slot)
                    void* pCharTable = CmdPtr(pAnim, 0x10);        // v22[4]
                    void* pChar = *reinterpret_cast<void**>(reinterpret_cast<char*>(pCharTable) + 4 * nId2);
                    void* pSlot = reinterpret_cast<char*>(pChar) + 0x0C;   // v24

                    if (CmdI32(pChar, 0x0C) == 0)   // *v24 == 0 -> not yet bound
                    {
                        // find the import-table entry whose id matches nId2.
                        int32_t nImportCount = CmdI32(pAnim, 0x20);   // v22[8]
                        int32_t nFound;
                        if (nImportCount <= 0)
                        {
                            nFound = -1;
                        }
                        else
                        {
                            char* pImport = reinterpret_cast<char*>(CmdPtr(pAnim, 0x24)) + 8;   // v22[9] + 8
                            int32_t k = 0;
                            for (;;)
                            {
                                if (*reinterpret_cast<int32_t*>(pImport) == nId2)
                                {
                                    nFound = k;
                                    break;
                                }
                                ++k;
                                pImport += 0x10;
                                if (k >= nImportCount)
                                {
                                    nFound = -1;
                                    break;
                                }
                            }
                        }

                        void* pSrcFile;
                        if (nFound == -1)
                        {
                            // fall back to the movie's own animation file:
                            // *(*(*(a3+32)+4)+4) + 12
                            void* q = CmdPtr(pParent, 0x20);
                            q = CmdPtr(q, 0x04);
                            q = CmdPtr(q, 0x04);
                            pSrcFile = reinterpret_cast<char*>(q) + 0x0C;
                        }
                        else
                        {
                            pSrcFile = reinterpret_cast<char*>(CmdPtr(pAnim, 0x24)) + 16 * nFound + 0x0C;
                        }
                        AptFile_operator(pSlot, pSrcFile);
                    }
                }
                sub_82B0AE08(a2, reinterpret_cast<float*>(reinterpret_cast<char*>(pCmd) + 4), pParent);
            }
            else if (eTag == 4)
            {
                sub_82AFD150(a2, CmdI32(pCmd, 0x04));
            }
            else if (eTag == 5 && !gbAptBackToScriptFired)
            {
                gpAptBackToScriptHook(CmdPtr(pCmd, 0x04));
                gbAptBackToScriptFired = 1;
            }
        }
    }
    return this;
}

// ===========================================================================
// queueFrameActions @ 0x82AE0228 -- queue every action (tag 1) command of frame
// nFrame onto the current animation target's director action queue (deferred until
// the queue drains). pCIH (a2) is the bound target/this. Faithful to the asm.
// ===========================================================================
AptMovie* AptMovie::queueFrameActions(void* pCIH, int nFrame)
{
    AptMovieFrame* pFrame = &mpFrames[nFrame];         // *(this+4) + 8*nFrame

    int32_t nCount = pFrame->mnCommandCount;
    if (nCount > 0)
    {
        for (int32_t i = 0; i < nCount; ++i)
        {
            void* pCmd = pFrame->mpCommands[i];        // *(v7[1] + v8)
            if (CmdI32(pCmd, 0x00) == 1)
            {
                // queue = off_8324E574->[+0x18]->[+0x0C]  (the director's AptActionQueueC)
                void* pDirector = CmdPtr(gpAptTarget, 0x18);   // *(off_8324E574 + 0x18)
                AptActionQueueC* pQueue =
                    reinterpret_cast<AptActionQueueC*>(CmdPtr(pDirector, 0x0C));
                // AddActionBack(queue, &v9[1] (the action payload), pCIH, frameId).
                // FLAG: the console passes the address pCmd+4 in r4 (the int iEventId
                // slot); the .apt event-id payload is read from there. Cast faithfully.
                pQueue->AddActionBack(
                    static_cast<s32>(reinterpret_cast<intptr_t>(reinterpret_cast<char*>(pCmd) + 4)),
                    reinterpret_cast<AptCIH*>(pCIH), gnAptActionFrameId);
            }
        }
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
