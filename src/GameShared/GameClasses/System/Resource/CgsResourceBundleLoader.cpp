#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoader.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"     // Pool, NewResource, Entry
#include "GameShared/GameClasses/System/Resource/CgsResourceBundle2.h"  // BundleV2
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"     // Type
#include "GameShared/GameClasses/System/Resource/CgsEntryListResource.h"
#include "GameShared/GameClasses/System/FileSystem/CgsDeviceManager.h"  // async FS engine (ReadWholeFile)
#include "GameShared/GameClasses/System/FileSystem/CgsFileSystem.h"     // EnsureDeviceManagerUp
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // gpDebugPrint (read trace)

#include <cstdlib>   // malloc / free
#include <cstring>   // memcpy

#include "pc/gcm/renderengine/FrameProfilePCLeaf.h"

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
        void* lpBuf = nullptr;
        {
            renderengine::FrameProfile::Scope lProfile(renderengine::FrameProfile::RESOURCE_FILE);
            lpBuf = lpManager->ReadWholeFile(lpcFileName, &luSize);
        }
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
    // CheckForUnloads 828FB52C-534 tags the zero-extended HashString result with bit 63.
    // The pool ID is a separate field; ordinary member resource IDs remain untagged.
    static ID BundleListId(const char* lpcFileName)
    {
        ID lId;
        lId.SetHash(static_cast<u32>(ID::HashString(reinterpret_cast<const u8*>(lpcFileName)))
                    | 0x8000000000000000ull);
        return lId;
    }

    // FLAG PC-platform leaf: the synchronous native loader owns its allocation transaction.
    // The console allocation state retries a whole batch before publishing it. Here a failed
    // allocation must undo every reference acquired by this attempt, including a revived entry's
    // exact previous count. No FixUp/PostFixUp runs until all allocations have succeeded.
    struct BundleReferencePC
    {
        Pool* mpPool;
        s32 miSlot;
        s16 miPreviousCount;
        bool mbCreated;
    };

    class BundleReferencesPC
    {
    public:
        explicit BundleReferencesPC(u32 luCount)
            : mpReferences(static_cast<BundleReferencePC*>(calloc(luCount, sizeof(BundleReferencePC)))),
              muCount(luCount), mbCommitted(false) {}
        ~BundleReferencesPC()
        {
            if (mpReferences && !mbCommitted)
            {
                for (u32 i = muCount; i != 0; --i)
                {
                    const BundleReferencePC& lrRef = mpReferences[i - 1];
                    if (!lrRef.mpPool) continue;
                    if (lrRef.mbCreated)
                        lrRef.mpPool->RemoveReference(static_cast<u32>(lrRef.miSlot));
                    else
                        lrRef.mpPool->SetEntryRefCount(lrRef.miSlot, lrRef.miPreviousCount);
                }
            }
            free(mpReferences);
        }
        bool IsValid() const { return mpReferences != nullptr; }
        const BundleReferencePC& operator[](u32 i) const { return mpReferences[i]; }
        void Acquire(u32 i, Pool* lpPool, s32 liSlot, bool lbCreated)
        {
            BundleReferencePC& lrRef = mpReferences[i];
            lrRef.mpPool = lpPool;
            lrRef.miSlot = liSlot;
            lrRef.mbCreated = lbCreated;
            lrRef.miPreviousCount = lpPool->GetEntryRefCount(liSlot);
            if (!lbCreated)
            {
                if (lrRef.miPreviousCount > 0) lpPool->IncEntryRefCount(liSlot);
                else lpPool->SetEntryRefCount(liSlot, 1);
            }
        }
        void Commit() { mbCommitted = true; }

    private:
        BundleReferencePC* mpReferences;
        u32 muCount;
        bool mbCommitted;
        BundleReferencesPC(const BundleReferencesPC&) = delete;
        BundleReferencesPC& operator=(const BundleReferencesPC&) = delete;
    };

    static const EntryListResource* GetBundleList(const Entry* lpEntry)
    {
        if (!lpEntry || !lpEntry->mpResourceType || lpEntry->mpResourceType->GetTypeID() != 29)
            return nullptr;
        const u32 luSize = lpEntry->mResourceDescriptor.m_baseResourceDescriptors[0].m_size;
        const EntryListResource* lpList =
            static_cast<const EntryListResource*>(lpEntry->mResource.m_baseResources[0]);
        if (!lpList || luSize < EntryListResource::KI_HEADERSIZE
            || lpList->muNumEntries > (luSize - EntryListResource::KI_HEADERSIZE) / sizeof(ID))
            return nullptr;
        return lpList;
    }

    s32 BundleLoader::LoadBundle(const char* lpcFileName, Pool* lpPool, FTypeResolver lpfnResolveType)
    {
        // ---- read the whole bundle file (through the live async FS engine; CRT leaf early) --
        long  llFileSize = 0;
        char* lpcBundle  = ReadBundleFile(lpcFileName, &llFileSize);
        if (lpcBundle == 0)
            return KI_LOAD_FILE_MISSING;   // [FLAG PC bring-up] -- see the header
        if (llFileSize < static_cast<long>(sizeof(BundleV2)))
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
        const u32 luFileSize = static_cast<u32>(llFileSize);
        if (lpHeader->muResourceEntriesOffset > luFileSize
            || luEntryCount > (luFileSize - lpHeader->muResourceEntriesOffset) / sizeof(BundleV2::ResourceEntry))
        {
            free(lpcBundle);
            return KI_LOAD_FAILED;
        }
        BundleV2::ResourceEntry* lpEntries =
            reinterpret_cast<BundleV2::ResourceEntry*>(lpcBundle + lpHeader->muResourceEntriesOffset);
        const ID lListId = BundleListId(lpcFileName);
        // Validate copy ranges before taking any references. Native bundles are uncompressed.
        for (u32 i = 0; i < luEntryCount; ++i)
        {
            if (lpEntries[i].mResourceId == lListId)
            {
                free(lpcBundle);
                return KI_LOAD_FAILED;
            }
            for (u32 t = 0; t < BundleV2::E_MEMTYPE_NUMTYPES; ++t)
            {
                const u32 luBytes = lpEntries[i].GetUncompressedSize(t);
                const u64 luOffset = static_cast<u64>(lpHeader->mauResourceDataOffset[t]) + lpEntries[i].mauDiskOffset[t];
                if (luBytes && (luOffset > luFileSize || luBytes > luFileSize - luOffset))
                {
                    free(lpcBundle);
                    return KI_LOAD_FAILED;
                }
            }
        }

        BundleReferencesPC lReferences(luEntryCount + 1);
        if (!lReferences.IsValid())
        {
            free(lpcBundle);
            return KI_LOAD_FAILED;
        }

        // AllocatePoolModuleState::CheckEntryListDependency searches this pool only, unlike
        // its member dependency pass. The list lives in the pool and follows its lifetime.
        s32 liListSlot = -1;
        Entry* lpListEntry = lpPool->FindResource(lListId, true, 3, &liListSlot);
        if (lpListEntry)
        {
            const EntryListResource* lpList = GetBundleList(lpListEntry);
            if (!lpList || lpList->muNumEntries != luEntryCount)
            {
                free(lpcBundle);
                return KI_LOAD_FAILED;
            }
            for (u32 i = 0; i < luEntryCount; ++i)
            {
                // A changed resident bundle belongs to the separate live-update path.
                if (lpList->mIds[i] != lpEntries[i].mResourceId)
                {
                    free(lpcBundle);
                    return KI_LOAD_FAILED;
                }
            }
            lReferences.Acquire(0, lpPool, liListSlot, false);
        }
        else
        {
            static EntryListResourceType sListType = [] {
                EntryListResourceType lType;
                lType.InitCachedValues();
                return lType;
            }();
            const u32 luAlignment = lpPool->GetHeapAlignment(0);
            const u64 luSize = (static_cast<u64>(luEntryCount) * sizeof(ID)
                + EntryListResource::KI_HEADERSIZE + luAlignment - 1) & ~static_cast<u64>(luAlignment - 1);
            if (luSize > 0xFFFFFFFFull)
            {
                free(lpcBundle);
                return KI_LOAD_FAILED;
            }
            NewResource lListResource = {};
            lListResource.mID = lListId;
            lListResource.mpResourceType = &sListType;
            lListResource.mResourceDescriptor.m_baseResourceDescriptors[0].m_size = static_cast<u32>(luSize);
            lListResource.mResourceDescriptor.m_baseResourceDescriptors[0].m_alignment = luAlignment;
            if (lpPool->CreateEntry(&lListResource, &lpListEntry, &liListSlot, true) != Pool::CREATERESULT_OK)
            {
                free(lpcBundle);
                return KI_LOAD_FAILED;
            }
            lReferences.Acquire(0, lpPool, liListSlot, true);
            EntryListResource* lpList = static_cast<EntryListResource*>(lpListEntry->mResource.m_baseResources[0]);
            std::strncpy(lpList->macOwnerName, lpcFileName, EntryListResource::KI_MAXOWNERLENGTH - 1);
            lpList->macOwnerName[EntryListResource::KI_MAXOWNERLENGTH - 1] = 0;
            lpList->muNumEntries = luEntryCount;
            for (u32 i = 0; i < luEntryCount; ++i) lpList->mIds[i] = lpEntries[i].mResourceId;
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
                continue;
            }

            // Resident: reference it and skip the create (X360 clamps the stored count to
            // at least 1 -- `v23 = v22 > 0 ? v22 + 1 : 1`).
            lReferences.Acquire(luIndex + 1, lpFoundPool, liFoundIndex, false);

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

            ++liShared;
        }

        // ---- pass 1: create + allocate + copy each resource ---------------------------
        s32 liLoaded = 0;
        for (u32 luIndex = 0; luIndex < luEntryCount; ++luIndex)
        {
            if (lReferences[luIndex + 1].mpPool)
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
                free(lpcBundle);
                return KI_LOAD_FAILED;
            }
            lReferences.Acquire(luIndex + 1, lpPool, liSlot, true);

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
            if (!lReferences[luIndex + 1].mbCreated) continue;
            const s32 liSlot = lReferences[luIndex + 1].miSlot;
            Entry* lpEntry = &lpPool->mpResourceEntries[liSlot];
            if (lpEntry->mpResourceType != 0)
                lpPool->FixUpEntry(lpEntry);
        }
        for (u32 luIndex = 0; luIndex < luEntryCount; ++luIndex)
        {
            if (!lReferences[luIndex + 1].mbCreated) continue;
            const s32 liSlot = lReferences[luIndex + 1].miSlot;
            if (lpPool->mpResourceEntries[liSlot].mpResourceType != 0)
                lpPool->ResolveImportsForEntry(liSlot);
        }
        for (u32 luIndex = 0; luIndex < luEntryCount; ++luIndex)
        {
            if (!lReferences[luIndex + 1].mbCreated) continue;
            const s32 liSlot = lReferences[luIndex + 1].miSlot;
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
            if (!lReferences[luIndex + 1].mbCreated) continue;
            const s32 liSlot = lReferences[luIndex + 1].miSlot;
            lpPool->SetEntryStatus(liSlot, 2);
        }
        lpPool->SetEntryStatus(liListSlot, 2);
        lReferences.Commit();

        if (liShared != 0 && (CgsDev::Message::gxMessageFilterFlags & 1))
            *CgsDev::Log::gpDebugPrint << "[stream]   '" << lpcFileName << "': " << liShared
                                       << " of " << static_cast<s32>(luEntryCount)
                                       << " already resident (referenced, not duplicated)\n";

        free(lpcBundle);
        return liLoaded;
    }

    // CheckForUnloads uses the resident entry list, never the bundle file. Keep the list alive
    // until all member references from this low-level load have been released. Higher-level
    // logical bundle refcounts and asynchronous DeAllocate scheduling remain the module's job.
    s32 BundleLoader::UnloadBundle(const char* lpcFileName, Pool* lpPool)
    {
        s32 liListSlot = -1;
        const EntryListResource* lpList = GetBundleList(
            lpPool->FindResource(BundleListId(lpcFileName), true, 3, &liListSlot));
        if (!lpList)
            return KI_LOAD_FAILED; // native cleanup also handles optional bundles whose load failed

        s32 liUnloaded = 0;
        for (u32 luIndex = 0; luIndex < lpList->muNumEntries; ++luIndex)
        {
            // find the resource by id (any in-use status; ignore the ref-count gate), then release a ref.
            // Ids are stored raw/untagged (see LoadBundle) and looked up in the same form. The search
            // spans the dependency pools exactly as LoadBundle's CheckListDependencies pass does, so
            // every reference that pass took is given back to the pool that owns it.
            ID lId = lpList->mIds[luIndex];
            Pool*     lpFoundPool = 0;
            const s32 liSlot = lpPool->FindResourceIndexWithDependencies(lId, &lpFoundPool, true, 0xFF);
            if (liSlot >= 0 && lpFoundPool != 0 && lpFoundPool->RemoveReference(static_cast<u32>(liSlot)))
                ++liUnloaded;
        }

        lpPool->RemoveReference(static_cast<u32>(liListSlot));
        return liUnloaded;
    }
}
