#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"

namespace CgsAttribSys
{
// Reconstructed from the DecFIGS DWARF (CgsAttribSysVaultAllocator.h). Only the
// layout (so it can be embedded by value in VaultArray) and Construct() (called by
// VaultArray::Construct, which the X360 build inlines) are modelled here. The
// remaining methods — Prepare/GetFreeSlot/ReleaseSlot/GetSlotMemory — belong to the
// allocator's own TUs and are declared there when reconstructed.
class StreamedVaultAllocator
{
public:
    typedef CgsContainers::BitArray<24u> UsedStreamedVaults;

    void Construct();

private:
    UsedStreamedVaults mUsedStreamedVaults;
    u8*                mpau8VaultMemory;
};
}
