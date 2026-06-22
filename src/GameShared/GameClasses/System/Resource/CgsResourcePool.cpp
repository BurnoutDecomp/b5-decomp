#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"   // overhead / per-pool allocators (InitPool)
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT

#include <cstdint>   // uintptr_t (the Heap allocation owner is the slot index)

// CgsResource::Pool method bodies, decompiled from the X360 ARTIST IDA (addresses noted
// per method). This file is built up incrementally: the lookup + reference/accessor slice
// first, then the create/allocate/fixup load path, with the defrag/scratch/relocator
// machinery reconstructed last. Methods not yet bodied here are simply absent (the per-TU
// gate is cl /c; the link milestone is where the remaining bodies / trap-stubs are needed).
//
// NOTE: the X360 Entry stride is 88 bytes (FindResource/GetResource index at 88*i); the
// PS3-DWARF Entry field list reconstructed in CgsResourceBasePool.h sums to fewer bytes,
// so the X360 Entry carries ~8 bytes this file's create path must reconcile (resolved when
// CreateEntryInSlot, which writes Entry fields at explicit offsets, is decompiled). The
// lookup methods below are unaffected (they use C++ array indexing, i.e. the PC sizeof).

namespace CgsResource
{
    // ---- lookup -------------------------------------------------------------------

    // 0x828ED190 - resolve an ID to its entry index via the hash table, then gate on the
    // refcount and the status mask. Returns -1 if absent or filtered out.
    // Polarity note (from the binary): lbCheckRefCount == true ACCEPTS an unreferenced
    // entry; == false requires refcount > 0.
    s32 Pool::FindResourceIndex(ID lID, bool lbCheckRefCount, u16 luStatusMask)
    {
        CGS_ASSERT(mbIsValid, "Pool is not valid\n");   // CgsResourcePool.cpp:805
        s32 liIndex = mHashTable.FindEntry(lID);
        if (liIndex < 0
            || (!lbCheckRefCount && mpiResourceRefCounts[liIndex] <= 0)
            || (mpx8ResourceStatuses[liIndex] & luStatusMask) == 0)
        {
            return -1;
        }
        return liIndex;
    }

    // 0x828F5D58 - FindResourceIndex, then return the Entry* (and the index via lpiOutIndex).
    Entry* Pool::FindResource(ID lID, bool lbCheckRefCount, u16 luStatusMask, s32* lpiOutIndex)
    {
        CGS_ASSERT(mbIsValid, "Pool is not valid\n");   // :770
        s32 liIndex = FindResourceIndex(lID, lbCheckRefCount, luStatusMask);
        if (liIndex < 0)
            return 0;
        if (lpiOutIndex)
            *lpiOutIndex = liIndex;
        return &mpResourceEntries[liIndex];
    }

    // 0x828F5E48 - FindResource here, else across each dependency pool; reports which pool the
    // resource was found in via lppOutPool.
    Entry* Pool::FindResourceWithDependencies(ID lID, Pool** lppOutPool, bool lbCheckRefCount, u16 luStatusMask, s32* lpiOutIndex)
    {
        CGS_ASSERT(mbIsValid, "Pool is not valid\n");   // :848

        Entry* lpEntry = FindResource(lID, lbCheckRefCount, luStatusMask, lpiOutIndex);
        if (lpEntry != 0)
        {
            *lppOutPool = this;
            return lpEntry;
        }

        for (s32 liDep = 0; liDep < miNumDependencies; ++liDep)
        {
            lpEntry = mapDependencies[liDep]->FindResource(lID, lbCheckRefCount, luStatusMask, lpiOutIndex);
            if (lpEntry != 0)
            {
                *lppOutPool = mapDependencies[liDep];
                return lpEntry;
            }
        }

        *lppOutPool = 0;
        return 0;
    }

    // Index form of the above (this pool, then dependencies).
    s32 Pool::FindResourceIndexWithDependencies(ID lID, Pool** lppOutPool, bool lbCheckRefCount, u16 luStatusMask)
    {
        s32    liIndex = -1;
        Entry* lpEntry = FindResourceWithDependencies(lID, lppOutPool, lbCheckRefCount, luStatusMask, &liIndex);
        return lpEntry != 0 ? liIndex : -1;
    }

