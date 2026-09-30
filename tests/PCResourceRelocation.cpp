#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceScratchPool.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "GameShared/Jobs/Relocator/CgsRelocator.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

static int checks,failures,assertions,retirements,fixups,publicationFailures;
static CgsResource::Pool* observedPool;
static CgsResource::BaseResourcePtr* aliases[2];
static const void* retired[16];
static bool retirementBeforePublication=true;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* message,const char*,int) { ++assertions; std::printf("ASSERT %s\n",message); return 0; }
void* EndAssert() { return nullptr; }
}
namespace Log {
static DebugPrint sink;
DebugPrint* gpDebugPrint=&sink;
StrStreamBase& DebugPrint::operator<<(const char*) { return *this; }
}
namespace Message { u64 gxMessageFilterFlags=0; }
}
namespace renderengine {
void WorldGeometry_OnResourceMemoryFreed(const void* base,size_t) {
    if(retirements<16) retired[retirements]=base;
    ++retirements;
    if(observedPool) {
        bool publishedOld=false;
        for(int i=0;i<2;++i) publishedOld |= observedPool->mpResourceEntries[i].mResource.m_baseResources[0]==base;
        retirementBeforePublication &= publishedOld;
    }
}
}
namespace CgsMemory {
// This suite drives the real silent mover. An accidental emergency call fails
// immediately; emergency jobs are a separate integration boundary, not faked success.
bool Relocator::Update(bool,bool) { std::abort(); }
}
namespace CgsResource {
CgsDev::StrStreamBase& operator<<(CgsDev::StrStreamBase& out,ID) { return out; }
}
#include "pc_resource_heap.inc"
#include "pc_resource_relocation.inc"

using namespace CgsResource;
static void Check(bool ok,const char* message) {
    ++checks;
    if(!ok) { ++failures; std::printf("FAIL %s\n",message); }
}
struct Payload {
    void* self;
    void* imported;
    void* graphics;
    u32 marker;
    u8 bytes[16];
};
struct MovingType : Type {
    u32 GetTypeID() const override { return 123; }
    // Actual Type::ReBase constructs and dispatches the delta. The fixture's
    // resource fixup models an ordinary internal pointer plus a graphics pointer.
    void FixUp(void* memory,const rw::Resource& offsets) const override {
        auto* body=static_cast<Payload*>(memory);
        ++fixups;
        if(body->marker>=1 && body->marker<=2 && aliases[body->marker-1]
            && aliases[body->marker-1]->mpResourceMemory!=memory)
            ++publicationFailures;
        body->self=reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(body->self)
            +reinterpret_cast<uintptr_t>(offsets.m_baseResources[0]));
        body->graphics=reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(body->graphics)
            +reinterpret_cast<uintptr_t>(offsets.m_baseResources[2]));
    }
};

