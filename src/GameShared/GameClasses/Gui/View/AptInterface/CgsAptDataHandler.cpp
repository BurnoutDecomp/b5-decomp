#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptDataHandler.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptDataHeader.h"  // AptDataHeader (the registered header type)

#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT, CgsDev::Assert::Begin/Fire/EndAssert
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStream (dynamic assert message)
#include "GameShared/GameClasses/Containers/CgsHash.h"         // CgsContainers::CgsHash::CalculateHash (the name -> hash key)

#include <cstddef>   // size_t
#include <cstring>   // strlen (name length for the hash)

// CgsGui::AptDataHandler memory methods, reconstructed from BURNOUT_X360_ARTIST.XEX.
// This TU (class:CgsGui::AptDataHandler) bodies the two X360-emitted methods:
//
//   AptAlloc @ 0x82847250  -> mpAllocator->Malloc(lnSize, 4), guarded
//   AptFree  @ 0x82847390  -> mpAllocator->Free(lpBlock), guarded
//
// X360 store-for-store notes:
//   - Both load the data allocator from `*(this + 1028)` (mpAllocator @ +0x404) and assert
//     it is non-null before use.
//   - AptAlloc: HeapMalloc::Malloc(allocator, lnSize, 4) is a non-static member call (the
//     allocator is `this` in r3), so it maps to mpAllocator->Malloc(lnSize, 4); the 4 is the
//     alignment (CgsMemory::HeapMalloc::KI_DEFAULT_ALIGNMENT). It then asserts the result is
//     non-null, streaming "Allocating <lnSize> bytes" into the assert message.
//   - AptFree: HeapMalloc::Free(allocator, lpBlock) maps to mpAllocator->Free(lpBlock).
//   - The X360-baked CgsAptDataHandler.h file/line (130/140/156/157) are discarded per
//     project convention; the assert strings are X360 rodata, reproduced verbatim.

// Layout note: under the X360 ABI mpAllocator sits at +1028 (0x404 == the `*(this + 1028)`
// load in both AptAlloc/AptFree), after the s32 count + the 128-entry 8-byte header table.
// We reconstruct with named members under the host (x64) ABI, where the trailing pointer is
// 8 bytes and the struct is 8-aligned, so its absolute offset differs from the X360's 1028 --
// that is expected and fine (access is by name, never by raw offset).

namespace CgsGui
{
    bool AptDataHandler::Prepare(CgsMemory::HeapMalloc* lpAllocator)
    {
        CGS_ASSERT(lpAllocator != 0,
                   "Passed in allocator to AptDataHandler::Prepare is null");
        mpAllocator = lpAllocator;
        return true;
    }

    // X360 0x82847250.
    void* AptDataHandler::AptAlloc(size_t lnSize)
    {
        CGS_ASSERT(mpAllocator != 0, "Apts data allocator is invalid in AptDataHandler::AptAlloc");

        void* lpBlock = mpAllocator->Malloc(static_cast<s32>(lnSize), CgsMemory::HeapMalloc::KI_DEFAULT_ALIGNMENT);

        if (lpBlock == 0)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "AptDataHandler::AptAlloc Failed to allocate memory! Allocating "
                       << static_cast<s32>(lnSize) << " bytes";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }

