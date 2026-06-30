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

#include <cstdint>

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
    inline int32_t BlobI32(void* pRec, int nOff)             { return *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(pRec) + nOff); }

    // The console relocation primitive: add the load base to a file-relative offset,
    // but only when the offset is non-zero (a zero offset stays a null pointer). The
    // asm guards EVERY relocation this way (cmplwi r11,0 / beq -> li r11,0).
    inline int32_t Reloc32(int32_t nOffset, int32_t nBase) { return nOffset ? (nOffset + nBase) : 0; }
    inline int64_t Reloc64(int64_t nOffset, int64_t nBase) { return nOffset ? (nOffset + nBase) : 0; }
}

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

// Resolve @0x80EEC4 -- resolve the (serialised) movie root against the load base.
AptCharacterAnimation* AptCharacterAnimation::Resolve(void* pBase, AptConstFile* pConstFile, void* pBlock)
{
    // FLAG: the console relocates pConstFile->mnSecondaryOffset in place (+= base)
    // around the Fixup; x64 computes addresses without the 32-bit write-back, so
    // only the scratch clear + the Fixup remain here.
    mpResolveState = 0;
    return Fixup(pBase, pConstFile, pBlock);
}

// Fixup @0x80E9E4 -- relocate/resolve the serialised character tree. POINTER-SIZE
// DUAL-PATH (a PC-compatibility refinement of the console, which only ever shipped the
// 4-byte format): the console walks the character / import / init tables and adds the
// load base to every record's file-relative offset IN PLACE, then uses the blob directly
// as the runtime root. Both paths run the SAME walk (FixupWalk below), differing only in
// the file-slot WIDTH: 8 -> FixupInPlace (the console relocation widened to 8-byte slots,
// faithful for an in-place 8-byte .apt), 4 -> FixupTranscode (the console 32-bit in-place
// relocation; the full native-struct transcode is FLAGged where the 32-bit slot cannot
// hold the 64-bit host pointer). Dispatch on the header's pointer size.
AptCharacterAnimation* AptCharacterAnimation::Fixup(void* pBase, AptConstFile* pConstFile, void* pBlock)
{
    if (pConstFile && pConstFile->GetPointerSizeBytes() == 8)
        return FixupInPlace(pBase, pConstFile, pBlock);
    return FixupTranscode(pBase, pConstFile, pBlock);
}

