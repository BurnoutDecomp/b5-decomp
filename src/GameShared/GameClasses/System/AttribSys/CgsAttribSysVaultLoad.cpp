// ===========================================================================
// CgsAttribSysVaultLoad.cpp -- the vault-array load/unload interior:
//   CgsAttribSys::VaultArray::RegisterVault   @ 0x8280E978
//   CgsAttribSys::VaultArray::UnregisterVault @ 0x8280F6C8
//   CgsAttribSys::VaultSlot::DoLoad           @ 0x8280E060
//   CgsAttribSys::VaultSlot::DoUnload         @ 0x8280F1B8
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (attrib-sdk wave 2026-07-27).
//
// STAGED TU -- NOT in tools/build/build_game_exe.bat yet: WorldLinkStubs.cpp
// still carries the link stubs for RegisterVault/UnregisterVault/DoLoad (and
// for the Attrib SDK symbols these bodies pull). Mount order (one change set):
//   1. delete the AttribSys stub cluster from WorldLinkStubs.cpp,
//   2. add this TU + the Attrib SDK runtime TUs to the exe source list,
//   3. flip CgsAttribSys::KB_PC_ATTRIB_SCHEMA_FILES (CgsAttribSysModule.h).
// See the attrib-sdk wave log for the exact list. The bodies split naturally
// into their home TUs (CgsAttribSysVaultArray.cpp / CgsAttribSysVaultSlot.cpp)
// once the stubs are gone; they are kept together here so the stub deletion is
// a single-file, single-step swap.
// ===========================================================================

#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultArray.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultSlot.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultAllocator.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysModuleIO.h"       // Register/UnregisterVaultRequest
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysMemoryManager.h"  // GetAttribSysAllocator
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysPackageAllocator.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"              // ResourcePtr (CreateFromHandle binding)
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                         // SPrintf (resource-id hex in the load logs)
#include "GameShared/GameClasses/Development/CgsStrStream.h"                    // StrStreamBase (debug prints)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                      // gpDebugPrint / gxMessageFilterFlags
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attribsys.h"        // Attrib::Database (export policies)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribloadandgo.h" // Attrib::Vault

#include <new>   // placement new (Vault over the package-allocator block)

namespace CgsAttribSys
{
namespace
{
    // The serialised AttribSysVault RESOURCE head (WORLDVAULT.BIN /
    // SURFACELIST.BIN payloads; CgsAttribSysVaultResourceType's descriptor
    // reads the same four words): {vltOffset, vltSize, binOffset, binSize}.
    // [FLAG PC seam] the X360 FixUp for resource type 0x1C rewrites the two
    // offset words into pointers before DoLoad runs; the PC FixUp override is
    // not committed yet, so the words still hold the serialised RESOURCE-
    // RELATIVE offsets and the accessors add the base explicitly. When the real
    // FixUp lands, switch these to the fixed-up-slot (PointerFromU32) reads.
    struct AttribSysVaultResource
    {
        u32 muVltOffset;   // +0x00
        u32 muVltSize;     // +0x04
        u32 muBinOffset;   // +0x08
        u32 muBinSize;     // +0x0C

        u8* GetVltData() const
        {
            return const_cast<u8*>(reinterpret_cast<const u8*>(this)) + muVltOffset;
        }
        u8* GetBinData() const
        {
            return const_cast<u8*>(reinterpret_cast<const u8*>(this)) + muBinOffset;
        }
    };

