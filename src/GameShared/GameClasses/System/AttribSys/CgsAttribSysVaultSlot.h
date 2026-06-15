#pragma once

namespace CgsAttribSys
{
class StreamedVaultAllocator;

// Owning header for VaultSlot, reconstructed from the DecFIGS DWARF
// (CgsAttribSysVaultSlot.h). Only the static vault-allocator registry — the
// spVaultAllocator pointer and its SetVaultAllocator setter, which VaultArray::Construct
// publishes into — is modelled here. VaultSlot's instance members (mResourceId, the
// Vault payload, ...) and its other methods belong to the VaultSlot TU and are added
// to this header when it is reconstructed.
struct VaultSlot
{
    static StreamedVaultAllocator* spVaultAllocator;
    static void SetVaultAllocator(StreamedVaultAllocator* lpAllocator);
};
}
