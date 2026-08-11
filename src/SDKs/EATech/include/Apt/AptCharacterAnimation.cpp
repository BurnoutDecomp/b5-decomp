// ===========================================================================
// EATech Apt -- AptCharacterAnimation accessors.   DECOMPILED from the PS3
// EXTERNAL ELF (cross-checked vs X360 ARTIST). See the header for the
// 32-bit-format / 64-bit-runtime fork: the console walks the in-place file
// records with fixed strides; these reconstruct the same logic against the
// transcoded runtime struct's named members.
//
//   IsImport @0x7E3738 / UnmapCharacter @0x7E36CC /
//   ClearCharacterList @0x810DEC / IncCharacterList @0x80E790
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"
#include "SDKs/EATech/include/Apt/AptCharacter.h"
#include "SDKs/EATech/include/Apt/AptFile.h"
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"   // AptSharedPtrIncRef/DecRef/Delete
#include "SDKs/EATech/include/Apt/AptConstFile.h"   // the serialised .apt header (Resolve/Fixup)
#include "SDKs/EATech/include/Apt/AptMovie.h"       // AptMovie::resolve (sprite/movie records)
#include "SDKs/EATech/include/Apt/Apt.h"            // gAptFuncs.pfnLoadRenderingUnit (case-1 shapes)
#include "SDKs/EATech/include/Apt/AptString/EAString.h"  // EAStringC RAII (import-name bracket)
#include "SDKs/EATech/include/Apt/AptTarget.h"      // gpAptTarget->mpLoader (the import loader)
#include "SDKs/EATech/include/Apt/AptLoader.h"      // AptLoader::Load (the real import loader, homed)
#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h"  // PushStaticData (the register-block window push)
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"   // gAptActionInterpreter (runStream/CleanupAfterExecution)
#include "SDKs/EATech/include/Apt/AptCIH.h"                 // AptCIH (ExecuteInitActions' typed CIH chase)
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"       // AptCharacterInst::mpRenderItem
#include "SDKs/EATech/include/Apt/AptRenderItem.h"          // AptRenderItem::mpCharacter
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiGeometryObjects.h"  // native-8 shape-geometry rebase

#include <cstdint>
#include <cstring>   // strstr (ExportClassDefinitionAssets' __Packages label scan)

// ===========================================================================
// Fixup walk -- serialised-blob access.   The serialised .apt records are the
// in-place file blob (no recovered runtime struct), so this TU addresses them by
// their [c:0xNN] byte offsets through the BlobI32/BlobPtr accessors below, exactly
// as the sibling AptMovie::resolve does for the timeline command records. The
// callees the walk recurses into are homed now (AptLoader::Load, AptMovie::resolve64,
// AptCharacter::SetupCharacter, the VM singleton -- see the notes below).
// ===========================================================================

// (The X360 AptLoader::Load @0x82AEEA70 by-value-return wrapper extern was
// DELETED 2026-07-10 -- the import pass below calls the homed member
// `lpLoader->Load(lName)` directly.)

// The AptData resource span the host stashes (x64 native-8 relocation bounds) so the
// Fixup case-5/9 -> AptMovie::resolve64 walk can bounds-check every serialised offset slot.
// HOMED: defined in CgsAptAux.cpp; declared here by name to avoid the heavy CgsAptAux.h.
namespace CgsGui { extern uintptr_t gAptResourceSpanBase; extern uint32_t gAptResourceSpanSize; }

// ===========================================================================
// Init-action execution (the ActionScript VM path).   The init-action passes
// (ExportClassDefinitionAssets / ExecuteInitAction(s)) drive the global
// AptActionInterpreter VM singleton (gAptActionInterpreter, AptGlobals.cpp) and the
// AS register-block window (AptScriptFunctionBase::PushStaticData) -- both homed
// and live; see the notes below.
// ===========================================================================

// AptActionInterpreter VM singleton (dword_8324E760) + its two entry points the init
// passes call. runStream(pStream, pCIH, frameNo=-1, pScope) parses/executes a class-
// definition / init action stream; CleanupAfterExecution(savedScratch, &localState)
// pops the run. The init passes call the members directly on the GLOBAL singleton.
// The VM singleton (dword_8324E760) is the real AptActionInterpreter object
// gAptActionInterpreter (AptGlobals.cpp), initialised at boot by AptUpdateInitialize;
// the init passes call its runStream/CleanupAfterExecution members directly. (The
// void*-placeholder gAptActionInterpreterVM + the two {} free-function stubs are
// retired -- IGNITION 2026-07-01: init-action ActionScript now actually executes.)
extern AptActionInterpreter gAptActionInterpreter;   // &dword_8324E760

// The "parse-arg scratch heap" IS the AS register-block window (off_8324E3D0 /
// dword_8324E3D4 == AptScriptFunctionBase::spRegBlockCurrentFrameBase /
// snRegBlockCurrentFrameCount): each init-action run pushes a fresh window
// (PushStaticData) and CleanupAfterExecution pops it back. The old
// gAptParseArgHeapPtr/Count duplicate globals are retired.

// (AptGetAnimationAtLevel @0x82B00788 -- the root AptCIH-at-level resolver -- is
// reached through AptCIH::GetRootAnimation (AptCIH.cpp), which owns the CIHNone/
// type-0x25 level-0 fallback; the direct extern here is retired.)

// The two host free hooks Unresolve routes released render/font units through
// (X360 dword_8324E870 / dword_8324E87C == XB1 qword_14147AA60 / qword_14147AA78).
// HOMED in AptRenderLinkStubs.cpp (PC-platform leaves there: the PC host installs no
// unit-free callbacks). The console's off-thread deferral apparatus (VM thread id
// dword_8324E500, the deferred-release queue off_8324E2C8/dword_8324E508 under
// unk_8324E728) is owned by AptGlobals.cpp/AptInit.cpp/AptRenderWalk.cpp; the XB1
// Unresolve twin (sub_140845200) frees directly on the single VM thread here, so
// this TU no longer declares it.
extern void AptFreeFontUnit(void* pUnit);        // dword_8324E870 host hook
extern void AptFreeRenderingUnit(void* pUnit);   // dword_8324E87C host hook

// gpAptTarget (off_8324E574) -- the current AS animation target; its import loader
// is mpLoader [c:+0x1C] (AptTarget.h). The asm reads gpAptTarget[+0x1C].

// AptCharacter::SetupCharacter @0x80E894 -- post-relocation per-character init (drops
// the anim-file ref, clears the count, sets the type flags). Bodied in AptCharacter.cpp;
// the asm calls it with r3=pChar only (no extra args), so it is invoked as the member.

// AptFile_::operator_ == AptSharedPtr<AptFile>::operator= (homed in AptSharedPtr.cpp);
// the import slot at entry+0x0C aliases an AptFilePtr (a single AptFile* word), so
// the assignment target is reinterpreted as one.

// ---------------------------------------------------------------------------
// Serialised-record byte accessors. The .apt record layout is the in-place file
// blob (console byte offsets); these keep the access pointer-width-safe on x64
// while documenting the [c:0xNN] offset, matching AptMovie.cpp's CmdI32/CmdPtr.
// ---------------------------------------------------------------------------
namespace
{
    inline int32_t  BlobI32(void* pRec, int nOff)            { return *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(pRec) + nOff); }
    inline int32_t& BlobI32Ref(void* pRec, int nOff)         { return *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(pRec) + nOff); }
    // BlobPtr reads a (already-relocated) pointer slot from a serialised record.
    // The records are addressed by their console byte offsets; on x64 every host
    // pointer slot the runtime touches is a full 8-byte word (the native-8 GUIAPT64
    // form -- the 4-byte console slot widens under the 8-byte pointer rule, the
    // same resolved fork the Fixup walk documents).
    inline void*    BlobPtr(void* pRec, int nOff)            { return *reinterpret_cast<void**>(reinterpret_cast<char*>(pRec) + nOff); }
    inline void*&   BlobPtrRef(void* pRec, int nOff)         { return *reinterpret_cast<void**>(reinterpret_cast<char*>(pRec) + nOff); }
    inline char*    BlobAt(void* pRec, int nOff)             { return reinterpret_cast<char*>(pRec) + nOff; }
}

// FAITHFULNESS (2026-06-30): the invented native-8 defensive apparatus (gAptFixupBound* /
// FixupInBounds / gAptFixupSkipped* + the CgsApt_*Probe sinks + gAptTimeline* + the
// AptRelocateTimeline8* routines) was REMOVED. It only existed because the old FixupWalk used a
// WRONG relocation base (the resource start, not aptDataOffset) so relocations produced garbage that
// had to be skipped; and it wrongly relocated the frame timeline inside Fixup (that is
// AptMovie::resolve's job). With the correct base + verified 64-bit offsets the walk is exact -- no
// guards, no skips, no timeline relocation here.