    // PC bring-up convenience: scan the in-use entries (status != 0) for the first whose handler
    // reports luTypeId. Used to pick the Font out of a freshly-loaded single-font bundle without
    // knowing its resource id. (Not an X360 method; the X360 resolves the font by its known id.)
    Entry* Pool::FindFirstResourceOfType(u32 luTypeId, s32* lpiOutIndex)
    {
        for (u16 luIndex = 0; luIndex < muMaxResources; ++luIndex)
        {
            if (mpx8ResourceStatuses[luIndex] == 0)
                continue;
            Entry* lpEntry = &mpResourceEntries[luIndex];
            if (lpEntry->mpResourceType != 0 && lpEntry->mpResourceType->GetTypeID() == luTypeId)
            {
                if (lpiOutIndex != 0)
                    *lpiOutIndex = static_cast<s32>(luIndex);
                return lpEntry;
            }
        }
        if (lpiOutIndex != 0)
            *lpiOutIndex = -1;
        return 0;
    }

    // 0x828D8A18 - Entry* by index (bounds-checked), gated on refcount + status as above.
    Entry* Pool::GetResource(s32 liIndex, bool lbCheckRefCount, u16 luStatusMask)
    {
        CGS_ASSERT(mbIsValid, "Pool is not valid\n");                          // :737
        CGS_ASSERT(liIndex >= 0 && liIndex < muMaxResources, "Invalid id\n");  // :738
        if ((lbCheckRefCount || mpiResourceRefCounts[liIndex] > 0)
            && (mpx8ResourceStatuses[liIndex] & luStatusMask) != 0)
        {
            return &mpResourceEntries[liIndex];
        }
        return 0;
    }

    // ---- trivial accessors --------------------------------------------------------
    // These are inlined in the X360 (no out-of-line body), so they are reconstructed from
    // their unambiguous semantics: each is a direct member or parallel-array access.

    // @ 0x828ED058 - bring the pool to its clean empty/invalid state (the per-pool init the
    // PoolModule runs for all 128 pools before they are InitPool'd with real memory).
    //
    // [SEMANTIC RECONSTRUCTION] The X360 body is raw-offset writes (264..272/304/308/416/452 plus a
    // 3x stride-64 loop over the heaps) emitted from a decomp whose local-variable allocation failed,
    // and our Pool is compiler-laid-out (named members, NOT byte-matched), so this reproduces the
    // OBSERVABLE effect by named member rather than transcribing offsets: construct the 3 heaps and
    // zero the management state, with the id/bank sentinels set to -1 (the X360's two -1 stores) and
    // the stages at their start/idle values. The X360 also sets one field (+308) to 1 that could not
    // be byte-mapped to a named member; it is almost certainly mbAllowDefragmentation, which InitPool
    // overwrites from InitOptions for any real pool, so it is left at the InitPool-supplied default
    // here (no observable difference for an un-InitPool'd pool). mResource/mDescriptor/mHashTable are
    // built by InitPool (the X360 Construct only zeroes them, which static-init already guarantees).
    void Pool::Construct()
    {
        for (s32 li = 0; li < 3; ++li)
            maHeaps[li].Construct();

        mbIsValid               = false;
        mpResourceEntries       = 0;
        mpx8ResourceStatuses    = 0;
        mpiResourceRefCounts    = 0;
        mpiResourceImportCounts = 0;
        mpHash                  = 0;
        mpTracker               = 0;
        mpnBatchIndices         = 0;
        muMaxResources          = 0;
        muNumFreeResources      = 0;
        miRefCountThreshold     = 0;
        miId                    = -1;   // X360 -1 sentinel: no id until InitPool assigns one
        macName[0]              = 0;
        mePrepareStage          = E_PREPARESTAGE_START;
        meReleaseStage          = E_RELEASESTAGE_START;
        meDefragStage           = DEFRAGSTAGE_IDLE;
        miNumDependencies       = 0;
        for (s32 li = 0; li < 16; ++li)   // mapDependencies[16]
            mapDependencies[li] = 0;
        mpHashEntries           = 0;
        miBankId                = -1;   // X360 -1 sentinel: no bank until InitPool assigns one
        mpCurrentRelocator      = 0;
        mpCurrentScratchPool    = 0;
        muNumRelocations        = 0;
        muNextRelocation        = 0;
        mpRelocateRequests      = 0;
        mpRelocateSources       = 0;
        miDefragFrame           = 0;
        miDefragMemType         = 0;
        mbAllowDefragmentation  = false;
        mbDefragHitEndResource  = false;
        miNumResourcesInPurgatory = 0;
    }