struct Fixture {
    Pool pool;
    ScratchPool scratch;
    MovingType type;
    void* backing[2];
    void* overhead;
    void* stage[2];
    Entry* entries[2];
    s32 slots[2];
    Fixture() {
        type.InitCachedValues();
        Pool::InitOptions options = {};
        options.miId=3; options.mpcName="relocation"; options.muMaxResources=8;
        options.miRefCountThreshold=1; options.mbAllowDefragmentation=true;
        for(int i=0;i<2;++i) {
            backing[i]=std::calloc(1,65536);
            options.mResource.m_baseResources[i]=backing[i];
            options.mDescriptor.m_baseResourceDescriptors[i].m_size=65536;
            options.mDescriptor.m_baseResourceDescriptors[i].m_alignment=16;
            options.maHeapInfo[i].muHeapMemorySize=4096;
            options.maHeapInfo[i].muHeapAlignment=16;
            options.maHeapInfo[i].muMaxNodes=64;
        }
        pool.Construct(); pool.InitPool(&options);
        ScratchPool::InitOptions staging = {};
        staging.muMaxEntries=1; // force two genuine staging batches
        staging.muOverheadMemorySize=ScratchPool::GetOverheadMemoryRequired(&staging);
        overhead=_aligned_malloc(staging.muOverheadMemorySize,128); staging.mpOverhead=overhead;
        for(int i=0;i<2;++i) {
            stage[i]=_aligned_malloc(256,128);
            std::memset(stage[i],0xEE,256);
            const int lane=i==0 ? 0 : 2;
            staging.mResource.m_baseResources[lane]=stage[i];
            staging.mDescriptor.m_baseResourceDescriptors[lane].m_size=256;
            staging.mDescriptor.m_baseResourceDescriptors[lane].m_alignment=128;
        }
        scratch.Construct(); scratch.InitPool(&staging);
        scratch.mGatherStream.SetBytesPerUpdate(48);
        scratch.mScatterStream.SetBytesPerUpdate(48);
        for(int i=0;i<2;++i) {
            NewResource request = {};
            request.mID.SetHash(0x1234000000000001ull+i);
            request.mpResourceType=&type;
            request.miNumImports=1; request.muImportTableOffset=80;
            request.mResourceDescriptor.m_baseResourceDescriptors[0].m_size=128;
            request.mResourceDescriptor.m_baseResourceDescriptors[0].m_alignment=16;
            request.mResourceDescriptor.m_baseResourceDescriptors[1].m_size=64;
            request.mResourceDescriptor.m_baseResourceDescriptors[1].m_alignment=16;
            pool.CreateEntry(&request,&entries[i],&slots[i],true);
            pool.SetEntryStatus(slots[i],2);
            auto* body=static_cast<Payload*>(entries[i]->mResource.m_baseResources[0]);
            std::memset(body,0,128);
            body->self=reinterpret_cast<u8*>(body)+32;
            body->graphics=static_cast<u8*>(entries[i]->mResource.m_baseResources[1])+16;
            body->marker=i+1; std::memset(body->bytes,0x51+i,sizeof(body->bytes));
        }
        for(int i=0;i<2;++i) {
            auto* body=static_cast<Payload*>(entries[i]->mResource.m_baseResources[0]);
            body->imported=entries[1-i]->mResource.m_baseResources[0];
            auto* import=reinterpret_cast<BundleV2::ImportEntry*>(reinterpret_cast<u8*>(body)+80);
            import->mResourceId=entries[1-i]->mID; import->muOffset=offsetof(Payload,imported);
        }
    }
    ~Fixture() {
        for(auto* p:backing) std::free(p);
        for(auto* p:stage) _aligned_free(p);
        _aligned_free(overhead);
    }
};