// IsImport @0x7E3738 -- find the import slot whose local id matches nId.
int32_t AptCharacterAnimation::IsImport(int32_t nId)
{
    if (mnImportCount <= 0)
        return -1;
    if (mpImportTable[0].mnId == nId)
        return 0;
    for (int32_t i = 1; i < mnImportCount; ++i)
    {
        if (mpImportTable[i].mnId == nId)
            return i;
    }
    return -1;
}

// UnmapCharacter @0x7E36CC -- index of pCharacter in the character table.
int32_t AptCharacterAnimation::UnmapCharacter(AptCharacter* pCharacter)
{
    if (mnCharacterCount <= 0)
        return -1;
    if (mpCharacterTable[0] == pCharacter)
        return 0;
    if (mnCharacterCount == 1)
        return -1;
    for (int32_t i = 1; i < mnCharacterCount; ++i)
    {
        if (mpCharacterTable[i] == pCharacter)
            return i;
    }
    return -1;
}

// ClearCharacterList @0x810DEC -- release one character reference per table entry
// (starting at index 1; index 0 is the movie's own root). Type-9 entries are
// imports still owned by the source movie, so they are left alone.
void AptCharacterAnimation::ClearCharacterList() const
{
    for (int32_t i = 1; i < mnCharacterCount; ++i)
    {
        AptCharacter* pCharacter = mpCharacterTable[i];
        if (pCharacter)
        {
            if (pCharacter->mnType != 9)
                pCharacter->ReleaseCharacterReference();
        }
    }
}

// IncCharacterList @0x80E790 -- add a character reference per table entry,
// binding any character that has no animation file yet to filePtr.
void AptCharacterAnimation::IncCharacterList(AptFilePtr filePtr) const
{
    for (int32_t i = 1; i < mnCharacterCount; ++i)
    {
        AptCharacter* pCharacter = mpCharacterTable[i];
        if (pCharacter)
        {
            if (!pCharacter->mpAnimationFile)
            {
                // AptSharedPtr assignment into the character's animation-file
                // slot (inc new, dec old). The old ref is null in this branch.
                AptFile* newp = filePtr.pData;
                AptFile* oldp = pCharacter->mpAnimationFile;
                pCharacter->mpAnimationFile = newp;
                if (newp)
                    AptSharedPtrIncRef(newp);
                if (oldp && AptSharedPtrDecRef(oldp) == 0)
                    AptSharedPtrDelete(oldp);
            }
            pCharacter->AddCharacterReference();
        }
    }
}

// ===========================================================================
// AptCharacterAnimation_Link @0x82AFF... (PS3 @0xF44894, the DecFIGS-named
// AptCharacterAnimation::Link(AptCharacter*, void*)).   DECOMPILED FAITHFULLY from
// the PS3 EXTERNAL ELF (the X360 ARTIST collapses this into the loader; the DWARF-
// named PS3 body is authoritative).
//
// Bind a just-loaded movie root into the scene -- the final import/cross-reference
// resolution pass AptLoader::Update runs once every import is available (state 3 ->
// 4). It is homed here (with the AptCharacterAnimation blob walk) rather than as a
// member because the loader calls it through the named extern hook on the embedded
// AptCharacterAnimation at AptFile::mpData+0x10 (root+16).
//
// The console signature is (this, AptCharacter* a2, void* a3); the body never reads
// a2/a3 (the asm leaves r4/r5 untouched), so the homed free-function form keeps them
// as the raw pData/pDataBlock the loader passes and the return (`this`, a chaining
// value the callers ignore) is modelled void -- matching the family's list-walk
// methods. Two passes over the serialised blob (the native-8 in-place records,
// the same ones Fixup/Unresolve walk -- [c:0xNN] offsets via the Blob* accessors):
//
//   1. import table [c:0x20]/[c:0x24]: for each import, resolve its class name in the
//      imported movie's export table (AptFile::FindExport), store the resolved
//      character into mpCharacterTable[importId], copy the import's loaded AptFile into
//      that character's mpAnimationFile slot (AptFilePtr assign: inc new / dec+delete
//      old), and add a character reference.
//   2. character table [c:0x0C]/[c:0x10]: turn each owned character's stored cross-
//      reference INDICES into live character pointers -- a type-8 import character's
//      two single index fields (+0x10, +0x14), or a type-3 character's index ARRAY
//      (count @+0x14, array @+0x18). (The inverse of Unresolve's index re-mapping.)
//
// x64 fork (RESOLVED -- native-8 GUIAPT64): the resolved-character pointers and the
// index->pointer rewrites are stored into 8-byte blob slots (the widened "1:7:8"
// form; XB1 Link twin sub_14083B6E0). The char table is a host AptCharacter**
// (matching Unresolve's slot stores), so the pointer width is correct on x64. The
// ref-count bookkeeping is faithful.
// ===========================================================================
void AptCharacterAnimation::Link(void* pData, void* pDataBlock)
{
    (void)pData;        // console a2 (r4) -- the asm never reads it
    (void)pDataBlock;   // console a3 (r5) -- the asm never reads it

    // ---- pass 1: resolve the import table -------------------------------------
    // The def-base head fields (importCount/table, charCount/table) are read through
    // the serialized 64-bit AptCharacterAnimation struct (charCount@0x18, charTable@
    // 0x20, importCount@0x34, importTable@0x38 -- the same offsets Fixup relocates).
    const int32_t nImports = mnImportCount;      // [c:0x20] mnImportCount
    if (nImports > 0)
    {
        for (int32_t i = 0; i < nImports; ++i)
        {
            AptImportEntry& rEntry = mpImportTable[i];   // [c:0x24] stride 0x20
            const int32_t nId = rEntry.mnId;                       // importEntry.mnId

            // FindExport(entry.mpFile, entry.mpClassName): the imported movie's export
            // table resolves the class name to a character.
            AptCharacter* const pResolved = rEntry.mpFile->FindExport(rEntry.mpClassName);

                        // Store the resolved character into mpCharacterTable[importId].
            AptCharacter** const pTable = mpCharacterTable;   // [c:0x10] charTable
            pTable[nId] = pResolved;

            // If it resolved, copy the import's AptFile into the resolved character's
            // animation-file slot and add a character reference.
            AptCharacter* const pChar = pTable[nId];
            if (pChar)
            {
                // AptFilePtr assign (char->mpAnimationFile = entry.mpFile): only when
                // the slots differ. inc new, dec+delete old (the console operator=).
                AptFile** pCharSlot  = &pChar->mpAnimationFile;       // resolvedChar +0xC
                AptFile** pEntrySlot = &rEntry.mpFile;                // importEntry AptFile slot
                if (pCharSlot != pEntrySlot)
                {
                    AptFile* const pNew = *pEntrySlot;
                    AptFile* const pOld = *pCharSlot;
                    *pCharSlot = pNew;
                    if (pNew)
                        AptSharedPtrIncRef(pNew);
                    if (pOld && AptSharedPtrDecRef(pOld) == 0)
                        AptSharedPtrDelete(pOld);
                }

                // AddCharacterReference (re-read the table slot, as the asm does).
                pTable[nId]->AddCharacterReference();
            }
        }
    }

    // ---- pass 2: turn stored cross-reference INDICES into character pointers ----
    const int32_t nChars = mnCharacterCount;    // [c:0x0C] mnCharacterCount
        if (nChars > 0)
    {
        AptCharacter** const pTable = mpCharacterTable;   // [c:0x10] charTable
        for (int32_t i = 0; i < nChars; ++i)
        {
            void* pChar = pTable[i];
            if (!pChar)
                continue;

            const int32_t eType = BlobI32(pChar, 0x00);     // mnType
            if (eType != 3)
            {
                if (eType == 8)
                {
                    // Type-8 character: its two cross-reference INDEX fields name
                    // characters by table index; rewrite them to live pointers.
                    // XB1-VERIFIED (Link sub_14083B6E0): the native-8 fields are the
                    // 8-BYTE slots @char+0x20 and @char+0x28 (console: 4-byte +0x10/
                    // +0x14) -- the prior console offsets read header padding on the
                    // GUIAPT64 bundles, so type-8 cross-refs never resolved.
                                        AptCharacter** const pT = mpCharacterTable;
                    const int32_t nIdx0 = BlobI32(pChar, 0x20);
                    const int32_t nIdx1 = BlobI32(pChar, 0x28);
                    // Data guard (host-only; the XB1 asm is unguarded on trusted data):
                    // only rewrite PLAUSIBLE indices -- a slot that already holds a
                    // pointer (or a stale/padding word) must not index the table.
                                        const int32_t nCount = mnCharacterCount;
                    if (nIdx0 >= 0 && nIdx0 < nCount && nIdx1 >= 0 && nIdx1 < nCount)
                    {
                        BlobPtrRef(pChar, 0x20) = pT[nIdx0];
                        BlobPtrRef(pChar, 0x28) = pT[nIdx1];
                    }
                }
                // (any other type: nothing to resolve)
            }
            else
            {
                // Type-3 (Font): an ARRAY of glyph cross-reference indices. XB1-VERIFIED
                // (Link sub_14083B6E0): native-8 count @char+0x28, array ptr @char+0x30,
                // 8-BYTE elements (console: count +0x14, array +0x18, 4-byte elements --
                // and the old loop wrote 8-byte pointers at a 4-byte stride, clobbering
                // its neighbours). The widener emits 8-byte glyph slots, matching XB1.
                const int32_t nRefs = BlobI32(pChar, 0x28);
                // Data guard (host-only): a Font glyph array is at most a few thousand
                // entries and its pointer must be a live heap/resource address; a record
                // whose +0x28/+0x30 read as padding or floats must not drive the rewrite
                // loop (an unbounded count here scribbled the heap -- 0xc0000374 at load).
                void* const pArrProbe = BlobPtr(pChar, 0x30);
                const uintptr_t luArr = reinterpret_cast<uintptr_t>(pArrProbe);
                const bool bArrSane =
                    (nRefs > 0 && nRefs <= 0x4000) &&
                    (luArr >= 0x10000u) && ((luArr >> 47) == 0u);
                if (bArrSane)
                {
                                        const int32_t nCount = mnCharacterCount;
                    for (int32_t r = 0; r < nRefs; ++r)
                    {
                                                AptCharacter** const pT = mpCharacterTable;
                        void* pArr = BlobPtr(pChar, 0x30);                 // the glyph index array
                        const int64_t nIdx = *reinterpret_cast<const int64_t*>(BlobAt(pArr, r * 8));
                        if (nIdx >= 0 && nIdx < nCount)
                            *reinterpret_cast<void**>(BlobAt(pArr, r * 8)) = pT[nIdx];
                        // (the asm reloads char[4]/char[6] each iteration)
                        pChar = pTable[i];
                    }
                }
            }
        }
    }
}