    // @ 0x828D8580 - one-shot prepare stage flip (asserts the stage is start/done, then
    // arms release-start and marks prepared). The PoolModule drives this for all 128 pools.
    bool Pool::Prepare()
    {
        CGS_ASSERT(mePrepareStage == E_PREPARESTAGE_START || mePrepareStage == E_PREPARESTAGE_DONE,
                   "Should never get here!");
        meReleaseStage = E_RELEASESTAGE_START;
        mePrepareStage = E_PREPARESTAGE_DONE;
        return true;
    }

    // @ 0x828D8600 - one-shot release stage flip (mirror of Prepare).
    bool Pool::Release()
    {
        CGS_ASSERT(meReleaseStage == E_RELEASESTAGE_START || meReleaseStage == E_RELEASESTAGE_DONE,
                   "Should never get here!");
        mePrepareStage = E_PREPARESTAGE_START;
        meReleaseStage = E_RELEASESTAGE_DONE;
        return true;
    }

    const char* Pool::GetName() const             { return macName; }
    s32  Pool::GetId() const                       { return miId; }
    s32  Pool::GetBankId()                         { return miBankId; }
    bool Pool::IsValid() const                     { return mbIsValid; }
    s32  Pool::GetNumDependencies() const          { return miNumDependencies; }
    s32  Pool::GetRefCountThreshold()              { return miRefCountThreshold; }
    s32  Pool::GetNumEntriesInPurgatory() const    { return miNumResourcesInPurgatory; }
    bool Pool::GetAllowDefragmentation() const     { return mbAllowDefragmentation; }
    void Pool::SetAllowDefragmentation(bool lbAllow) { mbAllowDefragmentation = lbAllow; }

    s16  Pool::GetEntryRefCount(s32 liIndex) const       { return mpiResourceRefCounts[liIndex]; }
    void Pool::SetEntryRefCount(s32 liIndex, s16 liRefCount) { mpiResourceRefCounts[liIndex] = liRefCount; }
    void Pool::IncEntryRefCount(s32 liIndex)             { ++mpiResourceRefCounts[liIndex]; }
    void Pool::DecEntryRefCount(s32 liIndex)             { --mpiResourceRefCounts[liIndex]; }
    u8   Pool::GetEntryStatus(s32 liIndex) const         { return mpx8ResourceStatuses[liIndex]; }
    void Pool::SetEntryStatus(s32 liIndex, u8 luStatus)  { mpx8ResourceStatuses[liIndex] = luStatus; }
    s32  Pool::GetEntryImportCount(s32 liIndex) const    { return mpiResourceImportCounts[liIndex]; }
    void Pool::SetEntryImportCount(s32 liIndex, s32 liCount) { mpiResourceImportCounts[liIndex] = liCount; }

    // ---- create / allocate --------------------------------------------------------

