#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"      // CgsResource::ID (mResourceId)
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"  // CgsResource::ResourceHandle (ContainsVaultResource param)

namespace CgsAttribSys
{
class StreamedVaultAllocator;

// Owning header for VaultSlot, reconstructed from the DecFIGS DWARF
// (CgsAttribSysVaultSlot.h). Carries the static vault-allocator registry (the
// spVaultAllocator pointer + its SetVaultAllocator setter, which VaultArray::Construct
// publishes into) plus the slot's instance bookkeeping the VaultSlot TU bodies:
//   miRefCount           -- live reference count; > 0 == occupied (IsOccupied).
//   miStreamedVaultIndex -- streamed-vault index, or -1 when the slot holds none.
//   mResourceId          -- the loaded vault's 64-bit resource-id hash.
// The Vault payload + the slot's remaining members belong to their own reconstruction
// and are added here (additively) when they land; only the members these four TU
// functions touch are modelled now.
struct VaultSlot
{
    static StreamedVaultAllocator* spVaultAllocator;
    static void SetVaultAllocator(StreamedVaultAllocator* lpAllocator);

    // @ 0x82802390 -- an occupied slot reports whether it holds a streamed vault;
    // an unoccupied slot asserts it carries no streamed-vault index.
    bool ContainsStreamedVault() const;

    // @ 0x828021F8 -- true when this (occupied) slot holds the given vault resource.
    bool ContainsVaultResource(CgsResource::ResourceHandle lVaultResHandle) const;

    // @ 0x82802288 / 0x82802330 -- adjust the slot's live reference count.
    void DecreaseRefCount();
    void IncreaseRefCount();

private:
    // ---- instance members (touched by the VaultSlot TU) ----
    CgsResource::ID mResourceId;           // the loaded vault's identity hash
    s32             miRefCount;            // live ref count; > 0 == occupied
    s32             miStreamedVaultIndex;  // streamed-vault index, or -1 for none
};
}