        return lpBlock;
    }

    // X360 0x82847390.
    void AptDataHandler::AptFree(void* lpBlock)
    {
        CGS_ASSERT(mpAllocator != 0, "Apts data allocator is invalid in AptDataHandler::AptFree");
        CGS_ASSERT(lpBlock != 0, "AptDataHandler::AptFree passed a null pointer to free");

        mpAllocator->Free(lpBlock);
    }

    // =========================================================================
    // FindAptData @0x828518F0 -- look up a loaded AptDataHeader by its movie name.
    // DECOMPILED FAITHFULLY from BURNOUT_X360_ARTIST.XEX. Hash the name (CgsHash::
    // CalculateHash over strlen(name)), then linear-scan the (hash,header) table for a
    // matching hash key; return the paired header, or null after 128 misses.
    //
    // The X360 body:
    //   v5 = CalculateHash(a2, strlen(a2));
    //   for (i = a1+4; *i != v5; i += 2) if (++count >= 128) return 0;
    //   return *(a1 + 8*(count+1));                 // == maAptDataHeaderInfo[count].mpAptDataHeader
    // The table stride is one sAptDataHeaderInfo (8 bytes on the console == {u32 hash,
    // u32 header}); here it is addressed by the named member so the x64 widening is exact.
    // =========================================================================
    AptDataHeader* AptDataHandler::FindAptData(const char* lpacName)
    {
        // strlen (the asm's `while (*v2++);` then v2 - a2 - 1).
        const u32 luHash =
            CgsContainers::CgsHash::CalculateHash(const_cast<char*>(lpacName),
                                                  static_cast<int>(std::strlen(lpacName)));

        for (s32 li = 0; li < KI_MAX_LOADED_APT_HEADERS; ++li)
        {
            if (maAptDataHeaderInfo[li].mHashKey == luHash)
                return maAptDataHeaderInfo[li].mpAptDataHeader;
        }
        return nullptr;   // 128 misses (the asm's `if (++v6 >= 128) return 0`)
    }

    // =========================================================================
    // AddAptData @0x82851990 -- register a loaded AptDataHeader (idempotent by name).
    // DECOMPILED FAITHFULLY from BURNOUT_X360_ARTIST.XEX. FindAptData first; on a miss,
    // assert capacity, hash the name, and append {hash, header} at the next free slot,
    // bumping the count. Returns the found-or-registered header.
    //
    // The X360 body reads the name from the header's first word (`*a2` == mpacMovieName)
    // and stores the header pointer (`a2`) itself. On x64 the header's mpacMovieName field
    // is an UN-RELOCATED file offset (the no-op FixUp; see CgsAptDataHeader.cpp), so the
    // console form resolves it as header base + offset (the converted 6-field header's name
    // offset is the 8-byte slot at +0x00); the two-arg form takes an already-resolved name
    // (FLAG PC-platform leaf: host .apt blob offset access -- the import fallback's
    // spelling of the same registration).
    // =========================================================================
    AptDataHeader* AptDataHandler::AddAptData(AptDataHeader* lpHeader)
    {
        const uintptr_t luBase    = reinterpret_cast<uintptr_t>(lpHeader);
        const uintptr_t luNameOff = static_cast<uintptr_t>(
            *reinterpret_cast<const unsigned long long*>(luBase));
        return AddAptData(lpHeader, reinterpret_cast<const char*>(luBase + luNameOff));
    }

    AptDataHeader* AptDataHandler::AddAptData(AptDataHeader* lpHeader, const char* lpacName)
    {
        AptDataHeader* lpFound = FindAptData(lpacName);
        if (lpFound)
            return lpFound;   // already registered (the asm's `if (!result) { ... }`)

        CGS_ASSERT(miNumLoadedAptData < KI_MAX_LOADED_APT_HEADERS,
                   "Trying to add to many AptDataHeaders in AptDataHandler::AddAptData");

        const u32 luHash =
            CgsContainers::CgsHash::CalculateHash(const_cast<char*>(lpacName),
                                                  static_cast<int>(std::strlen(lpacName)));

        // a1[2 * *a1 + 1] = hash; a1[2 * ++*a1] = header  (store hash at the current slot,
        // header at the pre-incremented slot -- i.e. slot index == the new count).
        maAptDataHeaderInfo[miNumLoadedAptData].mHashKey = luHash;
        ++miNumLoadedAptData;
        maAptDataHeaderInfo[miNumLoadedAptData - 1].mpAptDataHeader = lpHeader;
        return nullptr;   // the asm returns the (null) FindAptData result on the add path
    }

    // =========================================================================
    // ⭐⭐ RemoveAptData @0x8284A338 -- un-register a loaded AptDataHeader by its name hash.
    // DECOMPILED FAITHFULLY from BURNOUT_X360_ARTIST.XEX. The counterpart AddAptData never
    // had: scan the live part of the table for the hash, SWAP THE LAST ENTRY INTO THE HOLE,
    // zero the vacated tail slot and decrement the count; if no entry matched, fire the
    // "non-existent" assert (CgsAptDataHandler.cpp:202).
    //
    // The X360 body, statement for statement:
    //   v6 = 0; v7 = a1 + 1;                       // &maAptDataHeaderInfo[0].mHashKey
    //   while (*v7 != a2) { ++v6; v7 += 2; if (v6 >= *a1) goto notfound; }
    //   v8 = &a1[2 * v6];                          // v8[1] == hash[v6], v8[2] == header[v6]
    //   if (*(v8[2] + 20) == 2 && (gxMessageFilterFlags & 1))  <warning print>
    //   v18 = &a1[2 * *a1 - 1];                    // the LAST entry
    //   v8[1] = *v18; v8[2] = v18[1];              // move last -> hole
    //   a1[2 * *a1 - 1] = 0; a1[2 * (*a1)--] = 0;  // clear the tail, drop the count
    //
    // ⭐ THE SWAP IS LOAD-BEARING, not tidiness: FindAptData scans all 128 slots regardless of
    // the count, so a hole left with its old hash would keep answering lookups for a header
    // that is gone. Moving the tail in and zeroing it is what makes the entry unreachable.
    //
    // ⓘ ONE CONSOLE STATEMENT IS DELIBERATELY NOT REPRODUCED, and it is a debug print with no
    // behaviour: the "*** WARNING: AptDataHandler: Trying to remove an active AptDataHeader"
    // stream, gated on the header's +20 state word == 2 AND CgsDev::Message::gxMessageFilterFlags
    // bit 0. AptDataHeader is an incomplete type in this slice (its full home is its own TU), so
    // reproducing it would mean poking +20 through a raw offset cast -- which this project
    // forbids -- to print a line. Named here rather than faked; restore it when this TU can see
    // the header's real layout.
    // =========================================================================
    void AptDataHandler::RemoveAptData(u32 luNameHash)
    {
        bool lbRemoved = false;

        for (s32 li = 0; li < miNumLoadedAptData; ++li)
        {
            if (maAptDataHeaderInfo[li].mHashKey != luNameHash)
            {
                continue;
            }

            const s32 liLast = miNumLoadedAptData - 1;
            maAptDataHeaderInfo[li].mHashKey        = maAptDataHeaderInfo[liLast].mHashKey;
            maAptDataHeaderInfo[li].mpAptDataHeader = maAptDataHeaderInfo[liLast].mpAptDataHeader;
            maAptDataHeaderInfo[liLast].mHashKey        = 0;
            maAptDataHeaderInfo[liLast].mpAptDataHeader = nullptr;
            --miNumLoadedAptData;

            lbRemoved = true;
            break;
        }

        // The X360 streams the offending hash into the message ("... RemoveAptData, hash=%u");
        // reduced to the static condition, per this tree's convention for streamed asserts.
        CGS_ASSERT(lbRemoved,
                   "Trying to remove non-existent Apt Data in AptDataHandler::RemoveAptData");
    }
}
