#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsAllocatePoolModuleState.h"
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

static int assertions, checks, failures, retirements;
static const void* retiredBases[3];
static size_t retiredSizes[3];
static u32 freeBytesAtRetirement[3];
static CgsResource::Pool* observedRetirementPool;
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
struct MemberType : Type { u32 GetTypeID() const override { return 123; } };
static MemberType memberType;
// Type lookup is the only PoolModule boundary here. All allocation/state/heap bodies are production.
class PoolModule {
public:
    static const int KI_MAX_ALLOCATION_REQUESTS = 4096;
    const Type* FindResourceType(u32 id) { return id==123 ? &memberType : nullptr; }
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
    Check(assertions==0, "no unexpected engine assertion");
    std::printf("PCResourceBatch: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
