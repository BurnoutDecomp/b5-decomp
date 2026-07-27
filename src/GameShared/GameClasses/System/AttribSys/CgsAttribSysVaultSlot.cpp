#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultSlot.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h" // CgsResource::ResourceHandle::GetResourceId
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysModuleIO.h" // AttribSysIO::RegisterVaultRequest

// CgsAttribSys::VaultSlot member functions, reconstructed from BURNOUT_X360_ARTIST.XEX.
// A VaultSlot tracks one loaded (or free) vault: a live reference count, an optional
// streamed-vault index, and the loaded vault's resource id. A slot is "occupied" when
// its reference count is above zero.

namespace CgsAttribSys
{

// The static streamed-vault-allocator registry (X360 dword_83011B60): published by
// VaultArray::Construct (which inlines this setter as a direct store), read by DoLoad's
// streamed-vault path.
StreamedVaultAllocator* VaultSlot::spVaultAllocator = nullptr;

void VaultSlot::SetVaultAllocator(StreamedVaultAllocator* lpAllocator)
{
    spVaultAllocator = lpAllocator;
}

// Reset this slot to the free state. The X360 inlines the four stores into
// VaultArray::Prepare's per-slot loop (@0x82805CB0: `*(v11+8)=0; *(v11+12)=0;
// *(v11+16)=-1; *v11=0` = mpVault/miRefCount/miStreamedVaultIndex/mResourceId),
// with this method's own allocator guard baked per iteration
// (CgsAttribSysVaultSlot.cpp:52 "lpAllocator != NULL").
void VaultSlot::Construct(CgsMemory::LinearMalloc* lpAllocator)
{
    CGS_ASSERT(lpAllocator != nullptr, "lpAllocator != NULL");   // .cpp:52
    mpVault              = nullptr;
    miRefCount           = 0;
    miStreamedVaultIndex = -1;
    mResourceId.SetHash(0);
}

// @ 0x82802390 -- an occupied slot reports whether it holds a streamed vault. An
// unoccupied slot must never carry a streamed-vault index.
bool VaultSlot::ContainsStreamedVault() const
{
    if (miRefCount > 0)   // IsOccupied()
        return miStreamedVaultIndex != -1;

    CGS_ASSERT(miStreamedVaultIndex == -1, "miStreamedVaultIndex == -1");
    return false;
}

// @ 0x828021F8 -- only an occupied slot can match a resource. The 64-bit resource-id
// hashes are compared directly (X360 cmpld of the full ID).
bool VaultSlot::ContainsVaultResource(CgsResource::ResourceHandle lVaultResHandle) const
{
    return miRefCount > 0 && mResourceId == lVaultResHandle.GetResourceId();
}

// @ 0x8280E870 -- register interest in the request's vault. Validates the request, then
// either bumps the ref count (if this slot already holds that vault) or loads it. Returns
// the resulting ref count. Called by VaultArray::RegisterVault.
s32 VaultSlot::RegisterVault(const AttribSysIO::RegisterVaultRequest* lpRegisterRequest,
                            Attrib::IGarbageCollector* lpGarbageCollector)
{
    CGS_ASSERT(lpRegisterRequest != nullptr, "lpRegisterRequest != NULL");
    CGS_ASSERT(lpGarbageCollector != nullptr, "lpGarbageCollector != NULL");

    CgsResource::ResourceHandle lVaultResHandle = lpRegisterRequest->mVaultResourceHandle;

    CGS_ASSERT(!(IsOccupied() && !ContainsVaultResource(lVaultResHandle)),
        "!( IsOccupied() && !ContainsVaultResource( lVaultResHandle ) )");

    if (ContainsVaultResource(lVaultResHandle))
        IncreaseRefCount();
    else
        DoLoad(lpRegisterRequest, lpGarbageCollector);

    return miRefCount;
}

// @ 0x82802288 -- release one reference; the slot must currently be loaded.
void VaultSlot::DecreaseRefCount()
{
    CGS_ASSERT(miRefCount > 0,
        "Trying to decrease the ref count on a vault that isn't loaded");
    --miRefCount;
}

// @ 0x82802330 -- acquire one reference.
void VaultSlot::IncreaseRefCount()
{
    CGS_ASSERT(miRefCount >= 0, "miRefCount >= 0");
    ++miRefCount;
}

} // namespace CgsAttribSys
