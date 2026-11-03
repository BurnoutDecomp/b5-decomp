#include "SDKs/EATech/include/Apt/AptCharacterMorph.h"
#include "SDKs/EATech/include/Apt/AptCharacterShape.h"
#include "SDKs/EATech/include/Apt/AptCharacterDynamicText.h"
#include "SDKs/EATech/include/Apt/AptCharacterFont.h"
#include "SDKs/EATech/include/Apt/AptCharacterImage.h"
#include "SDKs/EATech/include/Apt/AptCharacterStaticText.h"
#include "SDKs/EATech/Apt/AptInit.h"
#include "eathread/eathread.h"
#include "eathread/eathread_mutex.h"
extern EA::Thread::ThreadId gAptSimThreadId;
extern int gAptDeferredReleaseCount;
extern void* gAptDeferredReleaseQueue;
#include "SDKs/EATech/include/Apt/AptCharacterAnimationInst.h" // native embedded movie accessor
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

#include "types.hpp"
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
namespace CgsGui {   }

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

// Unresolve routes released shape rendering units and image textures back to the
// host through gAptFuncs.pfnFreeRenderingUnit / pfnFreeTexture. The console's
// off-thread deferral apparatus (VM thread id
// dword_8324E500, the deferred-release queue off_8324E2C8/dword_8324E508 under
// unk_8324E728) is owned by AptGlobals.cpp/AptInit.cpp/AptRenderWalk.cpp; the XB1
// Unresolve twin (sub_140845200) frees directly on the single VM thread here, so
// this TU no longer declares it.

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

// ARTIST Link 0x82AFE158: resolve imports and native character indices.
void AptCharacterAnimation::Link(void* pData, void* pDataBlock)
{
    (void)pData;
    (void)pDataBlock;
    // ARTIST 0x82AFE158, recovered from the original image: sixteen-byte
    // imports and four-byte character and glyph slots.
    for (int32_t i = 0; i < mnImportCount; ++i)
    {
        AptImportEntry& entry = mpImportTable[i];
        mpCharacterTable[entry.mnId] = entry.mpFile->FindExport(entry.mpClassName);
        AptCharacter* character = mpCharacterTable[entry.mnId];
        if (character)
        {
            *reinterpret_cast<AptFilePtr*>(&character->mpAnimationFile) = *reinterpret_cast<AptFilePtr*>(&entry.mpFile);
            mpCharacterTable[entry.mnId]->AddCharacterReference();
        }
    }
    for (int32_t i = 0; i < mnCharacterCount; ++i)
    {
        AptCharacter* character = mpCharacterTable[i];
        if (!character) continue;
        if (character->mnType == 8)
        {
            AptCharacterMorph* morph = static_cast<AptCharacterMorph*>(character);
            morph->mpStartCharacter = mpCharacterTable[reinterpret_cast<intptr_t>(morph->mpStartCharacter)];
            morph->mpEndCharacter = mpCharacterTable[reinterpret_cast<intptr_t>(morph->mpEndCharacter)];
        }
        else if (character->mnType == 3)
        {
            AptCharacterFont* font = static_cast<AptCharacterFont*>(character);
            for (int32_t g = 0; g < font->mnGlyphCount; ++g)
                font->mpGlyphs[g] = mpCharacterTable[reinterpret_cast<intptr_t>(font->mpGlyphs[g])];
        }
    }
}

// ARTIST Resolve 0x82AFF5E0 relocates the constant table around Fixup.
namespace
{
    template<class T> void AptRelocate(T*& p, intptr_t base)
    { if (p) p = reinterpret_cast<T*>(reinterpret_cast<intptr_t>(p) + base); }
}

void AptCharacterAnimation::Resolve(void* pBase, AptConstFile* pConstFile, void* pBlock)
{
    if (pConstFile->mnSecondaryOffset)
        pConstFile->mnSecondaryOffset += reinterpret_cast<uintptr_t>(pConstFile);
    mnParsedValueCount = 0;
    Fixup(pBase, pConstFile, pBlock);
    if (pConstFile->mnSecondaryOffset)
        pConstFile->mnSecondaryOffset -= reinterpret_cast<uintptr_t>(pConstFile);
}

