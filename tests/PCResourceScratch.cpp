#include <cstdio>
#include <cstring>
#include <malloc.h>
#include "GameShared/GameClasses/System/Resource/CgsResourceScratchPool.h"

static int checks, failures, assertions;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* message, const char*, int) { ++assertions; std::printf("ASSERT %s\n",message); return 0; }
void* EndAssert() { return nullptr; }
} }
static void Check(bool ok,const char* message) {
    ++checks;
    if(!ok) { ++failures; std::printf("FAIL %s\n",message); }
}
using namespace CgsResource;
struct Fixture {
    ScratchPool pool;
    ScratchPool::InitOptions options = {};
    u8* overhead;
    u8* staging[2];
    explicit Fixture(u32 maxEntries=4,u32 bytes=1024) {
        options.muMaxEntries=maxEntries; options.miBankId=999;
        options.muOverheadMemorySize=ScratchPool::GetOverheadMemoryRequired(&options);
        // The console caller promises only 16-byte overhead alignment. The
        // master must align each of its table allocations to 128 bytes itself.
        overhead=static_cast<u8*>(_aligned_malloc(options.muOverheadMemorySize+256,128));
        std::memset(overhead,0xA5,options.muOverheadMemorySize+256);
        options.mpOverhead=overhead+16;
        for(int i=0;i<2;++i) {
            staging[i]=static_cast<u8*>(_aligned_malloc(bytes+128,128));
            std::memset(staging[i],0xEE,bytes+128);
            const int lane=i==0 ? 0 : 2;
            options.mResource.m_baseResources[lane]=staging[i];
            options.mDescriptor.m_baseResourceDescriptors[lane].m_size=bytes;
            options.mDescriptor.m_baseResourceDescriptors[lane].m_alignment=128;
        }
        pool.Construct(); pool.InitPool(&options);
    }
    ~Fixture() { _aligned_free(overhead); for(auto* p:staging) _aligned_free(p); }
    bool GuardIntact() const {
        for(u32 i=0;i<16;++i) if(overhead[i]!=0xA5) return false;
        for(u32 i=16+options.muOverheadMemorySize;i<options.muOverheadMemorySize+256;++i)
            if(overhead[i]!=0xA5) return false;
        return true;
    }
};

static Entry Resource(u64 id,void* source,u32 bytes,u32 alignment=16,int memType=0) {
    Entry entry = {};
    entry.mID.SetHash(id);
    entry.mResource.m_baseResources[memType]=source;
    entry.mResourceDescriptor.m_baseResourceDescriptors[memType].m_size=bytes;
    entry.mResourceDescriptor.m_baseResourceDescriptors[memType].m_alignment=alignment;
    return entry;
}
static bool Bytes(const void* p,u8 value,size_t n) {
    const auto* b=static_cast<const u8*>(p);
    for(size_t i=0;i<n;++i) if(b[i]!=value) return false;
    return true;
}