    // 0x828FDAC8 - carve Heap memory for an entry, one allocation per memory pool. Pools
    // whose descriptor size is 0 get a null pointer (not a failure). Defraggable resource
    // types (Type::GetCachedCanDefrag) allocate from the top of the heap, others from the
    // bottom. On any Malloc failure the already-allocated pools are freed and false is
    // returned (the X360 also fires a formatted "heap out of space" assert).
    bool Pool::AllocateMemoryForResource(Entry* lpEntry, const char* lpcDebugName, s32 liSlot, const Type* lpResourceType)
    {
        CGS_ASSERT(mbIsValid, "Pool is not valid\n");   // CgsResourcePool.cpp:573

        const bool lbAllocateFromTop = lpResourceType->GetCachedCanDefrag();   // Type+4 flag

        for (u32 luMemType = 0; luMemType < BasePool::KI_NUM_TYPES; ++luMemType)
        {
            const u32 luSize = lpEntry->mResourceDescriptor.m_baseResourceDescriptors[luMemType].m_size;
            if (luSize == 0)
            {
                // Nothing in this pool for this resource.
                lpEntry->mResource.m_baseResources[luMemType] = 0;
                continue;
            }

            u16   luFoundIndex = CgsContainers::IndexedLinkedList<HeapEntry, u16>::KI_LIST_INVALID;
            void* lpMemory = maHeaps[luMemType].Malloc(luSize, lpcDebugName,
                                                       reinterpret_cast<void*>(static_cast<uintptr_t>(liSlot)),
                                                       0xFFFF, &luFoundIndex, false, lbAllocateFromTop, 0);
            lpEntry->mResource.m_baseResources[luMemType] = lpMemory;
            lpEntry->mauHeapIndices[luMemType]            = luFoundIndex;

            if (lpMemory == 0)
            {
                // Out of space: hand back the pools allocated so far for this resource.
                for (u32 luFreeType = 0; luFreeType < luMemType; ++luFreeType)
                {
                    if (lpEntry->mResource.m_baseResources[luFreeType] != 0)
                        maHeaps[luFreeType].Free(lpEntry->mResource.m_baseResources[luFreeType]);
                }
                CGS_ASSERT(false, "Heap in pool has run out of space for memory type - specify a larger size when creating the pool\n");   // :630
                return false;
            }
        }
        return true;
    }

    // 0x828FDDB8 - populate the entry at liSlot from lpNewResource, (optionally) allocate its
    // memory, then mark it live (status=1, refcount=1, importCount=numImports) and register it
    // in the ID->index hash. Returns CREATERESULT_OUTOFMEMORY (and frees the slot) if the
    // allocation fails, else CREATERESULT_OK with *lppOutEntry set.
    Pool::ECreateResult Pool::CreateEntryInSlot(const NewResource* lpNewResource, Entry** lppOutEntry, s32 liSlot, bool lbAllocateMemory)
    {
        CGS_ASSERT(mbIsValid, "Pool is not valid\n");   // :1045

        Entry* lpEntry = &mpResourceEntries[liSlot];
        lpEntry->mID                 = lpNewResource->mID;
        lpEntry->mResourceDescriptor = lpNewResource->mResourceDescriptor;
        lpEntry->mpResourceType      = lpNewResource->mpResourceType;
        lpEntry->muImportTableOffset = lpNewResource->muImportTableOffset;
        lpEntry->mpDebugInfo         = 0;

        if (!lbAllocateMemory)
        {
            lpEntry->mResource.m_baseResources[0] = 0;
            lpEntry->mResource.m_baseResources[1] = 0;
            lpEntry->mResource.m_baseResources[2] = 0;
        }
        else if (!AllocateMemoryForResource(lpEntry, "", liSlot, lpNewResource->mpResourceType))
        {
            CGS_ASSERT(false, "Could not allocate memory for resource - SYSTEM ERROR: should have been picked up earlier\n");   // :1067
            FreeResourceEntry(static_cast<s16>(liSlot));
            return CREATERESULT_OUTOFMEMORY;
        }

        mpx8ResourceStatuses[liSlot]    = 1;
        mpiResourceRefCounts[liSlot]    = 1;
        mpiResourceImportCounts[liSlot] = lpNewResource->miNumImports;
        mHashTable.AddEntry(lpEntry->mID, liSlot);

        *lppOutEntry = lpEntry;
        return CREATERESULT_OK;
    }

    // 0x828D87B0 - reserve the first free entry slot (status == 0). Returns false when the
    // pool is full; otherwise decrements the free count and hands back the entry + index.
    bool Pool::AllocateResourceEntry(Entry** lppOutEntry, s32* lpiOutIndex)
    {
        CGS_ASSERT(mbIsValid, "Pool is not valid\n");   // :522

        if (muNumFreeResources == 0)
            return false;

        u16 luSlot = 0;
        for (u16 luIndex = 0; luIndex < muMaxResources; ++luIndex)
        {
            if (mpx8ResourceStatuses[luIndex] == 0)
            {
                luSlot = luIndex;
                break;
            }
            luSlot = static_cast<u16>(luIndex + 1);   // if none free, ends == muMaxResources
        }
        CGS_ASSERT(luSlot != muMaxResources, "No room for more resources\n");   // :538

        --muNumFreeResources;
        *lppOutEntry  = &mpResourceEntries[luSlot];
        *lpiOutIndex  = luSlot;
        return true;
    }