    // Hex form of a 64-bit resource id for the [ATTRIBSYS ...] logs (the X360
    // streams it through CgsResource::operator<<(StrStreamBase&, ID); same
    // printed text via the committed VaultArray-dump SPrintf idiom).
    void AppendResourceIdHex(CgsDev::StrStreamBase& lrStream, const CgsResource::ID& lrId)
    {
        const u8* lpBytes = reinterpret_cast<const u8*>(&lrId);
        char lacHex[32];
        CgsCore::SPrintf(lacHex, 32, "%02x%02x%02x%02x%02x%02x%02x%02x",
                         lpBytes[0], lpBytes[1], lpBytes[2], lpBytes[3],
                         lpBytes[4], lpBytes[5], lpBytes[6], lpBytes[7]);
        lrStream << lacHex;
    }
}

// @ 0x8280E060 -- load the request's vault into this (currently free) slot:
// bind the vault resource from the handle, carve the Attrib::Vault object from
// the AttribSys package allocator, construct it over the resource's .vlt image
// (type from the request; the world data vaults are RESIDENT), resolve the .bin
// dependency (a STREAMED vault first stages the bin into a streamed-vault slot),
// Initialize it into the live attribute database, then take the first reference
// and record the resource identity.
void VaultSlot::DoLoad(const AttribSysIO::RegisterVaultRequest* lpRegisterRequest,
                       Attrib::IGarbageCollector* lpGarbageCollector)
{
    CGS_ASSERT(!IsOccupied(), "!IsOccupied()");                                    // .cpp:158
    CGS_ASSERT(lpGarbageCollector != nullptr, "lpGarbageCollector != NULL");       // .cpp:161

    // Bind the resource memory from the request's handle (the X360 builds the
    // list-linked resource ptr on the stack; the dtor unlinks it exactly as the
    // X360's inline tail does).
    CgsResource::ResourceHandle lVaultResHandle = lpRegisterRequest->mVaultResourceHandle;
    CgsResource::ResourcePtr<AttribSysVaultResource> lVaultResource(lVaultResHandle);

    // Carve the vault object from the static AttribSys package allocator
    // (X360 Malloc(88); sizeof-based for the widened x64 object).
    AttribSysPackageAllocator* lpAllocator =
        AttribSysMemoryManager::GetAttribSysAllocator();
    void* lpVaultMem = lpAllocator->Malloc(sizeof(Attrib::Vault), 0);

    CGS_ASSERT(mpVault == nullptr, "mpVault == NULL");                             // .cpp:177

    const AttribSysVaultResource* lpResource = lVaultResource.operator->();
    if (lpVaultMem != nullptr)
    {
        // The X360 fetches the export policies through the database accessor
        // (the schema made the database live before any data vault registers).
        Attrib::ExportManager& lrPolicies = Attrib::Database::Get().GetExportPolicies();
        mpVault = new (lpVaultMem) Attrib::Vault(
            lrPolicies,
            /*lAssetId*/ 0,
            lpResource->GetVltData(),
            lpResource->muVltSize,
            /*lbType*/ 1,
            lpGarbageCollector);
    }
    else
    {
        mpVault = nullptr;
    }
    CGS_ASSERT(mpVault == lpVaultMem, "mpVault == lpVaultMem");                    // .cpp:189

    if (mpVault->HasUnresolvedDependency())
    {
        u8* lpBinData = lpResource->GetBinData();
        if (lpRegisterRequest->meVaultType == AttribSysIO::E_VAULT_TYPE_STREAMED)
        {
            // Streamed vaults stage the bin payload into a streamed-vault slot.
            miStreamedVaultIndex =
                spVaultAllocator->GetFreeSlot(lpBinData, lpResource->muBinSize);
            lpBinData = spVaultAllocator->GetSlotMemory(miStreamedVaultIndex);
        }
        mpVault->ResolveDependency(0, lpBinData, lpResource->muBinSize, 1);
    }
    CGS_ASSERT(!mpVault->HasUnresolvedDependency(),
               "!mpVault->HasUnresolvedDependency()");                             // .cpp:222

    mpVault->Initialize();
    ++miRefCount;
    mResourceId = lVaultResHandle.GetResourceId();

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        CgsDev::StrStreamBase& lrStream = *CgsDev::Log::gpDebugPrint;
        lrStream << "\n[ATTRIBSYS LOAD] Just loaded vault resource with ID ";
        AppendResourceIdHex(lrStream, mResourceId);
        lrStream << ", for the first time, ref count is ";
        lrStream << miRefCount;
        lrStream << "\n";
    }
}