// Resolve @0x82AFF5E0 (PS3 @0x80EEC4) -- resolve the (serialised) movie root against the
// load base. DECOMPILED FAITHFULLY from BURNOUT_X360_ARTIST.XEX. The X360 (a1=this,
// a2=pBase, a3=pConstFile; the 4th reg r6 == CompleteLoad's a5 (the AptDataHeader) is
// carried untouched into Fixup, so it is threaded here as pBlock):
//   v4 = *(a3+28); if (v4) v4 += a3; *(a3+28) = v4;   // relocate pConstFile->mnSecondaryOffset
//   *(a1+48) = 0;                                      // clear the resolve-state scratch (4-byte stw)
//   Fixup();
//   v8 = *(a3+28); if (v8) v8 -= a3; *(a3+28) = v8;   // un-relocate mnSecondaryOffset
//
// x64 LAYOUT CORRECTION (2026-08-11 -- the console offset must be WIDENED, not copied):
// the console `*(a1+48)=0` is def+0x30 on the 4-BYTE def, i.e. the dword AFTER
// initList@+0x2C -- the resolve walk's parsed-value accumulator. On the native-8 def
// initList widens to the 8-byte pointer @+0x48, so that slot is +0x50, and the XB1 twin
// proves it: CompleteLoad sub_1408348B0 (which INLINES Resolve) does
//   lea r10, [rax+20h]                 ; def = movie root + 0x20
//   mov qword ptr [r10+50h], 0         ; an 8-BYTE store at def+0x50
// def+0x30 on native-8 is the AUTHORED msPerFrame (16/33/83 in every one of the 290
// shipped GUIAPT bundles) that AptUpdate paces the timeline from -- clearing it here
// zeroed every movie's frame period on load.
//
// x64: the console's 32-bit in-place `*(a3+28) += a3` widens to the uintptr_t slot below
// (idempotence-guarded); AptMovie::resolve64 (case-5/9, LIVE) reads it as the constant-
// record table while the Fixup walk parses each stream, and it is un-relocated after.
AptCharacterAnimation* AptCharacterAnimation::Resolve(void* pBase, AptConstFile* pConstFile, void* pBlock)
{
    // Relocate the const chunk's record-table slot around Fixup (the console's
    // `*(a3+28) += a3` on the 4-byte layout; the x64 CompleteLoad sub_1408348B0 does the
    // widened slot @const+0x28 -- itemStart offset -> the ABSOLUTE constant-record table
    // pointer _parseStream reads at ctx+0x28 while the Fixup walk parses each stream).
    // The slot is const-chunk-relative (x64: `v12 = v11 + a4`). Idempotence-guarded the
    // same way every other slot relocate is (only a plausible small offset is promoted).
    uintptr_t* pnItemStart = nullptr;
    if (pConstFile != nullptr)
    {
        pnItemStart = reinterpret_cast<uintptr_t*>(
            reinterpret_cast<char*>(pConstFile) + 0x28);
        if (*pnItemStart != 0 && *pnItemStart < 0x10000000u)   // still a file offset
            *pnItemStart += reinterpret_cast<uintptr_t>(pConstFile);
        else
            pnItemStart = nullptr;                             // absent/already live: no un-relocate
    }

    // Clear the resolve walk's parsed-value accumulator (console `*(a1+48)=0` == def+0x30
    // on the 4-byte def == def+0x50 native-8; XB1 `mov qword ptr [r10+50h], 0`).
    mnParsedValueCount = 0;
    AptCharacterAnimation* pResult = Fixup(pBase, pConstFile, pBlock);

    // Un-relocate after Fixup (console `*(a3+28) -= a3`): the ctx table is live only
    // while the parse runs; the unload path re-parses with a null ctx (never reads it).
    if (pnItemStart != nullptr)
        *pnItemStart -= reinterpret_cast<uintptr_t>(pConstFile);

    return pResult;
}

// ===========================================================================
// AptResolveShapeGeometry -- the Step-2 case-1 shape-geometry resolve (the x64 native-8
// form of gAptFuncs.pfnLoadRenderingUnit == CgsGui::AptCallbackRender::LoadRenderingUnit
// @0x5BAEA8).
//
// CORRECTED 2026-07-02 against the CONVERTER's own reader/writer (libapt2
// GeometryStore::LoadGeometry/WriteGeometry -- apt_player renders the converted
// bundle correctly, making it the proven ground truth for this format). The
// previous walk guessed a 4-byte-field "GuiGeometryObject{numFiles,numTexPages,
// fileTableOff}" header with 4+4-pad table cells; the REAL converted chunk is:
//
//   header @ base+geometryOffset (the AptDataHeader field[4] @+0x20 -- that part
//   was right):
//     +0x00 u32 recordCount
//     +0x04 u32 dependencyCount
//     +0x08 u64 recordArrayOffset      (base-relative, FULL 8-byte pointer field)
//   record array: u64[recordCount]     (each base-relative)
//   geometry record == CgsResource::GuiGeometryFile verbatim:
//     {u32 muID, u32 muNumberOfMeshes, u64 mppGeometryMeshes}
//   mesh table: u64[numMeshes] (base-relative) -> GuiGeometryMesh, ALSO verbatim:
//     {miMeshType(=drawType), miTextureMode(=hasImage), miTextureId(=imageID),
//      pad, mpTexture(=the file-provider slot -- holds STALE converter junk on
//      disk!), muNumberOfVerticies, pad, mppVerticies}
//   vertex table: u64[count] (base-relative) -> 20-byte {x,y,rgba,u,v} records,
//   written CONTIGUOUSLY -- exactly CgsGraphics::Basic2dColouredTexturedVertex,
//   so the committed AptRenderHandler::Render consumes entry[0] as the run
//   unchanged.
//
// The lookup KEY is the shape record's own geometryId (its +0x30 pointer-width
// field), NOT the character index -- the second bug in the old resolve.
// ---------------------------------------------------------------------------
// AptFixupGeometryFileNative8 -- promote a geometry record's base-relative
// offsets to live pointers, in place and idempotently (a slot already >= the
// resource size is a pointer; a shared record rebases once). Also clears each
// textured mesh's mpTexture slot: on disk it holds the converter's stale
// file-provider junk (high dword 0x7FF7... on XB1 sources), which the render
// walk would otherwise bind as a texture pointer. FLAG (texture loader
// deferred): miTextureId (the image character id) is preserved for the future
// texture-load pass; a cleared slot draws through the white-texture fallback
// (vertex colours only).
static void AptFixupGeometryFileNative8(CgsResource::GuiGeometryFile* pFile,
                                        uintptr_t luBase, uint32_t luSize)
{
    if (pFile == nullptr)
        return;
    auto lbIsOffset = [luSize](uintptr_t luVal) -> bool { return luVal != 0 && luVal < luSize; };

    if (lbIsOffset(pFile->mppGeometryMeshes))
        pFile->mppGeometryMeshes += luBase;
    if (pFile->mppGeometryMeshes == 0)
        return;

    uintptr_t* const lpMeshTbl = reinterpret_cast<uintptr_t*>(pFile->mppGeometryMeshes);
    for (uint32_t luMesh = 0; luMesh < pFile->muNumberOfMeshes && luMesh < 4096u; ++luMesh)
    {
        if (lbIsOffset(lpMeshTbl[luMesh]))
            lpMeshTbl[luMesh] += luBase;
        if (lpMeshTbl[luMesh] == 0)
            continue;
        CgsResource::GuiGeometryMesh* const lpMesh =
            reinterpret_cast<CgsResource::GuiGeometryMesh*>(lpMeshTbl[luMesh]);

        if (lbIsOffset(lpMesh->mppVerticies))
        {
            // First touch of this mesh: rebase the vertex table. The texture slot is
            // scrubbed ONLY when it still holds a non-pointer value (a small serialized
            // id / in-span offset): the container-import pass (Pool::ResolveImportForEntry)
            // has already written the imported renderengine::Texture* into this slot at
            // bundle load, and that live pointer must SURVIVE for the textured-mesh bind
            // (AptRenderHandler::Render mode 1/2). Values below the resource span size are
            // unpatched serialized junk -> cleared to draw through the white fallback.
            lpMesh->mppVerticies += luBase;
            if (lpMesh->mpTexture < static_cast<uintptr_t>(luSize))
                lpMesh->mpTexture = 0;
        }
        if (lpMesh->mppVerticies == 0 || lpMesh->muNumberOfVerticies == 0)
            continue;

        uintptr_t* const lpVtxTbl = reinterpret_cast<uintptr_t*>(lpMesh->mppVerticies);
        for (uint32_t luV = 0; luV < lpMesh->muNumberOfVerticies && luV < 65536u; ++luV)
        {
            if (lbIsOffset(lpVtxTbl[luV]))
                lpVtxTbl[luV] += luBase;
        }
    }
}