    // 0x828D8938 - release an entry slot (mark free, bump the free count). The X360 also
    // brackets this in a perf-mon scope (debug instrumentation; omitted).
    void Pool::FreeResourceEntry(s16 liIndex)
    {
        CGS_ASSERT(mbIsValid, "Pool is not valid\n");   // :662
        mpx8ResourceStatuses[liIndex] = 0;
        ++muNumFreeResources;
    }

    // 0x82902388 - reserve a free slot then fill it (CreateEntryInSlot). Returns
    // CREATERESULT_OUTOFENTRIES if the pool has no free slots.
    Pool::ECreateResult Pool::CreateEntry(const NewResource* lpNewResource, Entry** lppOutEntry, s32* lpiOutIndex, bool lbAllocateMemory)
    {
        CGS_ASSERT(mbIsValid, "Pool is not valid\n");   // :1008

        Entry* lpReservedEntry;   // AllocateResourceEntry sets this; the slot fill uses lppOutEntry
        s32    liSlot;
        if (!AllocateResourceEntry(&lpReservedEntry, &liSlot))
        {
            CGS_ASSERT(false, "Could not create resource - out of entries. Try allocating pool with a larger possible entry count (see muMaxResources in request event)\n");   // :1016
            return CREATERESULT_OUTOFENTRIES;
        }

        *lpiOutIndex = liSlot;
        return CreateEntryInSlot(lpNewResource, lppOutEntry, liSlot, lbAllocateMemory);
    }

    // 0x828F5B28 - bring the management data up: size + initialise the id->index hash, then
    // mark every entry free. The X360 also resets each entry's ResourcePtr alias-list to an
    // empty self-referential state (Entry+52..); that region is modelled with the ResourcePtr
    // propagation and is not needed for the basic load path, so it is omitted here.
    void Pool::InitManagementData()
    {
        // Hash length = next power of two >= 3 * muMaxResources.
        u32 luLength = 3u * muMaxResources;
        --luLength;
        luLength |= luLength >> 1;
        luLength |= luLength >> 2;
        luLength |= luLength >> 4;
        luLength |= luLength >> 8;
        luLength |= luLength >> 16;
        ++luLength;
        mHashTable.Initialize(mpHashEntries, static_cast<s32>(luLength));

        for (u32 luIndex = 0; luIndex < muNumFreeResources; ++luIndex)
            mpx8ResourceStatuses[luIndex] = 0;

        miNumResourcesInPurgatory = 0;
    }

