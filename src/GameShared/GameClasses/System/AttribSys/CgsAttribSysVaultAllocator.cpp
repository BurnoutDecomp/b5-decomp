#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultAllocator.h"

namespace CgsDev
{
// CgsAssert.h is still a placeholder ("TODO: Uncomment when asserts are
// implemented"), so — matching every other reconstructed caller (e.g.
// CgsAttribSysVaultArray, BrnWorldRegion) — the minimal Assert API is declared
// locally here until the real assert system is reconstructed.
class Assert
{
public:
    static void BeginAssert();
    static void FireAssert(const char* lpcExpression, const char* lpcFile, int liLine);
    static void EndAssert();
};
}

namespace CgsAttribSys
{
// Returns a pointer to the 4 KiB bin backing the given streamed-vault slot. The
// X360 build inlines the BitArray bounds check and IsBitSet probe; both are
// restored here as the logical guard + container call.
u8* StreamedVaultAllocator::GetSlotMemory(s32 liSlotIndex)
{
    if (liSlotIndex < 0 || liSlotIndex >= KI_MAX_NUM_STREAMED_VAULTS)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "liSlotIndex >= 0 && liSlotIndex < KI_MAX_NUM_STREAMED_VAULTS",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\attribsys\\CgsAttribSysVaultAllocator.cpp",
            165);
        CgsDev::Assert::EndAssert();
    }

    // Inlined BitArray<24u>::IsBitSet bounds check (CgsBitArray.h:203).
    if (static_cast<u32>(liSlotIndex) >= mUsedStreamedVaults.GetCapacity())
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "luIndex < NUMBITS",
            "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsBitArray.h",
            203);
        CgsDev::Assert::EndAssert();
    }

    if (!mUsedStreamedVaults.IsBitSet(liSlotIndex))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "Trying to access memory for a slot that isn't in use",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\attribsys\\CgsAttribSysVaultAllocator.cpp",
            166);
        CgsDev::Assert::EndAssert();
    }

    return mpau8VaultMemory + liSlotIndex * KI_MAX_STREAMED_VAULT_BIN_SIZE;
}

// Frees a previously allocated streamed-vault slot by clearing its in-use bit.
// The X360 build inlines the IsBitSet probe and the UnSetBit clear; both are
// restored here as logical container calls.
void StreamedVaultAllocator::ReleaseSlot(s32 liSlotIndex)
{
    if (liSlotIndex < 0 || liSlotIndex >= KI_MAX_NUM_STREAMED_VAULTS)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "liSlotIndex >= 0 && liSlotIndex < KI_MAX_NUM_STREAMED_VAULTS",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\attribsys\\CgsAttribSysVaultAllocator.cpp",
            137);
        CgsDev::Assert::EndAssert();
    }

    // Inlined BitArray<24u>::IsBitSet bounds check (CgsBitArray.h:203).
    if (static_cast<u32>(liSlotIndex) >= mUsedStreamedVaults.GetCapacity())
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "luIndex < NUMBITS",
            "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsBitArray.h",
            203);
        CgsDev::Assert::EndAssert();
    }

    if (!mUsedStreamedVaults.IsBitSet(liSlotIndex))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "Trying to free a streamed vault slot that isn't in use",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\attribsys\\CgsAttribSysVaultAllocator.cpp",
            138);
        CgsDev::Assert::EndAssert();
    }

    // Inlined BitArray<24u>::UnSetBit bounds check (CgsBitArray.h:241).
    if (static_cast<u32>(liSlotIndex) >= mUsedStreamedVaults.GetCapacity())
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "luIndex < NUMBITS",
            "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsBitArray.h",
            241);
        CgsDev::Assert::EndAssert();
    }

    mUsedStreamedVaults.UnSetBit(liSlotIndex);
}
}
