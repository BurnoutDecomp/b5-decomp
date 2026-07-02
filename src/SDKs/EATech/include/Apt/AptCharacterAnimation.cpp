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
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiGeometryObjects.h"  // native-8 shape-geometry rebase

#include <cstdint>
#include <cstring>   // strstr (ExportClassDefinitionAssets' __Packages label scan)

// The pass-3 import-registration probe sink (host-implemented; weak no-op default so this TU
// links standalone). The host (BrnAptRuntimeBringUp) provides the strong definition that logs
// each import's movie/class name + the loader handle, and then SYNC-loads the import bundle.
#if defined(_MSC_VER)
extern "C" void CgsApt_ImportProbe(int nIndex, const char* pcMovieName, const char* pcClassName, void* pHandle);
#pragma comment(linker, "/alternatename:CgsApt_ImportProbe=CgsApt_ImportProbeDefault")
extern "C" void CgsApt_ImportProbeDefault(int, const char*, const char*, void*) {}
#else
extern "C" void CgsApt_ImportProbe(int nIndex, const char* pcMovieName, const char* pcClassName, void* pHandle);
#endif

// ===========================================================================
// Fixup walk -- un-homed callees / globals.   The serialised .apt records are the
// in-place file blob (no recovered runtime struct), so this TU addresses them by
// their console byte offsets through the BlobI32/BlobPtr accessors below, exactly
// as the sibling AptMovie::resolve does for the timeline command records. The
// callees the walk recurses into are owned by their own (still deferred) TUs and
// declared here with the raw-but-faithful call-site signatures so this TU links.
// FLAG: each lands when its owning TU is reconstructed.
// ===========================================================================

// AptLoader::Load @0x82AEEA70 -- the X360 3-arg overload (DISTINCT from the PS3
// AptLoader::Load(const EAStringC&) homed in AptLoader.cpp): registers/looks up the
// named .apt and writes the resulting AptFilePtr into *pOut, returning pOut. The
// loader object is gpAptTarget[+0x1C]. Declared free-standing (the X360 AptLoader
// runtime layout is a follow-on); the call site passes the raw pointers faithfully.
struct AptLoader;
extern AptFilePtr* AptLoader_LoadX360(AptFilePtr* pOut, AptLoader* pLoader, const EAStringC* pName);   // AptLoader::Load @0x82AEEA70 (FLAG)

// FLAG (x64 native-8 relocation bounds): the AptData resource span the host stashes so the
// Fixup case-5/9 -> AptMovie::resolve64 walk can bounds-check every serialised offset slot
// (defined in CgsAptAux.cpp). Declared here by name to avoid pulling the heavy CgsAptAux.h.
namespace CgsGui { extern uintptr_t gAptResourceSpanBase; extern uint32_t gAptResourceSpanSize; }

// ===========================================================================
// Init-action execution -- un-homed globals / callees (the ActionScript VM path).
// The init-action passes (ExportClassDefinitionAssets / ExecuteInitAction(s)) drive
// the global AptActionInterpreter VM singleton and a global parse-arg scratch heap,
// both still owned by their own deferred TUs. They are declared here with the raw
// call-site signatures so this TU links; FLAG: each lands when its TU is rebuilt.
// ===========================================================================

// AptActionInterpreter VM singleton (dword_8324E760) + its two entry points the init
// passes call. runStream(pStream, pCIH, frameNo=-1, pScope) parses/executes a class-
// definition / init action stream; CleanupAfterExecution(savedScratch, &localState)
// pops the run. Raw (int) call-site forms -- the VM's recovered AptActionInterpreter
// class (AptActionInterpreter.h) models the same two methods, but the init passes call
// them on the GLOBAL singleton with the console arg shapes, so they are bridged here.
// FLAG: gAptActionInterpreterVM + these two thunks land with the VM TU.
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

// AptGetAnimationAtLevel @0x82B00788 -- the root AptCIH-at-level resolver the init
// passes use when the supplied CIH is itself a level (type 0x25). Homed in
// AptCharacterHelper.cpp; CANONICAL return reconciled to AptCIH* (was void* here --
// the void* blob walk below still binds via the AptCIH*->void* assignment).
struct AptCIH;
extern AptCIH* AptGetAnimationAtLevel(int nLevel);

