#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourceBasePool.h"   // Entry, BasePool
#include "GameShared/GameClasses/System/Resource/CgsResourceHeap.h"       // Heap + Alloc*/Relocate*/LinearHeapNode/EBatchAllocResult
#include "GameShared/GameClasses/System/Resource/CgsResourceHashTable.h"  // HashTable
#include "GameShared/GameClasses/System/Resource/CgsSmallResource.h"      // SmallResource, SmallResourceDescriptor
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"         // ID
#include "GameShared/GameClasses/System/Resource/CgsResourceBundle2.h"    // BundleV2::ResourceEntry / ImportEntry

// CgsResource::Pool - a fixed-capacity store of loaded resources backed by three Heaps
// (one per in-memory pool: main / graphics-system / graphics-local). It owns the entry
// array + parallel status/refcount/import-count arrays, an ID->index HashTable, and a
// six-stage relocating defragmenter (ScratchPool + Relocator). The bundle loader drives
// it: CreateEntry reserves a slot, AllocateMemoryForResource carves Heap memory,
// FixUpEntry/ResolveImportsForEntry patch pointers, and FindResource looks up by ID.
//
// SOURCES: shape/field-names/method-signatures/enum-values from the DecFIGS *PS3* DWARF
// (CgsResourcePool.h); the X360 ARTIST target has no DWARF, so the layout was verified
// against the X360 IDA (e.g. Pool::FindResourceIndex 0x828ED190 puts mpx8ResourceStatuses
// at +236, mpiResourceRefCounts at +240 and mHashTable at +392; Heap::Reprepare confirms
// maHeaps/mbIsValid). Field widths follow the x64 PC target; order/types are faithful.
// Method bodies are reconstructed (load path first) in CgsResourcePool.cpp from the X360
// IDA pseudocode.

namespace CgsMemory { class Relocator; struct RelocationParams; }

namespace CgsResource
{
    class ScratchPool;          // defrag staging pool (pointer member)
    class DebugResourceTracker; // debug-only resource tracker (pointer member)
    class BundleLoader;         // PC synchronous bundle loader (drives create + fixup)

    // CgsResourcePool.h:60 - a snapshot of a pool's occupancy (for the debug report).
    struct PoolStats
    {
        const char* mpcPoolName;
        s32 miPoolMaxResources;
        s32 miPoolUsedResources;
        s32 miPoolFreeResources;
        s32 maiHeapSize[3];
        s32 maiHeapUsed[3];
        s32 maiHeapFree[3];
        s32 maiHeapMaxResources[3];
        s32 maiHeapUsedResources[3];
        s32 maiHeapFreeResources[3];
        s32 maiHeapLargestBlock[3];
    };

    // CgsResourcePool.h:85 - the per-pool batch-allocation work set (one alloc list per
    // memory pool), filled while creating a batch of entries then executed against the Heaps.
    struct AllocListSet
    {
        u32               manAllocRequestCounts[3];
        EBatchAllocResult maeAllocRequestResults[3];
        AllocRequest*     mapAllocRequests[3];
        AllocResult*      mapAllocResults[3];

        void ClearCountsAndResults();
    };

    // CgsResourcePool.h:106 - the description of a resource about to be created in the pool.
    struct NewResource
    {
        ID                       mID;
        Entry::ResourceDescriptor mResourceDescriptor;
        s32                      miNumImports;
        const Type*              mpResourceType;
        u32                      muImportTableOffset;
    };

    // CgsResourcePool.h:124
    class Pool : public BasePool
    {
        friend class BundleLoader;   // PC loader orchestrates the per-entry fixup/import passes