static void* AptResolveShapeGeometry(void* pBlock, int32_t nGeometryId)
{
    if (pBlock == nullptr)
        return nullptr;
    const uintptr_t luBase = CgsGui::gAptResourceSpanBase;
    const uint32_t  luSize = CgsGui::gAptResourceSpanSize;
    if (luBase == 0)
        return nullptr;

    // AptDataHeader field[4] (+0x20): the geometry chunk's base-relative offset.
    const uint64_t luGeomOff =
        *reinterpret_cast<const uint64_t*>(reinterpret_cast<char*>(pBlock) + 0x20);
    if (luGeomOff == 0 || (luSize != 0 && luGeomOff + 16u > luSize))
        return nullptr;

    // The chunk header {u32 recordCount, u32 depCount, u64 recordArrayOffset}.
    const char* const lpChunk = reinterpret_cast<const char*>(luBase + luGeomOff);
    const uint32_t luRecordCount = *reinterpret_cast<const uint32_t*>(lpChunk);
    const uint64_t luRecArrOff   = *reinterpret_cast<const uint64_t*>(lpChunk + 8);   // serialized .apt geometry chunk header @+8
    if (luRecordCount == 0 || luRecordCount > 4096u || luRecArrOff == 0 ||
        (luSize != 0 && luRecArrOff + luRecordCount * 8u > luSize))
        return nullptr;

    const uint64_t* const lpRecords =
        reinterpret_cast<const uint64_t*>(luBase + luRecArrOff);
    for (uint32_t lu = 0; lu < luRecordCount; ++lu)
    {
        const uint64_t luFileOff = lpRecords[lu];
        if (luFileOff == 0 || (luSize != 0 && luFileOff + 16u > luSize))
            continue;
        CgsResource::GuiGeometryFile* const lpFile =
            reinterpret_cast<CgsResource::GuiGeometryFile*>(luBase + luFileOff);
        if (lpFile->muID == static_cast<uint32_t>(nGeometryId))
        {
            AptFixupGeometryFileNative8(lpFile, luBase, luSize);
            return lpFile;
        }
    }
    return nullptr;   // no geometry record for this id -> no rendering unit (safe)
}