// ARTIST 0x82AFF268: relocate native 32-bit character and import records.
void AptCharacterAnimation::Fixup(void* pBase, AptConstFile* pConstFile, void* pBlock)
{
    const intptr_t base = reinterpret_cast<intptr_t>(pBase);
    AptRelocate(mpCharacterTable, base);
    AptRelocate(mpInitList, base);
    for (int32_t i = 0; i < mnInitListCount; ++i)
        AptRelocate(mpInitList[i].mpInitObject, base);
    for (int32_t i = 0; i < mnCharacterCount; ++i)
    {
        AptRelocate(mpCharacterTable[i], base);
        AptCharacter* p = mpCharacterTable[i];
        if (!p) continue;
        p->mpFixupLink = mpCharacterTable[0];
        switch (p->mnType)
        {
        case 1:
            static_cast<AptCharacterShape*>(p)->mRenderingUnit = gAptFuncs.pfnLoadRenderingUnit(pBlock, i);
            break;
        case 2:
        {
            AptCharacterDynamicText* text = static_cast<AptCharacterDynamicText*>(p);
            AptRelocate(text->mpDefaultText, base);
            AptRelocate(text->mpVariableName, base);
            break;
        }
        case 3:
        {
            AptCharacterFont* font = static_cast<AptCharacterFont*>(p);
            AptRelocate(font->mpName, base);
            AptRelocate(font->mpGlyphs, base);
            break;
        }
        case 5: case 9:
            reinterpret_cast<AptMovie*>(AptGetMovieCharacterAnimation(p))->resolve(static_cast<int>(base), pConstFile, &mnParsedValueCount);
            break;
        case 10:
        {
            AptCharacterStaticText* text = static_cast<AptCharacterStaticText*>(p);
            AptRelocate(text->mpParagraphs, base);
            for (int32_t n = 0; n < text->miNumParagraphs; ++n)
                AptRelocate(text->mpParagraphs[n].mpGlyphs, base);
            break;
        }
        default: break;
        }
        p->SetupCharacter();
    }
    AptRelocate(mpImportTable, base);
    for (int32_t i = 0; i < mnImportCount; ++i)
    {
        AptImportEntry& entry = mpImportTable[i];
        AptRelocate(entry.mpImportFileName, base);
        AptRelocate(entry.mpClassName, base);
        EAStringC name(entry.mpImportFileName);
        AptFilePtr loaded = gpAptTarget->mpLoader->Load(name);
        *reinterpret_cast<AptFilePtr*>(&entry.mpFile) = loaded;
        AptFilePtr::Dispose(loaded.pData);
    }
}