    public:
        // :179
        enum EPrepareStage { E_PREPARESTAGE_START = 0, E_PREPARESTAGE_DONE = 1 };
        // :186
        enum EReleaseStage { E_RELEASESTAGE_START = 0, E_RELEASESTAGE_DONE = 1 };
        // :193
        enum ECreateResult
        {
            CREATERESULT_OK           = 0,
            CREATERESULT_NEEDDEFRAGGING = 1,
            CREATERESULT_OUTOFMEMORY  = 2,
            CREATERESULT_OUTOFENTRIES = 3,
            CREATERESULT_OUTOFIMPORTS = 4,
        };
        // :203
        enum EDefragStage
        {
            DEFRAGSTAGE_IDLE                    = 0,
            DEFRAGSTAGE_WAIT_FOR_INITIAL_DEATHS = 1,
            DEFRAGSTAGE_MAKE_TEMP_COPIES        = 2,
            DEFRAGSTAGE_WAIT_FOR_SOURCE_DEATH   = 3,
            DEFRAGSTAGE_MAKE_DEST_COPIES        = 4,
            DEFRAGSTAGE_WAIT_FOR_TEMP_DEATH     = 5,
            DEFRAGSTAGE_DONE                    = 6,
        };

        // :216 - per-heap sizing for InitPool.
        struct HeapInfo
        {
            u32 muMaxNodes;
            u32 muHeapMemorySize;
            u32 muHeapAlignment;
        };

        // :225 - everything InitPool needs to build a pool.
        struct InitOptions
        {
            s32           miId;
            const char*   mpcName;
            HeapInfo      maHeapInfo[3];
            SmallResource mResource;               // ResourceHandle::Resource
            Entry::ResourceDescriptor mDescriptor;
            u32           muMaxResources;
            u32           muMaxImports;
            s32           miRefCountThreshold;
            s32           miNumDependencies;
            Pool*         mapDependencies[16];
            s32           miBankId;
            bool          mbAllowDefragmentation;
        };

        typedef void (*FPoolReportCallback)(const PoolStats& lrStats, void* lpUserData);   // :74

        // ---- lifecycle ----------------------------------------------------------------
        u32  GetOverheadMemoryRequired(const InitOptions* lpOptions);
        // X360 0x828E0BB8 - reads the InitOptions from r3 with no `this` use (mirrors the sibling
        // ScratchPool::GetOverheadMemoryRequired), so this is a static overhead estimator.
        static u32 PrintOverheadMemoryRequired(const InitOptions* lpOptions);
        void Construct();
        bool Prepare();
        bool Release();
        void Destruct();
        void InitPool(const InitOptions* lpOptions);
        void ResetPool();

        // ---- lookup -------------------------------------------------------------------
        Entry* GetResource(s32 liIndex, bool lbCheckRefCount, u16 luStatusMask);
        Entry* FindResource(ID lID, bool lbCheckRefCount, u16 luStatusMask, s32* lpiOutIndex);
        s32    FindResourceIndex(ID lID, bool lbCheckRefCount, u16 luStatusMask);
        // Find the first in-use entry whose handler reports luTypeId (PC bring-up convenience: a
        // single-resource bundle like the debug font is looked up by type rather than by its id).
        Entry* FindFirstResourceOfType(u32 luTypeId, s32* lpiOutIndex);
        Entry* FindResourceWithDependencies(ID lID, Pool** lppOutPool, bool lbCheckRefCount, u16 luStatusMask, s32* lpiOutIndex);
        s32    FindResourceIndexWithDependencies(ID lID, Pool** lppOutPool, bool lbCheckRefCount, u16 luStatusMask);

        // ---- create / allocate --------------------------------------------------------
        ECreateResult CreateEntry(const NewResource* lpNewResource, Entry** lppOutEntry, s32* lpiOutIndex, bool lbAllocateMemory);
        ECreateResult CreateEntryInSlot(const NewResource* lpNewResource, Entry** lppOutEntry, s32 liSlot, bool lbAllocateMemory);
        bool Invalidate(SmallResource* lpResource, Entry::ResourceDescriptor* lpDescriptor);
        bool Validate();
        const s16* CreateBatchEntrySlots(s16 liNumSlots);
        ECreateResult ExecuteBatchAllocations(AllocListSet* lpAllocListSet, bool lbAllowDefrag);
        EBatchAllocResult ExecuteBatchAllocation(AllocListSet* lpAllocListSet, s32 liMemType);
        EBatchAllocResult ExecuteBatchAddressedAllocation(s32 liMemType, AllocRequestAddressed* lpRequests, AllocResult* lpResults, u32 luNumRequests, bool lbFreeOnFailure);
        ECreateResult MergeBatchAllocations(AllocListSet* lpAllocListSet, const BundleV2::ResourceEntry* lpResourceEntry, SmallResource* lpResource);