// Fixup @0x82AFF268 (PS3 @0xF44D40) -- relocate the serialised movie root IN PLACE.
// DECOMPILED FAITHFULLY from the X360 ARTIST.XEX + PS3 DecFIGS. SINGLE native-64-bit path
// (we ship ONLY GUIAPT64 "1:7:8", 8-byte pointers). The console adds the load base to every
// record's file-relative pointer slot in place, then uses the blob directly as the runtime root.
//
//   `this` = the movie def-base (the AptCharacterAnimation body).
//   pBase   = the relocation base = the memory address of aptDataOffset (the "Apt Data:1:7:8"
//             magic). All serialised offsets are file-relative to aptDataOffset, so
//             pointer = offset + pBase. (The old code used the RESOURCE start as the base --
//             off by the resource header -- which produced garbage that the invented bounds
//             guards then had to skip. The correct base makes the walk exact; no guards.)
//   pConstFile = a3 (r5): the AptMovie::resolve parse context.
//   pBlock     = a4 (r6): the pfnLoadRenderingUnit user data.
//
// Three passes, exactly as the asm:
//   1. relocate mpCharacterTable[+0x20] + mpInitList[+0x48], then each init/export entry's
//      pointer (+0, stride 0x10).
//   2. the character table [count +0x18 / table +0x20]: relocate each slot, wire the back-link
//      (table[0] over the character's now-consumed signature slot @+0x08), switch on the
//      character type and relocate that type's pointer slots, then AptCharacter::SetupCharacter.
//   3. the import table [count +0x34 / table +0x38, stride 0x20]: relocate each entry's
//      name/class, AptLoader::Load(name), assign the handle into the entry's AptFile slot (+0x18).
//
// The 64-bit field offsets are VERIFIED against TITLE_SCREEN02.bundle (Shape geomId@+0x30,
// Text text@+0x50/var@+0x58, Font name@+0x20/glyph@+0x30, movie body @+0x20). Fixup does NOT
// touch the frame table (framesOffset@+0x08) -- that is AptMovie::resolve's job (the invented
// AptRelocateTimeline8 wrongly did it here).
AptCharacterAnimation* AptCharacterAnimation::Fixup(void* pBase, AptConstFile* pConstFile, void* pBlock)
{
    char* const base = reinterpret_cast<char*>(pBase);
    char* const self = reinterpret_cast<char*>(this);
    (void)pConstFile;   // a3: the AptMovie::resolve context (case-5/9, deferred to Step 5)
    (void)pBlock;       // a4: the pfnLoadRenderingUnit user data (case-1 shape, deferred to Step 2)

    // The console relocation primitive (asm guards every reloc: cmplwi r,0 / beq -> 0): a non-zero
    // file offset becomes offset+base; a zero offset stays null. Widened to the 8-byte slot.
    auto reloc = [base](void* pRec, int nOff)
    {
        uintptr_t* pSlot = reinterpret_cast<uintptr_t*>(reinterpret_cast<char*>(pRec) + nOff);
        if (*pSlot != 0)
            *pSlot = reinterpret_cast<uintptr_t>(base) + *pSlot;
    };
    auto ptrAt = [](void* pRec, int nOff) -> char*
    { return *reinterpret_cast<char**>(reinterpret_cast<char*>(pRec) + nOff); };
    auto i32At = [](void* pRec, int nOff) -> int32_t
    { return *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(pRec) + nOff); };

    // ---- pass 1: the character-table + init/export-list pointers -------------
    reloc(self, 0x20);   // mpCharacterTable (charsOffset)
    reloc(self, 0x48);   // mpInitList (exportsOffset)
    {
        const int32_t nInit = i32At(self, 0x40);
        char* const pInitList = ptrAt(self, 0x48);
        for (int32_t i = 0; i < nInit; ++i)
            reloc(pInitList, i * 0x10);   // init/export entry: nameOffset ptr @+0 (stride 0x10)
    }

    // ---- pass 2: the character table -----------------------------------------
    const int32_t nChars = i32At(self, 0x18);
    for (int32_t i = 0; i < nChars; ++i)
    {
        char* const pTable = ptrAt(self, 0x20);
        reloc(pTable, i * 8);             // relocate this table slot (char offset -> pointer)
        char* const pChar = reinterpret_cast<char**>(pTable)[i];
        if (pChar == nullptr)
            continue;

        // back-link: the asm writes table[0] over the character's (now-consumed) signature slot.
        *reinterpret_cast<char**>(BlobAt(pChar, 0x08)) = reinterpret_cast<char**>(pTable)[0];

        switch (i32At(pChar, 0x00))   // character type
        {
            case 1:   // Shape: store the host rendering-unit handle at the geometry slot (+0x20).
                // UN-DEFERRED (Step 2, 2026-07-01). X360 @0x82AFF498-0x82AFF4B4:
                //   *(void**)(pChar + 0x20) = gAptFuncs.pfnLoadRenderingUnit(pBlock /*a4*/, characterIndex);
                // pfnLoadRenderingUnit (== CgsGui::AptCallbackRender::LoadRenderingUnit) bsearches the
                // GuiGeometryObject's file table (at userData+12) for the file whose id == characterIndex
                // and returns that GuiGeometryFile*, which AptCharacter::render later hands to
                // DrawRenderingUnit -> AptRenderHandler::Render.
                //
                // x64 native-8 fork (RESOLVED): our .apt is native 8-byte, so the GuiGeometryObject is at the
                // AptDataHeader's field[4] (native-8 offset +32) as a FILE-RELATIVE OFFSET (not the console
                // +12 pointer), and its file/mesh/vertex tables are 4-byte offsets that must be rebased
                // against the load base. AptResolveShapeGeometry (below) does that rebase once + hands
                // pfnLoadRenderingUnit a shim userData whose +12 is the resolved GuiGeometryObject, exactly
                // reproducing the console store. (The current bundle's geometry is empty -- every mesh has
                // 0 vertices -- so this resolves a real, in-range GuiGeometryFile* that draws nothing; a
                // future bundle with real shape vertices renders unchanged.)
                {
                    // The shape's own geometry id (the +0x30 pointer-width field the
                    // converter writes) keys the geometry store -- NOT the character
                    // index (the old bug; libapt2 Shape::Parse reads geometryId via
                    // ReadPointer and GetGeometry(id) resolves with it).
                    const uint32_t luGeomId = static_cast<uint32_t>(
                        *reinterpret_cast<const uint64_t*>(BlobAt(pChar, 0x30)));
                    void* const lpUnit = AptResolveShapeGeometry(
                        pBlock, static_cast<int32_t>(luGeomId));
                    *reinterpret_cast<void**>(BlobAt(pChar, 0x20)) = lpUnit;
                }
                break;
            case 2:   // Text (edit-text): relocate the text (+0x50) + variable (+0x58) string pointers.
                reloc(pChar, 0x50);
                reloc(pChar, 0x58);
                break;
            case 3:   // Font: relocate the name (+0x20) + glyph-shape-id array (+0x30) pointers.
                reloc(pChar, 0x20);
                reloc(pChar, 0x30);
                break;
            case 5:   // Sprite
            case 9:   // Movie: the embedded timeline is resolved by AptMovie::resolve (body @ char+0x20).
                // STEP 5 (2026-07-01): the faithful native-8 AptMovie::resolve64 relocates the embedded
                // movie's frame table (framesOffset -> pointer) + its command records. The X360 calls
                // reinterpret_cast<AptMovie*>(pChar + 0x20)->resolve(base, pConstFile, self + 0x30); the
                // x64 fork widens the base to the full 8-byte load base + walks native-8 strides. For the
                // ROOT (charTable[0], type 9) this is where mpFrames @char+0x20+0x08 (the 0x5180 offset)
                // becomes a live pointer -- the exact relocation doFrameControls/tick need (was the frame-
                // table-un-relocated AV). FLAG: _parseStream (XB1 sub_14084A920) inside resolve64 is the
                // remaining AS gate -- the VM is live (2026-07-01); see AptMovie.cpp's resolve64 FLAG.
                {
                    // The relocation bounds the host stashed (CgsGui::gAptResourceSpanBase/Size);
                    // base == the load base == the AptData resource base on our path. When the host
                    // did not stash a span (0), fall back to base + a generous cap so the walk still
                    // runs (offsets are all small; the discrimination only needs a cap above them).
                    const uintptr_t luResBase = CgsGui::gAptResourceSpanBase != 0
                        ? CgsGui::gAptResourceSpanBase : reinterpret_cast<uintptr_t>(base);
                    const uint32_t luResSize = CgsGui::gAptResourceSpanSize != 0
                        ? CgsGui::gAptResourceSpanSize : 0x02000000u;   // 32 MB fallback cap
                    // ctx + count (the XB1 Fixup twin sub_1408378E0 case-5/9:
                    // `sub_1408567D0(char+0x20, base, a3=pConstFile, a1+0x50)`).
                    reinterpret_cast<AptMovie*>(pChar + 0x20)->resolve64(
                        reinterpret_cast<uintptr_t>(base), luResBase, luResSize,
                        pConstFile, &this->mnParsedValueCount);
                }
                break;
            case 10:  // StaticText: relocate the text-record array pointer (+0x50).
                reloc(pChar, 0x50);
                break;
            default:  // Image(7) / Button(4) / etc.: no per-type pointer slots to relocate.
                break;
        }

        // Post-relocation per-character init: AptCharacter::SetupCharacter (X360 @0x82AFE560 / PS3
        // @0xF44BF0) -- drops the anim-file ref (mpAnimationFile@0x18), clears the ref-count half, sets
        // the type flag half (mnRefAndFlags@0x10). The faithful body is written (AptCharacter.cpp) and the
        // x64 AptCharacter layout is static_assert-locked (@0x18/@0x10).
        // RE-ENABLED (Steps 6+10, 2026-07-01): the converter char[1] data bug is FIXED -- the GUIAPT/
        // TITLE_SCREEN02 bundle is now uniformly 64-bit (every character carries signature @+0x08; char[1]
        // @res+0x4AF4 is a clean widened Image with mpAnimationFile@+0x18 == 0), via the libapt2
        // Movie::Write pointer-alignment fix (tools/assets/bundles/libapt2_apt_convert.patch) / the
        // fix_title_screen02.py byte-patch. So SetupCharacter's ReleaseAnimationFile reads a null anim-file
        // slot and is safe on every character. This is the faithful console call, restored.
        reinterpret_cast<AptCharacter*>(pChar)->SetupCharacter();
    }

    // ---- pass 3: the import table --------------------------------------------
    // UN-DEFERRED (Step 3, 2026-07-01): the X360 registers each imported .apt with the loader:
    // for each import entry, relocate its movie-name + class-name pointers, then call
    // AptLoader::Load(gpAptTarget->mpLoader, movieName) (findFile-dedup; a miss allocates a
    // "requested" AptFile) and assign that handle into the entry's AptFile slot (+0x18). The
    // loader IS live now (gpAptTarget->mpLoader is initialised by AptCreateTargetInstance, and
    // DriveFaithfulLoad already drives Load/CompleteLoad on it), so the prior "loader
    // under-initialised -> AV" deferral is retired. The GUI Apt host (BrnGuiAptRuntime) then SYNC-
    // loads the referenced import bundles (GuiApt\<name>.bundle) and resolves them, so
    // charTable[importId] becomes a real character; an import bundle that is missing/unconvertable
    // is left in the "requested" state (AptFile slot resolved but mpData null) -- an honest data
    // boundary (an unresolved import is a real state), and the tick skips that one char safely.
    reloc(self, 0x38);   // mpImportTable
    {
        const int32_t nImports = i32At(self, 0x34);
        AptTarget* const lpTarget = gpAptTarget;                    // off_8324E574
        AptLoader* const lpLoader = lpTarget ? lpTarget->mpLoader : nullptr;   // [c:+0x1C]
        for (int32_t i = 0; i < nImports; ++i)
        {
            char* const pEntry = ptrAt(self, 0x38) + i * 0x20;   // import entry: stride 0x20 (native-8)
            reloc(pEntry, 0x00);   // mpImportFileName (movie name)  -- FAITHFUL relocation
            reloc(pEntry, 0x08);   // mpClassName (import name)      -- FAITHFUL relocation

            // Register the imported .apt with the loader (the X360 EAStringC::InitFromBuffer +
            // AptLoader::Load(&handle, loader, name) + AptFile::operator=(&entry+0x18, handle)).
            const char* const pcMovieName = reinterpret_cast<const char*>(BlobPtr(pEntry, 0x00));
            if (lpLoader != nullptr && pcMovieName != nullptr)
            {
                EAStringC lName(pcMovieName);
                AptFilePtr lHandle = lpLoader->Load(lName);
                // Assign the loader handle into the import entry's AptFile slot (+0x18, native-8).
                AptFilePtr* const pSlot = reinterpret_cast<AptFilePtr*>(pEntry + 0x18);
                pSlot->pData = lHandle.pData;   // share the loader's counted ref (the entry owns one)
            }
        }
    }
    return this;
}


