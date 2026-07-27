#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysPackageAllocator.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CgsDev::Assert Begin/Fire/End + KI_MESSAGEBUFFERSIZE
#include "GameShared/GameClasses/Development/CgsStrStream.h" // CgsDev::StrStream (OOM message build)
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"     // CgsMemory::HeapMalloc
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysMemoryManager.h" // AttribSysMemoryManager::HasMemoryBuffer (deallocate)

namespace CgsAttribSys
{
// Reset to the unprepared state for the given consumer package (the X360 inlines these
// stores into AttribSysMemoryManager::Prepare @0x828043B8 right before each package's
// Prepare call -- no standalone symbol): null heap, not-live, package id, zeroed totals.
void AttribSysPackageAllocator::Construct(EAttribSysUserPackage leUserPackage)
{
    mpHeapAllocator = NULL;
    mbHasAllocator  = false;
    meUserPackage   = leUserPackage;
    miAllocTotal    = 0;
    miFreeTotal     = 0;
}

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

// @ 0x82803F18 -- AttribSysPackageAllocator::deallocate(void*, size_t): the EASTL container
// allocator adapter's deallocate hook. Caller: the AttribSys database garbage-collect path
// with list<Attrib::Collection*, AttribSysPackageAllocator>. Asserts the AttribSys memory
// manager has been Prepare'd (sbHasLinearAllocator, CgsAttribSysMemoryManager.h:173 -- read
// through the committed HasMemoryBuffer() getter because sbHasLinearAllocator is PRIVATE) and
// the package allocator is live (mbHasAllocator @ +0x04, CgsAttribSysPackageAllocator.h:324),
// frees the block through the adopted heap, and adds 12 (one list-node's worth) to miFreeTotal
// @ +0x10. Both asserts collapse to one CGS_ASSERT each (file/line dropped per house rule).
//
// NOTE (verifier): the X360 reaches the package-allocator instance by FIXED static address
// (dword_83011B7C, a dedicated EASTL-package static) and IGNORES its own `this`. Modelled here
// as a normal member so the EASTL list can call it on its stored (prepared, singleton)
// allocator; behaviour matches provided the stored allocator is the prepared instance. The
// size argument is accepted for the EASTL interface but unused (only the +12 accounting fires).
void AttribSysPackageAllocator::deallocate(void* lpBlock, size_t /*lnSize*/)
{
    CGS_ASSERT(AttribSysMemoryManager::HasMemoryBuffer(), "sbHasLinearAllocator");
    CGS_ASSERT(mbHasAllocator, "mbHasAllocator");

    mpHeapAllocator->Free(lpBlock);
    miFreeTotal += 12;
}

// The size-accounting free inlined at the AttribSys edit teardown site
// (Attrib::EditRecord::~EditRecord @ 0x8280DE58). Asserts the package allocator is
// live (mbHasAllocator @ +0x04), forwards the block to the adopted heap, and adds
// lnSize to miFreeTotal @ +0x10 -- the free-side mirror of Malloc's miAllocTotal += size.
// The caller reaches this instance through GetAttribSysAllocator(), which already fired
// the sbHasLinearAllocator assert, so it is not repeated here (matching the X360 inline).
// The exact vendor symbol name is not independently attested. FLAG.
void AttribSysPackageAllocator::FreeSized(void* lpBlock, size_t lnSize)
{
    CGS_ASSERT(mbHasAllocator, "mbHasAllocator");

    mpHeapAllocator->Free(lpBlock);
    miFreeTotal += static_cast<s32>(lnSize);
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