        // ---- references / update ------------------------------------------------------
        Entry* AddReference(u32 luIndex);
        bool   RemoveReference(u32 luIndex);
        void   Update();

        // ---- accessors ----------------------------------------------------------------
        const char* GetName() const;
        s32   GetId() const;
        s32   GetBankId();
        u32   GetHeapAlignment(s32 liMemType) const;
        s32   GetNumDependencies() const;
        Pool* GetDependency(s32 liIndex) const;
        // X360 reads muMaxResources (the slot-array length) at pool+0x104 to bound the entry sweep
        // (e.g. the texture-pool debug browser walks every slot). Trivial inline accessor.
        u32   GetMaxResources() const { return muMaxResources; }
        // Direct (ungated) slot accessor: the X360 debug sweeps index the entry array after their own
        // status test (mpResourceEntries[i]); this exposes that read for a const Pool*.
        const Entry* GetEntryDirect(s32 liIndex) const { return &mpResourceEntries[liIndex]; }
        u8           GetEntryStatusDirect(s32 liIndex) const { return mpx8ResourceStatuses[liIndex]; }

        // ---- import resolution / defrag (reconstructed later; load path needs only a subset) ----
        void ResolveAllResources();
        void ResolveAllTempScratchResources();
        void ResolveAllDestScratchResources();
        void ResolveAllResourcesThatImportList(const ID* lpIDs, u32 luNumIDs);
        void ResolveAllResourcesThatImportList(const ID* lpIDs, void** lppValues, u32 luNumIDs);
        void FixUpAndResolveResourceList(const ID* lpIDs, s32 liA, s32 liB, s32 liC, bool lbD, bool lbE);
        void BeginDefragmentation(ScratchPool* lpScratchPool, RelocateRequest* lpRequests, RelocateSource* lpSources, u32 luNum, s32 liMemType);
        void BeginEmergencyDefragmentation(CgsMemory::Relocator* lpRelocator, CgsMemory::RelocationParams* lpParams, RelocateRequest* lpRequests, RelocateSource* lpSources, u32 luNum, s32 liMemType);
        void DeleteEntry(s16 liIndex);
        void DeleteMemoryForEntry(ID lID);
        bool ReAllocateMemoryForEntry(ID lID, Entry::ResourceDescriptor* lpDescriptor, s32 liMemType, SmallResource* lpResource);
        bool IsDefragmenting() const;
        // Returns miDefragMemType (X360 Pool+0x1BC; -1 == this pool's defrag stage is idle/done).
        // Read by IntelliFragPoolModuleState::Update @0x828FF7F8 to detect the -1 completion sentinel.
        s32  GetDefragMemType() const;
        void BuildAllocRequestForEntry(AllocListSet* lpAllocListSet, u16 luIndex, Entry* lpEntry);

        // ---- per-entry status / refcount / imports ------------------------------------
        s16  GetEntryRefCount(s32 liIndex) const;
        void SetEntryRefCount(s32 liIndex, s16 liRefCount);
        void IncEntryRefCount(s32 liIndex);
        void DecEntryRefCount(s32 liIndex);
        u8   GetEntryStatus(s32 liIndex) const;
        void SetEntryStatus(s32 liIndex, u8 luStatus);
        s32  GetEntryImportCount(s32 liIndex) const;
        void SetEntryImportCount(s32 liIndex, s32 liCount);
        s32  GetNumEntriesInPurgatory() const;
        s32  VerifyResourceImports();
        s32  GetRefCountThreshold();
        bool GetAllowDefragmentation() const;
        void SetAllowDefragmentation(bool lbAllow);
        bool IsValid() const;
        u16  GenerateLinearHeap(s32 liMemType, LinearHeapNode* lpOut, u16 luMaxLength);
        void DebugWipeHeap(s32 liMemType);
        void DebugReport(FPoolReportCallback lpfnCallback, void* lpUserData);

