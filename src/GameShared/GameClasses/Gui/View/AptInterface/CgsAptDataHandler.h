#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"   // CgsMemory::HeapMalloc (mpAllocator)

// CgsGui::AptDataHandler - the registry/allocator front-end the GUI's APT (movie)
// interface uses to load, look up and free APT data headers. It owns a fixed table of
// (hash -> AptDataHeader*) entries plus the HeapMalloc the APT runtime allocates its
// data blocks from. Member set, order, types and offsets are from the DecFIGS DWARF
// (CgsAptDataHandler.h:45-108); the two methods bodied in CgsAptDataHandler.cpp are
// grounded store-for-store against the X360 ARTIST binary:
//   AptAlloc @ 0x82847250   AptFree @ 0x82847390
//
// Layout (DWARF CgsAptDataHandler.h, matched to the X360 `*(this + 1028)` allocator load):
//   miNumLoadedAptData   [+0x000]  s32                              (h:97)
//   maAptDataHeaderInfo  [+0x004]  sAptDataHeaderInfo[128]          (h:105; 128 * 8 == 0x400)
//   mpAllocator          [+0x404]  CgsMemory::HeapMalloc*           (h:108; == +1028)
namespace CgsGui
{
    // Forward declaration: the APT data header the loader registers/returns. AptDataHandler
    // only stores/returns AptDataHeader* (the two memory methods here never dereference it),
    // so an incomplete type suffices for this slice. Its full home is its own TU.
    struct AptDataHeader;

    struct AptDataHandler
    {
        // CgsAptDataHandler.h:94 (DWARF) - capacity of the loaded-header table.
        static const s32 KI_MAX_LOADED_APT_HEADERS = 128;

        // CgsAptDataHandler.h:102 (DWARF) - one (hash, header) table entry.
        struct sAptDataHeaderInfo
        {
            u32            mHashKey;        // +0x0 (h:103)
            AptDataHeader* mpAptDataHeader; // +0x4 (h:104)
        };

        // ----- methods bodied by class:CgsGui::AptDataHandler (CgsAptDataHandler.cpp) -----

        // CgsAptDataHandler.h:86 (DWARF). X360 0x82847250: asserts the data allocator is
        // valid (mpAllocator != null), allocates lnSize bytes 4-aligned from it, asserts
        // the allocation succeeded, returns the block. The X360 calls the member
        // mpAllocator->Malloc(lnSize, 4).
        void* AptAlloc(size_t lnSize);

        // CgsAptDataHandler.h:90 (DWARF). X360 0x82847390: asserts the data allocator is
        // valid and the block pointer is non-null, then frees it via mpAllocator->Free.
        void  AptFree(void* lpBlock);

        // X360 0x8284A290. Install the allocator used by AptAlloc/AptFree.
        bool Prepare(CgsMemory::HeapMalloc* lpAllocator);

        // X360 0x828518F0 (CgsAptDataHandler.cpp). Look up a registered AptDataHeader by its
        // movie name (hash the name, linear-scan the (hash,header) table); null if absent.
        AptDataHeader* FindAptData(const char* lpacName);

        // X360 0x82851990 (CgsAptDataHandler.cpp). Register a loaded AptDataHeader keyed by
        // its movie name (idempotent -- returns the existing header if already registered).
        // lpacName is the resolved name string (// FLAG x64: the console reads it from the
        // header's relocated mpacMovieName, which is an un-relocated offset on x64).
        AptDataHeader* AddAptData(AptDataHeader* lpHeader, const char* lpacName);

        // The remaining methods (Construct / Prepare / Release / Destruct / RemoveAptData)
        // are homed by their own ledger TUs; declared there.

    private:
        s32                 miNumLoadedAptData;                      // +0x000 (h:97)
        sAptDataHeaderInfo  maAptDataHeaderInfo[KI_MAX_LOADED_APT_HEADERS]; // +0x004 (h:105)
        CgsMemory::HeapMalloc* mpAllocator;                          // +0x404 (h:108, == +1028)
    };
}