// @ 0x8280F1B8 -- actually unload this slot's vault (last reference): tear the
// vault out of the live database, drop its object refcount (destroying the
// 88-byte object through the deleting destructor on the final drop), collect
// the database garbage, release any streamed-vault slot, then free the slot.
void VaultSlot::DoUnload()
{
    CGS_ASSERT(miRefCount == 1, "miRefCount == 1");                                // .cpp:259

    mpVault->Deinitialize();
    if (mpVault->Release(0))
        Attrib::Vault_ScalarDeletingDtor(mpVault, 1);

    CGS_ASSERT(Attrib::Database::IsInitialized(), "Attribute database not initialized.");
    Attrib::Database::Get().CollectGarbage();

    if (ContainsStreamedVault())
    {
        spVaultAllocator->ReleaseSlot(miStreamedVaultIndex);
        miStreamedVaultIndex = -1;
    }

    DecreaseRefCount();
    mpVault = nullptr;

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        CgsDev::StrStreamBase& lrStream = *CgsDev::Log::gpDebugPrint;
        lrStream << "\n[ATTRIBSYS UNLOAD] Actually unloaded vault resource with ID ";
        AppendResourceIdHex(lrStream, mResourceId);
        lrStream << ", ref count is ";
        lrStream << miRefCount;
        lrStream << "\n";
    }

    mResourceId = CgsResource::ID();
    mResourceId.SetHash(0);
}

// @ 0x8280E978 -- register the request's vault into a slot: the slot already
// holding the resource (ref-count bump) or the first free one (VaultSlot::DoLoad).
void VaultArray::RegisterVault(AttribSysIO::RegisterVaultRequest* lpRegisterVaultRequest)
{
    CGS_ASSERT(mbPrepared,
               "Trying to register a vault slot before Prepare() has been called"); // .cpp:168

    const s32 liSlot =
        GetFreeSlotIndex(lpRegisterVaultRequest->mVaultResourceHandle);
    VaultSlot& lrSlot = mpaSlots[liSlot];
    const s32 liRefCount =
        lrSlot.RegisterVault(lpRegisterVaultRequest, mpGarbageCollector);

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        CgsDev::StrStreamBase& lrStream = *CgsDev::Log::gpDebugPrint;
        lrStream << "\n[ATTRIBSYS LOAD]: Increased ref count on slot ";
        lrStream << liSlot;
        lrStream << " containing resource ";
        AppendResourceIdHex(lrStream, lrSlot.GetResourceId());
        lrStream << " to ";
        lrStream << liRefCount;
        lrStream << "\n";
    }
}

// @ 0x8280F6C8 -- drop the vault named by the request: find the occupied slot
// whose resource id matches (the X360 compares the ids' low 32-bit words), then
// either fully unload it (last reference) or just decrement the count.
void VaultArray::UnregisterVault(AttribSysIO::UnregisterVaultRequest* lpUnregisterVaultRequest)
{
    CGS_ASSERT(mbPrepared,
               "Trying to unregister a vault slot before Prepare() has been called"); // .cpp:200

    const u32 luRequestIdLow = static_cast<u32>(
        lpUnregisterVaultRequest->mVaultResourceHandle.GetResourceId().GetHash());

    for (s32 liSlot = 0; liSlot < GetNumSlots(); ++liSlot)
    {
        VaultSlot& lrSlot = mpaSlots[liSlot];
        if (lrSlot.GetRefCount() > 0 &&
            static_cast<u32>(lrSlot.GetResourceId().GetHash()) == luRequestIdLow)
        {
            if (lrSlot.GetRefCount() <= 1)
                lrSlot.DoUnload();
            else
                lrSlot.DecreaseRefCount();

            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                CgsDev::StrStreamBase& lrStream = *CgsDev::Log::gpDebugPrint;
                lrStream << "\n[ATTRIBSYS UNLOAD]: Decreased ref count on slot ";
                lrStream << liSlot;
                lrStream << " containing resource ";
                AppendResourceIdHex(lrStream, lrSlot.GetResourceId());
                lrStream << " to ";
                lrStream << lrSlot.GetRefCount();
                lrStream << "\n";
            }
            return;
        }
    }

    CGS_ASSERT(false, "Tried to unregister a vault that wasn't loaded into a slot");  // .cpp:225
}

// StreamedVaultAllocator::GetFreeSlot @ 0x82808638 -- stage a streamed vault's
// bin payload into a free streamed slot. Own TU (the free-list walk + copy);
// only STREAMED vault registration reaches it -- the world data vaults are
// RESIDENT -- so it defers with the vault-allocator slice.
s32 StreamedVaultAllocator::GetFreeSlot(u8* lpau8BinData, u32 lu16BinSizeInBytes)
{
    (void)lpau8BinData; (void)lu16BinSizeInBytes;
    CGS_ASSERT(false, "StreamedVaultAllocator::GetFreeSlot @0x82808638: deferred TU (streamed vaults)");
    return -1;
}
}