// GetIDFromImportFile @0x82ADE998 -- given an import-table slot index, resolve the
// id of the character that the import's class name names in the imported movie's
// export table. Mirrors AptFile::FindExport's inline strcmp; returns -1 if the
// imported movie has no exports or the class name is absent.
int32_t AptCharacterAnimation::GetIDFromImportFile(int32_t nImportIndex)
{
    const AptImportEntry& import = mpImportTable[nImportIndex];
    const AptMovieData* pMovie = static_cast<const AptMovieData*>(import.mpFile->mpData);

    const int32_t nExportCount = pMovie->mnExportCount;
    if (nExportCount <= 0)
        return -1;

    const char* pClassName = import.mpClassName;
    for (int32_t iExport = 0; iExport < nExportCount; ++iExport)
    {
        // Inline strcmp: walk until the SEARCH key (class name) terminates,
        // breaking early on any mismatch. Bytes are unsigned (asm lbz).
        const char* pSearch = pClassName;
        const char* pExport = pMovie->mpExportTable[iExport].mpName;
        int nDiff;
        do
        {
            nDiff = static_cast<unsigned char>(*pSearch) - static_cast<unsigned char>(*pExport);
            if (*pSearch == '\0')
                break;
            ++pSearch;
            ++pExport;
        }
        while (nDiff == 0);

        if (nDiff == 0)   // names matched up to a shared terminator
            return pMovie->mpExportTable[iExport].mnCharacterId;
    }
    return -1;
}

// ===========================================================================
// The init-action passes (the "__Packages" class bootstrap + the per-character
// onClipEvent init streams). Both walk the serialised .apt timeline command records
// by their native-8 byte offsets (via Blob*) -- the in-place blob records have no
// recovered runtime struct, exactly the opaque-record handling the Fixup walk uses.
// The VM side (gAptActionInterpreter + the AS register-block window) is typed + live.
// ===========================================================================
namespace
{
    // The scope the runStream calls bind: the character inst of the level the
    // supplied CIH lives at. Shared verbatim by ExportClassDefinitionAssets /
    // ExecuteInitAction -- both inline the AptCIH::GetRootAnimation walk (CIHNone
    // value tag 0x25 -> the level-0 root, else chase the display-list parent
    // [c:+0x1C] until the char-inst type tag is 9/15), then read the level node's
    // char inst ([c:+0x20]). The console read the tag as charInst[c:+0x08]>>26 (the
    // X360 big-endian position); on x64 the tag is mTypeFlags & 0x3F, so the walk
    // goes through the TYPED members (GetRootAnimation, AptCIH.cpp). pA2==0 -> 0.
    AptCharacterInst* ResolveAnimationScope(void* pA2)
    {
        if (!pA2)
            return nullptr;
        AptCIH* const pLevel = static_cast<AptCIH*>(pA2)->GetRootAnimation();
        return pLevel ? pLevel->GetCharacterInst() : nullptr;   // v14 = *(pLevel+0x20)
    }
}

// ExportClassDefinitionAssets @0x82AEA8C8 -- run each pending "__Packages." class-
// definition frame-label exactly once (then mark it consumed by negating its
// indicator). this[c:0x28]/[c:0x2C] is the label/indicator list (an AptInitEntry list:
// {name@+0, indicator@8}); this[c:+4] is the timeline-command table the matching action
// stream lives in. The def-base head fields are read through the serialized 64-bit struct
// (initCount@0x40, initList@0x48, framesOffset@0x08). FAITHFUL to the asm; the VM
// singleton is the homed gAptActionInterpreter (AptGlobals.cpp).
void* AptCharacterAnimation::ExportClassDefinitionAssets(void* pA2)
{
    void* result = nullptr;

    const int32_t nLabels = mnInitListCount;                // [c:0x28] initCount
    for (int32_t iLabel = 0; iLabel < nLabels; ++iLabel)
    {
        AptInitEntry& rEntry = mpInitList[iLabel];          // [c:0x2C] initList (stride 0x10)

        // indicator < 0 -> already consumed -> stop the whole walk (asm: blt -> exit).
        if (rEntry.mnIndicator < 0)
            break;

        // strstr(name, "__Packages.") -- only the package-definition labels qualify.
        // (the label entry reuses the init-entry ptr slot as the frame-label name)
        result = const_cast<char*>(::strstr(reinterpret_cast<const char*>(rEntry.mpInitObject), "__Packages."));
        if (result)
        {
            // Find the type-8 timeline command whose id [+4] == this label's indicator.
            // NATIVE-8 (frame record {i32 count@0, pad, u64 cmds@8}; console was {count@0, cmds@4}).
            const int32_t nIndicator = rEntry.mnIndicator;
            void* pCmdHdr = mpFrames;                        // frame[0] == the init command table
            const int32_t nCmds = BlobI32(pCmdHdr, 0x00);
            if (nCmds > 0)
            {
                void** pCmdTable = reinterpret_cast<void**>(BlobPtr(pCmdHdr, 0x08));   // native-8 cmds@+8
                int32_t iCmd = 0;
                while (BlobI32(pCmdTable[iCmd], 0x00) != 8 ||
                       BlobI32(pCmdTable[iCmd], 0x04) != nIndicator)
                {
                    if (++iCmd >= nCmds)
                        goto consumeLabel;
                }

                // Run the matched command's action stream on the VM singleton.
                {
                    // Push a fresh register-block window (inlined PushStaticData on the console).
                    void* pSavedScratch = AptScriptFunctionBase::PushStaticData();

                    void* pScope  = ResolveAnimationScope(pA2);
                    void* pStream = BlobPtr(pCmdTable[iCmd], 0x10);   // native-8 tag-8 stream @cmd+0x10
                    // X360 runStream(&dword_8324E760, stream, a2, -1, scope) -> the member
                    // (pStream, pCIH = a2, nLength = -1, pCharInst = the resolved scope).
                    gAptActionInterpreter.runStream(
                        static_cast<const unsigned char*>(pStream),
                        reinterpret_cast<AptCIH*>(pA2), -1,
                        reinterpret_cast<AptCharacterInst*>(pScope));

                    // Pop the register-block window (the X360 2-arg CleanupAfterExecution;
                    // r3 carries PopStaticData's incidental return -- every caller ignores it).
                    gAptActionInterpreter.CleanupAfterExecution(pSavedScratch);
                    result = pSavedScratch;
                }
            }

        consumeLabel:
            // Mark the label consumed: indicator = -indicator.
            int32_t& rIndicator = mpInitList[iLabel].mnIndicator;
            rIndicator = -rIndicator;
        }
    }
    return result;
}

// ExecuteInitAction @0x82AEE2D8 -- find the type-8 init command whose id == nId, run
// its class-definition assets, run that command's action stream once, then mark it
// consumed (negate the id). this[c:+4] is the timeline-command table. FAITHFUL.
void* AptCharacterAnimation::ExecuteInitAction(void* pA2, int32_t nId)
{
    void* result = nullptr;

    void* pCmdHdr = mpFrames;                            // frame[0] == the init command table
    const int32_t nCmds = BlobI32(pCmdHdr, 0x00);        // NATIVE-8 frame record (count@0, cmds@8)
    if (nCmds <= 0)
        return result;

    void** pCmdTable = reinterpret_cast<void**>(BlobPtr(pCmdHdr, 0x08));   // native-8 cmds@+8
    int32_t iCmd = 0;
    void* pEntry;
    for (;;)
    {
        pEntry = pCmdTable[iCmd];
        if (BlobI32(pEntry, 0x00) == 8 && BlobI32(pEntry, 0x04) == nId)
            break;
        if (++iCmd >= nCmds)
            return result;                               // LABEL_17 (no match)
    }

    // Export this class's __Packages assets first, then run the init stream.
    ExportClassDefinitionAssets(pA2);

    // Push a fresh register-block window (inlined PushStaticData on the console).
    void* pSavedScratch = AptScriptFunctionBase::PushStaticData();

    void* pScope = ResolveAnimationScope(pA2);

    // stream = the matched command's action stream -- re-read the table (the asm reloads
    // it after the call). NATIVE-8: cmds@frame+8, tag-8 stream @cmd+0x10.
    void** pCmdTable2 = reinterpret_cast<void**>(BlobPtr(mpFrames, 0x08));
    void* pStream = BlobPtr(pCmdTable2[iCmd], 0x10);
    // X360 runStream(&dword_8324E760, stream, a2, -1, scope) -> the member.
    gAptActionInterpreter.runStream(
        static_cast<const unsigned char*>(pStream),
        reinterpret_cast<AptCIH*>(pA2), -1,
        reinterpret_cast<AptCharacterInst*>(pScope));

    // Pop the register-block window (X360 2-arg CleanupAfterExecution; the incidental
    // r3 return is ignored by every caller).
    gAptActionInterpreter.CleanupAfterExecution(pSavedScratch);
    result = pSavedScratch;

    // Mark consumed: entry[1] = -entry[1].
    BlobI32Ref(pEntry, 0x04) = -BlobI32(pEntry, 0x04);
    return result;
}

