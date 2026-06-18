#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoader.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"     // Pool, NewResource, Entry
#include "GameShared/GameClasses/System/Resource/CgsResourceBundle2.h"  // BundleV2
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"     // Type

#include <cstdio>    // fopen / fread / fseek / ftell / fclose
#include <cstdlib>   // malloc / free
#include <cstring>   // memcpy

// The PC bundle loader. Read CgsResourceBundleLoader.h for how this relates to the X360
// streaming BundleLoaderModule (this is the synchronous PC IO form). The header validation
// mirrors X360 BundleLoaderModule::ProcessBundleHeader (0x828D7A90); the per-resource create
// / fixup / import sequence mirrors the X360 load path (Pool::CreateEntry, then the three
// FixUp / ResolveImports / PostFixUp passes that FixUpAndResolveResourceList runs in order).

namespace CgsResource
{
    s32 BundleLoader::LoadBundle(const char* lpcFileName, Pool* lpPool, FTypeResolver lpfnResolveType)
    {
        // ---- read the whole bundle file (PC IO) ---------------------------------------
        FILE* lpFile = fopen(lpcFileName, "rb");
        if (lpFile == 0)
            return -1;

        fseek(lpFile, 0, SEEK_END);
        const long llFileSize = ftell(lpFile);
        fseek(lpFile, 0, SEEK_SET);
        if (llFileSize <= static_cast<long>(sizeof(BundleV2)))
        {
            fclose(lpFile);
            return -1;
        }

        char* lpcBundle = static_cast<char*>(malloc(static_cast<size_t>(llFileSize)));
        if (lpcBundle == 0)
        {
            fclose(lpFile);
            return -1;
        }
        const size_t lnRead = fread(lpcBundle, 1, static_cast<size_t>(llFileSize), lpFile);
        fclose(lpFile);
        if (lnRead != static_cast<size_t>(llFileSize))
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

        // ---- pass 1: create + allocate + copy each resource ---------------------------
        s32 liLoaded = 0;
        for (u32 luIndex = 0; luIndex < luEntryCount; ++luIndex)
        {
            BundleV2::ResourceEntry& lrEntry = lpEntries[luIndex];

            NewResource lNewResource;
            lNewResource.mID                 = lrEntry.mResourceId;
            lNewResource.miNumImports        = static_cast<s32>(lrEntry.muImportCount);
            lNewResource.muImportTableOffset = lrEntry.muImportOffset;
            lNewResource.mpResourceType      = (lpfnResolveType != 0) ? lpfnResolveType(lrEntry.muResourceTypeId) : 0;
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

        free(lpiSlots);
        free(lpcBundle);
        return liLoaded;
    }
}