int main() {
    {
        Fixture f;
        Check(f.pool.GetBankId()==999 && f.pool.GetNumEntries()==0,"native initialization binds original bank and empty entry set");
        Check((reinterpret_cast<uintptr_t>(f.pool.mpEntries)&127)==0
            && (reinterpret_cast<uintptr_t>(f.pool.mpEntryIds)&127)==0
            && (reinterpret_cast<uintptr_t>(f.pool.mpDistributionEntries)&127)==0,
            "16-byte-aligned overhead produces native tables aligned to 128 bytes");
        Check(f.GuardIntact() && f.pool.mAllocator.GetUsage()<=f.options.muOverheadMemorySize,
              "reported overhead covers all native arrays and alignment gaps without overwriting guards");
        alignas(128) u8 live[512];
        std::memset(live,0xC7,sizeof(live));
        std::memset(live,0x31,64); std::memset(live+256,0x72,160);
        u8 original[512]; std::memcpy(original,live,sizeof(live));
        Entry a=Resource(0xF000000000000007ull,live,64);
        Entry b=Resource(0x1000000000000007ull,live+256,160,128);
        auto* tempA=static_cast<u8*>(f.pool.AddEntry(&a,11,0,live+256));
        auto* tempB=static_cast<u8*>(f.pool.AddEntry(&b,27,0,live));
        Check(tempA==f.staging[0] && tempB==f.staging[0]+128,"per-resource alignment leaves the correct packed gap");
        Check(f.pool.GetNumEntries()==2 && f.pool.GetEntry(1)->miEntryId==27
            && f.pool.GetEntry(1)->mpSrcLocation==live+256 && f.pool.GetEntry(1)->mpDestLocation==live,
            "staged entries retain complete source, temporary and destination pointers");
        Check(f.pool.GetTempAddress(a.mID)==tempA && f.pool.GetDestAddress(a.mID)==live+256
            && f.pool.GetTempAddress(b.mID)==tempB && f.pool.GetDestAddress(b.mID)==live,
            "64-bit colliding IDs find their distinct native pointer pairs through a wrapped hash probe");
        f.pool.SortIdList();
        Check(f.pool.GetIdList()[0]==b.mID && f.pool.GetIdList()[1]==a.mID
            && f.pool.GetIdList()[2].GetHash()==~u64(0),"ID sort compares upper bits and appends original all-ones sentinel");
        Check(f.pool.GetTempAddress(a.mID)==tempA,"sorting IDs does not reorder scratch records or corrupt the address map");
        f.pool.mGatherStream.SetBytesPerUpdate(48);
        f.pool.mScatterStream.SetBytesPerUpdate(48);
        f.pool.BeginDistribution(0);
        Check(f.pool.meUpdateStage==ScratchPool::E_UPDATESTAGE_GATHERING && f.pool.mGatherStream.IsActive(),
              "distribution arms actual gather stream");
        bool done=false, noEarlyDestWrites=true; int updates=0;
        for(;updates<10 && !done;++updates) {
            done=f.pool.UpdateGather();
            noEarlyDestWrites &= std::memcmp(live,original,sizeof(live))==0;
        }
        Check(done && updates==5,"gather respects byte budget across partial entries and multiple updates");
        Check(noEarlyDestWrites,"gather completes before any overlapping live destination is overwritten");
        Check(Bytes(tempA,0x31,64) && Bytes(tempB,0x72,160) && Bytes(tempA+64,0xEE,64),
              "actual gathered bytes and alignment gap are preserved");
        Check(f.pool.meUpdateStage==ScratchPool::E_UPDATESTAGE_SCATTERING && !f.pool.mGatherStream.IsActive()
            && f.pool.mScatterStream.IsActive(),"gather completion arms scatter exactly once");
        done=false; updates=0;
        for(;updates<10 && !done;++updates) done=f.pool.UpdateScatter();
        Check(done && updates==5,"scatter respects the same budget at real copy boundaries");
        Check(Bytes(live+256,0x31,64) && Bytes(live,0x72,160),"relocation swaps overlapping source regions without losing either resource");
        Check(Bytes(live+160,0xC7,96) && Bytes(live+416,0xC7,96),"relocation preserves unrelated live bytes");
        Check(f.pool.meUpdateStage==ScratchPool::E_UPDATESTAGE_IDLE && !f.pool.mScatterStream.IsActive(),
              "scatter completion returns the scratch pool to idle");
        f.pool.Clear();
        Check(f.pool.GetNumEntries()==0 && f.pool.GetTempAddress(a.mID)==nullptr && f.pool.GetDestAddress(b.mID)==nullptr,
              "clear invalidates complete 64-bit hash keys and empties the entry list");
        Check(f.pool.AddEntry(&a,11,0,live+256)==tempA && f.GuardIntact(),"next batch reuses its scratch buffer without growing overhead");
    }
    {
        Fixture f(1);
        alignas(128) u8 source[128] = {}, dest[128] = {};
        Entry a=Resource(0x1000000000000001ull,source,32), b=Resource(0x2000000000000002ull,source+32,32);
        Check(f.pool.AddEntry(&a,1,0,dest)!=nullptr && f.pool.AddEntry(&b,2,0,dest+32)==nullptr,
              "entry limit refuses another record despite available scratch bytes");
        Check(f.pool.GetNumEntries()==1 && f.pool.GetDestAddress(b.mID)==nullptr && f.GuardIntact(),
              "capacity failure leaves existing entries and hash map intact");
    }
    {
        Fixture f(4,32);
        alignas(128) u8 source[128] = {}, dest[128] = {};
        Entry a=Resource(0x1234000000000007ull,source,64);
        Check(f.pool.AddEntry(&a,1,0,dest)==nullptr && f.pool.GetNumEntries()==0
            && f.pool.GetTempAddress(a.mID)==nullptr,"insufficient staging memory consumes no entry or hash value");
        a.mResourceDescriptor.m_baseResourceDescriptors[0].m_size=16;
        Check(f.pool.AddEntry(&a,1,0,dest)==nullptr,"original linear allocator remains exhausted for the current batch after overflow");
        f.pool.Clear();
        Check(f.pool.AddEntry(&a,1,0,dest)==f.staging[0],"clearing the batch restores staging space after a refused allocation");
    }
    {
        Fixture f;
        alignas(128) u8 source[128],dest[128];
        std::memset(source,0x69,sizeof(source)); std::memset(dest,0,sizeof(dest));
        Entry a=Resource(0x3456000000000003ull,source,32,16,1);
        void* temp=f.pool.AddEntry(&a,19,1,dest);
        Check(temp==f.staging[1],"graphics memory stages through RW lane 2 and small-resource lane 1");
        f.pool.BeginDistribution(1);
        Check(f.pool.UpdateGather() && f.pool.UpdateScatter() && Bytes(dest,0x69,32),
              "graphics resource data follows the real gather/scatter path");
        Check(Bytes(f.staging[0],0xEE,1024) && Bytes(dest+32,0,96),"graphics staging leaves main scratch and adjacent destination bytes untouched");
    }
    Check(assertions==0,"all copies and lifecycle steps satisfy original assertions");
    std::printf("PCResourceScratch: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
