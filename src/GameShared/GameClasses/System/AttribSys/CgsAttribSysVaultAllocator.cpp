#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultAllocator.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsAttribSys
{
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
