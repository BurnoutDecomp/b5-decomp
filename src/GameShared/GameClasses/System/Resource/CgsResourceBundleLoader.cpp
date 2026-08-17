#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoader.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"     // Pool, NewResource, Entry
#include "GameShared/GameClasses/System/Resource/CgsResourceBundle2.h"  // BundleV2
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"     // Type
#include "GameShared/GameClasses/System/FileSystem/CgsDeviceManager.h"  // async FS engine (ReadWholeFile)
#include "GameShared/GameClasses/System/FileSystem/CgsFileSystem.h"     // EnsureDeviceManagerUp
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // gpDebugPrint (read trace)

#include <cstdlib>   // malloc / free
#include <cstring>   // memcpy

namespace CgsResource
{
    // Read a whole bundle file through the LIVE async file-system engine (the DeviceManager worker
    // thread + OperationPool + Win32 DevicePhysicalPC leaf). EnsureDeviceManagerUp brings the
    // engine up if it is not already, so EVERY bundle load goes through it — including the early
    // movie/debug-font bootstrapping that runs before FileSystem::Prepare (no CRT fallback).
    // Returns a malloc'd buffer (caller free()s) + its size, or null on failure.
    static char* ReadBundleFile(const char* lpcFileName, long* lpOutSize)
    {
        CgsFileSystem::EnsureDeviceManagerUp();

        CgsFileSystem::DeviceManager* lpManager = CgsFileSystem::DeviceManager::GetIfInitialized();
        if (!lpManager)
            return 0;   // engine could not be brought up (catastrophic)

        u32   luSize = 0;
        void* lpBuf  = lpManager->ReadWholeFile(lpcFileName, &luSize);
        if (!lpBuf)
            return 0;

        if (CgsDev::Log::gpDebugPrint)
            *CgsDev::Log::gpDebugPrint << "[bundle] '" << lpcFileName
                                       << "' via async-FS (" << static_cast<s32>(luSize) << " bytes)\n";
        *lpOutSize = static_cast<long>(luSize);
        return static_cast<char*>(lpBuf);
    }
}

// The PC bundle loader. Read CgsResourceBundleLoader.h for how this relates to the X360
// streaming BundleLoaderModule (this is the synchronous PC IO form). The header validation
// mirrors X360 BundleLoaderModule::ProcessBundleHeader (0x828D7A90); the per-resource create
// / fixup / import sequence mirrors the X360 load path (Pool::CreateEntry, then the three
// FixUp / ResolveImports / PostFixUp passes that FixUpAndResolveResourceList runs in order).