// ===========================================================================
// The SHARED Fixup walk @0x82AFF268, decompiled FAITHFULLY from the X360 ARTIST.XEX.
//
// `this` is the serialised AptCharacterAnimation (the file blob; addressed by its
// console byte offsets via the Blob* accessors). a2/nBase = the load base; a3/pA3 =
// the AptMovie::resolve parse context (r5); a4/pA4 = the pfnLoadRenderingUnit user
// data + AptMovie::resolve a4 (r6). The walk has three passes:
//
//   1. init-indicator list: relocate mpInitList[c:0x2C] + mpCharacterTable[c:0x10],
//      then each init entry's object slot (+0, stride 8).
//   2. character table: relocate each table slot, wire mpFixupLink[c:+4] to table[0],
//      switch on the character type [c:+0], relocate that type's pointer slots
//      (recursing into AptMovie::resolve for sprites/movies (5/9) and calling the host
//      pfnLoadRenderingUnit for shapes (1)), then AptCharacter::SetupCharacter.
//   3. import table: relocate mpImportTable[c:0x24] + each entry's name/class slots,
//      then AptLoader::Load(name) and assign the handle into the entry's AptFile slot.
//
// The per-record offsets ARE the console ones (the blob is the 32-bit file format);
// the pointer-SLOT width is the only thing the two callers below vary -- TReloc is
// the relocation primitive (Reloc32 for the 4-byte .apt, Reloc64 for the 8-byte one)
// and nSlot the slot stride. Everything else is identical and faithful to the asm.
// ===========================================================================
namespace
{
    template <typename TOff, TOff (*TReloc)(TOff, TOff)>
    AptCharacterAnimation* FixupWalk(AptCharacterAnimation* pThis, void* pBase,
                                     void* pA3, void* pA4)
    {
        const TOff nBase = static_cast<TOff>(reinterpret_cast<intptr_t>(pBase));

        // Relocate one file-relative slot of width sizeof(TOff) in place.
        auto RelocSlot = [nBase](void* pRec, int nOff)
        {
            TOff* pSlot = reinterpret_cast<TOff*>(reinterpret_cast<char*>(pRec) + nOff);
            *pSlot = TReloc(*pSlot, nBase);
        };

        // Read a (now relocated) pointer slot of width sizeof(TOff) -- the file
        // pointer slots are TOff-wide, so they must NOT be read as host void* on the
        // 4-byte path. Returns the slot value as a host pointer.
        auto SlotPtr = [](void* pRec, int nOff) -> void*
        {
            return reinterpret_cast<void*>(static_cast<intptr_t>(
                *reinterpret_cast<TOff*>(reinterpret_cast<char*>(pRec) + nOff)));
        };

        // char_i = mpCharacterTable[i]; the table-slot stride is sizeof(TOff).
        auto CharAt = [&SlotPtr, pThis](int32_t i) -> void*
        {
            void* pTable = SlotPtr(pThis, 0x10);   // mpCharacterTable (relocated)
            return SlotPtr(pTable, i * static_cast<int>(sizeof(TOff)));
        };

        // The pointer-array slot stride (sizeof(TOff)); the char table + init list +
        // import table index by it. The console (4-byte) value is 4; the 8-byte .apt
        // widens it. FLAG: the AptInitEntry / AptImportEntry record sizes below mix a
        // pointer slot with trailing int fields -- their TOTAL stride for an 8-byte
        // .apt is not recoverable from the 4-byte console asm, so the trailing-field
        // portion (init +4, import +8/+0x0C) is assumed unchanged (console offsets);
        // only the leading pointer slots widen. This is exact for the 4-byte path.
        const int nPtr = static_cast<int>(sizeof(TOff));

        // ---- pass 1: the init-indicator list [c:0x28]/[c:0x2C] ----------------
        // mpCharacterTable [c:0x10] and mpInitList [c:0x2C] are relocated first;
        // then each AptInitEntry's object pointer (+0). Console record stride = 8
        // {ptr@+0, indicator@+4}; widened to nPtr + 4 for the 8-byte path.
        RelocSlot(pThis, 0x10);                          // mpCharacterTable
        RelocSlot(pThis, 0x2C);                          // mpInitList
        {
            const int32_t nInit = BlobI32(pThis, 0x28);  // mnInitListCount
            void* pInitList = SlotPtr(pThis, 0x2C);
            const int nInitStride = nPtr + 4;            // {ptr, int32 indicator}
            for (int32_t i = 0; i < nInit; ++i)
                RelocSlot(pInitList, i * nInitStride);   // AptInitEntry.mpInitObject (+0)
        }

        // ---- pass 2: the character table [c:0x0C]/[c:0x10] --------------------
        const int32_t nChars = BlobI32(pThis, 0x0C);     // mnCharacterCount
        for (int32_t i = 0; i < nChars; ++i)
        {
            // relocate this table slot (slot stride = sizeof(TOff)).
            RelocSlot(SlotPtr(pThis, 0x10), i * nPtr);

            // re-read the (now relocated) entry; null entries are skipped.
            void* pChar = CharAt(i);
            if (!pChar)
                continue;

            // mpFixupLink [c:+4] = the table base (table[0]); a back-link the asm
            // wires for every character: stw *(mpCharacterTable) -> char[+4]. The
            // link is a pointer slot, so it is TOff-wide.
            {
                void* pTable = SlotPtr(pThis, 0x10);
                TOff nTable0 = *reinterpret_cast<TOff*>(pTable);   // table[0]
                *reinterpret_cast<TOff*>(reinterpret_cast<char*>(pChar) + 0x04) = nTable0;
            }

            const int32_t eType = BlobI32(pChar, 0x00);  // mnType
            switch (eType)
            {
                case 1:
                    // Shape: load the host rendering unit for character index i and
                    // store the returned handle at char[+0x20] (a pointer slot, TOff-
                    // wide). dword_8324E878 == gAptFuncs+0x60 == pfnLoadRenderingUnit(
                    // a4, index). FLAG: the asm calls through the gAptFuncs slot by raw
                    // offset; routed through the named pfn.
                    *reinterpret_cast<TOff*>(reinterpret_cast<char*>(pChar) + 0x20) =
                        static_cast<TOff>(reinterpret_cast<intptr_t>(
                            gAptFuncs.pfnLoadRenderingUnit(pA4, i)));
                    break;

                case 2:
                    // Sprite-button / morph header: relocate char[+0x3C] and char[+0x40].
                    RelocSlot(pChar, 0x3C);
                    RelocSlot(pChar, 0x40);
                    break;

                case 3:
                    // Text / edit-text: relocate char[+0x10] and char[+0x18].
                    RelocSlot(pChar, 0x10);
                    RelocSlot(pChar, 0x18);
                    break;

                case 5:
                case 9:
                    // Sprite / movie: recurse into the embedded timeline at char+0x10.
                    // a4 (r6) = pThis+0x30 (the resolve-state scratch, AptCharacterAnimation
                    // console dword [12]); a3 (r5) = pA3; nBase (r4) = the load base.
                    reinterpret_cast<AptMovie*>(reinterpret_cast<char*>(pChar) + 0x10)
                        ->resolve(static_cast<int>(nBase), pA3,
                                  static_cast<int>(reinterpret_cast<intptr_t>(
                                      reinterpret_cast<char*>(pThis) + 0x30)));
                    break;

                case 10:
                    // Font: relocate char[+0x3C] (the glyph table), then walk its
                    // char[+0x38] entries (stride 0x38), relocating each entry's +0x34
                    // glyph-data slot.
                    RelocSlot(pChar, 0x3C);
                    {
                        const int32_t nGlyphs = BlobI32(pChar, 0x38);
                        void* pGlyphs = SlotPtr(pChar, 0x3C);   // relocated glyph table
                        for (int32_t g = 0; g < nGlyphs; ++g)
                            RelocSlot(pGlyphs, g * 0x38 + 0x34);
                    }
                    break;

                default:
                    break;
            }

            // Post-relocation per-character init (re-reads the relocated table slot).
            // The asm calls AptCharacter::SetupCharacter with r3=pChar only.
            reinterpret_cast<AptCharacter*>(CharAt(i))->SetupCharacter();
        }

        // ---- pass 3: the import table [c:0x20]/[c:0x24] -----------------------
        // Console AptImportEntry = 16 bytes {name@+0, class@+4, id@+8, AptFilePtr@+0xC}.
        // The leading name/class are pointer slots (TOff-wide); the AptFilePtr slot at
        // +0x0C is likewise one host AptFile* word. FLAG: the 8-byte-format entry size
        // is not recoverable from the 4-byte asm -- the per-entry layout below uses the
        // console offsets, exact for the 4-byte path (and the widened entry stride
        // tracks the two leading pointer slots).
        RelocSlot(pThis, 0x24);                          // mpImportTable
        const int32_t nImports = BlobI32(pThis, 0x20);   // mnImportCount
        const int nNameOff  = 0;
        const int nClassOff = nPtr;                      // class slot follows the name slot
        const int nFileOff  = nPtr * 2 + 4;              // {name*, class*, id(int32)} then AptFilePtr
        const int nImpStride = nPtr * 3 + 4;             // {name*, class*, id, AptFile*}
        for (int32_t i = 0; i < nImports; ++i)
        {
            void* pImports = SlotPtr(pThis, 0x24);
            const int nEntry = i * nImpStride;
            RelocSlot(pImports, nEntry + nNameOff);      // mpImportFileName
            RelocSlot(pImports, nEntry + nClassOff);     // mpClassName

            // EAStringC around the (relocated) import file name -- the ctor's
            // InitFromBuffer / dtor's DecreaseInternalRefCount are the asm's
            // per-iteration RAII bracket (var_4C).
            EAStringC importName(reinterpret_cast<const char*>(
                SlotPtr(SlotPtr(pThis, 0x24), nEntry + nNameOff)));

            // AptLoader::Load(out, gpAptTarget->mpLoader, &name) -> the AptFilePtr handle.
            AptFilePtr loaded;
            loaded.pData = nullptr;
            AptLoader_LoadX360(&loaded, gpAptTarget->mpLoader, &importName);

            // Assign the handle into the entry's AptFile slot (+0x0C console) -- the
            // console AptFile_::operator_ (AptSharedPtr<AptFile>::operator=): inc the
            // new ref, dec+delete the old. The slot is a TOff-wide pointer word.
            // FLAG (4-byte x64 fork): on the 4-byte path the host AptFile* is 64-bit
            // but the slot is only 32-bit, so the stored pointer is truncated -- the
            // genuine 32-bit-slot-can't-hold-a-64-bit-pointer impossibility. The native
            // transcode (a real AptFilePtr in a 64-bit native struct) lands with the
            // FixupTranscode rebuild; the ref-count bookkeeping below is faithful.
            TOff* pFileSlot = reinterpret_cast<TOff*>(
                reinterpret_cast<char*>(SlotPtr(pThis, 0x24)) + nEntry + nFileOff);
            AptFile* pNew = loaded.pData;
            AptFile* pOld = reinterpret_cast<AptFile*>(static_cast<intptr_t>(*pFileSlot));
            if (pNew)
                AptSharedPtrIncRef(pNew);
            *pFileSlot = static_cast<TOff>(reinterpret_cast<intptr_t>(pNew));
            if (pOld && AptSharedPtrDecRef(pOld) == 0)
                AptSharedPtrDelete(pOld);

            // Release the local `loaded` handle (asm: *var_50=0; DecRef; delete-if-zero).
            AptFile* pTmp = loaded.pData;
            loaded.pData = nullptr;
            if (pTmp && AptSharedPtrDecRef(pTmp) == 0)
                AptSharedPtrDelete(pTmp);
            // importName's dtor fires here (DecreaseInternalRefCount).
        }

        return pThis;
    }
}