// GetIDFromImportFile @0x82ADE998 -- given an import-table slot index, resolve the
// id of the character that the import's class name names in the imported movie's
// export table. Mirrors AptFile::FindExport's inline strcmp; returns -1 if the
// imported movie has no exports or the class name is absent.
int32_t AptCharacterAnimation::GetIDFromImportFile(int32_t nImportIndex)
{
    const AptImportEntry& import = mpImportTable[nImportIndex];
    const AptCharacterAnimation* pMovie = AptGetMovieCharacterAnimation(static_cast<AptCharacter*>(import.mpFile->mpData));

    const int32_t nExportCount = pMovie->mnInitListCount;
    if (nExportCount <= 0)
        return -1;

    const char* pClassName = import.mpClassName;
    for (int32_t iExport = 0; iExport < nExportCount; ++iExport)
    {
        // Inline strcmp: walk until the SEARCH key (class name) terminates,
        // breaking early on any mismatch. Bytes are unsigned (asm lbz).
        const char* pSearch = pClassName;
        const char* pExport = static_cast<const char*>(pMovie->mpInitList[iExport].mpInitObject);
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
            return pMovie->mpInitList[iExport].mnIndicator;
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
            return 0;
        AptCIH* const pLevel = static_cast<AptCIH*>(pA2)->GetRootAnimation();
        return pLevel ? pLevel->GetCharacterInst() : 0;   // v14 = *(pLevel+0x20)
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
    void* result = 0;

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
    void* result = 0;

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
    void* result = 0;
    int32_t nResolvedId = nId;                            // v5 (r27)
    void* pBucketOwner  = this;                           // v6 (r28)

    // Locate nId in mpImportTable (the serialized 64-bit import entry: id at +0x10).
    int32_t iSlot = -1;
    const int32_t nImports = mnImportCount;
    // Default bucket = the supplied CIH's clip movie (console chase *(*(*(pA2+0x20)+4)+4)+0x10
    // == CIH -> charInst -> renderItem -> character -> embedded movie). NATIVE-8: the typed
    // chain with the widened body offset (KU_AptEmbeddedMovieOff).
    void* pBucket = 0;
    {
        AptCIH* pCIH = reinterpret_cast<AptCIH*>(pA2);
        AptCharacterInst* pCI = pCIH ? pCIH->GetCharacterInst() : 0;
        AptRenderItem* pRI = pCI ? pCI->mpRenderItem : 0;
        AptCharacter* pChar = pRI ? pRI->mpCharacter : 0;
        if (pChar != 0)
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
            void* pImportedRoot = pFile ? pFile->mpData : 0;             // the root char header
            if (pImportedRoot != 0)
            {
                pBucketOwner = reinterpret_cast<char*>(pImportedRoot) + 0x20;  // def base (native-8)
                AptCharacterAnimation* pOwner =
                    reinterpret_cast<AptCharacterAnimation*>(pBucketOwner);
                if (nResolvedId >= 0 && nResolvedId < pOwner->mnCharacterCount &&
                    pOwner->mpCharacterTable != 0 &&
                    pOwner->mpCharacterTable[nResolvedId] != 0)
                {
                    pBucket = reinterpret_cast<char*>(pOwner->mpCharacterTable[nResolvedId]) + 0x20;
                }
            }
        }
    }
    if (pBucket == 0)
        return result;

    // Run every pending type-3 (PLACE) init command in the bucket's frame 0.
    // NATIVE-8: movie {fc@0, frames@8}; frame record {i32 count@0, pad, u64 cmds@8};
    // PLACE body at align8(cmd+4)=cmd+8 with charId @body+8 (== cmd+0x10).
    if (BlobI32(pBucket, 0x00) > 0)
    {
        void* pHdr    = BlobPtr(pBucket, 0x08);                          // frames -> frame[0]
        void** pCmds  = reinterpret_cast<void**>(BlobPtr(pHdr, 0x08));   // frame[0].cmds
        int32_t nCmds = BlobI32(pHdr, 0x00);                            // frame[0].count
        if (pHdr != 0 && pCmds != 0 && nCmds > 0)
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

// ARTIST 0x82AF77B0: inverse relocation of the native 32-bit records.
namespace
{
    template<class T> void AptUnrelocate(T*& p, intptr_t base)
    {
        if (p) p = reinterpret_cast<T*>(reinterpret_cast<intptr_t>(p) - base);
    }
}

void AptCharacterAnimation::Unresolve(intptr_t nBase)
{
    mnParsedValueCount = 0;
    for (int32_t i = 0; i < mnCharacterCount; ++i)
    {
        AptCharacter* p = mpCharacterTable[i];
        if (!p) continue;
        if (IsImport(i) == -1)
        {
            if (p->mnType == 8)
            {
                AptCharacterMorph* morph = static_cast<AptCharacterMorph*>(p);
                morph->mpStartCharacter = reinterpret_cast<AptCharacter*>(UnmapCharacter(morph->mpStartCharacter));
                morph->mpEndCharacter = reinterpret_cast<AptCharacter*>(UnmapCharacter(morph->mpEndCharacter));
            }
        }
        else p->ReleaseCharacterReference();
    }
    for (int32_t i = 0; i < mnCharacterCount; ++i)
    {
        AptCharacter* p = mpCharacterTable[i];
        if (!p || IsImport(i) != -1) continue;
        switch (p->mnType)
        {
        case 1:
        {
            AptCharacterShape* shape = static_cast<AptCharacterShape*>(p);
            p->mnRefAndFlags &= ~0x8000u;
            gAptFuncs.pfnFreeRenderingUnit(shape->mRenderingUnit);
            shape->mRenderingUnit = reinterpret_cast<AptAssetRenderingUnit>(i);
            break;
        }
        case 2:
        {
            AptCharacterDynamicText* text = static_cast<AptCharacterDynamicText*>(p);
            AptUnrelocate(text->mpDefaultText, nBase);
            AptUnrelocate(text->mpVariableName, nBase);
            break;
        }
        case 3:
        {
            AptCharacterFont* font = static_cast<AptCharacterFont*>(p);
            AptUnrelocate(font->mpName, nBase);
            for (int32_t g = 0; g < font->mnGlyphCount; ++g)
                font->mpGlyphs[g] = reinterpret_cast<AptCharacter*>(UnmapCharacter(font->mpGlyphs[g]));
            AptUnrelocate(font->mpGlyphs, nBase);
            break;
        }
        case 5: case 9:
            reinterpret_cast<AptMovie*>(AptGetMovieCharacterAnimation(p))->unresolve(static_cast<int>(nBase), &mnParsedValueCount);
            break;
        case 7:
        {
            AptCharacterImage* image = static_cast<AptCharacterImage*>(p);
            if ((p->mnRefAndFlags & 0x8000u) != 0)
            {
                if (gAptSimThreadId == EA::Thread::GetThreadId())
                {
                    AptUnresolveMutex().Lock();
                    static_cast<void**>(gAptDeferredReleaseQueue)[gAptDeferredReleaseCount++] = image->mTexture;
                    AptUnresolveMutex().Unlock();
                }
                else gAptFuncs.pfnFreeTexture(image->mTexture);
            }
            p->mnRefAndFlags &= ~0xC000u;
            image->mTexture = reinterpret_cast<AptAssetTexture>(i);
            break;
        }
        case 10:
        {
            AptCharacterStaticText* text = static_cast<AptCharacterStaticText*>(p);
            for (int32_t r = 0; r < text->miNumParagraphs; ++r)
                AptUnrelocate(text->mpParagraphs[r].mpGlyphs, nBase);
            AptUnrelocate(text->mpParagraphs, nBase);
            break;
        }
        default: break;
        }
    }
    for (int32_t i = 0; i < mnImportCount; ++i)
    {
        AptFilePtr* slot = reinterpret_cast<AptFilePtr*>(&mpImportTable[i].mpFile);
        AptFilePtr empty = { 0 };
        *slot = empty;
        AptFilePtr::Dispose(empty.pData);
        mpCharacterTable[mpImportTable[i].mnId] = 0;
    }
    for (int32_t i = 0; i < mnCharacterCount; ++i)
        if (mpCharacterTable[i])
        {
            mpCharacterTable[i]->mpFixupLink = reinterpret_cast<AptCharacter*>(0x09876543u);
            AptUnrelocate(mpCharacterTable[i], nBase);
        }
    AptUnrelocate(mpCharacterTable, nBase);
    for (int32_t i = 0; i < mnImportCount; ++i)
    {
        AptUnrelocate(mpImportTable[i].mpImportFileName, nBase);
        AptUnrelocate(mpImportTable[i].mpClassName, nBase);
    }
    for (int32_t i = 0; i < mnInitListCount; ++i)
    {
        AptUnrelocate(mpInitList[i].mpInitObject, nBase);
        if (mpInitList[i].mnIndicator < 0) mpInitList[i].mnIndicator = -mpInitList[i].mnIndicator;
    }
    AptUnrelocate(mpImportTable, nBase);
    AptUnrelocate(mpInitList, nBase);
    mnParsedValueCount = 0;
}

// ARTIST 0x82AD92A8: restore each movie's first pending init action,
// then the exported init-list indicators.
void AptCharacterAnimation::ResetInitIndicators()
{
    struct InitIndicatorCommand { int32_t mnTag; int32_t mnIndicator; };
    for (int32_t i = 0; i < mnCharacterCount; ++i)
    {
        AptCharacter* character = mpCharacterTable[i];
        if (!character || (character->mnType != 5 && character->mnType != 9)) continue;
        AptMovie* movie = reinterpret_cast<AptMovie*>(AptGetMovieCharacterAnimation(character));
        if (!movie->mpFrames || !movie->mpFrames[0].mpCommands) continue;
        AptMovieFrame& first = movie->mpFrames[0];
        for (int32_t k = 0; k < first.mnCommandCount; ++k)
        {
            InitIndicatorCommand* command = static_cast<InitIndicatorCommand*>(first.mpCommands[k]);
            if (command && command->mnTag == 8 && command->mnIndicator < 0)
            {
                command->mnIndicator = -command->mnIndicator;
                break;
            }
        }
    }
    for (int32_t i = 0; i < mnInitListCount; ++i)
        if (mpInitList[i].mnIndicator < 0)
            mpInitList[i].mnIndicator = -mpInitList[i].mnIndicator;
}
