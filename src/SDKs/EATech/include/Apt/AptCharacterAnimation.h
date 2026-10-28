#pragma once

// ARTIST's in-place 32-bit movie definition follows the 16-byte AptCharacter.
// The final name/id list serves both library exports and class-init indicators.
#include <cstddef>
#include "types.hpp"
#include "SDKs/EATech/include/Apt/AptCharacter.h"
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"

struct AptFile;

struct AptImportEntry
{
    const char* mpImportFileName;   // +0x00
    const char* mpClassName;        // +0x04
    int32_t mnId;                  // +0x08
    AptFile* mpFile;               // +0x0C
};

struct AptInitEntry
{
    void* mpInitObject;            // +0x00 name
    int32_t mnIndicator;           // +0x04 character id, negated when consumed
};

struct AptCharacterAnimation
{
    int32_t mnFrameCount;          // +0x00
    void* mpFrames;                // +0x04
    void* mpUnk10;                 // +0x08 label-hash slot
    int32_t mnCharacterCount;      // +0x0C
    AptCharacter** mpCharacterTable; // +0x10
    int32_t mnScreenW;             // +0x14
    int32_t mnScreenH;             // +0x18
    int32_t mnMillisecondsPerFrame; // +0x1C
    int32_t mnImportCount;         // +0x20
    AptImportEntry* mpImportTable; // +0x24
    int32_t mnInitListCount;       // +0x28
    AptInitEntry* mpInitList;      // +0x2C
    int32_t mnParsedValueCount;    // +0x30

    // @0x7E3738 -- index of the import whose id == nId, or -1.
    int32_t IsImport(int32_t nId);

    // @0x7E36CC -- index of pCharacter in the character table, or -1.
    int32_t UnmapCharacter(AptCharacter* pCharacter);

    // @0x810DEC -- drop a character reference on every table entry (except the
    // type-9 imports, which the importing movie still owns).
    void ClearCharacterList() const;

    // @0x80E790 -- add a character reference to every table entry, binding each
    // un-bound character to the given animation file.
    void IncCharacterList(AptFilePtr filePtr) const;

    // @0x7E37A4 (X360 @0x82AD92A8) -- restore the init-indicator list: walk the
    // init indicators and un-flag (restore the sign of) the ones that were marked
    // pending. Called when an animation instance is torn down before its init
    // actions have all run (AptCharacterAnimationInst::~AptCharacterAnimationInst).
    // The X360 body returns `this` (a chaining return the callers ignore); modelled
    // as `void` to match the family's list-walk methods. BODY in its own TU
    // (class:AptCharacterAnimation); declared here so callers compile.
    void ResetInitIndicators();

    // @0x82ADE998 -- resolve the character id that import-table slot nImportIndex's
    // class name names in the imported movie's export table (inline strcmp, like
    // AptFile::FindExport); returns -1 when the imported movie has no exports or the
    // name is absent. Body in AptCharacterAnimation.cpp.
    int32_t GetIDFromImportFile(int32_t nImportIndex);

    // --- init-action execution (the __Packages bootstrap) ------------------
    // @0x82AEA8C8 -- ExportClassDefinitionAssets: for each frame-label whose name
    // begins with "__Packages." (and is still pending), run the matching frame's
    // ActionScript class-definition stream once, then mark the label consumed.
    // pA2 is the AptCIH-at-level (r4); the asm walks the serialised label table.
    // Returns the last sub-result (callers ignore it). Body in the .cpp.
    void* ExportClassDefinitionAssets(void* pA2);

    // @0x82AEE2D8 -- ExecuteInitAction: find the type-8 import whose id == nId,
    // run its class-definition assets, then run that init slot's action stream and
    // mark it consumed (negate the id). pA2 is the AptCIH-at-level (r4); nId is r5.
    // Body in the .cpp.
    void* ExecuteInitAction(void* pA2, int32_t nId);

    // @0x82AF4340 -- ExecuteInitActions: locate the init-list bucket for character
    // nId (directly, or via GetIDFromImportFile through the import chain), run every
    // pending type-3 init command in it, then run the import-resolved init action.
    // pA2 is the AptCIH (r4); nId is r5. Body in the .cpp.
    void* ExecuteInitActions(void* pA2, int32_t nId);