// FixupInPlace -- the 8-byte path: the faithful console relocation, generalised to
// 8-byte slots. A 64-bit load base fits an 8-byte slot, so the console's in-place
// relocate works VERBATIM -- every file-relative pointer slot is widened to 8 bytes
// and the blob is used directly as the runtime root. FLAG: this is the console
// behaviour widened to 8-byte pointers (the in-place 8-byte .apt format shipped by
// other 64-bit builds of the engine); the per-record TYPE switch + the
// AptMovie::resolve / AptLoader::Load recursion are identical to the 4-byte path.
AptCharacterAnimation* AptCharacterAnimation::FixupInPlace(void* pBase, AptConstFile* pConstFile, void* pBlock)
{
    (void)pConstFile;
    // pConstFile is a3 (r5, the AptMovie::resolve context); pBlock is a4 (r6, the
    // pfnLoadRenderingUnit user data). Mapped from Resolve's Fixup(this, base, a3, a4).
    return FixupWalk<int64_t, Reloc64>(this, pBase, pConstFile, pBlock);
}

// FixupTranscode -- the 4-byte path (the console default): the SAME per-record walk,
// relocating the 32-bit file slots in place (Reloc32). FLAG (x64 fork): the truly
// faithful x64 form would TRANSCODE each 32-bit record into a native 64-bit runtime
// struct (a 64-bit load base does not fit a 32-bit slot), but the records are walked
// in place by AptMovie::resolve / AptLoader::Load (the X360 overload) /
// AptCharacter::SetupCharacter -- all still deferred and all operating on the SAME
// in-place 32-bit blob. Rebuilding into native structs here would desync those
// callees, so this reproduces the console's verbatim 32-bit in-place relocation; the
// transcode-to-native-struct rebuild is gated on those callees landing (then the
// only change is the slot width + writing to the native struct rather than the blob).
AptCharacterAnimation* AptCharacterAnimation::FixupTranscode(void* pBase, AptConstFile* pConstFile, void* pBlock)
{
    (void)pConstFile;
    return FixupWalk<int32_t, Reloc32>(this, pBase, pConstFile, pBlock);
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
