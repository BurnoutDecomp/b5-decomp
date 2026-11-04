#include "SDKs/EATech/include/Apt/AptPseudoData.h"
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

#include "types.hpp"
#include <cstring>   // strstr (ExportClassDefinitionAssets' __Packages label scan)

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
            AptFile* oldFile = character->mpAnimationFile;
            character->mpAnimationFile = entry.mpFile;
            if (entry.mpFile != 0)
                AptSharedPtrIncRef(entry.mpFile);
            if (oldFile != 0 && AptSharedPtrDecRef(oldFile) == 0)
                AptSharedPtrDelete(oldFile);
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

// Native frame-0 init actions. ARTIST 0x82AEA8C8 / 0x82AEE2D8 / 0x82AF4340.
namespace
{
    AptCharacterInst* ResolveAnimationScope(void* pCIH)
    {
        return pCIH ? static_cast<AptCIH*>(pCIH)->GetRootAnimation()->GetCharacterInst() : 0;
    }
}

void AptCharacterAnimation::ExportClassDefinitionAssets(void* pCIH)
{
    for (int32_t i = 0; i < mnInitListCount; ++i)
    {
        AptInitEntry& entry = mpInitList[i];
        if (entry.mnIndicator < 0) break;
        if (::strstr(static_cast<const char*>(entry.mpInitObject), "__Packages."))
        {
            AptMovieFrame* first = static_cast<AptMovieFrame*>(mpFrames);
            for (int32_t k = 0; k < first->mnCommandCount; ++k)
            {
                AptTimelineInitAction* command = static_cast<AptTimelineInitAction*>(first->mpCommands[k]);
                if (command->mnTag != 8 || command->mnIndicator != entry.mnIndicator) continue;
                void* saved = AptScriptFunctionBase::PushStaticData();
                gAptActionInterpreter.runStream(command->mpStream, static_cast<AptCIH*>(pCIH), -1, ResolveAnimationScope(pCIH));
                gAptActionInterpreter.CleanupAfterExecution(saved);
                break;
            }
            entry.mnIndicator = -entry.mnIndicator;
        }
    }
}

void AptCharacterAnimation::ExecuteInitAction(void* pCIH, int32_t nId)
{
    AptMovieFrame* first = static_cast<AptMovieFrame*>(mpFrames);
    for (int32_t i = 0; i < first->mnCommandCount; ++i)
    {
        AptTimelineInitAction* command = static_cast<AptTimelineInitAction*>(first->mpCommands[i]);
        if (command->mnTag != 8 || command->mnIndicator != nId) continue;
        ExportClassDefinitionAssets(pCIH);
        void* saved = AptScriptFunctionBase::PushStaticData();
        first = static_cast<AptMovieFrame*>(mpFrames);
        AptTimelineInitAction* current = static_cast<AptTimelineInitAction*>(first->mpCommands[i]);
        gAptActionInterpreter.runStream(current->mpStream, static_cast<AptCIH*>(pCIH), -1, ResolveAnimationScope(pCIH));
        gAptActionInterpreter.CleanupAfterExecution(saved);
        command->mnIndicator = -command->mnIndicator;
        return;
    }
}

void AptCharacterAnimation::ExecuteInitActions(void* pCIH, int32_t nId)
{
    int32_t resolvedId = nId;
    AptCharacterAnimation* owner = this;
    AptCharacter* currentCharacter = static_cast<AptCIH*>(pCIH)->GetCharacterInst()->GetRenderItem()->mpCharacter;
    AptMovie* movie = reinterpret_cast<AptMovie*>(AptGetMovieCharacterAnimation(currentCharacter));
    const int32_t importIndex = IsImport(nId);
    if (importIndex != -1)
    {
        resolvedId = GetIDFromImportFile(importIndex);
        if (resolvedId != -1)
        {
            AptCharacter* importedRoot = static_cast<AptCharacter*>(mpImportTable[importIndex].mpFile->mpData);
            owner = AptGetMovieCharacterAnimation(importedRoot);
            movie = reinterpret_cast<AptMovie*>(AptGetMovieCharacterAnimation(owner->mpCharacterTable[resolvedId]));
        }
    }
    if (movie->mnFrameCount > 0)
    {
        for (int32_t i = 0; i < movie->mpFrames[0].mnCommandCount; ++i)
        {
            AptTimelineCommand* command = static_cast<AptTimelineCommand*>(movie->mpFrames[0].mpCommands[i]);
            if (command->mnTag == 3)
            {
                AptPlaceObjectBody_t* body = reinterpret_cast<AptPlaceObjectInfo_t*>(command)->GetBody();
                if (body->mi32CharacterId != -1) owner->ExecuteInitAction(pCIH, body->mi32CharacterId);
            }
        }
    }
    if (resolvedId != -1) owner->ExecuteInitAction(pCIH, resolvedId);
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
        AptFilePtr empty;
        *slot = empty;
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
