#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysPackageAllocator.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CgsDev::Assert Begin/Fire/End + KI_MESSAGEBUFFERSIZE
#include "GameShared/GameClasses/Development/CgsStrStream.h" // CgsDev::StrStream (OOM message build)
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"     // CgsMemory::HeapMalloc

namespace CgsAttribSys
{
// @ 0x828041B0 - adopt the supplied heap + alignment and clear the running totals.
// Asserts (mbHasAllocator == false), (lpHeapAllocator != NULL) and a resolvable
// package name, then stores: mpHeapAllocator, miAlignment, clears miAllocTotal,
// sets mbHasAllocator = true, clears miFreeTotal. (X360 store order: +0x00, +0x14,
// +0x0C=0, +0x04=1, +0x10=0; returns 1.)
bool AttribSysPackageAllocator::Prepare(CgsMemory::HeapMalloc* lpHeapAllocator, s32 liAlignment)
{
    CGS_ASSERT(mbHasAllocator == false, "mbHasAllocator == false");
    CGS_ASSERT(lpHeapAllocator != NULL, "lpHeapAllocator != NULL");
    CGS_ASSERT(GetUserPackageName(), "GetUserPackageName()");

    mpHeapAllocator = lpHeapAllocator;
    miAlignment     = liAlignment;
    miAllocTotal    = 0;
    mbHasAllocator  = true;
    miFreeTotal     = 0;
    return true;
}

// @ 0x821F0358 - allocate liSize bytes from the adopted heap at miAlignment.
// X360 calls HeapMalloc::Malloc(this->mpHeapAllocator, liSize, this->miAlignment)
// (r3=heap, r4=size, r5=miAlignment). On NULL return the original streams
// "<package> memory overload.\n" into the assert message buffer and fires the
// assert; the running miAllocTotal is bumped by liSize regardless. (liFlags is
// part of the AttribSys allocator interface but unused on this path.)
void* AttribSysPackageAllocator::Malloc(size_t lnSize, int /*liFlags*/)
{
    const s32 liSize = static_cast<s32>(lnSize);

    CGS_ASSERT(mbHasAllocator, "mbHasAllocator");

    void* lpResult = mpHeapAllocator->Malloc(liSize, miAlignment);
    if (!lpResult)
    {
        // X360 built this message into CgsDev::Assert::gpcMessageBuffer via an inline
        // StrStream; reconstructed with a local message buffer (the CgsIDCompress
        // precedent). StrStream::operator<<(const char*) null-guards to "<NULLSTRING>",
        // matching the asm aNullstring fallback at 0x821F0418.
        char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << GetUserPackageName();
        lStrStream << " memory overload.\n";

        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            lStrStream.GetBuffer(),
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\attribsys\\CgsAttribSysPackageAllocator.h",
            249);
        CgsDev::Assert::EndAssert();
    }

    miAllocTotal += liSize;
    return lpResult;
}

// @ 0x828020C8 - free a block back to the adopted heap. The byte count is part of
// the interface but unused here (the heap tracks block sizes); only mbHasAllocator
// is asserted before forwarding to HeapMalloc::Free(this->mpHeapAllocator, block).
void AttribSysPackageAllocator::Free(void* lpBlock, size_t /*lnSize*/)
{
    CGS_ASSERT(mbHasAllocator, "mbHasAllocator");

    mpHeapAllocator->Free(lpBlock);
}

// @ 0x821F0200 - the diagnostic name for meUserPackage. Returns NULL (with an
// assert) for an id outside the known E_PACKAGE_* range. (X360 compares the raw
// +0x08 word: 0->"AttribSys", 1->"GameTalk", <3->"EASTL", else assert+NULL.)
const char* AttribSysPackageAllocator::GetUserPackageName()
{
    switch (meUserPackage)
    {
    case E_PACKAGE_ATTRIBSYS:
        return "AttribSys";
    case E_PACKAGE_GAMETALK:
        return "GameTalk";
    case E_PACKAGE_EASTL:
        return "EASTL";
    default:
        break;
    }

    CGS_ASSERT(false, "Unknown package.\n");
    return NULL;
}
}