namespace CgsResource
{
    s32 BundleLoader::LoadBundle(const char* lpcFileName, Pool* lpPool, FTypeResolver lpfnResolveType)
    {
        // ---- read the whole bundle file (through the live async FS engine; CRT leaf early) --
        long  llFileSize = 0;
        char* lpcBundle  = ReadBundleFile(lpcFileName, &llFileSize);
        if (lpcBundle == 0)
            return -1;
        if (llFileSize <= static_cast<long>(sizeof(BundleV2)))
        {
            free(lpcBundle);
            return -1;
        }

        // ---- validate the header (ProcessBundleHeader: must be a v2 bundle) ------------
        // The resource payloads are copied verbatim then pointer-fixed-up in place, so they MUST be
        // this build's native little-endian x64 images: require muPlatform == KU_PLATFORM (4). Stock
        // PC/X360/PS3 bundles (platform 1/2/3) have incompatible layouts and are refused here.
        BundleV2* lpHeader = reinterpret_cast<BundleV2*>(lpcBundle);
        if (lpHeader->muVersion != BundleV2::KU_VERSION || lpHeader->muPlatform != BundleV2::KU_PLATFORM)
        {
            free(lpcBundle);
            return -1;
        }

        const u32 luEntryCount = lpHeader->muResourceEntriesCount;
        BundleV2::ResourceEntry* lpEntries =
            reinterpret_cast<BundleV2::ResourceEntry*>(lpcBundle + lpHeader->muResourceEntriesOffset);

        // Remember each created slot so the fixup passes (which run only after every resource
        // is in place) can revisit them. -1 = the entry was skipped.
        s32* lpiSlots = static_cast<s32*>(malloc(sizeof(s32) * (luEntryCount != 0 ? luEntryCount : 1)));
        if (lpiSlots == 0)
        {
            free(lpcBundle);
            return -1;
        }

        // ---- pass 0: the dependency check == CgsResource::AllocatePoolModuleState::
        // CheckListDependencies @0x828FF228. For every bundle entry, look the id up through
        // the pool AND its dependency pools (FindResourceWithDependencies(id, &pool, true,
        // status mask 3, &slot)); when it is ALREADY RESIDENT the console does NOT create a
        // second entry -- it bumps the owning pool's ref count for that slot
        // (`*(pool+240)[slot] = count > 0 ? count + 1 : 1`) and clears the per-entry create
        // flag, so CreateResourceList skips it. Only the misses are created.
        //
        // This is load-bearing, not an optimisation: neighbouring track units share a large
        // fraction of their resources by id (TRK_UNIT33 and TRK_UNIT15 share 546 of 1243),
        // so the 25-zone PVS working set is 26423 bundle entries but only 6059 UNIQUE
        // resources. Creating one entry per bundle entry overflowed pool 3's 8500-resource /
        // 8500-node budget after seven units AND made BundleLoader::UnloadBundle free a
        // duplicate that another resident unit was still pointing at.
        //
        // The X360 also reports a "Hash conflict" when a resident resource's per-memtype size
        // differs from the bundle entry's; reproduced as a one-shot log.
        u8* lpu8Create = static_cast<u8*>(malloc(luEntryCount != 0 ? luEntryCount : 1));
        if (lpu8Create == 0)
        {
            free(lpiSlots);
            free(lpcBundle);
            return -1;
        }
        s32 liShared = 0;
        for (u32 luIndex = 0; luIndex < luEntryCount; ++luIndex)
        {
            BundleV2::ResourceEntry& lrEntry = lpEntries[luIndex];

            Pool* lpFoundPool  = 0;
            s32   liFoundIndex = -1;
            Entry* lpFound = lpPool->FindResourceWithDependencies(lrEntry.mResourceId, &lpFoundPool,
                                                                 true, 3, &liFoundIndex);
            if (lpFound == 0 || lpFoundPool == 0 || liFoundIndex < 0)
            {
                lpu8Create[luIndex] = 1;
                continue;
            }

            // Resident: reference it and skip the create (X360 clamps the stored count to
            // at least 1 -- `v23 = v22 > 0 ? v22 + 1 : 1`).
            if (lpFoundPool->GetEntryRefCount(liFoundIndex) > 0)
                lpFoundPool->IncEntryRefCount(liFoundIndex);
            else
                lpFoundPool->SetEntryRefCount(liFoundIndex, 1);

            for (u32 luMemType = 0; luMemType < BundleV2::E_MEMTYPE_NUMTYPES; ++luMemType)
            {
                if (lpFound->mResourceDescriptor.m_baseResourceDescriptors[luMemType].m_size
                        != lrEntry.GetUncompressedSize(luMemType)
                    && (CgsDev::Message::gxMessageFilterFlags & 1))
                {
                    static bool sbLoggedConflict = false;
                    if (!sbLoggedConflict)
                    {
                        sbLoggedConflict = true;
                        *CgsDev::Log::gpDebugPrint << "Hash conflict - resource in '" << lpcFileName
                                                   << "' conlicts\n";
                    }
                }
            }

            lpu8Create[luIndex] = 0;
            lpiSlots[luIndex]   = -1;   // not created here -- the fixup passes must skip it
            ++liShared;
        }

        // ⭐ ON THE MISSING "ENTRY-LIST LEG" (boot audit F-P7-21), resolved 2026-08-17.
        //
        // The console's CheckListDependencies closes with
        // `mbCreateEntryListResource = CheckEntryListDependency()` @0x828FF460-70, and this
        // mirror has no equivalent -- which the audit recorded as "no per-bundle EntryList
        // validation on PC".
        //
        // It is a mis-scope, not a gap. CheckEntryListDependency asks whether THE LIST'S OWN
        // entry-list resource is resident (`FindResource(mListId, ...)`), and mListId exists
        // only when a resource LIST is being allocated. This function loads a BUNDLE: it has
        // no list id and creates no entry-list resource, so there is nothing for that leg to
        // validate. Grep confirms it -- not one list-id or entry-list symbol appears in this
        // file.
        //
        // The leg IS present where it belongs: AllocatePoolModuleState carries
        // mbCreateEntryListResource and calls CheckEntryListDependency / CreateEntryListResource
        // (CgsAllocatePoolModuleState.cpp:133/:169/:221). What this mirror borrows from
        // CheckListDependencies is only the per-entry dependency pass above, which is the part
        // a bundle load needs.
        // Recorded here so the finding is not re-opened against this function.

        // ---- pass 1: create + allocate + copy each resource ---------------------------
        s32 liLoaded = 0;
        for (u32 luIndex = 0; luIndex < luEntryCount; ++luIndex)
        {
            if (lpu8Create[luIndex] == 0)
                continue;

            BundleV2::ResourceEntry& lrEntry = lpEntries[luIndex];

            NewResource lNewResource;
            // X360 stored-id form: the RAW on-disc 64-bit entry id, UNTAGGED --
            // AllocatePoolModuleState::CreateResourceList (0x828FF480) stores
            // `*(u64*)entryPtr` straight through CreateEntryInSlot, and every game-side
            // acquire emits the raw zero-extended HashString return (HashString
            // @0x828D84A8 ends `clrldi r3,32`; the "tagged" `| pool<<32` reads in the
            // Hex-Rays output were fusion artifacts of the separate miPoolId @+8 field
            // stores -- see WorldModule::LoadAttribSysVault asm @0x827D3DEC). The pool
            // is selected by the acquire event's miPoolId field (DoAcquireResourceRequest
            // 0x828FCD48 `lwz r4, 8(r31)`), never by the id's high dword.
            lNewResource.mID                 = lrEntry.mResourceId;
            lNewResource.miNumImports        = static_cast<s32>(lrEntry.muImportCount);
            lNewResource.muImportTableOffset = lrEntry.muImportOffset;
            lNewResource.mpResourceType      = (lpfnResolveType != 0) ? lpfnResolveType(lrEntry.muResourceTypeId) : 0;
            // [FLAG PC boot gate] Name the unregistered type ids as they appear -- a type
            // with no registered handler bypasses FixUp/imports without a word and shows up
            // much later as a half-built resource graph. DELETE once every shipped type id
            // is registered.
            if (lNewResource.mpResourceType == 0 && (CgsDev::Message::gxMessageFilterFlags & 1))
            {
                static u32 sauReported[16] = { 0 };
                static u32 suReportedCount = 0;
                bool lbSeen = false;
                for (u32 luSeen = 0; luSeen < suReportedCount; ++luSeen)
                    lbSeen = lbSeen || (sauReported[luSeen] == lrEntry.muResourceTypeId);
                if (!lbSeen && suReportedCount < 16)
                {
                    sauReported[suReportedCount++] = lrEntry.muResourceTypeId;
                    *CgsDev::Log::gpDebugPrint << "[bundle] UNREGISTERED resource type id "
                        << static_cast<s32>(lrEntry.muResourceTypeId) << " in '" << lpcFileName
                        << "' [FLAG PC boot gate]\n";
                }
            }
            for (u32 luMemType = 0; luMemType < BundleV2::E_MEMTYPE_NUMTYPES; ++luMemType)
            {
                lNewResource.mResourceDescriptor.m_baseResourceDescriptors[luMemType].m_size      = lrEntry.GetUncompressedSize(luMemType);
                lNewResource.mResourceDescriptor.m_baseResourceDescriptors[luMemType].m_alignment = lrEntry.GetUncompresssedAlignment(luMemType);
            }

            Entry* lpEntry = 0;
            s32    liSlot  = -1;
            if (lpPool->CreateEntry(&lNewResource, &lpEntry, &liSlot, true) != Pool::CREATERESULT_OK)
            {
                lpiSlots[luIndex] = -1;   // out of entries / memory -- skip
                continue;
            }
            lpiSlots[luIndex] = liSlot;

            // copy each memory pool's (uncompressed) data from the bundle into the allocation
            for (u32 luMemType = 0; luMemType < BundleV2::E_MEMTYPE_NUMTYPES; ++luMemType)
            {
                const u32 luBytes = lrEntry.GetUncompressedSize(luMemType);
                void*     lpDest  = lpEntry->mResource.m_baseResources[luMemType];
                if (luBytes != 0 && lpDest != 0)
                {
                    const char* lpcSrc = lpcBundle + lpHeader->mauResourceDataOffset[luMemType] + lrEntry.mauDiskOffset[luMemType];
                    memcpy(lpDest, lpcSrc, luBytes);
                }
            }
            ++liLoaded;
        }

        // ---- passes 2-4: fix up, resolve imports, post-fix-up (in that order, so imports
        // see every resource already fixed up). Only run for resources with a known Type.
        for (u32 luIndex = 0; luIndex < luEntryCount; ++luIndex)
        {
            const s32 liSlot = lpiSlots[luIndex];
            if (liSlot < 0) continue;
            Entry* lpEntry = &lpPool->mpResourceEntries[liSlot];
            if (lpEntry->mpResourceType != 0)
                lpPool->FixUpEntry(lpEntry);
        }
        for (u32 luIndex = 0; luIndex < luEntryCount; ++luIndex)
        {
            const s32 liSlot = lpiSlots[luIndex];
            if (liSlot < 0) continue;
            if (lpPool->mpResourceEntries[liSlot].mpResourceType != 0)
                lpPool->ResolveImportsForEntry(liSlot);
        }
        for (u32 luIndex = 0; luIndex < luEntryCount; ++luIndex)
        {
            const s32 liSlot = lpiSlots[luIndex];
            if (liSlot < 0) continue;
            Entry* lpEntry = &lpPool->mpResourceEntries[liSlot];
            if (lpEntry->mpResourceType != 0)
                lpPool->PostFixUpEntry(lpEntry);
        }

        // ---- pass 5: mark each fully-loaded resource LOADED (status 2) so it is acquirable.
        // Resource status lifecycle: 0 = free, 1 = created/loading (CreateEntry), 2 = loaded/ready.
        // The X360 async streamer sets a resource to "loaded" at stream-done; the PC synchronous load --
        // being the complete load -- sets it here at completion. Pool::FindResource (and thus
        // AcquireResource) gates on status & 2, so this is what makes a streamed resource acquirable.
        for (u32 luIndex = 0; luIndex < luEntryCount; ++luIndex)
        {
            const s32 liSlot = lpiSlots[luIndex];
            if (liSlot < 0) continue;
            lpPool->SetEntryStatus(liSlot, 2);
        }

        if (liShared != 0 && (CgsDev::Message::gxMessageFilterFlags & 1))
            *CgsDev::Log::gpDebugPrint << "[stream]   '" << lpcFileName << "': " << liShared
                                       << " of " << static_cast<s32>(luEntryCount)
                                       << " already resident (referenced, not duplicated)\n";

        free(lpu8Create);
        free(lpiSlots);
        free(lpcBundle);
        return liLoaded;
    }

