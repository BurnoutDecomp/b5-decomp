#include "GameShared/GameClasses/Memory/MemoryMap/CgsMemoryMap.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h" // CgsResource::GetLoadBase (rw::Resource load base)
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CgsDev::Assert Begin/Fire/End + KI_MESSAGEBUFFERSIZE
#include "GameShared/GameClasses/Development/CgsStrStream.h"             // CgsDev::StrStream (runtime-formatted assert message)

// Reconstructed from the DecFIGS DWARF (CgsMemoryMap.h/.cpp) + the X360 pseudocode
//   CgsMemory::MemoryMap::FixUp @ 0x8286C8B8
//
// Relocate the on-disk memory map's seven element-array pointers (mpBanks ..
// mpRWGeneralAllocators) by the resource load base. The X360 first asserts the loaded
// version equals KI_MEMORY_MAP_VERSION (== 0): the assert text embeds the runtime
// miVersion, so it is built through a CgsDev::StrStream into a stack message buffer and
// fired with the explicit Begin/Fire/End triad (matching the committed BrnCgsPlayerName
// precedent), NOT CGS_ASSERT. Each pointer is then relocated with the same uintptr-word
// arithmetic the committed CgsShaderConstants FixUps use: a non-null pointer gets the
// 32-bit load base added, a null pointer stays null.

namespace CgsMemory
{

// CgsMemoryMap.h:48 - the assert message reads "current version is 0".
const s32 MemoryMap::KI_MEMORY_MAP_VERSION = 0;

// CgsMemoryMap.cpp / X360 @ 0x8286C8B8
void MemoryMap::FixUp(const rw::Resource& lrResource)
{
    // Version guard. X360 `if (*result)` tests dword 0 (miVersion); the only supported
    // version is KI_MEMORY_MAP_VERSION (0), so a non-zero / mismatched version asserts.
    if (miVersion != KI_MEMORY_MAP_VERSION)
    {
        CgsDev::Assert::BeginAssert();
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "Memory map is version " << miVersion
                   << ", current version is " << KI_MEMORY_MAP_VERSION << "\n";
        CgsDev::Assert::FireAssert(
            lacMessageBuffer,
            "..\\..\\..\\GameShared\\GameClasses\\Memory\\MemoryMap\\CgsMemoryMap.cpp",
            43);
        CgsDev::Assert::EndAssert();
    }

    // Relocation delta: the rw::Resource's load base. The X360 adds this to each non-null
    // element-array pointer (dwords 9-15); null stays null.
    const uintptr_t luBase = static_cast<uintptr_t>(CgsResource::GetLoadBase(lrResource));

    if (mpBanks)
        mpBanks = reinterpret_cast<MemoryMapBank*>(reinterpret_cast<uintptr_t>(mpBanks) + luBase);
    if (mpPools)
        mpPools = reinterpret_cast<MemoryMapPool*>(reinterpret_cast<uintptr_t>(mpPools) + luBase);
    if (mpRawResources)
        mpRawResources = reinterpret_cast<MemoryMapRawResource*>(reinterpret_cast<uintptr_t>(mpRawResources) + luBase);
    if (mpLinearAllocators)
        mpLinearAllocators = reinterpret_cast<MemoryMapLinearAllocator*>(reinterpret_cast<uintptr_t>(mpLinearAllocators) + luBase);
    if (mpHeapAllocators)
        mpHeapAllocators = reinterpret_cast<MemoryMapHeapAllocator*>(reinterpret_cast<uintptr_t>(mpHeapAllocators) + luBase);
    if (mpRWLinearAllocators)
        mpRWLinearAllocators = reinterpret_cast<MemoryMapRWLinearAllocator*>(reinterpret_cast<uintptr_t>(mpRWLinearAllocators) + luBase);
    if (mpRWGeneralAllocators)
        mpRWGeneralAllocators = reinterpret_cast<MemoryMapRWGeneralAllocator*>(reinterpret_cast<uintptr_t>(mpRWGeneralAllocators) + luBase);
}

// CgsMemoryMap.h:66 / X360 @ 0x82663C88 (BrnResource::GameDataModule::CreateBanks)
// Return the bank record at liIndex. The X360 bounds-checks against (muNumBanks - 1) and
// indexes the array at (liIndex + 1): bank slot 0 is the reserved root, so the public
// indices map to mpBanks[1 .. muNumBanks-1]. The out-of-range message embeds the index, so
// it is rendered through a CgsDev::StrStream and fired with the Begin/Fire/End triad
// (matching FixUp above), NOT CGS_ASSERT.
const MemoryMapBank* MemoryMap::GetBank(s32 liIndex) const
{
    if (liIndex >= static_cast<s32>(muNumBanks) - 1)
    {
        CgsDev::Assert::BeginAssert();
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "Bank index " << liIndex << " is out of range\n";
        CgsDev::Assert::FireAssert(
            lacMessageBuffer,
            "..\\..\\..\\GameShared\\GameClasses\\Memory/MemoryMap/CgsMemoryMap.h",
            175);
        CgsDev::Assert::EndAssert();
    }

    // result = 84 * (liIndex + 1) + mpBanks  (X360: mulli 0x54 then add the array base)
    return mpBanks + (liIndex + 1);
}

// CgsMemoryMap.h:188 / X360 @ 0x82663D60 (BrnResource::GameDataModule::CreatePools)
// Return the pool record at liIndex. The X360 bounds-checks against muNumPools and indexes
// the array directly. Same StrStream-formatted assert idiom as GetBank.
const MemoryMapPool* MemoryMap::GetPool(s32 liIndex) const
{
    if (liIndex >= static_cast<s32>(muNumPools))
    {
        CgsDev::Assert::BeginAssert();
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "Pool index " << liIndex << " is out of range\n";
        CgsDev::Assert::FireAssert(
            lacMessageBuffer,
            "..\\..\\..\\GameShared\\GameClasses\\Memory/MemoryMap/CgsMemoryMap.h",
            188);
        CgsDev::Assert::EndAssert();
    }

    // result = 172 * liIndex + mpPools  (X360: mulli 0xAC then add the array base)
    return mpPools + liIndex;
}

}
