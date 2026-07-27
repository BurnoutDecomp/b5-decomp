#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"

namespace CgsMemory { class LinearMalloc; }

namespace CgsAttribSys
{
// Fixed-size pool allocator that hands out streamed-vault memory in equal-sized
// bins. Each of the KI_MAX_NUM_STREAMED_VAULTS slots maps to one
// KI_MAX_STREAMED_VAULT_BIN_SIZE-byte bin inside a single contiguous block, and a
// BitArray tracks which slots are currently in use.
//
// Layout and method set recovered from the DecFIGS DWARF
// (CgsAttribSysVaultAllocator.h); each method is X360-ledger attested:
//   - Construct        : inlined into VaultArray::Construct (no standalone symbol),
//                        but the VaultArray TU depends on this declaration.
//   - Prepare          @0x82803C18 (its own TU)
//   - GetFreeSlot      @0x82808638 (its own TU)
//   - ReleaseSlot      @0x82805E68 (reconstructed in this TU's .cpp)
//   - GetSlotMemory    @0x82806050 (reconstructed in this TU's .cpp)
// Bodies for Construct/Prepare/GetFreeSlot live in their own TUs and are declared
// here so callers compile; only ReleaseSlot/GetSlotMemory are defined alongside.
class StreamedVaultAllocator
{
public:
    // Each streamed vault bin is a fixed 4 KiB; there are 24 slots.
    static const s32 KI_MAX_STREAMED_VAULT_BIN_SIZE = 4096;
    static const s32 KI_MAX_NUM_STREAMED_VAULTS = 24;

    typedef CgsContainers::BitArray<24u> UsedStreamedVaults;

    void Construct();
    // (Parameter type fixed: the old `class LinearMalloc*` declared a phantom
    // CgsAttribSys::LinearMalloc -- the real allocator is CgsMemory::LinearMalloc,
    // per the X360 Prepare @0x82803C18 calling CgsMemory::LinearMalloc::Malloc.)
    void Prepare(CgsMemory::LinearMalloc* lpAllocator);
    s32 GetFreeSlot(u8* lpau8BinData, u32 lu16BinSizeInBytes);
    void ReleaseSlot(s32 liSlotIndex);
    u8* GetSlotMemory(s32 liSlotIndex);

private:
    UsedStreamedVaults mUsedStreamedVaults;
    u8*                mpau8VaultMemory;
};
}