    // Unload a bundle's resources from a pool (the inverse of LoadBundle). Re-read the bundle's resource
    // id list and ref-count-release each one; Pool::RemoveReference frees a resource's heap memory + slot
    // when its ref count reaches zero (so a resource still imported elsewhere survives). [PC synchronous
    // form -- the X360 async unload uses the DeAllocate state machine + tracks loaded bundles.]
    s32 BundleLoader::UnloadBundle(const char* lpcFileName, Pool* lpPool)
    {
        long  llFileSize = 0;
        char* lpcBundle  = ReadBundleFile(lpcFileName, &llFileSize);
        if (lpcBundle == 0)
            return -1;
        if (llFileSize <= static_cast<long>(sizeof(BundleV2)))
        {
            free(lpcBundle);
            return -1;
        }

        BundleV2* lpHeader = reinterpret_cast<BundleV2*>(lpcBundle);
        if (lpHeader->muVersion != BundleV2::KU_VERSION || lpHeader->muPlatform != BundleV2::KU_PLATFORM)
        {
            free(lpcBundle);
            return -1;
        }

        const u32 luEntryCount = lpHeader->muResourceEntriesCount;
        BundleV2::ResourceEntry* lpEntries =
            reinterpret_cast<BundleV2::ResourceEntry*>(lpcBundle + lpHeader->muResourceEntriesOffset);

        s32 liUnloaded = 0;
        for (u32 luIndex = 0; luIndex < luEntryCount; ++luIndex)
        {
            // find the resource by id (any in-use status; ignore the ref-count gate), then release a ref.
            // Ids are stored raw/untagged (see LoadBundle) and looked up in the same form. The search
            // spans the dependency pools exactly as LoadBundle's CheckListDependencies pass does, so
            // every reference that pass took is given back to the pool that owns it.
            ID lId = lpEntries[luIndex].mResourceId;
            Pool*     lpFoundPool = 0;
            const s32 liSlot = lpPool->FindResourceIndexWithDependencies(lId, &lpFoundPool, true, 0xFF);
            if (liSlot >= 0 && lpFoundPool != 0 && lpFoundPool->RemoveReference(static_cast<u32>(liSlot)))
                ++liUnloaded;
        }

        free(lpcBundle);
        return liUnloaded;
    }
}