    // 0x82901BC0 - stand the pool up from InitOptions: build a LinearMalloc over each memory
    // pool's backing memory, carve each heap's managed region, reserve one overhead block for
    // the management arrays / hash / heap-node arrays, prepare the three Heaps, copy the deps,
    // then InitManagementData + mark valid.
    //
    // PC MEMORY MODEL: the X360 sizes the overhead with hardcoded 88-byte entries + a 100*max
    // slack; the x64 Entry/HeapEntryNode are wider, so the overhead is recomputed from sizeof
    // (a bump allocator, so over-reserving is safe). This is the platform layer flagged for a
    // PC variant -- see [[platform-builds-and-dumps]] (TUB is the PC reference). Debug-only
    // bits (per-heap SPrintf name, PrintOverheadMemoryRequired) are reduced/omitted.
    void Pool::InitPool(const InitOptions* lpOptions)
    {
        miId = lpOptions->miId;

        // One allocator per memory pool, over the caller-supplied backing memory; the heap's
        // managed region is carved from it.
        CgsMemory::LinearMalloc laPoolAllocators[KI_NUM_TYPES];
        void* lapHeapMemory[KI_NUM_TYPES] = { 0, 0, 0 };
        for (u32 luMemType = 0; luMemType < KI_NUM_TYPES; ++luMemType)
        {
            const u32 luPoolSize = lpOptions->mDescriptor.m_baseResourceDescriptors[luMemType].m_size;
            if (luPoolSize == 0)
                continue;

            CGS_ASSERT(lpOptions->mResource.m_baseResources[luMemType] != 0, "Memory not allocated for memory type");   // :270
            laPoolAllocators[luMemType].Construct();
            laPoolAllocators[luMemType].Create(lpOptions->mResource.m_baseResources[luMemType], luPoolSize);
            laPoolAllocators[luMemType].SetAlignment(4);

            const u32 luHeapMemorySize = lpOptions->maHeapInfo[luMemType].muHeapMemorySize;
            if (luHeapMemorySize != 0)
            {
                lapHeapMemory[luMemType] = laPoolAllocators[luMemType].Malloc(luHeapMemorySize);
                CGS_ASSERT(lapHeapMemory[luMemType] != 0, "Failed to allocate for memory type");   // :281
            }
        }

        const u32 luMax = lpOptions->muMaxResources;

        // Hash length = next power of two >= 3 * muMaxResources (matches InitManagementData).
        u32 luHashLength = 3u * luMax;
        --luHashLength;
        luHashLength |= luHashLength >> 1;  luHashLength |= luHashLength >> 2;
        luHashLength |= luHashLength >> 4;  luHashLength |= luHashLength >> 8;
        luHashLength |= luHashLength >> 16; ++luHashLength;

        u32 luOverheadSize = static_cast<u32>(sizeof(Entry)) * luMax                       // mpResourceEntries
                           + luMax                                                          // mpx8ResourceStatuses (u8)
                           + 2u * luMax                                                     // mpiResourceRefCounts (s16)
                           + 4u * luMax                                                     // mpiResourceImportCounts (s32)
                           + 2u * luMax                                                     // mpnBatchIndices (s16)
                           + static_cast<u32>(sizeof(HashTable::HashEntry)) * luHashLength; // mpHashEntries
        for (u32 luMemType = 0; luMemType < KI_NUM_TYPES; ++luMemType)
            luOverheadSize += static_cast<u32>(sizeof(Heap::HeapEntryNode)) * lpOptions->maHeapInfo[luMemType].muMaxNodes;
        luOverheadSize += 16u * (KI_NUM_TYPES + 8);   // per-allocation alignment slack

        void* lpOverheadBlock = laPoolAllocators[E_MEMTYPE_MAINMEMORY].Malloc(luOverheadSize);
        CGS_ASSERT(lpOverheadBlock != 0, "Failed to allocate overhead memory for pool");   // :289

        // copy the pool name (X360 asserts < 32 chars and uses CgsStringUtils; a bounded copy here)
        {
            const char* lpcSrc = lpOptions->mpcName;
            u32 luChar = 0;
            for (; luChar < 31 && lpcSrc != 0 && lpcSrc[luChar] != 0; ++luChar)
                macName[luChar] = lpcSrc[luChar];
            macName[luChar] = 0;
        }

        CgsMemory::LinearMalloc lOverheadAllocator;
        lOverheadAllocator.Construct();
        lOverheadAllocator.Create(lpOverheadBlock, luOverheadSize);
        lOverheadAllocator.SetAlignment(4);

        mpnBatchIndices         = static_cast<s16*>(lOverheadAllocator.Malloc(2u * luMax));
        mpResourceEntries       = static_cast<Entry*>(lOverheadAllocator.Malloc(static_cast<u32>(sizeof(Entry)) * luMax));
        mpx8ResourceStatuses    = static_cast<u8*>(lOverheadAllocator.Malloc(luMax));
        mpiResourceRefCounts    = static_cast<s16*>(lOverheadAllocator.Malloc(2u * luMax));
        mpiResourceImportCounts = static_cast<s32*>(lOverheadAllocator.Malloc(4u * luMax));
        mpHashEntries           = static_cast<HashTable::HashEntry*>(lOverheadAllocator.Malloc(static_cast<u32>(sizeof(HashTable::HashEntry)) * luHashLength));

        muMaxResources         = static_cast<u16>(luMax);
        muNumFreeResources     = static_cast<u16>(luMax);
        miRefCountThreshold    = lpOptions->miRefCountThreshold;
        miBankId               = lpOptions->miBankId;
        mbAllowDefragmentation = lpOptions->mbAllowDefragmentation;
        meDefragStage          = DEFRAGSTAGE_IDLE;   // (defrag-state init; the full machinery is deferred)

        mResource   = lpOptions->mResource;
        mDescriptor = lpOptions->mDescriptor;

        // prepare each heap over its carved memory region (heap name = pool name; X360 SPrintf'd "%sH%d")
        for (u32 luMemType = 0; luMemType < KI_NUM_TYPES; ++luMemType)
        {
            const HeapInfo& lrHeapInfo = lpOptions->maHeapInfo[luMemType];
            if (lrHeapInfo.muHeapMemorySize == 0)
                continue;
            CGS_ASSERT(lrHeapInfo.muHeapAlignment >= 16, "For performance reasons heaps must have a minimum alignment of 16 bytes!");   // :353
            maHeaps[luMemType].Prepare(lrHeapInfo.muMaxNodes, &lOverheadAllocator, lapHeapMemory[luMemType],
                                       lrHeapInfo.muHeapMemorySize, lrHeapInfo.muHeapAlignment, macName);
        }

        miNumDependencies = lpOptions->miNumDependencies;
        for (s32 liDep = 0; liDep < miNumDependencies; ++liDep)
            mapDependencies[liDep] = lpOptions->mapDependencies[liDep];

        InitManagementData();
        mbIsValid = true;
    }