// ExecuteInitActions @0x82AF4340 -- locate the init-list bucket for character nId
// (either directly in mpImportTable [c:0x24]/[c:0x20], or by chasing the import via
// GetIDFromImportFile), then run every pending type-3 init command in that bucket and
// finally the import-resolved init action. FAITHFUL to the asm; the def-base head fields
// are read through the serialized 64-bit struct (importCount@0x34, importTable@0x38 stride
// 0x20), and the import->AptFile->embedded-movie chase is the TYPED native-8 walk below
// (file->mpData -> def base @root+0x20 -> charTable[id] -> char+0x20), guarded on the
// real data bounds -- the whole path runs on the widened records.
void* AptCharacterAnimation::ExecuteInitActions(void* pA2, int32_t nId)
{
    void* result = nullptr;
    int32_t nResolvedId = nId;                            // v5 (r27)
    void* pBucketOwner  = this;                           // v6 (r28)

    // Locate nId in mpImportTable (the serialized 64-bit import entry: id at +0x10).
    int32_t iSlot = -1;
    const int32_t nImports = mnImportCount;
    // Default bucket = the supplied CIH's clip movie (console chase *(*(*(pA2+0x20)+4)+4)+0x10
    // == CIH -> charInst -> renderItem -> character -> embedded movie). NATIVE-8: the typed
    // chain with the widened body offset (KU_AptEmbeddedMovieOff).
    void* pBucket = nullptr;
    {
        AptCIH* pCIH = reinterpret_cast<AptCIH*>(pA2);
        AptCharacterInst* pCI = pCIH ? pCIH->GetCharacterInst() : nullptr;
        AptRenderItem* pRI = pCI ? pCI->mpRenderItem : nullptr;
        AptCharacter* pChar = pRI ? pRI->mpCharacter : nullptr;
        if (pChar != nullptr)
            pBucket = reinterpret_cast<char*>(pChar) + 0x20;   // the embedded AptMovie
    }
    if (nImports > 0)
    {
        for (int32_t i = 0; i < nImports; ++i)
        {
            if (mpImportTable[i].mnId == nId)
            {
                iSlot = i;
                break;
            }
        }
    }

    if (iSlot != -1)
    {
        // Chase the import: id of the named export inside the imported movie.
        nResolvedId = GetIDFromImportFile(iSlot);
        if (nResolvedId != -1)
        {
            // pBucketOwner = imported movie DEF BASE; pBucket = the exported character's
            // embedded movie. NATIVE-8 typed walk (console: file+0x14 -> root+0x10 ->
            // *(def+0x10)[id*4] -> char+0x10).
            AptImportEntry& rImportEntry = mpImportTable[iSlot];
            AptFile* pFile = reinterpret_cast<AptFile*>(rImportEntry.mpFile);
            void* pImportedRoot = pFile ? pFile->mpData : nullptr;             // the root char header
            if (pImportedRoot != nullptr)
            {
                pBucketOwner = reinterpret_cast<char*>(pImportedRoot) + 0x20;  // def base (native-8)
                AptCharacterAnimation* pOwner =
                    reinterpret_cast<AptCharacterAnimation*>(pBucketOwner);
                if (nResolvedId >= 0 && nResolvedId < pOwner->mnCharacterCount &&
                    pOwner->mpCharacterTable != nullptr &&
                    pOwner->mpCharacterTable[nResolvedId] != nullptr)
                {
                    pBucket = reinterpret_cast<char*>(pOwner->mpCharacterTable[nResolvedId]) + 0x20;
                }
            }
        }
    }
    if (pBucket == nullptr)
        return result;

    // Run every pending type-3 (PLACE) init command in the bucket's frame 0.
    // NATIVE-8: movie {fc@0, frames@8}; frame record {i32 count@0, pad, u64 cmds@8};
    // PLACE body at align8(cmd+4)=cmd+8 with charId @body+8 (== cmd+0x10).
    if (BlobI32(pBucket, 0x00) > 0)
    {
        void* pHdr    = BlobPtr(pBucket, 0x08);                          // frames -> frame[0]
        void** pCmds  = reinterpret_cast<void**>(BlobPtr(pHdr, 0x08));   // frame[0].cmds
        int32_t nCmds = BlobI32(pHdr, 0x00);                            // frame[0].count
        if (pHdr != nullptr && pCmds != nullptr && nCmds > 0)
        {
            int32_t iCmd = 0;
            do
            {
                void* pCmd = pCmds[iCmd];
                if (BlobI32(pCmd, 0x00) == 3 && BlobI32(pCmd, 0x10) != -1)
                    result = reinterpret_cast<AptCharacterAnimation*>(pBucketOwner)->ExecuteInitAction(pA2, BlobI32(pCmd, 0x10));
                pHdr  = BlobPtr(pBucket, 0x08);                          // reload (asm re-reads)
                pCmds = reinterpret_cast<void**>(BlobPtr(pHdr, 0x08));
                ++iCmd;
            }
            while (iCmd < BlobI32(pHdr, 0x00));
        }
    }

    // Finally the import-resolved init action.
    if (nResolvedId != -1)
        result = reinterpret_cast<AptCharacterAnimation*>(pBucketOwner)->ExecuteInitAction(pA2, nResolvedId);

    return result;
}

