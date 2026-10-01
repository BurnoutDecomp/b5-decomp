#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h"
// Native raster ownership is covered by PCResourceTextureLifetime.
namespace renderengine {
void TextureResource_OnEntryFixedUp(const void*,void*) {}
void TextureResource_OnEntryFreed(const void*,void*) {}
}
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsAllocatePoolModuleState.h"
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsDeAllocatePoolModuleState.h"
#include "GameShared/GameClasses/System/Resource/CgsEntryListResource.h"
#include "GameShared/GameClasses/System/Resource/CgsPoolModuleIO.h"
#include "GameShared/GameClasses/Memory/CgsMemoryModuleIO.h"
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

static int assertions, checks, failures, retirements;
static const void* retiredBases[3];
static size_t retiredSizes[3];
static u32 freeBytesAtRetirement[3];
static CgsResource::Pool* observedRetirementPool;
static bool observeLifecycle, validationResult = true;
static int lifecycle[32], lifecycleCount;
struct ResourceBody { void* imported; int id; };
static void TraceLifecycle(int phase, const void* resource) {
    if(observeLifecycle && lifecycleCount<32)
        lifecycle[lifecycleCount++]=phase+static_cast<const ResourceBody*>(resource)->id;
}
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* message, const char*, int) { ++assertions; std::printf("ASSERT %s\n", message); return 0; }
void* EndAssert() { return nullptr; }
}
namespace Log {
static DebugPrint sink;
DebugPrint* gpDebugPrint = &sink;
StrStreamBase& DebugPrint::operator<<(const char*) { return *this; }
}
namespace Message { u64 gxMessageFilterFlags = 0; }
}
namespace renderengine { void WorldGeometry_OnResourceMemoryFreed(const void* base, size_t size) {
    if(retirements<3) {
        retiredBases[retirements]=base; retiredSizes[retirements]=size;
        if(observedRetirementPool)
            freeBytesAtRetirement[retirements]=observedRetirementPool->maHeaps[retirements].GetAmountFreeBytes();
    }
    ++retirements;
} }
namespace CgsResource {
static bool sabLoggedPoolFull[64] = {};
struct MemberType : Type {
    u32 GetTypeID() const override { return 123; }
    void FixUp(void* resource, const rw::Resource&) const override { TraceLifecycle(100,resource); }
    bool DeSerialise(void* resource) const override { TraceLifecycle(200,resource); return true; }
    void PostFixUp(void* resource, const rw::Resource&) const override { TraceLifecycle(300,resource); }
    bool DebugValidate(const void* resource) const override { TraceLifecycle(400,resource); return validationResult; }
};
static MemberType memberType;
// Module registry/slot lookup is the fixture boundary. Allocation, fixup, unload,
// response generation, queues, state machines and heap bodies are production.
class PoolModule {
public:
    static const int KI_MAX_ALLOCATION_REQUESTS = 4096;
    enum { E_UPDATESTATE_IDLE=0, E_UPDATESTATE_DEALLOCATING_LIST=2 };
    Pool* maPools = nullptr;
    int mProcessState = E_UPDATESTATE_IDLE;
    DeAllocatePoolModuleState mDeAllocateState = {};
    const Type* FindResourceType(u32 id) { return id==123 ? &memberType : nullptr; }
    int GetPoolIndex(s32 id) { return maPools && maPools->GetId()==id ? 0 : -1; }
    static void ConvertPoolRequestOptions(const void*, void*);
    void DoDeletePoolRequest(const void*);
    void DoFixUpAndResolveResourceListRequest(const Events::FixUpAndResolveResourceListRequest*,PoolIO::OutputBuffer*);
    void DoUnloadResourceListRequest(const Events::UnloadResourceListRequest*,PoolIO::OutputBuffer*);
};
CgsDev::StrStreamBase& operator<<(CgsDev::StrStreamBase& out, ID) { return out; } // disabled diagnostic formatting only
}
#include "pc_resource_heap.inc"
#include "pc_resource_batch.inc"

