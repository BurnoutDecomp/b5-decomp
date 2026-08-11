#include "GameShared/GameClasses/Gui/Model/Resources/CgsAptDataHeaderType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptDataHeader.h"     // CgsGui::AptDataHeader (named layout + relocation)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::AptDataHeaderType::FixDown          @ 0x8285D980
//   CgsResource::AptDataHeaderType::FixUp            @ 0x828559D8
//   CgsResource::AptDataHeaderType::GetImportCount   @ 0x8284BB18
//   CgsResource::AptDataHeaderType::GetImportPointer @ 0x8284BB80
//   CgsResource::AptDataHeaderType::GetTypeID        @ 0x8284BB10
//
// FixUp/FixDown (un)relocate the resource's CgsGui::AptDataHeader -- the X360 inlined the
// header's own FixUp/FixDown here, so these forward to the named struct method.
// GetImportCount / GetImportPointer walk the movie -> character -> import table hierarchy
// (raw-offset walk over the serialised blob). Widths/offsets/strides are pinned by the XB1
// x64 twins sub_1400CD140 / sub_1400CD1A0 -- the vtable siblings of the return-30 GetTypeID
// stub sub_1400CD0D0 and the five-widened-field FixUp/FixDown pair sub_1400CD0E0 /
// sub_1400CD110 (ARTIST 0x8284BB18 / 0x8284BB80 are the behavioural twins).

namespace CgsResource
{
    static const uint32_t KU_APT_DATA_HEADER_RESOURCE_TYPE_ID = 30;

    uint32_t AptDataHeaderType::GetTypeID() const
    {
        return KU_APT_DATA_HEADER_RESOURCE_TYPE_ID;
    }

    void AptDataHeaderType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<CgsGui::AptDataHeader*>(lpResource)->FixDown(CgsResource::GetLoadBase(lrResource), true);
    }

    void AptDataHeaderType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<CgsGui::AptDataHeader*>(lpResource)->FixUp(CgsResource::GetLoadBase(lrResource));
    }

    uint32_t AptDataHeaderType::GetImportCount(const void* lpResource) const
    {
        // x64 XB1 sub_1400CD140: root = [res+0x20] (the native-8 six-field header's movie
        // slot), movie count = u32[root+0], movie table = [root+8] with 8-BYTE entries; per
        // movie: import count = u32[movie+4], import table = [movie+8] with 8-BYTE entries;
        // count the entries whose u32[entry+4] (the live-import flag) is non-zero. (ARTIST
        // 0x8284BB18 is the behavioural twin with the console 4-byte strides.) On the XB1
        // every slot is a real pointer at walk time (FixUp @sub_1400CD0E0 relocated them in
        // place); this build's bundle keeps every serialised slot as a resource-relative
        // OFFSET (the no-op FixUp regime -- CgsAptDataHeader.cpp), so each slot resolves as
        // resource base + offset (the header sits at the resource base).
        const uintptr_t luBase    = reinterpret_cast<uintptr_t>(lpResource);
        const u64      luRootOff = *reinterpret_cast<const u64*>(luBase + 0x20u);   // serialized .apt header: movie root @+0x20
        if (luRootOff == 0)
            return 0;
        const uintptr_t luRoot = luBase + static_cast<uintptr_t>(luRootOff);

        uint32_t   luCount  = 0;
        const u32  luMovies = *reinterpret_cast<const u32*>(luRoot);                 // movie count @root+0
        const u64* lpMovieSlot = reinterpret_cast<const u64*>(
            luBase + static_cast<uintptr_t>(*reinterpret_cast<const u64*>(luRoot + 8u)));  // serialized .apt blob: movie table @root+8
        for (u32 luMovie = 0; luMovie < luMovies; ++luMovie)
        {
            const uintptr_t luChar    = luBase + static_cast<uintptr_t>(lpMovieSlot[luMovie]);
            const u32       luImports = *reinterpret_cast<const u32*>(luChar + 4u);  // serialized .apt blob: import count @movie+4
            if (luImports == 0)
                continue;
            const u64* lpImportSlot = reinterpret_cast<const u64*>(
                luBase + static_cast<uintptr_t>(*reinterpret_cast<const u64*>(luChar + 8u)));  // serialized .apt blob: import table @movie+8
            for (u32 luImport = 0; luImport < luImports; ++luImport)
            {
                const uintptr_t luEntry = luBase + static_cast<uintptr_t>(lpImportSlot[luImport]);
                if (*reinterpret_cast<const u32*>(luEntry + 4u) != 0)                // live-import flag @entry+4
                    ++luCount;
            }
        }
        return luCount;
    }

    void AptDataHeaderType::GetImportPointer(const void* lpResource, uint32_t luIndex,
                                             uint32_t* lpuOffset, const void** lppValue) const
    {
        // x64 XB1 sub_1400CD1A0 (ARTIST 0x8284BB80 is the behavioural twin): the same walk as
        // GetImportCount; at the luIndex-th LIVE entry (u32[entry+4] != 0), *lppValue is the
        // 8-byte value slot [entry+0x10] read RAW (FixUp does not relocate it -- it is the
        // patch destination the import system writes the resolved resource into), and
        // *lpuOffset = (u32)(entry - resource) + 0x10, the resource-relative address of that
        // patch slot (asm: mov rax,[rcx+10h]; mov ecx,[rax+rdx]; sub ecx,ebp; add ecx,10h).
        // Slot resolution is resource base + offset, as in GetImportCount (no-op FixUp regime).
        *lppValue = nullptr;
        *lpuOffset = 0;

        const uintptr_t luBase    = reinterpret_cast<uintptr_t>(lpResource);
        const u64      luRootOff = *reinterpret_cast<const u64*>(luBase + 0x20u);   // serialized .apt header: movie root @+0x20
        if (luRootOff == 0)
            return;
        const uintptr_t luRoot = luBase + static_cast<uintptr_t>(luRootOff);

        uint32_t   luSeen   = 0;
        const u32  luMovies = *reinterpret_cast<const u32*>(luRoot);              // serialized .apt blob: movie count
        const u64* lpMovieSlot = reinterpret_cast<const u64*>(
            luBase + static_cast<uintptr_t>(*reinterpret_cast<const u64*>(luRoot + 8u)));   // serialized .apt blob: movie table
        for (u32 luMovie = 0; luMovie < luMovies; ++luMovie)
        {
            const uintptr_t luChar    = luBase + static_cast<uintptr_t>(lpMovieSlot[luMovie]);
            const u32       luImports = *reinterpret_cast<const u32*>(luChar + 4u);   // serialized .apt blob: import count
            if (luImports == 0)
                continue;
            const u64* lpImportSlot = reinterpret_cast<const u64*>(
                luBase + static_cast<uintptr_t>(*reinterpret_cast<const u64*>(luChar + 8u)));   // serialized .apt blob: import table
            for (u32 luImport = 0; luImport < luImports; ++luImport)
            {
                const uintptr_t luEntry = luBase + static_cast<uintptr_t>(lpImportSlot[luImport]);
                if (*reinterpret_cast<const u32*>(luEntry + 4u) == 0)   // serialized .apt blob: unresolved import gate
                    continue;
                if (luSeen == luIndex)
                {
                    *lppValue  = reinterpret_cast<const void*>(static_cast<uintptr_t>(
                        *reinterpret_cast<const u64*>(luEntry + 0x10u)));   // serialized .apt blob: import value/patch slot @entry+0x10
                    *lpuOffset = static_cast<uint32_t>(luEntry - luBase) + 0x10u;
                    return;
                }
                ++luSeen;
            }
        }
    }
}