// Unresolve @0x82AF77B0 (X360) / sub_140845200 (XB1, THE NATIVE-8 ARBITER --
// located by its unique 0x09876543 freed-character sentinel and transcoded
// 2026-07-02, retiring the console 4-byte-stride form). The inverse of Fixup:
// called only from AptFile::~AptFile with the load base; releases the per-
// character animation refs, converts the type-8/font cross-reference POINTERS
// back to table indices, per-type un-relocates every record slot (subtract
// nBase), tears the imports down, stamps every owned character freed, and
// un-relocates the three table heads. All accesses go through the typed
// native-8 members (the XB1 offsets: charCount@+0x18, charTable@+0x20 8-byte
// slots, importCount@+0x34, importTable@+0x38 stride 0x20 {name,class,id,file},
// initCount@+0x40, initList@+0x48 stride 0x10, parsed-count@+0x50; character
// records: type@+0, fixup-link@+8, refcount@+0x10, flags-halfword@+0x12,
// anim-file@+0x18; Shape geometry@+0x30; Text text@+0x50/var@+0x58; Font
// name@+0x20/glyphCount@+0x28/glyphs@+0x30; sprite/movie body@+0x20 (KU_Apt
// EmbeddedMovieOff); Image live-unit@+0x20; StaticText recordCount@+0x48/
// records@+0x50 stride 0x40 w/ the glyph-array slot@+0x38; xref slots
// @+0x20/+0x28).
void* AptCharacterAnimation::Unresolve(int32_t nBase)
{
    const intptr_t nB = static_cast<intptr_t>(static_cast<uint32_t>(nBase));
    void* result = nullptr;
    mnParsedValueCount = 0;                                   // *(a1+0x50) = 0

    auto IsImportId = [this](int32_t nLogical) -> bool
    {
        for (int32_t i = 0; i < mnImportCount; ++i)
            if (mpImportTable[i].mnId == nLogical)
                return true;
        return false;
    };
    auto IndexOfChar = [this](const void* pWant) -> int64_t
    {
        for (int32_t i = 0; i < mnCharacterCount; ++i)
            if (mpCharacterTable[i] == pWant)
                return i;
        return -1;
    };
    auto Unreloc = [nB](void*& rSlot)
    {
        rSlot = rSlot ? reinterpret_cast<void*>(
                            reinterpret_cast<intptr_t>(rSlot) - nB)
                      : nullptr;
    };

    // ---- pass 1: release imported characters / re-index type-8 xrefs ----------
    for (int32_t i = 0; i < mnCharacterCount; ++i)
    {
        AptCharacter* const pChar = mpCharacterTable[i];
        if (pChar == nullptr)
            continue;
        if (!IsImportId(i))
        {
            if (pChar->mnType == 8)
            {
                // The two cross-reference pointer slots (+0x20 / +0x28) revert
                // to character-table indices (-1 when unmatched).
                void** const ppSlotA = reinterpret_cast<void**>(
                    reinterpret_cast<char*>(pChar) + 0x20);
                void** const ppSlotB = reinterpret_cast<void**>(
                    reinterpret_cast<char*>(pChar) + 0x28);
                *reinterpret_cast<int64_t*>(ppSlotA) = IndexOfChar(*ppSlotA);
                *reinterpret_cast<int64_t*>(ppSlotB) = IndexOfChar(*ppSlotB);
            }
        }
        else
        {
            // Imported: drop one character reference; the last drop releases the
            // animation file (the XB1 swaps the slot null then runs the shared-
            // ptr release twice -- the swap-and-dispose idiom).
            pChar->ReleaseCharacterReference();
            result = pChar;
        }
    }

    // ---- pass 2: per-character un-relocation + type-specific teardown ---------
    for (int32_t i = 0; i < mnCharacterCount; ++i)
    {
        AptCharacter* const pChar = mpCharacterTable[i];
        if (pChar == nullptr || IsImportId(i))
            continue;
        char* const pc = reinterpret_cast<char*>(pChar);
        switch (pChar->mnType)
        {
            case 1:   // Shape: clear the resolved flag, free the host rendering
            {         // unit (+0x30), null the slot.
                *reinterpret_cast<uint16_t*>(BlobAt(pc, 0x12)) &= ~0x8000u;
                void** const ppUnit = reinterpret_cast<void**>(pc + 0x30);
                AptFreeRenderingUnit(*ppUnit);        // qword_14147AA78 host hook
                *ppUnit = nullptr;
                result = nullptr;
                break;
            }
            case 2:   // Text: un-relocate the text (+0x50) + variable (+0x58).
                Unreloc(*reinterpret_cast<void**>(BlobAt(pc, 0x50)));
                Unreloc(*reinterpret_cast<void**>(BlobAt(pc, 0x58)));
                break;
            case 3:   // Font: un-relocate the name (+0x20); glyph pointers
            {         // (+0x30, count +0x28) back to indices; then the array ptr.
                Unreloc(*reinterpret_cast<void**>(BlobAt(pc, 0x20)));
                const int32_t nGlyphs = *reinterpret_cast<int32_t*>(BlobAt(pc, 0x28));
                void** const ppGlyphs = *reinterpret_cast<void***>(BlobAt(pc, 0x30));
                for (int32_t g = 0; g < nGlyphs; ++g)
                    *reinterpret_cast<int64_t*>(&ppGlyphs[g]) =
                        IndexOfChar(ppGlyphs[g]);
                Unreloc(*reinterpret_cast<void**>(BlobAt(pc, 0x30)));
                break;
            }
            case 5:
            case 9:   // Sprite / movie: recurse into the embedded timeline
                      // (the XB1 sub_14085EE40(body, base, &parsedCount) --
                      // AptMovie::unresolve; ours keeps its declared shape).
                result = reinterpret_cast<AptMovie*>(pc + 0x20)   // KU_AptEmbeddedMovieOff (AptCIH.h)
                             ->unresolve(nBase, static_cast<int>(mnParsedValueCount));
                break;
            case 7:   // Image: queued release of the live unit when resolved.
            {
                uint16_t& rFlags = *reinterpret_cast<uint16_t*>(BlobAt(pc, 0x12));
                void** const ppUnit = reinterpret_cast<void**>(pc + 0x20);
                if ((rFlags & 1u) != 0u)
                {
                    // The XB1 queues the release on the VM thread (the
                    // qword_14147A0E0 ring) or calls the host free hook
                    // directly off-thread; single-threaded bring-up = direct.
                    AptFreeFontUnit(*ppUnit);         // qword_14147AA60 host hook
                }
                rFlags &= ~3u;
                *ppUnit = nullptr;
                break;
            }
            case 10:  // StaticText: per record (stride 0x40) un-relocate the
            {         // glyph-array slot (+0x38); then the record-array ptr.
                const int32_t nRecs = *reinterpret_cast<int32_t*>(BlobAt(pc, 0x48));
                char* const pRecs = *reinterpret_cast<char**>(BlobAt(pc, 0x50));
                for (int32_t r = 0; r < nRecs; ++r)
                    Unreloc(*reinterpret_cast<void**>(pRecs + r * 0x40 + 0x38));
                Unreloc(*reinterpret_cast<void**>(BlobAt(pc, 0x50)));
                break;
            }
            default:
                break;
        }
    }

    // ---- pass 3: drop the imports + clear their character slots ---------------
    for (int32_t i = 0; i < mnImportCount; ++i)
    {
        AptImportEntry& rEntry = mpImportTable[i];
        AptFilePtr* const pSlot = reinterpret_cast<AptFilePtr*>(&rEntry.mpFile);
        AptFile* const pOld = pSlot->pData;
        pSlot->pData = nullptr;
        if (pOld != nullptr && AptSharedPtrDecRef(pOld) == 0)
            AptSharedPtrDelete(pOld);
        result = pOld;
        mpCharacterTable[rEntry.mnId] = nullptr;
    }

    // ---- pass 4: stamp the owned characters freed + un-relocate their slots ---
    for (int32_t i = 0; i < mnCharacterCount; ++i)
    {
        AptCharacter* const pChar = mpCharacterTable[i];
        if (pChar != nullptr)
        {
            // The freed sentinel lands in the fixup-link slot (+8).
            *reinterpret_cast<int32_t*>(
                reinterpret_cast<char*>(pChar) + 0x08) = 0x09876543;
            Unreloc(*reinterpret_cast<void**>(&mpCharacterTable[i]));
        }
    }

    // ---- un-relocate the table heads + entries --------------------------------
    Unreloc(*reinterpret_cast<void**>(&mpCharacterTable));
    for (int32_t i = 0; i < mnImportCount; ++i)
    {
        Unreloc(*reinterpret_cast<void**>(
            const_cast<char**>(&mpImportTable[i].mpImportFileName)));
        Unreloc(*reinterpret_cast<void**>(
            const_cast<char**>(&mpImportTable[i].mpClassName)));
    }
    for (int32_t i = 0; i < mnInitListCount; ++i)
    {
        Unreloc(mpInitList[i].mpInitObject);
        if (mpInitList[i].mnIndicator < 0)
            mpInitList[i].mnIndicator = -mpInitList[i].mnIndicator;
    }
    Unreloc(*reinterpret_cast<void**>(&mpImportTable));
    Unreloc(*reinterpret_cast<void**>(&mpInitList));

    mnParsedValueCount = 0;
    return result;
}

// ResetInitIndicators @0x82AD92A8 -- restore the pending init indicators flipped
// negative while the instance ran. Two passes: (1) per type-5/9 character, find the
// embedded movie's pending type-8 label and un-negate it; (2) per init-list entry,
// un-negate any negative indicator. FAITHFUL to the asm.
//
// GUIAPT64 native-8 (transcoded): the def-base + its tables are the NAMED
// serialized-64 members (charCount@0x18/charTable@0x20, initCount@0x40/initList@0x48
// stride 0x10 -- the console read them at 0x0C/0x10 and 0x28/0x2C stride 8), and the
// embedded movie/frame records are the same native-8 shapes the sibling init passes
// walk: body @char+0x20 (KU_AptEmbeddedMovieOff), movie {fc@0, frames@8}, frame
// record {i32 count@0, pad, u64 cmds@8} (console body@+0x10, frames@+4, cmds@+4).
// The old console-offset walk read the widened def-base's pad/frames slots as
// count/table -- garbage pointers on every real GUIAPT64 bundle.
void AptCharacterAnimation::ResetInitIndicators()
{
    // ---- pass 1: the type-5/9 characters' embedded label tables -------------------
    for (int32_t i = 0; i < mnCharacterCount; ++i)               // *(this+0x0C) console
    {
        AptCharacter* const pChar = mpCharacterTable[i];         // console 4-byte slots @+0x10
        if (!pChar)
            continue;
        if (pChar->mnType != 5 && pChar->mnType != 9)
            continue;

        // movie = char + 0x20 (native-8; console r11 += 0x10); frame[0] = *(movie+8).
        void* const pMovie  = reinterpret_cast<char*>(pChar) + KU_AptEmbeddedMovieOff;
        void* const pFrame0 = BlobPtr(pMovie, 0x08);             // *(movie+4) console
        if (!pFrame0)
            continue;
        void** const pTable = reinterpret_cast<void**>(BlobPtr(pFrame0, 0x08));   // *(mpFrames+4) console
        if (!pTable)
            continue;
        const int32_t nEntries = BlobI32(pFrame0, 0x00);         // *(mpFrames)
        if (nEntries <= 0)
            continue;

        // Un-negate the first pending (negative-id) type-8 label of this movie, then stop.
        for (int32_t iEntry = 0; iEntry < nEntries; ++iEntry)
        {
            void* const pEntry = pTable[iEntry];
            if (pEntry && BlobI32(pEntry, 0x00) == 8 && BlobI32(pEntry, 0x04) < 0)
            {
                BlobI32Ref(pEntry, 0x04) = -BlobI32(pEntry, 0x04);
                break;
            }
        }
    }

    // ---- pass 2: the init-indicator list [c:0x28]/[c:0x2C] ------------------------
    for (int32_t i = 0; i < mnInitListCount; ++i)                // console stride 8, id@+4
    {
        int32_t& rIndicator = mpInitList[i].mnIndicator;
        if (rIndicator < 0)
            rIndicator = -rIndicator;
    }
}
