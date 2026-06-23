#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptDataHandler.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT, CgsDev::Assert::Begin/Fire/EndAssert
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStream (dynamic assert message)

#include <cstddef>   // size_t

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
}