// Unresolve reaches the VM's deferred-release queue + a host free hook for shapes.
// dword_8324E500 = the VM (owning) thread id; the queue (off_8324E2C8 / count
// dword_8324E508) is drained under gAptUnresolveMutex; off-thread releases go through
// the host free thunks (dword_8324E870 for fonts, dword_8324E87C for shapes).
// FLAG: all owned by the VM / host TUs.
extern int   gAptVMThreadId;                          // dword_8324E500 (FLAG)
extern int   gAptDeferredReleaseCount;                // dword_8324E508 (FLAG)
extern void* gAptDeferredReleaseQueue;                // off_8324E2C8 (FLAG)
extern void  AptFreeFontUnit(void* pUnit);            // dword_8324E870 thunk (FLAG)
extern void  AptFreeRenderingUnit(void* pUnit);       // dword_8324E87C thunk (FLAG)

// EA::Thread primitives the case-7 (button) release path uses: the current thread id
// and the lock guarding the deferred-release queue (unk_8324E728 + its name
// unk_82143270). FLAG: the genuine EA::Thread::Mutex / GetThreadId live in the vendor
// EAThread layer; declared here at the call-site shapes the asm uses so this TU links
// without coupling to the (X360-divergent) vendor mutex global.
namespace EA { namespace Thread {
    extern int GetThreadId();
    struct AptUnresolveMutex;                                          // unk_8324E728 (FLAG)
    extern void Mutex_Lock(void* pMutex, void* pName);                 // EA::Thread::Mutex::Lock (FLAG)
    extern void Mutex_Unlock(void* pMutex);                           // EA::Thread::Mutex::Unlock (FLAG)
} }
extern void* gAptUnresolveMutex;       // unk_8324E728 (FLAG)
extern void* gAptUnresolveMutexName;   // unk_82143270 (FLAG)

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
    // pointer slot the runtime touches is a full 8-byte word (the transcoded form),
    // so the slot is read as a void* here. (The 4-byte file-format truncation is the
    // FLAGged x64 fork already documented for the Fixup walk above.)
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
// methods. Two passes over the serialised blob (the FLAGged in-place 32-bit fork,
// the same one Fixup/Unresolve walk -- console offsets via the Blob* accessors):
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
// FLAG (x64 fork): the resolved-character pointers and the index->pointer rewrites are
// stored into the in-place 32-bit blob slots -- the same 32-bit-slot-can't-hold-a-
// 64-bit-pointer impossibility already FLAGged for the Fixup walk. The char table is
// treated as a host AptCharacter** here (matching Unresolve's slot stores), so the
// pointer width stays correct on x64; the native-struct transcode lands with the
// FixupTranscode rebuild. The ref-count bookkeeping is faithful.
// ===========================================================================
void AptCharacterAnimation_Link(AptCharacterAnimation* pCharAnim, void* pData, void* pDataBlock)
{
    (void)pData;        // console a2 (r4) -- the asm never reads it
    (void)pDataBlock;   // console a3 (r5) -- the asm never reads it

    // ---- pass 1: resolve the import table -------------------------------------
    // The def-base head fields (importCount/table, charCount/table) are read through
    // the serialized 64-bit AptCharacterAnimation struct (charCount@0x18, charTable@
    // 0x20, importCount@0x34, importTable@0x38 -- the same offsets Fixup relocates).
    const int32_t nImports = pCharAnim->mnImportCount;      // [c:0x20] mnImportCount
    if (nImports > 0)
    {
        for (int32_t i = 0; i < nImports; ++i)
        {
            AptImportEntry& rEntry = pCharAnim->mpImportTable[i];   // [c:0x24] stride 0x20
            const int32_t nId = rEntry.mnId;                       // importEntry.mnId

            // FindExport(entry.mpFile, entry.mpClassName): the imported movie's export
            // table resolves the class name to a character.
            AptCharacter* const pResolved = rEntry.mpFile->FindExport(rEntry.mpClassName);

            // Store the resolved character into mpCharacterTable[importId].
            AptCharacter** const pTable = pCharAnim->mpCharacterTable;   // [c:0x10] charTable
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
    const int32_t nChars = pCharAnim->mnCharacterCount;    // [c:0x0C] mnCharacterCount
    if (nChars > 0)
    {
        AptCharacter** const pTable = pCharAnim->mpCharacterTable;   // [c:0x10] charTable
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
                    // Type-8 import character: its two cross-reference INDEX fields
                    // (+0x10, +0x14) name characters by table index; rewrite them to
                    // live pointers via mpCharacterTable.
                    AptCharacter** const pT = pCharAnim->mpCharacterTable;
                    // FLAG (x64 fork): +0x10 and +0x14 are adjacent 4-byte index fields
                    // on the console; an 8-byte host pointer written at +0x10 would clobber
                    // +0x14, so both indices are read BEFORE either pointer is stored (the
                    // console writes 4-byte slots, so ordering was immaterial there).
                    // FLAG (deferred sub-record): the type-8 char-entry cross-reference field
                    // offsets (+0x10/+0x14) are the CONSOLE record layout; their GUIAPT64
                    // widening lands with the char-entry sub-record recovery (this Link pass is
                    // the deferred cross-reference resolution, not the runtime instantiation path).
                    const int32_t nIdx0 = BlobI32(pChar, 0x10);
                    const int32_t nIdx1 = BlobI32(pChar, 0x14);
                    BlobPtrRef(pChar, 0x10) = pT[nIdx0];
                    BlobPtrRef(pChar, 0x14) = pT[nIdx1];
                }
                // (any other type: nothing to resolve)
            }
            else
            {
                // Type-3 character: an ARRAY of cross-reference indices (count @+0x14,
                // array @+0x18); rewrite each entry to a live character pointer.
                // FLAG (deferred sub-record): +0x14/+0x18 are the console char-entry offsets
                // (see the type-8 note); GUIAPT64 widening lands with the sub-record recovery.
                const int32_t nRefs = BlobI32(pChar, 0x14);
                if (nRefs > 0)
                {
                    for (int32_t r = 0; r < nRefs; ++r)
                    {
                        AptCharacter** const pT = pCharAnim->mpCharacterTable;
                        void* pArr = BlobPtr(pChar, 0x18);                 // char[6] (the index array)
                        const int32_t nIdx = BlobI32(pArr, r * 4);
                        *reinterpret_cast<void**>(BlobAt(pArr, r * 4)) = pT[nIdx];
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
// x64 (matches the console `*(a1+48)=0` 4-byte stw): mnResolveScratch is the 4-byte int
// at the serialised def-base +0x30 (the "ms" slot); importCount sits at +0x34 right after
// it, so this clears exactly the scratch word without touching importCount.
//
// FLAG (x64): the pConstFile->mnSecondaryOffset in-place relocate/un-relocate around Fixup
// is a 32-bit-slot += 64-bit-base write-back that x64 cannot perform; it is a temporary state
// only AptMovie::resolve (case-5/9, DEFERRED) reads, and nothing reads it between the relocate
// and un-relocate on this path, so it is omitted (as the prior homing already flagged).
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

    // Clear the resolve-state scratch at the serialised def-base +0x30 (console `*(a1+48)=0`).
    mnResolveScratch = 0;
    AptCharacterAnimation* pResult = Fixup(pBase, pConstFile, pBlock);

    // Un-relocate after Fixup (console `*(a3+28) -= a3`): the ctx table is live only
    // while the parse runs; the unload path re-parses with a null ctx (never reads it).
    if (pnItemStart != nullptr)
        *pnItemStart -= reinterpret_cast<uintptr_t>(pConstFile);

    return pResult;
}

// ===========================================================================
// AptResolveShapeGeometry -- the Step-2 case-1 shape-geometry resolve (the x64 native-8 fork
// of gAptFuncs.pfnLoadRenderingUnit == CgsGui::AptCallbackRender::LoadRenderingUnit @0x5BAEA8).
//
// The console reads the GuiGeometryObject at userData+12 (a 4-byte pointer) then bsearches its
// GuiGeometryFile* table for the file whose id == nCharacterId, returning that GuiGeometryFile*.
// On our native-8 .apt the geometry object is instead at the AptDataHeader field[4] (native-8
// offset +32) as a FILE-RELATIVE OFFSET, and its file pointer table is 8-byte-strided (each
// 4-byte offset followed by 4 pad bytes -- the converter widened the pointer slots). So this:
//   1. resolves the geometry object from the header (offset -> pointer against the resource base);
//   2. linearly scans its 8-byte-strided file table for the file whose id matches nCharacterId,
//      rebasing each file offset against the resource base;
//   3. returns that GuiGeometryFile* (or null if none matches -- a safe, faithful "no unit").
// This reproduces the console's "the shape's geometry file, keyed by character id" result.
//
// pBlock is the AptDataHeader (== f->mpDataBlock, threaded through Resolve). The resource base is
// the load base the host stashed (CgsGui::gAptResourceSpanBase); the header + every geometry
// offset is relative to it. // FLAG (x64 native-8 fork): the file-table 8-byte stride + the
// offset-not-pointer geometry field are the converter's native-8 form, resolved here explicitly.
// ---------------------------------------------------------------------------
// AptFixupGeometryFileNative8 -- the native-8 fork of GuiGeometryFile::FixUp @0x828500E0.
//
// The console FixUp descends {file -> mesh table -> mesh -> vertex table} adding the load delta to
// every 32-bit offset in place. Our native-8 (GUIAPT64 "1:7:8") geometry has WIDENED pointer fields
// (8 bytes) and 8-byte-strided pointer TABLES (a 4-byte offset + 4 pad per entry), so the strides
// differ from the console body -- reproduced here with the native-8 widths (layout verified from the
// loaded bundle). IDEMPOTENT: every slot that still holds a file-relative OFFSET (value < luSize, the
// same offset-vs-pointer discriminator CgsAptAux uses) is promoted to (luBase + offset); a slot that
// already holds a live pointer (value >= luSize) is left alone. So calling it once per shape id -- as
// AptResolveShapeGeometry does -- rebases a shared file exactly once regardless of how many characters
// reference it. // FLAG (x64 native-8 fork): the widened struct + 8-byte table stride are the
// converter's native-8 form; the descend order is faithful to the console FixUp.
static void AptFixupGeometryFileNative8(CgsResource::GuiGeometryFile* pFile,
                                        uintptr_t luBase, uint32_t luSize)
{
    if (pFile == nullptr)
        return;
    // A rebased pointer is >= luSize (out of the [0,size) offset range); an unrebased offset is < size.
    auto lbIsOffset = [luSize](uintptr_t luVal) -> bool { return luVal != 0 && luVal < luSize; };

    // (1) file -> mesh pointer table (file+0x08, 8-byte field; the table is 8-byte-strided).
    if (lbIsOffset(pFile->mppGeometryMeshes))
        pFile->mppGeometryMeshes += luBase;
    if (pFile->mppGeometryMeshes == 0)
        return;

    const uint32_t luMeshCount = pFile->muNumberOfMeshes;
    CgsResource::GuiGeometryPtrTableEntry* lpMeshTbl =
        reinterpret_cast<CgsResource::GuiGeometryPtrTableEntry*>(pFile->mppGeometryMeshes);

    for (uint32_t luMesh = 0; luMesh < luMeshCount && luMesh < 4096u; ++luMesh)
    {
        // (2) mesh-table entry k (8-byte slot): file-relative mesh offset -> mesh pointer.
        //     Promote the whole 8-byte slot to a live pointer written across {lo,pad}.
        uintptr_t luMeshPtr;
        {
            const uint32_t luMeshOff = lpMeshTbl[luMesh].muOffsetOrPtrLo;
            uintptr_t* lpSlot = reinterpret_cast<uintptr_t*>(&lpMeshTbl[luMesh]);
            if (lbIsOffset(*lpSlot))
                *lpSlot = luBase + luMeshOff;   // fold the 4-byte offset into the full 8-byte pointer
            luMeshPtr = *lpSlot;
        }
        if (luMeshPtr == 0)
            continue;
        CgsResource::GuiGeometryMesh* lpMesh =
            reinterpret_cast<CgsResource::GuiGeometryMesh*>(luMeshPtr);

        // (3) mesh -> vertex pointer table (mesh+0x20, 8-byte field).
        if (lbIsOffset(lpMesh->mppVerticies))
            lpMesh->mppVerticies += luBase;
        if (lpMesh->mppVerticies == 0 || lpMesh->muNumberOfVerticies == 0)
            continue;

        // (4) vertex-table entries (8-byte-strided): each holds a file-relative offset to a vertex run.
        //     The render walk reads the FIRST entry as the pointer to the contiguous vertex run, so
        //     rebase every entry (faithful to the console, which rebases all vertex pointers).
        CgsResource::GuiGeometryPtrTableEntry* lpVtxTbl =
            reinterpret_cast<CgsResource::GuiGeometryPtrTableEntry*>(lpMesh->mppVerticies);
        for (uint32_t luV = 0; luV < lpMesh->muNumberOfVerticies && luV < 65536u; ++luV)
        {
            uintptr_t* lpVSlot = reinterpret_cast<uintptr_t*>(&lpVtxTbl[luV]);
            if (lbIsOffset(*lpVSlot))
                *lpVSlot = luBase + lpVtxTbl[luV].muOffsetOrPtrLo;
        }
    }
}

static void* AptResolveShapeGeometry(void* pBlock, int32_t nCharacterId)
{
    if (pBlock == nullptr)
        return nullptr;
    const uintptr_t luBase = CgsGui::gAptResourceSpanBase;
    const uint32_t  luSize = CgsGui::gAptResourceSpanSize;
    if (luBase == 0)
        return nullptr;

    // The AptDataHeader field[4] (native-8 offset +32) holds the GuiGeometryObject's file offset.
    const uint32_t luGeomObjOff =
        static_cast<uint32_t>(*reinterpret_cast<const uint64_t*>(reinterpret_cast<char*>(pBlock) + 32));
    if (luGeomObjOff == 0 || (luSize != 0 && luGeomObjOff + 12u > luSize))
        return nullptr;

    // GuiGeometryObject {u32 numFiles, u32 numTexPages, u32 fileTableOff} (4-byte object header).
    const uint32_t* lpObj = reinterpret_cast<const uint32_t*>(luBase + luGeomObjOff);
    const uint32_t luNumFiles     = lpObj[0];
    const uint32_t luFileTableOff = lpObj[2];
    if (luNumFiles == 0 || luNumFiles > 4096u || luFileTableOff == 0 ||
        (luSize != 0 && luFileTableOff >= luSize))
        return nullptr;

    // The file pointer table is 8-byte-strided (native-8 fork): entry k at fileTableOff + k*8.
    for (uint32_t lu = 0; lu < luNumFiles; ++lu)
    {
        const uint32_t luEntryOff = luFileTableOff + lu * 8u;
        if (luSize != 0 && luEntryOff + 4u > luSize)
            break;
        const uint32_t luFileOff = *reinterpret_cast<const uint32_t*>(luBase + luEntryOff);
        if (luFileOff == 0 || (luSize != 0 && luFileOff + 12u > luSize))
            continue;
        // GuiGeometryFile {u32 id, u32 numMeshes, u32 meshTableOff}.
        const uint32_t luFileId = *reinterpret_cast<const uint32_t*>(luBase + luFileOff);
        if (static_cast<int32_t>(luFileId) == nCharacterId)
        {
            // Rebase the file's internal offset tables (mesh table, mesh vertex tables) offset->pointer
            // in place BEFORE handing it to the render walk -- the console runs GuiGeometryObject::FixUp
            // at load; on our .apt path the geometry is embedded (never FixUp'd), so this native-8 fork
            // does the same descend. Idempotent, so a shared file is rebased once.
            CgsResource::GuiGeometryFile* const lpFile =
                reinterpret_cast<CgsResource::GuiGeometryFile*>(luBase + luFileOff);
            AptFixupGeometryFileNative8(lpFile, luBase, luSize);
            return lpFile;   // the matching GuiGeometryFile* (internal tables now live pointers)
        }
    }
    return nullptr;   // no geometry file for this character id -> no rendering unit (safe)
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
        *reinterpret_cast<char**>(pChar + 0x08) = reinterpret_cast<char**>(pTable)[0];

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
                // FLAG (x64 native-8 fork): our .apt is native 8-byte, so the GuiGeometryObject is at the
                // AptDataHeader's field[4] (native-8 offset +32) as a FILE-RELATIVE OFFSET (not the console
                // +12 pointer), and its file/mesh/vertex tables are 4-byte offsets that must be rebased
                // against the load base. AptResolveShapeGeometry (below) does that rebase once + hands
                // pfnLoadRenderingUnit a shim userData whose +12 is the resolved GuiGeometryObject, exactly
                // reproducing the console store. (The current bundle's geometry is empty -- every mesh has
                // 0 vertices -- so this resolves a real, in-range GuiGeometryFile* that draws nothing; a
                // future bundle with real shape vertices renders unchanged.)
                {
                    void* const lpUnit = AptResolveShapeGeometry(pBlock, i);   // pfnLoadRenderingUnit(pBlock, i)
                    *reinterpret_cast<void**>(pChar + 0x20) = lpUnit;
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
    // under-initialised -> AV" deferral is retired. The host (BrnAptRuntimeBringUp) then SYNC-
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
                CgsApt_ImportProbe(i, pcMovieName,
                                   reinterpret_cast<const char*>(BlobPtr(pEntry, 0x08)),
                                   lHandle.pData);
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
// onClipEvent init streams). Both walk the serialised .apt records by their console
// byte offsets (via Blob*) -- they reach the AptMovie timeline command records and
// the global AptActionInterpreter VM scratch heap, neither of which has a recovered
// runtime struct -- exactly the FLAGged opaque-record handling the Fixup walk uses.
// ===========================================================================
namespace
{
    // The AptCIH "scope" the runStream calls bind: the level the supplied CIH lives
    // at. Shared verbatim by ExportClassDefinitionAssets / ExecuteInitAction (both
    // copies of the same console level-walk: type 0x25 -> the root level, else walk
    // the parent chain [c:+0x1C] until a type 9/15 [c:+8]>>26 record). pA2==0 -> 0.
    void* ResolveAnimationScope(void* pA2)
    {
        if (!pA2)
            return nullptr;

        void* pLevel = pA2;
        if ((BlobI32(pA2, 0x04) & 0x7F) == 0x25)        // clrlwi r11,r11,25; cmpwi 0x25
        {
            pLevel = AptGetAnimationAtLevel(0);
        }
        else
        {
            // i = *(pA2+0x20); loop: t = *(i+8) >> 26 (arithmetic); 9 or 15 -> stop,
            // else pLevel = *(pLevel+0x1C), i = *(pLevel+0x20).
            void* pNode = BlobPtr(pA2, 0x20);
            for (;;)
            {
                int32_t nTag = BlobI32(pNode, 0x08) >> 26;   // srawi ...,0x1A
                if (nTag == 9 || nTag == 15)
                    break;
                pLevel = BlobPtr(pLevel, 0x1C);
                pNode  = BlobPtr(pLevel, 0x20);
            }
        }
        return BlobPtr(pLevel, 0x20);                       // v14 = *(pLevel+0x20)
    }
}

// ExportClassDefinitionAssets @0x82AEA8C8 -- run each pending "__Packages." class-
// definition frame-label exactly once (then mark it consumed by negating its
// indicator). this[c:0x28]/[c:0x2C] is the label/indicator list (an AptInitEntry list:
// {name@+0, indicator@8}); this[c:+4] is the timeline-command table the matching action
// stream lives in. The def-base head fields are read through the serialized 64-bit struct
// (initCount@0x40, initList@0x48, framesOffset@0x08). FAITHFUL to the asm; the VM globals
// are FLAGged externs.
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
            const int32_t nIndicator = rEntry.mnIndicator;
            void* pCmdHdr = mpFrames;                        // [c:0x04] framesOffset (the command table)
            const int32_t nCmds = BlobI32(pCmdHdr, 0x00);    // *( *(this+4) )
            if (nCmds > 0)
            {
                void** pCmdTable = reinterpret_cast<void**>(BlobPtr(pCmdHdr, 0x04));   // *(*(this+4)+4)
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
                    void* pStream = BlobPtr(pCmdTable[iCmd], 0x08);   // cmd+8 = the stream
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

    void* pCmdHdr = mpFrames;                            // [c:0x04] framesOffset (command table)
    const int32_t nCmds = BlobI32(pCmdHdr, 0x00);        // *( *(this+4) )
    if (nCmds <= 0)
        return result;

    void** pCmdTable = reinterpret_cast<void**>(BlobPtr(pCmdHdr, 0x04));   // *(*(this+4)+4)
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

    // stream = *(*(4*iCmd + *(*(this+4)+4)) + 8) -- re-read the table (the asm reloads
    // *(this+4) after the call) and index the matched command's +8 stream.
    void** pCmdTable2 = reinterpret_cast<void**>(BlobPtr(mpFrames, 0x04));   // [c:0x04] framesOffset
    void* pStream = BlobPtr(pCmdTable2[iCmd], 0x08);
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
// 0x20). FLAG (deferred sub-record): the deeper import->AptFile->embedded-movie chase keeps
// the console record offsets pending the AptFile/embedded-movie sub-record recovery -- this
// init-action VM path is deferred (never runtime-reached in the current bring-up).
void* AptCharacterAnimation::ExecuteInitActions(void* pA2, int32_t nId)
{
    void* result = nullptr;
    int32_t nResolvedId = nId;                            // v5 (r27)
    void* pBucketOwner  = this;                           // v6 (r28)

    // Locate nId in mpImportTable (the serialized 64-bit import entry: id at +0x10).
    int32_t iSlot = -1;
    const int32_t nImports = mnImportCount;               // [c:0x20] importCount
    // v9 default = *(*(*(pA2+0x20)+4)+4) + 0x10 (the supplied CIH's init bucket).
    void* pBucket = BlobAt(BlobPtr(BlobPtr(BlobPtr(pA2, 0x20), 0x04), 0x04), 0x10);
    if (nImports > 0)
    {
        for (int32_t i = 0; i < nImports; ++i)
        {
            if (mpImportTable[i].mnId == nId)             // [c:0x24] importTable, id@+8 console
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
            // pBucketOwner = imported movie root; pBucket = its init list for the id.
            AptImportEntry& rImportEntry = mpImportTable[iSlot];   // [c:0x24] importTable (stride 0x20)
            // FLAG (deferred sub-record): entry.mpFile (the AptFile @+0x18 GUIAPT64) is chased
            // into its embedded movie (+0x14) and then root+0x10/+0x20 -- these AptFile/embedded-
            // movie offsets stay the console record layout pending their sub-record recovery.
            void* pImportedRoot = BlobPtr(rImportEntry.mpFile, 0x14);          // *( entry.mpFile+0x14 )
            pBucketOwner = BlobAt(pImportedRoot, 0x10);                        // root+0x10 (addi r28,r11,0x10)
            void* pInitTable = BlobPtr(pBucketOwner, 0x10);                    // *(pBucketOwner+0x10) == *(root+0x20)
            pBucket = BlobAt(BlobPtr(pInitTable, nResolvedId * 4), 0x10);
        }
    }

    // Run every pending type-3 init command in the bucket.
    // The command list is one indirection deeper: outer guard *(pBucket) > 0; then
    // header = *(pBucket+4), command COUNT = *(header), command ARRAY = *(header+4),
    // command = array[iCmd] (matching the ExecuteInitAction header read above).
    if (BlobI32(pBucket, 0x00) > 0)
    {
        void* pHdr    = BlobPtr(pBucket, 0x04);                          // *(pBucket+4)
        void** pCmds  = reinterpret_cast<void**>(BlobPtr(pHdr, 0x04));   // *(header+4) = command array
        int32_t nCmds = BlobI32(pHdr, 0x00);                            // *(header)   = command count
        if (nCmds > 0)
        {
            int32_t iCmd = 0;
            do
            {
                void* pCmd = pCmds[iCmd];
                if (BlobI32(pCmd, 0x00) == 3 && BlobI32(pCmd, 0x0C) != -1)
                    result = reinterpret_cast<AptCharacterAnimation*>(pBucketOwner)->ExecuteInitAction(pA2, BlobI32(pCmd, 0x0C));
                pHdr  = BlobPtr(pBucket, 0x04);                          // reload (asm re-reads *(pBucket+4))
                pCmds = reinterpret_cast<void**>(BlobPtr(pHdr, 0x04));
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
                *reinterpret_cast<uint16_t*>(pc + 0x12) &= ~0x8000u;
                void** const ppUnit = reinterpret_cast<void**>(pc + 0x30);
                AptFreeRenderingUnit(*ppUnit);        // qword_14147AA78 host hook
                *ppUnit = nullptr;
                result = nullptr;
                break;
            }
            case 2:   // Text: un-relocate the text (+0x50) + variable (+0x58).
                Unreloc(*reinterpret_cast<void**>(pc + 0x50));
                Unreloc(*reinterpret_cast<void**>(pc + 0x58));
                break;
            case 3:   // Font: un-relocate the name (+0x20); glyph pointers
            {         // (+0x30, count +0x28) back to indices; then the array ptr.
                Unreloc(*reinterpret_cast<void**>(pc + 0x20));
                const int32_t nGlyphs = *reinterpret_cast<int32_t*>(pc + 0x28);
                void** const ppGlyphs = *reinterpret_cast<void***>(pc + 0x30);
                for (int32_t g = 0; g < nGlyphs; ++g)
                    *reinterpret_cast<int64_t*>(&ppGlyphs[g]) =
                        IndexOfChar(ppGlyphs[g]);
                Unreloc(*reinterpret_cast<void**>(pc + 0x30));
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
                uint16_t& rFlags = *reinterpret_cast<uint16_t*>(pc + 0x12);
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
                const int32_t nRecs = *reinterpret_cast<int32_t*>(pc + 0x48);
                char* const pRecs = *reinterpret_cast<char**>(pc + 0x50);
                for (int32_t r = 0; r < nRecs; ++r)
                    Unreloc(*reinterpret_cast<void**>(pRecs + r * 0x40 + 0x38));
                Unreloc(*reinterpret_cast<void**>(pc + 0x50));
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
// FLAG (GUIAPT64 transcode pending): like Unresolve, this addresses the def-base + its
// tables at the CONSOLE record offsets/strides (charCount@0x0C / charTable@0x10 at a
// 4-byte slot stride, initCount@0x28 / initList@0x2C at stride 8). It runs only on the
// AptCharacterAnimationInst teardown (deferred with the instantiation), so the console
// form is kept; the GUIAPT64 offset/stride transcode lands with the un-defer.
void AptCharacterAnimation::ResetInitIndicators()
{
    // ---- pass 1: the type-5/9 characters' embedded label tables -------------------
    const int32_t nChars = BlobI32(this, 0x0C);          // *(this+0x0C)
    if (nChars > 0)
    {
        for (int32_t iByte = 0, n = nChars; n != 0; iByte += 4, --n)
        {
            void* pChar = *reinterpret_cast<void**>(BlobAt(reinterpret_cast<char*>(BlobPtr(this, 0x10)), iByte));
            if (!pChar)
                continue;
            const int32_t eType = BlobI32(pChar, 0x00);
            if (eType != 5 && eType != 9)
                continue;

            // movie = char+0x10; mpFrames = movie[1]=*(char+0x14); table = mpFrames[1].
            void* pMovie = BlobAt(pChar, 0x10);          // r11 += 0x10
            void* pFrames = BlobPtr(pMovie, 0x04);       // *(movie+4)
            if (!pFrames)
                continue;
            void** pTable = reinterpret_cast<void**>(BlobPtr(pFrames, 0x04));   // *(mpFrames+4)
            if (!pTable)
                continue;
            const int32_t nEntries = BlobI32(pFrames, 0x00);   // *(mpFrames)
            if (nEntries <= 0)
                continue;

            for (int32_t i = 0; i < nEntries; ++i)
            {
                void* pEntry = pTable[i];
                if (pEntry && BlobI32(pEntry, 0x00) == 8 && BlobI32(pEntry, 0x04) < 0)
                {
                    BlobI32Ref(pEntry, 0x04) = -BlobI32(pEntry, 0x04);
                    break;
                }
            }
        }
    }

    // ---- pass 2: the init-indicator list [c:0x28]/[c:0x2C] ------------------------
    const int32_t nInit = BlobI32(this, 0x28);
    if (nInit <= 0)
        return;
    for (int32_t iByte = 0, n = nInit; n != 0; iByte += 8, --n)
    {
        void* pEntry = BlobAt(BlobPtr(this, 0x2C), iByte);
        if (!pEntry)
            continue;
        int32_t v = BlobI32(pEntry, 0x04);
        if (v < 0)
            BlobI32Ref(pEntry, 0x04) = -v;
    }
}