int main() {
    {
        Fixture f;
        BaseResourcePtr first,second;
        BaseResourcePtr* links[]={&first,&second};
        void* original[2];
        for(int i=0;i<2;++i) {
            ResourceHandle handle;
            handle.mpResourceMemory=&f.entries[i]->mResource;
            handle.mpSourceEntry=f.entries[i];
            links[i]->CreateFromHandle(&handle); aliases[i]=links[i];
            original[i]=f.entries[i]->mResource.m_baseResources[0];
        }
        RelocateRequest requests[2] = {};
        RelocateSource sources[2] = {};
        char* heap=f.pool.maHeaps[0].GetBaseAddress();
        for(int i=0;i<2;++i) {
            int slot=1-i; // ascending source addresses, as the planner emits
            requests[i].muNode=f.entries[slot]->mauHeapIndices[0];
            requests[i].muDestOffset=128+i*128;
            sources[i].muSourceOffset=static_cast<u32>(static_cast<char*>(original[slot])-heap);
            sources[i].muSize=128; sources[i].mpOwner=reinterpret_cast<void*>(static_cast<uintptr_t>(f.slots[slot]));
        }
        retirements=fixups=publicationFailures=0; retirementBeforePublication=true; observedPool=&f.pool;
        f.pool.BeginDefragmentation(&f.scratch,requests,sources,2,0);
        Check(f.pool.IsDefragmenting() && f.pool.muNextRelocation==0,"silent relocation is armed before the first pool update");
        int updates=0,sourceWaits=0,tempWaits=0; bool tempPointers=true,imports=true;
        while(f.pool.IsDefragmenting() && updates<80) {
            f.pool.Update(); ++updates;
            if(f.pool.meDefragStage==Pool::DEFRAGSTAGE_WAIT_FOR_SOURCE_DEATH) {
                ++sourceWaits;
                const ScratchEntry* staged=f.scratch.GetEntry(0);
                auto* body=static_cast<Payload*>(staged->mpTempLocation);
                tempPointers &= body->self==reinterpret_cast<u8*>(body)+32;
                for(int i=0;i<2;++i) {
                    auto* current=static_cast<Payload*>(f.entries[i]->mResource.m_baseResources[0]);
                    imports &= current->imported==f.entries[1-i]->mResource.m_baseResources[0];
                }
            }
            if(f.pool.meDefragStage==Pool::DEFRAGSTAGE_WAIT_FOR_TEMP_DEATH) ++tempWaits;
        }
        Check(!f.pool.IsDefragmenting() && updates<80 && f.pool.muNextRelocation==2,"two bounded staging batches complete through actual pool updates");
        Check(sourceWaits>=6 && tempWaits>=6,"source and temporary memory survive their original frame delays in both batches");
        Check(tempPointers,"default resource ReBase fixes internal pointers while the resource is staged");
        Check(imports,"mutual imports follow the active scratch batch without changing unrelated imports");
        bool finalPointers=true,finalImports=true,bytes=true,handles=true;
        for(int i=0;i<2;++i) {
            void* expected=heap+(i==0 ? 256 : 128);
            auto* body=static_cast<Payload*>(f.entries[i]->mResource.m_baseResources[0]);
            finalPointers &= body==expected && body->self==reinterpret_cast<u8*>(expected)+32;
            finalImports &= body->imported==f.entries[1-i]->mResource.m_baseResources[0];
            bytes &= body->marker==i+1;
            for(u8 value:body->bytes) bytes &= value==0x51+i;
            const ResourceHandle owner=links[i]->GetResourceHandle();
            handles &= links[i]->mpResourceMemory==expected && owner.mpSourceEntry==f.entries[i]
                && owner.mpResourceMemory==&f.entries[i]->mResource;
        }
        Check(finalPointers,"destination resources contain rebased internal pointers after the real byte copies");
        Check(finalImports,"mutual imports point to final destination resources");
        Check(bytes,"resource payloads survive both copy directions and scratch reuse");
        Check(handles && publicationFailures==0 && fixups==4,"real alias rings publish before each of the four type fixups");
        Check(retirements==4 && retirementBeforePublication && retired[0]==original[1]
            && retired[1]==f.stage[0] && retired[2]==original[0] && retired[3]==f.stage[0],
            "native retirement sees every original and temporary range before aliases move");
        Check(f.pool.maHeaps[0].GetAmountFreeBytes()==4096-256 && f.pool.miDefragMemType==-1
            && f.scratch.GetNumEntries()==0,"completion retains both allocations, clears staging, and closes the silent defrag state");
        observedPool=nullptr; aliases[0]=aliases[1]=nullptr;
    }
    {
        Fixture f;
        f.pool.miRefCountThreshold=2;
        f.pool.SetEntryRefCount(f.slots[0],0);
        f.pool.Update();
        Check(f.pool.GetEntryRefCount(f.slots[0])==-1 && f.pool.GetNumEntriesInPurgatory()==1,"unreferenced loaded resource enters signed purgatory countdown");
        f.pool.Update();
        Check(f.pool.GetEntryRefCount(f.slots[0])==-2 && f.pool.GetNumEntriesInPurgatory()==1,"resource survives through the configured retirement threshold");
        const ID id=f.entries[0]->mID;
        f.pool.Update();
        Check(f.pool.FindResourceIndex(id,true,3)==-1 && f.pool.GetNumEntriesInPurgatory()==0
            && f.pool.maHeaps[0].GetAmountFreeBytes()==4096-128,"following update retires actual memory, hash and slot");
        f.pool.SetEntryStatus(f.slots[1],1); f.pool.SetEntryRefCount(f.slots[1],0);
        for(int i=0;i<4;++i) f.pool.Update();
        Check(f.pool.GetEntryStatus(f.slots[1])==1 && f.pool.GetEntryRefCount(f.slots[1])==0,
              "loading resources are excluded from the loaded-resource countdown");
    }
    {
        Fixture f;
        f.pool.mpCurrentScratchPool=&f.scratch;
        void* saved=f.entries[1]->mResource.m_baseResources[0];
        f.entries[1]->mResource.m_baseResources[0]=nullptr;
        f.entries[1]->muImportTableOffset=0;
        f.pool.ResolveAllTempScratchResources();
        f.pool.ResolveAllDestScratchResources();
        Check(static_cast<Payload*>(f.entries[0]->mResource.m_baseResources[0])->imported==saved,
              "null import tables are skipped without changing unrelated imports");
        f.entries[1]->mResource.m_baseResources[0]=saved;
    }
    Check(assertions==0,"real allocator, copy, alias and countdown paths satisfy original assertions");
    std::printf("PCResourceRelocation: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