    // @0x82AF77B0 -- Unresolve: the inverse of Fixup. Releases the per-character
    // animation refs, un-relocates every record's pointer slot (subtracting the load
    // base), tears down the imports, and stamps the freed characters. nBase is the
    // load base (r4). Returns the last sub-result. Body in the .cpp.
    // nBase is the 32-bit load address.
    void* Unresolve(intptr_t nBase);

    // @0x82AD92A8 (PS3 @0x7E37A4) -- restore the init-indicator list (declared above
    // as ResetInitIndicators); body now in the .cpp.

    // --- .apt load resolution (D) ------------------------------------------
    // @0x80EEC4 -- resolve this (serialised) movie root against the load base:
    // clear the resolve scratch + run the recursive Fixup. Returns the root.
    AptCharacterAnimation* Resolve(void* pBase, struct AptConstFile* pConstFile, void* pBlock);
    // @0x82AFF268 (PS3 @0xF44D40) -- relocate the serialised movie root IN PLACE against
    // the load base (the aptDataOffset memory address). The remaining PC
    // record accesses in this body still require native recovery.
    AptCharacterAnimation* Fixup(void* pBase, struct AptConstFile* pConstFile, void* pBlock);
    // @0x82AFF... (PS3 @0xF44894, DecFIGS AptCharacterAnimation::Link(AptCharacter*, void*)) --
    // bind a just-loaded movie root into the scene (AptLoader::Update's state 3->4 import/
    // cross-reference resolution pass). Console args a2/a3 are never read; kept as the raw
    // pData/pDataBlock the loader passes. Body in the .cpp.
    void Link(void* pData, void* pDataBlock);

};

// Native offsets used by FindExport, GetIDFromImportFile, Resolve and Fixup.
CGS_STATIC_ASSERT((sizeof(AptImportEntry) == 0x10), "ARTIST import stride");
CGS_STATIC_ASSERT((offsetof(AptImportEntry, mpClassName) == 0x04), "ARTIST import class");
CGS_STATIC_ASSERT((offsetof(AptImportEntry, mnId) == 0x08), "ARTIST import id");
CGS_STATIC_ASSERT((offsetof(AptImportEntry, mpFile) == 0x0C), "ARTIST import file");
CGS_STATIC_ASSERT((sizeof(AptInitEntry) == 0x08), "ARTIST export/init stride");
CGS_STATIC_ASSERT((offsetof(AptInitEntry, mnIndicator) == 0x04), "ARTIST export/init id");
CGS_STATIC_ASSERT((offsetof(AptCharacterAnimation, mpFrames) == 0x04), "ARTIST movie frames");
CGS_STATIC_ASSERT((offsetof(AptCharacterAnimation, mnCharacterCount) == 0x0C), "ARTIST movie characters");
CGS_STATIC_ASSERT((offsetof(AptCharacterAnimation, mpCharacterTable) == 0x10), "ARTIST movie character table");
CGS_STATIC_ASSERT((offsetof(AptCharacterAnimation, mnMillisecondsPerFrame) == 0x1C), "ARTIST movie frame period");
CGS_STATIC_ASSERT((offsetof(AptCharacterAnimation, mnImportCount) == 0x20), "ARTIST movie imports");
CGS_STATIC_ASSERT((offsetof(AptCharacterAnimation, mpImportTable) == 0x24), "ARTIST movie import table");
CGS_STATIC_ASSERT((offsetof(AptCharacterAnimation, mnInitListCount) == 0x28), "ARTIST movie exports/init count");
CGS_STATIC_ASSERT((offsetof(AptCharacterAnimation, mpInitList) == 0x2C), "ARTIST movie exports/init list");
CGS_STATIC_ASSERT((offsetof(AptCharacterAnimation, mnParsedValueCount) == 0x30), "ARTIST movie resolve counter");
CGS_STATIC_ASSERT((sizeof(AptCharacterAnimation) == 0x34), "ARTIST movie definition");
