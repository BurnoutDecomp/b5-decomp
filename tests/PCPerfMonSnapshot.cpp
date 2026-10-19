// The overlay must read a completed frame while current-frame counters run.
#include <atomic>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <thread>
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "rw/rwcore_structs.h"

// Test-only traps for unused allocator interface slots. The registry must use
// the supplied resource allocator's DoAllocate and matching DoFree overrides.
namespace rw {
void* IResourceAllocator::Alloc(size_t,const char*,uint32_t,uint32_t,uint32_t) { std::abort(); }
void* IResourceAllocator::Alloc(size_t,const char*,uint32_t) { std::abort(); }
void IResourceAllocator::Free(void*,size_t) { std::abort(); }
Resource IResourceAllocator::DoAllocate(const ResourceDescriptor&,const char*) { std::abort(); }
void IResourceAllocator::DoFreeDisposable(Resource&) { std::abort(); }
}

struct RegistryAllocator : rw::IResourceAllocator {
    alignas(16) unsigned char bytes[64 * 96];
    rw::ResourceDescriptor descriptor{};
    int allocations=0, frees=0;
    const char* name=nullptr;
    bool matchingFree=false;
    RegistryAllocator() { std::memset(bytes,0xCD,sizeof(bytes)); }
    rw::Resource DoAllocate(const rw::ResourceDescriptor& request,const char* tag) override {
        ++allocations; descriptor=request; name=tag;
        rw::Resource result; result.m_baseResources[0]=bytes; return result;
    }
    void DoFree(const rw::Resource& resource) override {
        ++frees; matchingFree=resource.m_baseResources[0]==bytes;
        for (unsigned lane=1;lane<rw::KU_RESOURCE_LANE_COUNT;++lane)
            matchingFree &= resource.m_baseResources[lane]==nullptr;
    }
};

namespace CgsDev { namespace Assert {
std::atomic<unsigned> assertions{0};
int BeginAssert() { return 0; }
int FireAssert(const char*,const char*,int) { ++assertions; return 0; }
void* EndAssert() { return nullptr; }
} }
static int checks, failures;
static void Check(bool good,const char* label) {
    ++checks; if (!good) { ++failures; std::printf("FAIL %s\n",label); }
}
int main() {
    using namespace CgsDev;
    using namespace CgsDev::PerfMonCpu;
    RegistryAllocator allocator;
    Check(Construct(64,&allocator),"registry constructed");
    Check(allocator.allocations==1 && allocator.name==nullptr,
          "registry allocation reaches the supplied allocator without a tag");
    bool descriptor=allocator.descriptor.m_baseResourceDescriptors[0].m_size==64*96
        && allocator.descriptor.m_baseResourceDescriptors[0].m_alignment==16;
    for (unsigned lane=1;lane<rw::KU_RESOURCE_LANE_COUNT;++lane)
        descriptor &= allocator.descriptor.m_baseResourceDescriptors[lane].m_size==0
            && allocator.descriptor.m_baseResourceDescriptors[lane].m_alignment==1;
    Check(descriptor,"original 96-byte records, 16-byte alignment and five-lane descriptor");
    bool cleared=true;
    for (unsigned char byte:allocator.bytes) cleared &= byte==0;
    Check(cleared,"Construct clears every byte of the allocator-provided registry");
    const int timer = AddMonitor("Dispatch work",E_PMP_GENERAL,false,1.0f,false);
    const int scaled = AddMonitor("Per-step work",E_PMP_GENERAL,false,1.0f,true);
    StartProfiling();
    std::atomic<bool> running{true};
    std::thread writer([&] {
        for (int i=0;i<10000;++i) { StartMonitor(timer); StopMonitor(timer); }
        running = false;
    });
    bool stable = true;
    do {
        PerfMonCpuMonitorData data = {};
        GetMonitorData(timer,&data);
        stable &= data.miNumCalls == 0 && data.mfCurrentValue == 0.0f;
    } while (running);
    writer.join();
    Check(stable,"overlay sees completed data while dispatch updates live counters");
    StopProfiling();
    PerfMonCpuMonitorData data = {}; GetMonitorData(timer,&data);
    Check(data.miNumCalls == 10000 && data.mfCurrentValue > 0,
          "joined frame publishes matching time and call count");
    StartProfiling();
    GetMonitorData(timer,&data);
    Check(data.miNumCalls == 10000 && data.mfCurrentValue > 0,
          "starting another frame does not erase the displayed snapshot");
    SetNumIterationsTaken(4);
    for (int i=0;i<8;++i) { StartMonitor(scaled); StopMonitor(scaled); }
    StopProfiling(); GetMonitorData(scaled,&data);
    Check(data.miNumCalls == 2,"published count uses the same simulation scaling as time");

    std::atomic<int> registrars{2};
    auto add = [&] {
        for (int i=0;i<16;++i) AddMonitor("Complete registered slot",E_PMP_GENERAL,false,1,false);
        --registrars;
    };
    std::thread first(add),second(add);
    bool published = true;
    do {
        const int count = GetMonitorCount();
        for (int i=2;i<count;++i) {
            GetMonitorData(i,&data);
            published &= std::strcmp(data.mpcName,"Complete registered slot") == 0;
        }
    } while (registrars);
    first.join(); second.join();
    Check(published && GetMonitorCount() == 34,"parallel registration publishes complete unique slots");
    ResetValuesInActiveMonitors(); StartProfiling(); GetMonitorData(timer,&data);
    Check(data.miNumCalls == 0 && data.mfCurrentValue == 0,"reset clears the completed snapshot");
    StopProfiling(); Destruct();
    Check(allocator.frees==1 && allocator.matchingFree,
          "host teardown returns the complete resource to its producing allocator");
    Check(CgsDev::Assert::assertions == 0,"independent frame owners raise no monitor assertions");
    std::printf("PCPerfMonSnapshot: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
