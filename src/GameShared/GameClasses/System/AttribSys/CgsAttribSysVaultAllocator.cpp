#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultAllocator.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"   // LinearMalloc::Malloc/GetAlignment (Prepare)

namespace CgsAttribSys
{
// Reset the allocator (the X360 inlines these stores into VaultArray::Construct
// @0x8280AA60 -- no standalone symbol): every slot bit clear, no backing memory.
void StreamedVaultAllocator::Construct()
{
    mUsedStreamedVaults.UnSetAll();
    mpau8VaultMemory = NULL;
}

// @ 0x82803C18 -- carve the streamed-vault bin block (24 x 4096 = 98304 bytes) out of the
// AttribSys linear allocator. Asserts the allocator pointer, that its current alignment is
// at least 16, and that the carve succeeded.
void StreamedVaultAllocator::Prepare(CgsMemory::LinearMalloc* lpAllocator)
{
    CGS_ASSERT(lpAllocator != NULL, "lpAllocator != NULL");                       // .cpp:65
    CGS_ASSERT(lpAllocator->GetAlignment() >= 16, "lpAllocator->GetAlignment() >= 16");   // .cpp:66

    mpau8VaultMemory = static_cast<u8*>(
        lpAllocator->Malloc(KI_MAX_NUM_STREAMED_VAULTS * KI_MAX_STREAMED_VAULT_BIN_SIZE));
    CGS_ASSERT(mpau8VaultMemory != NULL, "mpau8VaultMemory != NULL");             // .cpp:77
}
// Returns a pointer to the 4 KiB bin backing the given streamed-vault slot. The
// X360 build inlines the BitArray bounds check and IsBitSet probe; both are
// restored here as the logical guard + container call.
u8* StreamedVaultAllocator::GetSlotMemory(s32 liSlotIndex)
{
    CGS_ASSERT(liSlotIndex >= 0 && liSlotIndex < KI_MAX_NUM_STREAMED_VAULTS,
               "liSlotIndex >= 0 && liSlotIndex < KI_MAX_NUM_STREAMED_VAULTS");

    // Inlined BitArray<24u>::IsBitSet bounds check (CgsBitArray.h:203).
    CGS_ASSERT(static_cast<u32>(liSlotIndex) < mUsedStreamedVaults.GetCapacity(), "luIndex < NUMBITS");

    CGS_ASSERT(mUsedStreamedVaults.IsBitSet(liSlotIndex),
               "Trying to access memory for a slot that isn't in use");

    return mpau8VaultMemory + liSlotIndex * KI_MAX_STREAMED_VAULT_BIN_SIZE;
}

// Frees a previously allocated streamed-vault slot by clearing its in-use bit.
// The X360 build inlines the IsBitSet probe and the UnSetBit clear; both are
// restored here as logical container calls.
void StreamedVaultAllocator::ReleaseSlot(s32 liSlotIndex)
{
    CGS_ASSERT(liSlotIndex >= 0 && liSlotIndex < KI_MAX_NUM_STREAMED_VAULTS,
               "liSlotIndex >= 0 && liSlotIndex < KI_MAX_NUM_STREAMED_VAULTS");

    // Inlined BitArray<24u>::IsBitSet bounds check (CgsBitArray.h:203).
    CGS_ASSERT(static_cast<u32>(liSlotIndex) < mUsedStreamedVaults.GetCapacity(), "luIndex < NUMBITS");

    CGS_ASSERT(mUsedStreamedVaults.IsBitSet(liSlotIndex),
               "Trying to free a streamed vault slot that isn't in use");

    // Inlined BitArray<24u>::UnSetBit bounds check (CgsBitArray.h:241).
    CGS_ASSERT(static_cast<u32>(liSlotIndex) < mUsedStreamedVaults.GetCapacity(), "luIndex < NUMBITS");

    mUsedStreamedVaults.UnSetBit(liSlotIndex);
}
}