using namespace CgsResource;
static void Check(bool ok, const char* message) { ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n", message); } }
struct Fixture {
    Pool pool;
    PoolModule registry;
    AllocatePoolModuleState state = {};
    AllocListSet set = {};
    AllocRequest requests[3][16] = {};
    AllocResult results[3][16] = {};
    BundleV2::ResourceEntry entries[2] = {};
    SmallResource outputs[2];
    bool needs[2] = {};
    void* backing[3];
    ID list;
    explicit Fixture(int capacity=16) {
        Pool::InitOptions options = {};
        options.miId=3; options.mpcName="batch fixture"; options.muMaxResources=capacity;
        options.muMaxImports=64; options.miNumDependencies=0; options.mbAllowDefragmentation=false;
        for (int t=0; t<3; ++t) {
            backing[t]=std::calloc(1,65536);
            options.mResource.m_baseResources[t]=backing[t];
            options.mDescriptor.m_baseResourceDescriptors[t].m_size=65536;
            options.mDescriptor.m_baseResourceDescriptors[t].m_alignment=16;
            options.maHeapInfo[t].muHeapMemorySize=4096;
            options.maHeapInfo[t].muMaxNodes=64;
            options.maHeapInfo[t].muHeapAlignment=16;
            set.mapAllocRequests[t]=requests[t]; set.mapAllocResults[t]=results[t];
        }
        pool.Construct(); pool.InitPool(&options); state.Construct(&registry);
        registry.maPools=&pool; registry.mDeAllocateState.Construct(&registry);
        list.SetHash(0x80000000ABCDEFFFull);
        for (int i=0; i<2; ++i) {
            entries[i].mResourceId.SetHash(101+i); entries[i].muResourceTypeId=123;
            entries[i].muImportCount=static_cast<u16>(i+1);
            for (int t=0; t<3; ++t) {
                entries[i].mauUncompressedSizeAndAlignment[t]=0x40000000u | (t==1 ? 1024u : 32u);
                outputs[i].m_baseResources[t]=nullptr;
            }
        }
    }
    ~Fixture() { for (void* memory : backing) std::free(memory); }
    void Begin(bool allow=false) { state.BeginAllocation(&pool,list,entries,2,&set,needs,outputs,allow); }
    int Slot(ID id) { return pool.FindResourceIndex(id,true,3); }
};

int main() {
    memberType.InitCachedValues();
    {
        Fixture f;
        Check(!f.pool.IsDefragmenting() && f.pool.miDefragFrame==-1 && f.pool.mpCurrentScratchPool==nullptr,
              "newly initialized pool has the original idle defrag latch and no scratch owner");
        f.Begin();
        Check(f.state.Update()==AllocatePoolModuleState::E_RESULT_SUCCESS, "creation/allocation/merge completes in one successful update");
        Check(f.needs[0] && f.needs[1] && f.set.manAllocRequestCounts[0]==3
              && f.set.manAllocRequestCounts[1]==2 && f.set.manAllocRequestCounts[2]==2, "requests include list only in main memory");
        bool complete=true;
        for (int i=0;i<2;++i) for (int t=0;t<3;++t) complete &= f.outputs[i].m_baseResources[t]!=nullptr;
        Check(complete, "stream destinations receive all three memory types");
        int slot=f.Slot(f.entries[0].mResourceId), listSlot=f.Slot(f.list);
        Check(slot>=0 && listSlot>=0 && f.pool.GetEntryStatusDirect(slot)==1, "allocation leaves members loading until stream fixup");
        Check(f.pool.mpiResourceImportCounts[slot]==1 && f.pool.mpiResourceImportCounts[listSlot]==0, "merge replaces source-index markers with real import counts");
        auto* data=static_cast<EntryListResource*>(f.pool.mpResourceEntries[listSlot].mResource.m_baseResources[0]);
        Check(data && data->muNumEntries==2 && data->mIds[0]==f.entries[0].mResourceId && data->mIds[1]==f.entries[1].mResourceId,
              "batch list contains original IDs in canonical layout");
        Events::AllocateResourceListResponse response = {};
        f.state.GenerateResponse(&response);
        Check(response.miPoolId==3 && response.mListId==f.list && response.mpEntries==f.entries
              && response.miNumEntries==2 && response.mpNeeds==f.needs && response.mpResources==f.outputs
              && response.mpListEntry==&f.pool.mpResourceEntries[listSlot], "native response points to complete allocation working set");
        u32 freeBefore[3]; for(int t=0;t<3;++t) freeBefore[t]=f.pool.maHeaps[t].GetAmountFreeBytes();
        f.Begin();
        Check(f.state.Update()==AllocatePoolModuleState::E_RESULT_SUCCESS && !f.needs[0] && !f.needs[1], "second list allocation reuses resident members");
        bool noAllocation=true; for(int t=0;t<3;++t) noAllocation &= f.set.manAllocRequestCounts[t]==0 && freeBefore[t]==f.pool.maHeaps[t].GetAmountFreeBytes();
        Check(noAllocation && f.pool.GetEntryRefCount(slot)==2 && f.pool.GetEntryRefCount(listSlot)==2, "repeated list allocation takes references without allocating heaps");
    }
    {
        Fixture f;
        Heap& heap=f.pool.maHeaps[1];
        const u32 capacity=heap.GetAmountFreeBytes();
        u16 node=0;
        void* held=heap.Malloc(3072,"",reinterpret_cast<void*>(99),0xFFFF,&node,false,true,nullptr);
        Check(held && heap.GetAmountFreeBytes()==capacity-3072, "fixture occupies physical memory before pending allocation");
        f.pool.miNumResourcesInPurgatory=1;
        f.Begin();
        Check(f.state.Update()==AllocatePoolModuleState::E_RESULT_PEND && f.state.mbWaitingForPurgatory, "allocation waits for dying resources before reporting no room");
        Check(f.set.maeAllocRequestResults[0]==E_BATCHALLOCRESULT_SUCCESS && f.set.maeAllocRequestResults[1]==E_BATCHALLOCRESULT_FAIL_NO_ROOM
              && f.set.maeAllocRequestResults[2]==E_BATCHALLOCRESULT_SUCCESS, "successful memory types remain allocated while one type waits");
        void* main=f.results[0][0].mpData; void* graphics=f.results[2][0].mpData;
        const u32 mainFree=f.pool.maHeaps[0].GetAmountFreeBytes(), graphicsFree=f.pool.maHeaps[2].GetAmountFreeBytes();
        heap.Free(held); f.pool.miNumResourcesInPurgatory=0;
        Check(heap.GetAmountFreeBytes()==capacity, "pointer free returns dying block to the real heap");
        const u32 result=f.state.Update();
        Check(result==AllocatePoolModuleState::E_RESULT_SUCCESS, "pending allocation resumes after physical block is freed");
        Check(f.results[0][0].mpData==main && f.results[2][0].mpData==graphics
            && f.pool.maHeaps[0].GetAmountFreeBytes()==mainFree && f.pool.maHeaps[2].GetAmountFreeBytes()==graphicsFree,
            "retry does not reallocate successful memory types");
        Check(result==AllocatePoolModuleState::E_RESULT_SUCCESS && f.outputs[0].m_baseResources[0]==main
            && f.outputs[0].m_baseResources[2]==graphics, "merge publishes retained allocations after retry");
    }
    {
        Fixture f;
        Heap& heap=f.pool.maHeaps[1];
        void* blocks[4]; u16 nodes[4];
        for(int i=0;i<4;++i) blocks[i]=heap.Malloc(1024,"",reinterpret_cast<void*>(99),0xFFFF,&nodes[i],false,true,nullptr);
        heap.Free(blocks[0]); heap.Free(blocks[2]);
        f.entries[0].mauUncompressedSizeAndAlignment[1]=0x40000600;
        f.entries[1].mauUncompressedSizeAndAlignment[1]=0x40000020;
        f.Begin();
        const u32 result=f.state.Update();
        Check(result==AllocatePoolModuleState::E_RESULT_DEFRAGMENT
            && f.state.meState==AllocatePoolModuleState::E_STATE_DEFRAG_WAITING,
            "fragmented space requests defragmentation instead of reporting no room");
        Check(f.outputs[0].m_baseResources[0]==nullptr && f.state.miCountDown==1,
            "defrag handoff leaves stream destinations unpublished");
        if(result==AllocatePoolModuleState::E_RESULT_DEFRAGMENT) {
            // The planner boundary returns completed allocations after making contiguous space.
            heap.Free(blocks[1]);
            f.set.maeAllocRequestResults[1]=f.pool.ExecuteBatchAllocation(&f.set,1);
            Check(f.state.Update()==AllocatePoolModuleState::E_RESULT_SUCCESS
                && f.outputs[0].m_baseResources[1]==f.results[1][0].mpData,
                "completed defrag allocations merge without restarting the batch");
        }
    }
    {
        Fixture f;
        NewResource request = {};
        request.mID.SetHash(777); request.mpResourceType=&memberType;
        for(int t=0;t<3;++t) { request.mResourceDescriptor.m_baseResourceDescriptors[t].m_size=t==1?8192:32; request.mResourceDescriptor.m_baseResourceDescriptors[t].m_alignment=16; }
        const u32 before=f.pool.maHeaps[0].GetAmountFreeBytes();
        const u16 slots=f.pool.muNumFreeResources;
        Entry* entry=nullptr; s32 slot=-1;
        Check(f.pool.CreateEntry(&request,&entry,&slot,true)==Pool::CREATERESULT_OUTOFMEMORY, "later memory-type failure is reported");
        Check(f.pool.maHeaps[0].GetAmountFreeBytes()==before && f.pool.muNumFreeResources==slots,
              "real partial-allocation cleanup restores earlier heap and entry slot");
    }
    {
        Fixture f(2); f.Begin(true);
        Check(f.state.Update()==AllocatePoolModuleState::E_RESULT_FAILED_SAFELY && f.state.meState==AllocatePoolModuleState::E_STATE_IDLE,
              "insufficient member-plus-list capacity honors allowed failure");
        Check(f.pool.muNumFreeResources==2, "batch slot reservation is atomic when capacity is insufficient");
    }
    {
        Fixture f;
        Heap& heap=f.pool.maHeaps[0];
        u16 node; void* p=heap.Malloc(256,"",reinterpret_cast<void*>(99),0xFFFF,&node,false,true,nullptr);
        s32 used,unused,allocated,freeNodes,usedBytes,freeBytes,largest;
        heap.GetNodeUsageStatistics(&used,&unused,&allocated,&freeNodes,&usedBytes,&freeBytes,&largest);
        Check(allocated==1 && usedBytes==256 && freeBytes==3840 && largest==3840 && used==allocated+freeNodes,
              "heap statistics distinguish blocks and byte occupancy");
        heap.Free(p);
        heap.GetNodeUsageStatistics(&used,&unused,&allocated,&freeNodes,&usedBytes,&freeBytes,&largest);
        Check(allocated==0 && freeNodes==1 && usedBytes==0 && freeBytes==4096 && largest==4096, "pointer free coalesces and restores heap statistics");
    }
    {
        Fixture f;
        NewResource request = {};
        request.mID.SetHash(888); request.mpResourceType=&memberType;
        for(int t=0;t<3;++t) {
            request.mResourceDescriptor.m_baseResourceDescriptors[t].m_size=32;
            request.mResourceDescriptor.m_baseResourceDescriptors[t].m_alignment=16;
        }
        Entry* entry=nullptr; s32 slot=-1;
        Check(f.pool.CreateEntry(&request,&entry,&slot,true)==Pool::CREATERESULT_OK, "live replacement starts with real allocated entry");
        f.pool.SetEntryStatus(slot,2);
        void* old[3]; for(int t=0;t<3;++t) old[t]=entry->mResource.m_baseResources[t];
        {
            ResourceHandle handle;
            handle.mpResourceMemory=&entry->mResource.m_baseResources[0]; handle.mpSourceEntry=entry;
            BaseResourcePtr first, second;
            first.CreateFromHandle(&handle); second.CreateFromHandle(&handle);
            retirements=0; observedRetirementPool=&f.pool;
            f.pool.DeleteMemoryForEntry(request.mID);
            bool retired=true, reclaimed=true;
            for(int t=0;t<3;++t) {
                retired &= retiredBases[t]==old[t] && retiredSizes[t]==32 && freeBytesAtRetirement[t]==4064
                    && *static_cast<u8*>(old[t])==0xCD;
                reclaimed &= f.pool.maHeaps[t].GetAmountFreeBytes()==4096;
            }
            Check(retirements==3 && retired, "live replacement retires each original range before freeing it and applies console debug wipe");
            Check(reclaimed && f.pool.GetEntryRefCount(slot)==1 && f.pool.GetEntryStatusDirect(slot)==2,
                  "live replacement frees memory while retaining entry and references");
            for(int t=0;t<3;++t) {
                u16 node;
                f.pool.maHeaps[t].Malloc(64,"",reinterpret_cast<void*>(99),0xFFFF,&node,false,true,nullptr);
            }
            SmallResource out;
            Check(f.pool.ReAllocateMemoryForEntry(request.mID,&request.mResourceDescriptor,7,&out), "live replacement allocates new memory");
            bool propagated=true;
            BaseResourcePtr* aliases[] = {&first,&second};
            for(BaseResourcePtr* alias : aliases) {
                propagated &= alias->mpResourceMemory==out.m_baseResources[0]
                    && alias->mHandle.mpResourceMemory==out.m_baseResources[1]
                    && alias->mHandle.mpSourceEntry==out.m_baseResources[2];
                auto current=alias->GetResourceHandle();
                propagated &= current.mpResourceMemory==handle.mpResourceMemory && current.mpSourceEntry==entry;
            }
            Check(propagated && out.m_baseResources[0]!=old[0], "real alias ring receives changed native pointers and retains source handle");
            Check(f.pool.GetEntryStatusDirect(slot)==1 && f.pool.mpiResourceImportCounts[slot]==7
                && f.pool.GetEntryRefCount(slot)==1, "replacement restores loading state and new import count without altering references");
        }
        f.pool.DeleteEntry(static_cast<s16>(slot));
        observedRetirementPool=nullptr;
        Check(f.Slot(request.mID)<0 && f.pool.muNumFreeResources==16, "unconditional deletion retires replacement and frees hash/slot");
    }
    {
        Fixture f;
        for(int i=0;i<2;++i) {
            f.entries[i].mauUncompressedSizeAndAlignment[0]=0x40000060;
            f.entries[i].muImportOffset=32; f.entries[i].muImportCount=1;
        }
        f.Begin();
        Check(f.state.Update()==AllocatePoolModuleState::E_RESULT_SUCCESS,"lifecycle fixture allocates real heap resources");
        ID ids[2]={f.entries[0].mResourceId,f.entries[1].mResourceId};
        for(int i=0;i<2;++i) {
            auto* data=static_cast<ResourceBody*>(f.outputs[i].m_baseResources[0]);
            std::memset(data,0,96); data->id=i+1;
            auto* import=reinterpret_cast<BundleV2::ImportEntry*>(reinterpret_cast<char*>(data)+32);
            import->mResourceId=ids[1-i]; import->muOffset=0;
        }
        observeLifecycle=true; lifecycleCount=0;
        f.pool.FixUpAndResolveResourceList(ids,2,1,1,false,false);
        Check(lifecycleCount==2 && lifecycle[0]==102 && lifecycle[1]==202,
              "partial fixup obeys nonzero first index and runs deserialise after fixup");
        Check(static_cast<ResourceBody*>(f.outputs[1].m_baseResources[0])->imported==f.outputs[0].m_baseResources[0]
            && static_cast<ResourceBody*>(f.outputs[0].m_baseResources[0])->imported==nullptr,
              "partial range resolves only its own native import slots");
        Check(f.pool.GetEntryStatusDirect(f.Slot(ids[0]))==1 && f.pool.GetEntryStatusDirect(f.Slot(ids[1]))==1,
              "partial fixup leaves resources unpublished");
        f.pool.FixUpAndResolveResourceList(ids,2,0,1,false,false);
        Check(lifecycleCount==4 && lifecycle[2]==101 && lifecycle[3]==201
            && static_cast<ResourceBody*>(f.outputs[0].m_baseResources[0])->imported==f.outputs[1].m_baseResources[0],
              "second range resolves reciprocal imports without refixing the earlier range");
        f.pool.FixUpAndResolveResourceList(ids,2,2,0,true,false);
        Check(lifecycleCount==8 && lifecycle[4]==301 && lifecycle[5]==401 && lifecycle[6]==302 && lifecycle[7]==402,
              "empty final range publishes and validates all earlier chunks in list order");
        Check(f.pool.GetEntryStatusDirect(f.Slot(ids[0]))==2 && f.pool.GetEntryStatusDirect(f.Slot(ids[1]))==2,
              "final fixup marks both members loaded");
        f.pool.FixUpAndResolveResourceList(ids,2,0,2,true,false);
        Check(lifecycleCount==8,"already loaded shared resources are never fixed or post-fixed twice");
        observeLifecycle=false;
    }
    {
        Fixture dependency, f;
        for(int i=0;i<2;++i) {
            dependency.entries[i].mResourceId.SetHash(201+i);
            dependency.entries[i].muImportCount=0; f.entries[i].muImportCount=0;
        }
        dependency.list.SetHash(0x8000000000000200ull);
        dependency.Begin(); dependency.state.Update();
        ID dependencyIds[2]={dependency.entries[0].mResourceId,dependency.entries[1].mResourceId};
        f.pool.miNumDependencies=1; f.pool.mapDependencies[0]=&dependency.pool;
        f.pool.FixUpAndResolveResourceList(dependencyIds,2,0,2,true,false);
        Check(dependency.pool.GetEntryStatusDirect(dependency.Slot(dependencyIds[0]))==1,
              "local fixup does not publish dependency resources");
        f.pool.FixUpAndResolveResourceList(dependencyIds,2,0,2,true,true);
        Check(dependency.pool.GetEntryStatusDirect(dependency.Slot(dependencyIds[0]))==2
            && dependency.pool.GetEntryStatusDirect(dependency.Slot(dependencyIds[1]))==2,
              "dependency fixup publishes entries in their owning pool");
        dependency.pool.SetEntryStatus(dependency.Slot(dependency.list),2);
        f.entries[0].mResourceId=dependencyIds[0];
        f.Begin();
        Check(f.state.Update()==AllocatePoolModuleState::E_RESULT_SUCCESS && !f.needs[0] && f.needs[1],
              "new list shares loaded dependency and allocates only its local member");
        ID ids[2]={f.entries[0].mResourceId,f.entries[1].mResourceId};
        f.pool.FixUpAndResolveResourceList(ids,2,0,2,true,false);
        f.pool.SetEntryStatus(f.Slot(f.list),2);
        f.pool.miRefCountThreshold=3; dependency.pool.miRefCountThreshold=5;
        const int own=f.Slot(ids[1]), shared=dependency.Slot(ids[0]), list=f.Slot(f.list);
        const u32 freeBefore=f.pool.maHeaps[0].GetAmountFreeBytes();
        DeAllocatePoolModuleState release;
        release.Construct(&f.registry); release.Begin(&f.pool,f.list);
        Check(f.pool.GetEntryRefCount(own)==0 && f.pool.GetEntryRefCount(list)==0
            && dependency.pool.GetEntryRefCount(shared)==1,
              "unload decrements each member in its actual owning pool");
        Check(f.pool.maHeaps[0].GetAmountFreeBytes()==freeBefore && f.pool.GetEntryStatusDirect(own)==2,
              "unload acknowledgement retains memory for the pool retirement countdown");
        Check(release.Update()==DeAllocatePoolModuleState::E_UPDATE_BUSY && release.miFramesRemaining==2,
              "deallocation driver remains busy during retirement delay");
        release.Begin(&dependency.pool,dependency.list);
        Check(release.miFramesRemaining==5 && dependency.pool.GetEntryRefCount(shared)==0,
              "another unload extends delay to longer owning-pool threshold");
        bool busy=true;
        for(int i=0;i<5;++i) busy &= release.Update()==DeAllocatePoolModuleState::E_UPDATE_BUSY;
        Check(busy && release.Update()==DeAllocatePoolModuleState::E_UPDATE_IDLE
            && release.GetPendingAllocation()==nullptr,"deallocation completes after countdown without inventing pending requests");
    }
    {
        Fixture f;
        for(int i=0;i<2;++i) f.entries[i].muImportCount=0;
        f.Begin(); f.state.Update();
        PoolIO::OutputBuffer out;
        out.Construct(); out.LockForWrite();
        Events::FixUpAndResolveResourceListRequest fix = {};
        fix.mpUser=reinterpret_cast<CgsModule::BaseEventReceiverQueue*>(0x123456789000ull);
        fix.miEventId=91; fix.miPoolId=f.pool.GetId(); fix.mListId=f.list;
        fix.miCount=2; fix.mbFinalFixup=true;
        f.registry.DoFixUpAndResolveResourceListRequest(&fix,&out);
        out.UnlockForWrite(); out.LockForRead();
        const CgsModule::Event* event=nullptr; s32 size=0;
        auto* queue=static_cast<const PoolIO::OutputBuffer&>(out).GetPoolOutputQueue();
        int tag=queue->GetFirstEvent(&event,&size);
        const auto* fixed=reinterpret_cast<const Events::FixUpAndResolveResourceListResponse*>(event);
        Check(tag==19 && size==sizeof(*fixed) && fixed->mpUser==nullptr && fixed->miEventId==91
            && fixed->miPoolId==fix.miPoolId,"fixup response echoes IDs using the native record size and original null user");
        Entry* listEntry=&f.pool.mpResourceEntries[f.Slot(f.list)];
        const auto* list=static_cast<const EntryListResource*>(listEntry->mResource.m_baseResources[0]);
        Check(fixed->mListHandle.mpSourceEntry==listEntry && fixed->mListHandle.mpResourceMemory==&listEntry->mResource
            && fixed->mpIds==list->mIds && fixed->miNumEntries==2 && f.pool.GetEntryStatusDirect(f.Slot(f.list))==2,
            "fixup response retains full native owner handle and points to the pool-owned list");
        out.UnlockForRead(); out.LockForWrite(); out.GetPoolOutputQueue()->Clear();
        Events::UnloadResourceListRequest unload = {};
        unload.miPoolId=fix.miPoolId; unload.miEventId=92; unload.mListId=f.list;
        f.pool.miRefCountThreshold=3;
        f.registry.DoUnloadResourceListRequest(&unload,&out);
        out.UnlockForWrite(); out.LockForRead();
        tag=queue->GetFirstEvent(&event,&size);
        const auto* released=reinterpret_cast<const Events::UnloadResourceListResponse*>(event);
        Check(tag==21 && size==sizeof(*released) && released->mListId==f.list && released->miEventId==92
            && released->miPoolId==fix.miPoolId && released->mpUser==nullptr,"unload response preserves full list hash and native record size");
        Check(f.registry.mProcessState==PoolModule::E_UPDATESTATE_DEALLOCATING_LIST
            && f.registry.mDeAllocateState.miFramesRemaining==3 && f.pool.FindResourceIndex(f.list,true,2)>=0,
            "unload response is queued before retirement completes");
        out.UnlockForRead(); out.Destruct();

        Events::CreatePoolRequest create = {};
        create.miPoolId=19; std::strcpy(create.mpcName,"native pool");
        create.miDeletionDelayFrames=7; create.muMaxResources=200; create.muMaxImports=300;
        create.miNumDependencies=4; create.mbAllowDefragmentation=true;
        for(int i=0;i<3;++i) {
            create.mauMaxResources[i]=10+i;
            create.mDescriptor.m_baseResourceDescriptors[i].m_size=1000+i*256;
            create.mDescriptor.m_baseResourceDescriptors[i].m_alignment=16u<<i;
        }
        Pool::InitOptions options = {};
        options.miBankId=12345; options.mapDependencies[0]=&f.pool;
        options.mResource.m_baseResources[0]=reinterpret_cast<void*>(0xABCDEF123400ull);
        f.registry.ConvertPoolRequestOptions(&create,&options);
        Check(options.miId==19 && options.mpcName==create.mpcName && options.miRefCountThreshold==7
            && options.muMaxResources==200 && options.muMaxImports==300
            && options.miNumDependencies==4 && options.mbAllowDefragmentation,
            "native options conversion preserves scalar fields and full name pointer");
        bool heaps=true;
        for(int i=0;i<3;++i) heaps &= options.maHeapInfo[i].muMaxNodes==21+i*2
            && options.maHeapInfo[i].muHeapMemorySize==1000+i*256 && options.maHeapInfo[i].muHeapAlignment==(16u<<i);
        Check(heaps,"pool conversion maps each heap's count, size and alignment independently");
        Check(options.miBankId==12345 && options.mapDependencies[0]==&f.pool
            && options.mResource.m_baseResources[0]==reinterpret_cast<void*>(0xABCDEF123400ull),
            "options conversion leaves caller-owned backing and dependency fields untouched");
        CgsMemory::MemoryIO::DestroyBankResponse destroyed;
        destroyed.Construct(reinterpret_cast<CgsModule::BaseEventReceiverQueue*>(0x123400001000ull),87);
        const int before=assertions;
        f.registry.DoDeletePoolRequest(&destroyed);
        Check(assertions==before,"successful native bank response does not read event metadata as a failure code");
        destroyed.SetResult(CgsMemory::MemoryIO::E_RESULT_NOT_EMPTY);
        f.registry.DoDeletePoolRequest(&destroyed);
        Check(assertions==before+1,"failed native bank response triggers original destruction assertion");
        assertions=before;
    }
    Check(assertions==0, "no unexpected engine assertion");
    std::printf("PCResourceBatch: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
