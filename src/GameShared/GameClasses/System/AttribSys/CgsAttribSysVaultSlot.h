#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"      // CgsResource::ID (mResourceId)
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"  // CgsResource::ResourceHandle (ContainsVaultResource param)

namespace Attrib { class Vault; }
namespace Attrib { struct IGarbageCollector; }   // struct -- must match attribloadandgo.h's class-key (MSVC mangling)
namespace CgsMemory { class LinearMalloc; }

namespace CgsAttribSys
{
namespace AttribSysIO { struct RegisterVaultRequest; }
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

    // Reset this slot to the free state (X360 inlines the four stores into
    // VaultArray::Prepare's per-slot loop @0x82805CB0: mpVault=0, miRefCount=0,
    // miStreamedVaultIndex=-1, mResourceId=0 -- plus the per-slot
    // "lpAllocator != NULL" assert baked at CgsAttribSysVaultSlot.cpp:52, which is
    // this method's own allocator guard).
    void Construct(CgsMemory::LinearMalloc* lpAllocator);

    // @ 0x82802390 -- an occupied slot reports whether it holds a streamed vault;
    // an unoccupied slot asserts it carries no streamed-vault index.
    bool ContainsStreamedVault() const;

    // @ 0x828021F8 -- true when this (occupied) slot holds the given vault resource.
    bool ContainsVaultResource(CgsResource::ResourceHandle lVaultResHandle) const;

    // @ 0x8280E870 -- register interest in the request's vault. If this slot already holds
    // that vault, bumps the ref count; otherwise loads it. Returns the resulting ref count.
    s32 RegisterVault(const AttribSysIO::RegisterVaultRequest* lpRegisterRequest,
                      Attrib::IGarbageCollector* lpGarbageCollector);

    // A slot is occupied while at least one reference is live.
    bool IsOccupied() const { return miRefCount > 0; }

    // Named read accessors for the VaultArray debug dump (@0x82803888) -- the X360
    // inlines the raw field loads there; with the full slot layout committed the dump
    // reads by name through these.
    const CgsResource::ID& GetResourceId()         const { return mResourceId; }
    s32                    GetRefCount()           const { return miRefCount; }
    s32                    GetStreamedVaultIndex() const { return miStreamedVaultIndex; }

private:
    // Load the request's vault into this (currently free) slot. Body in its own TU.
    void DoLoad(const AttribSysIO::RegisterVaultRequest* lpRegisterRequest,
                Attrib::IGarbageCollector* lpGarbageCollector);

public:
    // @ 0x82802288 / 0x82802330 -- adjust the slot's live reference count.
    void DecreaseRefCount();
    void IncreaseRefCount();

private:
    // ---- instance members (touched by the VaultSlot TU) ----
    // X360 layout: mResourceId @+0x00 (8B), mpVault @+0x08, miRefCount @+0x0C,
    // miStreamedVaultIndex @+0x10 -- slot stride 24 (matches VaultArray::operator<<'s
    // raw-offset dump). RegisterVault reads IsOccupied() at +0x0C.
    CgsResource::ID mResourceId;           // +0x00 the loaded vault's identity hash
    Attrib::Vault*  mpVault;               // +0x08 the loaded vault payload (0 when free)
    s32             miRefCount;            // +0x0C live ref count; > 0 == occupied
    s32             miStreamedVaultIndex;  // +0x10 streamed-vault index, or -1 for none
};
}