    private:
        void InitManagementData();
        bool AllocateResourceEntry(Entry** lppOutEntry, s32* lpiOutIndex);
        void FreeResourceEntry(s16 liIndex);
        void FreeMemoryForResource(Entry* lpEntry);
        void FixUpEntry(Entry* lpEntry);
        void PostFixUpEntry(Entry* lpEntry);
        BundleV2::ImportEntry* GetResourceEntryImports(u16 luIndex);
        bool AllocateMemoryForResource(Entry* lpEntry, const char* lpcDebugName, s32 liSlot, const Type* lpResourceType);
        void UpdateDefrag();
        void UpdateEmergencyDefrag();
        void BeginDefragNextSetOfRelocations();
        bool AddResourceToScratchPool(u32 luIndex);
        bool RebaseResourceToScratchPool(s32 liIndex);
        bool RebaseResourceFromScratchPool(s32 liIndex);
        bool AddResourcesToScratchPool();
        bool RebaseResourcesToScratchPool();
        bool RebaseResourcesFromScratchPool();
        void DebugWipeSourceResources();
        void DebugWipeTempResources();
        u32  GenerateRangeMask(const ID* lpIDs, s32 liNumIDs);
        bool ResolveImportForEntry(Entry* lpEntry, BundleV2::ImportEntry* lpImport);
        void ClearImportsForEntry(s32 liIndex, u32 luImportCount);
        bool CheckImportsStillClearForEntry(s32 liIndex, u32 luImportCount);
        bool ResolveImportsForEntry(s32 liIndex);

        // ---- Layout (field order/types faithful; offsets verified against X360 IDA) ----
        Heap                  maHeaps[3];               // :561 main / graphics-system / graphics-local
        SmallResource         mResource;                // :562 the pool's backing memory (3 base ptrs)
        Entry::ResourceDescriptor mDescriptor;          // :563 the pool's total size/alignment
        bool                  mbIsValid;                // :565
        Entry*                mpResourceEntries;        // :567 the entry array
        u8*                   mpx8ResourceStatuses;     // :568 per-entry status bits
        s16*                  mpiResourceRefCounts;     // :569 per-entry refcount
        s32*                  mpiResourceImportCounts;  // :570 per-entry unresolved-import count
        ID*                   mpHash;                   // :572 per-entry id (parallel array)
        DebugResourceTracker* mpTracker;                // :575
        s16*                  mpnBatchIndices;          // :578 scratch slot indices for batch create
        u16                   muMaxResources;           // :579
        u16                   muNumFreeResources;       // :580
        s32                   miRefCountThreshold;      // :581
        s32                   miId;                     // :583
        char                  macName[32];              // :584
        EPrepareStage         mePrepareStage;           // :586
        EReleaseStage         meReleaseStage;           // :587
        EDefragStage          meDefragStage;            // :589
        s32                   miNumDependencies;        // :591
        Pool*                 mapDependencies[16];      // :592
        HashTable::HashEntry* mpHashEntries;            // :594 hash slot array (owned)
        HashTable             mHashTable;               // :595 id -> entry index
        s32                   miBankId;                 // :597
        CgsMemory::Relocator* mpCurrentRelocator;       // :599 active defrag relocator
        ScratchPool*          mpCurrentScratchPool;     // :600 active defrag staging pool
        u32                   muNumRelocations;         // :601
        u32                   muNextRelocation;         // :602
        RelocateRequest*      mpRelocateRequests;       // :603
        RelocateSource*       mpRelocateSources;        // :604
        s32                   miDefragFrame;            // :605
        s32                   miDefragMemType;          // :606
        bool                  mbAllowDefragmentation;   // :607
        bool                  mbDefragHitEndResource;   // :608
        s32                   miNumResourcesInPurgatory; // :610
    };
}
