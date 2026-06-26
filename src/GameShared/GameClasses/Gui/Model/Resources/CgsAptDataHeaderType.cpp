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
// (a deeper serialised structure, still offset-walked pending its own struct recovery).

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
        uintptr_t lRes = reinterpret_cast<uintptr_t>(lpResource);
        u32* lpRoot = *reinterpret_cast<u32**>(lRes + 12);

        uint32_t luCount = 0;
        u32 luMovies = lpRoot[0];
        if (luMovies)
        {
            u32* lpMovie = reinterpret_cast<u32*>(lpRoot[2]);
            do
            {
                u32* lpChar = reinterpret_cast<u32*>(*lpMovie);
                u32 luImports = lpChar[1];
                if (luImports)
                {
                    u32* lpImport = reinterpret_cast<u32*>(lpChar[2]);
                    do
                    {
                        if (reinterpret_cast<u32*>(*lpImport)[1])
                            ++luCount;
                        --luImports;
                        // FLAG: ARTIST 0x8284BB60 `addi r10,r10,4` advances the import-table
                        // cursor by 4 BYTES (one serialised 32-bit entry), not 4 entries.
                        // lpImport is a u32* so a single ++ == +4 bytes (DecFIGS 0xB73748:
                        // `4 * v6++ + importTable`).
                        ++lpImport;
                    } while (luImports);
                }
                --luMovies;
                lpMovie += 4;
            } while (luMovies);
        }
        return luCount;
    }

    void AptDataHeaderType::GetImportPointer(const void* lpResource, uint32_t luIndex,
                                             uint32_t* lpuOffset, const void** lppValue) const
    {
        uintptr_t lRes = reinterpret_cast<uintptr_t>(lpResource);
        *lppValue = nullptr;
        *lpuOffset = 0;

        u32* lpRoot = *reinterpret_cast<u32**>(lRes + 12);
        u32 luMovies = lpRoot[0];
        if (!luMovies)
            return;

        uint32_t luSeen = 0;
        u32* lpMovie = reinterpret_cast<u32*>(lpRoot[2]);
        u32 luMovie = 0;
        do
        {
            u32 luChar = *lpMovie;
            u32 luImports = reinterpret_cast<u32*>(*lpMovie)[1];
            if (luImports)
            {
                u32 luImport = 0;
                do
                {
                    // FLAG: ARTIST 0x8284BBC8 reads `*(importTable[luImport])[1]` -- the import
                    // table (lpChar[2], the WORD at luChar+8) is indexed by the running entry
                    // index, the slot value (entry pointer) is dereferenced, then its [1] field
                    // is tested. The prior reconstruction tested a fixed importTable[1] without
                    // indexing/derefing. DecFIGS 0xB737DC: `v15 = 4*v13 + importTable; v17 = *v15;
                    // if (*(v17 + 4))`.
                    u32* lpImportTable = *reinterpret_cast<u32**>(luChar + 8);
                    u32 luEntry = lpImportTable[luImport];
                    if (reinterpret_cast<u32*>(luEntry)[1])
                    {
                        if (luSeen == luIndex)
                        {
                            // FLAG: ARTIST 0x8284BC20-28 computes *lpuOffset from the ENTRY
                            // POINTER (the value loaded from the slot), not the slot address:
                            // `entryPtr - lpResource + 12`. DecFIGS: `*v7 = v17 + 12 - lpResource`.
                            *lppValue = reinterpret_cast<const void*>(reinterpret_cast<u32*>(luEntry)[3]);
                            *lpuOffset = luEntry - static_cast<u32>(lRes) + 12;
                            return;
                        }
                        ++luSeen;
                    }
                    ++luImport;
                } while (luImport < luImports);
            }
            ++luMovie;
            ++lpMovie;
        } while (luMovie < luMovies);
    }
}