    // ---- fixup / imports ----------------------------------------------------------

    // 0x828EB860 - rebase a freshly-loaded resource's internal pointers (Type::FixUp over the
    // rw::Resource view of its per-pool memory) then deserialise it (Type::DeSerialise).
    void Pool::FixUpEntry(Entry* lpEntry)
    {
        rw::Resource lrwResource = rw::Resource();
        lpEntry->mResource.ConvertToRWResource(lrwResource);
        lpEntry->mpResourceType->FixUp(lpEntry->mResource.m_baseResources[0], lrwResource);
        lpEntry->mpResourceType->DeSerialise(lpEntry->mResource.m_baseResources[0]);
    }

    // 0x828EB920 - the post-fixup pass (Type::PostFixUp), run after imports are resolved.
    void Pool::PostFixUpEntry(Entry* lpEntry)
    {
        rw::Resource lrwResource = rw::Resource();
        lpEntry->mResource.ConvertToRWResource(lrwResource);
        lpEntry->mpResourceType->PostFixUp(lpEntry->mResource.m_baseResources[0], lrwResource);
    }

    // 0x828F6068 - resolve one cross-resource import: find the imported resource by id (across
    // this pool and its dependencies) and write its main-memory pointer into the importing
    // resource at the import's byte offset. Returns false (and writes null) if unresolved.
    bool Pool::ResolveImportForEntry(Entry* lpEntry, BundleV2::ImportEntry* lpImport)
    {
        CGS_ASSERT(lpImport->muOffset < lpEntry->mResourceDescriptor.m_baseResourceDescriptors[0].m_size,
                   "Import offset out of range\n");   // :1730

        Pool*  lpOutPool = 0;
        Entry* lpFound   = FindResourceWithDependencies(lpImport->mResourceId, &lpOutPool, true, 3, 0);
        void** lppDest   = reinterpret_cast<void**>(static_cast<char*>(lpEntry->mResource.m_baseResources[0]) + lpImport->muOffset);

        if (lpFound != 0)
        {
            CGS_ASSERT(lpFound->mResource.m_baseResources[0] != 0, "Entry to import is NULL - this is insane!\n");   // :1766
            *lppDest = lpFound->mResource.m_baseResources[0];
            return true;
        }

        *lppDest = 0;
        CGS_ASSERT(false, "Failed to resolve import for resource\n");   // :1780
        return false;
    }

    // 0x828FAEA0 - resolve every import of the resource at liIndex. The import table lives at
    // (main memory + muImportTableOffset) as a run of 16-byte ImportEntry records.
    bool Pool::ResolveImportsForEntry(s32 liIndex)
    {
        bool lbAllResolved = true;

        const s32 liImportCount = mpiResourceImportCounts[liIndex];
        if (liImportCount <= 0)
            return true;

        Entry* lpEntry = &mpResourceEntries[liIndex];
        BundleV2::ImportEntry* lpImport =
            reinterpret_cast<BundleV2::ImportEntry*>(static_cast<char*>(lpEntry->mResource.m_baseResources[0]) + lpEntry->muImportTableOffset);

        for (s32 liImport = 0; liImport < liImportCount; ++liImport, ++lpImport)
        {
            if (!ResolveImportForEntry(lpEntry, lpImport))
                lbAllResolved = false;
        }
        return lbAllResolved;
    }
}
